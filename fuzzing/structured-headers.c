/* Adapted in part from MimeKit.Fuzzing.Targets.AddressParserTarget and HeaderValuesTarget.
 * Copyright (c) 2013-2026 .NET Foundation and Contributors.
 * Copyright (C) Jeffrey Stedfast.
 * Distributed under the MIT license; see LICENSE. */

#include "fuzz.h"
#include <gmime/gmime-internal.h>
#include <gmime/gmime-internet-address-private.h>
#include <gmime/gmime-parse-utils.h>
#include <string.h>

enum {
	ADDRESS_LIST, ADDRESS, MAILBOX, GROUP, DOMAINS, PARAMETERS, CONTENT_TYPE, DISPOSITION,
	AUTHENTICATION_RESULTS,
	RAW_ADDRESS_LIST, RAW_PARAMETERS, RAW_CONTENT_TYPE, RAW_DISPOSITION, PARSER_COUNT
};

static void
append_text (GString *snapshot, const char *text)
{
	if (text)
		g_string_append_printf (snapshot, "%" G_GSIZE_FORMAT ":%s\n", strlen (text), text);
	else
		g_string_append (snapshot, "-1:\n");
}

static void
record_warning (gint64 offset, GMimeParserWarning code, const char *item, gpointer user_data)
{
	GString *warnings = user_data;

	g_string_append_printf (warnings, "%" G_GINT64_FORMAT ":%d:", offset, code);
	append_text (warnings, item);
}

static GObject *
parse_model (guint parser, GMimeParserOptions *options, const char *text, gssize length, GError **error)
{
	switch (parser) {
	case ADDRESS_LIST:
		return (GObject *) g_mime_internet_address_list_parse_full (options, text, length, error);
	case ADDRESS:
		return (GObject *) g_mime_internet_address_parse (options, text, length, error);
	case MAILBOX:
		return (GObject *) g_mime_mailbox_address_parse (options, text, length, error);
	case GROUP:
		return (GObject *) g_mime_group_address_parse (options, text, length, error);
	case DOMAINS:
		return (GObject *) g_mime_domain_list_parse (text, length, error);
	case PARAMETERS:
		return (GObject *) g_mime_param_list_parse_full (options, text, length, error);
	case CONTENT_TYPE:
		return (GObject *) g_mime_content_type_parse_full (options, text, length, error);
	case DISPOSITION:
		return (GObject *) g_mime_content_disposition_parse_full (options, text, length, error);
	case AUTHENTICATION_RESULTS:
		return (GObject *) g_mime_authentication_results_parse (text, length, error);
	case RAW_ADDRESS_LIST:
		return (GObject *) _g_mime_internet_address_list_parse (options, text, length, 0);
	case RAW_PARAMETERS:
		return (GObject *) _g_mime_param_list_parse (options, text, length, 0);
	case RAW_CONTENT_TYPE:
		return (GObject *) _g_mime_content_type_parse (options, text, length, 0);
	case RAW_DISPOSITION:
		return (GObject *) _g_mime_content_disposition_parse (options, text, length, 0);
	default:
		g_assert_not_reached ();
	}
}

static void describe_address (GString *snapshot, GMimeInternetAddress *address);

static void
describe_addresses (GString *snapshot, GMimeInternetAddressList *list)
{
	int i;

	g_string_append_printf (snapshot, "%d\n", g_mime_internet_address_list_length (list));
	for (i = 0; i < g_mime_internet_address_list_length (list); i++)
		describe_address (snapshot, g_mime_internet_address_list_get_address (list, i));
}

static void
trim_domain_dot (char *text)
{
	char *end = text + strlen (text);

	while (end > text && end[-1] == '.')
		*--end = '\0';
}

