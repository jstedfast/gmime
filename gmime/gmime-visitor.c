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

#include "gmime-visitor.h"

/**
 * SECTION: gmime-visitor
 * @title: GMimeVisitor
 * @short_description: Extensible MIME tree visitors
 * @see_also: #GMimeEntity, #GMimeMessage
 *
 * An abstract GObject visitor modeled on MimeKit's MimeVisitor. Start with
 * g_mime_visitor_visit(), or dispatch directly with g_mime_entity_accept().
 * Concrete entities select their most specific visit method. Default methods
 * forward to the appropriate base-type method and traverse multipart children,
 * message bodies and encapsulated messages in depth-first order.
 *
 * Override individual visit methods to inspect or transform entities. Chain to
 * the parent implementation to keep its traversal; not chaining prunes that
 * subtree. Traversal uses accept directly, not the top-level visit method.
 * Entities are borrowed throughout, and leaf content is not opened or decoded.
 *
 * Unlike MimeKit, #GMimeMessage derives from #GMimeEntity, so one visit entry
 * point and one inherited accept method handle both messages and MIME parts.
 * Native cryptographic subclasses inherit their ordinary part or multipart
 * dispatch; this visitor does not decrypt or verify content.
 **/

G_DEFINE_ABSTRACT_TYPE (GMimeVisitor, g_mime_visitor, G_TYPE_OBJECT)

static void
visitor_visit (GMimeVisitor *visitor, GMimeEntity *entity)
{
	if (entity)
		g_mime_entity_accept (entity, visitor);
}

static void
visitor_visit_entity (GMimeVisitor *visitor, GMimeEntity *entity)
{
}

static void
visitor_visit_part (GMimeVisitor *visitor, GMimePart *part)
{
	g_mime_visitor_visit_entity (visitor, (GMimeEntity *) part);
}

static void
visitor_visit_body (GMimeVisitor *visitor, GMimeMessage *message)
{
	GMimeEntity *body = g_mime_message_get_mime_part (message);

	if (body)
		g_mime_entity_accept (body, visitor);
}

static void
visitor_visit_message (GMimeVisitor *visitor, GMimeMessage *message)
{
	g_mime_visitor_visit_body (visitor, message);
}

static void
visitor_visit_message_part_message (GMimeVisitor *visitor, GMimeMessagePart *part)
{
	GMimeMessage *message = g_mime_message_part_get_message (part);

	if (message)
		g_mime_entity_accept ((GMimeEntity *) message, visitor);
}

static void
visitor_visit_message_part (GMimeVisitor *visitor, GMimeMessagePart *part)
{
	g_mime_visitor_visit_entity (visitor, (GMimeEntity *) part);
	g_mime_visitor_visit_message_part_message (visitor, part);
}

static void
visitor_visit_message_partial (GMimeVisitor *visitor, GMimeMessagePartial *part)
{
	g_mime_visitor_visit_part (visitor, (GMimePart *) part);
}

static void
visitor_visit_message_delivery_status (GMimeVisitor *visitor, GMimeMessageDeliveryStatus *part)
{
	g_mime_visitor_visit_part (visitor, (GMimePart *) part);
}

static void
visitor_visit_message_disposition_notification (GMimeVisitor *visitor, GMimeMessageDispositionNotification *part)
{
	g_mime_visitor_visit_part (visitor, (GMimePart *) part);
}

static void
visitor_visit_message_feedback_report (GMimeVisitor *visitor, GMimeMessageFeedbackReport *part)
{
	g_mime_visitor_visit_part (visitor, (GMimePart *) part);
}

static void
visitor_visit_children (GMimeVisitor *visitor, GMimeMultipart *multipart)
{
	int i;

	for (i = 0; i < g_mime_multipart_get_count (multipart); i++)
		g_mime_entity_accept (g_mime_multipart_get_part (multipart, i), visitor);
}

static void
visitor_visit_multipart (GMimeVisitor *visitor, GMimeMultipart *multipart)
{
	g_mime_visitor_visit_entity (visitor, (GMimeEntity *) multipart);
	g_mime_visitor_visit_children (visitor, multipart);
}

static void
visitor_visit_multipart_alternative (GMimeVisitor *visitor, GMimeMultipartAlternative *multipart)
{
	g_mime_visitor_visit_multipart (visitor, (GMimeMultipart *) multipart);
}

static void
visitor_visit_multipart_related (GMimeVisitor *visitor, GMimeMultipartRelated *multipart)
{
	g_mime_visitor_visit_multipart (visitor, (GMimeMultipart *) multipart);
}

