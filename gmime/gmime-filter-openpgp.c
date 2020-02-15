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


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "gmime-filter-openpgp.h"


/**
 * SECTION: gmime-filter-openpgp
 * @title: GMimeFilterOpenPGP
 * @short_description: Detect OpenPGP markers.
 *
 * A #GMimeFilter for detecting OpenPGP markers and filtering out any content outside the bounds of said markers.
 **/

/* Note: if you add/remove markers, update GMimeParser as well */
const GMimeOpenPGPMarker g_mime_openpgp_markers[9] = {
	{ "-----BEGIN PGP MESSAGE-----",           27, GMIME_OPENPGP_NONE,                        GMIME_OPENPGP_BEGIN_PGP_MESSAGE,           FALSE },
	{ "-----END PGP MESSAGE-----",             25, GMIME_OPENPGP_BEGIN_PGP_MESSAGE,           GMIME_OPENPGP_END_PGP_MESSAGE,             TRUE  },
	{ "-----BEGIN PGP SIGNED MESSAGE-----",    34, GMIME_OPENPGP_NONE,                        GMIME_OPENPGP_BEGIN_PGP_SIGNED_MESSAGE,    FALSE },
	{ "-----BEGIN PGP SIGNATURE-----",         29, GMIME_OPENPGP_BEGIN_PGP_SIGNED_MESSAGE,    GMIME_OPENPGP_BEGIN_PGP_SIGNATURE,         FALSE },
	{ "-----END PGP SIGNATURE-----",           27, GMIME_OPENPGP_BEGIN_PGP_SIGNATURE,         GMIME_OPENPGP_END_PGP_SIGNATURE,           TRUE  },
	{ "-----BEGIN PGP PUBLIC KEY BLOCK-----",  36, GMIME_OPENPGP_NONE,                        GMIME_OPENPGP_BEGIN_PGP_PUBLIC_KEY_BLOCK,  FALSE },
	{ "-----END PGP PUBLIC KEY BLOCK-----",    34, GMIME_OPENPGP_BEGIN_PGP_PUBLIC_KEY_BLOCK,  GMIME_OPENPGP_END_PGP_PUBLIC_KEY_BLOCK,    TRUE  },
	{ "-----BEGIN PGP PRIVATE KEY BLOCK-----", 37, GMIME_OPENPGP_NONE,                        GMIME_OPENPGP_BEGIN_PGP_PRIVATE_KEY_BLOCK, FALSE },
	{ "-----END PGP PRIVATE KEY BLOCK-----",   35, GMIME_OPENPGP_BEGIN_PGP_PRIVATE_KEY_BLOCK, GMIME_OPENPGP_END_PGP_PRIVATE_KEY_BLOCK,   TRUE  }
};

#define NUM_OPENPGP_MARKERS G_N_ELEMENTS (g_mime_openpgp_markers)

static void g_mime_filter_openpgp_class_init (GMimeFilterOpenPGPClass *klass);
static void g_mime_filter_openpgp_init (GMimeFilterOpenPGP *filter, GMimeFilterOpenPGPClass *klass);

static GMimeFilter *filter_copy (GMimeFilter *filter);
static void filter_filter (GMimeFilter *filter, char *in, size_t len, size_t prespace,
			   char **out, size_t *outlen, size_t *outprespace);
static void filter_complete (GMimeFilter *filter, char *in, size_t len, size_t prespace,
			     char **out, size_t *outlen, size_t *outprespace);
static void filter_reset (GMimeFilter *filter);


static GMimeFilterClass *parent_class = NULL;


GType
g_mime_filter_openpgp_get_type (void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof (GMimeFilterOpenPGPClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc) g_mime_filter_openpgp_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (GMimeFilterOpenPGP),
			0,    /* n_preallocs */
			(GInstanceInitFunc) g_mime_filter_openpgp_init,
		};
		
		type = g_type_register_static (GMIME_TYPE_FILTER, "GMimeFilterOpenPGP", &info, 0);
	}
	
	return type;
}


