/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2026 Jeffrey Stedfast
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

#include <string.h>
#include <errno.h>

#include "gmime-multipart-related.h"
#include "gmime-visitor.h"
#include "gmime-message-id-list.h"
#include "gmime-part.h"
#include "gmime-error.h"
#include "gmime-internal.h"

/**
 * SECTION: gmime-multipart-related
 * @title: GMimeMultipartRelated
 * @short_description: Related MIME parts with a root document
 * @see_also: #GMimeMultipart
 *
 * A multipart/related container with root-document selection.
 **/

static gboolean multipart_related_try_get_value (GMimeMultipart *multipart, GMimeTextFormat format, GMimeTextPart **body);

G_DEFINE_TYPE (GMimeMultipartRelated, g_mime_multipart_related, GMIME_TYPE_MULTIPART)

static void
multipart_related_accept (GMimeEntity *object, GMimeVisitor *visitor)
{
	g_mime_visitor_visit_multipart_related (visitor, (GMimeMultipartRelated *) object);
}

static void
g_mime_multipart_related_class_init (GMimeMultipartRelatedClass *klass)
{
	GMIME_ENTITY_CLASS (klass)->accept = multipart_related_accept;
	GMIME_MULTIPART_CLASS (klass)->try_get_value = multipart_related_try_get_value;
}

static void
g_mime_multipart_related_init (GMimeMultipartRelated *related)
{
}

static char *
get_cid_path (const char *id)
{
	const char *end;
	gboolean authority;
	GString *path;
	char *text, *trim, **segments, *slash;
	guint i;
	int value;

	text = g_strdup (id);
	trim = text + strlen (text);
	while (trim > text && strchr (" \t\r\n", trim[-1]))
		*--trim = '\0';
	id = text;
	authority = g_str_has_prefix (id, "//");
	if (authority) {
		id += 2;
		id += strcspn (id, "/?#");
	}
	end = id + strcspn (id, "?#");
	path = g_string_new (NULL);
	while (id < end) {
		if (*id == '%' && end - id >= 3 && g_ascii_isxdigit (id[1]) && g_ascii_isxdigit (id[2])) {
			value = (g_ascii_xdigit_value (id[1]) << 4) | g_ascii_xdigit_value (id[2]);
			if (g_ascii_isalnum (value) || (value != 0 && strchr ("-._~", value)))
				g_string_append_c (path, value);
			else
				g_string_append_len (path, id, 3);
			id += 3;
		} else {
			if (g_ascii_isalnum (*id) || strchr ("-._~!$&'()*+,;=:@/[]", *id))
				g_string_append_c (path, *id);
			else
				g_string_append_printf (path, "%%%02X", (guchar) *id);
			id++;
		}
	}
	g_free (text);

	/* MimeKit compares the escaped path of a cid: URI, not the literal start value. */
	if (authority && path->len > 0) {
		segments = g_strsplit (path->str, "/", -1);
		g_string_truncate (path, 0);
		for (i = 1; segments[i]; i++) {
			if (!strcmp (segments[i], "..")) {
				slash = strrchr (path->str, '/');
				if (slash)
					g_string_truncate (path, slash - path->str);
			} else if (strcmp (segments[i], ".")) {
				g_string_append_c (path, '/');
				g_string_append (path, segments[i]);
			}
			if (!segments[i + 1] && (!strcmp (segments[i], ".") || !strcmp (segments[i], "..")))
				g_string_append_c (path, '/');
		}
		g_strfreev (segments);
	}
	return g_string_free (path, FALSE);
}

static char *
get_start_path (const char *start)
{
	GMimeMessageIdList *references;
	const char *id;
	char *path;

	references = g_mime_message_id_list_parse (NULL, start);
	id = g_mime_message_id_list_length (references) ?
		g_mime_message_id_list_get_message_id (references, 0) : start;
	path = get_cid_path (id);
	g_object_unref (references);
	return path;
}

