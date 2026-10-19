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
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef __GMIME_TEXT_RFC822_HEADERS_H__
#define __GMIME_TEXT_RFC822_HEADERS_H__

#include <gmime/gmime-message-part.h>

G_BEGIN_DECLS

#define GMIME_TYPE_TEXT_RFC822_HEADERS            (g_mime_text_rfc822_headers_get_type ())
#define GMIME_TEXT_RFC822_HEADERS(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GMIME_TYPE_TEXT_RFC822_HEADERS, GMimeTextRfc822Headers))
#define GMIME_TEXT_RFC822_HEADERS_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GMIME_TYPE_TEXT_RFC822_HEADERS, GMimeTextRfc822HeadersClass))
#define GMIME_IS_TEXT_RFC822_HEADERS(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GMIME_TYPE_TEXT_RFC822_HEADERS))
#define GMIME_IS_TEXT_RFC822_HEADERS_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GMIME_TYPE_TEXT_RFC822_HEADERS))
#define GMIME_TEXT_RFC822_HEADERS_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GMIME_TYPE_TEXT_RFC822_HEADERS, GMimeTextRfc822HeadersClass))

typedef struct _GMimeTextRfc822Headers GMimeTextRfc822Headers;
typedef struct _GMimeTextRfc822HeadersClass GMimeTextRfc822HeadersClass;

/**
 * GMimeTextRfc822Headers:
 * @parent_object: parent #GMimeMessagePart
 *
 * A text/rfc822-headers entity containing an embedded message's headers.
 **/
struct _GMimeTextRfc822Headers {
	GMimeMessagePart parent_object;
};

struct _GMimeTextRfc822HeadersClass {
	GMimeMessagePartClass parent_class;
};

GType g_mime_text_rfc822_headers_get_type (void);

GMimeTextRfc822Headers *g_mime_text_rfc822_headers_new (void);

G_END_DECLS

#endif /* __GMIME_TEXT_RFC822_HEADERS_H__ */
