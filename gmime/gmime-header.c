/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2020 Jeffrey Stedfast
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

#include <string.h>
#include <ctype.h>

#include "gmime-stream-filter.h"
#include "gmime-table-private.h"
#include "gmime-parse-utils.h"
#include "gmime-stream-mem.h"
#include "internet-address.h"
#include "gmime-references.h"
#include "gmime-internal.h"
#include "gmime-common.h"
#include "gmime-header.h"
#include "gmime-events.h"
#include "gmime-utils.h"


/**
 * SECTION: gmime-header
 * @title: GMimeHeaderList
 * @short_description: Message and MIME part headers
 * @see_also: #GMimeObject
 *
 * A #GMimeHeader is a collection of rfc822 header fields and their
 * values.
 **/


static struct {
	const char *name;
	GMimeHeaderRawValueFormatter formatter;
} formatters[] = {
	{ "Received",                    g_mime_header_format_received            },
	{ "Sender",                      g_mime_header_format_addrlist            },
	{ "From",                        g_mime_header_format_addrlist            },
	{ "Reply-To",                    g_mime_header_format_addrlist            },
	{ "To",                          g_mime_header_format_addrlist            },
	{ "Cc",                          g_mime_header_format_addrlist            },
	{ "Bcc",                         g_mime_header_format_addrlist            },
	{ "Message-Id",                  g_mime_header_format_message_id          },
	{ "In-Reply-To",                 g_mime_header_format_references          },
	{ "References",                  g_mime_header_format_references          },
	{ "Resent-Sender",               g_mime_header_format_addrlist            },
	{ "Resent-From",                 g_mime_header_format_addrlist            },
	{ "Resent-Reply-To",             g_mime_header_format_addrlist            },
	{ "Resent-To",                   g_mime_header_format_addrlist            },
	{ "Resent-Cc",                   g_mime_header_format_addrlist            },
	{ "Resent-Bcc",                  g_mime_header_format_addrlist            },
	{ "Resent-Message-Id",           g_mime_header_format_message_id          },
	{ "Content-Type",                g_mime_header_format_content_type        },
	{ "Content-Disposition",         g_mime_header_format_content_disposition },
	{ "Content-Id",                  g_mime_header_format_message_id          },
	{ "Disposition-Notification-To", g_mime_header_format_addrlist            },
};


static void g_mime_header_class_init (GMimeHeaderClass *klass);
static void g_mime_header_init (GMimeHeader *header, GMimeHeaderClass *klass);
static void g_mime_header_finalize (GObject *object);

static GObjectClass *parent_class = NULL;


GType
g_mime_header_get_type (void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof (GMimeHeaderClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc) g_mime_header_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (GMimeHeader),
			0,    /* n_preallocs */
			(GInstanceInitFunc) g_mime_header_init,
		};
		
		type = g_type_register_static (G_TYPE_OBJECT, "GMimeHeader", &info, 0);
	}
	
	return type;
}

static void
g_mime_header_class_init (GMimeHeaderClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	
	parent_class = g_type_class_ref (G_TYPE_OBJECT);
	
	object_class->finalize = g_mime_header_finalize;
}

static void
g_mime_header_init (GMimeHeader *header, GMimeHeaderClass *klass)
{
	header->changed = g_mime_event_new (header);
	header->formatter = NULL;
	header->options = NULL;
	header->reformat = FALSE;
	header->raw_value = NULL;
	header->raw_name = NULL;
	header->charset = NULL;
	header->value = NULL;
	header->name = NULL;
	header->offset = -1;
}

static void
g_mime_header_finalize (GObject *object)
{
	GMimeHeader *header = (GMimeHeader *) object;
	
	g_mime_event_free (header->changed);
	g_free (header->raw_value);
	g_free (header->raw_name);
	g_free (header->charset);
	g_free (header->value);
	g_free (header->name);
	
	G_OBJECT_CLASS (parent_class)->finalize (object);
}


/**
 * g_mime_header_new:
 * @options: (nullable): a #GMimeParserOptions or %NULL
 * @name: header name
 * @value: header value
 * @raw_value: raw header value
 * @charset: a charset
 * @offset: file/stream offset for the start of the header (or %-1 if unknown)
 *
 * Creates a new #GMimeHeader.
 *
 * Returns: a new #GMimeHeader with the specified values.
 **/
static GMimeHeader *
g_mime_header_new (GMimeParserOptions *options, const char *name, const char *value,
		   const char *raw_name, const char *raw_value, const char *charset,
		   gint64 offset)
{
	GMimeHeaderRawValueFormatter formatter;
	GMimeHeader *header;
	guint i;
	
	header = g_object_new (GMIME_TYPE_HEADER, NULL);
	header->raw_value = raw_value ? g_strdup (raw_value) : NULL;
	header->charset = charset ? g_strdup (charset) : NULL;
	header->value = value ? g_strdup (value) : NULL;
	header->raw_name = g_strdup (raw_name);
	header->name = g_strdup (name);
	header->reformat = !raw_value;
	header->options = options;
	header->offset = offset;
	
	formatter = g_mime_header_format_default;
	for (i = 0; i < G_N_ELEMENTS (formatters); i++) {
		if (!g_ascii_strcasecmp (formatters[i].name, name)) {
			formatter = header->formatter = formatters[i].formatter;
			break;
		}
	}
	
	if (!raw_value && value)
		header->raw_value = formatter (header, NULL, header->value, charset);
	
	return header;
}


