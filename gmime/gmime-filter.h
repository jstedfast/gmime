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


#ifndef __GMIME_FILTER_H__
#define __GMIME_FILTER_H__

#include <glib.h>
#include <glib-object.h>
#include <sys/types.h>

G_BEGIN_DECLS

#define GMIME_TYPE_FILTER            (g_mime_filter_get_type ())


/**
 * GMimeFilter:
 *
 * Base class for filters used by #GMimeStreamFilter.
 **/
G_DECLARE_DERIVABLE_TYPE (GMimeFilter, g_mime_filter, GMIME, FILTER, GObject)

struct _GMimeFilterClass {
	GObjectClass parent_class;
	
	/* virtual functions */
	GMimeFilter * (* copy) (GMimeFilter *filter);
	
	void (* filter)   (GMimeFilter *filter, char *inbuf, size_t inlen, size_t prespace,
			   char **outbuf, size_t *outlen, size_t *outprespace);
	
	void (* complete) (GMimeFilter *filter, char *inbuf, size_t inlen, size_t prespace,
			   char **outbuf, size_t *outlen, size_t *outprespace);
	
	void (* reset)    (GMimeFilter *filter);

	/*< private >*/
	gpointer reserved[8];
};


GMimeFilter *g_mime_filter_copy (GMimeFilter *filter);

void g_mime_filter_filter (GMimeFilter *filter,
			   char *inbuf, size_t inlen, size_t prespace,
			   char **outbuf, size_t *outlen, size_t *outprespace);

void g_mime_filter_complete (GMimeFilter *filter,
			     char *inbuf, size_t inlen, size_t prespace,
			     char **outbuf, size_t *outlen, size_t *outprespace);

void g_mime_filter_reset (GMimeFilter *filter);


/* sets/returns number of bytes backed up on the input */
void g_mime_filter_backup (GMimeFilter *filter, const char *data, size_t length);

/* ensure this much size available for filter output */
void g_mime_filter_set_size (GMimeFilter *filter, size_t size, gboolean keep);
char *g_mime_filter_get_output_buffer (GMimeFilter *filter, size_t *size, size_t *prespace);

G_END_DECLS

#endif /* __GMIME_FILTER_H__ */