static int
get_root_index (GMimeMultipartRelated *related)
{
	GMimeMultipart *multipart = GMIME_MULTIPART (related);
	GMimeEntity *entity;
	const char *start, *type;
	char *path, *mime_type;
	int i, index = -1;

	start = g_mime_entity_get_content_type_parameter (GMIME_ENTITY (related), "start");
	if (start) {
		path = get_start_path (start);
		for (i = 0; i < g_mime_multipart_get_count (multipart); i++) {
			entity = g_mime_multipart_get_part (multipart, i);
			if (!g_strcmp0 (g_mime_entity_get_content_id (entity), path)) {
				index = i;
				break;
			}
		}
		g_free (path);
		return index;
	}

	type = g_mime_entity_get_content_type_parameter (GMIME_ENTITY (related), "type");
	if (!type)
		return -1;

	for (i = 0; i < g_mime_multipart_get_count (multipart); i++) {
		entity = g_mime_multipart_get_part (multipart, i);
		mime_type = g_mime_content_type_get_mime_type (g_mime_entity_get_content_type (entity));
		index = g_ascii_strcasecmp (type, mime_type) == 0 ? i : -1;
		g_free (mime_type);
		if (index != -1)
			break;
	}
	return index;
}

static gboolean
multipart_related_try_get_value (GMimeMultipart *multipart, GMimeTextFormat format, GMimeTextPart **body)
{
	GMimeEntity *root = g_mime_multipart_related_get_root (GMIME_MULTIPART_RELATED (multipart));

	if (GMIME_IS_TEXT_PART (root) && _g_mime_text_part_is_format (GMIME_TEXT_PART (root), format)) {
		*body = GMIME_TEXT_PART (root);
		return TRUE;
	}
	/* The root document may itself be a multipart/alternative. */
	if (GMIME_IS_MULTIPART (root))
		return g_mime_multipart_try_get_value (GMIME_MULTIPART (root), format, body);
	return FALSE;
}

/**
 * g_mime_multipart_related_new:
 *
 * Creates an empty multipart/related container.
 *
 * Returns: (transfer full): a new #GMimeMultipartRelated.
 **/
GMimeMultipartRelated *
g_mime_multipart_related_new (void)
{
	GMimeMultipartRelated *related;
	GMimeContentType *content_type;

	related = g_object_new (GMIME_TYPE_MULTIPART_RELATED, NULL);
	content_type = g_mime_content_type_new ("multipart", "related");
	g_mime_entity_set_content_type (GMIME_ENTITY (related), content_type);
	g_object_unref (content_type);
	g_mime_multipart_set_boundary ((GMimeMultipart *) related, NULL);

	return related;
}

/**
 * g_mime_multipart_related_get_root:
 * @related: a #GMimeMultipartRelated
 *
 * Selects the immediate child identified by the Content-Type start
 * parameter. If start is absent, selects the first child matching the
 * type parameter, ignoring ASCII case. An unmatched selector falls back
 * to the first child. This does not modify headers or traverse descendants.
 *
 * Returns: (nullable) (transfer none): the root document, or %NULL if empty.
 **/
GMimeEntity *
g_mime_multipart_related_get_root (GMimeMultipartRelated *related)
{
	GMimeMultipart *multipart;
	int index;

	g_return_val_if_fail (GMIME_IS_MULTIPART_RELATED (related), NULL);

	multipart = GMIME_MULTIPART (related);
	if (g_mime_multipart_get_count (multipart) == 0)
		return NULL;
	index = get_root_index (related);
	return g_mime_multipart_get_part (multipart, MAX (index, 0));
}

/**
 * g_mime_multipart_related_set_root:
 * @related: a #GMimeMultipartRelated
 * @root: (transfer none): the new root document
 *
 * Replaces the explicitly selected child, or inserts @root first if
 * no selector matches. Sets the type parameter to the root's MIME type.
 * A root at index zero removes start; a later root uses its Content-Id,
 * generating one if necessary. The container retains @root without
 * removing other occurrences of that entity from the child list.
 **/