/**
 * g_mime_header_get_name:
 * @header: a #GMimeHeader
 *
 * Gets the header's name.
 *
 * Returns: the header name or %NULL if invalid.
 **/
const char *
g_mime_header_get_name (GMimeHeader *header)
{
	g_return_val_if_fail (GMIME_IS_HEADER (header), NULL);
	
	return header->name;
}


/**
 * g_mime_header_get_value:
 * @header: a #GMimeHeader
 *
 * Gets the header's unfolded value.
 *
 * Returns: the header's decoded value or %NULL if invalid.
 **/
const char *
g_mime_header_get_value (GMimeHeader *header)
{
	char *buf;
	
	g_return_val_if_fail (GMIME_IS_HEADER (header), NULL);
	
	if (!header->value && header->raw_value) {
		buf = g_mime_utils_header_unfold (header->raw_value);
		header->value = _g_mime_utils_header_decode_text (header->options, buf, NULL, header->offset);
		g_free (buf);
	}
	
	return header->value;
}


/**
 * g_mime_header_set_value:
 * @header: a #GMimeHeader
 * @options: (nullable): a #GMimeFormatOptions or %NULL
 * @value: the new header value
 * @charset: a charset
 *
 * Sets the header's decoded value.
 **/
void
g_mime_header_set_value (GMimeHeader *header, GMimeFormatOptions *options, const char *value, const char *charset)
{
	GMimeHeaderRawValueFormatter formatter;
	char *buf;
	
	g_return_if_fail (GMIME_IS_HEADER (header));
	g_return_if_fail (value != NULL);
	
	formatter = header->formatter ? header->formatter : g_mime_header_format_default;
	buf = g_mime_strdup_trim (value);
	g_free (header->raw_value);
	g_free (header->charset);
	g_free (header->value);
	
	header->raw_value = formatter (header, options, buf, charset);
	header->charset = charset ? g_strdup (charset) : NULL;
	header->reformat = TRUE;
	header->value = buf;
	
	g_mime_event_emit (header->changed, NULL);
}


/**
 * g_mime_header_get_raw_name:
 * @header: a #GMimeHeader
 *
 * Gets the header's raw name. The raw header name is the complete string up to
 * (but not including) the ':' separating the header's name from its value. This
 * string may be different from the value returned by g_mime_header_get_name()
 * if the parsed message's header contained trailing whitespace after the header
 * name, such as: "Subject : this is the subject\r\n".
 *
 * Returns: the raw header name.
 **/
const char *
g_mime_header_get_raw_name (GMimeHeader *header)
{
	g_return_val_if_fail (GMIME_IS_HEADER (header), NULL);
	
	return header->raw_name;
}


/**
 * g_mime_header_get_raw_value:
 * @header: a #GMimeHeader
 *
 * Gets the header's raw (folded) value.
 *
 * Returns: the header value or %NULL if invalid.
 **/
const char *
g_mime_header_get_raw_value (GMimeHeader *header)
{
	g_return_val_if_fail (GMIME_IS_HEADER (header), NULL);
	
	return header->raw_value;
}


#if 0
void
_g_mime_header_set_raw_value (GMimeHeader *header, const char *raw_value)
{
	char *buf = g_strdup (raw_value);
	
	g_free (header->raw_value);
	g_free (header->value);
	
	header->raw_value = buf;
	header->value = NULL;
}
#endif


/**
 * g_mime_header_set_raw_value:
 * @header: a #GMimeHeader
 * @raw_value: the raw value
 *
 * Sets the header's raw value.
 **/
void
g_mime_header_set_raw_value (GMimeHeader *header, const char *raw_value)
{
	char *buf;
	
	g_return_if_fail (GMIME_IS_HEADER (header));
	g_return_if_fail (raw_value != NULL);
	
	buf = g_strdup (raw_value);
	g_free (header->raw_value);
	g_free (header->value);

	header->reformat = FALSE;
	header->raw_value = buf;
	header->value = NULL;
	
	g_mime_event_emit (header->changed, NULL);
}


/**
 * g_mime_header_get_offset:
 * @header: a #GMimeHeader
 *
 * Gets the header's stream offset if known.
 *
 * Returns: the header offset or %-1 if unknown.
 **/
gint64
g_mime_header_get_offset (GMimeHeader *header)
{
	g_return_val_if_fail (GMIME_IS_HEADER (header), -1);
	
	return header->offset;
}


void
_g_mime_header_set_offset (GMimeHeader *header, gint64 offset)
{
	header->offset = offset;
}


/**
 * g_mime_header_write_to_stream:
 * @header: a #GMimeHeader
 * @options: (nullable): a #GMimeFormatOptions or %NULL
 * @stream: a #GMimeStream
 *
 * Write the header to the specified stream.
 *
 * Returns: the number of bytes written, or %-1 on fail.
 **/
ssize_t
g_mime_header_write_to_stream (GMimeHeader *header, GMimeFormatOptions *options, GMimeStream *stream)
{
	GMimeHeaderRawValueFormatter formatter;
	ssize_t nwritten, total = 0;
	char *raw_value, *buf;
	
	g_return_val_if_fail (GMIME_IS_HEADER (header), -1);
	g_return_val_if_fail (GMIME_IS_STREAM (stream), -1);
	
	if (!header->raw_value)
		return 0;

	if (header->reformat) {
		formatter = header->formatter ? header->formatter : g_mime_header_format_default;
		raw_value = formatter (header, options, header->value, header->charset);
	} else {
		raw_value = header->raw_value;
	}
	
	buf = g_strdup_printf ("%s:%s", header->raw_name, raw_value);
	nwritten = g_mime_stream_write_string (stream, buf);
	
	if (header->reformat)
		g_free (raw_value);
	g_free (buf);
	
	if (nwritten == -1)
		return -1;
	
	total += nwritten;
	
	return total;
}


