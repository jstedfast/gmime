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


#ifndef __GMIME_FILTER_OPENPGP_H__
#define __GMIME_FILTER_OPENPGP_H__

#include <gmime/gmime-filter.h>

G_BEGIN_DECLS

#define GMIME_TYPE_FILTER_OPENPGP            (g_mime_filter_openpgp_get_type ())
#define GMIME_FILTER_OPENPGP(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GMIME_TYPE_FILTER_OPENPGP, GMimeFilterOpenPGP))
#define GMIME_FILTER_OPENPGP_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GMIME_TYPE_FILTER_OPENPGP, GMimeFilterOpenPGPClass))
#define GMIME_IS_FILTER_OPENPGP(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GMIME_TYPE_FILTER_OPENPGP))
#define GMIME_IS_FILTER_OPENPGP_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GMIME_TYPE_FILTER_OPENPGP))
#define GMIME_FILTER_OPENPGP_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GMIME_TYPE_FILTER_OPENPGP, GMimeFilterOpenPGPClass))

typedef struct _GMimeFilterOpenPGP GMimeFilterOpenPGP;
typedef struct _GMimeFilterOpenPGPClass GMimeFilterOpenPGPClass;


/**
 * GMimeOpenPGPData:
 * @GMIME_OPENPGP_DATA_NONE: No OpenPGP data found.
 * @GMIME_OPENPGP_DATA_ENCRYPTED: The content contains OpenPGP encrypted data.
 * @GMIME_OPENPGP_DATA_SIGNED: The content contains OpenPGP signed data.
 * @GMIME_OPENPGP_DATA_PUBLIC_KEY: The content contains OpenPGP public key data.
 * @GMIME_OPENPGP_DATA_PRIVATE_KEY: The content contains OpenPGP private key data.
 *
 * The type of OpenPGP data found, if any.
 **/
typedef enum {
	GMIME_OPENPGP_DATA_NONE,
	GMIME_OPENPGP_DATA_ENCRYPTED,
	GMIME_OPENPGP_DATA_SIGNED,
	GMIME_OPENPGP_DATA_PUBLIC_KEY,
	GMIME_OPENPGP_DATA_PRIVATE_KEY
} GMimeOpenPGPData;


/**
 * GMimeOpenPGPState:
 * @GMIME_OPENPGP_NONE: No OpenPGP markers have been found (yet).
 * @GMIME_OPENPGP_BEGIN_PGP_MESSAGE: The "-----BEGIN PGP MESSAGE-----" marker has been found.
 * @GMIME_OPENPGP_END_PGP_MESSAGE: The "-----END PGP MESSAGE-----" marker has been found.
 * @GMIME_OPENPGP_BEGIN_PGP_SIGNED_MESSAGE: The "-----BEGIN PGP SIGNED MESSAGE-----" marker has been found.
 * @GMIME_OPENPGP_BEGIN_PGP_SIGNATURE: The "-----BEGIN PGP SIGNATURE-----" marker has been found.
 * @GMIME_OPENPGP_END_PGP_SIGNATURE: The "-----END PGP SIGNATURE-----" marker has been found.
 * @GMIME_OPENPGP_BEGIN_PGP_PUBLIC_KEY_BLOCK: The "-----BEGIN PGP PUBLIC KEY BLOCK-----" marker has been found.
 * @GMIME_OPENPGP_END_PGP_PUBLIC_KEY_BLOCK: The "-----END PGP PUBLIC KEY BLOCK-----" marker has been found.
 * @GMIME_OPENPGP_BEGIN_PGP_PRIVATE_KEY_BLOCK: The "-----BEGIN PGP PRIVATE KEY BLOCK-----" marker has been found.
 * @GMIME_OPENPGP_END_PGP_PRIVATE_KEY_BLOCK: The "-----END PGP PRIVATE KEY BLOCK-----" marker has been found.
 *
 * The current state of the #GMimeFilterOpenPGP filter.
 *
 * Since: 3.2
 **/
typedef enum {
	GMIME_OPENPGP_NONE                        = 0,
	GMIME_OPENPGP_BEGIN_PGP_MESSAGE           = (1 << 0),
	GMIME_OPENPGP_END_PGP_MESSAGE             = (1 << 1) | (1 << 0),
	GMIME_OPENPGP_BEGIN_PGP_SIGNED_MESSAGE    = (1 << 2),
	GMIME_OPENPGP_BEGIN_PGP_SIGNATURE         = (1 << 3) | (1 << 2),
	GMIME_OPENPGP_END_PGP_SIGNATURE           = (1 << 4) | (1 << 3) | (1 << 2),
	GMIME_OPENPGP_BEGIN_PGP_PUBLIC_KEY_BLOCK  = (1 << 5),
	GMIME_OPENPGP_END_PGP_PUBLIC_KEY_BLOCK    = (1 << 6) | (1 << 5),
	GMIME_OPENPGP_BEGIN_PGP_PRIVATE_KEY_BLOCK = (1 << 7),
	GMIME_OPENPGP_END_PGP_PRIVATE_KEY_BLOCK   = (1 << 8) | (1 << 7)
} GMimeOpenPGPState;


/**
 * GMimeOpenPGPMarker:
 * @marker: The OpenPGP marker.
 * @len: The length of the OpenPGP marker.
 * @before: The #GMimeOpenPGPState that the state machine must be in before encountering this marker.
 * @after: The #GMimeOpenPGPState that the state machine will transition into once this marker is found.
 * @is_end_marker: %TRUE if the marker is an end marker; otherwise, %FALSE.
 *
 * An OpenPGP marker for use with GMime's internal state machines used for detecting OpenPGP blocks.
 *
 * Since: 3.2
 **/
typedef struct {
	const char *marker;
	size_t len;
	GMimeOpenPGPState before;
	GMimeOpenPGPState after;
	gboolean is_end_marker;
} GMimeOpenPGPMarker;


/**
 * GMimeFilterOpenPGP:
 * @parent_object: parent #GMimeFilter
 *
 * A filter to detect OpenPGP markers.
 *
 * Since: 3.2
 **/
struct _GMimeFilterOpenPGP {
	GMimeFilter parent_object;
	
	/* < private > */
	GMimeOpenPGPState state;
	gboolean seen_end_marker;
	gboolean midline;
	gint64 begin_offset;
	gint64 end_offset;
	gint64 position;
	guint next;
};

struct _GMimeFilterOpenPGPClass {
	GMimeFilterClass parent_class;
	
};


GType g_mime_filter_openpgp_get_type (void);

GMimeFilter *g_mime_filter_openpgp_new (void);

GMimeOpenPGPData g_mime_filter_openpgp_get_data_type (GMimeFilterOpenPGP *openpgp);
gint64 g_mime_filter_openpgp_get_begin_offset (GMimeFilterOpenPGP *openpgp);
gint64 g_mime_filter_openpgp_get_end_offset (GMimeFilterOpenPGP *openpgp);

G_END_DECLS

#endif /* __GMIME_FILTER_OPENPGP_H__ */
