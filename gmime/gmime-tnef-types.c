/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2026 Jeffrey Stedfast
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

#include <errno.h>
#include <math.h>
#include <string.h>

#include "gmime-error.h"
#include "gmime-internal.h"
#include "gmime-tnef-private.h"

G_DEFINE_BOXED_TYPE (GMimeTnefNameId, g_mime_tnef_name_id, g_mime_tnef_name_id_ref, g_mime_tnef_name_id_unref)

/**
 * g_mime_tnef_property_tag_new:
 * @id: a 16-bit property identifier
 * @type: a property type, optionally including the multi-valued flag
 *
 * Constructs a property tag.
 *
 * Returns: the tag
 **/
GMimeTnefPropertyTag
g_mime_tnef_property_tag_new (guint16 id, guint16 type)
{
	return ((guint32) id << 16) | (guint32) type;
}

/**
 * g_mime_tnef_property_tag_get_id:
 * @tag: a property tag
 *
 * Gets the identifier, including identifiers not defined by the enumeration.
 *
 * Returns: the property identifier
 **/
guint16
g_mime_tnef_property_tag_get_id (GMimeTnefPropertyTag tag)
{
	return (guint16) (tag >> 16);
}

/**
 * g_mime_tnef_property_tag_get_property_type:
 * @tag: a property tag
 *
 * Gets the type including the multi-valued flag.
 *
 * Returns: the property type
 **/
GMimeTnefPropertyType
g_mime_tnef_property_tag_get_property_type (GMimeTnefPropertyTag tag)
{
	return (GMimeTnefPropertyType) (tag & 0xffff);
}

/**
 * g_mime_tnef_property_tag_get_value_type:
 * @tag: a property tag
 *
 * Gets the type of each individual value.
 *
 * Returns: the type without the multi-valued flag
 **/
GMimeTnefPropertyType
g_mime_tnef_property_tag_get_value_type (GMimeTnefPropertyTag tag)
{
	return (GMimeTnefPropertyType) (tag & 0xefff);
}

/**
 * g_mime_tnef_property_tag_is_multi_valued:
 * @tag: a property tag
 *
 * Checks the multi-valued flag.
 *
 * Returns: whether the property is multi-valued
 **/
gboolean
g_mime_tnef_property_tag_is_multi_valued (GMimeTnefPropertyTag tag)
{
	return (tag & 0x1000) != 0;
}

/**
 * g_mime_tnef_property_tag_is_named:
 * @tag: a property tag
 *
 * Checks whether a tag uses the named-property identifier range.
 *
 * Returns: whether the property is named
 **/
gboolean
g_mime_tnef_property_tag_is_named (GMimeTnefPropertyTag tag)
{
	return (tag >> 16) >= 0x8000;
}

/**
 * g_mime_tnef_checksum_update:
 * @checksum: the running checksum, initially zero
 * @buffer: (array length=length): bytes to include
 * @length: the number of bytes
 *
 * Updates a TNEF checksum (the sum of bytes modulo 65536).
 *
 * Returns: the updated checksum
 **/
guint16
g_mime_tnef_checksum_update (guint16 checksum, const guint8 *buffer, gsize length)
{
	gsize index;

	g_return_val_if_fail (buffer != NULL || length == 0, checksum);

	for (index = 0; index < length; index++)
		checksum += buffer[index];

	return checksum;
}

/**
 * g_mime_tnef_name_id_new_with_id:
 * @property_set: a GUID in canonical hyphenated form
 * @id: a numeric name
 *
 * Creates a numeric named-property identity. GUIDs are case-insensitive.
 *
 * Returns: (transfer full): the identity
 **/
GMimeTnefNameId *
g_mime_tnef_name_id_new_with_id (const char *property_set, gint32 id)
{
	GMimeTnefNameId *name;
	char *lower;

	g_return_val_if_fail (property_set != NULL && g_uuid_string_is_valid (property_set), NULL);

	name = g_new0 (GMimeTnefNameId, 1);
	name->ref_count = 1;
	name->kind = GMIME_TNEF_NAME_ID_KIND_ID;
	name->id = id;
	lower = g_ascii_strdown (property_set, -1);
	memcpy (name->property_set, lower, 37);
	g_free (lower);

	return name;
}

