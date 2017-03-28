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


#ifndef __GMIME_PARSE_UTILS_H__
#define __GMIME_PARSE_UTILS_H__

#include <glib.h>

G_BEGIN_DECLS

G_GNUC_INTERNAL gboolean g_mime_parse_content_type (const char **in, char **type, char **subtype);

G_GNUC_INTERNAL gboolean g_mime_skip_comment (const char **in);
#define skip_comment(in) g_mime_skip_comment (in)

G_GNUC_INTERNAL gboolean g_mime_skip_lwsp (const char **in);
#define skip_lwsp(in) g_mime_skip_lwsp (in)

G_GNUC_INTERNAL gboolean g_mime_skip_cfws (const char **in);
#define skip_cfws(in) g_mime_skip_cfws (in)

G_GNUC_INTERNAL gboolean g_mime_skip_quoted (const char **in);
#define skip_quoted(in) g_mime_skip_quoted (in)

G_GNUC_INTERNAL gboolean g_mime_skip_atom (const char **in);
#define skip_atom(in) g_mime_skip_atom (in)

G_GNUC_INTERNAL gboolean g_mime_skip_word (const char **in);
#define skip_word(in) g_mime_skip_word (in)


G_GNUC_INTERNAL const char *g_mime_decode_word (const char **in);
#define decode_word(in) g_mime_decode_word (in)

G_GNUC_INTERNAL gboolean g_mime_decode_domain (const char **in, GString *domain);
#define decode_domain(in, domain) g_mime_decode_domain (in, domain)


G_GNUC_INTERNAL char *g_mime_decode_addrspec (const char **in);
#define decode_addrspec(in) g_mime_decode_addrspec (in)

G_GNUC_INTERNAL char *g_mime_decode_msgid (const char **in);
#define decode_msgid(in) g_mime_decode_msgid (in)

G_END_DECLS

#endif /* __GMIME_PARSE_UTILS_H__ */
