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

#ifndef __GMIME_OBJECT_PRIVATE_H__
#define __GMIME_OBJECT_PRIVATE_H__

#include <gmime/gmime.h>

G_BEGIN_DECLS

struct _GMimeAnonymizer {
	GObject parent_instance;

	GHashTable *preserve_headers;
};

typedef struct {
	GMimeSecureMimeType smime_type;
} GMimeApplicationPkcs7MimePrivate;

#ifdef GMIME_PRIVATE_TESTS
static inline GMimeApplicationPkcs7MimePrivate *
_g_mime_application_pkcs7_mime_get_private (GMimeApplicationPkcs7Mime *object)
{
	return g_type_instance_get_private ((GTypeInstance *) object, GMIME_TYPE_APPLICATION_PKCS7_MIME);
}
#else
G_GNUC_INTERNAL GMimeApplicationPkcs7MimePrivate *_g_mime_application_pkcs7_mime_get_private (GMimeApplicationPkcs7Mime *object) G_GNUC_CONST;
#endif

struct _GMimeAttachmentCollection {
	GObject parent_instance;

	GPtrArray *array;
	gboolean linked;
};

struct _GMimeAutocryptHeader {
	GObject parent_instance;

	GMimeMailboxAddress *address;
	GMimeAutocryptPreferEncrypt prefer_encrypt;
	GBytes *keydata;
	GDateTime *effective_date;
};

struct _GMimeAutocryptHeaderList {
	GObject parent_instance;

	GPtrArray *array;
};

struct _GMimeBodyBuilder {
	GObject parent_instance;

	GMimeAttachmentCollection *attachments;
	GMimeAttachmentCollection *linked_resources;
	char *text_body;
	char *html_body;
	char *body_encoding;
};

struct _GMimeCertificate {
	GObject parent_instance;

	GMimePubKeyAlgo pubkey_algo;
	GMimeDigestAlgo digest_algo;
	GMimeTrust trust;
	char *issuer_serial;
	char *issuer_name;
	char *fingerprint;
	GDateTime *created;
	GDateTime *expires;
	char *keyid;
	char *email;
	char *name;
	char *user_id;
	GMimeValidity id_validity;
};

struct _GMimeCertificateList {
	GObject parent_instance;
	GPtrArray *array;
};

struct _GMimeContentType {
	GObject parent_instance;

	char *type, *subtype;
	GMimeParamList *params;
	gpointer changed;
};

typedef struct {
	GMimeContentEncoding encoding;
	GMimeStream *stream;


	GMimeNewLineFormat newline_format;
	gboolean has_newline_format;
} GMimeContentPrivate;

#ifdef GMIME_PRIVATE_TESTS
static inline GMimeContentPrivate *
_g_mime_content_get_private (GMimeContent *object)
{
	return g_type_instance_get_private ((GTypeInstance *) object, GMIME_TYPE_CONTENT);
}
#else
G_GNUC_INTERNAL GMimeContentPrivate *_g_mime_content_get_private (GMimeContent *object) G_GNUC_CONST;
#endif

typedef struct {
	GMimePasswordRequestFunc request_passwd;
} GMimeCryptoContextPrivate;

#ifdef GMIME_PRIVATE_TESTS
static inline GMimeCryptoContextPrivate *
_g_mime_crypto_context_get_private (GMimeCryptoContext *object)
{
	return g_type_instance_get_private ((GTypeInstance *) object, GMIME_TYPE_CRYPTO_CONTEXT);
}
#else
G_GNUC_INTERNAL GMimeCryptoContextPrivate *_g_mime_crypto_context_get_private (GMimeCryptoContext *object) G_GNUC_CONST;
#endif

struct _GMimeDecryptResult {
	GObject parent_instance;

	GMimeCertificateList *recipients;
	GMimeSignatureList *signatures;
	GMimeCipherAlgo cipher;
	GMimeDigestAlgo mdc;
	char *session_key;
};

struct _GMimeContentDisposition {
	GObject parent_instance;

