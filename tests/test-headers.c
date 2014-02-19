/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2014 Jeffrey Stedfast
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

#include <gmime/gmime.h>

#include <stdio.h>
#include <stdlib.h>

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

static void
test_header_sync (void)
{
	InternetAddressList *list;
	InternetAddress *addr, *ia;
	GMimeMessage *message;
	GMimeObject *object;
	const char *value;
	GMimePart *part;
	
	part = g_mime_part_new_with_type ("application", "octet-stream");
	object = (GMimeObject *) part;
	
	testsuite_check ("content-type synchronization");
	try {
		/* first, check that the current Content-Type header
		 * value is "application/octet-stream" as expected */
		if (!(value = g_mime_object_get_header (object, "Content-Type")))
			throw (exception_new ("initial content-type header was unexpectedly null"));
		
		if (strcmp ("application/octet-stream", value) != 0)
			throw (exception_new ("initial content-type header had unexpected value"));
		
		/* now change the content-type's media type... */
		g_mime_content_type_set_media_type (object->content_type, "text");
		if (!(value = g_mime_object_get_header (object, "Content-Type")))
			throw (exception_new ("content-type header was unexpectedly null after changing type"));
		if (strcmp ("text/octet-stream", value) != 0)
			throw (exception_new ("content-type header had unexpected value after changing type"));
		
		/* now change the content-type's media subtype... */
		g_mime_content_type_set_media_subtype (object->content_type, "plain");
		if (!(value = g_mime_object_get_header (object, "Content-Type")))
			throw (exception_new ("content-type header was unexpectedly null after changing subtype"));
		if (strcmp ("text/plain", value) != 0)
			throw (exception_new ("content-type header had unexpected value after changing subtype"));
		
		/* now change the content-type's parameters by setting a param */
		g_mime_content_type_set_parameter (object->content_type, "format", "flowed");
		if (!(value = g_mime_object_get_header (object, "Content-Type")))
			throw (exception_new ("content-type header was unexpectedly null after setting a param"));
		if (strcmp ("text/plain; format=flowed", value) != 0)
			throw (exception_new ("content-type header had unexpected value after setting a param"));
		
		/* now change the content-type's parameters by setting a param list */
		g_mime_content_type_set_params (object->content_type, NULL);
		if (!(value = g_mime_object_get_header (object, "Content-Type")))
			throw (exception_new ("content-type header was unexpectedly null after setting params"));
		if (strcmp ("text/plain", value) != 0)
			throw (exception_new ("content-type header had unexpected value after setting params"));
		
		testsuite_check_passed ();
	} catch (ex) {
		testsuite_check_failed ("content-type header not synchronized: %s", ex->message);
	} finally;
	
	testsuite_check ("content-disposition synchronization");
	try {
		g_mime_object_set_disposition (object, "attachment");
		
		/* first, check that the current Content-Disposition header
		 * value is "application/octet-stream" as expected */
		if (!(value = g_mime_object_get_header (object, "Content-Disposition")))
			throw (exception_new ("initial content-disposition header was unexpectedly null"));
		
		if (strcmp ("attachment", value) != 0)
			throw (exception_new ("initial content-disposition header had unexpected value"));
		
		/* now change the content-disposition's disposition */
		g_mime_content_disposition_set_disposition (object->disposition, "inline");
		if (!(value = g_mime_object_get_header (object, "Content-Disposition")))
			throw (exception_new ("content-disposition header was unexpectedly null after changing type"));
		if (strcmp ("inline", value) != 0)
			throw (exception_new ("content-disposition header had unexpected value after changing type"));
		
		/* now change the content-disposition's parameters by setting a param */
		g_mime_content_disposition_set_parameter (object->disposition, "filename", "hello.txt");
		if (!(value = g_mime_object_get_header (object, "Content-Disposition")))
			throw (exception_new ("content-disposition header was unexpectedly null after setting a param"));
		if (strcmp ("inline; filename=hello.txt", value) != 0)
			throw (exception_new ("content-disposition header had unexpected value after setting a param"));
		
		/* now change the content-disposition's parameters by setting a param list */
		g_mime_content_disposition_set_params (object->disposition, NULL);
		if (!(value = g_mime_object_get_header (object, "Content-Disposition")))
			throw (exception_new ("content-disposition header was unexpectedly null after setting params"));
		if (strcmp ("inline", value) != 0)
			throw (exception_new ("content-disposition header had unexpected value after setting params"));
		
		testsuite_check_passed ();
	} catch (ex) {
		testsuite_check_failed ("content-disposition header not synchronized: %s", ex->message);
	} finally;
	
	g_object_unref (part);
	
	message = g_mime_message_new (TRUE);
	list = g_mime_message_get_recipients (message, GMIME_RECIPIENT_TYPE_TO);
	object = (GMimeObject *) message;
	
	testsuite_check ("address header synchronization");
	try {
		/* first, check that the To recipients are empty */
		if (list == NULL || internet_address_list_length (list) != 0)
			throw (exception_new ("unexpected initial internet address list"));
		
		/* now check that the initial header value is null */
		if ((value = g_mime_object_get_header (object, "To")) != NULL)
			throw (exception_new ("unexpected initial address list header"));
		
		/* now try adding an address */
		addr = internet_address_mailbox_new ("Tester", "tester@localhost.com");
		internet_address_list_add (list, addr);
		
		if (!(value = g_mime_object_get_header (object, "To")))
			throw (exception_new ("address list header unexpectedly null after adding recipient"));
		
		if (strcmp ("Tester <tester@localhost.com>", value) != 0)
			throw (exception_new ("unexpected address list header after adding recipient"));
		
		/* now let's try changing the address name to make sure signals properly chain up */
		internet_address_set_name (addr, "Eva Lucy-Ann Tester");
		if (!(value = g_mime_object_get_header (object, "To")))
			throw (exception_new ("address list header unexpectedly null after changing name"));
		
		if (strcmp ("Eva Lucy-Ann Tester <tester@localhost.com>", value) != 0)
			throw (exception_new ("unexpected address list header after changing name"));
		
		/* now let's try changing the address mailbox... */
		internet_address_mailbox_set_addr ((InternetAddressMailbox *) addr,
						   "evalucyann@ximian.com");
		if (!(value = g_mime_object_get_header (object, "To")))
			throw (exception_new ("address list header unexpectedly null after changing mailbox"));
		
		if (strcmp ("Eva Lucy-Ann Tester <evalucyann@ximian.com>", value) != 0)
			throw (exception_new ("unexpected address list header after changing mailbox"));
		
		/* now let's try inserting a group address */
		g_object_unref (addr);
		addr = internet_address_group_new ("Group");
		internet_address_list_insert (list, 0, addr);
		
		if (!(value = g_mime_object_get_header (object, "To")))
			throw (exception_new ("address list header unexpectedly null after inserting group"));
		
		if (strcmp ("Group: ;, Eva Lucy-Ann Tester <evalucyann@ximian.com>", value) != 0)
			throw (exception_new ("unexpected address list header after inserting group"));
		
		/* now let's try removing the original recipient */
		internet_address_list_remove_at (list, 1);
		if (!(value = g_mime_object_get_header (object, "To")))
			throw (exception_new ("address list header unexpectedly null after removing recipient"));
		
		if (strcmp ("Group: ;", value) != 0)
			throw (exception_new ("unexpected address list header after removing recipient"));
		
		/* now let's try adding an address to the group... */
		ia = internet_address_mailbox_new ("Tester", "tester@hotmail.com");
		internet_address_list_add (((InternetAddressGroup *) addr)->members, ia);
		if (!(value = g_mime_object_get_header (object, "To")))
			throw (exception_new ("address list header unexpectedly null after adding addr to group"));
		
		if (strcmp ("Group: Tester <tester@hotmail.com>;", value) != 0)
			throw (exception_new ("unexpected address list header after adding addr to group"));
		
		testsuite_check_passed ();
	} catch (ex) {
		testsuite_check_failed ("address header not synchronized: %s", ex->message);
	} finally;
	
	g_object_unref (message);
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
	
	testsuite_start ("header synchronization");
	test_header_sync ();
	testsuite_end ();
	
	g_mime_shutdown ();
	
	return testsuite_exit ();
}
