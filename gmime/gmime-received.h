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

#ifndef __GMIME_RECEIVED_H__
#define __GMIME_RECEIVED_H__

#include <gmime/gmime-parser-options.h>
#include <gmime/gmime-received-clause.h>

G_BEGIN_DECLS

#define GMIME_TYPE_RECEIVED            (g_mime_received_get_type ())
#define GMIME_RECEIVED(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GMIME_TYPE_RECEIVED, GMimeReceived))
#define GMIME_RECEIVED_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GMIME_TYPE_RECEIVED, GMimeReceivedClass))
#define GMIME_IS_RECEIVED(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GMIME_TYPE_RECEIVED))
#define GMIME_IS_RECEIVED_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GMIME_TYPE_RECEIVED))
#define GMIME_RECEIVED_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GMIME_TYPE_RECEIVED, GMimeReceivedClass))

typedef struct _GMimeReceived GMimeReceived;
typedef struct _GMimeReceivedClass GMimeReceivedClass;

/**
 * GMimeReceived:
 * @parent_object: parent #GObject
 *
 * A parsed Received header value, containing ordered clauses and an optional date.
 **/
struct _GMimeReceived {
	GObject parent_object;

	/* <private> */
	GPtrArray *clauses;
	GDateTime *date_time;
	char *date_time_str;
};

struct _GMimeReceivedClass {
	GObjectClass parent_class;
};

GType g_mime_received_get_type (void);

GMimeReceived *g_mime_received_parse (GMimeParserOptions *options, const char *text, gssize length, GError **error);
int g_mime_received_get_clause_count (GMimeReceived *received);
GMimeReceivedClause *g_mime_received_get_clause (GMimeReceived *received, int index);
GDateTime *g_mime_received_get_date_time (GMimeReceived *received);

G_END_DECLS

#endif /* __GMIME_RECEIVED_H__ */
