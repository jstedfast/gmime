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


#ifndef __G_MIME_FILTER_BASIC_H__
#define __G_MIME_FILTER_BASIC_H__

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#include <gmime/gmime-filter.h>

#define GMIME_TYPE_FILTER_BASIC            (g_mime_filter_basic_get_type ())
#define GMIME_FILTER_BASIC(obj)            (GMIME_CHECK_CAST ((obj), GMIME_TYPE_FILTER_BASIC, GMimeFilterBasic))
#define GMIME_FILTER_BASIC_CLASS(klass)    (GMIME_CHECK_CLASS_CAST ((klass), GMIME_TYPE_FILTER_BASIC, GMimeFilterBasicClass))
#define GMIME_IS_FILTER_BASIC(obj)         (GMIME_CHECK_TYPE ((obj), GMIME_TYPE_FILTER_BASIC))
#define GMIME_IS_FILTER_BASIC_CLASS(klass) (GMIME_CHECK_CLASS_TYPE ((klass), GMIME_TYPE_FILTER_BASIC))
#define GMIME_FILTER_BASIC_GET_CLASS(obj)  (GMIME_CHECK_GET_CLASS ((obj), GMIME_TYPE_FILTER_BASIC, GMimeFilterBasicClass))

typedef struct _GMimeFilterBasic GMimeFilterBasic;
typedef struct _GMimeFilterBasicClass GMimeFilterBasicClass;

typedef enum {
	GMIME_FILTER_BASIC_BASE64_ENC = 1,
	GMIME_FILTER_BASIC_BASE64_DEC,
	GMIME_FILTER_BASIC_QP_ENC,
	GMIME_FILTER_BASIC_QP_DEC,
	GMIME_FILTER_BASIC_UU_ENC,
	GMIME_FILTER_BASIC_UU_DEC
} GMimeFilterBasicType;

struct _GMimeFilterBasic {
	GMimeFilter parent_object;
	
	GMimeFilterBasicType type;
	
	unsigned char uubuf[60];
	int state;
	int save;
};

struct _GMimeFilterBasicClass {
	GMimeFilterClass parent_class;
	
};


GType g_mime_filter_basic_get_type (void);

GMimeFilter *g_mime_filter_basic_new_type (GMimeFilterBasicType type);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GMIME_FILTER_BASIC_H__ */
