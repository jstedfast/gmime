/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2022 Jeffrey Stedfast
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
#include <errno.h>
#include <string.h>

#include "gmime-common.h"
#include "gmime-entity.h"
#include "gmime-error.h"
#include "gmime-visitor.h"
#include "gmime-parser.h"
#include "gmime-stream-cat.h"
#include "gmime-stream-file.h"
#include "gmime-stream-mem.h"
#include "gmime-internal.h"
#include "gmime-events.h"
#include "gmime-utils.h"
#include "gmime-parse-utils.h"


#include "gmime-object-private.h"

/**
 * SECTION: gmime-entity
 * @title: GMimeEntity
 * @short_description: Abstract MIME objects
 * @see_also:
 *
 * #GMimeEntity is an abstract class from which all message and MIME
 * parts are derived.
 *
 * Containers release their child references iteratively to avoid stack
 * overflow with deeply nested messages. Descendant releases may be deferred
 * until the outermost release completes. Subclass finalizers must not rely
 * on descendants having finalized when chaining to their parent finalizer.
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

static void _g_mime_entity_set_content_disposition (GMimeEntity *object, GMimeContentDisposition *disposition);
void _g_mime_entity_set_content_type (GMimeEntity *object, GMimeContentType *content_type);

static void g_mime_entity_class_init (GMimeEntityClass *klass);
static void g_mime_entity_init (GMimeEntity *object);
static void g_mime_entity_finalize (GObject *object);

static void object_header_added    (GMimeEntity *object, GMimeHeader *header);
static void object_header_changed  (GMimeEntity *object, GMimeHeader *header);
static void object_header_removed  (GMimeEntity *object, GMimeHeader *header);
static void object_headers_cleared (GMimeEntity *object);

static void object_set_content_type (GMimeEntity *object, GMimeContentType *content_type);
static char *object_get_headers (GMimeEntity *object, GMimeFormatOptions *options);
static ssize_t object_write_to_stream (GMimeEntity *object, GMimeFormatOptions *options, gboolean content_only, GMimeStream *stream);
static void object_accept (GMimeEntity *object, GMimeVisitor *visitor);

static void header_list_changed (gpointer sender, gpointer event_args, gpointer user_data);
static void content_type_changed (gpointer sender, gpointer args, gpointer user_data);
static void content_disposition_changed (gpointer sender, gpointer args, gpointer user_data);


static GHashTable *type_hash = NULL;

static GObjectClass *parent_class = NULL;

static GPrivate pending_entity_unrefs = G_PRIVATE_INIT (NULL);

void
_g_mime_entity_unref (GMimeEntity *entity)
{
	GQueue *pending = g_private_get (&pending_entity_unrefs);
	GQueue queue = G_QUEUE_INIT;

	if (pending) {
		/* Transfer the container's reference, without disposing shared children. */
		g_queue_push_tail (pending, entity);
		return;
	}

	g_private_set (&pending_entity_unrefs, &queue);
	g_object_unref (entity);

	while ((entity = g_queue_pop_head (&queue)))
		g_object_unref (entity);

	g_private_set (&pending_entity_unrefs, NULL);
}

static void
object_accept (GMimeEntity *object, GMimeVisitor *visitor)
{
	g_mime_visitor_visit_entity (visitor, object);
}

/**
 * g_mime_entity_accept:
 * @object: a #GMimeEntity
 * @visitor: a #GMimeVisitor
 *
 * Dispatches @object to the visitor's most specific visit method. Subclasses
 * can override the accept virtual method to select a different visit method.
 * The base implementation calls g_mime_visitor_visit_entity().
 **/
void
g_mime_entity_accept (GMimeEntity *object, GMimeVisitor *visitor)
{
	g_return_if_fail (GMIME_IS_ENTITY (object));
	g_return_if_fail (GMIME_IS_VISITOR (visitor));

	/* A visitor may remove this entity from its parent's live child list. */
	g_object_ref (object);
	GMIME_ENTITY_GET_CLASS (object)->accept (object, visitor);
	g_object_unref (object);
}



G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (GMimeEntity, g_mime_entity, G_TYPE_OBJECT)

GMimeEntityPrivate *
_g_mime_entity_get_private (GMimeEntity *object)
{
	return g_mime_entity_get_instance_private (object);
}


static void
g_mime_entity_class_init (GMimeEntityClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	
	parent_class = g_type_class_ref (G_TYPE_OBJECT);
	
	object_class->finalize = g_mime_entity_finalize;
	
	klass->header_added = object_header_added;
	klass->header_changed = object_header_changed;
	klass->header_removed = object_header_removed;
	klass->headers_cleared = object_headers_cleared;
	klass->set_content_type = object_set_content_type;
	klass->get_headers = object_get_headers;
	klass->write_to_stream = object_write_to_stream;
	klass->accept = object_accept;
	klass->prepare_with_max_line_length = NULL;
}

static void
g_mime_entity_init (GMimeEntity *object)
{
	GMimeEntityPrivate *priv;
	GMimeHeaderList *headers;
	
	headers = g_mime_header_list_new (g_mime_parser_options_get_default ());
	g_mime_event_add (headers->changed, (GMimeEventCallback) header_list_changed, object);
	priv = _g_mime_entity_get_private (object);

	priv->headers = headers;
	
	priv->ensure_newline = FALSE;
	priv->content_type = NULL;
	priv->disposition = NULL;
	priv->content_id = NULL;
	priv->content_base = NULL;
	priv->content_location = NULL;
	priv->content_base_loaded = FALSE;
	priv->content_location_loaded = FALSE;
}


