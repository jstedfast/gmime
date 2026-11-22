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

#ifndef __GMIME_AUTOCRYPT_PEER_STATE_H__
#define __GMIME_AUTOCRYPT_PEER_STATE_H__

#include <gmime/gmime-message.h>

G_BEGIN_DECLS

#define GMIME_TYPE_AUTOCRYPT_PEER_STATE (g_mime_autocrypt_peer_state_get_type ())

/**
 * GMimeAutocryptPeerState:
 *
 * An immutable snapshot of the direct and gossiped advertisements for one
 * mailbox. Keys are opaque bytes, not authenticated identities. No operation
 * imports keys, changes trust, or performs I/O.
 */
G_DECLARE_FINAL_TYPE (GMimeAutocryptPeerState, g_mime_autocrypt_peer_state, GMIME, AUTOCRYPT_PEER_STATE, GObject)

GMimeAutocryptPeerState *g_mime_autocrypt_peer_state_new (const char *address);
GMimeAutocryptPeerState *g_mime_autocrypt_peer_state_new_from_fields (const char *address, GDateTime *last_seen,
								     GDateTime *autocrypt_timestamp, GBytes *public_key,
								     GMimeAutocryptPreferEncrypt prefer_encrypt,
								     GDateTime *gossip_timestamp, GBytes *gossip_key);

const char *g_mime_autocrypt_peer_state_get_address (GMimeAutocryptPeerState *state);
GDateTime *g_mime_autocrypt_peer_state_get_last_seen (GMimeAutocryptPeerState *state);
GDateTime *g_mime_autocrypt_peer_state_get_autocrypt_timestamp (GMimeAutocryptPeerState *state);
GBytes *g_mime_autocrypt_peer_state_get_public_key (GMimeAutocryptPeerState *state);
GMimeAutocryptPreferEncrypt g_mime_autocrypt_peer_state_get_prefer_encrypt (GMimeAutocryptPeerState *state);
GDateTime *g_mime_autocrypt_peer_state_get_gossip_timestamp (GMimeAutocryptPeerState *state);
GBytes *g_mime_autocrypt_peer_state_get_gossip_key (GMimeAutocryptPeerState *state);

GMimeAutocryptPeerState *g_mime_autocrypt_peer_state_apply_observation (GMimeAutocryptPeerState *state,
								       GMimeAutocryptObservation *observation,
								       gboolean key_usable);
GMimeAutocryptPeerState *g_mime_autocrypt_peer_state_apply_gossip (GMimeAutocryptPeerState *state,
								  GMimeAutocryptHeader *header);

G_END_DECLS

#endif /* __GMIME_AUTOCRYPT_PEER_STATE_H__ */
