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

#include "gmime/gmime-dkim-backend-private.h"
#include <string.h>

#if defined (HAVE_DKIM_GNUTLS) || defined (HAVE_DKIM_OPENSSL)
/* Public test vectors only: RFC 8463, Appendix A. Both schemes are
 * deterministic, so compare signatures to the published values, not just
 * to what the same provider accepts. The input hashes are SHA-256 of the
 * relaxed header inputs from A.3 (with the respective b= value empty). */
static const struct {
	const char *pem, *public_key, *hash, *signature;
} vectors[] = {
	{
		"-----BEGIN RSA PRIVATE KEY-----\n"
		"MIICXQIBAAKBgQDkHlOQoBTzWRiGs5V6NpP3idY6Wk08a5qhdR6wy5bdOKb2jLQi\n"
		"Y/J16JYi0Qvx/byYzCNb3W91y3FutACDfzwQ/BC/e/8uBsCR+yz1Lxj+PL6lHvqM\n"
		"KrM3rG4hstT5QjvHO9PzoxZyVYLzBfO2EeC3Ip3G+2kryOTIKT+l/K4w3QIDAQAB\n"
		"AoGAH0cxOhFZDgzXWhDhnAJDw5s4roOXN4OhjiXa8W7Y3rhX3FJqmJSPuC8N9vQm\n"
		"6SVbaLAE4SG5mLMueHlh4KXffEpuLEiNp9Ss3O4YfLiQpbRqE7Tm5SxKjvvQoZZe\n"
		"zHorimOaChRL2it47iuWxzxSiRMv4c+j70GiWdxXnxe4UoECQQDzJB/0U58W7RZy\n"
		"6enGVj2kWF732CoWFZWzi1FicudrBFoy63QwcowpoCazKtvZGMNlPWnC7x/6o8Gc\n"
		"uSe0ga2xAkEA8C7PipPm1/1fTRQvj1o/dDmZp243044ZNyxjg+/OPN0oWCbXIGxy\n"
		"WvmZbXriOWoSALJTjExEgraHEgnXssuk7QJBALl5ICsYMu6hMxO73gnfNayNgPxd\n"
		"WFV6Z7ULnKyV7HSVYF0hgYOHjeYe9gaMtiJYoo0zGN+L3AAtNP9huqkWlzECQE1a\n"
		"licIeVlo1e+qJ6Mgqr0Q7Aa7falZ448ccbSFYEPD6oFxiOl9Y9se9iYHZKKfIcst\n"
		"o7DUw1/hz2Ck4N5JrgUCQQCyKveNvjzkkd8HjYs0SwM0fPjK16//5qDZ2UiDGnOe\n"
		"uEzxBDAr518Z8VFbR41in3W4Y3yCDgQlLlcETrS+zYcL\n"
		"-----END RSA PRIVATE KEY-----\n",
		"MIGfMA0GCSqGSIb3DQEBAQUAA4GNADCBiQKBgQDkHlOQoBTzWR"
		"iGs5V6NpP3idY6Wk08a5qhdR6wy5bdOKb2jLQiY/J16JYi0Qvx/byYzCNb3W91y3FutAC"
		"DfzwQ/BC/e/8uBsCR+yz1Lxj+PL6lHvqMKrM3rG4hstT5QjvHO9PzoxZyVYLzBfO2EeC3"
		"Ip3G+2kryOTIKT+l/K4w3QIDAQAB",
		"/tcebXD7Eju1w1OtxCghswS2udVPyGfzBPfGHaqqERE=",
		"F45dVWDfMbQDGHJFlXUNB2HKfbCeLRyhDXgFpEL8GwpsRe0IeIixNTe3"
		"DhCVlUrSjV4BwcVcOF6+FF3Zo9Rpo1tFOeS9mPYQTnGdaSGsgeefOsk2Jz"
		"dA+L10TeYt9BgDfQNZtKdN1WO//KgIqXP7OdEFE4LjFYNcUxZQ4FADY+8="
	}, {
		"-----BEGIN PRIVATE KEY-----\n"
		"MC4CAQAwBQYDK2VwBCIEIJ1hsZ3v/VpguoRK9JLsLMREScVpezJpGXA7rAMcrn9g\n"
		"-----END PRIVATE KEY-----\n",
		"11qYAYKxCrfVS/7TyWQHOg7hcvPapiMlrwIaaPcHURo=",
		"SM6aLHEOzhcQ/xVplrg2p/RUcOQ+/lZDB01uFpDtYuc=",
		"/gCrinpcQOoIfuHNQIbq4pgh9kyIK3AQUdt9OdqQehSwhEIug4D11Bus"
		"Fa3bT3FY5OsU7ZbnKELq+eXdp1Q1Dw=="
	}
};

