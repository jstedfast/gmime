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

typedef struct {
	GMimeVisitor parent_object;
	GString *trace;
	int mode;
	GMimeMultipart *root;
} TestVisitor;

typedef GMimeVisitorClass TestVisitorClass;

G_DEFINE_TYPE (TestVisitor, test_visitor, GMIME_TYPE_VISITOR)

static void
record_visit (GMimeVisitor *visitor, const char *name, GMimeEntity *entity)
{
	TestVisitor *test = (TestVisitor *) visitor;
	const char *id = entity ? g_mime_entity_get_header (entity, "X-Id") : NULL;

	g_string_append_printf (test->trace, "%s:%s;", name, id ? id : "-");
}

static void
test_visit (GMimeVisitor *visitor, GMimeEntity *entity)
{
	record_visit (visitor, "visit", entity);
	if (((TestVisitor *) visitor)->mode != 6)
		GMIME_VISITOR_CLASS (test_visitor_parent_class)->visit (visitor, entity);
}

static void
test_visit_entity (GMimeVisitor *visitor, GMimeEntity *entity)
{
	record_visit (visitor, "entity", entity);
	GMIME_VISITOR_CLASS (test_visitor_parent_class)->visit_entity (visitor, entity);
}

static void
test_visit_part (GMimeVisitor *visitor, GMimePart *part)
{
	record_visit (visitor, "part", (GMimeEntity *) part);
	GMIME_VISITOR_CLASS (test_visitor_parent_class)->visit_part (visitor, part);
}

static void
test_visit_multipart (GMimeVisitor *visitor, GMimeMultipart *multipart)
{
	record_visit (visitor, "multipart", (GMimeEntity *) multipart);
	if (((TestVisitor *) visitor)->mode != 1)
		GMIME_VISITOR_CLASS (test_visitor_parent_class)->visit_multipart (visitor, multipart);
}

static void
test_visit_children (GMimeVisitor *visitor, GMimeMultipart *multipart)
{
	record_visit (visitor, "children", (GMimeEntity *) multipart);
	if (((TestVisitor *) visitor)->mode != 2)
		GMIME_VISITOR_CLASS (test_visitor_parent_class)->visit_children (visitor, multipart);
}

static void
test_visit_body (GMimeVisitor *visitor, GMimeMessage *message)
{
	record_visit (visitor, "body", (GMimeEntity *) message);
	if (((TestVisitor *) visitor)->mode != 3)
		GMIME_VISITOR_CLASS (test_visitor_parent_class)->visit_body (visitor, message);
}

static void
test_visit_message_part_message (GMimeVisitor *visitor, GMimeMessagePart *part)
{
	record_visit (visitor, "embedded", (GMimeEntity *) part);
	if (((TestVisitor *) visitor)->mode != 4)
		GMIME_VISITOR_CLASS (test_visitor_parent_class)->visit_message_part_message (visitor, part);
}

static void
test_visit_multipart_report (GMimeVisitor *visitor, GMimeMultipartReport *multipart)
{
	record_visit (visitor, "report", (GMimeEntity *) multipart);
	if (((TestVisitor *) visitor)->mode != 5)
		GMIME_VISITOR_CLASS (test_visitor_parent_class)->visit_multipart_report (visitor, multipart);
}

static void
test_visit_text_part (GMimeVisitor *visitor, GMimeTextPart *part)
{
	TestVisitor *test = (TestVisitor *) visitor;
	const char *id = g_mime_entity_get_header ((GMimeEntity *) part, "X-Id");
	GMimeEntity *added, *removed;

	record_visit (visitor, "text", (GMimeEntity *) part);
	if (test->root && !g_strcmp0 (id, "text")) {
		switch (test->mode) {
		case 7:
			added = (GMimeEntity *) g_mime_part_new ();
			g_mime_entity_set_header (added, "X-Id", "added", NULL);
			g_mime_multipart_add (test->root, added);
			g_object_unref (added);
			break;
		case 8:
			removed = g_mime_multipart_remove_at (test->root, 0);
			g_object_unref (removed);
			break;
		case 9:
			g_mime_multipart_clear (test->root);
			break;
		}
	}
	GMIME_VISITOR_CLASS (test_visitor_parent_class)->visit_text_part (visitor, part);
}

