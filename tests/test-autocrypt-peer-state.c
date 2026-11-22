/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) Jeffrey Stedfast
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include <gmime/gmime.h>
#include <gmime/gmime-autocrypt-peer-state.h>

#define PEER "AlIce@example.org"
#define DIRECT_A "addr=ALICE@example.org; prefer-encrypt=mutual; keydata=YQ=="
#define DIRECT_B "addr=alice@example.org; keydata=Yg=="

static GMimeAutocryptPeerState *
apply_message (GMimeAutocryptPeerState *state, const char *from, gint64 timestamp, const char *header,
	       gboolean usable)
{
	g_autoptr (GMimeMessage) message = g_mime_message_new (FALSE);
	g_autoptr (GDateTime) date = g_date_time_new_from_unix_utc (timestamp);
	g_autoptr (GMimeAutocryptObservation) observation = NULL;

	if (from)
		g_assert_true (g_mime_message_add_mailbox (message, GMIME_ADDRESS_TYPE_FROM, NULL, from, NULL));
	if (header)
		g_mime_entity_set_header (GMIME_ENTITY (message), "Autocrypt", header, NULL);
	g_mime_message_set_date (message, date);
	observation = g_mime_message_observe_autocrypt (message, date);
	return g_mime_autocrypt_peer_state_apply_observation (state, observation, usable);
}

static void
assert_key (GBytes *key, char expected)
{
	g_assert_nonnull (key);
	g_assert_cmpmem (g_bytes_get_data (key, NULL), g_bytes_get_size (key), &expected, 1);
}

static void
assert_direct (GMimeAutocryptPeerState *state, gint64 seen, gint64 direct_date,
	       char key, GMimeAutocryptPreferEncrypt preference)
{
	g_assert_cmpint (g_date_time_to_unix (g_mime_autocrypt_peer_state_get_last_seen (state)), ==, seen);
	g_assert_cmpint (g_date_time_to_unix (g_mime_autocrypt_peer_state_get_autocrypt_timestamp (state)), ==, direct_date);
	assert_key (g_mime_autocrypt_peer_state_get_public_key (state), key);
	g_assert_cmpint (g_mime_autocrypt_peer_state_get_prefer_encrypt (state), ==, preference);
}

static void
test_direct (void)
{
	g_autoptr (GMimeAutocryptPeerState) empty = g_mime_autocrypt_peer_state_new (PEER);
	g_autoptr (GMimeAutocryptPeerState) other = apply_message (empty, "bob@example.org", 50, NULL, TRUE);
	g_autoptr (GMimeAutocryptPeerState) ignored = apply_message (empty, NULL, 60, NULL, TRUE);
	g_autoptr (GMimeAutocryptPeerState) seen = apply_message (empty, PEER, 30, NULL, TRUE);
	g_autoptr (GMimeAutocryptPeerState) direct = apply_message (seen, PEER, 20, DIRECT_A, TRUE);
	g_autoptr (GMimeAutocryptPeerState) older = apply_message (direct, PEER, 19, DIRECT_B, TRUE);
	g_autoptr (GMimeAutocryptPeerState) tied = apply_message (older, PEER, 20, DIRECT_B, TRUE);
	g_autoptr (GMimeAutocryptPeerState) rejected = apply_message (tied, PEER, 40, DIRECT_A, FALSE);

	g_assert_cmpstr (g_mime_autocrypt_peer_state_get_address (empty), ==, "alice@example.org");
	g_assert_null (g_mime_autocrypt_peer_state_get_last_seen (empty));
	g_assert_null (g_mime_autocrypt_peer_state_get_last_seen (other));
	g_assert_null (g_mime_autocrypt_peer_state_get_last_seen (ignored));
	g_assert_null (g_mime_autocrypt_peer_state_get_public_key (seen));
	assert_direct (direct, 30, 20, 'a', GMIME_AUTOCRYPT_PREFER_ENCRYPT_MUTUAL);
	assert_direct (older, 30, 20, 'a', GMIME_AUTOCRYPT_PREFER_ENCRYPT_MUTUAL);
	assert_direct (tied, 30, 20, 'b', GMIME_AUTOCRYPT_PREFER_ENCRYPT_NONE);
	assert_direct (rejected, 40, 20, 'b', GMIME_AUTOCRYPT_PREFER_ENCRYPT_NONE);
}

