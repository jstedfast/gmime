/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2001 Ximian, Inc. (www.ximian.com)
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


#ifndef __G_MIME_FILTER_CRLF_H__
#define __G_MIME_FILTER_CRLF_H__

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#include "gmime-filter.h"

typedef enum {
	GMIME_FILTER_CRLF_ENCODE,
	GMIME_FILTER_CRLF_DECODE
} GMimeFilterCRLFDirection;

typedef enum {
	GMIME_FILTER_CRLF_MODE_CRLF_DOTS,
	GMIME_FILTER_CRLF_MODE_CRLF_ONLY,
} GMimeFilterCRLFMode;

typedef struct _GMimeFilterCRLF {
	GMimeFilter parent;
	
	GMimeFilterCRLFDirection direction;
	GMimeFilterCRLFMode mode;
	gboolean saw_cr;
	gboolean saw_lf;
	gboolean saw_dot;
} GMimeFilterCRLF;

GMimeFilter *g_mime_filter_crlf_new (GMimeFilterCRLFDirection direction, GMimeFilterCRLFMode mode);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GMIME_FILTER_CRLF_H__ */
