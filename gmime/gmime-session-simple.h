/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2007 Jeffrey Stedfast
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


#ifndef __GMIME_SESSION_SIMPLE_H__
#define __GMIME_SESSION_SIMPLE_H__

#include <gmime/gmime-session.h>

G_BEGIN_DECLS

#define GMIME_TYPE_SESSION_SIMPLE            (g_mime_session_simple_get_type ())
#define GMIME_SESSION_SIMPLE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GMIME_TYPE_SESSION_SIMPLE, GMimeSessionSimple))
#define GMIME_SESSION_SIMPLE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GMIME_TYPE_SESSION_SIMPLE, GMimeSessionSimpleClass))
#define GMIME_IS_SESSION_SIMPLE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GMIME_TYPE_SESSION_SIMPLE))
#define GMIME_IS_SESSION_SIMPLE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GMIME_TYPE_SESSION_SIMPLE))
#define GMIME_SESSION_SIMPLE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GMIME_TYPE_SESSION_SIMPLE, GMimeSessionSimpleClass))

typedef struct _GMimeSessionSimple GMimeSessionSimple;
typedef struct _GMimeSessionSimpleClass GMimeSessionSimpleClass;

typedef gboolean (* GMimeSimpleIsOnlineFunc) (GMimeSession *session);
typedef char * (* GMimeSimpleRequestPasswdFunc) (GMimeSession *session, const char *prompt,
						 gboolean secret, const char *item,
						 GError **err);
typedef void (* GMimeSimpleForgetPasswdFunc) (GMimeSession *session, const char *item,
					      GError **err);

struct _GMimeSessionSimple {
	GMimeSession parent_object;
	
	GMimeSimpleIsOnlineFunc is_online;
	GMimeSimpleRequestPasswdFunc request_passwd;
	GMimeSimpleForgetPasswdFunc forget_passwd;
};

struct _GMimeSessionSimpleClass {
	GMimeSessionClass parent_class;
	
};


GType g_mime_session_simple_get_type (void);


void g_mime_session_simple_set_is_online (GMimeSessionSimple *session, GMimeSimpleIsOnlineFunc is_online);
void g_mime_session_simple_set_request_passwd (GMimeSessionSimple *session, GMimeSimpleRequestPasswdFunc request_passwd);
void g_mime_session_simple_set_forget_passwd (GMimeSessionSimple *session, GMimeSimpleForgetPasswdFunc forget_passwd);


G_END_DECLS

#endif /* __GMIME_SESSION_SIMPLE_H__ */
