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

#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include "gmime-multipart.h"
#include "gmime-utils.h"
#include "strlib.h"


#define d(x) x

/* GObject class methods */
static void g_mime_multipart_class_init (GMimeMultipartClass *klass);
static void g_mime_multipart_init (GMimeMultipart *multipart, GMimeMultipartClass *klass);
static void g_mime_multipart_finalize (GObject *object);

/* GMimeObject class methods */
static void multipart_init (GMimeObject *object);
static void multipart_add_header (GMimeObject *object, const char *header, const char *value);
static void multipart_set_header (GMimeObject *object, const char *header, const char *value);
static const char *multipart_get_header (GMimeObject *object, const char *header);
static void multipart_remove_header (GMimeObject *object, const char *header);
static void multipart_set_content_type (GMimeObject *object, GMimeContentType *content_type);
static char *multipart_get_headers (GMimeObject *object);
static ssize_t multipart_write_to_stream (GMimeObject *object, GMimeStream *stream);

/* GMimeMultipart class methods */
static void multipart_add_part (GMimeMultipart *multipart, GMimeObject *part);
static void multipart_add_part_at (GMimeMultipart *multipart, GMimeObject *part, int index);
static void multipart_remove_part (GMimeMultipart *multipart, GMimeObject *part);
static GMimeObject *multipart_remove_part_at (GMimeMultipart *multipart, int index);
static GMimeObject *multipart_get_part (GMimeMultipart *multipart, int index);
static int multipart_get_number (GMimeMultipart *multipart);
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
			16,   /* n_preallocs */
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
	
	object_class->init = multipart_init;
	object_class->add_header = multipart_add_header;
	object_class->set_header = multipart_set_header;
	object_class->get_header = multipart_get_header;
	object_class->remove_header = multipart_remove_header;
	object_class->set_content_type = multipart_set_content_type;
	object_class->get_headers = multipart_get_headers;
	object_class->write_to_stream = multipart_write_to_stream;
	
	klass->add_part = multipart_add_part;
	klass->add_part_at = multipart_add_part_at;
	klass->remove_part = multipart_remove_part;
	klass->remove_part_at = multipart_remove_part_at;
	klass->get_part = multipart_get_part;
	klass->get_number = multipart_get_number;
	klass->set_boundary = multipart_set_boundary;
	klass->get_boundary = multipart_get_boundary;
}

static void
g_mime_multipart_init (GMimeMultipart *multipart, GMimeMultipartClass *klass)
{
	multipart->boundary = NULL;
	multipart->preface = NULL;
	multipart->postface = NULL;
	multipart->subparts = NULL;
}

static void
g_mime_multipart_finalize (GObject *object)
{
	GMimeMultipart *multipart = (GMimeMultipart *) object;
	GList *node;
	
	g_free (multipart->boundary);
	g_free (multipart->preface);
	g_free (multipart->postface);
	
	node = multipart->subparts;
	while (node) {
		GMimeObject *part;
		
		part = node->data;
		g_mime_object_unref (part);
		node = node->next;
	}
	g_list_free (multipart->subparts);
	
	G_OBJECT_CLASS (parent_class)->finalize (object);
}


static void
multipart_init (GMimeObject *object)
{
	/* no-op */
	GMIME_OBJECT_CLASS (parent_class)->init (object);
}

static void
multipart_add_header (GMimeObject *object, const char *header, const char *value)
{
	/* Make sure that the header is a Content-* header, else it
           doesn't belong on a multipart */
	
	if (!strncasecmp ("Content-", header, 8))
		GMIME_OBJECT_CLASS (parent_class)->add_header (object, header, value);
}

static void
multipart_set_header (GMimeObject *object, const char *header, const char *value)
{
	/* RFC 1864 states that you cannot set a Content-MD5 on a multipart */
	if (!strcasecmp ("Content-MD5", header))
		return;
	
	/* Make sure that the header is a Content-* header, else it
           doesn't belong on a multipart */
	
	if (!strncasecmp ("Content-", header, 8))
		GMIME_OBJECT_CLASS (parent_class)->set_header (object, header, value);
}

