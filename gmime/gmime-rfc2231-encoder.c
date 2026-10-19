/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2026 Jeffrey Stedfast
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

#include "gmime-codec-private.h"
#include "gmime-table-private.h"

gsize
_g_mime_rfc2231_encoder_estimate_output_length (gsize length)
{
	return g_size_checked_mul (&length, length, 3) ? length : G_MAXSIZE;
}

gsize
_g_mime_rfc2231_encode (const guint8 *input, gsize length, guint8 *output, gsize capacity)
{
	static const char alphabet[] = "0123456789ABCDEF";
	guint8 *copy = NULL;
	gsize estimate, written = 0, i;
	guint8 c;

	g_return_val_if_fail (input != NULL || length == 0, 0);
	g_return_val_if_fail (output != NULL || capacity == 0, 0);
	estimate = _g_mime_rfc2231_encoder_estimate_output_length (length);
	g_return_val_if_fail (estimate != G_MAXSIZE, 0);
	g_return_val_if_fail (capacity >= estimate, 0);

	if (codec_buffers_overlap (input, length, output, estimate)) {
		copy = g_memdup2 (input, length);
		input = copy;
	}

	for (i = 0; i < length; i++) {
		c = input[i];
		if (is_attrchar (c)) {
			output[written++] = c;
		} else {
			output[written++] = '%';
			output[written++] = alphabet[c >> 4];
			output[written++] = alphabet[c & 0xf];
		}
	}

	g_free (copy);
	return written;
}
