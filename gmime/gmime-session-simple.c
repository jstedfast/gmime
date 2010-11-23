/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2010 Jeffrey Stedfast
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

#include "gmime-error.h"
#include "gmime-session-simple.h"


/**
 * SECTION: gmime-session-simple
 * @title: GMimeSessionSimple
 * @short_description: A Simple Session
 * @see_also: #GMimeSession
 *
 * A #GMimeSessionSimple can be used for simple applications that
 * don't care to implement their own #GMimeSession.
 **/

static void g_mime_session_simple_class_init (GMimeSessionSimpleClass *klass);
static void g_mime_session_simple_init (GMimeSessionSimple *session, GMimeSessionSimpleClass *klass);
static void g_mime_session_simple_finalize (GObject *object);

static gboolean simple_is_online (GMimeSession *session);
static char *simple_request_passwd (GMimeSession *session, const char *prompt,
				    gboolean secret, const char *item,
				    GError **err);
static void simple_forget_passwd (GMimeSession *session, const char *item,
				  GError **err);


static GMimeSessionClass *parent_class = NULL;


GType
g_mime_session_simple_get_type (void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof (GMimeSessionSimpleClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc) g_mime_session_simple_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (GMimeSessionSimple),
			0,    /* n_preallocs */
			(GInstanceInitFunc) g_mime_session_simple_init,
		};
		
		type = g_type_register_static (GMIME_TYPE_SESSION, "GMimeSessionSimple", &info, 0);
	}
	
	return type;
}


static void
g_mime_session_simple_class_init (GMimeSessionSimpleClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GMimeSessionClass *session_class = GMIME_SESSION_CLASS (klass);
	
	parent_class = g_type_class_ref (GMIME_TYPE_SESSION);
	
	object_class->finalize = g_mime_session_simple_finalize;
	
	session_class->is_online = simple_is_online;
	session_class->request_passwd = simple_request_passwd;
	session_class->forget_passwd = simple_forget_passwd;
}

static void
g_mime_session_simple_init (GMimeSessionSimple *session, GMimeSessionSimpleClass *klass)
{
	session->is_online = NULL;
	session->request_passwd = NULL;
	session->forget_passwd = NULL;
}

static void
g_mime_session_simple_finalize (GObject *object)
{
	G_OBJECT_CLASS (parent_class)->finalize (object);
}


static gboolean
simple_is_online (GMimeSession *session)
{
	GMimeSessionSimple *simple = (GMimeSessionSimple *) session;
	
	if (simple->is_online)
		return simple->is_online (session);
	
	return FALSE;
}

static char *
simple_request_passwd (GMimeSession *session, const char *prompt,
		       gboolean secret, const char *item,
		       GError **err)
{
	GMimeSessionSimple *simple = (GMimeSessionSimple *) session;
	
	if (simple->request_passwd)
		return simple->request_passwd (session, prompt, secret, item, err);
	
	g_set_error (err, GMIME_ERROR_QUARK, GMIME_ERROR_NOT_SUPPORTED,
		     "Password request mechanism has not been defined.");
	
	return NULL;
}


static void
simple_forget_passwd (GMimeSession *session, const char *item, GError **err)
{
	GMimeSessionSimple *simple = (GMimeSessionSimple *) session;
	
	if (simple->forget_passwd)
		simple->forget_passwd (session, item, err);
}


/**
 * g_mime_session_simple_set_is_online:
 * @session: Simple Session object
 * @is_online: callback to return if the system is connected to the 'net
 *
 * Sets the @is_online callback function on the simple session
 * object. @is_online should return %TRUE if the network is reachable
 * or %FALSE otherwise.
 **/
void
g_mime_session_simple_set_is_online (GMimeSessionSimple *session, GMimeSimpleIsOnlineFunc is_online)
{
	g_return_if_fail (GMIME_IS_SESSION_SIMPLE (session));
	
	session->is_online = is_online;
}


/**
 * g_mime_session_simple_set_request_passwd:
 * @session: Simple Session object
 * @request_passwd: callback to prompt the user for a passwd
 *
 * Sets the @request_passwd callback function on the simple session
 * object. @request_passwd should return a malloc'd string containing
 * the password that the user entered or %NULL on fail as well as
 * setting the 'err' argument. The 'item' argument can be used as a
 * unique key identifier if @request_passwd decides to cache the
 * passwd. The 'prompt' argument should be used as the string to
 * display to the user requesting the passwd. Finally, 'secret' should
 * be used to determine whether or not to hide the user's input.
 **/
void
g_mime_session_simple_set_request_passwd (GMimeSessionSimple *session, GMimeSimpleRequestPasswdFunc request_passwd)
{
	g_return_if_fail (GMIME_IS_SESSION_SIMPLE (session));
	
	session->request_passwd = request_passwd;
}


/**
 * g_mime_session_simple_set_forget_passwd:
 * @session: Simple Session object
 * @forget_passwd: callback to forget the cached passwd keyed for 'item'
 *
 * Sets the @forget_passwd callback function on the simple session
 * object. @forget_passwd should uncache the passwd for 'item'. See
 * g_mime_session_simple_set_request_passwd() for further details.
 **/
void
g_mime_session_simple_set_forget_passwd (GMimeSessionSimple *session, GMimeSimpleForgetPasswdFunc forget_passwd)
{
	g_return_if_fail (GMIME_IS_SESSION_SIMPLE (session));
	
	session->forget_passwd = forget_passwd;
}
