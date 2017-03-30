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


#ifndef __GMIME_REFERENCES_H__
#define __GMIME_REFERENCES_H__

#include <gmime/gmime-parser-options.h>

G_BEGIN_DECLS

#define GMIME_TYPE_REFERENCES (gmime_references_get_type ())

typedef struct _GMimeReferences GMimeReferences;

/**
 * GMimeReferences:
 * @array: the array of message-id references
 *
 * A List of references, as per the References or In-Reply-To header
 * fields.
 **/
struct _GMimeReferences {
	GPtrArray *array;
};


GType g_mime_references_get_type (void) G_GNUC_CONST;

GMimeReferences *g_mime_references_new (void);
void g_mime_references_free (GMimeReferences *refs);

GMimeReferences *g_mime_references_parse (GMimeParserOptions *options, const char *text);

GMimeReferences *g_mime_references_copy (GMimeReferences *refs);

int g_mime_references_length (GMimeReferences *refs);

void g_mime_references_append (GMimeReferences *refs, const char *msgid);
void g_mime_references_clear (GMimeReferences *refs);

const char *g_mime_references_get_message_id (GMimeReferences *refs, int index);
void g_mime_references_set_message_id (GMimeReferences *refs, int index, const char *msgid);

G_END_DECLS

#endif /* __GMIME_REFERENCES_H__ */
