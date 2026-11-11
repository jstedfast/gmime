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

#include "gmime-html-entity-private.h"
#include "gmime-html-entity.h"
#include "gmime-html-entity-tables-private.h"

/**
 * SECTION: gmime-html-entity
 * @title: HTML entity decoder
 * @short_description: Incremental named and numeric character references
 *
 * Push characters beginning with an ampersand until push() returns %FALSE.
 * The rejected character remains unconsumed. Obtain the decoded value, then
 * reset the decoder before starting another reference. Attribute values apply
 * HTML's historical missing-semicolon rule using the next unconsumed scalar.
 * Returned strings are borrowed and invalidated by decoder mutation.
 */
gboolean
_g_mime_html_entity_push (HtmlEntity *entity, gunichar c)
{
	const EntityTransition *table;
	guint state, digit;
	gsize n, lo, hi;

	if (entity->semicolon)
		return FALSE;

	if (!entity->index) {
		g_return_val_if_fail (c == '&', FALSE);
		entity->pushed[entity->index++] = '&';
		return TRUE;
	}

	if (entity->numeric) {
		if (!entity->base) {
			if (c == 'x' || c == 'X') {
				entity->pushed[entity->index++] = c;
				entity->base = 16;
				return TRUE;
			}

			entity->base = 10;
		}

		if (c == ';') {
			if (!entity->digits)
				return FALSE;

			entity->semicolon = TRUE;
		} else {
			if (c >= '0' && c <= '9')
				digit = c - '0';
			else if (entity->base == 16 && c >= 'A' && c <= 'F')
				digit = c - 'A' + 10;
			else if (entity->base == 16 && c >= 'a' && c <= 'f')
				digit = c - 'a' + 10;
			else
				return FALSE;

			/* Consume every digit, saturating above the Unicode range to avoid overflow. */
			if (entity->numeric_value <= 0x10ffff)
				entity->numeric_value = MIN (entity->numeric_value * entity->base + digit, 0x110000);

			entity->digits = TRUE;
		}

		if (entity->index < 33)
			entity->pushed[entity->index++] = c;

		return TRUE;
	}

	if (entity->index == 33)
		return FALSE;

	if (entity->index == 1 && c == '#') {
		entity->numeric = TRUE;
		entity->pushed[entity->index++] = '#';
		return TRUE;
	}

	table = entity_table (c, &n);
	state = entity->states[entity->index - 1];
	lo = 0;
	hi = n;

	while (lo < hi) {
		gsize mid = lo + (hi - lo) / 2;

		if (table[mid].from < state)
			lo = mid + 1;
		else
			hi = mid;
	}

	if (lo == n || table[lo].from != state)
		return FALSE;

	entity->states[entity->index] = table[lo].to;
	entity->pushed[entity->index++] = c;
	entity->semicolon = c == ';';

	return TRUE;
}

static guint
matched_length (HtmlEntity *entity)
{
	guint i = entity->index;

	while (i > 1 &&
	       (entity->states[i - 1] >= G_N_ELEMENTS (entity_values) || !entity_values[entity->states[i - 1]]))
		i--;

	return i;
}

static gboolean
historical (HtmlEntity *entity, guint matched, gunichar next)
{
	gunichar c = matched < entity->index ? (guchar) entity->pushed[matched] : next;

	return matched > 1 && entity->pushed[matched - 1] != ';' && (c == '=' || (c < 128 && g_ascii_isalnum (c)));
}

const char *
_g_mime_html_entity_value (HtmlEntity *entity, gboolean attribute, gunichar next)
{
	static const gunichar windows_1252[32] = { 0x20ac, 0x81,   0x201a, 0x192,  0x201e, 0x2026, 0x2020, 0x2021,
						   0x2c6,  0x2030, 0x160,  0x2039, 0x152,  0x8d,   0x17d,  0x8f,
						   0x90,   0x2018, 0x2019, 0x201c, 0x201d, 0x2022, 0x2013, 0x2014,
						   0x2dc,  0x2122, 0x161,  0x203a, 0x153,  0x9d,   0x17e,  0x178 };
	guint matched = matched_length (entity), len;
	gunichar value = entity->numeric_value;

	entity->pushed[entity->index] = 0;
	if (entity->numeric) {
		if (!entity->digits)
			return entity->pushed;

		if (!value || value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff))
			value = 0xfffd;
		else if (value >= 0x80 && value <= 0x9f)
			value = windows_1252[value - 0x80];

		len = g_unichar_to_utf8 (value, entity->value);
		entity->value[len] = 0;
	} else {
		if (matched <= 1 || (attribute && historical (entity, matched, next)))
			return entity->pushed;

		g_strlcpy (entity->value, entity_values[entity->states[matched - 1]], sizeof entity->value);
		g_strlcat (entity->value, entity->pushed + matched, sizeof entity->value);
	}

	return entity->value;
}

