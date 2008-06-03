/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2008 Jeffrey Stedfast
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
 *  Foundation, 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>

#include <gmime/gmime.h>

#include "testsuite.h"

extern int verbose;

#define d(x)
#define v(x) if (verbose > 3) x

typedef struct {
	const char *name;
	const char *value;
} Header;

static Header initial[] = {
	{ "Received",   "first received header"            },
	{ "Received",   "second received header"           },
	{ "Received",   "third received header"            },
	{ "Date",       "Sat, 31 May 2008 08:56:43 EST"    },
	{ "From",       "someone@somewhere.com"            },
	{ "Sender",     "someoneelse@somewhere.com"        },
	{ "To",         "coworker@somewhere.com"           },
	{ "Subject",    "hey, check this out"              },
	{ "Message-Id", "<136734928.123728@localhost.com>" },
};

static GMimeHeaderList *
header_list_new (void)
{
	GMimeHeaderList *list;
	guint i;
	
	list = g_mime_header_list_new ();
	for (i = 0; i < G_N_ELEMENTS (initial); i++)
		g_mime_header_list_append (list, initial[i].name, initial[i].value);
	
	return list;
}

static void
test_iter_forward_back (void)
{
	const char *name, *value;
	GMimeHeaderList *list;
	GMimeHeaderIter iter;
	guint i;
	
	list = header_list_new ();
	
	/* make sure initial iter is valid */
	testsuite_check ("initial iter");
	try {
		if (!g_mime_header_list_get_iter (list, &iter))
			throw (exception_new ("get_iter() failed"));
		
		if (!g_mime_header_iter_is_valid (&iter))
			throw (exception_new ("invalid iter"));
		
		name = g_mime_header_iter_get_name (&iter);
		value = g_mime_header_iter_get_value (&iter);
		
		if (strcmp (initial[0].name, name) != 0 || strcmp (initial[0].value, value) != 0)
			throw (exception_new ("resulted in unexpected header"));
		testsuite_check_passed ();
	} catch (ex) {
		testsuite_check_failed ("initial iter: %s", ex->message);
	} finally;
	
	/* make sure iter->next works as expected */
	for (i = 1; i < G_N_ELEMENTS (initial); i++) {
		testsuite_check ("next iter[%u]", i);
		try {
			if (!g_mime_header_iter_next (&iter))
				throw (exception_new ("failed to advance"));
			
			if (!g_mime_header_iter_is_valid (&iter))
				throw (exception_new ("advanced but is invalid"));
			
			name = g_mime_header_iter_get_name (&iter);
			value = g_mime_header_iter_get_value (&iter);
			
			if (strcmp (initial[i].name, name) != 0 ||
			    strcmp (initial[i].value, value) != 0)
				throw (exception_new ("resulted in unexpected header"));
			testsuite_check_passed ();
		} catch (ex) {
			testsuite_check_failed ("next iter[%u]: %s", i, ex->message);
		} finally;
	}
	
	/* make sure trying to advance past the last header fails */
	testsuite_check ("iter->next past end of headers");
	try {
		if (g_mime_header_iter_next (&iter))
			throw (exception_new ("should not have worked"));
		testsuite_check_passed ();
	} catch (ex) {
		testsuite_check_failed ("iter->next past end of headers: %s", ex->message);
	} finally;
	
	/* make sure iter->prev works as expected */
	i--;
	while (i > 0) {
		testsuite_check ("prev iter[%u]", i);
		try {
			if (!g_mime_header_iter_prev (&iter))
				throw (exception_new ("failed to advance"));
			
			if (!g_mime_header_iter_is_valid (&iter))
				throw (exception_new ("advanced but is invalid"));
			
			name = g_mime_header_iter_get_name (&iter);
			value = g_mime_header_iter_get_value (&iter);
			
			if (strcmp (initial[i - 1].name, name) != 0 ||
			    strcmp (initial[i - 1].value, value) != 0)
				throw (exception_new ("resulted in unexpected header"));
			testsuite_check_passed ();
		} catch (ex) {
			testsuite_check_failed ("prev iter[%u]: %s", i, ex->message);
		} finally;
		
		i--;
	}
	
	/* make sure trying to advance prev of the first header fails */
	testsuite_check ("iter->prev past beginning of headers");
	try {
		if (g_mime_header_iter_prev (&iter))
			throw (exception_new ("should not have worked"));
		testsuite_check_passed ();
	} catch (ex) {
		testsuite_check_failed ("iter->prev past beginning of headers: %s", ex->message);
	} finally;
	
	g_mime_header_list_destroy (list);
}

