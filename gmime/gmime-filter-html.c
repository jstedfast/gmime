/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2001 Ximian, Inc. (www.ximian.com)
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

#include "gmime-filter-html.h"

#include "strlib.h"

#include <stdio.h>
#include <string.h>
#include <ctype.h>

static void filter_destroy (GMimeFilter *filter);
static GMimeFilter *filter_copy (GMimeFilter *filter);
static void filter_filter (GMimeFilter *filter, char *in, size_t len, 
			   size_t prespace, char **out, 
			   size_t *outlen, size_t *outprespace);
static void filter_complete (GMimeFilter *filter, char *in, size_t len, 
			     size_t prespace, char **out, 
			     size_t *outlen, size_t *outprespace);
static void filter_reset (GMimeFilter *filter);

static GMimeFilter filter_template = {
	NULL, NULL, NULL, NULL,
	0, 0, NULL, 0, 0,
	filter_destroy,
	filter_copy,
	filter_filter,
	filter_complete,
	filter_reset,
};


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
	
	new = g_new (GMimeFilterHTML, 1);
	
	new->flags = flags;
	new->colour = colour;
	
	g_mime_filter_construct (GMIME_FILTER (new), &filter_template);
	
	return GMIME_FILTER (new);
}


static void
filter_destroy (GMimeFilter *filter)
{
	g_free (filter);
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

/* 1 = non-email-address chars: "()<>@,;:\\\"/[]`'|\n\t "  */
/* 2 = non-url chars:           "()<>,;\\\"[]`'|\n\t "   */
/* 3 = trailing url garbage:    ",.!?;:>)]}\\`'-_|\n\t "  */
static unsigned short special_chars[256] = {
	 0, 0, 0, 0, 0, 0, 0, 0, 0, 7, 7, 0, 0, 0, 0, 0,
	 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	 7, 4, 3, 0, 0, 0, 0, 7, 3, 7, 0, 0, 7, 4, 4, 1,
	 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5, 7, 3, 0, 7, 4,
	 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 7, 3, 0, 4,
	 7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 7, 4, 0, 0,
	 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};


#define IS_NON_ADDR   (1 << 0)
#define IS_NON_URL    (1 << 1)
#define IS_GARBAGE    (1 << 2)

#define NON_EMAIL_CHARS         "()<>@,;:\\\"/[]`'|\n\t "
#define NON_URL_CHARS           "()<>,;\\\"[]`'|\n\t "
#define TRAILING_URL_GARBAGE    ",.!?;:>)}\\`'-_|\n\t "

#define is_addr_char(c) (isprint ((int) c) && !(special_chars[(unsigned char) c] & IS_NON_ADDR))
#define is_url_char(c)  (isprint ((int) c) && !(special_chars[(unsigned char) c] & IS_NON_URL))
#define is_trailing_garbage(c) (!isprint ((int) c) || (special_chars[(unsigned char) c] & IS_GARBAGE))

#if 0
/* this is for building the special_chars table... */
static void
table_init (void)
{
	char *c;
	
	memset (special_chars, 0, sizeof (special_chars));
	for (c = NON_EMAIL_CHARS; *c; c++)
		special_chars[(int) *c] |= IS_NON_ADDR;
	for (c = NON_URL_CHARS; *c; c++)
		special_chars[(int) *c] |= IS_NON_URL;
	for (c = TRAILING_URL_GARBAGE; *c; c++)
		special_chars[(int) *c] |= IS_GARBAGE;
}
#endif

static char *
url_extract (char **in, int inlen, gboolean check, gboolean *backup)
{
	unsigned char *inptr, *inend, *p;
	char *url;
	
	inptr = (unsigned char *) *in;
	inend = inptr + inlen;
	
	while (inptr < inend && is_url_char (*inptr))
		inptr++;
	
	if ((char *) inptr == *in)
		return NULL;
	
	/* back up if we probably went too far. */
	while (inptr > (unsigned char *) *in && is_trailing_garbage (*(inptr - 1)))
		inptr--;
	
	if (check) {
		/* make sure we weren't fooled. */
		p = memchr (*in, ':', (unsigned) ((char *) inptr - *in));
		if (!p)
			return NULL;
	}
	
	if (inptr == inend && backup) {
		*backup = TRUE;
		return NULL;
	}
	
	url = g_strndup (*in, (unsigned) ((char *) inptr - *in));
	*in = inptr;
	
	return url;
}

static char *
email_address_extract (char **in, char *inend, char *start, char **outptr, gboolean *backup)
{
	char *addr, *pre, *end, *dot;
	
	/* *in points to the '@'. Look backward for a valid local-part */
	for (pre = *in; pre - 1 >= start && is_addr_char (*(pre - 1)); pre--);
	
	if (pre == *in)
		return NULL;
	
	/* Now look forward for a valid domain part */
	for (end = *in + 1, dot = NULL; end < inend && is_addr_char (*end); end++) {
		if (*end == '.' && !dot)
			dot = end;
	}
	
	if (end >= inend && backup) {
		*backup = TRUE;
		*outptr -= (*in - pre);
		*in = pre;
		return NULL;
	}
	
	if (!dot)
		return NULL;
	
	/* Remove trailing garbage */
	while (end > *in && is_trailing_garbage (*(end - 1)))
		end--;
	if (dot > end)
		return NULL;
	
	addr = g_strndup (pre, (unsigned) (end - pre));
	*outptr -= (*in - pre);
	*in = end;
	
	return addr;
}

static gboolean
is_citation (char *inptr, char *inend, gboolean saw_citation, gboolean *backup)
{
	if (*inptr != '>')
		return FALSE;
	
	if (inend - inptr >= 6) {
		/* make sure this isn't just mbox From-magling... */
		if (strncmp (inptr, ">From ", 6) != 0)
			return TRUE;
	} else if (backup) {
		/* we don't have enough data to tell, so return */
		*backup = TRUE;
		return saw_citation;
	}
	
	/* if the previous line was a citation, then say this one is too */
	if (saw_citation)
		return TRUE;
	
	/* otherwise it was just an isolated ">From " line */
	return FALSE;
}

static gboolean
is_protocol (char *inptr, char *inend, gboolean *backup)
{
	if (inend - inptr >= 8) {
		if (!strncasecmp (inptr, "http://", 7) ||
		    !strncasecmp (inptr, "https://", 8) ||
		    !strncasecmp (inptr, "ftp://", 6) ||
		    !strncasecmp (inptr, "nntp://", 7) ||
		    !strncasecmp (inptr, "mailto:", 7) ||
		    !strncasecmp (inptr, "news:", 5))
			return TRUE;
	} else if (backup) {
		*backup = TRUE;
		return FALSE;
	}
	
	return FALSE;
}

static void
html_convert (GMimeFilter *filter, char *in, size_t inlen, size_t prespace,
	      char **out, size_t *outlen, size_t *outprespace, gboolean flush)
{
	GMimeFilterHTML *html = (GMimeFilterHTML *) filter;
	char *inptr, *inend, *outptr, *outend, *start;
	gboolean backup = FALSE;
	
	g_mime_filter_set_size (filter, inlen * 2 + 6, FALSE);
	
	inptr = start = in;
	inend = in + inlen;
	outptr = filter->outbuf;
	outend = filter->outbuf + filter->outsize;
	
	if (html->flags & GMIME_FILTER_HTML_PRE && !html->pre_open) {
		outptr = stpcpy (outptr, "<pre>");
		html->pre_open = TRUE;
	}
	
	while (inptr < inend) {
		unsigned char u;
		
		if (html->flags & GMIME_FILTER_HTML_MARK_CITATION && html->column == 0) {
			html->saw_citation = is_citation (inptr, inend, html->saw_citation,
							  flush ? &backup : NULL);
			if (backup)
				break;
			
			if (html->saw_citation) {
				if (!html->coloured) {
					char font[25];
					
					g_snprintf (font, 25, "<font color=\"#%06x\">", html->colour);
					
					outptr = check_size (filter, outptr, &outend, 25);
					outptr = stpcpy (outptr, font);
					html->coloured = TRUE;
				}
			} else if (html->coloured) {
				outptr = check_size (filter, outptr, &outend, 10);
				outptr = stpcpy (outptr, "</font>");
				html->coloured = FALSE;
			}
			
			/* display mbox-mangled ">From " as "From " */
			if (*inptr == '>' && !html->saw_citation)
				inptr++;
		} else if (html->flags & GMIME_FILTER_HTML_CITE && html->column == 0) {
			outptr = check_size (filter, outptr, &outend, 6);
			outptr = stpcpy (outptr, "&gt; ");
		}
		
		if (html->flags & GMIME_FILTER_HTML_CONVERT_URLS && isalpha ((int) *inptr)) {
			char *refurl = NULL, *dispurl = NULL;
			
			if (is_protocol (inptr, inend, flush ? &backup : NULL)) {
				dispurl = url_extract (&inptr, inend - inptr, TRUE,
						       flush ? &backup : NULL);
				if (backup)
					break;
				
				if (dispurl)
					refurl = g_strdup (dispurl);
			} else {
				if (backup)
					break;
				
				if (!strncasecmp (inptr, "www.", 4) && ((unsigned char) inptr[4]) < 0x80
				    && isalnum ((int) inptr[4])) {
					dispurl = url_extract (&inptr, inend - inptr, FALSE,
							       flush ? &backup : NULL);
					if (backup)
						break;
					
					if (dispurl)
						refurl = g_strdup_printf ("http://%s", dispurl);
				}
			}
			
			if (dispurl) {
				outptr = check_size (filter, outptr, &outend,
						     strlen (refurl) +
						     strlen (dispurl) + 15);
				outptr += sprintf (outptr, "<a href=\"%s\">%s</a>",
						   refurl, dispurl);
				html->column += strlen (dispurl);
				g_free (refurl);
				g_free (dispurl);
			}
			
			if (inptr >= inend)
				break;
		}
		
		if (*inptr == '@' && (html->flags & GMIME_FILTER_HTML_CONVERT_ADDRESSES)) {
			char *addr, *dispaddr, *outaddr;
			
			addr = email_address_extract (&inptr, inend, start, &outptr,
						      flush ? &backup : NULL);
			if (backup)
				break;
			
			if (addr) {
				dispaddr = g_strdup (addr);
				outaddr = g_strdup_printf ("<a href=\"mailto:%s\">%s</a>",
							   addr, dispaddr);
				outptr = check_size (filter, outptr, &outend, strlen (outaddr));
				outptr = stpcpy (outptr, outaddr);
				html->column += strlen (addr);
				g_free (addr);
				g_free (dispaddr);
				g_free (outaddr);
			}
		}
		
		outptr = check_size (filter, outptr, &outend, 32);
		
		switch ((u = (unsigned char) *inptr++)) {
		case '<':
			outptr = stpcpy (outptr, "&lt;");
			html->column++;
			break;
			
		case '>':
			outptr = stpcpy (outptr, "&gt;");
			html->column++;
			break;
			
		case '&':
			outptr = stpcpy (outptr, "&amp;");
			html->column++;
			break;
			
		case '"':
			outptr = stpcpy (outptr, "&quot;");
			html->column++;
			break;
			
		case '\n':
			if (html->flags & GMIME_FILTER_HTML_CONVERT_NL)
				outptr = stpcpy (outptr, "<br>");
			
			*outptr++ = '\n';
			start = inptr;
			html->column = 0;
			break;
			
		case '\t':
			if (html->flags & (GMIME_FILTER_HTML_CONVERT_SPACES)) {
				do {
					outptr = check_size (filter, outptr, &outend, 7);
					outptr = stpcpy (outptr, "&nbsp;");
					html->column++;
				} while (html->column % 8);
				break;
			}
			/* otherwise, FALL THROUGH */
			
		case ' ':
			if (html->flags & GMIME_FILTER_HTML_CONVERT_SPACES) {
				if (inptr == in || (inptr < inend && (*(inptr + 1) == ' ' ||
								      *(inptr + 1) == '\t' ||
								      *(inptr - 1) == '\n'))) {
					outptr = stpcpy (outptr, "&nbsp;");
					html->column++;
					break;
				}
			}
			/* otherwise, FALL THROUGH */
			
		default:
			if ((u >= 0x20 && u < 0x80) ||
			    (u == '\r' || u == '\t')) {
				/* Default case, just copy. */
				*outptr++ = (char) u;
			} else {
				if (html->flags & GMIME_FILTER_HTML_ESCAPE_8BIT)
					*outptr++ = '?';
				else
					outptr += g_snprintf (outptr, 9, "&#%d;", (int) u);
			}
			html->column++;
			break;
		}
	}
	
	if (inptr < inend)
		g_mime_filter_backup (filter, inptr, (unsigned) (inend - inptr));
	
	if (flush && html->pre_open) {
		outptr = check_size (filter, outptr, &outend, 10);
		outptr = stpcpy (outptr, "</pre>");
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
	html->saw_citation = FALSE;
	html->coloured = FALSE;
}
