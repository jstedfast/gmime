/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2002 Ximian, Inc. (www.ximian.com)
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


#ifndef __GMIME_ICONV_UTILS_H__
#define __GMIME_ICONV_UTILS_H__

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#include <sys/types.h>
#include <iconv.h>

char *g_mime_iconv_strdup (iconv_t cd, const char *string);
char *g_mime_iconv_strndup (iconv_t cd, const char *string, size_t n);

char *g_mime_iconv_locale_to_utf8 (const char *string);
char *g_mime_iconv_locale_to_utf8_length (const char *string, size_t n);

char *g_mime_iconv_utf8_to_locale (const char *string);
char *g_mime_iconv_utf8_to_locale_length (const char *string, size_t n);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GMIME_ICONV_UTILS_H__ */
