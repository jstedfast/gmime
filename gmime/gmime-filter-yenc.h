/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2014 Jeffrey Stedfast
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


#ifndef __GMIME_FILTER_YENC_H__
#define __GMIME_FILTER_YENC_H__

#include <gmime/gmime-filter.h>

G_BEGIN_DECLS

#define GMIME_TYPE_FILTER_YENC            (g_mime_filter_yenc_get_type ())
#define GMIME_FILTER_YENC(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GMIME_TYPE_FILTER_YENC, GMimeFilterYenc))
#define GMIME_FILTER_YENC_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GMIME_TYPE_FILTER_YENC, GMimeFilterYencClass))
#define GMIME_IS_FILTER_YENC(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GMIME_TYPE_FILTER_YENC))
#define GMIME_IS_FILTER_YENC_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GMIME_TYPE_FILTER_YENC))
#define GMIME_FILTER_YENC_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GMIME_TYPE_FILTER_YENC, GMimeFilterYencClass))

typedef struct _GMimeFilterYenc GMimeFilterYenc;
typedef struct _GMimeFilterYencClass GMimeFilterYencClass;


/**
 * GMIME_YDECODE_STATE_INIT:
 *
 * Initial state for the g_mime_ydecode_step() function.
 **/
#define GMIME_YDECODE_STATE_INIT     (0)

/**
 * GMIME_YENCODE_STATE_INIT:
 *
 * Initial state for the g_mime_ydecode_step() function.
 **/
#define GMIME_YENCODE_STATE_INIT     (0)

/* first 8 bits are reserved for saving a byte */

/**
 * GMIME_YDECODE_STATE_EOLN:
 *
 * State bit that denotes the yEnc filter has reached an end-of-line.
 *
 * This state is for internal use only.
 **/
#define GMIME_YDECODE_STATE_EOLN     (1 << 8)

/**
 * GMIME_YDECODE_STATE_ESCAPE:
 *
 * State bit that denotes the yEnc filter has reached an escape
 * sequence.
 *
 * This state is for internal use only.
 **/
#define GMIME_YDECODE_STATE_ESCAPE   (1 << 9)

/* bits 10 and 11 reserved for later uses? */

/**
 * GMIME_YDECODE_STATE_BEGIN:
 *
 * State bit that denotes the yEnc filter has found the =ybegin line.
 **/
#define GMIME_YDECODE_STATE_BEGIN    (1 << 12)

/**
 * GMIME_YDECODE_STATE_PART:
 *
 * State bit that denotes the yEnc filter has found the =ypart
 * line. (Note: not all yencoded blocks have one)
 **/
#define GMIME_YDECODE_STATE_PART     (1 << 13)

/**
 * GMIME_YDECODE_STATE_DECODE:
 *
 * State bit that denotes yEnc filter has begun decoding the actual
 * yencoded content and will continue to do so until an =yend line is
 * found (or until there is nothing left to decode).
 **/
#define GMIME_YDECODE_STATE_DECODE   (1 << 14)

/**
 * GMIME_YDECODE_STATE_END:
 *
 * State bit that denoates that g_mime_ydecode_step() has finished
 * decoding.
 **/
#define GMIME_YDECODE_STATE_END      (1 << 15)

/**
 * GMIME_YENCODE_CRC_INIT:
 *
 * Initial state for the crc and pcrc state variables.
 **/
#define GMIME_YENCODE_CRC_INIT       (~0)

/**
 * GMIME_YENCODE_CRC_FINAL:
 * @crc: crc or pcrc state variable
 *
 * Gets the final crc value from @crc.
 **/
#define GMIME_YENCODE_CRC_FINAL(crc) (~crc)

/**
 * GMimeFilterYenc:
 * @parent_object: parent #GMimeFilter
 * @encode: encode vs decode
 * @part: part id
 * @state: encode/decode state
 * @pcrc: part crc
 * @crc: full crc
 *
 * A filter for yEncoding or yDecoding a stream.
 **/
struct _GMimeFilterYenc {
	GMimeFilter parent_object;
	
	gboolean encode;
	
	int part;
	
	int state;
	guint32 pcrc;
	guint32 crc;
};

struct _GMimeFilterYencClass {
	GMimeFilterClass parent_class;
	
};


GType g_mime_filter_yenc_get_type (void);

GMimeFilter *g_mime_filter_yenc_new (gboolean encode);

void g_mime_filter_yenc_set_state (GMimeFilterYenc *yenc, int state);
void g_mime_filter_yenc_set_crc (GMimeFilterYenc *yenc, guint32 crc);

/*int     g_mime_filter_yenc_get_part (GMimeFilterYenc *yenc);*/
guint32 g_mime_filter_yenc_get_pcrc (GMimeFilterYenc *yenc);
guint32 g_mime_filter_yenc_get_crc (GMimeFilterYenc *yenc);


size_t g_mime_ydecode_step  (const unsigned char *inbuf, size_t inlen, unsigned char *outbuf,
			     int *state, guint32 *pcrc, guint32 *crc);
size_t g_mime_yencode_step  (const unsigned char *inbuf, size_t inlen, unsigned char *outbuf,
			     int *state, guint32 *pcrc, guint32 *crc);
size_t g_mime_yencode_close (const unsigned char *inbuf, size_t inlen, unsigned char *outbuf,
			     int *state, guint32 *pcrc, guint32 *crc);

G_END_DECLS

#endif /* __GMIME_FILTER_YENC_H__ */