#define TEST_VISIT(name, Type, label) \
static void test_visit_##name (GMimeVisitor *visitor, Type *entity) \
{ \
	record_visit (visitor, label, (GMimeEntity *) entity); \
	GMIME_VISITOR_CLASS (test_visitor_parent_class)->visit_##name (visitor, entity); \
}

TEST_VISIT (message, GMimeMessage, "message")
TEST_VISIT (message_part, GMimeMessagePart, "message-part")
TEST_VISIT (message_partial, GMimeMessagePartial, "partial")
TEST_VISIT (message_delivery_status, GMimeMessageDeliveryStatus, "delivery")
TEST_VISIT (message_disposition_notification, GMimeMessageDispositionNotification, "notification")
TEST_VISIT (message_feedback_report, GMimeMessageFeedbackReport, "feedback")
TEST_VISIT (multipart_alternative, GMimeMultipartAlternative, "alternative")
TEST_VISIT (multipart_related, GMimeMultipartRelated, "related")
TEST_VISIT (text_rfc822_headers, GMimeTextRfc822Headers, "headers")

#undef TEST_VISIT

static void
test_visitor_finalize (GObject *object)
{
	g_string_free (((TestVisitor *) object)->trace, TRUE);
	G_OBJECT_CLASS (test_visitor_parent_class)->finalize (object);
}

static void
test_visitor_class_init (TestVisitorClass *klass)
{
	G_OBJECT_CLASS (klass)->finalize = test_visitor_finalize;
	klass->visit = test_visit;
	klass->visit_entity = test_visit_entity;
	klass->visit_part = test_visit_part;
	klass->visit_message = test_visit_message;
	klass->visit_message_part = test_visit_message_part;
	klass->visit_message_partial = test_visit_message_partial;
	klass->visit_message_delivery_status = test_visit_message_delivery_status;
	klass->visit_message_disposition_notification = test_visit_message_disposition_notification;
	klass->visit_message_feedback_report = test_visit_message_feedback_report;
	klass->visit_multipart = test_visit_multipart;
	klass->visit_multipart_alternative = test_visit_multipart_alternative;
	klass->visit_multipart_related = test_visit_multipart_related;
	klass->visit_multipart_report = test_visit_multipart_report;
	klass->visit_text_part = test_visit_text_part;
	klass->visit_text_rfc822_headers = test_visit_text_rfc822_headers;
	klass->visit_body = test_visit_body;
	klass->visit_children = test_visit_children;
	klass->visit_message_part_message = test_visit_message_part_message;
}

static void
test_visitor_init (TestVisitor *visitor)
{
	visitor->trace = g_string_new (NULL);
}

typedef GMimeEntity TestVisitorEntity;
typedef GMimeEntityClass TestVisitorEntityClass;

G_DEFINE_TYPE (TestVisitorEntity, test_visitor_entity, GMIME_TYPE_ENTITY)

static void test_visitor_entity_class_init (TestVisitorEntityClass *klass) {}
static void test_visitor_entity_init (TestVisitorEntity *entity) {}

typedef GMimeTextPart TestVisitorText;
typedef GMimeTextPartClass TestVisitorTextClass;

G_DEFINE_TYPE (TestVisitorText, test_visitor_text, GMIME_TYPE_TEXT_PART)

static void
test_visitor_text_accept (GMimeEntity *entity, GMimeVisitor *visitor)
{
	g_mime_visitor_visit_part (visitor, (GMimePart *) entity);
}

static void
test_visitor_text_class_init (TestVisitorTextClass *klass)
{
	GMIME_ENTITY_CLASS (klass)->accept = test_visitor_text_accept;
}

static void test_visitor_text_init (TestVisitorText *part) {}

static void
visitor_add (GMimeMultipart *parent, GMimeEntity *entity, const char *id)
{
	g_mime_entity_set_header (entity, "X-Id", id, NULL);
	g_mime_multipart_add (parent, entity);
	g_object_unref (entity);
}

static GMimeMessage *
visitor_message (const char *id, GMimeEntity *body)
{
	GMimeMessage *message = g_mime_message_new (FALSE);

	g_mime_entity_set_header ((GMimeEntity *) message, "X-Id", id, NULL);
	g_mime_message_set_mime_part (message, body);
	g_object_unref (body);
	return message;
}