/**
 * g_mime_tnef_name_id_new_with_name:
 * @property_set: a GUID in canonical hyphenated form
 * @text: a UTF-8 property name
 *
 * Creates a string named-property identity. Property names are case-sensitive.
 *
 * Returns: (transfer full): the identity
 **/
GMimeTnefNameId *
g_mime_tnef_name_id_new_with_name (const char *property_set, const char *text)
{
	GMimeTnefNameId *name;

	g_return_val_if_fail (text != NULL && g_utf8_validate (text, -1, NULL), NULL);

	if (!(name = g_mime_tnef_name_id_new_with_id (property_set, 0)))
		return NULL;

	name->kind = GMIME_TNEF_NAME_ID_KIND_NAME;
	name->name = g_strdup (text);

	return name;
}

/**
 * g_mime_tnef_name_id_ref:
 * @name: a named-property identity
 *
 * References an immutable identity.
 *
 * Returns: (transfer full): @name
 **/
GMimeTnefNameId *
g_mime_tnef_name_id_ref (GMimeTnefNameId *name)
{
	g_return_val_if_fail (name != NULL, NULL);

	g_atomic_int_inc (&name->ref_count);

	return name;
}

/**
 * g_mime_tnef_name_id_unref:
 * @name: a named-property identity
 *
 * Releases a reference.
 **/
void
g_mime_tnef_name_id_unref (GMimeTnefNameId *name)
{
	g_return_if_fail (name != NULL);

	if (g_atomic_int_dec_and_test (&name->ref_count)) {
		g_free (name->name);
		g_free (name);
	}
}

/**
 * g_mime_tnef_name_id_get_property_set:
 * @name: a named-property identity
 *
 * Gets the canonical GUID of the property set.
 *
 * Returns: (transfer none): the GUID
 **/
const char *
g_mime_tnef_name_id_get_property_set (GMimeTnefNameId *name)
{
	g_return_val_if_fail (name != NULL, NULL);

	return name->property_set;
}

/**
 * g_mime_tnef_name_id_get_kind:
 * @name: a named-property identity
 *
 * Gets the kind of name.
 *
 * Returns: the kind
 **/
GMimeTnefNameIdKind
g_mime_tnef_name_id_get_kind (GMimeTnefNameId *name)
{
	g_return_val_if_fail (name != NULL, GMIME_TNEF_NAME_ID_KIND_ID);

	return name->kind;
}

/**
 * g_mime_tnef_name_id_get_id:
 * @name: a numeric named-property identity
 *
 * Gets the numeric name.
 *
 * Returns: the numeric identifier
 **/
gint32
g_mime_tnef_name_id_get_id (GMimeTnefNameId *name)
{
	g_return_val_if_fail (name != NULL, 0);
	g_return_val_if_fail (name->kind == GMIME_TNEF_NAME_ID_KIND_ID, 0);

	return name->id;
}

/**
 * g_mime_tnef_name_id_get_name:
 * @name: a named-property identity
 *
 * Gets the string name.
 *
 * Returns: (transfer none) (nullable): the name, or %NULL for a numeric identity
 **/
const char *
g_mime_tnef_name_id_get_name (GMimeTnefNameId *name)
{
	g_return_val_if_fail (name != NULL, NULL);

	return name->name;
}

/**
 * g_mime_tnef_name_id_equal:
 * @name: the first identity
 * @other: the second identity
 *
 * Compares two named-property identities.
 *
 * Returns: whether they are equal
 **/
gboolean
g_mime_tnef_name_id_equal (const GMimeTnefNameId *name, const GMimeTnefNameId *other)
{
	g_return_val_if_fail (name != NULL && other != NULL, FALSE);

	return name->kind == other->kind && name->id == other->id &&
	       !strcmp (name->property_set, other->property_set) && !g_strcmp0 (name->name, other->name);
}

