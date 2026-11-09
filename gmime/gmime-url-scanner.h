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

#ifndef __GMIME_URL_SCANNER_H__
#define __GMIME_URL_SCANNER_H__

#include <glib.h>

G_BEGIN_DECLS

/**
 * GMimeUrlMatch:
 * @start: the inclusive byte offset of the match relative to the input buffer
 * @end: the exclusive byte offset of the match relative to the input buffer
 * @prefix: a borrowed, static URI prefix, or an empty string for an explicit scheme
 *
 * A URL or email address found by g_mime_url_scan(). Prepend @prefix to the
 * matched text to form the link target. The prefix is "http://" for bare
 * www. names, "ftp://" for bare ftp. names, and "mailto:" for email addresses.
 * The match does not own any memory and requires no cleanup.
 **/
typedef struct {
	gsize start;
	gsize end;
	const char *prefix;
} GMimeUrlMatch;

gboolean g_mime_url_scan (const char *text, gsize start, gsize length, GMimeUrlMatch *match);

G_END_DECLS

#endif
