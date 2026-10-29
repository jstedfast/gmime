/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2026 Jeffrey Stedfast
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

#ifndef __GMIME_MESSAGE_DELIVERY_STATUS_H__
#define __GMIME_MESSAGE_DELIVERY_STATUS_H__

#include <gmime/gmime-part.h>
#include <gmime/gmime-header-list-collection.h>

G_BEGIN_DECLS

#define GMIME_TYPE_MESSAGE_DELIVERY_STATUS            (g_mime_message_delivery_status_get_type ())


/**
 * GMimeMessageDeliveryStatus:
 *
 * A message/delivery-status part with lazily parsed status groups.
 **/
G_DECLARE_DERIVABLE_TYPE (GMimeMessageDeliveryStatus, g_mime_message_delivery_status, GMIME, MESSAGE_DELIVERY_STATUS, GMimePart)

struct _GMimeMessageDeliveryStatusClass {
	GMimePartClass parent_class;

	/*< private >*/
	gpointer reserved[8];
};


GMimeMessageDeliveryStatus *g_mime_message_delivery_status_new (void);
GMimeHeaderListCollection *g_mime_message_delivery_status_get_status_groups (GMimeMessageDeliveryStatus *status, GError **error);

G_END_DECLS

#endif /* __GMIME_MESSAGE_DELIVERY_STATUS_H__ */
