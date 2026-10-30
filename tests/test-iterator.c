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

#include <string.h>
#include <gmime/gmime.h>
#include "testsuite.h"

static GMimeEntity *
named_entity (GMimeEntity *entity, const char *name)
{
	g_object_set_data (G_OBJECT (entity), "iterator-name", (gpointer) name);
	return entity;
}

static const char *
entity_name (gpointer entity)
{
	return entity ? g_object_get_data (G_OBJECT (entity), "iterator-name") : "-";
}

static void
add_child (GMimeMultipart *parent, GMimeEntity *child, const char *name)
{
	g_mime_multipart_add (parent, named_entity (child, name));
	g_object_unref (child);
}

static GMimeMessagePart *
embedded_message (GMimeEntity *body, const char *name)
{
	GMimeMessage *message = g_mime_message_new (FALSE);
	GMimeMessagePart *part = g_mime_message_part_new ("rfc822");

	named_entity ((GMimeEntity *) message, name);
	g_mime_message_set_mime_part (message, body);
	g_mime_message_part_set_message (part, message);
	g_object_unref (message);

	return part;
}

static GMimeMessage *
iterator_message (void)
{
	GMimeMessage *message = g_mime_message_new (FALSE);
	GMimeMultipart *root = g_mime_multipart_new ();
	GMimeMultipart *inner = g_mime_multipart_new ();
	GMimeMultipart *four = g_mime_multipart_new ();
	GMimeMultipart *deep = g_mime_multipart_new ();
	GMimeMultipart *alternative = (GMimeMultipart *) g_mime_multipart_alternative_new ();
	GMimeMessagePart *part;

	named_entity ((GMimeEntity *) message, "message");
	named_entity ((GMimeEntity *) root, "root");
	g_mime_message_set_mime_part (message, (GMimeEntity *) root);
	add_child (root, (GMimeEntity *) g_mime_text_part_new (), "text");
	add_child (root, (GMimeEntity *) g_mime_part_new (), "part");

	named_entity ((GMimeEntity *) inner, "inner");
	add_child (inner, (GMimeEntity *) g_mime_text_part_new (), "inner-text");
	add_child (inner, (GMimeEntity *) g_mime_part_new (), "inner-part");
	part = embedded_message ((GMimeEntity *) inner, "message3");
	g_object_unref (inner);
	add_child (root, (GMimeEntity *) part, "rfc822");

	add_child (four, (GMimeEntity *) g_mime_part_new_with_type ("image", "gif"), "image");
	named_entity ((GMimeEntity *) deep, "deep");
	add_child (deep, (GMimeEntity *) g_mime_text_part_new (), "deep-text");
	add_child (alternative, (GMimeEntity *) g_mime_text_part_new (), "plain");
	add_child (alternative, (GMimeEntity *) g_mime_text_part_new_with_subtype ("richtext"), "richtext");
	add_child (deep, (GMimeEntity *) alternative, "alternative");
	part = embedded_message ((GMimeEntity *) deep, "message42");
	g_object_unref (deep);
	add_child (four, (GMimeEntity *) part, "deep-rfc822");
	add_child (root, (GMimeEntity *) four, "four");
	g_object_unref (root);

	return message;
}

static void
assert_path (GMimeIterator *iter, const char *expected)
{
	char *path = g_mime_iterator_get_path_specifier (iter);

	g_assert_cmpstr (path, ==, expected);
	g_free (path);
}

static void
assert_move_to (GMimeIterator *iter, const char *path, gboolean expected)
{
	GError *error = NULL;

	g_assert_cmpint (g_mime_iterator_move_to (iter, path, &error), ==, expected);
	g_assert_no_error (error);
	g_assert_cmpint (g_mime_iterator_is_valid (iter), ==, expected);
}

