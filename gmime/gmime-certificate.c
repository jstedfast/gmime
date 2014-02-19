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

#include <string.h>

#include "gmime-certificate.h"


/**
 * SECTION: gmime-certificate
 * @title: GMimeCertificate
 * @short_description: Digital certificates
 * @see_also:
 *
 * A #GMimeCertificate is an object containing useful information about a
 * digital certificate as used in signing and encrypting data.
 **/


static void g_mime_certificate_class_init (GMimeCertificateClass *klass);
static void g_mime_certificate_init (GMimeCertificate *cert, GMimeCertificateClass *klass);
static void g_mime_certificate_finalize (GObject *object);

static GObjectClass *parent_class = NULL;


GType
g_mime_certificate_get_type (void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof (GMimeCertificateClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc) g_mime_certificate_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (GMimeCertificate),
			0,    /* n_preallocs */
			(GInstanceInitFunc) g_mime_certificate_init,
		};
		
		type = g_type_register_static (G_TYPE_OBJECT, "GMimeCertificate", &info, 0);
	}
	
	return type;
}

static void
g_mime_certificate_class_init (GMimeCertificateClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	
	parent_class = g_type_class_ref (G_TYPE_OBJECT);
	
	object_class->finalize = g_mime_certificate_finalize;
}

static void
g_mime_certificate_init (GMimeCertificate *cert, GMimeCertificateClass *klass)
{
	cert->pubkey_algo = GMIME_PUBKEY_ALGO_DEFAULT;
	cert->digest_algo = GMIME_DIGEST_ALGO_DEFAULT;
	cert->trust = GMIME_CERTIFICATE_TRUST_NONE;
	cert->issuer_serial = NULL;
	cert->issuer_name = NULL;
	cert->fingerprint = NULL;
	cert->created = (time_t) -1;
	cert->expires = (time_t) -1;
	cert->keyid = NULL;
	cert->email = NULL;
	cert->name = NULL;
}

static void
g_mime_certificate_finalize (GObject *object)
{
	GMimeCertificate *cert = (GMimeCertificate *) object;
	
	g_free (cert->issuer_serial);
	g_free (cert->issuer_name);
	g_free (cert->fingerprint);
	g_free (cert->keyid);
	g_free (cert->email);
	g_free (cert->name);
	
	G_OBJECT_CLASS (parent_class)->finalize (object);
}


/**
 * g_mime_certificate_new:
 *
 * Creates a new #GMimeCertificate object.
 * 
 * Returns: a new #GMimeCertificate object.
 **/
GMimeCertificate *
g_mime_certificate_new (void)
{
	return g_object_newv (GMIME_TYPE_CERTIFICATE, 0, NULL);
}


/**
 * g_mime_certificate_set_trust:
 * @cert: a #GMimeCertificate
 * @trust: a #GMimeCertificateTrust value
 *
 * Set the certificate trust.
 **/
void
g_mime_certificate_set_trust (GMimeCertificate *cert, GMimeCertificateTrust trust)
{
	g_return_if_fail (GMIME_IS_CERTIFICATE (cert));
	
	cert->trust = trust;
}


/**
 * g_mime_certificate_get_trust:
 * @cert: a #GMimeCertificate
 *
 * Get the certificate trust.
 *
 * Returns: the certificate trust.
 **/
GMimeCertificateTrust
g_mime_certificate_get_trust (GMimeCertificate *cert)
{
	g_return_val_if_fail (GMIME_IS_CERTIFICATE (cert), GMIME_CERTIFICATE_TRUST_NONE);
	
	return cert->trust;
}


/**
 * g_mime_certificate_set_pubkey_algo:
 * @cert: a #GMimeCertificate
 * @algo: a #GMimePubKeyAlgo
 *
 * Set the public-key algorithm used by the certificate.
 **/
void
g_mime_certificate_set_pubkey_algo (GMimeCertificate *cert, GMimePubKeyAlgo algo)
{
	g_return_if_fail (GMIME_IS_CERTIFICATE (cert));
	
	cert->pubkey_algo = algo;
}


/**
 * g_mime_certificate_get_pubkey_algo:
 * @cert: a #GMimeCertificate
 *
 * Get the public-key algorithm used by the certificate.
 *
 * Returns: the public-key algorithm used by the certificate or
 * #GMIME_PUBKEY_ALGO_DEFAULT if unspecified.
 **/
