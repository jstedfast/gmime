/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2001-2004 Ximian, Inc. (www.ximian.com)
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


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <string.h>

#include <zlib.h>

#include "gmime-filter-gzip.h"


/* rfc1952 */

enum {
	GZIP_FLAG_FTEXT    = (1 << 0),
	GZIP_FLAG_FHCRC    = (1 << 1),
	GZIP_FLAG_FEXTRA   = (1 << 2),
	GZIP_FLAG_FNAME    = (1 << 3),
	GZIP_FLAG_FCOMMENT = (1 << 4),
	/* reserved */
	/* reserved */
	/* reserved */
};

struct _GMimeFilterGZipPrivate {
	z_stream *stream;
	
	union {
		unsigned char buf[10];
		struct {
			guint8 id1;
			guint8 id2;
			guint8 cm;
			guint8 flg;
			guint32 mtime;
			guint8 xfl;
			guint8 os;
		} v;
	} hdr;
	
	union {
		struct {
			guint16 xlen;
			guint16 xlen_nread;
			guint16 crc16;
			
			guint8 got_hdr:1;
			guint8 got_xlen:1;
			guint8 got_fname:1;
			guint8 got_fcomment:1;
			guint8 got_crc16:1;
		} unzip;
		struct {
			guint32 wrote_hdr:1;
		} zip;
	} state;
};

static void g_mime_filter_gzip_class_init (GMimeFilterGZipClass *klass);
static void g_mime_filter_gzip_init (GMimeFilterGZip *filter, GMimeFilterGZipClass *klass);
static void g_mime_filter_gzip_finalize (GObject *object);

static GMimeFilter *filter_copy (GMimeFilter *filter);
static void filter_filter (GMimeFilter *filter, char *in, size_t len, size_t prespace,
			   char **out, size_t *outlen, size_t *outprespace);
static void filter_complete (GMimeFilter *filter, char *in, size_t len, size_t prespace,
			     char **out, size_t *outlen, size_t *outprespace);
static void filter_reset (GMimeFilter *filter);


static GMimeFilterClass *parent_class = NULL;


GType
g_mime_filter_gzip_get_type (void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof (GMimeFilterGZipClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc) g_mime_filter_gzip_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (GMimeFilterGZip),
			0,    /* n_preallocs */
			(GInstanceInitFunc) g_mime_filter_gzip_init,
		};
		
		type = g_type_register_static (GMIME_TYPE_FILTER, "GMimeFilterGZip", &info, 0);
	}
	
	return type;
}


static void
g_mime_filter_gzip_class_init (GMimeFilterGZipClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GMimeFilterClass *filter_class = GMIME_FILTER_CLASS (klass);
	
	parent_class = g_type_class_ref (GMIME_TYPE_FILTER);
	
	object_class->finalize = g_mime_filter_gzip_finalize;
	
	filter_class->copy = filter_copy;
	filter_class->filter = filter_filter;
	filter_class->complete = filter_complete;
	filter_class->reset = filter_reset;
}

static void
g_mime_filter_gzip_init (GMimeFilterGZip *filter, GMimeFilterGZipClass *klass)
{
	filter->priv = g_new0 (struct _GMimeFilterGZipPrivate, 1);
	filter->priv->stream = g_new0 (z_stream, 1);
}

static void
g_mime_filter_gzip_finalize (GObject *object)
{
	G_OBJECT_CLASS (parent_class)->finalize (object);
}


static GMimeFilter *
filter_copy (GMimeFilter *filter)
{
	GMimeFilterGZip *gzip = (GMimeFilterGZip *) filter;
	
	return g_mime_filter_gzip_new (gzip->mode, gzip->level);
}

