/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2003 Ximian, Inc. (www.ximian.com)
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

#include "cache.h"


Cache *
cache_new (CacheNodeExpireFunc expire, CacheNodeFreeFunc free_node, unsigned int node_size, unsigned int max_size)
{
	Cache *cache;
	
	cache = g_new (Cache, 1);
	list_init (&cache->list);
	cache->expire = expire;
	cache->free_node = free_node;
	cache->node_hash = g_hash_table_new (g_str_hash, g_str_equal);
	cache->node_chunks = memchunk_new (node_size, max_size, FALSE);
	cache->max_size = max_size;
	cache->size = 0;
	
	return cache;
}


static void
cache_node_free (CacheNode *node)
{
	Cache *cache;
	
	cache = node->cache;
	
	cache->free_node (node);
	g_free (node->key);
	memchunk_free (cache->node_chunks, node);
}


static void
cache_node_foreach_cb (gpointer key, gpointer value, gpointer user_data)
{
	cache_node_free ((CacheNode *) value);
}

void
cache_free (Cache *cache)
{
	g_hash_table_foreach (cache->node_hash, cache_node_foreach_cb, NULL);
	g_hash_table_destroy (cache->node_hash);
	memchunk_destroy (cache->node_chunks);
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
			g_hash_table_remove (cache->node_hash, ((CacheNode *) node)->key);
			list_node_unlink (node);
			cache_node_free ((CacheNode *) node);
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
	
	node = memchunk_alloc (cache->node_chunks);
	node->key = g_strdup (key);
	node->cache = cache;
	
	g_hash_table_insert (cache->node_hash, node->key, node);
	list_prepend_node (&cache->list, (ListNode *) node);
	
	return node;
}

CacheNode *
cache_node_lookup (Cache *cache, const char *key, gboolean use)
{
	CacheNode *node;
	
	node = g_hash_table_lookup (cache->node_hash, key);
	if (node && use) {
		list_node_unlink ((ListNode *) node);
		list_prepend_node (&cache->list, (ListNode *) node);
	}
	
	return node;
}

void
cache_node_expire (CacheNode *node)
{
	Cache *cache;
	
	cache = node->cache;
	g_hash_table_remove (cache->node_hash, node->key);
	list_node_unlink ((ListNode *) node);
	cache_node_free (node);
	cache->size--;
}