/**
 * g_mime_header_format_content_disposition:
 * @header: a #GMimeHeader
 * @options: (nullable): a #GMimeFormatOptions or %NULL
 * @value: a Content-Disposition header value
 * @charset: a charset (note: unused)
 *
 * Parses the @value and then re-formats it to conform to the formatting options,
 * folding the value if necessary.
 *
 * Returns: a newly allocated string containing the reformatted value.
 **/
char *
g_mime_header_format_content_disposition (GMimeHeader *header, GMimeFormatOptions *options, const char *value, const char *charset)
{
	GMimeContentDisposition *disposition;
	GString *str;
	guint n;
	
	str = g_string_new (header->raw_name);
	g_string_append_c (str, ':');
	n = str->len;
	
	g_string_append_c (str, ' ');
	
	disposition = g_mime_content_disposition_parse (header->options, value);
	g_string_append (str, disposition->disposition);
	
	g_mime_param_list_encode (disposition->params, options, TRUE, str);
	g_object_unref (disposition);
	
	memmove (str->str, str->str + n, (str->len - n) + 1);
	
	return g_string_free (str, FALSE);
}


/**
 * g_mime_header_format_content_type:
 * @header: a #GMimeHeader
 * @options: (nullable): a #GMimeFormatOptions or %NULL
 * @value: a Content-Type header value
 * @charset: a charset (note: unused)
 *
 * Parses the @value and then re-formats it to conform to the formatting options,
 * folding the value if necessary.
 *
 * Returns: a newly allocated string containing the reformatted value.
 **/
char *
g_mime_header_format_content_type (GMimeHeader *header, GMimeFormatOptions *options, const char *value, const char *charset)
{
	GMimeContentType *content_type;
	GString *str;
	guint n;
	
	str = g_string_new (header->raw_name);
	g_string_append_c (str, ':');
	n = str->len;
	
	content_type = g_mime_content_type_parse (header->options, value);
	
	g_string_append_c (str, ' ');
	g_string_append (str, content_type->type ? content_type->type : "text");
	g_string_append_c (str, '/');
	g_string_append (str, content_type->subtype ? content_type->subtype : "plain");
	
	g_mime_param_list_encode (content_type->params, options, TRUE, str);
	g_object_unref (content_type);
	
	memmove (str->str, str->str + n, (str->len - n) + 1);
	
	return g_string_free (str, FALSE);
}


/**
 * g_mime_header_format_message_id:
 * @header: a #GMimeHeader
 * @options: (nullable): a #GMimeFormatOptions or %NULL
 * @value: a Message-Id or Content-Id header value
 * @charset: a charset (note: unused)
 *
 * Parses the @value and then re-formats it to conform to the formatting options,
 * folding the value if necessary.
 *
 * Returns: a newly allocated string containing the reformatted value.
 **/
char *
g_mime_header_format_message_id (GMimeHeader *header, GMimeFormatOptions *options, const char *value, const char *charset)
{
	const char *newline = g_mime_format_options_get_newline (options);
	
	return g_strdup_printf (" %s%s", value, newline);
}


/**
 * g_mime_header_format_references:
 * @header: a #GMimeHeader
 * @options: (nullable): a #GMimeFormatOptions or %NULL
 * @value: a References or In-Reply-To header value
 * @charset: a charset (note: unused)
 *
 * Parses the @value and then re-formats it to conform to the formatting options,
 * folding the value if necessary.
 *
 * Returns: a newly allocated string containing the reformatted value.
 **/
char *
g_mime_header_format_references (GMimeHeader *header, GMimeFormatOptions *options, const char *value, const char *charset)
{
	const char *newline, *msgid;
	GMimeReferences *refs;
	guint cur, len, n;
	GString *str;
	int count, i;
	
	/* Note: we don't want to break in the middle of msgid tokens as
	   it seems to break a lot of clients (and servers) */
	newline = g_mime_format_options_get_newline (options);
	refs = g_mime_references_parse (header->options, value);
	str = g_string_new (header->raw_name);
	g_string_append_c (str, ':');
	cur = n = str->len;
	
	count = g_mime_references_length (refs);
	for (i = 0; i < count; i++) {
		msgid = g_mime_references_get_message_id (refs, i);
		len = strlen (msgid);
		if (cur > 1 && cur + len + 3 >= GMIME_FOLD_LEN) {
			g_string_append (str, newline);
			g_string_append_c (str, '\t');
			cur = 1;
		} else {
			g_string_append_c (str, ' ');
			cur++;
		}
		
		g_string_append_c (str, '<');
		g_string_append_len (str, msgid, len);
		g_string_append_c (str, '>');
		cur += len + 2;
	}
	
	g_mime_references_free (refs);
	
	g_string_append (str, newline);
	
	memmove (str->str, str->str + n, (str->len - n) + 1);
	
	return g_string_free (str, FALSE);
}