static void
gzip_filter (GMimeFilter *filter, char *in, size_t len, size_t prespace,
	     char **out, size_t *outlen, size_t *outprespace, int flush)
{
	GMimeFilterGZip *gzip = (GMimeFilterGZip *) filter;
	struct _GMimeFilterGZipPrivate *priv = gzip->priv;
	int retval;
	
	if (!priv->state.zip.wrote_hdr) {
		priv->hdr.v.id1 = 31;
		priv->hdr.v.id2 = 139;
		priv->hdr.v.cm = 8;
		priv->hdr.v.mtime = 0;
		priv->hdr.v.flg = 0;
		priv->hdr.v.xfl = 0;
		priv->hdr.v.os = 255;
		
		g_mime_filter_set_size (filter, (len * 2) + 22, FALSE);
		
		memcpy (filter->outbuf, priv->hdr.buf, 10);
		
		priv->stream->next_out = filter->outbuf + 10;
		priv->stream->avail_out = filter->outsize - 10;
		
		priv->state.zip.wrote_hdr = TRUE;
	} else {
		g_mime_filter_set_size (filter, (len * 2) + 12, FALSE);
		
		priv->stream->next_out = filter->outbuf;
		priv->stream->avail_out = filter->outsize;
	}
	
	priv->stream->next_in = in;
	priv->stream->avail_in = len;
	
	do {
		/* FIXME: handle error cases? */
		if ((retval = deflate (priv->stream, flush)) != Z_OK)
			fprintf (stderr, "gzip: %d: %s\n", retval, priv->stream->msg);
		
		if (flush == Z_FULL_FLUSH) {
			size_t outlen;
			
			if (priv->stream->avail_in == 0)
				break;
			
			outlen = filter->outsize - priv->stream->avail_out;
			g_mime_filter_set_size (filter, outlen + (priv->stream->avail_in * 2) + 12, TRUE);
			priv->stream->avail_out = filter->outsize - outlen;
			priv->stream->next_out = filter->outbuf + outlen;
		} else {
			if (priv->stream->avail_in > 0)
				g_mime_filter_backup (filter, priv->stream->next_in, priv->stream->avail_in);
			
			break;
		}
	} while (1);
	
	*out = filter->outbuf;
	*outlen = filter->outsize - priv->stream->avail_out;
	*outprespace = filter->outpre;
}

static void
gunzip_filter (GMimeFilter *filter, char *in, size_t len, size_t prespace,
	       char **out, size_t *outlen, size_t *outprespace, int flush)
{
	GMimeFilterGZip *gzip = (GMimeFilterGZip *) filter;
	struct _GMimeFilterGZipPrivate *priv = gzip->priv;
	int retval;
	
	if (!priv->state.unzip.got_hdr) {
		if (len < 10) {
			g_mime_filter_backup (filter, in, len);
			return;
		}
		
		memcpy (priv->hdr.buf, in, 10);
		priv->state.unzip.got_hdr = TRUE;
		len -= 10;
		in += 10;
		
		/* FIXME: validate id1, id2, and cm? */
	}
	
	if (priv->hdr.v.flg & GZIP_FLAG_FEXTRA) {
		if (!priv->state.unzip.got_xlen) {
			if (len < 2) {
				g_mime_filter_backup (filter, in, len);
				return;
			}
			
			memcpy (&priv->state.unzip.xlen, in, 2);
			priv->state.unzip.got_xlen = TRUE;
			len -= 2;
			in += 2;
		}
		
		if (priv->state.unzip.xlen_nread < priv->state.unzip.xlen) {
			guint16 need = priv->state.unzip.xlen - priv->state.unzip.xlen_nread;
			
			if (need < len) {
				priv->state.unzip.xlen_nread += need;
				len -= need;
				in += need;
			} else {
				priv->state.unzip.xlen_nread += len;
				return;
			}
		}
	}
	
	if ((priv->hdr.v.flg & GZIP_FLAG_FNAME) && !priv->state.unzip.got_fname) {
		while (*in && len > 0) {
			len--;
			in++;
		}
		
		if (*in == '\0' && len > 0) {
			priv->state.unzip.got_fname = TRUE;
			len--;
			in++;
		}
		
		if (len == 0)
			return;
	}
	
	if ((priv->hdr.v.flg & GZIP_FLAG_FCOMMENT) && !priv->state.unzip.got_fcomment) {
		while (*in && len > 0) {
			len--;
			in++;
		}
		
		if (*in == '\0' && len > 0) {
			priv->state.unzip.got_fcomment = TRUE;
			len--;
			in++;
		}
		
		if (len == 0)
			return;
	}
	
	if ((priv->hdr.v.flg & GZIP_FLAG_FHCRC) && !priv->state.unzip.got_crc16) {
		if (len < 2) {
			g_mime_filter_backup (filter, in, len);
			return;
		}
		
		memcpy (&priv->state.unzip.crc16, in, 2);
		len -= 2;
		in += 2;
	}
	
	if (len == 0)
		return;
	
	g_mime_filter_set_size (filter, (len * 2) + 12, FALSE);
	
	priv->stream->next_in = in;
	priv->stream->avail_in = len;
	
	if (priv->hdr.v.flg & GZIP_FLAG_FHCRC)
		priv->stream->avail_in -= 8;
	
	priv->stream->next_out = filter->outbuf;
	priv->stream->avail_out = filter->outsize;
	
	do {
		/* FIXME: handle error cases? */
		if ((retval = inflate (priv->stream, flush)) != Z_OK)
			fprintf (stderr, "gunzip: %d: %s\n", retval, priv->stream->msg);
		
		if (flush == Z_FULL_FLUSH) {
			size_t outlen;
			
			if (priv->stream->avail_in == 0)
				break;
			
			outlen = filter->outsize - priv->stream->avail_out;
			g_mime_filter_set_size (filter, outlen + (priv->stream->avail_in * 2) + 12, TRUE);
			priv->stream->avail_out = filter->outsize - outlen;
			priv->stream->next_out = filter->outbuf + outlen;
		} else {
			if (priv->hdr.v.flg & GZIP_FLAG_FHCRC)
				priv->stream->avail_in += 8;
			
			if (priv->stream->avail_in > 0)
				g_mime_filter_backup (filter, priv->stream->next_in, priv->stream->avail_in);
			
			break;
		}
	} while (1);
	
	*out = filter->outbuf;
	*outlen = filter->outsize - priv->stream->avail_out;
	*outprespace = filter->outpre;
}

