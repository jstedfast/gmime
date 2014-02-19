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

#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>

#include "gmime-multipart.h"
#include "gmime-utils.h"


#define d(x)


/**
 * SECTION: gmime-multipart
 * @title: GMimeMultipart
 * @short_description: MIME multiparts
 * @see_also:
 *
 * A #GMimeMultipart represents all multipart MIME container parts.
 **/


/* GObject class methods */
static void g_mime_multipart_class_init (GMimeMultipartClass *klass);
static void g_mime_multipart_init (GMimeMultipart *multipart, GMimeMultipartClass *klass);
static void g_mime_multipart_finalize (GObject *object);

/* GMimeObject class methods */
static ssize_t multipart_write_to_stream (GMimeObject *object, GMimeStream *stream);
static void multipart_encode (GMimeObject *object, GMimeEncodingConstraint constraint);

/* GMimeMultipart class methods */
static void multipart_clear (GMimeMultipart *multipart);
static void multipart_add (GMimeMultipart *multipart, GMimeObject *part);
static void multipart_insert (GMimeMultipart *multipart, int index, GMimeObject *part);
static gboolean multipart_remove (GMimeMultipart *multipart, GMimeObject *part);
static GMimeObject *multipart_remove_at (GMimeMultipart *multipart, int index);
static GMimeObject *multipart_get_part (GMimeMultipart *multipart, int index);
static gboolean multipart_contains (GMimeMultipart *multipart, GMimeObject *part);
static int multipart_index_of (GMimeMultipart *multipart, GMimeObject *part);
static int multipart_get_count (GMimeMultipart *multipart);
static void multipart_set_boundary (GMimeMultipart *multipart, const char *boundary);
static const char *multipart_get_boundary (GMimeMultipart *multipart);


static GMimeObjectClass *parent_class = NULL;


GType
g_mime_multipart_get_type (void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof (GMimeMultipartClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc) g_mime_multipart_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (GMimeMultipart),
			0,    /* n_preallocs */
			(GInstanceInitFunc) g_mime_multipart_init,
		};
		
		type = g_type_register_static (GMIME_TYPE_OBJECT, "GMimeMultipart", &info, 0);
	}
	
	return type;
}


static void
g_mime_multipart_class_init (GMimeMultipartClass *klass)
{
	GMimeObjectClass *object_class = GMIME_OBJECT_CLASS (klass);
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	
	parent_class = g_type_class_ref (GMIME_TYPE_OBJECT);
	
	gobject_class->finalize = g_mime_multipart_finalize;
	
	object_class->write_to_stream = multipart_write_to_stream;
	object_class->encode = multipart_encode;
	
	klass->add = multipart_add;
	klass->clear = multipart_clear;
	klass->insert = multipart_insert;
	klass->remove = multipart_remove;
	klass->remove_at = multipart_remove_at;
	klass->get_part = multipart_get_part;
	klass->contains = multipart_contains;
	klass->index_of = multipart_index_of;
	klass->get_count = multipart_get_count;
	klass->set_boundary = multipart_set_boundary;
	klass->get_boundary = multipart_get_boundary;
}

static void
g_mime_multipart_init (GMimeMultipart *multipart, GMimeMultipartClass *klass)
{
	multipart->children = g_ptr_array_new ();
	multipart->preface = NULL;
	multipart->postface = NULL;
}

