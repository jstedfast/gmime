/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2008 Jeffrey Stedfast
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
#include <gmime/gmime-multipart.h>
#include <gmime/gmime-multipart-encrypted.h>
#include <gmime/gmime-multipart-signed.h>
#include <gmime/gmime-message.h>
#include <gmime/gmime-message-part.h>
#include <gmime/gmime-message-partial.h>
#include <gmime/internet-address.h>
#include <gmime/gmime-parser.h>
#include <gmime/gmime-utils.h>
#include <gmime/gmime-stream.h>
#include <gmime/gmime-stream-buffer.h>
#include <gmime/gmime-stream-cat.h>
#include <gmime/gmime-stream-file.h>
#include <gmime/gmime-stream-filter.h>
#include <gmime/gmime-stream-fs.h>
#include <gmime/gmime-stream-mem.h>
#include <gmime/gmime-stream-mmap.h>
#include <gmime/gmime-stream-null.h>
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
#include <gmime/gmime-session.h>
#include <gmime/gmime-session-simple.h>
#include <gmime/gmime-cipher-context.h>
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


/**
 * GMIME_CHECK_VERSION:
 * @major: Minimum major version
 * @minor: Minimum minor version
 * @micro: Minimum micro version
 *
 * Macro that just calls g_mime_check_version()
 **/
#define GMIME_CHECK_VERSION(major,minor,micro) g_mime_check_version (major, minor, micro)

gboolean g_mime_check_version (guint major, guint minor, guint micro);


/**
 * GMIME_INIT_FLAG_UTF8:
 *
 * Initialization flag to enable UTF-8 interfaces throughout GMime.
 *
 * Note: this flag is really a no-op and remains only for backward
 * compatablity. Interfaces will be UTF-8 whether this flag is used or
 * not.
 **/
#define GMIME_INIT_FLAG_UTF8  (1 << 0)

void g_mime_init (guint32 flags);
void g_mime_shutdown (void);

G_END_DECLS

#endif /* __GMIME_H__ */
