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

#include <string.h>

#include "gmime-error.h"
#include "gmime-tnef-message-private.h"

#define ATTR(name) GMIME_TNEF_ATTRIBUTE_TAG_##name
#define TAG(name) GMIME_TNEF_PROPERTY_TAG_##name
#define TYPE(name) GMIME_TNEF_PROPERTY_TYPE_##name
#define BODY(name) GMIME_TNEF_MESSAGE_BODY_FORMAT_##name

typedef struct {
	GMimeTnefPropertySet *set;
	GHashTable *legacy;
} PropertyBuilder;

typedef struct {
	char *name, *type, *email;
} Address;

typedef struct {
	GMimeTnefReader *reader;
	GMimeTnefMessage *message;
	GMimeTnefAttachment *attachment;
	PropertyBuilder message_props, attach_props;
	gboolean seen_body[3], attachment_limit;
	gboolean mapi_data, embedded, object;
	gint64 data_offset;
	Address owner;
} Loader;

static void
add_mapi (PropertyBuilder *builder, GMimeTnefProperty *p)
{
	gpointer key = GUINT_TO_POINTER (g_mime_tnef_property_tag_get_id (p->tag));

	/* A populated MAPI property supersedes its legacy attribute
	 * ([MS-OXTNEF] 2.1.3.4). An empty property is not a replacement. */
	if (builder->legacy && !p->name && p->values->len) {
		GMimeTnefProperty *old = g_hash_table_lookup (builder->legacy, key);

		if (old) {
			g_hash_table_remove (builder->legacy, key);
			g_ptr_array_remove (builder->set->items, old);
		}
	}

	g_ptr_array_add (builder->set->items, p);
}

static void
add_legacy (PropertyBuilder *builder, GMimeTnefPropertyTag tag, GValue *value)
{
	GMimeTnefProperty *prop;

	if (g_mime_tnef_property_set_lookup (builder->set, tag)) {
		g_value_unset (value);
		return;
	}

	prop = _g_mime_tnef_property_new (tag, NULL);
	g_array_append_val (prop->values, *value);
	g_ptr_array_add (builder->set->items, prop);

	if (!builder->legacy)
		builder->legacy = g_hash_table_new (g_direct_hash, g_direct_equal);

	g_hash_table_insert (builder->legacy, GUINT_TO_POINTER (g_mime_tnef_property_tag_get_id (tag)), prop);
}

static void
add_string (PropertyBuilder *builder, GMimeTnefPropertyTag tag, const char *text)
{
	GValue v = G_VALUE_INIT;

	if (text && *text) {
		g_value_init (&v, G_TYPE_STRING);
		g_value_set_string (&v, text);
		add_legacy (builder, tag, &v);
	}
}

static void
add_int (PropertyBuilder *builder, GMimeTnefPropertyTag tag, gint32 value)
{
	GValue v = G_VALUE_INIT;

	if (g_mime_tnef_property_tag_get_value_type (tag) == TYPE (BOOLEAN)) {
		g_value_init (&v, G_TYPE_BOOLEAN);
		g_value_set_boolean (&v, value != 0);
	} else {
		g_value_init (&v, G_TYPE_INT);
		g_value_set_int (&v, value);
	}

	add_legacy (builder, tag, &v);
}

static void
add_bytes (PropertyBuilder *builder, GMimeTnefPropertyTag tag, GBytes *bytes)
{
	GValue v = G_VALUE_INIT;

	if (g_bytes_get_size (bytes)) {
		g_value_init (&v, G_TYPE_BYTES);
		g_value_set_boxed (&v, bytes);
		add_legacy (builder, tag, &v);
	}
}

static void
invalid_attribute (Loader *loader)
{
	_g_mime_tnef_reader_log_attribute (loader->reader, GMIME_TNEF_COMPLIANCE_VIOLATION_INVALID_ATTRIBUTE_VALUE);
}

static char *
decode_string (Loader *loader, GBytes *bytes)
{
	char *charset = _g_mime_tnef_get_charset (g_mime_tnef_reader_get_codepage (loader->reader));
	char *text = _g_mime_tnef_decode_string (charset, bytes, FALSE);

	g_free (charset);

	return text;
}

static const char *
skip_space (const char *inptr)
{
	while (*inptr && g_unichar_isspace (g_utf8_get_char (inptr)))
		inptr = g_utf8_next_char (inptr);

	return inptr;
}

