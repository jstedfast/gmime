/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2020 Jeffrey Stedfast
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

#include "packed.h"


PackedByteArray *
packed_byte_array_new (void)
{
	PackedByteArray *packed;
	
	packed = g_slice_new (PackedByteArray);
	packed->buffer = g_malloc (sizeof (guint16) * 64);
	packed->allocated = 64;
	packed->cur = -1;
	packed->len = 0;
	
	return packed;
}


void
packed_byte_array_free (PackedByteArray *packed)
{
	g_free (packed->buffer);
	g_slice_free (PackedByteArray, packed);
}


void
packed_byte_array_clear (PackedByteArray *packed)
{
	packed->cur = -1;
	packed->len = 0;
}


static guint16 *
ensure_buffer_size (PackedByteArray *packed, int size)
{
	if (packed->allocated > size)
		return packed->buffer;
	
	size_t ideal = (size + 63) & ~63;
	
	packed->buffer = g_realloc (packed->buffer, sizeof (guint16) * ideal);
	packed->allocated = ideal;
	
	return packed->buffer;
}


void
packed_byte_array_add (PackedByteArray *packed, char c)
{
	guint16 *buffer = packed->buffer;
        int cur = packed->cur;
	
	if (cur < 0 || c != (char) (buffer[cur] & 0xff) || (buffer[cur] & 0xff00) == 0xff00) {
		buffer = ensure_buffer_size (packed, cur  + 2);
		buffer[++cur] = (guint16) ((1 << 8) | (unsigned char) c);
	} else {
		buffer[cur] += (1 << 8);
	}
	
	packed->cur = cur;
	packed->len++;
}


void
packed_byte_array_copy_to (PackedByteArray *packed, char *outbuf)
{
	register char *outptr = outbuf;
	int count, i, n;
	char c;
	
	for (i = 0; i <= packed->cur; i++) {
		count = (packed->buffer[i] >> 8) & 0xff;
		c = (char) (packed->buffer[i] & 0xff);
		
		for (n = 0; n < count; n++)
			*outptr++ = c;
	}
}
