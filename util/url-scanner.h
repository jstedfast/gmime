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


#ifndef __URL_SCANNER_H__
#define __URL_SCANNER_H__

#include <glib.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

typedef struct {
	const char *pattern;
	const char *prefix;
	off_t um_so;
	off_t um_eo;
} urlmatch_t;

typedef gboolean (*GUrlScanFunc) (const char *in, const char *pos, const char *inend, urlmatch_t *match);

/* some default GUrlScanFunc's */
gboolean g_url_file_start (const char *in, const char *pos, const char *inend, urlmatch_t *match);
gboolean g_url_file_end (const char *in, const char *pos, const char *inend, urlmatch_t *match);
gboolean g_url_web_start (const char *in, const char *pos, const char *inend, urlmatch_t *match);
gboolean g_url_web_end (const char *in, const char *pos, const char *inend, urlmatch_t *match);
gboolean g_url_addrspec_start (const char *in, const char *pos, const char *inend, urlmatch_t *match);
gboolean g_url_addrspec_end (const char *in, const char *pos, const char *inend, urlmatch_t *match);

typedef struct {
	char *pattern;
	char *prefix;
	GUrlScanFunc start;
	GUrlScanFunc end;
} urlpattern_t;

typedef struct _GUrlScanner GUrlScanner;

GUrlScanner *g_url_scanner_new (void);
void g_url_scanner_free (GUrlScanner *scanner);

void g_url_scanner_add (GUrlScanner *scanner, urlpattern_t *pattern);

gboolean g_url_scanner_scan (GUrlScanner *scanner, const char *in, size_t inlen, urlmatch_t *match);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __URL_SCANNER_H__ */