GMimePubKeyAlgo
g_mime_certificate_get_pubkey_algo (GMimeCertificate *cert)
{
	g_return_val_if_fail (GMIME_IS_CERTIFICATE (cert), GMIME_PUBKEY_ALGO_DEFAULT);
	
	return cert->pubkey_algo;
}


/**
 * g_mime_certificate_set_digest_algo:
 * @cert: a #GMimeCertificate
 * @algo: a #GMimeDigestAlgo
 *
 * Set the digest algorithm used by the certificate.
 **/
void
g_mime_certificate_set_digest_algo (GMimeCertificate *cert, GMimeDigestAlgo algo)
{
	g_return_if_fail (GMIME_IS_CERTIFICATE (cert));
	
	cert->digest_algo = algo;
}


/**
 * g_mime_certificate_get_digest_algo:
 * @cert: a #GMimeCertificate
 *
 * Get the digest algorithm used by the certificate.
 *
 * Returns: the digest algorithm used by the certificate or
 * #GMIME_DIGEST_ALGO_DEFAULT if unspecified.
 **/
GMimeDigestAlgo
g_mime_certificate_get_digest_algo (GMimeCertificate *cert)
{
	g_return_val_if_fail (GMIME_IS_CERTIFICATE (cert), GMIME_DIGEST_ALGO_DEFAULT);
	
	return cert->digest_algo;
}


/**
 * g_mime_certificate_set_issuer_serial:
 * @cert: a #GMimeCertificate
 * @issuer_serial: certificate's issuer serial
 *
 * Set the certificate's issuer serial.
 **/
void
g_mime_certificate_set_issuer_serial (GMimeCertificate *cert, const char *issuer_serial)
{
	g_return_if_fail (GMIME_IS_CERTIFICATE (cert));
	
	g_free (cert->issuer_serial);
	cert->issuer_serial = g_strdup (issuer_serial);
}


/**
 * g_mime_certificate_get_issuer_serial:
 * @cert: a #GMimeCertificate
 *
 * Get the certificate's issuer serial.
 *
 * Returns: the certificate's issuer serial or %NULL if unspecified.
 **/
const char *
g_mime_certificate_get_issuer_serial (GMimeCertificate *cert)
{
	g_return_val_if_fail (GMIME_IS_CERTIFICATE (cert), NULL);
	
	return cert->issuer_serial;
}


/**
 * g_mime_certificate_set_issuer_name:
 * @cert: a #GMimeCertificate
 * @issuer_name: certificate's issuer name
 *
 * Set the certificate's issuer name.
 **/
void
g_mime_certificate_set_issuer_name (GMimeCertificate *cert, const char *issuer_name)
{
	g_return_if_fail (GMIME_IS_CERTIFICATE (cert));
	
	g_free (cert->issuer_name);
	cert->issuer_name = g_strdup (issuer_name);
}


/**
 * g_mime_certificate_get_issuer_name:
 * @cert: a #GMimeCertificate
 *
 * Get the certificate's issuer name.
 *
 * Returns: the certificate's issuer name or %NULL if unspecified.
 **/
const char *
g_mime_certificate_get_issuer_name (GMimeCertificate *cert)
{
	g_return_val_if_fail (GMIME_IS_CERTIFICATE (cert), NULL);
	
	return cert->issuer_name;
}


/**
 * g_mime_certificate_set_fingerprint:
 * @cert: a #GMimeCertificate
 * @fingerprint: fingerprint string
 *
 * Set the certificate's key fingerprint.
 **/
void
g_mime_certificate_set_fingerprint (GMimeCertificate *cert, const char *fingerprint)
{
	g_return_if_fail (GMIME_IS_CERTIFICATE (cert));
	
	g_free (cert->fingerprint);
	cert->fingerprint = g_strdup (fingerprint);
}


/**
 * g_mime_certificate_get_fingerprint:
 * @cert: a #GMimeCertificate
 *
 * Get the certificate's key fingerprint.
 *
 * Returns: the certificate's key fingerprint or %NULL if unspecified.
 **/
const char *
g_mime_certificate_get_fingerprint (GMimeCertificate *cert)
{
	g_return_val_if_fail (GMIME_IS_CERTIFICATE (cert), NULL);
	
	return cert->fingerprint;
}


/**
 * g_mime_certificate_set_key_id:
 * @cert: a #GMimeCertificate
 * @key_id: key id
 *
 * Set the certificate's key id.
 **/
void
g_mime_certificate_set_key_id (GMimeCertificate *cert, const char *key_id)
{
	g_return_if_fail (GMIME_IS_CERTIFICATE (cert));
	
	g_free (cert->keyid);
	cert->keyid = g_strdup (key_id);
}


