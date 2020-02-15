/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2020 Jeffrey Stedfast
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


#ifndef __GMIME_FORMAT_OPTIONS_H__
#define __GMIME_FORMAT_OPTIONS_H__

#include <gmime/gmime-filter.h>

G_BEGIN_DECLS

#define GMIME_TYPE_FORMAT_OPTIONS (gmime_format_options_get_type ())

/**
 * GMimeNewLineFormat:
 * @GMIME_NEWLINE_FORMAT_UNIX: The Unix New-Line format ("\n").
 * @GMIME_NEWLINE_FORMAT_DOS: The DOS New-Line format ("\r\n").
 *
 * There are two commonly used line-endings used by modern Operating Systems.
 * Unix-based systems such as Linux and Mac OS use a single character ('\n' aka LF)
 * to represent the end of line where-as Windows (or DOS) uses a sequence of two
 * characters ("\r\n" aka CRLF). Most text-based network protocols such as SMTP,
 * POP3, and IMAP use the CRLF sequence as well.
 **/
typedef enum {
	GMIME_NEWLINE_FORMAT_UNIX,
	GMIME_NEWLINE_FORMAT_DOS
} GMimeNewLineFormat;


/**
 * GMimeParamEncodingMethod:
 * @GMIME_PARAM_ENCODING_METHOD_DEFAULT: Use the default encoding method set on the #GMimeFormatOptions.
 * @GMIME_PARAM_ENCODING_METHOD_RFC2231: Use the encoding method described in rfc2231.
 * @GMIME_PARAM_ENCODING_METHOD_RFC2047: Use the encoding method described in rfc2047.
 *
 * The MIME specifications specify that the proper method for encoding Content-Type and
 * Content-Disposition parameter values is the method described in
 * <a href="https://tools.ietf.org/html/rfc2231">rfc2231</a>. However, it is common for
 * some older email clients to improperly encode using the method described in
 * <a href="https://tools.ietf.org/html/rfc2047">rfc2047</a> instead.
 **/
typedef enum {
	GMIME_PARAM_ENCODING_METHOD_DEFAULT = 0,
	GMIME_PARAM_ENCODING_METHOD_RFC2231 = 1,
	GMIME_PARAM_ENCODING_METHOD_RFC2047 = 2
} GMimeParamEncodingMethod;


/**
 * GMimeFormatOptions:
 *
 * Format options for serializing various GMime objects.
 **/
typedef struct _GMimeFormatOptions GMimeFormatOptions;

GType g_mime_format_options_get_type (void);

GMimeFormatOptions *g_mime_format_options_get_default (void);

GMimeFormatOptions *g_mime_format_options_new (void);
void g_mime_format_options_free (GMimeFormatOptions *options);

GMimeFormatOptions *g_mime_format_options_clone (GMimeFormatOptions *options);

GMimeParamEncodingMethod g_mime_format_options_get_param_encoding_method (GMimeFormatOptions *options);
void g_mime_format_options_set_param_encoding_method (GMimeFormatOptions *options, GMimeParamEncodingMethod method);

GMimeNewLineFormat g_mime_format_options_get_newline_format (GMimeFormatOptions *options);
void g_mime_format_options_set_newline_format (GMimeFormatOptions *options, GMimeNewLineFormat newline);

const char *g_mime_format_options_get_newline (GMimeFormatOptions *options);
GMimeFilter *g_mime_format_options_create_newline_filter (GMimeFormatOptions *options, gboolean ensure_newline);

/*gboolean g_mime_format_options_get_allow_mixed_charsets (GMimeFormatOptions *options);*/
/*void g_mime_format_options_set_allow_mixed_charsets (GMimeFormatOptions *options, gboolean allow);*/

/*gboolean g_mime_format_options_get_allow_international (GMimeFormatOptions *options);*/
/*void g_mime_format_options_set_allow_international (GMimeFormatOptions *options, gboolean allow);*/

/*gboolean g_mime_format_options_get_max_line (GMimeFormatOptions *options);*/
/*void g_mime_format_options_set_max_line (GMimeFormatOptions *options, gboolean maxline);*/

gboolean g_mime_format_options_is_hidden_header (GMimeFormatOptions *options, const char *header);
void g_mime_format_options_add_hidden_header (GMimeFormatOptions *options, const char *header);
void g_mime_format_options_remove_hidden_header (GMimeFormatOptions *options, const char *header);
void g_mime_format_options_clear_hidden_headers (GMimeFormatOptions *options);

G_END_DECLS

#endif /* __GMIME_FORMAT_OPTIONS_H__ */
