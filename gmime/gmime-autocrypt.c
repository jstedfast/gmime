/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMimeAutocrypt
 *  Copyright (C) 2017 Daniel Kahn Gillmor
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

#include "gmime-autocrypt.h"

/**
 * SECTION: gmime-autocrypt-header
 * @title: GMimeAutocryptHeader
 * @short_description: A header containing cryptographic information about the sending address
 * @see_also: https://autocrypt.org
 *
 * A #GMimeAutocryptHeader is an object containing information derived
 * from a message about the sender's cryptographic keys and
 * preferences.  It can be used in conjunction with local storage and
 * business logic to make a better user experience for encrypted
 * e-mail.
 **/


static void g_mime_autocrypt_header_class_init (GMimeAutocryptHeaderClass *klass);
static void g_mime_autocrypt_header_init (GMimeAutocryptHeader *ah, GMimeAutocryptHeaderClass *klass);
static void g_mime_autocrypt_header_finalize (GObject *object);

static GObjectClass *parent_class = NULL;


GType
g_mime_autocrypt_header_get_type (void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof (GMimeAutocryptHeaderClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc) g_mime_autocrypt_header_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (GMimeAutocryptHeader),
			0,    /* n_preallocs */
			(GInstanceInitFunc) g_mime_autocrypt_header_init,
		};
		
		type = g_type_register_static (G_TYPE_OBJECT, "GMimeAutocryptHeader", &info, 0);
	}
	
	return type;
}

static void
g_mime_autocrypt_header_class_init (GMimeAutocryptHeaderClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	
	parent_class = g_type_class_ref (G_TYPE_OBJECT);
	
	object_class->finalize = g_mime_autocrypt_header_finalize;
}

static void
g_mime_autocrypt_header_init (GMimeAutocryptHeader *ah, GMimeAutocryptHeaderClass *klass)
{
	ah->actype = 1;
	ah->address = NULL;
	ah->prefer_encrypt = GMIME_AUTOCRYPT_PREFER_ENCRYPT_NONE;
	ah->keydata = NULL;
	ah->effective_date = NULL;
}

static void
g_mime_autocrypt_header_finalize (GObject *object)
{
	GMimeAutocryptHeader *ah = (GMimeAutocryptHeader *) object;
	
	if (ah->address)
		g_object_unref (ah->address);
	if (ah->keydata)
		g_byte_array_unref (ah->keydata);
	if (ah->effective_date)
		g_date_time_unref (ah->effective_date);
	
	G_OBJECT_CLASS (parent_class)->finalize (object);
}


/**
 * g_mime_autocrypt_header_new:
 *
 * Creates a new #GMimeAutocryptHeader object.
 * 
 * Returns: (transfer full): a new #GMimeAutocryptHeader object.
 **/
GMimeAutocryptHeader *
g_mime_autocrypt_header_new (void)
{
	return g_object_new (GMIME_TYPE_AUTOCRYPT_HEADER, NULL);
}


/**
 * g_mime_autocrypt_header_new_from_string:
 *
 * Creates a new #GMimeAutocryptHeader object based on the value of an
 * Autocrypt: header.
 *
 * Note that this will not have an @effective_date set, since the
 * @effective_date is derived from the Date: line in the same block of
 * e-mail headers, but cannot be extracted from the raw Autocrypt:
 * header itself.
 * 
 * Returns: (transfer full): a new #GMimeAutocryptHeader object, or %NULL on error.
 **/