static char *
trim_class (char *text)
{
	const char *start = skip_space (text), *end = start + strlen (start);

	while (end > start) {
		const char *prev = g_utf8_prev_char (end);

		if (!g_unichar_isspace (g_utf8_get_char (prev)))
			break;

		end = prev;
	}

	memmove (text, start, end - start);
	text[end - start] = '\0';

	return text;
}

static const char *
translate_class (const char *name)
{
	static const struct { const char *old_name, *new_name; } classes[] = {
		{ "IPM.Microsoft Mail.Note", "IPM.Note" },
		{ "IPM.Microsoft Mail.Read Receipt", "Report.IPM.Note.IPNRN" },
		{ "IPM.Microsoft Mail.Non-Delivery", "Report.IPM.Note.NDR" },
		{ "IPM.Microsoft Schedule.MtgRespP", "IPM.Schedule.Meeting.Resp.Pos" },
		{ "IPM.Microsoft Schedule.MtgRespN", "IPM.Schedule.Meeting.Resp.Neg" },
		{ "IPM.Microsoft Schedule.MtgRespA", "IPM.Schedule.Meeting.Resp.Tent" },
		{ "IPM.Microsoft Schedule.MtgReq", "IPM.Schedule.Meeting.Request" },
		{ "IPM.Microsoft Schedule.MtgCncl", "IPM.Schedule.Meeting.Canceled" }
	};
	const char prefix[] = "Microsoft Mail v3.0 ";
	const char *key = g_str_has_prefix (name, prefix) ? name + sizeof prefix - 1 : name;
	guint i;

	for (i = 0; i < G_N_ELEMENTS (classes); i++) {
		if (!strcmp (key, classes[i].old_name))
			return classes[i].new_name;
	}

	return name;
}

static void
clear_address (Address *address)
{
	g_free (address->name);
	g_free (address->type);
	g_free (address->email);
	memset (address, 0, sizeof *address);
}

static void
apply_address (PropertyBuilder *builder, Address *address, GMimeTnefPropertyTag name_tag,
	       GMimeTnefPropertyTag type_tag, GMimeTnefPropertyTag email_tag, GMimeTnefPropertyTag entry_tag)
{
	/* One-off EntryID: flags, provider UID, version, Unicode flag, and
	 * three nul-terminated UTF-16LE strings ([MS-OXCDATA] 2.2.5.1). */
	static const guint8 prefix[] = { 0, 0, 0, 0, 0x81, 0x2b, 0x1f, 0xa4, 0xbe, 0xa3, 0x10, 0x19,
					0x9d, 0x6e, 0, 0xdd, 1, 0x0f, 0x54, 2, 0, 0, 0, 0x80 };
	const char *fields[] = { address->name, address->type, address->email };
	GByteArray *data = g_byte_array_new ();
	GBytes *entry;
	guint i;

	add_string (builder, name_tag, address->name);
	add_string (builder, type_tag, address->type);
	add_string (builder, email_tag, address->email);

	g_byte_array_append (data, prefix, sizeof prefix);

	for (i = 0; i < G_N_ELEMENTS (fields); i++) {
		const char *s = fields[i] ? fields[i] : "";

		while (*s) {
			gunichar c = g_utf8_get_char (s);
			guint8 unit[2];

			if (c > 0xffff) {
				c -= 0x10000;
				_g_mime_tnef_put_uint16 (unit, 0xd800 | (c >> 10));
				g_byte_array_append (data, unit, 2);
				c = 0xdc00 | (c & 0x3ff);
			}

			_g_mime_tnef_put_uint16 (unit, (guint16) c);
			g_byte_array_append (data, unit, 2);
			s = g_utf8_next_char (s);
		}

		g_byte_array_append (data, (const guint8 *) "\0", 2);
	}

	entry = g_byte_array_free_to_bytes (data);
	add_bytes (builder, entry_tag, entry);
	g_bytes_unref (entry);
}

