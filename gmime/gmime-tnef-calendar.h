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

#ifndef __GMIME_TNEF_CALENDAR_H__
#define __GMIME_TNEF_CALENDAR_H__

#include "gmime-tnef-conversion.h"
#include "gmime-tnef-message.h"

typedef void (*TnefCalendarLoss) (GMimeTnefConversionLossKind kind, const char *detail, gpointer user_data);

/* Returns owned bytes or NULL. On success, method is static and consumed
 * receives borrowed attachment pointers (direct keys, no destroy notify). */
G_GNUC_INTERNAL GBytes *_g_mime_tnef_calendar_export (GMimeTnefMessage *message, const char *fallback_charset,
	guint max_exceptions, guint64 max_exceptions_size, const char **method, GHashTable *consumed,
	TnefCalendarLoss loss, gpointer user_data);

#endif
