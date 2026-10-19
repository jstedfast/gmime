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

#include <errno.h>

#include "gmime-message-delivery-status.h"
#include "gmime-visitor.h"
#include "gmime-filter-basic.h"
#include "gmime-stream-mem.h"
#include "gmime-internal.h"
#include "gmime-error.h"

/**
 * SECTION: gmime-message-delivery-status
 * @title: GMimeMessageDeliveryStatus
 * @short_description: Message delivery status parts
 * @see_also: #GMimePart, #GMimeMultipartReport, #GMimeHeaderListCollection
 *
 * A machine-readable delivery status notification. The first header group
 * contains per-message fields; subsequent groups describe individual recipients.
 * Both message/delivery-status and message/global-delivery-status use this class.
 **/

static void status_finalize (GObject *object);
static void groups_changed (GMimeHeaderListCollection *groups, gpointer args, GMimeMessageDeliveryStatus *status);

G_DEFINE_TYPE (GMimeMessageDeliveryStatus, g_mime_message_delivery_status, GMIME_TYPE_PART)

static void
status_accept (GMimeEntity *object, GMimeVisitor *visitor)
{
	g_mime_visitor_visit_message_delivery_status (visitor, (GMimeMessageDeliveryStatus *) object);
}

static void
g_mime_message_delivery_status_class_init (GMimeMessageDeliveryStatusClass *klass)
{
	GMIME_ENTITY_CLASS (klass)->accept = status_accept;
	G_OBJECT_CLASS (klass)->finalize = status_finalize;
}

static void
g_mime_message_delivery_status_init (GMimeMessageDeliveryStatus *status)
{
	status->groups = NULL;
}

static void
status_finalize (GObject *object)
{
	GMimeMessageDeliveryStatus *status = GMIME_MESSAGE_DELIVERY_STATUS (object);

	if (status->groups) {
		g_mime_event_remove (status->groups->changed, (GMimeEventCallback) groups_changed, status);
		g_object_unref (status->groups);
	}
	G_OBJECT_CLASS (g_mime_message_delivery_status_parent_class)->finalize (object);
}

static void
groups_changed (GMimeHeaderListCollection *groups, gpointer args, GMimeMessageDeliveryStatus *status)
{
	GMimeStream *stream = g_mime_stream_mem_new ();
	GMimeContent *content;
	int i, errsv;

	errno = 0;
	for (i = 0; i < g_mime_header_list_collection_get_count (groups); i++) {
		if (g_mime_header_list_write_to_stream (g_mime_header_list_collection_get_group (groups, i), NULL, stream) == -1)
			goto fail;
	}
	if (g_mime_stream_reset (stream) == -1)
		goto fail;

	content = g_mime_content_new_with_stream (stream, GMIME_CONTENT_ENCODING_UNSPECIFIED);
	g_mime_part_set_content ((GMimePart *) status, content);
	g_object_unref (content);
	g_object_unref (stream);
	return;

 fail:
	errsv = errno;
	g_warning ("Failed to serialize delivery status groups: %s",
		   errsv ? g_strerror (errsv) : "unknown I/O error");
	g_object_unref (stream);
}

static GMimeHeaderList *
parse_status_group (GMimeParser *parser, GError **error)
{
	GMimeHeaderList *group;

	/* RFC 3464 status groups are header blocks separated by one or more blank
	 * lines. Empty blocks at the start, between groups or at EOF are not groups. */
	while (!g_mime_parser_eos (parser)) {
		group = g_mime_parser_construct_headers (parser, NULL, error);
		if (!group || g_mime_header_list_get_count (group))
			return group;
		g_object_unref (group);
	}

	/* NULL without an error means there are no more status groups. */
	return NULL;
}

