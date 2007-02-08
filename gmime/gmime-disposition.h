/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2007 Jeffrey Stedfast
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
 *  Foundation, 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */


#ifndef __GMIME_DISPOSITION_H__
#define __GMIME_DISPOSITION_H__

#include <glib.h>
#include <gmime/gmime-param.h>

G_BEGIN_DECLS


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

G_END_DECLS

#endif /* __GMIME_DISPOSITION_H__ */
