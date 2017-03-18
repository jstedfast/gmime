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
#include <glib-object.h>
#include <gmime/gmime-format-options.h>
#include <gmime/gmime-parser-options.h>

G_BEGIN_DECLS

#define GMIME_TYPE_PARAM                  (g_mime_param_get_type ())
#define GMIME_PARAM(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), GMIME_TYPE_PARAM, GMimeParam))
#define GMIME_PARAM_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), GMIME_TYPE_PARAM, GMimeParamClass))
#define GMIME_IS_PARAM(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GMIME_TYPE_PARAM))
#define GMIME_IS_PARAM_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), GMIME_TYPE_PARAM))
#define GMIME_PARAM_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), GMIME_TYPE_PARAM, GMimeParamClass))

#define GMIME_TYPE_PARAM_LIST             (g_mime_param_list_get_type ())
#define GMIME_PARAM_LIST(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), GMIME_TYPE_PARAM_LIST, GMimeParamList))
#define GMIME_PARAM_LIST_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), GMIME_TYPE_PARAM_LIST, GMimeParamListClass))
#define GMIME_IS_PARAM_LIST(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GMIME_TYPE_PARAM_LIST))
#define GMIME_IS_PARAM_LIST_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), GMIME_TYPE_PARAM_LIST))
#define GMIME_PARAM_LIST_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), GMIME_TYPE_PARAM_LIST, GMimeParamListClass))

typedef struct _GMimeParam GMimeParam;
typedef struct _GMimeParamClass GMimeParamClass;

typedef struct _GMimeParamList GMimeParamList;
typedef struct _GMimeParamListClass GMimeParamListClass;


/**
 * GMimeParam:
 * @method: The encoding method used for the parameter value.
 * @charset: The charset to use when encoding the parameter value.
 * @lang: the language specifier to use when encoding the value.
 * @name: The parameter name.
 * @value: The parameter value.
 *
 * A parameter name/value pair as used in the Content-Type and Content-Disposition headers.
 **/
struct _GMimeParam {
	GObject parent_object;
	
	GMimeParamEncodingMethod method;
	char *charset, *lang;
	char *name, *value;
	
	/* < private > */
	gpointer changed;
};

struct _GMimeParamClass {
	GObjectClass parent_class;
	
};


GType g_mime_param_get_type (void);

const char *g_mime_param_get_name (GMimeParam *param);

const char *g_mime_param_get_value (GMimeParam *param);
void g_mime_param_set_value (GMimeParam *param, const char *value);

const char *g_mime_param_get_charset (GMimeParam *param);
void g_mime_param_set_charset (GMimeParam *param, const char *charset);

const char *g_mime_param_get_lang (GMimeParam *param);
void g_mime_param_set_lang (GMimeParam *param, const char *lang);

GMimeParamEncodingMethod g_mime_param_get_encoding_method (GMimeParam *param);
void g_mime_param_set_encoding_method (GMimeParam *param, GMimeParamEncodingMethod method);


/**
 * GMimeParamList:
 *
 * A list of Content-Type or Content-Disposition parameters.
 **/
struct _GMimeParamList {
	/* < private > */
	GObject parent_object;
	GPtrArray *array;
	gpointer changed;
};

struct _GMimeParamListClass {
	GObjectClass parent_class;
	
};


GType g_mime_param_list_get_type (void);

GMimeParamList *g_mime_param_list_new (void);
GMimeParamList *g_mime_param_list_parse (GMimeParserOptions *options, const char *str);

int g_mime_param_list_length (GMimeParamList *list);

void g_mime_param_list_clear (GMimeParamList *list);

void g_mime_param_list_set_parameter (GMimeParamList *list, const char *name, const char *value);
GMimeParam *g_mime_param_list_get_parameter (GMimeParamList *list, const char *name);
GMimeParam *g_mime_param_list_get_parameter_at (GMimeParamList *list, int index);

gboolean g_mime_param_list_remove (GMimeParamList *list, const char *name);
gboolean g_mime_param_list_remove_at (GMimeParamList *list, int index);

void g_mime_param_list_encode (GMimeParamList *list, GMimeFormatOptions *options, gboolean fold, GString *str);

G_END_DECLS

#endif /* __GMIME_PARAM_H__ */
