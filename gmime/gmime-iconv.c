/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2002 Ximian, Inc. (www.ximian.com)
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
#include <string.h>
#include <stdio.h>

#include "gmime-charset.h"
#include "gmime-iconv.h"
#include "cache.h"


#define GMIME_ICONV_DEBUG

#define ICONV_CACHE_SIZE   (16)

typedef struct {
	CacheNode node;
	guint32 refcount : 31;
	guint32 used : 1;
	iconv_t cd;
} IconvCacheNode;


static Cache *iconv_cache;
static GHashTable *iconv_open_hash;

#ifdef GMIME_ICONV_DEBUG
static int cache_misses = 0;
static int shutdown = 0;
#endif

#ifdef G_THREADS_ENABLED
static GStaticMutex iconv_cache_lock = G_STATIC_MUTEX_INIT;
#define ICONV_CACHE_LOCK()   g_static_mutex_lock (&iconv_cache_lock)
#define ICONV_CACHE_UNLOCK() g_static_mutex_unlock (&iconv_cache_lock)
#else
#define ICONV_CACHE_LOCK()
#define ICONV_CACHE_UNLOCK()
#endif /* G_THREADS_ENABLED */


/* caller *must* hold the iconv_cache_lock to call any of the following functions */


/**
 * iconv_cache_node_new:
 * @key: cache key
 * @cd: iconv descriptor
 *
 * Creates a new cache node, inserts it into the cache and increments
 * the cache size.
 *
 * Returns a pointer to the newly allocated cache node.
 **/
static IconvCacheNode *
iconv_cache_node_new (const char *key, iconv_t cd)
{
	IconvCacheNode *node;
	
	cache_misses++;
	
	node = (IconvCacheNode *) cache_node_insert (iconv_cache, key);
	node->refcount = 1;
	node->used = TRUE;
	node->cd = cd;
	
	return node;
}


static void
iconv_cache_node_free (CacheNode *node)
{
	IconvCacheNode *inode = (IconvCacheNode *) node;
	
#ifdef GMIME_ICONV_DEBUG
	if (shutdown) {
		fprintf (stderr, "%s: open=%d; used=%s\n", node->key,
			 inode->refcount, inode->used ? "yes" : "no");
	}
#endif
	
	iconv_close (inode->cd);
}


/**
 * iconv_cache_node_expire:
 * @node: cache node
 *
 * Decides whether or not a cache node should be expired.
 **/
static gboolean
iconv_cache_node_expire (Cache *cache, CacheNode *node)
{
	IconvCacheNode *inode = (IconvCacheNode *) node;
	
	if (inode->refcount == 0)
		return TRUE;
	
	return FALSE;
}


static void
g_mime_iconv_shutdown (void)
{
#ifdef GMIME_ICONV_DEBUG
	fprintf (stderr, "There were %d iconv cache misses\n", cache_misses);
	fprintf (stderr, "The following %d iconv cache buckets are still open:\n", iconv_cache->size);
	shutdown = 1;
#endif
	cache_free (iconv_cache);
	g_hash_table_destroy (iconv_open_hash);
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
	
	g_mime_charset_map_init ();
	
	iconv_open_hash = g_hash_table_new (g_direct_hash, g_direct_equal);
	iconv_cache = cache_new (iconv_cache_node_expire, iconv_cache_node_free,
				 sizeof (IconvCacheNode), ICONV_CACHE_SIZE);
	
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
	IconvCacheNode *node;
	iconv_t cd;
	char *key;
	
	if (from == NULL || to == NULL) {
		errno = EINVAL;
		return (iconv_t) -1;
	}
	
	if (!strcasecmp (from, "x-unknown"))
		from = g_mime_locale_charset ();
	
	from = g_mime_charset_iconv_name (from);
	to = g_mime_charset_iconv_name (to);
	key = g_alloca (strlen (from) + strlen (to) + 2);
	sprintf (key, "%s:%s", from, to);
	
	ICONV_CACHE_LOCK ();
	
	node = (IconvCacheNode *) cache_node_lookup (iconv_cache, key, TRUE);
	if (node) {
		if (node->used) {
			cd = iconv_open (to, from);
			if (cd == (iconv_t) -1)
				goto exception;
		} else {
			/* Apparently iconv on Solaris <= 7 segfaults if you pass in
			 * NULL for anything but inbuf; work around that. (NULL outbuf
			 * or NULL *outbuf is allowed by Unix98.)
			 */
			size_t inleft = 0, outleft = 0;
			char *outbuf = NULL;
			
			cd = node->cd;
			node->used = TRUE;
			
			/* reset the descriptor */
			iconv (cd, NULL, &inleft, &outbuf, &outleft);
		}
		
		node->refcount++;
	} else {
		cd = iconv_open (to, from);
		if (cd == (iconv_t) -1)
			goto exception;
		
		node = iconv_cache_node_new (key, cd);
	}
	
	g_hash_table_insert (iconv_open_hash, cd, ((CacheNode *) node)->key);
	
	ICONV_CACHE_UNLOCK ();
	
	return cd;
	
 exception:
	
	ICONV_CACHE_UNLOCK ();
	
	if (errno == EINVAL)
		g_warning ("Conversion from '%s' to '%s' is not supported", from, to);
	else
		g_warning ("Could not open converter from '%s' to '%s': %s",
			   from, to, g_strerror (errno));
	
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
	IconvCacheNode *node;
	const char *key;
	
	if (cd == (iconv_t) -1)
		return 0;
	
	ICONV_CACHE_LOCK ();
	
	key = g_hash_table_lookup (iconv_open_hash, cd);
	if (key) {
		g_hash_table_remove (iconv_open_hash, cd);
		
		node = (IconvCacheNode *) cache_node_lookup (iconv_cache, key, FALSE);
		g_assert (node);
		
		if (iconv_cache->size > ICONV_CACHE_SIZE) {
			/* expire before unreffing this node so that it wont get uncached */
			cache_expire_unused (iconv_cache);
		}
		
		node->refcount--;
		
		if (cd == node->cd)
			node->used = FALSE;
		else
			iconv_close (cd);
	} else {
		ICONV_CACHE_UNLOCK ();
		
		g_warning ("This iconv context wasn't opened using g_mime_iconv_open()");
		
		return iconv_close (cd);
	}
	
	ICONV_CACHE_UNLOCK ();
	
	return 0;
}
