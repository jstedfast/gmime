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

#ifndef __GMIME_MESSAGE_FEEDBACK_REPORT_H__
#define __GMIME_MESSAGE_FEEDBACK_REPORT_H__

#include <gmime/gmime-part.h>
#include <gmime/gmime-header.h>

G_BEGIN_DECLS

#define GMIME_TYPE_MESSAGE_FEEDBACK_REPORT            (g_mime_message_feedback_report_get_type ())
#define GMIME_MESSAGE_FEEDBACK_REPORT(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GMIME_TYPE_MESSAGE_FEEDBACK_REPORT, GMimeMessageFeedbackReport))
#define GMIME_MESSAGE_FEEDBACK_REPORT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GMIME_TYPE_MESSAGE_FEEDBACK_REPORT, GMimeMessageFeedbackReportClass))
#define GMIME_IS_MESSAGE_FEEDBACK_REPORT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GMIME_TYPE_MESSAGE_FEEDBACK_REPORT))
#define GMIME_IS_MESSAGE_FEEDBACK_REPORT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GMIME_TYPE_MESSAGE_FEEDBACK_REPORT))
#define GMIME_MESSAGE_FEEDBACK_REPORT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GMIME_TYPE_MESSAGE_FEEDBACK_REPORT, GMimeMessageFeedbackReportClass))

typedef struct _GMimeMessageFeedbackReport GMimeMessageFeedbackReport;
typedef struct _GMimeMessageFeedbackReportClass GMimeMessageFeedbackReportClass;

/**
 * GMimeMessageFeedbackReport:
 * @parent_object: parent #GMimePart
 *
 * A message/feedback-report part with lazily parsed fields.
 **/
struct _GMimeMessageFeedbackReport {
	GMimePart parent_object;

	/* < private > */
	GMimeHeaderList *fields;
};

struct _GMimeMessageFeedbackReportClass {
	GMimePartClass parent_class;
};

GType g_mime_message_feedback_report_get_type (void);

GMimeMessageFeedbackReport *g_mime_message_feedback_report_new (void);
GMimeHeaderList *g_mime_message_feedback_report_get_fields (GMimeMessageFeedbackReport *report, GError **error);

G_END_DECLS

#endif /* __GMIME_MESSAGE_FEEDBACK_REPORT_H__ */
