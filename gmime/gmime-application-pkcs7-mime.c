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

/**
 * SECTION: gmime-application-pkcs7-mime
 * @title: GMimeApplicationPkcs7Mime
 * @short_description: Pkcs7 MIME parts
 * @see_also:
 *
 * A #GMimeApplicationPkcs7Mime represents the application/pkcs7-mime MIME part.
 **/

/* GObject class methods */
static void g_mime_application_pkcs7_mime_class_init (GMimeApplicationPkcs7MimeClass *klass);
static void g_mime_application_pkcs7_mime_init (GMimeApplicationPkcs7Mime *catpart, GMimeApplicationPkcs7MimeClass *klass);
static void g_mime_application_pkcs7_mime_finalize (GObject *object);

/* GMimeObject class methods */
static void application_pkcs7_mime_prepend_header (GMimeObject *object, const char *header, const char *value, const char *raw_value, gint64 offset);
static void application_pkcs7_mime_append_header (GMimeObject *object, const char *header, const char *value, const char *raw_value, gint64 offset);
static void application_pkcs7_mime_set_header (GMimeObject *object, const char *header, const char *value, const char *raw_value, gint64 offset);
static const char *application_pkcs7_mime_get_header (GMimeObject *object, const char *header);
static gboolean application_pkcs7_mime_remove_header (GMimeObject *object, const char *header);
static void application_pkcs7_mime_set_content_type (GMimeObject *object, GMimeContentType *content_type);


static GMimePartClass *parent_class = NULL;


GType
g_mime_application_pkcs7_mime_get_type (void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof (GMimeApplicationPkcs7MimeClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc) g_mime_application_pkcs7_mime_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (GMimeApplicationPkcs7Mime),
			0,    /* n_preallocs */
			(GInstanceInitFunc) g_mime_application_pkcs7_mime_init,
		};
		
		type = g_type_register_static (GMIME_TYPE_PART, "GMimeApplicationPkcs7Mime", &info, 0);
	}
	
	return type;
}


static void
g_mime_application_pkcs7_mime_class_init (GMimeApplicationPkcs7MimeClass *klass)
{
	GMimeObjectClass *object_class = GMIME_OBJECT_CLASS (klass);
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	
	parent_class = g_type_class_ref (GMIME_TYPE_PART);
	
	gobject_class->finalize = g_mime_application_pkcs7_mime_finalize;
	
	object_class->prepend_header = application_pkcs7_mime_prepend_header;
	object_class->append_header = application_pkcs7_mime_append_header;
	object_class->remove_header = application_pkcs7_mime_remove_header;
	object_class->set_header = application_pkcs7_mime_set_header;
	object_class->get_header = application_pkcs7_mime_get_header;
	object_class->set_content_type = application_pkcs7_mime_set_content_type;
}

static void
g_mime_application_pkcs7_mime_init (GMimeApplicationPkcs7Mime *pkcs7_mime, GMimeApplicationPkcs7MimeClass *klass)
{
	pkcs7_mime->smime_type = GMIME_SECURE_MIME_TYPE_UNKNOWN;
}

