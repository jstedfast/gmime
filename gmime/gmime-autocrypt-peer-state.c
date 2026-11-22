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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "gmime-autocrypt-peer-state.h"
#include "gmime-internal.h"

struct _GMimeAutocryptPeerState {
	GObject parent_object;
	char *address;
	GDateTime *last_seen;
	GDateTime *autocrypt_timestamp;
	GBytes *public_key;
	GMimeAutocryptPreferEncrypt prefer_encrypt;
	GDateTime *gossip_timestamp;
	GBytes *gossip_key;
};

G_DEFINE_TYPE (GMimeAutocryptPeerState, g_mime_autocrypt_peer_state, G_TYPE_OBJECT)

static void
g_mime_autocrypt_peer_state_finalize (GObject *object)
{
	GMimeAutocryptPeerState *state = GMIME_AUTOCRYPT_PEER_STATE (object);

	g_free (state->address);
	g_clear_pointer (&state->last_seen, g_date_time_unref);
	g_clear_pointer (&state->autocrypt_timestamp, g_date_time_unref);
	g_clear_pointer (&state->public_key, g_bytes_unref);
	g_clear_pointer (&state->gossip_timestamp, g_date_time_unref);
	g_clear_pointer (&state->gossip_key, g_bytes_unref);

	G_OBJECT_CLASS (g_mime_autocrypt_peer_state_parent_class)->finalize (object);
}

static void
g_mime_autocrypt_peer_state_class_init (GMimeAutocryptPeerStateClass *klass)
{
	G_OBJECT_CLASS (klass)->finalize = g_mime_autocrypt_peer_state_finalize;
}

static void
g_mime_autocrypt_peer_state_init (GMimeAutocryptPeerState *state)
{
}

static GMimeAutocryptPeerState *
snapshot_new (char *owned_address, GDateTime *last_seen, GDateTime *direct_date, GBytes *direct_key,
	      GMimeAutocryptPreferEncrypt preference, GDateTime *gossip_date, GBytes *gossip_key)
{
	GMimeAutocryptPeerState *state = g_object_new (GMIME_TYPE_AUTOCRYPT_PEER_STATE, NULL);

	state->address = owned_address;
	state->last_seen = last_seen ? g_date_time_ref (last_seen) : NULL;
	state->autocrypt_timestamp = direct_date ? g_date_time_ref (direct_date) : NULL;
	state->public_key = direct_key ? g_bytes_ref (direct_key) : NULL;
	state->prefer_encrypt = preference;
	state->gossip_timestamp = gossip_date ? g_date_time_ref (gossip_date) : NULL;
	state->gossip_key = gossip_key ? g_bytes_ref (gossip_key) : NULL;

	return state;
}

/**
 * g_mime_autocrypt_peer_state_new_from_fields:
 * @address: a mailbox addr-spec
 * @last_seen: (nullable): most recent direct message
 * @autocrypt_timestamp: (nullable): date of accepted direct key
 * @public_key: (nullable): direct key bytes
 * @prefer_encrypt: preference from the direct advertisement, or NONE if absent
 * @gossip_timestamp: (nullable): date of accepted gossip key
 * @gossip_key: (nullable): gossip key bytes
 *
 * Restores the fields exposed by the snapshot getters. Key and timestamp
 * must be present together for each source; last_seen must be at least as
 * recent as the direct key, but is independent of gossip.
 *
 * Returns: (transfer full) (nullable): a snapshot, or %NULL for invalid fields.
 */
GMimeAutocryptPeerState *
g_mime_autocrypt_peer_state_new_from_fields (const char *address, GDateTime *last_seen,
					    GDateTime *autocrypt_timestamp, GBytes *public_key,
					    GMimeAutocryptPreferEncrypt prefer_encrypt,
					    GDateTime *gossip_timestamp, GBytes *gossip_key)
{
	GMimeInternetAddress *mailbox;
	char *canonical;

	if (!address || (public_key == NULL) != (autocrypt_timestamp == NULL) ||
	    (gossip_key == NULL) != (gossip_timestamp == NULL) ||
	    (autocrypt_timestamp && (!last_seen || g_date_time_compare (last_seen, autocrypt_timestamp) < 0)) ||
	    (public_key && !g_bytes_get_size (public_key)) || (gossip_key && !g_bytes_get_size (gossip_key)) ||
	    (prefer_encrypt != GMIME_AUTOCRYPT_PREFER_ENCRYPT_NONE &&
	     (prefer_encrypt != GMIME_AUTOCRYPT_PREFER_ENCRYPT_MUTUAL || !public_key)))
		return NULL;

	if (!(mailbox = g_mime_mailbox_address_new (NULL, address, NULL)))
		return NULL;

	canonical = _g_mime_autocrypt_canonicalize_mailbox (GMIME_MAILBOX_ADDRESS (mailbox));
	g_object_unref (mailbox);

	if (!canonical)
		return NULL;

	return snapshot_new (canonical, last_seen, autocrypt_timestamp, public_key, prefer_encrypt,
			     gossip_timestamp, gossip_key);
}

