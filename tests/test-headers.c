/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2017 Jeffrey Stedfast
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
	
	list = g_mime_header_list_new (g_mime_parser_options_get_default ());
	for (i = 1; i < G_N_ELEMENTS (initial); i++)
		g_mime_header_list_append (list, initial[i].name, initial[i].value);
	g_mime_header_list_prepend (list, initial[0].name, initial[0].value);
	
	return list;
}

static void
test_indexing (void)
{
	const char *name, *value;
	GMimeHeaderList *list;
	GMimeHeader *header;
	int index, count;
	
	list = header_list_new ();
	
	count = g_mime_header_list_get_count (list);
	
	/* make sure indexing works as expected */
	for (index = 0; index < G_N_ELEMENTS (initial); index++) {
		testsuite_check ("headers[%d]", index);
		try {
			if (!(header = g_mime_header_list_get_header_at (list, index)))
				throw (exception_new ("failed to get header at index"));
			
			name = g_mime_header_get_name (header);
			value = g_mime_header_get_value (header);
			
			if (strcmp (initial[index].name, name) != 0 ||
			    strcmp (initial[index].value, value) != 0)
				throw (exception_new ("resulted in unexpected header"));
			testsuite_check_passed ();
		} catch (ex) {
			testsuite_check_failed ("next iter[%d]: %s", index, ex->message);
		} finally;
	}
	
	/* make sure trying to advance past the last header fails */
	testsuite_check ("indexing past end of headers");
	try {
		if (g_mime_header_list_get_header_at (list, index) != NULL)
			throw (exception_new ("should not have worked"));
		testsuite_check_passed ();
	} catch (ex) {
		testsuite_check_failed ("indexing past end of headers: %s", ex->message);
	} finally;
	
	g_object_unref (list);
}

static void
test_remove (void)
{
	const char *name, *value;
	GMimeHeaderList *list;
	GMimeHeader *header;
	guint i;
	
	list = header_list_new ();
	
	testsuite_check ("remove first header");
	try {
		/* remove the first header */
		if (!g_mime_header_list_remove (list, initial[0].name))
			throw (exception_new ("failed to remove header"));
		
		/* make sure the first header is now the same as the second original header */
		header = g_mime_header_list_get_header_at (list, 0);
		name = g_mime_header_get_name (header);
		value = g_mime_header_get_value (header);
		
		if (strcmp (initial[1].name, name) != 0 ||
		    strcmp (initial[1].value, value) != 0)
			throw (exception_new ("expected second Received header"));
		
		/* make sure that the internal hash table was properly updated */
		if (!(header = g_mime_header_list_get_header (list, "Received")))
			throw (exception_new ("lookup of Received header failed"));
		
		if (!(value = g_mime_header_get_value (header)))
			throw (exception_new ("getting Received header value failed"));
		
		if (strcmp (initial[1].value, value) != 0)
			throw (exception_new ("expected second Received header value"));
		
		testsuite_check_passed ();
	} catch (ex) {
		testsuite_check_failed ("remove first header: %s", ex->message);
	} finally;
	
	testsuite_check ("remove last header");
	try {
		/* remove the last header */
		g_mime_header_list_remove (list, "Message-Id");
		
		if ((header = g_mime_header_list_get_header (list, "Message-Id")) != NULL)
			throw (exception_new ("lookup of Message-Id should have failed"));
		
		testsuite_check_passed ();
	} catch (ex) {
		testsuite_check_failed ("remove last header: %s", ex->message);
	} finally;
	
	g_object_unref (list);
}