void
g_mime_multipart_related_set_root (GMimeMultipartRelated *related, GMimeEntity *root)
{
	GMimeMultipart *multipart;
	GMimeEntity *replaced;
	GMimeContentType *content_type;
	const char *content_id;
	char *type, *start, *generated;
	int index;

	g_return_if_fail (GMIME_IS_MULTIPART_RELATED (related));
	g_return_if_fail (GMIME_IS_ENTITY (root));

	multipart = GMIME_MULTIPART (related);
	index = get_root_index (related);
	if (index != -1) {
		replaced = g_mime_multipart_replace (multipart, index, root);
		g_object_unref (replaced);
	} else {
		g_mime_multipart_insert (multipart, 0, root);
		index = 0;
	}

	content_type = g_mime_entity_get_content_type (GMIME_ENTITY (related));
	type = g_mime_content_type_get_mime_type (g_mime_entity_get_content_type (root));
	g_mime_content_type_set_parameter (content_type, "type", type);
	g_free (type);
	/* Only use start for a non-first root, working around Thunderbird bug 471402:
	 * https://bugzilla.mozilla.org/show_bug.cgi?id=471402 */
	if (index > 0) {
		content_id = g_mime_entity_get_content_id (root);
		if (!content_id || !*content_id) {
			generated = g_mime_utils_generate_message_id (NULL);
			g_mime_entity_set_content_id (root, generated);
			g_free (generated);
			content_id = g_mime_entity_get_content_id (root);
		}
		start = g_strdup_printf ("<%s>", content_id);
		g_mime_content_type_set_parameter (content_type, "start", start);
		g_free (start);
	} else {
		g_mime_param_list_remove (g_mime_content_type_get_parameters (content_type), "start");
	}
}

static gboolean
equal_resource_uri (GUri *left, GUri *right)
{
	return !g_ascii_strcasecmp (g_uri_get_scheme (left), g_uri_get_scheme (right)) &&
		!g_ascii_strcasecmp (g_uri_get_host (left) ? g_uri_get_host (left) : "",
				    g_uri_get_host (right) ? g_uri_get_host (right) : "") &&
		g_uri_get_port (left) == g_uri_get_port (right) &&
		!g_strcmp0 (g_uri_get_path (left), g_uri_get_path (right)) &&
		!g_strcmp0 (g_uri_get_query (left), g_uri_get_query (right));
}

/**
 * g_mime_multipart_related_index_of_uri:
 * @related: a #GMimeMultipartRelated
 * @uri: an absolute or relative UTF-8 URI string
 * @error: return location for an error, or %NULL
 *
 * Finds the first immediate child matching @uri. CID URIs compare their
 * escaped path to Content-Id, case-sensitively. Other absolute URIs match
 * Content-Location, resolving relative locations against the child's
 * Content-Base, or the container's Content-Base if the child has none.
 * Relative queries compare literal relative Content-Location strings.
 *
 * Absolute comparisons use GLib URI normalization, ignore user information
 * and fragments, and compare scheme and host ignoring ASCII case. Paths
 * and queries remain case-sensitive. No network or filesystem access occurs.
 *
 * Returns: the child index, or -1 if not found or if parsing or resolution
 * fails. On failure @error is set; a missing resource is not an error.
 **/
int
g_mime_multipart_related_index_of_uri (GMimeMultipartRelated *related, const char *uri, GError **error)
{
	const GUriFlags flags = G_URI_FLAGS_ENCODED | G_URI_FLAGS_SCHEME_NORMALIZE;
	GMimeMultipart *multipart;
	GMimeEntity *entity;
	GUri *requested, *candidate, *base;
	const char *scheme, *location, *base_uri;
	char *path;
	gboolean matches;
	int i, index = -1;

	g_return_val_if_fail (GMIME_IS_MULTIPART_RELATED (related), -1);
	g_return_val_if_fail (uri != NULL, -1);
	g_return_val_if_fail (error == NULL || *error == NULL, -1);

	if (!_g_mime_is_content_uri (uri, FALSE)) {
		g_set_error_literal (error, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR, "Invalid resource URI");
		return -1;
	}

	multipart = GMIME_MULTIPART (related);
	scheme = g_uri_peek_scheme (uri);
	if (scheme && !g_ascii_strcasecmp (scheme, "cid")) {
		path = get_cid_path (strchr (uri, ':') + 1);
		for (i = 0; i < g_mime_multipart_get_count (multipart); i++) {
			entity = g_mime_multipart_get_part (multipart, i);
			if (!g_strcmp0 (g_mime_entity_get_content_id (entity), path)) {
				index = i;
				break;
			}
		}
		g_free (path);
		return index;
	}

	requested = scheme ? g_uri_parse (uri, flags, error) : NULL;
	if (scheme && !requested)
		return -1;
	for (i = 0; i < g_mime_multipart_get_count (multipart); i++) {
		entity = g_mime_multipart_get_part (multipart, i);
		location = g_mime_entity_get_content_location (entity);
		if (!location)
			continue;
		if (!requested) {
			if (!g_uri_peek_scheme (location) && !strcmp (uri, location)) {
				index = i;
				break;
			}
			continue;
		}
		if (g_uri_peek_scheme (location)) {
			candidate = g_uri_parse (location, flags, error);
		} else {
			base_uri = g_mime_entity_get_content_base (entity);
			if (!base_uri)
				base_uri = g_mime_entity_get_content_base (GMIME_ENTITY (related));
			if (!base_uri)
				continue;
			base = g_uri_parse (base_uri, flags, error);
			if (!base)
				break;
			candidate = g_uri_parse_relative (base, location, flags, error);
			g_uri_unref (base);
		}
		if (!candidate)
			break;
		matches = equal_resource_uri (requested, candidate);
		g_uri_unref (candidate);
		if (matches) {
			index = i;
			break;
		}
	}
	if (requested)
		g_uri_unref (requested);
	return index;
}

