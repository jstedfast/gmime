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


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "gmime-object.h"


/**
 * g_mime_object_construct:
 * @object: object
 * @object_template: object template
 * @type: object type
 *
 * Initializes a new object of type @type, using the virtual methods
 * from @object_template.
 **/
void
g_mime_object_construct (GMimeObject *object, GMimeObject *object_template, unsigned type)
{
	object->type = type;
	object->refcount = 1;
	object->destroy = object_template->destroy;
}


/**
 * g_mime_object_ref:
 * @object: object
 *
 * Ref's an object.
 **/
void
g_mime_object_ref (GMimeObject *object)
{
	g_return_if_fail (object != NULL);
	
	object->refcount++;
}


/**
 * g_mime_object_unref:
 * @object: object
 *
 * Unref's an object.
 **/
void
g_mime_object_unref (GMimeObject *object)
{
	g_return_if_fail (object != NULL);
	
	object->refcount--;
	
	if (object->refcount == 0)
		object->destroy (object);
}