	char *disposition;
	GMimeParamList *params;
	gpointer changed;
};

struct _GMimeDomainList {
	GObject parent_instance;

	GPtrArray *array;
	gpointer changed;
};

typedef struct {
	GMimeContentDisposition *disposition;
	GMimeContentType *content_type;
	GMimeHeaderList *headers;

	char *content_id;
	char *content_base;
	char *content_location;

	unsigned int ensure_newline : 1;
	unsigned int content_base_loaded : 1;
	unsigned int content_location_loaded : 1;
} GMimeEntityPrivate;

#ifdef GMIME_PRIVATE_TESTS
static inline GMimeEntityPrivate *
_g_mime_entity_get_private (GMimeEntity *object)
{
	return g_type_instance_get_private ((GTypeInstance *) object, GMIME_TYPE_ENTITY);
}
#else
G_GNUC_INTERNAL GMimeEntityPrivate *_g_mime_entity_get_private (GMimeEntity *object) G_GNUC_CONST;
#endif

typedef struct {
	GMimeEncoder *encoder;
	GMimeDecoder *decoder;
} GMimeFilterBasicPrivate;

#ifdef GMIME_PRIVATE_TESTS
static inline GMimeFilterBasicPrivate *
_g_mime_filter_basic_get_private (GMimeFilterBasic *object)
{
	return g_type_instance_get_private ((GTypeInstance *) object, GMIME_TYPE_FILTER_BASIC);
}
#else
G_GNUC_INTERNAL GMimeFilterBasicPrivate *_g_mime_filter_basic_get_private (GMimeFilterBasic *object) G_GNUC_CONST;
#endif

typedef struct {
	GMimeFilterBestFlags flags;


	GMimeCharset charset;


	guint64 count0;
	guint64 count8;
	guint64 total;

	guint64 maxline;
	guint64 linelen;

	unsigned char frombuf[6];
	unsigned int fromlen : 4;
	unsigned int hadfrom : 1;

	unsigned char previous;
} GMimeFilterBestPrivate;

#ifdef GMIME_PRIVATE_TESTS
static inline GMimeFilterBestPrivate *
_g_mime_filter_best_get_private (GMimeFilterBest *object)
{
	return g_type_instance_get_private ((GTypeInstance *) object, GMIME_TYPE_FILTER_BEST);
}
#else
G_GNUC_INTERNAL GMimeFilterBestPrivate *_g_mime_filter_best_get_private (GMimeFilterBest *object) G_GNUC_CONST;
#endif

typedef struct {
	char *from_charset;
	char *to_charset;
	iconv_t cd;
} GMimeFilterCharsetPrivate;

#ifdef GMIME_PRIVATE_TESTS
static inline GMimeFilterCharsetPrivate *
_g_mime_filter_charset_get_private (GMimeFilterCharset *object)
{
	return g_type_instance_get_private ((GTypeInstance *) object, GMIME_TYPE_FILTER_CHARSET);
}
#else
G_GNUC_INTERNAL GMimeFilterCharsetPrivate *_g_mime_filter_charset_get_private (GMimeFilterCharset *object) G_GNUC_CONST;
#endif

typedef struct {
	GChecksum *checksum;
} GMimeFilterChecksumPrivate;

#ifdef GMIME_PRIVATE_TESTS
static inline GMimeFilterChecksumPrivate *
_g_mime_filter_checksum_get_private (GMimeFilterChecksum *object)
{
	return g_type_instance_get_private ((GTypeInstance *) object, GMIME_TYPE_FILTER_CHECKSUM);
}
#else
G_GNUC_INTERNAL GMimeFilterChecksumPrivate *_g_mime_filter_checksum_get_private (GMimeFilterChecksum *object) G_GNUC_CONST;
#endif

typedef struct {
	gboolean ensure_newline;
	char pc;
} GMimeFilterDos2UnixPrivate;

