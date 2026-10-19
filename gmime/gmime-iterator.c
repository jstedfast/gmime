/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2026 Jeffrey Stedfast
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

#include "gmime-iterator.h"
#include "gmime-message-part.h"
#include "gmime-multipart.h"
#include "gmime-error.h"

/**
 * SECTION: gmime-iterator
 * @title: GMimeIterator
 * @short_description: Depth-first MIME tree traversal and path navigation
 * @see_also: #GMimeMessage, #GMimeVisitor, #GMimePartIter
 *
 * Traverses a message's body, multipart children and encapsulated message
 * bodies in depth-first order, without visiting message objects themselves.
 * Newly created and reset iterators are positioned before the first entity.
 * Once exhausted, g_mime_iterator_move_next() returns %FALSE until
 * g_mime_iterator_reset() or g_mime_iterator_move_to().
 *
 * Path specifiers follow MimeKit's IMAP numbering: a message's single-part
 * body is part 1; multipart children are numbered from 1. A multipart body
 * has no part number of its own and is represented by a terminal 0.
 *
 * Traversal observes live collections. The iterator retains the message
 * and every entity in its current ancestry, keeping removed entities alive
 * until their frames are discarded. It does not snapshot or renumber frames
 * after external edits. Use g_mime_iterator_skip_children() to prune the
 * current subtree.
 * Cyclic MIME trees are not supported.
 *
 * This GObject API ports MimeKit's MimeIterator. The older boxed
 * #GMimePartIter retains its distinct traversal and editing semantics.
 **/

/* Each frame represents one level of the path from the message body to the current entity.
 * Index is the 0-based index of the entity within its parent multipart. Message bodies
 * have an index of 0 unless they are multiparts, in which case their index is -1
 * (they have no IMAP part number of their own). */
typedef struct {
	GMimeEntity *entity;
	int index;
} IteratorFrame;

#define FRAME(iter, index) g_array_index ((iter)->frames, IteratorFrame, (index))

static void iterator_finalize (GObject *object);

G_DEFINE_TYPE (GMimeIterator, g_mime_iterator, G_TYPE_OBJECT)

static void
frame_clear (gpointer data)
{
	IteratorFrame *frame = data;

	g_object_unref (frame->entity);
}

static void
g_mime_iterator_class_init (GMimeIteratorClass *klass)
{
	G_OBJECT_CLASS (klass)->finalize = iterator_finalize;
}

static void
g_mime_iterator_init (GMimeIterator *iter)
{
	iter->frames = g_array_new (FALSE, FALSE, sizeof (IteratorFrame));
	g_array_set_clear_func (iter->frames, frame_clear);
	iter->move_first = TRUE;
}

static void
iterator_finalize (GObject *object)
{
	GMimeIterator *iter = GMIME_ITERATOR (object);

	g_array_unref (iter->frames);
	g_clear_object (&iter->message);
	G_OBJECT_CLASS (g_mime_iterator_parent_class)->finalize (object);
}

static void
push_frame (GMimeIterator *iter, GMimeEntity *entity, int index)
{
	IteratorFrame frame = { g_object_ref (entity), index };

	g_array_append_val (iter->frames, frame);
}

static void
push_message_body (GMimeIterator *iter, GMimeEntity *body)
{
	push_frame (iter, body, GMIME_IS_MULTIPART (body) ? -1 : 0);
}

/**
 * g_mime_iterator_new:
 * @message: (transfer none): the message to traverse
 *
 * Creates an iterator positioned before the first entity. Retains @message.
 *
 * Returns: (transfer full): a new iterator.
 **/
GMimeIterator *
g_mime_iterator_new (GMimeMessage *message)
{
	GMimeIterator *iter;

	g_return_val_if_fail (GMIME_IS_MESSAGE (message), NULL);

	iter = g_object_new (GMIME_TYPE_ITERATOR, NULL);
	iter->message = g_object_ref (message);

	return iter;
}

/**
 * g_mime_iterator_get_message:
 * @iter: a #GMimeIterator
 *
 * Returns: (transfer none): the top-level message.
 **/
GMimeMessage *
g_mime_iterator_get_message (GMimeIterator *iter)
{
	g_return_val_if_fail (GMIME_IS_ITERATOR (iter), NULL);

	return iter->message;
}

