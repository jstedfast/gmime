/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2007 Jeffrey Stedfast
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
 *  Foundation, 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */


#ifndef __MEMCHUNK_H__
#define __MEMCHUNK_H__

#include <glib.h>
#include <sys/types.h>

G_BEGIN_DECLS

typedef struct _MemChunk MemChunk;

MemChunk *memchunk_new (size_t atomsize, size_t atomcount, gboolean autoclean);

void     *memchunk_alloc (MemChunk *memchunk);

void     *memchunk_alloc0 (MemChunk *memchunk);

void      memchunk_free (MemChunk *memchunk, void *mem);

void      memchunk_reset (MemChunk *memchunk);

void      memchunk_clean (MemChunk *memchunk);

void      memchunk_destroy (MemChunk *memchunk);

G_END_DECLS

#endif /* __MEMCHUNK_H__ */