/**
 * g_mime_tnef_name_id_hash:
 * @name: a named-property identity
 *
 * Computes a hash for a named-property identity.
 *
 * Returns: the hash
 **/
guint
g_mime_tnef_name_id_hash (const GMimeTnefNameId *name)
{
	g_return_val_if_fail (name != NULL, 0);

	return g_str_hash (name->property_set) ^ (guint) name->id ^ (name->name ? g_str_hash (name->name) : 0) ^
	       (guint) name->kind;
}

guint16
_g_mime_tnef_get_uint16 (const guint8 *buffer)
{
	return (guint16) (buffer[0] | ((guint16) buffer[1] << 8));
}

guint32
_g_mime_tnef_get_uint32 (const guint8 *buffer)
{
	return (guint32) buffer[0] | ((guint32) buffer[1] << 8) | ((guint32) buffer[2] << 16) |
	       ((guint32) buffer[3] << 24);
}

guint64
_g_mime_tnef_get_uint64 (const guint8 *buffer)
{
	return _g_mime_tnef_get_uint32 (buffer) | ((guint64) _g_mime_tnef_get_uint32 (buffer + 4) << 32);
}

void
_g_mime_tnef_put_uint16 (guint8 *buffer, guint16 value)
{
	buffer[0] = value;
	buffer[1] = value >> 8;
}

void
_g_mime_tnef_put_uint32 (guint8 *buffer, guint32 value)
{
	buffer[0] = value;
	buffer[1] = value >> 8;
	buffer[2] = value >> 16;
	buffer[3] = value >> 24;
}

void
_g_mime_tnef_put_uint64 (guint8 *buffer, guint64 value)
{
	_g_mime_tnef_put_uint32 (buffer, value);
	_g_mime_tnef_put_uint32 (buffer + 4, value >> 32);
}

char *
_g_mime_tnef_guid_to_string (const guint8 *buffer)
{
	return g_strdup_printf ("%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x", _g_mime_tnef_get_uint32 (buffer),
				_g_mime_tnef_get_uint16 (buffer + 4), _g_mime_tnef_get_uint16 (buffer + 6), buffer[8],
				buffer[9], buffer[10], buffer[11], buffer[12], buffer[13], buffer[14], buffer[15]);
}

gboolean
_g_mime_tnef_guid_from_string (const char *text, guint8 *buffer)
{
	static const guint8 positions[16] = { 6, 4, 2, 0, 11, 9, 16, 14, 19, 21, 24, 26, 28, 30, 32, 34 };
	guint index;

	if (!text || !g_uuid_string_is_valid (text))
		return FALSE;

	for (index = 0; index < 16; index++) {
		guint position = positions[index];
		buffer[index] =
			(g_ascii_xdigit_value (text[position]) << 4) | g_ascii_xdigit_value (text[position + 1]);
	}

	return TRUE;
}

char *
_g_mime_tnef_get_charset (int codepage)
{
	return _g_mime_charset_from_codepage (codepage ? codepage : 1252);
}

