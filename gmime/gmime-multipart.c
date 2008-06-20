/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2008 Jeffrey Stedfast
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

#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

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
static void multipart_prepend_header (GMimeObject *object, const char *header, const char *value);
static void multipart_append_header (GMimeObject *object, const char *header, const char *value);
static void multipart_set_header (GMimeObject *object, const char *header, const char *value);
static const char *multipart_get_header (GMimeObject *object, const char *header);
static gboolean multipart_remove_header (GMimeObject *object, const char *header);
static void multipart_set_content_type (GMimeObject *object, GMimeContentType *content_type);
static char *multipart_get_headers (GMimeObject *object);
static ssize_t multipart_write_to_stream (GMimeObject *object, GMimeStream *stream);

/* GMimeMultipart class methods */
static void multipart_add_part (GMimeMultipart *multipart, GMimeObject *part);
static void multipart_add_part_at (GMimeMultipart *multipart, GMimeObject *part, int index);
static gboolean multipart_remove_part (GMimeMultipart *multipart, GMimeObject *part);
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
	
	object_class->prepend_header = multipart_prepend_header;
	object_class->append_header = multipart_append_header;
	object_class->remove_header = multipart_remove_header;
	object_class->set_header = multipart_set_header;
	object_class->get_header = multipart_get_header;
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
	multipart->children = g_ptr_array_new ();
	multipart->boundary = NULL;
	multipart->preface = NULL;
	multipart->postface = NULL;
}

