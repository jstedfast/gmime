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


#include <stdio.h>
#include <string.h>
#include <ctype.h>

enum {
	FALSE,
	TRUE
};

#define CHARS_LWSP " \t\n\r"               /* linear whitespace chars */
#define CHARS_TSPECIAL "()<>@,;:\\\"/[]?="
#define CHARS_SPECIAL "()<>@,;:\\\".[]"
#define CHARS_CSPECIAL "()\\\r"	           /* not in comments */
#define CHARS_DSPECIAL "[]\\\r \t"	   /* not in domains */
#define CHARS_ESPECIAL "()<>@,;:\"/[]?.=_" /* encoded word specials (rfc2047 5.1) */
#define CHARS_PSPECIAL "!*+-/=_"           /* encoded phrase specials (rfc2047 5.3) */
#define CHARS_ATTRCHAR "*'% "              /* attribute-char from rfc2184 */

static unsigned short gmime_special_table[256];

enum {
	IS_CTRL		= (1 << 0),
	IS_LWSP		= (1 << 1),
	IS_TSPECIAL	= (1 << 2),
	IS_SPECIAL	= (1 << 3),
	IS_SPACE	= (1 << 4),
	IS_DSPECIAL	= (1 << 5),
	IS_QPSAFE	= (1 << 6),
	IS_ESAFE	= (1 << 7),  /* encoded word safe */
	IS_PSAFE	= (1 << 8),  /* encoded word in phrase safe */
	IS_ATTRCHAR     = (1 << 9),  /* attribute-char from rfc2184 */
	
	/* ctype replacements */
	IS_ASCII        = (1 << 10), /* ascii */
	IS_BLANK        = (1 << 11), /* space or tab */
};

#define is_ctrl(x) ((gmime_special_table[(unsigned char)(x)] & IS_CTRL) != 0)
#define is_lwsp(x) ((gmime_special_table[(unsigned char)(x)] & IS_LWSP) != 0)
#define is_tspecial(x) ((gmime_special_table[(unsigned char)(x)] & IS_TSPECIAL) != 0)
#define is_type(x, t) ((gmime_special_table[(unsigned char)(x)] & (t)) != 0)
#define is_ttoken(x) ((gmime_special_table[(unsigned char)(x)] & (IS_TSPECIAL|IS_LWSP|IS_CTRL)) == 0)
#define is_atom(x) ((gmime_special_table[(unsigned char)(x)] & (IS_SPECIAL|IS_SPACE|IS_CTRL)) == 0)
#define is_dtext(x) ((gmime_special_table[(unsigned char)(x)] & IS_DSPECIAL) == 0)
#define is_fieldname(x) ((gmime_special_table[(unsigned char)(x)] & (IS_CTRL|IS_SPACE)) == 0)
#define is_qpsafe(x) ((gmime_special_table[(unsigned char)(x)] & IS_QPSAFE) != 0)
#define is_especial(x) ((gmime_special_table[(unsigned char)(x)] & IS_ESAFE) != 0)
#define is_psafe(x) ((gmime_special_table[(unsigned char)(x)] & IS_PSAFE) != 0)
#define is_attrchar(x) ((gmime_special_table[(unsigned char)(x)] & IS_ATTRCHAR) != 0)

/* ctype replacements */
#define is_ascii(x) ((gmime_special_table[(unsigned char)(x)] & IS_ASCII) != 0)
#define is_blank(x) ((gmime_special_table[(unsigned char)(x)] & IS_BLANK) != 0)

/* code to rebuild the gmime_special_table */
static void
header_remove_bits (unsigned short bit, char *vals)
{
	register unsigned char *inptr = (unsigned char *) vals;
	
	while (*inptr != '\0')
		gmime_special_table[*inptr++] &= ~bit;
}

static void
header_init_bits (unsigned short bit, unsigned short bitcopy, int remove, char *vals)
{
	register unsigned char *inptr = (unsigned char *) vals;
	int i;
	
	if (!remove) {
		while (*inptr != '\0')
			gmime_special_table[*inptr++] |= bit;
		if (bitcopy) {
			for (i = 0; i < 256; i++) {
				if (gmime_special_table[i] & bitcopy)
					gmime_special_table[i] |= bit;
			}
		}
	} else {
		for (i = 0; i < 256; i++)
			gmime_special_table[i] |= bit;
		while (*inptr != '\0')
			gmime_special_table[*inptr++] &= ~bit;
		if (bitcopy) {
			for (i = 0; i < 256; i++) {
				if (gmime_special_table[i] & bitcopy)
					gmime_special_table[i] &= ~bit;
			}
		}
	}
}

static void
header_decode_init (void)
{
	int i;
	
	for (i = 0; i < 256; i++) {
		gmime_special_table[i] = 0;
		if (i < 32 || i == 127)
			gmime_special_table[i] |= IS_CTRL;
		if (i > 32 && i < 127)
			gmime_special_table[i] |= IS_ATTRCHAR;
		if ((i >= 33 && i <= 60) || (i >= 62 && i <= 126) || i == 32)
			gmime_special_table[i] |= (IS_QPSAFE | IS_ESAFE);
		if ((i >= '0' && i <= '9') || (i >= 'a' && i <= 'z') || (i >= 'A' && i <= 'Z'))
			gmime_special_table[i] |= IS_PSAFE;
		if (isascii (i))
			gmime_special_table[i] |= IS_ASCII;
	}
	
	gmime_special_table[' '] |= IS_SPACE | IS_BLANK;
	gmime_special_table['\t'] |= IS_QPSAFE | IS_BLANK;
	header_init_bits (IS_LWSP, 0, FALSE, CHARS_LWSP);
	header_init_bits (IS_TSPECIAL, IS_CTRL, FALSE, CHARS_TSPECIAL);
	header_init_bits (IS_SPECIAL, 0, FALSE, CHARS_SPECIAL);
	header_init_bits (IS_DSPECIAL, 0, FALSE, CHARS_DSPECIAL);
	header_remove_bits (IS_ESAFE, CHARS_ESPECIAL);
	header_remove_bits (IS_ATTRCHAR, CHARS_TSPECIAL CHARS_ATTRCHAR);
	header_init_bits (IS_PSAFE, 0, FALSE, CHARS_PSPECIAL);
}

