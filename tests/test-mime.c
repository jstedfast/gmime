/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@helixcode.com>
 *
 *  Copyright 2000 Helix Code, Inc. (www.helixcode.com)
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

#include <stdio.h>
#include <string.h>

#include <gmime/gmime.h>

#define ENABLE_ZENTIMER
#include "zentimer.h"

static iconv_t cd = (iconv_t) -1;

void
test_parser (char *data)
{
	GMimeParser *parser;
	GMimeMessage *message;
	GMimeStream *stream;
	char *text;
	
	fprintf (stdout, "\nTesting MIME parser...\n\n");
	
	stream = g_mime_stream_mem_new_with_buffer (data, strlen (data));
	parser = g_mime_parser_new ();
	g_mime_parser_init_with_stream (parser, stream);
	message = g_mime_parser_construct_message (parser);
	g_object_unref (parser);
	g_mime_stream_unref (stream);
	
	fprintf (stdout, "Test of GMimeHeader:\nTo: %s\n\n",
		 g_mime_message_get_header (message, "To"));
	text = g_mime_message_to_string (message);
	fprintf (stdout, "Result should match previous MIME message dump\n\n%s\n", text);
	g_free (text);
	g_mime_object_unref (GMIME_OBJECT (message));
}

void
test_multipart (void)
{
	GMimeMessage *message;
	GMimeContentType *mime_type;
	GMimeMultipart *multipart;
	GMimePart *text_part, *html_part;
	char *body, *text;
	gboolean is_html;
	
	multipart = g_mime_multipart_new_with_subtype ("alternative");
	/*g_mime_multipart_set_boundary (multipart, "spruce1234567890ABCDEFGHIJK");*/
	
	text_part = g_mime_part_new ();
	mime_type = g_mime_content_type_new ("text", "plain");
	g_mime_part_set_content_type (text_part, mime_type);
	g_mime_part_set_content_id (text_part, "1");
	g_mime_part_set_content_header (text_part, "Content-Test", "test-mime");
	g_mime_part_set_content_description (text_part, "this is a text part");
	g_mime_part_set_content (text_part, "This is the body of my message.\n",
				 strlen ("This is the body of my message.\n"));
	
	g_mime_multipart_add_part (multipart, GMIME_OBJECT (text_part));
	
	html_part = g_mime_part_new ();
	mime_type = g_mime_content_type_new ("text", "html");
	g_mime_part_set_content_type (html_part, mime_type);
	g_mime_part_set_filename (html_part, "this is a really really long filename that should force gmime to rfc2184 encode it - yay!.html");
	g_mime_part_set_content_description (html_part, "this is an html part and stuff");
	g_mime_part_set_content (html_part, "<html>\n\t<pre>This is the body of my message.</pre>\n</html>",
				 strlen ("<html>\n\t<pre>This is the body of my message.</pre>\n</html>"));
	g_mime_part_set_encoding (html_part, GMIME_PART_ENCODING_BASE64);
	g_mime_part_set_content_id (html_part, "2");
	
	g_mime_multipart_add_part (multipart, GMIME_OBJECT (html_part));
	
	message = g_mime_message_new (TRUE);
	g_mime_message_set_sender (message, "\"Jeffrey Stedfast\" <fejj@helixcode.com>");
	g_mime_message_set_reply_to (message, "fejj@helixcode.com");
	g_mime_message_add_recipient (message, GMIME_RECIPIENT_TYPE_TO,
				    "Federico Mena-Quintero", "federico@helixcode.com");
	g_mime_message_set_subject (message, "This is a test message");
	g_mime_message_set_header (message, "X-Mailer", "main.c");
	g_mime_message_set_mime_part (message, GMIME_OBJECT (multipart));
	
	text = g_mime_message_to_string (message);
	
	fprintf (stdout, "%s\n", text ? text : "(null)");
	
	/* get the body in text/plain */
	body = g_mime_message_get_body (message, TRUE, &is_html);
	fprintf (stdout, "Trying to get message body in plain text format:\n%s\n\n", body ? body : "(null)");
	g_free (body);
	
	/* get the body in text/html */
	body = g_mime_message_get_body (message, FALSE, &is_html);
	fprintf (stdout, "Trying to get message body in html format:\n%s\n\n", body ? body : "(null)");
	g_free (body);
	if (is_html)
		fprintf (stdout, "yep...got it in html format\n");
	
	g_mime_object_unref (GMIME_OBJECT (message));
	
	test_parser (text);
	
	g_free (text);
}