static void
g_mime_entity_finalize (GObject *object)
{
	GMimeEntity *mime = (GMimeEntity *) object;
	GMimeEvent *event;
	GMimeEntityPrivate *priv = _g_mime_entity_get_private (mime);

	if (priv->content_type) {
		event = priv->content_type->changed;
		g_mime_event_remove (event, (GMimeEventCallback) content_type_changed, object);
		g_object_unref (priv->content_type);
	}
	
	if (priv->disposition) {
		event = priv->disposition->changed;
		g_mime_event_remove (event, (GMimeEventCallback) content_disposition_changed, object);
		g_object_unref (priv->disposition);
	}
	
	if (priv->headers) {
		event = priv->headers->changed;
		g_mime_event_remove (event, (GMimeEventCallback) header_list_changed, object);
		g_object_unref (priv->headers);
	}
	
	g_free (priv->content_id);
	g_free (priv->content_base);
	g_free (priv->content_location);
	
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

enum {
	HEADER_CONTENT_DISPOSITION,
	HEADER_CONTENT_TYPE,
	HEADER_CONTENT_ID,
	HEADER_CONTENT_BASE,
	HEADER_CONTENT_LOCATION,
	HEADER_UNKNOWN,
};

static char *content_headers[] = {
	"Content-Disposition",
	"Content-Type",
	"Content-Id",
	"Content-Base",
	"Content-Location",
};

static void
object_header_added (GMimeEntity *object, GMimeHeader *header)
{
	object_header_changed (object, header);
}

static void
object_header_changed (GMimeEntity *object, GMimeHeader *header)
{
	GMimeEntityPrivate *priv = _g_mime_entity_get_private (object);
	GMimeParserOptions *options = _g_mime_header_list_get_options (priv->headers);
	gboolean can_warn = g_mime_parser_options_get_warning_callback (options) != NULL;
	GMimeContentDisposition *disposition;
	GMimeContentType *content_type;
	const char *name, *value;
	guint i;
	
	name = g_mime_header_get_name (header);

	/* validate header if requested, caches the decoded value */
	if (G_UNLIKELY (can_warn))
		g_mime_header_get_value (header);
	
	if (g_ascii_strncasecmp (name, "Content-", 8) != 0)
		return;
	
	for (i = 0; i < G_N_ELEMENTS (content_headers); i++) {
		if (!g_ascii_strcasecmp (content_headers[i] + 8, name + 8))
			break;
	}
	
	switch (i) {
	case HEADER_CONTENT_DISPOSITION:
		value = g_mime_header_get_value (header);
		disposition = _g_mime_content_disposition_parse (options, value, header->offset);
		_g_mime_entity_set_content_disposition (object, disposition);
		g_object_unref (disposition);
		break;
	case HEADER_CONTENT_TYPE:
		value = g_mime_header_get_value (header);
		content_type = _g_mime_content_type_parse (options, value, header->offset);
		_g_mime_entity_set_content_type (object, content_type);
		g_object_unref (content_type);
		break;
	case HEADER_CONTENT_ID:
		value = g_mime_header_get_value (header);
		g_free (priv->content_id);
		priv->content_id = g_mime_utils_decode_message_id (value);
		break;
	case HEADER_CONTENT_BASE:
		g_clear_pointer (&priv->content_base, g_free);
		priv->content_base_loaded = FALSE;
		break;
	case HEADER_CONTENT_LOCATION:
		g_clear_pointer (&priv->content_location, g_free);
		priv->content_location_loaded = FALSE;
		break;
	}
}

static void
object_header_removed (GMimeEntity *object, GMimeHeader *header)
{
	GMimeEntityPrivate *priv;
	GMimeEvent *event;
	const char *name;
	guint i;
	
	name = g_mime_header_get_name (header);
	
	if (g_ascii_strncasecmp (name, "Content-", 8) != 0)
		return;
	
	for (i = 0; i < G_N_ELEMENTS (content_headers); i++) {
		if (!g_ascii_strcasecmp (content_headers[i] + 8, name + 8))
			break;
	}
	
	priv = _g_mime_entity_get_private (object);

	switch (i) {
	case HEADER_CONTENT_DISPOSITION:
		if (priv->disposition) {
			event = priv->disposition->changed;
			g_mime_event_remove (event, (GMimeEventCallback) content_disposition_changed, object);
			g_object_unref (priv->disposition);
			priv->disposition = NULL;
		}
		break;
	case HEADER_CONTENT_TYPE:
		/* never allow the removal of the Content-Type header */
		break;
	case HEADER_CONTENT_ID:
		g_free (priv->content_id);
		priv->content_id = NULL;
		break;
	case HEADER_CONTENT_BASE:
		g_clear_pointer (&priv->content_base, g_free);
		priv->content_base_loaded = FALSE;
		break;
	case HEADER_CONTENT_LOCATION:
		g_clear_pointer (&priv->content_location, g_free);
		priv->content_location_loaded = FALSE;
		break;
	}
}

static void
object_headers_cleared (GMimeEntity *object)
{
	GMimeEvent *event;
	GMimeEntityPrivate *priv = _g_mime_entity_get_private (object);

	if (priv->disposition) {
		event = priv->disposition->changed;
		g_mime_event_remove (event, (GMimeEventCallback) content_disposition_changed, object);
		g_object_unref (priv->disposition);
		priv->disposition = NULL;
	}
	
	g_free (priv->content_id);
	priv->content_id = NULL;
	g_clear_pointer (&priv->content_base, g_free);
	g_clear_pointer (&priv->content_location, g_free);
	priv->content_base_loaded = FALSE;
	priv->content_location_loaded = FALSE;
}

static void
header_list_changed (gpointer sender, gpointer event_args, gpointer user_data)
{
	GMimeHeaderListChangedEventArgs *args = event_args;
	GMimeEntity *object = user_data;

	// FIXME: add a header_inserted() API so that header_added() can be better optimized. See gmime-message.c:message_header_added()/process_header().
	switch (args->action) {
	case GMIME_HEADER_LIST_CHANGED_ACTION_ADDED:
	case GMIME_HEADER_LIST_CHANGED_ACTION_INSERTED:
		GMIME_ENTITY_GET_CLASS (object)->header_added (object, args->header);
		break;
	case GMIME_HEADER_LIST_CHANGED_ACTION_CHANGED:
		GMIME_ENTITY_GET_CLASS (object)->header_changed (object, args->header);
		break;
	case GMIME_HEADER_LIST_CHANGED_ACTION_REMOVED:
		GMIME_ENTITY_GET_CLASS (object)->header_removed (object, args->header);
		break;
	case GMIME_HEADER_LIST_CHANGED_ACTION_CLEARED:
		GMIME_ENTITY_GET_CLASS (object)->headers_cleared (object);
		break;
	}
}

void
_g_mime_entity_block_header_list_changed (GMimeEntity *object)
{
	GMimeEntityPrivate *priv = _g_mime_entity_get_private (object);

	g_mime_event_block (priv->headers->changed, (GMimeEventCallback) header_list_changed, object);
}

void
_g_mime_entity_unblock_header_list_changed (GMimeEntity *object)
{
	GMimeEntityPrivate *priv = _g_mime_entity_get_private (object);

	g_mime_event_unblock (priv->headers->changed, (GMimeEventCallback) header_list_changed, object);
}

static void
content_type_changed (gpointer sender, gpointer args, gpointer user_data)
{
	GMimeContentType *content_type = sender;
	GMimeEntity *object = user_data;
	GMimeEntityPrivate *priv;
	char *raw_value;
	
	raw_value = g_mime_content_type_encode (content_type, NULL);
	
	_g_mime_entity_block_header_list_changed (object);
	priv = _g_mime_entity_get_private (object);

	_g_mime_header_list_set (priv->headers, "Content-Type", raw_value);
	_g_mime_entity_unblock_header_list_changed (object);
	g_free (raw_value);
}

static void
content_disposition_changed (gpointer sender, gpointer args, gpointer user_data)
{
	GMimeContentDisposition *disposition = sender;
	GMimeEntity *object = user_data;
	GMimeEntityPrivate *priv;
	char *raw_value;
	
	_g_mime_entity_block_header_list_changed (object);
	
	priv = _g_mime_entity_get_private (object);

	if (disposition) {
		raw_value = g_mime_content_disposition_encode (disposition, NULL);
		_g_mime_header_list_set (priv->headers, "Content-Disposition", raw_value);
		g_free (raw_value);
	} else {
		g_mime_header_list_remove (priv->headers, "Content-Disposition");
	}
	
	_g_mime_entity_unblock_header_list_changed (object);
}


/**
 * g_mime_entity_register_type:
 * @type: mime type
 * @subtype: mime subtype
 * @object_type: object type
 *
 * Registers the object type @object_type for use with the
 * g_mime_entity_new_type() convenience function.
 *
 * Note: You may use the wildcard "*" to match any type and/or
 * subtype.
 **/
void
g_mime_entity_register_type (const char *type, const char *subtype, GType object_type)
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
	
	if ((sub = g_hash_table_lookup (bucket->subtype_hash, subtype))) {
		sub->object_type = object_type;
		return;
	}

	sub = g_new (struct _subtype_bucket, 1);
	sub->subtype = g_strdup (subtype);
	sub->object_type = object_type;
	g_hash_table_insert (bucket->subtype_hash, sub->subtype, sub);
}