static void
describe_address (GString *snapshot, GMimeInternetAddress *address)
{
	const char *name = g_mime_internet_address_get_name (address);
	GString *collapsed = g_string_new (NULL);
	gboolean space = FALSE;
	GMimeMailboxAddress *mailbox;
	GMimeDomainList *route;
	const char *domain;
	char *encoded;
	gunichar c;
	int i;

	for (; name && *name; name = g_utf8_next_char (name)) {
		c = g_utf8_get_char (name);
		if (g_unichar_isspace (c) || c == 0x85) {
			space = TRUE;
		} else {
			if (space && collapsed->len)
				g_string_append_c (collapsed, ' ');
			g_string_append_unichar (collapsed, c);
			space = FALSE;
		}
	}
	append_text (snapshot, collapsed->str);
	g_string_free (collapsed, TRUE);

	if (GMIME_IS_GROUP_ADDRESS (address)) {
		g_string_append (snapshot, "group\n");
		describe_addresses (snapshot, g_mime_group_address_get_members (GMIME_GROUP_ADDRESS (address)));
		return;
	}

	g_string_append (snapshot, "mailbox\n");
	mailbox = GMIME_MAILBOX_ADDRESS (address);
	route = g_mime_mailbox_address_get_route (mailbox);
	g_string_append_printf (snapshot, "%d\n", g_mime_domain_list_length (route));
	for (i = 0; i < g_mime_domain_list_length (route); i++) {
		domain = g_mime_domain_list_get_domain (route, i);
		encoded = _g_mime_internet_address_encode_domain (domain);
		if (g_hostname_is_non_ascii (domain))
			trim_domain_dot (encoded);
		append_text (snapshot, encoded);
		g_free (encoded);
	}
	encoded = g_mime_mailbox_address_get_address (mailbox, TRUE);
	trim_domain_dot (encoded);
	append_text (snapshot, encoded);
	g_free (encoded);
}

static void
check_address_round_trip (GMimeParserOptions *options, GMimeFormatOptions *format, GObject *model)
{
	GString *original = g_string_new (NULL), *actual = g_string_new (NULL);
	GMimeInternetAddressList *list;
	GError *error = NULL;
	char *first, *next;
	guint stalled = 0;

	if (GMIME_IS_INTERNET_ADDRESS_LIST (model)) {
		describe_addresses (original, GMIME_INTERNET_ADDRESS_LIST (model));
		first = g_mime_internet_address_list_to_string (GMIME_INTERNET_ADDRESS_LIST (model), format, TRUE);
	} else {
		g_string_append (original, "1\n");
		describe_address (original, GMIME_INTERNET_ADDRESS (model));
		first = g_mime_internet_address_to_string (GMIME_INTERNET_ADDRESS (model), format, TRUE);
	}

	while (TRUE) {
		list = g_mime_internet_address_list_parse_full (options, first, strlen (first), &error);
		FUZZ_CHECK (list && !error, "Address parser rejected its formatted output: %s", first);
		next = g_mime_internet_address_list_to_string (list, format, TRUE);
		if (!strcmp (first, next)) {
			g_object_unref (list);
			g_free (next);
			break;
		}

		/* Folding may collapse name whitespace; IDNA may normalize domains. */
		g_string_truncate (actual, 0);
		describe_addresses (actual, list);
		FUZZ_CHECK (!strcmp (original->str, actual->str), "Address round trip changed more than name whitespace/IDNA: %s", first);
		g_object_unref (list);
		if (strlen (next) >= strlen (first))
			FUZZ_CHECK (++stalled <= 8, "Address output did not converge after eight non-shrinking rounds");
		g_free (first);
		first = next;
	}
	g_free (first);
	g_string_free (original, TRUE);
	g_string_free (actual, TRUE);
}

static GMimeParamList *
model_parameters (GObject *model)
{
	if (GMIME_IS_CONTENT_TYPE (model))
		return g_mime_content_type_get_parameters (GMIME_CONTENT_TYPE (model));

	return g_mime_content_disposition_get_parameters (GMIME_CONTENT_DISPOSITION (model));
}

static char *
encode_content_field (GObject *model, GMimeFormatOptions *format)
{
	if (GMIME_IS_CONTENT_TYPE (model))
		return g_mime_content_type_encode (GMIME_CONTENT_TYPE (model), format);

	return g_mime_content_disposition_encode (GMIME_CONTENT_DISPOSITION (model), format);
}

