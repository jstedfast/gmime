/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2001 Ximian, Inc. (www.ximian.com)
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


#ifndef __GMIME_CIPHER_CONTEXT_H__
#define __GMIME_CIPHER_CONTEXT_H__

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#include <glib.h>
#include <glib-object.h>

#include "gmime-stream.h"
#include "gmime-session.h"
#include "gmime-exception.h"

#define GMIME_TYPE_CIPHER_CONTEXT            (g_mime_cipher_context_get_type ())
#define GMIME_CIPHER_CONTEXT(obj)            (GMIME_CHECK_CAST ((obj), GMIME_TYPE_CIPHER_CONTEXT, GMimeCipherContext))
#define GMIME_CIPHER_CONTEXT_CLASS(klass)    (GMIME_CHECK_CLASS_CAST ((klass), GMIME_TYPE_CIPHER_CONTEXT, GMimeCipherContextClass))
#define GMIME_IS_CIPHER_CONTEXT(obj)         (GMIME_CHECK_TYPE ((obj), GMIME_TYPE_CIPHER_CONTEXT))
#define GMIME_IS_CIPHER_CONTEXT_CLASS(klass) (GMIME_CHECK_CLASS_TYPE ((klass), GMIME_TYPE_CIPHER_CONTEXT))
#define GMIME_CIPHER_CONTEXT_GET_CLASS(obj)  (GMIME_CHECK_GET_CLASS ((obj), GMIME_TYPE_CIPHER_CONTEXT, GMimeCipherContextClass))

typedef struct _GMimeCipherContext GMimeCipherContext;
typedef struct _GMimeCipherContextClass GMimeCipherContextClass;

typedef struct _GMimeCipherValidity GMimeCipherValidity;

typedef enum {
	GMIME_CIPHER_HASH_DEFAULT,
	GMIME_CIPHER_HASH_MD2,
	GMIME_CIPHER_HASH_MD5,
	GMIME_CIPHER_HASH_SHA1,
	GMIME_CIPHER_HASH_RIPEMD160,
	GMIME_CIPHER_HASH_TIGER192,
	GMIME_CIPHER_HASH_HAVAL5160
} GMimeCipherHash;

struct _GMimeCipherContext {
	GObject parent_object;
	
	GMimeSession *session;
	
	/* these must be set by the subclass */
	const char *sign_protocol;
	const char *encrypt_protocol;
	const char *key_protocol;
};

struct _GMimeCipherContextClass {
	GObjectClass parent_class;
	
	GMimeCipherHash       (*hash_id)     (GMimeCipherContext *ctx, const char *hash);
	
	const char *          (*hash_name)   (GMimeCipherContext *ctx, GMimeCipherHash hash);
	
	int                   (*sign)        (GMimeCipherContext *ctx, const char *userid,
					      GMimeCipherHash hash, GMimeStream *istream,
					      GMimeStream *ostream, GMimeException *ex);
	
	GMimeCipherValidity * (*verify)      (GMimeCipherContext *ctx, GMimeCipherHash hash,
					      GMimeStream *istream, GMimeStream *sigstream,
					      GMimeException *ex);
	
	int                   (*encrypt)     (GMimeCipherContext *ctx, gboolean sign,
					      const char *userid, GPtrArray *recipients,
					      GMimeStream *istream, GMimeStream *ostream,
					      GMimeException *ex);
	
	int                   (*decrypt)     (GMimeCipherContext *ctx, GMimeStream *istream,
					      GMimeStream *ostream, GMimeException *ex);
	
	int                   (*import_keys) (GMimeCipherContext *ctx, GMimeStream *istream,
					      GMimeException *ex);
	
	int                   (*export_keys) (GMimeCipherContext *ctx, GPtrArray *keys,
					      GMimeStream *ostream, GMimeException *ex);
};


GType g_mime_cipher_context_get_type (void);


/* hash routines */
GMimeCipherHash      g_mime_cipher_hash_id (GMimeCipherContext *ctx, const char *hash);

const char *         g_mime_cipher_hash_name (GMimeCipherContext *ctx, GMimeCipherHash hash);

/* cipher routines */
int                  g_mime_cipher_sign (GMimeCipherContext *ctx, const char *userid,
					 GMimeCipherHash hash, GMimeStream *istream,
					 GMimeStream *ostream, GMimeException *ex);

GMimeCipherValidity *g_mime_cipher_verify (GMimeCipherContext *ctx, GMimeCipherHash hash,
					   GMimeStream *istream, GMimeStream *sigstream,
					   GMimeException *ex);

int                  g_mime_cipher_encrypt (GMimeCipherContext *ctx, gboolean sign,
					    const char *userid, GPtrArray *recipients,
					    GMimeStream *istream, GMimeStream *ostream,
					    GMimeException *ex);

int                  g_mime_cipher_decrypt (GMimeCipherContext *ctx, GMimeStream *istream,
					    GMimeStream *ostream, GMimeException *ex);

/* key/certificate routines */
int                  g_mime_cipher_import_keys (GMimeCipherContext *ctx, GMimeStream *istream,
						GMimeException *ex);

int                  g_mime_cipher_export_keys (GMimeCipherContext *ctx, GPtrArray *keys,
						GMimeStream *ostream, GMimeException *ex);


/* GMimeCipherValidity utility functions */
GMimeCipherValidity *g_mime_cipher_validity_new (void);

void                 g_mime_cipher_validity_init (GMimeCipherValidity *validity);

gboolean             g_mime_cipher_validity_get_valid (GMimeCipherValidity *validity);

void                 g_mime_cipher_validity_set_valid (GMimeCipherValidity *validity, gboolean valid);

const char          *g_mime_cipher_validity_get_description (GMimeCipherValidity *validity);

void                 g_mime_cipher_validity_set_description (GMimeCipherValidity *validity,
							     const char *description);

void                 g_mime_cipher_validity_clear (GMimeCipherValidity *validity);

void                 g_mime_cipher_validity_free (GMimeCipherValidity *validity);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GMIME_CIPHER_CONTEXT_H__ */
