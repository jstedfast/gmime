/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2002 Ximain, Inc. (www.ximian.com)
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

#include <glib.h>
#include <errno.h>

#ifdef HAVE_ALLOCA_H
#include <alloca.h>
#endif

#include "gmime-charset.h"
#include "gmime-iconv.h"
#include "memchunk.h"


#define ICONV_CACHE_SIZE   (10)


struct _iconv_node {
	struct _iconv_node *next;
	struct _iconv_node *prev;
	
	struct _iconv_cache_bucket *bucket;
	
	gboolean used;
	iconv_t cd;
};

struct _iconv_cache_bucket {
	struct _iconv_cache_bucket *next;
	struct _iconv_cache_bucket *prev;
	
	struct _iconv_node *unused;
	struct _iconv_node *used;
	
	char *key;
};



static struct _iconv_cache_bucket *iconv_cache_buckets;
static struct _iconv_cache_bucket *iconv_cache_tail;
static unsigned int iconv_cache_size = 0;
static GHashTable *iconv_cache;
static GHashTable *iconv_open_hash;
static MemChunk *node_chunk;


static struct _iconv_node *
iconv_node_new (struct _iconv_cache_bucket *bucket)
{
	struct _iconv_node *node;
	
	node = memchunk_alloc (node_chunk);
	node->next = NULL;
	node->prev = NULL;
	node->bucket = bucket;
	node->used = FALSE;
	node->cd = (iconv_t) -1;
}

static void
iconv_node_set_used (struct _iconv_node *node, gboolean used)
{
	if (node->used == used)
		return;
	
	node->used = used;
	
	if (used) {
		/* this should be a lone unused node, so prepend it to the used list */
		node->next = node->bucket->used;
		node->bucket->used = node;
		
		/* add to the open hash */
		g_hash_table_insert (iconv_open_hash, node->cd, node);
	} else {
		/* this could be anywhere in the used node list... */
		if (node->prev) {
			node->prev->next = node->next;
			if (node->next)
				node->next->prev = node->prev;
		} else {
			node->bucket->used = node->next;
			if (node->next)
				node->next->prev = NULL;
		}
		
		/* remove from the iconv open hash */
		g_hash_table_remove (iconv_open_hash, node->cd);
	}
}

static void
iconv_node_destroy (struct _iconv_node *node)
{
	if (node) {
		if (node->cd != (iconv_t) -1)
			iconv_close (node->cd);
		
		memchunk_free (node_chunk, node);
	}
}



static struct _iconv_cache_bucket *
cache_bucket_new (const char *key)
{
	struct _iconv_cache_bucket *bucket;
	
	bucket = g_new (struct _iconv_cache_bucket, 1);
	bucket->next = NULL;
	bucket->prev = NULL;
	bucket->unused = NULL;
	bucket->used = NULL;
	bucket->key = g_strdup (key);
	
	return bucket;
}

static void
cache_bucket_add (struct _iconv_cache_bucket *bucket)
{
	if (iconv_cache_buckets)
		bucket->prev = iconv_cache_tail;
	
	iconv_cache_tail->next = bucket;
	iconv_cache_tail = bucket;
	
	g_hash_table_insert (iconv_cache, bucket->key, bucket);
}

static void
cache_bucket_add_node (struct _iconv_cache_bucket *bucket, struct _iconv_node *node)
{
	node = bucket->unused;
	bucket->unused = node;
}

static void
cache_bucket_remove (struct _iconv_cache_bucket *bucket)
{
	if (bucket->prev) {
		bucket->prev->next = bucket->next;
		if (bucket->next)
			bucket->next->prev = bucket->prev;
		else
			iconv_cache_tail = bucket->prev;
	} else {
		bucket->next->prev = NULL;
		iconv_cache_buckets = bucket->next;
		if (!iconv_cache_buckets)
			iconv_cache_tail = (struct _iconv_cache_bucket *) &iconv_cache_buckets;
	}
}

static struct _iconv_node *
cache_bucket_get_first_unused (struct _iconv_cache_bucket *bucket)
{
	struct _iconv_node *node = NULL;
	
	if (bucket->unused) {
		node = bucket->unused;
		bucket->unused = node->next;
		node->next = NULL;
	}
	
	return node;
}

static void
cache_bucket_destroy (struct _iconv_cache_bucket *bucket)
{
	struct _iconv_node *node, *next;
	
	node = bucket->unused;
	while (node) {
		next = node->next;
		iconv_node_destroy (node);
		node = next;
	}
	
	node = bucket->used;
	while (node) {
		next = node->next;
		iconv_node_destroy (node);
		node = next;
	}
	
	g_free (bucket->key);
	g_free (bucket);
}

