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
	GMimeHeaderIter *iter;
	guint i;
	
	list = header_list_new ();
	iter = g_mime_header_list_get_iter (list);
	
	/* make sure initial iter is valid */
	testsuite_check ("initial iter");
	try {
		if (!g_mime_header_iter_is_valid (iter))
			throw (exception_new ("invalid iter"));
		
		name = g_mime_header_iter_get_name (iter);
		value = g_mime_header_iter_get_value (iter);
		
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
			if (!g_mime_header_iter_next (iter))
				throw (exception_new ("failed to advance"));
			
			if (!g_mime_header_iter_is_valid (iter))
				throw (exception_new ("advanced but is invalid"));
			
			name = g_mime_header_iter_get_name (iter);
			value = g_mime_header_iter_get_value (iter);
			
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
		if (g_mime_header_iter_next (iter))
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
			if (!g_mime_header_iter_prev (iter))
				throw (exception_new ("failed to advance"));
			
			if (!g_mime_header_iter_is_valid (iter))
				throw (exception_new ("advanced but is invalid"));
			
			name = g_mime_header_iter_get_name (iter);
			value = g_mime_header_iter_get_value (iter);
			
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
		if (g_mime_header_iter_prev (iter))
			throw (exception_new ("should not have worked"));
		testsuite_check_passed ();
	} catch (ex) {
		testsuite_check_failed ("iter->prev past beginning of headers: %s", ex->message);
	} finally;
	
	g_mime_header_iter_free (iter);
	g_mime_header_list_destroy (list);
}

static void
test_iter_remove_all (void)
{
	GMimeHeaderIter *iter, *iter1;
	GMimeHeaderList *list;
	guint i = 0;
	
	list = header_list_new ();
	iter = g_mime_header_list_get_iter (list);
	
	testsuite_check ("removing all headers");
	try {
		while (g_mime_header_iter_remove (iter))
			i++;
		
		if (i != G_N_ELEMENTS (initial))
			throw (exception_new ("only removed %u of %u", i, G_N_ELEMENTS (initial)));
		
		if (g_mime_header_iter_is_valid (iter))
			throw (exception_new ("expected invalid iter"));
		
		testsuite_check_passed ();
	} catch (ex) {
		testsuite_check_failed ("removing all headers: %s", ex->message);
	} finally;
	
	g_mime_header_iter_free (iter);
	
	iter = g_mime_header_list_get_iter (list);
	
	testsuite_check ("empty list iter");
	try {
		if (g_mime_header_iter_is_valid (iter))
			throw (exception_new ("expected invalid iter"));
		
		testsuite_check_passed ();
	} catch (ex) {
		testsuite_check_failed ("empty list iter: %s", ex->message);
	} finally;
	
	g_mime_header_iter_free (iter);
	
	g_mime_header_list_destroy (list);
}