static void
filter_filter (GMimeFilter *filter, char *in, size_t len, size_t prespace,
	       char **out, size_t *outlen, size_t *outprespace)
{
	GMimeFilterGZip *gzip = (GMimeFilterGZip *) filter;
	
	if (gzip->mode == GMIME_FILTER_GZIP_MODE_ZIP)
		gzip_filter (filter, in, len, prespace, out, outlen, outprespace, Z_SYNC_FLUSH);
	else
		gunzip_filter (filter, in, len, prespace, out, outlen, outprespace, Z_SYNC_FLUSH);
}

static void
filter_complete (GMimeFilter *filter, char *in, size_t len, size_t prespace,
		 char **out, size_t *outlen, size_t *outprespace)
{
	GMimeFilterGZip *gzip = (GMimeFilterGZip *) filter;
	
	if (gzip->mode == GMIME_FILTER_GZIP_MODE_ZIP)
		gzip_filter (filter, in, len, prespace, out, outlen, outprespace, Z_FULL_FLUSH);
	else
		gunzip_filter (filter, in, len, prespace, out, outlen, outprespace, Z_FULL_FLUSH);
}

/* should this 'flush' outstanding state/data bytes? */
static void
filter_reset (GMimeFilter *filter)
{
	GMimeFilterGZip *gzip = (GMimeFilterGZip *) filter;
	struct _GMimeFilterGZipPrivate *priv = gzip->priv;
	
	memset (&priv->state, 0, sizeof (priv->state));
	
	if (gzip->mode == GMIME_FILTER_GZIP_MODE_ZIP)
		deflateReset (priv->stream);
	else
		inflateReset (priv->stream);
}


/**
 * g_mime_filter_gzip_new:
 * @mode: zip or unzip
 * @level: compression level
 *
 * Creates a new gzip (or gunzip) filter.
 *
 * Returns a new gzip (or gunzip) filter.
 **/
GMimeFilter *
g_mime_filter_gzip_new (GMimeFilterGZipMode mode, int level)
{
	GMimeFilterGZip *new;
	
	new = g_object_new (GMIME_TYPE_FILTER_GZIP, NULL, NULL);
	new->mode = mode;
	new->level = level;
	
	if (mode == GMIME_FILTER_GZIP_MODE_ZIP)
		deflateInit (new->priv->stream, level);
	else
		inflateInit (new->priv->stream);
	
	return (GMimeFilter *) new;
}
