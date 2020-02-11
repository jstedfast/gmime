/*
 *  Example application demonstrating the GMime parser's feature for
 *  detecting and reporting RFC violations in messages.
 *
 *  Written by (C) Albrecht Dreß <albrecht.dress@arcor.de> 2017
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <gmime/gmime.h>


typedef struct {
	gint64 offset;
	GMimeParserWarning errcode;
	gchar *message;
} issue_log_elem_t;


static const gchar *
errcode2str(GMimeParserWarning errcode)
{
	switch (errcode) {
	case GMIME_WARN_DUPLICATED_HEADER:
		return "duplicated header";
	case GMIME_WARN_DUPLICATED_PARAMETER:
		return "duplicated header parameter";
	case GMIME_WARN_UNENCODED_8BIT_HEADER:
		return "unencoded 8-bit characters in header";
	case GMIME_WARN_INVALID_CONTENT_TYPE:
		return "invalid Content-Type";
	case GMIME_WARN_INVALID_RFC2047_HEADER_VALUE:
		return "invalid RFC 2047 encoded header value";
	case GMIME_WARN_INVALID_PARAMETER:
		return "invalid header parameter";
	case GMIME_WARN_MALFORMED_MULTIPART:
		return "malformed multipart";
	case GMIME_WARN_TRUNCATED_MESSAGE:
		return "truncated message";
	case GMIME_WARN_MALFORMED_MESSAGE:
		return "malformed message";
	case GMIME_WARN_INVALID_ADDRESS_LIST:
		return "invalid address list";
	case GMIME_CRIT_INVALID_HEADER_NAME:
		return "invalid header name, parser may skip the message or parts of it";
	case GMIME_CRIT_CONFLICTING_HEADER:
		return "conflicting duplicated header";
	case GMIME_CRIT_CONFLICTING_PARAMETER:
		return "conflicting header parameter";
	case GMIME_CRIT_MULTIPART_WITHOUT_BOUNDARY:
		return "multipart without boundary";
	default:
		return "unknown";
	}
}


static void
parser_issue (gint64 offset, GMimeParserWarning errcode, const gchar *item, gpointer user_data)
{
	GList **issues = (GList **) user_data;
	issue_log_elem_t *new_issue;

	new_issue = g_new (issue_log_elem_t, 1U);
	new_issue->offset = offset;
	new_issue->errcode = errcode;
	if (item == NULL) {
		new_issue->message = g_strdup (errcode2str (errcode));
	} else {
		gchar *buf;

		buf = g_strdup (item);
		g_strstrip (buf);
		new_issue->message = g_strdup_printf ("%s: '%s'", errcode2str (errcode), buf);
		g_free (buf);
	}
	*issues = g_list_append (*issues, new_issue);
}


static void
check_msg_file (const gchar *filename)
{
	GMimeStream *stream;
	GError *error = NULL;

	stream = g_mime_stream_file_open (filename, "r", &error);

	if (stream == NULL) {
		g_warning ("failed to open %s: %s", filename, error->message);
		g_error_free (error);
	} else {
		GMimeParser *parser;
		GMimeParserOptions *options;
		GMimeMessage *message;
		GList *issues = NULL;

		parser = g_mime_parser_new ();
		g_mime_parser_init_with_stream (parser, stream);
		options = g_mime_parser_options_new ();
		g_mime_parser_options_set_warning_callback (options, parser_issue, &issues);
		message = g_mime_parser_construct_message (parser, options);
		g_mime_parser_options_free (options);
		g_object_unref (parser);
		g_object_unref (stream);
		if (message != NULL) {
			g_object_unref (message);
		}

		if (issues == NULL) {
			g_printf ("%s: message looks benign\n", filename);
		} else {
			GList *this_issue;

			g_printf ("%s: message contains %u RFC violations:\n", filename, g_list_length (issues));
			for (this_issue = issues; this_issue != NULL; this_issue = this_issue->next) {
				issue_log_elem_t *issue_data = (issue_log_elem_t *) this_issue->data;

				g_printf ("offset %" G_GINT64_FORMAT ": [%u] %s\n",
					issue_data->offset, issue_data->errcode, issue_data->message);
				g_free (issue_data->message);
				g_free (issue_data);
			}
			g_list_free (issues);
		}
	}
}


int
main (int argc, char **argv)
{
	int n;

	if (argc < 2) {
		g_message ("usage: %s <filename> [<filename> ...]", argv[0]);
		exit (1);
	}

	g_mime_init ();

	for (n = 1; n < argc; n++) {
		check_msg_file (argv[n]);
	}

	g_mime_shutdown ();
	return 0;
}
