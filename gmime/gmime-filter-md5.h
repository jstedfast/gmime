/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 1999-2006 Jeffrey Stedfast
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
 */


#ifndef __GMIME_FILTER_MD5_H__
#define __GMIME_FILTER_MD5_H__

#include <gmime/gmime-filter.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#define GMIME_TYPE_FILTER_MD5            (g_mime_filter_md5_get_type ())
#define GMIME_FILTER_MD5(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GMIME_TYPE_FILTER_MD5, GMimeFilterMd5))
#define GMIME_FILTER_MD5_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GMIME_TYPE_FILTER_MD5, GMimeFilterMd5Class))
#define GMIME_IS_FILTER_MD5(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GMIME_TYPE_FILTER_MD5))
#define GMIME_IS_FILTER_MD5_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GMIME_TYPE_FILTER_MD5))
#define GMIME_FILTER_MD5_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GMIME_TYPE_FILTER_MD5, GMimeFilterMd5Class))

typedef struct _GMimeFilterMd5 GMimeFilterMd5;
typedef struct _GMimeFilterMd5Class GMimeFilterMd5Class;

struct _GMimeFilterMd5 {
	GMimeFilter parent_object;
	
	struct _GMimeFilterMd5Private *priv;
};

struct _GMimeFilterMd5Class {
	GMimeFilterClass parent_class;
	
};


GType g_mime_filter_md5_get_type (void);

GMimeFilter *g_mime_filter_md5_new (void);

void g_mime_filter_md5_get_digest (GMimeFilterMd5 *md5, unsigned char digest[16]);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GMIME_FILTER_MD5_H__ */
