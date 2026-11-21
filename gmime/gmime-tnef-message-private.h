/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) Jeffrey Stedfast
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

#ifndef __GMIME_TNEF_MESSAGE_PRIVATE_H__
#define __GMIME_TNEF_MESSAGE_PRIVATE_H__

#include "gmime-tnef-message.h"
#include "gmime-tnef-private.h"

struct _GMimeTnefMessage {
	GObject parent_instance;
	int codepage;
	guint16 legacy_key;
	char *message_class;
	GMimeTnefPropertySet *properties;
	GPtrArray *recipients, *attachments;
	GMimeTnefMessageBody *bodies[3];
};

struct _GMimeTnefAttachment {
	GObject parent_instance;
	GMimeTnefPropertySet *properties;
	GBytes *content;
	TnefReplayContext *embedded;
};

struct _GMimeTnefRecipient {
	GObject parent_instance;
	GMimeTnefPropertySet *properties;
};

struct _GMimeTnefMessageBody {
	GObject parent_instance;
	GMimeTnefMessageBodyFormat format;
	GMimeTnefPropertyTag tag;
	GBytes *content;
	char *charset, *fallback;
};

G_GNUC_INTERNAL const char *_g_mime_tnef_set_string (GMimeTnefPropertySet *set, GMimeTnefPropertyTag tag);
G_GNUC_INTERNAL int _g_mime_tnef_set_int (GMimeTnefPropertySet *set, GMimeTnefPropertyTag tag, int fallback);

#endif
