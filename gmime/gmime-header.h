/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2022 Jeffrey Stedfast
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


#ifndef __GMIME_HEADER_H__
#define __GMIME_HEADER_H__

#include <glib.h>
#include <gmime/gmime-format-options.h>
#include <gmime/gmime-parser-options.h>
#include <gmime/gmime-stream.h>

G_BEGIN_DECLS

#define GMIME_TYPE_HEADER                  (g_mime_header_get_type ())
#define GMIME_HEADER(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), GMIME_TYPE_HEADER, GMimeHeader))
#define GMIME_HEADER_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), GMIME_TYPE_HEADER, GMimeHeaderClass))
#define GMIME_IS_HEADER(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GMIME_TYPE_HEADER))
#define GMIME_IS_HEADER_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), GMIME_TYPE_HEADER))
#define GMIME_HEADER_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), GMIME_TYPE_HEADER, GMimeHeaderClass))

#define GMIME_TYPE_HEADER_LIST             (g_mime_header_list_get_type ())
#define GMIME_HEADER_LIST(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), GMIME_TYPE_HEADER_LIST, GMimeHeaderList))
#define GMIME_HEADER_LIST_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), GMIME_TYPE_HEADER_LIST, GMimeHeaderListClass))
#define GMIME_IS_HEADER_LIST(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GMIME_TYPE_HEADER_LIST))
#define GMIME_IS_HEADER_LIST_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), GMIME_TYPE_HEADER_LIST))
#define GMIME_HEADER_LIST_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), GMIME_TYPE_HEADER_LIST, GMimeHeaderListClass))

typedef struct _GMimeHeader GMimeHeader;
typedef struct _GMimeHeaderClass GMimeHeaderClass;

typedef struct _GMimeHeaderList GMimeHeaderList;
typedef struct _GMimeHeaderListClass GMimeHeaderListClass;