static void
test_iter_remove_all (void)
{
	GMimeHeaderList *list;
	GMimeHeaderIter iter;
	guint i = 0;
	
	list = header_list_new ();
	
	g_mime_header_list_get_iter (list, &iter);
	
	testsuite_check ("removing all headers");
	try {
		while (g_mime_header_iter_remove (&iter))
			i++;
		
		if (i != G_N_ELEMENTS (initial))
			throw (exception_new ("only removed %u of %u", i, G_N_ELEMENTS (initial)));
		
		if (g_mime_header_iter_is_valid (&iter))
			throw (exception_new ("expected invalid iter"));
		
		testsuite_check_passed ();
	} catch (ex) {
		testsuite_check_failed ("removing all headers: %s", ex->message);
	} finally;
	
	g_mime_header_list_get_iter (list, &iter);
	
	testsuite_check ("empty list iter");
	try {
		if (g_mime_header_iter_is_valid (&iter))
			throw (exception_new ("expected invalid iter"));
		
		testsuite_check_passed ();
	} catch (ex) {
		testsuite_check_failed ("empty list iter: %s", ex->message);
	} finally;
	
	g_mime_header_list_destroy (list);
}

static void
test_iter_remove (void)
{
	GMimeHeaderIter iter, iter1, iter2, iter3;
	const char *name, *value;
	GMimeHeaderList *list;
	guint i;
	
	list = header_list_new ();
	
	g_mime_header_list_get_iter (list, &iter1);
	
	testsuite_check ("iter copying");
	try {
		/* make iter2 point to the second header */
		g_mime_header_iter_copy_to (&iter1, &iter2);
		if (!g_mime_header_iter_next (&iter2))
			throw (exception_new ("iter2->next failed"));
		
		name = g_mime_header_iter_get_name (&iter2);
		value = g_mime_header_iter_get_value (&iter2);
		
		if (strcmp (initial[1].name, name) != 0 ||
		    strcmp (initial[1].value, value) != 0)
			throw (exception_new ("iter2 resulted in unexpected header"));
		
		/* make iter3 point to the third header */
		g_mime_header_iter_copy_to (&iter2, &iter3);
		if (!g_mime_header_iter_next (&iter3))
			throw (exception_new ("iter3->next failed"));
		
		name = g_mime_header_iter_get_name (&iter3);
		value = g_mime_header_iter_get_value (&iter3);
		
		if (strcmp (initial[2].name, name) != 0 ||
		    strcmp (initial[2].value, value) != 0)
			throw (exception_new ("iter3 resulted in unexpected header"));
		
		testsuite_check_passed ();
	} catch (ex) {
		testsuite_check_failed ("iter copying: %s", ex->message);
	} finally;
	
	testsuite_check ("remove first header");
	try {
		/* remove the first header */
		g_mime_header_iter_copy_to (&iter1, &iter);
		if (!g_mime_header_iter_remove (&iter))
			throw (exception_new ("iter::remove() failed"));
		
		/* make sure iter now points to the 2nd header */
		name = g_mime_header_iter_get_name (&iter);
		value = g_mime_header_iter_get_value (&iter);
		
		if (strcmp (initial[1].name, name) != 0 ||
		    strcmp (initial[1].value, value) != 0)
			throw (exception_new ("iter doesn't point to 2nd header as expected"));
		
		/* make sure that the other iters have been invalidated */
		if (g_mime_header_iter_is_valid (&iter1))
			throw (exception_new ("iter::remove() iter1::isvalid() incorrect"));
		if (g_mime_header_iter_is_valid (&iter2))
			throw (exception_new ("iter::remove() iter2::isvalid() incorrect"));
		if (g_mime_header_iter_is_valid (&iter3))
			throw (exception_new ("iter::remove() iter3::isvalid() incorrect"));
		
		testsuite_check_passed ();
	} catch (ex) {
		testsuite_check_failed ("remove first header: %s", ex->message);
	} finally;
	
	testsuite_check ("remove last header");
	try {
		/* remove the last header */
		g_mime_header_iter_last (&iter);
		
		if (!g_mime_header_iter_remove (&iter))
			throw (exception_new ("iter::remove() failed"));
		
		/* iter should be invalid now because it couldn't advance to a header beyond the last */
		if (g_mime_header_iter_is_valid (&iter))
			throw (exception_new ("iter::remove() iter is valid when it shouldn't be"));
		
		testsuite_check_passed ();
	} catch (ex) {
		testsuite_check_failed ("remove last header: %s", ex->message);
	} finally;
	
	testsuite_check ("remove middle header");
	try {
		g_mime_header_list_get_iter (list, &iter);
		
		/* advance to a header in the middle somewhere... */
		g_mime_header_iter_next (&iter);
		g_mime_header_iter_next (&iter);
		
		/* we should now be pointing to the 3rd header (4th from the initial headers) */
		name = g_mime_header_iter_get_name (&iter);
		value = g_mime_header_iter_get_value (&iter);
		
		if (strcmp (initial[3].name, name) != 0 ||
		    strcmp (initial[3].value, value) != 0)
			throw (exception_new ("iter doesn't point to 3rd header as expected"));
		
		/* remove it */
		if (!g_mime_header_iter_remove (&iter))
			throw (exception_new ("iter::remove() failed"));
		
		/* make sure the iter is still valid */
		if (!g_mime_header_iter_is_valid (&iter))
			throw (exception_new ("iter::remove() iter isn't valid when it should be"));
		
		/* make sure iter now points to the 4th header */
		name = g_mime_header_iter_get_name (&iter);
		value = g_mime_header_iter_get_value (&iter);
		
		if (strcmp (initial[4].name, name) != 0 ||
		    strcmp (initial[4].value, value) != 0)
			throw (exception_new ("iter doesn't point to 4th header as expected"));
		
		testsuite_check_passed ();
	} catch (ex) {
		testsuite_check_failed ("remove first header: %s", ex->message);
	} finally;
	
	testsuite_check ("resulting lists match");
	try {
		g_mime_header_list_get_iter (list, &iter);
		i = 1;
		
		do {
			name = g_mime_header_iter_get_name (&iter);
			value = g_mime_header_iter_get_value (&iter);
			
			if (i == 3)
				i++;
			
			if (strcmp (initial[i].name, name) != 0 ||
			    strcmp (initial[i].value, value) != 0)
				throw (exception_new ("iter vs array mismatch @ index %u", i));
			
			i++;
		} while (g_mime_header_iter_next (&iter));
		
		if (++i != G_N_ELEMENTS (initial))
			throw (exception_new ("iter didn't have as many headers as expected"));
		
		testsuite_check_passed ();
	} catch (ex) {
		testsuite_check_failed ("resulting lists match: %s", ex->message);
	} finally;
	
	g_mime_header_list_destroy (list);
}

int main (int argc, char **argv)
{
	g_mime_init (0);
	
	testsuite_init (argc, argv);
	
	testsuite_start ("iterating forward and backward");
	test_iter_forward_back ();
	testsuite_end ();
	
	testsuite_start ("removing all headers");
	test_iter_remove_all ();
	testsuite_end ();
	
	testsuite_start ("removing individual headers");
	test_iter_remove ();
	testsuite_end ();
	
	g_mime_shutdown ();
	
	return testsuite_exit ();
}