/**
 * g_mime_entity_new:
 * @options: (nullable): a #GMimeParserOptions or %NULL
 * @content_type: a #GMimeContentType object
 *
 * Performs a lookup of registered #GMimeEntity subclasses, registered
 * using g_mime_entity_register_type(), to find an appropriate class
 * capable of handling MIME parts of the specified Content-Type. If no
 * class has been registered to handle that type, it looks for a
 * registered class that can handle @content_type's media type. If
 * that also fails, then it will use the generic part class,
 * #GMimePart.
 *
 * Returns: an appropriate #GMimeEntity registered to handle MIME
 * parts appropriate for @content_type.
 **/
GMimeEntity *
g_mime_entity_new (GMimeParserOptions *options, GMimeContentType *content_type)
{
	GMimeEntityPrivate *priv;
	struct _type_bucket *bucket;
	struct _subtype_bucket *sub;
	GMimeEntity *object;
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
	
	object = g_object_new (obj_type, NULL);
	priv = _g_mime_entity_get_private (object);

	_g_mime_header_list_set_options (priv->headers, options);
	
	g_mime_entity_set_content_type (object, content_type);
	
	return object;
}


/**
 * g_mime_entity_new_type:
 * @options: (nullable): a #GMimeParserOptions or %NULL
 * @type: mime type
 * @subtype: mime subtype
 *
 * Performs a lookup of registered #GMimeEntity subclasses, registered
 * using g_mime_entity_register_type(), to find an appropriate class
 * capable of handling MIME parts of type @type/@subtype. If no class
 * has been registered to handle that type, it looks for a registered
 * class that can handle @type. If that also fails, then it will use
 * the generic part class, #GMimePart.
 *
 * Returns: an appropriate #GMimeEntity registered to handle mime-types
 * of @type/@subtype.
 **/
GMimeEntity *
g_mime_entity_new_type (GMimeParserOptions *options, const char *type, const char *subtype)
{
	GMimeEntityPrivate *priv;
	struct _type_bucket *bucket;
	struct _subtype_bucket *sub;
	GMimeEntity *object;
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
	
	object = g_object_new (obj_type, NULL);
	priv = _g_mime_entity_get_private (object);

	_g_mime_header_list_set_options (priv->headers, options);
	
	return object;
}


/**
 * g_mime_entity_load:
 * @options: (nullable): parser options, or %NULL for defaults
 * @stream: the input stream at the start of the entity
 * @error: return location for a #GError, or %NULL
 *
 * Loads a MIME entity, copying its content into memory. The input need
 * not support seeking and is neither reset nor closed. The returned
 * entity remains usable after @stream is closed, modified or released.
 *
 * Unlike g_mime_parser_construct_part(), empty input is a parse error.
 * The input must start with entity headers; message-specific byte-order
 * mark handling is provided by g_mime_message_load().
 *
 * Returns: (nullable) (transfer full): the entity, or %NULL on failure.
 **/
GMimeEntity *
g_mime_entity_load (GMimeParserOptions *options, GMimeStream *stream, GError **error)
{
	return g_mime_entity_load_with_storage (options, stream, GMIME_CONTENT_STORAGE_MEMORY, error);
}


/**
 * g_mime_entity_load_with_storage:
 * @options: (nullable): parser options, or %NULL for defaults
 * @stream: input positioned at the start of the entity
 * @storage: storage for parsed MIME part content
 * @error: return location for a #GError, or %NULL
 *
 * Loads an entity using the requested content storage. Memory storage
 * behaves like g_mime_entity_load(). Source storage references leaf content
 * through substreams instead of copying it, and requires working tell,
 * seek and substream operations. Seekability is checked with a zero-offset
 * seek from the current position before reading. Unsupported operations
 * and I/O failures report %GMIME_ERROR rather than falling back to memory.
 *
 * The caller owns @stream in both modes. With source storage, keep it open
 * and unchanged while parsed content is in use. Content views retain it;
 * releasing the entity does not close a stream still owned by the caller.
 * Empty input is a parse error. The stream is neither reset nor closed.
 *
 * Returns: (nullable) (transfer full): the entity, or %NULL on failure.
 **/
GMimeEntity *
g_mime_entity_load_with_storage (GMimeParserOptions *options, GMimeStream *stream,
				GMimeContentStorage storage, GError **error)
{
	return _g_mime_parser_load (options, stream, storage, FALSE, error);
}


/**
 * g_mime_entity_load_from_file:
 * @options: (nullable): parser options, or %NULL for defaults
 * @filename: (type filename): the input file path
 * @error: return location for a #GError, or %NULL
 *
 * Loads a MIME entity from @filename using g_mime_entity_load(). The file
 * is opened for reading and closed before returning, on success or
 * failure. Content is copied into memory and does not retain the file.
 * Open, read, parse and close failures return %NULL with @error set.
 *
 * Returns: (nullable) (transfer full): the entity, or %NULL on failure.
 **/
GMimeEntity *
g_mime_entity_load_from_file (GMimeParserOptions *options, const char *filename, GError **error)
{
	return _g_mime_parser_load_from_file (options, filename, FALSE, error);
}


/**
 * g_mime_entity_load_with_content_type:
 * @options: (nullable): parser options, or %NULL for defaults
 * @content_type: the Content-Type of @content
 * @content: the body stream, positioned at the start of the content to load
 * @error: return location for a #GError, or %NULL
 *
 * Loads an entity from a body stream whose Content-Type was supplied
 * separately, such as an HTTP response body. A serialized Content-Type
 * header is prepended before parsing; @content_type is not retained or
 * modified. The parser selects the entity subclass from that header.
 *
 * Content is copied into memory. The returned entity remains usable after
 * @content is closed, modified or released. The input is read from its
 * current position without seeking, resetting or closing it, and need not
 * support seeking. Parser options and errors have the same semantics as
 * g_mime_parser_construct_part().
 *
 * Returns: (nullable) (transfer full): the loaded entity, or %NULL on failure.
 **/
