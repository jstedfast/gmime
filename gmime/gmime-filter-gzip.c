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

#include <stdio.h>
#include <string.h>

#include <zlib.h>

#include "gmime-filter-gzip.h"

#ifdef ENABLE_WARNINGS
#define w(x) x
#else
#define w(x)
#endif /* ENABLE_WARNINGS */


#include "gmime-object-private.h"

/**
 * SECTION: gmime-filter-gzip
 * @title: GMimeFilterGZip
 * @short_description: GNU Zip compression/decompression
 * @see_also: #GMimeFilter
 *
 * A #GMimeFilter used for compressing or decompressing a stream using
 * GNU Zip.
 **/


/* rfc1952 */

enum {
	GZIP_FLAG_FTEXT     = (1 << 0),
	GZIP_FLAG_FHCRC     = (1 << 1),
	GZIP_FLAG_FEXTRA    = (1 << 2),
	GZIP_FLAG_FNAME     = (1 << 3),
	GZIP_FLAG_FCOMMENT  = (1 << 4),
	GZIP_FLAG_RESERVED0 = (1 << 5),
	GZIP_FLAG_RESERVED1 = (1 << 6),
	GZIP_FLAG_RESERVED2 = (1 << 7),
};

enum {
	GZIP_OS_FAT,
	GZIP_OS_AMIGA,
	GZIP_OS_VMS,
	GZIP_OS_UNIX,
	GZIP_OS_VM_CMS,
	GZIP_OS_ATARI_TOS,
	GZIP_OS_HPFS,
	GZIP_OS_MACINTOSH,
	GZIP_OS_ZSYSTEM,
	GZIP_OS_CPM,
	GZIP_OS_TOPS20,
	GZIP_OS_NTFS,
	GZIP_OS_QDOS,
	GZIP_OS_ACORN_RISCOS,
	GZIP_OS_UNKNOWN = 255
};

#define GZIP_FLAG_RESERVED (GZIP_FLAG_RESERVED0 | GZIP_FLAG_RESERVED1 | GZIP_FLAG_RESERVED2)

/* http://www.gzip.org/zlib/rfc-gzip.html */
typedef union {
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
} gzip_hdr_t;

typedef union {
	struct {
		guint16 xlen;
		guint16 xlen_nread;
		guint16 crc16;
		
		guint8 got_hdr:1;
		guint8 is_valid:1;
		guint8 got_xlen:1;
		guint8 got_fname:1;
		guint8 got_fcomment:1;
		guint8 got_crc16:1;
	} unzip;
	struct {
		guint32 wrote_hdr:1;
		guint32 flushed:1;
	} zip;
} gzip_state_t;

struct _GMimeFilterGZipPrivate {
	GMimeFilterGZipMode mode;
	int level;

	z_stream *stream;
	
	gzip_state_t state;
	gzip_hdr_t hdr;
	
	char *filename;
	char *comment;
	
	guint32 crc32;
	guint32 isize;
};

static void g_mime_filter_gzip_class_init (GMimeFilterGZipClass *klass);
static void g_mime_filter_gzip_init (GMimeFilterGZip *filter);
static void g_mime_filter_gzip_finalize (GObject *object);

static GMimeFilter *filter_copy (GMimeFilter *filter);
static void filter_filter (GMimeFilter *filter, char *in, size_t len, size_t prespace,
			   char **out, size_t *outlen, size_t *outprespace);
static void filter_complete (GMimeFilter *filter, char *in, size_t len, size_t prespace,
			     char **out, size_t *outlen, size_t *outprespace);
static void filter_reset (GMimeFilter *filter);


static GMimeFilterClass *parent_class = NULL;



G_DEFINE_TYPE_WITH_PRIVATE (GMimeFilterGZip, g_mime_filter_gzip, GMIME_TYPE_FILTER)