static const char *
multipart_get_header (GMimeObject *object, const char *header)
{
	/* Make sure that the header is a Content-* header, else it
           doesn't belong on a multipart */
	
	if (!strncasecmp ("Content-", header, 8))
		return GMIME_OBJECT_CLASS (parent_class)->get_header (object, header);
	else
		return NULL;
}

static void
multipart_remove_header (GMimeObject *object, const char *header)
{
	/* Make sure that the header is a Content-* header, else it
           doesn't belong on a multipart */
	
	if (!strncasecmp ("Content-", header, 8))
		return GMIME_OBJECT_CLASS (parent_class)->remove_header (object, header);
}

static void
multipart_set_content_type (GMimeObject *object, GMimeContentType *content_type)
{
	GMimeMultipart *multipart = (GMimeMultipart *) object;
	const char *boundary;
	
	boundary = g_mime_content_type_get_parameter (content_type, "boundary");
	g_free (multipart->boundary);
	multipart->boundary = g_strdup (boundary);
	
	GMIME_OBJECT_CLASS (parent_class)->set_content_type (object, content_type);
}

static char *
multipart_get_headers (GMimeObject *object)
{
	return GMIME_OBJECT_CLASS (parent_class)->get_headers (object);
}

static ssize_t
multipart_write_to_stream (GMimeObject *object, GMimeStream *stream)
{
	GMimeMultipart *multipart = (GMimeMultipart *) object;
	ssize_t nwritten, total = 0;
	GMimeObject *part;
	GList *node;
	
	/* make sure a boundary is set */
	if (!multipart->boundary)
		g_mime_multipart_set_boundary (multipart, NULL);
	
	/* write the content headers */
	nwritten = g_mime_header_write_to_stream (object->headers, stream);
	if (nwritten == -1)
		return -1;
	
	total += nwritten;
	
	/* write the preface */
	if (multipart->preface) {
		/* terminate the headers */
		if (g_mime_stream_write (stream, "\n", 1) == -1)
			return -1;
		
		total++;
		
		nwritten = g_mime_stream_write_string (stream, multipart->preface);
		if (nwritten == -1)
			return -1;
		
		total += nwritten;
	}
	
	node = multipart->subparts;
	while (node) {
		part = node->data;
		
		/* write the boundary */
		nwritten = g_mime_stream_printf (stream, "\n--%s\n", multipart->boundary);
		if (nwritten == -1)
			return -1;
		
		total += nwritten;
		
		/* write this part out */
		nwritten = g_mime_object_write_to_stream (part, stream);
		if (nwritten == -1)
			return -1;
		
		total += nwritten;
		
		node = node->next;
	}
	
	nwritten = g_mime_stream_printf (stream, "\n--%s--\n", multipart->boundary);
	if (nwritten == -1)
		return -1;
	
	total += nwritten;
	
	/* write the postface */
	if (multipart->postface) {
		nwritten = g_mime_stream_write_string (stream, multipart->postface);
		if (nwritten == -1)
			return -1;
		
		total += nwritten;
	}
		
	return total;
}


/**
 * g_mime_multipart_new:
 *
 * Creates a new MIME multipart object with a default content-type of
 * multipart/mixed.
 *
 * Returns an empty MIME multipart object with a default content-type of
 * multipart/mixed.
 **/
GMimeMultipart *
g_mime_multipart_new ()
{
	GMimeMultipart *multipart;
	GMimeContentType *type;
	
	multipart = g_object_new (GMIME_TYPE_MULTIPART, NULL, NULL);
	
	type = g_mime_content_type_new ("multipart", "mixed");
	g_mime_object_set_content_type (GMIME_OBJECT (multipart), type);
	
	return multipart;
}


