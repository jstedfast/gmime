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


#ifndef __G_MIME_FILTER_BASIC_H__
#define __G_MIME_FILTER_BASIC_H__

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#include "gmime-filter.h"

typedef enum {
	GMIME_FILTER_BASIC_BASE64_ENC = 1,
	GMIME_FILTER_BASIC_BASE64_DEC,
	GMIME_FILTER_BASIC_QP_ENC,
	GMIME_FILTER_BASIC_QP_DEC,
	GMIME_FILTER_BASIC_UU_ENC,
	GMIME_FILTER_BASIC_UU_DEC,
} GMimeFilterBasicType;

typedef struct _GMimeFilterBasic {
	GMimeFilter parent;
	
	GMimeFilterBasicType type;
	
	unsigned char uubuf[60];
	int state;
	int save;
} GMimeFilterBasic;

GMimeFilter *g_mime_filter_basic_new_type (GMimeFilterBasicType type);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GMIME_FILTER_BASIC_H__ */
