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


#ifndef __UNICODE_H__
#define __UNICODE_H__

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#include <glib.h>
#include <string.h>

/* unicode_* and unichar are stolen from glib2... */
typedef guint32 unichar;

extern const char unicode_skip[256];

#define unicode_next_char(p) (char *)((p) + unicode_skip[*(unsigned char *)(p)])

unichar unicode_get_char (const char  *p);

gboolean unichar_validate (unichar ch);

#define unichar_isspace(c)  (strchr (" \t\r\n", c) != NULL)

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __UNICODE_H__ */
