#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <glib.h>
#include <time.h>

#include "gmime.h"

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
	fprintf (stdout, "Content-Type: %s/%s\n", type->type, type->subtype);
	
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
test_parser (gchar *data)
{
	GMimeMessage *message;
	gboolean is_html;
	gchar *text;
	gchar *body;
	time_t now, past;
	
	fprintf (stdout, "\nTesting MIME parser...\n\n");
	time (&past);
	message = g_mime_parser_construct_message (data, TRUE);
	time (&now);
	fprintf (stderr, "parsed message in %lus.\n", now - past);
	time (&past);
	text = g_mime_message_to_string (message);
	time (&now);
	fprintf (stderr, "wrote message in %lus\n", now - past);
	/*fprintf (stdout, "Result should match previous MIME message dump\n\n%s\n", text);*/
	g_free (text);
	
	/* test of get_body */
	body = g_mime_message_get_body (message, FALSE, &is_html);
	fprintf (stderr, "Testing get_body (looking for html...%s)\n\n%s\n",
		 body && is_html ? "found" : "not found",
		 body ? body : "No message body found");
	
	g_free (body);
	
	/* print mime structure */
	print_mime_struct (message->mime_part, 0);
	
	g_mime_message_destroy (message);
}

int main (int argc, char *argv[])
{
	char *filename = NULL;
	char *data;
	struct stat st;
	int fd;
	
	if (argc > 1)
		filename = argv[1];
	else
		return 0;
	
	fd = open (filename, O_RDONLY);
	if (fd == -1)
		return 0;
	
	if (fstat (fd, &st) == -1)
		return 0;
	
	data = g_malloc0 (st.st_size + 1);
	
	read (fd, data, st.st_size);
	
	close (fd);
	
	/*fprintf (stdout, "%s\n", data);*/
	
	test_parser (data);
	
	g_free (data);
	
	return 0;
}
