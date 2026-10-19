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
#define GMIME_MESSAGE_DELIVERY_STATUS(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GMIME_TYPE_MESSAGE_DELIVERY_STATUS, GMimeMessageDeliveryStatus))
#define GMIME_MESSAGE_DELIVERY_STATUS_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GMIME_TYPE_MESSAGE_DELIVERY_STATUS, GMimeMessageDeliveryStatusClass))
#define GMIME_IS_MESSAGE_DELIVERY_STATUS(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GMIME_TYPE_MESSAGE_DELIVERY_STATUS))
#define GMIME_IS_MESSAGE_DELIVERY_STATUS_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GMIME_TYPE_MESSAGE_DELIVERY_STATUS))
#define GMIME_MESSAGE_DELIVERY_STATUS_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GMIME_TYPE_MESSAGE_DELIVERY_STATUS, GMimeMessageDeliveryStatusClass))

typedef struct _GMimeMessageDeliveryStatus GMimeMessageDeliveryStatus;
typedef struct _GMimeMessageDeliveryStatusClass GMimeMessageDeliveryStatusClass;

/**
 * GMimeMessageDeliveryStatus:
 * @parent_object: parent #GMimePart
 *
 * A message/delivery-status part with lazily parsed status groups.
 **/
struct _GMimeMessageDeliveryStatus {
	GMimePart parent_object;

	/* < private > */
	GMimeHeaderListCollection *groups;
};

struct _GMimeMessageDeliveryStatusClass {
	GMimePartClass parent_class;
};

GType g_mime_message_delivery_status_get_type (void);

GMimeMessageDeliveryStatus *g_mime_message_delivery_status_new (void);
GMimeHeaderListCollection *g_mime_message_delivery_status_get_status_groups (GMimeMessageDeliveryStatus *status, GError **error);

G_END_DECLS

#endif /* __GMIME_MESSAGE_DELIVERY_STATUS_H__ */