/**
 * GMimeHeaderId:
 * @GMIME_HEADER_ACCEPT_LANGUAGE: Accept-Language.
 * @GMIME_HEADER_AD_HOC: Ad-Hoc.
 * @GMIME_HEADER_ALTERNATE_RECIPIENT: Alternate-Recipient.
 * @GMIME_HEADER_APPARENTLY_TO: Apparently-To.
 * @GMIME_HEADER_APPROVED: Approved.
 * @GMIME_HEADER_ARC_AUTHENTICATION_RESULTS: ARC-Authentication-Results.
 * @GMIME_HEADER_ARC_MESSAGE_SIGNATURE: ARC-Message-Signature.
 * @GMIME_HEADER_ARC_SEAL: ARC-Seal.
 * @GMIME_HEADER_ARCHIVE: Archive.
 * @GMIME_HEADER_ARCHIVED_AT: Archived-At.
 * @GMIME_HEADER_ARTICLE: Article.
 * @GMIME_HEADER_AUTHENTICATION_RESULTS: Authentication-Results.
 * @GMIME_HEADER_AUTOCRYPT: Autocrypt.
 * @GMIME_HEADER_AUTOCRYPT_GOSSIP: Autocrypt-Gossip.
 * @GMIME_HEADER_AUTOCRYPT_SETUP_MESSAGE: Autocrypt-Setup-Message.
 * @GMIME_HEADER_AUTOFORWARDED: Autoforwarded.
 * @GMIME_HEADER_AUTO_SUBMITTED: Auto-Submitted, as defined in RFC 3834.
 * @GMIME_HEADER_AUTOSUBMITTED: Legacy Autosubmitted, as defined in RFC 2156.
 *   Most callers want %GMIME_HEADER_AUTO_SUBMITTED instead.
 * @GMIME_HEADER_BASE: Base.
 * @GMIME_HEADER_BCC: Bcc.
 * @GMIME_HEADER_BODY: Body.
 * @GMIME_HEADER_BYTES: Bytes.
 * @GMIME_HEADER_CC: Cc.
 * @GMIME_HEADER_COMMENTS: Comments.
 * @GMIME_HEADER_CONTENT_ALTERNATIVE: Content-Alternative.
 * @GMIME_HEADER_CONTENT_BASE: Content-Base.
 * @GMIME_HEADER_CONTENT_CLASS: Content-Class.
 * @GMIME_HEADER_CONTENT_DESCRIPTION: Content-Description.
 * @GMIME_HEADER_CONTENT_DISPOSITION: Content-Disposition.
 * @GMIME_HEADER_CONTENT_DURATION: Content-Duration.
 * @GMIME_HEADER_CONTENT_FEATURES: Content-Features.
 * @GMIME_HEADER_CONTENT_ID: Content-Id.
 * @GMIME_HEADER_CONTENT_IDENTIFIER: Content-Identifier.
 * @GMIME_HEADER_CONTENT_LANGUAGE: Content-Language.
 * @GMIME_HEADER_CONTENT_LENGTH: Content-Length.
 * @GMIME_HEADER_CONTENT_LOCATION: Content-Location.
 * @GMIME_HEADER_CONTENT_MD5: Content-Md5.
 * @GMIME_HEADER_CONTENT_RETURN: Content-Return.
 * @GMIME_HEADER_CONTENT_TRANSFER_ENCODING: Content-Transfer-Encoding.
 * @GMIME_HEADER_CONTENT_TRANSLATION_TYPE: Content-Translation-Type.
 * @GMIME_HEADER_CONTENT_TYPE: Content-Type.
 * @GMIME_HEADER_CONTROL: Control.
 * @GMIME_HEADER_CONVERSION: Conversion.
 * @GMIME_HEADER_CONVERSION_WITH_LOSS: Conversion-With-Loss.
 * @GMIME_HEADER_DATE: Date.
 * @GMIME_HEADER_DATE_RECEIVED: Date-Received.
 * @GMIME_HEADER_DEFERRED_DELIVERY: Deferred-Delivery.
 * @GMIME_HEADER_DELIVERY_DATE: Delivery-Date.
 * @GMIME_HEADER_DISCLOSE_RECIPIENTS: Disclose-Recipients.
 * @GMIME_HEADER_DISPOSITION_NOTIFICATION_OPTIONS: Disposition-Notification-Options.
 * @GMIME_HEADER_DISPOSITION_NOTIFICATION_TO: Disposition-Notification-To.
 * @GMIME_HEADER_DISTRIBUTION: Distribution.
 * @GMIME_HEADER_DKIM_SIGNATURE: DKIM-Signature.
 * @GMIME_HEADER_DOMAIN_KEY_SIGNATURE: DomainKey-Signature.
 * @GMIME_HEADER_ENCODING: Encoding.
 * @GMIME_HEADER_ENCRYPTED: Encrypted.
 * @GMIME_HEADER_EXPIRES: Expires.
 * @GMIME_HEADER_EXPIRY_DATE: Expiry-Date.
 * @GMIME_HEADER_FOLLOWUP_TO: Followup-To.
 * @GMIME_HEADER_FROM: From.
 * @GMIME_HEADER_GENERATE_DELIVERY_REPORT: Generate-Delivery-Report.
 * @GMIME_HEADER_IMPORTANCE: Importance.
 * @GMIME_HEADER_INJECTION_DATE: Injection-Date.
 * @GMIME_HEADER_INJECTION_INFO: Injection-Info.
 * @GMIME_HEADER_IN_REPLY_TO: In-Reply-To.
 * @GMIME_HEADER_KEYWORDS: Keywords.
 * @GMIME_HEADER_LANGUAGE: Language.
 * @GMIME_HEADER_LATEST_DELIVERY_TIME: Latest-Delivery-Time.
 * @GMIME_HEADER_LINES: Lines.
 * @GMIME_HEADER_LIST_ARCHIVE: List-Archive.
 * @GMIME_HEADER_LIST_HELP: List-Help.
 * @GMIME_HEADER_LIST_ID: List-Id.
 * @GMIME_HEADER_LIST_OWNER: List-Owner.
 * @GMIME_HEADER_LIST_POST: List-Post.
 * @GMIME_HEADER_LIST_SUBSCRIBE: List-Subscribe.
 * @GMIME_HEADER_LIST_UNSUBSCRIBE: List-Unsubscribe.
 * @GMIME_HEADER_LIST_UNSUBSCRIBE_POST: List-Unsubscribe-Post.
 * @GMIME_HEADER_MESSAGE_ID: Message-Id.
 * @GMIME_HEADER_MIME_VERSION: MIME-Version.
 * @GMIME_HEADER_NEWSGROUPS: Newsgroups.
 * @GMIME_HEADER_NNTP_POSTING_HOST: Nntp-Posting-Host.
 * @GMIME_HEADER_ORGANIZATION: Organization.
 * @GMIME_HEADER_ORIGINAL_FROM: Original-From.
 * @GMIME_HEADER_ORIGINAL_MESSAGE_ID: Original-Message-Id.
 * @GMIME_HEADER_ORIGINAL_RECIPIENT: Original-Recipient.
 * @GMIME_HEADER_ORIGINAL_RETURN_ADDRESS: Original-Return-Address.
 * @GMIME_HEADER_ORIGINAL_SUBJECT: Original-Subject.
 * @GMIME_HEADER_PATH: Path.
 * @GMIME_HEADER_PRECEDENCE: Precedence.
 * @GMIME_HEADER_PREVENT_NON_DELIVERY_REPORT: Prevent-NonDelivery-Report.
 * @GMIME_HEADER_PRIORITY: Priority.
 * @GMIME_HEADER_RECEIVED: Received.
 * @GMIME_HEADER_RECEIVED_SPF: Received-SPF.
 * @GMIME_HEADER_REFERENCES: References.
 * @GMIME_HEADER_RELAY_VERSION: Relay-Version.
 * @GMIME_HEADER_REPLY_BY: Reply-By.
 * @GMIME_HEADER_REPLY_TO: Reply-To.
 * @GMIME_HEADER_REQUIRE_RECIPIENT_VALID_SINCE: Require-Recipient-Valid-Since.
 * @GMIME_HEADER_RESENT_BCC: Resent-Bcc.
 * @GMIME_HEADER_RESENT_CC: Resent-Cc.
 * @GMIME_HEADER_RESENT_DATE: Resent-Date.
 * @GMIME_HEADER_RESENT_FROM: Resent-From.
 * @GMIME_HEADER_RESENT_MESSAGE_ID: Resent-Message-Id.
 * @GMIME_HEADER_RESENT_REPLY_TO: Resent-Reply-To.
 * @GMIME_HEADER_RESENT_SENDER: Resent-Sender.
 * @GMIME_HEADER_RESENT_TO: Resent-To.
 * @GMIME_HEADER_RETURN_PATH: Return-Path.
 * @GMIME_HEADER_RETURN_RECEIPT_TO: Return-Receipt-To.
 * @GMIME_HEADER_SEE_ALSO: See-Also.
 * @GMIME_HEADER_SENDER: Sender.
 * @GMIME_HEADER_SENSITIVITY: Sensitivity.
 * @GMIME_HEADER_SOLICITATION: Solicitation.
 * @GMIME_HEADER_STATUS: Status.
 * @GMIME_HEADER_SUBJECT: Subject.
 * @GMIME_HEADER_SUMMARY: Summary.
 * @GMIME_HEADER_SUPERSEDES: Supersedes.
 * @GMIME_HEADER_TLS_REQUIRED: TLS-Required.
 * @GMIME_HEADER_TO: To.
 * @GMIME_HEADER_USER_AGENT: User-Agent.
 * @GMIME_HEADER_X400_CONTENT_IDENTIFIER: X400-Content-Identifier.
 * @GMIME_HEADER_X400_CONTENT_RETURN: X400-Content-Return.
 * @GMIME_HEADER_X400_CONTENT_TYPE: X400-Content-Type.
 * @GMIME_HEADER_X400_MTS_IDENTIFIER: X400-MTS-Identifier.
 * @GMIME_HEADER_X400_ORIGINATOR: X400-Originator.
 * @GMIME_HEADER_X400_RECEIVED: X400-Received.
 * @GMIME_HEADER_X400_RECIPIENTS: X400-Recipients.
 * @GMIME_HEADER_X400_TRACE: X400-Trace.
 * @GMIME_HEADER_X_MAILER: X-Mailer.
 * @GMIME_HEADER_X_MSMAIL_PRIORITY: X-MSMail-Priority.
 * @GMIME_HEADER_X_PRIORITY: X-Priority.
 * @GMIME_HEADER_X_STATUS: X-Status.
 * @GMIME_HEADER_UNKNOWN: An unknown header field.
 *
 * Identifiers for common header fields. Comparing enum values is faster
 * and less error-prone than comparing strings.
 **/