static void
g_mime_multipart_finalize (GObject *object)
{
	GMimeMultipart *multipart = (GMimeMultipart *) object;
	guint i;
	
	g_free (multipart->preface);
	g_free (multipart->postface);
	
	for (i = 0; i < multipart->children->len; i++)
		g_object_unref (multipart->children->pdata[i]);
	
	g_ptr_array_free (multipart->children, TRUE);
	
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static ssize_t
multipart_write_to_stream (GMimeObject *object, GMimeStream *stream)
{
	GMimeMultipart *multipart = (GMimeMultipart *) object;
	ssize_t nwritten, total = 0;
	const char *boundary;
	GMimeObject *part;
	guint i;
	
	/* make sure a boundary is set unless we are writing out a raw
	 * header (in which case it should already be set... or if
	 * not, then it's a broken multipart and so we don't want to
	 * alter it or we'll completely break the output) */
	boundary = g_mime_object_get_content_type_parameter (object, "boundary");
	if (!boundary && !g_mime_header_list_get_stream (object->headers)) {
		g_mime_multipart_set_boundary (multipart, NULL);
		boundary = g_mime_object_get_content_type_parameter (object, "boundary");
	}
	
	/* write the content headers */
	if ((nwritten = g_mime_header_list_write_to_stream (object->headers, stream)) == -1)
		return -1;
	
	total += nwritten;
	
	/* write the preface */
	if (multipart->preface) {
		/* terminate the headers */
		if (g_mime_stream_write (stream, "\n", 1) == -1)
			return -1;
		
		total++;
		
		if ((nwritten = g_mime_stream_write_string (stream, multipart->preface)) == -1)
			return -1;
		
		total += nwritten;
	}
	
	for (i = 0; i < multipart->children->len; i++) {
		part = multipart->children->pdata[i];
		
		/* write the boundary */
		if ((nwritten = g_mime_stream_printf (stream, "\n--%s\n", boundary)) == -1)
			return -1;
		
		total += nwritten;
		
		/* write this part out */
		if ((nwritten = g_mime_object_write_to_stream (part, stream)) == -1)
			return -1;
		
		total += nwritten;
	}
	
	/* write the end-boundary (but only if a boundary is set) */
	if (boundary) {
		if ((nwritten = g_mime_stream_printf (stream, "\n--%s--\n", boundary)) == -1)
			return -1;
		
		total += nwritten;
	}
	
	/* write the postface */
	if (multipart->postface) {
		if ((nwritten = g_mime_stream_write_string (stream, multipart->postface)) == -1)
			return -1;
		
		total += nwritten;
	}
	
	return total;
}

static void
multipart_encode (GMimeObject *object, GMimeEncodingConstraint constraint)
{
	GMimeMultipart *multipart = (GMimeMultipart *) object;
	GMimeObject *subpart;
	int i;
	
	for (i = 0; i < g_mime_multipart_get_count (multipart); i++) {
		subpart = g_mime_multipart_get_part (multipart, i);
		g_mime_object_encode (subpart, constraint);
	}
}


/**
 * g_mime_multipart_new:
 *
 * Creates a new MIME multipart object with a default content-type of
 * multipart/mixed.
 *
 * Returns: an empty MIME multipart object with a default content-type of
 * multipart/mixed.
 **/
GMimeMultipart *
g_mime_multipart_new (void)
{
	GMimeContentType *content_type;
	GMimeMultipart *multipart;
	
	multipart = g_object_newv (GMIME_TYPE_MULTIPART, 0, NULL);
	
	content_type = g_mime_content_type_new ("multipart", "mixed");
	g_mime_object_set_content_type (GMIME_OBJECT (multipart), content_type);
	g_object_unref (content_type);
	
	return multipart;
}


/**
 * g_mime_multipart_new_with_subtype:
 * @subtype: content-type subtype
 *
 * Creates a new MIME multipart object with a content-type of
 * multipart/@subtype.
 *
 * Returns: an empty MIME multipart object with a content-type of
 * multipart/@subtype.
 **/
GMimeMultipart *
g_mime_multipart_new_with_subtype (const char *subtype)
{
	GMimeContentType *content_type;
	GMimeMultipart *multipart;
	
	multipart = g_object_newv (GMIME_TYPE_MULTIPART, 0, NULL);
	
	content_type = g_mime_content_type_new ("multipart", subtype ? subtype : "mixed");
	g_mime_object_set_content_type (GMIME_OBJECT (multipart), content_type);
	g_object_unref (content_type);
	
	return multipart;
}


/**
 * g_mime_multipart_set_preface:
 * @multipart: a #GMimeMultipart object
 * @preface: preface
 *
 * Sets the preface on the multipart.
 **/
void
g_mime_multipart_set_preface (GMimeMultipart *multipart, const char *preface)
{
	g_return_if_fail (GMIME_IS_MULTIPART (multipart));
	
	g_free (multipart->preface);
	multipart->preface = g_strdup (preface);
}


/**
 * g_mime_multipart_get_preface:
 * @multipart: a #GMimeMultipart object
 *
 * Gets the preface on the multipart.
 *
 * Returns: a pointer to the preface string on the multipart.
 **/
const char *
g_mime_multipart_get_preface (GMimeMultipart *multipart)
{
	g_return_val_if_fail (GMIME_IS_MULTIPART (multipart), NULL);
	
	return multipart->preface;
}


/**
 * g_mime_multipart_set_postface:
 * @multipart: a #GMimeMultipart object
 * @postface: postface
 *
 * Sets the postface on the multipart.
 **/
void
g_mime_multipart_set_postface (GMimeMultipart *multipart, const char *postface)
{
	g_return_if_fail (GMIME_IS_MULTIPART (multipart));
	
	g_free (multipart->postface);
	multipart->postface = g_strdup (postface);
}


/**
 * g_mime_multipart_get_postface:
 * @multipart: a #GMimeMultipart object
 *
 * Gets the postface on the multipart.
 *
 * Returns: a pointer to the postface string on the multipart.
 **/
const char *
g_mime_multipart_get_postface (GMimeMultipart *multipart)
{
	g_return_val_if_fail (GMIME_IS_MULTIPART (multipart), NULL);
	
	return multipart->postface;
}


static void
multipart_clear (GMimeMultipart *multipart)
{
	guint i;
	
	for (i = 0; i < multipart->children->len; i++)
		g_object_unref (multipart->children->pdata[i]);
	
	g_ptr_array_set_size (multipart->children, 0);
}


/**
 * g_mime_multipart_clear:
 * @multipart: a #GMimeMultipart object
 *
 * Removes all subparts from @multipart.
 **/
void
g_mime_multipart_clear (GMimeMultipart *multipart)
{
	g_return_if_fail (GMIME_IS_MULTIPART (multipart));
	
	GMIME_MULTIPART_GET_CLASS (multipart)->clear (multipart);
}


static void
multipart_add (GMimeMultipart *multipart, GMimeObject *part)
{
	g_ptr_array_add (multipart->children, part);
	g_object_ref (part);
}


/**
 * g_mime_multipart_add:
 * @multipart: a #GMimeMultipart object
 * @part: a #GMimeObject
 *
 * Adds a mime part to the multipart.
 **/
void
g_mime_multipart_add (GMimeMultipart *multipart, GMimeObject *part)
{
	g_return_if_fail (GMIME_IS_MULTIPART (multipart));
	g_return_if_fail (GMIME_IS_OBJECT (part));
	
	GMIME_MULTIPART_GET_CLASS (multipart)->add (multipart, part);
}


static void
ptr_array_insert (GPtrArray *array, guint index, gpointer object)
{
	unsigned char *dest, *src;
	guint n;
	
	if (index < array->len) {
		/* need to shift some items */
		g_ptr_array_set_size (array, array->len + 1);
		
		dest = ((unsigned char *) array->pdata) + (sizeof (void *) * (index + 1));
		src = ((unsigned char *) array->pdata) + (sizeof (void *) * index);
		n = array->len - index - 1;
		
		g_memmove (dest, src, (sizeof (void *) * n));
		array->pdata[index] = object;
	} else {
		/* the easy case */
		g_ptr_array_add (array, object);
	}
}

static void
multipart_insert (GMimeMultipart *multipart, int index, GMimeObject *part)
{
	ptr_array_insert (multipart->children, index, part);
	g_object_ref (part);
}


/**
 * g_mime_multipart_insert:
 * @multipart: a #GMimeMultipart object
 * @part: mime part
 * @index: position to insert the mime part
 *
 * Inserts the specified mime part into the multipart at the position
 * @index.
 **/
void
g_mime_multipart_insert (GMimeMultipart *multipart, int index, GMimeObject *part)
{
	g_return_if_fail (GMIME_IS_MULTIPART (multipart));
	g_return_if_fail (GMIME_IS_OBJECT (part));
	g_return_if_fail (index >= 0);
	
	GMIME_MULTIPART_GET_CLASS (multipart)->insert (multipart, index, part);
}


static gboolean
multipart_remove (GMimeMultipart *multipart, GMimeObject *part)
{
	if (!g_ptr_array_remove (multipart->children, part))
		return FALSE;
	
	g_object_unref (part);
	
	return TRUE;
}


/**
 * g_mime_multipart_remove:
 * @multipart: a #GMimeMultipart object
 * @part: mime part
 *
 * Removes the specified mime part from the multipart.
 *
 * Returns: %TRUE if the part was removed or %FALSE otherwise.
 **/
gboolean
g_mime_multipart_remove (GMimeMultipart *multipart, GMimeObject *part)
{
	g_return_val_if_fail (GMIME_IS_MULTIPART (multipart), FALSE);
	g_return_val_if_fail (GMIME_IS_OBJECT (part), FALSE);
	
	return GMIME_MULTIPART_GET_CLASS (multipart)->remove (multipart, part);
}


static GMimeObject *
multipart_remove_at (GMimeMultipart *multipart, int index)
{
	GMimeObject *part;
	
	if ((guint) index >= multipart->children->len)
		return NULL;
	
	part = multipart->children->pdata[index];
	
	g_ptr_array_remove_index (multipart->children, index);
	
	return part;
}


/**
 * g_mime_multipart_remove_at:
 * @multipart: a #GMimeMultipart object
 * @index: position of the mime part to remove
 *
 * Removes the mime part at position @index from the multipart.
 *
 * Returns: (transfer full): the mime part that was removed or %NULL
 * if the part was not contained within the multipart.
 **/
GMimeObject *
g_mime_multipart_remove_at (GMimeMultipart *multipart, int index)
{
	g_return_val_if_fail (GMIME_IS_MULTIPART (multipart), NULL);
	g_return_val_if_fail (index >= 0, NULL);
	
	return GMIME_MULTIPART_GET_CLASS (multipart)->remove_at (multipart, index);
}


/**
 * g_mime_multipart_replace:
 * @multipart: a #GMimeMultipart object
 * @index: position of the mime part to replace
 * @replacement: a #GMimeObject to use as the replacement
 *
 * Replaces the mime part at position @index within @multipart with
 * @replacement.
 *
 * Returns: (transfer full): the mime part that was replaced or %NULL
 * if the part was not contained within the multipart.
 **/
GMimeObject *
g_mime_multipart_replace (GMimeMultipart *multipart, int index, GMimeObject *replacement)
{
	GMimeObject *replaced;
	
	g_return_val_if_fail (GMIME_IS_MULTIPART (multipart), NULL);
	g_return_val_if_fail (GMIME_IS_OBJECT (replacement), NULL);
	g_return_val_if_fail (index >= 0, NULL);
	
	if ((guint) index >= multipart->children->len)
		return NULL;
	
	replaced = multipart->children->pdata[index];
	multipart->children->pdata[index] = replacement;
	g_object_ref (replacement);
	
	return replaced;
}


static GMimeObject *
multipart_get_part (GMimeMultipart *multipart, int index)
{
	GMimeObject *part;
	
	if ((guint) index >= multipart->children->len)
		return NULL;
	
	part = multipart->children->pdata[index];
	
	return part;
}


/**
 * g_mime_multipart_get_part:
 * @multipart: a #GMimeMultipart object
 * @index: position of the mime part
 *
 * Gets the mime part at position @index within the multipart.
 *
 * Returns: (transfer none): the mime part at position @index.
 **/
GMimeObject *
g_mime_multipart_get_part (GMimeMultipart *multipart, int index)
{
	g_return_val_if_fail (GMIME_IS_MULTIPART (multipart), NULL);
	g_return_val_if_fail (index >= 0, NULL);
	
	return GMIME_MULTIPART_GET_CLASS (multipart)->get_part (multipart, index);
}


static gboolean
multipart_contains (GMimeMultipart *multipart, GMimeObject *part)
{
	guint i;
	
	for (i = 0; i < multipart->children->len; i++) {
		if (part == (GMimeObject *) multipart->children->pdata[i])
			return TRUE;
	}
	
	return FALSE;
}


/**
 * g_mime_multipart_contains:
 * @multipart: a #GMimeMultipart object
 * @part: mime part
 *
 * Checks if @part is contained within @multipart.
 *
 * Returns: %TRUE if @part is a subpart of @multipart or %FALSE
 * otherwise.
 **/
gboolean
g_mime_multipart_contains (GMimeMultipart *multipart, GMimeObject *part)
{
	g_return_val_if_fail (GMIME_IS_MULTIPART (multipart), FALSE);
	g_return_val_if_fail (GMIME_IS_OBJECT (part), FALSE);
	
	return GMIME_MULTIPART_GET_CLASS (multipart)->contains (multipart, part);
}


static int
multipart_index_of (GMimeMultipart *multipart, GMimeObject *part)
{
	guint i;
	
	for (i = 0; i < multipart->children->len; i++) {
		if (part == (GMimeObject *) multipart->children->pdata[i])
			return i;
	}
	
	return -1;
}


/**
 * g_mime_multipart_index_of:
 * @multipart: a #GMimeMultipart object
 * @part: mime part
 *
 * Gets the index of @part within @multipart.
 *
 * Returns: the index of @part within @multipart or %-1 if not found.
 **/
int
g_mime_multipart_index_of (GMimeMultipart *multipart, GMimeObject *part)
{
	g_return_val_if_fail (GMIME_IS_MULTIPART (multipart), -1);
	g_return_val_if_fail (GMIME_IS_OBJECT (part), -1);
	
	return GMIME_MULTIPART_GET_CLASS (multipart)->index_of (multipart, part);
}


static int
multipart_get_count (GMimeMultipart *multipart)
{
	return multipart->children->len;
}


/**
 * g_mime_multipart_get_count:
 * @multipart: a #GMimeMultipart object
 *
 * Gets the number of mime parts contained within the multipart.
 *
 * Returns: the number of mime parts contained within the multipart.
 **/
int
g_mime_multipart_get_count (GMimeMultipart *multipart)
{
	g_return_val_if_fail (GMIME_IS_MULTIPART (multipart), -1);
	
	return GMIME_MULTIPART_GET_CLASS (multipart)->get_count (multipart);
}


static void
read_random_pool (unsigned char *buffer, size_t bytes)
{
#ifdef __unix__
	size_t nread = 0;
	ssize_t n;
	int fd;
	
	if ((fd = open ("/dev/urandom", O_RDONLY)) == -1) {
		if ((fd = open ("/dev/random", O_RDONLY)) == -1)
			return;
	}
	
	do {
		do {
			n = read (fd, (char *) buffer + nread, bytes - nread);
		} while (n == -1 && errno == EINTR);
		
		if (n == -1 || n == 0)
			break;
		
		nread += n;
	} while (nread < bytes);
	
	close (fd);
#else
	size_t i;
	
	srand ((unsigned int) time (NULL));
	
	for (i = 0; i < bytes; i++)
		buffer[i] = (unsigned char) (rand () % 256);
#endif
}

static void
multipart_set_boundary (GMimeMultipart *multipart, const char *boundary)
{
	char bbuf[35];
	
	if (!boundary) {
		/* Generate a fairly random boundary string. */
		unsigned char digest[16], *p;
		guint32 save = 0;
		int state = 0;
		
		read_random_pool (digest, 16);
		
		strcpy (bbuf, "=-");
		p = (unsigned char *) bbuf + 2;
		p += g_mime_encoding_base64_encode_step (digest, 16, p, &state, &save);
		*p = '\0';
		
		boundary = bbuf;
	}
	
	g_mime_object_set_content_type_parameter (GMIME_OBJECT (multipart), "boundary", boundary);
}


/**
 * g_mime_multipart_set_boundary:
 * @multipart: a #GMimeMultipart object
 * @boundary: boundary or %NULL to autogenerate one
 *
 * Sets @boundary as the boundary on the multipart. If @boundary is
 * %NULL, then a boundary will be auto-generated for you.
 **/
void
g_mime_multipart_set_boundary (GMimeMultipart *multipart, const char *boundary)
{
	g_return_if_fail (GMIME_IS_MULTIPART (multipart));
	
	GMIME_MULTIPART_GET_CLASS (multipart)->set_boundary (multipart, boundary);
}


static const char *
multipart_get_boundary (GMimeMultipart *multipart)
{
	GMimeObject *object = (GMimeObject *) multipart;
	const char *boundary;
	
	if ((boundary = g_mime_object_get_content_type_parameter (object, "boundary")))
		return boundary;
	
	multipart_set_boundary (multipart, NULL);
	
	return g_mime_object_get_content_type_parameter (object, "boundary");
}


/**
 * g_mime_multipart_get_boundary:
 * @multipart: a #GMimeMultipart object
 *
 * Gets the boundary on the multipart. If the internal boundary is
 * %NULL, then an auto-generated boundary will be set on the multipart
 * and returned.
 *
 * Returns: the boundary on the multipart.
 **/
const char *
g_mime_multipart_get_boundary (GMimeMultipart *multipart)
{
	g_return_val_if_fail (GMIME_IS_MULTIPART (multipart), NULL);
	
	return GMIME_MULTIPART_GET_CLASS (multipart)->get_boundary (multipart);
}


static void
multipart_foreach (GMimeMultipart *multipart, GMimeObjectForeachFunc callback, gpointer user_data)
{
	GMimeObject *part;
	guint i;
	
	for (i = 0; i < multipart->children->len; i++) {
		part = (GMimeObject *) multipart->children->pdata[i];
		callback ((GMimeObject *) multipart, part, user_data);
		
		if (GMIME_IS_MULTIPART (part))
			multipart_foreach ((GMimeMultipart *) part, callback, user_data);
	}
}


/**
 * g_mime_multipart_foreach: 
 * @multipart: a #GMimeMultipart
 * @callback: (scope call): function to call for each of @multipart's
 *   subparts.
 * @user_data: user-supplied callback data
 * 
 * Recursively calls @callback on each of @multipart's subparts.
 **/
void
g_mime_multipart_foreach (GMimeMultipart *multipart, GMimeObjectForeachFunc callback, gpointer user_data)
{
	g_return_if_fail (GMIME_IS_MULTIPART (multipart));
	g_return_if_fail (callback != NULL);
	
	multipart_foreach (multipart, callback, user_data);
}


/**
 * g_mime_multipart_get_subpart_from_content_id: 
 * @multipart: a multipart
 * @content_id: the content id of the part to look for
 *
 * Gets the mime part with the content-id @content_id from the
 * multipart @multipart.
 *
 * Returns: (transfer none): the #GMimeObject whose content-id matches
 * the search string, or %NULL if a match cannot be found.
 **/
GMimeObject *
g_mime_multipart_get_subpart_from_content_id (GMimeMultipart *multipart, const char *content_id)
{
	GMimeObject *object = (GMimeObject *) multipart;
	GMimeObject *subpart, *part;
	GMimeMultipart *mpart;
	guint i;
	
	g_return_val_if_fail (GMIME_IS_MULTIPART (multipart), NULL);
	g_return_val_if_fail (content_id != NULL, NULL);
	
	if (object->content_id && !strcmp (object->content_id, content_id))
		return object;
	
	for (i = 0; i < multipart->children->len; i++) {
		subpart = multipart->children->pdata[i];
		
		if (subpart->content_id && !strcmp (subpart->content_id, content_id))
			return subpart;
		
		if (GMIME_IS_MULTIPART (subpart)) {
			mpart = (GMimeMultipart *) subpart;
			if ((part = g_mime_multipart_get_subpart_from_content_id (mpart, content_id)))
				return part;
		}
	}
	
	return NULL;
}
