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

#ifndef __GMIME_TNEF_READER_H__
#define __GMIME_TNEF_READER_H__

#include <gmime/gmime-stream.h>
#include <gmime/gmime-tnef-options.h>
#include <gmime/gmime-tnef-property.h>

G_BEGIN_DECLS

/**
 * GMimeTnefReader:
 *
 * A forward-only TNEF attribute reader. Advancing skips unread bytes and
 * verifies the previous attribute's checksum. Malformed input is reported
 * through the compliance logger; I/O and invalid operations use #GError.
 **/
#define GMIME_TYPE_TNEF_READER (g_mime_tnef_reader_get_type ())

G_DECLARE_FINAL_TYPE (GMimeTnefReader, g_mime_tnef_reader, GMIME, TNEF_READER, GObject)

GMimeTnefReader *g_mime_tnef_reader_new (GMimeStream *stream, const GMimeTnefOptions *options);

const GMimeTnefOptions *g_mime_tnef_reader_get_options (GMimeTnefReader *reader);

void g_mime_tnef_reader_set_compliance_logger (GMimeTnefReader *reader, GMimeTnefComplianceLogger logger,
					       gpointer user_data, GDestroyNotify destroy);

GMimeTnefComplianceOptions *g_mime_tnef_reader_get_compliance_options (GMimeTnefReader *reader);
void g_mime_tnef_reader_set_compliance_options (GMimeTnefReader *reader, const GMimeTnefComplianceOptions *options);

guint g_mime_tnef_reader_get_depth (GMimeTnefReader *reader);
guint16 g_mime_tnef_reader_get_legacy_key (GMimeTnefReader *reader);
int g_mime_tnef_reader_get_codepage (GMimeTnefReader *reader);

GMimeTnefAttributeLevel g_mime_tnef_reader_get_level (GMimeTnefReader *reader);
GMimeTnefAttributeTag g_mime_tnef_reader_get_tag (GMimeTnefReader *reader);
GMimeTnefAttributeType g_mime_tnef_reader_get_attribute_type (GMimeTnefReader *reader);

guint g_mime_tnef_reader_get_length (GMimeTnefReader *reader);
gint64 g_mime_tnef_reader_get_stream_offset (GMimeTnefReader *reader);

gboolean g_mime_tnef_reader_read (GMimeTnefReader *reader, GError **error);
gboolean g_mime_tnef_reader_read_value_as_int16 (GMimeTnefReader *reader, gint16 *value, GError **error);
gboolean g_mime_tnef_reader_read_value_as_int32 (GMimeTnefReader *reader, gint32 *value, GError **error);
GDateTime *g_mime_tnef_reader_read_value_as_date_time (GMimeTnefReader *reader, GError **error);
char *g_mime_tnef_reader_read_value_as_string (GMimeTnefReader *reader, GError **error);
GBytes *g_mime_tnef_reader_read_value_as_bytes (GMimeTnefReader *reader, GError **error);
GMimeStream *g_mime_tnef_reader_open_value_stream (GMimeTnefReader *reader, GError **error);

GMimeTnefPropertyReader *g_mime_tnef_reader_get_property_reader (GMimeTnefReader *reader, GError **error);

/**
 * GMimeTnefPropertyReader:
 *
 * A forward-only MAPI property reader, invalidated when its attribute reader
 * advances. Reading a property automatically positions its first value.
 * Fixed-width scalar values may be reread; variable-length values are claimed
 * by the first scalar, byte-array, stream or embedded-reader operation.
 **/
#define GMIME_TYPE_TNEF_PROPERTY_READER (g_mime_tnef_property_reader_get_type ())

G_DECLARE_FINAL_TYPE (GMimeTnefPropertyReader, g_mime_tnef_property_reader, GMIME, TNEF_PROPERTY_READER, GObject)

int g_mime_tnef_property_reader_get_row_count (GMimeTnefPropertyReader *reader);
int g_mime_tnef_property_reader_get_property_count (GMimeTnefPropertyReader *reader);

GMimeTnefPropertyTag g_mime_tnef_property_reader_get_tag (GMimeTnefPropertyReader *reader);

GMimeTnefPropertyType g_mime_tnef_property_reader_get_property_type (GMimeTnefPropertyReader *reader);
gboolean g_mime_tnef_property_reader_is_multi_valued (GMimeTnefPropertyReader *reader);
GMimeTnefNameId *g_mime_tnef_property_reader_get_name (GMimeTnefPropertyReader *reader);

int g_mime_tnef_property_reader_get_value_count (GMimeTnefPropertyReader *reader);

gboolean g_mime_tnef_property_reader_is_value_consumed (GMimeTnefPropertyReader *reader);
gboolean g_mime_tnef_property_reader_is_embedded_message (GMimeTnefPropertyReader *properties);

gboolean g_mime_tnef_property_reader_read_next_row (GMimeTnefPropertyReader *properties, GError **error);
gboolean g_mime_tnef_property_reader_read_next_property (GMimeTnefPropertyReader *properties, GError **error);
gboolean g_mime_tnef_property_reader_read_next_value (GMimeTnefPropertyReader *properties, GError **error);

gboolean g_mime_tnef_property_reader_read_value_as_boolean (GMimeTnefPropertyReader *properties, gboolean *value,
							    GError **error);
gboolean g_mime_tnef_property_reader_read_value_as_int16 (GMimeTnefPropertyReader *properties, gint16 *value,
							  GError **error);
gboolean g_mime_tnef_property_reader_read_value_as_int32 (GMimeTnefPropertyReader *properties, gint32 *value,
							  GError **error);
gboolean g_mime_tnef_property_reader_read_value_as_int64 (GMimeTnefPropertyReader *properties, gint64 *value,
							  GError **error);
gboolean g_mime_tnef_property_reader_read_value_as_float (GMimeTnefPropertyReader *properties, float *value,
							  GError **error);
gboolean g_mime_tnef_property_reader_read_value_as_double (GMimeTnefPropertyReader *properties, double *value,
							   GError **error);
GDateTime *g_mime_tnef_property_reader_read_value_as_date_time (GMimeTnefPropertyReader *properties, GError **error);
char *g_mime_tnef_property_reader_read_value_as_guid (GMimeTnefPropertyReader *properties, GError **error);
char *g_mime_tnef_property_reader_read_value_as_string (GMimeTnefPropertyReader *properties, GError **error);
GBytes *g_mime_tnef_property_reader_read_value_as_bytes (GMimeTnefPropertyReader *properties, GError **error);

gboolean g_mime_tnef_property_reader_read_value (GMimeTnefPropertyReader *properties, GValue *value, GError **error);

GMimeStream *g_mime_tnef_property_reader_open_value_stream (GMimeTnefPropertyReader *properties, GError **error);

GMimeTnefReader *g_mime_tnef_property_reader_open_embedded_message (GMimeTnefPropertyReader *properties,
								    GError **error);

GMimeTnefProperty *g_mime_tnef_property_reader_read_property (GMimeTnefPropertyReader *properties, GError **error);

GMimeTnefPropertySet *g_mime_tnef_property_reader_read_property_set (GMimeTnefPropertyReader *properties,
								     GError **error);

GPtrArray *g_mime_tnef_property_reader_read_rows_as_property_sets (GMimeTnefPropertyReader *properties, GError **error);

G_END_DECLS

#endif