static void
g_mime_multipart_finalize (GObject *object)
{
	GMimeMultipart *multipart = (GMimeMultipart *) object;
	guint i;
	
	g_free (multipart->boundary);
	g_free (multipart->preface);
	g_free (multipart->postface);
	
	for (i = 0; i < multipart->children->len; i++)
		g_object_unref (multipart->children->pdata[i]);
	
	g_ptr_array_free (multipart->children, TRUE);
	
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
multipart_prepend_header (GMimeObject *object, const char *header, const char *value)
{
	/* Make sure that the header is a Content-* header, else it
           doesn't belong on a multipart */
	if (!g_ascii_strncasecmp ("Content-", header, 8))
		GMIME_OBJECT_CLASS (parent_class)->prepend_header (object, header, value);
}

static void
multipart_append_header (GMimeObject *object, const char *header, const char *value)
{
	/* Make sure that the header is a Content-* header, else it
           doesn't belong on a multipart */
	if (!g_ascii_strncasecmp ("Content-", header, 8))
		GMIME_OBJECT_CLASS (parent_class)->append_header (object, header, value);
}

static void
multipart_set_header (GMimeObject *object, const char *header, const char *value)
{
	/* RFC 1864 states that you cannot set a Content-MD5 on a multipart */
	if (!g_ascii_strcasecmp ("Content-MD5", header))
		return;
	
	/* Make sure that the header is a Content-* header, else it
           doesn't belong on a multipart */
	if (!g_ascii_strncasecmp ("Content-", header, 8))
		GMIME_OBJECT_CLASS (parent_class)->set_header (object, header, value);
}

static const char *
multipart_get_header (GMimeObject *object, const char *header)
{
	/* Make sure that the header is a Content-* header, else it
           doesn't belong on a multipart */
	if (!g_ascii_strncasecmp ("Content-", header, 8))
		return GMIME_OBJECT_CLASS (parent_class)->get_header (object, header);
	else
		return NULL;
}

static gboolean
multipart_remove_header (GMimeObject *object, const char *header)
{
	/* Make sure that the header is a Content-* header, else it
           doesn't belong on a multipart */
	if (g_ascii_strncasecmp ("Content-", header, 8) != 0)
		return FALSE;
	
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
	guint i;
	
	/* make sure a boundary is set unless we are writing out a raw
	 * header (in which case it should already be set... or if
	 * not, then it's a broken multipart and so we don't want to
	 * alter it or we'll completely break the output) */
	if (!multipart->boundary && !g_mime_header_list_has_raw (object->headers))
		g_mime_multipart_set_boundary (multipart, NULL);
	
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
		if ((nwritten = g_mime_stream_printf (stream, "\n--%s\n", multipart->boundary)) == -1)
			return -1;
		
		total += nwritten;
		
		/* write this part out */
		if ((nwritten = g_mime_object_write_to_stream (part, stream)) == -1)
			return -1;
		
		total += nwritten;
	}
	
	/* write the end-boundary (but only if a boundary is set) */
	if (multipart->boundary) {
		if ((nwritten = g_mime_stream_printf (stream, "\n--%s--\n", multipart->boundary)) == -1)
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
	GMimeMultipart *multipart;
	GMimeContentType *type;
	
	multipart = g_object_new (GMIME_TYPE_MULTIPART, NULL);
	
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
 * Returns: an empty MIME multipart object with a content-type of
 * multipart/@subtype.
 **/
GMimeMultipart *
g_mime_multipart_new_with_subtype (const char *subtype)
{
	GMimeMultipart *multipart;
	GMimeContentType *type;
	
	multipart = g_object_new (GMIME_TYPE_MULTIPART, NULL);
	
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
 * Returns: a pointer to the postface string on the multipart.
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
	g_object_ref (part);
	
	g_ptr_array_add (multipart->children, part);
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
ptr_array_insert (GPtrArray *array, guint index, gpointer object)
{
	unsigned char *dest, *src;
	guint n;
	
	g_ptr_array_set_size (array, array->len + 1);
	
	if (index == array->len) {
		/* need to move items down */
		dest = ((unsigned char *) array->pdata) + (sizeof (void *) * (index + 1));
		src = ((unsigned char *) array->pdata) + (sizeof (void *) * index);
		n = array->len - index - 1;
		
		g_memmove (dest, src, (sizeof (void *) * n));
	}
	
	array->pdata[index] = object;
}

static void
multipart_add_part_at (GMimeMultipart *multipart, GMimeObject *part, int index)
{
	g_object_ref (part);
	
	ptr_array_insert (multipart->children, index, part);
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


static gboolean
multipart_remove_part (GMimeMultipart *multipart, GMimeObject *part)
{
	if (!g_ptr_array_remove (multipart->children, part))
		return FALSE;
	
	g_object_unref (part);
	
	return TRUE;
}


/**
 * g_mime_multipart_remove_part:
 * @multipart: multipart
 * @part: mime part
 *
 * Removes the specified mime part from the multipart.
 *
 * Returns: %TRUE if the part was removed or %FALSE otherwise.
 **/
gboolean
g_mime_multipart_remove_part (GMimeMultipart *multipart, GMimeObject *part)
{
	g_return_val_if_fail (GMIME_IS_MULTIPART (multipart), FALSE);
	g_return_val_if_fail (GMIME_IS_OBJECT (part), FALSE);
	
	return GMIME_MULTIPART_GET_CLASS (multipart)->remove_part (multipart, part);
}


static GMimeObject *
multipart_remove_part_at (GMimeMultipart *multipart, int index)
{
	GMimeObject *part;
	
	if (index >= multipart->children->len)
		return NULL;
	
	part = multipart->children->pdata[index];
	
	g_ptr_array_remove_index (multipart->children, index);
	
	return part;
}


/**
 * g_mime_multipart_remove_part_at:
 * @multipart: multipart
 * @index: position of the mime part to remove
 *
 * Removes the mime part at position @index from the multipart.
 *
 * Returns: the mime part that was removed.
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
	
	if (index >= multipart->children->len)
		return NULL;
	
	part = multipart->children->pdata[index];
	g_object_ref (part);
	
	return part;
}


/**
 * g_mime_multipart_get_part:
 * @multipart: multipart
 * @index: position of the mime part
 *
 * Gets the mime part at position @index within the multipart.
 *
 * Returns: the mime part at position @index.
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
	return multipart->children->len;
}


/**
 * g_mime_multipart_get_number:
 * @multipart: multipart
 *
 * Gets the number of mime parts contained within the multipart.
 *
 * Returns: the number of mime parts contained within the multipart.
 **/
int
g_mime_multipart_get_number (GMimeMultipart *multipart)
{
	g_return_val_if_fail (GMIME_IS_MULTIPART (multipart), -1);
	
	return GMIME_MULTIPART_GET_CLASS (multipart)->get_number (multipart);
}


static void
read_random_pool (unsigned char *buffer, size_t bytes)
{
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
 * Returns: the boundary on the multipart.
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
 * Calls @callback on each of @multipart's subparts.
 **/
void
g_mime_multipart_foreach (GMimeMultipart *multipart, GMimePartFunc callback, gpointer user_data)
{
	GMimeObject *part;
	guint i;
	
	g_return_if_fail (GMIME_IS_MULTIPART (multipart));
	g_return_if_fail (callback != NULL);
	
	for (i = 0; i < multipart->children->len; i++) {
		part = multipart->children->pdata[i];
		callback (part, user_data);
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
 * Returns: the GMimeObject whose content-id matches the search string,
 * or %NULL if a match cannot be found.
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
	
	if (object->content_id && !strcmp (object->content_id, content_id)) {
		g_object_ref (object);
		return object;
	}
	
	for (i = 0; i < multipart->children->len; i++) {
		subpart = multipart->children->pdata[i];
		
		if (GMIME_IS_MULTIPART (subpart)) {
			mpart = (GMimeMultipart *) subpart;
			if ((part = g_mime_multipart_get_subpart_from_content_id (mpart, content_id)))
				return part;
		} else if (subpart->content_id && !strcmp (subpart->content_id, content_id)) {
			g_object_ref (subpart);
			return subpart;
		}
	}
	
	return NULL;
}
