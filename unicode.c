/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2002 Ximian, Inc. (www.ximian.com)
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

#include "unicode.h"

const char unicode_skip[256] = {
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
	3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,4,4,4,4,4,4,4,4,5,5,5,5,6,6,1,1
};

#define UTF8_COMPUTE(ch, mask, len)					      \
  if (ch < 128) 							      \
    {									      \
      len = 1;								      \
      mask = 0x7f;							      \
    }									      \
  else if ((ch & 0xe0) == 0xc0) 					      \
    {									      \
      len = 2;								      \
      mask = 0x1f;							      \
    }									      \
  else if ((ch & 0xf0) == 0xe0) 					      \
    {									      \
      len = 3;								      \
      mask = 0x0f;							      \
    }									      \
  else if ((ch & 0xf8) == 0xf0) 	 				      \
    {									      \
      len = 4;								      \
      mask = 0x07;							      \
    }									      \
  else if ((ch & 0xfc) == 0xf8) 					      \
    {									      \
      len = 5;								      \
      mask = 0x03;							      \
    }									      \
  else if ((ch & 0xfe) == 0xfc) 					      \
    {									      \
      len = 6;								      \
      mask = 0x01;							      \
    }									      \
  else									      \
    len = -1;

#define UTF8_GET(result, chars, count, mask, len)			      \
  (result) = (chars)[0] & (mask);					      \
  for ((count) = 1; (count) < (len); ++(count))				      \
    {									      \
      if (((chars)[(count)] & 0xc0) != 0x80)				      \
	{								      \
	  (result) = -1;						      \
	  break;							      \
	}								      \
      (result) <<= 6;							      \
      (result) |= ((chars)[(count)] & 0x3f);				      \
    }

#define UNICODE_VALID(ch)                   \
    ((ch) < 0x110000 &&                     \
     ((ch) < 0xD800 || (ch) >= 0xE000) &&   \
     (ch) != 0xFFFE && (ch) != 0xFFFF)



#define unicode_next_char(p) (char *)((p) + unicode_skip[*(unsigned char *)(p)])

unichar
unicode_get_char (const char  *p)
{
	unsigned char c = (unsigned char) *p;
	int i, mask = 0, len;
	unichar result;
	
	UTF8_COMPUTE (c, mask, len);
	if (len == -1)
		return (unichar) -1;
	UTF8_GET (result, p, i, mask, len);
	
	return result;
}

gboolean
unichar_validate (unichar ch)
{
	return UNICODE_VALID (ch);
}
