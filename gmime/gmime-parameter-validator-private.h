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

#ifndef __GMIME_PARAMETER_VALIDATOR_PRIVATE_H__
#define __GMIME_PARAMETER_VALIDATOR_PRIVATE_H__

#include "gmime-compliance.h"

G_BEGIN_DECLS

/* Inspect a raw Content-Type (TRUE) or Content-Disposition (FALSE) value.
 * Coordinates identify buffer[0]; callbacks borrow issues. Missing segments
 * are reported after scanning, at the first continuation's element start.
 * Malformed declarations/fragments use ElementStart positions. RFC 2231 encoded
 * syntax is checked per segment without charset lookup or value assembly. */
G_GNUC_INTERNAL void _g_mime_parameter_validate (const guint8 *buffer, gsize length, gboolean content_type,
	GMimeComplianceIssueFunc report, gpointer user_data, GMimeComplianceContext context,
	gint64 stream_offset, guint64 line_number, guint64 column_number);

G_END_DECLS

#endif /* __GMIME_PARAMETER_VALIDATOR_PRIVATE_H__ */