static void
visitor_visit_multipart_report (GMimeVisitor *visitor, GMimeMultipartReport *multipart)
{
	g_mime_visitor_visit_multipart (visitor, (GMimeMultipart *) multipart);
}

static void
visitor_visit_text_part (GMimeVisitor *visitor, GMimeTextPart *part)
{
	g_mime_visitor_visit_part (visitor, (GMimePart *) part);
}

static void
visitor_visit_text_rfc822_headers (GMimeVisitor *visitor, GMimeTextRfc822Headers *part)
{
	g_mime_visitor_visit_message_part (visitor, (GMimeMessagePart *) part);
}

static void
g_mime_visitor_class_init (GMimeVisitorClass *klass)
{
	klass->visit = visitor_visit;
	klass->visit_entity = visitor_visit_entity;
	klass->visit_part = visitor_visit_part;
	klass->visit_message = visitor_visit_message;
	klass->visit_message_part = visitor_visit_message_part;
	klass->visit_message_partial = visitor_visit_message_partial;
	klass->visit_message_delivery_status = visitor_visit_message_delivery_status;
	klass->visit_message_disposition_notification = visitor_visit_message_disposition_notification;
	klass->visit_message_feedback_report = visitor_visit_message_feedback_report;
	klass->visit_multipart = visitor_visit_multipart;
	klass->visit_multipart_alternative = visitor_visit_multipart_alternative;
	klass->visit_multipart_related = visitor_visit_multipart_related;
	klass->visit_multipart_report = visitor_visit_multipart_report;
	klass->visit_text_part = visitor_visit_text_part;
	klass->visit_text_rfc822_headers = visitor_visit_text_rfc822_headers;
	klass->visit_body = visitor_visit_body;
	klass->visit_children = visitor_visit_children;
	klass->visit_message_part_message = visitor_visit_message_part_message;
}

static void
g_mime_visitor_init (GMimeVisitor *visitor)
{
}

/**
 * g_mime_visitor_visit:
 * @visitor: a #GMimeVisitor
 * @entity: (nullable): a #GMimeEntity, including a #GMimeMessage, or %NULL
 *
 * Dispatches @entity through its virtual accept method. The default does
 * nothing for %NULL. Recursive traversal bypasses this entry point.
 **/
void
g_mime_visitor_visit (GMimeVisitor *visitor, GMimeEntity *entity)
{
	g_return_if_fail (GMIME_IS_VISITOR (visitor));
	g_return_if_fail (entity == NULL || GMIME_IS_ENTITY (entity));

	GMIME_VISITOR_GET_CLASS (visitor)->visit (visitor, entity);
}

/**
 * g_mime_visitor_visit_entity:
 * @visitor: a #GMimeVisitor
 * @entity: a #GMimeEntity
 *
 * Visits an entity. The default does nothing.
 **/
void
g_mime_visitor_visit_entity (GMimeVisitor *visitor, GMimeEntity *entity)
{
	g_return_if_fail (GMIME_IS_VISITOR (visitor));
	g_return_if_fail (GMIME_IS_ENTITY (entity));

	GMIME_VISITOR_GET_CLASS (visitor)->visit_entity (visitor, entity);
}

/**
 * g_mime_visitor_visit_part:
 * @visitor: a #GMimeVisitor
 * @part: a #GMimePart
 *
 * Visits a leaf part. The default calls g_mime_visitor_visit_entity().
 **/
void
g_mime_visitor_visit_part (GMimeVisitor *visitor, GMimePart *part)
{
	g_return_if_fail (GMIME_IS_VISITOR (visitor));
	g_return_if_fail (GMIME_IS_PART (part));

	GMIME_VISITOR_GET_CLASS (visitor)->visit_part (visitor, part);
}

/**
 * g_mime_visitor_visit_message:
 * @visitor: a #GMimeVisitor
 * @message: a #GMimeMessage
 *
 * Visits a message. The default calls g_mime_visitor_visit_body(), not
 * g_mime_visitor_visit_entity().
 **/
void
g_mime_visitor_visit_message (GMimeVisitor *visitor, GMimeMessage *message)
{
	g_return_if_fail (GMIME_IS_VISITOR (visitor));
	g_return_if_fail (GMIME_IS_MESSAGE (message));

	GMIME_VISITOR_GET_CLASS (visitor)->visit_message (visitor, message);
}