/**
 * g_mime_autocrypt_peer_state_new:
 * @address: a mailbox addr-spec
 *
 * Returns: (transfer full) (nullable): an empty snapshot, or %NULL for an invalid address.
 */
GMimeAutocryptPeerState *
g_mime_autocrypt_peer_state_new (const char *address)
{
	return g_mime_autocrypt_peer_state_new_from_fields (address, NULL, NULL, NULL,
							   GMIME_AUTOCRYPT_PREFER_ENCRYPT_NONE, NULL, NULL);
}

/**
 * g_mime_autocrypt_peer_state_get_address:
 * @state: a peer snapshot
 *
 * Returns: (transfer none): the canonical lowercase, IDN-encoded address.
 */
const char *
g_mime_autocrypt_peer_state_get_address (GMimeAutocryptPeerState *state)
{
	g_return_val_if_fail (GMIME_IS_AUTOCRYPT_PEER_STATE (state), NULL);

	return state->address;
}

/**
 * g_mime_autocrypt_peer_state_get_last_seen:
 * @state: a peer snapshot
 *
 * Returns: (transfer none) (nullable): most recent eligible direct message date.
 */
GDateTime *
g_mime_autocrypt_peer_state_get_last_seen (GMimeAutocryptPeerState *state)
{
	g_return_val_if_fail (GMIME_IS_AUTOCRYPT_PEER_STATE (state), NULL);

	return state->last_seen;
}

/**
 * g_mime_autocrypt_peer_state_get_autocrypt_timestamp:
 * @state: a peer snapshot
 *
 * Returns: (transfer none) (nullable): date of the current direct key.
 */
GDateTime *
g_mime_autocrypt_peer_state_get_autocrypt_timestamp (GMimeAutocryptPeerState *state)
{
	g_return_val_if_fail (GMIME_IS_AUTOCRYPT_PEER_STATE (state), NULL);

	return state->autocrypt_timestamp;
}

/**
 * g_mime_autocrypt_peer_state_get_public_key:
 * @state: a peer snapshot
 *
 * Returns: (transfer none) (nullable): the directly advertised key.
 */
GBytes *
g_mime_autocrypt_peer_state_get_public_key (GMimeAutocryptPeerState *state)
{
	g_return_val_if_fail (GMIME_IS_AUTOCRYPT_PEER_STATE (state), NULL);

	return state->public_key;
}

/**
 * g_mime_autocrypt_peer_state_get_prefer_encrypt:
 * @state: a peer snapshot
 *
 * Returns: the directly advertised encryption preference.
 */
GMimeAutocryptPreferEncrypt
g_mime_autocrypt_peer_state_get_prefer_encrypt (GMimeAutocryptPeerState *state)
{
	g_return_val_if_fail (GMIME_IS_AUTOCRYPT_PEER_STATE (state), GMIME_AUTOCRYPT_PREFER_ENCRYPT_NONE);

	return state->prefer_encrypt;
}

/**
 * g_mime_autocrypt_peer_state_get_gossip_timestamp:
 * @state: a peer snapshot
 *
 * Returns: (transfer none) (nullable): date of the current gossip key.
 */
GDateTime *
g_mime_autocrypt_peer_state_get_gossip_timestamp (GMimeAutocryptPeerState *state)
{
	g_return_val_if_fail (GMIME_IS_AUTOCRYPT_PEER_STATE (state), NULL);

	return state->gossip_timestamp;
}

/**
 * g_mime_autocrypt_peer_state_get_gossip_key:
 * @state: a peer snapshot
 *
 * Returns: (transfer none) (nullable): the key obtained through gossip.
 */
