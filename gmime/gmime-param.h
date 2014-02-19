/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2014 Jeffrey Stedfast
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

typedef struct _GMimeParam GMimeParam;


/**
 * GMimeParam:
 * @next: Pointer to the next param.
 * @name: Parameter name.
 * @value: Parameter value.
 *
 * A parameter name/value pair as used for some Content header fields.
 **/
struct _GMimeParam {
	GMimeParam *next;
	char *name;
	char *value;
};

GMimeParam *g_mime_param_new (const char *name, const char *value);
GMimeParam *g_mime_param_new_from_string (const char *str);
void g_mime_param_destroy (GMimeParam *param);

const GMimeParam *g_mime_param_next (const GMimeParam *param);

GMimeParam *g_mime_param_append (GMimeParam *params, const char *name, const char *value);
GMimeParam *g_mime_param_append_param (GMimeParam *params, GMimeParam *param);

const char *g_mime_param_get_name (const GMimeParam *param);
const char *g_mime_param_get_value (const GMimeParam *param);

void g_mime_param_write_to_string (const GMimeParam *param, gboolean fold, GString *str);

G_END_DECLS

#endif /* __GMIME_PARAM_H__ */
