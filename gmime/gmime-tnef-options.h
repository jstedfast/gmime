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

#ifndef __GMIME_TNEF_OPTIONS_H__
#define __GMIME_TNEF_OPTIONS_H__

#include <gmime/gmime-compliance.h>
#include <gmime/gmime-tnef-types.h>

G_BEGIN_DECLS

/**
 * GMimeTnefOptions:
 *
 * Reference-counted limits and default encoding for low-level TNEF reading.
 * Readers copy these options at construction. Materialization limits do not
 * restrict streaming values.
 **/
typedef struct _GMimeTnefOptions GMimeTnefOptions;

#define GMIME_TYPE_TNEF_OPTIONS (g_mime_tnef_options_get_type ())

GType g_mime_tnef_options_get_type (void);

GMimeTnefOptions *g_mime_tnef_options_new (void);

GMimeTnefOptions *g_mime_tnef_options_ref (GMimeTnefOptions *options);
void g_mime_tnef_options_unref (GMimeTnefOptions *options);

GMimeTnefOptions *g_mime_tnef_options_clone (const GMimeTnefOptions *options);

guint g_mime_tnef_options_get_max_nesting_depth (const GMimeTnefOptions *options);
void g_mime_tnef_options_set_max_nesting_depth (GMimeTnefOptions *options, guint maximum);

guint g_mime_tnef_options_get_max_property_value_length (const GMimeTnefOptions *options);
void g_mime_tnef_options_set_max_property_value_length (GMimeTnefOptions *options, guint maximum);

guint64 g_mime_tnef_options_get_max_total_data_bytes (const GMimeTnefOptions *options);
void g_mime_tnef_options_set_max_total_data_bytes (GMimeTnefOptions *options, guint64 maximum);

int g_mime_tnef_options_get_default_codepage (const GMimeTnefOptions *options);
void g_mime_tnef_options_set_default_codepage (GMimeTnefOptions *options, int codepage);

/**
 * GMimeTnefComplianceOptions:
 *
 * Reference-counted diagnostic reporting limits.
 **/
typedef struct _GMimeTnefComplianceOptions GMimeTnefComplianceOptions;

#define GMIME_TYPE_TNEF_COMPLIANCE_OPTIONS (g_mime_tnef_compliance_options_get_type ())

GType g_mime_tnef_compliance_options_get_type (void);

GMimeTnefComplianceOptions *g_mime_tnef_compliance_options_new (void);

GMimeTnefComplianceOptions *g_mime_tnef_compliance_options_ref (GMimeTnefComplianceOptions *options);
void g_mime_tnef_compliance_options_unref (GMimeTnefComplianceOptions *options);

GMimeTnefComplianceOptions *g_mime_tnef_compliance_options_clone (const GMimeTnefComplianceOptions *options);

guint g_mime_tnef_compliance_options_get_max_issues_per_violation (const GMimeTnefComplianceOptions *options);
void g_mime_tnef_compliance_options_set_max_issues_per_violation (GMimeTnefComplianceOptions *options, guint maximum);

/**
 * GMimeTnefComplianceIssue:
 * @violation: the reported violation
 * @severity: diagnostic severity
 * @categories: diagnostic categories
 * @stream_offset: byte offset relative to the outermost TNEF stream
 * @depth: embedded message depth
 * @attribute_tag: associated attribute, or zero
 * @property_tag: associated property, or zero
 * @description: (transfer none): borrowed diagnostic description
 *
 * A diagnostic borrowed for the duration of a compliance callback.
 **/
typedef struct {
	GMimeTnefComplianceViolation violation;
	GMimeComplianceSeverity severity;
	GMimeComplianceCategories categories;
	gint64 stream_offset;
	guint depth;
	GMimeTnefAttributeTag attribute_tag;
	GMimeTnefPropertyTag property_tag;
	const char *description;
} GMimeTnefComplianceIssue;

/**
 * GMimeTnefComplianceLogger:
 * @issue: (transfer none): a diagnostic valid during this callback
 * @user_data: (closure) (nullable): callback data
 *
 * Receives TNEF diagnostics. Callbacks must not reenter the reader.
 **/
typedef void (*GMimeTnefComplianceLogger) (const GMimeTnefComplianceIssue *issue, gpointer user_data);

const char *g_mime_tnef_compliance_violation_get_description (GMimeTnefComplianceViolation violation);
const char *g_mime_tnef_compliance_violation_get_remarks (GMimeTnefComplianceViolation violation);
GMimeComplianceSeverity g_mime_tnef_compliance_violation_get_severity (GMimeTnefComplianceViolation violation);
GMimeComplianceCategories g_mime_tnef_compliance_violation_get_categories (GMimeTnefComplianceViolation violation);

G_END_DECLS

#endif
