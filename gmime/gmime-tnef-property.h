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

#ifndef __GMIME_TNEF_PROPERTY_H__
#define __GMIME_TNEF_PROPERTY_H__

#include <gmime/gmime-tnef-types.h>

G_BEGIN_DECLS

/**
 * GMimeTnefProperty:
 *
 * An immutable snapshot of a MAPI property's tag, name and native GLib
 * values. Values remain valid after the input reader advances or is freed.
 * Binary data is materialized; use value streams for large attachments.
 **/
typedef struct _GMimeTnefProperty GMimeTnefProperty;

#define GMIME_TYPE_TNEF_PROPERTY (g_mime_tnef_property_get_type ())
#define GMIME_TYPE_TNEF_PROPERTY_SET (g_mime_tnef_property_set_get_type ())

GType g_mime_tnef_property_get_type (void);

GMimeTnefProperty *g_mime_tnef_property_ref (GMimeTnefProperty *property);
void g_mime_tnef_property_unref (GMimeTnefProperty *property);

GMimeTnefPropertyTag g_mime_tnef_property_get_tag (const GMimeTnefProperty *property);
GMimeTnefNameId *g_mime_tnef_property_get_name (const GMimeTnefProperty *property);
guint g_mime_tnef_property_get_count (const GMimeTnefProperty *property);
const GValue *g_mime_tnef_property_get_value (const GMimeTnefProperty *property, guint index);

/**
 * GMimeTnefPropertySet:
 *
 * An immutable ordered collection of MAPI properties. Duplicates are
 * preserved; lookup returns the first matching identifier or name.
 **/
typedef struct _GMimeTnefPropertySet GMimeTnefPropertySet;

GType g_mime_tnef_property_set_get_type (void);

GMimeTnefPropertySet *g_mime_tnef_property_set_ref (GMimeTnefPropertySet *properties);
void g_mime_tnef_property_set_unref (GMimeTnefPropertySet *properties);

guint g_mime_tnef_property_set_get_count (const GMimeTnefPropertySet *properties);
GMimeTnefProperty *g_mime_tnef_property_set_get_property (const GMimeTnefPropertySet *properties, guint index);
GMimeTnefProperty *g_mime_tnef_property_set_lookup (const GMimeTnefPropertySet *properties, GMimeTnefPropertyTag tag);
GMimeTnefProperty *g_mime_tnef_property_set_lookup_name (const GMimeTnefPropertySet *properties,
							 const GMimeTnefNameId *name);

G_END_DECLS

#endif