static void
read_address (Loader *loader, GBytes *bytes, gboolean owner)
{
	gsize length, name_start, email_start, name_length, email_length;
	const guint8 *data = g_bytes_get_data (bytes, &length);
	Address address = { 0 };
	char *full, *colon;
	GBytes *slice;

	/* attOwner uses two length-prefixed strings; attFrom starts with
	 * a one-off TRP descriptor ([MS-OXTNEF] 2.1.3.3.1). */
	if (owner) {
		if (length < 2)
			goto invalid;

		name_start = 2;
		name_length = _g_mime_tnef_get_uint16 (data);
		email_start = 4 + name_length;

		if (email_start > length)
			goto invalid;

		email_length = _g_mime_tnef_get_uint16 (data + email_start - 2);
	} else {
		if (length < 8 || _g_mime_tnef_get_uint16 (data) != 4)
			goto invalid;

		name_start = 8;
		name_length = _g_mime_tnef_get_uint16 (data + 4);
		email_start = 8 + name_length;
		email_length = _g_mime_tnef_get_uint16 (data + 6);
	}

	if (email_start + email_length > length)
		goto invalid;

	slice = g_bytes_new_from_bytes (bytes, name_start, name_length);
	address.name = decode_string (loader, slice);
	g_bytes_unref (slice);

	slice = g_bytes_new_from_bytes (bytes, email_start, email_length);
	full = decode_string (loader, slice);
	g_bytes_unref (slice);

	colon = strchr (full, ':');

	if (colon && colon != full) {
		address.type = g_strndup (full, colon - full);
		address.email = g_strdup (colon + 1);
		g_free (full);
	} else {
		address.email = full;
	}

	if (owner) {
		clear_address (&loader->owner);
		loader->owner = address;
	} else {
		apply_address (&loader->message_props, &address, TAG (SENDER_NAME_W), TAG (SENDER_ADDRTYPE_W),
			       TAG (SENDER_EMAIL_ADDRESS_W), TAG (SENDER_ENTRY_ID));
		clear_address (&address);
	}

	return;

invalid:
	invalid_attribute (loader);
}

static void
read_search_key (Loader *loader, GBytes *bytes)
{
	gsize length, i;
	const guint8 *data = g_bytes_get_data (bytes, &length);
	GBytes *result;
	guint8 *key;

	while (length && !data[length - 1])
		length--;

	if (!length)
		return;

	if (length & 1) {
		invalid_attribute (loader);
		return;
	}

	key = g_malloc (length / 2);

	for (i = 0; i < length; i += 2) {
		int hi = g_ascii_xdigit_value (data[i]), lo = g_ascii_xdigit_value (data[i + 1]);

		if (hi < 0 || lo < 0) {
			invalid_attribute (loader);
			g_free (key);
			return;
		}

		key[i / 2] = (hi << 4) | lo;
	}

	result = g_bytes_new_take (key, length / 2);

	add_bytes (&loader->message_props, TAG (SEARCH_KEY), result);
	g_bytes_unref (result);
}

static void
read_render_data (Loader *loader, GBytes *bytes)
{
	static const guint8 ole[] = { 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x14, 3, 0x0a, 3, 2, 1 };
	static const guint8 mac[] = { 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x14, 3, 0x0b, 1 };
	gsize length;
	const guint8 *data = g_bytes_get_data (bytes, &length);

	if (length < 14) {
		invalid_attribute (loader);
		return;
	}

	add_int (&loader->attach_props, TAG (RENDERING_POSITION), (gint32) _g_mime_tnef_get_uint32 (data + 2));

	if (_g_mime_tnef_get_uint16 (data) == 2) {
		GBytes *value = g_bytes_new_static (ole, sizeof ole);
		add_bytes (&loader->attach_props, TAG (ATTACH_TAG), value);
		g_bytes_unref (value);
	}

	if (_g_mime_tnef_get_uint32 (data + 10) & 1) {
		GBytes *value = g_bytes_new_static (mac, sizeof mac);
		add_bytes (&loader->attach_props, TAG (ATTACH_ENCODING), value);
		g_bytes_unref (value);
	}
}

static int
get_body_format (GMimeTnefPropertyTag tag)
{
	GMimeTnefPropertyType type = g_mime_tnef_property_tag_get_value_type (tag);
	gboolean string = type == TYPE (STRING8) || type == TYPE (UNICODE);

	if (!g_mime_tnef_property_tag_is_multi_valued (tag)) {
		switch (g_mime_tnef_property_tag_get_id (tag)) {
		case GMIME_TNEF_PROPERTY_ID_BODY: if (string) return BODY (TEXT); break;
		case GMIME_TNEF_PROPERTY_ID_BODY_HTML: if (string || type == TYPE (BINARY)) return BODY (HTML); break;
		case GMIME_TNEF_PROPERTY_ID_RTF_COMPRESSED: if (type == TYPE (BINARY)) return BODY (COMPRESSED_RTF); break;
		}
	}

	return -1;
}