/**
 * g_mime_multipart_related_contains_uri:
 * @related: a #GMimeMultipartRelated
 * @uri: an absolute or relative UTF-8 URI string
 * @error: return location for an error, or %NULL
 *
 * Tests whether g_mime_multipart_related_index_of_uri() finds a resource.
 *
 * Returns: %TRUE if a matching child exists, otherwise %FALSE.
 **/
gboolean
g_mime_multipart_related_contains_uri (GMimeMultipartRelated *related, const char *uri, GError **error)
{
	return g_mime_multipart_related_index_of_uri (related, uri, error) != -1;
}

/**
 * g_mime_multipart_related_open:
 * @related: a #GMimeMultipartRelated
 * @uri: an absolute or relative UTF-8 URI string
 * @mime_type: (out) (optional) (nullable) (transfer full): the allocated MIME type
 * @charset: (out) (optional) (nullable) (transfer full): the allocated charset, if present
 * @error: return location for an error, or %NULL
 *
 * Opens a lazy decoded reader for the first matching child. A missing
 * resource, non-leaf child, or leaf without content reports %GMIME_ERROR
 * with ENOENT. A content object without a backing stream reports
 * %GMIME_ERROR_INVALID_OPERATION. URI parsing/resolution and backing-stream
 * rewind errors are propagated. Both metadata outputs are %NULL on failure.
 *
 * The stream retains its source, not the multipart or child. Reader state,
 * backing-stream position, and subsequent read errors follow
 * g_mime_content_open(). No external URI is fetched.
 *
 * Returns: (nullable) (transfer full): a decoded stream, or %NULL on error.
 **/
GMimeStream *
g_mime_multipart_related_open (GMimeMultipartRelated *related, const char *uri,
			     char **mime_type, char **charset, GError **error)
{
	GMimeContentType *content_type;
	GMimeContent *content = NULL;
	GMimeEntity *entity;
	GMimeStream *stream;
	GError *local_error = NULL;
	int index, errsv;

	g_return_val_if_fail (GMIME_IS_MULTIPART_RELATED (related), NULL);
	g_return_val_if_fail (uri != NULL, NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	if (mime_type)
		*mime_type = NULL;
	if (charset)
		*charset = NULL;
	index = g_mime_multipart_related_index_of_uri (related, uri, &local_error);
	if (local_error) {
		g_propagate_error (error, local_error);
		return NULL;
	}
	if (index != -1) {
		entity = g_mime_multipart_get_part (GMIME_MULTIPART (related), index);
		if (GMIME_IS_PART (entity))
			content = g_mime_part_get_content (GMIME_PART (entity));
	}
	if (!content) {
		g_set_error_literal (error, GMIME_ERROR, ENOENT, "Related resource has no readable content");
		return NULL;
	}
	if (!g_mime_content_get_stream (content)) {
		g_set_error_literal (error, GMIME_ERROR, GMIME_ERROR_INVALID_OPERATION, "Related content has no backing stream");
		return NULL;
	}
	errno = 0;
	stream = g_mime_content_open (content);
	if (!stream) {
		errsv = errno;
		g_set_error (error, GMIME_ERROR, errsv ? errsv : GMIME_ERROR_GENERAL,
			     "Failed to rewind related content: %s", errsv ? g_strerror (errsv) : "unknown I/O error");
		return NULL;
	}
	content_type = g_mime_entity_get_content_type (entity);
	if (mime_type)
		*mime_type = g_mime_content_type_get_mime_type (content_type);
	if (charset)
		*charset = g_strdup (g_mime_content_type_get_parameter (content_type, "charset"));
	return stream;
}
