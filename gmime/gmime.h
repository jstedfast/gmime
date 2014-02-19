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


#ifndef __GMIME_H__
#define __GMIME_H__

#include <glib.h>
#include <gmime/gmime-version.h>
#include <gmime/gmime-error.h>
#include <gmime/gmime-charset.h>
#include <gmime/gmime-iconv.h>
#include <gmime/gmime-iconv-utils.h>
#include <gmime/gmime-param.h>
#include <gmime/gmime-content-type.h>
#include <gmime/gmime-disposition.h>
#include <gmime/gmime-data-wrapper.h>
#include <gmime/gmime-object.h>
#include <gmime/gmime-part.h>
#include <gmime/gmime-part-iter.h>
#include <gmime/gmime-multipart.h>
#include <gmime/gmime-multipart-encrypted.h>
#include <gmime/gmime-multipart-signed.h>
#include <gmime/gmime-message.h>
#include <gmime/gmime-message-part.h>
#include <gmime/gmime-message-partial.h>
#include <gmime/internet-address.h>
#include <gmime/gmime-encodings.h>
#include <gmime/gmime-parser.h>
#include <gmime/gmime-utils.h>
#include <gmime/gmime-stream.h>
#include <gmime/gmime-stream-buffer.h>
#include <gmime/gmime-stream-cat.h>
#include <gmime/gmime-stream-file.h>
#include <gmime/gmime-stream-filter.h>
#include <gmime/gmime-stream-fs.h>
#include <gmime/gmime-stream-gio.h>
#include <gmime/gmime-stream-mem.h>
#include <gmime/gmime-stream-mmap.h>
#include <gmime/gmime-stream-null.h>
#include <gmime/gmime-stream-pipe.h>
#include <gmime/gmime-filter.h>
#include <gmime/gmime-filter-basic.h>
#include <gmime/gmime-filter-best.h>
#include <gmime/gmime-filter-charset.h>
#include <gmime/gmime-filter-crlf.h>
#include <gmime/gmime-filter-enriched.h>
#include <gmime/gmime-filter-from.h>
#include <gmime/gmime-filter-gzip.h>
#include <gmime/gmime-filter-html.h>
#include <gmime/gmime-filter-md5.h>
#include <gmime/gmime-filter-strip.h>
#include <gmime/gmime-filter-windows.h>
#include <gmime/gmime-filter-yenc.h>
#include <gmime/gmime-crypto-context.h>
#include <gmime/gmime-pkcs7-context.h>
#include <gmime/gmime-gpg-context.h>

G_BEGIN_DECLS

/* GMIME version */

/**
 * gmime_major_version:
 *
 * GMime's major version.
 **/
extern const guint gmime_major_version;

/**
 * gmime_minor_version:
 *
 * GMime's minor version.
 **/
extern const guint gmime_minor_version;

/**
 * gmime_micro_version:
 *
 * GMime's micro version.
 **/
extern const guint gmime_micro_version;

/**
 * gmime_interface_age:
 *
 * GMime's interface age.
 **/
extern const guint gmime_interface_age;

/**
 * gmime_binary_age:
 *
 * GMime's binary age.
 **/
extern const guint gmime_binary_age;

gboolean g_mime_check_version (guint major, guint minor, guint micro);


/**
 * GMIME_ENABLE_RFC2047_WORKAROUNDS:
 *
 * Initialization flag to enable workarounds for badly formed rfc2047
 * encoded-words.
 **/
#define GMIME_ENABLE_RFC2047_WORKAROUNDS     (1 << 0)

/**
 * GMIME_ENABLE_USE_ONLY_USER_CHARSETS:
 *
 * Initialization flag that hints to the rfc2047 encoder to use only
 * the configured user-charsets (set via g_mime_set_user_charsets())
 * instead of trying to first use iso-8859-1.
 *
 * Since: 2.6.16
 **/
#define GMIME_ENABLE_USE_ONLY_USER_CHARSETS  (1 << 1)

void g_mime_init (guint32 flags);
void g_mime_shutdown (void);

G_END_DECLS

#endif /* __GMIME_H__ */
