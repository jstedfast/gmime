/* Adapted from MimeKit.Fuzzing.Program. Copyright (c) 2013-2026 .NET Foundation
 * and Contributors. Distributed under the MIT license; see LICENSE. */

#include "fuzz.h"
#include <string.h>

static gboolean
collect_files (const char *path, GPtrArray *files)
{
	if (g_file_test (path, G_FILE_TEST_IS_DIR)) {
		GError *error = NULL;
		GDir *dir;
		const char *name;

		if (g_file_test (path, G_FILE_TEST_IS_SYMLINK)) {
			g_printerr ("Refusing to recurse into symbolic-link directory: %s\n", path);
			return FALSE;
		}
		dir = g_dir_open (path, 0, &error);
		if (!dir) {
			g_printerr ("%s\n", error->message);
			g_error_free (error);
			return FALSE;
		}
		while ((name = g_dir_read_name (dir))) {
			char *child = g_build_filename (path, name, NULL);
			gboolean success = collect_files (child, files);

			g_free (child);
			if (!success) {
				g_dir_close (dir);
				return FALSE;
			}
		}
		g_dir_close (dir);
	} else if (g_file_test (path, G_FILE_TEST_IS_REGULAR)) {
		g_ptr_array_add (files, g_strdup (path));
	} else {
		g_printerr ("Not a regular file or directory: %s\n", path);
		return FALSE;
	}
	return TRUE;
}

static gint
compare_paths (gconstpointer first, gconstpointer second)
{
	return strcmp (*(char * const *) first, *(char * const *) second);
}

int
main (int argc, char **argv)
{
	static const char *names[] = { "mime-reader", "mime-parser" };
	static const FuzzRun targets[] = { fuzz_mime_reader, fuzz_mime_parser };
	GPtrArray *files;
	guint i, j, count = 0, skipped = 0;
	int arg;
	gboolean all;

	if (argc == 2 && !strcmp (argv[1], "--list")) {
		for (i = 0; i < G_N_ELEMENTS (names); i++)
			g_print ("%s\t%d\n", names[i], FUZZ_MAX_LENGTH);
		return 0;
	}
	if (argc < 4 || strcmp (argv[1], "--replay")) {
		g_printerr ("Usage: gmime-fuzz --list | --replay <mime-reader|mime-parser|all> <file|directory>...\n");
		return 2;
	}
	all = !strcmp (argv[2], "all");
	if (!all && strcmp (argv[2], names[0]) && strcmp (argv[2], names[1])) {
		g_printerr ("Unknown fuzz target: %s\n", argv[2]);
		return 2;
	}
	files = g_ptr_array_new_with_free_func (g_free);
	for (arg = 3; arg < argc; arg++) {
		if (!collect_files (argv[arg], files)) {
			g_ptr_array_unref (files);
			return 2;
		}
	}
	g_ptr_array_sort (files, compare_paths);
	fuzz_init ();
	for (i = 0; i < G_N_ELEMENTS (names); i++) {
		if (!all && strcmp (argv[2], names[i]))
			continue;
		for (j = 0; j < files->len; j++) {
			const char *file = files->pdata[j];
			GError *error = NULL;
			char *data;
			gsize length;
			gint64 start;

			if (!g_file_get_contents (file, &data, &length, &error)) {
				g_printerr ("%s\n", error->message);
				g_error_free (error);
				g_ptr_array_unref (files);
				g_mime_shutdown ();
				return 2;
			}
			if (length > FUZZ_MAX_LENGTH) {
				skipped++;
				g_free (data);
				continue;
			}
			g_print ("REPLAY %s %s\n", names[i], file);
			start = g_get_monotonic_time ();
			targets[i] ((const guint8 *) data, length);
			if (g_get_monotonic_time () - start > 2 * G_TIME_SPAN_SECOND)
				g_print ("SLOW %s %s\n", names[i], file);
			g_free (data);
			count++;
		}
	}
	g_print ("Replayed %u input(s), skipped %u oversized input(s).\n", count, skipped);
	g_ptr_array_unref (files);
	g_mime_shutdown ();
	return count ? 0 : 2;
}