#ifdef GMIME_PRIVATE_TESTS
static inline GMimeFilterDos2UnixPrivate *
_g_mime_filter_dos2unix_get_private (GMimeFilterDos2Unix *object)
{
	return g_type_instance_get_private ((GTypeInstance *) object, GMIME_TYPE_FILTER_DOS2UNIX);
}
#else
G_GNUC_INTERNAL GMimeFilterDos2UnixPrivate *_g_mime_filter_dos2unix_get_private (GMimeFilterDos2Unix *object) G_GNUC_CONST;
#endif

typedef struct {
	guint32 flags;
	int nofill;
} GMimeFilterEnrichedPrivate;

#ifdef GMIME_PRIVATE_TESTS
static inline GMimeFilterEnrichedPrivate *
_g_mime_filter_enriched_get_private (GMimeFilterEnriched *object)
{
	return g_type_instance_get_private ((GTypeInstance *) object, GMIME_TYPE_FILTER_ENRICHED);
}
#else
G_GNUC_INTERNAL GMimeFilterEnrichedPrivate *_g_mime_filter_enriched_get_private (GMimeFilterEnriched *object) G_GNUC_CONST;
#endif

typedef struct {
	GMimeFilterFromMode mode;
	gboolean midline;
} GMimeFilterFromPrivate;

#ifdef GMIME_PRIVATE_TESTS
static inline GMimeFilterFromPrivate *
_g_mime_filter_from_get_private (GMimeFilterFrom *object)
{
	return g_type_instance_get_private ((GTypeInstance *) object, GMIME_TYPE_FILTER_FROM);
}
#else
G_GNUC_INTERNAL GMimeFilterFromPrivate *_g_mime_filter_from_get_private (GMimeFilterFrom *object) G_GNUC_CONST;
#endif

typedef struct _GMimeFilterGZipPrivate GMimeFilterGZipPrivate;

#ifdef GMIME_PRIVATE_TESTS
static inline GMimeFilterGZipPrivate *
_g_mime_filter_gzip_get_private (GMimeFilterGZip *object)
{
	return g_type_instance_get_private ((GTypeInstance *) object, GMIME_TYPE_FILTER_GZIP);
}
#else
G_GNUC_INTERNAL GMimeFilterGZipPrivate *_g_mime_filter_gzip_get_private (GMimeFilterGZip *object) G_GNUC_CONST;
#endif

typedef struct {
	struct _UrlScanner *scanner;

	guint32 flags;
	guint32 colour;

	guint32 column       : 31;
	guint32 pre_open     : 1;

	guint32 citation_depth;
} GMimeFilterHTMLPrivate;

#ifdef GMIME_PRIVATE_TESTS
static inline GMimeFilterHTMLPrivate *
_g_mime_filter_html_get_private (GMimeFilterHTML *object)
{
	return g_type_instance_get_private ((GTypeInstance *) object, GMIME_TYPE_FILTER_HTML);
}
#else
G_GNUC_INTERNAL GMimeFilterHTMLPrivate *_g_mime_filter_html_get_private (GMimeFilterHTML *object) G_GNUC_CONST;
#endif

typedef struct {
	GMimeOpenPGPPrivate state;
	gboolean seen_end_marker;
	gboolean midline;
	gint64 begin_offset;
	gint64 end_offset;
	gint64 position;
	guint next;
} GMimeFilterOpenPGPPrivate;

#ifdef GMIME_PRIVATE_TESTS
static inline GMimeFilterOpenPGPPrivate *
_g_mime_filter_openpgp_get_private (GMimeFilterOpenPGP *object)
{
	return g_type_instance_get_private ((GTypeInstance *) object, GMIME_TYPE_FILTER_OPENPGP);
}
#else
G_GNUC_INTERNAL GMimeFilterOpenPGPPrivate *_g_mime_filter_openpgp_get_private (GMimeFilterOpenPGP *object) G_GNUC_CONST;
#endif

typedef struct {
	gboolean bol;
} GMimeFilterSmtpDataPrivate;