static void
append_position (GString *trace, GMimeIterator *iter)
{
	char *path = g_mime_iterator_get_path_specifier (iter);
	int i, depth = g_mime_iterator_get_depth (iter);

	g_string_append_printf (trace, "%s:%s:%s:%d:%d:%s", path,
				entity_name (g_mime_iterator_get_current (iter)),
				entity_name (g_mime_iterator_get_parent (iter)), depth,
				g_mime_iterator_get_index (iter),
				entity_name (g_mime_iterator_get_current_message (iter)));
	for (i = 1; i <= depth; i++)
		g_string_append_printf (trace, ":%s", entity_name (g_mime_iterator_get_ancestor (iter, i)));
	g_string_append_c (trace, ';');
	g_free (path);
}

static char *
iterator_trace (int mode)
{
	GMimeMessage *message = iterator_message ();
	GMimeIterator *iter = g_mime_iterator_new (message);
	GMimeMultipart *root = (GMimeMultipart *) g_mime_message_get_mime_part (message);
	GString *trace = g_string_new (NULL);
	GMimeEntity *current;
	const char *name;

	while (g_mime_iterator_move_next (iter)) {
		append_position (trace, iter);
		current = g_mime_iterator_get_current (iter);
		name = entity_name (current);
		if ((mode == 1 && GMIME_IS_MESSAGE_PART (current)) ||
		    (mode == 2 && !strcmp (name, "root")) ||
		    (mode == 3 && !strcmp (name, "four")))
			g_mime_iterator_skip_children (iter);
		if (!strcmp (name, "text")) {
			if (mode == 4)
				add_child (root, (GMimeEntity *) g_mime_part_new (), "appended");
			else if (mode == 5)
				g_assert_true (g_mime_multipart_remove (root, current));
			else if (mode == 6)
				g_mime_multipart_clear (root);
		}
	}
	g_assert_false (g_mime_iterator_is_valid (iter));
	g_assert_false (g_mime_iterator_move_next (iter));
	g_object_unref (iter);
	g_object_unref (message);

	return g_string_free (trace, FALSE);
}

static void
test_traversal (void)
{
	const char *paths[] = {
		"0", "1", "2", "3", "3.0", "3.1", "3.2", "4", "4.1",
		"4.2", "4.2.0", "4.2.1", "4.2.2", "4.2.2.1", "4.2.2.2"
	};
	const char *parents[] = {
		"-", "root", "root", "root", "rfc822", "inner", "inner", "root", "four",
		"four", "deep-rfc822", "deep", "deep", "alternative", "alternative"
	};
	const int depths[] = { 0, 1, 1, 1, 2, 3, 3, 1, 2, 2, 3, 4, 4, 5, 5 };
	const int indexes[] = { -1, 0, 1, 2, -1, 0, 1, 3, 0, 1, -1, 0, 1, 0, 1 };
	GMimeMessage *message = iterator_message ();
	GMimeIterator *iter = g_mime_iterator_new (message);
	GMimeEntity *current;
	char *trace;
	guint i, pass;
	int generation;

	g_assert_true (g_mime_iterator_get_message (iter) == message);
	g_assert_false (g_mime_iterator_is_valid (iter));
	for (pass = 0; pass < 2; pass++) {
		i = 0;
		while (g_mime_iterator_move_next (iter)) {
			g_assert_cmpuint (i, <, G_N_ELEMENTS (paths));
			assert_path (iter, paths[i]);
			g_assert_cmpint (g_mime_iterator_get_depth (iter), ==, depths[i]);
			g_assert_cmpint (g_mime_iterator_get_index (iter), ==, indexes[i]);
			g_assert_cmpstr (entity_name (g_mime_iterator_get_parent (iter)), ==, parents[i]);
			g_assert_cmpstr (entity_name (g_mime_iterator_get_current_message (iter)), ==,
					i >= 10 ? "message42" : i >= 4 && i <= 6 ? "message3" : "message");
			current = g_mime_iterator_get_current (iter);
			if (indexes[i] != -1)
				g_assert_true (g_mime_multipart_get_part ((GMimeMultipart *) g_mime_iterator_get_parent (iter),
									indexes[i]) == current);
			if (GMIME_IS_PART (current))
				g_assert_null (g_mime_part_get_content ((GMimePart *) current));
			i++;
		}
		g_assert_cmpuint (i, ==, G_N_ELEMENTS (paths));
		g_assert_false (g_mime_iterator_move_next (iter));
		g_mime_iterator_reset (iter);
	}
	for (i = 0; i < G_N_ELEMENTS (paths); i++) {
		assert_move_to (iter, paths[i], TRUE);
		assert_path (iter, paths[i]);
		g_assert_cmpint (g_mime_iterator_get_depth (iter), ==, depths[i]);
		g_assert_cmpint (g_mime_iterator_get_index (iter), ==, indexes[i]);
	}
	for (generation = 1; generation <= 5; generation++) {
		g_assert_nonnull (g_mime_iterator_get_ancestor (iter, generation));
		if (generation == 1)
			g_assert_true (g_mime_iterator_get_ancestor (iter, generation) == g_mime_iterator_get_parent (iter));
	}
	assert_move_to (iter, "3.1", TRUE);
	g_assert_true (g_mime_iterator_move_next (iter));
	assert_path (iter, "3.2");
	g_assert_true (g_mime_iterator_move_next (iter));
	assert_path (iter, "4");
	g_object_unref (iter);
	g_object_unref (message);

	for (i = 0; i <= 6; i++) {
		trace = iterator_trace (i);
		g_assert_nonnull (trace);
		if (i == 1)
			g_assert_null (strstr (trace, "inner-text"));
		else if (i == 2)
			g_assert_cmpstr (trace, ==, "0:root:-:0:-1:message;");
		else if (i == 3)
			g_assert_null (strstr (trace, "image"));
		else if (i == 4)
			g_assert_nonnull (strstr (trace, "5:appended:root:1:4:message:root;"));
		else if (i == 5) {
			g_assert_null (strstr (trace, "2:part:"));
			g_assert_nonnull (strstr (trace, "2:rfc822:root:1:1:message:root;"));
		} else if (i == 6)
			g_assert_cmpstr (trace, ==, "0:root:-:0:-1:message;1:text:root:1:0:message:root;");
		g_free (trace);
	}
}