GBytes *
g_mime_autocrypt_peer_state_get_gossip_key (GMimeAutocryptPeerState *state)
{
	g_return_val_if_fail (GMIME_IS_AUTOCRYPT_PEER_STATE (state), NULL);

	return state->gossip_key;
}

/**
 * g_mime_autocrypt_peer_state_apply_observation:
 * @state: a peer snapshot
 * @observation: a direct message observation
 * @key_usable: whether the caller accepts the advertised key bytes
 *
 * Ignores ineligible messages, other addresses, and dates strictly older
 * than the direct key timestamp (not last_seen). At equal timestamps the
 * last usable advertisement applied wins, as in Level 1. Seen-only messages
 * and unusable keys can advance last_seen but never replace a key or preference.
 *
 * Returns: (transfer full): the resulting snapshot, possibly a new reference to @state.
 */
GMimeAutocryptPeerState *
g_mime_autocrypt_peer_state_apply_observation (GMimeAutocryptPeerState *state,
					      GMimeAutocryptObservation *observation, gboolean key_usable)
{
	GMimeAutocryptObservationStatus status;
	GDateTime *date, *last_seen;
	GBytes *key = NULL;

	g_return_val_if_fail (GMIME_IS_AUTOCRYPT_PEER_STATE (state), NULL);
	g_return_val_if_fail (GMIME_IS_AUTOCRYPT_OBSERVATION (observation), NULL);

	status = g_mime_autocrypt_observation_get_status (observation);

	if (status == GMIME_AUTOCRYPT_OBSERVATION_IGNORED ||
	    g_strcmp0 (state->address, g_mime_autocrypt_observation_get_address (observation)))
		return g_object_ref (state);

	date = g_mime_autocrypt_observation_get_effective_date (observation);

	if (!date || (state->autocrypt_timestamp && g_date_time_compare (date, state->autocrypt_timestamp) < 0))
		return g_object_ref (state);

	last_seen = state->last_seen;

	if (!last_seen || g_date_time_compare (date, last_seen) > 0)
		last_seen = date;

	if (status == GMIME_AUTOCRYPT_OBSERVATION_ADVERTISED && key_usable)
		key = g_mime_autocrypt_observation_get_keydata (observation);

	if (!key && last_seen == state->last_seen)
		return g_object_ref (state);

	return snapshot_new (g_strdup (state->address), last_seen, key ? date : state->autocrypt_timestamp,
			     key ? key : state->public_key,
			     key ? g_mime_autocrypt_observation_get_prefer_encrypt (observation) : state->prefer_encrypt,
			     state->gossip_timestamp, state->gossip_key);
}

/**
 * g_mime_autocrypt_peer_state_apply_gossip:
 * @state: a peer snapshot
 * @header: a recipient-matched gossip header from a decrypted inner part
 *
 * Call only after checking the gossip source and key usability. Incomplete
 * headers, other addresses and timestamps no newer than the gossip timestamp
 * are ignored (first gossip at an equal timestamp wins). Gossip never updates
 * last_seen, the direct key, or the peer's preference.
 *
 * Returns: (transfer full): the resulting snapshot, possibly a new reference to @state.
 */
GMimeAutocryptPeerState *
g_mime_autocrypt_peer_state_apply_gossip (GMimeAutocryptPeerState *state, GMimeAutocryptHeader *header)
{
	GMimeMailboxAddress *mailbox;
	gboolean matches;
	GDateTime *date;
	char *canonical;
	GBytes *key;

	g_return_val_if_fail (GMIME_IS_AUTOCRYPT_PEER_STATE (state), NULL);
	g_return_val_if_fail (GMIME_IS_AUTOCRYPT_HEADER (header), NULL);

	date = g_mime_autocrypt_header_get_effective_date (header);
	key = g_mime_autocrypt_header_get_keydata (header);
	mailbox = g_mime_autocrypt_header_get_address (header);

	if (!date || !key || !g_bytes_get_size (key) || !mailbox ||
	    (state->gossip_timestamp && g_date_time_compare (date, state->gossip_timestamp) <= 0))
		return g_object_ref (state);

	canonical = _g_mime_autocrypt_canonicalize_mailbox (mailbox);
	matches = !g_strcmp0 (state->address, canonical);
	g_free (canonical);

	if (!matches)
		return g_object_ref (state);

	return snapshot_new (g_strdup (state->address), state->last_seen, state->autocrypt_timestamp,
			     state->public_key, state->prefer_encrypt, date, key);
}
