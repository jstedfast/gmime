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

#ifdef _REENTRANT
#include <pthread.h>
#endif

#include "gmime-charset.h"
#include "gmime-iconv.h"
#include "memchunk.h"


#define ICONV_CACHE_SIZE   (16)

struct _iconv_cache_bucket {
	struct _iconv_cache_bucket *next;
	struct _iconv_cache_bucket *prev;
	guint32 refcount;
	gboolean used;
	iconv_t cd;
	char *key;
};


static MemChunk *cache_chunk;
static struct _iconv_cache_bucket *iconv_cache_buckets;
static GHashTable *iconv_cache;
static GHashTable *iconv_open_hash;
static unsigned int iconv_cache_size = 0;
#ifdef _REENTRANT
static pthread_mutex_t iconv_cache_lock = PTHREAD_MUTEX_INITIALIZER;
#define ICONV_CACHE_LOCK()   pthread_mutex_lock (&iconv_cache_lock)
#define ICONV_CACHE_UNLOCK() pthread_mutex_unlock (&iconv_cache_lock)
#else
#define ICONV_CACHE_LOCK()
#define ICONV_CACHE_UNLOCK()
#endif /* _REENTRANT */


/* caller *must* hold the iconv_cache_lock to call any of the following functions */


/**
 * iconv_cache_bucket_new:
 * @key: cache key
 * @cd: iconv descriptor
 *
 * Creates a new cache bucket, inserts it into the cache and
 * increments the cache size.
 *
 * Returns a pointer to the newly allocated cache bucket.
 **/
static struct _iconv_cache_bucket *
iconv_cache_bucket_new (const char *key, iconv_t cd)
{
	struct _iconv_cache_bucket *bucket;
	
	bucket = memchunk_alloc (cache_chunk);
	bucket->next = NULL;
	bucket->prev = NULL;
	bucket->key = g_strdup (key);
	bucket->refcount = 1;
	bucket->used = TRUE;
	bucket->cd = cd;
	
	g_hash_table_insert (iconv_cache, bucket->key, bucket);
	
	/* FIXME: Since iconv_cache_expire_unused() traverses the list
           from head to tail, perhaps it might be better to append new
           nodes rather than prepending? This way older cache buckets
           expire first? */
	bucket->next = iconv_cache_buckets;
	iconv_cache_buckets = bucket;
	
	iconv_cache_size++;
	
	return bucket;
}


/**
 * iconv_cache_bucket_expire:
 * @bucket: cache bucket
 *
 * Expires a single cache bucket @bucket. This should only ever be
 * called on a bucket that currently has no used iconv descriptors
 * open.
 **/
static void
iconv_cache_bucket_expire (struct _iconv_cache_bucket *bucket)
{
	g_hash_table_remove (iconv_cache, bucket->key);
	
	if (bucket->prev) {
		bucket->prev->next = bucket->next;
		if (bucket->next)
			bucket->next->prev = bucket->prev;
	} else {
		iconv_cache_buckets = bucket->next;
		if (bucket->next)
			bucket->next->prev = NULL;
	}
	
	g_free (bucket->key);
	iconv_close (bucket->cd);
	memchunk_free (cache_chunk, bucket);
	
	iconv_cache_size--;
}


/**
 * iconv_cache_expire_unused:
 *
 * Expires as many unused cache buckets as it needs to in order to get
 * the total number of buckets < ICONV_CACHE_SIZE.
 **/
static void
iconv_cache_expire_unused (void)
{
	struct _iconv_cache_bucket *bucket, *next;
	
	bucket = iconv_cache_buckets;
	while (bucket && iconv_cache_size >= ICONV_CACHE_SIZE) {
		next = bucket->next;
		
		if (bucket->refcount == 0)
			iconv_cache_bucket_expire (bucket);
		
		bucket = next;
	}
}


static void
g_mime_iconv_shutdown (void)
{
	struct _iconv_cache_bucket *bucket, *next;
	
	bucket = iconv_cache_buckets;
	while (bucket) {
		next = bucket->next;
		
		g_free (bucket->key);
		iconv_close (bucket->cd);
		memchunk_free (cache_chunk, bucket);
		
		bucket = next;
	}
	
	g_hash_table_destroy (iconv_cache);
	g_hash_table_destroy (iconv_open_hash);
	
	memchunk_destroy (cache_chunk);
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
	
	iconv_cache_buckets = NULL;
	iconv_cache = g_hash_table_new (g_str_hash, g_str_equal);
	iconv_open_hash = g_hash_table_new (g_direct_hash, g_direct_equal);
	
	cache_chunk = memchunk_new (sizeof (struct _iconv_cache_bucket),
				    ICONV_CACHE_SIZE, FALSE);
	
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
	struct _iconv_cache_bucket *bucket;
	iconv_t cd;
	char *key;
	
	if (from == NULL || to == NULL) {
		errno = EINVAL;
		return (iconv_t) -1;
	}
	
	if (!g_strcasecmp (from, "x-unknown"))
		from = g_mime_charset_locale_name ();
	
	from = g_mime_charset_name (from);
	to = g_mime_charset_name (to);
	key = g_alloca (strlen (from) + strlen (to) + 2);
	sprintf (key, "%s:%s", from, to);
	
	ICONV_CACHE_LOCK ();
	
	bucket = g_hash_table_lookup (iconv_cache, key);
	if (bucket) {
		if (bucket->used) {
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
			
			cd = bucket->cd;
			bucket->used = TRUE;
			
			/* reset the descriptor */
			iconv (cd, NULL, &inleft, &outbuf, &outleft);
		}
		
		bucket->refcount++;
	} else {
		cd = iconv_open (to, from);
		if (cd == (iconv_t) -1)
			goto exception;
		
		iconv_cache_expire_unused ();
		
		bucket = iconv_cache_bucket_new (key, cd);
	}
	
	g_hash_table_insert (iconv_open_hash, cd, bucket->key);
	
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
	struct _iconv_cache_bucket *bucket;
	const char *key;
	
	if (cd == (iconv_t) -1)
		return 0;
	
	ICONV_CACHE_LOCK ();
	
	key = g_hash_table_lookup (iconv_open_hash, cd);
	if (key) {
		g_hash_table_remove (iconv_open_hash, cd);
		
		bucket = g_hash_table_lookup (iconv_cache, key);
		g_assert (bucket);
		
		bucket->refcount--;
		
		if (cd == bucket->cd)
			bucket->used = FALSE;
		else
			iconv_close (cd);
		
		if (!bucket->refcount && iconv_cache_size > ICONV_CACHE_SIZE) {
			/* expire this cache bucket */
			iconv_cache_bucket_expire (bucket);
		}
	} else {
		ICONV_CACHE_UNLOCK ();
		
		g_warning ("This iconv context wasn't opened using g_mime_iconv_open()!");
		
		return iconv_close (cd);
	}
	
	ICONV_CACHE_UNLOCK ();
	
	return 0;
}