static void
set_body (Loader *loader, int format, GMimeTnefPropertyTag tag, GBytes *bytes)
{
	GMimeTnefMessageBody *body = g_object_new (GMIME_TYPE_TNEF_MESSAGE_BODY, NULL);
	GMimeTnefPropertyType type = g_mime_tnef_property_tag_get_value_type (tag);
	gsize length;
	const guint8 *data = g_bytes_get_data (bytes, &length);

	if (type == TYPE (UNICODE) && !(length & 1)) {
		while (length >= 2 && !data[length - 1] && !data[length - 2])
			length -= 2;
	} else if (type == TYPE (STRING8)) {
		while (length && !data[length - 1])
			length--;
	}

	body->content = length == g_bytes_get_size (bytes) ? g_bytes_ref (bytes) : g_bytes_new_from_bytes (bytes, 0, length);
	body->format = format;
	body->tag = tag;

	g_clear_object (&loader->message->bodies[format]);
	loader->message->bodies[format] = body;
}

static gboolean
is_attach_data (GMimeTnefPropertyTag tag)
{
	GMimeTnefPropertyType type = g_mime_tnef_property_tag_get_value_type (tag);

	return !g_mime_tnef_property_tag_is_multi_valued (tag) &&
		g_mime_tnef_property_tag_get_id (tag) == GMIME_TNEF_PROPERTY_ID_ATTACH_DATA &&
		(type == TYPE (OBJECT) || type == TYPE (BINARY));
}

static void
read_properties (Loader *loader, gboolean attachment, GError **error)
{
	GMimeTnefPropertyReader *reader = g_mime_tnef_reader_get_property_reader (loader->reader, error);

	if (!reader)
		return;

	while (!*error && g_mime_tnef_property_reader_read_next_property (reader, error)) {
		GMimeTnefPropertyTag tag = g_mime_tnef_property_reader_get_tag (reader);
		gboolean has_value = g_mime_tnef_property_reader_get_value_count (reader) > 0;
		int format = attachment ? -1 : get_body_format (tag);

		if (has_value && format >= 0) {
			if (!loader->seen_body[format]) {
				GBytes *bytes = g_mime_tnef_property_reader_read_value_as_bytes (reader, error);

				loader->seen_body[format] = TRUE;

				if (bytes && !_g_mime_tnef_reader_value_skipped (loader->reader))
					set_body (loader, format, tag, bytes);

				g_clear_pointer (&bytes, g_bytes_unref);
			}
		} else if (has_value && attachment && is_attach_data (tag)) {
			if (!loader->mapi_data) {
				gboolean embedded = g_mime_tnef_property_reader_is_embedded_message (reader);
				gint64 offset = _g_mime_tnef_reader_absolute_offset (loader->reader);
				GBytes *bytes = g_mime_tnef_property_reader_read_value_as_bytes (reader, error);

				if (bytes && !_g_mime_tnef_reader_value_skipped (loader->reader)) {
					g_clear_pointer (&loader->attachment->content, g_bytes_unref);
					loader->attachment->content = g_bytes_ref (bytes);
					loader->data_offset = offset;
					loader->mapi_data = TRUE;
					loader->embedded = embedded;
					loader->object = g_mime_tnef_property_tag_get_value_type (tag) == TYPE (OBJECT);
				}

				g_clear_pointer (&bytes, g_bytes_unref);
			}
		} else {
			GMimeTnefProperty *prop = g_mime_tnef_property_reader_read_property (reader, error);

			if (prop)
				add_mapi (attachment ? &loader->attach_props : &loader->message_props, prop);
		}
	}

	g_object_unref (reader);
}

static void
finish_attachment (Loader *loader)
{
	GMimeTnefAttachment *attachment = loader->attachment;

	if (!attachment)
		return;

	if (loader->mapi_data) {
		if (g_mime_tnef_attachment_get_method (attachment) == GMIME_TNEF_ATTACH_METHOD_EMBEDDED_MESSAGE)
			loader->embedded = TRUE;

		if (loader->embedded || loader->object) {
			gsize len = g_bytes_get_size (attachment->content);
			gsize prefix = MIN (len, 16);
			GBytes *payload = g_bytes_new_from_bytes (attachment->content, prefix, len - prefix);

			g_bytes_unref (attachment->content);
			attachment->content = payload;
		}

		/* An Object value starts with a 16-byte interface IID. The
		 * embedded message's offsets start after it ([MS-OXTNEF] 2.1.3.5.1). */
		if (loader->embedded)
			attachment->embedded = _g_mime_tnef_reader_capture (loader->reader, loader->data_offset + 16);
	}

	g_clear_pointer (&loader->attach_props.legacy, g_hash_table_unref);

	loader->attachment = NULL;
	loader->mapi_data = loader->embedded = loader->object = FALSE;
}

