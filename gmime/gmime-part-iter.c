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


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "gmime-part-iter.h"
#include "gmime-message-part.h"
#include "gmime-multipart.h"
#include "gmime-message.h"
#include "gmime-part.h"

/**
 * SECTION: gmime-part-iter
 * @title: GMimePartIter
 * @short_description: MIME part iterators
 * @see_also: #GMimeObject
 *
 * #GMimePartIter is an iterator for traversing a #GMimeObject tree in
 * Depth-First order.
 **/


typedef struct _GMimeObjectStack GMimeObjectStack;

struct _GMimeObjectStack {
	GMimeObjectStack *parent;
	GMimeObject *object;
	gboolean indexed;
};

struct _GMimePartIter {
	GMimeObjectStack *parent;
	GMimeObject *toplevel;
	GMimeObject *current;
	GArray *path;
	int index;
};

static void
g_mime_part_iter_push (GMimePartIter *iter, GMimeObject *object, int index)
{
	GMimeObjectStack *node;
	
	if (index != -1)
		g_array_append_val (iter->path, index);
	
	node = g_slice_new (GMimeObjectStack);
	node->indexed = index != -1;
	node->parent = iter->parent;
	node->object = object;
	iter->parent = node;
}

static gboolean
g_mime_part_iter_pop (GMimePartIter *iter)
{
	GMimeObjectStack *node;
	
	if (!iter->parent || !iter->parent->parent)
		return FALSE;
	
	if (iter->parent->indexed) {
		iter->index = g_array_index (iter->path, int, iter->path->len - 1);
		g_array_set_size (iter->path, iter->path->len - 1);
	}
	
	iter->current = iter->parent->object;
	
	node = iter->parent;
	iter->parent = node->parent;
	g_slice_free (GMimeObjectStack, node);
	
	return TRUE;
}


/**
 * g_mime_part_iter_new:
 * @toplevel: a #GMimeObject to use as the toplevel
 *
 * Creates a new #GMimePartIter for iterating over @toplevel's subparts.
 *
 * Returns: a newly allocated #GMimePartIter which should be freed
 * using g_mime_part_iter_free() when finished with it.
 **/
GMimePartIter *
g_mime_part_iter_new (GMimeObject *toplevel)
{
	GMimePartIter *iter;
	
	g_return_val_if_fail (GMIME_IS_OBJECT (toplevel), NULL);
	
	iter = g_slice_new (GMimePartIter);
	iter->path = g_array_new (FALSE, FALSE, sizeof (int));
	iter->toplevel = toplevel;
	g_object_ref (toplevel);
	iter->parent = NULL;
	
	g_mime_part_iter_reset (iter);
	
	return iter;
}


/**
 * g_mime_part_iter_free:
 * @iter: a #GMimePartIter
 *
 * Frees the memory allocated by g_mime_part_iter_new().
 **/
void
g_mime_part_iter_free (GMimePartIter *iter)
{
	if (iter == NULL)
		return;
	
	g_object_unref (iter->toplevel);
	g_array_free (iter->path, TRUE);
	if (iter->parent != NULL)
		g_slice_free_chain (GMimeObjectStack, iter->parent, parent);
	g_slice_free (GMimePartIter, iter);
}


/**
 * g_mime_part_iter_reset:
 * @iter: a #GMimePartIter
 *
 * Resets the state of @iter to its initial state.
 **/
void
g_mime_part_iter_reset (GMimePartIter *iter)
{
	g_return_if_fail (iter != NULL);
	
	if (GMIME_IS_MESSAGE (iter->toplevel))
		iter->current = g_mime_message_get_mime_part ((GMimeMessage *) iter->toplevel);
	else
		iter->current = iter->toplevel;
	
	g_slice_free_chain (GMimeObjectStack, iter->parent, parent);
	g_array_set_size (iter->path, 0);
	iter->parent = NULL;
	iter->index = -1;
	
	if (!GMIME_IS_PART (iter->current)) {
		/* set our initial 'current' part to our first child */
		g_mime_part_iter_next (iter);
	}
}


