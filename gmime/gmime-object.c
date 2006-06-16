/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 1999-2006 Jeffrey Stedfast
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
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <ctype.h>
#include <string.h>

#include "gmime-common.h"
#include "gmime-object.h"
#include "gmime-stream-mem.h"
#include "gmime-utils.h"

struct _type_bucket {
	char *type;
	GType object_type;
	GHashTable *subtype_hash;
};

struct _subtype_bucket {
	char *subtype;
	GType object_type;
};

static void g_mime_object_class_init (GMimeObjectClass *klass);
static void g_mime_object_init (GMimeObject *object, GMimeObjectClass *klass);
static void g_mime_object_finalize (GObject *object);

static void init (GMimeObject *object);
static void add_header (GMimeObject *object, const char *header, const char *value);
static void set_header (GMimeObject *object, const char *header, const char *value);
static const char *get_header (GMimeObject *object, const char *header);
static void remove_header (GMimeObject *object, const char *header);
static void set_content_type (GMimeObject *object, GMimeContentType *content_type);
static char *get_headers (GMimeObject *object);
static ssize_t write_to_stream (GMimeObject *object, GMimeStream *stream);

static ssize_t write_content_type (GMimeStream *stream, const char *name, const char *value);

static void type_registry_init (void);


static GHashTable *type_hash = NULL;

static GObjectClass *parent_class = NULL;


GType
g_mime_object_get_type (void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof (GMimeObjectClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc) g_mime_object_class_init,
			NULL, /* class_finalize */
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
	klass->set_content_type = set_content_type;
	klass->get_headers = get_headers;
	klass->write_to_stream = write_to_stream;
	
	type_registry_init ();
}

static void
g_mime_object_init (GMimeObject *object, GMimeObjectClass *klass)
{
	object->content_type = NULL;
	object->headers = g_mime_header_new ();
	object->content_id = NULL;
	
	g_mime_header_register_writer (object->headers, "Content-Type", write_content_type);
}

static void
g_mime_object_finalize (GObject *object)
{
	GMimeObject *mime = (GMimeObject *) object;
	
	if (mime->content_type)
		g_mime_content_type_destroy (mime->content_type);
	
	if (mime->headers)
		g_mime_header_destroy (mime->headers);
	
	g_free (mime->content_id);
	
	G_OBJECT_CLASS (parent_class)->finalize (object);
}


static ssize_t
write_content_type (GMimeStream *stream, const char *name, const char *value)
{
	GMimeContentType *content_type;
	ssize_t nwritten;
	GString *out;
	char *val;
	
	out = g_string_new ("");
	g_string_printf (out, "%s: ", name);
	
	content_type = g_mime_content_type_new_from_string (value);
	
	val = g_mime_content_type_to_string (content_type);
	g_string_append (out, val);
	g_free (val);
	
	g_mime_param_write_to_string (content_type->params, TRUE, out);
	g_mime_content_type_destroy (content_type);
	
	nwritten = g_mime_stream_write (stream, out->str, out->len);
	g_string_free (out, TRUE);
	
	return nwritten;
}


/**
 * g_mime_object_ref:
 * @object: mime object
 *
 * Ref's a MIME object.
 *
 * WARNING: This method is deprecated. Use g_object_ref() instead.
 **/
void
g_mime_object_ref (GMimeObject *object)
{
	g_return_if_fail (GMIME_IS_OBJECT (object));
	
	g_object_ref (object);
}


/**
 * g_mime_object_unref:
 * @object: mime object
 *
 * Unref's a MIME object.
 *
 * WARNING: This method is deprecated. Use g_object_unref() instead.
 **/
void
g_mime_object_unref (GMimeObject *object)
{
	g_return_if_fail (GMIME_IS_OBJECT (object));
	
	g_object_unref (object);
}


/**
 * g_mime_object_register_type:
 * @type: mime type
 * @subtype: mime subtype
 * @object_type: object type
 *
 * Registers the object type @object_type for use with the
 * g_mime_object_new_type() convenience function.
 *
 * Note: You may use the wildcard "*" to match any type and/or
 * subtype.
 **/