GMimeAutocryptHeader *
g_mime_autocrypt_header_new_from_string (const char *string)
{
	GMimeAutocryptHeader *ret = NULL;
	gchar **ksplit = NULL;
	gchar *kjoined = NULL;
	GByteArray *newkeydata = NULL;
	if (string == NULL)
		return NULL;
	
	/* TODO: this doesn't deal with quoting or RFC 2047 encoding,
	   both of which might happen to mails in transit. So this
	   could be improved.  It's also probably not currently the
	   most efficient implementation. */

	struct _attr { char *val; const char *name; size_t sz; int count; };
#define attr(n, str) struct _attr n = { .name = str, .sz = sizeof(str)-1 }
	attr(t, "type");
	attr(k, "keydata");
	attr(p, "prefer-encrypt");
	attr(a, "addr");
#undef attr
	struct _attr *attrs[] = { &t, &k, &p, &a };
	
	gchar **vals = g_strsplit (string, ";", -1);
	gchar **x;
	for (x = vals; *x; x++) {
		gboolean known = FALSE;
		int i;
		g_strstrip (*x);
		
		for (i = 0; i < G_N_ELEMENTS(attrs); i++) {
			if (g_ascii_strncasecmp (attrs[i]->name, *x, attrs[i]->sz) == 0 &&
			    (*x)[attrs[i]->sz] == '=') {
				attrs[i]->val = (*x) + attrs[i]->sz+1;
				attrs[i]->count++;
				known = TRUE;
				break;
			}
		}
		if (!known) {
			/* skip a line that has a critical element we
			   do not understnd */
			if ((*x)[0] != '_') 
				goto done;
			
		}
	}

	if (k.count != 1)
		goto done;
	if (a.count != 1)
		goto done;
	if (t.count > 1)
		goto done;
	if (p.count > 1)
		goto done;
	gchar *endptr=NULL;
	guint64 newtype = 1;
	GMimeAutocryptPreferEncrypt newpref = GMIME_AUTOCRYPT_PREFER_ENCRYPT_NONE;

	if (t.count) {
		newtype = g_ascii_strtoull (t.val, &endptr, 10);
		if (*endptr) /* should be NULL after the unsigned int conversion */
			goto done;
		if (newtype > G_MAXINT)
			goto done;
	}
	if (p.count && (g_ascii_strcasecmp ("mutual", p.val) == 0))
		newpref = GMIME_AUTOCRYPT_PREFER_ENCRYPT_MUTUAL;
	
	ret = g_object_new (GMIME_TYPE_AUTOCRYPT_HEADER, NULL);
	g_mime_autocrypt_header_set_address_from_string (ret, a.val);
	g_mime_autocrypt_header_set_actype (ret, newtype);
	g_mime_autocrypt_header_set_prefer_encrypt (ret, newpref);
	ksplit = g_strsplit_set (k.val, " \r\n\t", -1);
	kjoined = g_strjoinv ("", ksplit);
	gsize decodedlen = 0;
	g_base64_decode_inplace (kjoined, &decodedlen);
	newkeydata = g_byte_array_new_take (kjoined, decodedlen);
	kjoined = NULL;
	g_mime_autocrypt_header_set_keydata (ret, newkeydata);
	
 done:
	if (vals)
		g_strfreev (vals);
	if (ksplit)
		g_strfreev (ksplit);
	if (newkeydata)
		g_byte_array_unref (newkeydata);
	g_free (kjoined);
	return ret;
}


/**
 * g_mime_autocrypt_header_set_actype:
 * @ah: a #GMimeAutocryptHeader object
 * @type: an integer value associated with the type
 *
 * Set the type associated with the Autocrypt header.
 **/
void
g_mime_autocrypt_header_set_actype (GMimeAutocryptHeader *ah, gint actype)
{
	g_return_if_fail (GMIME_IS_AUTOCRYPT_HEADER (ah));

	ah->actype = actype;
}

/**
 * g_mime_autocrypt_header_get_actype:
 * @ah: a #GMimeAutocryptHeader object
 *
 * Gets the type of the Autocrypt header.
 *
 * Returns: the type of the Autocrypt header
 **/
gint
g_mime_autocrypt_header_get_actype (GMimeAutocryptHeader *ah)
{
	g_return_val_if_fail (GMIME_IS_AUTOCRYPT_HEADER (ah), 0);

	return ah->actype;
}


/**
 * g_mime_autocrypt_header_set_address_from_string:
 * @ah: a #GMimeAutocryptHeader object
 * @address: a %NULL-terminated string that is a raw e-mail address
 *
 * Set the address associated with the autocrypt_header.
 **/
