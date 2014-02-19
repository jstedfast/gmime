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

#include <ctype.h>
#include <string.h>

#include "gmime-common.h"
#include "gmime-object.h"
#include "gmime-stream-mem.h"
#include "gmime-events.h"
#include "gmime-utils.h"


/**
 * SECTION: gmime-object
 * @title: GMimeObject
 * @short_description: Abstract MIME objects
 * @see_also:
 *
 * #GMimeObject is an abstract class from which all message and MIME
 * parts are derived.
 **/


struct _type_bucket {
	char *type;
	GType object_type;
	GHashTable *subtype_hash;
};

struct _subtype_bucket {
	char *subtype;
	GType object_type;
};

static void _g_mime_object_set_content_disposition (GMimeObject *object, GMimeContentDisposition *disposition);
void _g_mime_object_set_content_type (GMimeObject *object, GMimeContentType *content_type);

static void g_mime_object_class_init (GMimeObjectClass *klass);
static void g_mime_object_init (GMimeObject *object, GMimeObjectClass *klass);
static void g_mime_object_finalize (GObject *object);

static void object_prepend_header (GMimeObject *object, const char *name, const char *value);
static void object_append_header (GMimeObject *object, const char *name, const char *value);
static void object_set_header (GMimeObject *object, const char *name, const char *value);
static const char *object_get_header (GMimeObject *object, const char *name);
static gboolean object_remove_header (GMimeObject *object, const char *name);
static void object_set_content_type (GMimeObject *object, GMimeContentType *content_type);
static char *object_get_headers (GMimeObject *object);
static ssize_t object_write_to_stream (GMimeObject *object, GMimeStream *stream);
static void object_encode (GMimeObject *object, GMimeEncodingConstraint constraint);

static ssize_t write_content_type (GMimeStream *stream, const char *name, const char *value);
static ssize_t write_disposition (GMimeStream *stream, const char *name, const char *value);

static void content_type_changed (GMimeContentType *content_type, gpointer args, GMimeObject *object);
static void content_disposition_changed (GMimeContentDisposition *disposition, gpointer args, GMimeObject *object);


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
			0,    /* n_preallocs */
			(GInstanceInitFunc) g_mime_object_init,
		};
		
		type = g_type_register_static (G_TYPE_OBJECT, "GMimeObject",
					       &info, G_TYPE_FLAG_ABSTRACT);
	}
	
	return type;
}


static void
g_mime_object_class_init (GMimeObjectClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	
	parent_class = g_type_class_ref (G_TYPE_OBJECT);
	
	object_class->finalize = g_mime_object_finalize;
	
	klass->prepend_header = object_prepend_header;
	klass->append_header = object_append_header;
	klass->remove_header = object_remove_header;
	klass->set_header = object_set_header;
	klass->get_header = object_get_header;
	klass->set_content_type = object_set_content_type;
	klass->get_headers = object_get_headers;
	klass->write_to_stream = object_write_to_stream;
	klass->encode = object_encode;
}

static void
g_mime_object_init (GMimeObject *object, GMimeObjectClass *klass)
{
	object->headers = g_mime_header_list_new ();
	object->content_type = NULL;
	object->disposition = NULL;
	object->content_id = NULL;
	
	g_mime_header_list_register_writer (object->headers, "Content-Type", write_content_type);
	g_mime_header_list_register_writer (object->headers, "Content-Disposition", write_disposition);
}

static void
g_mime_object_finalize (GObject *object)
{
	GMimeObject *mime = (GMimeObject *) object;
	
	if (mime->content_type) {
		g_mime_event_remove (mime->content_type->priv, (GMimeEventCallback) content_type_changed, object);
		g_object_unref (mime->content_type);
	}
	
	if (mime->disposition) {
		g_mime_event_remove (mime->disposition->priv, (GMimeEventCallback) content_disposition_changed, object);
		g_object_unref (mime->disposition);
	}
	
	if (mime->headers)
		g_mime_header_list_destroy (mime->headers);
	
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
	g_object_unref (content_type);
	
	nwritten = g_mime_stream_write (stream, out->str, out->len);
	g_string_free (out, TRUE);
	
	return nwritten;
}

