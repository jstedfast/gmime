/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2022 Jeffrey Stedfast
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

#include "gmime-internet-address-private.h"
#include "gmime-events.h"


#include "gmime-object-private.h"

/**
 * SECTION: gmime-group-address
 * @title: GMimeGroupAddress
 * @short_description: rfc822 'group' address
 * @see_also: #GMimeInternetAddress
 *
 * An #GMimeGroupAddress represents an rfc822 'group' address.
 **/


static void g_mime_group_address_class_init (GMimeGroupAddressClass *klass);
static void g_mime_group_address_init (GMimeGroupAddress *group);
static void g_mime_group_address_finalize (GObject *object);

static void group_to_string (GMimeInternetAddress *ia, GMimeFormatOptions *options, guint32 flags,
			     size_t *linelen, GString *out);


static GObjectClass *group_parent_class = NULL;

static GMimeInternetAddress *
group_clone (GMimeInternetAddress *ia)
{
	GMimeGroupAddressPrivate *group_priv = _g_mime_group_address_get_private (GMIME_GROUP_ADDRESS (ia));
	GMimeInternetAddressList *members = g_mime_internet_address_list_clone (group_priv->members);
	GMimeInternetAddressPrivate *ia_priv = _g_mime_internet_address_get_private (ia);
	GMimeInternetAddress *clone = g_mime_group_address_new (ia_priv->name);
	GMimeInternetAddressPrivate *clone_priv;

	g_mime_group_address_set_members (GMIME_GROUP_ADDRESS (clone), members);
	clone_priv = _g_mime_internet_address_get_private (clone);

	clone_priv->charset = g_strdup (ia_priv->charset);
	g_object_unref (members);

	return clone;
}

static gboolean
group_equal (GMimeInternetAddress *ia, GMimeInternetAddress *other)
{
	GMimeInternetAddressPrivate *ia_priv = _g_mime_internet_address_get_private (ia);
	GMimeInternetAddressPrivate *other_priv = _g_mime_internet_address_get_private (other);
	GMimeGroupAddressPrivate *group_priv, *other_group_priv;

	if (!GMIME_IS_GROUP_ADDRESS (other) || g_strcmp0 (ia_priv->name, other_priv->name))
		return FALSE;

	group_priv = _g_mime_group_address_get_private (GMIME_GROUP_ADDRESS (ia));
	other_group_priv = _g_mime_group_address_get_private (GMIME_GROUP_ADDRESS (other));

	return g_mime_internet_address_list_equal (group_priv->members, other_group_priv->members);
}



G_DEFINE_TYPE_WITH_PRIVATE (GMimeGroupAddress, g_mime_group_address, GMIME_TYPE_INTERNET_ADDRESS)

GMimeGroupAddressPrivate *
_g_mime_group_address_get_private (GMimeGroupAddress *object)
{
	return g_mime_group_address_get_instance_private (object);
}


static void
g_mime_group_address_class_init (GMimeGroupAddressClass *klass)
{
	GMimeInternetAddressClass *address_class = GMIME_INTERNET_ADDRESS_CLASS (klass);
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	group_parent_class = g_type_class_ref (GMIME_TYPE_INTERNET_ADDRESS);

	object_class->finalize = g_mime_group_address_finalize;

	address_class->to_string = group_to_string;
	address_class->clone = group_clone;
	address_class->equal = group_equal;
}

static void
members_changed (gpointer sender, gpointer args, gpointer user_data)
{
	GMimeInternetAddress *group = user_data;
	GMimeInternetAddressPrivate *priv = _g_mime_internet_address_get_private (((GMimeInternetAddress *) group));

	g_mime_event_emit (priv->changed, NULL);
}

static void
g_mime_group_address_init (GMimeGroupAddress *group)
{
	GMimeGroupAddressPrivate *priv = _g_mime_group_address_get_private (group);

	priv->members = g_mime_internet_address_list_new ();

	g_mime_event_add (priv->members->changed, (GMimeEventCallback) members_changed, group);
}

static void
g_mime_group_address_finalize (GObject *object)
{
	GMimeGroupAddress *group = (GMimeGroupAddress *) object;
	GMimeGroupAddressPrivate *priv = _g_mime_group_address_get_private (group);

	g_mime_event_remove (priv->members->changed, (GMimeEventCallback) members_changed, group);

	g_object_unref (priv->members);

	G_OBJECT_CLASS (group_parent_class)->finalize (object);
}


/**
 * g_mime_group_address_new:
 * @name: (nullable): group name
 *
 * Creates a new #GMimeGroupAddress object with the specified
 * @name. Leading and trailing Unicode whitespace is removed from @name.
 *
 * Returns: a new #GMimeGroupAddress object.
 *
 * Note: The @name string should be in UTF-8.
 **/