static void
test_navigation_and_skipping (void)
{
	const char *missing[] = {
		"0.1", "4.0", "3.0.1", "5", "3.3", "4.2.2.3", "1.1", "4.2.2.1.1",
		"2147483647", "000000000000000000000002147483647", "1.0", "4.2.2.0"
	};
	const char *invalid[] = {
		"xyz", "-1", "+1", "1..2", "1.", ".1", " 1", "1 ", "1\t", "1\n",
		"2147483648", "999999999999999999999999999999", "1/2", "1e0", "\xc2\xb2", "1.\xff"
	};
	GMimeMessage *message = iterator_message ();
	GMimeIterator *iter = g_mime_iterator_new (message);
	GError *error = NULL;
	guint i;

	for (i = 0; i < G_N_ELEMENTS (missing); i++) {
		assert_move_to (iter, missing[i], FALSE);
		g_assert_false (g_mime_iterator_move_next (iter));
	}
	for (i = 0; i < G_N_ELEMENTS (invalid); i++) {
		assert_move_to (iter, "4", TRUE);
		g_mime_iterator_skip_children (iter);
		g_assert_false (g_mime_iterator_move_to (iter, invalid[i], &error));
		g_assert_error (error, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR);
		g_clear_error (&error);
		assert_path (iter, "4");
		g_assert_false (g_mime_iterator_move_next (iter));
	}
	assert_move_to (iter, "000000000000000000000000003.0000000000000000000000000", TRUE);
	assert_path (iter, "3.0");
	g_mime_iterator_skip_children (iter);
	g_assert_true (g_mime_iterator_move_next (iter));
	assert_path (iter, "4");
	g_mime_iterator_skip_children (iter);
	assert_move_to (iter, "4", TRUE);
	g_assert_true (g_mime_iterator_move_next (iter));
	assert_path (iter, "4.1");
	g_mime_iterator_skip_children (iter);
	g_assert_true (g_mime_iterator_move_next (iter));
	assert_path (iter, "4.2");
	g_mime_iterator_skip_children (iter);
	g_mime_iterator_reset (iter);
	g_assert_true (g_mime_iterator_move_next (iter));
	g_assert_true (g_mime_iterator_move_next (iter));
	assert_path (iter, "1");
	g_mime_iterator_reset (iter);
	g_assert_false (g_mime_iterator_move_to (iter, "xyz", &error));
	g_assert_error (error, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR);
	g_clear_error (&error);
	g_assert_true (g_mime_iterator_move_next (iter));
	assert_path (iter, "0");
	g_object_unref (iter);
	g_object_unref (message);
}