static void
test_remove_at (void)
{
	const char *name, *value;
	GMimeHeaderList *list;
	GMimeHeader *header;
	int count;
	guint i;
	
	list = header_list_new ();
	
	testsuite_check ("remove first header");
	try {
		/* remove the first header */
		g_mime_header_list_remove_at (list, 0);
		
		/* make sure the first header is now the same as the second original header */
		header = g_mime_header_list_get_header_at (list, 0);
		name = g_mime_header_get_name (header);
		value = g_mime_header_get_value (header);
		
		if (strcmp (initial[1].name, name) != 0 ||
		    strcmp (initial[1].value, value) != 0)
			throw (exception_new ("expected second Received header"));
		
		/* make sure that the internal hash table was properly updated */
		if (!(header = g_mime_header_list_get_header (list, "Received")))
			throw (exception_new ("lookup of Received header failed"));
		
		if (!(value = g_mime_header_get_value (header)))
			throw (exception_new ("getting Received header value failed"));
		
		if (strcmp (initial[1].value, value) != 0)
			throw (exception_new ("expected second Received header value"));
		
		testsuite_check_passed ();
	} catch (ex) {
		testsuite_check_failed ("remove first header: %s", ex->message);
	} finally;
	
	testsuite_check ("remove last header");
	try {
		/* remove the last header */
		count = g_mime_header_list_get_count (list);
		g_mime_header_list_remove_at (list, count - 1);
		
		if ((header = g_mime_header_list_get_header (list, "Message-Id")) != NULL)
			throw (exception_new ("lookup of Message-Id should have failed"));
		
		testsuite_check_passed ();
	} catch (ex) {
		testsuite_check_failed ("remove last header: %s", ex->message);
	} finally;
	
	g_object_unref (list);
}

static void
test_content_type_sync (void)
{
	GMimeHeaderList *headers;
	GMimeContentType *type;
	GMimeParamList *params;
	GMimeObject *object;
	GMimeHeader *header;
	const char *value;
	GMimePart *part;
	
	object = (GMimeObject *) (part = g_mime_part_new ());
	headers = g_mime_object_get_header_list (object);
	
	testsuite_check ("content-type synchronization");
	try {
		/* first, check that the current Content-Type header
		 * value is "application/octet-stream" as expected */
		if (!(value = g_mime_object_get_header (object, "Content-Type")))
			throw (exception_new ("initial content-type header was unexpectedly null"));
		
		if (strcmp ("application/octet-stream", value) != 0)
			throw (exception_new ("initial content-type header had unexpected value: %s", value));
		
		/* now change the content-type's media type... */
		type = g_mime_object_get_content_type (object);
		g_mime_content_type_set_media_type (type, "text");
		if (!(value = g_mime_object_get_header (object, "Content-Type")))
			throw (exception_new ("content-type header was unexpectedly null after changing type"));
		if (strcmp ("text/octet-stream", value) != 0)
			throw (exception_new ("content-type header had unexpected value after changing type"));
		
		/* now change the content-type's media subtype... */
		g_mime_content_type_set_media_subtype (type, "plain");
		if (!(value = g_mime_object_get_header (object, "Content-Type")))
			throw (exception_new ("content-type header was unexpectedly null after changing subtype"));
		if (strcmp ("text/plain", value) != 0)
			throw (exception_new ("content-type header had unexpected value after changing subtype"));
		
		/* now change the content-type's parameters by setting a param */
		g_mime_content_type_set_parameter (type, "format", "flowed");
		if (!(value = g_mime_object_get_header (object, "Content-Type")))
			throw (exception_new ("content-type header was unexpectedly null after setting a param"));
		if (strcmp ("text/plain; format=flowed", value) != 0)
			throw (exception_new ("content-type header had unexpected value after setting a param"));
		
		/* now change the content-type's parameters by clearing the params */
		params = g_mime_content_type_get_parameters (type);
		g_mime_param_list_clear (params);
		if (!(value = g_mime_object_get_header (object, "Content-Type")))
			throw (exception_new ("content-type header was unexpectedly null after clearing params"));
		if (strcmp ("text/plain", value) != 0)
			throw (exception_new ("content-type header had unexpected value after setting params"));
		
		/* let's try this in reverse... set the header value and make sure GMimeContentType gets updated */
		header = g_mime_header_list_get_header_at (headers, 0);
		g_mime_header_set_value (header, "text/html; charset=utf-8");
		type = g_mime_object_get_content_type (object);
		if (!g_mime_content_type_is_type (type, "text", "html"))
			throw (exception_new ("GMimeContentType object was not updated"));
		
		testsuite_check_passed ();
	} catch (ex) {
		testsuite_check_failed ("content-type header not synchronized: %s", ex->message);
	} finally;
	
	g_object_unref (part);
}

