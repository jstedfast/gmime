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


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <string.h>

#include "memchunk.h"


typedef struct _MemChunkFreeNode {
	struct _MemChunkFreeNode *next;
	unsigned int atoms;
} MemChunkFreeNode;

typedef struct _MemChunkNodeInfo {
	struct _MemChunkNodeInfo *next;
	unsigned char *block;
	size_t blocksize;
	size_t atoms;
} MemChunkNodeInfo;

struct _MemChunk {
	size_t atomsize;
	size_t atomcount;
	size_t blocksize;
	GPtrArray *blocks;
	gboolean autoclean;
	MemChunkFreeNode *free;
};


static int
tree_compare (MemChunkNodeInfo *a, MemChunkNodeInfo *b)
{
	return a->block - b->block;
}

static int
tree_search (MemChunkNodeInfo *info, unsigned char *mem)
{
	if (info->block <= mem) {
		if (mem < info->block + info->blocksize)
			return 0;
		
		return 1;
	}
	
	return -1;
}

MemChunk *
memchunk_new (size_t atomsize, size_t atomcount, gboolean autoclean)
{
	MemChunk *chunk;
	
	/* each atom has to be at least the size of a MemChunkFreeNode
           for this to work */
	atomsize = MAX (atomsize, sizeof (MemChunkFreeNode));
	
	chunk = g_malloc (sizeof (MemChunk));
	chunk->atomsize = atomsize;
	chunk->atomcount = atomcount;
	chunk->blocksize = atomsize * atomcount;
	chunk->autoclean = autoclean;
	chunk->blocks = g_ptr_array_new ();
	chunk->free = NULL;
	
	return chunk;
}


void *
memchunk_alloc (MemChunk *memchunk)
{
	MemChunkFreeNode *node;
	char *block;
	
	if (memchunk->free) {
		node = memchunk->free;
		node->atoms--;
		if (node->atoms > 0)
			return (void *) node + (node->atoms * memchunk->atomsize);
		
		memchunk->free = node->next;
		
		return (void *) node;
	} else {
		block = g_malloc (memchunk->blocksize);
		g_ptr_array_add (memchunk->blocks, block);
		node = (MemChunkFreeNode *) (block + memchunk->atomsize);
		node->next = NULL;
		node->atoms = memchunk->atomcount - 1;
		memchunk->free = node;
		
		return (void *) block;
	}
}


void *
memchunk_alloc0 (MemChunk *memchunk)
{
	void *mem;
	
	mem = memchunk_alloc (memchunk);
	memset (mem, 0, memchunk->atomsize);
	
	return mem;
}


void
memchunk_free (MemChunk *memchunk, void *mem)
{
	MemChunkFreeNode *node;
	
	node = (MemChunkFreeNode *) mem;
	node->next = memchunk->free;
	node->atoms = 1;
	memchunk->free = node;
	
	/* yuck, this'll be slow... */
	if (memchunk->autoclean)
		memchunk_clean (memchunk);
}


void
memchunk_reset (MemChunk *memchunk)
{
	MemChunkFreeNode *node, *next = NULL;
	int i;
	
	for (i = 0; i < memchunk->blocks->len; i++) {
		node = memchunk->blocks->pdata[i];
		node->atoms = memchunk->atomcount;
		node->next = next;
		next = node;
	}
	
	memchunk->free = next;
}

void
memchunk_clean (MemChunk *memchunk)
{
	MemChunkNodeInfo *info, *next = NULL;
	MemChunkFreeNode *node;
	GTree *tree;
	int i;
	
	node = memchunk->free;
	
	if (memchunk->blocks->len == 0 || node == NULL)
		return;
	
	tree = g_tree_new ((GCompareFunc) tree_compare);
	for (i = 0; i < memchunk->blocks->len; i++) {
		info = alloca (sizeof (MemChunkNodeInfo));
		info->next = next;
		info->block = memchunk->blocks->pdata[i];
		info->blocksize = memchunk->blocksize;
		info->atoms = 0;
		
		next = info;
		
		g_tree_insert (tree, info, info);
	}
	
	while (node) {
		info = g_tree_search (tree, (GSearchFunc) tree_search, node);
		if (info)
			info->atoms += node->atoms;
		
		node = node->next;
	}
	
	info = next;
	while (info) {
		if (info->atoms == memchunk->atomcount) {
			g_ptr_array_remove_fast (memchunk->blocks, info->block);
			g_free (info->block);
		}
		
		info = info->next;
	}
}

void
memchunk_destroy (MemChunk *memchunk)
{
	int i;
	
	for (i = 0; i < memchunk->blocks->len; i++)
		g_free (memchunk->blocks->pdata[i]);
	
	g_ptr_array_free (memchunk->blocks, TRUE);
	
	g_free (memchunk);
}
