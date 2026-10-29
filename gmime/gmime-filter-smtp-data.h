/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2022 Jeffrey Stedfast
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


/**
 * GMimeFilterSmtpData:
 *
 * A filter to byte-stuff SMTP DATA.
 **/
G_DECLARE_DERIVABLE_TYPE (GMimeFilterSmtpData, g_mime_filter_smtp_data, GMIME, FILTER_SMTP_DATA, GMimeFilter)

struct _GMimeFilterSmtpDataClass {
	GMimeFilterClass parent_class;

	/*< private >*/
	gpointer reserved[8];
};


GMimeFilter *g_mime_filter_smtp_data_new (void);

G_END_DECLS

#endif /* __GMIME_FILTER_SMTP_DATA_H__ */
