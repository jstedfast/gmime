#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <glib.h>

#include "gmime.h"

void
test_parser (gchar *data)
{
	GMimeMessage *message;
	gchar *text;
	
	fprintf (stdout, "\nTesting MIME parser...\n\n");
	message = g_mime_parser_construct_message (data, TRUE);
	text = g_mime_message_to_string (message);
	fprintf (stderr, "Result should match previous MIME message dump\n\n%s\n", text);
	g_free (text);
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
	
	fprintf (stderr, "%s\n", data);
	
	test_parser (data);
	
	g_free (data);
	
	return 0;
}