static GBytes *
from_base64 (const char *base64)
{
	gsize len;
	guint8 *data = g_base64_decode (base64, &len);
	return g_bytes_new_take (data, len);
}

static void
test_vector (gconstpointer arg)
{
	gboolean ed = GPOINTER_TO_INT (arg);
	GBytes *pem = g_bytes_new_static (vectors[ed].pem, strlen (vectors[ed].pem));
	GBytes *key = from_base64 (vectors[ed].public_key);
	GBytes *hash = from_base64 (vectors[ed].hash);
	GBytes *sig = from_base64 (vectors[ed].signature);
	const guint8 *digest = g_bytes_get_data (hash, NULL);
	guint8 other[32] = { 0 };
	GError *error = NULL;
	GBytes *actual;

	actual = _g_mime_dkim_sign_digest (ed, pem, digest, &error);
	g_assert_no_error (error);
	g_assert_nonnull (actual);
	g_assert_true (g_bytes_equal (actual, sig));
	g_bytes_unref (actual);
	g_assert_true (_g_mime_dkim_verify_digest (ed, key, digest, sig, &error));
	g_assert_no_error (error);
	g_assert_false (_g_mime_dkim_verify_digest (ed, key, other, sig, &error));
	g_assert_no_error (error);
	g_assert_null (_g_mime_dkim_sign_digest (!ed, pem, digest, &error));
	g_assert_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA);
	g_clear_error (&error);
	g_assert_false (_g_mime_dkim_verify_digest (!ed, key, digest, sig, &error));
	g_assert_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA);
	g_clear_error (&error);
	if (!ed) {
		/* The SPKI BIT STRING at offset 22 contains the PKCS#1 form. */
		GBytes *bare = g_bytes_new_from_bytes (key, 22, g_bytes_get_size (key) - 22);
		g_assert_true (_g_mime_dkim_verify_digest (FALSE, bare, digest, sig, &error));
		g_assert_no_error (error);
		g_bytes_unref (bare);
	}
	g_bytes_unref (sig);
	g_bytes_unref (hash);
	g_bytes_unref (key);
	g_bytes_unref (pem);
}

static void
test_bad_keys (void)
{
	guint8 hash[32] = { 0 };
	/* A 1023-bit RSA modulus still occupies 128 octets; checking only
	 * the signature length would miss this RFC 8301 boundary case. */
	guint8 weak[139] = { 0x30, 0x81, 0x88, 0x02, 0x81, 0x80, 0x77 };
	const char *encrypted = "-----BEGIN ENCRYPTED PRIVATE KEY-----\nAAAA\n-----END ENCRYPTED PRIVATE KEY-----\n";
	GError *error = NULL;
	GBytes *key, *pem, *sig = from_base64 (vectors[0].signature);

	memset (weak + 7, 0x77, 127);
	memcpy (weak + 134, "\x02\x03\x01\x00\x01", 5);
	key = g_bytes_new_static (weak, sizeof weak);
	g_assert_false (_g_mime_dkim_verify_digest (FALSE, key, hash, sig, &error));
	g_assert_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA);
	g_clear_error (&error);
	pem = g_bytes_new_static (encrypted, strlen (encrypted));
	g_assert_null (_g_mime_dkim_sign_digest (TRUE, pem, hash, &error));
	g_assert_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA);
	g_clear_error (&error);
	g_bytes_unref (pem);
	g_bytes_unref (key);
	g_bytes_unref (sig);
}

#else
static void
test_disabled (void)
{
	guint8 digest[32] = { 0 };
	GBytes *empty = g_bytes_new_static ("", 0);
	GError *error = NULL;

	g_assert_null (_g_mime_dkim_sign_digest (FALSE, empty, digest, &error));
	g_assert_error (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED);
	g_clear_error (&error);
	g_assert_false (_g_mime_dkim_verify_digest (TRUE, empty, digest, empty, &error));
	g_assert_error (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED);
	g_clear_error (&error);
	g_bytes_unref (empty);
}
#endif

int
main (int argc, char **argv)
{
	g_test_init (&argc, &argv, NULL);
#if defined (HAVE_DKIM_GNUTLS) || defined (HAVE_DKIM_OPENSSL)
	g_test_add_data_func ("/dkim/crypto/rsa", GINT_TO_POINTER (FALSE), test_vector);
	g_test_add_data_func ("/dkim/crypto/ed25519", GINT_TO_POINTER (TRUE), test_vector);
	g_test_add_func ("/dkim/crypto/bad-keys", test_bad_keys);
#else
	g_test_add_func ("/dkim/crypto/disabled", test_disabled);
#endif
	return g_test_run ();
}
