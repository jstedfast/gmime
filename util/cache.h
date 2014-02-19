/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2014 Jeffrey Stedfast
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


#ifndef __CACHE_H__
#define __CACHE_H__

#include <glib.h>

#include <sys/types.h>

#include <util/list.h>

G_BEGIN_DECLS

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
	size_t size;
	size_t max_size;
	size_t node_size;
	GHashTable *node_hash;
	CacheNodeExpireFunc expire;
	CacheNodeFreeFunc free_node;
};


G_GNUC_INTERNAL Cache *cache_new (CacheNodeExpireFunc expire, CacheNodeFreeFunc free_node,
				  size_t bucket_size, size_t max_cache_size);

G_GNUC_INTERNAL void cache_free (Cache *cache);

G_GNUC_INTERNAL CacheNode *cache_node_insert (Cache *cache, const char *key);
G_GNUC_INTERNAL CacheNode *cache_node_lookup (Cache *cache, const char *key, gboolean use);

G_GNUC_INTERNAL void cache_expire_unused (Cache *cache);
G_GNUC_INTERNAL void cache_node_expire (CacheNode *node);

G_END_DECLS

#endif /* __CACHE_H__ */
