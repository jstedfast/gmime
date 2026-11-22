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

#include <string.h>

#include <gmime/gmime.h>

static GMimeDmarcIdentifierStatus
dkim_status (const GError *error)
{
	if (g_error_matches (error, G_RESOLVER_ERROR, G_RESOLVER_ERROR_NOT_FOUND) ||
	    g_error_matches (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT))
		return GMIME_DMARC_IDENTIFIER_PERM_ERROR;

	if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA))
		return GMIME_DMARC_IDENTIFIER_FAIL;

	return GMIME_DMARC_IDENTIFIER_TEMP_ERROR;
}

static gboolean
verify_file (const char *path, GResolver *resolver, GMimeDmarcVerifier *verifier)
{
	static const char *const names[] = { "NONE", "PASS", "FAIL", "TEMPERROR", "PERMERROR" };
	GMimeAuthenticationMethodResult *method;
	GMimeHeaderList *headers;
	GMimeDmarcResult *result;
	GMimeMessage *message;
	GError *error = NULL;
	GPtrArray *domains;
	char *formatted;
	GArray *ids;

	if (!(message = g_mime_message_load_from_file (NULL, path, &error))) {
		g_printerr ("%s: %s\n", path, error->message);
		g_error_free (error);
		return FALSE;
	}

	g_print ("%s:\n", path);

	headers = g_mime_entity_get_header_list (GMIME_ENTITY (message));
	ids = g_array_new (FALSE, FALSE, sizeof (GMimeDmarcIdentifier));
	domains = g_ptr_array_new_with_free_func (g_free);

	for (int i = 0; i < g_mime_header_list_get_count (headers); i++) {
		GMimeHeader *header = g_mime_header_list_get_header_at (headers, i);
		char *domain, *selector, *d, *s;
		GMimeDmarcIdentifier id;
		gboolean valid;

		if (g_mime_header_get_id (header) != GMIME_HEADER_DKIM_SIGNATURE)
			continue;

		if (ids->len == g_mime_dmarc_verifier_get_max_dkim_signatures (verifier)) {
			g_print ("  DKIM verification limit reached; remaining signatures skipped\n");
			break;
		}

		valid = g_mime_dkim_verify_full (message, ids->len, resolver, NULL, &domain, &selector, &error);
		id.status = valid ? GMIME_DMARC_IDENTIFIER_PASS : dkim_status (error);
		id.domain = domain;

		g_array_append_val (ids, id);
		g_ptr_array_add (domains, domain);

		d = g_strescape (domain ? domain : "?", NULL);
		s = g_strescape (selector ? selector : "?", NULL);

		g_print ("  DKIM d=%s; s=%s -> %s%s%s\n", d, s, names[id.status],
			 error ? ": " : "", error ? error->message : "");

		g_free (d);
		g_free (s);
		g_free (selector);
		g_clear_error (&error);
	}

	if (!ids->len)
		g_print ("  DKIM: NO SIGNATURE\n");

	/* A receiver can also pass its SPF check for the SMTP MAIL FROM domain.
	 * That transaction is unavailable in a message file; only aligned DKIM
	 * can pass DMARC here. Never substitute incoming Authentication-Results. */
	result = g_mime_dmarc_verifier_verify_with_results (verifier, message, (GMimeDmarcIdentifier *) ids->data,
							    ids->len, NULL, NULL, &error);
	if (result) {
		method = g_mime_dmarc_result_to_authentication_method_result (result);
		formatted = g_mime_authentication_method_result_to_string (method);

		g_print ("  %s\n", formatted);

		if (g_mime_dmarc_result_get_dkim_aligned (result))
			g_print ("  aligned d=%s\n", g_mime_dmarc_result_get_aligned_dkim_result (result)->domain);

		if (g_mime_dmarc_result_get_errors (result))
			g_print ("  DMARC diagnostic flags: 0x%x\n", (guint) g_mime_dmarc_result_get_errors (result));

		g_free (formatted);
		g_object_unref (method);
	} else {
		g_printerr ("%s: %s\n", path, error->message);
		g_error_free (error);
	}

	g_array_unref (ids);
	g_ptr_array_unref (domains);
	g_object_unref (message);

	if (!result)
		return FALSE;

	g_mime_dmarc_result_free (result);

	return TRUE;
}

int
main (int argc, char **argv)
{
	GMimeDmarcVerifier *verifier;
	GResolver *resolver;
	int status = 0;

	for (int i = 1; i < argc; i++) {
		if (!strcmp (argv[i], "--help") || !strcmp (argv[i], "-h")) {
			g_print ("Usage: dkim-verifier FILE.eml [FILE.eml ...]\n");
			return 0;
		}
	}

	if (argc == 1) {
		g_printerr ("Usage: dkim-verifier FILE.eml [FILE.eml ...]\n");
		return 1;
	}

	g_mime_init ();

	resolver = g_resolver_get_default ();
	verifier = g_mime_dmarc_verifier_new (resolver);

	for (int i = 1; i < argc; i++) {
		if (!verify_file (argv[i], resolver, verifier))
			status = 1;
	}

	g_object_unref (verifier);
	g_object_unref (resolver);

	g_mime_shutdown ();

	return status;
}
