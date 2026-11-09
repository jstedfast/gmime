/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) Jeffrey Stedfast
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

#ifndef __GMIME_TNEF_WRITER_H__
#define __GMIME_TNEF_WRITER_H__

#include <gmime/gmime-stream.h>
#include <gmime/gmime-tnef-types.h>
#include <gmime/gmime-tnef-property.h>

G_BEGIN_DECLS

/**
 * GMimeTnefWriter:
 *
 * A forward-only TNEF writer. The header, version, codepage, attribute
 * lengths and checksums are generated automatically. Message attributes
 * precede attachments; MapiProperties and Attachment finish their respective
 * property lists. Explicitly close child streams/property writers and then
 * the writer to report completion errors. Finalization performs no I/O.
 **/
#define GMIME_TYPE_TNEF_WRITER (g_mime_tnef_writer_get_type ())

G_DECLARE_FINAL_TYPE (GMimeTnefWriter, g_mime_tnef_writer, GMIME, TNEF_WRITER, GObject)

/**
 * GMimeTnefTemporaryStreamFactory:
 * @user_data: (closure) (nullable): callback data
 * @error: return location for an error
 *
 * Creates a new, empty, readable/writable, seekable stream at position zero.
 * The writer owns it. Embedded writers inherit the configured factory.
 *
 * Returns: (transfer full) (nullable): a temporary stream, or %NULL on error
 **/
typedef GMimeStream *(*GMimeTnefTemporaryStreamFactory) (gpointer user_data, GError **error);

GMimeTnefWriter *g_mime_tnef_writer_new (GMimeStream *stream, int codepage, guint16 legacy_key, GError **error);

int g_mime_tnef_writer_get_codepage (GMimeTnefWriter *writer);
guint16 g_mime_tnef_writer_get_legacy_key (GMimeTnefWriter *writer);

void g_mime_tnef_writer_set_temporary_stream_factory (GMimeTnefWriter *writer, GMimeTnefTemporaryStreamFactory factory,
						      gpointer user_data, GDestroyNotify destroy);

gboolean g_mime_tnef_writer_write_attribute_int32 (GMimeTnefWriter *writer, GMimeTnefAttributeTag tag, gint32 value,
						   GError **error);
gboolean g_mime_tnef_writer_write_attribute_string (GMimeTnefWriter *writer, GMimeTnefAttributeTag tag,
						    const char *value, GError **error);
gboolean g_mime_tnef_writer_write_attribute_date_time (GMimeTnefWriter *writer, GMimeTnefAttributeTag tag,
						       GDateTime *value, GError **error);
gboolean g_mime_tnef_writer_write_attribute_bytes (GMimeTnefWriter *writer, GMimeTnefAttributeTag tag,
						   const guint8 *buffer, gsize length, GError **error);
GMimeStream *g_mime_tnef_writer_open_attribute_stream (GMimeTnefWriter *writer, GMimeTnefAttributeTag tag,
						       GError **error);
GMimeTnefPropertyWriter *g_mime_tnef_writer_open_property_writer (GMimeTnefWriter *writer, GMimeTnefAttributeTag tag,
								  GError **error);

gboolean g_mime_tnef_writer_flush (GMimeTnefWriter *writer, GError **error);
gboolean g_mime_tnef_writer_close (GMimeTnefWriter *writer, GError **error);

/**
 * GMimeTnefPropertyWriter:
 *
 * A MAPI property-list or recipient-table writer. Start each property with
 * its tag or name, then write its values. Recipient tables require
 * g_mime_tnef_property_writer_begin_row(). Explicit close patches counts
 * and completes the attribute.
 **/
#define GMIME_TYPE_TNEF_PROPERTY_WRITER (g_mime_tnef_property_writer_get_type ())

G_DECLARE_FINAL_TYPE (GMimeTnefPropertyWriter, g_mime_tnef_property_writer, GMIME, TNEF_PROPERTY_WRITER, GObject)

int g_mime_tnef_property_writer_get_row_count (GMimeTnefPropertyWriter *writer);
int g_mime_tnef_property_writer_get_property_count (GMimeTnefPropertyWriter *writer);

gboolean g_mime_tnef_property_writer_begin_row (GMimeTnefPropertyWriter *writer, GError **error);

gboolean g_mime_tnef_property_writer_write_property_tag (GMimeTnefPropertyWriter *writer, GMimeTnefPropertyTag tag,
							 GError **error);
gboolean g_mime_tnef_property_writer_write_named_property (GMimeTnefPropertyWriter *writer, GMimeTnefNameId *name,
							   guint16 type, GError **error);
gboolean g_mime_tnef_property_writer_write_value_boolean (GMimeTnefPropertyWriter *writer, gboolean value,
							  GError **error);
gboolean g_mime_tnef_property_writer_write_value_int16 (GMimeTnefPropertyWriter *writer, gint16 value, GError **error);
gboolean g_mime_tnef_property_writer_write_value_int32 (GMimeTnefPropertyWriter *writer, gint32 value, GError **error);
gboolean g_mime_tnef_property_writer_write_value_int64 (GMimeTnefPropertyWriter *writer, gint64 value, GError **error);
gboolean g_mime_tnef_property_writer_write_value_float (GMimeTnefPropertyWriter *writer, float value, GError **error);
gboolean g_mime_tnef_property_writer_write_value_double (GMimeTnefPropertyWriter *writer, double value, GError **error);
gboolean g_mime_tnef_property_writer_write_value_currency (GMimeTnefPropertyWriter *writer, gint64 scaled_value,
							   GError **error);
gboolean g_mime_tnef_property_writer_write_value_date_time (GMimeTnefPropertyWriter *writer, GDateTime *value,
							    GError **error);
gboolean g_mime_tnef_property_writer_write_value_guid (GMimeTnefPropertyWriter *writer, const char *value,
						       GError **error);
gboolean g_mime_tnef_property_writer_write_value_string (GMimeTnefPropertyWriter *writer, const char *value,
							 GError **error);
gboolean g_mime_tnef_property_writer_write_value_bytes (GMimeTnefPropertyWriter *writer, const guint8 *buffer,
							gsize length, GError **error);
gboolean g_mime_tnef_property_writer_write_value (GMimeTnefPropertyWriter *writer, const GValue *value, GError **error);
gboolean g_mime_tnef_property_writer_write_property (GMimeTnefPropertyWriter *writer, const GMimeTnefProperty *property,
						     GError **error);
GMimeStream *g_mime_tnef_property_writer_open_value_stream (GMimeTnefPropertyWriter *writer, GError **error);

GMimeStream *g_mime_tnef_property_writer_open_rtf_compressed_stream (GMimeTnefPropertyWriter *writer, gboolean compress,
								     GError **error);
GMimeTnefWriter *g_mime_tnef_property_writer_open_embedded_message (GMimeTnefPropertyWriter *writer, int codepage,
								    GError **error);
gboolean g_mime_tnef_property_writer_close (GMimeTnefPropertyWriter *writer, GError **error);

G_END_DECLS

#endif