/**
 * g_mime_part_iter_jump_to:
 * @iter: a #GMimePartIter
 * @path: a string representing the path to jump to
 *
 * Updates the state of @iter to point to the #GMimeObject specified
 * by @path.
 *
 * Returns: %TRUE if the #GMimeObject specified by @path exists or
 * %FALSE otherwise.
 **/
gboolean
g_mime_part_iter_jump_to (GMimePartIter *iter, const char *path)
{
	GMimeMessagePart *message_part;
	GMimeMultipart *multipart;
	GMimeMessage *message;
	GMimeObject *current;
	GMimeObject *parent;
	const char *inptr;
	int index;
	char *dot;
	
	g_return_val_if_fail (iter != NULL, FALSE);
	
	if (!path || !path[0])
		return FALSE;
	
	g_mime_part_iter_reset (iter);
	
	if (!strcmp (path, "0"))
		return TRUE;
	
	parent = iter->parent->object;
	iter->current = NULL;
	current = NULL;
	inptr = path;
	index = -1;
	
	while (*inptr) {
		/* Note: path components are 1-based instead of 0-based */
		if ((index = strtol (inptr, &dot, 10)) <= 0 || errno == ERANGE ||
		    index == G_MAXINT || !(*dot == '.' || *dot == '\0'))
			return FALSE;
		
		/* normalize to a 0-based index */
		index--;
		
		if (GMIME_IS_MESSAGE_PART (parent)) {
			message_part = (GMimeMessagePart *) parent;
			if (!(message = g_mime_message_part_get_message (message_part)))
				return FALSE;
			
			if (!(parent = g_mime_message_get_mime_part (message)))
				return FALSE;
			
			if (!GMIME_IS_MULTIPART (parent))
				return FALSE;
			
			goto multipart;
		} else if (GMIME_IS_MULTIPART (parent)) {
		multipart:
			multipart = (GMimeMultipart *) parent;
			if (index >= g_mime_multipart_get_count (multipart))
				return FALSE;
			
			current = g_mime_multipart_get_part (multipart, index);
			iter->index = index;
		} else if (GMIME_IS_MESSAGE (parent)) {
			message = (GMimeMessage *) parent;
			if (!(current = g_mime_message_get_mime_part (message)))
				return FALSE;
			
			iter->index = -1;
		} else {
			return FALSE;
		}
		
		if (*dot == '\0')
			break;
		
		g_mime_part_iter_push (iter, current, iter->index);
		parent = current;
		current = NULL;
		index = -1;
		
		if (*dot != '.')
			break;
		
		inptr = dot + 1;
	}
	
	iter->current = current;
	iter->index = index;
	
	return current != NULL;
}


/**
 * g_mime_part_iter_is_valid:
 * @iter: a #GMimePartIter
 *
 * Checks that the current state of @iter is valid.
 *
 * Returns: %TRUE if @iter is valid or %FALSE otherwise.
 **/
gboolean
g_mime_part_iter_is_valid (GMimePartIter *iter)
{
	g_return_val_if_fail (iter != NULL, FALSE);
	
	return iter->current != NULL;
}


/**
 * g_mime_part_iter_next:
 * @iter: a #GMimePartIter
 *
 * Advances to the next part in the MIME structure used to initialize
 * @iter.
 *
 * Returns: %TRUE if successful or %FALSE otherwise.
 **/
