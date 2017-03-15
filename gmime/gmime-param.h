/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2017 Jeffrey Stedfast
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
#include <gmime/gmime-parser-options.h>

G_BEGIN_DECLS

typedef struct _GMimeParam GMimeParam;
typedef struct _GMimeParamList GMimeParamList;


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
GMimeParam *g_mime_param_parse (GMimeParserOptions *options, const char *str);
void g_mime_param_free (GMimeParam *param);

const GMimeParam *g_mime_param_next (const GMimeParam *param);

GMimeParam *g_mime_param_append (GMimeParam *params, const char *name, const char *value);
GMimeParam *g_mime_param_append_param (GMimeParam *params, GMimeParam *param);

const char *g_mime_param_get_name (const GMimeParam *param);
const char *g_mime_param_get_value (const GMimeParam *param);

void g_mime_param_write_to_string (const GMimeParam *param, gboolean fold, GString *str);


/**
 * GMimeParamList:
 *
 * A list of Content-Type or Content-Disposition parameters.
 **/
struct _GMimeParamList {
	/* < private > */
	GPtrArray *params;
	GHashTable *hash;
	void *changed;
};

GMimeParamList *g_mime_param_list_new (void);
void g_mime_param_list_free (GMimeParamList *list);

int g_mime_param_list_length (GMimeParamList *list);

void g_mime_param_list_clear (GMimeParamList *list);

int g_mime_param_list_add (GMimeParamList *list, GMimeParam *param);
void g_mime_param_list_insert (GMimeParamList *list, int index, GMimeParam *param);
gboolean g_mime_param_list_remove (GMimeParamList *list, GMimeParam *param);
gboolean g_mime_param_list_remove_at (GMimeParamList *list, int index);

gboolean g_mime_param_list_contains (GMimeParamList *list, GMimeParam *param);
int g_mime_param_list_index_of (GMimeParamList *list, GMimeParam *param);

GMimeParam *g_mime_param_list_get_param (GMimeParamList *list, int index);
void g_mime_param_list_set_param (GMimeParamList *list, int index, GMimeParam *param);

G_END_DECLS

#endif /* __GMIME_PARAM_H__ */
