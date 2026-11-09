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

#include "gmime-tnef-private.h"

G_DEFINE_BOXED_TYPE (GMimeTnefProperty, g_mime_tnef_property, g_mime_tnef_property_ref, g_mime_tnef_property_unref)
G_DEFINE_BOXED_TYPE (GMimeTnefPropertySet, g_mime_tnef_property_set, g_mime_tnef_property_set_ref,
		     g_mime_tnef_property_set_unref)

GMimeTnefProperty *
_g_mime_tnef_property_new (GMimeTnefPropertyTag tag, GMimeTnefNameId *name)
{
	GMimeTnefProperty *property = g_new0 (GMimeTnefProperty, 1);

	property->ref_count = 1;
	property->tag = tag;
	property->name = name ? g_mime_tnef_name_id_ref (name) : NULL;
	property->values = g_array_new (FALSE, FALSE, sizeof (GValue));

	g_array_set_clear_func (property->values, (GDestroyNotify) g_value_unset);

	return property;
}

/**
 * g_mime_tnef_property_ref:
 * @property: a property snapshot
 *
 * References the snapshot.
 *
 * Returns: (transfer full): @property
 **/
GMimeTnefProperty *
g_mime_tnef_property_ref (GMimeTnefProperty *property)
{
	g_return_val_if_fail (property != NULL, NULL);

	g_atomic_int_inc (&property->ref_count);

	return property;
}

/**
 * g_mime_tnef_property_unref:
 * @property: a property snapshot
 *
 * Releases a reference.
 **/
void
g_mime_tnef_property_unref (GMimeTnefProperty *property)
{
	g_return_if_fail (property != NULL);

	if (g_atomic_int_dec_and_test (&property->ref_count)) {
		g_clear_pointer (&property->name, g_mime_tnef_name_id_unref);
		g_array_unref (property->values);
		g_free (property);
	}
}

/**
 * g_mime_tnef_property_get_tag:
 * @property: a property snapshot
 *
 * Gets the original tag. Named identifiers are local to the source stream.
 *
 * Returns: the property tag
 **/
GMimeTnefPropertyTag
g_mime_tnef_property_get_tag (const GMimeTnefProperty *property)
{
	g_return_val_if_fail (property != NULL, 0);

	return property->tag;
}

/**
 * g_mime_tnef_property_get_name:
 * @property: a property snapshot
 *
 * Gets the named-property identity.
 *
 * Returns: (transfer none) (nullable): the identity, or %NULL
 **/
GMimeTnefNameId *
g_mime_tnef_property_get_name (const GMimeTnefProperty *property)
{
	g_return_val_if_fail (property != NULL, NULL);

	return property->name;
}

/**
 * g_mime_tnef_property_get_count:
 * @property: a property snapshot
 *
 * Gets the number of materialized values. Malformed or size-limited
 * properties may have no values.
 *
 * Returns: the value count
 **/
guint
g_mime_tnef_property_get_count (const GMimeTnefProperty *property)
{
	g_return_val_if_fail (property != NULL, 0);

	return property->values->len;
}

/**
 * g_mime_tnef_property_get_value:
 * @property: a property snapshot
 * @index: a zero-based value index
 *
 * Gets a value without consuming it. Uses the property reader's native
 * GLib representations, including exact scaled-integer Currency values.
 *
 * Returns: (transfer none): the immutable value
 **/
const GValue *
g_mime_tnef_property_get_value (const GMimeTnefProperty *property, guint index)
{
	g_return_val_if_fail (property != NULL && index < property->values->len, NULL);

	return &g_array_index (property->values, GValue, index);
}

GMimeTnefPropertySet *
_g_mime_tnef_property_set_new (void)
{
	GMimeTnefPropertySet *properties = g_new0 (GMimeTnefPropertySet, 1);

	properties->ref_count = 1;
	properties->items = g_ptr_array_new_with_free_func ((GDestroyNotify) g_mime_tnef_property_unref);

	return properties;
}

/**
 * g_mime_tnef_property_set_ref:
 * @properties: a property set
 *
 * References the set.
 *
 * Returns: (transfer full): @properties
 **/
GMimeTnefPropertySet *
g_mime_tnef_property_set_ref (GMimeTnefPropertySet *properties)
{
	g_return_val_if_fail (properties != NULL, NULL);

	g_atomic_int_inc (&properties->ref_count);

	return properties;
}

/**
 * g_mime_tnef_property_set_unref:
 * @properties: a property set
 *
 * Releases a reference.
 **/
void
g_mime_tnef_property_set_unref (GMimeTnefPropertySet *properties)
{
	g_return_if_fail (properties != NULL);

	if (g_atomic_int_dec_and_test (&properties->ref_count)) {
		g_ptr_array_unref (properties->items);
		g_free (properties);
	}
}

/**
 * g_mime_tnef_property_set_get_count:
 * @properties: a property set
 *
 * Gets the number of properties, including duplicates.
 *
 * Returns: the property count
 **/
guint
g_mime_tnef_property_set_get_count (const GMimeTnefPropertySet *properties)
{
	g_return_val_if_fail (properties != NULL, 0);

	return properties->items->len;
}

/**
 * g_mime_tnef_property_set_get_property:
 * @properties: a property set
 * @index: a zero-based property index
 *
 * Gets a property in original wire order.
 *
 * Returns: (transfer none): the property
 **/
GMimeTnefProperty *
g_mime_tnef_property_set_get_property (const GMimeTnefPropertySet *properties, guint index)
{
	g_return_val_if_fail (properties != NULL && index < properties->items->len, NULL);

	return g_ptr_array_index (properties->items, index);
}

/**
 * g_mime_tnef_property_set_lookup:
 * @properties: a property set
 * @tag: the requested tag
 *
 * Finds the first property with the same identifier, regardless of type.
 * Use g_mime_tnef_property_set_lookup_name() for named identities, which are
 * stable across streams.
 *
 * Returns: (transfer none) (nullable): the property, or %NULL if absent
 **/
GMimeTnefProperty *
g_mime_tnef_property_set_lookup (const GMimeTnefPropertySet *properties, GMimeTnefPropertyTag tag)
{
	GMimeTnefProperty *property;
	guint i;

	g_return_val_if_fail (properties != NULL, NULL);

	for (i = 0; i < properties->items->len; i++) {
		property = g_ptr_array_index (properties->items, i);

		if (g_mime_tnef_property_tag_get_id (property->tag) == g_mime_tnef_property_tag_get_id (tag))
			return property;
	}

	return NULL;
}

/**
 * g_mime_tnef_property_set_lookup_name:
 * @properties: a property set
 * @name: the requested identity
 *
 * Finds the first property with this named identity.
 *
 * Returns: (transfer none) (nullable): the property, or %NULL if absent
 **/
GMimeTnefProperty *
g_mime_tnef_property_set_lookup_name (const GMimeTnefPropertySet *properties, const GMimeTnefNameId *name)
{
	GMimeTnefProperty *property;
	guint i;

	g_return_val_if_fail (properties != NULL && name != NULL, NULL);

	for (i = 0; i < properties->items->len; i++) {
		property = g_ptr_array_index (properties->items, i);

		if (property->name && g_mime_tnef_name_id_equal (property->name, name))
			return property;
	}

	return NULL;
}
