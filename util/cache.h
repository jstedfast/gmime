/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 1999-2006 Jeffrey Stedfast
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
 */


#ifndef __CACHE_H__
#define __CACHE_H__

#include <glib.h>

#include <util/list.h>
#include <util/memchunk.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

typedef struct _Cache Cache;

typedef struct {
	ListNode node;
	Cache *cache;
	char *key;
} CacheNode;

typedef gboolean (*CacheNodeExpireFunc) (Cache *cache, CacheNode *node);
typedef void (*CacheNodeFreeFunc) (CacheNode *node);

struct _Cache {
	List list;
	unsigned int size;
	unsigned int max_size;
	MemChunk *node_chunks;
	GHashTable *node_hash;
	CacheNodeExpireFunc expire;
	CacheNodeFreeFunc free_node;
};


Cache *cache_new (CacheNodeExpireFunc expire, CacheNodeFreeFunc free_node,
		  unsigned int bucket_size, unsigned int max_cache_size);

void cache_free (Cache *cache);

CacheNode *cache_node_insert (Cache *cache, const char *key);
CacheNode *cache_node_lookup (Cache *cache, const char *key, gboolean use);

void cache_expire_unused (Cache *cache);
void cache_node_expire (CacheNode *node);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __CACHE_H__ */