static void
g_mime_filter_openpgp_class_init (GMimeFilterOpenPGPClass *klass)
{
	GMimeFilterClass *filter_class = GMIME_FILTER_CLASS (klass);
	
	parent_class = g_type_class_ref (GMIME_TYPE_FILTER);
	
	filter_class->copy = filter_copy;
	filter_class->filter = filter_filter;
	filter_class->complete = filter_complete;
	filter_class->reset = filter_reset;
}

static void
g_mime_filter_openpgp_init (GMimeFilterOpenPGP *filter, GMimeFilterOpenPGPClass *klass)
{
	filter->state = GMIME_OPENPGP_NONE;
	filter->seen_end_marker = FALSE;
	filter->midline = FALSE;
	filter->begin_offset = -1;
	filter->end_offset = -1;
	filter->position = 0;
	filter->next = 0;
}


static GMimeFilter *
filter_copy (GMimeFilter *filter)
{
	return g_mime_filter_openpgp_new ();
}

static gboolean
is_marker (const char *input, const char *inend, const GMimeOpenPGPMarker *marker, gboolean *cr)
{
	const char *markerend = marker->marker + marker->len;
	const char *markerptr = marker->marker;
	const char *inptr = input;
	
	*cr = FALSE;
	
	while (inptr < inend && markerptr < markerend) {
		if (*inptr != *markerptr)
			return FALSE;
		
		markerptr++;
		inptr++;
	}
	
	if (markerptr < markerend)
		return FALSE;
	
	if (inptr < inend && *inptr == '\r') {
		*cr = TRUE;
		inptr++;
	}
	
	return *inptr == '\n';
}

static gboolean
is_partial_match (const char *input, const char *inend, const GMimeOpenPGPMarker *marker)
{
	const char *markerend = marker->marker + marker->len;
	const char *markerptr = marker->marker;
	const char *inptr = input;
	
	while (inptr < inend && markerptr < markerend) {
		if (*inptr != *markerptr)
			return FALSE;
		
		markerptr++;
		inptr++;
	}
	
	if (inptr < inend && *inptr == '\r')
		inptr++;
	
	return inptr == inend;
}

static void
set_position (GMimeFilterOpenPGP *openpgp, gint64 offset, guint marker, gboolean cr)
{
	size_t length = g_mime_openpgp_markers[marker].len + (cr ? 2 : 1);
	
	switch (openpgp->state) {
	case GMIME_OPENPGP_BEGIN_PGP_PRIVATE_KEY_BLOCK: openpgp->begin_offset = openpgp->position + offset; break;
	case GMIME_OPENPGP_END_PGP_PRIVATE_KEY_BLOCK: openpgp->end_offset = openpgp->position + offset + length; break;
	case GMIME_OPENPGP_BEGIN_PGP_PUBLIC_KEY_BLOCK: openpgp->begin_offset = openpgp->position + offset; break;
	case GMIME_OPENPGP_END_PGP_PUBLIC_KEY_BLOCK: openpgp->end_offset = openpgp->position + offset + length; break;
	case GMIME_OPENPGP_BEGIN_PGP_SIGNED_MESSAGE: openpgp->begin_offset = openpgp->position + offset; break;
	case GMIME_OPENPGP_END_PGP_SIGNATURE: openpgp->end_offset = openpgp->position + offset + length; break;
	case GMIME_OPENPGP_BEGIN_PGP_MESSAGE: openpgp->begin_offset = openpgp->position + offset; break;
	case GMIME_OPENPGP_END_PGP_MESSAGE: openpgp->end_offset = openpgp->position + offset + length; break;
	default: break;
	}
}