/**
 * g_mime_header_format_addrlist:
 * @header: a #GMimeHeader
 * @options: (nullable): a #GMimeFormatOptions or %NULL
 * @value: a Sender, From, Reply-To, To, Cc, or Bcc header value
 * @charset: a charset (note: unused)
 *
 * Parses the @value and then re-formats it to conform to the formatting options,
 * folding the value if necessary.
 *
 * Returns: a newly allocated string containing the reformatted value.
 **/
char *
g_mime_header_format_addrlist (GMimeHeader *header, GMimeFormatOptions *options, const char *value, const char *charset)
{
	InternetAddressList *addrlist;
	GString *str;
	guint n;
	
	str = g_string_new (header->raw_name);
	g_string_append_c (str, ':');
	n = str->len;
	
	g_string_append_c (str, ' ');
	
	if (value && (addrlist = _internet_address_list_parse (header->options, value, -1))) {
		internet_address_list_encode (addrlist, options, str);
		g_object_unref (addrlist);
	}
	
	memmove (str->str, str->str + n, (str->len - n) + 1);
	
	return g_string_free (str, FALSE);
}


typedef void (* token_skip_t) (const char **in);

struct _received_token {
	char *token;
	size_t len;
	token_skip_t skip;
};

static void skip_cfws_atom (const char **in);
static void skip_domain    (const char **in);
static void skip_addr      (const char **in);
static void skip_msgid     (const char **in);

static struct _received_token received_tokens[] = {
	{ "from ", 5, skip_domain    },
	{ "by ",   3, skip_domain    },
	{ "via ",  4, skip_cfws_atom },
	{ "with ", 5, skip_cfws_atom },
	{ "id ",   3, skip_msgid     },
	{ "for ",  4, skip_addr      }
};

static void
skip_cfws_atom (const char **in)
{
	skip_cfws (in);
	skip_atom (in);
}

static void
skip_domain_subliteral (const char **in)
{
	const char *inptr = *in;
	
	while (*inptr && *inptr != '.' && *inptr != ']') {
		if (is_dtext (*inptr)) {
			inptr++;
		} else if (is_lwsp (*inptr)) {
			skip_cfws (&inptr);
		} else {
			break;
		}
	}
	
	*in = inptr;
}

static void
skip_domain_literal (const char **in)
{
	const char *inptr = *in;
	
	skip_cfws (&inptr);
	while (*inptr && *inptr != ']') {
		skip_domain_subliteral (&inptr);
		if (*inptr && *inptr != ']')
			inptr++;
	}
	
	*in = inptr;
}

static void
skip_domain (const char **in)
{
	const char *save, *inptr = *in;
	
	while (inptr && *inptr) {
		skip_cfws (&inptr);
		if (*inptr == '[') {
			/* domain literal */
			inptr++;
			skip_domain_literal (&inptr);
			if (*inptr == ']')
				inptr++;
		} else {
			skip_atom (&inptr);
		}
		
		save = inptr;
		skip_cfws (&inptr);
		if (*inptr != '.') {
			inptr = save;
			break;
		}
		
		inptr++;
	}
	
	*in = inptr;
}

static void
skip_addrspec (const char **in)
{
	const char *inptr = *in;
	
	skip_cfws (&inptr);
	skip_word (&inptr);
	skip_cfws (&inptr);
	
	while (*inptr == '.') {
		inptr++;
		skip_cfws (&inptr);
		skip_word (&inptr);
		skip_cfws (&inptr);
	}
	
	if (*inptr == '@') {
		inptr++;
		skip_domain (&inptr);
	}
	
	*in = inptr;
}

static void
skip_addr (const char **in)
{
	const char *inptr = *in;
	
	skip_cfws (&inptr);
	if (*inptr == '<') {
		inptr++;
		skip_addrspec (&inptr);
		if (*inptr == '>')
			inptr++;
	} else {
		skip_addrspec (&inptr);
	}
	
	*in = inptr;
}

static void
skip_msgid (const char **in)
{
	const char *inptr = *in;
	
	skip_cfws (&inptr);
	if (*inptr == '<') {
		inptr++;
		skip_addrspec (&inptr);
		if (*inptr == '>')
			inptr++;
	} else {
		skip_atom (&inptr);
	}
	
	*in = inptr;
}

struct _received_part {
	struct _received_part *next;
	const char *start;
	size_t len;
};


/**
 * g_mime_header_format_received:
 * @header: a #GMimeHeader
 * @options: (nullable): a #GMimeFormatOptions or %NULL
 * @value: a Received header value
 * @charset: a charset (note: unused)
 *
 * Parses the @value and then re-formats it to conform to the formatting options,
 * folding the value if necessary.
 *
 * Returns: a newly allocated string containing the reformatted value.
 **/