/**
 * g_mime_certificate_get_key_id:
 * @cert: a #GMimeCertificate
 *
 * Get the certificate's key id.
 *
 * Returns: the certificate's key id or %NULL if unspecified.
 **/
const char *
g_mime_certificate_get_key_id (GMimeCertificate *cert)
{
	g_return_val_if_fail (GMIME_IS_CERTIFICATE (cert), NULL);
	
	return cert->keyid;
}


/**
 * g_mime_certificate_set_email:
 * @cert: a #GMimeCertificate
 * @email: certificate's email
 *
 * Set the certificate's email.
 **/
void
g_mime_certificate_set_email (GMimeCertificate *cert, const char *email)
{
	g_return_if_fail (GMIME_IS_CERTIFICATE (cert));
	
	g_free (cert->email);
	cert->email = g_strdup (email);
}


/**
 * g_mime_certificate_get_email:
 * @cert: a #GMimeCertificate
 *
 * Get the certificate's email.
 *
 * Returns: the certificate's email or %NULL if unspecified.
 **/
const char *
g_mime_certificate_get_email (GMimeCertificate *cert)
{
	g_return_val_if_fail (GMIME_IS_CERTIFICATE (cert), NULL);
	
	return cert->email;
}


/**
 * g_mime_certificate_set_name:
 * @cert: a #GMimeCertificate
 * @name: certificate's name
 *
 * Set the certificate's name.
 **/
void
g_mime_certificate_set_name (GMimeCertificate *cert, const char *name)
{
	g_return_if_fail (GMIME_IS_CERTIFICATE (cert));
	
	g_free (cert->name);
	cert->name = g_strdup (name);
}


/**
 * g_mime_certificate_get_name:
 * @cert: a #GMimeCertificate
 *
 * Get the certificate's name.
 *
 * Returns: the certificate's name or %NULL if unspecified.
 **/
const char *
g_mime_certificate_get_name (GMimeCertificate *cert)
{
	g_return_val_if_fail (GMIME_IS_CERTIFICATE (cert), NULL);
	
	return cert->name;
}


/**
 * g_mime_certificate_set_created:
 * @cert: a #GMimeCertificate
 * @created: creation date
 *
 * Set the creation date of the certificate's key.
 **/
void
g_mime_certificate_set_created (GMimeCertificate *cert, time_t created)
{
	g_return_if_fail (GMIME_IS_CERTIFICATE (cert));
	
	cert->created = created;
}


/**
 * g_mime_certificate_get_created:
 * @cert: a #GMimeCertificate
 *
 * Get the creation date of the certificate's key.
 *
 * Returns: the creation date of the certificate's key or %-1 if unknown.
 **/
time_t
g_mime_certificate_get_created (GMimeCertificate *cert)
{
	g_return_val_if_fail (GMIME_IS_CERTIFICATE (cert), (time_t) -1);
	
	return cert->created;
}


/**
 * g_mime_certificate_set_expires:
 * @cert: a #GMimeCertificate
 * @expires: expiration date
 *
 * Set the expiration date of the certificate's key.
 **/
void
g_mime_certificate_set_expires (GMimeCertificate *cert, time_t expires)
{
	g_return_if_fail (GMIME_IS_CERTIFICATE (cert));
	
	cert->expires = expires;
}


/**
 * g_mime_certificate_get_expires:
 * @cert: a #GMimeCertificate
 *
 * Get the expiration date of the certificate's key.
 *
 * Returns: the expiration date of the certificate's key or %-1 if unknown.
 **/
time_t
g_mime_certificate_get_expires (GMimeCertificate *cert)
{
	g_return_val_if_fail (GMIME_IS_CERTIFICATE (cert), (time_t) -1);
	
	return cert->expires;
}


static void g_mime_certificate_list_class_init (GMimeCertificateListClass *klass);
static void g_mime_certificate_list_init (GMimeCertificateList *list, GMimeCertificateListClass *klass);
static void g_mime_certificate_list_finalize (GObject *object);


static GObjectClass *list_parent_class = NULL;


GType
g_mime_certificate_list_get_type (void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof (GMimeCertificateListClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc) g_mime_certificate_list_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (GMimeCertificateList),
			0,    /* n_preallocs */
			(GInstanceInitFunc) g_mime_certificate_list_init,
		};
		
		type = g_type_register_static (G_TYPE_OBJECT, "GMimeCertificateList", &info, 0);
	}
	
	return type;
}


