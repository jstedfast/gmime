/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2001-2002 Ximian, Inc. (www.ximian.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <regex.h>

#include "gmime-filter-html.h"

#define d(x)

struct _UrlRegexPattern {
	unsigned int mask;
	char *pattern;
	char *prefix;
	regex_t *preg;
	regmatch_t matches;
};

static struct _UrlRegexPattern patterns[] = {
	{ GMIME_FILTER_HTML_CONVERT_URLS, "(news|nntp|telnet|file|ftp|http|https)://([-a-z0-9]+(:[-a-z0-9]+)?@)?[-a-z0-9.]+[-a-z0-9](:[0-9]*)?(/[-a-z0-9_$.+!*(),;:@%&=?/~#]*[^]'.}>\\) ,?!;:\"]?)?", "", NULL, { 0, 0 } },
	{ GMIME_FILTER_HTML_CONVERT_URLS, "www\\.[-a-z0-9.]+[-a-z0-9](:[0-9]*)?(/[-A-Za-z0-9_$.+!*(),;:@%&=?/~#]*[^]'.}>\\) ,?!;:\"]?)?", "http://", NULL, { 0, 0 } },
	{ GMIME_FILTER_HTML_CONVERT_URLS, "ftp\\.[-a-z0-9.]+[-a-z0-9](:[0-9]*)?(/[-A-Za-z0-9_$.+!*(),;:@%&=?/~#]*[^]'.}>\\) ,?!;:\"]?)?", "ftp://", NULL, { 0, 0 } },
	{ GMIME_FILTER_HTML_CONVERT_ADDRESSES, "([-_a-z0-9.\\+]+@[-_a-z0-9.]+)", "mailto:", NULL, { 0, 0 } }
};

#define NUM_URL_REGEX_PATTERNS (sizeof (patterns) / sizeof (patterns[0]))

static void g_mime_filter_html_class_init (GMimeFilterHTMLClass *klass);
static void g_mime_filter_html_init (GMimeFilterHTML *filter, GMimeFilterHTMLClass *klass);
static void g_mime_filter_html_finalize (GObject *object);

static GMimeFilter *filter_copy (GMimeFilter *filter);
static void filter_filter (GMimeFilter *filter, char *in, size_t len, size_t prespace,
			   char **out, size_t *outlen, size_t *outprespace);
static void filter_complete (GMimeFilter *filter, char *in, size_t len, size_t prespace,
			     char **out, size_t *outlen, size_t *outprespace);
static void filter_reset (GMimeFilter *filter);


static GMimeFilterClass *parent_class = NULL;


GType
g_mime_filter_html_get_type (void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof (GMimeFilterHTMLClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc) g_mime_filter_html_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (GMimeFilterHTML),
			0,    /* n_preallocs */
			(GInstanceInitFunc) g_mime_filter_html_init,
		};
		
		type = g_type_register_static (GMIME_TYPE_FILTER, "GMimeFilterHTML", &info, 0);
	}
	
	return type;
}


static void
g_mime_filter_html_class_init (GMimeFilterHTMLClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GMimeFilterClass *filter_class = GMIME_FILTER_CLASS (klass);
	
	parent_class = g_type_class_ref (GMIME_TYPE_FILTER);
	
	object_class->finalize = g_mime_filter_html_finalize;
	
	filter_class->copy = filter_copy;
	filter_class->filter = filter_filter;
	filter_class->complete = filter_complete;
	filter_class->reset = filter_reset;
}

static void
g_mime_filter_html_init (GMimeFilterHTML *filter, GMimeFilterHTMLClass *klass)
{
	int i;
	
	filter->patterns = g_malloc (sizeof (patterns));
	memcpy (filter->patterns, patterns, sizeof (patterns));
	
	for (i = 0; i < NUM_URL_REGEX_PATTERNS; i++) {
		filter->patterns[i].preg = g_malloc (sizeof (regex_t));
		if (regcomp (filter->patterns[i].preg, patterns[i].pattern, REG_EXTENDED) == -1) {
			/* error building the regex_t so we can't use this pattern */
			filter->patterns[i].preg = NULL;
			filter->patterns[i].mask = 0;
		}
	}
	
	filter->flags = 0;
	filter->colour = 0;
	filter->column = 0;
	filter->pre_open = FALSE;
}

static void
g_mime_filter_html_finalize (GObject *object)
{
	GMimeFilterHTML *html = (GMimeFilterHTML *) object;
	int i;
	
	for (i = 0; i < NUM_URL_REGEX_PATTERNS; i++) {
		if (html->patterns[i].preg) {
			regfree (html->patterns[i].preg);
			g_free (html->patterns[i].preg);
		}
	}
	
	g_free (html->patterns);
	
	G_OBJECT_CLASS (parent_class)->finalize (object);
}


