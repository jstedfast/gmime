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


#ifndef __GMIME_STREAM_MEM_H__
#define __GMIME_STREAM_MEM_H__

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#include <glib.h>
#include <gmime-stream.h>

typedef struct _GMimeStreamMem {
	GMimeStream parent;
	
	gboolean owner;
	GByteArray *buffer;
} GMimeStreamMem;

#define GMIME_STREAM_MEM_TYPE g_str_hash ("GMimeStreamMem")
#define GMIME_IS_STREAM_MEM(stream) (((GMimeStream *) stream)->type == GMIME_STREAM_MEM)
#define GMIME_STREAM_MEM(stream) ((GMimeStreamMem *) stream)

GMimeStream *g_mime_stream_mem_new (void);
GMimeStream *g_mime_stream_mem_new_with_byte_array (GByteArray *array);
GMimeStream *g_mime_stream_mem_new_with_buffer (const char *buffer, size_t len);

void g_mime_stream_mem_set_byte_array (GMimeStreamMem *mem, GByteArray *array);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GMIME_STREAM_MEM_H__ */