static void
g_mime_application_pkcs7_mime_finalize (GObject *object)
{
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
application_pkcs7_mime_prepend_header (GMimeObject *object, const char *header, const char *value, const char *raw_value, gint64 offset)
{
	GMIME_OBJECT_CLASS (parent_class)->prepend_header (object, header, value, raw_value, offset);
}

static void
application_pkcs7_mime_append_header (GMimeObject *object, const char *header, const char *value, const char *raw_value, gint64 offset)
{
	GMIME_OBJECT_CLASS (parent_class)->append_header (object, header, value, raw_value, offset);
}

static void
application_pkcs7_mime_set_header (GMimeObject *object, const char *header, const char *value, const char *raw_value, gint64 offset)
{
	GMIME_OBJECT_CLASS (parent_class)->set_header (object, header, value, raw_value, offset);
}

static const char *
application_pkcs7_mime_get_header (GMimeObject *object, const char *header)
{
	return GMIME_OBJECT_CLASS (parent_class)->get_header (object, header);
}

static gboolean
application_pkcs7_mime_remove_header (GMimeObject *object, const char *header)
{
	return GMIME_OBJECT_CLASS (parent_class)->remove_header (object, header);
}

static void
application_pkcs7_mime_set_content_type (GMimeObject *object, GMimeContentType *content_type)
{
	GMimeApplicationPkcs7Mime *pkcs7_mime = (GMimeApplicationPkcs7Mime *) object;
	const char *value;
	
	if ((value = g_mime_content_type_get_parameter (content_type, "smime-type")) != NULL) {
		if (!g_ascii_strcasecmp (value, "compressed-data"))
			pkcs7_mime->smime_type = GMIME_SECURE_MIME_TYPE_COMPRESSED_DATA;
		else if (!g_ascii_strcasecmp (value, "enveloped-data"))
			pkcs7_mime->smime_type = GMIME_SECURE_MIME_TYPE_ENVELOPED_DATA;
		else if (!g_ascii_strcasecmp (value, "signed-data"))
			pkcs7_mime->smime_type = GMIME_SECURE_MIME_TYPE_SIGNED_DATA;
		else if (!g_ascii_strcasecmp (value, "certs-only"))
			pkcs7_mime->smime_type = GMIME_SECURE_MIME_TYPE_CERTS_ONLY;
		else
			pkcs7_mime->smime_type = GMIME_SECURE_MIME_TYPE_UNKNOWN;
	} else {
		pkcs7_mime->smime_type = GMIME_SECURE_MIME_TYPE_UNKNOWN;
	}
	
	GMIME_OBJECT_CLASS (parent_class)->set_content_type (object, content_type);
}

/**
 * g_mime_application_pkcs7_mime_new:
 * @type: The type of S/MIME data contained within the part.
 *
 * Creates a new application/pkcs7-mime object.
 *
 * Returns: an empty application/pkcs7-mime object.
 **/
GMimeApplicationPkcs7Mime *
g_mime_application_pkcs7_mime_new (GMimeSecureMimeType type)
{
	GMimeApplicationPkcs7Mime *pkcs7_mime;
	GMimeContentType *content_type;
	
	pkcs7_mime = g_object_newv (GMIME_TYPE_APPLICATION_PKCS7_MIME, 0, NULL);
	content_type = g_mime_content_type_new ("application", "pkcs7-mime");
	
	switch (type) {
	case GMIME_SECURE_MIME_TYPE_COMPRESSED_DATA:
		g_mime_content_type_set_parameter (content_type, "smime-type", "compressed-data");
		break;
	case GMIME_SECURE_MIME_TYPE_ENVELOPED_DATA:
		g_mime_content_type_set_parameter (content_type, "smime-type", "enveloped-data");
		break;
	case GMIME_SECURE_MIME_TYPE_SIGNED_DATA:
		g_mime_content_type_set_parameter (content_type, "smime-type", "signed-data");
		break;
	case GMIME_SECURE_MIME_TYPE_CERTS_ONLY:
		g_mime_content_type_set_parameter (content_type, "smime-type", "certs-only");
		break;
	default:
		break;
	}
	
	g_mime_object_set_content_type (GMIME_OBJECT (pkcs7_mime), content_type);
	g_object_unref (content_type);
	
	return pkcs7_mime;
}


/**
 * g_mime_application_pkcs7_mime_get_smime_type:
 * @pkcs7_mime: A #GMimeApplicationPkcs7Mime object
 *
 * Gets the smime-type value of the Content-Type header.
 *
 * Returns: the smime-type value.
 **/
GMimeSecureMimeType
g_mime_application_pkcs7_mime_get_smime_type (GMimeApplicationPkcs7Mime *pkcs7_mime)
{
	g_return_val_if_fail (GMIME_IS_APPLICATION_PKCS7_MIME (pkcs7_mime), GMIME_SECURE_MIME_TYPE_UNKNOWN);
	
	return pkcs7_mime->smime_type;
}