int main (int argc, char **argv)
{
	int i;
	
	header_decode_init ();
	
	printf ("/* THIS FILE IS AUTOGENERATED: DO NOT EDIT! */\n\n");
	printf ("/**\n * To regenerate:\n * make gen-table\n");
	printf (" * ./gen-table > gmime-table-private.h\n **/\n\n");
	
	/* print out the table */
	printf ("static unsigned short gmime_special_table[256] = {");
	for (i = 0; i < 256; i++) {
		printf ("%s%4d%s", (i % 16) ? "" : "\n\t",
			gmime_special_table[i], i != 255 ? "," : "\n");
	}
	printf ("};\n\n");
	
	/* print out the enum */
	printf ("enum {\n");
	printf ("\tIS_CTRL     = (1 << 0),\n");
	printf ("\tIS_LWSP     = (1 << 1),\n");
	printf ("\tIS_TSPECIAL = (1 << 2),\n");
	printf ("\tIS_SPECIAL  = (1 << 3),\n");
	printf ("\tIS_SPACE    = (1 << 4),\n");
	printf ("\tIS_DSPECIAL = (1 << 5),\n");
	printf ("\tIS_QPSAFE   = (1 << 6),\n");
	printf ("\tIS_ESAFE    = (1 << 7),  /* encoded word safe */\n");
	printf ("\tIS_PSAFE    = (1 << 8),  /* encode word in phrase safe */\n");
	printf ("\tIS_ATTRCHAR = (1 << 9),  /* attribute-char from rfc2184 */\n");
	printf ("\t\n");
	printf ("\t/* ctype replacements */\n");
	printf ("\tIS_ASCII    = (1 << 10), /* ascii */\n");
	printf ("\tIS_BLANK    = (1 << 11), /* space or tab */\n");
	printf ("};\n\n");
	
	printf ("#define is_ctrl(x) ((gmime_special_table[(unsigned char)(x)] & IS_CTRL) != 0)\n");
	printf ("#define is_lwsp(x) ((gmime_special_table[(unsigned char)(x)] & IS_LWSP) != 0)\n");
	printf ("#define is_tspecial(x) ((gmime_special_table[(unsigned char)(x)] & IS_TSPECIAL) != 0)\n");
	printf ("#define is_type(x, t) ((gmime_special_table[(unsigned char)(x)] & (t)) != 0)\n");
	printf ("#define is_ttoken(x) ((gmime_special_table[(unsigned char)(x)] & (IS_TSPECIAL|IS_LWSP|IS_CTRL)) == 0)\n");
	printf ("#define is_atom(x) ((gmime_special_table[(unsigned char)(x)] & (IS_SPECIAL|IS_SPACE|IS_CTRL)) == 0)\n");
	printf ("#define is_dtext(x) ((gmime_special_table[(unsigned char)(x)] & IS_DSPECIAL) == 0)\n");
	printf ("#define is_fieldname(x) ((gmime_special_table[(unsigned char)(x)] & (IS_CTRL|IS_SPACE)) == 0)\n");
	printf ("#define is_qpsafe(x) ((gmime_special_table[(unsigned char)(x)] & IS_QPSAFE) != 0)\n");
	printf ("#define is_especial(x) ((gmime_special_table[(unsigned char)(x)] & IS_ESAFE) != 0)\n");
	printf ("#define is_psafe(x) ((gmime_special_table[(unsigned char)(x)] & IS_PSAFE) != 0)\n");
	printf ("#define is_attrchar(x) ((gmime_special_table[(unsigned char)(x)] & IS_ATTRCHAR) != 0)\n");
	printf ("\n");
	printf ("/* ctype replacements */\n");
	printf ("#define is_ascii(x) ((gmime_special_table[(unsigned char)(x)] & IS_ASCII) != 0)\n");
	printf ("#define is_blank(x) ((gmime_special_table[(unsigned char)(x)] & IS_BLANK) != 0)\n");
	printf ("\n");
	
	printf ("#define CHARS_LWSP \" \\t\\n\\r\"               /* linear whitespace chars */\n");
	printf ("#define CHARS_TSPECIAL \"()<>@,;:\\\\\\\"/[]?=\"\n");
	printf ("#define CHARS_SPECIAL \"()<>@,;:\\\\\\\".[]\"\n");
	printf ("#define CHARS_CSPECIAL \"()\\\\\\r\"	           /* not in comments */\n");
	printf ("#define CHARS_DSPECIAL \"[]\\\\\\r \\t\"         /* not in domains */\n");
	printf ("#define CHARS_ESPECIAL \"()<>@,;:\\\"/[]?.=_\" /* encoded word specials (rfc2047 5.1) */\n");
	printf ("#define CHARS_PSPECIAL \"!*+-/=_\"           /* encoded phrase specials (rfc2047 5.3) */\n");
	printf ("#define CHARS_ATTRCHAR \"*'%% \"              /* attribute-char from rfc2184 */\n");
	printf ("\n");
	
	printf ("#define GMIME_FOLD_LEN 76\n");
	
	return 0;
}