gboolean
g_mime_part_iter_next (GMimePartIter *iter)
{
	GMimeMessagePart *message_part;
	GMimeMultipart *multipart;
	GMimeObject *mime_part;
	GMimeMessage *message;
	
	if (!g_mime_part_iter_is_valid (iter))
		return FALSE;
	
	if (GMIME_IS_MESSAGE_PART (iter->current)) {
		/* descend into our children */
		message_part = (GMimeMessagePart *) iter->current;
		message = g_mime_message_part_get_message (message_part);
		mime_part = message ? g_mime_message_get_mime_part (message) : NULL;
		if (mime_part != NULL) {
			g_mime_part_iter_push (iter, iter->current, iter->index);
			iter->current = mime_part;
			
			if (GMIME_IS_MULTIPART (mime_part)) {
				iter->index = -1;
				goto multipart;
			}
			
			iter->index = 0;
			
			return TRUE;
		}
	} else if (GMIME_IS_MULTIPART (iter->current)) {
		/* descend into our children */
	multipart:
		multipart = (GMimeMultipart *) iter->current;
		if (g_mime_multipart_get_count (multipart) > 0) {
			g_mime_part_iter_push (iter, iter->current, iter->index);
			iter->current = g_mime_multipart_get_part (multipart, 0);
			iter->index = 0;
			return TRUE;
		}
	}
	
	/* find the next sibling */
	while (iter->parent) {
		if (GMIME_IS_MULTIPART (iter->parent->object)) {
			/* iterate to the next part in the multipart */
			multipart = (GMimeMultipart *) iter->parent->object;
			iter->index++;
			
			if (g_mime_multipart_get_count (multipart) > iter->index) {
				iter->current = g_mime_multipart_get_part (multipart, iter->index);
				return TRUE;
			}
		}
		
		if (!g_mime_part_iter_pop (iter))
			break;
	}
	
	iter->current = NULL;
	iter->index = -1;
	
	return FALSE;
}


/**
 * g_mime_part_iter_prev:
 * @iter: a #GMimePartIter
 *
 * Rewinds to the previous part in the MIME structure used to
 * initialize @iter.
 *
 * Returns: %TRUE if successful or %FALSE otherwise.
 **/
gboolean
g_mime_part_iter_prev (GMimePartIter *iter)
{
	GMimeMultipart *multipart;
	
	if (!g_mime_part_iter_is_valid (iter))
		return FALSE;
	
	if (iter->parent == NULL) {
		iter->current = NULL;
		iter->index = -1;
		return FALSE;
	}
	
	if (GMIME_IS_MULTIPART (iter->parent->object)) {
		/* revert backward to the previous part in the multipart */
		multipart = (GMimeMultipart *) iter->parent->object;
		iter->index--;
		
		if (iter->index >= 0) {
			iter->current = g_mime_multipart_get_part (multipart, iter->index);
			return TRUE;
		}
	}
	
	return g_mime_part_iter_pop (iter);
}


/**
 * g_mime_part_iter_get_toplevel:
 * @iter: a #GMimePartIter
 *
 * Gets the toplevel #GMimeObject used to initialize @iter.
 *
 * Returns: (transfer none): the toplevel #GMimeObject.
 **/
GMimeObject *
g_mime_part_iter_get_toplevel (GMimePartIter *iter)
{
	g_return_val_if_fail (iter != NULL, NULL);
	
	return iter->toplevel;
}


/**
 * g_mime_part_iter_get_current:
 * @iter: a #GMimePartIter
 *
 * Gets the #GMimeObject at the current #GMimePartIter position.
 *
 * Returns: (transfer none): the current #GMimeObject or %NULL if the
 * state of @iter is invalid.
 **/
GMimeObject *
g_mime_part_iter_get_current (GMimePartIter *iter)
{
	g_return_val_if_fail (iter != NULL, NULL);
	
	return iter->current;
}


/**
 * g_mime_part_iter_get_parent:
 * @iter: a #GMimePartIter
 *
 * Gets the parent of the #GMimeObject at the current #GMimePartIter
 * position.
 *
 * Returns: (transfer none): the parent #GMimeObject or %NULL if the
 * state of @iter is invalid.
 **/
GMimeObject *
g_mime_part_iter_get_parent (GMimePartIter *iter)
{
	g_return_val_if_fail (iter != NULL, NULL);
	
	if (!g_mime_part_iter_is_valid (iter))
		return NULL;
	
	return iter->parent ? iter->parent->object : NULL;
}


/**
 * g_mime_part_iter_get_path:
 * @iter: a #GMimePartIter
 *
 * Gets the path of the current #GMimeObject in the MIME structure
 * used to initialize @iter.
 *
 * Returns: a newly allocated string representation of the path to the
 * #GMimeObject at the current #GMimePartIter position.
 **/
