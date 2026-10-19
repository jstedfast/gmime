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

#ifndef __GMIME_MULTIPART_REPORT_H__
#define __GMIME_MULTIPART_REPORT_H__

#include <gmime/gmime-multipart.h>

G_BEGIN_DECLS

#define GMIME_TYPE_MULTIPART_REPORT            (g_mime_multipart_report_get_type ())
#define GMIME_MULTIPART_REPORT(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GMIME_TYPE_MULTIPART_REPORT, GMimeMultipartReport))
#define GMIME_MULTIPART_REPORT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GMIME_TYPE_MULTIPART_REPORT, GMimeMultipartReportClass))
#define GMIME_IS_MULTIPART_REPORT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GMIME_TYPE_MULTIPART_REPORT))
#define GMIME_IS_MULTIPART_REPORT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GMIME_TYPE_MULTIPART_REPORT))
#define GMIME_MULTIPART_REPORT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GMIME_TYPE_MULTIPART_REPORT, GMimeMultipartReportClass))

typedef struct _GMimeMultipartReport GMimeMultipartReport;
typedef struct _GMimeMultipartReportClass GMimeMultipartReportClass;

/**
 * GMimeMultipartReport:
 * @parent_object: parent #GMimeMultipart
 *
 * A multipart/report container for mail reports.
 **/
struct _GMimeMultipartReport {
	GMimeMultipart parent_object;
};

struct _GMimeMultipartReportClass {
	GMimeMultipartClass parent_class;
};

GType g_mime_multipart_report_get_type (void);

GMimeMultipartReport *g_mime_multipart_report_new (const char *report_type);

const char *g_mime_multipart_report_get_report_type (GMimeMultipartReport *report);
void g_mime_multipart_report_set_report_type (GMimeMultipartReport *report, const char *report_type);

G_END_DECLS

#endif /* __GMIME_MULTIPART_REPORT_H__ */