void
test_onepart (void)
{
	GMimeMessage *message;
	GMimePart *mime_part;
	char *body, *text;
	gboolean is_html;
	
	mime_part = g_mime_part_new_with_type ("text", "plain");
	
	g_mime_part_set_content (mime_part, "This is the body of my message.\n",
				 strlen ("This is the body of my message.\n"));
	
	message = g_mime_message_new (TRUE);
	g_mime_message_set_sender (message, "\"Jeffrey Stedfast\" <fejj@helixcode.com>");
	g_mime_message_set_reply_to (message, "fejj@helixcode.com");
	g_mime_message_add_recipient (message, GMIME_RECIPIENT_TYPE_TO,
				    "Federico Mena-Quintero", "federico@helixcode.com");
	g_mime_message_set_subject (message, "This is a test message");
	g_mime_message_set_header (message, "X-Mailer", "main.c");
	
	g_mime_message_set_mime_part (message, GMIME_OBJECT (mime_part));
	g_mime_object_unref (GMIME_OBJECT (mime_part));
	
	text = g_mime_message_to_string (message);
	
	fprintf (stdout, "%s\n", text ? text : "(null)");
	
	body = g_mime_message_get_body (message, TRUE, &is_html);
	fprintf (stdout, "Trying to get message body:\n%s\n\n", body ? body : "(null)");
	g_free (body);
	
	g_mime_object_unref (GMIME_OBJECT (message));
	
	test_parser (text);
	
	g_free (text);
}

static char *string = "I have no idea what to test here so I'll just test a "
"bunch of crap like this and see what it encodes it as.\r\n"
"I'm thinking this line should be really really really long "
"so as to test the line wrapping at 76 chars, dontcha think? "
"I most certainly do. Lets get some weird chars in here too. "
"Lets try: = ååååaa\nOkay, now on a side note...uhm, ?= =? and stuff.\n"
"danw wanted me to try this next line...so lets give it a shot:\n"
"foo   \n"
"Lets also try some tabs in here like this:\t\tis that 2 tabs? I hope so.";