/**
 * g_mime_multipart_new_with_subtype:
 * @subtype: content-type subtype
 *
 * Creates a new MIME multipart object with a content-type of
 * multipart/@subtype.
 *
 * Returns an empty MIME multipart object with a content-type of
 * multipart/@subtype.
 **/
GMimeMultipart *
g_mime_multipart_new_with_subtype (const char *subtype)
{
	GMimeMultipart *multipart;
	GMimeContentType *type;
	
	multipart = g_object_new (GMIME_TYPE_MULTIPART, NULL, NULL);
	
	type = g_mime_content_type_new ("multipart", subtype ? subtype : "mixed");
	g_mime_object_set_content_type (GMIME_OBJECT (multipart), type);
	
	return multipart;
}


/**
 * g_mime_multipart_set_preface:
 * @multipart: multipart
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
 * @multipart: multipart
 *
 * Gets the preface on the multipart.
 *
 * Returns a pointer to the preface string on the multipart.
 **/
const char *
g_mime_multipart_get_preface (GMimeMultipart *multipart)
{
	g_return_val_if_fail (GMIME_IS_MULTIPART (multipart), NULL);
	
	return multipart->preface;
}


/**
 * g_mime_multipart_set_postface:
 * @multipart: multipart
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
 * @multipart: multipart
 *
 * Gets the postface on the multipart.
 *
 * Returns a pointer to the postface string on the multipart.
 **/
const char *
g_mime_multipart_get_postface (GMimeMultipart *multipart)
{
	g_return_val_if_fail (GMIME_IS_MULTIPART (multipart), NULL);
	
	return multipart->postface;
}


static void
multipart_add_part (GMimeMultipart *multipart, GMimeObject *part)
{
	g_mime_object_ref (part);
	multipart->subparts = g_list_append (multipart->subparts, part);
}


/**
 * g_mime_multipart_add_part:
 * @multipart: multipart
 * @part: mime part
 *
 * Adds a mime part to the multipart.
 **/
void
g_mime_multipart_add_part (GMimeMultipart *multipart, GMimeObject *part)
{
	g_return_if_fail (GMIME_IS_MULTIPART (multipart));
	g_return_if_fail (GMIME_IS_OBJECT (part));
	
	GMIME_MULTIPART_GET_CLASS (multipart)->add_part (multipart, part);
}


static void
multipart_add_part_at (GMimeMultipart *multipart, GMimeObject *part, int index)
{
	g_mime_object_ref (part);
	multipart->subparts = g_list_insert (multipart->subparts, part, index);
}


/**
 * g_mime_multipart_add_part_at:
 * @multipart: multipart
 * @part: mime part
 * @index: position to insert the mime part
 *
 * Adds a mime part to the multipart at the position @index.
 **/
void
g_mime_multipart_add_part_at (GMimeMultipart *multipart, GMimeObject *part, int index)
{
	g_return_if_fail (GMIME_IS_MULTIPART (multipart));
	g_return_if_fail (GMIME_IS_OBJECT (part));
	g_return_if_fail (index >= 0);
	
	GMIME_MULTIPART_GET_CLASS (multipart)->add_part_at (multipart, part, index);
}


static void
multipart_remove_part (GMimeMultipart *multipart, GMimeObject *part)
{
	GList *node;
	
	/* *sigh* fucking glist... */
	
	node = multipart->subparts;
	while (node) {
		if (node->data == (gpointer) part)
			break;
		node = node->next;
	}
	
	if (node == NULL) {
		d(g_warning ("multipart_remove_part: %p does not seem to be a subpart of %p",
			     part, multipart));
		return;
	}
	
	if (node == multipart->subparts) {
		if (node->next)
			node->next->prev = NULL;
		multipart->subparts = node->next;
	} else {
		if (node->next)
			node->next->prev = node->prev;
	}
	
	g_list_free_1 (node);
	
	g_mime_object_unref (part);
}