char *
_g_mime_tnef_decode_string (const char *charset, GBytes *bytes, gboolean unicode)
{
	const guint8 *data;
	gsize length;
	char *text;

	data = g_bytes_get_data (bytes, &length);

	if (unicode) {
		length &= ~(gsize) 1;

		/* Unicode strings are usually nul-terminated. */
		while (length >= 2 && !data[length - 1] && !data[length - 2])
			length -= 2;
	} else {
		/* 8-bit strings are usually nul-terminated. */
		while (length && !data[length - 1])
			length--;
	}

	if (!length)
		return g_strdup ("");

	text = _g_mime_iconv_convert ((const char *) data, length, "UTF-8", unicode ? "UTF-16LE" : charset, NULL, NULL);
	if (text)
		return text;

	/* Recover malformed sequences rather than rejecting otherwise usable text. */
	if (unicode) {
		GString *decoded = g_string_new (NULL);
		gsize index;

		for (index = 0; index < length; index += 2) {
			gunichar character = _g_mime_tnef_get_uint16 (data + index);

			if (character >= 0xd800 && character <= 0xdbff && index + 3 < length) {
				gunichar low = _g_mime_tnef_get_uint16 (data + index + 2);

				if (low >= 0xdc00 && low <= 0xdfff) {
					character = 0x10000 + ((character - 0xd800) << 10) + low - 0xdc00;
					index += 2;
				}
			}

			if (character >= 0xd800 && character <= 0xdfff)
				character = 0xfffd;

			g_string_append_unichar (decoded, character);
		}

		return g_string_free (decoded, FALSE);
	}

	if (!g_ascii_strcasecmp (charset, "UTF-8"))
		return g_utf8_make_valid ((const char *) data, length);

	/* Recover using the reader's default Windows-1252 encoding, including its undefined C1 control slots. */
	{
		static const gunichar c1[32] = { 0x20ac, 0x0081, 0x201a, 0x0192, 0x201e, 0x2026, 0x2020, 0x2021,
						 0x02c6, 0x2030, 0x0160, 0x2039, 0x0152, 0x008d, 0x017d, 0x008f,
						 0x0090, 0x2018, 0x2019, 0x201c, 0x201d, 0x2022, 0x2013, 0x2014,
						 0x02dc, 0x2122, 0x0161, 0x203a, 0x0153, 0x009d, 0x017e, 0x0178 };
		GString *decoded = g_string_new (NULL);
		gsize index;

		for (index = 0; index < length; index++)
			g_string_append_unichar (decoded, data[index] >= 0x80 && data[index] < 0xa0 ?
								  c1[data[index] - 0x80] :
								  data[index]);

		return g_string_free (decoded, FALSE);
	}
}

GBytes *
_g_mime_tnef_encode_string (const char *charset, const char *text, gboolean unicode, GError **error)
{
	static const guint8 terminator[2] = { 0, 0 };
	char *input = (char *) text, *resume = NULL, *output, buffer[1024];
	size_t remaining = strlen (text), resume_length = 0, available, status;
	gboolean replacing = FALSE, flushing = FALSE;
	GByteArray *encoded;
	iconv_t converter;
	guint count;
	int errsv;

	if (!g_utf8_validate (text, -1, NULL)) {
		g_set_error_literal (error, G_CONVERT_ERROR, G_CONVERT_ERROR_ILLEGAL_SEQUENCE, "Invalid UTF-8 string.");
		return NULL;
	}

	if ((converter = g_mime_iconv_open (unicode ? "UTF-16LE" : charset, "UTF-8")) == (iconv_t) -1) {
		errsv = errno;
		g_set_error (error, G_CONVERT_ERROR, G_CONVERT_ERROR_NO_CONVERSION,
			     "Cannot open TNEF string converter: %s", g_strerror (errsv));
		return NULL;
	}

	encoded = g_byte_array_new ();

	while (TRUE) {
		output = buffer;
		available = sizeof buffer;
		status = g_mime_iconv (converter, flushing ? NULL : &input, flushing ? NULL : &remaining, &output,
				       &available);
		errsv = errno;
		count = sizeof buffer - available;

		if (count > G_MAXUINT - sizeof terminator - encoded->len) {
			g_set_error_literal (error, G_CONVERT_ERROR, G_CONVERT_ERROR_NO_MEMORY,
					     "The encoded TNEF string is too large.");
			goto fail;
		}

		g_byte_array_append (encoded, (const guint8 *) buffer, count);

		if (status != (size_t) -1) {
			if (replacing) {
				input = resume;
				remaining = resume_length;
				replacing = FALSE;
			} else if (flushing) {
				break;
			} else {
				flushing = TRUE;
			}
		} else if (errsv == E2BIG) {
			continue;
		} else if (errsv == EILSEQ && remaining && !replacing && !flushing) {
			/* Encode the replacement through the same descriptor to preserve shift state. */
			resume = g_utf8_next_char (input);
			resume_length = remaining - (resume - input);
			input = (char *) "?";
			remaining = 1;
			replacing = TRUE;
		} else {
			g_set_error (error, G_CONVERT_ERROR, G_CONVERT_ERROR_FAILED, "Cannot encode TNEF string: %s",
				     g_strerror (errsv));
			goto fail;
		}
	}

	g_mime_iconv_close (converter);
	g_byte_array_append (encoded, terminator, unicode ? 2 : 1);

	return g_byte_array_free_to_bytes (encoded);

fail:
	g_mime_iconv_close (converter);
	g_byte_array_unref (encoded);

	return NULL;
}

