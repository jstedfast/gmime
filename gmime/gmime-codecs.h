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

#ifndef __GMIME_CODECS_H__
#define __GMIME_CODECS_H__

#include <gmime/gmime-encoder.h>
#include <gmime/gmime-decoder.h>

G_BEGIN_DECLS

/**
 * SECTION: gmime-codecs
 * @title: MIME codecs
 * @short_description: Concrete MIME encoders and decoders
 *
 * Base64, quoted-printable, uuencode, yEnc, and pass-through implementations of
 * #GMimeEncoder and #GMimeDecoder.
 **/

/**
 * GMimeBase64Encoder:
 *
 * Incremental Base64 encoder with MIME line wrapping.
 **/
#define GMIME_TYPE_BASE64_ENCODER (g_mime_base64_encoder_get_type ())
G_DECLARE_FINAL_TYPE (GMimeBase64Encoder, g_mime_base64_encoder, GMIME, BASE64_ENCODER, GObject)
/**
 * GMimeBase64Decoder:
 *
 * Incremental Base64 decoder which ignores non-alphabet bytes.
 **/
#define GMIME_TYPE_BASE64_DECODER (g_mime_base64_decoder_get_type ())
G_DECLARE_FINAL_TYPE (GMimeBase64Decoder, g_mime_base64_decoder, GMIME, BASE64_DECODER, GObject)
/**
 * GMimeQuotedPrintableEncoder:
 *
 * Incremental quoted-printable encoder with MIME line wrapping.
 **/
#define GMIME_TYPE_QUOTED_PRINTABLE_ENCODER (g_mime_quoted_printable_encoder_get_type ())
G_DECLARE_FINAL_TYPE (GMimeQuotedPrintableEncoder, g_mime_quoted_printable_encoder, GMIME, QUOTED_PRINTABLE_ENCODER, GObject)
/**
 * GMimeQuotedPrintableDecoder:
 *
 * Incremental body or RFC 2047 quoted-printable decoder.
 **/
#define GMIME_TYPE_QUOTED_PRINTABLE_DECODER (g_mime_quoted_printable_decoder_get_type ())
G_DECLARE_FINAL_TYPE (GMimeQuotedPrintableDecoder, g_mime_quoted_printable_decoder, GMIME, QUOTED_PRINTABLE_DECODER, GObject)
/**
 * GMimeUUEncoder:
 *
 * Incremental uuencode payload encoder.
 **/
#define GMIME_TYPE_UU_ENCODER (g_mime_uu_encoder_get_type ())
G_DECLARE_FINAL_TYPE (GMimeUUEncoder, g_mime_uu_encoder, GMIME, UU_ENCODER, GObject)
/**
 * GMimeUUDecoder:
 *
 * Incremental full-block or payload-only uudecoder.
 **/
#define GMIME_TYPE_UU_DECODER (g_mime_uu_decoder_get_type ())
G_DECLARE_FINAL_TYPE (GMimeUUDecoder, g_mime_uu_decoder, GMIME, UU_DECODER, GObject)
/**
 * GMimePassThroughEncoder:
 *
 * An encoder which copies bytes unchanged.
 **/
#define GMIME_TYPE_PASS_THROUGH_ENCODER (g_mime_pass_through_encoder_get_type ())
G_DECLARE_FINAL_TYPE (GMimePassThroughEncoder, g_mime_pass_through_encoder, GMIME, PASS_THROUGH_ENCODER, GObject)
/**
 * GMimePassThroughDecoder:
 *
 * A decoder which copies bytes unchanged.
 **/
#define GMIME_TYPE_PASS_THROUGH_DECODER (g_mime_pass_through_decoder_get_type ())
G_DECLARE_FINAL_TYPE (GMimePassThroughDecoder, g_mime_pass_through_decoder, GMIME, PASS_THROUGH_DECODER, GObject)

/**
 * GMimeYencEncoder:
 *
 * Incremental yEnc payload encoder with a running CRC-32 checksum.
 * Flush ends the current line but retains the checksum until reset.
 **/
#define GMIME_TYPE_YENC_ENCODER (g_mime_yenc_encoder_get_type ())
G_DECLARE_FINAL_TYPE (GMimeYencEncoder, g_mime_yenc_encoder, GMIME, YENC_ENCODER, GObject)
/**
 * GMimeYencDecoder:
 *
 * Incremental full-block or payload-only yEnc decoder with a running
 * CRC-32 checksum. Header and trailer metadata are not validated.
 **/
#define GMIME_TYPE_YENC_DECODER (g_mime_yenc_decoder_get_type ())
G_DECLARE_FINAL_TYPE (GMimeYencDecoder, g_mime_yenc_decoder, GMIME, YENC_DECODER, GObject)

GMimeBase64Encoder *g_mime_base64_encoder_new (void);
GMimeBase64Encoder *g_mime_base64_encoder_new_with_max_line_length (guint max_line_length);
GMimeBase64Decoder *g_mime_base64_decoder_new (void);
GMimeQuotedPrintableEncoder *g_mime_quoted_printable_encoder_new (void);
GMimeQuotedPrintableEncoder *g_mime_quoted_printable_encoder_new_with_max_line_length (guint max_line_length);
GMimeQuotedPrintableDecoder *g_mime_quoted_printable_decoder_new (gboolean rfc2047);
GMimeUUEncoder *g_mime_uu_encoder_new (void);
GMimeUUDecoder *g_mime_uu_decoder_new (gboolean payload_only);
GMimePassThroughEncoder *g_mime_pass_through_encoder_new (GMimeContentEncoding encoding);
GMimePassThroughDecoder *g_mime_pass_through_decoder_new (GMimeContentEncoding encoding);
GMimeYencEncoder *g_mime_yenc_encoder_new (void);
GMimeYencEncoder *g_mime_yenc_encoder_new_with_max_line_length (guint max_line_length);
guint32 g_mime_yenc_encoder_get_checksum (GMimeYencEncoder *encoder);
GMimeYencDecoder *g_mime_yenc_decoder_new (gboolean payload_only);
guint32 g_mime_yenc_decoder_get_checksum (GMimeYencDecoder *decoder);

G_END_DECLS
#endif