void
test_encodings (void)
{
	int pos, state = -1, save = 0;
	char *enc, *dec;
	
	enc = "=?iso-8859-1?Q?Copy_of_Rapport_fra_Norges_R=E5fisklag.doc?=";
	fprintf (stderr, "encoded: %s\n", enc);
	dec = g_mime_utils_8bit_header_decode (enc);
	fprintf (stderr, "decoded: %s\n", dec);
	g_free (dec);
	
	enc = "=?iso-8859-1?Q?Copy_of_Rapport_fra_Norges_R=E5fisklagdoc?=";
	fprintf (stderr, "encoded: %s\n", enc);
	dec = g_mime_utils_8bit_header_decode (enc);
	fprintf (stderr, "decoded: %s\n", dec);
	g_free (dec);
	
	enc = "=?iso-8859-1?B?dGVzdOb45S50eHQ=?=";
	fprintf (stderr, "encoded: %s\n", enc);
	dec = g_mime_utils_8bit_header_decode (enc);
	fprintf (stderr, "decoded: %s\n", dec);
	g_free (dec);
	
	enc = "Re: !!! =?windows-1250?Q?Nab=EDz=EDm_scanov=E1n=ED_negativ=F9?= "
		"=?windows-1250?Q?=2C_p=F8edloh_do_A4=2C_=E8/b_lasertov=FD_ti?= "
		"=?windows-1250?Q?sk_a_=E8/b_inkoutov=FD_tisk_do_A2!!!?=";
	fprintf (stderr, "encoded: %s\n", enc);
	dec = g_mime_utils_8bit_header_decode (enc);
	fprintf (stderr, "decoded: %s\n", dec);
	g_free (dec);
	
	enc = "OT - ich =?ISO-8859-1?Q?wei=DF?=, trotzdem";
	fprintf (stderr, "encoded: %s\n", enc);
	dec = g_mime_utils_8bit_header_decode (enc);
	fprintf (stderr, "decoded: %s\n", dec);
	g_free (dec);
	
	enc = "OT - ich =?iso-8859-1?b?d2Vp3yw=?= trotzdem";
	fprintf (stderr, "encoded: %s\n", enc);
	dec = g_mime_utils_8bit_header_decode (enc);
	fprintf (stderr, "decoded: %s\n", dec);
	g_free (dec);
	
	enc = "OT - ich =?ISO-8859-1?Q?wei=DF,?= trotzdem";
	fprintf (stderr, "encoded: %s\n", enc);
	dec = g_mime_utils_8bit_header_decode (enc);
	fprintf (stderr, "decoded: %s\n", dec);
	g_free (dec);
	
	dec = g_mime_iconv_strdup (cd, "OT - ich weiß, trotzdem");
	fprintf (stderr, "pre-encoded: %s\n", dec);
	enc = g_mime_utils_8bit_header_encode (dec);
	g_free (dec);
	fprintf (stderr, "encoded: %s\n", enc);
	dec = g_mime_utils_8bit_header_decode (enc);
	fprintf (stderr, "decoded: %s\n", dec);
	g_free (enc);
	g_free (dec);
	
	enc = g_strdup ("=?iso-8859-1?Q?=DE?=:\t=?iso-8859-1?Q?=AD=BE=EF?= spells 0xdeadbeef");
	fprintf (stderr, "incorrect: %s\n", enc);
	dec = g_mime_utils_8bit_header_decode (enc);
	fprintf (stderr, "decoded: %s\n", dec);
	g_free (enc);
	enc = g_mime_utils_8bit_header_encode (dec);
	g_free (dec);
	fprintf (stderr, "correct: %s\n", enc);
	dec = g_mime_utils_8bit_header_decode (enc);
	fprintf (stderr, "decoded: %s\n", dec);
	g_free (enc);
	g_free (dec);
	
	enc = g_strdup ("=?iso-8859-1?q?blablah?=");
	fprintf (stderr, "encoded: %s\n", enc);
	dec = g_mime_utils_8bit_header_decode (enc);
	fprintf (stderr, "decoded: %s\n", dec);
	g_free (enc);
	g_free (dec);
	
	enc = g_strdup ("=?iso-8859-1?Q?blablah?=");
	fprintf (stderr, "encoded: %s\n", enc);
	dec = g_mime_utils_8bit_header_decode (enc);
	fprintf (stderr, "decoded: %s\n", dec);
	g_free (enc);
	g_free (dec);
	
	dec = g_mime_iconv_strdup (cd, "Kristoffer Brånemyr");
	fprintf (stderr, "pre-encoded: %s\n", dec);
	enc = g_mime_utils_8bit_header_encode (dec);
	g_free (dec);
	fprintf (stderr, "encoded: %s\n", enc);
	dec = g_mime_utils_8bit_header_decode (enc);
	fprintf (stderr, "decoded: %s\n", dec);
	g_free (enc);
	g_free (dec);
	
	dec = g_mime_iconv_strdup (cd, "åaåååaåå aaaa ååååaa");
	enc = g_mime_utils_8bit_header_encode (dec);
	fprintf (stderr, "encoded: %s\n", enc);
	g_free (dec);
	dec = g_mime_utils_8bit_header_decode (enc);
	fprintf (stderr, "decoded: %s\n", dec);
	g_free (enc);
	g_free (dec);
	
	dec = g_mime_iconv_strdup (cd, "åaåååaåå aaaa ååååaa");
	enc = g_mime_utils_8bit_header_encode_phrase (dec);
	fprintf (stderr, "encoded: %s\n", enc);
	g_free (dec);
	dec = g_mime_utils_8bit_header_decode (enc);
	fprintf (stderr, "decoded: %s\n", dec);
	g_free (enc);
	g_free (dec);
	
	fprintf (stderr, "test that white space between 8bit words is preserved\n");
	dec = g_mime_iconv_strdup (cd, "åaåååaåå  \t  ååååaa");
	enc = g_mime_utils_8bit_header_encode (dec);
	fprintf (stderr, "encoded: %s\n", enc);
	g_free (dec);
	dec = g_mime_utils_8bit_header_decode (enc);
	fprintf (stderr, "decoded: %s\n", dec);
	g_free (enc);
	g_free (dec);
	
	enc = g_mime_utils_quote_string ("this is an \"embedded\" quoted string.");
	fprintf (stderr, "quoted: %s\n", enc);
	g_mime_utils_unquote_string (enc);
	fprintf (stderr, "unquoted: %s\n", enc);
	g_free (enc);
	
	enc = g_mime_utils_quote_string ("Jeffrey \"The Fejjinator\" Stedfast");
	fprintf (stderr, "quoted: %s\n", enc);
	g_mime_utils_unquote_string (enc);
	fprintf (stderr, "unquoted: %s\n", enc);
	g_free (enc);
	
	enc = g_malloc (strlen (string) * 3);
	pos = g_mime_utils_quoted_encode_close (string, strlen (string), enc, &state, &save);
	enc[pos] = '\0';
	
	fprintf (stderr, "\nencoded:\n-------\n%s\n-------\n", enc);
	
	g_free (enc);
}

