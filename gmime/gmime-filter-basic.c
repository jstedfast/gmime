/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2022 Jeffrey Stedfast
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

#include "gmime-filter-basic.h"
#include "gmime-filter-pass-through.h"
#include "gmime-common.h"
#include "gmime-utils.h"


#include "gmime-object-private.h"

/**
 * SECTION: gmime-filter-basic
 * @title: GMimeFilterBasic
 * @short_description: Basic transfer encoding filter
 * @see_also: #GMimeFilter
 *
 * A #GMimeFilter which can encode or decode basic MIME encodings such
 * as Quoted-Printable, Base64 and UUEncode, or use an explicitly supplied
 * #GMimeYencEncoder or #GMimeYencDecoder for yEnc.
 *
 * The g_mime_filter_basic_create_encoder() and
 * g_mime_filter_basic_create_decoder() factory families follow MimeKit's
 * encoding selection, returning #GMimeFilterPassThrough for unhandled
 * encodings. Encoder factories default to CRLF, unlike the existing
 * LF-based g_mime_filter_basic_new() constructor.
 **/


static void g_mime_filter_basic_class_init (GMimeFilterBasicClass *klass);
static void g_mime_filter_basic_init (GMimeFilterBasic *filter);
static void g_mime_filter_basic_finalize (GObject *object);

static GMimeFilter *filter_copy (GMimeFilter *filter);
static void filter_filter (GMimeFilter *filter, char *in, size_t len, size_t prespace,
			   char **out, size_t *outlen, size_t *outprespace);
static void filter_complete (GMimeFilter *filter, char *in, size_t len, size_t prespace,
			     char **out, size_t *outlen, size_t *outprespace);
static void filter_reset (GMimeFilter *filter);


static GMimeFilterClass *parent_class = NULL;



G_DEFINE_TYPE_WITH_PRIVATE (GMimeFilterBasic, g_mime_filter_basic, GMIME_TYPE_FILTER)

GMimeFilterBasicPrivate *
_g_mime_filter_basic_get_private (GMimeFilterBasic *object)
{
	return g_mime_filter_basic_get_instance_private (object);
}


static void
g_mime_filter_basic_class_init (GMimeFilterBasicClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GMimeFilterClass *filter_class = GMIME_FILTER_CLASS (klass);
	
	parent_class = g_type_class_ref (GMIME_TYPE_FILTER);
	
	object_class->finalize = g_mime_filter_basic_finalize;
	
	filter_class->copy = filter_copy;
	filter_class->filter = filter_filter;
	filter_class->complete = filter_complete;
	filter_class->reset = filter_reset;
}

static void
g_mime_filter_basic_init (GMimeFilterBasic *filter)
{
	
}

static void
g_mime_filter_basic_finalize (GObject *object)
{
	GMimeFilterBasicPrivate *priv = _g_mime_filter_basic_get_private ((GMimeFilterBasic *) object);

	g_clear_object (&priv->encoder);
	g_clear_object (&priv->decoder);
	G_OBJECT_CLASS (parent_class)->finalize (object);
}


static GMimeFilter *
filter_copy (GMimeFilter *filter)
{
	GMimeFilterBasicPrivate *priv = _g_mime_filter_basic_get_private ((GMimeFilterBasic *) filter);
	GMimeFilter *copy;

	if (priv->encoder) {
		GMimeEncoder *encoder = g_mime_encoder_clone (priv->encoder);
		g_mime_encoder_reset (encoder);
		copy = g_mime_filter_basic_new_with_encoder (encoder);
		g_object_unref (encoder);
	} else {
		GMimeDecoder *decoder = g_mime_decoder_clone (priv->decoder);
		g_mime_decoder_reset (decoder);
		copy = g_mime_filter_basic_new_with_decoder (decoder);
		g_object_unref (decoder);
	}
	return copy;
}

