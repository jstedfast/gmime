/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) Jeffrey Stedfast
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

#ifndef __GMIME_RTF_COMPLIANCE_PRIVATE_H__
#define __GMIME_RTF_COMPLIANCE_PRIVATE_H__

#include "gmime-rtf-compliance.h"

typedef struct {
	GMimeRtfComplianceLogger logger;
	gpointer user_data;
	guint maximum, counts[GMIME_RTF_COMPLIANCE_VIOLATION_GROUP_DEPTH_LIMIT_EXCEEDED + 1];
	gboolean suppressed;
} GMimeRtfDiagnostics;

G_GNUC_INTERNAL void _g_mime_rtf_diagnostics_init (GMimeRtfDiagnostics *diag, const GMimeRtfComplianceOptions *options,
						   GMimeRtfComplianceLogger logger, gpointer user_data);
G_GNUC_INTERNAL void _g_mime_rtf_report (GMimeRtfDiagnostics *diag, GMimeRtfComplianceViolation violation,
					guint64 offset, guint64 line, guint64 column, guint64 depth);

#endif
