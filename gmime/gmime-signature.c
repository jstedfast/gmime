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

#include "gmime-signature.h"


/**
 * SECTION: gmime-signature
 * @title: GMimeSignature
 * @short_description: Digital signatures
 * @see_also:
 *
 * A #GMimeSignature is an object containing useful information about a
 * digital signature as used in signing and encrypting data.
 **/


static void g_mime_signature_class_init (GMimeSignatureClass *klass);
static void g_mime_signature_init (GMimeSignature *sig, GMimeSignatureClass *klass);
static void g_mime_signature_finalize (GObject *object);

static GObjectClass *parent_class = NULL;


GType
g_mime_signature_get_type (void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof (GMimeSignatureClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc) g_mime_signature_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (GMimeSignature),
			0,    /* n_preallocs */
			(GInstanceInitFunc) g_mime_signature_init,
		};
		
		type = g_type_register_static (G_TYPE_OBJECT, "GMimeSignature", &info, 0);
	}
	
	return type;
}

static void
g_mime_signature_class_init (GMimeSignatureClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	
	parent_class = g_type_class_ref (G_TYPE_OBJECT);
	
	object_class->finalize = g_mime_signature_finalize;
}

static void
g_mime_signature_init (GMimeSignature *sig, GMimeSignatureClass *klass)
{
	sig->status = GMIME_SIGNATURE_STATUS_GOOD;
	sig->errors = GMIME_SIGNATURE_ERROR_NONE;
	sig->cert = g_mime_certificate_new ();
	sig->created = (time_t) -1;
	sig->expires = (time_t) -1;
}

static void
g_mime_signature_finalize (GObject *object)
{
	GMimeSignature *sig = (GMimeSignature *) object;
	
	if (sig->cert)
		g_object_unref (sig->cert);
	
	G_OBJECT_CLASS (parent_class)->finalize (object);
}


/**
 * g_mime_signature_new:
 *
 * Creates a new #GMimeSignature object.
 * 
 * Returns: a new #GMimeSignature object.
 **/
GMimeSignature *
g_mime_signature_new (void)
{
	return g_object_newv (GMIME_TYPE_SIGNATURE, 0, NULL);
}


/**
 * g_mime_signature_set_status:
 * @sig: a #GMimeSignature
 * @status: a #GMimeSignatureStatus
 *
 * Set the status on the signature.
 **/
void
g_mime_signature_set_status (GMimeSignature *sig, GMimeSignatureStatus status)
{
	g_return_if_fail (GMIME_IS_SIGNATURE (sig));
	
	sig->status = status;
}


/**
 * g_mime_signature_get_status:
 * @sig: a #GMimeSignature
 *
 * Get the signature status.
 *
 * Returns: the signature status.
 **/
GMimeSignatureStatus
g_mime_signature_get_status (GMimeSignature *sig)
{
	g_return_val_if_fail (GMIME_IS_SIGNATURE (sig), GMIME_SIGNATURE_STATUS_BAD);
	
	return sig->status;
}


/**
 * g_mime_signature_set_errors:
 * @sig: a #GMimeSignature
 * @errors: a #GMimeSignatureError
 *
 * Set the errors on the signature.
 **/
void
g_mime_signature_set_errors (GMimeSignature *sig, GMimeSignatureError errors)
{
	g_return_if_fail (GMIME_IS_SIGNATURE (sig));
	
	sig->errors = errors;
}


/**
 * g_mime_signature_get_errors:
 * @sig: a #GMimeSignature
 *
 * Get the signature errors. If the #GMimeSignatureStatus returned from
 * g_mime_signature_get_status() is not #GMIME_SIGNATURE_STATUS_GOOD, then the
 * errors may provide a clue as to why.
 *
 * Returns: a bitfield of errors.
 **/
GMimeSignatureError
g_mime_signature_get_errors (GMimeSignature *sig)
{
	g_return_val_if_fail (GMIME_IS_SIGNATURE (sig), GMIME_SIGNATURE_ERROR_NONE);
	
	return sig->errors;
}