char *
g_mime_header_format_received (GMimeHeader *header, GMimeFormatOptions *options, const char *value, const char *charset)
{
	struct _received_part *parts, *part, *tail;
	const char *inptr, *lwsp = NULL;
	const char *newline;
	guint len, n, i;
	GString *str;
	
	newline = g_mime_format_options_get_newline (options);
	
	while (is_lwsp (*value))
		value++;
	
	if (*value == '\0')
		return g_strdup (newline);
	
	str = g_string_new (header->raw_name);
	g_string_append_c (str, ':');
	n = str->len;
	
	g_string_append_c (str, ' ');
	len = str->len;
	
	tail = parts = part = g_alloca (sizeof (struct _received_part));
	part->start = inptr = value;
	part->next = NULL;
	
	while (*inptr) {
		for (i = 0; i < G_N_ELEMENTS (received_tokens); i++) {
			if (!strncmp (inptr, received_tokens[i].token, received_tokens[i].len)) {
				if (inptr > part->start) {
					g_assert (lwsp != NULL);
					part->len = lwsp - part->start;
					
					part = g_alloca (sizeof (struct _received_part));
					part->start = inptr;
					part->next = NULL;
					
					tail->next = part;
					tail = part;
				}
				
				inptr += received_tokens[i].len;
				received_tokens[i].skip (&inptr);
				
				lwsp = inptr;
				while (is_lwsp (*inptr))
					inptr++;
				
				if (*inptr == ';') {
					inptr++;
					
					part->len = inptr - part->start;
					
					lwsp = inptr;
					while (is_lwsp (*inptr))
						inptr++;
					
					part = g_alloca (sizeof (struct _received_part));
					part->start = inptr;
					part->next = NULL;
					
					tail->next = part;
					tail = part;
				}
				
				break;
			}
		}
		
		if (i == G_N_ELEMENTS (received_tokens)) {
			while (*inptr && !is_lwsp (*inptr))
				inptr++;
			
			lwsp = inptr;
			while (is_lwsp (*inptr))
				inptr++;
		}
		
		if (*inptr == '(') {
			skip_comment (&inptr);
			
			lwsp = inptr;
			while (is_lwsp (*inptr))
				inptr++;
		}
	}
	
	part->len = lwsp - part->start;
	
	lwsp = NULL;
	part = parts;
	do {
		len += lwsp ? part->start - lwsp : 0;
		if (len + part->len > GMIME_FOLD_LEN && part != parts) {
			g_string_append (str, newline);
			g_string_append_c (str, '\t');
			len = 1;
		} else if (lwsp) {
			g_string_append_len (str, lwsp, (size_t) (part->start - lwsp));
		}
		
		g_string_append_len (str, part->start, part->len);
		lwsp = part->start + part->len;
		len += part->len;
		
		part = part->next;
	} while (part != NULL);
	
	g_string_append (str, newline);
	
	memmove (str->str, str->str + n, (str->len - n) + 1);
	
	return g_string_free (str, FALSE);
}


/**
 * g_mime_header_format_default:
 * @header: a #GMimeHeader
 * @options: (nullable): a #GMimeFormatOptions or %NULL
 * @value: a header value
 * @charset: a charset to use when encoding the @value
 *
 * Parses the @value and then re-formats it to conform to the formatting options,
 * folding the value if necessary.
 *
 * Returns: a newly allocated string containing the reformatted value.
 **/
char *
g_mime_header_format_default (GMimeHeader *header, GMimeFormatOptions *options, const char *value, const char *charset)
{
	char *encoded, *raw_value;
	
	encoded = g_mime_utils_header_encode_text (options, value, charset);
	raw_value = _g_mime_utils_unstructured_header_fold (header->options, options, header->raw_name, encoded);
	g_free (encoded);
	
	return raw_value;
}


static void
header_changed (GMimeHeader *header, gpointer user_args, GMimeHeaderList *list)
{
	GMimeHeaderListChangedEventArgs args;
	
	args.action = GMIME_HEADER_LIST_CHANGED_ACTION_CHANGED;
	args.header = header;
	
	g_mime_event_emit (list->changed, &args);
}


static void g_mime_header_list_class_init (GMimeHeaderListClass *klass);
static void g_mime_header_list_init (GMimeHeaderList *list, GMimeHeaderListClass *klass);
static void g_mime_header_list_finalize (GObject *object);


static GObjectClass *list_parent_class = NULL;


GType
g_mime_header_list_get_type (void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof (GMimeHeaderListClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc) g_mime_header_list_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (GMimeHeaderList),
			0,    /* n_preallocs */
			(GInstanceInitFunc) g_mime_header_list_init,
		};
		
		type = g_type_register_static (G_TYPE_OBJECT, "GMimeHeaderList", &info, 0);
	}
	
	return type;
}


static void
g_mime_header_list_class_init (GMimeHeaderListClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	
	list_parent_class = g_type_class_ref (G_TYPE_OBJECT);
	
	object_class->finalize = g_mime_header_list_finalize;
}

static void
g_mime_header_list_init (GMimeHeaderList *list, GMimeHeaderListClass *klass)
{
	list->hash = g_hash_table_new (g_mime_strcase_hash,
				       g_mime_strcase_equal);
	list->changed = g_mime_event_new (list);
	list->array = g_ptr_array_new ();
}

static void
g_mime_header_list_finalize (GObject *object)
{
	GMimeHeaderList *headers = (GMimeHeaderList *) object;
	GMimeHeader *header;
	guint i;
	
	for (i = 0; i < headers->array->len; i++) {
		header = (GMimeHeader *) headers->array->pdata[i];
		g_mime_event_remove (header->changed, (GMimeEventCallback) header_changed, headers);
		g_object_unref (header);
	}
	
	g_ptr_array_free (headers->array, TRUE);
	
	g_mime_parser_options_free (headers->options);
	g_hash_table_destroy (headers->hash);
	g_mime_event_free (headers->changed);
	
	G_OBJECT_CLASS (list_parent_class)->finalize (object);
}


/**
 * g_mime_header_list_new:
 * @options: (nullable): a #GMimeParserOptions or %NULL
 *
 * Creates a new #GMimeHeaderList object.
 *
 * Returns: a new header list object.
 **/