static void
test_empty_single_and_headers (void)
{
	GMimeMessage *message = g_mime_message_new (FALSE), *nested;
	GMimeIterator *iter = g_mime_iterator_new (message);
	GMimeTextPart *text = g_mime_text_part_new ();
	GMimeMessagePart *part = g_mime_message_part_new ("rfc822");
	GMimeTextRfc822Headers *headers = g_mime_text_rfc822_headers_new ();
	GMimeMultipart *root = g_mime_multipart_new ();

	g_assert_false (g_mime_iterator_move_next (iter));
	g_mime_message_set_mime_part (message, (GMimeEntity *) text);
	g_assert_false (g_mime_iterator_move_next (iter));
	g_mime_iterator_reset (iter);
	g_assert_true (g_mime_iterator_move_next (iter));
	assert_path (iter, "1");
	g_assert_null (g_mime_iterator_get_parent (iter));
	g_assert_cmpint (g_mime_iterator_get_index (iter), ==, -1);
	g_assert_false (g_mime_iterator_move_next (iter));
	assert_move_to (iter, "0", FALSE);
	assert_move_to (iter, "2", FALSE);
	assert_move_to (iter, "1", TRUE);

	nested = g_mime_message_new (FALSE);
	g_mime_message_set_mime_part (nested, (GMimeEntity *) text);
	g_mime_message_part_set_message (part, nested);
	g_mime_message_set_mime_part (message, (GMimeEntity *) part);
	assert_move_to (iter, "1.1", TRUE);
	g_assert_true (g_mime_iterator_get_current (iter) == (GMimeEntity *) text);
	g_assert_true (g_mime_iterator_get_parent (iter) == (GMimeEntity *) part);
	g_assert_true (g_mime_iterator_get_current_message (iter) == nested);
	g_assert_cmpint (g_mime_iterator_get_index (iter), ==, -1);
	assert_move_to (iter, "1.0", FALSE);
	assert_move_to (iter, "1.2", FALSE);
	assert_move_to (iter, "1", TRUE);
	g_assert_true (g_mime_iterator_move_next (iter));
	assert_path (iter, "1.1");

	g_mime_message_part_set_message ((GMimeMessagePart *) headers, nested);
	g_mime_message_set_mime_part (message, (GMimeEntity *) headers);
	assert_move_to (iter, "1.1", TRUE);
	g_assert_true (g_mime_iterator_get_parent (iter) == (GMimeEntity *) headers);
	g_object_unref (nested);
	nested = g_mime_message_new (FALSE);
	g_mime_message_part_set_message ((GMimeMessagePart *) headers, nested);
	g_mime_message_part_set_message (part, NULL);
	g_mime_multipart_add (root, (GMimeEntity *) part);
	g_mime_multipart_add (root, (GMimeEntity *) headers);
	g_mime_multipart_add (root, (GMimeEntity *) text);
	g_mime_message_set_mime_part (message, (GMimeEntity *) root);
	assert_move_to (iter, "1.1", FALSE);
	assert_move_to (iter, "2.1", FALSE);
	assert_move_to (iter, "0", TRUE);
	g_assert_true (g_mime_iterator_move_next (iter));
	assert_path (iter, "1");
	g_assert_true (g_mime_iterator_move_next (iter));
	assert_path (iter, "2");
	g_assert_true (g_mime_iterator_move_next (iter));
	assert_path (iter, "3");
	g_assert_false (g_mime_iterator_move_next (iter));
	g_mime_multipart_clear (root);
	assert_move_to (iter, "0", TRUE);
	g_assert_false (g_mime_iterator_move_next (iter));
	g_object_unref (root);
	g_object_unref (headers);
	g_object_unref (part);
	g_object_unref (text);
	g_object_unref (nested);
	g_object_unref (iter);
	g_object_unref (message);
}