typedef enum {
	GMIME_HEADER_ACCEPT_LANGUAGE,
	GMIME_HEADER_AD_HOC,
	GMIME_HEADER_ALTERNATE_RECIPIENT,
	GMIME_HEADER_APPARENTLY_TO,
	GMIME_HEADER_APPROVED,
	GMIME_HEADER_ARC_AUTHENTICATION_RESULTS,
	GMIME_HEADER_ARC_MESSAGE_SIGNATURE,
	GMIME_HEADER_ARC_SEAL,
	GMIME_HEADER_ARCHIVE,
	GMIME_HEADER_ARCHIVED_AT,
	GMIME_HEADER_ARTICLE,
	GMIME_HEADER_AUTHENTICATION_RESULTS,
	GMIME_HEADER_AUTOCRYPT,
	GMIME_HEADER_AUTOCRYPT_GOSSIP,
	GMIME_HEADER_AUTOCRYPT_SETUP_MESSAGE,
	GMIME_HEADER_AUTOFORWARDED,
	GMIME_HEADER_AUTO_SUBMITTED,
	GMIME_HEADER_AUTOSUBMITTED,
	GMIME_HEADER_BASE,
	GMIME_HEADER_BCC,
	GMIME_HEADER_BODY,
	GMIME_HEADER_BYTES,
	GMIME_HEADER_CC,
	GMIME_HEADER_COMMENTS,
	GMIME_HEADER_CONTENT_ALTERNATIVE,
	GMIME_HEADER_CONTENT_BASE,
	GMIME_HEADER_CONTENT_CLASS,
	GMIME_HEADER_CONTENT_DESCRIPTION,
	GMIME_HEADER_CONTENT_DISPOSITION,
	GMIME_HEADER_CONTENT_DURATION,
	GMIME_HEADER_CONTENT_FEATURES,
	GMIME_HEADER_CONTENT_ID,
	GMIME_HEADER_CONTENT_IDENTIFIER,
	GMIME_HEADER_CONTENT_LANGUAGE,
	GMIME_HEADER_CONTENT_LENGTH,
	GMIME_HEADER_CONTENT_LOCATION,
	GMIME_HEADER_CONTENT_MD5,
	GMIME_HEADER_CONTENT_RETURN,
	GMIME_HEADER_CONTENT_TRANSFER_ENCODING,
	GMIME_HEADER_CONTENT_TRANSLATION_TYPE,
	GMIME_HEADER_CONTENT_TYPE,
	GMIME_HEADER_CONTROL,
	GMIME_HEADER_CONVERSION,
	GMIME_HEADER_CONVERSION_WITH_LOSS,
	GMIME_HEADER_DATE,
	GMIME_HEADER_DATE_RECEIVED,
	GMIME_HEADER_DEFERRED_DELIVERY,
	GMIME_HEADER_DELIVERY_DATE,
	GMIME_HEADER_DISCLOSE_RECIPIENTS,
	GMIME_HEADER_DISPOSITION_NOTIFICATION_OPTIONS,
	GMIME_HEADER_DISPOSITION_NOTIFICATION_TO,
	GMIME_HEADER_DISTRIBUTION,
	GMIME_HEADER_DKIM_SIGNATURE,
	GMIME_HEADER_DOMAIN_KEY_SIGNATURE,
	GMIME_HEADER_ENCODING,
	GMIME_HEADER_ENCRYPTED,
	GMIME_HEADER_EXPIRES,
	GMIME_HEADER_EXPIRY_DATE,
	GMIME_HEADER_FOLLOWUP_TO,
	GMIME_HEADER_FROM,
	GMIME_HEADER_GENERATE_DELIVERY_REPORT,
	GMIME_HEADER_IMPORTANCE,
	GMIME_HEADER_INJECTION_DATE,
	GMIME_HEADER_INJECTION_INFO,
	GMIME_HEADER_IN_REPLY_TO,
	GMIME_HEADER_KEYWORDS,
	GMIME_HEADER_LANGUAGE,
	GMIME_HEADER_LATEST_DELIVERY_TIME,
	GMIME_HEADER_LINES,
	GMIME_HEADER_LIST_ARCHIVE,
	GMIME_HEADER_LIST_HELP,
	GMIME_HEADER_LIST_ID,
	GMIME_HEADER_LIST_OWNER,
	GMIME_HEADER_LIST_POST,
	GMIME_HEADER_LIST_SUBSCRIBE,
	GMIME_HEADER_LIST_UNSUBSCRIBE,
	GMIME_HEADER_LIST_UNSUBSCRIBE_POST,
	GMIME_HEADER_MESSAGE_ID,
	GMIME_HEADER_MIME_VERSION,
	GMIME_HEADER_NEWSGROUPS,
	GMIME_HEADER_NNTP_POSTING_HOST,
	GMIME_HEADER_ORGANIZATION,
	GMIME_HEADER_ORIGINAL_FROM,
	GMIME_HEADER_ORIGINAL_MESSAGE_ID,
	GMIME_HEADER_ORIGINAL_RECIPIENT,
	GMIME_HEADER_ORIGINAL_RETURN_ADDRESS,
	GMIME_HEADER_ORIGINAL_SUBJECT,
	GMIME_HEADER_PATH,
	GMIME_HEADER_PRECEDENCE,
	GMIME_HEADER_PREVENT_NON_DELIVERY_REPORT,
	GMIME_HEADER_PRIORITY,
	GMIME_HEADER_RECEIVED,
	GMIME_HEADER_RECEIVED_SPF,
	GMIME_HEADER_REFERENCES,
	GMIME_HEADER_RELAY_VERSION,
	GMIME_HEADER_REPLY_BY,
	GMIME_HEADER_REPLY_TO,
	GMIME_HEADER_REQUIRE_RECIPIENT_VALID_SINCE,
	GMIME_HEADER_RESENT_BCC,
	GMIME_HEADER_RESENT_CC,
	GMIME_HEADER_RESENT_DATE,
	GMIME_HEADER_RESENT_FROM,
	GMIME_HEADER_RESENT_MESSAGE_ID,
	GMIME_HEADER_RESENT_REPLY_TO,
	GMIME_HEADER_RESENT_SENDER,
	GMIME_HEADER_RESENT_TO,
	GMIME_HEADER_RETURN_PATH,
	GMIME_HEADER_RETURN_RECEIPT_TO,
	GMIME_HEADER_SEE_ALSO,
	GMIME_HEADER_SENDER,
	GMIME_HEADER_SENSITIVITY,
	GMIME_HEADER_SOLICITATION,
	GMIME_HEADER_STATUS,
	GMIME_HEADER_SUBJECT,
	GMIME_HEADER_SUMMARY,
	GMIME_HEADER_SUPERSEDES,
	GMIME_HEADER_TLS_REQUIRED,
	GMIME_HEADER_TO,
	GMIME_HEADER_USER_AGENT,
	GMIME_HEADER_X400_CONTENT_IDENTIFIER,
	GMIME_HEADER_X400_CONTENT_RETURN,
	GMIME_HEADER_X400_CONTENT_TYPE,
	GMIME_HEADER_X400_MTS_IDENTIFIER,
	GMIME_HEADER_X400_ORIGINATOR,
	GMIME_HEADER_X400_RECEIVED,
	GMIME_HEADER_X400_RECIPIENTS,
	GMIME_HEADER_X400_TRACE,
	GMIME_HEADER_X_MAILER,
	GMIME_HEADER_X_MSMAIL_PRIORITY,
	GMIME_HEADER_X_PRIORITY,
	GMIME_HEADER_X_STATUS,
	GMIME_HEADER_UNKNOWN = -1
} GMimeHeaderId;