#ifdef GMIME_PRIVATE_TESTS
static inline GMimeFilterSmtpDataPrivate *
_g_mime_filter_smtp_data_get_private (GMimeFilterSmtpData *object)
{
	return g_type_instance_get_private ((GTypeInstance *) object, GMIME_TYPE_FILTER_SMTP_DATA);
}
#else
G_GNUC_INTERNAL GMimeFilterSmtpDataPrivate *_g_mime_filter_smtp_data_get_private (GMimeFilterSmtpData *object) G_GNUC_CONST;
#endif

typedef struct {
	void *lwsp;
} GMimeFilterStripPrivate;

#ifdef GMIME_PRIVATE_TESTS
static inline GMimeFilterStripPrivate *
_g_mime_filter_strip_get_private (GMimeFilterStrip *object)
{
	return g_type_instance_get_private ((GTypeInstance *) object, GMIME_TYPE_FILTER_STRIP);
}
#else
G_GNUC_INTERNAL GMimeFilterStripPrivate *_g_mime_filter_strip_get_private (GMimeFilterStrip *object) G_GNUC_CONST;
#endif

typedef struct {
	gboolean ensure_newline;
	char pc;
} GMimeFilterUnix2DosPrivate;

#ifdef GMIME_PRIVATE_TESTS
static inline GMimeFilterUnix2DosPrivate *
_g_mime_filter_unix2dos_get_private (GMimeFilterUnix2Dos *object)
{
	return g_type_instance_get_private ((GTypeInstance *) object, GMIME_TYPE_FILTER_UNIX2DOS);
}
#else
G_GNUC_INTERNAL GMimeFilterUnix2DosPrivate *_g_mime_filter_unix2dos_get_private (GMimeFilterUnix2Dos *object) G_GNUC_CONST;
#endif

typedef struct {
	gboolean is_windows;
	char *claimed_charset;
} GMimeFilterWindowsPrivate;

#ifdef GMIME_PRIVATE_TESTS
static inline GMimeFilterWindowsPrivate *
_g_mime_filter_windows_get_private (GMimeFilterWindows *object)
{
	return g_type_instance_get_private ((GTypeInstance *) object, GMIME_TYPE_FILTER_WINDOWS);
}
#else
G_GNUC_INTERNAL GMimeFilterWindowsPrivate *_g_mime_filter_windows_get_private (GMimeFilterWindows *object) G_GNUC_CONST;
#endif

typedef struct {
	char *inbuf;
	size_t inlen;

	char *outreal;
	char *outbuf;
	char *outptr;
	size_t outsize;
	size_t outpre;

	char *backbuf;
	size_t backsize;
	size_t backlen;
} GMimeFilterPrivate;

#ifdef GMIME_PRIVATE_TESTS
static inline GMimeFilterPrivate *
_g_mime_filter_get_private (GMimeFilter *object)
{
	return g_type_instance_get_private ((GTypeInstance *) object, GMIME_TYPE_FILTER);
}
#else
G_GNUC_INTERNAL GMimeFilterPrivate *_g_mime_filter_get_private (GMimeFilter *object) G_GNUC_CONST;
#endif

struct _GMimeHeaderListCollection {
	GObject parent_instance;

	GPtrArray *array;
	gpointer changed;
};

struct _GMimeHeader {
	GObject parent_instance;

	char *name, *value;

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

struct _GMimeHeaderList {
	GObject parent_instance;