static void
test_lifetime_and_live_messages (void)
{
	GMimeMessage *message = iterator_message (), *replacement = g_mime_message_new (FALSE);
	GMimeIterator *iter = g_mime_iterator_new (message);
	GMimeEntity *body = g_mime_message_get_mime_part (message), *current, *part;
	GMimeEntity *new_body = (GMimeEntity *) g_mime_text_part_new ();
	gpointer weak_body = body, weak_message = message, weak_current;

	g_object_add_weak_pointer (G_OBJECT (body), &weak_body);
	g_object_add_weak_pointer (G_OBJECT (message), &weak_message);
	assert_move_to (iter, "3.1", TRUE);
	current = g_mime_iterator_get_current (iter);
	weak_current = current;
	g_object_add_weak_pointer (G_OBJECT (current), &weak_current);
	part = g_mime_iterator_get_ancestor (iter, 2);
	g_mime_message_part_set_message ((GMimeMessagePart *) part, replacement);
	g_assert_true (g_mime_iterator_get_current_message (iter) == replacement);
	g_assert_nonnull (weak_current);
	g_mime_message_part_set_message ((GMimeMessagePart *) part, NULL);
	g_assert_true (g_mime_iterator_get_current_message (iter) == message);
	g_mime_message_set_mime_part (message, new_body);
	g_object_unref (new_body);
	g_object_unref (message);
	g_assert_nonnull (weak_message);
	g_assert_nonnull (weak_body);
	g_assert_nonnull (weak_current);
	assert_path (iter, "3.1");
	g_mime_iterator_reset (iter);
	g_assert_null (weak_body);
	g_assert_null (weak_current);
	g_assert_nonnull (weak_message);
	g_object_unref (iter);
	g_assert_null (weak_message);
	g_object_unref (replacement);
}

