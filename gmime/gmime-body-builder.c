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

#include "gmime-body-builder.h"
#include "gmime-error.h"
#include "gmime-internal.h"
#include "gmime-multipart-alternative.h"
#include "gmime-multipart-related.h"
#include "gmime-text-part.h"

/**
 * SECTION: gmime-body-builder
 * @title: GMimeBodyBuilder
 * @short_description: Building common MIME message bodies
 * @see_also: #GMimeAttachmentCollection, #GMimeMultipartAlternative, #GMimeMultipartRelated
 *
 * Ports MimeKit's BodyBuilder. Plain text and HTML form multipart/alternative
 * when both are present. HTML and linked resources form multipart/related,
 * and attachments wrap the body in multipart/mixed. Resources are ignored
 * without an HTML body. A lone attachment without either body is returned
 * directly; an otherwise empty builder creates an empty text/plain part.
 *
 * Body strings are copied UTF-8 strings, with %NULL meaning absent and an
 * empty string meaning present. BodyEncoding is represented by a charset name,
 * defaulting to "utf-8". Generated text parts use that charset without charset
 * detection or transfer-encoding selection. Unsupported charsets and conversion
 * failures are reported rather than silently replacing unrepresentable text.
 *
 * Each build creates fresh text parts and containers, but retains the original
 * attachment and resource instances without modifying their headers or removing
 * them from the builder's collections.
 **/

static void body_builder_finalize (GObject *object);

G_DEFINE_TYPE (GMimeBodyBuilder, g_mime_body_builder, G_TYPE_OBJECT)

static void
g_mime_body_builder_class_init (GMimeBodyBuilderClass *klass)
{
	G_OBJECT_CLASS (klass)->finalize = body_builder_finalize;
}

static void
g_mime_body_builder_init (GMimeBodyBuilder *builder)
{
	builder->attachments = g_mime_attachment_collection_new (FALSE);
	builder->linked_resources = g_mime_attachment_collection_new (TRUE);
	builder->body_encoding = g_strdup ("utf-8");
}

static void
body_builder_finalize (GObject *object)
{
	GMimeBodyBuilder *builder = GMIME_BODY_BUILDER (object);

	g_object_unref (builder->attachments);
	g_object_unref (builder->linked_resources);
	g_free (builder->text_body);
	g_free (builder->html_body);
	g_free (builder->body_encoding);
	G_OBJECT_CLASS (g_mime_body_builder_parent_class)->finalize (object);
}

static GMimeEntity *
create_text_part (GMimeBodyBuilder *builder, const char *subtype, const char *text, GError **error)
{
	GMimeTextPart *part = g_mime_text_part_new_with_subtype (subtype);

	if (!g_mime_text_part_set_text_with_charset (part, builder->body_encoding, text, error)) {
		g_object_unref (part);
		return NULL;
	}

	return (GMimeEntity *) part;
}

/**
 * g_mime_body_builder_new:
 *
 * Returns: (transfer full): a new builder with empty attachment collections,
 * absent text and HTML bodies, and UTF-8 body encoding.
 **/
GMimeBodyBuilder *
g_mime_body_builder_new (void)
{
	return g_object_new (GMIME_TYPE_BODY_BUILDER, NULL);
}

/**
 * g_mime_body_builder_get_attachments:
 * @builder: a #GMimeBodyBuilder
 *
 * Returns: (transfer none): the persistent mutable attachment collection.
 **/
GMimeAttachmentCollection *
g_mime_body_builder_get_attachments (GMimeBodyBuilder *builder)
{
	g_return_val_if_fail (GMIME_IS_BODY_BUILDER (builder), NULL);

	return builder->attachments;
}

/**
 * g_mime_body_builder_get_linked_resources:
 * @builder: a #GMimeBodyBuilder
 *
 * Returns: (transfer none): the persistent mutable linked-resource collection.
 **/
GMimeAttachmentCollection *
g_mime_body_builder_get_linked_resources (GMimeBodyBuilder *builder)
{
	g_return_val_if_fail (GMIME_IS_BODY_BUILDER (builder), NULL);

	return builder->linked_resources;
}