/**
 * g_mime_signature_set_certificate:
 * @sig: a #GMimeSignature
 * @cert: a #GMimeCertificate
 *
 * Set the signature's certificate.
 **/
void
g_mime_signature_set_certificate (GMimeSignature *sig, GMimeCertificate *cert)
{
	g_return_if_fail (GMIME_IS_SIGNATURE (sig));
	g_return_if_fail (GMIME_IS_CERTIFICATE (cert));
	
	if (sig->cert == cert)
		return;
	
	if (sig->cert != NULL)
		g_object_unref (sig->cert);
	
	if (cert != NULL)
		g_object_ref (cert);
	
	sig->cert = cert;
}


/**
 * g_mime_signature_get_certificate:
 * @sig: a #GMimeSignature
 *
 * Get the signature's certificate.
 *
 * Returns: (transfer none): the signature's certificate.
 **/
GMimeCertificate *
g_mime_signature_get_certificate (GMimeSignature *sig)
{
	g_return_val_if_fail (GMIME_IS_SIGNATURE (sig), NULL);
	
	return sig->cert;
}


/**
 * g_mime_signature_set_created:
 * @sig: a #GMimeSignature
 * @created: creation date
 *
 * Set the creation date of the signature.
 **/
void
g_mime_signature_set_created (GMimeSignature *sig, time_t created)
{
	g_return_if_fail (GMIME_IS_SIGNATURE (sig));
	
	sig->created = created;
}


/**
 * g_mime_signature_get_created:
 * @sig: a #GMimeSignature
 *
 * Get the creation date of the signature.
 *
 * Returns: the creation date of the signature or %-1 if unknown.
 **/
time_t
g_mime_signature_get_created (GMimeSignature *sig)
{
	g_return_val_if_fail (GMIME_IS_SIGNATURE (sig), (time_t) -1);
	
	return sig->created;
}


/**
 * g_mime_signature_set_expires:
 * @sig: a #GMimeSignature
 * @expires: expiration date
 *
 * Set the expiration date of the signature.
 **/
void
g_mime_signature_set_expires (GMimeSignature *sig, time_t expires)
{
	g_return_if_fail (GMIME_IS_SIGNATURE (sig));
	
	sig->expires = expires;
}


/**
 * g_mime_signature_get_expires:
 * @sig: a #GMimeSignature
 *
 * Get the expiration date of the signature.
 *
 * Returns: the expiration date of the signature or %-1 if unknown.
 **/
time_t
g_mime_signature_get_expires (GMimeSignature *sig)
{
	g_return_val_if_fail (GMIME_IS_SIGNATURE (sig), (time_t) -1);
	
	return sig->expires;
}


static void g_mime_signature_list_class_init (GMimeSignatureListClass *klass);
static void g_mime_signature_list_init (GMimeSignatureList *list, GMimeSignatureListClass *klass);
static void g_mime_signature_list_finalize (GObject *object);


static GObjectClass *list_parent_class = NULL;


GType
g_mime_signature_list_get_type (void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof (GMimeSignatureListClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc) g_mime_signature_list_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (GMimeSignatureList),
			0,    /* n_preallocs */
			(GInstanceInitFunc) g_mime_signature_list_init,
		};
		
		type = g_type_register_static (G_TYPE_OBJECT, "GMimeSignatureList", &info, 0);
	}
	
	return type;
}


static void
g_mime_signature_list_class_init (GMimeSignatureListClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	
	list_parent_class = g_type_class_ref (G_TYPE_OBJECT);
	
	object_class->finalize = g_mime_signature_list_finalize;
}

static void
g_mime_signature_list_init (GMimeSignatureList *list, GMimeSignatureListClass *klass)
{
	list->array = g_ptr_array_new ();
}

static void
g_mime_signature_list_finalize (GObject *object)
{
	GMimeSignatureList *list = (GMimeSignatureList *) object;
	GMimeSignature *sig;
	guint i;
	
	for (i = 0; i < list->array->len; i++) {
		sig = (GMimeSignature *) list->array->pdata[i];
		g_object_unref (sig);
	}
	
	g_ptr_array_free (list->array, TRUE);
	
	G_OBJECT_CLASS (list_parent_class)->finalize (object);
}


