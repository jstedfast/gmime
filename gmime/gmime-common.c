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
_g_mime_is_unicode_whitespace (gunichar c)
{
	/* Unlike .NET, GLib does not classify VT and NEL as whitespace. */
	return c == '\v' || c == 0x85 || g_unichar_isspace (c);
}

char *
_g_mime_strdup_trim_unicode (const char *value)
{
	const char *start, *end, *previous;

	if (!value)
		return NULL;

	start = value;

	while (*start && _g_mime_is_unicode_whitespace (g_utf8_get_char (start)))
		start = g_utf8_next_char (start);

	end = start + strlen (start);

	while (end > start) {
		previous = g_utf8_prev_char (end);
		if (!_g_mime_is_unicode_whitespace (g_utf8_get_char (previous)))
			break;

		end = previous;
	}

	return g_strndup (start, end - start);
}

gboolean
_g_mime_strcase_equal_trimmed (const char *value, const char *token)
{
	size_t length = strlen (token);

	if (!value)
		return FALSE;
	while (*value && _g_mime_is_unicode_whitespace (g_utf8_get_char (value)))
		value = g_utf8_next_char (value);
	if (g_ascii_strncasecmp (value, token, length))
		return FALSE;
	value += length;
	while (*value && _g_mime_is_unicode_whitespace (g_utf8_get_char (value)))
		value = g_utf8_next_char (value);

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
