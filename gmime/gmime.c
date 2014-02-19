/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2014 Jeffrey Stedfast
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

#include <time.h>

#ifdef ENABLE_SMIME
#include <gpgme.h>
#endif

#include "gmime.h"

#ifdef ENABLE_CRYPTOGRAPHY
#include "gmime-pkcs7-context.h"
#include "gmime-gpg-context.h"
#endif


/**
 * SECTION: gmime
 * @title: gmime
 * @short_description: Initialization, shutdown and version-check routines
 * @see_also:
 *
 * Initialization, shutdown, and version-check functions.
 **/

extern gboolean _g_mime_enable_rfc2047_workarounds (void);
extern gboolean _g_mime_use_only_user_charsets (void);

extern void g_mime_iconv_utils_shutdown (void);
extern void g_mime_iconv_utils_init (void);

extern void _g_mime_iconv_cache_unlock (void);
extern void _g_mime_iconv_cache_lock (void);
extern void _g_mime_iconv_utils_unlock (void);
extern void _g_mime_iconv_utils_lock (void);
extern void _g_mime_charset_unlock (void);
extern void _g_mime_charset_lock (void);
extern void _g_mime_msgid_unlock (void);
extern void _g_mime_msgid_lock (void);

GQuark gmime_gpgme_error_quark;
GQuark gmime_error_quark;

const guint gmime_major_version = GMIME_MAJOR_VERSION;
const guint gmime_minor_version = GMIME_MINOR_VERSION;
const guint gmime_micro_version = GMIME_MICRO_VERSION;
const guint gmime_interface_age = GMIME_INTERFACE_AGE;
const guint gmime_binary_age = GMIME_BINARY_AGE;

G_LOCK_DEFINE_STATIC (iconv_cache);
G_LOCK_DEFINE_STATIC (iconv_utils);
G_LOCK_DEFINE_STATIC (charset);
G_LOCK_DEFINE_STATIC (msgid);

static unsigned int initialized = 0;
static guint32 enable = 0;


/**
 * g_mime_check_version:
 * @major: Minimum major version
 * @minor: Minimum minor version
 * @micro: Minimum micro version
 *
 * Checks that the GMime library version meets the requirements of the
 * required version.
 *
 * Returns: %TRUE if the requirement is met or %FALSE otherwise.
 **/
gboolean
g_mime_check_version (guint major, guint minor, guint micro)
{
	if (GMIME_MAJOR_VERSION > major)
		return TRUE;
	
	if (GMIME_MAJOR_VERSION == major && GMIME_MINOR_VERSION > minor)
		return TRUE;
	
	if (GMIME_MAJOR_VERSION == major && GMIME_MINOR_VERSION == minor && GMIME_MICRO_VERSION >= micro)
		return TRUE;
	
	return FALSE;
}


/**
 * g_mime_init:
 * @flags: initialization flags
 *
 * Initializes GMime.
 *
 * Note: Calls g_mime_charset_map_init() and g_mime_iconv_init() as
 * well.
 **/
