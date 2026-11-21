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

#ifndef __GMIME_TNEF_PART_H__
#define __GMIME_TNEF_PART_H__

#include <gmime/gmime-part.h>
#include <gmime/gmime-tnef-message.h>

G_BEGIN_DECLS

#define GMIME_TYPE_TNEF_PART (g_mime_tnef_part_get_type ())

/**
 * GMimeTnefPart:
 *
 * A MIME part containing a TNEF message.
 **/
G_DECLARE_DERIVABLE_TYPE (GMimeTnefPart, g_mime_tnef_part, GMIME, TNEF_PART, GMimePart)

struct _GMimeTnefPartClass {
	GMimePartClass parent_class;
};

GMimeTnefPart *g_mime_tnef_part_new (void);
GMimeTnefMessage *g_mime_tnef_part_load_tnef_message (GMimeTnefPart *part, const GMimeTnefOptions *options,
						      GError **error);

G_END_DECLS
#endif /* __GMIME_TNEF_PART_H__ */
