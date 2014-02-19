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

#include <glib.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <locale.h>
#include <iconv.h>
#include <errno.h>

#ifdef HAVE_CODESET
#include <langinfo.h>
#endif


static struct {
	char *name;        /* charset name */
	int multibyte;     /* charset type */
	unsigned int bit;  /* assigned bit */
} tables[] = {
	/* These are the 8bit character sets (other than iso-8859-1,
	 * which is special-cased) which are supported by both other
	 * mailers and the GNOME environment. Note that the order
	 * they're listed in is the order they'll be tried in, so put
	 * the more-popular ones first.
	 */
	{ "iso-8859-2",   0, 0 },  /* Central/Eastern European */
	{ "iso-8859-4",   0, 0 },  /* Baltic */
	{ "koi8-r",       0, 0 },  /* Russian */
	{ "koi8-u",       0, 0 },  /* Ukranian */
	{ "iso-8859-5",   0, 0 },  /* Least-popular Russian encoding */
	{ "iso-8859-6",   0, 0 },  /* Arabic */
	{ "iso-8859-7",   0, 0 },  /* Greek */
	{ "iso-8859-8",   0, 0 },  /* Hebrew; Visual */
	{ "iso-8859-9",   0, 0 },  /* Turkish */
	{ "iso-8859-13",  0, 0 },  /* Baltic again */
	{ "iso-8859-15",  0, 0 },  /* New-and-improved iso-8859-1, but most
				    * programs that support this support UTF8
				    */
	{ "windows-1251", 0, 0 },  /* Russian */
	
	/* These are the multibyte character sets which are commonly
	 * supported by other mail clients. Note: order for multibyte
	 * charsets does not affect priority unlike the 8bit charsets
	 * listed above.
	 */
	{ "iso-2022-jp",  1, 0 },  /* Japanese designed for use over the Net */
	{ "Shift-JIS",    1, 0 },  /* Japanese as used by Windows and MacOS systems */
	{ "euc-jp",       1, 0 },  /* Japanese traditionally used on Unix systems */
	{ "euc-kr",       1, 0 },  /* Korean */
	{ "gb2312",       1, 0 },  /* Simplified Chinese */
	{ "Big5",         1, 0 },  /* Traditional Chinese */
	{ "euc-tw",       1, 0 },
	{ NULL,           0, 0 }
};

unsigned int encoding_map[256 * 256];

#if G_BYTE_ORDER == G_BIG_ENDIAN
#define UCS "UCS-4BE"
#else
#define UCS "UCS-4LE"
#endif

static guint
block_hash (gconstpointer v)
{
	const signed char *p = v;
	guint32 h = *p++;
	int i;
	
	for (i = 0; i < 256; i++)
		h = (h << 5) - h + *p++;
	
	return h;
}

static int
block_equal (gconstpointer v1, gconstpointer v2)
{
	return !memcmp (v1, v2, 256);
}

