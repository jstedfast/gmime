/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2001-2004 Ximian, Inc. (www.ximian.com)
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


#ifndef __GMIME_DISPOSITION_H__
#define __GMIME_DISPOSITION_H__

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#include <glib.h>
#include <gmime/gmime-param.h>


/**
 * GMIME_DISPOSITION_ATTACHMENT:
 *
 * Standard attachment disposition.
 **/
#define GMIME_DISPOSITION_ATTACHMENT "attachment"


/**
 * GMIME_DISPOSITION_INLINE:
 *
 * Standard inline disposition.
 **/
#define GMIME_DISPOSITION_INLINE     "inline"


struct _GMimeDisposition {
	char *disposition;
	GMimeParam *params;
	GHashTable *param_hash;
};

typedef struct _GMimeDisposition GMimeDisposition;

GMimeDisposition *g_mime_disposition_new (const char *disposition);

void g_mime_disposition_destroy (GMimeDisposition *disposition);

void g_mime_disposition_set (GMimeDisposition *disposition, const char *value);
const char *g_mime_disposition_get (GMimeDisposition *disposition);

void g_mime_disposition_add_parameter (GMimeDisposition *disposition, const char *attribute,
				       const char *value);
const char *g_mime_disposition_get_parameter (GMimeDisposition *disposition, const char *attribute);

char *g_mime_disposition_header (GMimeDisposition *disposition, gboolean fold);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GMIME_DISPOSITION_H__ */
