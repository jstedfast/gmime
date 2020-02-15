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


#ifndef __PACKED_BYTE_ARRAY_H__
#define __PACKED_BYTE_ARRAY_H__

#include <glib.h>
#include <sys/types.h>

G_BEGIN_DECLS

typedef struct {
	guint16 *buffer;
	int allocated;
	int cur, len;
} PackedByteArray;

G_GNUC_INTERNAL PackedByteArray *packed_byte_array_new (void);
G_GNUC_INTERNAL void packed_byte_array_free (PackedByteArray *packed);

G_GNUC_INTERNAL void packed_byte_array_clear (PackedByteArray *packed);

G_GNUC_INTERNAL void packed_byte_array_add (PackedByteArray *packed, char c);

G_GNUC_INTERNAL void packed_byte_array_copy_to (PackedByteArray *packed, char *outbuf);

G_END_DECLS

#endif /* __PACKED_BYTE_ARRAY_H__ */
