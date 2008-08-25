/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2008 Jeffrey Stedfast
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


/**
 * GMimeContentDisposition:
 * @parent_object: parent object pointer
 * @param_hash: parameter hash table keyed by param name
 * @params: a #GMimeParam list
 * @disposition: disposition
 *
 * A data structure representing a Content-Disposition.
 **/
struct _GMimeContentDisposition {
	/* <private> */
	gpointer parent_object;
	GHashTable *param_hash;
	
	/* <public:read-only> */
	GMimeParam *params;
	char *disposition;
};

typedef struct _GMimeContentDisposition GMimeContentDisposition;

GMimeContentDisposition *g_mime_content_disposition_new (void);
GMimeContentDisposition *g_mime_content_disposition_new_from_string (const char *str);

void g_mime_content_disposition_destroy (GMimeContentDisposition *disposition);

void g_mime_content_disposition_set_disposition (GMimeContentDisposition *disposition, const char *value);
const char *g_mime_content_disposition_get_disposition (const GMimeContentDisposition *disposition);

void g_mime_content_disposition_set_params (GMimeContentDisposition *disposition, GMimeParam *params);
const GMimeParam *g_mime_content_disposition_get_params (const GMimeContentDisposition *disposition);

void g_mime_content_disposition_set_parameter (GMimeContentDisposition *disposition,
					       const char *attribute, const char *value);
const char *g_mime_content_disposition_get_parameter (const GMimeContentDisposition *disposition,
						      const char *attribute);

char *g_mime_content_disposition_to_string (const GMimeContentDisposition *disposition, gboolean fold);

G_END_DECLS

#endif /* __GMIME_DISPOSITION_H__ */
