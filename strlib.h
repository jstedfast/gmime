/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2001 Ximain, Inc. (www.ximian.com)
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


#ifndef __STRLIB_H__
#define __STRLIB_H__

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#include <sys/types.h>
#include <string.h>

#if !defined (HAVE_MEMCHR) || !defined (memchr)
void *memchr (const void *s, int c, size_t n);
#endif

#if !defined (HAVE_MEMRCHR) || !defined (memrchr)
void *memrchr (const void *s, int c, size_t n);
#endif

#if !defined (HAVE_MEMMEM) || !defined (memmem)
void *memmem (const void *haystack, size_t haystacklen,
	      const void *needle, size_t needlelen);
#endif

#if !defined (HAVE_STRLEN) || !defined (strlen)
size_t strlen (const char *s);
#endif

#if !defined (HAVE_STRCPY) || !defined (strcpy)
char *strcpy (char *dest, const char *src);
#endif

#if !defined (HAVE_STRNCPY) || !defined (strncpy)
char *strncpy (char *dest, const char *src, size_t n);
#endif

#if !defined (HAVE_STRLCPY) || !defined (strlcpy)
char *strlcpy (char *dest, const char *src, size_t n);
#endif

#if !defined (HAVE_STPCPY) || !defined (stpcpy)
char *stpcpy (char *dest, const char *src);
#endif

#if !defined (HAVE_STRCAT) || !defined (strcat)
char *strcat (char *dest, const char *src);
#endif

#if !defined (HAVE_STRNCAT) || !defined (strncat)
char *strncat (char *dest, const char *src, size_t n);
#endif

#if !defined (HAVE_STRLCAT) || !defined (strlcat)
char *strlcat (char *dest, const char *src, size_t n);
#endif

#if !defined (HAVE_STRCHR) || !defined (strchr)
char *strchr (const char *s, int c);
#endif

#if !defined (HAVE_STRRCHR) || !defined (strrchr)
char *strrchr (const char *s, int c);
#endif

#if !defined (HAVE_STRNSTR) || !defined (strnstr)
char *strnstr (const char *haystack, const char *needle, size_t haystacklen);
#endif

#if !defined (HAVE_STRSTR) || !defined (strstr)
char *strstr (const char *haystack, const char *needle);
#endif

#if !defined (HAVE_STRNCASESTR) || !defined (strncasestr)
char *strncasestr (const char *haystack, const char *needle, size_t haystacklen);
#endif

#if !defined (HAVE_STRCASESTR) || !defined (strcasestr)
char *strcasestr (const char *haystack, const char *needle);
#endif

#if !defined (HAVE_STRNCASECMP) || !defined (strncasecmp)
int strncasecmp (const char *s1, const char *s2, size_t n);
#endif

#if !defined (HAVE_STRCASECMP) || !defined (strcasecmp)
int strcasecmp (const char *s1, const char *s2);
#endif

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __STRLIB_H__ */
