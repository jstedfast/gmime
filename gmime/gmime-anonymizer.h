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

#ifndef __GMIME_ANONYMIZER_H__
#define __GMIME_ANONYMIZER_H__

#include <gmime/gmime-header.h>
#include <gmime/gmime-message.h>

G_BEGIN_DECLS

#define GMIME_TYPE_ANONYMIZER            (g_mime_anonymizer_get_type ())


/**
 * GMimeAnonymizer:
 *
 * MIME anonymization with configurable preserved header fields.
 **/
G_DECLARE_FINAL_TYPE (GMimeAnonymizer, g_mime_anonymizer, GMIME, ANONYMIZER, GObject)


GMimeAnonymizer *g_mime_anonymizer_new (void);
gboolean g_mime_anonymizer_add_preserved_header (GMimeAnonymizer *anonymizer, const char *name);
gboolean g_mime_anonymizer_remove_preserved_header (GMimeAnonymizer *anonymizer, const char *name);
gboolean g_mime_anonymizer_contains_preserved_header (GMimeAnonymizer *anonymizer, const char *name);
void g_mime_anonymizer_clear_preserved_headers (GMimeAnonymizer *anonymizer);
char **g_mime_anonymizer_get_preserved_headers (GMimeAnonymizer *anonymizer);

gboolean g_mime_anonymizer_write_headers (GMimeAnonymizer *anonymizer, GMimeFormatOptions *options,
					GMimeHeaderList *headers, GMimeStream *stream, GError **error);
gboolean g_mime_anonymizer_write_message (GMimeAnonymizer *anonymizer, GMimeFormatOptions *options,
					GMimeMessage *message, GMimeStream *stream, GError **error);
gboolean g_mime_anonymizer_write_entity (GMimeAnonymizer *anonymizer, GMimeFormatOptions *options,
				       GMimeEntity *entity, GMimeStream *stream, GError **error);

G_END_DECLS

#endif /* __GMIME_ANONYMIZER_H__ */
