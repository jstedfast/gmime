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

#ifndef __GMIME_HTML_ENTITY_PRIVATE_H__
#define __GMIME_HTML_ENTITY_PRIVATE_H__

#include "gmime-html-compliance.h"

typedef struct {
	char pushed[34];
	guint16 states[33];
	guint numeric_value;
	guint index, base;
	gboolean numeric, semicolon, digits;
	char value[140];
} HtmlEntity;

gboolean _g_mime_html_entity_push (HtmlEntity *entity, gunichar c);

const char *_g_mime_html_entity_value (HtmlEntity *entity, gboolean attribute, gunichar next);

void _g_mime_html_entity_violations (HtmlEntity *entity, gboolean attribute, gunichar next,
				     GMimeHtmlComplianceViolation *first, GMimeHtmlComplianceViolation *second);

int _g_mime_html_entity_unknown_length (HtmlEntity *entity);

void _g_mime_html_entity_reset (HtmlEntity *entity);

#endif