static void
check_content_round_trip (guint parser, GMimeParserOptions *options, GMimeFormatOptions *format, GObject *model)
{
	GMimeParamList *original = model_parameters (model), *params;
	GObject *reparsed;
	GError *error = NULL;
	char *first = encode_content_field (model, format), *next;
	GMimeParam *a, *b;
	guint round;
	int i;

	for (round = 0; round < 2; round++) {
		reparsed = parse_model (parser, options, first, strlen (first), &error);
		FUZZ_CHECK (reparsed && !error, "Content-field parser rejected its formatted output: %s", first);
		next = encode_content_field (reparsed, format);
		if (!strcmp (first, next)) {
			g_object_unref (reparsed);
			g_free (next);
			break;
		}
		FUZZ_CHECK (!round, "Content-field formatting did not stabilize after charset normalization: %s", first);
		params = model_parameters (reparsed);
		FUZZ_CHECK (g_mime_param_list_length (original) == g_mime_param_list_length (params),
			    "Content-field round trip changed parameter count");
		for (i = 0; i < g_mime_param_list_length (original); i++) {
			a = g_mime_param_list_get_parameter_at (original, i);
			b = g_mime_param_list_get_parameter_at (params, i);
			FUZZ_CHECK (!strcmp (g_mime_param_get_name (a), g_mime_param_get_name (b)) &&
				    !strcmp (g_mime_param_get_value (a), g_mime_param_get_value (b)),
				    "Content-field round trip changed parameter name/value");
		}
		g_object_unref (reparsed);
		g_free (first);
		first = next;
	}
	g_free (first);
}

static char *
snapshot_model (GObject *model, GError *error, const char *warnings)
{
	GString *snapshot = g_string_new (NULL);
	GMimeParamList *params = NULL;
	GMimeParam *param;
	char *text = NULL;
	int i;

	if (model) {
		if (GMIME_IS_INTERNET_ADDRESS_LIST (model)) {
			text = g_mime_internet_address_list_to_string (GMIME_INTERNET_ADDRESS_LIST (model), NULL, FALSE);
		} else if (GMIME_IS_INTERNET_ADDRESS (model)) {
			text = g_mime_internet_address_to_string (GMIME_INTERNET_ADDRESS (model), NULL, FALSE);
		} else if (GMIME_IS_DOMAIN_LIST (model)) {
			text = g_mime_domain_list_to_string (GMIME_DOMAIN_LIST (model), NULL, FALSE);
		} else if (GMIME_IS_PARAM_LIST (model)) {
			params = GMIME_PARAM_LIST (model);
			text = g_mime_param_list_to_string (params);
		} else if (GMIME_IS_CONTENT_TYPE (model)) {
			params = g_mime_content_type_get_parameters (GMIME_CONTENT_TYPE (model));
			text = g_mime_content_type_to_string (GMIME_CONTENT_TYPE (model));
		} else if (GMIME_IS_CONTENT_DISPOSITION (model)) {
			params = g_mime_content_disposition_get_parameters (GMIME_CONTENT_DISPOSITION (model));
			text = g_mime_content_disposition_to_string (GMIME_CONTENT_DISPOSITION (model));
		} else if (GMIME_IS_AUTHENTICATION_RESULTS (model)) {
			text = g_mime_authentication_results_to_string (GMIME_AUTHENTICATION_RESULTS (model));
		} else {
			g_assert_not_reached ();
		}
	}
	g_string_append_printf (snapshot, "%d\n", model != NULL);
	append_text (snapshot, text);
	g_free (text);
	if (params) {
		g_string_append_printf (snapshot, "%d\n", g_mime_param_list_length (params));
		for (i = 0; i < g_mime_param_list_length (params); i++) {
			param = g_mime_param_list_get_parameter_at (params, i);
			append_text (snapshot, g_mime_param_get_name (param));
			append_text (snapshot, g_mime_param_get_value (param));
			append_text (snapshot, g_mime_param_get_charset (param));
			append_text (snapshot, g_mime_param_get_lang (param));
			g_string_append_printf (snapshot, "%d:%d\n", g_mime_param_get_encoding_method (param),
						g_mime_param_get_always_quote (param));
		}
	}
	if (error) {
		FUZZ_CHECK (!model && error->domain == GMIME_ERROR && error->code == GMIME_ERROR_PARSE_ERROR,
			    "Unexpected structured-header parse error: %s", error->message);
		append_text (snapshot, error->message);
	} else {
		append_text (snapshot, NULL);
	}
	append_text (snapshot, warnings);
	return g_string_free (snapshot, FALSE);
}

