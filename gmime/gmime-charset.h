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


#ifndef __GMIME_CHARSET_H__
#define __GMIME_CHARSET_H__

#include <glib.h>
#include <sys/types.h>

G_BEGIN_DECLS

void        g_mime_charset_map_init (void);
void        g_mime_charset_map_shutdown (void);

const char *g_mime_locale_charset (void);
const char *g_mime_locale_language (void);

const char *g_mime_charset_language (const char *charset);

const char *g_mime_charset_canon_name (const char *charset);
const char *g_mime_charset_iconv_name (const char *charset);

#ifndef GMIME_DISABLE_DEPRECATED
const char *g_mime_charset_name (const char *charset);
const char *g_mime_charset_locale_name (void);
#endif

const char *g_mime_charset_iso_to_windows (const char *isocharset);

typedef struct _GMimeCharset GMimeCharset;

/**
 * GMimeCharset:
 * @mask: charset mask
 * @level: charset level
 *
 * State used by g_mime_charset_best() and g_mime_charset_best_name().
 **/
struct _GMimeCharset {
	unsigned int mask;
	unsigned int level;
};

void g_mime_charset_init (GMimeCharset *charset);
void g_mime_charset_step (GMimeCharset *charset, const char *inbuf, size_t inlen);
const char *g_mime_charset_best_name (GMimeCharset *charset);

const char *g_mime_charset_best (const char *inbuf, size_t inlen);

gboolean g_mime_charset_can_encode (GMimeCharset *mask, const char *charset,
				    const char *text, size_t len);


void g_mime_set_user_charsets (const char **charsets);
const char **g_mime_user_charsets (void);


G_END_DECLS

#endif /* __GMIME_CHARSET_H__ */
