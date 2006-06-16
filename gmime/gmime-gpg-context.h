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


#ifndef __GMIME_GPG_CONTEXT_H__
#define __GMIME_GPG_CONTEXT_H__

#include <gmime/gmime-cipher-context.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#define GMIME_TYPE_GPG_CONTEXT            (g_mime_gpg_context_get_type ())
#define GMIME_GPG_CONTEXT(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GMIME_TYPE_GPG_CONTEXT, GMimeGpgContext))
#define GMIME_GPG_CONTEXT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GMIME_TYPE_GPG_CONTEXT, GMimeGpgContextClass))
#define GMIME_IS_GPG_CONTEXT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GMIME_TYPE_GPG_CONTEXT))
#define GMIME_IS_GPG_CONTEXT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GMIME_TYPE_GPG_CONTEXT))
#define GMIME_GPG_CONTEXT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GMIME_TYPE_GPG_CONTEXT, GMimeGpgContextClass))

typedef struct _GMimeGpgContext GMimeGpgContext;
typedef struct _GMimeGpgContextClass GMimeGpgContextClass;


struct _GMimeGpgContext {
	GMimeCipherContext parent_object;
	
	char *path;
	gboolean always_trust;
};

struct _GMimeGpgContextClass {
	GMimeCipherContextClass parent_class;
	
};


GType g_mime_gpg_context_get_type (void);


GMimeCipherContext *g_mime_gpg_context_new (GMimeSession *session, const char *path);

gboolean g_mime_gpg_context_get_always_trust (GMimeGpgContext *ctx);
void g_mime_gpg_context_set_always_trust (GMimeGpgContext *ctx, gboolean always_trust);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GMIME_GPG_CONTEXT_H__ */
