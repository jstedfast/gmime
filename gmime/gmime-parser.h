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

#ifndef __GMIME_PARSER_H__
#define __GMIME_PARSER_H__

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus } */

#include <stdio.h> /* for FILE */
#include <glib.h>
#include "gmime-message.h"
#include "gmime-part.h"
#include "gmime-content-type.h"
#include "gmime-param.h"


GMimePart *g_mime_parser_construct_part (const gchar *in, guint inlen);

GMimeMessage *g_mime_parser_construct_message (const gchar *in, guint inlen, gboolean save_extra_headers);

GMimeMessage *g_mime_parser_construct_message_from_file (FILE *fp, gboolean save_extra_headers);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GMIME_PARSER_H__ */