GMimeFilterGZipPrivate *
_g_mime_filter_gzip_get_private (GMimeFilterGZip *object)
{
	return g_mime_filter_gzip_get_instance_private (object);
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
g_mime_filter_gzip_init (GMimeFilterGZip *filter)
{
	GMimeFilterGZipPrivate *priv = _g_mime_filter_gzip_get_private (filter);

	priv->stream = g_new0 (z_stream, 1);
	priv->crc32 = crc32 (0, Z_NULL, 0);
}

static void
g_mime_filter_gzip_finalize (GObject *object)
{
	GMimeFilterGZipPrivate *priv = _g_mime_filter_gzip_get_private ((GMimeFilterGZip *) object);
	
	if (priv->mode == GMIME_FILTER_GZIP_MODE_ZIP)
		deflateEnd (priv->stream);
	else
		inflateEnd (priv->stream);
	
	g_free (priv->filename);
	g_free (priv->comment);
	g_free (priv->stream);
	
	G_OBJECT_CLASS (parent_class)->finalize (object);
}


static GMimeFilter *
filter_copy (GMimeFilter *filter)
{
	GMimeFilterGZipPrivate *priv = _g_mime_filter_gzip_get_private ((GMimeFilterGZip *) filter);

	return g_mime_filter_gzip_new (priv->mode, priv->level);
}

static inline size_t
next_alloc_size (size_t n)
{
	return (n + 1023) & ~1023;
}

static void
gzip_filter (GMimeFilter *filter, char *in, size_t len, size_t prespace,
	     char **out, size_t *outlen, size_t *outprespace, gboolean flush)
{
	GMimeFilterGZipPrivate *gzip = _g_mime_filter_gzip_get_private ((GMimeFilterGZip *) filter);
	GMimeFilterPrivate *priv = _g_mime_filter_get_private (filter);
	size_t atleast, olen;
	int retval;
	
	if (gzip->state.zip.flushed) {
		*outprespace = prespace;
		*outlen = 0;
		*out = in;
		return;
	}
	
	if (!gzip->state.zip.wrote_hdr) {
		size_t filenamelen = gzip->filename ? strlen (gzip->filename) + 1 : 0;
		size_t commentlen = gzip->comment ? strlen (gzip->comment) + 1 : 0;
		size_t hdrlen = 10 + filenamelen + commentlen;
		char *outptr;
		
		gzip->hdr.v.id1 = 31;
		gzip->hdr.v.id2 = 139;
		gzip->hdr.v.cm = Z_DEFLATED;
		gzip->hdr.v.mtime = 0;
		gzip->hdr.v.flg = 0;

		if (gzip->filename)
			gzip->hdr.v.flg |= GZIP_FLAG_FNAME;
		if (gzip->comment)
			gzip->hdr.v.flg |= GZIP_FLAG_FCOMMENT;
		if (gzip->level == Z_BEST_COMPRESSION)
			gzip->hdr.v.xfl = 2;
		else if (gzip->level == Z_BEST_SPEED)
			gzip->hdr.v.xfl = 4;
		else
			gzip->hdr.v.xfl = 0;

		gzip->hdr.v.os = GZIP_OS_UNKNOWN;
		
		atleast = next_alloc_size ((len * 2) + hdrlen + 12);
		g_mime_filter_set_size (filter, atleast, FALSE);
		
		memcpy (priv->outbuf, gzip->hdr.buf, 10);
		outptr = priv->outbuf + 10;
		
		if (gzip->filename) {
			memcpy (outptr, gzip->filename, filenamelen);
			outptr += filenamelen;
		}
		
		if (gzip->comment) {
			memcpy (outptr, gzip->comment, commentlen);
			outptr += commentlen;
		}
		
		gzip->stream->next_out = (unsigned char *) outptr;
		gzip->stream->avail_out = priv->outsize - hdrlen;
		
		gzip->state.zip.wrote_hdr = TRUE;
	} else {
		atleast = next_alloc_size ((len * 2) + 12);
		g_mime_filter_set_size (filter, atleast, FALSE);
		
		gzip->stream->next_out = (unsigned char *) priv->outbuf;
		gzip->stream->avail_out = priv->outsize;
	}
	
	gzip->stream->next_in = (unsigned char *) in;
	gzip->stream->avail_in = len;
	
	do {
		retval = deflate (gzip->stream, Z_NO_FLUSH);
		
		if (retval != Z_OK && !(retval == Z_BUF_ERROR && !gzip->stream->avail_in)) {
			w(fprintf (stderr, "gzip: %d: %s\n", retval, gzip->stream->msg));
			break;
		}
		
		if (gzip->stream->avail_out > 0) {
			if (gzip->stream->avail_in > 0) {
				g_mime_filter_backup (filter, (char *) gzip->stream->next_in,
						      gzip->stream->avail_in);
			}
			break;
		}
		
		if (gzip->stream->avail_in == 0)
			break;
		
		olen = priv->outsize - gzip->stream->avail_out;
		atleast = next_alloc_size (olen + gzip->stream->avail_in * 2);
		g_mime_filter_set_size (filter, atleast, TRUE);
		gzip->stream->next_out = (unsigned char *) priv->outbuf + olen;
		gzip->stream->avail_out = priv->outsize - olen;
	} while (1);
	
	if (flush) {
		guint32 val;
		
		do {
			retval = deflate (gzip->stream, Z_FULL_FLUSH);
			
			if (retval != Z_OK && !(retval == Z_BUF_ERROR && !gzip->stream->avail_in)) {
				w(fprintf (stderr, "gzip: %d: %s\n", retval, gzip->stream->msg));
				break;
			}
			
			if (gzip->stream->avail_out > 0)
				break;
			
			olen = priv->outsize;
			g_mime_filter_set_size (filter, olen + 1024, TRUE);
			gzip->stream->next_out = (unsigned char *) priv->outbuf + olen;
			gzip->stream->avail_out = priv->outsize - olen;
		} while (1);
		
		if (gzip->stream->avail_out < 8) {
			olen = priv->outsize;
			g_mime_filter_set_size (filter, olen + 8, TRUE);
			gzip->stream->next_out = (unsigned char *) priv->outbuf + olen;
			gzip->stream->avail_out = priv->outsize - olen;
		}
		
		val = GUINT32_TO_LE (gzip->crc32);
		memcpy (gzip->stream->next_out, &val, 4);
		gzip->stream->avail_out -= 4;
		gzip->stream->next_out += 4;
		
		val = GUINT32_TO_LE (gzip->isize);
		memcpy (gzip->stream->next_out, &val, 4);
		gzip->stream->avail_out -= 4;
		gzip->stream->next_out += 4;
		
		gzip->state.zip.flushed = TRUE;
	}
	
	gzip->crc32 = crc32 (gzip->crc32, (unsigned char *) in, len - gzip->stream->avail_in);
	gzip->isize += len - gzip->stream->avail_in;
	
	*out = priv->outbuf;
	*outlen = priv->outsize - gzip->stream->avail_out;
	*outprespace = priv->outpre;
}

static void
gunzip_filter (GMimeFilter *filter, char *in, size_t len, size_t prespace,
	       char **out, size_t *outlen, size_t *outprespace, gboolean flush)
{
	GMimeFilterGZipPrivate *gzip = _g_mime_filter_gzip_get_private ((GMimeFilterGZip *) filter);
	GMimeFilterPrivate *priv = _g_mime_filter_get_private (filter);
	const char *inend = in + len;
	const char *inptr = in;
	guint16 need, val;
	size_t left = len;
	int retval;
	
	*outprespace = prespace;
	*outlen = 0;
	*out = in;
	
	if (!gzip->state.unzip.got_hdr) {
		if (left < 10) {
			/* not enough data to decode the header */
			g_mime_filter_backup (filter, inptr, left);
			return;
		}
		
		memcpy (gzip->hdr.buf, inptr, 10);
		gzip->state.unzip.got_hdr = TRUE;
		inptr += 10;
		left -= 10;
		
		gzip->state.unzip.is_valid = (gzip->hdr.v.id1 == 31 &&
					      gzip->hdr.v.id2 == 139 &&
					      gzip->hdr.v.cm == Z_DEFLATED);
	}
	
	if (!gzip->state.unzip.is_valid)
		return;
	
	if (gzip->hdr.v.flg & GZIP_FLAG_FEXTRA) {
		if (!gzip->state.unzip.got_xlen) {
			if (left < 2) {
				/* not enough data to get the xlen field */
				g_mime_filter_backup (filter, inptr, left);
				return;
			}
			
			memcpy (&val, inptr, 2);
			gzip->state.unzip.xlen = GUINT16_FROM_LE (val);
			gzip->state.unzip.got_xlen = TRUE;
			inptr += 2;
			left -= 2;
		}
		
		if (gzip->state.unzip.xlen_nread < gzip->state.unzip.xlen) {
			need = gzip->state.unzip.xlen - gzip->state.unzip.xlen_nread;
			
			if (need >= left) {
				gzip->state.unzip.xlen_nread += left;
				return;
			}
			
			gzip->state.unzip.xlen_nread += need;
			inptr += need;
			left -= need;
		}
	}
	
	if ((gzip->hdr.v.flg & GZIP_FLAG_FNAME) && !gzip->state.unzip.got_fname) {
		const char *start = inptr;
		
		while (inptr < inend && *inptr)
			inptr++;
		
		if (inptr == inend) {
			g_mime_filter_backup (filter, start, left);
			return;
		}
		
		g_free (gzip->filename);
		gzip->filename = g_strndup (start, inptr - start);
		gzip->state.unzip.got_fname = TRUE;
		inptr++;
		
		left -= (inptr - start);
	}
	
	if ((gzip->hdr.v.flg & GZIP_FLAG_FCOMMENT) && !gzip->state.unzip.got_fcomment) {
		const char *start = inptr;
		
		while (inptr < inend && *inptr)
			inptr++;
		
		if (inptr == inend) {
			g_mime_filter_backup (filter, start, left);
			return;
		}

		g_free (gzip->comment);
		gzip->comment = g_strndup (start, inptr - start);
		gzip->state.unzip.got_fcomment = TRUE;
		inptr++;
		
		left -= (inptr - start);
	}
	
	if ((gzip->hdr.v.flg & GZIP_FLAG_FHCRC) && !gzip->state.unzip.got_crc16) {
		if (left < 2) {
			g_mime_filter_backup (filter, inptr, left);
			return;
		}
		
		memcpy (&val, inptr, 2);
		gzip->state.unzip.crc16 = GUINT16_FROM_LE (val);
		gzip->state.unzip.got_crc16 = TRUE;
		inptr += 2;
		left -= 2;
	}
	
	if (left == 0 && !flush)
		return;
	
	g_mime_filter_set_size (filter, (left * 2) + 12, FALSE);
	
	gzip->stream->next_in = (unsigned char *) inptr;
	gzip->stream->avail_in = left;

	gzip->stream->next_out = (unsigned char *) priv->outbuf;
	gzip->stream->avail_out = priv->outsize;
	
	do {
		/* FIXME: handle error cases? */
		/* Note: Z_BUF_ERROR is not really an error unless there is input available */
		retval = inflate (gzip->stream, flush ? Z_FULL_FLUSH : Z_NO_FLUSH);
		
		if (retval != Z_OK && !(retval == Z_BUF_ERROR && !gzip->stream->avail_in)) {
			w(fprintf (stderr, "gunzip: %d: %s\n", retval, gzip->stream->msg));
			break;
		}
		
		if (gzip->stream->avail_in == 0)
			break;
		
		if (flush) {
			size_t olen = priv->outsize - gzip->stream->avail_out;
			
			g_mime_filter_set_size (filter, olen + (gzip->stream->avail_in * 2) + 12, TRUE);
			gzip->stream->next_out = (unsigned char *) priv->outbuf + olen;
			gzip->stream->avail_out = priv->outsize - olen;
		} else {
			g_mime_filter_backup (filter, (char *) gzip->stream->next_in,
					      gzip->stream->avail_in);
			break;
		}
	} while (1);
	
	/* FIXME: if we keep this, we could check that the gzip'd
	 * stream is sane, but how would we tell our consumer if it
	 * was/wasn't? */
	/*gzip->crc32 = crc32 (gzip->crc32, in, len - gzip->stream->avail_in - 8);
	  gzip->isize += len - gzip->stream->avail_in - 8;*/
	
	*out = priv->outbuf;
	*outlen = priv->outsize - gzip->stream->avail_out;
	*outprespace = priv->outpre;
}

static void
filter_filter (GMimeFilter *filter, char *in, size_t len, size_t prespace,
	       char **out, size_t *outlen, size_t *outprespace)
{
	GMimeFilterGZipPrivate *priv = _g_mime_filter_gzip_get_private ((GMimeFilterGZip *) filter);

	if (priv->mode == GMIME_FILTER_GZIP_MODE_ZIP)
		gzip_filter (filter, in, len, prespace, out, outlen, outprespace, FALSE);
	else
		gunzip_filter (filter, in, len, prespace, out, outlen, outprespace, FALSE);
}

static void
filter_complete (GMimeFilter *filter, char *in, size_t len, size_t prespace,
		 char **out, size_t *outlen, size_t *outprespace)
{
	GMimeFilterGZipPrivate *priv = _g_mime_filter_gzip_get_private ((GMimeFilterGZip *) filter);

	if (priv->mode == GMIME_FILTER_GZIP_MODE_ZIP)
		gzip_filter (filter, in, len, prespace, out, outlen, outprespace, TRUE);
	else
		gunzip_filter (filter, in, len, prespace, out, outlen, outprespace, TRUE);
}

static void
filter_reset (GMimeFilter *filter)
{
	GMimeFilterGZipPrivate *priv = _g_mime_filter_gzip_get_private ((GMimeFilterGZip *) filter);
	
	memset (&priv->state, 0, sizeof (priv->state));
	
	if (priv->mode == GMIME_FILTER_GZIP_MODE_ZIP) {
		deflateReset (priv->stream);
	} else {
		inflateReset (priv->stream);
		g_free (priv->filename);
		g_free (priv->comment);
		priv->filename = NULL;
		priv->comment = NULL;
	}
	
	priv->crc32 = crc32 (0, Z_NULL, 0);
	priv->isize = 0;
}


/**
 * g_mime_filter_gzip_new:
 * @mode: zip or unzip
 * @level: compression level
 *
 * Creates a new gzip (or gunzip) filter.
 *
 * Returns: a new gzip (or gunzip) filter.
 **/
GMimeFilter *
g_mime_filter_gzip_new (GMimeFilterGZipMode mode, int level)
{
	GMimeFilterGZipPrivate *priv;
	GMimeFilterGZip *gzip;
	int retval;
	
	gzip = g_object_new (GMIME_TYPE_FILTER_GZIP, NULL);
	priv = _g_mime_filter_gzip_get_private (gzip);

	priv->mode = mode;
	priv->level = level;
	
	if (mode == GMIME_FILTER_GZIP_MODE_ZIP)
		retval = deflateInit2 (priv->stream, level, Z_DEFLATED, -MAX_WBITS, MAX_MEM_LEVEL, Z_DEFAULT_STRATEGY);
	else
		retval = inflateInit2 (priv->stream, -MAX_WBITS);
	
	if (retval != Z_OK) {
		g_object_unref (gzip);
		return NULL;
	}
	
	return (GMimeFilter *) gzip;
}


/**
 * g_mime_filter_gzip_get_filename:
 * @gzip: A #GMimeFilterGZip filter
 *
 * Gets the filename that was either previously set or retrieved when decoding a gzip stream.
 *
 * Returns: a string containing th ename of the file.
 *
 * Since: 3.2
 **/
const char *
g_mime_filter_gzip_get_filename (GMimeFilterGZip *gzip)
{
	GMimeFilterGZipPrivate *priv;

	g_return_val_if_fail (GMIME_IS_FILTER_GZIP (gzip), NULL);
	
	priv = _g_mime_filter_gzip_get_private (gzip);

	return priv->filename;
}


/**
 * g_mime_filter_gzip_set_filename:
 * @gzip: A #GMimeFilterGZip filter
 * @filename: The name of the file
 *
 * Sets the filename that should be used when generating the gzip header.
 *
 * Since: 3.2
 **/
void
g_mime_filter_gzip_set_filename (GMimeFilterGZip *gzip, const char *filename)
{
	GMimeFilterGZipPrivate *priv;
	char *buf;
	
	g_return_if_fail (GMIME_IS_FILTER_GZIP (gzip));
	
	buf = g_strdup (filename);
	priv = _g_mime_filter_gzip_get_private (gzip);

	g_free (priv->filename);
	priv->filename = buf;
}


/**
 * g_mime_filter_gzip_get_comment:
 * @gzip: A #GMimeFilterGZip filter
 *
 * Gets the comment that was either previously set or retrieved when decoding a gzip stream.
 *
 * Returns: a string containing the comment.
 *
 * Since: 3.2
 **/
const char *
g_mime_filter_gzip_get_comment (GMimeFilterGZip *gzip)
{
	GMimeFilterGZipPrivate *priv;

	g_return_val_if_fail (GMIME_IS_FILTER_GZIP (gzip), NULL);
	
	priv = _g_mime_filter_gzip_get_private (gzip);

	return priv->comment;
}


/**
 * g_mime_filter_gzip_set_comment:
 * @gzip: A #GMimeFilterGZip filter
 * @comment: The comment
 *
 * Sets the comment that should be used when generating the gzip header.
 *
 * Since: 3.2
 **/
void
g_mime_filter_gzip_set_comment (GMimeFilterGZip *gzip, const char *comment)
{
	GMimeFilterGZipPrivate *priv;
	char *buf;
	
	g_return_if_fail (GMIME_IS_FILTER_GZIP (gzip));
	
	buf = g_strdup (comment);
	priv = _g_mime_filter_gzip_get_private (gzip);

	g_free (priv->comment);
	priv->comment = buf;
}