char *
g_mime_part_iter_get_path (GMimePartIter *iter)
{
	GString *path;
	int i, v;
	
	if (!g_mime_part_iter_is_valid (iter))
		return NULL;
	
	/* Note: path components are 1-based instead of 0-based */
	
	path = g_string_new ("");
	for (i = 0; i < iter->path->len; i++) {
		v = g_array_index (iter->path, int, i);
		g_string_append_printf (path, "%d.", v + 1);
	}
	
	g_string_append_printf (path, "%d", iter->index + 1);
	
	return g_string_free (path, FALSE);
}


/**
 * g_mime_part_iter_replace:
 * @iter: a #GMimePartIter
 * @replacement: a #GMimeObject
 *
 * Replaces the #GMimeObject at the current position with @replacement.
 *
 * Returns: %TRUE if the part at the current position was replaced or
 * %FALSE otherwise.
 **/
gboolean
g_mime_part_iter_replace (GMimePartIter *iter, GMimeObject *replacement)
{
	GMimeMessagePart *message_part;
	GMimeMessage *message;
	GMimeObject *current;
	GMimeObject *parent;
	int index;
	
	g_return_val_if_fail (GMIME_IS_OBJECT (replacement), FALSE);
	
	if (!g_mime_part_iter_is_valid (iter))
		return FALSE;
	
	if (iter->current == iter->toplevel) {
		g_object_unref (iter->toplevel);
		iter->toplevel = replacement;
		g_object_ref (replacement);
		return TRUE;
	}
	
	parent = iter->parent ? iter->parent->object : iter->toplevel;
	index = iter->index;
	
	/* now we can safely replace the previously referenced part in its parent */
	if (GMIME_IS_MESSAGE_PART (parent)) {
		/* depending on what we've been given as a
		 * replacement, we might replace the message in the
		 * message/rfc822 part or we might end up replacing
		 * the toplevel mime part of said message. */
		message_part = (GMimeMessagePart *) parent;
		message = g_mime_message_part_get_message (message_part);
		if (GMIME_IS_MESSAGE (replacement))
			g_mime_message_part_set_message (message_part, (GMimeMessage *) replacement);
		else
			g_mime_message_set_mime_part (message, replacement);
	} else if (GMIME_IS_MULTIPART (parent)) {
		current = g_mime_multipart_replace ((GMimeMultipart *) parent, index, replacement);
		g_object_unref (current);
	} else if (GMIME_IS_MESSAGE (parent)) {
		g_mime_message_set_mime_part ((GMimeMessage *) parent, replacement);
	} else {
		g_assert_not_reached ();
	}
	
	iter->current = replacement;
	
	return TRUE;
}


/**
 * g_mime_part_iter_remove:
 * @iter: a #GMimePartIter
 *
 * Removes the #GMimeObject at the current position from its
 * parent. If successful, @iter is advanced to the next position
 * (since the current position will become invalid).
 *
 * Returns: %TRUE if the part at the current position was removed or
 * %FALSE otherwise.
 **/
gboolean
g_mime_part_iter_remove (GMimePartIter *iter)
{
	GMimeObject *current;
	GMimeObject *parent;
	int index;
	
	if (!g_mime_part_iter_is_valid (iter))
		return FALSE;
	
	if (iter->current == iter->toplevel)
		return FALSE;
	
	parent = iter->parent ? iter->parent->object : iter->toplevel;
	current = iter->current;
	index = iter->index;
	
	/* iterate to the next part so we have something valid to refer to */
	g_mime_part_iter_next (iter);
	
	/* now we can safely remove the previously referenced part from its parent */
	if (GMIME_IS_MESSAGE_PART (parent)) {
		g_mime_message_part_set_message ((GMimeMessagePart *) parent, NULL);
	} else if (GMIME_IS_MULTIPART (parent)) {
		g_mime_multipart_remove_at ((GMimeMultipart *) parent, index);
		g_object_unref (current);
	} else if (GMIME_IS_MESSAGE (parent)) {
		g_mime_message_set_mime_part ((GMimeMessage *) parent, NULL);
	} else {
		g_assert_not_reached ();
	}
	
	return TRUE;
}
