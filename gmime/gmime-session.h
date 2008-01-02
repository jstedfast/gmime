/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2008 Jeffrey Stedfast
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


#ifndef __GMIME_SESSION_H__
#define __GMIME_SESSION_H__

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define GMIME_TYPE_SESSION            (g_mime_session_get_type ())
#define GMIME_SESSION(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GMIME_TYPE_SESSION, GMimeSession))
#define GMIME_SESSION_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GMIME_TYPE_SESSION, GMimeSessionClass))
#define GMIME_IS_SESSION(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GMIME_TYPE_SESSION))
#define GMIME_IS_SESSION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GMIME_TYPE_SESSION))
#define GMIME_SESSION_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GMIME_TYPE_SESSION, GMimeSessionClass))

typedef struct _GMimeSession GMimeSession;
typedef struct _GMimeSessionClass GMimeSessionClass;

struct _GMimeSession {
	GObject parent_object;
	
};

struct _GMimeSessionClass {
	GObjectClass parent_class;
	
	gboolean (* is_online) (GMimeSession *session);
	
	char *   (* request_passwd) (GMimeSession *session, const char *prompt,
				     gboolean secret, const char *item,
				     GError **err);
	
	void     (* forget_passwd)  (GMimeSession *session, const char *item,
				     GError **err);
};


GType g_mime_session_get_type (void);


gboolean g_mime_session_is_online (GMimeSession *session);

char *g_mime_session_request_passwd (GMimeSession *session, const char *prompt,
				     gboolean secret, const char *item,
				     GError **err);

void g_mime_session_forget_passwd (GMimeSession *session, const char *item,
				   GError **err);


G_END_DECLS

#endif /* __GMIME_SESSION_H__ */
