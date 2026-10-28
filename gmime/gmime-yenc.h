/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* GMime - Copyright (C) 2026 Jeffrey Stedfast
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef __GMIME_YENC_H__
#define __GMIME_YENC_H__

#include <gmime/gmime-stream.h>
#include <gmime/gmime-format-options.h>

G_BEGIN_DECLS

typedef struct _GMimeYencOptions GMimeYencOptions;

#define GMIME_TYPE_YENC_OPTIONS (g_mime_yenc_options_get_type ())
GType g_mime_yenc_options_get_type (void);
GMimeYencOptions *g_mime_yenc_options_new (void);
GMimeYencOptions *g_mime_yenc_options_clone (GMimeYencOptions *options);
void g_mime_yenc_options_free (GMimeYencOptions *options);
guint g_mime_yenc_options_get_max_line_length (GMimeYencOptions *options);
void g_mime_yenc_options_set_max_line_length (GMimeYencOptions *options, guint length);
gint64 g_mime_yenc_options_get_max_decoded_size (GMimeYencOptions *options);
void g_mime_yenc_options_set_max_decoded_size (GMimeYencOptions *options, gint64 size);
const char *g_mime_yenc_options_get_filename_charset (GMimeYencOptions *options);
gboolean g_mime_yenc_options_set_filename_charset (GMimeYencOptions *options, const char *charset, GError **error);
GMimeNewLineFormat g_mime_yenc_options_get_newline_format (GMimeYencOptions *options);
void g_mime_yenc_options_set_newline_format (GMimeYencOptions *options, GMimeNewLineFormat format);

#define GMIME_TYPE_YENC_HEADER (g_mime_yenc_header_get_type ())
G_DECLARE_FINAL_TYPE (GMimeYencHeader, g_mime_yenc_header, GMIME, YENC_HEADER, GObject)

GMimeYencHeader *g_mime_yenc_header_new (const char *filename, gint64 size, guint line_length,
				       guint part_number, guint total_parts, gint64 begin, gint64 end, GError **error);
const char *g_mime_yenc_header_get_filename (GMimeYencHeader *header);
gint64 g_mime_yenc_header_get_size (GMimeYencHeader *header);
guint g_mime_yenc_header_get_line_length (GMimeYencHeader *header);
guint g_mime_yenc_header_get_part_number (GMimeYencHeader *header);
guint g_mime_yenc_header_get_total_parts (GMimeYencHeader *header);
gint64 g_mime_yenc_header_get_begin (GMimeYencHeader *header);
gint64 g_mime_yenc_header_get_end (GMimeYencHeader *header);
gint64 g_mime_yenc_header_get_decoded_size (GMimeYencHeader *header);

#define GMIME_TYPE_YENC_READER (g_mime_yenc_reader_get_type ())
G_DECLARE_FINAL_TYPE (GMimeYencReader, g_mime_yenc_reader, GMIME, YENC_READER, GObject)

GMimeYencReader *g_mime_yenc_reader_new (GMimeStream *stream, GMimeYencOptions *options);
gboolean g_mime_yenc_reader_read_next (GMimeYencReader *reader, GError **error);
gboolean g_mime_yenc_reader_decode_to (GMimeYencReader *reader, GMimeStream *destination, GError **error);
GMimeYencHeader *g_mime_yenc_reader_get_header (GMimeYencReader *reader);
gint64 g_mime_yenc_reader_get_decoded_size (GMimeYencReader *reader);
gboolean g_mime_yenc_reader_get_checksum (GMimeYencReader *reader, guint32 *checksum);
gboolean g_mime_yenc_reader_get_expected_checksum (GMimeYencReader *reader, guint32 *checksum);
gboolean g_mime_yenc_reader_get_expected_part_checksum (GMimeYencReader *reader, guint32 *checksum);

#define GMIME_TYPE_YENC_WRITER (g_mime_yenc_writer_get_type ())
G_DECLARE_FINAL_TYPE (GMimeYencWriter, g_mime_yenc_writer, GMIME, YENC_WRITER, GObject)

GMimeYencWriter *g_mime_yenc_writer_new (GMimeStream *stream, GMimeYencOptions *options);
gboolean g_mime_yenc_writer_write (GMimeYencWriter *writer, GMimeYencHeader *header, GMimeStream *source,
				   const guint32 *file_checksum, guint32 *checksum, GError **error);

G_END_DECLS

#endif
