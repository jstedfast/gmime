/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2001,2002 Ximain, Inc. (www.ximian.com)
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

#include "gmime-object.h"
#include "gmime-stream-mem.h"

struct _type_bucket {
	char *type;
	GType object_type;
	GHashTable *subtype_hash;
};

static void g_mime_object_base_class_init (GMimeObjectClass *klass);
static void g_mime_object_base_class_finalize (GMimeObjectClass *klass);
static void g_mime_object_class_init (GMimeObjectClass *klass);
static void g_mime_object_class_finalize (GMimeObjectClass *klass);
static void g_mime_object_init (GMimeObject *object, GMimeObjectClass *klass);
static void g_mime_object_finalize (GObject *object);

static void init (GMimeObject *object);
static void add_header (GMimeObject *object, const char *header, const char *value);
static void set_header (GMimeObject *object, const char *header, const char *value);
static const char *get_header (GMimeObject *object, const char *header);
static void remove_header (GMimeObject *object, const char *header);
static char *get_headers (GMimeObject *object);
static ssize_t write_to_stream (GMimeObject *object, GMimeStream *stream);


static GHashTable *type_hash = NULL;

static GObjectClass *parent_class = NULL;


GType
g_mime_object_get_type (void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof (GMimeObjectClass),
			(GBaseInitFunc) g_mime_object_base_class_init,
			(GBaseFinalizeFunc) g_mime_object_base_class_finalize,
			(GClassInitFunc) g_mime_object_class_init,
			(GClassFinalizeFunc) g_mime_object_class_finalize,
			NULL, /* class_data */
			sizeof (GMimeObject),
			16,   /* n_preallocs */
			(GInstanceInitFunc) g_mime_object_init,
		};
		
		type = g_type_register_static (G_TYPE_OBJECT, "GMimeObject", &info, 0);
	}
	
	return type;
}


static void
g_mime_object_base_class_init (GMimeObjectClass *klass)
{
	/* reset instance specific methods that don't get inherited */
	;
}

static void
g_mime_object_base_class_finalize (GMimeObjectClass *klass)
{
	;
}

static void
g_mime_object_class_init (GMimeObjectClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	
	parent_class = g_type_class_ref (G_TYPE_OBJECT);
	
	object_class->finalize = g_mime_object_finalize;
	
	klass->init = init;
	klass->add_header = add_header;
	klass->set_header = set_header;
	klass->get_header = get_header;
	klass->remove_header = remove_header;
	klass->get_headers = get_headers;
	klass->write_to_stream = write_to_stream;
	
	type_hash = g_hash_table_new (g_strcase_hash, g_strcase_equal);
}

static void
subtype_bucket_foreach (gpointer key, gpointer value, gpointer user_data)
{
	g_free (key);
}

static void
type_bucket_foreach (gpointer key, gpointer value, gpointer user_data)
{
	struct _type_bucket *bucket = value;
	
	g_free (bucket->type);
	
	if (bucket->subtype_hash) {
		g_hash_table_foreach (bucket->subtype_hash, subtype_bucket_foreach, NULL);
		g_hash_table_destroy (bucket->subtype_hash);
	}
	
	g_free (bucket);
}

static void
g_mime_object_class_finalize (GMimeObjectClass *klass)
{
	g_hash_table_foreach (type_hash, type_bucket_foreach, NULL);
	g_hash_table_destroy (type_hash);
}

static void
g_mime_object_init (GMimeObject *object, GMimeObjectClass *klass)
{
	object->headers = g_mime_header_new ();
}

static void
g_mime_object_finalize (GObject *object)
{
	GMimeObject *mime = (GMimeObject *) object;
	
	if (mime->headers)
		g_mime_header_destroy (mime->headers);
	
	G_OBJECT_CLASS (parent_class)->finalize (object);
}


/**
 * g_mime_object_ref:
 * @object: mime object
 *
 * Ref's a MIME object.
 **/
void
g_mime_object_ref (GMimeObject *object)
{
	g_return_if_fail (GMIME_IS_OBJECT (object));
	
	g_object_ref ((GObject *) object);
}


/**
 * g_mime_object_unref:
 * @object: mime object
 *
 * Unref's a MIME object.
 **/
void
g_mime_object_unref (GMimeObject *object)
{
	g_return_if_fail (GMIME_IS_OBJECT (object));
	
	g_object_unref ((GObject *) object);
}


/**
 * g_mime_object_register_type:
 * @type: mime type
 * @subtype: mime subtype
 * @object_type: GMimeObject subclass type that handles mime
 *               parts of the type specified by @type/@subtype
 *
 * Registers the object type for the g_mime_object_new_type()
 * convenience function.
 *
 * Note: In order for the parser to work properly, each leaf MIME part
 * class MUST be registered. You may use the wildcard "*" to match any
 * type and/or subtype.
 **/
void
g_mime_object_register_type (const char *type, const char *subtype, GType object_type)
{
	struct _type_bucket *bucket;
	
	g_return_if_fail (object_type != NULL);
	g_return_if_fail (subtype != NULL);
	g_return_if_fail (type != NULL);
	
	bucket = g_hash_table_lookup (type_hash, type);
	if (!bucket) {
		bucket = g_new (struct _type_bucket, 1);
		bucket->type = g_strdup (type);
		bucket->object_type = *type == '*' ? object_type : NULL;
		bucket->subtype_hash = g_hash_table_new (g_strcase_hash, g_strcase_equal);
		g_hash_table_insert (type_hash, bucket->type, bucket);
	}
	
	g_hash_table_insert (bucket->subtype_hash, g_strdup (subtype), object_type);
}


