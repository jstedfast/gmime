/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2002 Ximian, Inc. (www.ximian.com)
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
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */


#ifndef __G_MIME_FILTER_YENC_H__
#define __G_MIME_FILTER_YENC_H__

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#include <gmime/gmime-filter.h>

#define GMIME_TYPE_FILTER_YENC            (g_mime_filter_yenc_get_type ())
#define GMIME_FILTER_YENC(obj)            (GMIME_CHECK_CAST ((obj), GMIME_TYPE_FILTER_YENC, GMimeFilterYenc))
#define GMIME_FILTER_YENC_CLASS(klass)    (GMIME_CHECK_CLASS_CAST ((klass), GMIME_TYPE_FILTER_YENC, GMimeFilterYencClass))
#define GMIME_IS_FILTER_YENC(obj)         (GMIME_CHECK_TYPE ((obj), GMIME_TYPE_FILTER_YENC))
#define GMIME_IS_FILTER_YENC_CLASS(klass) (GMIME_CHECK_CLASS_TYPE ((klass), GMIME_TYPE_FILTER_YENC))
#define GMIME_FILTER_YENC_GET_CLASS(obj)  (GMIME_CHECK_GET_CLASS ((obj), GMIME_TYPE_FILTER_YENC, GMimeFilterYencClass))

typedef struct _GMimeFilterYenc GMimeFilterYenc;
typedef struct _GMimeFilterYencClass GMimeFilterYencClass;

typedef enum {
	GMIME_FILTER_YENC_DIRECTION_ENCODE,
	GMIME_FILTER_YENC_DIRECTION_DECODE,
} GMimeFilterYencDirection;

#define GMIME_YDECODE_STATE_INIT     (0)
#define GMIME_YENCODE_STATE_INIT     (0)

/* first 8 bits are reserved for saving a byte */

/* reserved for use only within g_mime_ydecode_step */
#define GMIME_YDECODE_STATE_EOLN     (1 << 8)
#define GMIME_YDECODE_STATE_ESCAPE   (1 << 9)

/* bits 10 and 11 reserved for later uses? */

#define GMIME_YDECODE_STATE_BEGIN    (1 << 12)
#define GMIME_YDECODE_STATE_PART     (1 << 13)
#define GMIME_YDECODE_STATE_DECODE   (1 << 14)
#define GMIME_YDECODE_STATE_END      (1 << 15)

#define GMIME_YENCODE_CRC_INIT       (~0)
#define GMIME_YENCODE_CRC_FINAL(crc) (~crc)

struct _GMimeFilterYenc {
	GMimeFilter parent_object;
	
	GMimeFilterYencDirection direction;
	
	int part;
	
	int state;
	guint32 pcrc;
	guint32 crc;
};

struct _GMimeFilterYencClass {
	GMimeFilterClass parent_class;
	
};


GType g_mime_filter_yenc_get_type (void);

GMimeFilter *g_mime_filter_yenc_new (GMimeFilterYencDirection direction);

void g_mime_filter_yenc_set_state (GMimeFilterYenc *yenc, int state);
void g_mime_filter_yenc_set_crc (GMimeFilterYenc *yenc, guint32 crc);

/*int     g_mime_filter_yenc_get_part (GMimeFilterYenc *yenc);*/
guint32 g_mime_filter_yenc_get_pcrc (GMimeFilterYenc *yenc);
guint32 g_mime_filter_yenc_get_crc (GMimeFilterYenc *yenc);


size_t g_mime_ydecode_step  (const unsigned char *in, size_t inlen, unsigned char *out,
			     int *state, guint32 *pcrc, guint32 *crc);
size_t g_mime_yencode_step  (const unsigned char *in, size_t inlen, unsigned char *out,
			     int *state, guint32 *pcrc, guint32 *crc);
size_t g_mime_yencode_close (const unsigned char *in, size_t inlen, unsigned char *out,
			     int *state, guint32 *pcrc, guint32 *crc);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GMIME_FILTER_YENC_H__ */