GMimeHeaderList *
g_mime_header_list_new (GMimeParserOptions *options)
{
	GMimeHeaderList *headers;
	
	headers = g_object_new (GMIME_TYPE_HEADER_LIST, NULL);
	headers->options = g_mime_parser_options_clone (options);
	
	return headers;
}


/**
 * g_mime_header_list_clear:
 * @headers: a #GMimeHeaderList
 *
 * Removes all of the headers from the #GMimeHeaderList.
 **/
void
g_mime_header_list_clear (GMimeHeaderList *headers)
{
	GMimeHeaderListChangedEventArgs args;
	GMimeHeader *header;
	guint i;
	
	g_return_if_fail (GMIME_IS_HEADER_LIST (headers));
	
	for (i = 0; i < headers->array->len; i++) {
		header = (GMimeHeader *) headers->array->pdata[i];
		g_mime_event_remove (header->changed, (GMimeEventCallback) header_changed, headers);
		g_object_unref (header);
	}
	
	g_hash_table_remove_all (headers->hash);
	
	g_ptr_array_set_size (headers->array, 0);
	
	args.action = GMIME_HEADER_LIST_CHANGED_ACTION_CLEARED;
	args.header = NULL;
	
	g_mime_event_emit (headers->changed, &args);
}


GMimeParserOptions *
_g_mime_header_list_get_options (GMimeHeaderList *headers)
{
	return headers->options;
}

void
_g_mime_header_list_set_options (GMimeHeaderList *headers, GMimeParserOptions *options)
{
	g_mime_parser_options_free (headers->options);
	headers->options = g_mime_parser_options_clone (options);
}


/**
 * g_mime_header_list_get_count:
 * @headers: a #GMimeHeaderList
 *
 * Gets the number of headers contained within the header list.
 *
 * Returns: the number of headers in the header list.
 **/
int
g_mime_header_list_get_count (GMimeHeaderList *headers)
{
	g_return_val_if_fail (GMIME_IS_HEADER_LIST (headers), -1);
	
	return headers->array->len;
}


/**
 * g_mime_header_list_contains:
 * @headers: a #GMimeHeaderList
 * @name: header name
 *
 * Checks whether or not a header exists.
 *
 * Returns: %TRUE if the specified header exists or %FALSE otherwise.
 **/
gboolean
g_mime_header_list_contains (GMimeHeaderList *headers, const char *name)
{
	GMimeHeader *header;
	
	g_return_val_if_fail (GMIME_IS_HEADER_LIST (headers), FALSE);
	g_return_val_if_fail (name != NULL, FALSE);
	
	if (!(header = g_hash_table_lookup (headers->hash, name)))
		return FALSE;
	
	return TRUE;
}


/**
 * g_mime_header_list_prepend:
 * @headers: a #GMimeHeaderList
 * @name: header name
 * @value: header value
 * @charset: a charset
 *
 * Prepends a header. If @value is %NULL, a space will be set aside
 * for it (useful for setting the order of headers before values can
 * be obtained for them) otherwise the header will be unset.
 **/
void
g_mime_header_list_prepend (GMimeHeaderList *headers, const char *name, const char *value, const char *charset)
{
	GMimeHeaderListChangedEventArgs args;
	unsigned char *dest, *src;
	GMimeHeader *header;
	guint n;
	
	g_return_if_fail (GMIME_IS_HEADER_LIST (headers));
	g_return_if_fail (name != NULL);
	
	header = g_mime_header_new (headers->options, name, value, name, NULL, charset, -1);
	g_mime_event_add (header->changed, (GMimeEventCallback) header_changed, headers);
	g_hash_table_replace (headers->hash, header->name, header);
	
	if (headers->array->len > 0) {
		g_ptr_array_set_size (headers->array, headers->array->len + 1);
		
		dest = ((unsigned char *) headers->array->pdata) + sizeof (void *);
		src = (unsigned char *) headers->array->pdata;
		n = headers->array->len - 1;
		
		memmove (dest, src, (sizeof (void *) * n));
		headers->array->pdata[0] = header;
	} else {
		g_ptr_array_add (headers->array, header);
	}
	
	args.action = GMIME_HEADER_LIST_CHANGED_ACTION_ADDED;
	args.header = header;
	
	g_mime_event_emit (headers->changed, &args);
}


void
_g_mime_header_list_append (GMimeHeaderList *headers, const char *name, const char *raw_name,
			    const char *raw_value, gint64 offset)
{
	GMimeHeaderListChangedEventArgs args;
	GMimeHeader *header;
	
	header = g_mime_header_new (headers->options, name, NULL, raw_name, raw_value, NULL, offset);
	g_mime_event_add (header->changed, (GMimeEventCallback) header_changed, headers);
	g_ptr_array_add (headers->array, header);
	
	if (!g_hash_table_lookup (headers->hash, name))
		g_hash_table_insert (headers->hash, header->name, header);
	
	args.action = GMIME_HEADER_LIST_CHANGED_ACTION_ADDED;
	args.header = header;
	
	g_mime_event_emit (headers->changed, &args);
}


/**
 * g_mime_header_list_append:
 * @headers: a #GMimeHeaderList
 * @name: header name
 * @value: header value
 * @charset: a charset
 *
 * Appends a header. If @value is %NULL, a space will be set aside for it
 * (useful for setting the order of headers before values can be
 * obtained for them) otherwise the header will be unset.
 **/