GMimeEntity *
g_mime_entity_load_with_content_type (GMimeParserOptions *options, GMimeContentType *content_type,
				    GMimeStream *content, GError **error)
{
	GMimeFormatOptions *format;
	GMimeStream *chained, *stream;
	GMimeParser *parser;
	GMimeEntity *entity;
	char *encoded, *header;
	int errsv;

	g_return_val_if_fail (GMIME_IS_CONTENT_TYPE (content_type), NULL);
	g_return_val_if_fail (GMIME_IS_STREAM (content), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	format = g_mime_format_options_clone (NULL);
	g_mime_format_options_set_newline_format (format, GMIME_NEWLINE_FORMAT_DOS);
	encoded = g_mime_content_type_encode (content_type, format);
	header = g_strdup_printf ("Content-Type:%s\r\n", encoded);
	stream = g_mime_stream_mem_new_with_buffer (header, strlen (header));
	errsv = errno;
	g_mime_format_options_unref (format);
	g_free (encoded);
	g_free (header);

	if (!stream) {
		g_set_error_literal (error, GMIME_ERROR, errsv, "Encoded Content-Type header is too large.");
		errno = errsv;
		return NULL;
	}

	chained = g_mime_stream_cat_new ();
	g_mime_stream_cat_add_source ((GMimeStreamCat *) chained, stream);
	g_mime_stream_cat_add_source ((GMimeStreamCat *) chained, content);
	g_object_unref (stream);

	parser = g_mime_parser_new ();
	g_mime_parser_set_persist_stream (parser, FALSE);
	g_mime_parser_init_with_stream (parser, chained);
	entity = g_mime_parser_construct_part (parser, options, error);
	errsv = errno;
	g_object_unref (parser);
	g_object_unref (chained);
	errno = errsv;

	return entity;
}


static void
object_set_content_type (GMimeEntity *object, GMimeContentType *content_type)
{
	GMimeEntityPrivate *priv = _g_mime_entity_get_private (object);

	if (priv->content_type) {
		g_mime_event_remove (priv->content_type->changed, (GMimeEventCallback) content_type_changed, object);
		g_object_unref (priv->content_type);
	}
	
	g_mime_event_add (content_type->changed, (GMimeEventCallback) content_type_changed, object);
	priv->content_type = content_type;
	g_object_ref (content_type);
}


/**
 * _g_mime_entity_set_content_type:
 * @object: a #GMimeEntity
 * @content_type: a #GMimeContentType object
 *
 * Sets the content-type for the specified MIME object.
 *
 * Note: This method is meant for internal-use only and avoids
 * serialization of @content_type to the Content-Type header field.
 **/
void
_g_mime_entity_set_content_type (GMimeEntity *object, GMimeContentType *content_type)
{
	GMIME_ENTITY_GET_CLASS (object)->set_content_type (object, content_type);
}


/**
 * g_mime_entity_set_content_type:
 * @object: a #GMimeEntity
 * @content_type: a #GMimeContentType object
 *
 * Sets the content-type for the specified MIME object and then
 * serializes it to the Content-Type header field.
 **/
void
g_mime_entity_set_content_type (GMimeEntity *object, GMimeContentType *content_type)
{
	GMimeEntityPrivate *priv;

	g_return_if_fail (GMIME_IS_CONTENT_TYPE (content_type));
	g_return_if_fail (GMIME_IS_ENTITY (object));
	
	priv = _g_mime_entity_get_private (object);

	if (priv->content_type == content_type)
		return;
	
	GMIME_ENTITY_GET_CLASS (object)->set_content_type (object, content_type);
	
	content_type_changed (content_type, NULL, object);
}


/**
 * g_mime_entity_get_content_type:
 * @object: a #GMimeEntity
 *
 * Gets the #GMimeContentType object for the given MIME object or
 * %NULL on fail.
 *
 * Returns: (transfer none): the content-type object for the specified
 * MIME object.
 **/
GMimeContentType *
g_mime_entity_get_content_type (GMimeEntity *object)
{
	GMimeEntityPrivate *priv;

	g_return_val_if_fail (GMIME_IS_ENTITY (object), NULL);
	
	priv = _g_mime_entity_get_private (object);

	return priv->content_type;
}


/**
 * g_mime_entity_set_content_type_parameter:
 * @object: a #GMimeEntity
 * @name: param name
 * @value: param value
 *
 * Sets the content-type param @name to the value @value.
 *
 * Note: The @name string should be in US-ASCII while the @value
 * string should be in UTF-8.
 **/
void
g_mime_entity_set_content_type_parameter (GMimeEntity *object, const char *name, const char *value)
{
	GMimeEntityPrivate *priv;

	g_return_if_fail (GMIME_IS_ENTITY (object));
	g_return_if_fail (name != NULL);
	
	priv = _g_mime_entity_get_private (object);

	g_mime_content_type_set_parameter (priv->content_type, name, value);
}


/**
 * g_mime_entity_get_content_type_parameter:
 * @object: a #GMimeEntity
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
g_mime_entity_get_content_type_parameter (GMimeEntity *object, const char *name)
{
	GMimeEntityPrivate *priv;

	g_return_val_if_fail (GMIME_IS_ENTITY (object), NULL);
	g_return_val_if_fail (name != NULL, NULL);
	
	priv = _g_mime_entity_get_private (object);

	return g_mime_content_type_get_parameter (priv->content_type, name);
}


/**
 * g_mime_entity_get_content_disposition:
 * @object: a #GMimeEntity
 *
 * Gets the #GMimeContentDisposition for the specified MIME object.
 *
 * Returns: (transfer none): the #GMimeContentDisposition set on the
 * MIME object.
 **/
GMimeContentDisposition *
g_mime_entity_get_content_disposition (GMimeEntity *object)
{
	GMimeEntityPrivate *priv;

	g_return_val_if_fail (GMIME_IS_ENTITY (object), NULL);
	
	priv = _g_mime_entity_get_private (object);

	return priv->disposition;
}

/**
 * _g_mime_entity_set_content_disposition:
 * @object: a #GMimeEntity
 * @disposition: a #GMimeContentDisposition object
 *
 * Set the content disposition for the specified mime part.
 *
 * Note: This method is meant for internal-use only and avoids
 * serialization of @disposition to the Content-Disposition header
 * field.
 **/
static void
_g_mime_entity_set_content_disposition (GMimeEntity *object, GMimeContentDisposition *disposition)
{
	GMimeEntityPrivate *priv = _g_mime_entity_get_private (object);

	if (priv->disposition) {
		g_mime_event_remove (priv->disposition->changed, (GMimeEventCallback) content_disposition_changed, object);
		g_object_unref (priv->disposition);
	}
	
	g_mime_event_add (disposition->changed, (GMimeEventCallback) content_disposition_changed, object);
	priv->disposition = disposition;
	g_object_ref (disposition);
}


/**
 * g_mime_entity_set_content_disposition:
 * @object: a #GMimeEntity
 * @disposition: a #GMimeContentDisposition object
 *
 * Set the content disposition for the specified mime part and then
 * serializes it to the Content-Disposition header field.
 **/
void
g_mime_entity_set_content_disposition (GMimeEntity *object, GMimeContentDisposition *disposition)
{
	GMimeEntityPrivate *priv;

	g_return_if_fail (GMIME_IS_CONTENT_DISPOSITION (disposition));
	g_return_if_fail (GMIME_IS_ENTITY (object));
	
	priv = _g_mime_entity_get_private (object);

	if (priv->disposition == disposition)
		return;
	
	_g_mime_entity_set_content_disposition (object, disposition);
	
	content_disposition_changed (disposition, NULL, object);
}


/**
 * g_mime_entity_set_disposition:
 * @object: a #GMimeEntity
 * @disposition: disposition ("attachment" or "inline")
 *
 * Sets the disposition to @disposition which may be one of
 * #GMIME_DISPOSITION_ATTACHMENT or #GMIME_DISPOSITION_INLINE or, by
 * your choice, any other string which would indicate how the MIME
 * part should be displayed by the MUA.
 **/
void
g_mime_entity_set_disposition (GMimeEntity *object, const char *disposition)
{
	GMimeEntityPrivate *priv;
	GMimeContentDisposition *disp;
	
	g_return_if_fail (GMIME_IS_ENTITY (object));
	g_return_if_fail (disposition != NULL);
	
	priv = _g_mime_entity_get_private (object);

	if (priv->disposition) {
		g_mime_content_disposition_set_disposition (priv->disposition, disposition);
		return;
	}
	
	disp = g_mime_content_disposition_new ();
	g_mime_content_disposition_set_disposition (disp, disposition);
	g_mime_entity_set_content_disposition (object, disp);
	g_object_unref (disp);
}


/**
 * g_mime_entity_get_disposition:
 * @object: a #GMimeEntity
 *
 * Gets the MIME object's disposition if set or %NULL otherwise.
 *
 * Returns: the disposition string which is probably one of
 * #GMIME_DISPOSITION_ATTACHMENT or #GMIME_DISPOSITION_INLINE.
 **/
const char *
g_mime_entity_get_disposition (GMimeEntity *object)
{
	GMimeEntityPrivate *priv;

	g_return_val_if_fail (GMIME_IS_ENTITY (object), NULL);
	
	priv = _g_mime_entity_get_private (object);

	if (priv->disposition)
		return g_mime_content_disposition_get_disposition (priv->disposition);
	
	return NULL;
}


/**
 * g_mime_entity_is_attachment:
 * @object: a #GMimeEntity
 *
 * Determines whether the Content-Disposition is "attachment", ignoring
 * ASCII case. A filename alone does not make an entity an attachment.
 *
 * Returns: %TRUE for an attachment, or %FALSE otherwise.
 **/
gboolean
g_mime_entity_is_attachment (GMimeEntity *object)
{
	GMimeEntityPrivate *priv;

	g_return_val_if_fail (GMIME_IS_ENTITY (object), FALSE);

	priv = _g_mime_entity_get_private (object);

	return priv->disposition != NULL && g_mime_content_disposition_is_attachment (priv->disposition);
}

/**
 * g_mime_entity_set_is_attachment:
 * @object: a #GMimeEntity
 * @attachment: whether the entity is an attachment
 *
 * Sets Content-Disposition to "attachment" when @attachment is %TRUE and
 * the entity is not already an attachment. Setting %FALSE changes an
 * attachment to "inline", but leaves missing or other dispositions alone.
 * Existing parameters are retained. Assigning the current attachment
 * status preserves raw spelling and duplicate headers.
 **/
void
g_mime_entity_set_is_attachment (GMimeEntity *object, gboolean attachment)
{
	g_return_if_fail (GMIME_IS_ENTITY (object));

	if (attachment) {
		if (!g_mime_entity_is_attachment (object))
			g_mime_entity_set_disposition (object, GMIME_DISPOSITION_ATTACHMENT);
	} else if (g_mime_entity_is_attachment (object)) {
		g_mime_entity_set_disposition (object, GMIME_DISPOSITION_INLINE);
	}
}

/**
 * g_mime_entity_set_content_disposition_parameter:
 * @object: a #GMimeEntity
 * @name: parameter name
 * @value: parameter value
 *
 * Add a content-disposition parameter to the specified mime part.
 *
 * Note: The @name string should be in US-ASCII while the @value
 * string should be in UTF-8.
 **/
void
g_mime_entity_set_content_disposition_parameter (GMimeEntity *object, const char *name, const char *value)
{
	GMimeEntityPrivate *priv;
	GMimeContentDisposition *disposition;
	
	g_return_if_fail (GMIME_IS_ENTITY (object));
	g_return_if_fail (name != NULL);
	
	priv = _g_mime_entity_get_private (object);

	if (!priv->disposition) {
		disposition = g_mime_content_disposition_new ();
		_g_mime_entity_set_content_disposition (object, disposition);
		g_object_unref (disposition);
	}
	
	g_mime_content_disposition_set_parameter (priv->disposition, name, value);
}


/**
 * g_mime_entity_get_content_disposition_parameter:
 * @object: a #GMimeEntity
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
g_mime_entity_get_content_disposition_parameter (GMimeEntity *object, const char *name)
{
	GMimeEntityPrivate *priv;

	g_return_val_if_fail (GMIME_IS_ENTITY (object), NULL);
	g_return_val_if_fail (name != NULL, NULL);
	
	priv = _g_mime_entity_get_private (object);

	if (!priv->disposition)
		return NULL;
	
	return g_mime_content_disposition_get_parameter (priv->disposition, name);
}


/**
 * g_mime_entity_set_content_id:
 * @object: a #GMimeEntity
 * @content_id: (nullable): a UTF-8 content-id, with or without angle brackets
 *
 * Sets the Content-Id after normalizing a complete message-id, or removes
 * all Content-Id headers if @content_id is %NULL. Invalid values are
 * programming errors and leave the entity unchanged. Assigning the current
 * ID preserves the original raw header.
 **/
void
g_mime_entity_set_content_id (GMimeEntity *object, const char *content_id)
{
	GMimeEntityPrivate *priv;
	char *id = NULL, *msgid = NULL;
	
	g_return_if_fail (GMIME_IS_ENTITY (object));

	priv = _g_mime_entity_get_private (object);

	if (content_id) {
		if (!g_strcmp0 (priv->content_id, content_id))
			return;

		id = _g_mime_parse_message_id_value (content_id);
		g_return_if_fail (id != NULL);
		msgid = g_strdup_printf ("<%s>", id);
	}
	
	g_free (priv->content_id);
	priv->content_id = id;
	
	_g_mime_entity_block_header_list_changed (object);

	if (msgid)
		g_mime_header_list_set (priv->headers, "Content-Id", msgid, NULL);
	else
		g_mime_header_list_remove_all (priv->headers, "Content-Id");

	_g_mime_entity_unblock_header_list_changed (object);
	g_free (msgid);
}


/**
 * g_mime_entity_get_content_id:
 * @object: a #GMimeEntity
 *
 * Gets the Content-Id of the MIME object or NULL if one is not set.
 *
 * Returns: (nullable): the Content-Id, or %NULL if not set.
 **/
const char *
g_mime_entity_get_content_id (GMimeEntity *object)
{
	GMimeEntityPrivate *priv;

	g_return_val_if_fail (GMIME_IS_ENTITY (object), NULL);
	
	priv = _g_mime_entity_get_private (object);

	return priv->content_id;
}

gboolean
_g_mime_is_content_uri (const char *uri, gboolean absolute)
{
	const guchar *p;

	if (!g_utf8_validate (uri, -1, NULL) || (absolute && !g_uri_peek_scheme (uri)))
		return FALSE;

	for (p = (const guchar *) uri; *p; p++) {
		if (*p <= 0x20 || *p == 0x7f || strchr ("<>\"\\^`{|}", *p))
			return FALSE;
	}

	return g_uri_split (uri, G_URI_FLAGS_ENCODED, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
}

/* Loaded-state bookkeeping stays in the accessors because bitfields cannot
 * be addressed. These helpers only load or replace the cached URI strings. */
static void
load_content_uri (GMimeEntity *object, const char *name, char **cache, gboolean absolute)
{
	GMimeHeader *header;
	const char *value;
	char *uri;
	GMimeEntityPrivate *priv = _g_mime_entity_get_private (object);

	header = g_mime_header_list_get_header (priv->headers, name);
	if (header && (value = g_mime_header_get_value (header))) {
		uri = g_mime_strdup_trim (value);

		if (_g_mime_is_content_uri (uri, absolute))
			*cache = uri;
		else
			g_free (uri);
	}
}

static void
set_content_uri (GMimeEntity *object, const char *name, const char *uri, char **cache)
{
	GMimeEntityPrivate *priv;
	char *copy;

	copy = g_strdup (uri);
	g_free (*cache);
	*cache = copy;

	_g_mime_entity_block_header_list_changed (object);

	priv = _g_mime_entity_get_private (object);

	if (copy)
		g_mime_header_list_set (priv->headers, name, copy, NULL);
	else
		g_mime_header_list_remove_all (priv->headers, name);

	_g_mime_entity_unblock_header_list_changed (object);
}

/**
 * g_mime_entity_set_content_base:
 * @object: a #GMimeEntity
 * @uri: (nullable): an absolute UTF-8 URI, or %NULL to remove the header
 *
 * Sets Content-Base, replacing duplicate headers. Invalid URI strings and
 * relative references are programming errors and leave the entity unchanged.
 * URI strings are validated using GLib without canonicalizing their spelling.
 * Unescaped ASCII whitespace, controls, and unsafe delimiters are rejected.
 *
 * Assigning the cached string preserves raw headers and duplicates. Setting
 * %NULL before the first read removes even malformed headers; after a read
 * returning %NULL, assigning %NULL is a no-op until matching headers change.
 **/
void
g_mime_entity_set_content_base (GMimeEntity *object, const char *uri)
{
	GMimeEntityPrivate *priv;

	g_return_if_fail (GMIME_IS_ENTITY (object));
	g_return_if_fail (uri == NULL || _g_mime_is_content_uri (uri, TRUE));

	priv = _g_mime_entity_get_private (object);

	if (priv->content_base_loaded && !g_strcmp0 (priv->content_base, uri))
		return;

	priv->content_base_loaded = TRUE;
	set_content_uri (object, "Content-Base", uri, &priv->content_base);
}

/**
 * g_mime_entity_get_content_base:
 * @object: a #GMimeEntity
 *
 * Gets the absolute URI from the first Content-Base header. After header
 * decoding, only surrounding ASCII linear whitespace (SP, HTAB, CR and LF)
 * is trimmed. Invalid or relative values return %NULL without falling back
 * to later duplicates. Reads preserve raw headers; matching header edits
 * invalidate the cached value.
 *
 * Returns: (nullable): a borrowed URI string, or %NULL if absent or invalid.
 **/
const char *
g_mime_entity_get_content_base (GMimeEntity *object)
{
	GMimeEntityPrivate *priv;

	g_return_val_if_fail (GMIME_IS_ENTITY (object), NULL);

	priv = _g_mime_entity_get_private (object);

	if (!priv->content_base_loaded) {
		load_content_uri (object, "Content-Base", &priv->content_base, TRUE);
		priv->content_base_loaded = TRUE;
	}

	return priv->content_base;
}

/**
 * g_mime_entity_set_content_location:
 * @object: a #GMimeEntity
 * @uri: (nullable): an absolute or relative UTF-8 URI, or %NULL to remove the header
 *
 * Sets Content-Location, replacing duplicate headers. An empty string is a
 * valid relative reference. Validation and cached-string no-op behavior are
 * the same as g_mime_entity_set_content_base(), but relative references are
 * accepted. URI spelling and percent escapes are preserved.
 **/
void
g_mime_entity_set_content_location (GMimeEntity *object, const char *uri)
{
	GMimeEntityPrivate *priv;

	g_return_if_fail (GMIME_IS_ENTITY (object));
	g_return_if_fail (uri == NULL || _g_mime_is_content_uri (uri, FALSE));

	priv = _g_mime_entity_get_private (object);

	if (priv->content_location_loaded && !g_strcmp0 (priv->content_location, uri))
		return;

	priv->content_location_loaded = TRUE;
	set_content_uri (object, "Content-Location", uri, &priv->content_location);
}

/**
 * g_mime_entity_get_content_location:
 * @object: a #GMimeEntity
 *
 * Gets the absolute or relative URI from the first Content-Location header.
 * Trimming, invalid-value handling, and cache invalidation are the same as
 * g_mime_entity_get_content_base(). An empty relative reference is returned
 * as an empty string, not %NULL.
 *
 * Returns: (nullable): a borrowed URI string, or %NULL if absent or invalid.
 **/
const char *
g_mime_entity_get_content_location (GMimeEntity *object)
{
	GMimeEntityPrivate *priv;

	g_return_val_if_fail (GMIME_IS_ENTITY (object), NULL);

	priv = _g_mime_entity_get_private (object);

	if (!priv->content_location_loaded) {
		load_content_uri (object, "Content-Location", &priv->content_location, FALSE);
		priv->content_location_loaded = TRUE;
	}

	return priv->content_location;
}


/**
 * g_mime_entity_prepend_header:
 * @object: a #GMimeEntity
 * @header: header name
 * @value: header value
 * @charset: (nullable): a charset
 *
 * Prepends a new header to the header list.
 **/
void
g_mime_entity_prepend_header (GMimeEntity *object, const char *header, const char *value, const char *charset)
{
	GMimeEntityPrivate *priv;

	g_return_if_fail (GMIME_IS_ENTITY (object));
	g_return_if_fail (header != NULL);
	
	priv = _g_mime_entity_get_private (object);

	g_mime_header_list_prepend (priv->headers, header, value, charset);
}


void
_g_mime_entity_append_header (GMimeEntity *object, const char *header, const char *raw_name,
			      const char *raw_value, gint64 offset)
{
	GMimeEntityPrivate *priv = _g_mime_entity_get_private (object);

	_g_mime_header_list_append (priv->headers, header, raw_name, raw_value, offset);
}


/**
 * g_mime_entity_append_header:
 * @object: a #GMimeEntity
 * @header: header name
 * @value: header value
 * @charset: (nullable): a charset
 *
 * Appends a new header to the header list.
 **/
void
g_mime_entity_append_header (GMimeEntity *object, const char *header, const char *value, const char *charset)
{
	GMimeEntityPrivate *priv;

	g_return_if_fail (GMIME_IS_ENTITY (object));
	g_return_if_fail (header != NULL);
	
	priv = _g_mime_entity_get_private (object);

	g_mime_header_list_append (priv->headers, header, value, charset);
}


/**
 * g_mime_entity_set_header:
 * @object: a #GMimeEntity
 * @header: header name
 * @value: header value
 * @charset: (nullable): a charset
 *
 * Sets a header to the specified value.
 **/
void
g_mime_entity_set_header (GMimeEntity *object, const char *header, const char *value, const char *charset)
{
	GMimeEntityPrivate *priv;

	g_return_if_fail (GMIME_IS_ENTITY (object));
	g_return_if_fail (header != NULL);
	
	priv = _g_mime_entity_get_private (object);

	g_mime_header_list_set (priv->headers, header, value, charset);
}


/**
 * g_mime_entity_get_header:
 * @object: a #GMimeEntity
 * @header: header name
 *
 * Gets the value of the first header with the specified name.
 *
 * Returns: (nullable): the value of the requested header if it
 * exists or %NULL otherwise.
 **/
const char *
g_mime_entity_get_header (GMimeEntity *object, const char *header)
{
	GMimeEntityPrivate *priv;
	GMimeHeader *hdr;
	
	g_return_val_if_fail (GMIME_IS_ENTITY (object), NULL);
	
	priv = _g_mime_entity_get_private (object);

	if (!(hdr = g_mime_header_list_get_header (priv->headers, header)))
		return NULL;
	
	return g_mime_header_get_value (hdr);
}


/**
 * g_mime_entity_remove_header:
 * @object: a #GMimeEntity
 * @header: header name
 *
 * Removed the specified header if it exists.
 *
 * Returns: %TRUE if the header was removed or %FALSE if it could not
 * be found.
 **/
gboolean
g_mime_entity_remove_header (GMimeEntity *object, const char *header)
{
	GMimeEntityPrivate *priv;

	g_return_val_if_fail (GMIME_IS_ENTITY (object), FALSE);
	
	priv = _g_mime_entity_get_private (object);

	return g_mime_header_list_remove (priv->headers, header);
}


static char *
object_get_headers (GMimeEntity *object, GMimeFormatOptions *options)
{
	GMimeEntityPrivate *priv = _g_mime_entity_get_private (object);

	return g_mime_header_list_to_string (priv->headers, options);
}


/**
 * g_mime_entity_get_headers:
 * @object: a #GMimeEntity
 * @options: (nullable): a #GMimeFormatOptions or %NULL
 *
 * Allocates a string buffer containing all of the MIME object's raw
 * headers.
 *
 * Returns: (nullable): an allocated string containing all of the raw MIME
 * headers, or %NULL on failure.
 *
 * Note: The returned string will not be suitable for display.
 **/
char *
g_mime_entity_get_headers (GMimeEntity *object, GMimeFormatOptions *options)
{
	g_return_val_if_fail (GMIME_IS_ENTITY (object), NULL);
	
	return GMIME_ENTITY_GET_CLASS (object)->get_headers (object, options);
}


static ssize_t
object_write_to_stream (GMimeEntity *object, GMimeFormatOptions *options, gboolean content_only, GMimeStream *stream)
{
	return -1;
}


/**
 * g_mime_entity_write_to_stream:
 * @object: a #GMimeEntity
 * @options: (nullable): a #GMimeFormatOptions or %NULL
 * @stream: stream
 *
 * Write the headers and content of the MIME object to @stream.
 *
 * Returns: the number of bytes written or %-1 on fail.
 **/
ssize_t
g_mime_entity_write_to_stream (GMimeEntity *object, GMimeFormatOptions *options, GMimeStream *stream)
{
	g_return_val_if_fail (GMIME_IS_ENTITY (object), -1);
	g_return_val_if_fail (GMIME_IS_STREAM (stream), -1);
	
	return GMIME_ENTITY_GET_CLASS (object)->write_to_stream (object, options, FALSE, stream);
}


/**
 * g_mime_entity_write_content_to_stream:
 * @object: a #GMimeEntity
 * @options: (nullable): a #GMimeFormatOptions or %NULL
 * @stream: stream
 *
 * Write only the content of the MIME object to @stream.
 *
 * Returns: the number of bytes written or %-1 on fail.
 **/
ssize_t
g_mime_entity_write_content_to_stream (GMimeEntity *object, GMimeFormatOptions *options, GMimeStream *stream)
{
	g_return_val_if_fail (GMIME_IS_ENTITY (object), -1);
	g_return_val_if_fail (GMIME_IS_STREAM (stream), -1);
	
	return GMIME_ENTITY_GET_CLASS (object)->write_to_stream (object, options, TRUE, stream);
}

static gboolean
entity_write_to_file (GMimeEntity *object, GMimeFormatOptions *options, const char *filename,
		      gboolean content_only, GError **error)
{
	const char *operation = NULL;
	GMimeStream *stream;
	ssize_t nwritten;
	int errsv = 0;

	g_return_val_if_fail (GMIME_IS_ENTITY (object), FALSE);
	g_return_val_if_fail (filename != NULL, FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	if (!(stream = g_mime_stream_file_open (filename, "wb", error)))
		return FALSE;

	errno = 0;
	nwritten = content_only ? g_mime_entity_write_content_to_stream (object, options, stream) :
		g_mime_entity_write_to_stream (object, options, stream);
	if (nwritten < 0) {
		operation = "write";
		errsv = errno;
	} else if (g_mime_stream_flush (stream) < 0) {
		operation = "flush";
		errsv = errno;
	}

	/* Closing must not replace an earlier write or flush failure. */
	errno = 0;
	if (g_mime_stream_close (stream) < 0 && !operation) {
		operation = "close";
		errsv = errno;
	}
	g_object_unref (stream);

	if (operation) {
		g_set_error (error, GMIME_ERROR, errsv ? errsv : GMIME_ERROR_GENERAL,
			     "Failed to %s `%s': %s", operation, filename,
			     errsv ? g_strerror (errsv) : "unknown I/O error");
		errno = errsv;
		return FALSE;
	}

	return TRUE;
}


/**
 * g_mime_entity_write_to_file:
 * @object: a #GMimeEntity
 * @options: (nullable): formatting options, or %NULL for defaults
 * @filename: (type filename): the output file path
 * @error: return location for a #GError, or %NULL
 *
 * Writes the entity's headers and content to @filename using the same
 * virtual writer as g_mime_entity_write_to_stream(). Also accepts a
 * #GMimeMessage to write a complete message.
 *
 * Creates the file or truncates an existing file before serialization,
 * then flushes and closes it. Failures leave the created or partially
 * written file in place; the operation is not atomic. Do not use the
 * file backing the entity's source content as the destination.
 *
 * Open, write, flush and close failures report %GMIME_ERROR with the
 * corresponding errno code, or %GMIME_ERROR_GENERAL if none is supplied.
 * Cleanup preserves the first failure. The destination is closed on both
 * success and failure, even if a custom writer retains the stream.
 *
 * Returns: %TRUE on success, or %FALSE on failure.
 **/
gboolean
g_mime_entity_write_to_file (GMimeEntity *object, GMimeFormatOptions *options,
			   const char *filename, GError **error)
{
	return entity_write_to_file (object, options, filename, FALSE, error);
}


/**
 * g_mime_entity_write_content_to_file:
 * @object: a #GMimeEntity
 * @options: (nullable): formatting options, or %NULL for defaults
 * @filename: (type filename): the output file path
 * @error: return location for a #GError, or %NULL
 *
 * Writes only the entity's content to @filename, without its headers.
 * Uses g_mime_entity_write_content_to_stream() semantics and the same
 * create/truncate, flush, close and error handling as
 * g_mime_entity_write_to_file().
 *
 * Returns: %TRUE on success, or %FALSE on failure.
 **/
gboolean
g_mime_entity_write_content_to_file (GMimeEntity *object, GMimeFormatOptions *options,
				   const char *filename, GError **error)
{
	return entity_write_to_file (object, options, filename, TRUE, error);
}


/**
 * g_mime_entity_prepare:
 * @object: a #GMimeEntity
 * @constraint: a #GMimeEncodingConstraint
 * @error: return location for a #GError, or %NULL
 *
 * Prepares this entity and its children for transport using a maximum line
 * length of 78. Equivalent to g_mime_entity_prepare_with_max_line_length()
 * with that limit.
 *
 * Returns: %TRUE on success, or %FALSE on failure.
 **/
gboolean
g_mime_entity_prepare (GMimeEntity *object, GMimeEncodingConstraint constraint, GError **error)
{
	return g_mime_entity_prepare_with_max_line_length (object, constraint, 78, error);
}


/**
 * g_mime_entity_prepare_with_max_line_length:
 * @object: a #GMimeEntity
 * @constraint: a #GMimeEncodingConstraint
 * @max_line_length: maximum line length, excluding CRLF, from 60 through 998
 * @error: return location for a #GError, or %NULL
 *
 * Prepares an entity tree for transport using each entity's
 * prepare_with_max_line_length virtual method. Parts use
 * g_mime_part_prepare_with_max_line_length(); multiparts prepare their children
 * in order; message parts prepare their encapsulated messages. Signed
 * multiparts do not modify or analyze their children.
 *
 * Messages add MIME-Version: 1.0 when their body has headers but their first
 * MIME-Version header is absent or invalid, then prepare the body.
 *
 * An invalid line limit reports %GMIME_ERROR with %EINVAL before virtual
 * dispatch or any header changes. Constraint validation otherwise follows
 * the leaf-part policy, including its safe-encoding fast paths. A subclass
 * without a prepare implementation reports %GMIME_ERROR_NOT_SUPPORTED.
 *
 * Preparation stops at the first failure. Earlier child changes and message
 * MIME-Version updates are retained, matching MimeKit's preparation order;
 * this is not an atomic operation on the entire tree. The failing leaf
 * preserves its transfer headers and previous encoder limit.
 *
 * Reports analysis failures and carries the requested encoder line limit
 * into serialization.
 *
 * Returns: %TRUE on success, or %FALSE on failure.
 **/
gboolean
g_mime_entity_prepare_with_max_line_length (GMimeEntity *object, GMimeEncodingConstraint constraint,
					  guint max_line_length, GError **error)
{
	GMimeEntityClass *klass;
	gboolean prepared;

	g_return_val_if_fail (GMIME_IS_ENTITY (object), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	/* Validate even for empty containers and signed entities whose prepare
	 * implementation deliberately avoids inspecting their children. */
	if (max_line_length < 60 || max_line_length > 998) {
		g_set_error_literal (error, GMIME_ERROR, EINVAL, "Maximum line length must be between 60 and 998");
		return FALSE;
	}

	klass = GMIME_ENTITY_GET_CLASS (object);
	if (!klass->prepare_with_max_line_length) {
		g_set_error (error, GMIME_ERROR, GMIME_ERROR_NOT_SUPPORTED,
			     "%s does not implement transport preparation", G_OBJECT_TYPE_NAME (object));
		return FALSE;
	}

	/* Header callbacks may detach the entity from the tree that owns it. */
	g_object_ref (object);
	prepared = klass->prepare_with_max_line_length (object, constraint, max_line_length, error);
	g_object_unref (object);

	return prepared;
}


/**
 * g_mime_entity_to_string:
 * @object: a #GMimeEntity
 * @options: (nullable): a #GMimeFormatOptions or %NULL
 *
 * Allocates a string buffer containing the contents of @object.
 *
 * Returns: an allocated string containing the contents of the mime
 * object.
 **/
char *
g_mime_entity_to_string (GMimeEntity *object, GMimeFormatOptions *options)
{
	GMimeStream *stream;
	GByteArray *array;
	char *str;
	
	g_return_val_if_fail (GMIME_IS_ENTITY (object), NULL);
	
	array = g_byte_array_new ();
	stream = g_mime_stream_mem_new ();
	g_mime_stream_mem_set_byte_array (GMIME_STREAM_MEM (stream), array);
	
	g_mime_entity_write_to_stream (object, options, stream);
	
	g_object_unref (stream);
	g_byte_array_append (array, (unsigned char *) "", 1);
	str = (char *) array->data;
	g_byte_array_free (array, FALSE);
	
	return str;
}


/**
 * g_mime_entity_get_header_list:
 * @object: a #GMimeEntity
 *
 * Get the header list for @object.
 *
 * Returns: (transfer none): the #GMimeHeaderList for @object. Do not
 * free this pointer when you are done with it.
 **/
GMimeHeaderList *
g_mime_entity_get_header_list (GMimeEntity *object)
{
	GMimeEntityPrivate *priv;

	g_return_val_if_fail (GMIME_IS_ENTITY (object), NULL);
	
	priv = _g_mime_entity_get_private (object);

	return priv->headers;
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
g_mime_entity_type_registry_shutdown (void)
{
	g_hash_table_foreach (type_hash, type_bucket_foreach, NULL);
	g_hash_table_destroy (type_hash);
	type_hash = NULL;
}

void
g_mime_entity_type_registry_init (void)
{
	if (type_hash)
		return;
	
	type_hash = g_hash_table_new (g_mime_strcase_hash, g_mime_strcase_equal);
}


/**
 * g_mime_entity_get_autocrypt_headers:
 * @mime_part: a #GMimeEntity
 * @effective_date: a #GDateTime object
 * @matchheader: the header we want to match
 * @addresses: a #GMimeInternetAddressList
 * @keep_incomplete: %TRUE if the we should include incompletes
 *
 * Creates a new #GMimeAutocryptHeaderList of relevant headers of the
 * given type based on the @addresses of an @mime_part.
 * 
 * Each header in the returned list will:
 *
 *  - have a valid address
 *  - be of the type requested
 *  - be complete
 *
 * If no Autocrypt header is found for an address, no
 * #GMimeAutocryptHeader will be in the list associated with that e-mail address.
 *
 * Note that the following types of Autocrypt headers will not be
 * returned by this function:
 *
 *  - headers of an unrequested type
 *  - headers that do not match an address in "From:"
 *  - unparseable headers
 *  - headers with unknown critical attributes
 *  - duplicate valid headers for a given address
 * 
 * On error (e.g. if this version of GMime cannot handle the requested
 * Autocrypt type, or if a parameter is missing or malformed), returns
 * %NULL
 *
 * The returned Autocrypt headers will have it effective_date set to
 * @effective_date
 *
 * if @keep_incomplete isn't set, incompletes are removed
 *
 * Returns: (nullable) (transfer full): a new #GMimeAutocryptHeaderList object,
 * or %NULL on error.
 **/
GMimeAutocryptHeaderList *
g_mime_entity_get_autocrypt_headers (GMimeEntity *mime_part, GDateTime *effective_date,
				     const char *matchheader, GMimeInternetAddressList *addresses,
				     gboolean keep_incomplete)
{
	g_return_val_if_fail (GMIME_IS_ENTITY (mime_part), NULL);

	int i;

	GMimeAutocryptHeaderList *ret = g_mime_autocrypt_header_list_new ();
	guint count = g_mime_autocrypt_header_list_add_missing_addresses (ret, addresses);
	if (!count)
		return ret;

	/* scan for Autocrypt headers whose addr= attribute matches
	 * the From: header. */
	
	GMimeHeaderList *headers = g_mime_entity_get_header_list(mime_part);
	for (i = 0; i < g_mime_header_list_get_count (headers); i++) {
		GMimeHeader *header = g_mime_header_list_get_header_at (headers, i);
		if (g_ascii_strcasecmp (matchheader, header->name) == 0) {
			GMimeAutocryptHeader *ah = g_mime_autocrypt_header_new_from_string (g_mime_header_get_value (header));
			if (!ah || ! g_mime_autocrypt_header_is_complete (ah))
				goto done;
			g_mime_autocrypt_header_set_effective_date (ah, effective_date);
			GMimeAutocryptHeader *prev = g_mime_autocrypt_header_list_get_header_for_address (ret, ah->address);
			if (!prev) /* not a valid address (was not in From:) */
				goto done;
			if (g_mime_autocrypt_header_is_complete (prev)) {
				/* this is a duplicate (we use effective_date=NULL as an internal marker for this) */
				g_mime_autocrypt_header_set_effective_date (prev, NULL);
			} else {
				g_mime_autocrypt_header_clone (prev, ah);
			}
		done:
			if (ah)
				g_object_unref (ah);
		}
			
	}
	for (i = 0; i < g_mime_autocrypt_header_list_get_count (ret); i++) {
		GMimeAutocryptHeader *ah = g_mime_autocrypt_header_list_get_header_at (ret, i);
		/* drop keydata from duplicates */
		if (ah->effective_date == NULL) {
			g_mime_autocrypt_header_set_keydata (ah, NULL);
			g_mime_autocrypt_header_set_effective_date (ah, effective_date);
		}
	}

	if (!keep_incomplete)
		g_mime_autocrypt_header_list_remove_incomplete (ret);
	return ret;
}