static char *addresses[] = {
	"fejj@helixcode.com",
	"Jeffrey Stedfast <fejj@helixcode.com>",
	"Jeffrey \"fejj\" Stedfast <fejj@helixcode.com>",
	"\"Stedfast, Jeffrey\" <fejj@helixcode.com>",
	"fejj@helixcode.com (Jeffrey Stedfast)",
	"Jeff <fejj(recursive (comment) block)@helixcode.(and a comment here)com>",
	"=?iso-8859-1?q?Kristoffer_Br=E5nemyr?= <ztion@swipenet.se>",
	"fpons@mandrakesoft.com (=?iso-8859-1?q?Fran=E7ois?= Pons)",
	"GNOME Hackers: miguel@gnome.org (Miguel de Icaza), Havoc Pennington <hp@redhat.com>;, fejj@helixcode.com",
	"Local recipients: phil, joe, alex, bob",
	"@develop:sblab!att!thumper.bellcore.com!nsb",
	"\":sysmail\"@  Some-Group. Some-Org,\n Muhammed.(I am  the greatest) Ali @(the)Vegas.WBA",
	"Charles S. Kerr <charles@foo.com>", /* the '.' in the name part is illegal */
	"Charles \"Likes, to, put, commas, in, quoted, strings\" Kerr <charles@foo.com>",
	"Charles Kerr, Pan Programmer <charles@superpimp.org>", /* bad placement of a comma ;-) */
	"Charles Kerr <charles@[127.0.0.1]>",
	"Charles <charles@[127..0.1]>",  /* where'd it go? Hollywood said, "where did *who* go!?" */
	"Charles Kerr,, likes illegal commas <charles@superpimp.org>", /* ouch this is bad... */
	"<charles@>",
	"<charles@broken.host.com.> (Charles Kerr)",
	"fpons@mandrakesoft.com (=?iso-8859-1?q?Fran=E7ois?= Pons likes _'s and \t's too)",
	"Tõivo Leedjärv <leedjarv@interest.ee>",
	"fbosi@mokabyte.it;, rspazzoli@mokabyte.it",
	"\"Miles (Star Trekkin) O'Brian\" <mobrian@starfleet.org>",
	"Patrick Lamaizière <email@domain>",
	NULL
};

