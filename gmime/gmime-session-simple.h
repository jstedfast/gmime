/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2003 Ximian, Inc. (www.ximian.com)
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


#ifndef __GMIME_SESSION_SIMPLE_H__
#define __GMIME_SESSION_SIMPLE_H__

#include <gmime/gmime-session.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#define GMIME_TYPE_SESSION_SIMPLE            (g_mime_session_simple_get_type ())
#define GMIME_SESSION_SIMPLE(obj)            (GMIME_CHECK_CAST ((obj), GMIME_TYPE_SESSION_SIMPLE, GMimeSessionSimple))
#define GMIME_SESSION_SIMPLE_CLASS(klass)    (GMIME_CHECK_CLASS_CAST ((klass), GMIME_TYPE_SESSION_SIMPLE, GMimeSessionSimpleClass))
#define GMIME_IS_SESSION_SIMPLE(obj)         (GMIME_CHECK_TYPE ((obj), GMIME_TYPE_SESSION_SIMPLE))
#define GMIME_IS_SESSION_SIMPLE_CLASS(klass) (GMIME_CHECK_CLASS_TYPE ((klass), GMIME_TYPE_SESSION_SIMPLE))
#define GMIME_SESSION_SIMPLE_GET_CLASS(obj)  (GMIME_CHECK_GET_CLASS ((obj), GMIME_TYPE_SESSION_SIMPLE, GMimeSessionSimpleClass))

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


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GMIME_SESSION_SIMPLE_H__ */
