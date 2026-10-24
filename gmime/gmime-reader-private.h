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

#ifndef __GMIME_READER_PRIVATE_H__
#define __GMIME_READER_PRIVATE_H__

#include "gmime-reader.h"

G_BEGIN_DECLS

GMimeStream *_g_mime_reader_get_stream (GMimeReader *reader);
gboolean _g_mime_reader_skip_blank_lines (GMimeReader *reader, GError **error);
gboolean _g_mime_reader_restore_stream_position (GMimeReader *reader, GError **error);
GMimeStream *_g_mime_reader_read_to_eos (GMimeReader *reader, GError **error);

G_END_DECLS

#endif /* __GMIME_READER_PRIVATE_H__ */