static gboolean
plausible_message_id (const char *id)
{
	const char *at = strchr (id, '@'), *p;

	if (!at || at == id || !at[1] || strchr (at + 1, '@'))
		return FALSE;
	for (p = id; *p; p++)
		if ((guchar) *p <= ' ' || *p == 127 || strchr ("\"<>()\\:", *p))
			return FALSE;
	return TRUE;
}

static char *
snapshot_identifiers (const char *text, gsize length)
{
	GString *snapshot = g_string_new (NULL);
	const char *in = text, *end = text + length, *previous;
	char *id;

	id = g_mime_decode_msgid (&in, end);
	FUZZ_CHECK (in >= text && in <= end, "Message-ID parser escaped its input slice");
	FUZZ_CHECK (!id || g_utf8_validate (id, -1, NULL), "Message-ID parser returned invalid UTF-8");
	g_string_append_printf (snapshot, "%" G_GSIZE_FORMAT "\n", (gsize) (in - text));
	append_text (snapshot, id);
	if (id && plausible_message_id (id)) {
		char *formatted = g_strdup_printf ("<%s>", id), *reparsed;
		const char *p = formatted;

		reparsed = g_mime_decode_msgid (&p, formatted + strlen (formatted));
		FUZZ_CHECK (reparsed && !strcmp (id, reparsed), "Plausible Message-ID did not round trip");
		g_free (formatted);
		g_free (reparsed);
	}
	g_free (id);

	in = text;
	do {
		previous = in;
		id = _g_mime_parse_reference (&in, end);
		FUZZ_CHECK (in >= previous && in <= end, "References parser escaped its input slice");
		FUZZ_CHECK (!id || (in > previous && g_utf8_validate (id, -1, NULL)),
			    "References parser returned invalid UTF-8 or made no progress");
		g_string_append_printf (snapshot, "%" G_GSIZE_FORMAT "\n", (gsize) (in - text));
		append_text (snapshot, id);
		if (!id)
			break;
		g_free (id);
	} while (in < end);

	return g_string_free (snapshot, FALSE);
}

static void
check_equal (const char *expected, const char *actual, guint parser, const char *variant)
{
	FUZZ_CHECK (!strcmp (expected, actual), "Structured-header parser %u differs for %s", parser, variant);
}

static void
check_date (const char *exact, const char *padded, gsize length)
{
	GDateTime *date = _g_mime_utils_header_decode_date (exact, length);
	GDateTime *padded_date = _g_mime_utils_header_decode_date (padded, length);

	FUZZ_CHECK ((date != NULL) == (padded_date != NULL), "Date parser read outside its slice");
	if (date) {
		GDateTime *reparsed;
		char *formatted = g_mime_utils_header_format_date (date);

		reparsed = g_mime_utils_header_decode_date (formatted);
		FUZZ_CHECK (g_date_time_equal (date, padded_date) &&
			    g_date_time_get_utc_offset (date) == g_date_time_get_utc_offset (padded_date),
			    "Date parsing changed with out-of-range bytes");
		FUZZ_CHECK (reparsed && g_date_time_equal (date, reparsed) &&
			    g_date_time_get_utc_offset (date) == g_date_time_get_utc_offset (reparsed),
			    "Date round trip changed its instant or UTC offset");
		g_date_time_unref (reparsed);
		g_free (formatted);
	}
	g_clear_pointer (&date, g_date_time_unref);
	g_clear_pointer (&padded_date, g_date_time_unref);
}

