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


#ifndef __MEMCHUNK_H__
#define __MEMCHUNK_H__

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#include <glib.h>
#include <sys/types.h>

typedef struct _MemChunk MemChunk;

MemChunk *memchunk_new (size_t atomsize, size_t atomcount, gboolean autoclean);

void     *memchunk_alloc (MemChunk *memchunk);

void     *memchunk_alloc0 (MemChunk *memchunk);

void      memchunk_free (MemChunk *memchunk, void *mem);

void      memchunk_reset (MemChunk *memchunk);

void      memchunk_clean (MemChunk *memchunk);

void      memchunk_destroy (MemChunk *memchunk);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __MEMCHUNK_H__ */