static char *
visitor_trace (int mode, gboolean direct)
{
	TestVisitor *visitor = g_object_new (test_visitor_get_type (), NULL);
	GMimeMultipart *root = g_mime_multipart_new ();
	GMimeMultipart *alternative = (GMimeMultipart *) g_mime_multipart_alternative_new ();
	GMimeMultipart *related = (GMimeMultipart *) g_mime_multipart_related_new ();
	GMimeMultipart *report = (GMimeMultipart *) g_mime_multipart_report_new ("feedback-report");
	GMimeMessagePart *part = g_mime_message_part_new ("rfc822");
	GMimeMessagePart *headers = (GMimeMessagePart *) g_mime_text_rfc822_headers_new ();
	GMimeMessage *message;
	GMimeEntity *body;
	char *trace;

	visitor->mode = mode;
	visitor->root = root;
	g_mime_entity_set_header ((GMimeEntity *) root, "X-Id", "root", NULL);
	visitor_add (root, (GMimeEntity *) g_mime_text_part_new (), "text");
	visitor_add (alternative, (GMimeEntity *) g_mime_text_part_new (), "plain");
	visitor_add (alternative, (GMimeEntity *) g_mime_text_part_new_with_subtype ("html"), "html");
	visitor_add (root, (GMimeEntity *) alternative, "alternative");
	visitor_add (related, (GMimeEntity *) g_mime_text_part_new (), "related-text");
	visitor_add (related, (GMimeEntity *) g_mime_part_new_with_type ("image", "png"), "image");
	visitor_add (root, (GMimeEntity *) related, "related");
	visitor_add (report, (GMimeEntity *) g_mime_message_delivery_status_new (), "delivery");
	visitor_add (report, (GMimeEntity *) g_mime_message_disposition_notification_new (), "notification");
	visitor_add (report, (GMimeEntity *) g_mime_message_feedback_report_new (), "feedback");
	visitor_add (root, (GMimeEntity *) report, "report");
	body = (GMimeEntity *) g_mime_text_part_new ();
	g_mime_entity_set_header (body, "X-Id", "nested-text", NULL);
	message = visitor_message ("nested", body);
	g_mime_message_part_set_message (part, message);
	g_object_unref (message);
	visitor_add (root, (GMimeEntity *) part, "encapsulated");
	body = (GMimeEntity *) g_mime_part_new ();
	g_mime_entity_set_header (body, "X-Id", "headers-body", NULL);
	message = visitor_message ("headers-message", body);
	g_mime_message_part_set_message (headers, message);
	g_object_unref (message);
	visitor_add (root, (GMimeEntity *) headers, "headers");
	visitor_add (root, (GMimeEntity *) g_mime_message_partial_new ("id", 1, 2), "partial");
	message = visitor_message ("message", (GMimeEntity *) root);
	if (direct)
		g_mime_entity_accept ((GMimeEntity *) message, (GMimeVisitor *) visitor);
	else
		g_mime_visitor_visit ((GMimeVisitor *) visitor, (GMimeEntity *) message);
	trace = g_strdup (visitor->trace->str);
	g_object_unref (message);
	g_object_unref (visitor);
	return trace;
}

