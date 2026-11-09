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

#ifndef __GMIME_HTML_TO_HTML_H__
#define __GMIME_HTML_TO_HTML_H__

#include <gmime/gmime-text-converter.h>
#include <gmime/gmime-html-tag-context.h>
#include <gmime/gmime-html-compliance.h>

G_BEGIN_DECLS

/**
 * GMimeHtmlNoScriptHandling:
 * @GMIME_HTML_NO_SCRIPT_HANDLING_UNWRAP: Parse with scripting disabled and omit
 *   the noscript start and end tags while processing their contents as HTML.
 *   This is the default, renderer-independent mode.
 * @GMIME_HTML_NO_SCRIPT_HANDLING_SCRIPTING_ENABLED: Parse with scripting enabled,
 *   treating noscript contents as raw text and retaining the noscript tags.
 *   The output renderer must also have scripting enabled.
 * @GMIME_HTML_NO_SCRIPT_HANDLING_SCRIPTING_DISABLED: Parse with scripting disabled,
 *   processing noscript contents as HTML and retaining the noscript tags.
 *   The output renderer must also have scripting disabled.
 *
 * Controls noscript parsing and output in #GMimeHtmlToHtml. Normal filtering and
 * tag callbacks still apply. These modes do not make the converter a sanitizer.
 **/
typedef enum {
	GMIME_HTML_NO_SCRIPT_HANDLING_UNWRAP,
	GMIME_HTML_NO_SCRIPT_HANDLING_SCRIPTING_ENABLED,
	GMIME_HTML_NO_SCRIPT_HANDLING_SCRIPTING_DISABLED
} GMimeHtmlNoScriptHandling;

#define GMIME_TYPE_HTML_TO_HTML (g_mime_html_to_html_get_type ())

/**
 * GMimeHtmlToHtml:
 *
 * Rewrites HTML with optional tag callbacks and compliance reporting.
 * This converter is not an HTML sanitizer.
 **/
G_DECLARE_FINAL_TYPE (GMimeHtmlToHtml, g_mime_html_to_html, GMIME, HTML_TO_HTML, GMimeTextConverter)

GMimeTextConverter *g_mime_html_to_html_new (void);

gboolean g_mime_html_to_html_get_filter_comments (GMimeHtmlToHtml *converter);
void g_mime_html_to_html_set_filter_comments (GMimeHtmlToHtml *converter, gboolean value);

gboolean g_mime_html_to_html_get_output_html_fragment (GMimeHtmlToHtml *converter);
void g_mime_html_to_html_set_output_html_fragment (GMimeHtmlToHtml *converter, gboolean value);

guint g_mime_html_to_html_get_max_element_depth (GMimeHtmlToHtml *converter);
void g_mime_html_to_html_set_max_element_depth (GMimeHtmlToHtml *converter, guint value);

GMimeHeaderFooterFormat g_mime_html_to_html_get_header_format (GMimeHtmlToHtml *converter);
void g_mime_html_to_html_set_header_format (GMimeHtmlToHtml *converter, GMimeHeaderFooterFormat value);

GMimeHeaderFooterFormat g_mime_html_to_html_get_footer_format (GMimeHtmlToHtml *converter);
void g_mime_html_to_html_set_footer_format (GMimeHtmlToHtml *converter, GMimeHeaderFooterFormat value);

GMimeHtmlNoScriptHandling g_mime_html_to_html_get_no_script_handling (GMimeHtmlToHtml *converter);
void g_mime_html_to_html_set_no_script_handling (GMimeHtmlToHtml *converter, GMimeHtmlNoScriptHandling value);

void g_mime_html_to_html_set_html_tag_callback (GMimeHtmlToHtml *converter, GMimeHtmlTagCallback callback,
						gpointer user_data, GDestroyNotify destroy);
GMimeHtmlTagCallback g_mime_html_to_html_get_html_tag_callback (GMimeHtmlToHtml *converter, gpointer *user_data);

void g_mime_html_to_html_set_compliance_logger (GMimeHtmlToHtml *converter, GMimeHtmlComplianceLogger logger,
						gpointer user_data, GDestroyNotify destroy);
GMimeHtmlComplianceLogger g_mime_html_to_html_get_compliance_logger (GMimeHtmlToHtml *converter, gpointer *user_data);

GMimeHtmlComplianceOptions *g_mime_html_to_html_get_compliance_options (GMimeHtmlToHtml *converter);
void g_mime_html_to_html_set_compliance_options (GMimeHtmlToHtml *converter, GMimeHtmlComplianceOptions *options);

G_END_DECLS

#endif