static void
dump_addrlist (InternetAddressList *addrlist, int i, gboolean group, gboolean destroy)
{
	InternetAddressList *addr;
	InternetAddress *ia;
	char *str;
	
	addr = addrlist;
	while (addr) {
		ia = addr->address;
		addr = addr->next;
		
		if (i != -1) {
			str = g_mime_iconv_strdup (cd, addresses[i]);
			fprintf (stderr, "Original: %s\n", str);
			g_free (str);
		}
		
		if (ia->type == INTERNET_ADDRESS_GROUP) {
			fprintf (stderr, "Address is a group:\n");
			fprintf (stderr, "Name: %s\n", ia->name ? ia->name : "");
			dump_addrlist (ia->value.members, -1, TRUE, FALSE);
			fprintf (stderr, "End of group.\n");
		} else if (ia->type == INTERNET_ADDRESS_NAME) {
			fprintf (stderr, "%sName: %s\n", group ? "\t" : "",
				 ia->name ? ia->name : "");
			fprintf (stderr, "%sEMail: %s\n", group ? "\t" : "",
				 ia->value.addr ? ia->value.addr : "");
		}
		
		str = internet_address_to_string (ia, FALSE);
		fprintf (stderr, "%sRewritten (display): %s\n", group ? "\t" : "",
			 str ? str : "");
		g_free (str);
		
		str = internet_address_to_string (ia, TRUE);
		fprintf (stderr, "%sRewritten (encoded): %s\n\n", group ? "\t" : "",
			 str ? str : "");
		g_free (str);
	}
}

void
test_addresses (void)
{
	int i;
	
	for (i = 0; addresses[i]; i++) {
		InternetAddressList *addrlist;
		
		addrlist = internet_address_parse_string (addresses[i]);
		if (!addrlist) {
			fprintf (stderr, "failed to parse '%s'.\n", addresses[i]);
			continue;
		}
		
		dump_addrlist (addrlist, i, FALSE, TRUE);
		
		internet_address_list_destroy (addrlist);
	}
}

/*#include "date-strings.h"*/

void
test_date (void)
{
	int offset = 0;
	char *in, *out;
	time_t date;
	
	in = "Mon, 17 Jan 1994 11:14:55 -0500";
	fprintf (stderr, "date  in: [%s]\n", in);
	date = g_mime_utils_header_decode_date (in, &offset);
	out = g_mime_utils_header_format_date (date, offset);
	fprintf (stderr, "date out: [%s]\n", out);
	g_free (out);
	
	in = "Mon, 17 Jan 01 11:14:55 -0500";
	fprintf (stderr, "date  in: [%s]\n", in);
	date = g_mime_utils_header_decode_date (in, &offset);
	out = g_mime_utils_header_format_date (date, offset);
	fprintf (stderr, "date out: [%s]\n", out);
	g_free (out);

#if 0
	{
		char *string;
		int i;
		
		ZenTimerStart ();
		for (i = 0; i < num_date_strings; i++) {
			in = date_strings[i];
			printf ("date  in: [%s]\n", in);
			date = g_mime_utils_header_decode_date (in, &offset);
			out = g_mime_utils_header_format_date (date, offset);
			printf ("date out: [%s]\n", out);
			g_free (out);
		}
		ZenTimerStop ();
		
		string = g_strdup_printf ("date parser (%d dates parsed)", num_date_strings);
		ZenTimerReport (string);
		g_free (string);
	}
#endif
}


int main (int argc, char *argv[])
{
	g_mime_init (0);
	
	cd = g_mime_iconv_open ("UTF-8", "iso-8859-1");
	
	test_date ();
	
	test_onepart ();
	
	test_multipart ();
	
	test_encodings ();
	
	test_addresses ();
	
	g_mime_iconv_close (cd);
	
	return 0;
}