/**
 * g_mime_iterator_is_valid:
 * @iter: a #GMimeIterator
 *
 * Returns: whether @iter currently points to an entity.
 **/
gboolean
g_mime_iterator_is_valid (GMimeIterator *iter)
{
	g_return_val_if_fail (GMIME_IS_ITERATOR (iter), FALSE);

	return iter->frames->len > 0;
}

/**
 * g_mime_iterator_get_current:
 * @iter: a positioned #GMimeIterator
 *
 * Requires a successful g_mime_iterator_move_next() or
 * g_mime_iterator_move_to() not followed by exhaustion or
 * g_mime_iterator_reset(). Use g_mime_iterator_is_valid() to check the state.
 *
 * Returns: (transfer none): the current entity, borrowed until navigation.
 **/
GMimeEntity *
g_mime_iterator_get_current (GMimeIterator *iter)
{
	g_return_val_if_fail (GMIME_IS_ITERATOR (iter), NULL);
	g_return_val_if_fail (iter->frames->len > 0, NULL);

	return FRAME (iter, iter->frames->len - 1).entity;
}

/**
 * g_mime_iterator_get_parent:
 * @iter: a positioned #GMimeIterator
 *
 * Returns: (transfer none) (nullable): the current entity's parent multipart
 * or message part, or %NULL for the top-level body. Borrowed until navigation.
 **/
GMimeEntity *
g_mime_iterator_get_parent (GMimeIterator *iter)
{
	g_return_val_if_fail (GMIME_IS_ITERATOR (iter), NULL);
	g_return_val_if_fail (iter->frames->len > 0, NULL);

	return iter->frames->len > 1 ? FRAME (iter, iter->frames->len - 2).entity : NULL;
}

/**
 * g_mime_iterator_get_path_specifier:
 * @iter: a positioned #GMimeIterator
 *
 * Returns: (transfer full): the current entity's IMAP-style path specifier.
 **/
char *
g_mime_iterator_get_path_specifier (GMimeIterator *iter)
{
	GString *specifier;
	guint i, last;

	g_return_val_if_fail (GMIME_IS_ITERATOR (iter), NULL);
	g_return_val_if_fail (iter->frames->len > 0, NULL);

	specifier = g_string_new (NULL);
	last = iter->frames->len - 1;
	for (i = 0; i < last; i++) {
		/* Multiparts that are message bodies have no part number of their own. */
		if (FRAME (iter, i).index == -1)
			continue;

		g_string_append_printf (specifier, "%d.", FRAME (iter, i).index + 1);
	}
	g_string_append_printf (specifier, "%d", FRAME (iter, last).index + 1);

	return g_string_free (specifier, FALSE);
}

/**
 * g_mime_iterator_get_depth:
 * @iter: a positioned #GMimeIterator
 *
 * Returns: the current entity's depth; the top-level body has depth 0.
 **/
int
g_mime_iterator_get_depth (GMimeIterator *iter)
{
	g_return_val_if_fail (GMIME_IS_ITERATOR (iter), -1);
	g_return_val_if_fail (iter->frames->len > 0, -1);

	return iter->frames->len - 1;
}

/**
 * g_mime_iterator_get_index:
 * @iter: a positioned #GMimeIterator
 *
 * Returns: the current entity's zero-based index in its parent multipart,
 * or -1 if it is a message body rather than a multipart child.
 **/
int
g_mime_iterator_get_index (GMimeIterator *iter)
{
	g_return_val_if_fail (GMIME_IS_ITERATOR (iter), -1);
	g_return_val_if_fail (iter->frames->len > 0, -1);

	if (iter->frames->len > 1 && GMIME_IS_MULTIPART (FRAME (iter, iter->frames->len - 2).entity))
		return FRAME (iter, iter->frames->len - 1).index;

	return -1;
}

/**
 * g_mime_iterator_get_current_message:
 * @iter: a positioned #GMimeIterator
 *
 * Returns: (transfer none): the nearest containing encapsulated message,
 * or the top-level message. This reflects live message-part assignments.
 **/
GMimeMessage *
g_mime_iterator_get_current_message (GMimeIterator *iter)
{
	GMimeEntity *entity;
	GMimeMessage *message;
	int i;

	g_return_val_if_fail (GMIME_IS_ITERATOR (iter), NULL);
	g_return_val_if_fail (iter->frames->len > 0, NULL);

	for (i = (int) iter->frames->len - 2; i >= 0; i--) {
		entity = FRAME (iter, i).entity;
		if (GMIME_IS_MESSAGE_PART (entity)) {
			message = g_mime_message_part_get_message ((GMimeMessagePart *) entity);
			if (message)
				return message;
		}
	}

	return iter->message;
}

