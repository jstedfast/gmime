/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2002 Ximian, Inc. (www.ximian.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "gmime.h"


/**
 * g_mime_init:
 * @flags: initialization flags
 *
 * Initializes GMime.
 **/
void
g_mime_init (guint32 flags)
{
	static int initialized = FALSE;
	
	if (initialized)
		return;
	
	initialized = TRUE;
	
	g_type_init ();
	
	g_mime_charset_map_init ();
	
	g_mime_iconv_init ();
	
	/* register our default mime object types */
	g_mime_object_register_type ("*", "*", g_mime_part_get_type ());
	g_mime_object_register_type ("multipart", "*", g_mime_multipart_get_type ());
	g_mime_object_register_type ("multipart", "encrypted", g_mime_multipart_encrypted_get_type ());
	g_mime_object_register_type ("multipart", "signed", g_mime_multipart_signed_get_type ());
	g_mime_object_register_type ("message", "rfc822", g_mime_message_part_get_type ());
	g_mime_object_register_type ("message", "rfc2822", g_mime_message_part_get_type ());
	g_mime_object_register_type ("message", "news", g_mime_message_part_get_type ());
	g_mime_object_register_type ("message", "partial", g_mime_message_partial_get_type ());
}