static void
test_traversal (void)
{
	const char *expected =
		"visit:message;message:message;body:message;multipart:root;entity:root;children:root;"
		"text:text;part:text;entity:text;"
		"alternative:alternative;multipart:alternative;entity:alternative;children:alternative;"
		"text:plain;part:plain;entity:plain;text:html;part:html;entity:html;"
		"related:related;multipart:related;entity:related;children:related;"
		"text:related-text;part:related-text;entity:related-text;part:image;entity:image;"
		"report:report;multipart:report;entity:report;children:report;"
		"delivery:delivery;part:delivery;entity:delivery;"
		"notification:notification;part:notification;entity:notification;"
		"feedback:feedback;part:feedback;entity:feedback;"
		"message-part:encapsulated;entity:encapsulated;embedded:encapsulated;"
		"message:nested;body:nested;text:nested-text;part:nested-text;entity:nested-text;"
		"headers:headers;message-part:headers;entity:headers;embedded:headers;"
		"message:headers-message;body:headers-message;part:headers-body;entity:headers-body;"
		"partial:partial;part:partial;entity:partial;";
	char *trace, *direct;
	int mode;

	g_assert_true (G_TYPE_IS_ABSTRACT (GMIME_TYPE_VISITOR));
	trace = visitor_trace (0, FALSE);
	g_assert_cmpstr (trace, ==, expected);
	direct = visitor_trace (0, TRUE);
	g_assert_cmpstr (direct, ==, expected + strlen ("visit:message;"));
	g_free (direct);
	g_free (trace);
	for (mode = 1; mode <= 9; mode++) {
		trace = visitor_trace (mode, FALSE);
		switch (mode) {
		case 1:
			g_assert_cmpstr (trace, ==, "visit:message;message:message;body:message;multipart:root;");
			break;
		case 2:
			g_assert_cmpstr (trace, ==, "visit:message;message:message;body:message;multipart:root;entity:root;children:root;");
			break;
		case 3:
			g_assert_cmpstr (trace, ==, "visit:message;message:message;body:message;");
			break;
		case 4:
			g_assert_null (strstr (trace, "message:nested;"));
			g_assert_null (strstr (trace, "message:headers-message;"));
			g_assert_nonnull (strstr (trace, "partial:partial;"));
			break;
		case 5:
			g_assert_nonnull (strstr (trace, "report:report;message-part:encapsulated;"));
			g_assert_null (strstr (trace, "feedback:feedback;"));
			break;
		case 6:
			g_assert_cmpstr (trace, ==, "visit:message;");
			break;
		case 7:
			g_assert_true (g_str_has_suffix (trace, "part:added;entity:added;"));
			break;
		case 8:
			g_assert_null (strstr (trace, "alternative:alternative;"));
			g_assert_nonnull (strstr (trace, "related:related;"));
			break;
		case 9:
			g_assert_cmpstr (trace, ==, "visit:message;message:message;body:message;multipart:root;"
					    "entity:root;children:root;text:text;part:text;entity:text;");
			break;
		}
		g_free (trace);
	}
}

static void
test_empty_and_inherited_dispatch (void)
{
	TestVisitor *visitor = g_object_new (test_visitor_get_type (), NULL);
	GMimeEntity *entities[] = {
		(GMimeEntity *) g_mime_message_new (FALSE),
		(GMimeEntity *) g_mime_message_part_new ("rfc822"),
		(GMimeEntity *) g_mime_text_rfc822_headers_new (),
		(GMimeEntity *) g_mime_multipart_new (),
		g_object_new (test_visitor_entity_get_type (), NULL),
		g_object_new (test_visitor_text_get_type (), NULL),
		(GMimeEntity *) g_mime_multipart_signed_new (),
		(GMimeEntity *) g_mime_multipart_encrypted_new (),
		(GMimeEntity *) g_mime_application_pkcs7_mime_new (GMIME_SECURE_MIME_TYPE_ENVELOPED_DATA)
	};
	const char *expected[] = {
		"message:-;body:-;",
		"message-part:-;entity:-;embedded:-;",
		"headers:-;message-part:-;entity:-;embedded:-;",
		"multipart:-;entity:-;children:-;",
		"entity:-;",
		"part:-;entity:-;",
		"multipart:-;entity:-;children:-;",
		"multipart:-;entity:-;children:-;",
		"part:-;entity:-;"
	};
	gpointer weak;
	guint i;

	g_mime_visitor_visit ((GMimeVisitor *) visitor, NULL);
	g_assert_cmpstr (visitor->trace->str, ==, "visit:-;");
	for (i = 0; i < G_N_ELEMENTS (entities); i++) {
		g_string_truncate (visitor->trace, 0);
		g_mime_entity_accept (entities[i], (GMimeVisitor *) visitor);
		g_assert_cmpstr (visitor->trace->str, ==, expected[i]);
		if (GMIME_IS_PART (entities[i]))
			g_assert_null (g_mime_part_get_content ((GMimePart *) entities[i]));
		weak = entities[i];
		g_object_add_weak_pointer (G_OBJECT (entities[i]), &weak);
		g_object_unref (entities[i]);
		g_assert_null (weak);
	}
	g_object_unref (visitor);
}

int
main (int argc, char **argv)
{
	g_mime_init ();
	testsuite_init (argc, argv);
	testsuite_start ("visitor traversal and dispatch");
	test_traversal ();
	test_empty_and_inherited_dispatch ();
	testsuite_end ();
	g_mime_shutdown ();
	return testsuite_exit ();
}
