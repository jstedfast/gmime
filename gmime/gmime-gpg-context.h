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


#ifndef __GMIME_GPG_CONTEXT_H__
#define __GMIME_GPG_CONTEXT_H__

#include <gmime/gmime-crypto-context.h>

G_BEGIN_DECLS

#define GMIME_TYPE_GPG_CONTEXT            (g_mime_gpg_context_get_type ())


G_DECLARE_FINAL_TYPE (GMimeGpgContext, g_mime_gpg_context, GMIME, GPG_CONTEXT, GMimeCryptoContext)

GMimeCryptoContext *g_mime_gpg_context_new (void);

gpointer g_mime_gpg_context_get_gpgme_ctx (GMimeGpgContext *ctx);

G_END_DECLS

#endif /* __GMIME_GPG_CONTEXT_H__ */