/**
 * g_mime_body_builder_get_text_body:
 * @builder: a #GMimeBodyBuilder
 *
 * Returns: (nullable): the borrowed UTF-8 plain-text body, or %NULL if absent.
 **/
const char *
g_mime_body_builder_get_text_body (GMimeBodyBuilder *builder)
{
	g_return_val_if_fail (GMIME_IS_BODY_BUILDER (builder), NULL);

	return builder->text_body;
}

/**
 * g_mime_body_builder_set_text_body:
 * @builder: a #GMimeBodyBuilder
 * @text: (nullable): a UTF-8 body string, or %NULL to omit the plain-text body
 *
 * Copies @text. An empty string is a present, empty body.
 **/
void
g_mime_body_builder_set_text_body (GMimeBodyBuilder *builder, const char *text)
{
	char *copy;

	g_return_if_fail (GMIME_IS_BODY_BUILDER (builder));
	g_return_if_fail (text == NULL || g_utf8_validate (text, -1, NULL));

	copy = g_strdup (text);
	g_free (builder->text_body);
	builder->text_body = copy;
}

/**
 * g_mime_body_builder_get_html_body:
 * @builder: a #GMimeBodyBuilder
 *
 * Returns: (nullable): the borrowed UTF-8 HTML body, or %NULL if absent.
 **/
const char *
g_mime_body_builder_get_html_body (GMimeBodyBuilder *builder)
{
	g_return_val_if_fail (GMIME_IS_BODY_BUILDER (builder), NULL);

	return builder->html_body;
}

/**
 * g_mime_body_builder_set_html_body:
 * @builder: a #GMimeBodyBuilder
 * @html: (nullable): a UTF-8 body string, or %NULL to omit the HTML body
 *
 * Copies @html. An empty string is a present, empty body.
 **/
void
g_mime_body_builder_set_html_body (GMimeBodyBuilder *builder, const char *html)
{
	char *copy;

	g_return_if_fail (GMIME_IS_BODY_BUILDER (builder));
	g_return_if_fail (html == NULL || g_utf8_validate (html, -1, NULL));

	copy = g_strdup (html);
	g_free (builder->html_body);
	builder->html_body = copy;
}

/**
 * g_mime_body_builder_get_body_encoding:
 * @builder: a #GMimeBodyBuilder
 *
 * Returns: the borrowed MIME charset name used for generated text parts.
 **/
const char *
g_mime_body_builder_get_body_encoding (GMimeBodyBuilder *builder)
{
	g_return_val_if_fail (GMIME_IS_BODY_BUILDER (builder), NULL);

	return builder->body_encoding;
}

/**
 * g_mime_body_builder_set_body_encoding:
 * @builder: a #GMimeBodyBuilder
 * @charset: a non-NULL charset name
 * @error: return location for an error, or %NULL
 *
 * Resolves @charset with GMime's native charset aliases and MIME-name mapping,
 * and checks that a UTF-8 converter is available. Charset names must be MIME
 * tokens; iconv modifiers such as //IGNORE and //TRANSLIT are not accepted.
 * Generic UTF-16/32 aliases are normalized before conversion, so generated
 * content does not include a Unicode byte-order mark.
 * The "unicode" alias selects UTF-16, describing a managed Encoding object,
 * unlike the MIME charset alias accepted by #GMimeTextPart.
 *
 * Unlike a managed Encoding object, this API does not support custom encoders
 * or replacement fallbacks. Unrepresentable body text causes a conversion
 * error during g_mime_body_builder_to_message_body(), not a charset change.
 *
 * Returns: %TRUE on success, or %FALSE with a %G_CONVERT_ERROR on failure,
 * leaving the previous encoding unchanged.
 **/