const char *g_mime_header_id_to_name (GMimeHeaderId id);
GMimeHeaderId g_mime_header_id_from_name (const char *name);


/**
 * GMimeHeaderRawValueFormatter:
 * @header: a #GMimeHeader
 * @options: a #GMimeFormatOptions
 * @value: an unencoded header value
 * @charset: a charset
 *
 * Function callback for encoding and formatting a header value.
 *
 * Returns: the encoded and formatted raw header value.
 **/
typedef char * (* GMimeHeaderRawValueFormatter) (GMimeHeader *header, GMimeFormatOptions *options,
						 const char *value, const char *charset);

char *g_mime_header_format_content_disposition (GMimeHeader *header, GMimeFormatOptions *options, const char *value, const char *charset);
char *g_mime_header_format_content_type (GMimeHeader *header, GMimeFormatOptions *options, const char *value, const char *charset);
char *g_mime_header_format_disposition_notification_options (GMimeHeader *header, GMimeFormatOptions *options, const char *value, const char *charset);
char *g_mime_header_format_dkim_signature (GMimeHeader *header, GMimeFormatOptions *options, const char *value, const char *charset);
char *g_mime_header_format_mailing_list_command (GMimeHeader *header, GMimeFormatOptions *options, const char *value, const char *charset);
char *g_mime_header_format_message_id (GMimeHeader *header, GMimeFormatOptions *options, const char *value, const char *charset);
char *g_mime_header_format_references (GMimeHeader *header, GMimeFormatOptions *options, const char *value, const char *charset);
char *g_mime_header_format_addrlist (GMimeHeader *header, GMimeFormatOptions *options, const char *value, const char *charset);
char *g_mime_header_format_received (GMimeHeader *header, GMimeFormatOptions *options, const char *value, const char *charset);
char *g_mime_header_format_newsgroups (GMimeHeader *header, GMimeFormatOptions *options, const char *value, const char *charset);
char *g_mime_header_format_default (GMimeHeader *header, GMimeFormatOptions *options, const char *value, const char *charset);


