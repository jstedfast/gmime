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

#ifndef __GMIME_TNEF_PRIVATE_H__
#define __GMIME_TNEF_PRIVATE_H__

#include "gmime-tnef-reader.h"

struct _GMimeTnefOptions {
	gint ref_count;
	guint max_nesting_depth;
	guint max_property_value_length;
	guint64 max_total_data_bytes;
	int default_codepage;
};

struct _GMimeTnefComplianceOptions {
	gint ref_count;
	guint max_issues_per_violation;
};

struct _GMimeTnefNameId {
	gint ref_count;
	char property_set[37];
	GMimeTnefNameIdKind kind;
	gint32 id;
	char *name;
};

struct _GMimeTnefProperty {
	gint ref_count;
	GMimeTnefPropertyTag tag;
	GMimeTnefNameId *name;
	GArray *values;
};

struct _GMimeTnefPropertySet {
	gint ref_count;
	GPtrArray *items;
};

G_GNUC_INTERNAL GMimeTnefProperty *_g_mime_tnef_property_new (GMimeTnefPropertyTag tag, GMimeTnefNameId *name);
G_GNUC_INTERNAL GMimeTnefPropertySet *_g_mime_tnef_property_set_new (void);

G_GNUC_INTERNAL guint16 _g_mime_tnef_get_uint16 (const guint8 *buffer);
G_GNUC_INTERNAL guint32 _g_mime_tnef_get_uint32 (const guint8 *buffer);
G_GNUC_INTERNAL guint64 _g_mime_tnef_get_uint64 (const guint8 *buffer);
G_GNUC_INTERNAL void _g_mime_tnef_put_uint16 (guint8 *buffer, guint16 value);
G_GNUC_INTERNAL void _g_mime_tnef_put_uint32 (guint8 *buffer, guint32 value);
G_GNUC_INTERNAL void _g_mime_tnef_put_uint64 (guint8 *buffer, guint64 value);
G_GNUC_INTERNAL char *_g_mime_tnef_guid_to_string (const guint8 *buffer);
G_GNUC_INTERNAL gboolean _g_mime_tnef_guid_from_string (const char *text, guint8 *buffer);
G_GNUC_INTERNAL char *_g_mime_tnef_get_charset (int codepage);
G_GNUC_INTERNAL char *_g_mime_tnef_decode_string (const char *charset, GBytes *bytes, gboolean unicode);
G_GNUC_INTERNAL GBytes *_g_mime_tnef_encode_string (const char *charset, const char *text, gboolean unicode,
						    GError **error);
G_GNUC_INTERNAL int _g_mime_tnef_get_fixed_width (GMimeTnefPropertyType type);
G_GNUC_INTERNAL int _g_mime_tnef_get_attribute_level (GMimeTnefAttributeTag tag);
G_GNUC_INTERNAL gboolean _g_mime_tnef_invalid_operation (GError **error, const char *message);
G_GNUC_INTERNAL gboolean _g_mime_tnef_io_error (GError **error);
G_GNUC_INTERNAL gboolean _g_mime_tnef_write_all (GMimeStream *stream, const guint8 *buffer, gsize length,
						 GError **error);
G_GNUC_INTERNAL GDateTime *_g_mime_tnef_decode_date (GMimeTnefPropertyType type, const guint8 *buffer);
G_GNUC_INTERNAL ssize_t _g_mime_tnef_stream_no_read (GMimeStream *stream, char *buffer, size_t length);
G_GNUC_INTERNAL int _g_mime_tnef_stream_no_reset (GMimeStream *stream);
G_GNUC_INTERNAL gint64 _g_mime_tnef_stream_no_seek (GMimeStream *stream, gint64 offset, GMimeSeekWhence whence);
G_GNUC_INTERNAL GMimeStream *_g_mime_tnef_stream_no_substream (GMimeStream *stream, gint64 start, gint64 end);

typedef gboolean (*GMimeTnefStreamClosedFunc) (GObject *owner, GMimeStream *stream, const GError *fault,
					       GError **error);
G_GNUC_INTERNAL GMimeStream *_g_mime_tnef_rtf_write_stream_new (GMimeStream *output, gboolean compress, GObject *owner,
								GMimeTnefStreamClosedFunc completed, GError **error);

#endif
