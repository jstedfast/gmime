/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* gmime-execpetion.h : exception utils */

/* 
 *
 * Authors: Bertrand Guiheneuf <bertrand@helixcode.com>
 *
 * Copyright 1999, 2000 Helix Code, Inc. (http://www.helixcode.com)
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License as 
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */


#ifndef GMIME_EXCEPTION_H
#define GMIME_EXCEPTION_H 1

#include <glib.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus }*/

typedef enum {
#include "gmime-exception-list.def"
} ExceptionId;

struct _GMimeException {
	/* do not access the fields directly */
	ExceptionId id;
	char *desc;
};

typedef struct _GMimeException GMimeException;

/* creation and destruction functions */
GMimeException           *g_mime_exception_new           (void);
void                      g_mime_exception_free          (GMimeException *exception);
void                      g_mime_exception_init          (GMimeException *ex);


/* exception content manipulation */
void                      g_mime_exception_clear         (GMimeException *exception);
void                      g_mime_exception_set           (GMimeException *ex,
							 ExceptionId id,
							 const char *desc);
void                      g_mime_exception_setv          (GMimeException *ex,
							 ExceptionId id,
							 const char *format,  
							 ...);


/* exception content transfer */
void                      g_mime_exception_xfer          (GMimeException *ex_dst,
							 GMimeException *ex_src);


/* exception content retrieval */
ExceptionId               g_mime_exception_get_id        (GMimeException *ex);
const char               *g_mime_exception_get_description (GMimeException *ex);

#define g_mime_exception_is_set(ex) (g_mime_exception_get_id (ex) != GMIME_EXCEPTION_NONE)

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* GMIME_EXCEPTION_H */