static GMimeHeaderListCollection *
parse_status_groups (GMimeStream *stream, GError **error)
{
	GMimeHeaderListCollection *groups = g_mime_header_list_collection_new ();
	GMimeContentEncoding encoding = GMIME_CONTENT_ENCODING_UNSPECIFIED;
	GMimeParser *parser = g_mime_parser_new_with_stream (stream);
	GMimeHeaderList *group;
	GMimeHeader *header;
	GMimeStream *raw, *filtered;
	GMimeFilter *filter;
	GError *local_error = NULL;

	while ((group = parse_status_group (parser, &local_error))) {
		g_mime_header_list_collection_add (groups, group);
		header = g_mime_header_list_get_header (group, "Content-Transfer-Encoding");
		encoding = header ? g_mime_content_encoding_from_string (g_mime_header_get_value (header)) :
			GMIME_CONTENT_ENCODING_UNSPECIFIED;
		g_object_unref (group);

		/* Office365 sometimes puts a Content-Transfer-Encoding in the first status
		 * group and encodes everything after it (MimeKit issue #250). Decode the
		 * remaining input before parsing any further groups. */
		if (encoding == GMIME_CONTENT_ENCODING_BASE64 || encoding == GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE ||
		    encoding == GMIME_CONTENT_ENCODING_UUENCODE)
			break;

		/* 7bit, 8bit and binary need no decoding; continue with the same parser. */
		encoding = GMIME_CONTENT_ENCODING_UNSPECIFIED;
	}
	if (local_error)
		goto done;

	if (encoding != GMIME_CONTENT_ENCODING_UNSPECIFIED) {
		raw = _g_mime_parser_read_to_eos (parser, &local_error);
		if (!raw)
			goto done;
		filtered = g_mime_stream_filter_new (raw);
		filter = g_mime_filter_basic_new (encoding, FALSE);
		g_mime_stream_filter_add ((GMimeStreamFilter *) filtered, filter);
		g_object_unref (filter);
		g_object_unref (raw);
		g_mime_parser_init_with_stream (parser, filtered);
		g_object_unref (filtered);

		/* Only the original remainder is decoded; transfer-encoding fields inside
		 * it do not recursively change the interpretation of later groups. */
		while ((group = parse_status_group (parser, &local_error))) {
			g_mime_header_list_collection_add (groups, group);
			g_object_unref (group);
		}
	}

 done:
	g_object_unref (parser);
	if (local_error) {
		if (g_error_matches (local_error, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR) ||
		    g_error_matches (local_error, GMIME_ERROR, GMIME_ERROR_HEADER_TOO_LONG)) {
			/* MimeKit retains completed groups when malformed content raises a
			 * FormatException. Do not apply that recovery to stream I/O failures. */
			g_warning ("Malformed delivery status content: %s", local_error->message);
			g_error_free (local_error);
		} else {
			g_object_unref (groups);
			g_propagate_error (error, local_error);
			return NULL;
		}
	}
	return groups;
}

/**
 * g_mime_message_delivery_status_new:
 *
 * Creates an empty message/delivery-status part. Status groups and an empty
 * content stream are created on first group access.
 *
 * Returns: (transfer full): a new #GMimeMessageDeliveryStatus.
 **/
GMimeMessageDeliveryStatus *
g_mime_message_delivery_status_new (void)
{
	GMimeMessageDeliveryStatus *status;
	GMimeContentType *content_type;

	status = g_object_new (GMIME_TYPE_MESSAGE_DELIVERY_STATUS, NULL);
	content_type = g_mime_content_type_new ("message", "delivery-status");
	g_mime_entity_set_content_type ((GMimeEntity *) status, content_type);
	g_object_unref (content_type);
	return status;
}

/**
 * g_mime_message_delivery_status_get_status_groups:
 * @status: a #GMimeMessageDeliveryStatus
 * @error: return location for an error, or %NULL
 *
 * Lazily parses the decoded content with default parser options. The first
 * group contains per-message fields, and later groups contain per-recipient
 * fields. Blank lines do not create empty groups. Existing content is not
 * rewritten merely by reading groups. With no content, an empty collection
 * and content stream are created.
 *
 * Handles Office365 reports whose first group declares a transfer encoding
 * for all remaining groups. Malformed input logs a warning and retains
 * completed groups, matching MimeKit's syntax-error recovery. Rewind and read
 * failures return %NULL without caching partial results and may be retried.
 * Content without a backing stream reports an invalid-operation error.
 *
 * Collection and contained-header changes replace the content using default
 * format options without changing the part's transfer-encoding header.
 * Collection indexed replacement does not notify; see
 * g_mime_header_list_collection_set_group(). Once loaded, the same collection
 * is returned even if the inherited content is replaced.
 *
 * Returns: (transfer none) (nullable): the cached status groups, or %NULL on error.
 **/
GMimeHeaderListCollection *
g_mime_message_delivery_status_get_status_groups (GMimeMessageDeliveryStatus *status, GError **error)
{
	GMimeHeaderListCollection *groups;
	GMimeContent *content;
	GMimeStream *stream;
	int errsv;

	g_return_val_if_fail (GMIME_IS_MESSAGE_DELIVERY_STATUS (status), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	if (status->groups)
		return status->groups;

	content = g_mime_part_get_content ((GMimePart *) status);
	if (content) {
		if (!g_mime_content_get_stream (content)) {
			g_set_error_literal (error, GMIME_ERROR, GMIME_ERROR_INVALID_OPERATION,
					     "Delivery status content has no backing stream");
			return NULL;
		}
		errno = 0;
		stream = g_mime_content_open (content);
		if (!stream) {
			errsv = errno;
			g_set_error (error, GMIME_ERROR, errsv ? errsv : GMIME_ERROR_GENERAL,
				     "Failed to rewind delivery status content: %s",
				     errsv ? g_strerror (errsv) : "unknown I/O error");
			return NULL;
		}
		groups = parse_status_groups (stream, error);
		g_object_unref (stream);
		if (!groups)
			return NULL;
	} else {
		stream = g_mime_stream_mem_new ();
		content = g_mime_content_new_with_stream (stream, GMIME_CONTENT_ENCODING_UNSPECIFIED);
		g_mime_part_set_content ((GMimePart *) status, content);
		g_object_unref (content);
		g_object_unref (stream);
		groups = g_mime_header_list_collection_new ();
	}
	status->groups = groups;
	g_mime_event_add (groups->changed, (GMimeEventCallback) groups_changed, status);
	return groups;
}