/**
 * g_mime_multipart_remove_part:
 * @multipart: multipart
 * @part: mime part
 *
 * Removes the specified mime part from the multipart.
 **/
void
g_mime_multipart_remove_part (GMimeMultipart *multipart, GMimeObject *part)
{
	g_return_if_fail (GMIME_IS_MULTIPART (multipart));
	g_return_if_fail (GMIME_IS_OBJECT (part));
	
	GMIME_MULTIPART_GET_CLASS (multipart)->remove_part (multipart, part);
}


static GMimeObject *
multipart_remove_part_at (GMimeMultipart *multipart, int index)
{
	GMimeObject *part;
	GList *node;
	
	node = g_list_nth (multipart->subparts, index);
	if (!node) {
		d(g_warning ("multipart_remove_part_at: no part at index %d within %p", index, multipart));
		return NULL;
	}
	
	part = node->data;
	
	if (node == multipart->subparts) {
		if (node->next)
			node->next->prev = NULL;
		multipart->subparts = node->next;
	} else {
		if (node->next)
			node->next->prev = node->prev;
	}
	
	g_list_free_1 (node);
	
	return part;
}


/**
 * g_mime_multipart_remove_part_at:
 * @multipart: multipart
 * @index: position of the mime part to remove
 *
 * Removes the mime part at position @index from the multipart.
 *
 * Returns the mime part that was removed.
 **/
GMimeObject *
g_mime_multipart_remove_part_at (GMimeMultipart *multipart, int index)
{
	g_return_val_if_fail (GMIME_IS_MULTIPART (multipart), NULL);
	g_return_val_if_fail (index >= 0, NULL);
	
	return GMIME_MULTIPART_GET_CLASS (multipart)->remove_part_at (multipart, index);
}


static GMimeObject *
multipart_get_part (GMimeMultipart *multipart, int index)
{
	GMimeObject *part;
	GList *node;
	
	node = g_list_nth (multipart->subparts, index);
	if (!node) {
		d(g_warning ("multipart_get_part: no part at index %d within %p", index, multipart));
		return NULL;
	}
	
	part = node->data;
	
	g_mime_object_ref (part);
	
	return part;
}


/**
 * g_mime_multipart_get_part:
 * @multipart: multipart
 * @index: position of the mime part
 *
 * Gets the mime part at position @index within the multipart.
 *
 * Returns the mime part at position @index.
 **/
GMimeObject *
g_mime_multipart_get_part (GMimeMultipart *multipart, int index)
{
	g_return_val_if_fail (GMIME_IS_MULTIPART (multipart), NULL);
	g_return_val_if_fail (index >= 0, NULL);
	
	return GMIME_MULTIPART_GET_CLASS (multipart)->get_part (multipart, index);
}


static int
multipart_get_number (GMimeMultipart *multipart)
{
	if (!multipart->subparts)
		return 0;
	
	return g_list_length (multipart->subparts);
}


/**
 * g_mime_multipart_get_number:
 * @multipart: multipart
 *
 * Gets the number of mime parts contained within the multipart.
 *
 * Returns the number of mime parts contained within the multipart.
 **/
int
g_mime_multipart_get_number (GMimeMultipart *multipart)
{
	g_return_val_if_fail (GMIME_IS_MULTIPART (multipart), -1);
	
	return GMIME_MULTIPART_GET_CLASS (multipart)->get_number (multipart);
}


static void
read_random_pool (char *buffer, size_t bytes)
{
	int fd;
	
	fd = open ("/dev/urandom", O_RDONLY);
	if (fd == -1) {
		fd = open ("/dev/random", O_RDONLY);
		if (fd == -1)
			return;
	}
	
	read (fd, buffer, bytes);
	close (fd);
}

