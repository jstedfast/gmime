/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2020 Jeffrey Stedfast
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


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <errno.h>

#include "gmime-charset.h"
#include "gmime-iconv.h"


/**
 * SECTION: gmime-iconv
 * @title: gmime-iconv
 * @short_description: Low-level routines for converting text from one charset to another
 * @see_also:
 *
 * These functions are wrappers around the system iconv(3) routines. The
 * purpose of this wrapper is to use the appropriate system charset alias for
 * the MIME charset names given as arguments.
 **/


/**
 * g_mime_iconv_open: (skip)
 * @to: charset to convert to
 * @from: charset to convert from
 *
 * Allocates a coversion descriptor suitable for converting byte
 * sequences from charset @from to charset @to. The resulting
 * descriptor can be used with iconv() (or the g_mime_iconv() wrapper) any
 * number of times until closed using g_mime_iconv_close().
 *
 * See the manual page for iconv_open(3) for further details.
 *
 * Returns: a new conversion descriptor for use with g_mime_iconv() on
 * success or (iconv_t) %-1 on fail as well as setting an appropriate
 * errno value.
 **/
iconv_t
g_mime_iconv_open (const char *to, const char *from)
{
	if (from == NULL || to == NULL) {
		errno = EINVAL;
		return (iconv_t) -1;
	}
	
	if (!g_ascii_strcasecmp (from, "x-unknown"))
		from = g_mime_locale_charset ();
	
	from = g_mime_charset_iconv_name (from);
	to = g_mime_charset_iconv_name (to);
	
	return iconv_open (to, from);
}


/**
 * g_mime_iconv_close: (skip)
 * @cd: iconv conversion descriptor
 *
 * Closes the iconv descriptor @cd.
 *
 * See the manual page for iconv_close(3) for further details.
 *
 * Returns: %0 on success or %-1 on fail as well as setting an
 * appropriate errno value.
 **/
int
g_mime_iconv_close (iconv_t cd)
{
	return iconv_close (cd);
}