/**
 * GMimeHeader:
 * @name: the name of the header
 * @value: the unfolded value of the header
 *
 * A message or mime-part header.
 **/
struct _GMimeHeader {
	/* <private> */
	GObject parent_object;
	
	char *name, *value;
	
	/* <private> */
	GMimeHeaderRawValueFormatter formatter;
	GMimeParserOptions *options;
	gboolean reformat;
	gpointer changed;
	GBytes *raw_value;
	GBytes *raw_name;
	char *charset;
	gint64 offset;
	gboolean owns_options;
	gboolean explicit_raw_value;
	gboolean invalid;
	GMimeHeaderId id;
};

struct _GMimeHeaderClass {
	GObjectClass parent_class;
	
};


GType g_mime_header_get_type (void);

GMimeHeader *g_mime_header_new (const char *name, const char *value, const char *charset);
GMimeHeader *g_mime_header_new_full (const char *name, const char *value, const char *charset, GError **error);
GMimeHeader *g_mime_header_parse (GMimeParserOptions *options, const guint8 *buffer, gsize length, GError **error);
GMimeHeader *g_mime_header_parse_text (GMimeParserOptions *options, const char *text, GError **error);
GMimeHeader *g_mime_header_clone (GMimeHeader *header);

char *g_mime_header_unfold (const char *text);