/**
 * g_mime_iterator_get_ancestor:
 * @iter: a positioned #GMimeIterator
 * @generation: generations above the current entity, from 1 through depth
 *
 * Returns: (transfer none): the ancestor, borrowed until navigation.
 **/
GMimeEntity *
g_mime_iterator_get_ancestor (GMimeIterator *iter, int generation)
{
	g_return_val_if_fail (GMIME_IS_ITERATOR (iter), NULL);
	g_return_val_if_fail (iter->frames->len > 0, NULL);
	g_return_val_if_fail (generation >= 1 && (guint) generation < iter->frames->len, NULL);

	return FRAME (iter, iter->frames->len - 1 - generation).entity;
}

/**
 * g_mime_iterator_move_next:
 * @iter: a #GMimeIterator
 *
 * Advances in depth-first order, descending into multipart children and
 * nonempty encapsulated message bodies unless g_mime_iterator_skip_children()
 * was requested.
 * Does not decode leaf content. Exhaustion releases the current ancestry.
 *
 * Returns: %TRUE if positioned on an entity, or %FALSE at the end.
 **/
gboolean
g_mime_iterator_move_next (GMimeIterator *iter)
{
	GMimeEntity *current, *body, *parent;
	GMimeMessage *message;
	int index;

	g_return_val_if_fail (GMIME_IS_ITERATOR (iter), FALSE);
	g_return_val_if_fail (GMIME_IS_MESSAGE (iter->message), FALSE);

	if (iter->move_first) {
		iter->move_first = FALSE;
		body = g_mime_message_get_mime_part (iter->message);
		if (!body)
			return FALSE;

		push_message_body (iter, body);
		return TRUE;
	}

	if (iter->frames->len == 0)
		return FALSE;

	current = FRAME (iter, iter->frames->len - 1).entity;
	if (iter->skip_requested) {
		iter->skip_requested = FALSE;
	} else if (GMIME_IS_MESSAGE_PART (current)) {
		message = g_mime_message_part_get_message ((GMimeMessagePart *) current);
		body = message ? g_mime_message_get_mime_part (message) : NULL;
		if (body) {
			push_message_body (iter, body);
			return TRUE;
		}
	} else if (GMIME_IS_MULTIPART (current) && g_mime_multipart_get_count ((GMimeMultipart *) current) > 0) {
		push_frame (iter, g_mime_multipart_get_part ((GMimeMultipart *) current, 0), 0);
		return TRUE;
	}

	/* Find the next sibling of the current entity or of its nearest ancestor that has one. */
	while (iter->frames->len > 1) {
		index = FRAME (iter, iter->frames->len - 1).index + 1;
		g_array_set_size (iter->frames, iter->frames->len - 1);
		parent = FRAME (iter, iter->frames->len - 1).entity;
		if (GMIME_IS_MULTIPART (parent) && index < g_mime_multipart_get_count ((GMimeMultipart *) parent)) {
			push_frame (iter, g_mime_multipart_get_part ((GMimeMultipart *) parent, index), index);
			return TRUE;
		}
	}

	g_array_set_size (iter->frames, 0);
	return FALSE;
}

/**
 * g_mime_iterator_skip_children:
 * @iter: a positioned #GMimeIterator
 *
 * Requests that the next g_mime_iterator_move_next() skip the current
 * entity's descendants. Has no effect on leaves. g_mime_iterator_reset()
 * and syntactically valid g_mime_iterator_move_to() calls cancel the request.
 **/
void
g_mime_iterator_skip_children (GMimeIterator *iter)
{
	g_return_if_fail (GMIME_IS_ITERATOR (iter));
	g_return_if_fail (iter->frames->len > 0);

	iter->skip_requested = TRUE;
}

static gboolean
parse_path (const char *path, GArray *indexes)
{
	const unsigned char *p = (const unsigned char *) path;
	int index, digit;

	do {
		if (*p < '0' || *p > '9')
			return FALSE;

		index = 0;
		do {
			digit = *p++ - '0';
			if (index > (G_MAXINT - digit) / 10)
				return FALSE;
			index = index * 10 + digit;
		} while (*p >= '0' && *p <= '9');

		index--;
		g_array_append_val (indexes, index);
		if (!*p)
			return TRUE;
	} while (*p++ == '.');

	return FALSE;
}

