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

#ifndef __GMIME_RECEIVED_CLAUSE_H__
#define __GMIME_RECEIVED_CLAUSE_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define GMIME_TYPE_RECEIVED_CLAUSE            (g_mime_received_clause_get_type ())


/**
 * GMimeReceivedClauseId:
 * @GMIME_RECEIVED_CLAUSE_NONE: comments before the first keyword
 * @GMIME_RECEIVED_CLAUSE_FROM: the sending host
 * @GMIME_RECEIVED_CLAUSE_BY: the receiving host
 * @GMIME_RECEIVED_CLAUSE_VIA: the physical link or protocol
 * @GMIME_RECEIVED_CLAUSE_WITH: the mail protocol
 * @GMIME_RECEIVED_CLAUSE_ID: the message identifier
 * @GMIME_RECEIVED_CLAUSE_FOR: the recipient mailbox
 * @GMIME_RECEIVED_CLAUSE_UNKNOWN: an unrecognized keyword
 *
 * Identifiers for common Received header clauses.
 **/
typedef enum {
	GMIME_RECEIVED_CLAUSE_NONE,
	GMIME_RECEIVED_CLAUSE_FROM,
	GMIME_RECEIVED_CLAUSE_BY,
	GMIME_RECEIVED_CLAUSE_VIA,
	GMIME_RECEIVED_CLAUSE_WITH,
	GMIME_RECEIVED_CLAUSE_ID,
	GMIME_RECEIVED_CLAUSE_FOR,
	GMIME_RECEIVED_CLAUSE_UNKNOWN
} GMimeReceivedClauseId;

/**
 * GMimeReceivedClause:
 *
 * A Received header keyword, value and ordered collection of comments.
 **/
G_DECLARE_FINAL_TYPE (GMimeReceivedClause, g_mime_received_clause, GMIME, RECEIVED_CLAUSE, GObject)


GMimeReceivedClause *g_mime_received_clause_new (const char *keyword, const char *value, GError **error);
GMimeReceivedClause *g_mime_received_clause_new_with_comment (const char *keyword, const char *value,
							   const char *comment, GError **error);

GMimeReceivedClauseId g_mime_received_clause_get_id (GMimeReceivedClause *clause);
const char *g_mime_received_clause_get_keyword (GMimeReceivedClause *clause);
const char *g_mime_received_clause_get_value (GMimeReceivedClause *clause);
gboolean g_mime_received_clause_set_value (GMimeReceivedClause *clause, const char *value, GError **error);

int g_mime_received_clause_get_comment_count (GMimeReceivedClause *clause);
const char *g_mime_received_clause_get_comment (GMimeReceivedClause *clause, int index);
int g_mime_received_clause_index_of_comment (GMimeReceivedClause *clause, const char *comment);
void g_mime_received_clause_add_comment (GMimeReceivedClause *clause, const char *comment);
void g_mime_received_clause_insert_comment (GMimeReceivedClause *clause, int index, const char *comment);
void g_mime_received_clause_set_comment (GMimeReceivedClause *clause, int index, const char *comment);
gboolean g_mime_received_clause_remove_comment (GMimeReceivedClause *clause, const char *comment);
void g_mime_received_clause_remove_comment_at (GMimeReceivedClause *clause, int index);
void g_mime_received_clause_clear_comments (GMimeReceivedClause *clause);

G_END_DECLS

#endif /* __GMIME_RECEIVED_CLAUSE_H__ */
