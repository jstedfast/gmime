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


#ifndef __GMIME_GPGME_UTILS_H__
#define __GMIME_GPGME_UTILS_H__

#ifdef ENABLE_CRYPTO

#include <glib.h>
#include <gpgme.h>
#include <gmime/gmime-crypto-context.h>

G_BEGIN_DECLS

G_GNUC_INTERNAL gpgme_error_t g_mime_gpgme_passphrase_callback (void *hook, const char *uid_hint,
								const char *passphrase_info,
								int prev_was_bad, int fd);

G_GNUC_INTERNAL int g_mime_gpgme_sign (gpgme_ctx_t ctx, gpgme_sig_mode_t mode, const char *userid,
				       GMimeStream *istream, GMimeStream *ostream, GError **err);

G_GNUC_INTERNAL GMimeSignatureList *g_mime_gpgme_verify (gpgme_ctx_t ctx, GMimeVerifyFlags flags, GMimeStream *istream,
							 GMimeStream *sigstream, GMimeStream *ostream, GError **err);

G_GNUC_INTERNAL int g_mime_gpgme_encrypt (gpgme_ctx_t ctx, gboolean sign, const char *userid,
					  GMimeEncryptFlags flags, GPtrArray *recipients,
					  GMimeStream *istream, GMimeStream *ostream,
					  GError **err);

G_GNUC_INTERNAL GMimeDecryptResult *g_mime_gpgme_decrypt (gpgme_ctx_t ctx, GMimeDecryptFlags flags, const char *session_key,
							  GMimeStream *istream, GMimeStream *ostream, GError **err);

G_GNUC_INTERNAL int g_mime_gpgme_import (gpgme_ctx_t ctx, GMimeStream *istream, GError **err);

G_GNUC_INTERNAL int g_mime_gpgme_export (gpgme_ctx_t ctx, const char *keys[], GMimeStream *ostream, GError **err);

G_END_DECLS

#endif /* ENABLE_CRYPTO */

#endif /* __GMIME_GPGME_UTILS_H__ */