	GMimeParserOptions *options;
	gpointer changed;
	GHashTable *hash;
	GPtrArray *array;
	gboolean has_body_separator;
};

typedef struct {
	char *charset;
	char *name;

	gpointer changed;
} GMimeInternetAddressPrivate;

#ifdef GMIME_PRIVATE_TESTS
static inline GMimeInternetAddressPrivate *
_g_mime_internet_address_get_private (GMimeInternetAddress *object)
{
	return g_type_instance_get_private ((GTypeInstance *) object, GMIME_TYPE_INTERNET_ADDRESS);
}
#else
G_GNUC_INTERNAL GMimeInternetAddressPrivate *_g_mime_internet_address_get_private (GMimeInternetAddress *object) G_GNUC_CONST;
#endif

typedef struct {
	char *addrspec;
	int at;
	GMimeDomainList *route;
} GMimeMailboxAddressPrivate;

#ifdef GMIME_PRIVATE_TESTS
static inline GMimeMailboxAddressPrivate *
_g_mime_mailbox_address_get_private (GMimeMailboxAddress *object)
{
	return g_type_instance_get_private ((GTypeInstance *) object, GMIME_TYPE_MAILBOX_ADDRESS);
}
#else
G_GNUC_INTERNAL GMimeMailboxAddressPrivate *_g_mime_mailbox_address_get_private (GMimeMailboxAddress *object) G_GNUC_CONST;
#endif

typedef struct {
	GMimeInternetAddressList *members;
} GMimeGroupAddressPrivate;

#ifdef GMIME_PRIVATE_TESTS
static inline GMimeGroupAddressPrivate *
_g_mime_group_address_get_private (GMimeGroupAddress *object)
{
	return g_type_instance_get_private ((GTypeInstance *) object, GMIME_TYPE_GROUP_ADDRESS);
}
#else
G_GNUC_INTERNAL GMimeGroupAddressPrivate *_g_mime_group_address_get_private (GMimeGroupAddress *object) G_GNUC_CONST;
#endif

struct _GMimeInternetAddressList {
	GObject parent_instance;

	GPtrArray *array;
	gpointer changed;
};

struct _GMimeIterator {
	GObject parent_instance;

	GMimeMessage *message;
	GArray *frames;
	gboolean move_first;
	gboolean skip_requested;
};

typedef struct {
	GMimeHeaderListCollection *groups;
} GMimeMessageDeliveryStatusPrivate;

#ifdef GMIME_PRIVATE_TESTS
static inline GMimeMessageDeliveryStatusPrivate *
_g_mime_message_delivery_status_get_private (GMimeMessageDeliveryStatus *object)
{
	return g_type_instance_get_private ((GTypeInstance *) object, GMIME_TYPE_MESSAGE_DELIVERY_STATUS);
}
#else
G_GNUC_INTERNAL GMimeMessageDeliveryStatusPrivate *_g_mime_message_delivery_status_get_private (GMimeMessageDeliveryStatus *object) G_GNUC_CONST;
#endif

typedef struct {
	GMimeHeaderList *fields;
} GMimeMessageDispositionNotificationPrivate;

#ifdef GMIME_PRIVATE_TESTS
static inline GMimeMessageDispositionNotificationPrivate *
_g_mime_message_disposition_notification_get_private (GMimeMessageDispositionNotification *object)
{
	return g_type_instance_get_private ((GTypeInstance *) object, GMIME_TYPE_MESSAGE_DISPOSITION_NOTIFICATION);
}
#else
G_GNUC_INTERNAL GMimeMessageDispositionNotificationPrivate *_g_mime_message_disposition_notification_get_private (GMimeMessageDispositionNotification *object) G_GNUC_CONST;
#endif

typedef struct {
	GMimeHeaderList *fields;
} GMimeMessageFeedbackReportPrivate;

#ifdef GMIME_PRIVATE_TESTS
static inline GMimeMessageFeedbackReportPrivate *
_g_mime_message_feedback_report_get_private (GMimeMessageFeedbackReport *object)
{
	return g_type_instance_get_private ((GTypeInstance *) object, GMIME_TYPE_MESSAGE_FEEDBACK_REPORT);
}
#else
G_GNUC_INTERNAL GMimeMessageFeedbackReportPrivate *_g_mime_message_feedback_report_get_private (GMimeMessageFeedbackReport *object) G_GNUC_CONST;
#endif

struct _GMimeMessageIdList {
	GObject parent_instance;

