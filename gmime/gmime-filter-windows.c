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


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "gmime-filter-windows.h"
#include "gmime-charset.h"

#define d(x)


/**
 * SECTION: gmime-filter-windows
 * @title: GMimeFilterWindows
 * @short_description: Determine if text is in a Microsoft Windows codepage
 * @see_also: #GMimeFilter
 *
 * A #GMimeFilter used for determining if text marked as iso-8859-##
 * is actually encoded in one of the Windows-CP125# charsets.
 **/


static void g_mime_filter_windows_class_init (GMimeFilterWindowsClass *klass);
static void g_mime_filter_windows_init (GMimeFilterWindows *filter, GMimeFilterWindowsClass *klass);
static void g_mime_filter_windows_finalize (GObject *object);

static GMimeFilter *filter_copy (GMimeFilter *filter);
static void filter_filter (GMimeFilter *filter, char *in, size_t len, size_t prespace,
			   char **out, size_t *outlen, size_t *outprespace);
static void filter_complete (GMimeFilter *filter, char *in, size_t len, size_t prespace,
			     char **out, size_t *outlen, size_t *outprespace);
static void filter_reset (GMimeFilter *filter);


static GMimeFilterClass *parent_class = NULL;


GType
g_mime_filter_windows_get_type (void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof (GMimeFilterWindowsClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc) g_mime_filter_windows_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (GMimeFilterWindows),
			0,    /* n_preallocs */
			(GInstanceInitFunc) g_mime_filter_windows_init,
		};
		
		type = g_type_register_static (GMIME_TYPE_FILTER, "GMimeFilterWindows", &info, 0);
	}
	
	return type;
}


static void
g_mime_filter_windows_class_init (GMimeFilterWindowsClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GMimeFilterClass *filter_class = GMIME_FILTER_CLASS (klass);
	
	parent_class = g_type_class_ref (GMIME_TYPE_FILTER);
	
	object_class->finalize = g_mime_filter_windows_finalize;
	
	filter_class->copy = filter_copy;
	filter_class->filter = filter_filter;
	filter_class->complete = filter_complete;
	filter_class->reset = filter_reset;
}

static void
g_mime_filter_windows_init (GMimeFilterWindows *filter, GMimeFilterWindowsClass *klass)
{
	filter->claimed_charset = NULL;
	filter->is_windows = FALSE;
}

static void
g_mime_filter_windows_finalize (GObject *object)
{
	GMimeFilterWindows *filter = (GMimeFilterWindows *) object;
	
	g_free (filter->claimed_charset);
	
	G_OBJECT_CLASS (parent_class)->finalize (object);
}


static GMimeFilter *
filter_copy (GMimeFilter *filter)
{
	GMimeFilterWindows *windows = (GMimeFilterWindows *) filter;
	
	return g_mime_filter_windows_new (windows->claimed_charset);
}

static void
filter_filter (GMimeFilter *filter, char *in, size_t len, size_t prespace,
	       char **out, size_t *outlen, size_t *outprespace)
{
	GMimeFilterWindows *windows = (GMimeFilterWindows *) filter;
	register unsigned char *inptr;
	unsigned char *inend;
	
	if (!windows->is_windows) {
		inptr = (unsigned char *) in;
		inend = inptr + len;
		
		while (inptr < inend) {
			register unsigned char c = *inptr++;
			
			if (c >= 128 && c <= 159) {
				d(g_warning ("Encountered text encoded in a Windows charset trying "
					     "to pass itself off as being encoded in %s",
					     windows->claimed_charset));
				windows->is_windows = TRUE;
				break;
			}
		}
	}
	
	*out = in;
	*outlen = len;
	*outprespace = prespace;	
}

static void 
filter_complete (GMimeFilter *filter, char *in, size_t len, size_t prespace,
		 char **out, size_t *outlen, size_t *outprespace)
{
	filter_filter (filter, in, len, prespace, out, outlen, outprespace);
}

static void
filter_reset (GMimeFilter *filter)
{
	GMimeFilterWindows *windows = (GMimeFilterWindows *) filter;
	
	windows->is_windows = FALSE;
}


/**
 * g_mime_filter_windows_new:
 * @claimed_charset: charset that a text stream claims to be
 *
 * Creates a new GMimeFilterWindows filter. When a stream of text has
 * been filtered, it can be determined whether or not said text stream
 * was in @claimed_charset or the equivalent Windows-CP125# charset.
 *
 * Returns: a new windows filter.
 **/
GMimeFilter *
g_mime_filter_windows_new (const char *claimed_charset)
{
	GMimeFilterWindows *new;
	
	g_return_val_if_fail (claimed_charset != NULL, NULL);
	
	new = g_object_newv (GMIME_TYPE_FILTER_WINDOWS, 0, NULL);
	new->claimed_charset = g_strdup (claimed_charset);
	
	return (GMimeFilter *) new;
}


/**
 * g_mime_filter_windows_is_windows_charset:
 * @filter: windows filter object
 *
 * Determines whether or not a Windows-CP125# charset has been
 * detected so far.
 *
 * Returns: %TRUE if the filtered stream has been detected to contain
 * Windows-CP125# characters or %FALSE otherwise.
 **/
gboolean
g_mime_filter_windows_is_windows_charset (GMimeFilterWindows *filter)
{
	g_return_val_if_fail (GMIME_IS_FILTER_WINDOWS (filter), FALSE);
	
	return filter->is_windows;
}


/**
 * g_mime_filter_windows_real_charset:
 * @filter: windows filter object
 *
 * Figures out the real charset that the text is encoded in based on whether or not Windows-CP125# characters were found.
 *
 * Returns: a const string pointer to the claimed charset if filtered
 * text stream was found not to contain any Windows-CP125# characters
 * or the proper Windows-CP125# charset.
 **/
const char *
g_mime_filter_windows_real_charset (GMimeFilterWindows *filter)
{
	g_return_val_if_fail (GMIME_IS_FILTER_WINDOWS (filter), NULL);
	
	if (filter->is_windows)
		return g_mime_charset_iso_to_windows (filter->claimed_charset);
	else
		return filter->claimed_charset;
}