/**
 * g_mime_signature_list_new:
 *
 * Creates a new #GMimeSignatureList.
 *
 * Returns: a new #GMimeSignatureList.
 **/
GMimeSignatureList *
g_mime_signature_list_new (void)
{
	return g_object_newv (GMIME_TYPE_SIGNATURE_LIST, 0, NULL);
}


/**
 * g_mime_signature_list_length:
 * @list: a #GMimeSignatureList
 *
 * Gets the length of the list.
 *
 * Returns: the number of #GMimeSignature objects in the list.
 **/
int
g_mime_signature_list_length (GMimeSignatureList *list)
{
	g_return_val_if_fail (GMIME_IS_SIGNATURE_LIST (list), -1);
	
	return list->array->len;
}


/**
 * g_mime_signature_list_clear:
 * @list: a #GMimeSignatureList
 *
 * Clears the list of addresses.
 **/
void
g_mime_signature_list_clear (GMimeSignatureList *list)
{
	GMimeSignature *sig;
	guint i;
	
	g_return_if_fail (GMIME_IS_SIGNATURE_LIST (list));
	
	for (i = 0; i < list->array->len; i++) {
		sig = (GMimeSignature *) list->array->pdata[i];
		g_object_unref (sig);
	}
	
	g_ptr_array_set_size (list->array, 0);
}


/**
 * g_mime_signature_list_add:
 * @list: a #GMimeSignatureList
 * @sig: a #GMimeSignature
 *
 * Adds a #GMimeSignature to the #GMimeSignatureList.
 *
 * Returns: the index of the added #GMimeSignature.
 **/
int
g_mime_signature_list_add (GMimeSignatureList *list, GMimeSignature *sig)
{
	int index;
	
	g_return_val_if_fail (GMIME_IS_SIGNATURE_LIST (list), -1);
	g_return_val_if_fail (GMIME_IS_SIGNATURE (sig), -1);
	
	index = list->array->len;
	g_ptr_array_add (list->array, sig);
	g_object_ref (sig);
	
	return index;
}


/**
 * g_mime_signature_list_insert:
 * @list: a #GMimeSignatureList
 * @index: index to insert at
 * @sig: a #GMimeSignature
 *
 * Inserts a #GMimeSignature into the #GMimeSignatureList at the specified
 * index.
 **/
void
g_mime_signature_list_insert (GMimeSignatureList *list, int index, GMimeSignature *sig)
{
	char *dest, *src;
	size_t n;
	
	g_return_if_fail (GMIME_IS_SIGNATURE_LIST (list));
	g_return_if_fail (GMIME_IS_SIGNATURE (sig));
	g_return_if_fail (index >= 0);
	
	if ((guint) index < list->array->len) {
		g_ptr_array_set_size (list->array, list->array->len + 1);
		
		dest = ((char *) list->array->pdata) + (sizeof (void *) * (index + 1));
		src = ((char *) list->array->pdata) + (sizeof (void *) * index);
		n = list->array->len - index - 1;
		
		g_memmove (dest, src, (sizeof (void *) * n));
		list->array->pdata[index] = sig;
	} else {
		/* the easy case */
		g_ptr_array_add (list->array, sig);
	}
	
	g_object_ref (sig);
}


/**
 * g_mime_signature_list_remove:
 * @list: a #GMimeSignatureList
 * @sig: a #GMimeSignature
 *
 * Removes a #GMimeSignature from the #GMimeSignatureList.
 *
 * Returns: %TRUE if the specified #GMimeSignature was removed or %FALSE
 * otherwise.
 **/
gboolean
g_mime_signature_list_remove (GMimeSignatureList *list, GMimeSignature *sig)
{
	int index;
	
	g_return_val_if_fail (GMIME_IS_SIGNATURE_LIST (list), FALSE);
	g_return_val_if_fail (GMIME_IS_SIGNATURE (sig), FALSE);
	
	if ((index = g_mime_signature_list_index_of (list, sig)) == -1)
		return FALSE;
	
	g_mime_signature_list_remove_at (list, index);
	
	return TRUE;
}