static void
check_slice (GMimeParserOptions *options, GMimeFormatOptions *format, const guint8 *data, gsize length,
	     gboolean round_trip)
{
	static const char suffix[] = "X<outside@slice>; name=outside";
	GString *warnings = g_string_new (NULL);
	char *allocation = g_malloc (MAX (length, 1));
	char *exact = length ? allocation : allocation + 1;
	char *padded = g_malloc (length + sizeof (suffix));
	char *expected, *actual;
	GError *error = NULL;
	GObject *model;
	gboolean has_nul = memchr (data, '\0', length) != NULL;
	guint parser, variant;

	/* Even an empty slice ends at the allocation boundary. No NUL is added. */
	memcpy (exact, data, length);
	memcpy (padded, data, length);
	memcpy (padded + length, suffix, sizeof (suffix));
	g_mime_parser_options_set_warning_callback (options, record_warning, warnings, NULL);

	for (parser = 0; parser < PARSER_COUNT; parser++) {
		g_string_truncate (warnings, 0);
		model = parse_model (parser, options, exact, length, &error);
		if (has_nul && parser < RAW_ADDRESS_LIST)
			FUZZ_CHECK (!model && error, "Checked structured-header parser accepted an embedded NUL");
		expected = snapshot_model (model, error, warnings->str);
		if (round_trip && model) {
			if (parser <= GROUP)
				check_address_round_trip (options, format, model);
			else if (parser == CONTENT_TYPE || parser == DISPOSITION)
				check_content_round_trip (parser, options, format, model);
			else if (parser == AUTHENTICATION_RESULTS) {
				GMimeAuthenticationResults *again;
				char *text = g_mime_authentication_results_to_string (GMIME_AUTHENTICATION_RESULTS (model));
				char *next;

				again = g_mime_authentication_results_parse (text, -1, &error);
				FUZZ_CHECK (again && !error, "Authentication-Results rejected its output: %s", text);
				next = g_mime_authentication_results_to_string (again);
				FUZZ_CHECK (!strcmp (text, next), "Authentication-Results serialization did not stabilize");
				g_object_unref (again);
				g_free (next);
				g_free (text);
			}
		}
		g_clear_object (&model);
		g_clear_error (&error);

		for (variant = 0; variant < 2; variant++) {
			if (variant && (has_nul || parser >= RAW_ADDRESS_LIST))
				break;
			padded[length] = variant ? '\0' : suffix[0];
			g_string_truncate (warnings, 0);
			model = parse_model (parser, options, padded, variant ? -1 : (gssize) length, &error);
			actual = snapshot_model (model, error, warnings->str);
			check_equal (expected, actual, parser, variant ? "C-string input" : "out-of-range suffix");
			g_clear_object (&model);
			g_clear_error (&error);
			g_free (actual);
		}
		g_free (expected);
	}

	padded[length] = suffix[0];
	expected = snapshot_identifiers (exact, length);
	actual = snapshot_identifiers (padded, length);
	check_equal (expected, actual, PARSER_COUNT, "identifier out-of-range suffix");
	g_free (expected);
	g_free (actual);
	check_date (exact, padded, length);
	fuzz_equal (data, length, (const guint8 *) exact, length, "Structured-header input preservation");
	g_mime_parser_options_set_warning_callback (options, NULL, NULL, NULL);
	g_string_free (warnings, TRUE);
	g_free (padded);
	g_free (allocation);
}

void
fuzz_structured_headers (const guint8 *data, gsize length)
{
	static const GMimeRfcComplianceMode modes[] = {
		GMIME_RFC_COMPLIANCE_STRICT, GMIME_RFC_COMPLIANCE_LOOSE, GMIME_RFC_COMPLIANCE_LOOSER
	};
	GMimeParserOptions *options = fuzz_parser_options ();
	GMimeFormatOptions *format = g_mime_format_options_new ();
	guint32 selector = fuzz_selector (data, length);
	gsize split = selector % (length + 1);

	if (!length)
		data = (const guint8 *) "";
	g_mime_parser_options_set_address_compliance_mode (options, modes[selector % G_N_ELEMENTS (modes)]);
	g_mime_parser_options_set_parameter_compliance_mode (options, modes[selector % G_N_ELEMENTS (modes)]);
	g_mime_parser_options_set_allow_addresses_without_domain (options, (selector & 4) != 0);
	g_mime_parser_options_set_allow_unquoted_commas_in_addresses (options, (selector & 8) != 0);
	g_mime_parser_options_set_rfc2047_compliance_mode (options, selector & 4 ?
							GMIME_RFC_COMPLIANCE_STRICT : GMIME_RFC_COMPLIANCE_LOOSE);
	g_mime_format_options_set_newline_format (format, GMIME_NEWLINE_FORMAT_UNIX);
	check_slice (options, format, data, length, TRUE);
	if (length) {
		check_slice (options, format, data, split, FALSE);
		if (split)
			check_slice (options, format, data + split, length - split, FALSE);
	}
	g_mime_format_options_unref (format);
	g_mime_parser_options_unref (options);
}