GMimeHeaderId g_mime_header_get_id (GMimeHeader *header);
const char *g_mime_header_get_name (GMimeHeader *header);
GBytes *g_mime_header_get_raw_name (GMimeHeader *header);

const char *g_mime_header_get_value (GMimeHeader *header);
char *g_mime_header_get_value_with_charset (GMimeHeader *header, const char *charset, GError **error);
void g_mime_header_set_value (GMimeHeader *header, GMimeFormatOptions *options, const char *value, const char *charset);
gboolean g_mime_header_set_value_full (GMimeHeader *header, GMimeFormatOptions *options, const char *value,
				     const char *charset, GError **error);

GBytes *g_mime_header_get_raw_value (GMimeHeader *header);
gboolean g_mime_header_set_raw_value (GMimeHeader *header, GBytes *raw_value, GError **error);

gint64 g_mime_header_get_offset (GMimeHeader *header);

ssize_t g_mime_header_write_to_stream (GMimeHeader *header, GMimeFormatOptions *options, GMimeStream *stream);
char *g_mime_header_to_string (GMimeHeader *header);


/**
 * GMimeHeaderList:
 *
 * A list of message or mime-part headers.
 **/
struct _GMimeHeaderList {
	GObject parent_object;
	
	/* < private > */
	GMimeParserOptions *options;
	gpointer changed;
	GHashTable *hash;
	GPtrArray *array;
	gboolean has_body_separator;
};