	GPtrArray *array;
	gpointer changed;
};

typedef struct {
	GMimeMessage *message;
} GMimeMessagePartPrivate;

#ifdef GMIME_PRIVATE_TESTS
static inline GMimeMessagePartPrivate *
_g_mime_message_part_get_private (GMimeMessagePart *object)
{
	return g_type_instance_get_private ((GTypeInstance *) object, GMIME_TYPE_MESSAGE_PART);
}
#else
G_GNUC_INTERNAL GMimeMessagePartPrivate *_g_mime_message_part_get_private (GMimeMessagePart *object) G_GNUC_CONST;
#endif

typedef struct {
	GBytes *raw_marker;
	char *marker;

	GMimeInternetAddressList **addrlists;
	GMimeMessageImportance importance;
	GMimeXMessagePriority x_priority;
	GMimeMessagePriority priority;

	GMimeMessageIdList *references;
	char *in_reply_to;
	char *message_id;
	GDateTime *date;
	char *subject;

	char *resent_message_id;
	GDateTime *resent_date;

	GMimeEntity *mime_part;
} GMimeMessagePrivate;

#ifdef GMIME_PRIVATE_TESTS
static inline GMimeMessagePrivate *
_g_mime_message_get_private (GMimeMessage *object)
{
	return g_type_instance_get_private ((GTypeInstance *) object, GMIME_TYPE_MESSAGE);
}
#else
G_GNUC_INTERNAL GMimeMessagePrivate *_g_mime_message_get_private (GMimeMessage *object) G_GNUC_CONST;
#endif

typedef struct {
	char *boundary;

	char *preamble;
	GBytes *raw_preamble;

	GPtrArray *children;
	GPtrArray *raw_boundaries;
	GBytes *raw_end_boundary;
	GBytes *raw_trailing_boundary;

	char *epilogue;
	GBytes *raw_epilogue;

	gboolean write_end_boundary;
	gboolean reader_model;
} GMimeMultipartPrivate;

#ifdef GMIME_PRIVATE_TESTS
static inline GMimeMultipartPrivate *
_g_mime_multipart_get_private (GMimeMultipart *object)
{
	return g_type_instance_get_private ((GTypeInstance *) object, GMIME_TYPE_MULTIPART);
}
#else
G_GNUC_INTERNAL GMimeMultipartPrivate *_g_mime_multipart_get_private (GMimeMultipart *object) G_GNUC_CONST;
#endif

struct _GMimeParam {
	GObject parent_instance;

	GMimeParamEncodingMethod method;
	gboolean always_quote;
	char *charset, *lang;
	char *name, *value;

	gpointer changed;
};

struct _GMimeParamList {
	GObject parent_instance;
	GPtrArray *array;
	gpointer changed;
};

struct _GMimeParser {
	GObject parent_instance;

	struct _GMimeParserPrivate *priv;
};

typedef struct {
	GMimeContentEncoding encoding;
	GMimeOpenPGPData openpgp;
	GMimeContent *content;

	char *content_description;
	int content_duration;
	char *content_md5;
	char *filename;

	unsigned int content_duration_loaded : 1;
	unsigned int content_description_loaded : 1;
	unsigned int encoding_loaded : 1;
	unsigned int content_md5_loaded : 1;
	unsigned int encoder_max_line_length : 10;
} GMimePartPrivate;

#ifdef GMIME_PRIVATE_TESTS
static inline GMimePartPrivate *
_g_mime_part_get_private (GMimePart *object)
{
	return g_type_instance_get_private ((GTypeInstance *) object, GMIME_TYPE_PART);
}
#else
G_GNUC_INTERNAL GMimePartPrivate *_g_mime_part_get_private (GMimePart *object) G_GNUC_CONST;
#endif

struct _GMimeReceivedClause {
	GObject parent_instance;

	GMimeReceivedClauseId id;
	char *keyword;
	char *value;

	GPtrArray *comments;
};

struct _GMimeReceived {
	GObject parent_instance;

	GPtrArray *clauses;

	GDateTime *date_time;
	char *date_time_str;
};

struct _GMimeSignature {
	GObject parent_instance;

