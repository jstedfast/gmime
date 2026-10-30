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

#include "gmime-message-feedback-report.h"
#include "gmime-visitor.h"
#include "gmime-internal.h"

#include "gmime-object-private.h"

/**
 * SECTION: gmime-message-feedback-report
 * @title: GMimeMessageFeedbackReport
 * @short_description: Message feedback report parts
 * @see_also: #GMimePart, #GMimeMultipartReport, #GMimeHeaderList
 *
 * A machine-readable message/feedback-report part with lazily parsed fields.
 **/

static void report_finalize (GObject *object);
static void fields_changed (gpointer sender, gpointer args, gpointer user_data);


G_DEFINE_TYPE_WITH_PRIVATE (GMimeMessageFeedbackReport, g_mime_message_feedback_report, GMIME_TYPE_PART)

GMimeMessageFeedbackReportPrivate *
_g_mime_message_feedback_report_get_private (GMimeMessageFeedbackReport *object)
{
	return g_mime_message_feedback_report_get_instance_private (object);
}

static void
report_accept (GMimeEntity *object, GMimeVisitor *visitor)
{
	g_mime_visitor_visit_message_feedback_report (visitor, (GMimeMessageFeedbackReport *) object);
}

static void
g_mime_message_feedback_report_class_init (GMimeMessageFeedbackReportClass *klass)
{
	GMIME_ENTITY_CLASS (klass)->accept = report_accept;
	G_OBJECT_CLASS (klass)->finalize = report_finalize;
}

static void
g_mime_message_feedback_report_init (GMimeMessageFeedbackReport *report)
{
	GMimeMessageFeedbackReportPrivate *priv = _g_mime_message_feedback_report_get_private (report);

	priv->fields = NULL;
}

static void
report_finalize (GObject *object)
{
	GMimeMessageFeedbackReport *report = GMIME_MESSAGE_FEEDBACK_REPORT (object);
	GMimeMessageFeedbackReportPrivate *priv = _g_mime_message_feedback_report_get_private (report);

	if (priv->fields) {
		g_mime_event_remove (priv->fields->changed, (GMimeEventCallback) fields_changed, report);
		g_object_unref (priv->fields);
	}

	G_OBJECT_CLASS (g_mime_message_feedback_report_parent_class)->finalize (object);
}

static void
fields_changed (gpointer sender, gpointer args, gpointer user_data)
{
	GMimeHeaderList *fields = sender;
	GMimeMessageFeedbackReport *report = user_data;

	/* Fields cannot be NULL while this handler is registered to their Changed event. */
	_g_mime_part_serialize_fields ((GMimePart *) report, fields, "feedback report");
}

/**
 * g_mime_message_feedback_report_new:
 *
 * Creates an empty message/feedback-report part. Fields and an empty content
 * stream are created on first field access.
 *
 * Returns: (transfer full): a new #GMimeMessageFeedbackReport.
 **/
GMimeMessageFeedbackReport *
g_mime_message_feedback_report_new (void)
{
	GMimeMessageFeedbackReport *report;
	GMimeContentType *content_type;

	report = g_object_new (GMIME_TYPE_MESSAGE_FEEDBACK_REPORT, NULL);
	content_type = g_mime_content_type_new ("message", "feedback-report");
	g_mime_entity_set_content_type ((GMimeEntity *) report, content_type);
	g_object_unref (content_type);

	return report;
}

/**
 * g_mime_message_feedback_report_get_fields:
 * @report: a #GMimeMessageFeedbackReport
 * @error: return location for an error, or %NULL
 *
 * Lazily loads the first header block of the decoded content using default
 * parser options. Reading fields preserves existing content. With no content,
 * an empty header list and content stream are created.
 *
 * List and contained-header changes replace the content using default format
 * options without changing the part's transfer-encoding header. Once loaded,
 * the same list is returned even if the inherited content is replaced.
 *
 * Rewind, read and parse failures return %NULL without caching a partial list
 * and can be retried. Content without a backing stream reports
 * %GMIME_ERROR_INVALID_OPERATION. Stream positioning follows
 * g_mime_content_open().
 *
 * Returns: (transfer none) (nullable): the cached fields, or %NULL on error.
 **/
GMimeHeaderList *
g_mime_message_feedback_report_get_fields (GMimeMessageFeedbackReport *report, GError **error)
{
	GMimeMessageFeedbackReportPrivate *priv;
	GMimeHeaderList *fields;

	g_return_val_if_fail (GMIME_IS_MESSAGE_FEEDBACK_REPORT (report), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	priv = _g_mime_message_feedback_report_get_private (report);

	if (priv->fields)
		return priv->fields;

	if (!(fields = _g_mime_part_load_fields ((GMimePart *) report, "Feedback report", error)))
		return NULL;

	priv->fields = fields;
	g_mime_event_add (fields->changed, (GMimeEventCallback) fields_changed, report);

	return fields;
}