static gboolean
try_navigate (GMimeIterator *iter, GArray *indexes)
{
	GMimeEntity *body = g_mime_message_get_mime_part (iter->message);
	GMimeEntity *entity;
	GMimeMessage *message;
	IteratorFrame frame;
	guint i = 0;
	int index;

	if (!body)
		return FALSE;

	push_message_body (iter, body);
	if (!GMIME_IS_MULTIPART (body)) {
		/* A non-multipart top-level body is part 1. */
		if (g_array_index (indexes, int, 0) != 0)
			return FALSE;
		i++;
	}

	while (i < indexes->len) {
		frame = FRAME (iter, iter->frames->len - 1);
		entity = frame.entity;
		index = g_array_index (indexes, int, i);
		if (GMIME_IS_MULTIPART (entity)) {
			if (index == -1) {
				/* "0" refers to a multipart that is a message body and must be the last component. */
				return frame.index == -1 && i == indexes->len - 1;
			}
			if (index >= g_mime_multipart_get_count ((GMimeMultipart *) entity))
				return FALSE;

			push_frame (iter, g_mime_multipart_get_part ((GMimeMultipart *) entity, index), index);
			i++;
		} else if (GMIME_IS_MESSAGE_PART (entity)) {
			message = g_mime_message_part_get_message ((GMimeMessagePart *) entity);
			body = message ? g_mime_message_get_mime_part (message) : NULL;
			if (!body)
				return FALSE;

			if (GMIME_IS_MULTIPART (body)) {
				/* A multipart message body has no part number of its own, so the next
				 * component addresses either the multipart itself ("0") or one of its children. */
				push_frame (iter, body, -1);
			} else {
				if (index != 0)
					return FALSE;
				push_frame (iter, body, 0);
				i++;
			}
		} else {
			return FALSE;
		}
	}

	return TRUE;
}

/**
 * g_mime_iterator_move_to:
 * @iter: a #GMimeIterator
 * @path_specifier: a nonempty IMAP-style path specifier
 * @error: (nullable): return location for a #GError
 *
 * Navigates directly to a path. Components are unsigned ASCII decimal
 * integers no greater than 2147483647, separated by dots; leading zeroes
 * are accepted. Invalid syntax sets %GMIME_ERROR_PARSE_ERROR and preserves
 * the current position and pending skip. An empty or %NULL path is a
 * programming error. A valid but nonexistent path exhausts the iterator
 * without an error. A valid path cancels a pending skip.
 *
 * Returns: %TRUE if positioned on an entity, or %FALSE if not found or on error.
 **/
gboolean
g_mime_iterator_move_to (GMimeIterator *iter, const char *path_specifier, GError **error)
{
	GArray *indexes;
	gboolean found;

	g_return_val_if_fail (GMIME_IS_ITERATOR (iter), FALSE);
	g_return_val_if_fail (GMIME_IS_MESSAGE (iter->message), FALSE);
	g_return_val_if_fail (path_specifier != NULL && *path_specifier != '\0', FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	indexes = g_array_new (FALSE, FALSE, sizeof (int));
	if (!parse_path (path_specifier, indexes)) {
		g_array_unref (indexes);
		g_set_error_literal (error, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR, "Invalid path specifier format.");
		return FALSE;
	}

	g_array_set_size (iter->frames, 0);
	iter->skip_requested = FALSE;
	iter->move_first = FALSE;
	found = try_navigate (iter, indexes);
	g_array_unref (indexes);
	if (!found)
		g_array_set_size (iter->frames, 0);

	return found;
}

/**
 * g_mime_iterator_reset:
 * @iter: a #GMimeIterator
 *
 * Releases the current ancestry, cancels a pending skip and positions @iter
 * before the first entity. The next g_mime_iterator_move_next() reads the
 * message's live body.
 **/
void
g_mime_iterator_reset (GMimeIterator *iter)
{
	g_return_if_fail (GMIME_IS_ITERATOR (iter));

	iter->move_first = TRUE;
	iter->skip_requested = FALSE;
	g_array_set_size (iter->frames, 0);
}