static void
scan (GMimeFilter *filter, char *inbuf, size_t inlen, size_t prespace,
      char **outbuf, size_t *outlen, size_t *outprespace, gboolean flush)
{
	GMimeFilterOpenPGP *openpgp = (GMimeFilterOpenPGP *) filter;
	const char *inend = inbuf + inlen;
	const char *inptr = inbuf;
	gboolean cr;
	guint i;
	
	*outprespace = prespace;
	*outbuf = inbuf;
	*outlen = 0;
	
	if (openpgp->seen_end_marker || inlen == 0)
		return;
	
	if (openpgp->midline) {
		while (inptr < inend && *inptr != '\n')
			inptr++;
		
		if (inptr == inend) {
			if (openpgp->state != GMIME_OPENPGP_NONE)
				*outlen = inlen;
			
			openpgp->position += inlen;
			return;
		}
		
		openpgp->midline = FALSE;
	}
	
	if (openpgp->state == GMIME_OPENPGP_NONE) {
		do {
			const char *lineptr = inptr;
			
			while (inptr < inend && *inptr != '\n')
				inptr++;
			
			if (inptr == inend) {
				gboolean partial = FALSE;
				
				for (i = 0; i < NUM_OPENPGP_MARKERS; i++) {
					if (g_mime_openpgp_markers[i].before == openpgp->state && is_partial_match (lineptr, inend, &g_mime_openpgp_markers[i])) {
						partial = TRUE;
						break;
					}
				}
				
				if (partial) {
					g_mime_filter_backup (filter, lineptr, inptr - lineptr);
					openpgp->position += lineptr - inbuf;
				} else {
					openpgp->position += inptr - lineptr;
					openpgp->midline = TRUE;
				}
				
				return;
			}
			
			inptr++;
			
			for (i = 0; i < NUM_OPENPGP_MARKERS; i++) {
				if (g_mime_openpgp_markers[i].before == openpgp->state && is_marker (lineptr, inend, &g_mime_openpgp_markers[i], &cr)) {
					openpgp->state = g_mime_openpgp_markers[i].after;
					set_position (openpgp, lineptr - inbuf, i, cr);
					*outbuf = (char *) lineptr;
					*outlen = inptr - lineptr;
					
					if (!g_mime_openpgp_markers[i].is_end_marker)
						openpgp->next = i + 1;
					
					break;
				}
			}
		} while (inptr < inend && openpgp->state == GMIME_OPENPGP_NONE);
		
		if (inptr == inend) {
			openpgp->position += inptr - inbuf;
			return;
		}
	}
	
	do {
		const char *lineptr = inptr;
		
		while (inptr < inend && *inptr != '\n')
			inptr++;
		
		if (inptr == inend) {
			if (!flush) {
				if (is_partial_match (lineptr, inend, &g_mime_openpgp_markers[openpgp->next])) {
					g_mime_filter_backup (filter, lineptr, inptr - lineptr);
					openpgp->position += lineptr - inbuf;
					*outlen = lineptr - *outbuf;
				} else {
					openpgp->position += inptr - inbuf;
					*outlen = inptr - *outbuf;
					openpgp->midline = TRUE;
				}
				
				return;
			}
			
			openpgp->position += inptr - inbuf;
			*outlen = inptr - *outbuf;
			
			return;
		}
		
		inptr++;
		
		if (is_marker (lineptr, inend, &g_mime_openpgp_markers[openpgp->next], &cr)) {
			openpgp->seen_end_marker = g_mime_openpgp_markers[openpgp->next].is_end_marker;
			openpgp->state = g_mime_openpgp_markers[openpgp->next].after;
			set_position (openpgp, lineptr - inbuf, openpgp->next, cr);
			
			if (openpgp->seen_end_marker)
				break;
			
			openpgp->next++;
		}
	} while (inptr < inend);
	
	openpgp->position += inptr - inbuf;
	*outlen = inptr - *outbuf;
}