static gboolean
begin_attachment (Loader *loader)
{
	finish_attachment (loader);

	if (loader->attachment_limit)
		return FALSE;

	if (loader->message->attachments->len >= g_mime_tnef_reader_get_options (loader->reader)->max_attachments) {
		_g_mime_tnef_reader_log_attribute (loader->reader, GMIME_TNEF_COMPLIANCE_VIOLATION_TOO_MANY_ATTACHMENTS);
		loader->attachment_limit = TRUE;
		return FALSE;
	}

	loader->attachment = g_object_new (GMIME_TYPE_TNEF_ATTACHMENT, NULL);
	loader->attach_props.set = loader->attachment->properties;
	g_ptr_array_add (loader->message->attachments, loader->attachment);

	return TRUE;
}

static void
read_recipients (Loader *loader, GError **error)
{
	GMimeTnefPropertyReader *reader = g_mime_tnef_reader_get_property_reader (loader->reader, error);

	if (!reader)
		return;

	while (!*error && g_mime_tnef_property_reader_read_next_row (reader, error)) {
		GMimeTnefPropertySet *row = g_mime_tnef_property_reader_read_property_set (reader, error);

		if (row) {
			GMimeTnefRecipient *recipient = g_object_new (GMIME_TYPE_TNEF_RECIPIENT, NULL);
			recipient->properties = row;
			g_ptr_array_add (loader->message->recipients, recipient);
		}
	}

	g_object_unref (reader);
}

static void
read_date (Loader *loader, PropertyBuilder *builder, GMimeTnefPropertyTag tag, GError **error)
{
	GDateTime *date = g_mime_tnef_reader_read_value_as_date_time (loader->reader, error);
	GValue v = G_VALUE_INIT;

	if (!date)
		return;

	if (g_date_time_to_unix (date) == G_GINT64_CONSTANT (-62135596800)) {
		g_date_time_unref (date);
		return;
	}

	g_value_init (&v, G_TYPE_DATE_TIME);
	g_value_take_boxed (&v, date);
	add_legacy (builder, tag, &v);
}