static GMimeFilter *
filter_copy (GMimeFilter *filter)
{
	GMimeFilterHTML *html = (GMimeFilterHTML *) filter;
	
	return g_mime_filter_html_new (html->flags, html->colour);
}

static char *
check_size (GMimeFilter *filter, char *outptr, char **outend, size_t len)
{
	size_t offset;
	
	if (*outend - outptr >= len)
		return outptr;
	
	offset = outptr - filter->outbuf;
	
	g_mime_filter_set_size (filter, filter->outsize + len, TRUE);
	
	*outend = filter->outbuf + filter->outsize;
	
	return filter->outbuf + offset;
}

static int
citation_depth (const char *in)
{
	register const char *inptr = in;
	int depth = 1;
	
	if (*inptr++ != '>')
		return 0;
	
	/* check that it isn't an escaped From line */
	if (!strncmp (inptr, "From", 4))
		return 0;
	
	while (*inptr != '\n') {
		if (*inptr == ' ')
			inptr++;
		
		if (*inptr++ != '>')
			break;
		
		depth++;
	}
	
	return depth;
}

static char *
writeln (GMimeFilter *filter, const char *in, const char *inend, char *outptr, char **outend)
{
	GMimeFilterHTML *html = (GMimeFilterHTML *) filter;
	register const char *inptr = in;
	
	while (inptr < inend) {
		unsigned char u;
		
		outptr = check_size (filter, outptr, outend, 9);
		
		switch ((u = (unsigned char) *inptr++)) {
		case '<':
			outptr = g_stpcpy (outptr, "&lt;");
			html->column++;
			break;
		case '>':
			outptr = g_stpcpy (outptr, "&gt;");
			html->column++;
			break;
		case '&':
			outptr = g_stpcpy (outptr, "&amp;");
			html->column++;
			break;
		case '"':
			outptr = g_stpcpy (outptr, "&quot;");
			html->column++;
			break;
		case '\t':
			if (html->flags & (GMIME_FILTER_HTML_CONVERT_SPACES)) {
				do {
					outptr = check_size (filter, outptr, outend, 7);
					outptr = g_stpcpy (outptr, "&nbsp;");
					html->column++;
				} while (html->column % 8);
				break;
			}
			/* otherwise, FALL THROUGH */
		case ' ':
			if (html->flags & GMIME_FILTER_HTML_CONVERT_SPACES) {
				if (inptr == (in + 1) || *inptr == ' ' || *inptr == '\t') {
					outptr = g_stpcpy (outptr, "&nbsp;");
					html->column++;
					break;
				}
			}
			/* otherwise, FALL THROUGH */
		default:
			if (!(u >= 0x20 && u < 0x80)) {
				if (html->flags & GMIME_FILTER_HTML_ESCAPE_8BIT)
					*outptr++ = '?';
				else
					outptr += g_snprintf (outptr, 9, "&#%d;", (int) u);
			} else {
				*outptr++ = (char) u;
			}
			html->column++;
			break;
		}
	}
	
	return outptr;
}

