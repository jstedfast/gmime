/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2001 Ximain, Inc. (www.ximian.com)
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

static unsigned short gmime_special_table[256] = {
	  5,  5,  5,  5,  5,  5,  5,  5,  5,231,  7,  5,  5, 39,  5,  5,
	  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,
	242,448, 76,192,192,192,192,192, 76, 76,448,448, 76,448, 72,324,
	448,448,448,448,448,448,448,448,448,448, 76, 76, 76,  4, 76, 68,
	 76,448,448,448,448,448,448,448,448,448,448,448,448,448,448,448,
	448,448,448,448,448,448,448,448,448,448,448,108,236,108,192, 64,
	192,448,448,448,448,448,448,448,448,448,448,448,448,448,448,448,
	448,448,448,448,448,448,448,448,448,448,448,192,192,192,192,  5,
	  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
};

enum {
	IS_CTRL		= 1<<0,
	IS_LWSP		= 1<<1,
	IS_TSPECIAL	= 1<<2,
	IS_SPECIAL	= 1<<3,
	IS_SPACE	= 1<<4,
	IS_DSPECIAL	= 1<<5,
	IS_QPSAFE	= 1<<6,
	IS_ESAFE	= 1<<7,	/* encoded word safe */
	IS_PSAFE	= 1<<8,	/* encoded word in phrase safe */
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
#define is_especial(x) ((gmime_special_table[(unsigned char)(x)] & IS_ESPECIAL) != 0)
#define is_psafe(x) ((gmime_special_table[(unsigned char)(x)] & IS_PSAFE) != 0)

#ifndef HAVE_ISBLANK
#define isblank(c) ((c) == ' ' || (c) == '\t')
#endif /* HAVE_ISBLANK */

#define CHARS_LWSP " \t\n\r"               /* linear whitespace chars */
#define CHARS_TSPECIAL "()<>@,;:\\\"/[]?="
#define CHARS_SPECIAL "()<>@,;:\\\".[]"
#define CHARS_CSPECIAL "()\\\r"	           /* not in comments */
#define CHARS_DSPECIAL "[]\\\r \t"	   /* not in domains */
#define CHARS_ESPECIAL "()<>@,;:\"/[]?.="  /* encoded word specials (rfc2047 5.1) */
#define CHARS_PSPECIAL "!*+-/"             /* encoded phrase specials (rfc2047 5.3) */
