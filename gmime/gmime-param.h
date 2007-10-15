/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2007 Jeffrey Stedfast
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


#ifndef __GMIME_PARAM_H__
#define __GMIME_PARAM_H__

#include <glib.h>

G_BEGIN_DECLS

struct _GMimeParam {
	struct _GMimeParam *next;
	char *name;
	char *value;
};

typedef struct _GMimeParam GMimeParam;

GMimeParam *g_mime_param_new (const char *name, const char *value);
GMimeParam *g_mime_param_new_from_string (const char *string);
void g_mime_param_destroy (GMimeParam *param);

GMimeParam *g_mime_param_append (GMimeParam *params, const char *name, const char *value);
GMimeParam *g_mime_param_append_param (GMimeParam *params, GMimeParam *param);

void g_mime_param_write_to_string (GMimeParam *param, gboolean fold, GString *string);

G_END_DECLS

#endif /* __GMIME_PARAM_H__ */
