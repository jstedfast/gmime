/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2001-2004 Ximian, Inc. (www.ximian.com)
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


#ifndef __GMIME_CHARSET_H__
#define __GMIME_CHARSET_H__

#include <glib.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

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


typedef struct _GMimeCharset {
	unsigned int mask;
	unsigned int level;
} GMimeCharset;

void g_mime_charset_init (GMimeCharset *charset);

void g_mime_charset_step (GMimeCharset *charset, const char *in, size_t len);

const char *g_mime_charset_best_name (GMimeCharset *charset);

const char *g_mime_charset_best (const char *in, size_t inlen);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GMIME_CHARSET_H__ */