static void
cache_bucket_flush_unused (struct _iconv_cache_bucket *bucket)
{
	struct _iconv_node *node, *next;
	
	node = bucket->unused;
	while (node && iconv_cache_size >= ICONV_CACHE_SIZE) {
		next = node->next;
		iconv_node_destroy (node);
		iconv_cache_size--;
		node = next;
	}
	
	bucket->unused = node;
	
	if (!bucket->unused && !bucket->used) {
		/* expire this cache bucket... */
		cache_bucket_remove (bucket);
	}
}



static void
g_mime_iconv_shutdown (void)
{
	struct _iconv_cache_bucket *bucket, *next;
	
	bucket = iconv_cache_buckets;
	while (bucket) {
		next = bucket->next;
		cache_bucket_destroy (bucket);
		bucket = next;
	}
	
	g_hash_table_destroy (iconv_cache);
	g_hash_table_destroy (iconv_open_hash);
	
	memchunk_destroy (node_chunk);
}


/**
 * g_mime_iconv_init:
 *
 * Initialize GMime's iconv cache. This *MUST* be called before any
 * gmime-iconv interfaces will work correctly.
 **/
void
g_mime_iconv_init (void)
{
	static gboolean initialized = FALSE;
	
	if (initialized)
		return;
	
	g_mime_charset_init ();
	
	node_chunk = memchunk_new (sizeof (struct _iconv_node),
				   ICONV_CACHE_SIZE, FALSE);
	
	iconv_cache_buckets = NULL;
	iconv_cache_tail = (struct _iconv_cache_bucket *) &iconv_cache_buckets;
	iconv_cache = g_hash_table_new (g_str_hash, g_str_equal);
	iconv_open_hash = g_hash_table_new (g_direct_hash, g_direct_equal);
	
	g_atexit (g_mime_iconv_shutdown);
	
	initialized = TRUE;
}


/**
 * g_mime_iconv_open:
 * @to: charset to convert to
 * @from: charset to convert from
 *
 * Allocates a coversion descriptor suitable for converting byte
 * sequences from charset @from to charset @to. The resulting
 * descriptor can be used with iconv (or the g_mime_iconv wrapper) any
 * number of times until closed using g_mime_iconv_close.
 *
 * Returns a new conversion descriptor for use with iconv on success
 * or (iconv_t) -1 on fail as well as setting an appropriate errno
 * value.
 **/
iconv_t
g_mime_iconv_open (const char *to, const char *from)
{
	struct _iconv_cache_bucket *bucket, *prev;
	struct _iconv_node *node;
	iconv_t cd;
	char *key;
	
	if (from == NULL || to == NULL) {
		errno = EINVAL;
		return (iconv_t) -1;
	}
	
	from = g_mime_charset_name (from);
	to = g_mime_charset_name (to);
	key = alloca (strlen (from) + strlen (to) + 2);
	sprintf (key, "%s:%s", from, to);
	
	bucket = g_hash_table_lookup (iconv_cache, key);
	if (bucket) {
		node = cache_bucket_get_first_unused (bucket);
	} else {
		/* make room for another cache bucket */
		bucket = iconv_cache_tail;
		while (bucket && iconv_cache_size >= ICONV_CACHE_SIZE) {
			prev = bucket->prev;
			cache_bucket_flush_unused (bucket);
			bucket = prev;
		}
		
		bucket = cache_bucket_new (key);
		cache_bucket_add (bucket);
		
		node = NULL;
	}
	
	if (node == NULL) {
		node = iconv_node_new (bucket);
		
		/* make room for this node */
		bucket = iconv_cache_tail;
		while (bucket && iconv_cache_size >= ICONV_CACHE_SIZE) {
			prev = bucket->prev;
			cache_bucket_flush_unused (bucket);
			bucket = prev;
		}
		
		cd = iconv_open (from, to);
		if (cd == (iconv_t) -1) {
			iconv_node_destroy (node);
			return cd;
		}
		
		node->cd = cd;
		
		cache_bucket_add_node (bucket, node);
	} else {
		cd = node->cd;
		
		/* reset the iconv descriptor */
		iconv (cd, NULL, NULL, NULL, NULL);
	}
	
	iconv_node_set_used (node, TRUE);
	
	return cd;
}


/**
 * g_mime_iconv_close:
 * @cd: iconv conversion descriptor
 *
 * Closes the iconv descriptor @cd.
 *
 * Returns 0 on success or -1 on fail as well as setting an
 * appropriate errno value.
 **/
int
g_mime_iconv_close (iconv_t cd)
{
	struct _iconv_cache_bucket *bucket;
	struct _iconv_node *node;
	
	if (cd == (iconv_t) -1)
		return 0;
	
	node = g_hash_table_lookup (iconv_open_hash, cd);
	if (node) {
		iconv_node_set_used (node, FALSE);
	} else {
		g_warning ("This iconv context wasn't opened using g_mime_iconv_open()!");
		return iconv_close (cd);
	}
	
	return 0;
}