static void
read_attribute (Loader *loader, GError **error)
{
	GMimeTnefReader *reader = loader->reader;
	GMimeTnefAttributeTag tag = g_mime_tnef_reader_get_tag (reader);
	PropertyBuilder *builder = &loader->message_props;
	GMimeTnefPropertyTag dest = 0;
	GBytes *bytes;
	char *text;
	gint32 number;

	if (_g_mime_tnef_get_attribute_level (tag) == GMIME_TNEF_ATTRIBUTE_LEVEL_ATTACHMENT) {
		if ((tag == ATTR (ATTACH_RENDER_DATA) || !loader->attachment) && !begin_attachment (loader))
			return;

		builder = &loader->attach_props;
	}

	switch (tag) {
	case ATTR (MAPI_PROPERTIES): read_properties (loader, FALSE, error); return;
	case ATTR (ATTACHMENT): read_properties (loader, TRUE, error); return;
	case ATTR (RECIPIENT_TABLE): read_recipients (loader, error); return;
	case ATTR (DATE_SENT): read_date (loader, builder, TAG (CLIENT_SUBMIT_TIME), error); return;
	case ATTR (DATE_RECEIVED): read_date (loader, builder, TAG (MESSAGE_DELIVERY_TIME), error); return;
	case ATTR (DATE_MODIFIED):
	case ATTR (ATTACH_MODIFY_DATE): read_date (loader, builder, TAG (LAST_MODIFICATION_TIME), error); return;
	case ATTR (DATE_START): read_date (loader, builder, TAG (START_DATE), error); return;
	case ATTR (DATE_END): read_date (loader, builder, TAG (END_DATE), error); return;
	case ATTR (ATTACH_CREATE_DATE): read_date (loader, builder, TAG (CREATION_TIME), error); return;
	case ATTR (REQUEST_RESPONSE):
	case ATTR (PRIORITY):
	case ATTR (AID_OWNER):
		if (!g_mime_tnef_reader_read_value_as_int32 (reader, &number, error))
			return;

		if (tag == ATTR (AID_OWNER))
			add_int (builder, TAG (OWNER_APPT_ID), number);
		else if (tag == ATTR (REQUEST_RESPONSE))
			add_int (builder, TAG (RESPONSE_REQUESTED), number & 0xff);
		else if (number >= 1 && number <= 3)
			add_int (builder, TAG (IMPORTANCE), 3 - number);
		else
			invalid_attribute (loader);

		return;
	case ATTR (BODY):
		if (loader->message->bodies[BODY (TEXT)])
			return;
		break;
	case ATTR (ATTACH_DATA):
		if (loader->attachment->content)
			return;
		break;
	case ATTR (ORIGINAL_MESSAGE_CLASS): dest = TAG (ORIG_MESSAGE_CLASS_W); break;
	case ATTR (MESSAGE_CLASS): dest = TAG (MESSAGE_CLASS_W); break;
	case ATTR (SUBJECT): dest = TAG (SUBJECT_W); break;
	case ATTR (ATTACH_TITLE): dest = TAG (ATTACH_LONG_FILENAME_W); break;
	case ATTR (ATTACH_TRANSPORT_FILENAME): dest = TAG (ATTACH_TRANSPORT_NAME_W); break;
	case ATTR (DELEGATE):
	case ATTR (ATTACH_META_FILE):
	case ATTR (ATTACH_RENDER_DATA):
	case ATTR (FROM):
	case ATTR (OWNER):
	case ATTR (MESSAGE_STATUS):
	case ATTR (MESSAGE_ID): break;
	default: return;
	}

	bytes = g_mime_tnef_reader_read_value_as_bytes (reader, error);

	if (!bytes)
		return;

	if (!_g_mime_tnef_reader_value_skipped (reader)) {
		switch (tag) {
		case ATTR (BODY): set_body (loader, BODY (TEXT), TAG (BODY_A), bytes); break;
		case ATTR (ATTACH_DATA): loader->attachment->content = g_bytes_ref (bytes); break;
		case ATTR (DELEGATE): add_bytes (builder, TAG (RCVD_REPRESENTING_ENTRY_ID), bytes); break;
		case ATTR (ATTACH_META_FILE): add_bytes (builder, TAG (ATTACH_RENDERING), bytes); break;
		case ATTR (ATTACH_RENDER_DATA): read_render_data (loader, bytes); break;
		case ATTR (FROM):
		case ATTR (OWNER): read_address (loader, bytes, tag == ATTR (OWNER)); break;
		/* attMessageID is a hexadecimal PidTagSearchKey, not an Internet Message-ID. */
		case ATTR (MESSAGE_ID): read_search_key (loader, bytes); break;
		case ATTR (MESSAGE_STATUS):
			if (!g_bytes_get_size (bytes)) {
				invalid_attribute (loader);
			} else {
				guint8 status = *(const guint8 *) g_bytes_get_data (bytes, NULL);
				guint flags = ((status & 0x20) >> 5) | ((~status & 1) << 1) | (status & 4) |
					((status & 2) << 2) | ((status & 0x80) >> 3);
				add_int (builder, TAG (MESSAGE_FLAGS), flags);
			}
			break;
		default:
			text = decode_string (loader, bytes);

			if (tag == ATTR (MESSAGE_CLASS) || tag == ATTR (ORIGINAL_MESSAGE_CLASS))
				add_string (builder, dest, translate_class (trim_class (text)));
			else
				add_string (builder, dest, text);

			g_free (text);
			break;
		}
	}

	g_bytes_unref (bytes);
}

