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


#ifndef __GMIME_FILTER_SMTP_DATA_H__
#define __GMIME_FILTER_SMTP_DATA_H__

#include <gmime/gmime-filter.h>

G_BEGIN_DECLS

#define GMIME_TYPE_FILTER_SMTP_DATA            (g_mime_filter_smtp_data_get_type ())
#define GMIME_FILTER_SMTP_DATA(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GMIME_TYPE_FILTER_SMTP_DATA, GMimeFilterSmtpData))
#define GMIME_FILTER_SMTP_DATA_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GMIME_TYPE_FILTER_SMTP_DATA, GMimeFilterSmtpDataClass))
#define GMIME_IS_FILTER_SMTP_DATA(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GMIME_TYPE_FILTER_SMTP_DATA))
#define GMIME_IS_FILTER_SMTP_DATA_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GMIME_TYPE_FILTER_SMTP_DATA))
#define GMIME_FILTER_SMTP_DATA_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GMIME_TYPE_FILTER_SMTP_DATA, GMimeFilterSmtpDataClass))

typedef struct _GMimeFilterSmtpData GMimeFilterSmtpData;
typedef struct _GMimeFilterSmtpDataClass GMimeFilterSmtpDataClass;

/**
 * GMimeFilterSmtpData:
 * @parent_object: parent #GMimeFilter
 * @bol: beginning-of-line state.
 *
 * A filter to byte-stuff SMTP DATA.
 **/
struct _GMimeFilterSmtpData {
	GMimeFilter parent_object;
	
	gboolean bol;
};

struct _GMimeFilterSmtpDataClass {
	GMimeFilterClass parent_class;
	
};


GType g_mime_filter_smtp_data_get_type (void);

GMimeFilter *g_mime_filter_smtp_data_new (void);

G_END_DECLS

#endif /* __GMIME_FILTER_SMTP_DATA_H__ */