static void
test_disposition_sync (void)
{
	GMimeContentDisposition *disposition;
	GMimeHeaderList *headers;
	GMimeParamList *params;
	GMimeObject *object;
	GMimeHeader *header;
	const char *value;
	GMimePart *part;
	
	object = (GMimeObject *) (part = g_mime_part_new ());
	headers = g_mime_object_get_header_list (object);
	
	testsuite_check ("content-disposition synchronization");
	try {
		g_mime_object_set_disposition (object, "attachment");
		
		/* first, check that the current Content-Disposition header
		 * value is "application/octet-stream" as expected */
		if (!(value = g_mime_object_get_header (object, "Content-Disposition")))
			throw (exception_new ("initial content-disposition header was unexpectedly null"));
		
		if (strcmp ("attachment", value) != 0)
			throw (exception_new ("initial content-disposition header had unexpected value: %s", value));
		
		/* now change the content-disposition's disposition */
		disposition = g_mime_object_get_content_disposition (object);
		g_mime_content_disposition_set_disposition (disposition, "inline");
		if (!(value = g_mime_object_get_header (object, "Content-Disposition")))
			throw (exception_new ("content-disposition header was unexpectedly null after changing type"));
		if (strcmp ("inline", value) != 0)
			throw (exception_new ("content-disposition header had unexpected value after changing type"));
		
		/* now change the content-disposition's parameters by setting a param */
		g_mime_content_disposition_set_parameter (disposition, "filename", "hello.txt");
		if (!(value = g_mime_object_get_header (object, "Content-Disposition")))
			throw (exception_new ("content-disposition header was unexpectedly null after setting a param"));
		if (strcmp ("inline; filename=hello.txt", value) != 0)
			throw (exception_new ("content-disposition header had unexpected value after setting a param"));
		
		/* now change the content-disposition's parameters by clearing the params */
		params = g_mime_content_disposition_get_parameters (disposition);
		g_mime_param_list_clear (params);
		if (!(value = g_mime_object_get_header (object, "Content-Disposition")))
			throw (exception_new ("content-disposition header was unexpectedly null after setting params"));
		if (strcmp ("inline", value) != 0)
			throw (exception_new ("content-disposition header had unexpected value after setting params"));
		
		/* let's try this in reverse... set the header value and make sure GMimeContentDisposition gets updated */
		header = g_mime_header_list_get_header_at (headers, 1);
		g_mime_header_set_value (header, "attachment; filename=xyz");
		disposition = g_mime_object_get_content_disposition (object);
		if (!g_mime_content_disposition_is_attachment (disposition))
			throw (exception_new ("GMimeContentDisposition object was not updated"));
		
		testsuite_check_passed ();
	} catch (ex) {
		testsuite_check_failed ("content-disposition header not synchronized: %s", ex->message);
	} finally;
	
	g_object_unref (part);
}

static void
test_address_sync (void)
{
	InternetAddress *addr, *ia;
	InternetAddressList *list;
	GMimeHeaderList *headers;
	GMimeParamList *params;
	GMimeMessage *message;
	GMimeObject *object;
	const char *value;
	
	message = g_mime_message_new (TRUE);
	list = g_mime_message_get_addresses (message, GMIME_ADDRESS_TYPE_TO);
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
			throw (exception_new ("unexpected address list header after changing name: %s", value));
		
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
	g_mime_init ();
	
	testsuite_init (argc, argv);
	
	testsuite_start ("indexing");
	test_indexing ();
	testsuite_end ();

	testsuite_start ("removing");
	test_remove ();
	testsuite_end ();
	
	testsuite_start ("removing at an index");
	test_remove_at ();
	testsuite_end ();
	
	testsuite_start ("header synchronization");
	test_content_type_sync ();
	test_disposition_sync ();
	test_address_sync ();
	testsuite_end ();
	
	g_mime_shutdown ();
	
	return testsuite_exit ();
}