static void
g_mime_certificate_list_class_init (GMimeCertificateListClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	
	list_parent_class = g_type_class_ref (G_TYPE_OBJECT);
	
	object_class->finalize = g_mime_certificate_list_finalize;
}

static void
g_mime_certificate_list_init (GMimeCertificateList *list, GMimeCertificateListClass *klass)
{
	list->array = g_ptr_array_new ();
}

static void
g_mime_certificate_list_finalize (GObject *object)
{
	GMimeCertificateList *list = (GMimeCertificateList *) object;
	GMimeCertificate *cert;
	guint i;
	
	for (i = 0; i < list->array->len; i++) {
		cert = (GMimeCertificate *) list->array->pdata[i];
		g_object_unref (cert);
	}
	
	g_ptr_array_free (list->array, TRUE);
	
	G_OBJECT_CLASS (list_parent_class)->finalize (object);
}


/**
 * g_mime_certificate_list_new:
 *
 * Creates a new #GMimeCertificateList.
 *
 * Returns: a new #GMimeCertificateList.
 **/
GMimeCertificateList *
g_mime_certificate_list_new (void)
{
	return g_object_newv (GMIME_TYPE_CERTIFICATE_LIST, 0, NULL);
}


/**
 * g_mime_certificate_list_length:
 * @list: a #GMimeCertificateList
 *
 * Gets the length of the list.
 *
 * Returns: the number of #GMimeCertificate objects in the list.
 **/
int
g_mime_certificate_list_length (GMimeCertificateList *list)
{
	g_return_val_if_fail (GMIME_IS_CERTIFICATE_LIST (list), -1);
	
	return list->array->len;
}


/**
 * g_mime_certificate_list_clear:
 * @list: a #GMimeCertificateList
 *
 * Clears the list of addresses.
 **/
void
g_mime_certificate_list_clear (GMimeCertificateList *list)
{
	GMimeCertificate *cert;
	guint i;
	
	g_return_if_fail (GMIME_IS_CERTIFICATE_LIST (list));
	
	for (i = 0; i < list->array->len; i++) {
		cert = (GMimeCertificate *) list->array->pdata[i];
		g_object_unref (cert);
	}
	
	g_ptr_array_set_size (list->array, 0);
}


/**
 * g_mime_certificate_list_add:
 * @list: a #GMimeCertificateList
 * @cert: a #GMimeCertificate
 *
 * Adds a #GMimeCertificate to the #GMimeCertificateList.
 *
 * Returns: the index of the added #GMimeCertificate.
 **/
int
g_mime_certificate_list_add (GMimeCertificateList *list, GMimeCertificate *cert)
{
	int index;
	
	g_return_val_if_fail (GMIME_IS_CERTIFICATE_LIST (list), -1);
	g_return_val_if_fail (GMIME_IS_CERTIFICATE (cert), -1);
	
	index = list->array->len;
	g_ptr_array_add (list->array, cert);
	g_object_ref (cert);
	
	return index;
}


/**
 * g_mime_certificate_list_insert:
 * @list: a #GMimeCertificateList
 * @index: index to insert at
 * @cert: a #GMimeCertificate
 *
 * Inserts a #GMimeCertificate into the #GMimeCertificateList at the specified
 * index.
 **/
void
g_mime_certificate_list_insert (GMimeCertificateList *list, int index, GMimeCertificate *cert)
{
	char *dest, *src;
	size_t n;
	
	g_return_if_fail (GMIME_IS_CERTIFICATE_LIST (list));
	g_return_if_fail (GMIME_IS_CERTIFICATE (cert));
	g_return_if_fail (index >= 0);
	
	if ((guint) index < list->array->len) {
		g_ptr_array_set_size (list->array, list->array->len + 1);
		
		dest = ((char *) list->array->pdata) + (sizeof (void *) * (index + 1));
		src = ((char *) list->array->pdata) + (sizeof (void *) * index);
		n = list->array->len - index - 1;
		
		g_memmove (dest, src, (sizeof (void *) * n));
		list->array->pdata[index] = cert;
	} else {
		/* the easy case */
		g_ptr_array_add (list->array, cert);
	}
	
	g_object_ref (cert);
}


/**
 * g_mime_certificate_list_remove:
 * @list: a #GMimeCertificateList
 * @cert: a #GMimeCertificate
 *
 * Removes a #GMimeCertificate from the #GMimeCertificateList.
 *
 * Returns: %TRUE if the specified #GMimeCertificate was removed or %FALSE
 * otherwise.
 **/