static void
content_type_changed (GMimeContentType *content_type, gpointer args, GMimeObject *object)
{
	GMimeParam *params;
	GString *string;
	char *type, *p;
	
	string = g_string_new ("Content-Type: ");
	
	type = g_mime_content_type_to_string (content_type);
	g_string_append (string, type);
	g_free (type);
	
	if ((params = content_type->params))
		g_mime_param_write_to_string (params, FALSE, string);
	
	p = string->str;
	g_string_free (string, FALSE);
	
	type = p + strlen ("Content-Type: ");
	g_mime_header_list_set (object->headers, "Content-Type", type);
	g_free (p);
}

static ssize_t
write_disposition (GMimeStream *stream, const char *name, const char *value)
{
	GMimeContentDisposition *disposition;
	ssize_t nwritten;
	GString *out;
	
	out = g_string_new ("");
	g_string_printf (out, "%s: ", name);
	
	disposition = g_mime_content_disposition_new_from_string (value);
	g_string_append (out, disposition->disposition);
	
	g_mime_param_write_to_string (disposition->params, TRUE, out);
	g_object_unref (disposition);
	
	nwritten = g_mime_stream_write (stream, out->str, out->len);
	g_string_free (out, TRUE);
	
	return nwritten;
}

static void
content_disposition_changed (GMimeContentDisposition *disposition, gpointer args, GMimeObject *object)
{
	char *str;
	
	if (object->disposition) {
		str = g_mime_content_disposition_to_string (object->disposition, FALSE);
		g_mime_header_list_set (object->headers, "Content-Disposition", str);
		g_free (str);
	} else {
		g_mime_header_list_remove (object->headers, "Content-Disposition");
	}
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
	
	if (!(bucket = g_hash_table_lookup (type_hash, type))) {
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


/**
 * g_mime_object_new:
 * @content_type: a #GMimeContentType object
 *
 * Performs a lookup of registered #GMimeObject subclasses, registered
 * using g_mime_object_register_type(), to find an appropriate class
 * capable of handling MIME parts of the specified Content-Type. If no
 * class has been registered to handle that type, it looks for a
 * registered class that can handle @content_type's media type. If
 * that also fails, then it will use the generic part class,
 * #GMimePart.
 *
 * Returns: an appropriate #GMimeObject registered to handle MIME
 * parts appropriate for @content_type.
 **/
GMimeObject *
g_mime_object_new (GMimeContentType *content_type)
{
	struct _type_bucket *bucket;
	struct _subtype_bucket *sub;
	GMimeObject *object;
	GType obj_type;
	
	g_return_val_if_fail (GMIME_IS_CONTENT_TYPE (content_type), NULL);
	
	if ((bucket = g_hash_table_lookup (type_hash, content_type->type))) {
		if (!(sub = g_hash_table_lookup (bucket->subtype_hash, content_type->subtype)))
			sub = g_hash_table_lookup (bucket->subtype_hash, "*");
		
		obj_type = sub ? sub->object_type : 0;
	} else {
		bucket = g_hash_table_lookup (type_hash, "*");
		obj_type = bucket ? bucket->object_type : 0;
	}
	
	if (!obj_type) {
		/* use the default mime object */
		if ((bucket = g_hash_table_lookup (type_hash, "*"))) {
			sub = g_hash_table_lookup (bucket->subtype_hash, "*");
			obj_type = sub ? sub->object_type : 0;
		}
		
		if (!obj_type)
			return NULL;
	}
	
	object = g_object_newv (obj_type, 0, NULL);
	
	g_mime_object_set_content_type (object, content_type);
	
	return object;
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
 * Returns: an appropriate #GMimeObject registered to handle mime-types
 * of @type/@subtype.
 **/
GMimeObject *
g_mime_object_new_type (const char *type, const char *subtype)
{
	struct _type_bucket *bucket;
	struct _subtype_bucket *sub;
	GType obj_type;
	
	g_return_val_if_fail (type != NULL, NULL);
	
	if ((bucket = g_hash_table_lookup (type_hash, type))) {
		if (!(sub = g_hash_table_lookup (bucket->subtype_hash, subtype)))
			sub = g_hash_table_lookup (bucket->subtype_hash, "*");
		
		obj_type = sub ? sub->object_type : 0;
	} else {
		bucket = g_hash_table_lookup (type_hash, "*");
		obj_type = bucket ? bucket->object_type : 0;
	}
	
	if (!obj_type) {
		/* use the default mime object */
		if ((bucket = g_hash_table_lookup (type_hash, "*"))) {
			sub = g_hash_table_lookup (bucket->subtype_hash, "*");
			obj_type = sub ? sub->object_type : 0;
		}
		
		if (!obj_type)
			return NULL;
	}
	
	return g_object_newv (obj_type, 0, NULL);
}


static void
object_set_content_type (GMimeObject *object, GMimeContentType *content_type)
{
	if (object->content_type) {
		g_mime_event_remove (object->content_type->priv, (GMimeEventCallback) content_type_changed, object);
		g_object_unref (object->content_type);
	}
	
	g_mime_event_add (content_type->priv, (GMimeEventCallback) content_type_changed, object);
	object->content_type = content_type;
	g_object_ref (content_type);
}


/**
 * _g_mime_object_set_content_type:
 * @object: a #GMimeObject
 * @content_type: a #GMimeContentType object
 *
 * Sets the content-type for the specified MIME object.
 *
 * Note: This method is meant for internal-use only and avoids
 * serialization of @content_type to the Content-Type header field.
 **/
void
_g_mime_object_set_content_type (GMimeObject *object, GMimeContentType *content_type)
{
	GMIME_OBJECT_GET_CLASS (object)->set_content_type (object, content_type);
}


/**
 * g_mime_object_set_content_type:
 * @object: a #GMimeObject
 * @content_type: a #GMimeContentType object
 *
 * Sets the content-type for the specified MIME object and then
 * serializes it to the Content-Type header field.
 **/
void
g_mime_object_set_content_type (GMimeObject *object, GMimeContentType *content_type)
{
	g_return_if_fail (GMIME_IS_CONTENT_TYPE (content_type));
	g_return_if_fail (GMIME_IS_OBJECT (object));
	
	if (object->content_type == content_type)
		return;
	
	GMIME_OBJECT_GET_CLASS (object)->set_content_type (object, content_type);
	
	content_type_changed (content_type, NULL, object);
}


/**
 * g_mime_object_get_content_type:
 * @object: a #GMimeObject
 *
 * Gets the #GMimeContentType object for the given MIME object or
 * %NULL on fail.
 *
 * Returns: (transfer none): the content-type object for the specified
 * MIME object.
 **/
GMimeContentType *
g_mime_object_get_content_type (GMimeObject *object)
{
	g_return_val_if_fail (GMIME_IS_OBJECT (object), NULL);
	
	return object->content_type;
}


/**
 * g_mime_object_set_content_type_parameter:
 * @object: a #GMimeObject
 * @name: param name
 * @value: param value
 *
 * Sets the content-type param @name to the value @value.
 *
 * Note: The @name string should be in US-ASCII while the @value
 * string should be in UTF-8.
 **/
void
g_mime_object_set_content_type_parameter (GMimeObject *object, const char *name, const char *value)
{
	g_return_if_fail (GMIME_IS_OBJECT (object));
	g_return_if_fail (name != NULL);
	
	g_mime_content_type_set_parameter (object->content_type, name, value);
}


/**
 * g_mime_object_get_content_type_parameter:
 * @object: a #GMimeObject
 * @name: param name
 *
 * Gets the value of the content-type param @name set on the MIME part
 * @object.
 *
 * Returns: the value of the requested content-type param or %NULL if
 * the param doesn't exist. If the param is set, the returned string
 * will be in UTF-8.
 **/
const char *
g_mime_object_get_content_type_parameter (GMimeObject *object, const char *name)
{
	g_return_val_if_fail (GMIME_IS_OBJECT (object), NULL);
	g_return_val_if_fail (name != NULL, NULL);
	
	return g_mime_content_type_get_parameter (object->content_type, name);
}


/**
 * g_mime_object_get_content_disposition:
 * @object: a #GMimeObject
 *
 * Gets the #GMimeContentDisposition for the specified MIME object.
 *
 * Returns: (transfer none): the #GMimeContentDisposition set on the
 * MIME object.
 **/
GMimeContentDisposition *
g_mime_object_get_content_disposition (GMimeObject *object)
{
	g_return_val_if_fail (GMIME_IS_OBJECT (object), NULL);
	
	return object->disposition;
}

/**
 * g_mime_object_set_content_disposition:
 * @object: a #GMimeObject
 * @disposition: a #GMimeContentDisposition object
 *
 * Set the content disposition for the specified mime part.
 *
 * Note: This method is meant for internal-use only and avoids
 * serialization of @disposition to the Content-Disposition header
 * field.
 **/
static void
_g_mime_object_set_content_disposition (GMimeObject *object, GMimeContentDisposition *disposition)
{
	if (object->disposition) {
		g_mime_event_remove (object->disposition->priv, (GMimeEventCallback) content_disposition_changed, object);
		g_object_unref (object->disposition);
	}
	
	g_mime_event_add (disposition->priv, (GMimeEventCallback) content_disposition_changed, object);
	object->disposition = disposition;
	g_object_ref (disposition);
}


/**
 * g_mime_object_set_content_disposition:
 * @object: a #GMimeObject
 * @disposition: a #GMimeContentDisposition object
 *
 * Set the content disposition for the specified mime part and then
 * serializes it to the Content-Disposition header field.
 **/
void
g_mime_object_set_content_disposition (GMimeObject *object, GMimeContentDisposition *disposition)
{
	g_return_if_fail (GMIME_IS_CONTENT_DISPOSITION (disposition));
	g_return_if_fail (GMIME_IS_OBJECT (object));
	
	if (object->disposition == disposition)
		return;
	
	_g_mime_object_set_content_disposition (object, disposition);
	
	content_disposition_changed (disposition, NULL, object);
}


/**
 * g_mime_object_set_disposition:
 * @object: a #GMimeObject
 * @disposition: disposition ("attachment" or "inline")
 *
 * Sets the disposition to @disposition which may be one of
 * #GMIME_DISPOSITION_ATTACHMENT or #GMIME_DISPOSITION_INLINE or, by
 * your choice, any other string which would indicate how the MIME
 * part should be displayed by the MUA.
 **/
void
g_mime_object_set_disposition (GMimeObject *object, const char *disposition)
{
	GMimeContentDisposition *disp;
	
	g_return_if_fail (GMIME_IS_OBJECT (object));
	g_return_if_fail (disposition != NULL);
	
	if (object->disposition) {
		g_mime_content_disposition_set_disposition (object->disposition, disposition);
		return;
	}
	
	disp = g_mime_content_disposition_new ();
	g_mime_content_disposition_set_disposition (disp, disposition);
	g_mime_object_set_content_disposition (object, disp);
	g_object_unref (disp);
}


/**
 * g_mime_object_get_disposition:
 * @object: a #GMimeObject
 *
 * Gets the MIME object's disposition if set or %NULL otherwise.
 *
 * Returns: the disposition string which is probably one of
 * #GMIME_DISPOSITION_ATTACHMENT or #GMIME_DISPOSITION_INLINE.
 **/
const char *
g_mime_object_get_disposition (GMimeObject *object)
{
	g_return_val_if_fail (GMIME_IS_OBJECT (object), NULL);
	
	if (object->disposition)
		return g_mime_content_disposition_get_disposition (object->disposition);
	
	return NULL;
}


/**
 * g_mime_object_set_content_disposition_parameter:
 * @object: a #GMimeObject
 * @name: parameter name
 * @value: parameter value
 *
 * Add a content-disposition parameter to the specified mime part.
 *
 * Note: The @name string should be in US-ASCII while the @value
 * string should be in UTF-8.
 **/
void
g_mime_object_set_content_disposition_parameter (GMimeObject *object, const char *name, const char *value)
{
	GMimeContentDisposition *disposition;
	
	g_return_if_fail (GMIME_IS_OBJECT (object));
	g_return_if_fail (name != NULL);
	
	if (!object->disposition) {
		disposition = g_mime_content_disposition_new ();
		_g_mime_object_set_content_disposition (object, disposition);
		g_object_unref (disposition);
	}
	
	g_mime_content_disposition_set_parameter (object->disposition, name, value);
}


/**
 * g_mime_object_get_content_disposition_parameter:
 * @object: a #GMimeObject
 * @name: parameter name
 *
 * Gets the value of the Content-Disposition parameter specified by
 * @name, or %NULL if the parameter does not exist.
 *
 * Returns: the value of the requested content-disposition param or
 * %NULL if the param doesn't exist. If the param is set, the returned
 * string will be in UTF-8.
 **/
const char *
g_mime_object_get_content_disposition_parameter (GMimeObject *object, const char *name)
{
	g_return_val_if_fail (GMIME_IS_OBJECT (object), NULL);
	g_return_val_if_fail (name != NULL, NULL);
	
	if (!object->disposition)
		return NULL;
	
	return g_mime_content_disposition_get_parameter (object->disposition, name);
}


/**
 * g_mime_object_set_content_id:
 * @object: a #GMimeObject
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
 * @object: a #GMimeObject
 *
 * Gets the Content-Id of the MIME object or NULL if one is not set.
 *
 * Returns: a const pointer to the Content-Id header.
 **/
const char *
g_mime_object_get_content_id (GMimeObject *object)
{
	g_return_val_if_fail (GMIME_IS_OBJECT (object), NULL);
	
	return object->content_id;
}


enum {
	HEADER_CONTENT_DISPOSITION,
	HEADER_CONTENT_TYPE,
	HEADER_CONTENT_ID,
	HEADER_UNKNOWN,
};

static char *content_headers[] = {
	"Content-Disposition",
	"Content-Type",
	"Content-Id",
};

static gboolean
process_header (GMimeObject *object, const char *header, const char *value)
{
	GMimeContentDisposition *disposition;
	GMimeContentType *content_type;
	guint i;
	
	if (g_ascii_strncasecmp (header, "Content-", 8) != 0)
		return FALSE;
	
	for (i = 0; i < G_N_ELEMENTS (content_headers); i++) {
		if (!g_ascii_strcasecmp (content_headers[i] + 8, header + 8))
			break;
	}
	
	switch (i) {
	case HEADER_CONTENT_DISPOSITION:
		disposition = g_mime_content_disposition_new_from_string (value);
		_g_mime_object_set_content_disposition (object, disposition);
		g_object_unref (disposition);
		break;
	case HEADER_CONTENT_TYPE:
		content_type = g_mime_content_type_new_from_string (value);
		_g_mime_object_set_content_type (object, content_type);
		g_object_unref (content_type);
		break;
	case HEADER_CONTENT_ID:
		g_free (object->content_id);
		object->content_id = g_mime_utils_decode_message_id (value);
		break;
	default:
		return FALSE;
	}
	
	g_mime_header_list_set (object->headers, header, value);
	
	return TRUE;
}

static void
object_prepend_header (GMimeObject *object, const char *header, const char *value)
{
	if (!process_header (object, header, value))
		g_mime_header_list_prepend (object->headers, header, value);
}


/**
 * g_mime_object_prepend_header:
 * @object: a #GMimeObject
 * @header: header name
 * @value: header value
 *
 * Prepends a raw, unprocessed header to the MIME object.
 *
 * Note: @value should be encoded with a function such as
 * g_mime_utils_header_encode_text().
 **/
void
g_mime_object_prepend_header (GMimeObject *object, const char *header, const char *value)
{
	g_return_if_fail (GMIME_IS_OBJECT (object));
	g_return_if_fail (header != NULL);
	g_return_if_fail (value != NULL);
	
	GMIME_OBJECT_GET_CLASS (object)->prepend_header (object, header, value);
}

static void
object_append_header (GMimeObject *object, const char *header, const char *value)
{
	if (!process_header (object, header, value))
		g_mime_header_list_append (object->headers, header, value);
}


/**
 * g_mime_object_append_header:
 * @object: a #GMimeObject
 * @header: header name
 * @value: header value
 *
 * Appends a raw, unprocessed header to the MIME object.
 *
 * Note: @value should be encoded with a function such as
 * g_mime_utils_header_encode_text().
 **/
void
g_mime_object_append_header (GMimeObject *object, const char *header, const char *value)
{
	g_return_if_fail (GMIME_IS_OBJECT (object));
	g_return_if_fail (header != NULL);
	g_return_if_fail (value != NULL);
	
	GMIME_OBJECT_GET_CLASS (object)->append_header (object, header, value);
}


static void
object_set_header (GMimeObject *object, const char *header, const char *value)
{
	if (!process_header (object, header, value))
		g_mime_header_list_set (object->headers, header, value);
}


/**
 * g_mime_object_set_header:
 * @object: a #GMimeObject
 * @header: header name
 * @value: header value
 *
 * Sets an arbitrary raw, unprocessed header on the MIME object.
 *
 * Note: @value should be encoded with a function such as
 * g_mime_utils_header_encode_text().
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
object_get_header (GMimeObject *object, const char *header)
{
	return g_mime_header_list_get (object->headers, header);
}


/**
 * g_mime_object_get_header:
 * @object: a #GMimeObject
 * @header: header name
 *
 * Gets the raw, unprocessed value of the requested header.
 *
 * Returns: the raw, unprocessed value of the requested header if it
 * exists or %NULL otherwise.
 *
 * Note: The returned value should be decoded with a function such as
 * g_mime_utils_header_decode_text() before displaying to the user.
 **/
const char *
g_mime_object_get_header (GMimeObject *object, const char *header)
{
	g_return_val_if_fail (GMIME_IS_OBJECT (object), NULL);
	g_return_val_if_fail (header != NULL, NULL);
	
	return GMIME_OBJECT_GET_CLASS (object)->get_header (object, header);
}


static gboolean
object_remove_header (GMimeObject *object, const char *header)
{
	guint i;
	
	for (i = 0; i < G_N_ELEMENTS (content_headers); i++) {
		if (!g_ascii_strcasecmp (content_headers[i], header))
			break;
	}
	
	switch (i) {
	case HEADER_CONTENT_DISPOSITION:
		if (object->disposition) {
			g_mime_event_remove (object->disposition->priv, (GMimeEventCallback) content_disposition_changed, object);
			g_object_unref (object->disposition);
			object->disposition = NULL;
		}
		break;
	case HEADER_CONTENT_TYPE:
		/* never allow the removal of the Content-Type header */
		return FALSE;
	case HEADER_CONTENT_ID:
		g_free (object->content_id);
		object->content_id = NULL;
		break;
	default:
		break;
	}
	
	return g_mime_header_list_remove (object->headers, header);
}


/**
 * g_mime_object_remove_header:
 * @object: a #GMimeObject
 * @header: header name
 *
 * Removed the specified header if it exists.
 *
 * Returns: %TRUE if the header was removed or %FALSE if it could not
 * be found.
 **/
gboolean
g_mime_object_remove_header (GMimeObject *object, const char *header)
{
	g_return_val_if_fail (GMIME_IS_OBJECT (object), FALSE);
	g_return_val_if_fail (header != NULL, FALSE);
	
	return GMIME_OBJECT_GET_CLASS (object)->remove_header (object, header);
}


static char *
object_get_headers (GMimeObject *object)
{
	return g_mime_header_list_to_string (object->headers);
}


/**
 * g_mime_object_get_headers:
 * @object: a #GMimeObject
 *
 * Allocates a string buffer containing all of the MIME object's raw
 * headers.
 *
 * Returns: an allocated string containing all of the raw MIME headers.
 *
 * Note: The returned string will not be suitable for display.
 **/
char *
g_mime_object_get_headers (GMimeObject *object)
{
	g_return_val_if_fail (GMIME_IS_OBJECT (object), NULL);
	
	return GMIME_OBJECT_GET_CLASS (object)->get_headers (object);
}


static ssize_t
object_write_to_stream (GMimeObject *object, GMimeStream *stream)
{
	return -1;
}


/**
 * g_mime_object_write_to_stream:
 * @object: a #GMimeObject
 * @stream: stream
 *
 * Write the contents of the MIME object to @stream.
 *
 * Returns: the number of bytes written or %-1 on fail.
 **/
ssize_t
g_mime_object_write_to_stream (GMimeObject *object, GMimeStream *stream)
{
	g_return_val_if_fail (GMIME_IS_OBJECT (object), -1);
	g_return_val_if_fail (GMIME_IS_STREAM (stream), -1);
	
	return GMIME_OBJECT_GET_CLASS (object)->write_to_stream (object, stream);
}


static void
object_encode (GMimeObject *object, GMimeEncodingConstraint constraint)
{
	return;
}


/**
 * g_mime_object_encode:
 * @object: a #GMimeObject
 * @constraint: a #GMimeEncodingConstraint
 *
 * Calculates and sets the most efficient Content-Transfer-Encoding
 * for this #GMimeObject and all child parts based on the @constraint
 * provided.
 **/
void
g_mime_object_encode (GMimeObject *object, GMimeEncodingConstraint constraint)
{
	g_return_if_fail (GMIME_IS_OBJECT (object));
	
	GMIME_OBJECT_GET_CLASS (object)->encode (object, constraint);
}


/**
 * g_mime_object_to_string:
 * @object: a #GMimeObject
 *
 * Allocates a string buffer containing the contents of @object.
 *
 * Returns: an allocated string containing the contents of the mime
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
	g_byte_array_append (array, (unsigned char *) "", 1);
	str = (char *) array->data;
	g_byte_array_free (array, FALSE);
	
	return str;
}


/**
 * g_mime_object_get_header_list:
 * @object: a #GMimeObject
 *
 * Get the header list for @object.
 *
 * Returns: (transfer none): the #GMimeHeaderList for @object. Do not
 * free this pointer when you are done with it.
 **/
GMimeHeaderList *
g_mime_object_get_header_list (GMimeObject *object)
{
	g_return_val_if_fail (GMIME_IS_OBJECT (object), NULL);
	
	return object->headers;
}


static void
subtype_bucket_foreach (gpointer key, gpointer value, gpointer user_data)
{
	struct _subtype_bucket *bucket = value;
	
	g_free (bucket->subtype);
	g_free (bucket);
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

void
g_mime_object_type_registry_shutdown (void)
{
	g_hash_table_foreach (type_hash, type_bucket_foreach, NULL);
	g_hash_table_destroy (type_hash);
	type_hash = NULL;
}

void
g_mime_object_type_registry_init (void)
{
	if (type_hash)
		return;
	
	type_hash = g_hash_table_new (g_mime_strcase_hash, g_mime_strcase_equal);
}
