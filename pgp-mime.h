/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@helixcode.com>
 *
 *  Copyright 2000 Helix Code, Inc. (www.helixcode.com)
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

#ifndef __PGP_MIME_H__
#define __PGP_MIME_H__

#include <glib.h>
#include "gmime-exception.h"
#include "gmime-part.h"
#include "pgp-utils.h"

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

void pgp_mime_init (const gchar *path, PgpType type, PgpPasswdFunc callback, gpointer data);

void pgp_mime_part_sign (GMimePart **part, const gchar *userid, PgpHashType hash, GMimeException *ex);

gboolean pgp_mime_part_verify_signature (GMimePart *part, const gchar *sign_key, GMimeException *ex);

void pgp_mime_part_encrypt (GMimePart **part, const gchar *recipients, gboolean sign, GMimeException *ex);

void pgp_mime_part_decrypt (GMimePart **part, GMimeException *ex);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __PGP_MIME_H__ */