/* here we do all of the basic mime filtering */
static void
filter_filter (GMimeFilter *filter, char *inbuf, size_t inlen, size_t prespace,
	       char **outbuf, size_t *outlen, size_t *outprespace)
{
	GMimeFilterBasicPrivate *basic = _g_mime_filter_basic_get_private ((GMimeFilterBasic *) filter);
	GMimeFilterPrivate *priv = _g_mime_filter_get_private (filter);
	size_t nwritten, len;

	len = basic->encoder ? g_mime_encoder_estimate_output_length (basic->encoder, inlen) :
		g_mime_decoder_estimate_output_length (basic->decoder, inlen);

	g_mime_filter_set_size (filter, len, FALSE);

	nwritten = basic->encoder ?
		g_mime_encoder_encode (basic->encoder, (const guint8 *) inbuf, inlen, (guint8 *) priv->outbuf, len) :
		g_mime_decoder_decode (basic->decoder, (const guint8 *) inbuf, inlen, (guint8 *) priv->outbuf, len);
	g_assert (nwritten <= len);
	
	*outprespace = priv->outpre;
	*outbuf = priv->outbuf;
	*outlen = nwritten;
}

static void
filter_complete (GMimeFilter *filter, char *inbuf, size_t inlen, size_t prespace,
		 char **outbuf, size_t *outlen, size_t *outprespace)
{
	GMimeFilterBasicPrivate *basic = _g_mime_filter_basic_get_private ((GMimeFilterBasic *) filter);
	GMimeFilterPrivate *priv = _g_mime_filter_get_private (filter);
	size_t nwritten, len;

	if (!basic->encoder) {
		filter_filter (filter, inbuf, inlen, prespace, outbuf, outlen, outprespace);
		return;
	}

	len = g_mime_encoder_estimate_output_length (basic->encoder, inlen);
	g_mime_filter_set_size (filter, len, FALSE);

	nwritten = g_mime_encoder_flush (basic->encoder, (const guint8 *) inbuf, inlen,
					(guint8 *) priv->outbuf, len);
	g_assert (nwritten <= len);
	
	*outprespace = priv->outpre;
	*outbuf = priv->outbuf;
	*outlen = nwritten;
}

/* should this 'flush' outstanding state/data bytes? */
static void
filter_reset (GMimeFilter *filter)
{
	GMimeFilterBasicPrivate *basic = _g_mime_filter_basic_get_private ((GMimeFilterBasic *) filter);

	if (basic->encoder)
		g_mime_encoder_reset (basic->encoder);
	else
		g_mime_decoder_reset (basic->decoder);
}


/**
 * g_mime_filter_basic_new:
 * @encoding: a #GMimeContentEncoding
 * @encode: %TRUE to encode or %FALSE to decode
 *
 * Creates a new basic filter for @encoding.
 * Encoders use LF line endings. Unspecified, 7bit, 8bit and binary encodings
 * use copying pass-through codecs, unlike
 * g_mime_filter_basic_create_encoder() and
 * g_mime_filter_basic_create_decoder(), which return #GMimeFilterPassThrough.
 *
 * Returns: a new basic encoder filter.
 **/
GMimeFilter *
g_mime_filter_basic_new (GMimeContentEncoding encoding, gboolean encode)
{
	GMimeFilter *filter;
	GMimeEncoder *encoder;
	GMimeDecoder *decoder;

	g_return_val_if_fail (encoding >= GMIME_CONTENT_ENCODING_UNSPECIFIED && encoding <= GMIME_CONTENT_ENCODING_UUENCODE, NULL);

	if (encode) {
		switch (encoding) {
		case GMIME_CONTENT_ENCODING_BASE64:
			encoder = GMIME_ENCODER (g_mime_base64_encoder_new ());
			break;
		case GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE:
			encoder = GMIME_ENCODER (g_mime_quoted_printable_encoder_new ());
			break;
		case GMIME_CONTENT_ENCODING_UUENCODE:
			encoder = GMIME_ENCODER (g_mime_uu_encoder_new ());
			break;
		default:
			encoder = GMIME_ENCODER (g_mime_pass_through_encoder_new (encoding));
			break;
		}

		filter = g_mime_filter_basic_new_with_encoder (encoder);
		g_object_unref (encoder);
	} else {
		switch (encoding) {
		case GMIME_CONTENT_ENCODING_BASE64:
			decoder = GMIME_DECODER (g_mime_base64_decoder_new ());
			break;
		case GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE:
			decoder = GMIME_DECODER (g_mime_quoted_printable_decoder_new (FALSE));
			break;
		case GMIME_CONTENT_ENCODING_UUENCODE:
			decoder = GMIME_DECODER (g_mime_uu_decoder_new (FALSE));
			break;
		default:
			decoder = GMIME_DECODER (g_mime_pass_through_decoder_new (encoding));
			break;
		}

		filter = g_mime_filter_basic_new_with_decoder (decoder);
		g_object_unref (decoder);
	}

	return filter;
}

