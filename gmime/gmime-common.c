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

#include <string.h>

#include "gmime-table-private.h"
#include "gmime-common.h"

#ifndef g_tolower
#define g_tolower(x) (((x) >= 'A' && (x) <= 'Z') ? (x) - 'A' + 'a' : (x))
#endif

int
g_mime_strcase_equal (gconstpointer v, gconstpointer v2)
{
	return g_ascii_strcasecmp ((const char *) v, (const char *) v2) == 0;
}


guint
g_mime_strcase_hash (gconstpointer key)
{
	const char *p = key;
	guint h = 0;
	
	while (*p != '\0') {
		h = (h << 5) - h + g_tolower (*p);
		p++;
	}
	
	return h;
}


/**
 * g_mime_strdup_trim:
 * @str: The string to duplicate and trim
 *
 * Duplicates the given input string while also trimming leading and
 * trailing whitespace.
 *
 * Returns a duplicate string, minus any leading and trailing
 * whitespace that the original string may have contained.
 **/
char *
g_mime_strdup_trim (const char *str)
{
	register const char *inptr = str;
	register const char *end;
	const char *start;
	
	while (is_lwsp (*inptr))
		inptr++;
	
	start = inptr;
	end = inptr;
	
	while (*inptr) {
		if (!is_lwsp (*inptr++))
			end = inptr;
	}
	
	return g_strndup (start, (size_t) (end - start));
}