static void
test_entity_subtree (void)
{
	const char *paths[] = { "0", "1", "2", "2.0", "2.1", "2.2", "2.2.1", "2.2.2" };
	const char *parents[] = { "-", "four", "four", "deep-rfc822", "deep", "deep", "alternative", "alternative" };
	const int depths[] = { 0, 1, 1, 2, 3, 3, 4, 4 };
	const int indexes[] = { -1, 0, 1, -1, 0, 1, 0, 1 };
	GMimeMessage *message = iterator_message ();
	GMimeMultipart *body = (GMimeMultipart *) g_mime_message_get_mime_part (message);
	GMimeEntity *root = g_mime_multipart_get_part (body, 3), *current;
	GMimeIterator *iter = g_mime_iterator_new_for_entity (root);
	GMimeIterator *seek = g_mime_iterator_new_for_entity (root);
	gpointer weak_message = message, weak_root = root, weak_current;
	GError *error = NULL;
	guint i, pass;

	g_object_add_weak_pointer (G_OBJECT (message), &weak_message);
	g_object_add_weak_pointer (G_OBJECT (root), &weak_root);
	g_object_unref (message);
	g_assert_null (weak_message);
	g_assert_nonnull (weak_root);
	g_assert_null (g_mime_iterator_get_message (iter));
	g_assert_false (g_mime_iterator_is_valid (iter));

	for (pass = 0; pass < 2; pass++) {
		i = 0;
		while (g_mime_iterator_move_next (iter)) {
			g_assert_cmpuint (i, <, G_N_ELEMENTS (paths));
			assert_path (iter, paths[i]);
			g_assert_cmpint (g_mime_iterator_get_depth (iter), ==, depths[i]);
			g_assert_cmpint (g_mime_iterator_get_index (iter), ==, indexes[i]);
			g_assert_cmpstr (entity_name (g_mime_iterator_get_parent (iter)), ==, parents[i]);
			g_assert_cmpstr (entity_name (g_mime_iterator_get_current_message (iter)), ==,
					i < 3 ? "-" : "message42");
			if (depths[i] > 0)
				g_assert_true (g_mime_iterator_get_ancestor (iter, depths[i]) == root);
			assert_move_to (seek, paths[i], TRUE);
			g_assert_true (g_mime_iterator_get_current (seek) == g_mime_iterator_get_current (iter));
			assert_path (seek, paths[i]);
			i++;
		}
		g_assert_cmpuint (i, ==, G_N_ELEMENTS (paths));
		g_assert_false (g_mime_iterator_move_next (iter));
		g_mime_iterator_reset (iter);
	}
	g_object_unref (seek);

	assert_move_to (iter, "4.2", FALSE);
	assert_move_to (iter, "0.1", FALSE);
	g_assert_false (g_mime_iterator_move_next (iter));
	assert_move_to (iter, "2", TRUE);
	g_mime_iterator_skip_children (iter);
	g_assert_false (g_mime_iterator_move_to (iter, "2.", &error));
	g_assert_error (error, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR);
	g_clear_error (&error);
	assert_path (iter, "2");
	g_assert_false (g_mime_iterator_move_next (iter));
	g_mime_iterator_reset (iter);
	g_assert_true (g_mime_iterator_move_next (iter));
	g_mime_iterator_skip_children (iter);
	g_assert_false (g_mime_iterator_move_next (iter));

	assert_move_to (iter, "2.2.2", TRUE);
	current = g_mime_iterator_get_current (iter);
	weak_current = current;
	g_object_add_weak_pointer (G_OBJECT (current), &weak_current);
	g_mime_multipart_clear ((GMimeMultipart *) root);
	g_assert_nonnull (weak_current);
	assert_path (iter, "2.2.2");
	g_mime_iterator_reset (iter);
	g_assert_null (weak_current);
	g_assert_nonnull (weak_root);
	g_assert_true (g_mime_iterator_move_next (iter));
	assert_path (iter, "0");
	g_assert_false (g_mime_iterator_move_next (iter));
	add_child ((GMimeMultipart *) root, (GMimeEntity *) g_mime_text_part_new (), "appended");
	g_assert_false (g_mime_iterator_move_next (iter));
	assert_move_to (iter, "1", TRUE);
	g_assert_cmpstr (entity_name (g_mime_iterator_get_current (iter)), ==, "appended");
	g_object_unref (iter);
	g_assert_null (weak_root);
}