static void
filter_filter (GMimeFilter *filter, char *inbuf, size_t inlen, size_t prespace,
	       char **outbuf, size_t *outlen, size_t *outprespace)
{
	scan (filter, inbuf, inlen, prespace, outbuf, outlen, outprespace, FALSE);
}

static void 
filter_complete (GMimeFilter *filter, char *inbuf, size_t inlen, size_t prespace,
		 char **outbuf, size_t *outlen, size_t *outprespace)
{
	scan (filter, inbuf, inlen, prespace, outbuf, outlen, outprespace, TRUE);
}

static void
filter_reset (GMimeFilter *filter)
{
	GMimeFilterOpenPGP *openpgp = (GMimeFilterOpenPGP *) filter;
	
	openpgp->state = GMIME_OPENPGP_NONE;
	openpgp->seen_end_marker = FALSE;
	openpgp->midline = FALSE;
	openpgp->begin_offset = -1;
	openpgp->end_offset = -1;
	openpgp->position = 0;
	openpgp->next = 0;
}


/**
 * g_mime_filter_openpgp_new:
 *
 * Creates a new #GMimeFilterOpenPGP filter.
 *
 * Returns: a new #GMimeFilterOpenPGP filter.
 *
 * Since: 3.2
 **/
GMimeFilter *
g_mime_filter_openpgp_new (void)
{
	return (GMimeFilter *) g_object_new (GMIME_TYPE_FILTER_OPENPGP, NULL);
}


/**
 * g_mime_filter_openpgp_get_data_type:
 * @openpgp: A #GMimeFilterOpenPGP filter
 *
 * Gets the type of OpenPGP data that has been detected.
 *
 * Returns: a #GMimeOpenPGPData value.
 *
 * Since: 3.2
 **/
GMimeOpenPGPData
g_mime_filter_openpgp_get_data_type (GMimeFilterOpenPGP *openpgp)
{
	g_return_val_if_fail (GMIME_IS_FILTER_OPENPGP (openpgp), GMIME_OPENPGP_DATA_NONE);
	
	switch (openpgp->state) {
	case GMIME_OPENPGP_END_PGP_PRIVATE_KEY_BLOCK: return GMIME_OPENPGP_DATA_PRIVATE_KEY;
	case GMIME_OPENPGP_END_PGP_PUBLIC_KEY_BLOCK: return GMIME_OPENPGP_DATA_PUBLIC_KEY;
	case GMIME_OPENPGP_END_PGP_SIGNATURE: return GMIME_OPENPGP_DATA_SIGNED;
	case GMIME_OPENPGP_END_PGP_MESSAGE: return GMIME_OPENPGP_DATA_ENCRYPTED;
	default: return GMIME_OPENPGP_DATA_NONE;
	}
}


/**
 * g_mime_filter_openpgp_get_begin_offset:
 * @openpgp: A #GMimeFilterOpenPGP filter
 *
 * Gets the stream offset of the beginning of the OpenPGP data block, if any have been found.
 *
 * Returns: The stream offset or %-1 if no OpenPGP block was found.
 *
 * Since: 3.2
 **/
gint64
g_mime_filter_openpgp_get_begin_offset (GMimeFilterOpenPGP *openpgp)
{
	g_return_val_if_fail (GMIME_IS_FILTER_OPENPGP (openpgp), -1);
	
	return openpgp->begin_offset;
}


/**
 * g_mime_filter_openpgp_get_end_offset:
 * @openpgp: A #GMimeFilterOpenPGP filter
 *
 * Gets the stream offset of the end of the OpenPGP data block, if any have been found.
 *
 * Returns: The stream offset or %-1 if no OpenPGP block was found.
 *
 * Since: 3.2
 **/
gint64
g_mime_filter_openpgp_get_end_offset (GMimeFilterOpenPGP *openpgp)
{
	g_return_val_if_fail (GMIME_IS_FILTER_OPENPGP (openpgp), -1);
	
	return openpgp->end_offset;
}
