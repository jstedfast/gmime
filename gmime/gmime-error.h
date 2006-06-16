/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 1999-2006 Jeffrey Stedfast
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
 */


#ifndef __GMIME_ERROR_H__
#define __GMIME_ERROR_H__

#include <glib.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

extern GQuark gmime_error_quark;
#define GMIME_ERROR_QUARK gmime_error_quark

/**
 * GMIME_ERROR:
 *
 * The GMime error domain GQuark value.
 **/
#define GMIME_ERROR GMIME_ERROR_QUARK

/**
 * GMIME_ERROR_IS_SYSTEM:
 * @error: integer error value
 *
 * Decides if an error is a system error (aka errno value) vs. a GMime
 * error.
 *
 * Meant to be used with GError::code
 **/
#define GMIME_ERROR_IS_SYSTEM(error) ((error) > 0)

/* errno is a positive value, so negative values should be safe to use */
enum {
	GMIME_ERROR_GENERAL             =  0,
	GMIME_ERROR_NOT_SUPPORTED       = -1,
	GMIME_ERROR_PARSE_ERROR         = -2,
	GMIME_ERROR_PROTOCOL_ERROR      = -3,
	GMIME_ERROR_BAD_PASSWORD        = -4,
	GMIME_ERROR_NO_VALID_RECIPIENTS = -5
};

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GMIME_ERROR_H__ */
