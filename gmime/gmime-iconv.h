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


#ifndef __GMIME_ICONV_H__
#define __GMIME_ICONV_H__

#include <glib.h>
#include <iconv.h>

G_BEGIN_DECLS

void g_mime_iconv_init (void);
void g_mime_iconv_shutdown (void);

iconv_t g_mime_iconv_open (const char *to, const char *from);


/**
 * g_mime_iconv:
 * @cd: iconv_t conversion descriptor
 * @inbuf: input buffer
 * @inleft: number of bytes left in @inbuf
 * @outbuf: output buffer
 * @outleft: number of bytes left in @outbuf
 *
 * The argument @cd must be a conversion descriptor created using the
 * function #g_mime_iconv_open.
 *
 * The main case is when @inbuf is not %NULL and *inbuf is not
 * %NULL. In this case, the #g_mime_iconv function converts the
 * multibyte sequence starting at *inbuf to a multibyte sequence
 * starting at *outbuf. At most *inleft bytes, starting at *inbuf,
 * will be read. At most *outleft bytes, starting at *outbuf, will
 * be written.
 *
 * The #g_mime_iconv function converts one multibyte character at a
 * time, and for each character conversion it increments *inbuf and
 * decrements *inleft by the number of converted input bytes, it
 * increments *outbuf and decrements *outleft by the number of
 * converted output bytes, and it updates the conversion state
 * contained in @cd. The conversion can stop for four reasons:
 *
 * 1. An invalid multibyte sequence is encountered in the input. In
 * this case it sets errno to %EILSEQ and returns (size_t)(-1).
 * *inbuf is left pointing to the beginning of the invalid multibyte
 * sequence.
 *
 * 2. The input byte sequence has been entirely converted, i.e.
 * *inleft has gone down to %0. In this case #g_mime_iconv returns
 * the number of non-reversible conversions performed during this
 * call.
 *
 * 3. An incomplete multibyte sequence is encountered in the input,
 * and the input byte sequence terminates after it. In this case it
 * sets errno to %EINVAL and returns (size_t)(-1). *inbuf is left
 * pointing to the beginning of the incomplete multibyte sequence.
 *
 * 4. The output buffer has no more room for the next converted
 * character. In this case it sets errno to %E2BIG and returns
 * (size_t)(-1).
 *
 * A different case is when @inbuf is %NULL or *inbuf is %NULL, but
 * @outbuf is not %NULL and *outbuf is not %NULL. In this case, the
 * #g_mime_iconv function attempts to set @cd's conversion state to
 * the initial state and store a corresponding shift sequence at
 * *outbuf.  At most *outleft bytes, starting at *outbuf, will be
 * written.  If the output buffer has no more room for this reset
 * sequence, it sets errno to %E2BIG and returns (size_t)(-1).
 * Otherwise it increments *outbuf and decrements *outleft by the
 * number of bytes written.
 *
 * A third case is when @inbuf is %NULL or *inbuf is %NULL, and
 * @outbuf is %NULL or *outbuf is %NULL. In this case, the
 * #g_mime_iconv function sets @cd's conversion state to the initial
 * state.
 *
 * Returns: the number of characters converted in a nonreversible way
 * during this call; reversible conversions are not counted. In case
 * of error, it sets errno and returns (size_t)(-1).
 **/
#define g_mime_iconv(cd,inbuf,inleft,outbuf,outleft) iconv (cd, inbuf, inleft, outbuf, outleft)

int g_mime_iconv_close (iconv_t cd);

G_END_DECLS

#endif /* __GMIME_ICONV_H__ */