void
g_mime_autocrypt_header_set_address_from_string (GMimeAutocryptHeader *ah, const char *address)
{
	g_return_if_fail (GMIME_IS_AUTOCRYPT_HEADER (ah));

	if (ah->address)
		g_object_unref (ah->address);
	ah->address = INTERNET_ADDRESS_MAILBOX(internet_address_mailbox_new (NULL, address));
}

/**
 * g_mime_autocrypt_header_set_address:
 * @ah: a #GMimeAutocryptHeader object
 * @address: a #InternetAddressMailbox value
 *
 * Set the address associated with the autocrypt_header.
 **/
void
g_mime_autocrypt_header_set_address (GMimeAutocryptHeader *ah, InternetAddressMailbox *address)
{
	g_return_if_fail (GMIME_IS_AUTOCRYPT_HEADER (ah));

	if (ah->address)
		g_object_unref (ah->address);
	
	ah->address = address;
	g_object_ref (address);
}

/**
 * g_mime_autocrypt_header_get_address:
 * @ah: a #GMimeAutocryptHeader object
 *
 * Gets the internal address of the Autocrypt header, or %NULL if not set.
 *
 * Returns: (transfer none): the address associated with the Autocrypt header
 **/
InternetAddressMailbox*
g_mime_autocrypt_header_get_address (GMimeAutocryptHeader *ah)
{
	g_return_val_if_fail (GMIME_IS_AUTOCRYPT_HEADER (ah), NULL);

	return ah->address;
}


/**
 * g_mime_autocrypt_header_get_address_as_string:
 * @ah: a #GMimeAutocryptHeader object
 *
 * Gets the internal address of the Autocrypt header as a C string, or %NULL if not set.
 *
 * Returns: (transfer none): the address associated with the Autocrypt header
 **/
const char*
g_mime_autocrypt_header_get_address_as_string (GMimeAutocryptHeader *ah)
{
	g_return_val_if_fail (GMIME_IS_AUTOCRYPT_HEADER (ah), NULL);

	return ah->address->addr;
}


/**
 * g_mime_autocrypt_header_set_prefer_encrypt:
 * @ah: a #GMimeAutocryptHeader object
 * @pref: a #GMimeAutocryptPreferEncrypt value
 *
 * Set the encryption preference associated with the Autocrypt header.
 **/
void
g_mime_autocrypt_header_set_prefer_encrypt (GMimeAutocryptHeader *ah, GMimeAutocryptPreferEncrypt pref)
{
	g_return_if_fail (GMIME_IS_AUTOCRYPT_HEADER (ah));

	ah->prefer_encrypt = pref;
}

/**
 * g_mime_autocrypt_header_get_prefer_encrypt:
 * @ah: a #GMimeAutocryptHeader object
 *
 * Gets the encryption preference stated by the Autocrypt header.
 *
 * Returns: the encryption preference associated with the Autocrypt header
 **/
GMimeAutocryptPreferEncrypt
g_mime_autocrypt_header_get_prefer_encrypt (GMimeAutocryptHeader *ah)
{
	g_return_val_if_fail (GMIME_IS_AUTOCRYPT_HEADER (ah), GMIME_AUTOCRYPT_PREFER_ENCRYPT_NONE);

	return ah->prefer_encrypt;
}


/**
 * g_mime_autocrypt_header_set_keydata:
 * @ah: a #GMimeAutocryptHeader object
 * @keydata: a #GByteArray object
 *
 * Set the raw key data associated with the Autocrypt header.
 **/
void
g_mime_autocrypt_header_set_keydata (GMimeAutocryptHeader *ah, GByteArray *keydata)
{
	g_return_if_fail (GMIME_IS_AUTOCRYPT_HEADER (ah));

	if (ah->keydata)
		g_byte_array_unref (ah->keydata);
	
	ah->keydata = keydata;
	if (keydata)
		g_byte_array_ref (keydata);
}

/**
 * g_mime_autocrypt_header_get_keydata:
 * @ah: a #GMimeAutocryptHeader object
 *
 * Gets the raw keydata of the Autocrypt header, or %NULL if not set.
 *
 * Returns: (transfer none): the raw key data associated with the Autocrypt header
 **/