int
_g_mime_tnef_get_fixed_width (GMimeTnefPropertyType type)
{
	switch (type) {
	case GMIME_TNEF_PROPERTY_TYPE_NULL:
		return 0;
	case GMIME_TNEF_PROPERTY_TYPE_I2:
	case GMIME_TNEF_PROPERTY_TYPE_LONG:
	case GMIME_TNEF_PROPERTY_TYPE_R4:
	case GMIME_TNEF_PROPERTY_TYPE_ERROR:
	case GMIME_TNEF_PROPERTY_TYPE_BOOLEAN:
		/* [MS-OXTNEF] pads 16-bit values out to 4 bytes. */
		return 4;
	case GMIME_TNEF_PROPERTY_TYPE_DOUBLE:
	case GMIME_TNEF_PROPERTY_TYPE_CURRENCY:
	case GMIME_TNEF_PROPERTY_TYPE_APP_TIME:
	case GMIME_TNEF_PROPERTY_TYPE_SYS_TIME:
	case GMIME_TNEF_PROPERTY_TYPE_I8:
		return 8;
	case GMIME_TNEF_PROPERTY_TYPE_CLASS_ID:
		return 16;
	case GMIME_TNEF_PROPERTY_TYPE_UNICODE:
	case GMIME_TNEF_PROPERTY_TYPE_STRING8:
	case GMIME_TNEF_PROPERTY_TYPE_BINARY:
	case GMIME_TNEF_PROPERTY_TYPE_OBJECT:
		return -1;
	default:
		return -2;
	}
}

int
_g_mime_tnef_get_attribute_level (GMimeTnefAttributeTag tag)
{
	switch (tag) {
	case GMIME_TNEF_ATTRIBUTE_TAG_ATTACH_CREATE_DATE:
	case GMIME_TNEF_ATTRIBUTE_TAG_ATTACH_DATA:
	case GMIME_TNEF_ATTRIBUTE_TAG_ATTACHMENT:
	case GMIME_TNEF_ATTRIBUTE_TAG_ATTACH_META_FILE:
	case GMIME_TNEF_ATTRIBUTE_TAG_ATTACH_MODIFY_DATE:
	case GMIME_TNEF_ATTRIBUTE_TAG_ATTACH_RENDER_DATA:
	case GMIME_TNEF_ATTRIBUTE_TAG_ATTACH_TITLE:
	case GMIME_TNEF_ATTRIBUTE_TAG_ATTACH_TRANSPORT_FILENAME:
		return GMIME_TNEF_ATTRIBUTE_LEVEL_ATTACHMENT;
	case GMIME_TNEF_ATTRIBUTE_TAG_AID_OWNER:
	case GMIME_TNEF_ATTRIBUTE_TAG_BODY:
	case GMIME_TNEF_ATTRIBUTE_TAG_CONVERSATION_ID:
	case GMIME_TNEF_ATTRIBUTE_TAG_DATE_END:
	case GMIME_TNEF_ATTRIBUTE_TAG_DATE_MODIFIED:
	case GMIME_TNEF_ATTRIBUTE_TAG_DATE_RECEIVED:
	case GMIME_TNEF_ATTRIBUTE_TAG_DATE_SENT:
	case GMIME_TNEF_ATTRIBUTE_TAG_DATE_START:
	case GMIME_TNEF_ATTRIBUTE_TAG_DELEGATE:
	case GMIME_TNEF_ATTRIBUTE_TAG_FROM:
	case GMIME_TNEF_ATTRIBUTE_TAG_MAPI_PROPERTIES:
	case GMIME_TNEF_ATTRIBUTE_TAG_MESSAGE_CLASS:
	case GMIME_TNEF_ATTRIBUTE_TAG_MESSAGE_ID:
	case GMIME_TNEF_ATTRIBUTE_TAG_MESSAGE_STATUS:
	case GMIME_TNEF_ATTRIBUTE_TAG_ORIGINAL_MESSAGE_CLASS:
	case GMIME_TNEF_ATTRIBUTE_TAG_OWNER:
	case GMIME_TNEF_ATTRIBUTE_TAG_PARENT_ID:
	case GMIME_TNEF_ATTRIBUTE_TAG_PRIORITY:
	case GMIME_TNEF_ATTRIBUTE_TAG_RECIPIENT_TABLE:
	case GMIME_TNEF_ATTRIBUTE_TAG_REQUEST_RESPONSE:
	case GMIME_TNEF_ATTRIBUTE_TAG_SENT_FOR:
	case GMIME_TNEF_ATTRIBUTE_TAG_SUBJECT:
	case GMIME_TNEF_ATTRIBUTE_TAG_TNEF_VERSION:
	case GMIME_TNEF_ATTRIBUTE_TAG_OEM_CODEPAGE:
		return GMIME_TNEF_ATTRIBUTE_LEVEL_MESSAGE;
	default:
		return 0;
	}
}

