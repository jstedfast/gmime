/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 1999-2006 Jeffrey Stedfast
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
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "gmime-session.h"
#include "gmime-error.h"


static void g_mime_session_class_init (GMimeSessionClass *klass);
static void g_mime_session_init (GMimeSession *session, GMimeSessionClass *klass);
static void g_mime_session_finalize (GObject *object);

static gboolean session_is_online (GMimeSession *session);
static char *session_request_passwd (GMimeSession *session, const char *prompt,
				     gboolean secret, const char *item,
				     GError **err);
static void session_forget_passwd (GMimeSession *session, const char *item,
				   GError **err);


static GObjectClass *parent_class = NULL;


GType
g_mime_session_get_type (void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof (GMimeSessionClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc) g_mime_session_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (GMimeSession),
			0,    /* n_preallocs */
			(GInstanceInitFunc) g_mime_session_init,
		};
		
		type = g_type_register_static (G_TYPE_OBJECT, "GMimeSession", &info, 0);
	}
	
	return type;
}


static void
g_mime_session_class_init (GMimeSessionClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	
	parent_class = g_type_class_ref (G_TYPE_OBJECT);
	
	object_class->finalize = g_mime_session_finalize;
	
	klass->is_online = session_is_online;
	klass->request_passwd = session_request_passwd;
	klass->forget_passwd = session_forget_passwd;
}

static void
g_mime_session_init (GMimeSession *session, GMimeSessionClass *klass)
{
	
}

static void
g_mime_session_finalize (GObject *object)
{
	G_OBJECT_CLASS (parent_class)->finalize (object);
}


static gboolean
session_is_online (GMimeSession *session)
{
	return FALSE;
}


/**
 * g_mime_session_is_online:
 * @session: session object
 *
 * Gets whether or not the session is 'online' or not (online meaning
 * that we are connected to the internet).
 *
 * Returns %TRUE if the session is online or %FALSE otherwise.
 **/
gboolean
g_mime_session_is_online (GMimeSession *session)
{
	g_return_val_if_fail (GMIME_IS_SESSION (session), FALSE);
	
	return GMIME_SESSION_GET_CLASS (session)->is_online (session);
}


static char *
session_request_passwd (GMimeSession *session, const char *prompt,
			gboolean secret, const char *item,
			GError **err)
{
	g_set_error (err, GMIME_ERROR_QUARK, GMIME_ERROR_NOT_SUPPORTED,
		     "Password request mechanism is not implemented.");
	return NULL;
}


/**
 * g_mime_session_request_passwd:
 * @session: session object
 * @prompt: prompt to present to the user
 * @secret: %TRUE if the characters the user types should be hidden
 * @item: item name
 * @err: exception
 *
 * Requests the password for item @item.
 *
 * Returns a string buffer containing the password for the requested
 * item or %NULL on fail.
 **/
char *
g_mime_session_request_passwd (GMimeSession *session, const char *prompt,
			       gboolean secret, const char *item,
			       GError **err)
{
	g_return_val_if_fail (GMIME_IS_SESSION (session), NULL);
	
	return GMIME_SESSION_GET_CLASS (session)->request_passwd (session, prompt, secret, item, err);
}


static void
session_forget_passwd (GMimeSession *session, const char *item, GError **err)
{
	g_set_error (err, GMIME_ERROR_QUARK, GMIME_ERROR_NOT_SUPPORTED,
		     "Password forget mechanism is not implemented.");
}


/**
 * g_mime_session_forget_passwd:
 * @session: session object
 * @item: item name
 * @err: exception
 *
 * Forgets the password for item @item.
 **/
void
g_mime_session_forget_passwd (GMimeSession *session, const char *item, GError **err)
{
	g_return_if_fail (GMIME_IS_SESSION (session));
	
	GMIME_SESSION_GET_CLASS (session)->forget_passwd (session, item, err);
}