static void
complete_message (Loader *loader)
{
	GMimeTnefMessage *message = loader->message;
	int internet_cp = _g_mime_tnef_set_int (message->properties, TAG (INTERNET_CODEPAGE), 0);
	const char *name = _g_mime_tnef_set_string (message->properties, TAG (MESSAGE_CLASS_W));
	guint i;

	finish_attachment (loader);
	/* Without attOemCodepage, PidTagInternetCodepage can follow the body.
	 * Resolve body encodings only once it is known ([MS-OXTNEF] 2.3.3.2). */
	message->codepage = g_mime_tnef_reader_get_codepage (loader->reader);
	message->legacy_key = g_mime_tnef_reader_get_legacy_key (loader->reader);

	if (name) {
		message->message_class = trim_class (g_strdup (name));

		if (!*message->message_class)
			g_clear_pointer (&message->message_class, g_free);
	}

	/* attOwner denotes the delegator on a meeting response. Defer this
	 * mapping until we know the final message class. */
	if (loader->owner.name) {
		const char response[] = "IPM.Schedule.Meeting.Resp.";

		if (message->message_class && !g_ascii_strncasecmp (message->message_class, response, sizeof response - 1))
			apply_address (&loader->message_props, &loader->owner, TAG (RCVD_REPRESENTING_NAME_W),
				       TAG (RCVD_REPRESENTING_ADDRTYPE_W), TAG (RCVD_REPRESENTING_EMAIL_ADDRESS_W), TAG (RCVD_REPRESENTING_ENTRY_ID));
		else
			apply_address (&loader->message_props, &loader->owner, TAG (SENT_REPRESENTING_NAME_W),
				       TAG (SENT_REPRESENTING_ADDRTYPE_W), TAG (SENT_REPRESENTING_EMAIL_ADDRESS_W), TAG (SENT_REPRESENTING_ENTRY_ID));
	}

	for (i = 0; i < G_N_ELEMENTS (message->bodies); i++) {
		GMimeTnefMessageBody *body = message->bodies[i];

		if (!body)
			continue;

		body->fallback = _g_mime_tnef_get_charset (message->codepage);
		switch (g_mime_tnef_property_tag_get_value_type (body->tag)) {
		case TYPE (UNICODE): body->charset = g_strdup ("UTF-16LE"); break;
		case TYPE (STRING8): body->charset = g_strdup (body->fallback); break;
		default:
			if (i == BODY (HTML) && internet_cp > 0)
				body->charset = _g_mime_tnef_get_charset (internet_cp);
			break;
		}
	}
}

/**
 * g_mime_tnef_message_load_from_reader:
 * @reader: a TNEF reader which has not yet been advanced
 * @error: return location for an error
 *
 * Reads all attributes, reporting recoverable damage through @reader's
 * diagnostic callback. A bad signature or I/O error prevents loading.
 * The model does not retain the reader or its source stream.
 *
 * Returns: (transfer full) (nullable): a loaded message, or %NULL on error
 **/
GMimeTnefMessage *
g_mime_tnef_message_load_from_reader (GMimeTnefReader *reader, GError **error)
{
	Loader loader = { 0 };
	GError *fault = NULL;

	g_return_val_if_fail (GMIME_IS_TNEF_READER (reader), NULL);
	g_return_val_if_fail (!error || !*error, NULL);

	if (_g_mime_tnef_reader_started (reader)) {
		_g_mime_tnef_invalid_operation (error, "The reader has already been advanced.");
		return NULL;
	}

	loader.reader = reader;
	loader.message = g_object_new (GMIME_TYPE_TNEF_MESSAGE, NULL);
	loader.message_props.set = loader.message->properties;

	while (!fault && g_mime_tnef_reader_read (reader, &fault))
		read_attribute (&loader, &fault);

	if (!fault && _g_mime_tnef_reader_invalid_signature (reader))
		g_set_error_literal (&fault, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR, "The stream does not begin with the TNEF signature.");

	if (fault) {
		g_clear_object (&loader.message);
		g_propagate_error (error, fault);
	} else {
		complete_message (&loader);
	}

	clear_address (&loader.owner);
	g_clear_pointer (&loader.attach_props.legacy, g_hash_table_unref);
	g_clear_pointer (&loader.message_props.legacy, g_hash_table_unref);

	return loader.message;
}

/**
 * g_mime_tnef_message_load:
 * @stream: input, positioned at the TNEF signature
 * @options: (nullable): materialization and reader limits, or %NULL for defaults
 * @error: return location for an error
 *
 * Synchronously loads a message without seeking or closing @stream.
 *
 * Returns: (transfer full) (nullable): a loaded message, or %NULL on error
 **/
GMimeTnefMessage *
g_mime_tnef_message_load (GMimeStream *stream, const GMimeTnefOptions *options, GError **error)
{
	GMimeTnefMessage *message;
	GMimeTnefReader *reader;

	g_return_val_if_fail (GMIME_IS_STREAM (stream), NULL);
	g_return_val_if_fail (!error || !*error, NULL);

	reader = g_mime_tnef_reader_new (stream, options);
	message = g_mime_tnef_message_load_from_reader (reader, error);
	g_object_unref (reader);

	return message;
}
