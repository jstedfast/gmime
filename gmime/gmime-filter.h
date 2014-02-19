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


#ifndef __GMIME_FILTER_H__
#define __GMIME_FILTER_H__

#include <glib.h>
#include <glib-object.h>
#include <sys/types.h>

G_BEGIN_DECLS

#define GMIME_TYPE_FILTER            (g_mime_filter_get_type ())
#define GMIME_FILTER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GMIME_TYPE_FILTER, GMimeFilter))
#define GMIME_FILTER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GMIME_TYPE_FILTER, GMimeFilterClass))
#define GMIME_IS_FILTER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GMIME_TYPE_FILTER))
#define GMIME_IS_FILTER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GMIME_TYPE_FILTER))
#define GMIME_FILTER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GMIME_TYPE_FILTER, GMimeFilterClass))

typedef struct _GMimeFilter GMimeFilter;
typedef struct _GMimeFilterClass GMimeFilterClass;

/**
 * GMimeFilter:
 * @parent_object: parent #GObject
 * @priv: private state data
 * @outreal: (array) (element-type guint8): real malloc'd buffer
 * @outbuf: (array length=outsize) (element-type guint8): first
 *   writable position allowed (outreal + outpre)
 * @outptr: current writale position
 * @outsize: current bytes in @outbuf
 * @outpre: current prespace of @outbuf
 * @backbuf: (array length=backsize) (element-type guint8): backup
 *   buffer
 * @backsize: backup buffer size
 * @backlen: number of bytes backed-up in @backbuf
 *
 * Base class for filters used by #GMimeStreamFilter.
 **/
struct _GMimeFilter {
	GObject parent_object;
	
	struct _GMimeFilterPrivate *priv;
	
	/* <protected> */
	char *outreal;		/* real malloc'd buffer */
	char *outbuf;		/* first 'writable' position allowed (outreal + outpre) */
	char *outptr;
	size_t outsize;
	size_t outpre;		/* prespace of this buffer */
	
	char *backbuf;
	size_t backsize;
	size_t backlen;		/* significant data there */
};

struct _GMimeFilterClass {
	GObjectClass parent_class;
	
	/* virtual functions */
	GMimeFilter * (* copy) (GMimeFilter *filter);
	
	void (* filter)   (GMimeFilter *filter, char *inbuf, size_t inlen, size_t prespace,
			   char **outbuf, size_t *outlen, size_t *outprespace);
	
	void (* complete) (GMimeFilter *filter, char *inbuf, size_t inlen, size_t prespace,
			   char **outbuf, size_t *outlen, size_t *outprespace);
	
	void (* reset)    (GMimeFilter *filter);
};


GType g_mime_filter_get_type (void);


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

G_END_DECLS

#endif /* __GMIME_FILTER_H__ */