/**
 * g_mime_signature_list_remove_at:
 * @list: a #GMimeSignatureList
 * @index: index to remove
 *
 * Removes a #GMimeSignature from the #GMimeSignatureList at the specified
 * index.
 *
 * Returns: %TRUE if an #GMimeSignature was removed or %FALSE otherwise.
 **/
gboolean
g_mime_signature_list_remove_at (GMimeSignatureList *list, int index)
{
	GMimeSignature *sig;
	
	g_return_val_if_fail (GMIME_IS_SIGNATURE_LIST (list), FALSE);
	g_return_val_if_fail (index >= 0, FALSE);
	
	if ((guint) index >= list->array->len)
		return FALSE;
	
	sig = list->array->pdata[index];
	g_ptr_array_remove_index (list->array, index);
	g_object_unref (sig);
	
	return TRUE;
}


/**
 * g_mime_signature_list_contains:
 * @list: a #GMimeSignatureList
 * @sig: a #GMimeSignature
 *
 * Checks whether or not the specified #GMimeSignature is contained within
 * the #GMimeSignatureList.
 *
 * Returns: %TRUE if the specified #GMimeSignature is contained within the
 * specified #GMimeSignatureList or %FALSE otherwise.
 **/
gboolean
g_mime_signature_list_contains (GMimeSignatureList *list, GMimeSignature *sig)
{
	return g_mime_signature_list_index_of (list, sig) != -1;
}


/**
 * g_mime_signature_list_index_of:
 * @list: a #GMimeSignatureList
 * @sig: a #GMimeSignature
 *
 * Gets the index of the specified #GMimeSignature inside the
 * #GMimeSignatureList.
 *
 * Returns: the index of the requested #GMimeSignature within the
 * #GMimeSignatureList or %-1 if it is not contained within the
 * #GMimeSignatureList.
 **/
int
g_mime_signature_list_index_of (GMimeSignatureList *list, GMimeSignature *sig)
{
	guint i;
	
	g_return_val_if_fail (GMIME_IS_SIGNATURE_LIST (list), -1);
	g_return_val_if_fail (GMIME_IS_SIGNATURE (sig), -1);
	
	for (i = 0; i < list->array->len; i++) {
		if (list->array->pdata[i] == sig)
			return i;
	}
	
	return -1;
}


/**
 * g_mime_signature_list_get_signature:
 * @list: a #GMimeSignatureList
 * @index: index of #GMimeSignature to get
 *
 * Gets the #GMimeSignature at the specified index.
 *
 * Returns: (transfer none): the #GMimeSignature at the specified
 * index or %NULL if the index is out of range.
 **/
GMimeSignature *
g_mime_signature_list_get_signature (GMimeSignatureList *list, int index)
{
	g_return_val_if_fail (GMIME_IS_SIGNATURE_LIST (list), NULL);
	g_return_val_if_fail (index >= 0, NULL);
	
	if ((guint) index >= list->array->len)
		return NULL;
	
	return list->array->pdata[index];
}


/**
 * g_mime_signature_list_set_signature:
 * @list: a #GMimeSignatureList
 * @index: index of #GMimeSignature to set
 * @sig: a #GMimeSignature
 *
 * Sets the #GMimeSignature at the specified index to @sig.
 **/
void
g_mime_signature_list_set_signature (GMimeSignatureList *list, int index, GMimeSignature *sig)
{
	GMimeSignature *old;
	
	g_return_if_fail (GMIME_IS_SIGNATURE_LIST (list));
	g_return_if_fail (GMIME_IS_SIGNATURE (sig));
	g_return_if_fail (index >= 0);
	
	if ((guint) index > list->array->len)
		return;
	
	if ((guint) index == list->array->len) {
		g_mime_signature_list_add (list, sig);
		return;
	}
	
	if ((old = list->array->pdata[index]) == sig)
		return;
	
	list->array->pdata[index] = sig;
	g_object_unref (old);
	g_object_ref (sig);
}