void
g_mime_header_list_append (GMimeHeaderList *headers, const char *name, const char *value, const char *charset)
{
	GMimeHeaderListChangedEventArgs args;
	GMimeHeader *header;
	
	g_return_if_fail (GMIME_IS_HEADER_LIST (headers));
	g_return_if_fail (name != NULL);
	
	header = g_mime_header_new (headers->options, name, value, name, NULL, charset, -1);
	g_mime_event_add (header->changed, (GMimeEventCallback) header_changed, headers);
	g_ptr_array_add (headers->array, header);
	
	if (!g_hash_table_lookup (headers->hash, name))
		g_hash_table_insert (headers->hash, header->name, header);
	
	args.action = GMIME_HEADER_LIST_CHANGED_ACTION_ADDED;
	args.header = header;
	
	g_mime_event_emit (headers->changed, &args);
}


/**
 * g_mime_header_list_get_header:
 * @headers: a #GMimeHeaderList
 * @name: header name
 *
 * Gets the first header with the specified name.
 *
 * Returns: (transfer none): a #GMimeHeader for the specified @name.
 **/
GMimeHeader *
g_mime_header_list_get_header (GMimeHeaderList *headers, const char *name)
{
	g_return_val_if_fail (GMIME_IS_HEADER_LIST (headers), NULL);
	g_return_val_if_fail (name != NULL, NULL);
	
	return g_hash_table_lookup (headers->hash, name);
}


void
_g_mime_header_list_set (GMimeHeaderList *headers, const char *name, const char *raw_value)
{
	GMimeHeaderListChangedEventArgs args;
	GMimeHeader *header, *hdr;
	guint i;
	
	g_return_if_fail (GMIME_IS_HEADER_LIST (headers));
	g_return_if_fail (name != NULL);
	
	if ((header = g_hash_table_lookup (headers->hash, name))) {
		g_mime_header_set_raw_value (header, raw_value);
		
		for (i = headers->array->len - 1; i > 0; i--) {
			hdr = (GMimeHeader *) headers->array->pdata[i];
			
			if (hdr == header)
				break;
			
			if (g_ascii_strcasecmp (header->name, hdr->name) != 0)
				continue;
			
			g_mime_event_remove (hdr->changed, (GMimeEventCallback) header_changed, headers);
			g_ptr_array_remove_index (headers->array, i);
			g_object_unref (hdr);
		}
		
		args.action = GMIME_HEADER_LIST_CHANGED_ACTION_CHANGED;
		args.header = header;
		
		g_mime_event_emit (headers->changed, &args);
	} else {
		_g_mime_header_list_append (headers, name, name, raw_value, -1);
	}
}


/**
 * g_mime_header_list_set:
 * @headers: a #GMimeHeaderList
 * @name: header name
 * @value: header value
 * @charset: a charset
 *
 * Set the value of the specified header. If @value is %NULL and the
 * header, @name, had not been previously set, a space will be set
 * aside for it (useful for setting the order of headers before values
 * can be obtained for them) otherwise the header will be unset.
 *
 * Note: If there are multiple headers with the specified field name,
 * the first instance of the header will be replaced and further
 * instances will be removed.
 **/
void
g_mime_header_list_set (GMimeHeaderList *headers, const char *name, const char *value, const char *charset)
{
	GMimeHeaderListChangedEventArgs args;
	GMimeHeader *header, *hdr;
	guint i;
	
	g_return_if_fail (GMIME_IS_HEADER_LIST (headers));
	g_return_if_fail (name != NULL);
	
	if ((header = g_hash_table_lookup (headers->hash, name))) {
		g_mime_header_set_value (header, NULL, value, charset);
		
		for (i = headers->array->len - 1; i > 0; i--) {
			hdr = (GMimeHeader *) headers->array->pdata[i];
			
			if (hdr == header)
				break;
			
			if (g_ascii_strcasecmp (header->name, hdr->name) != 0)
				continue;
			
			g_mime_event_remove (hdr->changed, (GMimeEventCallback) header_changed, headers);
			g_ptr_array_remove_index (headers->array, i);
			g_object_unref (hdr);
		}
		
		args.action = GMIME_HEADER_LIST_CHANGED_ACTION_CHANGED;
		args.header = header;
		
		g_mime_event_emit (headers->changed, &args);
	} else {
		g_mime_header_list_append (headers, name, value, charset);
	}
}


/**
 * g_mime_header_list_get_header_at:
 * @headers: a #GMimeHeaderList
 * @index: the 0-based index of the header
 *
 * Gets the header at the specified @index within the list.
 *
 * Returns: (transfer none): the header at position @index.
 **/
GMimeHeader *
g_mime_header_list_get_header_at (GMimeHeaderList *headers, int index)
{
	g_return_val_if_fail (GMIME_IS_HEADER_LIST (headers), NULL);
	g_return_val_if_fail (index >= 0, NULL);
	
	if ((guint) index >= headers->array->len)
		return NULL;
	
	return (GMimeHeader *) headers->array->pdata[index];
}


/**
 * g_mime_header_list_remove:
 * @headers: a #GMimeHeaderList
 * @name: header name
 *
 * Remove the first instance of the specified header.
 *
 * Returns: %TRUE if the header was successfully removed or %FALSE if
 * the specified header could not be found.
 **/
