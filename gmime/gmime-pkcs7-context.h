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


#ifndef __GMIME_PKCS7_CONTEXT_H__
#define __GMIME_PKCS7_CONTEXT_H__

#include <gmime/gmime-crypto-context.h>

G_BEGIN_DECLS

#define GMIME_TYPE_PKCS7_CONTEXT            (g_mime_pkcs7_context_get_type ())
#define GMIME_PKCS7_CONTEXT(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GMIME_TYPE_PKCS7_CONTEXT, GMimePkcs7Context))
#define GMIME_PKCS7_CONTEXT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GMIME_TYPE_PKCS7_CONTEXT, GMimePkcs7ContextClass))
#define GMIME_IS_PKCS7_CONTEXT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GMIME_TYPE_PKCS7_CONTEXT))
#define GMIME_IS_PKCS7_CONTEXT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GMIME_TYPE_PKCS7_CONTEXT))
#define GMIME_PKCS7_CONTEXT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GMIME_TYPE_PKCS7_CONTEXT, GMimePkcs7ContextClass))

typedef struct _GMimePkcs7Context GMimePkcs7Context;
typedef struct _GMimePkcs7ContextClass GMimePkcs7ContextClass;


/**
 * GMimePkcs7Context:
 * @parent_object: parent #GMimeCryptoContext
 * @priv: private context data
 *
 * A PKCS7 crypto context.
 **/
struct _GMimePkcs7Context {
	GMimeCryptoContext parent_object;
	
	struct _GMimePkcs7ContextPrivate *priv;
};

struct _GMimePkcs7ContextClass {
	GMimeCryptoContextClass parent_class;
	
};


GType g_mime_pkcs7_context_get_type (void);

GMimeCryptoContext *g_mime_pkcs7_context_new (GMimePasswordRequestFunc request_passwd);

gboolean g_mime_pkcs7_context_get_always_trust (GMimePkcs7Context *ctx);
void g_mime_pkcs7_context_set_always_trust (GMimePkcs7Context *ctx, gboolean always_trust);

G_END_DECLS

#endif /* __GMIME_PKCS7_CONTEXT_H__ */
