/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2002 Ximain, Inc. (www.ximian.com)
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


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include "gmime-iconv.h"


static char *charsets[] = {
	"iso-8859-1",
	"iso-8859-2",
	"iso-8859-4",
	"iso-8859-5",
	"iso-8859-7",
	"iso-8859-8",
	"iso-8859-9",
	"iso-8859-13",
	"iso-8859-15",
	"koi8-r",
	"koi8-u",
	"windows-1250",
	"windows-1251",
	"windows-1252",
	"windows-1253",
	"windows-1254",
	"windows-1255",
	"windows-1256",
	"windows-1257",
	"euc-kr",
	"euc-jp",
	"iso-2022-kr",
	"iso-2022-jp",
	"utf-8",
};

static int num_charsets = sizeof (charsets) / sizeof (charsets[0]);


int main (int argc, char **argv)
{
	GSList *node, *open_cds = NULL;
	iconv_t cd;
	int i;
	
	g_mime_iconv_init ();
	
	for (i = 0; i < 500; i++) {
		const char *from, *to;
		int which;
		
		which = rand () % num_charsets;
		from = charsets[which];
		which = rand () % num_charsets;
		to = charsets[which];
		
		cd = g_mime_iconv_open (from, to);
		if (cd == (iconv_t) -1) {
			g_warning ("%d: failed to open converter for %s to %s",
				   i, from, to);
			continue;
		}
		
		which = rand () % 3;
		if (!which) {
			g_mime_iconv_close (cd);
		} else {
			open_cds = g_slist_prepend (open_cds, cd);
		}
	}
	
	node = open_cds;
	while (node) {
		cd = node->data;
		g_mime_iconv_close (cd);
		node = node->next;
	}
	
	g_slist_free (open_cds);
	
	return 0;
}
