/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2022 Jeffrey Stedfast
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

#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "gmime-table-private.h"
#include "gmime-common.h"

void
g_mime_read_random_pool (unsigned char *buffer, size_t bytes)
{
#ifdef __unix__
	size_t nread = 0;
	ssize_t n;
	int fd;
	
	if ((fd = open ("/dev/urandom", O_RDONLY)) == -1) {
		if ((fd = open ("/dev/random", O_RDONLY)) == -1)
			return;
	}
	
	do {
		do {
			n = read (fd, (char *) buffer + nread, bytes - nread);
		} while (n == -1 && errno == EINTR);
		
		if (n == -1 || n == 0)
			break;
		
		nread += n;
	} while (nread < bytes);
	
	close (fd);
#else
	size_t i;
	
	for (i = 0; i < bytes; i++)
		buffer[i] = (unsigned char) (rand () % 256);
#endif
}

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

gboolean
_g_mime_is_valid_boundary (const char *boundary)
{
	gsize length = strlen (boundary), i;

	if (!length || length > 70 || boundary[length - 1] == ' ')
		return FALSE;

	for (i = 0; i < length; i++) {
		if (!is_bchar (boundary[i]))
			return FALSE;
	}

	return TRUE;
}

static const char *
skip_unicode_whitespace (const char *value)
{
	gunichar c;

	while (*value) {
		c = g_utf8_get_char (value);

		if (!g_unichar_isspace (c) && c != 0x0b && c != 0x85)
			break;

		value = g_utf8_next_char (value);
	}

	return value;
}

gboolean
_g_mime_strcase_equal_trimmed (const char *value, const char *token)
{
	size_t length = strlen (token);

	if (!value || !g_utf8_validate (value, -1, NULL))
		return FALSE;

	value = skip_unicode_whitespace (value);

	if (g_ascii_strncasecmp (value, token, length))
		return FALSE;

	value += length;

	value = skip_unicode_whitespace (value);

	return *value == '\0';
}

size_t
_g_mime_count_utf16_units (const char *start, const char *end)
{
	size_t length = 0;

	while (start < end) {
		length += g_utf8_get_char (start) >= 0x10000 ? 2 : 1;
		start = g_utf8_next_char (start);
	}

	return length;
}

const char *
_g_mime_get_utf16_chunk_end (const char *start, const char *end, size_t *length)
{
	size_t limit = *length, units = 0;

	while (start < end) {
		size_t count = g_utf8_get_char (start) >= 0x10000 ? 2 : 1;

		/* Never split a surrogate pair between values, or each half would be encoded separately (and lost). */
		if (units && (units >= limit || count > limit - units))
			break;

		units += count;
		start = g_utf8_next_char (start);
	}

	*length = units;

	return start;
}
