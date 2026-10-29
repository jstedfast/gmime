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

#include <gio/gio.h>
#include <gmime/gmime-format-options.h>
#include <gmime/gmime-parser-options.h>
#include <gmime/gmime-received-clause.h>

G_BEGIN_DECLS

#define GMIME_TYPE_RECEIVED            (g_mime_received_get_type ())


/**
 * GMimeReceived:
 *
 * A Received header value, containing ordered clauses and an optional date.
 **/
G_DECLARE_FINAL_TYPE (GMimeReceived, g_mime_received, GMIME, RECEIVED, GObject)


GMimeReceived *g_mime_received_new (void);
GMimeReceived *g_mime_received_new_with_hosts (const char *from, GInetAddress *from_tcp_info,
					    const char *by, GInetAddress *by_tcp_info,
					    GDateTime *date_time, GError **error);
GMimeReceived *g_mime_received_parse (GMimeParserOptions *options, const char *text, gssize length, GError **error);
char *g_mime_received_to_string (GMimeReceived *received, GMimeFormatOptions *options);

int g_mime_received_get_clause_count (GMimeReceived *received);
GMimeReceivedClause *g_mime_received_get_clause (GMimeReceived *received, int index);
void g_mime_received_set_clause (GMimeReceived *received, int index, GMimeReceivedClause *clause);
void g_mime_received_add_clause (GMimeReceived *received, GMimeReceivedClause *clause);
void g_mime_received_insert_clause (GMimeReceived *received, int index, GMimeReceivedClause *clause);
int g_mime_received_index_of_clause (GMimeReceived *received, GMimeReceivedClause *clause);
gboolean g_mime_received_contains_clause (GMimeReceived *received, GMimeReceivedClause *clause);
gboolean g_mime_received_remove_clause (GMimeReceived *received, GMimeReceivedClause *clause);
void g_mime_received_remove_clause_at (GMimeReceived *received, int index);
void g_mime_received_clear_clauses (GMimeReceived *received);

GDateTime *g_mime_received_get_date_time (GMimeReceived *received);
void g_mime_received_set_date_time (GMimeReceived *received, GDateTime *date_time);

const char *g_mime_received_get_from (GMimeReceived *received);
gboolean g_mime_received_set_from (GMimeReceived *received, const char *value, GError **error);
const char *g_mime_received_get_from_tcp_info (GMimeReceived *received);
void g_mime_received_set_from_tcp_info (GMimeReceived *received, const char *comment);
const char *g_mime_received_get_by (GMimeReceived *received);
gboolean g_mime_received_set_by (GMimeReceived *received, const char *value, GError **error);
const char *g_mime_received_get_by_tcp_info (GMimeReceived *received);
void g_mime_received_set_by_tcp_info (GMimeReceived *received, const char *comment);
const char *g_mime_received_get_via (GMimeReceived *received);
gboolean g_mime_received_set_via (GMimeReceived *received, const char *value, GError **error);
const char *g_mime_received_get_with (GMimeReceived *received);
gboolean g_mime_received_set_with (GMimeReceived *received, const char *value, GError **error);
const char *g_mime_received_get_id (GMimeReceived *received);
gboolean g_mime_received_set_id (GMimeReceived *received, const char *value, GError **error);
const char *g_mime_received_get_for (GMimeReceived *received);
gboolean g_mime_received_set_for (GMimeReceived *received, const char *value, GError **error);

G_END_DECLS

#endif /* __GMIME_RECEIVED_H__ */
