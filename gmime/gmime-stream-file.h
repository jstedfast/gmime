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


#ifndef __GMIME_STREAM_FILE_H__
#define __GMIME_STREAM_FILE_H__

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#include <glib.h>
#include <stdio.h>
#include <gmime-stream.h>

typedef struct _GMimeStreamFile {
	GMimeStream parent;
	
	FILE *fp;
} GMimeStreamFile;

#define GMIME_STREAM_FILE_TYPE g_str_hash ("GMimeStreamFile")
#define GMIME_IS_STREAM_FILE(stream) (((GMimeStream *) stream)->type == GMIME_STREAM_FILE)
#define GMIME_STREAM_FILE(stream) ((GMimeStreamFile *) stream)

GMimeStream *g_mime_stream_file_new (FILE *fp);
GMimeStream *g_mime_stream_file_new_with_bounds (FILE *fp, off_t start, off_t end);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GMIME_STREAM_FILE_H__ */