/**
 * g_mime_filter_basic_new_with_encoder:
 * @encoder: (transfer none): an encoder
 *
 * The filter retains a reference and uses the encoder's current state.
 * Copies of the filter use independent, reset clones of the encoder.
 *
 * Returns: (transfer full): a new encoding filter.
 **/
GMimeFilter *
g_mime_filter_basic_new_with_encoder (GMimeEncoder *encoder)
{
	GMimeFilterBasicPrivate *priv;
	GMimeFilterBasic *basic;

	g_return_val_if_fail (GMIME_IS_ENCODER (encoder), NULL);

	basic = g_object_new (GMIME_TYPE_FILTER_BASIC, NULL);
	priv = _g_mime_filter_basic_get_private (basic);

	priv->encoder = g_object_ref (encoder);

	return GMIME_FILTER (basic);
}

/**
 * g_mime_filter_basic_new_with_decoder:
 * @decoder: (transfer none): a decoder
 *
 * The filter retains a reference and uses the decoder's current state.
 * Copies of the filter use independent, reset clones of the decoder.
 *
 * Returns: (transfer full): a new decoding filter.
 **/
GMimeFilter *
g_mime_filter_basic_new_with_decoder (GMimeDecoder *decoder)
{
	GMimeFilterBasicPrivate *priv;
	GMimeFilterBasic *basic;

	g_return_val_if_fail (GMIME_IS_DECODER (decoder), NULL);

	basic = g_object_new (GMIME_TYPE_FILTER_BASIC, NULL);
	priv = _g_mime_filter_basic_get_private (basic);

	priv->decoder = g_object_ref (decoder);

	return GMIME_FILTER (basic);
}

/**
 * g_mime_filter_basic_create_encoder:
 * @encoding: a content encoding, including unrecognized enum values
 *
 * Creates an encoding filter using CRLF line endings and a maximum line
 * length of 78, capped by the selected encoder's MIME limits.
 * Encodings other than Base64, quoted-printable and uuencode pass through
 * unchanged without copying or allocating an output buffer.
 *
 * Returns: (transfer full): a new encoding or pass-through filter.
 **/
GMimeFilter *
g_mime_filter_basic_create_encoder (GMimeContentEncoding encoding)
{
	return g_mime_filter_basic_create_encoder_with_newline_format (encoding, GMIME_NEWLINE_FORMAT_DOS, 78);
}

/**
 * g_mime_filter_basic_create_encoder_with_newline_format:
 * @encoding: a content encoding, including unrecognized enum values
 * @newline_format: LF or CRLF line endings
 * @max_line_length: a maximum line length, excluding the terminator
 *
 * Creates a Base64, quoted-printable or uuencode filter with the selected
 * line endings. Base64 and quoted-printable require a line length between
 * 60 and 998 and cap it at 76. uuencode and unhandled encodings ignore the
 * line-length argument. Unhandled encodings return #GMimeFilterPassThrough.
 * The newline format must be valid even for pass-through filters.
 *
 * Returns: (transfer full): a new encoding or pass-through filter.
 **/