static GMimeAutocryptPeerState *
apply_gossip (GMimeAutocryptPeerState *state, const char *address, gint64 timestamp, char data)
{
	g_autoptr (GMimeAutocryptHeader) header = g_mime_autocrypt_header_new ();
	g_autoptr (GDateTime) date = g_date_time_new_from_unix_utc (timestamp);
	g_autoptr (GBytes) key = g_bytes_new (&data, 1);

	g_assert_true (g_mime_autocrypt_header_set_address_from_string (header, address, NULL));
	g_mime_autocrypt_header_set_effective_date (header, date);
	g_mime_autocrypt_header_set_keydata (header, key);
	return g_mime_autocrypt_peer_state_apply_gossip (state, header);
}

static void
test_gossip_and_restore (void)
{
	g_autoptr (GMimeAutocryptPeerState) empty = g_mime_autocrypt_peer_state_new ("alice@example.org");
	g_autoptr (GMimeAutocryptPeerState) gossip = apply_gossip (empty, PEER, 90, 'g');
	g_autoptr (GMimeAutocryptPeerState) direct = apply_message (gossip, PEER, 10, DIRECT_A, TRUE);
	g_autoptr (GMimeAutocryptPeerState) restored = g_mime_autocrypt_peer_state_new_from_fields (
		g_mime_autocrypt_peer_state_get_address (direct), g_mime_autocrypt_peer_state_get_last_seen (direct),
		g_mime_autocrypt_peer_state_get_autocrypt_timestamp (direct), g_mime_autocrypt_peer_state_get_public_key (direct),
		g_mime_autocrypt_peer_state_get_prefer_encrypt (direct), g_mime_autocrypt_peer_state_get_gossip_timestamp (direct),
		g_mime_autocrypt_peer_state_get_gossip_key (direct));
	g_autoptr (GMimeAutocryptPeerState) older = NULL, tied = NULL, other = NULL, newer = NULL;

	g_assert_null (g_mime_autocrypt_peer_state_get_gossip_key (empty));
	g_assert_null (g_mime_autocrypt_peer_state_get_last_seen (gossip));
	g_clear_object (&direct);
	g_clear_object (&gossip);
	g_assert_nonnull (restored);
	older = apply_gossip (restored, PEER, 89, 'o');
	tied = apply_gossip (older, PEER, 90, 't');
	other = apply_gossip (tied, "bob@example.org", 100, 'x');
	newer = apply_gossip (other, PEER, 91, 'n');
	assert_key (g_mime_autocrypt_peer_state_get_gossip_key (older), 'g');
	assert_key (g_mime_autocrypt_peer_state_get_gossip_key (tied), 'g');
	assert_key (g_mime_autocrypt_peer_state_get_gossip_key (other), 'g');
	assert_key (g_mime_autocrypt_peer_state_get_gossip_key (newer), 'n');
	g_assert_cmpint (g_date_time_to_unix (g_mime_autocrypt_peer_state_get_gossip_timestamp (restored)), ==, 90);
	g_assert_cmpint (g_date_time_to_unix (g_mime_autocrypt_peer_state_get_gossip_timestamp (newer)), ==, 91);
	assert_direct (newer, 10, 10, 'a', GMIME_AUTOCRYPT_PREFER_ENCRYPT_MUTUAL);
}

int
main (int argc, char **argv)
{
	int result;

	g_test_init (&argc, &argv, NULL);
	g_mime_init ();
	g_test_add_func ("/autocrypt/peer-state/direct", test_direct);
	g_test_add_func ("/autocrypt/peer-state/gossip-and-restore", test_gossip_and_restore);
	result = g_test_run ();
	g_mime_shutdown ();
	return result;
}