void
g_mime_init (guint32 flags)
{
	if (initialized++)
		return;
	
#if defined (HAVE_TIMEZONE) || defined (HAVE__TIMEZONE)
	/* initialize timezone */
	tzset ();
#endif
	
	enable = flags;
	
#if !GLIB_CHECK_VERSION(2, 35, 1)
	g_type_init ();
#endif
	
#ifdef G_THREADS_ENABLED
	g_mutex_init (&G_LOCK_NAME (iconv_cache));
	g_mutex_init (&G_LOCK_NAME (iconv_utils));
	g_mutex_init (&G_LOCK_NAME (charset));
	g_mutex_init (&G_LOCK_NAME (msgid));
#endif
	
	g_mime_charset_map_init ();
	g_mime_iconv_utils_init ();
	g_mime_iconv_init ();
	
#ifdef ENABLE_SMIME
	/* gpgme_check_version() initializes GpgMe */
	gpgme_check_version (NULL);
#endif /* ENABLE_SMIME */
	
	gmime_gpgme_error_quark = g_quark_from_static_string ("gmime-gpgme");
	gmime_error_quark = g_quark_from_static_string ("gmime");
	
	/* register our GObject types with the GType system */
	g_mime_crypto_context_get_type ();
	g_mime_decrypt_result_get_type ();
	g_mime_certificate_list_get_type ();
	g_mime_signature_list_get_type ();
	g_mime_certificate_get_type ();
	g_mime_signature_get_type ();
#ifdef ENABLE_CRYPTOGRAPHY
	g_mime_gpg_context_get_type ();
	g_mime_pkcs7_context_get_type ();
#endif
	
	g_mime_filter_get_type ();
	g_mime_filter_basic_get_type ();
	g_mime_filter_best_get_type ();
	g_mime_filter_charset_get_type ();
	g_mime_filter_crlf_get_type ();
	g_mime_filter_enriched_get_type ();
	g_mime_filter_from_get_type ();
	g_mime_filter_gzip_get_type ();
	g_mime_filter_html_get_type ();
	g_mime_filter_md5_get_type ();
	g_mime_filter_strip_get_type ();
	g_mime_filter_windows_get_type ();
	g_mime_filter_yenc_get_type ();
	
	g_mime_stream_get_type ();
	g_mime_stream_buffer_get_type ();
	g_mime_stream_cat_get_type ();
	g_mime_stream_file_get_type ();
	g_mime_stream_filter_get_type ();
	g_mime_stream_fs_get_type ();
	g_mime_stream_gio_get_type ();
	g_mime_stream_mem_get_type ();
	g_mime_stream_mmap_get_type ();
	g_mime_stream_null_get_type ();
	g_mime_stream_pipe_get_type ();
	
	g_mime_parser_get_type ();
	g_mime_message_get_type ();
	g_mime_data_wrapper_get_type ();
	g_mime_content_type_get_type ();
	g_mime_content_disposition_get_type ();
	
	internet_address_get_type ();
	internet_address_list_get_type ();
	internet_address_group_get_type ();
	internet_address_mailbox_get_type ();
	
	/* register our default mime object types */
	g_mime_object_type_registry_init ();
	g_mime_object_register_type ("*", "*", g_mime_part_get_type ());
	g_mime_object_register_type ("multipart", "*", g_mime_multipart_get_type ());
	g_mime_object_register_type ("multipart", "encrypted", g_mime_multipart_encrypted_get_type ());
	g_mime_object_register_type ("multipart", "signed", g_mime_multipart_signed_get_type ());
	g_mime_object_register_type ("message", "rfc822", g_mime_message_part_get_type ());
	g_mime_object_register_type ("message", "rfc2822", g_mime_message_part_get_type ());
	g_mime_object_register_type ("message", "news", g_mime_message_part_get_type ());
	g_mime_object_register_type ("message", "partial", g_mime_message_partial_get_type ());
}


/**
 * g_mime_shutdown:
 *
 * Frees internally allocated tables created in g_mime_init(). Also
 * calls g_mime_charset_map_shutdown() and g_mime_iconv_shutdown().
 **/
void
g_mime_shutdown (void)
{
	if (--initialized)
		return;
	
	g_mime_object_type_registry_shutdown ();
	g_mime_charset_map_shutdown ();
	g_mime_iconv_utils_shutdown ();
	g_mime_iconv_shutdown ();
	
#ifdef G_THREADS_ENABLED
	if (glib_check_version (2, 37, 4) == NULL) {
		/* The implementation of g_mutex_clear() prior
		 * to glib 2.37.4 did not properly reset the
		 * internal mutex pointer to NULL, so re-initializing
		 * GMime would not properly re-initialize the mutexes.
		 **/
		g_mutex_clear (&G_LOCK_NAME (iconv_cache));
		g_mutex_clear (&G_LOCK_NAME (iconv_utils));
		g_mutex_clear (&G_LOCK_NAME (charset));
		g_mutex_clear (&G_LOCK_NAME (msgid));
	}
#endif
}


gboolean
_g_mime_enable_rfc2047_workarounds (void)
{
	return (enable & GMIME_ENABLE_RFC2047_WORKAROUNDS);
}

gboolean
_g_mime_use_only_user_charsets (void)
{
	return (enable & GMIME_ENABLE_USE_ONLY_USER_CHARSETS);
}

void
_g_mime_iconv_cache_unlock (void)
{
	return G_UNLOCK (iconv_cache);
}

void
_g_mime_iconv_cache_lock (void)
{
	return G_LOCK (iconv_cache);
}

void
_g_mime_iconv_utils_unlock (void)
{
	return G_UNLOCK (iconv_utils);
}

void
_g_mime_iconv_utils_lock (void)
{
	return G_LOCK (iconv_utils);
}

void
_g_mime_charset_unlock (void)
{
	return G_UNLOCK (charset);
}

void
_g_mime_charset_lock (void)
{
	return G_LOCK (charset);
}

void
_g_mime_msgid_unlock (void)
{
	return G_UNLOCK (msgid);
}

void
_g_mime_msgid_lock (void)
{
	return G_LOCK (msgid);
}
