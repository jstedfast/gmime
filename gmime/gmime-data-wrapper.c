/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2001 Ximain, Inc. (www.ximian.com)
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


#include "gmime-data-wrapper.h"


GMimeDataWrapper *
g_mime_data_wrapper_new (void)
{
	return g_new0 (GMimeDataWrapper, 1);
}


GMimeDataWrapper *
g_mime_data_wrapper_new_with_stream (GMimeStream *stream, GMimePartEncodingType encoding)
{
	GMimeDataWrapper *wrapper;
	
	wrapper = g_new (GMimeDataWrapper, 1);
	wrapper->encoding = encoding;
	wrapper->stream = stream;
	
	return wrapper;
}


void
g_mime_data_wrapper_destroy (GMimeDataWrapper *wrapper)
{
	if (wrapper) {
		if (wrapper->stream)
			g_mime_stream_unref (wrapper->stream);
		g_free (wrapper);
	}
}


void
g_mime_data_wrapper_set_stream (GMimeDataWrapper *wrapper, GMimeStream *stream)
{
	g_return_if_fail (wrapper != NULL);
	
	if (wrapper->stream)
		g_mime_stream_unref (wrapper->stream);
	wrapper->stream = stream;
}


void
g_mime_data_wrapper_set_encoding (GMimeDataWrapper *wrapper, GMimePartEncodingType encoding)
{
	g_return_if_fail (wrapper != NULL);
	
	wrapper->encoding = encoding;
}
