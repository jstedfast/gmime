/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2014 Jeffrey Stedfast
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


#ifndef __GMIME_ERROR_H__
#define __GMIME_ERROR_H__

#include <glib.h>

G_BEGIN_DECLS

extern GQuark gmime_error_quark;

/**
 * GMIME_ERROR:
 *
 * The GMime error domain GQuark value.
 **/
#define GMIME_ERROR gmime_error_quark

/**
 * GMIME_ERROR_IS_SYSTEM:
 * @error: integer error value
 *
 * Decides if an error is a system error (aka errno value) vs. a GMime
 * error.
 *
 * Meant to be used with #GError::code
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


extern GQuark gmime_gpgme_error_quark;

/**
 * GMIME_GPGME_ERROR:
 *
 * The GMime GpgMe error domain GQuark value.
 **/
#define GMIME_GPGME_ERROR gmime_gpgme_error_quark

G_END_DECLS

#endif /* __GMIME_ERROR_H__ */
