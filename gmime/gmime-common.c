/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2003 Ximian, Inc. (www.ximian.com)
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

#include "gmime-common.h"

#ifndef g_tolower
#define g_tolower(x) (((x) >= 'A' && (x) <= 'Z') ? (x) - 'A' + 'a' : (x))
#endif

int
g_mime_strcase_equal (gconstpointer v, gconstpointer v2)
{
	return strcasecmp ((const char *) v, (const char *) v2) == 0;
}


guint
g_mime_strcase_hash (gconstpointer key)
{
	const char *p = key;
	guint h = 0;
	
	while (*p != '\0')
		h = (h << 5) - h + g_tolower (*p++);
	
	return h;
}

#ifndef HAVE_STRNCASECMP
/**
 * strncasecmp:
 * @s1: string 1
 * @s2: string 2
 * @n:
 *
 * Compares the first @n characters of the 2 strings, @s1 and @s2,
 * ignoring the case of the characters.
 *
 * Returns an integer less than, equal to, or greater than zero if @s1
 * is found, respectively, to be less than, to match, or to be greater
 * than @s2.
 **/
int
strncasecmp (const char *s1, const char *s2, size_t n)
{
	register const char *p1 = s1, *p2 = s2;
	const char *q1 = s1 + n;
	
	for ( ; *p1 && p1 < q1; p1++, p2++)
		if (g_tolower (*p1) != g_tolower (*p2))
			return g_tolower (*p1) - g_tolower (*p2);
	
	return 0;
}
#endif

#ifndef HAVE_STRCASECMP
/**
 * strncasecmp:
 * @s1: string 1
 * @s2: string 2
 *
 * Compares the 2 strings, @s1 and @s2, ignoring the case of the
 * characters.
 *
 * Returns an integer less than, equal to, or greater than zero if @s1
 * is found, respectively, to be less than, to match, or to be greater
 * than @s2.
 **/
int
strcasecmp (const char *s1, const char *s2)
{
	register const char *p1 = s1, *p2 = s2;
	
	for ( ; *p1; p1++, p2++)
		if (g_tolower (*p1) != g_tolower (*p2))
			break;
	
	return g_tolower (*p1) - g_tolower (*p2);
}
#endif