/**
 * g_mime_visitor_visit_message_part:
 * @visitor: a #GMimeVisitor
 * @part: a #GMimeMessagePart
 *
 * Calls g_mime_visitor_visit_entity(), then
 * g_mime_visitor_visit_message_part_message() by default.
 **/
void
g_mime_visitor_visit_message_part (GMimeVisitor *visitor, GMimeMessagePart *part)
{
	g_return_if_fail (GMIME_IS_VISITOR (visitor));
	g_return_if_fail (GMIME_IS_MESSAGE_PART (part));

	GMIME_VISITOR_GET_CLASS (visitor)->visit_message_part (visitor, part);
}

/**
 * g_mime_visitor_visit_message_partial:
 * @visitor: a #GMimeVisitor
 * @part: a #GMimeMessagePartial
 *
 * Visits a message/partial. The default calls g_mime_visitor_visit_part().
 **/
void
g_mime_visitor_visit_message_partial (GMimeVisitor *visitor, GMimeMessagePartial *part)
{
	g_return_if_fail (GMIME_IS_VISITOR (visitor));
	g_return_if_fail (GMIME_IS_MESSAGE_PARTIAL (part));

	GMIME_VISITOR_GET_CLASS (visitor)->visit_message_partial (visitor, part);
}

/**
 * g_mime_visitor_visit_message_delivery_status:
 * @visitor: a #GMimeVisitor
 * @part: a #GMimeMessageDeliveryStatus
 *
 * Visits delivery status. The default calls g_mime_visitor_visit_part().
 **/
void
g_mime_visitor_visit_message_delivery_status (GMimeVisitor *visitor, GMimeMessageDeliveryStatus *part)
{
	g_return_if_fail (GMIME_IS_VISITOR (visitor));
	g_return_if_fail (GMIME_IS_MESSAGE_DELIVERY_STATUS (part));

	GMIME_VISITOR_GET_CLASS (visitor)->visit_message_delivery_status (visitor, part);
}

/**
 * g_mime_visitor_visit_message_disposition_notification:
 * @visitor: a #GMimeVisitor
 * @part: a #GMimeMessageDispositionNotification
 *
 * Visits a disposition notification. The default calls g_mime_visitor_visit_part().
 **/
void
g_mime_visitor_visit_message_disposition_notification (GMimeVisitor *visitor, GMimeMessageDispositionNotification *part)
{
	g_return_if_fail (GMIME_IS_VISITOR (visitor));
	g_return_if_fail (GMIME_IS_MESSAGE_DISPOSITION_NOTIFICATION (part));

	GMIME_VISITOR_GET_CLASS (visitor)->visit_message_disposition_notification (visitor, part);
}

/**
 * g_mime_visitor_visit_message_feedback_report:
 * @visitor: a #GMimeVisitor
 * @part: a #GMimeMessageFeedbackReport
 *
 * Visits a feedback report. The default calls g_mime_visitor_visit_part().
 **/
void
g_mime_visitor_visit_message_feedback_report (GMimeVisitor *visitor, GMimeMessageFeedbackReport *part)
{
	g_return_if_fail (GMIME_IS_VISITOR (visitor));
	g_return_if_fail (GMIME_IS_MESSAGE_FEEDBACK_REPORT (part));

	GMIME_VISITOR_GET_CLASS (visitor)->visit_message_feedback_report (visitor, part);
}

/**
 * g_mime_visitor_visit_multipart:
 * @visitor: a #GMimeVisitor
 * @multipart: a #GMimeMultipart
 *
 * Calls g_mime_visitor_visit_entity(), then g_mime_visitor_visit_children()
 * by default.
 **/
void
g_mime_visitor_visit_multipart (GMimeVisitor *visitor, GMimeMultipart *multipart)
{
	g_return_if_fail (GMIME_IS_VISITOR (visitor));
	g_return_if_fail (GMIME_IS_MULTIPART (multipart));

	GMIME_VISITOR_GET_CLASS (visitor)->visit_multipart (visitor, multipart);
}

/**
 * g_mime_visitor_visit_multipart_alternative:
 * @visitor: a #GMimeVisitor
 * @multipart: a #GMimeMultipartAlternative
 *
 * Visits alternatives. The default calls g_mime_visitor_visit_multipart().
 **/
void
g_mime_visitor_visit_multipart_alternative (GMimeVisitor *visitor, GMimeMultipartAlternative *multipart)
{
	g_return_if_fail (GMIME_IS_VISITOR (visitor));
	g_return_if_fail (GMIME_IS_MULTIPART_ALTERNATIVE (multipart));

	GMIME_VISITOR_GET_CLASS (visitor)->visit_multipart_alternative (visitor, multipart);
}