static GMimeHtmlComplianceViolation
numeric_violation (guint value)
{
	if (!value)
		return GMIME_HTML_COMPLIANCE_VIOLATION_NULL_CHARACTER_REFERENCE;

	if (value > 0x10ffff)
		return GMIME_HTML_COMPLIANCE_VIOLATION_CHARACTER_REFERENCE_OUTSIDE_UNICODE_RANGE;

	if (value >= 0xd800 && value <= 0xdfff)
		return GMIME_HTML_COMPLIANCE_VIOLATION_SURROGATE_CHARACTER_REFERENCE;

	if ((value >= 0xfdd0 && value <= 0xfdef) || (value & 0xfffe) == 0xfffe)
		return GMIME_HTML_COMPLIANCE_VIOLATION_NONCHARACTER_CHARACTER_REFERENCE;

	if (value == 13 || ((value < 32 || (value >= 127 && value <= 159)) && value != 9 && value != 10 && value != 12))
		return GMIME_HTML_COMPLIANCE_VIOLATION_CONTROL_CHARACTER_REFERENCE;

	return GMIME_HTML_COMPLIANCE_VIOLATION_NONE;
}

void
_g_mime_html_entity_violations (HtmlEntity *entity, gboolean attribute, gunichar next,
				GMimeHtmlComplianceViolation *first, GMimeHtmlComplianceViolation *second)
{
	guint matched;

	*first = *second = GMIME_HTML_COMPLIANCE_VIOLATION_NONE;
	if (entity->numeric) {
		if (!entity->digits)
			*first = GMIME_HTML_COMPLIANCE_VIOLATION_ABSENCE_OF_DIGITS_IN_NUMERIC_CHARACTER_REFERENCE;
		else if (!entity->semicolon) {
			*first = GMIME_HTML_COMPLIANCE_VIOLATION_MISSING_SEMICOLON_AFTER_CHARACTER_REFERENCE;
			*second = numeric_violation (entity->numeric_value);
		} else
			*first = numeric_violation (entity->numeric_value);
	} else {
		matched = matched_length (entity);

		if (matched > 1 && entity->pushed[matched - 1] != ';' &&
		    !(attribute && historical (entity, matched, next)))
			*first = GMIME_HTML_COMPLIANCE_VIOLATION_MISSING_SEMICOLON_AFTER_CHARACTER_REFERENCE;
	}
}

int
_g_mime_html_entity_unknown_length (HtmlEntity *entity)
{
	return entity->numeric || matched_length (entity) > 1 ? -1 : (int) entity->index - 1;
}

void
_g_mime_html_entity_reset (HtmlEntity *entity)
{
	memset (entity, 0, sizeof *entity);
}

struct _GMimeHtmlEntityDecoder {
	GObject parent;
	HtmlEntity entity;
};

G_DEFINE_FINAL_TYPE (GMimeHtmlEntityDecoder, g_mime_html_entity_decoder, G_TYPE_OBJECT)

static void
g_mime_html_entity_decoder_class_init (GMimeHtmlEntityDecoderClass *klass)
{
}

static void
g_mime_html_entity_decoder_init (GMimeHtmlEntityDecoder *decoder)
{
}

/**
 * g_mime_html_entity_decoder_new:
 *
 * Returns: (transfer full): a new incremental entity decoder
 */
GMimeHtmlEntityDecoder *
g_mime_html_entity_decoder_new (void)
{
	return g_object_new (GMIME_TYPE_HTML_ENTITY_DECODER, NULL);
}

/**
 * g_mime_html_entity_decoder_push:
 * @decoder: an entity decoder
 * @c: the next Unicode scalar
 *
 * Feeds a character reference, starting with '&'.
 *
 * Returns: %TRUE if @c was consumed, or %FALSE if it was refused
 */
gboolean
g_mime_html_entity_decoder_push (GMimeHtmlEntityDecoder *decoder, gunichar c)
{
	g_return_val_if_fail (GMIME_IS_HTML_ENTITY_DECODER (decoder), FALSE);

	return _g_mime_html_entity_push (&decoder->entity, c);
}

/**
 * g_mime_html_entity_decoder_get_value:
 * @decoder: an entity decoder
 *
 * Returns: (transfer none): the decoded value, valid until the next push,
 * reset, or value call on @decoder
 */
const char *
g_mime_html_entity_decoder_get_value (GMimeHtmlEntityDecoder *decoder)
{
	g_return_val_if_fail (GMIME_IS_HTML_ENTITY_DECODER (decoder), NULL);

	return _g_mime_html_entity_value (&decoder->entity, FALSE, 0);
}

/**
 * g_mime_html_entity_decoder_get_attribute_value:
 * @decoder: an entity decoder
 * @next: the following Unicode scalar, or zero at end of input
 *
 * Applies the character-reference rules for attribute values.
 *
 * Returns: (transfer none): the decoded value, valid until the next push,
 * reset, or value call on @decoder
 */
const char *
g_mime_html_entity_decoder_get_attribute_value (GMimeHtmlEntityDecoder *decoder, gunichar next)
{
	g_return_val_if_fail (GMIME_IS_HTML_ENTITY_DECODER (decoder), NULL);

	return _g_mime_html_entity_value (&decoder->entity, TRUE, next);
}

/**
 * g_mime_html_entity_decoder_reset:
 * @decoder: a #GMimeHtmlEntityDecoder
 *
 * Clears the accumulated character reference and invalidates borrowed decoded
 * values. The next character pushed must be an ampersand.
 **/
void
g_mime_html_entity_decoder_reset (GMimeHtmlEntityDecoder *decoder)
{
	g_return_if_fail (GMIME_IS_HTML_ENTITY_DECODER (decoder));

	_g_mime_html_entity_reset (&decoder->entity);
}