static void
init (GMimeObject *object)
{
	/* no-op */
}


/**
 * g_mime_object_new_type:
 * @type: mime type
 * @subtype: mime subtype
 *
 **/
GMimeObject *
g_mime_object_new_type (const char *type, const char *subtype)
{
	struct _type_bucket *bucket;
	GMimeObject *object;
	GType obj_type;
	
	g_return_val_if_fail (type != NULL, NULL);
	
	bucket = g_hash_table_lookup (type_hash, type);
	if (!bucket) {
		bucket = g_hash_table_lookup (type_hash, "*");
		obj_type = bucket ? bucket->object_type : NULL;
	} else {
		obj_type = g_hash_table_lookup (subtype_hash, subtype);
		if (!obj_type)
			obj_type = g_hash_table_lookup (subtype_hash, "*");
	}
	
	if (!obj_type)
		return NULL;
	
	object = g_object_new (obj_type, NULL, NULL);
	
	GMIME_OBJECT_GET_CLASS (object)->init (object);
	
	return object;
}


static void
add_header (GMimeObject *object, const char *header, const char *value)
{
	g_mime_header_add (object->headers, header, value);
}


/**
 * g_mime_object_add_header:
 * @object: mime object
 * @header: header name
 * @value: header value
 *
 * Add an arbitrary header to the MIME object.
 **/
void
g_mime_object_add_header (GMimeObject *object, const char *header, const char *value)
{
	g_return_if_fail (GMIME_IS_OBJECT (object));
	g_return_if_fail (header != NULL);
	
	GMIME_OBJECT_GET_CLASS (object)->add_header (object, header, value);
}


static void
set_header (GMimeObject *object, const char *header, const char *value)
{
	g_mime_header_set (object->headers, header, value);
}


/**
 * g_mime_object_set_header:
 * @object: mime object
 * @header: header name
 * @value: header value
 *
 * Sets an arbitrary header on the MIME object.
 **/
void
g_mime_object_set_header (GMimeObject *object, const char *header, const char *value)
{
	g_return_if_fail (GMIME_IS_OBJECT (object));
	g_return_if_fail (header != NULL);
	
	GMIME_OBJECT_GET_CLASS (object)->set_header (object, header, value);
}


static const char *
get_header (GMimeObject *object, const char *header)
{
	return g_mime_header_get (object->headers, header);
}


/**
 * g_mime_object_get_header:
 * @object: mime object
 * @header: header name
 *
 * Gets the value of the requested header if it exists or %NULL
 * otherwise.
 *
 * Returns the value of the header @header.
 **/
const char *
g_mime_object_get_header (GMimeObject *object, const char *header)
{
	g_return_val_if_fail (GMIME_IS_OBJECT (object), NULL);
	g_return_val_if_fail (header != NULL, NULL);
	
	return GMIME_OBJECT_GET_CLASS (object)->get_header (object, header);
}


static void
remove_header (GMimeObject *object, const char *header)
{
	g_mime_header_remove (object->headers, header);
}


/**
 * g_mime_object_remove_header:
 * @object: mime object
 * @header: header name
 *
 * Removed the specified header if it exists.
 **/
void
g_mime_object_remove_header (GMimeObject *object, const char *header)
{
	g_return_val_if_fail (GMIME_IS_OBJECT (object), NULL);
	g_return_val_if_fail (header != NULL, NULL);
	
	GMIME_OBJECT_GET_CLASS (object)->remove_header (object, header);
}


static char *
get_headers (GMimeObject *object)
{
	return g_mime_header_to_string (object->headers);
}


/**
 * g_mime_object_get_headers:
 * @object: mime object
 *
 * Allocates a string buffer containing the MIME object's raw headers.
 *
 * Returns an allocated string containing the raw MIME headers.
 **/
char *
g_mime_object_get_headers (GMimeObject *object)
{
	g_return_val_if_fail (GMIME_IS_OBJECT (object), NULL);
	
	return GMIME_OBJECT_GET_CLASS (object)->get_headerss (object);
}


static ssize_t
write_to_stream (GMimeObject *object, GMimeStream *stream)
{
	return -1;
}


/**
 * g_mime_object_write_to_stream:
 * @object: mime object
 * @stream: stream
 *
 * Write the contents of the MIME object to @stream.
 *
 * Returns -1 on fail.
 **/
ssize_t
g_mime_object_write_to_stream (GMimeObject *object, GMimeStream *stream)
{
	g_return_val_if_fail (GMIME_IS_OBJECT (object), -1);
	g_return_val_if_fail (GMIME_IS_STREAM (stream), -1);
	
	return GMIME_OBJECT_GET_CLASS (object)->write_to_stream (object, stream);
}


/**
 * g_mime_object_to_string:
 * @object: mime object
 *
 * Allocates a string buffer containing the contents of @object.
 *
 * Returns an allocated string containing the contents of the mime
 * object.
 **/
char *
g_mime_object_to_string (GMimeObject *object)
{
	GMimeStream *stream;
	GByteArray *array;
	char *str;
	
	g_return_val_if_fail (GMIME_IS_OBJECT (message), NULL);
	
	array = g_byte_array_new ();
	stream = g_mime_stream_mem_new ();
	g_mime_stream_mem_set_byte_array (GMIME_STREAM_MEM (stream), array);
	
	g_mime_object_write_to_stream (object, stream);
	
	g_mime_stream_unref (stream);
	g_byte_array_append (array, "", 1);
	str = array->data;
	g_byte_array_free (array, FALSE);
	
	return str;
}
