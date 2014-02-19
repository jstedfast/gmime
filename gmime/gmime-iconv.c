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

#include <glib.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>

#include "gmime-charset.h"
#include "gmime-iconv.h"
#include "cache.h"

#ifdef ENABLE_WARNINGS
#define w(x) x
#else
#define w(x)
#endif /* ENABLE_WARNINGS */


/**
 * SECTION: gmime-iconv
 * @title: gmime-iconv
 * @short_description: Low-level routines for converting text from one charset to another
 * @see_also:
 *
 * These functions are wrappers around the system iconv(3)
 * routines. The purpose of these wrappers are two-fold:
 *
 * 1. Cache iconv_t descriptors for you in order to optimize
 * opening/closing many descriptors frequently
 *
 * and
 *
 * 2. To use the appropriate system charset alias for the MIME charset
 * names given as arguments.
 **/


#define ICONV_CACHE_SIZE   (16)

typedef struct {
	CacheNode node;
	guint32 refcount : 31;
	guint32 used : 1;
	iconv_t cd;
} IconvCacheNode;


static Cache *iconv_cache = NULL;
static GHashTable *iconv_open_hash = NULL;

#ifdef GMIME_ICONV_DEBUG
static int cache_misses = 0;
static int shutdown = 0;
#define d(x) x
#else
#define d(x)
#endif /* GMIME_ICONV_DEBUG */

#ifdef G_THREADS_ENABLED
extern void _g_mime_iconv_cache_unlock (void);
extern void _g_mime_iconv_cache_lock (void);
#define ICONV_CACHE_UNLOCK() _g_mime_iconv_cache_unlock ()
#define ICONV_CACHE_LOCK()   _g_mime_iconv_cache_lock ()
#else
#define ICONV_CACHE_UNLOCK()
#define ICONV_CACHE_LOCK()
#endif /* G_THREADS_ENABLED */


/* caller *must* hold the iconv_cache_lock to call any of the following functions */


/**
 * iconv_cache_node_new: (skip)
 * @key: cache key
 * @cd: iconv descriptor
 *
 * Creates a new cache node, inserts it into the cache and increments
 * the cache size.
 *
 * Returns: a pointer to the newly allocated cache node.
 **/
static IconvCacheNode *
iconv_cache_node_new (const char *key, iconv_t cd)
{
	IconvCacheNode *node;
	
#ifdef GMIME_ICONV_DEBUG
	cache_misses++;
#endif
	
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
iconv_open_node_free (gpointer key, gpointer value, gpointer user_data)
{
	iconv_t cd = (iconv_t) key;
	IconvCacheNode *node;
	
	node = (IconvCacheNode *) cache_node_lookup (iconv_cache, value, FALSE);
	g_assert (node);
	
	if (cd != node->cd) {
		node->refcount--;
		iconv_close (cd);
	}
}


/**
 * g_mime_iconv_shutdown:
 *
 * Frees internal iconv caches created in g_mime_iconv_init().
 *
 * Note: this function is called for you by g_mime_shutdown().
 **/
void
g_mime_iconv_shutdown (void)
{
	if (!iconv_cache)
		return;
	
#ifdef GMIME_ICONV_DEBUG
	fprintf (stderr, "There were %d iconv cache misses\n", cache_misses);
	fprintf (stderr, "The following %d iconv cache buckets are still open:\n", iconv_cache->size);
	shutdown = 1;
#endif
	
	g_hash_table_foreach (iconv_open_hash, iconv_open_node_free, NULL);
	g_hash_table_destroy (iconv_open_hash);
	iconv_open_hash = NULL;
	
	cache_free (iconv_cache);
	iconv_cache = NULL;
}


/**
 * g_mime_iconv_init:
 *
 * Initialize GMime's iconv cache. This *MUST* be called before any
 * gmime-iconv interfaces will work correctly.
 *
 * Note: this function is called for you by g_mime_init().
 **/
void
g_mime_iconv_init (void)
{
	if (iconv_cache)
		return;
	
	g_mime_charset_map_init ();
	
	iconv_open_hash = g_hash_table_new (g_direct_hash, g_direct_equal);
	iconv_cache = cache_new (iconv_cache_node_expire, iconv_cache_node_free,
				 sizeof (IconvCacheNode), ICONV_CACHE_SIZE);
}


/**
 * g_mime_iconv_open: (skip)
 * @to: charset to convert to
 * @from: charset to convert from
 *
 * Allocates a coversion descriptor suitable for converting byte
 * sequences from charset @from to charset @to. The resulting
 * descriptor can be used with iconv() (or the g_mime_iconv() wrapper) any
 * number of times until closed using g_mime_iconv_close().
 *
 * See the manual page for iconv_open(3) for further details.
 *
 * Returns: a new conversion descriptor for use with g_mime_iconv() on
 * success or (iconv_t) %-1 on fail as well as setting an appropriate
 * errno value.
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
	
	if (!g_ascii_strcasecmp (from, "x-unknown"))
		from = g_mime_locale_charset ();
	
	from = g_mime_charset_iconv_name (from);
	to = g_mime_charset_iconv_name (to);
	key = g_alloca (strlen (from) + strlen (to) + 2);
	sprintf (key, "%s:%s", from, to);
	
	ICONV_CACHE_LOCK ();
	
	if ((node = (IconvCacheNode *) cache_node_lookup (iconv_cache, key, TRUE))) {
		if (node->used) {
			if ((cd = iconv_open (to, from)) == (iconv_t) -1)
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
		if ((cd = iconv_open (to, from)) == (iconv_t) -1)
			goto exception;
		
		node = iconv_cache_node_new (key, cd);
	}
	
	g_hash_table_insert (iconv_open_hash, cd, ((CacheNode *) node)->key);
	
	ICONV_CACHE_UNLOCK ();
	
	return cd;
	
 exception:
	
	ICONV_CACHE_UNLOCK ();
	
#if w(!)0
	if (errno == EINVAL)
		g_warning ("Conversion from '%s' to '%s' is not supported", from, to);
	else
		g_warning ("Could not open converter from '%s' to '%s': %s",
			   from, to, strerror (errno));
#endif
	
	return cd;
}


/**
 * g_mime_iconv_close: (skip)
 * @cd: iconv conversion descriptor
 *
 * Closes the iconv descriptor @cd.
 *
 * See the manual page for iconv_close(3) for further details.
 *
 * Returns: %0 on success or %-1 on fail as well as setting an
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
	
	if ((key = g_hash_table_lookup (iconv_open_hash, cd))) {
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
		
		d(g_warning ("This iconv context wasn't opened using g_mime_iconv_open()"));
		
		return iconv_close (cd);
	}
	
	ICONV_CACHE_UNLOCK ();
	
	return 0;
}