gboolean
g_mime_body_builder_set_body_encoding (GMimeBodyBuilder *builder, const char *charset, GError **error)
{
	char *name, *converted;

	g_return_val_if_fail (GMIME_IS_BODY_BUILDER (builder), FALSE);
	g_return_val_if_fail (charset != NULL, FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	/* BodyEncoding describes an Encoding object, not TextPart's MIME
	 * charset overload: preserve the managed "unicode" name for UTF-16. */
	if (!g_ascii_strcasecmp (charset, "unicode"))
		charset = "utf-16";

	/* Probe the same encoder used by text parts without changing the
	 * selected charset until validation and converter setup succeed. */
	converted = _g_mime_charset_encode_text (charset, "", NULL, &name, error);
	if (!converted)
		return FALSE;
	g_free (converted);
	g_free (builder->body_encoding);
	builder->body_encoding = name;
	return TRUE;
}

/**
 * g_mime_body_builder_to_message_body:
 * @builder: a #GMimeBodyBuilder
 * @error: return location for an error, or %NULL
 *
 * Constructs the MIME tree without changing the builder's state. NULL body
 * strings are absent; empty strings are present. Linked resources participate
 * only when HTML is present. Attachment and resource ordering and identity
 * are preserved, including duplicate occurrences. Generated text parts keep
 * the transfer encoding unspecified, matching MimeKit's TextPart.SetText().
 *
 * A lone attachment is returned by reference if neither text body is present.
 * If no body or attachments are present, returns an empty text/plain part in
 * the selected charset. Conversion errors use %G_CONVERT_ERROR and converted
 * content exceeding the #GMimeStreamMem limit uses %GMIME_ERROR with EOVERFLOW.
 * Failures release partial trees without modifying either collection.
 *
 * Returns: (nullable) (transfer full): the message body, or %NULL on failure.
 **/
GMimeEntity *
g_mime_body_builder_to_message_body (GMimeBodyBuilder *builder, GError **error)
{
	GMimeMultipartAlternative *alternative = NULL;
	GMimeMultipartRelated *related;
	GMimeMultipart *mixed;
	GMimeEntity *body = NULL, *text, *html;
	int i, count;

	g_return_val_if_fail (GMIME_IS_BODY_BUILDER (builder), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	if (builder->text_body) {
		text = create_text_part (builder, "plain", builder->text_body, error);
		if (!text)
			return NULL;
		if (builder->html_body) {
			alternative = g_mime_multipart_alternative_new ();
			g_mime_multipart_add ((GMimeMultipart *) alternative, text);
			g_object_unref (text);
			body = (GMimeEntity *) alternative;
		} else {
			body = text;
		}
	}

	if (builder->html_body) {
		text = create_text_part (builder, "html", builder->html_body, error);
		if (!text) {
			g_clear_object (&body);
			return NULL;
		}
		count = g_mime_attachment_collection_get_count (builder->linked_resources);
		if (count > 0) {
			related = g_mime_multipart_related_new ();
			g_mime_multipart_related_set_root (related, text);
			g_object_unref (text);
			for (i = 0; i < count; i++)
				g_mime_multipart_add ((GMimeMultipart *) related,
						     g_mime_attachment_collection_get_attachment (builder->linked_resources, i));
			html = (GMimeEntity *) related;
		} else {
			html = text;
		}
		if (alternative) {
			g_mime_multipart_add ((GMimeMultipart *) alternative, html);
			g_object_unref (html);
		} else {
			body = html;
		}
	}

	count = g_mime_attachment_collection_get_count (builder->attachments);
	if (count > 0) {
		if (!body && count == 1)
			return g_object_ref (g_mime_attachment_collection_get_attachment (builder->attachments, 0));
		mixed = g_mime_multipart_new_with_subtype ("mixed");
		if (body) {
			g_mime_multipart_add (mixed, body);
			g_object_unref (body);
		}
		for (i = 0; i < count; i++)
			g_mime_multipart_add (mixed, g_mime_attachment_collection_get_attachment (builder->attachments, i));
		body = (GMimeEntity *) mixed;
	}

	if (!body)
		body = create_text_part (builder, "plain", "", error);

	return body;
}
