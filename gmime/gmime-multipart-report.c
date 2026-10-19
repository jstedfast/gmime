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

#include "gmime-multipart-report.h"
#include "gmime-visitor.h"
#include "gmime-common.h"

/**
 * SECTION: gmime-multipart-report
 * @title: GMimeMultipartReport
 * @short_description: Mail report containers
 * @see_also: #GMimeMultipart
 *
 * A multipart/report container with a report-type Content-Type parameter.
 * The report type is normally the subtype of the second child, but is
 * managed independently of the children.
 **/

G_DEFINE_TYPE (GMimeMultipartReport, g_mime_multipart_report, GMIME_TYPE_MULTIPART)

static void
multipart_report_accept (GMimeEntity *object, GMimeVisitor *visitor)
{
	g_mime_visitor_visit_multipart_report (visitor, (GMimeMultipartReport *) object);
}

static void
g_mime_multipart_report_class_init (GMimeMultipartReportClass *klass)
{
	GMIME_ENTITY_CLASS (klass)->accept = multipart_report_accept;
}

static void
g_mime_multipart_report_init (GMimeMultipartReport *report)
{
}

/**
 * g_mime_multipart_report_new:
 * @report_type: a UTF-8 report type
 *
 * Creates an empty multipart/report container with the specified report type.
 * Leading and trailing Unicode whitespace is removed from @report_type.
 * Empty values are allowed; %NULL and invalid UTF-8 are programming errors.
 *
 * Returns: (transfer full): a new #GMimeMultipartReport.
 **/
GMimeMultipartReport *
g_mime_multipart_report_new (const char *report_type)
{
	GMimeMultipartReport *report;
	GMimeContentType *content_type;

	g_return_val_if_fail (report_type != NULL && g_utf8_validate (report_type, -1, NULL), NULL);

	report = g_object_new (GMIME_TYPE_MULTIPART_REPORT, NULL);
	content_type = g_mime_content_type_new ("multipart", "report");
	g_mime_entity_set_content_type ((GMimeEntity *) report, content_type);
	g_object_unref (content_type);
	g_mime_multipart_set_boundary ((GMimeMultipart *) report, NULL);
	g_mime_multipart_report_set_report_type (report, report_type);
	return report;
}

/**
 * g_mime_multipart_report_get_report_type:
 * @report: a #GMimeMultipartReport
 *
 * Gets the report-type parameter without modifying the Content-Type header.
 *
 * Returns: (transfer none) (nullable): the report type, or %NULL if absent.
 **/
const char *
g_mime_multipart_report_get_report_type (GMimeMultipartReport *report)
{
	g_return_val_if_fail (GMIME_IS_MULTIPART_REPORT (report), NULL);

	return g_mime_entity_get_content_type_parameter ((GMimeEntity *) report, "report-type");
}

/**
 * g_mime_multipart_report_set_report_type:
 * @report: a #GMimeMultipartReport
 * @report_type: a UTF-8 report type
 *
 * Sets the report-type parameter after removing leading and trailing Unicode
 * whitespace. Case and internal whitespace are preserved. Empty values are
 * allowed; %NULL and invalid UTF-8 are programming errors. Assigning an
 * equivalent value preserves the raw header and parameter metadata.
 **/
void
g_mime_multipart_report_set_report_type (GMimeMultipartReport *report, const char *report_type)
{
	const char *start, *end, *previous;
	char *value;

	g_return_if_fail (GMIME_IS_MULTIPART_REPORT (report));
	g_return_if_fail (report_type != NULL && g_utf8_validate (report_type, -1, NULL));

	if (!g_strcmp0 (g_mime_multipart_report_get_report_type (report), report_type))
		return;

	start = report_type;
	while (*start && _g_mime_is_unicode_whitespace (g_utf8_get_char (start)))
		start = g_utf8_next_char (start);
	end = start + strlen (start);
	while (end > start) {
		previous = g_utf8_prev_char (end);
		if (!_g_mime_is_unicode_whitespace (g_utf8_get_char (previous)))
			break;
		end = previous;
	}
	value = g_strndup (start, end - start);
	g_mime_entity_set_content_type_parameter ((GMimeEntity *) report, "report-type", value);
	g_free (value);
}
