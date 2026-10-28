/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2022 Jeffrey Stedfast
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
		g_mime_header_list_append (list, initial[i].name, initial[i].value, NULL);
	g_mime_header_list_prepend (list, initial[0].name, initial[0].value, NULL);
	
	return list;
}

static void
test_indexing (void)
{
	const char *name, *value, *raw_value;
	GMimeHeaderList *list;
	GMimeHeader *header;
	int index;
	size_t len;
	
	list = header_list_new ();
	
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
				throw (exception_new ("resulted in unexpected value"));
			
			if (!(raw_value = testsuite_header_raw_text (header)))
				throw (exception_new ("null raw value"));
			
			len = strlen (raw_value);
			if (raw_value[len - 1] != '\n')
				throw (exception_new ("raw value does not end with a \\n"));
			
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
	const char *raw_value, *value;
	GMimeHeaderList *headers;
	GMimeContentType *type;
	GMimeParamList *params;
	GMimeEntity *object;
	GMimeHeader *header;
	GMimePart *part;
	
	object = (GMimeEntity *) (part = g_mime_part_new ());
	headers = g_mime_entity_get_header_list (object);
	
	testsuite_check ("content-type synchronization");
	try {
		/* first, check that the current Content-Type header
		 * value is "application/octet-stream" as expected */
		if (!(value = g_mime_entity_get_header (object, "Content-Type")))
			throw (exception_new ("initial content-type header was unexpectedly null"));
		
		if (strcmp ("application/octet-stream", value) != 0)
			throw (exception_new ("initial content-type header had unexpected value: %s", value));
		
		/* now change the content-type's media type... */
		type = g_mime_entity_get_content_type (object);
		g_mime_content_type_set_media_type (type, "text");
		if (!(value = g_mime_entity_get_header (object, "Content-Type")))
			throw (exception_new ("content-type header was unexpectedly null after changing type"));
		if (strcmp ("text/octet-stream", value) != 0)
			throw (exception_new ("content-type header had unexpected value after changing type"));
		header = g_mime_header_list_get_header (headers, "Content-Type");
		if (!(raw_value = testsuite_header_raw_text (header)))
			throw (exception_new ("content-type raw_value was unexpectedly null after changing type"));
		if (strcmp (" text/octet-stream\n", raw_value) != 0)
			throw (exception_new ("content-type raw_value had unexpected value after changing type"));
		
		/* now change the content-type's media subtype... */
		g_mime_content_type_set_media_subtype (type, "plain");
		if (!(value = g_mime_entity_get_header (object, "Content-Type")))
			throw (exception_new ("content-type header was unexpectedly null after changing subtype"));
		if (strcmp ("text/plain", value) != 0)
			throw (exception_new ("content-type header had unexpected value after changing subtype"));
		header = g_mime_header_list_get_header (headers, "Content-Type");
		if (!(raw_value = testsuite_header_raw_text (header)))
			throw (exception_new ("content-type raw_value was unexpectedly null after changing subtype"));
		if (strcmp (" text/plain\n", raw_value) != 0)
			throw (exception_new ("content-type raw_value had unexpected value after changing subtype"));
		
		/* now change the content-type's parameters by setting a param */
		g_mime_content_type_set_parameter (type, "format", "flowed");
		if (!(value = g_mime_entity_get_header (object, "Content-Type")))
			throw (exception_new ("content-type header was unexpectedly null after setting a param"));
		if (strcmp ("text/plain; format=flowed", value) != 0)
			throw (exception_new ("content-type header had unexpected value after setting a param"));
		header = g_mime_header_list_get_header (headers, "Content-Type");
		if (!(raw_value = testsuite_header_raw_text (header)))
			throw (exception_new ("content-type raw_value was unexpectedly null after setting a param"));
		if (strcmp (" text/plain; format=flowed\n", raw_value) != 0)
			throw (exception_new ("content-type raw_value had unexpected value after setting a param"));
		
		/* now change the content-type's parameters by clearing the params */
		params = g_mime_content_type_get_parameters (type);
		g_mime_param_list_clear (params);
		if (!(value = g_mime_entity_get_header (object, "Content-Type")))
			throw (exception_new ("content-type header was unexpectedly null after clearing params"));
		if (strcmp ("text/plain", value) != 0)
			throw (exception_new ("content-type header had unexpected value after clearing params"));
		header = g_mime_header_list_get_header (headers, "Content-Type");
		if (!(raw_value = testsuite_header_raw_text (header)))
			throw (exception_new ("content-type raw_value was unexpectedly null after clearing params"));
		if (strcmp (" text/plain\n", raw_value) != 0)
			throw (exception_new ("content-type raw_value had unexpected value after clearing params"));
		
		/* let's try this in reverse... set the header value and make sure GMimeContentType gets updated */
		header = g_mime_header_list_get_header_at (headers, 0);
		g_mime_header_set_value (header, NULL, "text/html; charset=utf-8", NULL);
		type = g_mime_entity_get_content_type (object);
		if (!g_mime_content_type_is_type (type, "text", "html"))
			throw (exception_new ("GMimeContentType object was not updated"));
		header = g_mime_header_list_get_header (headers, "Content-Type");
		if (!(raw_value = testsuite_header_raw_text (header)))
			throw (exception_new ("content-type raw_value was unexpectedly null after setting value"));
		if (strcmp (" text/html; charset=utf-8\n", raw_value) != 0)
			throw (exception_new ("content-type raw_value had unexpected value after setting value"));
		
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
	const char *raw_value, *value;
	GMimeHeaderList *headers;
	GMimeParamList *params;
	GMimeEntity *object;
	GMimeHeader *header;
	GMimePart *part;
	
	object = (GMimeEntity *) (part = g_mime_part_new ());
	headers = g_mime_entity_get_header_list (object);
	
	testsuite_check ("content-disposition synchronization");
	try {
		g_mime_entity_set_disposition (object, "attachment");
		
		/* first, check that the current Content-Disposition header
		 * value is "application/octet-stream" as expected */
		if (!(value = g_mime_entity_get_header (object, "Content-Disposition")))
			throw (exception_new ("initial content-disposition header was unexpectedly null"));
		if (strcmp ("attachment", value) != 0)
			throw (exception_new ("initial content-disposition header had unexpected value: %s", value));
		header = g_mime_header_list_get_header (headers, "Content-Disposition");
		if (!(raw_value = testsuite_header_raw_text (header)))
			throw (exception_new ("initial content-disposition raw_value was unexpectedly null"));
		if (strcmp (" attachment\n", raw_value) != 0)
			throw (exception_new ("initial content-disposition raw_value had unexpected value: %s", raw_value));
		
		/* now change the content-disposition's disposition */
		disposition = g_mime_entity_get_content_disposition (object);
		g_mime_content_disposition_set_disposition (disposition, "inline");
		if (!(value = g_mime_entity_get_header (object, "Content-Disposition")))
			throw (exception_new ("content-disposition header was unexpectedly null after changing type"));
		if (strcmp ("inline", value) != 0)
			throw (exception_new ("content-disposition header had unexpected value after changing type"));
		header = g_mime_header_list_get_header (headers, "Content-Disposition");
		if (!(raw_value = testsuite_header_raw_text (header)))
			throw (exception_new ("content-disposition raw_value was unexpectedly null after changing type"));
		if (strcmp (" inline\n", raw_value) != 0)
			throw (exception_new ("content-disposition raw_value had unexpected value after changing type: %s", raw_value));
		
		/* now change the content-disposition's parameters by setting a param */
		g_mime_content_disposition_set_parameter (disposition, "filename", "hello.txt");
		if (!(value = g_mime_entity_get_header (object, "Content-Disposition")))
			throw (exception_new ("content-disposition header was unexpectedly null after setting a param"));
		if (strcmp ("inline; filename=hello.txt", value) != 0)
			throw (exception_new ("content-disposition header had unexpected value after setting a param"));
		header = g_mime_header_list_get_header (headers, "Content-Disposition");
		if (!(raw_value = testsuite_header_raw_text (header)))
			throw (exception_new ("content-disposition raw_value was unexpectedly null after setting a param"));
		if (strcmp (" inline; filename=hello.txt\n", raw_value) != 0)
			throw (exception_new ("content-disposition raw_value had unexpected value after setting a param: %s", raw_value));
		
		/* now change the content-disposition's parameters by clearing the params */
		params = g_mime_content_disposition_get_parameters (disposition);
		g_mime_param_list_clear (params);
		if (!(value = g_mime_entity_get_header (object, "Content-Disposition")))
			throw (exception_new ("content-disposition header was unexpectedly null after clearing params"));
		if (strcmp ("inline", value) != 0)
			throw (exception_new ("content-disposition header had unexpected value after clearing params"));
		header = g_mime_header_list_get_header (headers, "Content-Disposition");
		if (!(raw_value = testsuite_header_raw_text (header)))
			throw (exception_new ("content-disposition raw_value was unexpectedly null after clearing params"));
		if (strcmp (" inline\n", raw_value) != 0)
			throw (exception_new ("content-disposition raw_value had unexpected value after clearing params: %s", raw_value));
		
		/* let's try this in reverse... set the header value and make sure GMimeContentDisposition gets updated */
		header = g_mime_header_list_get_header_at (headers, 1);
		g_mime_header_set_value (header, NULL, "attachment; filename=xyz", NULL);
		disposition = g_mime_entity_get_content_disposition (object);
		if (!g_mime_content_disposition_is_attachment (disposition))
			throw (exception_new ("GMimeContentDisposition object was not updated"));
		header = g_mime_header_list_get_header (headers, "Content-Disposition");
		if (!(raw_value = testsuite_header_raw_text (header)))
			throw (exception_new ("content-disposition raw_value was unexpectedly null after setting value"));
		if (strcmp (" attachment; filename=xyz\n", raw_value) != 0)
			throw (exception_new ("content-disposition raw_value had unexpected value after setting value: %s", raw_value));
		
		testsuite_check_passed ();
	} catch (ex) {
		testsuite_check_failed ("content-disposition header not synchronized: %s", ex->message);
	} finally;
	
	g_object_unref (part);
}

static void
test_address_sync (void)
{
	const char *raw_value, *value;
	GMimeInternetAddress *addr, *ia;
	GMimeInternetAddressList *list;
	GMimeHeaderList *headers;
	GMimeMessage *message;
	GMimeEntity *object;
	GMimeHeader *header;
	
	message = g_mime_message_new (TRUE);
	list = g_mime_message_get_addresses (message, GMIME_ADDRESS_TYPE_TO);
	object = (GMimeEntity *) message;
	headers = object->headers;
	
	testsuite_check ("address header synchronization");
	try {
		/* first, check that the To recipients are empty */
		if (list == NULL || g_mime_internet_address_list_length (list) != 0)
			throw (exception_new ("unexpected initial internet address list"));
		
		/* now check that the initial header value is null */
		if ((value = g_mime_entity_get_header (object, "To")) != NULL)
			throw (exception_new ("unexpected initial value"));
		
		header = g_mime_header_list_get_header (headers, "To");
		if ((raw_value = testsuite_header_raw_text (header)) != NULL)
			throw (exception_new ("unexpected initial raw_value"));
		
		/* now try adding an address */
		addr = g_mime_mailbox_address_new ("Tester", "tester@localhost.com", NULL);
		g_mime_internet_address_list_add (list, addr);
		
		if (!(value = g_mime_entity_get_header (object, "To")))
			throw (exception_new ("address list header unexpectedly null after adding recipient"));
		if (strcmp ("Tester <tester@localhost.com>", value) != 0)
			throw (exception_new ("unexpected address list header after adding recipient"));
		header = g_mime_header_list_get_header (headers, "To");
		if (!(raw_value = testsuite_header_raw_text (header)))
			throw (exception_new ("raw_value is null after adding recipient"));
		if (strcmp (" Tester <tester@localhost.com>\n", raw_value) != 0)
			throw (exception_new ("unexpected raw_value after adding recipient: %s", raw_value));
		
		/* now let's try changing the address name to make sure signals properly chain up */
		g_mime_internet_address_set_name (addr, "Eva Lucy-Ann Tester");
		if (!(value = g_mime_entity_get_header (object, "To")))
			throw (exception_new ("address list header unexpectedly null after changing name"));
		if (strcmp ("Eva Lucy-Ann Tester <tester@localhost.com>", value) != 0)
			throw (exception_new ("unexpected address list header after changing name: %s", value));
		header = g_mime_header_list_get_header (headers, "To");
		if (!(raw_value = testsuite_header_raw_text (header)))
			throw (exception_new ("raw_value is null after changing name"));
		if (strcmp (" Eva Lucy-Ann Tester <tester@localhost.com>\n", raw_value) != 0)
			throw (exception_new ("unexpected raw_value after changing name"));
		
		/* now let's try changing the address mailbox... */
		g_mime_mailbox_address_set_addrspec ((GMimeMailboxAddress *) addr, "evalucyann@ximian.com", NULL);
		if (!(value = g_mime_entity_get_header (object, "To")))
			throw (exception_new ("address list header unexpectedly null after changing mailbox"));
		if (strcmp ("Eva Lucy-Ann Tester <evalucyann@ximian.com>", value) != 0)
			throw (exception_new ("unexpected address list header after changing mailbox"));
		header = g_mime_header_list_get_header (headers, "To");
		if (!(raw_value = testsuite_header_raw_text (header)))
			throw (exception_new ("raw_value is null after changing mailbox"));
		if (strcmp (" Eva Lucy-Ann Tester <evalucyann@ximian.com>\n", raw_value) != 0)
			throw (exception_new ("unexpected raw_value after changing mailbox"));
		
		/* now let's try inserting a group address */
		g_object_unref (addr);
		addr = g_mime_group_address_new ("Group");
		g_mime_internet_address_list_insert (list, 0, addr);
		g_object_unref (addr);
		
		if (!(value = g_mime_entity_get_header (object, "To")))
			throw (exception_new ("address list header unexpectedly null after inserting group"));
		if (strcmp ("Group: ;, Eva Lucy-Ann Tester <evalucyann@ximian.com>", value) != 0)
			throw (exception_new ("unexpected address list header after inserting group"));
		header = g_mime_header_list_get_header (headers, "To");
		if (!(raw_value = testsuite_header_raw_text (header)))
			throw (exception_new ("raw_value is null after inserting group"));
		if (strcmp (" Group: ;, Eva Lucy-Ann Tester <evalucyann@ximian.com>\n", raw_value) != 0)
			throw (exception_new ("unexpected raw_value after inserting group"));
		
		/* now let's try removing the original recipient */
		g_mime_internet_address_list_remove_at (list, 1);
		if (!(value = g_mime_entity_get_header (object, "To")))
			throw (exception_new ("address list header unexpectedly null after removing recipient"));
		if (strcmp ("Group: ;", value) != 0)
			throw (exception_new ("unexpected address list header after removing recipient"));
		header = g_mime_header_list_get_header (headers, "To");
		if (!(raw_value = testsuite_header_raw_text (header)))
			throw (exception_new ("raw_value is null after removing recipient"));
		if (strcmp (" Group: ;\n", raw_value) != 0)
			throw (exception_new ("unexpected raw_value after removing recipient"));
		
		/* now let's try adding an address to the group... */
		ia = g_mime_mailbox_address_new ("Tester", "tester@hotmail.com", NULL);
		g_mime_internet_address_list_add (((GMimeGroupAddress *) addr)->members, ia);
		g_object_unref (ia);
		if (!(value = g_mime_entity_get_header (object, "To")))
			throw (exception_new ("address list header unexpectedly null after adding addr to group"));
		if (strcmp ("Group: Tester <tester@hotmail.com>;", value) != 0)
			throw (exception_new ("unexpected address list header after adding addr to group"));
		header = g_mime_header_list_get_header (headers, "To");
		if (!(raw_value = testsuite_header_raw_text (header)))
			throw (exception_new ("raw_value is null after adding addr to group"));
		if (strcmp (" Group: Tester <tester@hotmail.com>;\n", raw_value) != 0)
			throw (exception_new ("unexpected raw_value after adding addr to group"));
		
		/* let's try this in reverse... set the header value and make sure GMimeInternetAddressList gets updated */
		g_mime_entity_set_header (object, "To", "fpons@mandrakesoft.com (=?iso-8859-1?q?Fran=E7ois?= Pons)", NULL);
		if (g_mime_internet_address_list_length (list) != 1)
			throw (exception_new ("unexpected number of addresses in addrlist after setting header value"));
		ia = g_mime_internet_address_list_get_address (list, 0);
		value = g_mime_internet_address_get_name (ia);
		if (strcmp ("Fran\xc3\xa7ois Pons", value) != 0)
			throw (exception_new ("unexpected name after setting header value"));
		value = g_mime_mailbox_address_get_addrspec ((GMimeMailboxAddress *) ia);
		if (strcmp ("fpons@mandrakesoft.com", value) != 0)
			throw (exception_new ("unexpected addr after setting header value"));
		header = g_mime_header_list_get_header (headers, "To");
		if (!(raw_value = testsuite_header_raw_text (header)))
			throw (exception_new ("raw_value is null after setting header value"));
		if (strcmp (" =?iso-8859-1?q?Fran=E7ois?= Pons <fpons@mandrakesoft.com>\n", raw_value) != 0)
			throw (exception_new ("unexpected raw_value after setting header value: %s", raw_value));
		
		testsuite_check_passed ();
	} catch (ex) {
		testsuite_check_failed ("address header not synchronized: %s", ex->message);
	} finally;
	
	g_object_unref (message);
}

static struct {
	const char *name;
	const char *value;
	const char *raw_value;
} headers[] = {
	{ "Received",
	  "by greenbush.bellcore.com (4.1/4.7) id <AA12840> for nsb; Thu, 19 Sep 91 12:41:43 EDT",
	  " by greenbush.bellcore.com (4.1/4.7) id <AA12840> for nsb;\n\tThu, 19 Sep 91 12:41:43 EDT\n" },
	{ "Received",
	  "from thumper.bellcore.com by greenbush.bellcore.com (4.1/4.7) id <AA01648> for nsb; Fri, 29 Nov 91 07:13:33 EST",
	  " from thumper.bellcore.com by greenbush.bellcore.com (4.1/4.7)\n\tid <AA01648> for nsb; Fri, 29 Nov 91 07:13:33 EST\n" },
	{ "To",
	  "abel, bianchi, braun, cameron, carmen, jfp, jxr, kraut, lamb, lowery, lynn, mlittman, nancyg, sau, shoshi, slr, stornett@flash, tkl",
	  " <abel>, <bianchi>, <braun>, <cameron>, <carmen>, <jfp>, <jxr>, <kraut>,\n <lamb>, <lowery>, <lynn>, <mlittman>, <nancyg>, <sau>, <shoshi>, <slr>,\n stornett@flash, <tkl>\n" },
	{ "References",
	  "<3ohapq$h3b@gandalf.rutgers.edu> <3notqh$b52@ns2.ny.ubs.com> <3npoh0$2oo@news.blkbox.com> <3nqp09$r7t@ns2.ny.ubs.com>",
	  " <3ohapq$h3b@gandalf.rutgers.edu> <3notqh$b52@ns2.ny.ubs.com>\n\t<3npoh0$2oo@news.blkbox.com> <3nqp09$r7t@ns2.ny.ubs.com>\n" },
	{ "Message-Id",
	  "<this.is.a.really.really.reeeaalllllllllllllly.looooooooooong.message.id@some.internet.host.com>",
	  " <this.is.a.really.really.reeeaalllllllllllllly.looooooooooong.message.id@some.internet.host.com>\n" },
	{ "Subject",
	  "this is a really, really, reeeeeeaaaaaaalllllllllllllly loooooooooooooonnnnnggggggggggg test subject which should get folded into multiple lines",
	  " this is a really, really, reeeeeeaaaaaaalllllllllllllly\n loooooooooooooonnnnnggggggggggg test subject which should get folded into\n multiple lines\n" },
};

static void
test_header_formatting (void)
{
	const char *raw_value;
	GMimeHeaderList *list;
	GMimeHeader *header;
	guint i;
	
	list = g_mime_header_list_new (g_mime_parser_options_get_default ());
	
	for (i = 0; i < G_N_ELEMENTS (headers); i++) {
		testsuite_check ("header[%u]", i);
		
		try {
			g_mime_header_list_append (list, headers[i].name, headers[i].value, NULL);
			header = g_mime_header_list_get_header_at (list, (int) i);
			raw_value = testsuite_header_raw_text (header);
			
			if (strcmp (headers[i].raw_value, raw_value) != 0)
				throw (exception_new ("raw values do not match: %s", raw_value));
			
			testsuite_check_passed ();
		} catch (ex) {
			testsuite_check_failed ("header[%u] failed: %s", i, ex->message);
		} finally;
	}
	
	g_object_unref (list);
}

static void
test_address_format_case (GMimeFormatOptions *options, const char *value, const char *expected)
{
	GMimeHeaderList *headers = g_mime_header_list_new (NULL);
	GMimeInternetAddressList *addresses = NULL, *parsed = NULL;
	GMimeHeader *header;
	GString *str = g_string_new ("To: ");
	char *full = g_strconcat ("To:", expected, NULL);
	char *wire = NULL, *original = NULL, *roundtrip = NULL, *encoded = NULL, *unfolded = NULL;

	testsuite_check ("address folding[%u,%u]: %s",
			 g_mime_format_options_get_max_line_length (options),
			 g_mime_format_options_get_newline_format (options), value);
	try {
		addresses = g_mime_internet_address_list_parse (NULL, value);
		if (!addresses)
			throw (exception_new ("could not parse address fixture"));

		g_mime_internet_address_list_encode (addresses, options, str);
		if (strcmp (str->str, full) != 0)
			throw (exception_new ("unexpected address folding:\nexpected: %s\nactual: %s", full, str->str));

		g_mime_header_list_append (headers, "To", "", NULL);
		header = g_mime_header_list_get_header_at (headers, 0);
		g_mime_header_set_value (header, options, value, NULL);
		wire = g_mime_header_list_to_string (headers, options);
		if (g_strcmp0 (testsuite_header_raw_text (header), expected) != 0 || strcmp (wire, full) != 0)
			throw (exception_new ("address header formatter ignored the formatting options"));

		encoded = g_mime_internet_address_list_to_string (addresses, options, TRUE);
		if (!encoded || strchr (encoded, '\r') || strchr (encoded, '\n'))
			throw (exception_new ("nonfolding address serialization introduced a line break"));
		g_clear_pointer (&encoded, g_free);
		encoded = g_mime_internet_address_to_string (
			g_mime_internet_address_list_get_address (addresses, 0), options, TRUE);
		if (!encoded || strchr (encoded, '\r') || strchr (encoded, '\n'))
			throw (exception_new ("nonfolding individual address serialization introduced a line break"));

		unfolded = g_mime_utils_header_unfold (expected);
		parsed = g_mime_internet_address_list_parse (NULL, unfolded);
		if (!parsed)
			throw (exception_new ("could not parse folded addresses"));
		original = g_mime_internet_address_list_to_string (addresses, NULL, FALSE);
		roundtrip = g_mime_internet_address_list_to_string (parsed, NULL, FALSE);
		if (g_strcmp0 (original, roundtrip) != 0)
			throw (exception_new ("folded addresses did not round-trip"));

		testsuite_check_passed ();
	} catch (ex) {
		testsuite_check_failed ("%s", ex->message);
	} finally;

	g_clear_object (&addresses);
	g_clear_object (&parsed);
	g_object_unref (headers);
	g_string_free (str, TRUE);
	g_free (full);
	g_free (wire);
	g_free (original);
	g_free (roundtrip);
	g_free (encoded);
	g_free (unfolded);
}

static char *
address_test_name (guint length, char c)
{
	char *name = g_strnfill (length, c);
	guint i;

	for (i = 12; i + 1 < length; i += 13)
		name[i] = ' ';

	return name;
}

static void
test_address_name_case (const char *name, const char *expected)
{
	GMimeInternetAddress *address;
	const char *stored;
	guint group;

	for (group = 0; group < 2; group++) {
		address = group ? g_mime_group_address_new (name) :
			g_mime_mailbox_address_new (name, "user@example.com", NULL);
		testsuite_check ("address name normalization[%u]: %s", group, name ? name : "(null)");
		try {
			stored = g_mime_internet_address_get_name (address);
			if (g_strcmp0 (stored, expected))
				throw (exception_new ("constructor did not normalize the name"));
			g_mime_internet_address_set_name (address, name);
			if (g_mime_internet_address_get_name (address) != stored)
				throw (exception_new ("equivalent name replaced the stored string"));
			g_mime_internet_address_set_name (address, "Replacement");
			g_mime_internet_address_set_name (address, name);
			stored = g_mime_internet_address_get_name (address);
			if (g_strcmp0 (stored, expected))
				throw (exception_new ("setter did not normalize the name"));
			g_mime_internet_address_set_name (address, stored);
			if (g_mime_internet_address_get_name (address) != stored)
				throw (exception_new ("self-assignment replaced the stored string"));
			testsuite_check_passed ();
		} catch (ex) {
			testsuite_check_failed ("%s", ex->message);
		} finally;
		g_object_unref (address);
	}
}

static void
test_address_name_normalization (void)
{
	static const struct {
		const char *name, *expected;
	} cases[] = {
		{ NULL, NULL },
		{ "", "" },
		{ " \tJohn Smith\302\240 ", "John Smith\302\240" },
		{ "  Jane Smith  ", "Jane Smith" },
		{ " \t\v\f\r\n\302\205 ", "\v\f\r\n\302\205" },
		{ " \t\r\n ", "" },
		{ " \tFirst\t  Last \t", "First\t  Last" },
		{ " \303\251mile \360\237\230\200 ", "\303\251mile \360\237\230\200" },
		{ " \342\200\213 Name \342\200\213 ", "\342\200\213 Name \342\200\213" }
	};
	static const gunichar whitespace[] = {
		0x0009, 0x000a, 0x000b, 0x000c, 0x000d, 0x0020, 0x0085, 0x00a0,
		0x1680, 0x2000, 0x2001, 0x2002, 0x2003, 0x2004, 0x2005, 0x2006,
		0x2007, 0x2008, 0x2009, 0x200a, 0x2028, 0x2029, 0x202f, 0x205f, 0x3000
	};
	static const gunichar non_whitespace[] = {
		0x0001, 0x0008, 0x000e, 0x001c, 0x001d, 0x001e, 0x001f,
		0x180e, 0x200b, 0x2060, 0xfeff, 0x1f600
	};
	GMimeInternetAddress *address;
	const char *stored;
	char utf8[7], *name;
	guint i, space;

	for (i = 0; i < G_N_ELEMENTS (cases); i++)
		test_address_name_case (cases[i].name, cases[i].expected);
	for (space = 0; space < 2; space++) {
		for (i = 0; i < (space ? G_N_ELEMENTS (whitespace) : G_N_ELEMENTS (non_whitespace)); i++) {
			gunichar c = space ? whitespace[i] : non_whitespace[i];
			gboolean trim = c == ' ' || c == '\t' || c == '\r' || c == '\n';
			char *padded;

			utf8[g_unichar_to_utf8 (c, utf8)] = '\0';
			test_address_name_case (utf8, trim ? "" : utf8);
			name = g_strconcat (utf8, "First\t Last", utf8, NULL);
			test_address_name_case (name, trim ? "First\t Last" : name);
			padded = g_strconcat (" \t", name, "\r\n", NULL);
			test_address_name_case (padded, trim ? "First\t Last" : name);
			g_free (padded);
			g_free (name);
		}
	}

	testsuite_check ("address name assignment from its own substring");
	address = g_mime_mailbox_address_new ("Prefix Name", "user@example.com", NULL);
	stored = g_mime_internet_address_get_name (address);
	g_mime_internet_address_set_name (address, stored + 7);
	if (g_strcmp0 (g_mime_internet_address_get_name (address), "Name"))
		testsuite_check_failed ("aliased name assignment was not preserved");
	else
		testsuite_check_passed ();
	g_object_unref (address);
}

static void
test_address_name_sync (void)
{
	static const char *raw[] = {
		" \tOld\r\n\t<user@example.com>\r\n",
		" \tTeam:\r\n\tOld <user@example.com>;\r\n"
	};
	GMimeInternetAddressList *list;
	GMimeInternetAddress *address, *group;
	GMimeMessage *message;
	GMimeHeader *header;
	guint nested;

	for (nested = 0; nested < 2; nested++) {
		char *snapshot = NULL;

		message = g_mime_message_new (FALSE);
		g_mime_entity_set_header (GMIME_ENTITY (message), "To", "", NULL);
		header = g_mime_header_list_get_header (g_mime_entity_get_header_list (GMIME_ENTITY (message)), "To");
		testsuite_header_set_raw_text (header, raw[nested]);
		list = g_mime_message_get_addresses (message, GMIME_ADDRESS_TYPE_TO);
		address = g_mime_internet_address_list_get_address (list, 0);
		group = nested ? address : NULL;
		if (nested)
			address = g_mime_internet_address_list_get_address (
				g_mime_group_address_get_members (GMIME_GROUP_ADDRESS (group)), 0);

		testsuite_check ("normalized address name header synchronization[%u]", nested);
		try {
			g_mime_internet_address_set_name (address, " \tOld\r\n ");
			if (group)
				g_mime_internet_address_set_name (group, " \tTeam\r\n");
			if (g_strcmp0 (testsuite_header_raw_text (header), raw[nested]))
				throw (exception_new ("equivalent name regenerated the raw header"));
			g_mime_internet_address_set_name (address, g_mime_internet_address_get_name (address));
			if (g_strcmp0 (testsuite_header_raw_text (header), raw[nested]))
				throw (exception_new ("self-assignment regenerated the raw header"));
			g_mime_internet_address_set_name (address, " \tNew\r\n ");
			if (g_strcmp0 (testsuite_header_raw_text (header), nested ?
				       " Team: New <user@example.com>;\n" : " New <user@example.com>\n"))
				throw (exception_new ("normalized name change did not update the raw header"));
			if (group) {
				g_mime_internet_address_set_name (group, " \t\r\n ");
				if (g_strcmp0 (testsuite_header_raw_text (header), " \"\": New <user@example.com>;\n"))
					throw (exception_new ("whitespace-only group name did not update the raw header"));
			}
			snapshot = g_strdup (testsuite_header_raw_text (header));
			g_mime_internet_address_set_name (address, " \tNew\302\240 ");
			if (g_strcmp0 (g_mime_internet_address_get_name (address), "New\302\240") ||
			    !strcmp (testsuite_header_raw_text (header), snapshot))
				throw (exception_new ("Unicode name padding did not update the name and raw header"));
			g_free (snapshot);
			snapshot = g_strdup (testsuite_header_raw_text (header));
			g_mime_internet_address_set_name (address, " \tNew\302\240\r\n");
			if (g_strcmp0 (testsuite_header_raw_text (header), snapshot))
				throw (exception_new ("equivalent Unicode name regenerated the raw header"));
			testsuite_check_passed ();
		} catch (ex) {
			testsuite_check_failed ("%s", ex->message);
		} finally;
		g_free (snapshot);
		g_object_unref (message);
	}
}

static void
test_domainless_mailbox_formatting (void)
{
	static const struct {
		const char *name;
		const char *addr;
		const char *display;
		const char *encoded;
	} cases[] = {
		{ NULL, "local", "local", "<local>" },
		{ "", "local", "local", "<local>" },
		{ NULL, "local.part", "local.part", "<local.part>" },
		{ NULL, "\"local name\"", "\"local name\"", "<\"local name\">" },
		{ NULL, "\"local@name\"", "\"local@name\"", "<\"local@name\">" },
		{ NULL, "\"\"", "\"\"", "<\"\">" },
		{ NULL, "", "", "" },
		{ "Name", "local", "Name <local>", "Name <local>" },
		{ "Name", "", "Name <>", "Name <>" },
		{ NULL, "local@example.com", "local@example.com", "local@example.com" }
	};
	GMimeParserOptions *options = g_mime_parser_options_new ();
	GMimeInternetAddressList *list, *parsed;
	GMimeInternetAddress *mailbox;
	char *encoded, *display;
	guint i, strict, commas;

	g_mime_parser_options_set_allow_addresses_without_domain (options, FALSE);
	for (strict = 0; strict < 2; strict++) {
		g_mime_parser_options_set_address_compliance_mode (options, strict ?
			GMIME_RFC_COMPLIANCE_STRICT : GMIME_RFC_COMPLIANCE_LOOSE);
		for (commas = 0; commas < 2; commas++) {
			g_mime_parser_options_set_allow_unquoted_commas_in_addresses (options, commas);
			for (i = 0; i < G_N_ELEMENTS (cases); i++) {
				list = g_mime_internet_address_list_new ();
				mailbox = g_mime_mailbox_address_new (cases[i].name, cases[i].addr, NULL);
				g_mime_internet_address_list_add (list, mailbox);
				parsed = NULL;
				display = NULL;
				encoded = NULL;

				testsuite_check ("domainless mailbox formatting[%u,%u,%u]", strict, commas, i);
				try {
					display = g_mime_internet_address_to_string (mailbox, NULL, FALSE);
					encoded = g_mime_internet_address_to_string (mailbox, NULL, TRUE);
					if (g_strcmp0 (display, cases[i].display) != 0 ||
					    g_strcmp0 (encoded, cases[i].encoded) != 0)
						throw (exception_new ("unexpected individual mailbox serialization"));
					g_clear_pointer (&display, g_free);
					g_clear_pointer (&encoded, g_free);
					display = g_mime_internet_address_list_to_string (list, NULL, FALSE);
					encoded = g_mime_internet_address_list_to_string (list, NULL, TRUE);
					if (g_strcmp0 (display, cases[i].display) != 0 ||
					    g_strcmp0 (encoded, cases[i].encoded) != 0)
						throw (exception_new ("unexpected mailbox list serialization"));
					g_clear_pointer (&display, g_free);

					if (*cases[i].addr) {
						parsed = g_mime_internet_address_list_parse (options, encoded);
						if (!parsed || g_mime_internet_address_list_length (parsed) != 1)
							throw (exception_new ("encoded mailbox requires domainless parsing"));
						display = g_mime_internet_address_list_to_string (parsed, NULL, FALSE);
						if (g_strcmp0 (display, cases[i].display) != 0)
							throw (exception_new ("encoded mailbox did not round-trip"));
					}

					testsuite_check_passed ();
				} catch (ex) {
					testsuite_check_failed ("%s", ex->message);
				} finally;

				g_free (display);
				g_free (encoded);
				g_clear_object (&parsed);
				g_object_unref (mailbox);
				g_object_unref (list);
			}
		}
	}
	g_mime_parser_options_free (options);
}

static void
test_unnamed_group_formatting (void)
{
	static const char *names[] = { NULL, "", "  ", "\t\r\n " };
	static const char *addresses[] = { NULL, "a@example.com", "local" };
	GMimeParserOptions *options = g_mime_parser_options_new ();
	GMimeInternetAddressList *list, *parsed, *members;
	GMimeInternetAddress *address;
	GMimeGroupAddress *group;
	GString *expected, *display;
	char *encoded, *actual;
	guint i, depth, populated, strict, j;

	g_mime_parser_options_set_allow_addresses_without_domain (options, FALSE);
	for (i = 0; i < G_N_ELEMENTS (names); i++) {
		for (depth = 1; depth <= 3; depth++) {
			for (populated = 0; populated < G_N_ELEMENTS (addresses); populated++) {
				list = g_mime_internet_address_list_new ();
				members = list;
				expected = g_string_new ("");
				display = g_string_new ("");
				for (j = 0; j < depth; j++) {
					address = g_mime_group_address_new (names[i]);
					g_mime_internet_address_list_add (members, address);
					group = GMIME_GROUP_ADDRESS (address);
					members = g_mime_group_address_get_members (group);
					g_object_unref (address);
					g_string_append (expected, "\"\": ");
					g_string_append (display, ": ");
				}
				if (populated) {
					address = g_mime_mailbox_address_new (NULL, addresses[populated], NULL);
					g_mime_internet_address_list_add (members, address);
					g_object_unref (address);
					g_string_append (expected, populated == 2 ? "<local>" : addresses[populated]);
					g_string_append (display, addresses[populated]);
				}
				for (j = 0; j < depth; j++) {
					g_string_append_c (expected, ';');
					g_string_append_c (display, ';');
				}

				for (strict = 0; strict < 2; strict++) {
					g_mime_parser_options_set_address_compliance_mode (options, strict ?
						GMIME_RFC_COMPLIANCE_STRICT : GMIME_RFC_COMPLIANCE_LOOSE);
					encoded = NULL;
					actual = NULL;
					parsed = NULL;

					testsuite_check ("unnamed group formatting[%u,%u,%u,%u]", i, depth, populated, strict);
					try {
						address = g_mime_internet_address_list_get_address (list, 0);
						encoded = g_mime_internet_address_to_string (address, NULL, TRUE);
						actual = g_mime_internet_address_to_string (address, NULL, FALSE);
						if (g_strcmp0 (encoded, expected->str) || g_strcmp0 (actual, display->str))
							throw (exception_new ("unexpected individual group serialization"));
						g_clear_pointer (&encoded, g_free);
						g_clear_pointer (&actual, g_free);
						encoded = g_mime_internet_address_list_to_string (list, NULL, TRUE);
						actual = g_mime_internet_address_list_to_string (list, NULL, FALSE);
						if (g_strcmp0 (encoded, expected->str) || g_strcmp0 (actual, display->str))
							throw (exception_new ("unexpected group list serialization"));
						g_clear_pointer (&actual, g_free);

						parsed = g_mime_internet_address_list_parse (options, encoded);
						members = parsed;
						for (j = 0; j < depth; j++) {
							if (!members || g_mime_internet_address_list_length (members) != 1)
								throw (exception_new ("nested group was lost during parsing"));
							address = g_mime_internet_address_list_get_address (members, 0);
							if (!GMIME_IS_GROUP_ADDRESS (address) ||
							    g_strcmp0 (g_mime_internet_address_get_name (address), ""))
								throw (exception_new ("parsed group has an unexpected type or name"));
							members = g_mime_group_address_get_members (GMIME_GROUP_ADDRESS (address));
						}
						if (g_mime_internet_address_list_length (members) != (populated != 0))
							throw (exception_new ("parsed group has an unexpected member count"));
						if (populated) {
							address = g_mime_internet_address_list_get_address (members, 0);
							if (!GMIME_IS_MAILBOX_ADDRESS (address) ||
							    g_strcmp0 (g_mime_mailbox_address_get_addrspec (GMIME_MAILBOX_ADDRESS (address)),
								       addresses[populated]))
								throw (exception_new ("nested mailbox was not preserved"));
						}
						actual = g_mime_internet_address_list_to_string (parsed, NULL, TRUE);
						if (g_strcmp0 (actual, expected->str))
							throw (exception_new ("encoded groups did not round-trip"));
						testsuite_check_passed ();
					} catch (ex) {
						testsuite_check_failed ("%s", ex->message);
					} finally;

					g_free (encoded);
					g_free (actual);
					g_clear_object (&parsed);
				}
				g_string_free (expected, TRUE);
				g_string_free (display, TRUE);
				g_object_unref (list);
			}
		}
	}
	g_mime_parser_options_free (options);
}

static void
test_address_line_lengths (void)
{
	static const guint widths[] = { 60, 61, 78, 120, 998 };
	GMimeFormatOptions *options = g_mime_format_options_new (), *clone;
	GMimeHeaderList *headers = g_mime_header_list_new (NULL);
	GMimeHeader *header;
	const char *newline;
	char *name, *addr, *value, *expected, *quoted, *raw, *full, *wire;
	guint i, dos;
	int offset;

	for (i = 0; i < G_N_ELEMENTS (widths); i++) {
		g_mime_format_options_set_max_line_length (options, widths[i]);
		for (dos = 0; dos < 2; dos++) {
			g_mime_format_options_set_newline_format (options, dos ?
				GMIME_NEWLINE_FORMAT_DOS : GMIME_NEWLINE_FORMAT_UNIX);
			clone = g_mime_format_options_clone (options);
			g_mime_format_options_set_max_line_length (options, widths[i] == 60 ? 998 : 60);
			g_mime_format_options_set_newline_format (options, dos ?
				GMIME_NEWLINE_FORMAT_UNIX : GMIME_NEWLINE_FORMAT_DOS);
			newline = g_mime_format_options_get_newline (clone);

			for (offset = -1; offset <= 1; offset++) {
				name = address_test_name (widths[i] - 20 + offset, 'n');
				value = g_strdup_printf ("%s <a@example.com>", name);
				expected = offset > 0 ?
					g_strdup_printf (" %s%s\t<a@example.com>%s", name, newline, newline) :
					g_strdup_printf (" %s <a@example.com>%s", name, newline);
				test_address_format_case (clone, value, expected);
				g_free (name);
				g_free (value);
				g_free (expected);

				name = g_strnfill (widths[i] - 31 + offset, 'b');
				addr = g_strconcat (name, "@example.com", NULL);
				value = g_strdup_printf ("a@example.com, %s", addr);
				expected = offset > 0 ?
					g_strdup_printf (" a@example.com,%s %s%s", newline, addr, newline) :
					g_strdup_printf (" a@example.com, %s%s", addr, newline);
				test_address_format_case (clone, value, expected);
				g_free (name);
				g_free (addr);
				g_free (value);
				g_free (expected);

				addr = g_strnfill (widths[i] - 21 + offset, 'l');
				value = g_strdup_printf ("a@example.com, %s", addr);
				expected = offset > 0 ?
					g_strdup_printf (" a@example.com,%s <%s>%s", newline, addr, newline) :
					g_strdup_printf (" a@example.com, <%s>%s", addr, newline);
				test_address_format_case (clone, value, expected);
				g_free (addr);
				g_free (value);
				g_free (expected);

				name = g_strnfill (widths[i] - 25 + offset, 'b');
				addr = g_strconcat (name, "@example.com", NULL);
				value = g_strdup_printf ("local, %s", addr);
				expected = offset > 0 ?
					g_strdup_printf (" <local>,%s %s%s", newline, addr, newline) :
					g_strdup_printf (" <local>, %s%s", addr, newline);
				test_address_format_case (clone, value, expected);
				g_free (name);
				g_free (addr);
				g_free (value);
				g_free (expected);

				addr = g_strnfill (widths[i] - 13 + offset, 'l');
				value = g_strdup_printf ("Group: %s;", addr);
				expected = offset > 0 ?
					g_strdup_printf (" Group:%s <%s>;%s", newline, addr, newline) :
					g_strdup_printf (" Group: <%s>;%s", addr, newline);
				test_address_format_case (clone, value, expected);
				g_free (addr);
				g_free (value);
				g_free (expected);

				name = g_strnfill (widths[i] - 20 + offset, 'g');
				addr = g_strconcat (name, "@example.com", NULL);
				value = g_strdup_printf ("\"\": %s;", addr);
				expected = offset > 0 ?
					g_strdup_printf (" \"\":%s %s;%s", newline, addr, newline) :
					g_strdup_printf (" \"\": %s;%s", addr, newline);
				test_address_format_case (clone, value, expected);
				g_free (name);
				g_free (addr);
				g_free (value);
				g_free (expected);

				name = g_strnfill (widths[i] - 23 + offset, 'g');
				addr = g_strconcat (name, "@example.com", NULL);
				value = g_strdup_printf ("\"\": ;, %s", addr);
				expected = offset > 0 ?
					g_strdup_printf (" \"\": ;,%s %s%s", newline, addr, newline) :
					g_strdup_printf (" \"\": ;, %s%s", addr, newline);
				test_address_format_case (clone, value, expected);
				g_free (name);
				g_free (addr);
				g_free (value);
				g_free (expected);

				name = address_test_name (widths[i] - 19 + offset, 'g');
				value = g_strdup_printf ("a@example.com, %s: ;", name);
				expected = offset > 0 ?
					g_strdup_printf (" a@example.com,%s %s: ;%s", newline, name, newline) :
					g_strdup_printf (" a@example.com, %s: ;%s", name, newline);
				test_address_format_case (clone, value, expected);
				g_free (name);
				g_free (value);
				g_free (expected);

				name = g_strnfill (widths[i] - 38 + offset, 'e');
				addr = g_strconcat (name, "@example.com", NULL);
				value = g_strdup_printf ("%s, =?utf-8?q?Jos=C3=A9?= <b@example.com>", addr);
				expected = offset > 0 ?
					g_strdup_printf (" %s,%s =?utf-8?b?Sm9zw6k=?= <b@example.com>%s", addr, newline, newline) :
					g_strdup_printf (" %s, =?utf-8?b?Sm9zw6k=?=%s\t<b@example.com>%s", addr, newline, newline);
				test_address_format_case (clone, value, expected);
				g_free (name);
				g_free (addr);
				g_free (value);
				g_free (expected);
			}

			name = address_test_name (widths[i] - 3, 'n');
			value = g_strdup_printf ("%s <a@example.com>", name);
			expected = g_strdup_printf (" %s%s\t<a@example.com>%s", name, newline, newline);
			test_address_format_case (clone, value, expected);
			g_free (value);
			g_free (expected);
			value = g_strdup_printf ("%s: ;", name);
			expected = g_strdup_printf (" %s: ;%s", name, newline);
			test_address_format_case (clone, value, expected);
			g_free (name);
			g_free (value);
			g_free (expected);

			name = address_test_name (widths[i] - 10, 'g');
			value = g_strdup_printf ("%s %s: a@example.com;", name, name);
			expected = g_strdup_printf (" %s%s %s:%s a@example.com;%s",
						    name, newline, name, newline, newline);
			test_address_format_case (clone, value, expected);
			g_free (value);
			g_free (expected);
			value = g_strdup_printf ("a@example.com, %s <b@example.com>", name);
			expected = g_strdup_printf (" a@example.com,%s %s%s\t<b@example.com>%s",
						    newline, name, newline, newline);
			test_address_format_case (clone, value, expected);
			g_free (name);
			g_free (value);
			g_free (expected);

			name = address_test_name (widths[i] + 20, 'n');
			quoted = g_strdup_printf ("\"Name, %s\"", name);
			value = g_strdup_printf ("%s <b@example.com>", quoted);
			expected = g_strdup_printf (" %s%s\t<b@example.com>%s", quoted, newline, newline);
			test_address_format_case (clone, value, expected);
			g_free (value);
			g_free (expected);
			value = g_strdup_printf ("a@example.com, %s <b@example.com>", quoted);
			expected = g_strdup_printf (" a@example.com,%s %s%s\t<b@example.com>%s",
						    newline, quoted, newline, newline);
			test_address_format_case (clone, value, expected);
			g_free (value);
			g_free (expected);
			value = g_strdup_printf ("%s: b@example.com;", quoted);
			expected = g_strdup_printf (" %s:%s b@example.com;%s", quoted, newline, newline);
			test_address_format_case (clone, value, expected);
			g_free (value);
			g_free (expected);
			g_free (quoted);
			g_free (name);

			if (widths[i] <= 61) {
				name = g_strnfill (widths[i] + 10, 'q');
				quoted = g_strdup_printf ("\"%s,\\\"Q\"", name);
				raw = g_strdup_printf ("=?utf-8?q?%.*s?=%s =?utf-8?q?%s=2C=22Q?=",
						       (int) widths[i] - 17, name, newline, name + widths[i] - 17);
				value = g_strdup_printf ("a@example.com, %s <b@example.com>", quoted);
				expected = g_strdup_printf (" a@example.com,%s %s%s\t<b@example.com>%s",
							    newline, raw, newline, newline);
				test_address_format_case (clone, value, expected);
				g_free (name);
				g_free (value);
				g_free (expected);
				value = g_strdup_printf ("%s <b@example.com>", quoted);
				expected = g_strdup_printf (" %s%s\t<b@example.com>%s", raw, newline, newline);
				test_address_format_case (clone, value, expected);
				g_free (quoted);
				g_free (value);
				g_free (expected);
				g_free (raw);
			}

			name = g_strnfill (widths[i] + 10, 'q');
			addr = g_strconcat (name, "@example.com", NULL);
			value = g_strdup_printf ("%s, b@example.com", addr);
			expected = g_strdup_printf (" %s,%s b@example.com%s", addr, newline, newline);
			test_address_format_case (clone, value, expected);
			g_free (value);
			g_free (expected);
			value = g_strdup_printf ("Outer: Inner: %s;;", addr);
			expected = g_strdup_printf (" Outer: Inner:%s %s;;%s", newline, addr, newline);
			test_address_format_case (clone, value, expected);
			g_free (name);
			g_free (addr);
			g_free (value);
			g_free (expected);

			addr = g_strnfill (widths[i] + 10, 'l');
			value = g_strdup_printf ("%s, b@example.com", addr);
			expected = g_strdup_printf (" <%s>,%s b@example.com%s", addr, newline, newline);
			test_address_format_case (clone, value, expected);
			g_free (addr);
			g_free (value);
			g_free (expected);

			name = g_strnfill (widths[i] - 16, 'g');
			addr = g_strconcat (name, "@example.com", NULL);
			value = g_strdup_printf ("%s, \"\": ;", addr);
			expected = g_strdup_printf (" %s, \"\": ;%s", addr, newline);
			test_address_format_case (clone, value, expected);
			g_free (name);
			g_free (addr);
			g_free (value);
			g_free (expected);

			testsuite_check ("untouched raw address headers[%u,%u]", widths[i], dos);
			g_mime_header_list_clear (headers);
			g_mime_header_list_append (headers, "To", "", NULL);
			header = g_mime_header_list_get_header_at (headers, 0);
			raw = g_strdup_printf (" First <a@example.com>,%s\tSecond <b@example.com>%s", newline, newline);
			full = g_strconcat ("To:", raw, NULL);
			testsuite_header_set_raw_text (header, raw);
			wire = g_mime_header_list_to_string (headers, clone);
			try {
				if (g_strcmp0 (testsuite_header_raw_text (header), raw) != 0 || strcmp (wire, full) != 0)
					throw (exception_new ("untouched raw address folding changed"));
				testsuite_check_passed ();
			} catch (ex) {
				testsuite_check_failed ("%s", ex->message);
			} finally;
			g_free (raw);
			g_free (full);
			g_free (wire);
			g_mime_format_options_free (clone);
			g_mime_format_options_set_max_line_length (options, widths[i]);
		}
	}

	test_address_format_case (NULL, "First <a@example.com>, Second <b@example.com>",
				 " First <a@example.com>, Second <b@example.com>\n");
	g_object_unref (headers);
	g_mime_format_options_free (options);
}

static void
test_text_fold_case (GMimeFormatOptions *options, const char *value, const char *expected, gboolean structured)
{
	char *input = g_strconcat ("Subject: ", value, NULL);
	char *folded, *formatted = NULL;

	testsuite_check ("text folding[%u,%u,%u]: %s",
			 g_mime_format_options_get_max_line_length (options),
			 g_mime_format_options_get_newline_format (options), structured, value);
	folded = structured ? g_mime_utils_structured_header_fold (NULL, options, input) :
		g_mime_utils_unstructured_header_fold (NULL, options, input);
	try {
		if (g_strcmp0 (folded, expected) != 0)
			throw (exception_new ("unexpected text folding:\nexpected: %s\nactual: %s", expected, folded));
		if (!structured) {
			formatted = g_mime_utils_header_printf (NULL, options, "Subject: %s", value);
			if (g_strcmp0 (formatted, expected) != 0)
				throw (exception_new ("header_printf ignored the formatting options"));
		}
		testsuite_check_passed ();
	} catch (ex) {
		testsuite_check_failed ("%s", ex->message);
	} finally;

	g_free (input);
	g_free (folded);
	g_free (formatted);
}

static void
test_text_header_line_lengths (void)
{
	static const guint widths[] = { 60, 61, 78, 120, 998 };
	GMimeFormatOptions *options = g_mime_format_options_new (), *clone;
	GMimeHeaderList *headers = g_mime_header_list_new (NULL);
	GMimeHeader *header;
	const char *newline;
	char *word, *value, *expected, *wire, *raw, *full;
	guint i, dos, structured, n;
	int offset;

	for (i = 0; i < G_N_ELEMENTS (widths); i++) {
		g_mime_format_options_set_max_line_length (options, widths[i]);
		for (dos = 0; dos < 2; dos++) {
			g_mime_format_options_set_newline_format (options, dos ?
				GMIME_NEWLINE_FORMAT_DOS : GMIME_NEWLINE_FORMAT_UNIX);
			clone = g_mime_format_options_clone (options);
			g_mime_format_options_set_max_line_length (options, widths[i] == 60 ? 998 : 60);
			g_mime_format_options_set_newline_format (options, dos ?
				GMIME_NEWLINE_FORMAT_UNIX : GMIME_NEWLINE_FORMAT_DOS);
			newline = g_mime_format_options_get_newline (clone);

			for (structured = 0; structured < 2; structured++) {
				for (offset = -1; offset <= 1; offset++) {
					word = g_strnfill (widths[i] - 9 + offset, 'a');
					value = g_strconcat (word, " end", NULL);
					expected = g_strdup_printf ("Subject: %s%s end%s", word, newline, newline);
					test_text_fold_case (clone, value, expected, structured);
					g_free (value);
					g_free (expected);
					expected = g_strdup_printf ("Subject: %s%s", word, newline);
					test_text_fold_case (clone, word, expected, structured);
					g_free (word);
					g_free (expected);
				}

				word = g_strnfill (widths[i] - 2, 'a');
				expected = g_strdup_printf ("Subject: %s%s", word, newline);
				test_text_fold_case (clone, word, expected, structured);
				g_free (word);
				g_free (expected);

				for (offset = -1; offset <= 1; offset++) {
					word = g_strnfill (widths[i] + offset, 'a');
					value = g_strconcat ("first ", word, NULL);
					expected = offset < 0 ?
						g_strdup_printf ("Subject: first%s %s%s", newline, word, newline) :
						g_strdup_printf ("Subject: first%s %.*s%s%c%.*s%s",
								    newline, (int) widths[i] - 1, word, newline,
								    structured ? '\t' : ' ', offset + 1, word, newline);
					test_text_fold_case (clone, value, expected, structured);
					g_free (word);
					g_free (value);
					g_free (expected);
				}

				for (n = 0; n < 2; n++) {
					word = g_strnfill (widths[i] - 10, 'a');
					value = g_strdup_printf ("%s%s end", word, n ? " \t" : " ");
					expected = g_strdup_printf ("Subject: %s %s%send%s",
								    word, newline, n ? "\t " : " ", newline);
					test_text_fold_case (clone, value, expected, structured);
					g_free (word);
					g_free (value);
					g_free (expected);
				}

				word = g_strnfill (widths[i] * 3, 'a');
				value = g_strconcat (word, " end", NULL);
				expected = g_strdup_printf ("Subject: %.*s%s%c%.*s%s%c%.*s%s%c%.*s end%s",
							    (int) widths[i] - 9, word, newline, structured ? '\t' : ' ',
							    (int) widths[i] - 1, word, newline, structured ? '\t' : ' ',
							    (int) widths[i] - 1, word, newline, structured ? '\t' : ' ',
							    11, word, newline);
				test_text_fold_case (clone, value, expected, structured);
				g_free (word);
				g_free (value);
				g_free (expected);

				expected = g_strdup_printf ("Subject: first%s second%s%cthird%s",
							    newline, newline, structured ? '\t' : ' ', newline);
				test_text_fold_case (clone, "first\r\n second\nthird", expected, structured);
				g_free (expected);

				expected = g_strconcat ("Subject: =?UtF-8*en?q?hello?=\t =?utf-8?b?d29ybGQ=?=", newline, NULL);
				test_text_fold_case (clone, "=?UtF-8*en?q?hello?=\t =?utf-8?b?d29ybGQ=?=", expected, structured);
				g_free (expected);
				expected = g_strconcat ("Subject: =?utf-8?q?one?==?utf-8?q?two?=", newline, NULL);
				test_text_fold_case (clone, "=?utf-8?q?one?==?utf-8?q?two?=", expected, structured);
				g_free (expected);

				word = g_strnfill (widths[i] - 25, 'a');
				value = g_strdup_printf ("=?us-ascii?q?%s?=\t =?utf-8?b?d29ybGQ=?=", word);
				expected = structured ?
					g_strdup_printf ("Subject: =?us-ascii?q?%s?=%s\t =?utf-8?b?d29ybGQ=?=%s",
							 word, newline, newline) :
					g_strdup_printf ("Subject: =?us-ascii?q?%s?=\t%s =?utf-8?b?d29ybGQ=?=%s",
							 word, newline, newline);
				test_text_fold_case (clone, value, expected, structured);
				g_free (word);
				g_free (value);
				g_free (expected);

				word = g_strnfill (widths[i] + 10, 'a');
				value = g_strdup_printf ("=?us-ascii?Q?%s?=", word);
				expected = g_strdup_printf ("Subject: %s%s", value, newline);
				test_text_fold_case (clone, value, expected, structured);
				g_free (word);
				g_free (value);
				g_free (expected);

				for (offset = -1; offset <= 1; offset++) {
					word = g_strnfill (widths[i] - 30 + offset, 'a');
					value = g_strdup_printf ("%s =?us-ascii?Q?hello?=", word);
					expected = offset > 0 ?
						g_strdup_printf ("Subject: %s%s =?us-ascii?Q?hello?=%s", word, newline, newline) :
						g_strdup_printf ("Subject: %s =?us-ascii?Q?hello?=%s", word, newline);
					test_text_fold_case (clone, value, expected, structured);
					g_free (word);
					g_free (value);
					g_free (expected);
				}
			}

			word = address_test_name (widths[i] - 9, 'a');
			value = g_strconcat (word, " end", NULL);
			raw = g_strdup_printf (" %s%s end%s", word, newline, newline);
			full = g_strconcat ("Subject:", raw, NULL);
			g_mime_header_list_clear (headers);
			g_mime_header_list_append (headers, "Subject", "", NULL);
			header = g_mime_header_list_get_header_at (headers, 0);
			g_mime_header_set_value (header, clone, value, NULL);
			wire = g_mime_header_list_to_string (headers, clone);
			testsuite_check ("text header formatter[%u,%u]", widths[i], dos);
			try {
				if (g_strcmp0 (testsuite_header_raw_text (header), raw) != 0 || strcmp (wire, full) != 0)
					throw (exception_new ("text header formatter ignored the formatting options: %s", wire));
				testsuite_check_passed ();
			} catch (ex) {
				testsuite_check_failed ("%s", ex->message);
			} finally;
			g_free (word);
			g_free (value);
			g_free (raw);
			g_free (full);
			g_free (wire);

			raw = g_strdup_printf (" first%s\tsecond%s", newline, newline);
			full = g_strconcat ("Subject:", raw, NULL);
			testsuite_header_set_raw_text (header, raw);
			wire = g_mime_header_list_to_string (headers, clone);
			testsuite_check ("untouched raw text headers[%u,%u]", widths[i], dos);
			try {
				if (g_strcmp0 (testsuite_header_raw_text (header), raw) != 0 || strcmp (wire, full) != 0)
					throw (exception_new ("untouched raw text folding changed"));
				testsuite_check_passed ();
			} catch (ex) {
				testsuite_check_failed ("%s", ex->message);
			} finally;
			g_free (raw);
			g_free (full);
			g_free (wire);
			g_mime_format_options_free (clone);
			g_mime_format_options_set_max_line_length (options, widths[i]);
		}
	}

	test_text_fold_case (NULL, "text", "Subject: text\n", FALSE);
	g_object_unref (headers);
	g_mime_format_options_free (options);
}

static void
test_header_field_lengths (void)
{
	static const char *fields[] = { "", "X-Text \t", "List-Post \t", "tO \t" };
	static const guint widths[] = { 60, 78, 120, 998 };
	GMimeFormatOptions *options = g_mime_format_options_new ();
	GMimeHeader *header;
	GMimeStream *stream;
	GByteArray *output;
	GError *error = NULL;
	const char *newline;
	char *word, *value, *input, *expected;
	guint i, j, dos, international, parsed;

	for (i = 0; i < G_N_ELEMENTS (widths); i++) {
		g_mime_format_options_set_max_line_length (options, widths[i]);
		for (dos = 0; dos < 2; dos++) {
			g_mime_format_options_set_newline_format (options, dos ?
				GMIME_NEWLINE_FORMAT_DOS : GMIME_NEWLINE_FORMAT_UNIX);
			newline = g_mime_format_options_get_newline (options);
			for (international = 0; international < 2; international++) {
				g_mime_format_options_set_allow_international (options, international);
				for (j = 0; j < G_N_ELEMENTS (fields); j++) {
					word = g_strnfill (widths[i] - strlen (fields[j]) - (j == 3 ? 7 : 2), 'a');
					value = j == 3 ? g_strconcat (word, "@a, x@b", NULL) :
						g_strconcat (word, " x", NULL);
					input = g_strconcat (fields[j], ": ", value, newline, NULL);
					for (parsed = 0; parsed < 2; parsed++) {
						header = g_mime_header_parse_text (NULL, input, &error);
						g_assert_no_error (error);
						g_assert_nonnull (header);
						if (!parsed)
							g_mime_header_set_value (header, options, value, "utf-8");

						if (parsed && (!international || j != 0))
							expected = g_strconcat (fields[j], ": ", value, newline, NULL);
						else if (j == 3)
							expected = g_strconcat (fields[j], ": ", word, "@a,", newline, " x@b", newline, NULL);
						else
							expected = g_strconcat (fields[j], ": ", word, newline, " x", newline, NULL);

						stream = g_mime_stream_mem_new ();
						testsuite_check ("field length formatting[%u,%u,%u,%u,%u]",
								 widths[i], dos, international, j, parsed);
						try {
							if (g_mime_header_write_to_stream (header, options, stream) < 0)
								throw (exception_new ("could not serialize header"));
							output = g_mime_stream_mem_get_byte_array (GMIME_STREAM_MEM (stream));
							if (output->len != strlen (expected) ||
							    memcmp (output->data, expected, output->len))
								throw (exception_new ("field length changed folding or raw field bytes:\n"
										      "expected: %s\nactual: %.*s",
										      expected, (int) output->len, output->data));
							testsuite_check_passed ();
						} catch (ex) {
							testsuite_check_failed ("%s", ex->message);
						} finally;
						g_object_unref (stream);
						g_object_unref (header);
						g_free (expected);
					}
					g_free (input);
					g_free (value);
					g_free (word);
				}
			}
		}
	}

	g_mime_format_options_free (options);
}

static void
test_header_formatting_parity (void)
{
	static const guint widths[] = { 60, 61, 78, 120, 998 };
	static const struct {
		const char *field, *value, *wire;
	} cases[] = {
		{ "To", "invalid <", "To: invalid <" },
		{ "To", "a@example.com, invalid <", "To: a@example.com, invalid <" },
		{ "Subject", "", "Subject:" },
		{ "X-Test", "", "X-Test:" }
	};
	GMimeFormatOptions *options = g_mime_format_options_new ();
	GMimeHeaderList *headers = g_mime_header_list_new (NULL);
	GMimeHeader *header;
	const char *newline;
	char *wire, *expected, *value, *word, *decoded, *unfolded;
	guint i, j, dos, international;
	int offset;

	for (i = 0; i < G_N_ELEMENTS (widths); i++) {
		g_mime_format_options_set_max_line_length (options, widths[i]);
		for (dos = 0; dos < 2; dos++) {
			g_mime_format_options_set_newline_format (options, dos ?
				GMIME_NEWLINE_FORMAT_DOS : GMIME_NEWLINE_FORMAT_UNIX);
			newline = g_mime_format_options_get_newline (options);
			for (international = 0; international < 2; international++) {
				g_mime_format_options_set_allow_international (options, international);
				for (j = 0; j < G_N_ELEMENTS (cases); j++) {
					g_mime_header_list_clear (headers);
					g_mime_header_list_append (headers, cases[j].field, "original", NULL);
					header = g_mime_header_list_get_header_at (headers, 0);
					g_mime_header_set_value (header, options, cases[j].value, "utf-8");
					wire = g_mime_header_list_to_string (headers, options);
					expected = g_strconcat (cases[j].wire,
						international && !*cases[j].value ? " " : "", newline, NULL);
					testsuite_check ("header formatter parity[%u,%u,%u,%u]", widths[i], dos, international, j);
					try {
						if (strcmp (wire, expected))
							throw (exception_new ("invalid address fallback or empty header changed: %s", wire));
						testsuite_check_passed ();
					} catch (ex) {
						testsuite_check_failed ("%s", ex->message);
					} finally;
					g_free (expected);
					g_free (wire);
				}

				g_mime_header_list_clear (headers);
				g_mime_header_list_append (headers, "Subject", "", NULL);
				header = g_mime_header_list_get_header_at (headers, 0);
				word = g_strnfill (widths[i] - 12, 'x');
				value = g_strconcat (word, "\t \t tail", NULL);
				expected = g_strdup_printf ("Subject: %s\t \t%s tail%s", word, newline, newline);
				g_mime_header_set_value (header, options, value, "utf-8");
				wire = g_mime_header_list_to_string (headers, options);
				testsuite_check ("whitespace-run folding parity[%u,%u,%u]", widths[i], dos, international);
				try {
					if (strcmp (wire, expected))
						throw (exception_new ("whitespace run folded at the wrong character"));
					testsuite_check_passed ();
				} catch (ex) {
					testsuite_check_failed ("%s", ex->message);
				} finally;
				g_free (expected);
				g_free (wire);
				g_free (value);
				g_free (word);

				for (offset = -1; international && offset <= 1; offset++) {
					word = g_strnfill (widths[i] + offset, 'x');
					g_mime_header_set_value (header, options, word, "utf-8");
					wire = g_mime_header_list_to_string (headers, options);
					decoded = g_mime_utils_header_decode_text (NULL, wire + strlen ("Subject:"));
					unfolded = g_mime_utils_header_unfold (decoded);
					testsuite_check ("international oversized word[%u,%u,%d]", widths[i], dos, offset);
					try {
						if (strcmp (unfolded, word) ||
						    ((strstr (wire, "=?") != NULL) != (offset >= 0)))
							throw (exception_new ("international folding inserted whitespace into a word"));
						testsuite_check_passed ();
					} catch (ex) {
						testsuite_check_failed ("%s", ex->message);
					} finally;
					g_free (unfolded);
					g_free (decoded);
					g_free (wire);
					g_free (word);
				}

				g_mime_header_list_clear (headers);
				g_mime_header_list_append (headers, "To", "", NULL);
				header = g_mime_header_list_get_header_at (headers, 0);
				testsuite_header_set_raw_text (header, "\tinvalid <\r\n");
				wire = g_mime_header_list_to_string (headers, options);
				expected = g_strconcat ("To:\tinvalid <", newline, NULL);
				testsuite_check ("untouched invalid address[%u,%u,%u]", widths[i], dos, international);
				try {
					if (strcmp (wire, expected) || strcmp (testsuite_header_raw_text (header), "\tinvalid <\r\n"))
						throw (exception_new ("invalid raw address was discarded or mutated"));
					testsuite_check_passed ();
				} catch (ex) {
					testsuite_check_failed ("%s", ex->message);
				} finally;
				g_free (expected);
				g_free (wire);
			}
		}
	}

	g_mime_format_options_set_max_line_length (options, 60);
	g_mime_format_options_set_allow_international (options, TRUE);
	word = g_strdup ("attachment; filename=\"\320\237\321\200\320\270\320\262\320\265\321\202.txt\"");
	for (dos = 0; dos < 2; dos++) {
		g_mime_format_options_set_newline_format (options, dos ?
			GMIME_NEWLINE_FORMAT_DOS : GMIME_NEWLINE_FORMAT_UNIX);
		newline = g_mime_format_options_get_newline (options);
		for (j = 0; j < 2; j++) {
			g_mime_header_list_clear (headers);
			g_mime_header_list_append (headers, "Content-Disposition", "", NULL);
			header = g_mime_header_list_get_header_at (headers, 0);
			value = g_strconcat (word, j ? "; after=end" : "", NULL);
			g_mime_header_set_value (header, options, value, "utf-8");
			wire = g_mime_header_list_to_string (headers, options);
			expected = j ?
				g_strdup_printf ("Content-Disposition: attachment;%s\t%s; after=end%s",
						 newline, word + strlen ("attachment; "), newline) :
				g_strconcat ("Content-Disposition: ", word, newline, NULL);
			testsuite_check ("international parameter exact physical fit[%u,%u]", dos, j);
			try {
				if (strcmp (wire, expected))
					throw (exception_new ("exact fit lost parity or left no room for the next semicolon"));
				testsuite_check_passed ();
			} catch (ex) {
				testsuite_check_failed ("%s", ex->message);
			} finally;
			g_free (expected);
			g_free (wire);
			g_free (value);
		}
	}
	g_free (word);
	g_object_unref (headers);
	g_mime_format_options_free (options);
}

static void
test_header_serialization_parity (void)
{
	static const char input[] =
		"To:\tinvalid <\r\nMIME-Version: 1.0\r\n"
		"X-Preserved:\tkeep  spaces\r\n \tand fold\r\n"
		"Content-Type: text/plain; charset=utf-8\r\n"
		"Content-Transfer-Encoding: 8bit\r\n\r\nbody\r\n";
	GMimeFormatOptions *options = g_mime_format_options_new ();
	GMimeEntity *entity;
	GMimeStream *source, *output;
	GError *error = NULL;
	GByteArray *bytes;
	const char *newline;
	char *expected;
	guint dos, international, message;

	for (dos = 0; dos < 2; dos++) {
		g_mime_format_options_set_newline_format (options, dos ?
			GMIME_NEWLINE_FORMAT_DOS : GMIME_NEWLINE_FORMAT_UNIX);
		newline = g_mime_format_options_get_newline (options);
		for (international = 0; international < 2; international++) {
			g_mime_format_options_set_allow_international (options, international);
			expected = g_strconcat ("To:\tinvalid <", newline, "MIME-Version: 1.0", newline,
				international ? "X-Preserved: keep  spaces \tand fold" : "X-Preserved:\tkeep  spaces",
				international ? "" : newline, international ? "" : " \tand fold", newline,
				"Content-Type: text/plain; charset=utf-8", newline,
				"Content-Transfer-Encoding: 8bit", newline, newline, "body", newline, NULL);
			for (message = 0; message < 2; message++) {
				entity = NULL;
				source = g_mime_stream_mem_new_with_buffer (input, sizeof input - 1);
				output = g_mime_stream_mem_new ();
				testsuite_check ("header serialization parity[%u,%u,%u]", dos, international, message);
				try {
					entity = message ? GMIME_ENTITY (g_mime_message_load (NULL, source, &error)) :
						g_mime_entity_load (NULL, source, &error);
					if (!entity || error)
						throw (exception_new ("could not load header fixture"));
					if (g_mime_entity_write_to_stream (entity, options, output) < 0)
						throw (exception_new ("could not serialize header fixture"));
					bytes = g_mime_stream_mem_get_byte_array (GMIME_STREAM_MEM (output));
					if (bytes->len != strlen (expected) || memcmp (bytes->data, expected, bytes->len))
						throw (exception_new ("message or entity formatting differs from MimeKit"));
					testsuite_check_passed ();
				} catch (ex) {
					testsuite_check_failed ("%s", ex->message);
				} finally;
				g_clear_error (&error);
				g_clear_object (&entity);
				g_object_unref (output);
				g_object_unref (source);
			}
			g_free (expected);
		}
	}
	g_mime_format_options_free (options);
}

static void
test_address_header_whitespace (void)
{
	static const char *names[] = { "To", "Cc", "From", "Reply-To", "Resent-To" };
	static const struct {
		const char *value, *raw;
	} cases[] = {
		{ "\"First\tLast\" <user@example.com>", " First Last <user@example.com>" },
		{ "\"First\t\t Last\" <user@example.com>", " First   Last <user@example.com>" },
		{ "\"First,\tLast\" <user@example.com>", " \"First, Last\" <user@example.com>" },
		{ "\"Group\tName\": \"First\tLast\" <user@example.com>;", " Group Name: First Last <user@example.com>;" },
		{ "user@example.com (First\tLast)", " First Last <user@example.com>" },
		{ "user@example.com (\"First\"\tLast)", " \"\\\"First\\\" Last\" <user@example.com>" },
		{ "user@example.com (First\\\tLast)", " \"First\\\\ Last\" <user@example.com>" },
		{ "user (First\tLast)", " First Last <user>" },
		{ "Group: user (First\tLast);, other@example.com", " Group: First Last <user>;, other@example.com" },
		{ "user (First\tLast), other@example.com", " <user>, other@example.com" },
		{ "\"First\\\tLast\" <user@example.com>", " First Last <user@example.com>" }
	};
	GMimeHeaderList *headers = g_mime_header_list_new (NULL);
	GMimeFormatOptions *format = g_mime_format_options_new ();
	GMimeHeader *header;
	const char *newline;
	char *expected, *wire, *raw;
	guint dos, j, k;

	for (dos = 0; dos < 2; dos++) {
		g_mime_format_options_set_newline_format (format, dos ?
			GMIME_NEWLINE_FORMAT_DOS : GMIME_NEWLINE_FORMAT_UNIX);
		newline = g_mime_format_options_get_newline (format);
		for (j = 0; j < G_N_ELEMENTS (names); j++) {
			for (k = 0; k < G_N_ELEMENTS (cases); k++) {
				g_mime_header_list_clear (headers);
				g_mime_header_list_append (headers, names[j], "", NULL);
				header = g_mime_header_list_get_header_at (headers, 0);
				g_mime_header_set_value (header, format, cases[k].value, NULL);
				raw = g_strconcat (cases[k].raw, newline, NULL);
				expected = g_strconcat (names[j], ":", raw, NULL);
				wire = g_mime_header_list_to_string (headers, format);
				testsuite_check ("address header name whitespace[%u,%u,%u]", dos, j, k);
				try {
					if (g_strcmp0 (testsuite_header_raw_text (header), raw) || strcmp (wire, expected))
						throw (exception_new ("address header did not normalize literal tabs: %s", wire));
					if (g_strcmp0 (g_mime_header_get_value (header), cases[k].value))
						throw (exception_new ("address parsing changed the cached header text"));
					testsuite_check_passed ();
				} catch (ex) {
					testsuite_check_failed ("%s", ex->message);
				} finally;
				g_free (wire);
				g_free (expected);
				g_free (raw);

				raw = g_strconcat (" ", cases[k].value, newline, NULL);
				testsuite_header_set_raw_text (header, raw);
				g_mime_header_get_value (header);
				expected = g_strconcat (names[j], ":", raw, NULL);
				wire = g_mime_header_list_to_string (headers, format);
				testsuite_check ("untouched raw address name whitespace[%u,%u,%u]", dos, j, k);
				if (g_strcmp0 (testsuite_header_raw_text (header), raw) || strcmp (wire, expected))
					testsuite_check_failed ("address normalization changed an untouched raw header");
				else
					testsuite_check_passed ();
				g_free (wire);
				g_free (expected);
				g_free (raw);
			}
		}
	}
	g_mime_format_options_free (format);
	g_object_unref (headers);
}

static void
test_literal_encoded_word_headers (void)
{
	static const char *names[] = { "Subject", "Comments", "X-Literal" };
	static const char *values[] = {
		"=?utf-8?q?hello?=",
		"=?utf-8?b?SGVsbG8=?=",
		"=?utf-8?Q?caf=C3=A9?=",
		"=?utf-8?q?one?= =?utf-8?q?two?=",
		"=?utf-8?q?one?==?utf-8?q?two?=",
		"before =?utf-8?q?hello?= after",
		"\xe4\xb8\xad=?utf-8?q?hello?=",
		"=?utf-8?q?hello?=\xf0\x9f\x98\x80"
	};
	static const guint widths[] = { 60, 78, 998 };
	GMimeFormatOptions *format = g_mime_format_options_new ();
	GMimeHeaderList *headers = g_mime_header_list_new (NULL);
	GMimeHeader *header;
	char *wire, *raw, *expected;
	guint i, j, k, dos;

	for (i = 0; i < G_N_ELEMENTS (widths); i++) {
		g_mime_format_options_set_max_line_length (format, widths[i]);
		for (dos = 0; dos < 2; dos++) {
			g_mime_format_options_set_newline_format (format, dos ?
				GMIME_NEWLINE_FORMAT_DOS : GMIME_NEWLINE_FORMAT_UNIX);
			for (j = 0; j < G_N_ELEMENTS (names); j++) {
				g_mime_header_list_clear (headers);
				g_mime_header_list_append (headers, names[j], "", NULL);
				header = g_mime_header_list_get_header_at (headers, 0);
				for (k = 0; k < G_N_ELEMENTS (values); k++) {
					g_mime_header_set_value (header, format, values[k], "utf-8");
					raw = g_strdup (testsuite_header_raw_text (header));
					expected = g_strdup_printf ("%s:%s", names[j], raw);
					wire = g_mime_header_list_to_string (headers, format);
					testsuite_header_set_raw_text (header, raw);
					testsuite_check ("literal encoded-word header[%u,%u,%u,%u]", widths[i], dos, j, k);
					try {
						if (strcmp (wire, expected))
							throw (exception_new ("literal header serialization changed: %s", wire));
						if (g_strcmp0 (g_mime_header_get_value (header), values[k]))
							throw (exception_new ("serialized header changed the literal text: %s",
									      g_mime_header_get_value (header)));
						testsuite_check_passed ();
					} catch (ex) {
						testsuite_check_failed ("%s", ex->message);
					} finally;
					g_free (expected);
					g_free (wire);
					g_free (raw);
				}

				raw = g_strdup_printf (" =?UtF-8?q?hello?=%s", g_mime_format_options_get_newline (format));
				testsuite_header_set_raw_text (header, raw);
				expected = g_strdup_printf ("%s:%s", names[j], raw);
				wire = NULL;
				testsuite_check ("literal protection preserves raw headers[%u,%u,%u]", widths[i], dos, j);
				try {
					if (g_strcmp0 (g_mime_header_get_value (header), "hello"))
						throw (exception_new ("raw encoded-word header was not decoded"));
					wire = g_mime_header_list_to_string (headers, format);
					if (strcmp (wire, expected))
						throw (exception_new ("literal protection re-encoded an untouched raw header"));
					testsuite_check_passed ();
				} catch (ex) {
					testsuite_check_failed ("%s", ex->message);
				} finally;
				g_free (wire);
				g_free (expected);
				g_free (raw);
			}
		}
	}
	g_object_unref (headers);
	g_mime_format_options_free (format);
}

static void
test_structured_header_line_lengths (void)
{
	static const guint widths[] = { 60, 120, 998 };
	static const struct {
		const char *value, *narrow, *wide;
	} received[] = {
		{ "", "\n", "\n" },
		{ ";", " ;\n", " ;\n" },
		{ "from example.com by relay.example with ESMTP id abc; Mon, 19 Oct 2026 01:02:03 +0000",
		  " from example.com by relay.example with ESMTP\n\tid abc; Mon, 19 Oct 2026 01:02:03 +0000\n",
		  " from example.com by relay.example with ESMTP id abc; Mon, 19 Oct 2026 01:02:03 +0000\n" },
		{ "(first) (second) from example.com by relay.example; date",
		  " (first) (second) from example.com by relay.example\n\t; date\n",
		  " (first) (second) from example.com by relay.example; date\n" },
		{ "FROM host  UNKNOWN value (comment) ;  date",
		  " FROM host UNKNOWN value (comment); date\n",
		  " FROM host UNKNOWN value (comment); date\n" },
		{ "(a(b)) FROM (c) h(d) BY r; date",
		  " (a(b)) FROM (c) h(d) BY r; date\n",
		  " (a(b)) FROM (c) h(d) BY r; date\n" },
		{ "(a\\)b) from h; date", " (a\\)b) from h; date\n", " (a\\)b) from h; date\n" },
		{ "(unterminated", " (unterminated\n", " (unterminated\n" },
		{ "from host (unterminated", " from host (unterminated\n", " from host (unterminated\n" },
		{ "from\r\n\texample.com\r\n\tby relay.example; date",
		  " from\texample.com by relay.example; date\n", " from\texample.com by relay.example; date\n" }
	};
	GMimeFormatOptions *options = g_mime_format_options_new ();
	GMimeHeaderList *list = g_mime_header_list_new (NULL);
	GMimeHeader *header;
	GMimeMessageIdList *refs;
	const char *newline, *pattern;
	char *padding, *second, *value, *expected, *full, *wire, **lines;
	guint i, j, dos;
	int offset;

	for (i = 0; i < G_N_ELEMENTS (widths); i++) {
		g_mime_format_options_set_max_line_length (options, widths[i]);
		for (dos = 0; dos < 2; dos++) {
			g_mime_format_options_set_newline_format (options, dos ?
				GMIME_NEWLINE_FORMAT_DOS : GMIME_NEWLINE_FORMAT_UNIX);
			newline = g_mime_format_options_get_newline (options);
			g_mime_header_list_clear (list);
			g_mime_header_list_append (list, "References", "", NULL);
			header = g_mime_header_list_get_header_at (list, 0);
			for (offset = -1; offset <= 1; offset++) {
				testsuite_check ("References exact line boundary[%u,%u,%d]", widths[i], dos, offset);
				padding = g_strnfill (widths[i] - 42 + offset, 'b');
				second = g_strconcat (padding, "@example.com", NULL);
				value = g_strdup_printf ("<a@example.com> <%s> <c@example.com>", second);
				expected = offset > 0 ?
					g_strdup_printf (" <a@example.com>%s\t<%s> <c@example.com>%s", newline, second, newline) :
					g_strdup_printf (" <a@example.com> <%s>%s\t<c@example.com>%s", second, newline, newline);
				g_mime_header_set_value (header, options, value, NULL);
				wire = g_mime_header_list_to_string (list, options);
				full = g_strconcat ("References:", expected, NULL);
				refs = g_mime_message_id_list_parse (NULL, testsuite_header_raw_text (header));
				try {
					if (g_strcmp0 (testsuite_header_raw_text (header), expected) != 0 ||
					    g_strcmp0 (wire, full) != 0)
						throw (exception_new ("reference folding did not respect the exact boundary"));
					if (g_mime_message_id_list_length (refs) != 3 ||
					    g_strcmp0 (g_mime_message_id_list_get_message_id (refs, 1), second) != 0)
						throw (exception_new ("reference tokens did not round-trip"));
					testsuite_check_passed ();
				} catch (ex) {
					testsuite_check_failed ("%s", ex->message);
				} finally;
				g_object_unref (refs);
				g_free (padding);
				g_free (second);
				g_free (value);
				g_free (expected);
				g_free (full);
				g_free (wire);
			}

			g_mime_header_list_clear (list);
			g_mime_header_list_append (list, "Received", "", NULL);
			header = g_mime_header_list_get_header_at (list, 0);
			for (j = 0; j < G_N_ELEMENTS (received); j++) {
				testsuite_check ("Received clause folding[%u,%u,%u]", widths[i], dos, j);
				pattern = widths[i] == 60 ? received[j].narrow : received[j].wide;
				lines = g_strsplit (pattern, "\n", -1);
				expected = g_strjoinv (newline, lines);
				g_strfreev (lines);
				g_mime_header_set_value (header, options, received[j].value, NULL);
				wire = g_mime_header_list_to_string (list, options);
				full = g_strconcat ("Received:", expected, NULL);
				try {
					if (g_strcmp0 (testsuite_header_raw_text (header), expected) != 0 ||
					    g_strcmp0 (wire, full) != 0)
						throw (exception_new ("unexpected Received clause formatting: %s",
								      testsuite_header_raw_text (header)));
					testsuite_check_passed ();
				} catch (ex) {
					testsuite_check_failed ("%s", ex->message);
				} finally;
				g_free (expected);
				g_free (full);
				g_free (wire);
			}
			testsuite_check ("Received raw value preservation[%u,%u]", widths[i], dos);
			testsuite_header_set_raw_text (header, " from h\n\tby r; date\n");
			wire = g_mime_header_list_to_string (list, options);
			full = g_strdup_printf ("Received: from h%s\tby r; date%s", newline, newline);
			try {
				if (g_strcmp0 (wire, full) != 0)
					throw (exception_new ("raw header was unexpectedly reformatted"));
				testsuite_check_passed ();
			} catch (ex) {
				testsuite_check_failed ("%s", ex->message);
			} finally;
			g_free (full);
			g_free (wire);
		}
	}
	g_object_unref (list);
	g_mime_format_options_free (options);
}

static void
test_message_id_formatting (void)
{
	static const char *names[] = {
		"Message-Id", "Content-Id", "Resent-Message-Id", "Original-Message-Id", "In-Reply-To"
	};
	static const struct {
		const char *input, *expected;
	} cases[] = {
		{ "", "" },
		{ "(comment) <a@example.com> (other) <b@example.com>",
		  "(comment) <a@example.com> (other) <b@example.com>" },
		{ " <a@example.com>\r\n\t<b@example.com> ", "<a@example.com>\t<b@example.com>" },
		{ NULL, NULL }
	};
	GMimeFormatOptions *options = g_mime_format_options_new ();
	GMimeHeaderList *list = g_mime_header_list_new (NULL);
	GMimeHeader *header;
	const char *newline;
	char *padding = g_strnfill (1200, 'a'), *long_id, *expected, *full, *wire;
	guint i, j, dos;

	long_id = g_strdup_printf ("<%s@example.com>", padding);
	g_free (padding);
	for (i = 0; i < G_N_ELEMENTS (names); i++) {
		g_mime_header_list_clear (list);
		g_mime_header_list_append (list, names[i], "", NULL);
		header = g_mime_header_list_get_header_at (list, 0);
		for (dos = 0; dos < 2; dos++) {
			g_mime_format_options_set_max_line_length (options, dos ? 998 : 60);
			g_mime_format_options_set_newline_format (options, dos ?
				GMIME_NEWLINE_FORMAT_DOS : GMIME_NEWLINE_FORMAT_UNIX);
			newline = g_mime_format_options_get_newline (options);
			for (j = 0; j < G_N_ELEMENTS (cases); j++) {
				testsuite_check ("%s opaque formatting[%u,%u]", names[i], dos, j);
				expected = g_strdup_printf (" %s%s", cases[j].expected ? cases[j].expected : long_id, newline);
				g_mime_header_set_value (header, options, cases[j].input ? cases[j].input : long_id, NULL);
				wire = g_mime_header_list_to_string (list, options);
				full = g_strconcat (names[i], ":", expected, NULL);
				try {
					if (g_strcmp0 (testsuite_header_raw_text (header), expected) != 0 ||
					    g_strcmp0 (wire, full) != 0)
						throw (exception_new ("message-id value was parsed or folded"));
					testsuite_check_passed ();
				} catch (ex) {
					testsuite_check_failed ("%s", ex->message);
				} finally;
				g_free (expected);
				g_free (full);
				g_free (wire);
			}
		}
	}
	g_free (long_id);
	g_object_unref (list);
	g_mime_format_options_free (options);
}

static void
test_notification_options_formatting (void)
{
	static const guint widths[] = { 60, 78, 120, 998 };
	static const struct {
		const char *input, *narrow, *wide;
	} cases[] = {
		{ "", "\n", "\n" },
		{ " \t\r\n ", "\n", "\n" },
		{ " x =\r\n optional,\t one ; ", " x=optional,one;\n", " x=optional,one;\n" },
		{ "x=optional,one; y=required,two;",
		  " x=optional,one;\n\ty=required,two;\n", " x=optional,one; y=required,two;\n" },
		{ "X=optional,A; X=required,B", " X=optional,A; X=required,B\n", " X=optional,A; X=required,B\n" },
		{ "x=optional,\"a b\"", " x=optional,\"ab\"\n", " x=optional,\"ab\"\n" },
		{ "bare word ; other", " bareword;other\n", " bareword;other\n" },
		{ "x=; y=", " x=; y=\n", " x=; y=\n" }
	};
	const char *name = "Disposition-Notification-Options", *newline, *pattern;
	GMimeFormatOptions *options = g_mime_format_options_new ();
	GMimeHeaderList *list = g_mime_header_list_new (NULL);
	GMimeHeader *header;
	char *padding, *input, *expected, *direct, *full, *wire, **lines;
	guint i, j, dos;
	int offset;

	g_mime_header_list_append (list, name, "", NULL);
	header = g_mime_header_list_get_header_at (list, 0);
	for (i = 0; i < G_N_ELEMENTS (widths); i++) {
		g_mime_format_options_set_max_line_length (options, widths[i]);
		for (dos = 0; dos < 2; dos++) {
			g_mime_format_options_set_newline_format (options, dos ?
				GMIME_NEWLINE_FORMAT_DOS : GMIME_NEWLINE_FORMAT_UNIX);
			newline = g_mime_format_options_get_newline (options);
			for (j = 0; j < G_N_ELEMENTS (cases); j++) {
				testsuite_check ("notification options normalization[%u,%u,%u]", widths[i], dos, j);
				pattern = widths[i] == 60 ? cases[j].narrow : cases[j].wide;
				lines = g_strsplit (pattern, "\n", -1);
				expected = g_strjoinv (newline, lines);
				g_strfreev (lines);
				direct = g_mime_header_format_disposition_notification_options (
					header, i == 1 && !dos ? NULL : options, cases[j].input, NULL);
				g_mime_header_set_value (header, options, cases[j].input, NULL);
				wire = g_mime_header_list_to_string (list, options);
				full = g_strconcat (name, ":", expected, NULL);
				try {
					if (g_strcmp0 (direct, expected) != 0 ||
					    g_strcmp0 (testsuite_header_raw_text (header), expected) != 0 ||
					    g_strcmp0 (wire, full) != 0)
						throw (exception_new ("unexpected notification options: %s", direct));
					testsuite_check_passed ();
				} catch (ex) {
					testsuite_check_failed ("%s", ex->message);
				} finally;
				g_free (expected);
				g_free (direct);
				g_free (full);
				g_free (wire);
			}
			for (offset = -1; offset <= 1; offset++) {
				testsuite_check ("notification options line boundary[%u,%u,%d]", widths[i], dos, offset);
				padding = g_strnfill (widths[i] - (strlen (name) + strlen (": x=a; y=;")) + offset, 'b');
				input = g_strdup_printf ("x=a; y=%s; z=q", padding);
				expected = offset > 0 ?
					g_strdup_printf (" x=a;%s\ty=%s; z=q%s", newline, padding, newline) :
					g_strdup_printf (" x=a; y=%s;%s\tz=q%s", padding, newline, newline);
				g_mime_header_set_value (header, options, input, NULL);
				try {
					if (g_strcmp0 (testsuite_header_raw_text (header), expected) != 0)
						throw (exception_new ("notification parameter did not fit the exact line boundary"));
					testsuite_check_passed ();
				} catch (ex) {
					testsuite_check_failed ("%s", ex->message);
				} finally;
				g_free (padding);
				g_free (input);
				g_free (expected);
			}
		}
	}
	testsuite_check ("oversized notification parameters remain intact");
	padding = g_strnfill (1200, 'a');
	input = g_strdup_printf ("x=%s; y=%s", padding, padding);
	expected = g_strdup_printf (" x=%s;\r\n\ty=%s\r\n", padding, padding);
	direct = g_mime_header_format_disposition_notification_options (header, options, input, NULL);
	try {
		if (g_strcmp0 (direct, expected) != 0)
			throw (exception_new ("oversized parameters were split or the first one was moved"));
		testsuite_check_passed ();
	} catch (ex) {
		testsuite_check_failed ("%s", ex->message);
	} finally;
	g_free (padding);
	g_free (input);
	g_free (expected);
	g_free (direct);

	testsuite_check ("raw notification options remain untouched");
	testsuite_header_set_raw_text (header, " x = optional, one;\n\ty = required, two\n");
	wire = g_mime_header_list_to_string (list, options);
	try {
		if (g_strcmp0 (wire, "Disposition-Notification-Options: x = optional, one;\r\n\ty = required, two\r\n") != 0)
			throw (exception_new ("raw notification options were reformatted"));
		testsuite_check_passed ();
	} catch (ex) {
		testsuite_check_failed ("%s", ex->message);
	} finally;
	g_free (wire);
	g_object_unref (list);
	g_mime_format_options_free (options);
}

static struct {
	const char *value;
	const char *rfc2047_encoded;
	const char *rfc2231_encoded;
} parameter_lists[] = {
	{ "KKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKK=\"ќ\"",
	  ";\n\tKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKK=\"=?utf-8?b?0Zw=?=\"\n",
	  ";\n\tKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKK*0*=utf-8'';\n\tKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKK*1*=%D1%9C\n" },
};

static void
test_parameter_lists (void)
{
	GMimeFormatOptions *rfc2047, *rfc2231;
	GMimeParamList *list;
	GString *str;
	size_t len;
	guint i;

	str = g_string_new ("Content-Type: text/plain");
	len = str->len;

	rfc2047 = g_mime_format_options_new ();
	g_mime_format_options_set_newline_format (rfc2047, GMIME_NEWLINE_FORMAT_UNIX);
	g_mime_format_options_set_param_encoding_method (rfc2047, GMIME_PARAM_ENCODING_METHOD_RFC2047);

	rfc2231 = g_mime_format_options_clone (rfc2047);
	g_mime_format_options_set_param_encoding_method (rfc2231, GMIME_PARAM_ENCODING_METHOD_RFC2231);

	for (i = 0; i < G_N_ELEMENTS (parameter_lists); i++) {
		testsuite_check ("parameter_lists[%u]", i);

		list = g_mime_param_list_parse (NULL, parameter_lists[i].value);

		try {
			g_mime_param_list_encode (list, rfc2047, TRUE, str);
			
			if (strcmp (parameter_lists[i].rfc2047_encoded, str->str + len) != 0)
				throw (exception_new ("rfc2047 encoded values do not match: %s", str->str + len));

			g_string_truncate (str, len);
			g_mime_param_list_encode (list, rfc2231, TRUE, str);
			
			if (strcmp (parameter_lists[i].rfc2231_encoded, str->str + len) != 0)
				throw (exception_new ("rfc2231 encoded values do not match: %s", str->str + len));
			
			testsuite_check_passed ();
		} catch (ex) {
			testsuite_check_failed ("parameter_lists[%u] failed: %s", i, ex->message);
		} finally;

		g_string_truncate (str, len);
		g_object_unref (list);
	}

	g_mime_format_options_free (rfc2047);
	g_mime_format_options_free (rfc2231);
	g_string_free (str, TRUE);
}

static void
test_parameter_percent_encoding (void)
{
	static const struct {
		const char *input;
		const char *expected;
	} cases[] = {
		{ "filename*=utf-8'en'%E2%82%AC.txt", "\342\202\254.txt" },
		{ "filename*0*=utf-8'en'%E; filename*1*=2%82; filename*2*=%AC.txt", "\342\202\254.txt" },
		{ "filename*2*=%AC.txt; filename*1*=2%82; filename*0*=utf-8'en'%E", "\342\202\254.txt" },
		{ "filename*0*=utf-8''a%; filename*1*=\"\"; filename*2*=41.txt", "aA.txt" },
		{ "filename*=utf-8''a%", "a" },
		{ "filename*=utf-8''a%4", "a" },
		{ "filename*=utf-8''a%G%41", "a%G%41" },
		{ "filename*=utf-8''a%%41", "a%%41" },
		{ "filename*=utf-8''a%GG%41", "a%GGA" },
		{ "filename*=utf-8''a+b%20c", "a+b c" },
		{ "filename*0*=utf-8''a%; filename*1=b; filename*2*=41.txt", "ab41.txt" },
		{ "filename*0*=utf-8''a%4; filename*1=b; filename*2*=1.txt", "ab1.txt" },
		{ "filename*0*=utf-8''%41; filename*1=\"%42\"; filename*2*=%43", "A%42C" },
		{ "filename*0=\"first-\"; filename*1*=utf-8''%41.txt", "first-A.txt" },
		{ "filename*=utf-8''a%; other*=utf-8''41", "a" },
		{ "filename*0*=utf-8''%4; filename*2*=1", "A" }
	};
	GMimeParamList *list;
	GMimeParam *param;
	const char *value;
	guint i;

	for (i = 0; i < G_N_ELEMENTS (cases); i++) {
		list = g_mime_param_list_parse (NULL, cases[i].input);
		testsuite_check ("parameter percent encoding[%u]", i);
		try {
			if (!(param = g_mime_param_list_get_parameter (list, "filename")))
				throw (exception_new ("missing filename parameter"));
			value = g_mime_param_get_value (param);
			if (g_strcmp0 (value, cases[i].expected) != 0)
				throw (exception_new ("unexpected value: %s", value ? value : "(null)"));
			if (g_mime_param_get_encoding_method (param) != GMIME_PARAM_ENCODING_METHOD_RFC2231)
				throw (exception_new ("unexpected encoding method"));
			if (i < 3 && g_strcmp0 (g_mime_param_get_lang (param), "en") != 0)
				throw (exception_new ("missing language after sorting continuations"));
			if (!g_mime_param_get_charset (param) ||
			    g_ascii_strcasecmp (g_mime_param_get_charset (param), "utf-8") != 0)
				throw (exception_new ("missing charset"));
			testsuite_check_passed ();
		} catch (ex) {
			testsuite_check_failed ("parameter percent encoding[%u]: %s", i, ex->message);
		} finally;
		g_object_unref (list);
	}
}

static void
test_parameter_continuation_charsets (void)
{
	static const struct {
		const char *fallback;
		const char *input;
		const char *expected;
		const char *charset;
		const char *lang;
	} cases[] = {
		{ "windows-1251", "filename*0=\"\317\360\350\"; filename*1=\"\342\345\362\"",
		  "\320\237\321\200\320\270\320\262\320\265\321\202", NULL, NULL },
		{ "windows-1251", "filename*0=\"\303\251\"; filename*1=\".txt\"",
		  "\303\251.txt", NULL, NULL },
		{ "utf-8", "filename*0=\"\342\"; filename*1=\"\202\254\"",
		  "\303\242\302\202\302\254", NULL, NULL },
		{ "windows-1251", "filename*0*=iso-8859-1'fr'%E9; filename*1=\"\317\360\350\342\345\362\"; filename*2*=%E9",
		  "\303\251\320\237\321\200\320\270\320\262\320\265\321\202\303\251", "iso-8859-1", "fr" },
		{ "windows-1251", "filename*0=\"\317\360\350\"; filename*1*=iso-8859-1'en'%E9",
		  "\320\237\321\200\320\270\303\251", "iso-8859-1", "en" },
		{ "windows-1251", "filename*2*=%AC; filename*0*=utf-8'en'%E; filename*1*=2%82",
		  "\342\202\254", "utf-8", "en" },
		{ "windows-1251", "filename*0*=utf-8''%E2; filename*1=\"-\"; filename*2*=%82%AC",
		  "?-??", "utf-8", NULL },
		{ "windows-1251", "filename*0*=utf-8''%E2; filename*1=\"\"; filename*2*=%82%AC",
		  "???", "utf-8", NULL },
		{ "windows-1251", "filename*0*=utf-8''a%; filename*1=\"b\"; filename*2*=41",
		  "ab41", "utf-8", NULL },
		{ "windows-1251", "filename*0*=utf-8''a%; filename*1=\"\"; filename*2*=41",
		  "a41", "utf-8", NULL },
		{ "windows-1251", "filename*=x-gmime-unsupported'en'%C3%A9",
		  "\303\203\302\251", "iso-8859-1", "en" },
		{ "windows-1251", "filename*=%E9",
		  "\303\251", "iso-8859-1", NULL },
		{ "windows-1251", "filename*=''%E9",
		  "''\303\251", "iso-8859-1", NULL },
		{ "windows-1251", "filename*='en'%E9",
		  "'en'\303\251", "iso-8859-1", NULL },
		{ "windows-1251", "filename*=utf-8'%E9",
		  "utf-8'\303\251", "iso-8859-1", NULL },
		{ "windows-1251", "filename*=utf-8''%E2%82",
		  "?", "utf-8", NULL },
		{ "windows-1251", "filename*=utf-8''%E2%82x",
		  "?x", "utf-8", NULL },
		{ "windows-1251", "filename*=utf-8''%ED%A0%80",
		  "???", "utf-8", NULL },
		{ "windows-1251", "filename*=us-ascii''%C3%A9",
		  "??", "us-ascii", NULL },
		{ "windows-1251", "filename*=utf-16le''%00%D8%41%00",
		  "?A", "utf-16le", NULL },
		{ "windows-1251", "filename*=utf-16be''%D8%00%00%41",
		  "?A", "utf-16be", NULL },
		{ "windows-1251", "filename*=utf-32le''%FF%FF%FF%FF",
		  "?", "utf-32le", NULL },
		{ "windows-1251", "filename*0*=iso-2022-jp''%1B$B$; filename*1*=%22%1B(B",
		  "\343\201\202", "iso-2022-jp", NULL },
		{ "windows-1251", "filename*0*=iso-2022-jp''%1B$B$%22; filename*1=\"-\"; filename*2*=$$",
		  "\343\201\202-$$", "iso-2022-jp", NULL },
		{ "shift_jis", "filename*0=\"\225\\X\"; filename*1=\".txt\"",
		  "\350\241\250X.txt", NULL, NULL },
		{ "windows-1251", "filename*0=\"a\\\\b\"; filename*1=\"\317\\\"x\"",
		  "a\\b\320\237\"x", NULL, NULL },
		{ "windows-1251", "filename*0=\" a\r\n\tb \"; filename*1=\" c \"",
		  " a\tb  c ", NULL, NULL },
		{ "windows-1251", "filename*0=\"=?utf-8?q?hello?=\"; filename*1=\".txt\"",
		  "=?utf-8?q?hello?=.txt", NULL, NULL },
		{ "windows-1251", "filename*=\"utf-8'en'%E2%82%AC\"",
		  "\342\202\254", "utf-8", "en" },
		{ "windows-1251", "filename*0*=iso-8859-1''a; filename*1*=utf-8''%E9",
		  "autf-8''\303\251", "iso-8859-1", NULL }
	};
	GMimeParserOptions *options = g_mime_parser_options_new ();
	GMimeContentDisposition *disposition;
	GMimeContentType *content_type;
	GMimeParamList *list;
	GMimeParam *param;
	GError *error = NULL;
	const char *value, *charset;
	char *text, *prefix, *suffix, *expected;
	guint i;

	for (i = 0; i < G_N_ELEMENTS (cases); i++) {
		testsuite_check ("parameter continuation charset[%u]", i);
		list = NULL;
		content_type = NULL;
		disposition = NULL;
		try {
			if (!g_mime_parser_options_set_charset_encoding (options, cases[i].fallback, &error))
				throw (exception_new ("cannot configure charset: %s", error ? error->message : "(no error)"));
			list = g_mime_param_list_parse (options, cases[i].input);
			if (!(param = g_mime_param_list_get_parameter (list, "filename")))
				throw (exception_new ("missing filename parameter"));
			value = g_mime_param_get_value (param);
			if (g_strcmp0 (value, cases[i].expected) != 0 || !g_utf8_validate (value, -1, NULL))
				throw (exception_new ("unexpected UTF-8 value: %s", value ? value : "(null)"));
			charset = g_mime_param_get_charset (param);
			if (cases[i].charset ? !charset || g_ascii_strcasecmp (charset, cases[i].charset) : charset != NULL)
				throw (exception_new ("unexpected charset: %s", charset ? charset : "(null)"));
			if (g_strcmp0 (g_mime_param_get_lang (param), cases[i].lang) != 0)
				throw (exception_new ("unexpected language"));
			if (g_mime_param_get_encoding_method (param) != GMIME_PARAM_ENCODING_METHOD_RFC2231)
				throw (exception_new ("unexpected encoding method"));
			text = g_strdup_printf ("attachment; %s", cases[i].input);
			disposition = g_mime_content_disposition_parse (options, text);
			g_free (text);
			if (!disposition || g_strcmp0 (g_mime_content_disposition_get_parameter (disposition, "filename"),
						      cases[i].expected) != 0)
				throw (exception_new ("Content-Disposition value differs"));
			text = g_strdup_printf ("application/octet-stream; %s", cases[i].input);
			content_type = g_mime_content_type_parse (options, text);
			g_free (text);
			if (!content_type || g_strcmp0 (g_mime_content_type_get_parameter (content_type, "filename"),
						       cases[i].expected) != 0)
				throw (exception_new ("Content-Type value differs"));
			testsuite_check_passed ();
		} catch (ex) {
			testsuite_check_failed ("parameter continuation charset[%u]: %s", i, ex->message);
		} finally;
		g_clear_error (&error);
		g_clear_object (&list);
		g_clear_object (&content_type);
		g_clear_object (&disposition);
	}

	testsuite_check ("parameter charset conversion spans output buffers");
	prefix = g_strnfill (255, 'a');
	suffix = g_strnfill (300, 'b');
	text = g_strdup_printf ("filename*=utf-8''%s%%E2%%82%%AC%s", prefix, suffix);
	expected = g_strconcat (prefix, "\342\202\254", suffix, NULL);
	list = g_mime_param_list_parse (options, text);
	try {
		if (!(param = g_mime_param_list_get_parameter (list, "filename")))
			throw (exception_new ("missing filename parameter"));
		value = g_mime_param_get_value (param);
		if (g_strcmp0 (value, expected) != 0)
			throw (exception_new ("long parameter value differs"));
		testsuite_check_passed ();
	} catch (ex) {
		testsuite_check_failed ("%s", ex->message);
	} finally;
	g_object_unref (list);
	g_free (prefix);
	g_free (suffix);
	g_free (text);
	g_free (expected);
	g_mime_parser_options_free (options);
}

static void
test_parameter_quoting (void)
{
	static const struct {
		const char *value;
		const char *normal;
		const char *quoted;
	} cases[] = {
		{ "", "\"\"", "\"\"" },
		{ "flowed", "flowed", "\"flowed\"" },
		{ "file.txt", "file.txt", "\"file.txt\"" },
		{ "two words", "\"two words\"", "\"two words\"" },
		{ "a\\b", "\"a\\\\b\"", "\"a\\\\b\"" },
		{ "a\"b", "\"a\\\"b\"", "\"a\\\"b\"" },
		{ "a;b", "\"a;b\"", "\"a;b\"" },
		{ "a'b", "\"a'b\"", "\"a'b\"" },
		{ "a*b", "\"a*b\"", "\"a*b\"" },
		{ "a=b", "\"a=b\"", "\"a=b\"" }
	};
	GMimeFormatOptions *options = g_mime_format_options_new ();
	GMimeFormatOptions *clone = NULL;
	GMimeContentDisposition *disposition;
	GMimeContentType *type;
	GMimeParamList *list, *parsed = NULL;
	GMimeParam *param;
	GMimePart *part;
	GMimeHeader *header;
	GString *str = g_string_new ("");
	const char *newline, *expected_value, *raw;
	char *expected, *value;
	guint i, flags;

	testsuite_check ("parameter quoting option defaults and cloning");
	try {
		if (g_mime_format_options_get_always_quote_param_values (options) ||
		    g_mime_format_options_get_always_quote_param_values (NULL))
			throw (exception_new ("quoting should default to false"));
		clone = g_mime_format_options_clone (NULL);
		if (g_mime_format_options_get_always_quote_param_values (clone))
			throw (exception_new ("cloned defaults should not force quoting"));
		g_mime_format_options_free (clone);
		clone = NULL;
		g_mime_format_options_set_always_quote_param_values (options, -7);
		g_mime_format_options_set_newline_format (options, GMIME_NEWLINE_FORMAT_DOS);
		clone = g_mime_format_options_clone (options);
		g_mime_format_options_set_always_quote_param_values (options, FALSE);
		if (g_mime_format_options_get_always_quote_param_values (clone) != TRUE ||
		    g_mime_format_options_get_always_quote_param_values (options) ||
		    g_mime_format_options_get_newline_format (clone) != GMIME_NEWLINE_FORMAT_DOS)
			throw (exception_new ("clone did not retain independent quoting options"));
		testsuite_check_passed ();
	} catch (ex) {
		testsuite_check_failed ("%s", ex->message);
	} finally;
	if (clone)
		g_mime_format_options_free (clone);

	for (i = 0; i < G_N_ELEMENTS (cases); i++) {
		for (flags = 0; flags < 32; flags++) {
			testsuite_check ("parameter quoting[%u,%u]", i, flags);
			list = g_mime_param_list_new ();
			g_mime_param_list_set_parameter (list, "filename", cases[i].value);
			param = g_mime_param_list_get_parameter (list, "filename");
			g_mime_param_set_always_quote (param, flags & 1);
			g_mime_format_options_set_always_quote_param_values (options, flags & 2);
			g_mime_format_options_set_param_encoding_method (options, flags & 4 ?
				GMIME_PARAM_ENCODING_METHOD_RFC2047 : GMIME_PARAM_ENCODING_METHOD_RFC2231);
			g_mime_format_options_set_newline_format (options, flags & 8 ?
				GMIME_NEWLINE_FORMAT_DOS : GMIME_NEWLINE_FORMAT_UNIX);
			newline = flags & 16 ? g_mime_format_options_get_newline (options) : "";
			expected_value = flags & 3 ? cases[i].quoted : cases[i].normal;
			expected = g_strdup_printf ("; filename=%s%s", expected_value, newline);
			g_string_truncate (str, 0);
			try {
				g_mime_param_list_encode (list, options, flags & 16, str);
				if (strcmp (str->str, expected) != 0)
					throw (exception_new ("unexpected encoding: %s", str->str));
				parsed = g_mime_param_list_parse (NULL, str->str);
				param = g_mime_param_list_get_parameter (parsed, "filename");
				if (!param || g_strcmp0 (g_mime_param_get_value (param), cases[i].value) != 0)
					throw (exception_new ("quoted value did not round-trip"));
				testsuite_check_passed ();
			} catch (ex) {
				testsuite_check_failed ("%s", ex->message);
			} finally;
			g_clear_object (&parsed);
			g_object_unref (list);
			g_free (expected);
		}
	}

	testsuite_check ("parameter quoting boolean normalization");
	list = g_mime_param_list_new ();
	g_mime_param_list_set_parameter (list, "format", "flowed");
	param = g_mime_param_list_get_parameter (list, "format");
	try {
		if (g_mime_param_get_always_quote (param))
			throw (exception_new ("parameter quoting should default to false"));
		g_mime_param_set_always_quote (param, -7);
		g_mime_param_set_always_quote (param, TRUE);
		g_mime_param_set_always_quote (param, 42);
		if (g_mime_param_get_always_quote (param) != TRUE)
			throw (exception_new ("nonzero values should normalize to true"));
		g_mime_param_set_always_quote (param, FALSE);
		g_mime_param_set_always_quote (param, FALSE);
		if (g_mime_param_get_always_quote (param))
			throw (exception_new ("parameter quoting did not reset"));
		testsuite_check_passed ();
	} catch (ex) {
		testsuite_check_failed ("%s", ex->message);
	} finally;
	g_object_unref (list);

	g_mime_format_options_set_newline_format (options, GMIME_NEWLINE_FORMAT_UNIX);
	g_mime_format_options_set_always_quote_param_values (options, TRUE);
	for (i = 0; i < 2; i++) {
		testsuite_check ("parameter quoting preserves encoded syntax[%u]", i);
		g_mime_format_options_set_param_encoding_method (options, i ?
			GMIME_PARAM_ENCODING_METHOD_RFC2047 : GMIME_PARAM_ENCODING_METHOD_RFC2231);
		list = g_mime_param_list_new ();
		g_mime_param_list_set_parameter (list, "filename", "\303\251");
		param = g_mime_param_list_get_parameter (list, "filename");
		g_mime_param_set_charset (param, "iso-8859-1");
		g_mime_param_set_always_quote (param, TRUE);
		g_string_truncate (str, 0);
		try {
			g_mime_param_list_encode (list, options, FALSE, str);
			expected_value = i ? "; filename=\"=?iso-8859-1?b?6Q==?=\"" :
				"; filename*=iso-8859-1''%E9";
			if (strcmp (str->str, expected_value) != 0)
				throw (exception_new ("unexpected encoded value: %s", str->str));
			testsuite_check_passed ();
		} catch (ex) {
			testsuite_check_failed ("%s", ex->message);
		} finally;
		g_object_unref (list);
	}

	testsuite_check ("parameter quoting header synchronization");
	part = g_mime_part_new ();
	type = g_mime_entity_get_content_type (GMIME_ENTITY (part));
	g_mime_content_type_set_parameter (type, "format", "flowed");
	g_mime_entity_set_disposition (GMIME_ENTITY (part), "attachment");
	disposition = g_mime_entity_get_content_disposition (GMIME_ENTITY (part));
	g_mime_content_disposition_set_parameter (disposition, "filename", "file.txt");
	try {
		list = g_mime_content_type_get_parameters (type);
		param = g_mime_param_list_get_parameter (list, "format");
		header = g_mime_header_list_get_header (g_mime_entity_get_header_list (GMIME_ENTITY (part)), "Content-Type");
		testsuite_header_raw_text (header);
		g_mime_param_set_always_quote (param, TRUE);
		raw = testsuite_header_raw_text (header);
		if (g_strcmp0 (raw, " application/octet-stream; format=\"flowed\"\n") != 0)
			throw (exception_new ("Content-Type did not update: %s", raw ? raw : "(null)"));
		g_mime_param_set_always_quote (param, FALSE);
		raw = testsuite_header_raw_text (header);
		if (g_strcmp0 (raw, " application/octet-stream; format=flowed\n") != 0)
			throw (exception_new ("Content-Type quoting did not reset"));
		list = g_mime_content_disposition_get_parameters (disposition);
		param = g_mime_param_list_get_parameter (list, "filename");
		header = g_mime_header_list_get_header (g_mime_entity_get_header_list (GMIME_ENTITY (part)), "Content-Disposition");
		testsuite_header_raw_text (header);
		g_mime_param_set_always_quote (param, TRUE);
		raw = testsuite_header_raw_text (header);
		if (g_strcmp0 (raw, " attachment; filename=\"file.txt\"\n") != 0)
			throw (exception_new ("Content-Disposition did not update"));
		g_mime_param_set_always_quote (param, FALSE);
		raw = testsuite_header_raw_text (header);
		if (g_strcmp0 (raw, " attachment; filename=file.txt\n") != 0)
			throw (exception_new ("Content-Disposition quoting did not reset"));
		testsuite_check_passed ();
	} catch (ex) {
		testsuite_check_failed ("%s", ex->message);
	} finally;
	g_object_unref (part);

	g_mime_format_options_set_param_encoding_method (options, GMIME_PARAM_ENCODING_METHOD_RFC2231);
	for (i = 32; i <= 36; i++) {
		for (flags = 0; flags < 3; flags++) {
			testsuite_check ("quoted parameter fold boundary[%u,%u]", i, flags);
			value = g_strnfill (i + (flags == 2), flags == 1 ? ' ' : 'a');
			if (flags == 2)
				value[i] = '\\';
			list = g_mime_param_list_new ();
			g_mime_param_list_set_parameter (list, "filename", value);
			g_string_assign (str, "Content-Disposition: attachment");
			expected = g_strdup_printf ("Content-Disposition: attachment;%sfilename=\"%.*s%s\"\n",
						    i + (flags == 2 ? 2 : 0) >= 34 ? "\n\t" : " ",
						    (int) i, value, flags == 2 ? "\\\\" : "");
			try {
				g_mime_param_list_encode (list, options, TRUE, str);
				if (strcmp (str->str, expected) != 0)
					throw (exception_new ("unexpected fold boundary: %s", str->str));
				testsuite_check_passed ();
			} catch (ex) {
				testsuite_check_failed ("%s", ex->message);
			} finally;
			g_object_unref (list);
			g_free (expected);
			g_free (value);
		}
	}

	g_mime_format_options_free (options);
	g_string_free (str, TRUE);
}

static void
test_parameter_output_charsets (void)
{
	static const struct {
		const char *charset;
		const char *value;
		const char *encoded;
	} cases[] = {
		{ NULL, "\303\251", "iso-8859-1''%E9" },
		{ "utf-8", "\303\251", "iso-8859-1''%E9" },
		{ "windows-1251", "\303\251", "iso-8859-1''%E9" },
		{ "utf-16le", "\303\251", "iso-8859-1''%E9" },
		{ "x-gmime-unsupported", "\303\251", "iso-8859-1''%E9" },
		{ NULL, "\302\200", "iso-8859-1''%80" },
		{ NULL, "\320\226", "utf-8''%D0%96" },
		{ NULL, "\316\251", "utf-8''%CE%A9" },
		{ NULL, "\344\270\255", "utf-8''%E4%B8%AD" },
		{ NULL, "\360\237\230\200", "utf-8''%F0%9F%98%80" },
		{ "windows-1251", "\320\226", "windows-1251''%C6" },
		{ "iso-8859-5", "\320\226", "iso-8859-5''%B6" },
		{ "us-ascii", "\320\226", "utf-8''%D0%96" },
		{ "iso-8859-1", "\320\226", "utf-8''%D0%96" },
		{ "windows-1251", "\344\270\255", "utf-8''%E4%B8%AD" },
		{ "windows-1251", "\320\226\344\270\255", "utf-8''%D0%96%E4%B8%AD" },
		{ "x-gmime-unsupported", "\344\270\255", "utf-8''%E4%B8%AD" },
		{ "x-unknown", "\344\270\255", "utf-8''%E4%B8%AD" },
		{ "us-ascii//TRANSLIT", "\320\226", "utf-8''%D0%96" },
		{ "utf-16le", "A\344\270\255", "utf-16le''A%00-N" },
		{ "utf-16be", "A\344\270\255", "utf-16be''%00AN-" },
		{ "utf-32le", "A\344\270\255", "utf-32le''A%00%00%00-N%00%00" },
		{ "utf-32be", "A\344\270\255", "utf-32be''%00%00%00A%00%00N-" }
	};
	GMimeFormatOptions *options = g_mime_format_options_new ();
	GMimeParamList *list, *parsed = NULL;
	GMimeParam *param;
	GString *str = g_string_new ("");
	const char *charset;
	char *expected, *value;
	guint i;

	for (i = 0; i < G_N_ELEMENTS (cases); i++) {
		testsuite_check ("parameter output charset[%u]", i);
		list = g_mime_param_list_new ();
		g_mime_param_list_set_parameter (list, "filename", cases[i].value);
		param = g_mime_param_list_get_parameter (list, "filename");
		g_mime_param_set_charset (param, cases[i].charset);
		expected = g_strdup_printf ("; filename*=%s", cases[i].encoded);
		g_string_truncate (str, 0);
		try {
			g_mime_param_list_encode (list, options, FALSE, str);
			if (strcmp (str->str, expected) != 0)
				throw (exception_new ("unexpected encoding: %s", str->str));
			if (g_strcmp0 (g_mime_param_get_charset (param), cases[i].charset) != 0)
				throw (exception_new ("serialization changed the preferred charset"));
			parsed = g_mime_param_list_parse (NULL, str->str);
			if (!(param = g_mime_param_list_get_parameter (parsed, "filename")) ||
			    g_strcmp0 (g_mime_param_get_value (param), cases[i].value) != 0)
				throw (exception_new ("output did not round-trip"));
			testsuite_check_passed ();
		} catch (ex) {
			testsuite_check_failed ("%s", ex->message);
		} finally;
		g_clear_object (&parsed);
		g_object_unref (list);
		g_free (expected);
	}

	for (i = 0; i < 4; i++) {
		testsuite_check ("large parameter output charset[%u]", i);
		charset = i == 0 ? "utf-32le" : i == 1 ? "iso-2022-jp" : "utf-16le";
		value = g_strnfill (300, 'a');
		expected = g_strconcat (i < 2 ? "\344\270\255" : i == 2 ? "\177" : "", value, NULL);
		g_free (value);
		list = g_mime_param_list_new ();
		g_mime_param_list_set_parameter (list, "filename", expected);
		param = g_mime_param_list_get_parameter (list, "filename");
		g_mime_param_set_charset (param, charset);
		g_mime_param_set_lang (param, "en");
		g_string_truncate (str, 0);
		try {
			g_mime_param_list_encode (list, options, TRUE, str);
			if (g_strcmp0 (g_mime_param_get_charset (param), charset) != 0 ||
			    g_strcmp0 (g_mime_param_get_lang (param), "en") != 0)
				throw (exception_new ("serialization changed parameter metadata"));
			if (i < 3) {
				value = g_strdup_printf ("filename*0*=%s'en'", i == 2 ? "iso-8859-1" : charset);
				if (!strstr (str->str, value)) {
					g_free (value);
					throw (exception_new ("unexpected large-value charset: %s", str->str));
				}
				g_free (value);
			} else if (!strstr (str->str, "filename*0*=us-ascii'en'")) {
				throw (exception_new ("ASCII value used the preferred wide charset"));
			}
			parsed = g_mime_param_list_parse (NULL, str->str);
			if (!(param = g_mime_param_list_get_parameter (parsed, "filename")) ||
			    g_strcmp0 (g_mime_param_get_value (param), expected) != 0)
				throw (exception_new ("large converted value did not round-trip"));
			testsuite_check_passed ();
		} catch (ex) {
			testsuite_check_failed ("%s", ex->message);
		} finally;
		g_clear_object (&parsed);
		g_object_unref (list);
		g_free (expected);
	}

	g_mime_format_options_free (options);
	g_string_free (str, TRUE);
}

static void
test_parameter_encoding_selection (void)
{
	static const char *markers[] = {
		"=?", "=?utf-8?q?hello?=", "before =?us-ascii?b?eA==?= after",
		" =?broken?= ", "a=?b", "=?""?=", "?=", "=", "?"
	};
	GMimeFormatOptions *options = g_mime_format_options_new ();
	GMimeParamList *list, *parsed = NULL;
	GMimeParam *param;
	GString *str = g_string_new ("");
	const char *charset, *name;
	char *value, *part;
	guint i, mode, flags;

	testsuite_check ("invalid parameter encoding methods preserve the current method");
	list = g_mime_param_list_new ();
	g_mime_param_list_set_parameter (list, "filename", "=?");
	param = g_mime_param_list_get_parameter (list, "filename");
	g_mime_param_set_encoding_method (param, GMIME_PARAM_ENCODING_METHOD_RFC2047);
	try {
		g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*method == GMIME_PARAM_ENCODING_METHOD_DEFAULT*");
		g_mime_param_set_encoding_method (param, (GMimeParamEncodingMethod) -1);
		g_test_assert_expected_messages ();
		g_test_expect_message ("gmime", G_LOG_LEVEL_CRITICAL, "*method == GMIME_PARAM_ENCODING_METHOD_DEFAULT*");
		g_mime_param_set_encoding_method (param, (GMimeParamEncodingMethod) 3);
		g_test_assert_expected_messages ();
		if (g_mime_param_get_encoding_method (param) != GMIME_PARAM_ENCODING_METHOD_RFC2047)
			throw (exception_new ("invalid method changed the parameter"));
		testsuite_check_passed ();
	} catch (ex) {
		testsuite_check_failed ("%s", ex->message);
	} finally;
	g_object_unref (list);

	for (mode = 0; mode < 2; mode++) {
		g_mime_format_options_set_param_encoding_method (options, mode ?
			GMIME_PARAM_ENCODING_METHOD_RFC2047 : GMIME_PARAM_ENCODING_METHOD_RFC2231);
		for (i = 1; i <= 127; i++) {
			if (i >= 32 && i < 127)
				continue;
			testsuite_check ("parameter control character[%u,%u]", mode, i);
			value = g_strdup_printf (" \tleft%c right \t", (char) i);
			list = g_mime_param_list_new ();
			g_mime_param_list_set_parameter (list, "filename", value);
			g_string_truncate (str, 0);
			try {
				g_mime_param_list_encode (list, options, i & 1, str);
				if (!strstr (str->str, mode ? "filename=\"=?" : "filename*="))
					throw (exception_new ("control character was not encoded"));
				if (!(i & 1) && (strchr (str->str, '\n') || strchr (str->str, '\r')))
					throw (exception_new ("non-folding output contains line breaks"));
				parsed = g_mime_param_list_parse (NULL, str->str);
				if (!(param = g_mime_param_list_get_parameter (parsed, "filename")) ||
				    g_strcmp0 (g_mime_param_get_value (param), value) != 0)
					throw (exception_new ("control character or edge whitespace did not round-trip"));
				testsuite_check_passed ();
			} catch (ex) {
				testsuite_check_failed ("%s", ex->message);
			} finally;
			g_clear_object (&parsed);
			g_object_unref (list);
			g_free (value);
		}

		for (i = 0; i < G_N_ELEMENTS (markers); i++) {
			for (flags = 0; flags < 4; flags++) {
				testsuite_check ("parameter encoded-word protection[%u,%u,%u]", mode, i, flags);
				list = g_mime_param_list_new ();
				g_mime_param_list_set_parameter (list, "filename", markers[i]);
				param = g_mime_param_list_get_parameter (list, "filename");
				g_mime_param_set_always_quote (param, flags & 1);
				g_mime_format_options_set_always_quote_param_values (options, flags & 2);
				g_string_truncate (str, 0);
				try {
					g_mime_param_list_encode (list, options, TRUE, str);
					if (strstr (markers[i], "=?") && !strstr (str->str, mode ? "filename=\"=?" : "filename*="))
						throw (exception_new ("encoded-word lookalike was not protected"));
					parsed = g_mime_param_list_parse (NULL, str->str);
					if (!(param = g_mime_param_list_get_parameter (parsed, "filename")) ||
					    g_strcmp0 (g_mime_param_get_value (param), markers[i]) != 0)
						throw (exception_new ("encoded-word lookalike did not round-trip"));
					testsuite_check_passed ();
				} catch (ex) {
					testsuite_check_failed ("%s", ex->message);
				} finally;
				g_clear_object (&parsed);
				g_object_unref (list);
			}
		}
	}

	for (mode = 0; mode < 2; mode++) {
		g_mime_format_options_set_param_encoding_method (options, mode ?
			GMIME_PARAM_ENCODING_METHOD_RFC2047 : GMIME_PARAM_ENCODING_METHOD_RFC2231);
		for (i = 64; i <= 69; i++) {
			for (flags = 0; flags < 3; flags++) {
				testsuite_check ("parameter encoding width[%u,%u,%u]", mode, i, flags);
				value = g_strnfill (i, flags == 2 ? '\\' : 'a');
				list = g_mime_param_list_new ();
				g_mime_param_list_set_parameter (list, "filename", value);
				param = g_mime_param_list_get_parameter (list, "filename");
				g_mime_param_set_always_quote (param, flags == 1);
				g_mime_format_options_set_always_quote_param_values (options, FALSE);
				g_string_truncate (str, 0);
				try {
					g_mime_param_list_encode (list, options, FALSE, str);
					if (!mode && i == 64 && flags == 2) {
						if (!g_str_has_prefix (str->str, "; filename=\"") ||
						    strlen (str->str) != strlen ("; filename=\"\"") + 2 * i)
							throw (exception_new ("non-folding quoted chunk differs from MimeKit"));
					} else if ((i + 9 + (flags == 2 ? i + 2 : flags == 1 ? 2 : 0) >= 78) !=
					    (strstr (str->str, mode ? "filename=\"=?" : "filename*") != NULL))
						throw (exception_new ("incorrect encoding-width boundary"));
					parsed = g_mime_param_list_parse (NULL, str->str);
					if (!(param = g_mime_param_list_get_parameter (parsed, "filename")) ||
					    g_strcmp0 (g_mime_param_get_value (param), value) != 0)
						throw (exception_new ("encoding-width value did not round-trip"));
					testsuite_check_passed ();
				} catch (ex) {
					testsuite_check_failed ("%s", ex->message);
				} finally;
				g_clear_object (&parsed);
				g_object_unref (list);
				g_free (value);
			}
		}
	}

	g_mime_format_options_set_param_encoding_method (options, GMIME_PARAM_ENCODING_METHOD_RFC2231);
	for (i = 0; i < 9; i++) {
		testsuite_check ("RFC 2047 parameter charset[%u]", i);
		charset = i == 8 ? "x-gmime-unsupported" : i == 7 ? "utf-16le" :
			i < 4 ? (i & 1 ? "utf-32be" : "utf-16le") :
			i == 4 ? "iso-2022-jp" : "windows-1251";
		part = g_strnfill (300, i < 2 ? 'a' : ' ');
		value = g_strconcat (i == 7 ? "\360\237\230\200" : i == 6 ? "\320\226" : "\344\270\255", part, " \t", NULL);
		g_free (part);
		list = g_mime_param_list_new ();
		g_mime_param_list_set_parameter (list, "filename", value);
		param = g_mime_param_list_get_parameter (list, "filename");
		g_mime_param_set_charset (param, charset);
		g_mime_param_set_encoding_method (param, GMIME_PARAM_ENCODING_METHOD_RFC2047);
		g_string_truncate (str, 0);
		try {
			g_mime_param_list_encode (list, options, TRUE, str);
			if (g_strcmp0 (g_mime_param_get_charset (param), charset) != 0)
				throw (exception_new ("serialization changed the preferred charset"));
			if (i == 4 && strstr (str->str, "?q?"))
				throw (exception_new ("ISO-2022-JP encoded-word did not use Base64"));
			parsed = g_mime_param_list_parse (NULL, str->str);
			if (!(param = g_mime_param_list_get_parameter (parsed, "filename")) ||
			    g_strcmp0 (g_mime_param_get_value (param), value) != 0)
				throw (exception_new ("RFC 2047 charset value did not round-trip"));
			testsuite_check_passed ();
		} catch (ex) {
			testsuite_check_failed ("%s", ex->message);
		} finally;
		g_clear_object (&parsed);
		g_object_unref (list);
		g_free (value);
	}

	for (i = 0; i < 2; i++) {
		testsuite_check ("empty value with oversized parameter name[%u]", i);
		name = value = g_strnfill (100, 'a');
		list = g_mime_param_list_new ();
		g_mime_param_list_set_parameter (list, name, "");
		param = g_mime_param_list_get_parameter (list, name);
		g_mime_param_set_encoding_method (param, i ? GMIME_PARAM_ENCODING_METHOD_RFC2047 : GMIME_PARAM_ENCODING_METHOD_RFC2231);
		g_string_truncate (str, 0);
		try {
			g_mime_param_list_encode (list, options, TRUE, str);
			if (!strstr (str->str, "=\"\"") || strstr (str->str, "*="))
				throw (exception_new ("empty value was encoded instead of quoted"));
			testsuite_check_passed ();
		} catch (ex) {
			testsuite_check_failed ("%s", ex->message);
		} finally;
		g_object_unref (list);
		g_free (value);
	}

	for (i = 0; i < 4; i++) {
		testsuite_check ("parameter name contributes to encoding width[%u]", i);
		name = value = g_strnfill (i == 0 ? 1 : i == 1 ? 72 : i == 2 ? 74 : 100, 'a');
		list = g_mime_param_list_new ();
		g_mime_param_list_set_parameter (list, name, "raw");
		g_string_truncate (str, 0);
		try {
			g_mime_param_list_encode (list, options, TRUE, str);
			if (strchr (str->str, '*'))
				throw (exception_new ("short raw value gained unnecessary encoding or continuations"));
			parsed = g_mime_param_list_parse (NULL, str->str);
			if (!(param = g_mime_param_list_get_parameter (parsed, name)) ||
			    g_strcmp0 (g_mime_param_get_value (param), "raw") != 0)
				throw (exception_new ("wide parameter name did not round-trip"));
			testsuite_check_passed ();
		} catch (ex) {
			testsuite_check_failed ("%s", ex->message);
		} finally;
		g_clear_object (&parsed);
		g_object_unref (list);
		g_free (value);
	}

	g_mime_format_options_free (options);
	g_string_free (str, TRUE);
}

static void
test_parameter_continuation_formatting (void)
{
	static const struct {
		const char *charset, *prefix, *text, *suffix, *lang;
		guint repeat, name_length;
	} cases[] = {
		{ NULL, "", "a", "", NULL, 1200, 8 },
		{ NULL, "", "\\\"", "", NULL, 90, 8 },
		{ NULL, "", " ", "", NULL, 150, 8 },
		{ NULL, "", "\303\251", "", NULL, 100, 8 },
		{ NULL, "", "\360\237\230\200", "", NULL, 100, 8 },
		{ "utf-8", "\344\270\255", "a", "\360\237\230\200", NULL, 160, 8 },
		{ "utf-16le", "", "\344\270\255", "", NULL, 120, 8 },
		{ "utf-16be", "", "\344\205\202", "", NULL, 120, 8 },
		{ "utf-32le", "", "\360\237\230\200", "", NULL, 100, 8 },
		{ "iso-2022-jp", "\344\270\255", "a", "\344\270\255", NULL, 160, 8 },
		{ "iso-2022-jp", "", "\346\227\245", "", NULL, 100, 8 },
		{ NULL, "", "a", "", "en", 160, 8 },
		{ NULL, "", "\344\270\255", "", "en", 1, 74 },
		{ NULL, "", "raw", "", NULL, 2, 100 },
		{ "x-gmime-unsupported", "\344\270\255", "a", "", NULL, 160, 8 },
		{ "utf-16", "", "\344\270\255", "", NULL, 120, 8 },
		{ "utf-32", "", "\360\237\230\200", "", NULL, 100, 8 },
		{ "utf-16le", "", "\357\273\277\344\270\255", "", NULL, 100, 8 },
		{ NULL, "", "=?utf-8?q?hello?=", "", NULL, 12, 8 },
		{ NULL, "", "\r\n\t a\\\"", "", NULL, 30, 8 }
	};
	GMimeFormatOptions *options = g_mime_format_options_new ();
	GMimeParamList *list, *parsed = NULL;
	GString *input = g_string_new (NULL), *str = g_string_new (NULL);
	const char *header = "Content-Disposition: attachment", *line, *end;
	GMimeParam *param;
	char *name;
	guint i, mode, j;

	for (i = 0; i < G_N_ELEMENTS (cases); i++) {
		g_string_assign (input, cases[i].prefix);
		for (j = 0; j < cases[i].repeat; j++)
			g_string_append (input, cases[i].text);
		g_string_append (input, cases[i].suffix);
		name = cases[i].name_length == 8 ? g_strdup ("filename") : g_strnfill (cases[i].name_length, 'n');
		list = g_mime_param_list_new ();
		g_mime_param_list_set_parameter (list, name, input->str);
		g_mime_param_list_set_parameter (list, "after", "end");
		param = g_mime_param_list_get_parameter (list, name);
		g_mime_param_set_charset (param, cases[i].charset);
		g_mime_param_set_lang (param, cases[i].lang);

		for (mode = 0; mode < 4; mode++) {
			testsuite_check ("RFC 2231 continuation formatting[%u,%u]", i, mode);
			g_mime_format_options_set_newline_format (options, mode & 2 ?
				GMIME_NEWLINE_FORMAT_DOS : GMIME_NEWLINE_FORMAT_UNIX);
			g_mime_format_options_set_always_quote_param_values (options, mode & 2);
			g_mime_param_set_always_quote (param, mode & 2);
			g_string_assign (str, header);
			try {
				g_mime_param_list_encode (list, options, mode & 1, str);
				if (!(mode & 1) && (strchr (str->str, '\n') || strchr (str->str, '\r')))
					throw (exception_new ("non-folding continuation contains line breaks"));
				if ((mode & 1) && cases[i].name_length == 8) {
					for (line = str->str; *line; line = *end ? end + 1 : end) {
						end = strchr (line, '\n');
						if (!end)
							end = line + strlen (line);
						if (end - line - (end > line && end[-1] == '\r' ? 1 : 0) > 78)
							throw (exception_new ("continuation line exceeds 78 columns"));
						if (*end && (mode & 2) && (end == line || end[-1] != '\r'))
							throw (exception_new ("DOS continuation lacks CRLF"));
					}
				}
				if (i == 0 && (!strstr (str->str, "filename*10=") || strstr (str->str, "*=")))
					throw (exception_new ("long ASCII value was encoded instead of split into raw pieces"));
				if ((i == 1 || i == 2) && !strstr (str->str, "filename*0=\""))
					throw (exception_new ("continuation was not quoted"));
				if (i == 5 && (!strstr (str->str, "filename*1=") || !strstr (str->str, "filename*2*=")))
					throw (exception_new ("mixed value did not use raw and encoded pieces"));
				if (i == 12 && (!strstr (str->str, "*0*=utf-8'en'") || !strstr (str->str, "*1*=%E4%B8%AD")))
					throw (exception_new ("oversized name split a declaration or Unicode character"));
				if (g_strcmp0 (g_mime_param_get_charset (param), cases[i].charset) != 0 ||
				    g_strcmp0 (g_mime_param_get_lang (param), cases[i].lang) != 0)
					throw (exception_new ("serialization changed stored metadata"));
				parsed = g_mime_param_list_parse (NULL, str->str + strlen (header));
				{
					GMimeParam *restored = g_mime_param_list_get_parameter (parsed, name);
					GMimeParam *after = g_mime_param_list_get_parameter (parsed, "after");

					if (!restored || g_strcmp0 (g_mime_param_get_value (restored), input->str) != 0 ||
					    !after || g_strcmp0 (g_mime_param_get_value (after), "end") != 0)
						throw (exception_new ("continuations or following parameter did not round-trip"));
					if (cases[i].lang && g_strcmp0 (g_mime_param_get_lang (restored), cases[i].lang) != 0)
						throw (exception_new ("serialized language did not round-trip"));
				}
				testsuite_check_passed ();
			} catch (ex) {
				testsuite_check_failed ("%s", ex->message);
			} finally;
			g_clear_object (&parsed);
		}
		g_object_unref (list);
		g_free (name);
	}

	g_string_free (input, TRUE);
	g_string_free (str, TRUE);
	g_mime_format_options_free (options);
}

static void
test_parameter_formatting_parity (void)
{
	static const struct {
		const char *glyph, *charset, *expected;
		guint repeat, width;
		GMimeParamEncodingMethod method;
	} cases[] = {
		{ "\303\251", "utf-8",
		  "Content-Type: application/octet-stream;\n"
		  "\tfilename=\"=?iso-8859-1?b?6enp6enp6enp6enp6enp6enp6ek=?=\";\n\tafter=end\n",
		  20, 60, GMIME_PARAM_ENCODING_METHOD_RFC2047 },
		{ "\346\227\245", "utf-8",
		  "Content-Type: application/octet-stream; filename=\"=?utf-8?b?5pel5pel5pel?=\n"
		  "\t=?utf-8?b?5pel5pel5pel5pel5pel5pel5pel?=\"; after=end\n",
		  10, 78, GMIME_PARAM_ENCODING_METHOD_RFC2047 },
		{ "\346\227\245", "utf-8",
		  "Content-Type: application/octet-stream;\n"
		  "\tfilename*0*=utf-8''%E6%97%A5%E6%97%A5%E6%97%A5%E6%97%A5;\n"
		  "\tfilename*1*=%E6%97%A5%E6%97%A5%E6%97%A5%E6%97%A5%E6%97%A5;\n"
		  "\tfilename*2*=%E6%97%A5%E6%97%A5%E6%97%A5; after=end\n",
		  12, 60, GMIME_PARAM_ENCODING_METHOD_RFC2231 },
		{ "\346\227\245.txt", "iso-2022-jp",
		  "Content-Type: application/octet-stream; filename=\"=?iso-2022-jp?b?" "?=\n"
		  "\t=?iso-2022-jp?b?GyRCRnwbKEIudHh0?=\"; after=end\n",
		  1, 78, GMIME_PARAM_ENCODING_METHOD_RFC2047 },
		{ "=?=========", "utf-8",
		  "Content-Type: application/octet-stream;\n"
		  "\tfilename=\"=?us-ascii?q?=3D=3F=3D=3D=3D=3D=3D=3D=3D=3D?=\n"
		  "\t=?us-ascii?q?=3D?=\"; after=end\n",
		  1, 60, GMIME_PARAM_ENCODING_METHOD_RFC2047 }
	};
	GMimeFormatOptions *options = g_mime_format_options_new ();
	GMimeContentType *type, *parsed;
	GMimeParam *param;
	GString *value = g_string_new (NULL);
	char *raw, *wire, *expected, **lines;
	guint i, j, dos;

	for (i = 0; i < G_N_ELEMENTS (cases); i++) {
		g_string_truncate (value, 0);
		for (j = 0; j < cases[i].repeat; j++)
			g_string_append (value, cases[i].glyph);
		type = g_mime_content_type_new ("application", "octet-stream");
		g_mime_content_type_set_parameter (type, "filename", value->str);
		g_mime_content_type_set_parameter (type, "after", "end");
		param = g_mime_param_list_get_parameter (g_mime_content_type_get_parameters (type), "filename");
		g_mime_param_set_charset (param, cases[i].charset);
		g_mime_param_set_encoding_method (param, cases[i].method);
		g_mime_format_options_set_max_line_length (options, cases[i].width);
		for (dos = 0; dos < 2; dos++) {
			g_mime_format_options_set_newline_format (options, dos ?
				GMIME_NEWLINE_FORMAT_DOS : GMIME_NEWLINE_FORMAT_UNIX);
			lines = g_strsplit (cases[i].expected, "\n", -1);
			expected = g_strjoinv (g_mime_format_options_get_newline (options), lines);
			raw = g_mime_content_type_encode (type, options);
			wire = g_strconcat ("Content-Type:", raw, NULL);
			parsed = g_mime_content_type_parse (NULL, raw);
			testsuite_check ("parameter formatting parity[%u,%u]", i, dos);
			try {
				if (strcmp (wire, expected))
					throw (exception_new ("unexpected parameter chunk output: %s", wire));
				if (!parsed || g_strcmp0 (g_mime_content_type_get_parameter (parsed, "filename"), value->str) ||
				    g_strcmp0 (g_mime_content_type_get_parameter (parsed, "after"), "end"))
					throw (exception_new ("formatted parameter did not round-trip"));
				testsuite_check_passed ();
			} catch (ex) {
				testsuite_check_failed ("%s", ex->message);
			} finally;
			g_clear_object (&parsed);
			g_free (wire);
			g_free (raw);
			g_free (expected);
			g_strfreev (lines);
		}
		g_object_unref (type);
	}

	g_string_truncate (value, 0);
	for (i = 0; i < 720; i++)
		g_string_append_c (value, 'a');
	type = g_mime_content_type_new ("application", "octet-stream");
	g_mime_content_type_set_parameter (type, "filename", value->str);
	g_mime_content_type_set_parameter (type, "after", "end");
	g_mime_format_options_set_param_encoding_method (options, GMIME_PARAM_ENCODING_METHOD_RFC2231);
	g_mime_format_options_set_max_line_length (options, 60);
	for (dos = 0; dos < 2; dos++) {
		g_mime_format_options_set_newline_format (options, dos ?
			GMIME_NEWLINE_FORMAT_DOS : GMIME_NEWLINE_FORMAT_UNIX);
		expected = g_strdup_printf ("\tfilename*9=%.*s;%s\tfilename*10=%.*s;%s",
			46, value->str, g_mime_format_options_get_newline (options),
			46, value->str, g_mime_format_options_get_newline (options));
		raw = g_mime_content_type_encode (type, options);
		parsed = g_mime_content_type_parse (NULL, raw);
		testsuite_check ("raw continuation index width parity[%u]", dos);
		try {
			if (!strstr (raw, expected))
				throw (exception_new ("continuation index growth prematurely shortened a raw chunk"));
			if (!parsed || g_strcmp0 (g_mime_content_type_get_parameter (parsed, "filename"), value->str) ||
			    g_strcmp0 (g_mime_content_type_get_parameter (parsed, "after"), "end"))
				throw (exception_new ("raw continuations did not round-trip"));
			testsuite_check_passed ();
		} catch (ex) {
			testsuite_check_failed ("%s", ex->message);
		} finally;
		g_clear_object (&parsed);
		g_free (raw);
		g_free (expected);
	}

	g_mime_content_type_set_parameter (type, "filename", "=?=========");
	g_mime_param_list_remove (g_mime_content_type_get_parameters (type), "after");
	g_mime_format_options_set_param_encoding_method (options, GMIME_PARAM_ENCODING_METHOD_RFC2047);
	for (dos = 0; dos < 2; dos++) {
		g_mime_format_options_set_newline_format (options, dos ?
			GMIME_NEWLINE_FORMAT_DOS : GMIME_NEWLINE_FORMAT_UNIX);
		expected = g_strdup_printf (" application/octet-stream;%s"
			"\tfilename=\"=?us-ascii?q?=3D=3F=3D=3D=3D=3D=3D=3D=3D=3D=3D?=\"%s",
			g_mime_format_options_get_newline (options), g_mime_format_options_get_newline (options));
		raw = g_mime_content_type_encode (type, options);
		testsuite_check ("final encoded parameter exact physical fit[%u]", dos);
		try {
			if (strcmp (raw, expected))
				throw (exception_new ("final encoded parameter unnecessarily reserved a semicolon"));
			testsuite_check_passed ();
		} catch (ex) {
			testsuite_check_failed ("%s", ex->message);
		} finally;
		g_free (raw);
		g_free (expected);
	}
	g_object_unref (type);
	g_string_free (value, TRUE);
	g_mime_format_options_free (options);
}

static void
test_parameter_line_lengths (void)
{
	static const guint widths[] = { 60, 78, 120, 998 };
	static const struct {
		const char *charset, *prefix, *text, *suffix;
		guint repeat;
	} cases[] = {
		{ NULL, "", "a", "", 1200 },
		{ NULL, "", " \\\"", "", 200 },
		{ NULL, "=?", "=", "", 9 },
		{ NULL, "", "\360\237\230\200", "", 200 },
		{ "utf-16le", "", "\344\270\255", "", 200 },
		{ "iso-2022-jp", "\344\270\255", "a", "\344\270\255", 1000 },
		{ "utf-32be", "", "\360\237\230\200", "", 100 },
		{ "utf-32be", "", "\344\270\255", "", 1 }
	};
	GMimeFormatOptions *options = g_mime_format_options_new ();
	GMimeParamList *list, *parsed = NULL;
	GMimeParam *param;
	GString *input = g_string_new (NULL), *str = g_string_new (NULL);
	const char *header = "Content-Disposition: attachment", *line, *end, *word;
	gboolean wide;
	guint i, j, k, method, fold, boundary;
	char *value;

	for (i = 0; i < G_N_ELEMENTS (widths); i++) {
		g_mime_format_options_set_max_line_length (options, widths[i]);
		g_mime_format_options_set_newline_format (options, i & 1 ?
			GMIME_NEWLINE_FORMAT_DOS : GMIME_NEWLINE_FORMAT_UNIX);
		for (method = 0; method < 2; method++) {
			g_mime_format_options_set_param_encoding_method (options, method ?
				GMIME_PARAM_ENCODING_METHOD_RFC2047 : GMIME_PARAM_ENCODING_METHOD_RFC2231);
			testsuite_check ("ordinary parameter folding[%u,%u]", widths[i], method);
			value = g_strnfill (widths[i] == 60 ? 20 : 40, 'a');
			list = g_mime_param_list_new ();
			g_mime_param_list_set_parameter (list, "filename", value);
			g_string_assign (str, header);
			try {
				g_mime_param_list_encode (list, options, TRUE, str);
				if ((strstr (str->str, "\tfilename=") != NULL) !=
				    (strlen (header) + strlen ("; filename=") + strlen (value) >= widths[i]))
					throw (exception_new ("ordinary parameter folding ignored the configured width"));
				testsuite_check_passed ();
			} catch (ex) {
				testsuite_check_failed ("%s", ex->message);
			} finally;
			g_object_unref (list);
			g_free (value);

			for (boundary = 0; boundary < 2; boundary++) {
				testsuite_check ("parameter encoding threshold[%u,%u,%u]", widths[i], method, boundary);
				value = g_strnfill (widths[i] - 10 + boundary, 'a');
				list = g_mime_param_list_new ();
				g_mime_param_list_set_parameter (list, "filename", value);
				g_string_assign (str, header);
				try {
					g_mime_param_list_encode (list, options, TRUE, str);
					if ((strstr (str->str, method ? "filename=\"=?" : "filename*") != NULL) != (boundary != 0))
						throw (exception_new ("encoding selection ignored the configured width"));
					testsuite_check_passed ();
				} catch (ex) {
					testsuite_check_failed ("%s", ex->message);
				} finally;
				g_object_unref (list);
				g_free (value);
			}

			for (j = 0; j < G_N_ELEMENTS (cases); j++) {
				g_string_assign (input, cases[j].prefix);
				for (k = 0; k < cases[j].repeat; k++)
					g_string_append (input, cases[j].text);
				g_string_append (input, cases[j].suffix);
				list = g_mime_param_list_new ();
				g_mime_param_list_set_parameter (list, "filename", input->str);
				g_mime_param_list_set_parameter (list, "after", "end");
				param = g_mime_param_list_get_parameter (list, "filename");
				g_mime_param_set_charset (param, cases[j].charset);
				for (fold = 0; fold < 2; fold++) {
					testsuite_check ("parameter line length[%u,%u,%u,%u]", widths[i], method, j, fold);
					g_string_assign (str, header);
					try {
						g_mime_param_list_encode (list, options, fold, str);
						if (!fold && (strchr (str->str, '\r') || strchr (str->str, '\n')))
							throw (exception_new ("non-folding output contains line breaks"));
						wide = FALSE;
						for (line = str->str; fold && *line; line = *end ? end + 1 : end) {
							end = strchr (line, '\n');
							if (!end)
								end = line + strlen (line);
							k = (guint) (end - line - (end > line && end[-1] == '\r' ? 1 : 0));
							if (k > widths[i])
								throw (exception_new ("physical line exceeds configured width"));
							if (k > 78)
								wide = TRUE;
						}
						if (fold && method && widths[i] > 78 && input->len > 100 && !wide)
							throw (exception_new ("RFC 2047 words did not share wider physical lines"));
						for (word = str->str; method && (word = strstr (word, "=?")); word = end + 2) {
							if (!(end = strstr (word + 2, "?=")) || end + 2 - word > 75)
								throw (exception_new ("encoded-word exceeds 75 characters"));
						}
						parsed = g_mime_param_list_parse (NULL, str->str + strlen (header));
						param = g_mime_param_list_get_parameter (parsed, "filename");
						if (!param || g_strcmp0 (g_mime_param_get_value (param), input->str) != 0)
							throw (exception_new ("parameter did not round-trip"));
						param = g_mime_param_list_get_parameter (parsed, "after");
						if (!param || g_strcmp0 (g_mime_param_get_value (param), "end") != 0)
							throw (exception_new ("following parameter did not round-trip"));
						testsuite_check_passed ();
					} catch (ex) {
						testsuite_check_failed ("%s", ex->message);
					} finally;
					g_clear_object (&parsed);
				}
				g_object_unref (list);
			}
		}
	}

	g_string_free (input, TRUE);
	g_string_free (str, TRUE);
	g_mime_format_options_free (options);
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
	test_address_name_normalization ();
	test_address_name_sync ();
	testsuite_end ();
	
	testsuite_start ("header formatting");
	test_header_formatting ();
	test_domainless_mailbox_formatting ();
	test_unnamed_group_formatting ();
	test_address_line_lengths ();
	test_text_header_line_lengths ();
	test_header_field_lengths ();
	test_header_formatting_parity ();
	test_header_serialization_parity ();
	test_address_header_whitespace ();
	test_literal_encoded_word_headers ();
	test_structured_header_line_lengths ();
	test_message_id_formatting ();
	test_notification_options_formatting ();
	testsuite_end ();

	testsuite_start ("parameter lists");
	test_parameter_lists ();
	test_parameter_percent_encoding ();
	test_parameter_continuation_charsets ();
	test_parameter_quoting ();
	test_parameter_output_charsets ();
	test_parameter_encoding_selection ();
	test_parameter_continuation_formatting ();
	test_parameter_formatting_parity ();
	test_parameter_line_lengths ();
	testsuite_end ();
	
	g_mime_shutdown ();
	
	return testsuite_exit ();
}