gboolean
g_mime_certificate_list_remove (GMimeCertificateList *list, GMimeCertificate *cert)
{
	int index;
	
	g_return_val_if_fail (GMIME_IS_CERTIFICATE_LIST (list), FALSE);
	g_return_val_if_fail (GMIME_IS_CERTIFICATE (cert), FALSE);
	
	if ((index = g_mime_certificate_list_index_of (list, cert)) == -1)
		return FALSE;
	
	g_mime_certificate_list_remove_at (list, index);
	
	return TRUE;
}


/**
 * g_mime_certificate_list_remove_at:
 * @list: a #GMimeCertificateList
 * @index: index to remove
 *
 * Removes a #GMimeCertificate from the #GMimeCertificateList at the specified
 * index.
 *
 * Returns: %TRUE if an #GMimeCertificate was removed or %FALSE otherwise.
 **/
gboolean
g_mime_certificate_list_remove_at (GMimeCertificateList *list, int index)
{
	GMimeCertificate *cert;
	
	g_return_val_if_fail (GMIME_IS_CERTIFICATE_LIST (list), FALSE);
	g_return_val_if_fail (index >= 0, FALSE);
	
	if ((guint) index >= list->array->len)
		return FALSE;
	
	cert = list->array->pdata[index];
	g_ptr_array_remove_index (list->array, index);
	g_object_unref (cert);
	
	return TRUE;
}


/**
 * g_mime_certificate_list_contains:
 * @list: a #GMimeCertificateList
 * @cert: a #GMimeCertificate
 *
 * Checks whether or not the specified #GMimeCertificate is contained within
 * the #GMimeCertificateList.
 *
 * Returns: %TRUE if the specified #GMimeCertificate is contained within the
 * specified #GMimeCertificateList or %FALSE otherwise.
 **/
gboolean
g_mime_certificate_list_contains (GMimeCertificateList *list, GMimeCertificate *cert)
{
	return g_mime_certificate_list_index_of (list, cert) != -1;
}


/**
 * g_mime_certificate_list_index_of:
 * @list: a #GMimeCertificateList
 * @cert: a #GMimeCertificate
 *
 * Gets the index of the specified #GMimeCertificate inside the
 * #GMimeCertificateList.
 *
 * Returns: the index of the requested #GMimeCertificate within the
 * #GMimeCertificateList or %-1 if it is not contained within the
 * #GMimeCertificateList.
 **/
int
g_mime_certificate_list_index_of (GMimeCertificateList *list, GMimeCertificate *cert)
{
	guint i;
	
	g_return_val_if_fail (GMIME_IS_CERTIFICATE_LIST (list), -1);
	g_return_val_if_fail (GMIME_IS_CERTIFICATE (cert), -1);
	
	for (i = 0; i < list->array->len; i++) {
		if (list->array->pdata[i] == cert)
			return i;
	}
	
	return -1;
}


/**
 * g_mime_certificate_list_get_certificate:
 * @list: a #GMimeCertificateList
 * @index: index of #GMimeCertificate to get
 *
 * Gets the #GMimeCertificate at the specified index.
 *
 * Returns: (transfer full): the #GMimeCertificate at the specified
 * index or %NULL if the index is out of range.
 **/
GMimeCertificate *
g_mime_certificate_list_get_certificate (GMimeCertificateList *list, int index)
{
	g_return_val_if_fail (GMIME_IS_CERTIFICATE_LIST (list), NULL);
	g_return_val_if_fail (index >= 0, NULL);
	
	if ((guint) index >= list->array->len)
		return NULL;
	
	return list->array->pdata[index];
}


/**
 * g_mime_certificate_list_set_certificate:
 * @list: a #GMimeCertificateList
 * @index: index of #GMimeCertificate to set
 * @cert: a #GMimeCertificate
 *
 * Sets the #GMimeCertificate at the specified index to @cert.
 **/
void
g_mime_certificate_list_set_certificate (GMimeCertificateList *list, int index, GMimeCertificate *cert)
{
	GMimeCertificate *old;
	
	g_return_if_fail (GMIME_IS_CERTIFICATE_LIST (list));
	g_return_if_fail (GMIME_IS_CERTIFICATE (cert));
	g_return_if_fail (index >= 0);
	
	if ((guint) index > list->array->len)
		return;
	
	if ((guint) index == list->array->len) {
		g_mime_certificate_list_add (list, cert);
		return;
	}
	
	if ((old = list->array->pdata[index]) == cert)
		return;
	
	list->array->pdata[index] = cert;
	g_object_unref (old);
	g_object_ref (cert);
}
