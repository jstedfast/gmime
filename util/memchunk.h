/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2007 Jeffrey Stedfast
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


#ifndef __MEMCHUNK_H__
#define __MEMCHUNK_H__

#include <glib.h>
#include <sys/types.h>

G_BEGIN_DECLS

typedef struct _MemChunk MemChunk;

G_GNUC_INTERNAL MemChunk *memchunk_new (size_t atomsize, size_t atomcount, gboolean autoclean);

G_GNUC_INTERNAL void     *memchunk_alloc (MemChunk *memchunk);

G_GNUC_INTERNAL void     *memchunk_alloc0 (MemChunk *memchunk);

G_GNUC_INTERNAL void      memchunk_free (MemChunk *memchunk, void *mem);

G_GNUC_INTERNAL void      memchunk_reset (MemChunk *memchunk);

G_GNUC_INTERNAL void      memchunk_clean (MemChunk *memchunk);

G_GNUC_INTERNAL void      memchunk_destroy (MemChunk *memchunk);

G_END_DECLS

#endif /* __MEMCHUNK_H__ */