static void
test_iter_remove (void)
{
	GMimeHeaderIter *iter, *iter1, *iter2, *iter3, *iter4;
	const char *name, *value;
	GMimeHeaderList *list;
	guint i;
	
	list = header_list_new ();
	iter1 = g_mime_header_list_get_iter (list);
	
	testsuite_check ("iter copying");
	try {
		/* make iter2 point to the second header */
		iter2 = g_mime_header_iter_copy (iter1);
		if (!g_mime_header_iter_next (iter2))
			throw (exception_new ("iter2->next failed"));
		
		name = g_mime_header_iter_get_name (iter2);
		value = g_mime_header_iter_get_value (iter2);
		
		if (strcmp (initial[1].name, name) != 0 ||
		    strcmp (initial[1].value, value) != 0)
			throw (exception_new ("iter2 resulted in unexpected header"));
		
		/* make iter3 point to the third header */
		iter3 = g_mime_header_iter_copy (iter2);
		if (!g_mime_header_iter_next (iter3))
			throw (exception_new ("iter3->next failed"));
		
		name = g_mime_header_iter_get_name (iter3);
		value = g_mime_header_iter_get_value (iter3);
		
		if (strcmp (initial[2].name, name) != 0 ||
		    strcmp (initial[2].value, value) != 0)
			throw (exception_new ("iter3 resulted in unexpected header"));
		
		/* make iter4 point to the forth header */
		iter4 = g_mime_header_iter_copy (iter3);
		if (!g_mime_header_iter_next (iter4))
			throw (exception_new ("iter4->next failed"));
		
		name = g_mime_header_iter_get_name (iter4);
		value = g_mime_header_iter_get_value (iter4);
		
		if (strcmp (initial[3].name, name) != 0 ||
		    strcmp (initial[3].value, value) != 0)
			throw (exception_new ("iter4 resulted in unexpected header"));
		
		testsuite_check_passed ();
	} catch (ex) {
		testsuite_check_failed ("iter copying: %s", ex->message);
	} finally;
	
	testsuite_check ("iter remove");
	try {
		/* remove the third header */
		iter = g_mime_header_iter_copy (iter3);
		if (!g_mime_header_iter_remove (iter)) {
			g_mime_header_iter_free (iter);
			throw (exception_new ("iter::remove(3) failed"));
		}
		
		/* make sure iter now points to the same header as iter4 */
		if (!g_mime_header_iter_equal (iter, iter4)) {
			g_mime_header_iter_free (iter);
			throw (exception_new ("iter::remove (3) iter::equal(4) failed"));
		}
		
		g_mime_header_iter_free (iter);
		
		/* make sure that iter3 is invalid */
		if (g_mime_header_iter_is_valid (iter3))
			throw (exception_new ("iter::remove (3) iter3::isvalid() incorrect"));
		
		/* make sure that iter1, iter2, and iter4 are still valid */
		if (!g_mime_header_iter_is_valid (iter1))
			throw (exception_new ("iter::remove (3) iter1::isvalid() incorrect"));
		if (!g_mime_header_iter_is_valid (iter2))
			throw (exception_new ("iter::remove (3) iter2::isvalid() incorrect"));
		if (!g_mime_header_iter_is_valid (iter4))
			throw (exception_new ("iter::remove (3) iter4::isvalid() incorrect"));
		
		/* remove the first header */
		iter = g_mime_header_iter_copy (iter1);
		if (!g_mime_header_iter_remove (iter)) {
			g_mime_header_iter_free (iter);
			throw (exception_new ("iter::remove(1) failed"));
		}
		
		/* make sure iter now points to the same header as iter2 */
		if (!g_mime_header_iter_equal (iter, iter2)) {
			g_mime_header_iter_free (iter);
			throw (exception_new ("iter::remove (1) iter::equal(2) failed"));
		}
		
		g_mime_header_iter_free (iter);
		
		/* make sure that iter1 is invalid */
		if (g_mime_header_iter_is_valid (iter1))
			throw (exception_new ("iter::remove (1) iter1::isvalid() incorrect"));
		
		/* make sure that iter2 and iter4 are still valid */
		if (!g_mime_header_iter_is_valid (iter2))
			throw (exception_new ("iter::remove (1) iter2::isvalid() incorrect"));
		if (!g_mime_header_iter_is_valid (iter4))
			throw (exception_new ("iter::remove (1) iter4::isvalid() incorrect"));
		
		testsuite_check_passed ();
	} catch (ex) {
		testsuite_check_failed ("iter remove: %s", ex->message);
	} finally;
	
	g_mime_header_iter_free (iter1);
	g_mime_header_iter_free (iter2);
	g_mime_header_iter_free (iter3);
	g_mime_header_iter_free (iter4);
	
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
	
	testsuite_start ("removing headers");
	test_iter_remove ();
	testsuite_end ();
	
	g_mime_shutdown ();
	
	return testsuite_exit ();
}
