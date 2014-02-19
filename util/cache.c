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


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "cache.h"


static void
cache_node_free (gpointer node_data)
{
	CacheNode *node = (CacheNode *) node_data;
	Cache *cache = node->cache;
	
	cache->free_node (node);
	g_free (node->key);
	
	g_slice_free1 (cache->node_size, node);
}


Cache *
cache_new (CacheNodeExpireFunc expire, CacheNodeFreeFunc free_node, size_t node_size, size_t max_size)
{
	Cache *cache;
	
	cache = g_new (Cache, 1);
	list_init (&cache->list);
	cache->expire = expire;
	cache->free_node = free_node;
	cache->node_hash = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, cache_node_free);
	cache->node_size = node_size;
	cache->max_size = max_size;
	cache->size = 0;
	
	return cache;
}


void
cache_free (Cache *cache)
{
	g_hash_table_destroy (cache->node_hash);
	g_free (cache);
}


void
cache_expire_unused (Cache *cache)
{
	ListNode *node, *prev;
	
	node = cache->list.tailpred;
	while (node->prev && cache->size > cache->max_size) {
		prev = node->prev;
		if (cache->expire (cache, (CacheNode *) node)) {
			list_unlink (node);
			g_hash_table_remove (cache->node_hash, ((CacheNode *) node)->key);
			cache->size--;
		}
		node = prev;
	}
}

CacheNode *
cache_node_insert (Cache *cache, const char *key)
{
	CacheNode *node;
	
	cache->size++;
	
	if (cache->size > cache->max_size)
		cache_expire_unused (cache);
	
	node = g_slice_alloc (cache->node_size);
	node->key = g_strdup (key);
	node->cache = cache;
	
	g_hash_table_insert (cache->node_hash, node->key, node);
	list_prepend (&cache->list, (ListNode *) node);
	
	return node;
}

CacheNode *
cache_node_lookup (Cache *cache, const char *key, gboolean use)
{
	CacheNode *node;
	
	node = g_hash_table_lookup (cache->node_hash, key);
	if (node && use) {
		list_unlink ((ListNode *) node);
		list_prepend (&cache->list, (ListNode *) node);
	}
	
	return node;
}

void
cache_node_expire (CacheNode *node)
{
	Cache *cache;
	
	cache = node->cache;
	list_unlink ((ListNode *) node);
	g_hash_table_remove (cache->node_hash, node->key);
	cache->size--;
}