GMimeFilter *
g_mime_filter_basic_create_encoder_with_newline_format (GMimeContentEncoding encoding,
						      GMimeNewLineFormat newline_format, guint max_line_length)
{
	GMimeEncoder *encoder;
	GMimeFilter *filter;

	g_return_val_if_fail (newline_format == GMIME_NEWLINE_FORMAT_UNIX || newline_format == GMIME_NEWLINE_FORMAT_DOS, NULL);

	/* Match the selected codec's argument checks, rather than imposing a
	 * line-length limit on uuencode or on unchanged content. */
	switch (encoding) {
	case GMIME_CONTENT_ENCODING_BASE64:
		encoder = GMIME_ENCODER (g_mime_base64_encoder_new_with_newline_format (newline_format, max_line_length));
		break;
	case GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE:
		encoder = GMIME_ENCODER (g_mime_quoted_printable_encoder_new_with_newline_format (newline_format, max_line_length));
		break;
	case GMIME_CONTENT_ENCODING_UUENCODE:
		encoder = GMIME_ENCODER (g_mime_uu_encoder_new_with_newline_format (newline_format));
		break;
	default:
		return g_mime_filter_pass_through_new ();
	}

	if (!encoder)
		return NULL;

	filter = g_mime_filter_basic_new_with_encoder (encoder);
	g_object_unref (encoder);

	return filter;
}

/**
 * g_mime_filter_basic_create_encoder_by_name:
 * @name: a non-%NULL UTF-8 content-transfer-encoding name
 *
 * Creates an encoding filter using CRLF and a maximum line length of 78.
 * Matching is case-insensitive, skips leading Unicode whitespace, and
 * stops at the first whitespace or semicolon. Unknown names pass through
 * unchanged without copying.
 *
 * Returns: (transfer full): a new encoding or pass-through filter.
 **/
GMimeFilter *
g_mime_filter_basic_create_encoder_by_name (const char *name)
{
	return g_mime_filter_basic_create_encoder_by_name_with_newline_format (name, GMIME_NEWLINE_FORMAT_DOS, 78);
}

/**
 * g_mime_filter_basic_create_encoder_by_name_with_newline_format:
 * @name: a non-%NULL UTF-8 content-transfer-encoding name
 * @newline_format: LF or CRLF line endings
 * @max_line_length: a maximum line length, excluding the terminator
 *
 * Resolves @name as in g_mime_filter_basic_create_encoder_by_name() and
 * selects a filter as in
 * g_mime_filter_basic_create_encoder_with_newline_format().
 * Base64 and quoted-printable validate @max_line_length; other encodings
 * ignore it. The newline format is always validated.
 *
 * Returns: (transfer full): a new encoding or pass-through filter.
 **/
GMimeFilter *
g_mime_filter_basic_create_encoder_by_name_with_newline_format (const char *name,
							      GMimeNewLineFormat newline_format, guint max_line_length)
{
	g_return_val_if_fail (name != NULL, NULL);
	g_return_val_if_fail (g_utf8_validate (name, -1, NULL), NULL);

	return g_mime_filter_basic_create_encoder_with_newline_format (g_mime_content_encoding_from_string (name),
								     newline_format, max_line_length);
}

/**
 * g_mime_filter_basic_create_decoder:
 * @encoding: a content encoding, including unrecognized enum values
 *
 * Creates a Base64, quoted-printable or uuencode decoder filter. The UU
 * decoder looks for a begin-line before decoding the payload. Other
 * encodings pass through unchanged without copying.
 *
 * Returns: (transfer full): a new decoding or pass-through filter.
 **/
GMimeFilter *
g_mime_filter_basic_create_decoder (GMimeContentEncoding encoding)
{
	switch (encoding) {
	case GMIME_CONTENT_ENCODING_BASE64:
	case GMIME_CONTENT_ENCODING_QUOTEDPRINTABLE:
	case GMIME_CONTENT_ENCODING_UUENCODE:
		return g_mime_filter_basic_new (encoding, FALSE);
	default:
		return g_mime_filter_pass_through_new ();
	}
}

/**
 * g_mime_filter_basic_create_decoder_by_name:
 * @name: a non-%NULL UTF-8 content-transfer-encoding name
 *
 * Resolves @name as in g_mime_filter_basic_create_encoder_by_name() and
 * selects a filter as in g_mime_filter_basic_create_decoder().
 *
 * Returns: (transfer full): a new decoding or pass-through filter.
 **/
GMimeFilter *
g_mime_filter_basic_create_decoder_by_name (const char *name)
{
	g_return_val_if_fail (name != NULL, NULL);
	g_return_val_if_fail (g_utf8_validate (name, -1, NULL), NULL);

	return g_mime_filter_basic_create_decoder (g_mime_content_encoding_from_string (name));
}
