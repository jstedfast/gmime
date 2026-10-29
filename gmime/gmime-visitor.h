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

#ifndef __GMIME_VISITOR_H__
#define __GMIME_VISITOR_H__

#include <gmime/gmime-message.h>
#include <gmime/gmime-message-part.h>
#include <gmime/gmime-message-partial.h>
#include <gmime/gmime-message-delivery-status.h>
#include <gmime/gmime-message-disposition-notification.h>
#include <gmime/gmime-message-feedback-report.h>
#include <gmime/gmime-multipart-alternative.h>
#include <gmime/gmime-multipart-related.h>
#include <gmime/gmime-multipart-report.h>
#include <gmime/gmime-text-part.h>
#include <gmime/gmime-text-rfc822-headers.h>

G_BEGIN_DECLS

#define GMIME_TYPE_VISITOR            (g_mime_visitor_get_type ())


/**
 * GMimeVisitor:
 *
 * An abstract visitor for MIME trees. Derive from this type to examine,
 * transform or copy a tree, overriding the relevant visit methods.
 **/
G_DECLARE_DERIVABLE_TYPE (GMimeVisitor, g_mime_visitor, GMIME, VISITOR, GObject)

/**
 * GMimeVisitorClass:
 * @parent_class: parent #GObjectClass
 * @visit: dispatch a nullable entity through g_mime_entity_accept()
 * @visit_entity: visit an entity without traversing it
 * @visit_part: visit a leaf part
 * @visit_message: visit a message's body
 * @visit_message_part: visit an encapsulated-message entity and its message
 * @visit_message_partial: visit a message/partial part
 * @visit_message_delivery_status: visit a delivery-status part
 * @visit_message_disposition_notification: visit a disposition notification
 * @visit_message_feedback_report: visit a feedback report
 * @visit_multipart: visit a multipart entity and its children
 * @visit_multipart_alternative: visit a multipart/alternative
 * @visit_multipart_related: visit a multipart/related
 * @visit_multipart_report: visit a multipart/report
 * @visit_text_part: visit a text part
 * @visit_text_rfc822_headers: visit a text/rfc822-headers entity
 * @visit_body: traverse a message's body
 * @visit_children: traverse a multipart's children in order
 * @visit_message_part_message: traverse an encapsulated message
 *
 * All methods have defaults. Chain to the parent implementation to retain
 * its traversal, or omit chaining to prune that subtree.
 **/
struct _GMimeVisitorClass {
	GObjectClass parent_class;

	void (* visit) (GMimeVisitor *visitor, GMimeEntity *entity);
	void (* visit_entity) (GMimeVisitor *visitor, GMimeEntity *entity);
	void (* visit_part) (GMimeVisitor *visitor, GMimePart *part);
	void (* visit_message) (GMimeVisitor *visitor, GMimeMessage *message);
	void (* visit_message_part) (GMimeVisitor *visitor, GMimeMessagePart *part);
	void (* visit_message_partial) (GMimeVisitor *visitor, GMimeMessagePartial *part);
	void (* visit_message_delivery_status) (GMimeVisitor *visitor, GMimeMessageDeliveryStatus *part);
	void (* visit_message_disposition_notification) (GMimeVisitor *visitor, GMimeMessageDispositionNotification *part);
	void (* visit_message_feedback_report) (GMimeVisitor *visitor, GMimeMessageFeedbackReport *part);
	void (* visit_multipart) (GMimeVisitor *visitor, GMimeMultipart *multipart);
	void (* visit_multipart_alternative) (GMimeVisitor *visitor, GMimeMultipartAlternative *multipart);
	void (* visit_multipart_related) (GMimeVisitor *visitor, GMimeMultipartRelated *multipart);
	void (* visit_multipart_report) (GMimeVisitor *visitor, GMimeMultipartReport *multipart);
	void (* visit_text_part) (GMimeVisitor *visitor, GMimeTextPart *part);
	void (* visit_text_rfc822_headers) (GMimeVisitor *visitor, GMimeTextRfc822Headers *part);

	void (* visit_body) (GMimeVisitor *visitor, GMimeMessage *message);
	void (* visit_children) (GMimeVisitor *visitor, GMimeMultipart *multipart);
	void (* visit_message_part_message) (GMimeVisitor *visitor, GMimeMessagePart *part);

	/*< private >*/
	gpointer reserved[8];
};


void g_mime_visitor_visit (GMimeVisitor *visitor, GMimeEntity *entity);
void g_mime_visitor_visit_entity (GMimeVisitor *visitor, GMimeEntity *entity);
void g_mime_visitor_visit_part (GMimeVisitor *visitor, GMimePart *part);
void g_mime_visitor_visit_message (GMimeVisitor *visitor, GMimeMessage *message);
void g_mime_visitor_visit_message_part (GMimeVisitor *visitor, GMimeMessagePart *part);
void g_mime_visitor_visit_message_partial (GMimeVisitor *visitor, GMimeMessagePartial *part);
void g_mime_visitor_visit_message_delivery_status (GMimeVisitor *visitor, GMimeMessageDeliveryStatus *part);
void g_mime_visitor_visit_message_disposition_notification (GMimeVisitor *visitor, GMimeMessageDispositionNotification *part);
void g_mime_visitor_visit_message_feedback_report (GMimeVisitor *visitor, GMimeMessageFeedbackReport *part);
void g_mime_visitor_visit_multipart (GMimeVisitor *visitor, GMimeMultipart *multipart);
void g_mime_visitor_visit_multipart_alternative (GMimeVisitor *visitor, GMimeMultipartAlternative *multipart);
void g_mime_visitor_visit_multipart_related (GMimeVisitor *visitor, GMimeMultipartRelated *multipart);
void g_mime_visitor_visit_multipart_report (GMimeVisitor *visitor, GMimeMultipartReport *multipart);
void g_mime_visitor_visit_text_part (GMimeVisitor *visitor, GMimeTextPart *part);
void g_mime_visitor_visit_text_rfc822_headers (GMimeVisitor *visitor, GMimeTextRfc822Headers *part);
void g_mime_visitor_visit_body (GMimeVisitor *visitor, GMimeMessage *message);
void g_mime_visitor_visit_children (GMimeVisitor *visitor, GMimeMultipart *multipart);
void g_mime_visitor_visit_message_part_message (GMimeVisitor *visitor, GMimeMessagePart *part);

G_END_DECLS

#endif /* __GMIME_VISITOR_H__ */