gboolean
_g_mime_tnef_invalid_operation (GError **error, const char *message)
{
	g_set_error_literal (error, GMIME_ERROR, GMIME_ERROR_INVALID_OPERATION, message);
	return FALSE;
}

gboolean
_g_mime_tnef_io_error (GError **error)
{
	int saved_errno = errno ? errno : EIO;

	g_set_error (error, GMIME_ERROR, saved_errno, "TNEF stream I/O failed: %s", g_strerror (saved_errno));
	return FALSE;
}

gboolean
_g_mime_tnef_write_all (GMimeStream *stream, const guint8 *buffer, gsize length, GError **error)
{
	return _g_mime_stream_write_all (stream, (const char *) buffer, length) >= 0 || _g_mime_tnef_io_error (error);
}

ssize_t
_g_mime_tnef_stream_no_read (GMimeStream *stream, char *buffer, size_t length)
{
	errno = EBADF;
	return -1;
}

int
_g_mime_tnef_stream_no_reset (GMimeStream *stream)
{
	errno = ESPIPE;
	return -1;
}

gint64
_g_mime_tnef_stream_no_seek (GMimeStream *stream, gint64 offset, GMimeSeekWhence whence)
{
	errno = ESPIPE;
	return -1;
}

GMimeStream *
_g_mime_tnef_stream_no_substream (GMimeStream *stream, gint64 start, gint64 end)
{
	errno = ESPIPE;
	return NULL;
}

GDateTime *
_g_mime_tnef_decode_date (GMimeTnefPropertyType type, const guint8 *buffer)
{
	gint64 ticks = (gint64) _g_mime_tnef_get_uint64 (buffer);
	GDateTime *epoch, *date;
	double days;

	if (type == GMIME_TNEF_PROPERTY_TYPE_SYS_TIME) {
		/* FILETIME counts 100-nanosecond intervals since January 1, 1601 UTC. */
		if (ticks < 0 || ticks > G_GINT64_CONSTANT (2650467743999999999))
			return NULL;

		epoch = g_date_time_new_utc (1601, 1, 1, 0, 0, 0);
		date = g_date_time_add (epoch, ticks / 10);
	} else {
		memcpy (&days, &ticks, sizeof days);

		if (!isfinite (days) || days <= -657435.0 || days >= 2958466.0)
			return NULL;

		/* The fractional part of a negative OLE date is still a positive time of day. */
		days = trunc (days) + fabs (days - trunc (days));
		epoch = g_date_time_new_utc (1899, 12, 30, 0, 0, 0);
		date = g_date_time_add (epoch, (gint64) round (days * 86400000.0) * 1000);
	}

	g_date_time_unref (epoch);

	return date;
}