static void
html_convert (GMimeFilter *filter, char *in, size_t inlen, size_t prespace,
	      char **out, size_t *outlen, size_t *outprespace, gboolean flush)
{
	GMimeFilterHTML *html = (GMimeFilterHTML *) filter;
	register char *inptr, *outptr;
	char *start, *outend;
	const char *inend;
	int depth;
	
	g_mime_filter_set_size (filter, inlen * 2 + 6, FALSE);
	
	inptr = in;
	inend = in + inlen;
	outptr = filter->outbuf;
	outend = filter->outbuf + filter->outsize;
	
	if (html->flags & GMIME_FILTER_HTML_PRE && !html->pre_open) {
		outptr = g_stpcpy (outptr, "<pre>");
		html->pre_open = TRUE;
	}
	
	start = inptr;
	while (inptr < inend && *inptr != '\n')
		inptr++;
	
	while (inptr < inend) {
		html->column = 0;
		
		if (html->flags & GMIME_FILTER_HTML_MARK_CITATION) {
			if ((depth = citation_depth (start)) > 0) {
				char font[25];
				
				/* FIXME: we could easily support multiple colour depths here */
				
				g_snprintf (font, 25, "<font color=\"#%06x\">", html->colour);
				
				outptr = check_size (filter, outptr, &outend, 25);
				outptr = g_stpcpy (outptr, font);
			} else if (*start == '>') {
				/* >From line */
				start++;
			}
		} else if (html->flags & GMIME_FILTER_HTML_CITE) {
			outptr = check_size (filter, outptr, &outend, 6);
			outptr = g_stpcpy (outptr, "&gt; ");
			html->column += 2;
		}
		
#define CONVERT_URLS_OR_ADDRESSES (GMIME_FILTER_HTML_CONVERT_URLS | GMIME_FILTER_HTML_CONVERT_ADDRESSES)
		if (html->flags & CONVERT_URLS_OR_ADDRESSES) {
			struct _UrlRegexPattern *fmatch, *pat;
			char *linebuf, *refurl, *dispurl;
			size_t matchlen, len;
			regoff_t offset;
			char save;
			int i;
			
			len = inptr - start;
			linebuf = g_malloc (len + 1);
			memcpy (linebuf, start, len);
			linebuf[len] = '\0';
			
			start = linebuf;
			
			do {
				/* search for all of our patterns */
				offset = 0;
				fmatch = NULL;
				for (i = 0; i < NUM_URL_REGEX_PATTERNS; i++) {
					pat = html->patterns + i;
					if ((html->flags & pat->mask) &&
					    !regexec (pat->preg, start, 1, &pat->matches, 0)) {
						if (pat->matches.rm_so < offset) {
							*(start + offset) = save;
							fmatch = NULL;
						}
						
						if (!fmatch) {
							fmatch = pat;
							offset = pat->matches.rm_so;
							
							/* optimisation so we don't have to search the
							   entire line buffer for the next pattern */
							save = *(start + offset);
							*(start + offset) = '\0';
						}
					}
				}
				
				if (fmatch) {
					/* restore our char */
					*(start + offset) = save;
					
					/* write out anything before the first regex match */
					outptr = writeln (filter, start, start + offset, outptr, &outend);
					start += offset;
					len -= offset;
					
#define MATCHLEN(matches) (matches.rm_eo - matches.rm_so)
					matchlen = MATCHLEN (fmatch->matches);
					
					i = 20 + strlen (fmatch->prefix) + matchlen + matchlen;
					outptr = check_size (filter, outptr, &outend, i);
					
					/* write out the href tag */
					outptr = g_stpcpy (outptr, "<a href=\"");
					outptr = g_stpcpy (outptr, fmatch->prefix);
					memcpy (outptr, start, matchlen);
					outptr += matchlen;
					outptr = g_stpcpy (outptr, "\">");
					
					/* now write the matched string */
					memcpy (outptr, start, matchlen);
					html->column += matchlen;
					outptr += matchlen;
					start += matchlen;
					len -= matchlen;
					
					/* close the href tag */
					outptr = g_stpcpy (outptr, "</a>");
				} else {
					/* nothing matched so write out the remainder of this line buffer */
					outptr = writeln (filter, start, start + len, outptr, &outend);
					break;
				}
			} while (len > 0);
			
			g_free (linebuf);
		} else {
			outptr = writeln (filter, start, inptr, outptr, &outend);
		}
		
		if ((html->flags & GMIME_FILTER_HTML_MARK_CITATION) && depth > 0) {
			outptr = check_size (filter, outptr, &outend, 8);
			outptr = g_stpcpy (outptr, "</font>");
		}
		
		if (html->flags & GMIME_FILTER_HTML_CONVERT_NL) {
			outptr = check_size (filter, outptr, &outend, 5);
			outptr = g_stpcpy (outptr, "<br>");
		}
		
		*outptr++ = '\n';
		
		start = ++inptr;
		while (inptr < inend && *inptr != '\n')
			inptr++;
	}
	
	if (flush) {
		/* flush the rest of our input buffer */
		if (start < inend)
			outptr = writeln (filter, start, inend, outptr, &outend);
		
		if (html->pre_open) {
			/* close the pre-tag */
			outptr = check_size (filter, outptr, &outend, 10);
			outptr = g_stpcpy (outptr, "</pre>");
		}
	} else if (start < inend) {
		/* backup */
		g_mime_filter_backup (filter, start, (unsigned) (inend - start));
	}
	
	*out = filter->outbuf;
	*outlen = outptr - filter->outbuf;
	*outprespace = filter->outpre;
}

static void
filter_filter (GMimeFilter *filter, char *in, size_t len, size_t prespace,
	       char **out, size_t *outlen, size_t *outprespace)
{
	html_convert (filter, in, len, prespace, out, outlen, outprespace, FALSE);
}

static void 
filter_complete (GMimeFilter *filter, char *in, size_t len, size_t prespace,
		 char **out, size_t *outlen, size_t *outprespace)
{
	html_convert (filter, in, len, prespace, out, outlen, outprespace, TRUE);
}

static void
filter_reset (GMimeFilter *filter)
{
	GMimeFilterHTML *html = (GMimeFilterHTML *) filter;
	
	html->column = 0;
	html->pre_open = FALSE;
}


/**
 * g_mime_filter_html_new:
 * @flags: html flags
 * @colour: citation colour
 *
 * Creates a new GMimeFilterHTML filter.
 *
 * Returns a new html filter.
 **/
GMimeFilter *
g_mime_filter_html_new (guint32 flags, guint32 colour)
{
	GMimeFilterHTML *new;
	
	new = g_object_new (GMIME_TYPE_FILTER_HTML, NULL, NULL);
	new->flags = flags;
	new->colour = colour;
	
	return (GMimeFilter *) new;
}
