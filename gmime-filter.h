/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2001 Ximain, Inc. (www.ximian.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */


#ifndef __GMIME_FILTER_H__
#define __GMIME_FILTER_H__

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#include <glib.h>
#include <sys/types.h>

typedef struct _GMimeFilter GMimeFilter;

struct _GMimeFilter {
	struct _GMimeFilterPrivate *priv;
	
	char *outreal;		/* real malloc'd buffer */
	char *outbuf;		/* first 'writable' position allowed (outreal + outpre) */
	char *outptr;
	size_t outsize;
	size_t outpre;		/* prespace of this buffer */
	
	char *backbuf;
	size_t backsize;
	size_t backlen;		/* significant data there */
	
	/* virtual functions */
	void (*destroy)  (GMimeFilter *filter);
	
	GMimeFilter *(*copy) (GMimeFilter *filter);
	
	void (*filter)   (GMimeFilter *filter,
			  char *in, size_t len, size_t prespace,
			  char **out, size_t *outlen, size_t *outprespace);
	
	void (*complete) (GMimeFilter *filter,
			  char *in, size_t len, size_t prespace,
			  char **out, size_t *outlen, size_t *outprespace);
	
	void (*reset)    (GMimeFilter *filter);
};

#define GMIME_FILTER(filter) ((GMimeFilter *) filter)

void g_mime_filter_construct (GMimeFilter *filter, GMimeFilter *filter_template);

void g_mime_filter_destroy (GMimeFilter *filter);

GMimeFilter *g_mime_filter_copy (GMimeFilter *filter);

void g_mime_filter_filter (GMimeFilter *filter,
			   char *in, size_t len, size_t prespace,
			   char **out, size_t *outlen, size_t *outprespace);

void g_mime_filter_complete (GMimeFilter *filter,
			     char *in, size_t len, size_t prespace,
			     char **out, size_t *outlen, size_t *outprespace);

void g_mime_filter_reset (GMimeFilter *filter);

/* sets/returns number of bytes backed up on the input */
void g_mime_filter_backup (GMimeFilter *filter, const char *data, size_t length);

/* ensure this much size available for filter output */
void g_mime_filter_set_size (GMimeFilter *filter, size_t size, gboolean keep);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GMIME_FILTER_H__ */