gboolean
g_mime_header_list_remove (GMimeHeaderList *headers, const char *name)
{
	GMimeHeaderListChangedEventArgs args;
	GMimeHeader *header, *hdr;
	guint i;
	
	g_return_val_if_fail (GMIME_IS_HEADER_LIST (headers), FALSE);
	g_return_val_if_fail (name != NULL, FALSE);
	
	if (!(header = g_hash_table_lookup (headers->hash, name)))
		return FALSE;
	
	/* get the index of the header */
	for (i = 0; i < headers->array->len; i++) {
		if (headers->array->pdata[i] == header)
			break;
	}
	
	g_mime_event_remove (header->changed, (GMimeEventCallback) header_changed, headers);
	g_ptr_array_remove_index (headers->array, i);
	g_hash_table_remove (headers->hash, name);
	
	/* look for another header with the same name... */
	while (i < headers->array->len) {
		hdr = (GMimeHeader *) headers->array->pdata[i];
		
		if (!g_ascii_strcasecmp (hdr->name, name)) {
			/* enter this node into the lookup table */
			g_hash_table_insert (headers->hash, hdr->name, hdr);
			break;
		}
		
		i++;
	}
	
	args.action = GMIME_HEADER_LIST_CHANGED_ACTION_REMOVED;
	args.header = header;
	
	g_mime_event_emit (headers->changed, &args);
	g_object_unref (header);
	
	return TRUE;
}


/**
 * g_mime_header_list_remove_at:
 * @headers: a #GMimeHeaderList
 * @index: the 0-based index of the header to remove
 *
 * Removes the header at the specified @index from @headers.
 **/
void
g_mime_header_list_remove_at (GMimeHeaderList *headers, int index)
{
	GMimeHeaderListChangedEventArgs args;
	GMimeHeader *header, *hdr;
	guint i;
	
	g_return_if_fail (GMIME_IS_HEADER_LIST (headers));
	g_return_if_fail (index >= 0);
	
	if ((guint) index >= headers->array->len)
		return;
	
	header = (GMimeHeader *) headers->array->pdata[index];
	g_mime_event_remove (header->changed, (GMimeEventCallback) header_changed, headers);
	g_ptr_array_remove_index (headers->array, index);
	
	/* if this is the first instance of a header with this name, then we'll
	 * need to update the hash table to point to the next instance... */
	if ((hdr = g_hash_table_lookup (headers->hash, header->name)) == header) {
		g_hash_table_remove (headers->hash, header->name);
		
		for (i = (guint) index; i < headers->array->len; i++) {
			hdr = (GMimeHeader *) headers->array->pdata[i];
			
			if (!g_ascii_strcasecmp (header->name, hdr->name)) {
				g_hash_table_insert (headers->hash, hdr->name, hdr);
				break;
			}
		}
	}
	
	args.action = GMIME_HEADER_LIST_CHANGED_ACTION_REMOVED;
	args.header = header;
	
	g_mime_event_emit (headers->changed, &args);
	g_object_unref (header);
}


/**
 * g_mime_header_list_write_to_stream:
 * @headers: a #GMimeHeaderList
 * @options: (nullable): a #GMimeFormatOptions or %NULL
 * @stream: output stream
 *
 * Write the headers to a stream.
 *
 * Returns: the number of bytes written or %-1 on fail.
 **/
ssize_t
g_mime_header_list_write_to_stream (GMimeHeaderList *headers, GMimeFormatOptions *options, GMimeStream *stream)
{
	ssize_t nwritten, total = 0;
	GMimeStream *filtered;
	GMimeHeader *header;
	GMimeFilter *filter;
	guint i;
	
	g_return_val_if_fail (GMIME_IS_HEADER_LIST (headers), -1);
	g_return_val_if_fail (GMIME_IS_STREAM (stream), -1);
	
	filtered = g_mime_stream_filter_new (stream);
	filter = g_mime_format_options_create_newline_filter (options, FALSE);
	g_mime_stream_filter_add ((GMimeStreamFilter *) filtered, filter);
	g_object_unref (filter);
	
	for (i = 0; i < headers->array->len; i++) {
		header = (GMimeHeader *) headers->array->pdata[i];
		
		if (!g_mime_format_options_is_hidden_header (options, header->name)) {
			if ((nwritten = g_mime_header_write_to_stream (header, options, filtered)) == -1)
				return -1;
			
			total += nwritten;
		}
	}
	
	g_mime_stream_flush (filtered);
	g_object_unref (filtered);
	
	return total;
}


/**
 * g_mime_header_list_to_string:
 * @headers: a #GMimeHeaderList
 * @options: (nullable): a #GMimeFormatOptions or %NULL
 *
 * Allocates a string buffer containing the raw rfc822 headers
 * contained in @headers.
 *
 * Returns: a string containing the header block.
 **/
char *
g_mime_header_list_to_string (GMimeHeaderList *headers, GMimeFormatOptions *options)
{
	GMimeStream *stream;
	GByteArray *array;
	char *str;
	
	g_return_val_if_fail (GMIME_IS_HEADER_LIST (headers), NULL);
	
	array = g_byte_array_new ();
	stream = g_mime_stream_mem_new ();
	g_mime_stream_mem_set_byte_array ((GMimeStreamMem *) stream, array);
	g_mime_header_list_write_to_stream (headers, options, stream);
	g_object_unref (stream);
	
	g_byte_array_append (array, (unsigned char *) "", 1);
	str = (char *) array->data;
	g_byte_array_free (array, FALSE);
	
	return str;
}