GByteArray*
g_mime_autocrypt_header_get_keydata (GMimeAutocryptHeader *ah)
{
	g_return_val_if_fail (GMIME_IS_AUTOCRYPT_HEADER (ah), NULL);

	return ah->keydata;
}


/**
 * g_mime_autocrypt_header_set_effective_date:
 * @ah: a #GMimeAutocryptHeader object
 * @effective_date: a #GDateTime object
 *
 * Set the effective date associated with the Autocrypt header.
 **/
void
g_mime_autocrypt_header_set_effective_date (GMimeAutocryptHeader *ah, GDateTime *effective_date)
{
	g_return_if_fail (GMIME_IS_AUTOCRYPT_HEADER (ah));

	if (ah->effective_date)
		g_date_time_unref (ah->effective_date);
	
	ah->effective_date = effective_date;
	g_date_time_ref (effective_date);
}

/**
 * g_mime_autocrypt_header_get_effective_date:
 * @ah: a #GMimeAutocryptHeader object
 *
 * Gets the effective date of the Autocrypt header, or %NULL if not set.
 *
 * Returns: (transfer none): the effective date associated with the Autocrypt header
 **/
GDateTime*
g_mime_autocrypt_header_get_effective_date (GMimeAutocryptHeader *ah)
{
	g_return_val_if_fail (GMIME_IS_AUTOCRYPT_HEADER (ah), NULL);

	return ah->effective_date;
}

/**
 * g_mime_autocrypt_header_is_complete:
 * @ah: a #GMimeAutocryptHeader object
 *
 * When dealing with Autocrypt headers derived from a message, some
 * sender addresses will not have a legitimate/complete header
 * associated with them.  When a given sender address has no complete
 * header of a specific type, it should "reset" the state of the
 * associated address.
 *
 * Returns: %TRUE if the header is complete, or %FALSE if it is incomplete.
 */
gboolean
g_mime_autocrypt_header_is_complete (GMimeAutocryptHeader *ah)
{
	g_return_val_if_fail (GMIME_IS_AUTOCRYPT_HEADER (ah), FALSE);
	return (ah->address && ah->address->addr && ah->keydata && ah->keydata->len && ah->keydata->data);
}


/**
 * g_mime_autocrypt_header_get_string:
 * @ah: a #GMimeAutocryptHeader object
 *
 * Gets the string representation of the Autocrypt header, or %NULL on
 * error.  For example, it might return:
 *
 *     type=1; prefer-encrypt=mutual; addr=bob\@example.com keydata=AAAB15BE...
 *
 * Returns: (transfer full): the string representation of the
 * Autocrypt header.
 **/
char *
g_mime_autocrypt_header_get_string (GMimeAutocryptHeader *ah)
{
	g_return_val_if_fail (GMIME_IS_AUTOCRYPT_HEADER (ah), NULL);
	if (!g_mime_autocrypt_header_is_complete (ah))
		return NULL;
	char *pe = "";
	if (ah->prefer_encrypt == GMIME_AUTOCRYPT_PREFER_ENCRYPT_MUTUAL)
		pe = "prefer-encrypt=mutual; ";
	const char *addr = internet_address_mailbox_get_addr (ah->address);
	GPtrArray *lines = g_ptr_array_new_with_free_func (g_free);

	gchar * first = g_strdup_printf("addr=%s; type=%d; %skeydata=",
					addr, ah->actype, pe);
	size_t n = strlen (first);
	const size_t maxwid = 72;
	const size_t firstline = maxwid - sizeof ("Autocrypt:");
	gsize offset = 0;
	if (n < firstline) {
		gsize firstlinekeylen = ((firstline - n)/4)*3;
		if (firstlinekeylen > ah->keydata->len)
			firstlinekeylen = ah->keydata->len;
		gchar *kdata = g_base64_encode (ah->keydata->data, firstlinekeylen);
		gchar *newfirst = g_strconcat (first, kdata, NULL);
		g_free (first);
		g_free (kdata);
		first = newfirst;
		offset = firstlinekeylen;
	}
	g_ptr_array_add (lines, first);

	while (offset < ah->keydata->len) {
		gsize newsz = MIN((maxwid/4)*3, ah->keydata->len - offset);
		g_ptr_array_add (lines, g_base64_encode (ah->keydata->data + offset, newsz));
		offset += newsz;
	}

	g_ptr_array_add (lines, NULL);

	char *ret = g_strjoinv ("\r\n ", (gchar**)(lines->pdata));
	g_ptr_array_unref (lines);
	return ret;
}