static void
multipart_set_boundary (GMimeMultipart *multipart, const char *boundary)
{
	char bbuf[35];
	
	if (!boundary) {
		/* Generate a fairly random boundary string. */
		char digest[16], *p;
		int state, save;
		
		read_random_pool (digest, 16);
		
		strcpy (bbuf, "=-");
		p = bbuf + 2;
		state = save = 0;
		p += g_mime_utils_base64_encode_step (digest, 16, p, &state, &save);
		*p = '\0';
		
		boundary = bbuf;
	}
	
	g_free (multipart->boundary);
	multipart->boundary = g_strdup (boundary);
	
	g_mime_object_set_content_type_parameter (GMIME_OBJECT (multipart), "boundary", boundary);
}


/**
 * g_mime_multipart_set_boundary:
 * @multipart: multipart
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
	if (!multipart->boundary)
		multipart_set_boundary (multipart, NULL);
	
	return multipart->boundary;
}


/**
 * g_mime_multipart_get_boundary:
 * @multipart: multipart
 *
 * Gets the boundary on the multipart. If the internal boundary is
 * %NULL, then an auto-generated boundary will be set on the multipart
 * and returned.
 *
 * Returns the boundary on the multipart.
 **/
const char *
g_mime_multipart_get_boundary (GMimeMultipart *multipart)
{
	g_return_val_if_fail (GMIME_IS_MULTIPART (multipart), NULL);
	
	return GMIME_MULTIPART_GET_CLASS (multipart)->get_boundary (multipart);
}


/**
 * g_mime_multipart_foreach: 
 * @multipart: a multipart
 * @callback: function to call for @multipart and all of its subparts
 * @user_data: extra data to pass to the callback
 * 
 * Calls @callback on @multipart and each of its subparts.
 **/
void
g_mime_multipart_foreach (GMimeMultipart *multipart, GMimePartFunc callback, gpointer user_data)
{
	g_return_if_fail (GMIME_IS_MULTIPART (multipart));
	g_return_if_fail (callback != NULL);
	
	callback (GMIME_OBJECT (multipart), user_data);
	
	if (multipart->subparts) {
		GList *subpart;
		
		subpart = multipart->subparts;
		while (subpart) {
			GMimeObject *part = subpart->data;
			
			if (GMIME_IS_MULTIPART (part))
				g_mime_multipart_foreach (GMIME_MULTIPART (part), callback, user_data);
			else
				callback (part, user_data);
			
			subpart = subpart->next;
		}
	}
}


/**
 * g_mime_multipart_get_subpart_from_content_id: 
 * @multipart: a multipart
 * @content_id: the content id of the part to look for
 *
 * Gets the mime part with the content-id @content_id from the
 * multipart @multipart.
 *
 * Returns the GMimeObject whose content-id matches the search string,
 * or %NULL if a match cannot be found.
 **/
const GMimeObject *
g_mime_multipart_get_subpart_from_content_id (GMimeMultipart *multipart, const char *content_id)
{
	GMimeObject *object = (GMimeObject *) multipart;
	GList *subparts;
	
	g_return_val_if_fail (GMIME_IS_MULTIPART (multipart), NULL);
	g_return_val_if_fail (content_id != NULL, NULL);
	
	if (object->content_id && !strcmp (object->content_id, content_id))
		return object;
	
	subparts = multipart->subparts;
	while (subparts) {
		const GMimeContentType *type;
		const GMimeObject *part;
		GMimeObject *subpart;
		
		subpart = subparts->data;
		type = g_mime_object_get_content_type (GMIME_OBJECT (subpart));
		
		if (g_mime_content_type_is_type (type, "multipart", "*")) {
			part = g_mime_multipart_get_subpart_from_content_id (GMIME_MULTIPART (subpart),
									     content_id);
		} else if (subpart->content_id && !strcmp (subpart->content_id, content_id)) {
			part = subpart;
		}
		
		if (part)
			return part;
		
		subparts = subparts->next;
	}
	
	return NULL;
}