static void
test_entity_root_kinds (void)
{
	GMimeTextPart *text = g_mime_text_part_new ();
	GMimeMultipart *multipart = g_mime_multipart_new ();
	GMimeMessagePart *part = embedded_message ((GMimeEntity *) text, "nested");
	GMimeMessage *message = g_mime_message_new (FALSE);
	GMimeIterator *iter = g_mime_iterator_new_for_entity ((GMimeEntity *) text);
	gpointer weak_text = text, weak_message = message;

	g_object_add_weak_pointer (G_OBJECT (text), &weak_text);
	g_object_unref (text);
	g_assert_true (g_mime_iterator_move_next (iter));
	assert_path (iter, "1");
	g_assert_cmpint (g_mime_iterator_get_depth (iter), ==, 0);
	g_assert_cmpint (g_mime_iterator_get_index (iter), ==, -1);
	g_assert_null (g_mime_iterator_get_parent (iter));
	g_assert_null (g_mime_iterator_get_current_message (iter));
	g_assert_false (g_mime_iterator_move_next (iter));
	assert_move_to (iter, "0", FALSE);
	assert_move_to (iter, "1.1", FALSE);
	assert_move_to (iter, "1", TRUE);
	g_object_unref (iter);

	iter = g_mime_iterator_new_for_entity ((GMimeEntity *) part);
	g_assert_true (g_mime_iterator_move_next (iter));
	assert_path (iter, "1");
	g_assert_null (g_mime_iterator_get_message (iter));
	g_assert_null (g_mime_iterator_get_current_message (iter));
	g_assert_true (g_mime_iterator_move_next (iter));
	assert_path (iter, "1.1");
	g_assert_true (g_mime_iterator_get_current (iter) == (GMimeEntity *) text);
	g_assert_true (g_mime_iterator_get_parent (iter) == (GMimeEntity *) part);
	g_assert_true (g_mime_iterator_get_current_message (iter) == g_mime_message_part_get_message (part));
	g_assert_cmpint (g_mime_iterator_get_index (iter), ==, -1);
	g_assert_false (g_mime_iterator_move_next (iter));
	assert_move_to (iter, "1.1", TRUE);
	assert_move_to (iter, "1.0", FALSE);
	g_mime_message_set_mime_part (g_mime_message_part_get_message (part), (GMimeEntity *) multipart);
	g_assert_null (weak_text);
	assert_move_to (iter, "1.0", TRUE);
	g_assert_true (g_mime_iterator_get_current (iter) == (GMimeEntity *) multipart);
	g_mime_message_part_set_message (part, NULL);
	g_assert_null (g_mime_iterator_get_current_message (iter));
	g_mime_iterator_reset (iter);
	g_assert_true (g_mime_iterator_move_next (iter));
	g_assert_false (g_mime_iterator_move_next (iter));
	assert_move_to (iter, "1.0", FALSE);
	g_object_unref (part);
	g_object_unref (iter);

	iter = g_mime_iterator_new_for_entity ((GMimeEntity *) multipart);
	g_object_unref (multipart);
	g_assert_true (g_mime_iterator_move_next (iter));
	assert_path (iter, "0");
	g_assert_false (g_mime_iterator_move_next (iter));
	assert_move_to (iter, "1", FALSE);
	assert_move_to (iter, "0", TRUE);
	g_object_unref (iter);

	iter = g_mime_iterator_new_for_entity ((GMimeEntity *) message);
	g_object_add_weak_pointer (G_OBJECT (message), &weak_message);
	g_assert_true (g_mime_iterator_get_message (iter) == message);
	g_assert_false (g_mime_iterator_move_next (iter));
	text = g_mime_text_part_new ();
	g_mime_message_set_mime_part (message, (GMimeEntity *) text);
	g_object_unref (text);
	g_object_unref (message);
	g_assert_nonnull (weak_message);
	g_assert_false (g_mime_iterator_move_next (iter));
	g_mime_iterator_reset (iter);
	g_assert_true (g_mime_iterator_move_next (iter));
	assert_path (iter, "1");
	g_assert_true (g_mime_iterator_get_current (iter) == (GMimeEntity *) text);
	g_assert_true (g_mime_iterator_get_current_message (iter) == message);
	assert_move_to (iter, "1", TRUE);
	g_object_unref (iter);
	g_assert_null (weak_message);
}

