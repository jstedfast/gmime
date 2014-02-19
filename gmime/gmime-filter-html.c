/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2014 Jeffrey Stedfast
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public License
 *  as published by the Free Software Foundation; either version 2.1
 *  of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, 51 Franklin Street, Fifth Floor, Boston, MA
 *  02110-1301, USA.
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <string.h>

#include "url-scanner.h"
#include "gmime-filter-html.h"

#ifdef ENABLE_WARNINGS
#define w(x) x
#else
#define w(x)
#endif /* ENABLE_WARNINGS */

#define d(x)


/**
 * SECTION: gmime-filter-html
 * @title: GMimeFilterHTML
 * @short_description: Convert plain text into HTML
 * @see_also: #GMimeFilter
 *
 * A #GMimeFilter used for converting plain text into HTML.
 **/


#define CONVERT_WEB_URLS  GMIME_FILTER_HTML_CONVERT_URLS
#define CONVERT_ADDRSPEC  GMIME_FILTER_HTML_CONVERT_ADDRESSES

static struct {
	unsigned int mask;
	urlpattern_t pattern;
} patterns[] = {
	{ CONVERT_WEB_URLS, { "file://",   "",        url_file_start,     url_file_end     } },
	{ CONVERT_WEB_URLS, { "ftp://",    "",        url_web_start,      url_web_end      } },
	{ CONVERT_WEB_URLS, { "sftp://",   "",        url_web_start,      url_web_end      } },
	{ CONVERT_WEB_URLS, { "http://",   "",        url_web_start,      url_web_end      } },
	{ CONVERT_WEB_URLS, { "https://",  "",        url_web_start,      url_web_end      } },
	{ CONVERT_WEB_URLS, { "news://",   "",        url_web_start,      url_web_end      } },
	{ CONVERT_WEB_URLS, { "nntp://",   "",        url_web_start,      url_web_end      } },
	{ CONVERT_WEB_URLS, { "telnet://", "",        url_web_start,      url_web_end      } },
	{ CONVERT_WEB_URLS, { "webcal://", "",        url_web_start,      url_web_end      } },
	{ CONVERT_WEB_URLS, { "mailto:",   "",        url_web_start,      url_web_end      } },
	{ CONVERT_WEB_URLS, { "callto:",   "",        url_web_start,      url_web_end      } },
	{ CONVERT_WEB_URLS, { "h323:",     "",        url_web_start,      url_web_end      } },
	{ CONVERT_WEB_URLS, { "sip:",      "",        url_web_start,      url_web_end      } },
	{ CONVERT_WEB_URLS, { "www.",      "http://", url_web_start,      url_web_end      } },
	{ CONVERT_WEB_URLS, { "ftp.",      "ftp://",  url_web_start,      url_web_end      } },
	{ CONVERT_ADDRSPEC, { "@",         "mailto:", url_addrspec_start, url_addrspec_end } },
};

#define NUM_URL_PATTERNS (sizeof (patterns) / sizeof (patterns[0]))

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
	filter->scanner = url_scanner_new ();
	
	filter->flags = 0;
	filter->colour = 0;
	filter->column = 0;
	filter->pre_open = FALSE;
}

static void
g_mime_filter_html_finalize (GObject *object)
{
	GMimeFilterHTML *html = (GMimeFilterHTML *) object;
	
	url_scanner_free (html->scanner);
	
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
	size_t outleft = (size_t) (*outend - outptr);
	size_t offset;
	
	if (outleft >= len)
		return outptr;
	
	offset = outptr - filter->outbuf;
	
	g_mime_filter_set_size (filter, filter->outsize + len, TRUE);
	
	*outend = filter->outbuf + filter->outsize;
	
	return filter->outbuf + offset;
}

static int
citation_depth (const char *in, const char *inend)
{
	register const char *inptr = in;
	int depth = 1;
	
	if (*inptr++ != '>')
		return 0;
	
	/* check that it isn't an escaped From line */
	if (!strncmp (inptr, "From", 4))
		return 0;
	
	while (inptr < inend && *inptr != '\n') {
		if (*inptr == ' ')
			inptr++;
		
		if (inptr >= inend || *inptr++ != '>')
			break;
		
		depth++;
	}
	
	return depth;
}