GMimeInternetAddress *
g_mime_group_address_new (const char *name)
{
	GMimeInternetAddress *group;

	g_return_val_if_fail (name == NULL || g_utf8_validate (name, -1, NULL), NULL);

	group = g_object_new (GMIME_TYPE_GROUP_ADDRESS, NULL);
	_g_mime_internet_address_set_name (group, name);

	return group;
}

/**
 * g_mime_group_address_parse:
 * @options: (nullable): parser options
 * @str: group text
 * @length: byte length, or -1 for a NUL-terminated string
 * @error: (nullable): return location for a #GError
 *
 * Returns: (nullable) (transfer full): exactly one group, or %NULL on error.
 **/
GMimeInternetAddress *
g_mime_group_address_parse (GMimeParserOptions *options, const char *str, gssize length, GError **error)
{
	return _g_mime_internet_address_parse (options, str, length, GMIME_TYPE_GROUP_ADDRESS, error);
}


/**
 * g_mime_group_address_set_members:
 * @group: a #GMimeGroupAddress
 * @members: a #GMimeInternetAddressList
 *
 * Set the members of the internet address group.
 **/
void
g_mime_group_address_set_members (GMimeGroupAddress *group, GMimeInternetAddressList *members)
{
	GMimeInternetAddressPrivate *ia_priv;
	GMimeGroupAddressPrivate *group_priv;

	g_return_if_fail (GMIME_IS_GROUP_ADDRESS (group));
	g_return_if_fail (GMIME_IS_INTERNET_ADDRESS_LIST (members));

	group_priv = _g_mime_group_address_get_private (group);

	if (group_priv->members == members)
		return;

	g_object_ref (members);
	if (group_priv->members) {
		g_mime_event_remove (group_priv->members->changed, (GMimeEventCallback) members_changed, group);
		g_object_unref (group_priv->members);
	}

	if (members) {
		g_mime_event_add (members->changed, (GMimeEventCallback) members_changed, group);
	}

	group_priv->members = members;

	ia_priv = _g_mime_internet_address_get_private (((GMimeInternetAddress *) group));

	g_mime_event_emit (ia_priv->changed, NULL);
}


/**
 * g_mime_group_address_get_members:
 * @group: a #GMimeGroupAddress
 *
 * Gets the #GMimeInternetAddressList containing the group members of an
 * rfc822 group address.
 *
 * Returns: (transfer none): a #GMimeInternetAddressList containing the
 * members of @group.
 **/
GMimeInternetAddressList *
g_mime_group_address_get_members (GMimeGroupAddress *group)
{
	GMimeGroupAddressPrivate *priv;

	g_return_val_if_fail (GMIME_IS_GROUP_ADDRESS (group), NULL);

	priv = _g_mime_group_address_get_private (group);

	return priv->members;
}


/**
 * g_mime_group_address_add_member:
 * @group: a #GMimeGroupAddress
 * @member: a #GMimeInternetAddress
 *
 * Add a contact to the internet address group.
 *
 * Returns: the index of the newly added member.
 **/
int
g_mime_group_address_add_member (GMimeGroupAddress *group, GMimeInternetAddress *member)
{
	GMimeGroupAddressPrivate *priv;

	g_return_val_if_fail (GMIME_IS_GROUP_ADDRESS (group), -1);
	g_return_val_if_fail (GMIME_IS_INTERNET_ADDRESS (member), -1);

	priv = _g_mime_group_address_get_private (group);

	return g_mime_internet_address_list_add (priv->members, member);
}


static void
group_to_string (GMimeInternetAddress *ia, GMimeFormatOptions *options, guint32 flags, size_t *linelen, GString *str)
{
	GMimeInternetAddressPrivate *ia_priv = _g_mime_internet_address_get_private (ia);
	GMimeGroupAddress *group = (GMimeGroupAddress *) ia;
	GMimeGroupAddressPrivate *group_priv = _g_mime_group_address_get_private (group);
	gboolean encode = flags & GMIME_INTERNET_ADDRESS_ENCODE;
	char *name = NULL;

	if (encode && (!ia_priv->name || !*ia_priv->name)) {
		/* RFC 5322 requires a display name. Quoting an empty one also keeps nested
		 * unnamed groups parseable instead of looking like extraneous colons. */
		g_string_append_len (str, "\"\"", 2);
		*linelen += 2;
	} else if (ia_priv->name != NULL) {
		name = _g_mime_internet_address_encode_name (options, ia_priv->name, encode, ia_priv->charset);
		_g_mime_internet_address_append_name (str, linelen, name, options, flags);
	}

	g_string_append_len (str, ": ", 2);
	*linelen += 2;
	g_free (name);

	_g_mime_internet_address_list_to_string (group_priv->members, options, flags & ~GMIME_INTERNET_ADDRESS_FIRST,
						linelen, str);
	g_string_append_c (str, ';');
	*linelen += 1;
}
