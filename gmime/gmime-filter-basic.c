/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2001 Ximian, Inc. (www.ximian.com)
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


#include "gmime-filter-basic.h"
#include "gmime-utils.h"

static void filter_destroy (GMimeFilter *filter);
static GMimeFilter *filter_copy (GMimeFilter *filter);
static void filter_filter (GMimeFilter *filter, char *in, size_t len, 
			   size_t prespace, char **out, 
			   size_t *outlen, size_t *outprespace);
static void filter_complete (GMimeFilter *filter, char *in, size_t len, 
			     size_t prespace, char **out, 
			     size_t *outlen, size_t *outprespace);
static void filter_reset (GMimeFilter *filter);

static GMimeFilter filter_template = {
	NULL, NULL, NULL, NULL,
	0, 0, NULL, 0, 0,
	filter_destroy,
	filter_copy,
	filter_filter,
	filter_complete,
	filter_reset,
};


/**
 * g_mime_filter_basic_new_type:
 * @type:
 *
 * Returns a new basic filter of type @type.
 **/
GMimeFilter *
g_mime_filter_basic_new_type (GMimeFilterBasicType type)
{
	GMimeFilterBasic *new;
	
	new = g_new (GMimeFilterBasic, 1);
	
	new->type = type;
	new->state = 0;
	new->save = 0;
	new->uulen = '\0';
	
	g_mime_filter_construct (GMIME_FILTER (new), &filter_template);
	
	filter_reset (GMIME_FILTER (new));
	
	return GMIME_FILTER (new);
}


static void
filter_destroy (GMimeFilter *filter)
{
	g_free (filter);
}

static GMimeFilter *
filter_copy (GMimeFilter *filter)
{
	GMimeFilterBasic *basic = (GMimeFilterBasic *) filter;
	
	return g_mime_filter_basic_new_type (basic->type);
}

/* here we do all of the basic mime filtering */
static void
filter_filter (GMimeFilter *filter, char *in, size_t len, size_t prespace,
	       char **out, size_t *outlen, size_t *outprespace)
{
	GMimeFilterBasic *basic = (GMimeFilterBasic *) filter;
	int newlen = 0;
	
	switch (basic->type) {
	case GMIME_FILTER_BASIC_BASE64_ENC:
		/* wont go to more than 2x size (overly conservative) */
		g_mime_filter_set_size (filter, len * 2 + 6, FALSE);
		newlen = g_mime_utils_base64_encode_step (in, len, filter->outbuf, &basic->state, &basic->save);
		g_assert (newlen <= len * 2 + 6);
		break;
	case GMIME_FILTER_BASIC_QP_ENC:
		/* *4 is overly conservative, but will do */
		g_mime_filter_set_size (filter, len * 4 + 4, FALSE);
		newlen = g_mime_utils_quoted_encode_step (in, len, filter->outbuf, &basic->state, &basic->save);
		g_assert (newlen <= len * 4 + 4);
		break;
	case GMIME_FILTER_BASIC_UU_ENC:
		g_message ("FIXME: Implement this");
		newlen = 0;
		break;
	case GMIME_FILTER_BASIC_BASE64_DEC:
		/* output can't possibly exceed the input size */
		g_mime_filter_set_size (filter, len + 3, FALSE);
		newlen = g_mime_utils_base64_decode_step (in, len, filter->outbuf, &basic->state, &basic->save);
		g_assert (newlen <= len + 3);
		break;
	case GMIME_FILTER_BASIC_QP_DEC:
		/* output can't possibly exceed the input size */
		g_mime_filter_set_size (filter, len, FALSE);
		newlen = g_mime_utils_quoted_decode_step (in, len, filter->outbuf, &basic->state, &basic->save);
		g_assert (newlen <= len);
		break;
	case GMIME_FILTER_BASIC_UU_DEC:
		/* output can't possibly exceed the input size */
		g_mime_filter_set_size (filter, len, FALSE);
		newlen = g_mime_utils_uudecode_step (in, len, filter->outbuf, &basic->state, &basic->save, &basic->uulen);
		break;
	}
	
	*out = filter->outbuf;
	*outlen = newlen;
	*outprespace = filter->outpre;
}

static void
filter_complete (GMimeFilter *filter, char *in, size_t len, size_t prespace,
		 char **out, size_t *outlen, size_t *outprespace)
{
	GMimeFilterBasic *basic = (GMimeFilterBasic *) filter;
	int newlen = 0;
	
	switch (basic->type) {
	case GMIME_FILTER_BASIC_BASE64_ENC:
		/* wont go to more than 2x size (overly conservative) */
		g_mime_filter_set_size (filter, len*2+6, FALSE);
		newlen = g_mime_utils_base64_encode_close (in, len, filter->outbuf, &basic->state, &basic->save);
		g_assert (newlen <= len * 2 + 6);
		break;
	case GMIME_FILTER_BASIC_QP_ENC:
		/* *4 is definetly more than needed ... */
		g_mime_filter_set_size (filter, len * 4 + 4, FALSE);
		newlen = g_mime_utils_quoted_encode_close (in, len, filter->outbuf, &basic->state, &basic->save);
		g_assert (newlen <= len * 4 + 4);
		break;
	case GMIME_FILTER_BASIC_UU_ENC:
		g_message ("FIXME: Implement this");
		newlen = 0;
		break;
	case GMIME_FILTER_BASIC_BASE64_DEC:
		/* output can't possibly exceed the input size */
 		g_mime_filter_set_size (filter, len, FALSE);
		newlen = g_mime_utils_base64_decode_step (in, len, filter->outbuf, &basic->state, &basic->save);
		g_assert (newlen <= len);
		break;
	case GMIME_FILTER_BASIC_QP_DEC:
		/* output can't possibly exceed the input size */
		g_mime_filter_set_size (filter, len, FALSE);
		newlen = g_mime_utils_quoted_decode_step (in, len, filter->outbuf, &basic->state, &basic->save);
		g_assert (newlen <= len);
		break;
	case GMIME_FILTER_BASIC_UU_DEC:
		/* output can't possibly exceed the input size */
		g_mime_filter_set_size (filter, len, FALSE);
		newlen = g_mime_utils_uudecode_step (in, len, filter->outbuf, &basic->state, &basic->save, &basic->uulen);
		break;
	}
	
	*out = filter->outbuf;
	*outlen = newlen;
	*outprespace = filter->outpre;
}

/* should this 'flush' outstanding state/data bytes? */
static void
filter_reset (GMimeFilter *filter)
{
	GMimeFilterBasic *basic = (GMimeFilterBasic *) filter;
	
	switch (basic->type) {
	case GMIME_FILTER_BASIC_QP_ENC:
		basic->state = -1;
		break;
	default:
		basic->state = 0;
	}
	basic->save = 0;
	basic->uulen = '\0';
}
