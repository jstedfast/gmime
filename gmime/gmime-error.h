/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2002-2004 Ximian, Inc. (www.ximian.com)
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


#ifndef __GMIME_ERROR_H__
#define __GMIME_ERROR_H__

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#include <glib.h>

typedef enum {
	GMIME_ERROR_GENERAL             =  0,
	GMIME_ERROR_NOT_SUPPORTED       = -1,
	GMIME_ERROR_PARSE_ERROR         = -2,
	GMIME_ERROR_PROTOCOL_ERROR      = -3,
	GMIME_ERROR_BAD_PASSWORD        = -4,
	GMIME_ERROR_NO_VALID_RECIPIENTS = -5,
} GMimeError;

#define GMIME_ERROR_QUARK (g_quark_from_static_string ("gmime"))

#ifdef __cplusplus
}
#endif

#endif /* __GMIME_ERROR_H__ */