	GMimeSignatureStatus status;
	GMimeCertificate *cert;
	GDateTime *created;
	GDateTime *expires;
};

struct _GMimeSignatureList {
	GObject parent_instance;

	GPtrArray *array;
};

typedef struct {
	struct _GMimeStreamBlockStorage *storage;
} GMimeStreamBlockPrivate;

#ifdef GMIME_PRIVATE_TESTS
static inline GMimeStreamBlockPrivate *
_g_mime_stream_block_get_private (GMimeStreamBlock *object)
{
	return g_type_instance_get_private ((GTypeInstance *) object, GMIME_TYPE_STREAM_BLOCK);
}
#else
G_GNUC_INTERNAL GMimeStreamBlockPrivate *_g_mime_stream_block_get_private (GMimeStreamBlock *object) G_GNUC_CONST;
#endif

typedef struct {
	GMimeStreamBufferMode mode;
	GMimeStream *source;

	char *buffer;
	char *bufptr;
	char *bufend;
	size_t buflen;
} GMimeStreamBufferPrivate;

#ifdef GMIME_PRIVATE_TESTS
static inline GMimeStreamBufferPrivate *
_g_mime_stream_buffer_get_private (GMimeStreamBuffer *object)
{
	return g_type_instance_get_private ((GTypeInstance *) object, GMIME_TYPE_STREAM_BUFFER);
}
#else
G_GNUC_INTERNAL GMimeStreamBufferPrivate *_g_mime_stream_buffer_get_private (GMimeStreamBuffer *object) G_GNUC_CONST;
#endif

typedef struct {
	struct _cat_node *sources;
	struct _cat_node *current;

	gboolean read_eof;
} GMimeStreamCatPrivate;

#ifdef GMIME_PRIVATE_TESTS
static inline GMimeStreamCatPrivate *
_g_mime_stream_cat_get_private (GMimeStreamCat *object)
{
	return g_type_instance_get_private ((GTypeInstance *) object, GMIME_TYPE_STREAM_CAT);
}
#else
G_GNUC_INTERNAL GMimeStreamCatPrivate *_g_mime_stream_cat_get_private (GMimeStreamCat *object) G_GNUC_CONST;
#endif

typedef struct {
	gboolean owner;
	FILE *fp;

	gboolean cached_eos;
} GMimeStreamFilePrivate;

#ifdef GMIME_PRIVATE_TESTS
static inline GMimeStreamFilePrivate *
_g_mime_stream_file_get_private (GMimeStreamFile *object)
{
	return g_type_instance_get_private ((GTypeInstance *) object, GMIME_TYPE_STREAM_FILE);
}
#else
G_GNUC_INTERNAL GMimeStreamFilePrivate *_g_mime_stream_file_get_private (GMimeStreamFile *object) G_GNUC_CONST;
#endif

typedef struct _GMimeStreamFilterPrivate GMimeStreamFilterPrivate;

#ifdef GMIME_PRIVATE_TESTS
static inline GMimeStreamFilterPrivate *
_g_mime_stream_filter_get_private (GMimeStreamFilter *object)
{
	return g_type_instance_get_private ((GTypeInstance *) object, GMIME_TYPE_STREAM_FILTER);
}
#else
G_GNUC_INTERNAL GMimeStreamFilterPrivate *_g_mime_stream_filter_get_private (GMimeStreamFilter *object) G_GNUC_CONST;
#endif

typedef struct {
	gboolean owner;
	gboolean eos;
	int fd;
} GMimeStreamFsPrivate;

#ifdef GMIME_PRIVATE_TESTS
static inline GMimeStreamFsPrivate *
_g_mime_stream_fs_get_private (GMimeStreamFs *object)
{
	return g_type_instance_get_private ((GTypeInstance *) object, GMIME_TYPE_STREAM_FS);
}
#else
G_GNUC_INTERNAL GMimeStreamFsPrivate *_g_mime_stream_fs_get_private (GMimeStreamFs *object) G_GNUC_CONST;
#endif

typedef struct {
	GOutputStream *ostream;
	GInputStream *istream;
	GFile *file;

	gboolean owner;
	gboolean eos;
} GMimeStreamGIOPrivate;

#ifdef GMIME_PRIVATE_TESTS
static inline GMimeStreamGIOPrivate *
_g_mime_stream_gio_get_private (GMimeStreamGIO *object)
{
	return g_type_instance_get_private ((GTypeInstance *) object, GMIME_TYPE_STREAM_GIO);
}
#else
G_GNUC_INTERNAL GMimeStreamGIOPrivate *_g_mime_stream_gio_get_private (GMimeStreamGIO *object) G_GNUC_CONST;
#endif

typedef struct {
	GByteArray *buffer;
	gboolean owner;

	GByteArray *shared_buffer;
} GMimeStreamMemPrivate;

#ifdef GMIME_PRIVATE_TESTS
static inline GMimeStreamMemPrivate *
_g_mime_stream_mem_get_private (GMimeStreamMem *object)
{
	return g_type_instance_get_private ((GTypeInstance *) object, GMIME_TYPE_STREAM_MEM);
}
#else
G_GNUC_INTERNAL GMimeStreamMemPrivate *_g_mime_stream_mem_get_private (GMimeStreamMem *object) G_GNUC_CONST;
#endif

typedef struct {
	gboolean owner;
	gboolean eos;

	int fd;
	int prot;
	char *map;
	size_t maplen;
} GMimeStreamMmapPrivate;

#ifdef GMIME_PRIVATE_TESTS
static inline GMimeStreamMmapPrivate *
_g_mime_stream_mmap_get_private (GMimeStreamMmap *object)
{
	return g_type_instance_get_private ((GTypeInstance *) object, GMIME_TYPE_STREAM_MMAP);
}
#else
G_GNUC_INTERNAL GMimeStreamMmapPrivate *_g_mime_stream_mmap_get_private (GMimeStreamMmap *object) G_GNUC_CONST;
#endif

typedef struct {
	size_t written;
	size_t newlines;

	gboolean count_newlines;
	gint64 measured_end;
} GMimeStreamNullPrivate;

#ifdef GMIME_PRIVATE_TESTS
static inline GMimeStreamNullPrivate *
_g_mime_stream_null_get_private (GMimeStreamNull *object)
{
	return g_type_instance_get_private ((GTypeInstance *) object, GMIME_TYPE_STREAM_NULL);
}
#else
G_GNUC_INTERNAL GMimeStreamNullPrivate *_g_mime_stream_null_get_private (GMimeStreamNull *object) G_GNUC_CONST;
#endif

typedef struct {
	gboolean owner;
	gboolean eos;
	int fd;
} GMimeStreamPipePrivate;

#ifdef GMIME_PRIVATE_TESTS
static inline GMimeStreamPipePrivate *
_g_mime_stream_pipe_get_private (GMimeStreamPipe *object)
{
	return g_type_instance_get_private ((GTypeInstance *) object, GMIME_TYPE_STREAM_PIPE);
}
#else
G_GNUC_INTERNAL GMimeStreamPipePrivate *_g_mime_stream_pipe_get_private (GMimeStreamPipe *object) G_GNUC_CONST;
#endif

typedef struct {
	GMimeStream *super_stream;

	gint64 position;
	gint64 bound_start;
	gint64 bound_end;
} GMimeStreamPrivate;

#ifdef GMIME_PRIVATE_TESTS
static inline GMimeStreamPrivate *
_g_mime_stream_get_private (GMimeStream *object)
{
	return g_type_instance_get_private ((GTypeInstance *) object, GMIME_TYPE_STREAM);
}
#else
G_GNUC_INTERNAL GMimeStreamPrivate *_g_mime_stream_get_private (GMimeStream *object) G_GNUC_CONST;
#endif

G_END_DECLS

#endif /* __GMIME_OBJECT_PRIVATE_H__ */
