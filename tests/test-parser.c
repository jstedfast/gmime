#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <glib.h>
#include <time.h>

#include "gmime.h"

#define ENABLE_ZENTIMER
#include "zentimer.h"

#define TEST_PRESERVE_HEADERS
#define TEST_GET_BODY
#define PRINT_MIME_STRUCT
#define VALIDATE_PGP_MIME_SIGNATURES

#ifdef VALIDATE_PGP_MIME_SIGNATURES
#include "gmime-pgp-mime.h"

/* we have to disable getting the body cuz it kills the raw data */
#ifdef TEST_GET_BODY
#undef TEST_GET_BODY
#endif

static char *pgp_path = "/usr/bin/gpg";
static GMimePgpType pgp_type = GMIME_PGP_TYPE_GPG;
static char *userid = "pgp-mime@xtorshun.org";
static char *passphrase = "PGP/MIME is rfc2015, now go and read it.";

static char *
get_passwd (const char *prompt, gpointer data)
{	return g_strdup (passphrase);
}
#endif

void
print_depth (int depth)
{
	int i;
	
	for (i = 0; i < depth; i++)
		fprintf (stdout, "   ");
}

void
print_mime_struct (GMimePart *part, int depth)
{
	const GMimeContentType *type;
	
	print_depth (depth);
	type = g_mime_part_get_content_type (part);
	
	fprintf (stdout, "Content-Type: %s/%s", type->type, type->subtype);
	
#ifdef VALIDATE_PGP_MIME_SIGNATURES
	if (g_mime_content_type_is_type (type, "multipart", "signed")) {
		GMimeCipherValidity *validity;
		GMimePgpContext *ctx;
		int valid;
		
		ctx = g_mime_pgp_context_new (pgp_type, pgp_path, get_passwd, NULL);
		
		validity = g_mime_pgp_mime_part_verify (ctx, part, NULL);
		valid = g_mime_cipher_validity_get_valid (validity);
		fprintf (stdout, "  [%s signature]\n", valid ? "GOOD" : "BAD");
		if (!valid) {
			const char *desc;
			
			desc = g_mime_cipher_validity_get_description (validity);
			fprintf (stderr, "%s", desc ? desc : "");
		}
		g_mime_cipher_validity_free (validity);
		
		g_mime_object_unref (GMIME_OBJECT (ctx));
	} else {
		printf ("\n");
	}
#else
	printf ("\n");
#endif
	
	if (g_mime_content_type_is_type (type, "multipart", "*")) {
		GList *child;
		
		child = part->children;
		while (child) {
			print_mime_struct (child->data, depth + 1);
			child = child->next;
		}
	}
}

void
test_parser (GMimeStream *stream)
{
	GMimeMessage *message;
	gboolean is_html;
	char *text;
	
	fprintf (stdout, "\nTesting MIME parser...\n\n");
	
	ZenTimerStart();
	message = g_mime_parser_construct_message (stream, TRUE);
	ZenTimerStop();
	ZenTimerReport ("gmime::parser_construct_message");
	
	ZenTimerStart();
	text = g_mime_message_to_string (message);
	ZenTimerStop();
	ZenTimerReport ("gmime::message_to_string");
	/*fprintf (stdout, "Result should match previous MIME message dump\n\n%s\n", text);*/
	g_free (text);
	
#ifdef TEST_PRESERVE_HEADERS
	{
		GMimeStream *stream;
		
		fprintf (stdout, "\nTesting preservation of headers...\n\n");
		stream = g_mime_stream_file_new (stdout);
		g_mime_header_write_to_stream (message->header->headers, stream);
		g_mime_stream_flush (stream);
		GMIME_STREAM_FILE (stream)->fp = NULL;
		g_mime_stream_unref (stream);
		fprintf (stdout, "\n");
	}
#endif
	
#ifdef TEST_GET_BODY
	{
		/* test of get_body */
		char *body;
		
		body = g_mime_message_get_body (message, FALSE, &is_html);
		fprintf (stdout, "Testing get_body (looking for html...%s)\n\n%s\n\n",
			 body && is_html ? "found" : "not found",
			 body ? body : "No message body found");
		
		g_free (body);
	}
#endif
	
#ifdef PRINT_MIME_STRUCT
	/* print mime structure */
	print_mime_struct (message->mime_part, 0);
#endif
	
	g_mime_object_unref (GMIME_OBJECT (message));
}



/* you can only enable one of these at a time... */
/*#define STREAM_BUFFER*/
/*#define STREAM_MEM*/
/*#define STREAM_MMAP*/

int main (int argc, char **argv)
{
	char *filename = NULL;
	GMimeStream *stream, *istream;
	int fd;
	
	if (argc > 1)
		filename = argv[1];
	else
		return 0;
	
	fd = open (filename, O_RDONLY);
	if (fd == -1)
		return 0;
	
#ifdef STREAM_MMAP
	stream = g_mime_stream_mmap_new (fd, PROT_READ, MAP_PRIVATE);
	g_assert (stream != NULL);
#else
	stream = g_mime_stream_fs_new (fd);
#endif /* STREAM_MMAP */
	
#ifdef STREAM_MEM
	istream = g_mime_stream_mem_new ();
	g_mime_stream_write_to_stream (stream, istream);
	g_mime_stream_reset (istream);
	g_mime_stream_unref (stream);
	stream = istream;
#endif
	
#ifdef STREAM_BUFFER
	istream = g_mime_stream_buffer_new (stream,
					    GMIME_STREAM_BUFFER_BLOCK_READ);
	g_mime_stream_unref (stream);
	stream = istream;
#endif
	
	test_parser (stream);
	
	g_mime_stream_unref (stream);
	
	return 0;
}
