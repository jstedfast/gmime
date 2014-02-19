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


#ifndef __GMIME_ICONV_UTILS_H__
#define __GMIME_ICONV_UTILS_H__

#include <glib.h>

#include <sys/types.h>
#include <iconv.h>

G_BEGIN_DECLS

char *g_mime_iconv_strdup (iconv_t cd, const char *str);
char *g_mime_iconv_strndup (iconv_t cd, const char *str, size_t n);

char *g_mime_iconv_locale_to_utf8 (const char *str);
char *g_mime_iconv_locale_to_utf8_length (const char *str, size_t n);

char *g_mime_iconv_utf8_to_locale (const char *str);
char *g_mime_iconv_utf8_to_locale_length (const char *str, size_t n);

G_END_DECLS

#endif /* __GMIME_ICONV_UTILS_H__ */