struct _GMimeHeaderListClass {
	GObjectClass parent_class;
	
};


GType g_mime_header_list_get_type (void);

GMimeHeaderList *g_mime_header_list_new (GMimeParserOptions *options);
GMimeHeaderList *g_mime_header_list_load (GMimeParserOptions *options, GMimeStream *stream, GError **error);
GMimeHeaderList *g_mime_header_list_load_file (GMimeParserOptions *options, const char *filename, GError **error);

void g_mime_header_list_clear (GMimeHeaderList *headers);
int g_mime_header_list_get_count (GMimeHeaderList *headers);
gboolean g_mime_header_list_contains (GMimeHeaderList *headers, const char *name);
gboolean g_mime_header_list_contains_id (GMimeHeaderList *headers, GMimeHeaderId id);
gboolean g_mime_header_list_contains_header (GMimeHeaderList *headers, GMimeHeader *header);
int g_mime_header_list_index_of (GMimeHeaderList *headers, const char *name);
int g_mime_header_list_index_of_id (GMimeHeaderList *headers, GMimeHeaderId id);
int g_mime_header_list_index_of_header (GMimeHeaderList *headers, GMimeHeader *header);
int g_mime_header_list_last_index_of (GMimeHeaderList *headers, const char *name);
int g_mime_header_list_last_index_of_id (GMimeHeaderList *headers, GMimeHeaderId id);
int g_mime_header_list_add (GMimeHeaderList *headers, GMimeHeader *header);
void g_mime_header_list_insert_header (GMimeHeaderList *headers, int index, GMimeHeader *header);
void g_mime_header_list_insert (GMimeHeaderList *headers, int index, const char *name, const char *value, const char *charset);
gboolean g_mime_header_list_insert_full (GMimeHeaderList *headers, int index, const char *name, const char *value,
				       const char *charset, GError **error);
void g_mime_header_list_prepend (GMimeHeaderList *headers, const char *name, const char *value, const char *charset);
gboolean g_mime_header_list_prepend_full (GMimeHeaderList *headers, const char *name, const char *value,
					const char *charset, GError **error);
void g_mime_header_list_append (GMimeHeaderList *headers, const char *name, const char *value, const char *charset);
gboolean g_mime_header_list_append_full (GMimeHeaderList *headers, const char *name, const char *value,
				       const char *charset, GError **error);
void g_mime_header_list_set (GMimeHeaderList *headers, const char *name, const char *value, const char *charset);
const char *g_mime_header_list_get_value (GMimeHeaderList *headers, const char *name);
const char *g_mime_header_list_get_value_by_id (GMimeHeaderList *headers, GMimeHeaderId id);
gboolean g_mime_header_list_set_value_full (GMimeHeaderList *headers, const char *name, const char *value,
					  const char *charset, GError **error);
void g_mime_header_list_replace (GMimeHeaderList *headers, GMimeHeader *header);
gboolean g_mime_header_list_replace_value_full (GMimeHeaderList *headers, const char *name, const char *value,
					      const char *charset, GError **error);
GMimeHeader *g_mime_header_list_get_header (GMimeHeaderList *headers, const char *name);
GMimeHeader *g_mime_header_list_get_header_at (GMimeHeaderList *headers, int index);
void g_mime_header_list_set_header_at (GMimeHeaderList *headers, int index, GMimeHeader *header);
gboolean g_mime_header_list_remove (GMimeHeaderList *headers, const char *name);
gboolean g_mime_header_list_remove_id (GMimeHeaderList *headers, GMimeHeaderId id);
gboolean g_mime_header_list_remove_header (GMimeHeaderList *headers, GMimeHeader *header);
void g_mime_header_list_remove_all (GMimeHeaderList *headers, const char *name);
void g_mime_header_list_remove_all_id (GMimeHeaderList *headers, GMimeHeaderId id);
void g_mime_header_list_remove_at (GMimeHeaderList *headers, int index);

ssize_t g_mime_header_list_write_to_stream (GMimeHeaderList *headers, GMimeFormatOptions *options, GMimeStream *stream);
char *g_mime_header_list_to_string (GMimeHeaderList *headers, GMimeFormatOptions *options);

G_END_DECLS

#endif /* __GMIME_HEADER_H__ */