/**
 * g_mime_autocrypt_header_compare:
 * @ah1: a #GMimeAutocryptHeader object
 * @ah2: a #GMimeAutocryptHeader object
 *
 * Compare two Autocrypt Headers.  This is useful for comparison, as well as for
 * sorting headers by:
 *
 *  - address
 *  - actype
 *  - effective_date
 *  - keydata
 *  - prefer_encrypt
 *
 * Returns: -1, 0, or 1 @ah1 is less than, equal to, or greater than @ah2.
 **/
int
g_mime_autocrypt_header_compare (GMimeAutocryptHeader *ah1, GMimeAutocryptHeader *ah2)
{
	/* note that NULL values sort before non-NULL values */
	int ret;
	if ((!ah1->address || !ah1->address->addr) && (ah2->address && ah2->address->addr))
		return -1;
	if ((ah1->address && ah1->address->addr) && (!ah2->address || !ah2->address->addr))
		return 1;
	if (ah1->address && ah1->address->addr && ah2->address && ah2->address->addr) {
		ret = strcmp (ah1->address->addr, ah2->address->addr);
		if (ret)
			return ret;
	}
	
	if (ah1->actype < ah2->actype)
		return -1;
	if (ah1->actype > ah2->actype)
		return 1;
	
	if (!ah1->effective_date && ah2->effective_date)
		return -1;
	if (ah1->effective_date && !ah2->effective_date)
		return 1;
	if (ah1->effective_date && ah2->effective_date) {
		ret = g_date_time_compare (ah1->effective_date, ah2->effective_date);
		if (ret)
			return ret;
	}

	if (!ah1->keydata && ah2->keydata)
		return -1;
	if (ah1->keydata && !ah2->keydata)
		return 1;
	if (ah1->keydata && ah2->keydata) {
		if (ah1->keydata->len < ah2->keydata->len)
			return -1;
		if (ah1->keydata->len > ah2->keydata->len)
			return 1;
		ret = memcmp (ah1->keydata->data, ah2->keydata->data, ah1->keydata->len);
		if (ret)
			return ret;
	}

	if (ah1->prefer_encrypt < ah2->prefer_encrypt)
		return -1;
	if (ah1->prefer_encrypt > ah2->prefer_encrypt)
		return 1;
	return 0;
}

/**
 * g_mime_autocrypt_header_clone:
 * @dst: a #GMimeAutocryptHeader object
 * @src: a #GMimeAutocryptHeader object
 *
 * If address and type already match between @src and @dst, copy
 * keydata, prefer_encrypt, effective_date from @src to @dst.
 *
 **/
void
g_mime_autocrypt_header_clone (GMimeAutocryptHeader *dst, GMimeAutocryptHeader *src)
{
	if (dst->actype != src->actype)
		return;
	if (!dst->address || !src->address)
		return;
	if (g_strcmp0 (internet_address_mailbox_get_idn_addr (dst->address), internet_address_mailbox_get_idn_addr (src->address)))
		return;

	if (dst->keydata)
		g_byte_array_unref (dst->keydata);
	if (src->keydata) {
		g_byte_array_ref (src->keydata);
		dst->keydata = src->keydata;
	} else
		dst->keydata = NULL;

	dst->prefer_encrypt = src->prefer_encrypt;

	if (dst->effective_date)
		g_date_time_unref (dst->effective_date);
	if (src->effective_date) {
		g_date_time_ref (src->effective_date);
		dst->effective_date = src->effective_date;
	} else
		dst->effective_date = NULL;
}