void
g_mime_object_register_type (const char *type, const char *subtype, GType object_type)
{
	struct _type_bucket *bucket;
	struct _subtype_bucket *sub;
	
	g_return_if_fail (object_type != 0);
	g_return_if_fail (subtype != NULL);
	g_return_if_fail (type != NULL);
	
	type_registry_init ();
	
	bucket = g_hash_table_lookup (type_hash, type);
	if (!bucket) {
		bucket = g_new (struct _type_bucket, 1);
		bucket->type = g_strdup (type);
		bucket->object_type = *type == '*' ? object_type : 0;
		bucket->subtype_hash = g_hash_table_new (g_mime_strcase_hash, g_mime_strcase_equal);
		g_hash_table_insert (type_hash, bucket->type, bucket);
	}
	
	sub = g_new (struct _subtype_bucket, 1);
	sub->subtype = g_strdup (subtype);
	sub->object_type = object_type;
	g_hash_table_insert (bucket->subtype_hash, sub->subtype, sub);
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
 * Performs a lookup of registered #GMimeObject subclasses, registered
 * using g_mime_object_register_type(), to find an appropriate class
 * capable of handling MIME parts of type @type/@subtype. If no class
 * has been registered to handle that type, it looks for a registered
 * class that can handle @type. If that also fails, then it will use
 * the generic part class, #GMimePart.
 *
 * Returns an appropriate #GMimeObject registered to handle mime-types
 * of @type/@subtype.
 **/
GMimeObject *
g_mime_object_new_type (const char *type, const char *subtype)
{
	struct _type_bucket *bucket;
	struct _subtype_bucket *sub;
	GMimeObject *object;
	GType obj_type;
	
	g_return_val_if_fail (type != NULL, NULL);
	
	type_registry_init ();
	
	bucket = g_hash_table_lookup (type_hash, type);
	if (!bucket) {
		bucket = g_hash_table_lookup (type_hash, "*");
		obj_type = bucket ? bucket->object_type : 0;
	} else {
		if (!(sub = g_hash_table_lookup (bucket->subtype_hash, subtype)))
			sub = g_hash_table_lookup (bucket->subtype_hash, "*");
		
		obj_type = sub ? sub->object_type : 0;
	}
	
	if (!obj_type) {
		/* use the default mime object */
		bucket = g_hash_table_lookup (type_hash, "*");
		if (bucket) {
			sub = g_hash_table_lookup (bucket->subtype_hash, "*");
			obj_type = sub ? sub->object_type : 0;
		}
		
		if (!obj_type)
			return NULL;
	}
	
	object = g_object_new (obj_type, NULL, NULL);
	
	GMIME_OBJECT_GET_CLASS (object)->init (object);
	
	return object;
}


static void
sync_content_type (GMimeObject *object)
{
	GMimeContentType *content_type;
	GMimeParam *params;
	GString *string;
	char *type, *p;
	
	content_type = object->content_type;
	
	string = g_string_new ("Content-Type: ");
	
	type = g_mime_content_type_to_string (content_type);
	g_string_append (string, type);
	g_free (type);
	
	params = content_type->params;
	if (params)
		g_mime_param_write_to_string (params, FALSE, string);
	
	p = string->str;
	g_string_free (string, FALSE);
	
	type = p + strlen ("Content-Type: ");
	g_mime_header_set (object->headers, "Content-Type", type);
	g_free (p);
}


static void
set_content_type (GMimeObject *object, GMimeContentType *content_type)
{
	if (object->content_type)
		g_mime_content_type_destroy (object->content_type);
	
	object->content_type = content_type;
	
	sync_content_type (object);
}


/**
 * g_mime_object_set_content_type:
 * @object: MIME object
 * @mime_type: MIME type
 *
 * Sets the content-type for the specified MIME object.
 **/
void
g_mime_object_set_content_type (GMimeObject *object, GMimeContentType *mime_type)
{
	g_return_if_fail (GMIME_IS_OBJECT (object));
	g_return_if_fail (mime_type != NULL);
	
	GMIME_OBJECT_GET_CLASS (object)->set_content_type (object, mime_type);
}


/**
 * g_mime_object_get_content_type:
 * @object: MIME object
 *
 * Gets the Content-Type object for the given MIME object or %NULL on
 * fail.
 *
 * Returns the content-type object for the specified MIME object.
 **/
const GMimeContentType *
g_mime_object_get_content_type (GMimeObject *object)
{
	g_return_val_if_fail (GMIME_IS_OBJECT (object), NULL);
	
	return object->content_type;
}


/**
 * g_mime_object_set_content_type_parameter:
 * @object: MIME object
 * @name: param name
 * @value: param value
 *
 * Sets the content-type param @name to the value @value.
 **/
void
g_mime_object_set_content_type_parameter (GMimeObject *object, const char *name, const char *value)
{
	g_return_if_fail (GMIME_IS_OBJECT (object));
	g_return_if_fail (name != NULL);
	
	g_mime_content_type_set_parameter (object->content_type, name, value);
	
	sync_content_type (object);
}


/**
 * g_mime_object_get_content_type_parameter:
 * @object: MIME object
 * @name: param name
 *
 * Gets the value of the content-type param @name set on the MIME part
 * @object.
 *
 * Returns the value of the requested content-type param or %NULL on
 * if the param doesn't exist.
 **/
const char *
g_mime_object_get_content_type_parameter (GMimeObject *object, const char *name)
{
	g_return_val_if_fail (GMIME_IS_OBJECT (object), NULL);
	g_return_val_if_fail (name != NULL, NULL);
	
	return g_mime_content_type_get_parameter (object->content_type, name);
}


/**
 * g_mime_object_set_content_id:
 * @object: MIME object
 * @content_id: content-id (addr-spec portion)
 *
 * Sets the Content-Id of the MIME object.
 **/
void
g_mime_object_set_content_id (GMimeObject *object, const char *content_id)
{
	char *msgid;
	
	g_return_if_fail (GMIME_IS_OBJECT (object));
	
	g_free (object->content_id);
	object->content_id = g_strdup (content_id);
	
	msgid = g_strdup_printf ("<%s>", content_id);
	g_mime_object_set_header (object, "Content-Id", msgid);
	g_free (msgid);
}


/**
 * g_mime_object_get_content_id:
 * @object: MIME object
 *
 * Gets the Content-Id of the MIME object or NULL if one is not set.
 *
 * Returns a const pointer to the Content-Id header.
 **/
const char *
g_mime_object_get_content_id (GMimeObject *object)
{
	g_return_val_if_fail (GMIME_IS_OBJECT (object), NULL);
	
	return object->content_id;
}


enum {
	HEADER_CONTENT_TYPE,
	HEADER_CONTENT_ID,
	HEADER_UNKNOWN,
};

static char *headers[] = {
	"Content-Type",
	"Content-Id",
	NULL
};

static gboolean
process_header (GMimeObject *object, const char *header, const char *value)
{
	GMimeContentType *content_type;
	int i;
	
	for (i = 0; headers[i]; i++) {
		if (!strcasecmp (headers[i], header))
			break;
	}
	
	switch (i) {
	case HEADER_CONTENT_TYPE:
		content_type = g_mime_content_type_new_from_string (value);
		g_mime_object_set_content_type (object, content_type);
		break;
	case HEADER_CONTENT_ID:
		g_free (object->content_id);
		object->content_id = g_mime_utils_decode_message_id (value);
		break;
	default:
		return FALSE;
	}
	
	g_mime_header_set (object->headers, header, value);
	
	return TRUE;
}

static void
add_header (GMimeObject *object, const char *header, const char *value)
{
	if (!process_header (object, header, value))
		g_mime_header_add (object->headers, header, value);
}


/**
 * g_mime_object_add_header:
 * @object: mime object
 * @header: header name
 * @value: header value
 *
 * Adds an arbitrary header to the MIME object.
 **/
void
g_mime_object_add_header (GMimeObject *object, const char *header, const char *value)
{
	g_return_if_fail (GMIME_IS_OBJECT (object));
	g_return_if_fail (header != NULL);
	g_return_if_fail (value != NULL);
	
	GMIME_OBJECT_GET_CLASS (object)->add_header (object, header, value);
}


static void
set_header (GMimeObject *object, const char *header, const char *value)
{
	if (!process_header (object, header, value))
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
	g_return_if_fail (value != NULL);
	
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
 * Returns the value of the header @header if it exists or %NULL
 * otherwise.
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
	g_return_if_fail (GMIME_IS_OBJECT (object));
	g_return_if_fail (header != NULL);
	
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
 * Allocates a string buffer containing all of the MIME object's raw
 * headers.
 *
 * Returns an allocated string containing all of the raw MIME headers.
 **/
char *
g_mime_object_get_headers (GMimeObject *object)
{
	g_return_val_if_fail (GMIME_IS_OBJECT (object), NULL);
	
	return GMIME_OBJECT_GET_CLASS (object)->get_headers (object);
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
	
	g_return_val_if_fail (GMIME_IS_OBJECT (object), NULL);
	
	array = g_byte_array_new ();
	stream = g_mime_stream_mem_new ();
	g_mime_stream_mem_set_byte_array (GMIME_STREAM_MEM (stream), array);
	
	g_mime_object_write_to_stream (object, stream);
	
	g_object_unref (stream);
	g_byte_array_append (array, "", 1);
	str = array->data;
	g_byte_array_free (array, FALSE);
	
	return str;
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
type_registry_shutdown (void)
{
	g_hash_table_foreach (type_hash, type_bucket_foreach, NULL);
	g_hash_table_destroy (type_hash);
}

static void
type_registry_init (void)
{
	if (type_hash)
		return;
	
	type_hash = g_hash_table_new (g_mime_strcase_hash, g_mime_strcase_equal);
	
	g_atexit (type_registry_shutdown);
}