static void
test_preconditions (void)
{
	GMimeMessage *message = iterator_message ();
	GMimeIterator *iter = g_mime_iterator_new (message);
	GError *error = NULL;

	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*GMIME_IS_MESSAGE (message)*");
	g_assert_null (g_mime_iterator_new (NULL));
	g_test_assert_expected_messages ();
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*GMIME_IS_ENTITY (entity)*");
	g_assert_null (g_mime_iterator_new_for_entity (NULL));
	g_test_assert_expected_messages ();
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*frames->len > 0*");
	g_assert_null (g_mime_iterator_get_current (iter));
	g_test_assert_expected_messages ();
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*frames->len > 0*");
	g_assert_null (g_mime_iterator_get_parent (iter));
	g_test_assert_expected_messages ();
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*frames->len > 0*");
	g_assert_null (g_mime_iterator_get_current_message (iter));
	g_test_assert_expected_messages ();
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*frames->len > 0*");
	g_assert_null (g_mime_iterator_get_path_specifier (iter));
	g_test_assert_expected_messages ();
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*frames->len > 0*");
	g_assert_cmpint (g_mime_iterator_get_depth (iter), ==, -1);
	g_test_assert_expected_messages ();
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*frames->len > 0*");
	g_assert_cmpint (g_mime_iterator_get_index (iter), ==, -1);
	g_test_assert_expected_messages ();
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*frames->len > 0*");
	g_assert_null (g_mime_iterator_get_ancestor (iter, 1));
	g_test_assert_expected_messages ();
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*frames->len > 0*");
	g_mime_iterator_skip_children (iter);
	g_test_assert_expected_messages ();
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*path_specifier != NULL*");
	g_assert_false (g_mime_iterator_move_to (iter, NULL, &error));
	g_test_assert_expected_messages ();
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*path_specifier != NULL*");
	g_assert_false (g_mime_iterator_move_to (iter, "", &error));
	g_test_assert_expected_messages ();
	g_assert_no_error (error);
	g_assert_true (g_mime_iterator_move_next (iter));
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*generation >= 1*");
	g_assert_null (g_mime_iterator_get_ancestor (iter, 0));
	g_test_assert_expected_messages ();
	g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*generation >= 1*");
	g_assert_null (g_mime_iterator_get_ancestor (iter, 1));
	g_test_assert_expected_messages ();
	g_object_unref (iter);
	g_object_unref (message);
}

static void
test_deep_traversal (void)
{
	const int depth = 10000;
	GPtrArray *nodes = g_ptr_array_new_with_free_func (g_object_unref);
	GMimeMessage *message = g_mime_message_new (FALSE);
	GMimeMultipart *root = g_mime_multipart_new (), *parent = root, *child;
	GMimeTextPart *leaf = g_mime_text_part_new ();
	GMimeIterator *iter;
	GString *path = g_string_new ("1");
	int i, count = 0;

	g_ptr_array_add (nodes, root);
	for (i = 1; i < depth; i++) {
		child = g_mime_multipart_new ();
		g_ptr_array_add (nodes, child);
		g_mime_multipart_add (parent, (GMimeEntity *) child);
		parent = child;
		g_string_append (path, ".1");
	}
	g_mime_multipart_add (parent, (GMimeEntity *) leaf);
	g_mime_message_set_mime_part (message, (GMimeEntity *) root);
	iter = g_mime_iterator_new (message);
	while (g_mime_iterator_move_next (iter))
		count++;
	g_assert_cmpint (count, ==, depth + 1);
	assert_move_to (iter, path->str, TRUE);
	g_assert_true (g_mime_iterator_get_current (iter) == (GMimeEntity *) leaf);
	g_assert_cmpint (g_mime_iterator_get_depth (iter), ==, depth);
	assert_path (iter, path->str);
	g_assert_true (g_mime_iterator_get_ancestor (iter, depth) == (GMimeEntity *) root);
	g_string_prepend (path, "0.");
	assert_move_to (iter, path->str, FALSE);
	g_object_unref (iter);
	/* Dismantle iteratively as well: entity finalization itself is still recursive. */
	for (i = 0; i < depth; i++)
		g_mime_multipart_clear (nodes->pdata[i]);
	g_object_unref (message);
	g_object_unref (leaf);
	g_ptr_array_unref (nodes);
	g_string_free (path, TRUE);
}

int
main (int argc, char **argv)
{
	g_mime_init ();
	testsuite_init (argc, argv);
	testsuite_start ("MIME iterator traversal and navigation");
	test_traversal ();
	test_navigation_and_skipping ();
	test_empty_single_and_headers ();
	test_lifetime_and_live_messages ();
	test_entity_subtree ();
	test_entity_root_kinds ();
	test_preconditions ();
	test_deep_traversal ();
	testsuite_end ();
	g_mime_shutdown ();
	return testsuite_exit ();
}
