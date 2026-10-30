/* GMime - internal GPGME integration helpers.
 * Copyright (C) 2026 Jeffrey Stedfast
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef __GMIME_GPGME_PRIVATE_H__
#define __GMIME_GPGME_PRIVATE_H__

#ifdef ENABLE_CRYPTO

#include "gmime-gpgme.h"

G_GNUC_INTERNAL gboolean _g_mime_gpgme_check_protocol (gpgme_ctx_t ctx, gpgme_protocol_t protocol, GError **error);
G_GNUC_INTERNAL const char *_g_mime_gpgme_signature_protocol (gpgme_ctx_t ctx, GError **error);
G_GNUC_INTERNAL char *_g_mime_gpgme_micalg (gpgme_ctx_t ctx, gpgme_sign_result_t result, GError **error);
G_GNUC_INTERNAL gboolean _g_mime_gpgme_io_error (GError **error);
G_GNUC_INTERNAL GMimeStream *_g_mime_gpgme_decode (GMimePart *part, GError **error);
G_GNUC_INTERNAL GMimeStream *_g_mime_gpgme_serialize (GMimeEntity *entity, GError **error);
G_GNUC_INTERNAL GMimeEntity *_g_mime_gpgme_parse (GMimeStream *stream, GError **error);

#endif

#endif /* __GMIME_GPGME_PRIVATE_H__ */
