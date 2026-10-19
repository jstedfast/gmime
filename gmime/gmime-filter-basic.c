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
#include "gmime-utils.h"


/**
 * SECTION: gmime-filter-basic
 * @title: GMimeFilterBasic
 * @short_description: Basic transfer encoding filter
 * @see_also: #GMimeFilter
 *
 * A #GMimeFilter which can encode or decode basic MIME encodings such
 * as Quoted-Printable, Base64 and UUEncode, or use an explicitly supplied
 * #GMimeYencEncoder or #GMimeYencDecoder for yEnc.
 **/


static void g_mime_filter_basic_class_init (GMimeFilterBasicClass *klass);
static void g_mime_filter_basic_init (GMimeFilterBasic *filter, GMimeFilterBasicClass *klass);
static void g_mime_filter_basic_finalize (GObject *object);

static GMimeFilter *filter_copy (GMimeFilter *filter);
static void filter_filter (GMimeFilter *filter, char *in, size_t len, size_t prespace,
			   char **out, size_t *outlen, size_t *outprespace);
static void filter_complete (GMimeFilter *filter, char *in, size_t len, size_t prespace,
			     char **out, size_t *outlen, size_t *outprespace);
static void filter_reset (GMimeFilter *filter);


static GMimeFilterClass *parent_class = NULL;


GType
g_mime_filter_basic_get_type (void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof (GMimeFilterBasicClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc) g_mime_filter_basic_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (GMimeFilterBasic),
			0,    /* n_preallocs */
			(GInstanceInitFunc) g_mime_filter_basic_init,
		};
		
		type = g_type_register_static (GMIME_TYPE_FILTER, "GMimeFilterBasic", &info, 0);
	}
	
	return type;
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
g_mime_filter_basic_init (GMimeFilterBasic *filter, GMimeFilterBasicClass *klass)
{
	
}

static void
g_mime_filter_basic_finalize (GObject *object)
{
	GMimeFilterBasic *basic = (GMimeFilterBasic *) object;

	g_clear_object (&basic->encoder);
	g_clear_object (&basic->decoder);
	G_OBJECT_CLASS (parent_class)->finalize (object);
}


static GMimeFilter *
filter_copy (GMimeFilter *filter)
{
	GMimeFilterBasic *basic = (GMimeFilterBasic *) filter;
	GMimeFilter *copy;

	if (basic->encoder) {
		GMimeEncoder *encoder = g_mime_encoder_clone (basic->encoder);
		g_mime_encoder_reset (encoder);
		copy = g_mime_filter_basic_new_with_encoder (encoder);
		g_object_unref (encoder);
	} else {
		GMimeDecoder *decoder = g_mime_decoder_clone (basic->decoder);
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
	GMimeFilterBasic *basic = (GMimeFilterBasic *) filter;
	size_t nwritten, len;

	len = basic->encoder ? g_mime_encoder_estimate_output_length (basic->encoder, inlen) :
		g_mime_decoder_estimate_output_length (basic->decoder, inlen);
	g_mime_filter_set_size (filter, len, FALSE);
	nwritten = basic->encoder ?
		g_mime_encoder_encode (basic->encoder, (const guint8 *) inbuf, inlen, (guint8 *) filter->outbuf, len) :
		g_mime_decoder_decode (basic->decoder, (const guint8 *) inbuf, inlen, (guint8 *) filter->outbuf, len);
	g_assert (nwritten <= len);
	
	*outprespace = filter->outpre;
	*outbuf = filter->outbuf;
	*outlen = nwritten;
}

static void
filter_complete (GMimeFilter *filter, char *inbuf, size_t inlen, size_t prespace,
		 char **outbuf, size_t *outlen, size_t *outprespace)
{
	GMimeFilterBasic *basic = (GMimeFilterBasic *) filter;
	size_t nwritten, len;

	if (!basic->encoder) {
		filter_filter (filter, inbuf, inlen, prespace, outbuf, outlen, outprespace);
		return;
	}

	len = g_mime_encoder_estimate_output_length (basic->encoder, inlen);
	g_mime_filter_set_size (filter, len, FALSE);
	nwritten = g_mime_encoder_flush (basic->encoder, (const guint8 *) inbuf, inlen,
					(guint8 *) filter->outbuf, len);
	g_assert (nwritten <= len);
	
	*outprespace = filter->outpre;
	*outbuf = filter->outbuf;
	*outlen = nwritten;
}

/* should this 'flush' outstanding state/data bytes? */
static void
filter_reset (GMimeFilter *filter)
{
	GMimeFilterBasic *basic = (GMimeFilterBasic *) filter;
	
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
	GMimeFilterBasic *basic;
	g_return_val_if_fail (GMIME_IS_ENCODER (encoder), NULL);
	basic = g_object_new (GMIME_TYPE_FILTER_BASIC, NULL);
	basic->encoder = g_object_ref (encoder);
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
	GMimeFilterBasic *basic;
	g_return_val_if_fail (GMIME_IS_DECODER (decoder), NULL);
	basic = g_object_new (GMIME_TYPE_FILTER_BASIC, NULL);
	basic->decoder = g_object_ref (decoder);
	return GMIME_FILTER (basic);
}