static inline gunichar
html_utf8_getc (const unsigned char **in, const unsigned char *inend)
{
	register const unsigned char *inptr = *in;
	register unsigned char c, r;
	register gunichar u, m;
	
	if (inptr == inend)
		return 0;
	
	while (inptr < inend) {
		r = *inptr++;
	loop:
		if (r < 0x80) {
			*in = inptr;
			return r;
		} else if (r < 0xf8) { /* valid start char? */
			u = r;
			m = 0x7f80;	/* used to mask out the length bits */
			do {
				if (inptr >= inend)
					return 0xffff;
				
				c = *inptr++;
				if ((c & 0xc0) != 0x80) {
					r = c;
					goto loop;
				}
				
				u = (u << 6) | (c & 0x3f);
				r <<= 1;
				m <<= 5;
			} while (r & 0x40);
			
			*in = inptr;
			
			u &= ~m;
			
			return u;
		}
	}
	
	return 0xffff;
}

static char *
writeln (GMimeFilter *filter, const char *in, const char *end, char *outptr, char **outend)
{
	GMimeFilterHTML *html = (GMimeFilterHTML *) filter;
	const unsigned char *instart = (const unsigned char *) in;
	const unsigned char *inend = (const unsigned char *) end;
	const unsigned char *inptr = instart;
	
	while (inptr < inend) {
		gunichar u;
		
		outptr = check_size (filter, outptr, outend, 16);
		
		u = html_utf8_getc (&inptr, inend);
		switch (u) {
		case 0xffff:
			w(g_warning ("Invalid UTF-8 sequence encountered"));
			return outptr;
			break;
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
				if (inptr == (instart + 1) || (inptr < inend && (*inptr == ' ' || *inptr == '\t'))) {
					outptr = g_stpcpy (outptr, "&nbsp;");
					html->column++;
					break;
				}
			}
			/* otherwise, FALL THROUGH */
		default:
			if (u >= 0x20 && u < 0x80) {
				*outptr++ = (char) (u & 0xff);
			} else {
				if (html->flags & GMIME_FILTER_HTML_ESCAPE_8BIT)
					*outptr++ = '?';
				else
					outptr += sprintf (outptr, "&#%u;", u);
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
	
	start = inptr = in;
	inend = in + inlen;
	outptr = filter->outbuf;
	outend = filter->outbuf + filter->outsize;
	
	if (html->flags & GMIME_FILTER_HTML_PRE && !html->pre_open) {
		outptr = g_stpcpy (outptr, "<pre>");
		html->pre_open = TRUE;
	}
	
	do {
		while (inptr < inend && *inptr != '\n')
			inptr++;
		
		if (inptr == inend && !flush)
			break;
		
		html->column = 0;
		depth = 0;
		
		if (html->flags & GMIME_FILTER_HTML_MARK_CITATION) {
			if ((depth = citation_depth (start, inend)) > 0) {
				char font[25];
				
				/* FIXME: we could easily support multiple colour depths here */
				
				g_snprintf (font, 25, "<font color=\"#%06x\">", (html->colour & 0xffffff));
				
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
			size_t matchlen, buflen, len;
			urlmatch_t match;
			
			len = inptr - start;
			
			do {
				if (url_scanner_scan (html->scanner, start, len, &match)) {
					/* write out anything before the first regex match */
					outptr = writeln (filter, start, start + match.um_so,
							  outptr, &outend);
					
					start += match.um_so;
					len -= match.um_so;
					
					matchlen = match.um_eo - match.um_so;
					
					buflen = 20 + strlen (match.prefix) + matchlen + matchlen;
					outptr = check_size (filter, outptr, &outend, buflen);
					
					/* write out the href tag */
					outptr = g_stpcpy (outptr, "<a href=\"");
					outptr = g_stpcpy (outptr, match.prefix);
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
		
		if (inptr < inend)
			*outptr++ = '\n';
		
		start = ++inptr;
	} while (inptr < inend);
	
	if (flush) {
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
 * Creates a new GMimeFilterHTML filter which can be used to convert a
 * plain UTF-8 text stream into an html stream.
 *
 * Returns: a new html filter.
 **/
GMimeFilter *
g_mime_filter_html_new (guint32 flags, guint32 colour)
{
	GMimeFilterHTML *filter;
	guint i;
	
	filter = g_object_newv (GMIME_TYPE_FILTER_HTML, 0, NULL);
	filter->flags = flags;
	filter->colour = colour;
	
	for (i = 0; i < NUM_URL_PATTERNS; i++) {
		if (patterns[i].mask & flags)
			url_scanner_add (filter->scanner, &patterns[i].pattern);
	}
	
	return (GMimeFilter *) filter;
}