int main (int argc, char **argv)
{
	unsigned char *block = NULL;
	unsigned int bit = 0x01;
	GHashTable *table_hash;
	size_t inleft, outleft;
	char *inbuf, *outbuf;
	guint32 out[128], c;
	unsigned int i;
	char in[128];
	iconv_t cd;
	int bytes;
	int j, k;
	
	/* dont count the terminator */
	bytes = ((sizeof (tables) / sizeof (tables[0])) + 7 - 1) / 8;
	g_assert (bytes <= 4);
	
	for (i = 0; i < 128; i++)
		in[i] = i + 128;
	
	for (j = 0; tables[j].name && !tables[j].multibyte; j++) {
		cd = iconv_open (UCS, tables[j].name);
		inbuf = in;
		inleft = sizeof (in);
		outbuf = (char *) out;
		outleft = sizeof (out);
		while (iconv (cd, &inbuf, &inleft, &outbuf, &outleft) == (size_t) -1) {
			if (errno == EILSEQ) {
				inbuf++;
				inleft--;
			} else {
				g_warning ("iconv (%s->UCS4, ..., %zu, ..., %zu): %s",
					   tables[j].name, inleft, outleft,
					   g_strerror (errno));
				exit (1);
			}
		}
		iconv_close (cd);
		
		for (i = 0; i < 128 - outleft / 4; i++) {
			encoding_map[i] |= bit;
			encoding_map[out[i]] |= bit;
		}
		
		tables[j].bit = bit;
		bit <<= 1;
	}
	
	/* Mutibyte tables */
	for ( ; tables[j].name && tables[j].multibyte; j++) {
		cd = iconv_open (tables[j].name, UCS);
		if (cd == (iconv_t) -1)
			continue;
		
		for (c = 128, i = 0; c < 65535 && i < 65535; c++) {
			inbuf = (char *) &c;
			inleft = sizeof (c);
			outbuf = in;
			outleft = sizeof (in);
			
			if (iconv (cd, &inbuf, &inleft, &outbuf, &outleft) != (size_t) -1) {
				/* this is a legal character in charset table[j].name */
				iconv (cd, NULL, NULL, &outbuf, &outleft);
				encoding_map[i++] |= bit;
				encoding_map[c] |= bit;
			} else {
				/* reset the iconv descriptor */
				iconv (cd, NULL, NULL, NULL, NULL);
			}
		}
		
		iconv_close (cd);
		
		tables[j].bit = bit;
		bit <<= 1;
	}
	
	printf ("/* This file is automatically generated: DO NOT EDIT */\n\n");
	
	table_hash = g_hash_table_new_full (block_hash, block_equal, g_free, g_free);
	
	for (i = 0; i < 256; i++) {
		for (k = 0; k < bytes; k++) {
			char name[32], *alias;
			int has_bits = FALSE;
			
			if (!block) {
				/* we reuse malloc'd blocks that are not added to the
				 * hash table to avoid unnecessary malloc/free's */
				block = g_malloc (256);
			}
			
			for (j = 0; j < 256; j++) {
				if ((block[j] = (encoding_map[i * 256 + j] >> (k * 8)) & 0xff))
					has_bits = TRUE;
			}
			
			if (!has_bits)
				continue;
			
			sprintf (name, "m%02x%x", i, k);
			
			if ((alias = g_hash_table_lookup (table_hash, block))) {
				/* this block is identical to an earlier block, just alias it */
				printf ("#define %s %s\n\n", name, alias);
			} else {
				/* unique block, dump it */
				g_hash_table_insert (table_hash, block, g_strdup (name));
				
				printf ("static unsigned char %s[256] = {\n\t", name);
				for (j = 0; j < 256; j++) {
					printf ("0x%02x, ", block[j]);
					if (((j + 1) & 7) == 0 && j < 255)
						printf ("\n\t");
				}
				printf ("\n};\n\n");
				
				/* force the next loop to malloc a new block */
				block = NULL;
			}
		}
	}
	
	g_hash_table_destroy (table_hash);
	g_free (block);
	
	printf ("static const struct {\n");
	for (k = 0; k < bytes; k++)
		printf ("\tunsigned char *bits%d;\n", k);
	
	printf ("} charmap[256] = {\n\t");
	for (i = 0; i < 256; i++) {
		printf ("{ ");
		for (k = 0; k < bytes; k++) {
			for (j = 0; j < 256; j++) {
				if ((encoding_map[i * 256 + j] & (0xff << (k * 8))) != 0)
					break;
			}
			
			if (j < 256)
				printf ("m%02x%x, ", i, k);
			else
				printf ("NULL, ");
		}
		
		printf ("}, ");
		if (((i + 1) & 3) == 0 && i < 255)
			printf ("\n\t");
	}
	printf ("\n};\n\n");
	
	printf ("static const struct {\n\tconst char *name;\n\tunsigned int bit;\n} charinfo[] = {\n");
	for (j = 0; tables[j].name; j++)
		printf ("\t{ \"%s\", 0x%08x },\n", tables[j].name, tables[j].bit);
	printf ("};\n\n");
	
	printf ("#define charset_mask(x) \\\n");
	for (k = 0; k < bytes; k++) {
		if (k != 0)
			printf ("\t| ");
		else
			printf ("\t");
		
		printf ("(charmap[(x) >> 8].bits%d ? charmap[(x) >> 8].bits%d[(x) & 0xff] << %d : 0)",
			k, k, k * 8);
		
		if (k < bytes - 1)
			printf ("\t\\\n");
	}
	printf ("\n\n");
	
	return 0;
}