/**
 * g_mime_visitor_visit_multipart_related:
 * @visitor: a #GMimeVisitor
 * @multipart: a #GMimeMultipartRelated
 *
 * Visits related parts. The default calls g_mime_visitor_visit_multipart().
 **/
void
g_mime_visitor_visit_multipart_related (GMimeVisitor *visitor, GMimeMultipartRelated *multipart)
{
	g_return_if_fail (GMIME_IS_VISITOR (visitor));
	g_return_if_fail (GMIME_IS_MULTIPART_RELATED (multipart));

	GMIME_VISITOR_GET_CLASS (visitor)->visit_multipart_related (visitor, multipart);
}

/**
 * g_mime_visitor_visit_multipart_report:
 * @visitor: a #GMimeVisitor
 * @multipart: a #GMimeMultipartReport
 *
 * Visits a report. The default calls g_mime_visitor_visit_multipart().
 **/
void
g_mime_visitor_visit_multipart_report (GMimeVisitor *visitor, GMimeMultipartReport *multipart)
{
	g_return_if_fail (GMIME_IS_VISITOR (visitor));
	g_return_if_fail (GMIME_IS_MULTIPART_REPORT (multipart));

	GMIME_VISITOR_GET_CLASS (visitor)->visit_multipart_report (visitor, multipart);
}

/**
 * g_mime_visitor_visit_text_part:
 * @visitor: a #GMimeVisitor
 * @part: a #GMimeTextPart
 *
 * Visits text. The default calls g_mime_visitor_visit_part().
 **/
void
g_mime_visitor_visit_text_part (GMimeVisitor *visitor, GMimeTextPart *part)
{
	g_return_if_fail (GMIME_IS_VISITOR (visitor));
	g_return_if_fail (GMIME_IS_TEXT_PART (part));

	GMIME_VISITOR_GET_CLASS (visitor)->visit_text_part (visitor, part);
}

/**
 * g_mime_visitor_visit_text_rfc822_headers:
 * @visitor: a #GMimeVisitor
 * @part: a #GMimeTextRfc822Headers
 *
 * Visits encapsulated headers. The default calls g_mime_visitor_visit_message_part().
 **/
void
g_mime_visitor_visit_text_rfc822_headers (GMimeVisitor *visitor, GMimeTextRfc822Headers *part)
{
	g_return_if_fail (GMIME_IS_VISITOR (visitor));
	g_return_if_fail (GMIME_IS_TEXT_RFC822_HEADERS (part));

	GMIME_VISITOR_GET_CLASS (visitor)->visit_text_rfc822_headers (visitor, part);
}

/**
 * g_mime_visitor_visit_body:
 * @visitor: a #GMimeVisitor
 * @message: a #GMimeMessage
 *
 * Dispatches the message body through accept if present.
 **/
void
g_mime_visitor_visit_body (GMimeVisitor *visitor, GMimeMessage *message)
{
	g_return_if_fail (GMIME_IS_VISITOR (visitor));
	g_return_if_fail (GMIME_IS_MESSAGE (message));

	GMIME_VISITOR_GET_CLASS (visitor)->visit_body (visitor, message);
}

/**
 * g_mime_visitor_visit_children:
 * @visitor: a #GMimeVisitor
 * @multipart: a #GMimeMultipart
 *
 * Dispatches each current child through accept in index order. The child
 * count is rechecked after every visit, matching MimeKit's live traversal.
 **/
void
g_mime_visitor_visit_children (GMimeVisitor *visitor, GMimeMultipart *multipart)
{
	g_return_if_fail (GMIME_IS_VISITOR (visitor));
	g_return_if_fail (GMIME_IS_MULTIPART (multipart));

	GMIME_VISITOR_GET_CLASS (visitor)->visit_children (visitor, multipart);
}

/**
 * g_mime_visitor_visit_message_part_message:
 * @visitor: a #GMimeVisitor
 * @part: a #GMimeMessagePart
 *
 * Dispatches an encapsulated message through accept if present. This is
 * MimeKit's VisitMessage(MessagePart) hook.
 **/
void
g_mime_visitor_visit_message_part_message (GMimeVisitor *visitor, GMimeMessagePart *part)
{
	g_return_if_fail (GMIME_IS_VISITOR (visitor));
	g_return_if_fail (GMIME_IS_MESSAGE_PART (part));

	GMIME_VISITOR_GET_CLASS (visitor)->visit_message_part_message (visitor, part);
}
