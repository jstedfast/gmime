/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2020 Jeffrey Stedfast
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

#include <glib.h>
#include <glib/gstdio.h>

#include <stdlib.h>
#ifdef ENABLE_THREADS
#include <pthread.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "testsuite.h"


static struct _stack *stack = NULL;
static int total_errors = 0;
static int total_tests = 0;
int verbose = 0;

enum {
	TEST,
	CHECK
};

typedef enum {
	UNKNOWN,
	PASSED,
	WARNING,
	FAILED
} status_t;

struct _stack {
	struct _stack *next;
	char *message;
	int type;
	union {
		struct {
			int failures;
			int warnings;
			int passed;
		} test;
		struct {
			status_t status;
		} check;
	} v;
};

#ifdef ENABLE_THREADS
static pthread_key_t key;

static void
key_data_destroy (void *user_data)
{
	ExceptionEnv *stack = user_data;
	ExceptionEnv *parent;
	
	while (stack != NULL) {
		parent = stack->parent;
		g_free (stack);
		stack = parent;
	}
}

#define exception_env_set(env) pthread_setspecific (key, env)
#define exception_env_get()    pthread_getspecific (key)
#else
static ExceptionEnv *exenv = NULL;

#define exception_env_set(env) exenv = (env)
#define exception_env_get()    exenv
#endif


void
testsuite_init (int argc, char **argv)
{
	int i, j;
	
	for (i = 1; i < argc; i++) {
		if (argv[i][0] == '-' && argv[i][1] != '-') {
			for (j = 1; argv[i][j]; j++) {
				if (argv[i][j] == 'v')
					verbose++;
			}
		}
	}
	
#ifdef ENABLE_THREADS
	pthread_key_create (&key, key_data_destroy);
#endif
}

int
testsuite_exit (void)
{
	return total_errors;
}


void
testsuite_printf (FILE *out, int verbosity, const char *fmt, ...)
{
	va_list ap;
	
	if (verbose < verbosity)
		return;
	
	va_start (ap, fmt);
	vfprintf (out, fmt, ap);
	va_end (ap);
}

void
testsuite_start (const char *test)
{
	struct _stack *s;
	
	s = g_new (struct _stack, 1);
	s->next = stack;
	stack = s;
	
	s->type = TEST;
	s->message = g_strdup (test);
	s->v.test.failures = 0;
	s->v.test.warnings = 0;
	s->v.test.passed = 0;
}

void
testsuite_end (void)
{
	struct _stack *s = stack;
	
	if (verbose > 0) {
		fputs ("Testing ", stdout);
		fputs (s->message, stdout);
		
		if (stack->v.test.failures > 0) {
			fprintf (stdout, ": failed (%d errors, %d warnings)\n",
				 stack->v.test.failures,
				 stack->v.test.warnings);
		} else if (stack->v.test.warnings > 0) {
			fprintf (stdout, ": passed (%d warnings)\n",
				 stack->v.test.warnings);
		} else if (stack->v.test.passed > 0) {
			fputs (": passed\n", stdout);
		} else {
			fputs (": no tests performed\n", stdout);
		}
	}
	
	stack = stack->next;
	g_free (s->message);
	g_free (s);
}

void
testsuite_check (const char *checking, ...)
{
	struct _stack *s;
	va_list ap;
	
	g_assert (stack != NULL && stack->type == TEST);
	
	s = g_new (struct _stack, 1);
	s->next = stack;
	stack = s;
	
	s->type = CHECK;
	s->v.check.status = UNKNOWN;
	
	va_start (ap, checking);
	s->message = g_strdup_vprintf (checking, ap);
	va_end (ap);
}

static void
testsuite_check_pop (void)
{
	struct _stack *s = stack;
	
	g_assert (stack != NULL && stack->type == CHECK);
	
	if (verbose > 1) {
		fputs ("Checking ", stdout);
		fputs (s->message, stdout);
		fputs ("... ", stdout);
		
		switch (stack->v.check.status) {
		case PASSED:
			fputs ("PASSED\n", stdout);
			break;
		case WARNING:
			fputs ("WARNING\n", stdout);
			break;
		case FAILED:
			fputs ("FAILED\n", stdout);
			break;
		default:
			g_assert_not_reached ();
		}
	}
	
	stack = stack->next;
	g_free (s->message);
	g_free (s);
}

void
testsuite_check_failed (const char *fmt, ...)
{
	va_list ap;
	
	g_assert (stack != NULL && stack->type == CHECK);
	
	if (verbose > 2) {
		va_start (ap, fmt);
		vfprintf (stderr, fmt, ap);
		va_end (ap);
		fputc ('\n', stderr);
	}
	
	stack->v.check.status = FAILED;
	
	testsuite_check_pop ();
	
	stack->v.test.failures++;
	total_errors++;
	total_tests++;
}

void
testsuite_check_warn (const char *fmt, ...)
{
	va_list ap;
	
	g_assert (stack != NULL && stack->type == CHECK);
	
	if (verbose > 2) {
		va_start (ap, fmt);
		vfprintf (stderr, fmt, ap);
		va_end (ap);
		fputc ('\n', stderr);
	}
	
	stack->v.check.status = WARNING;
	
	testsuite_check_pop ();
	
	stack->v.test.warnings++;
	total_tests++;
}

void
testsuite_check_passed (void)
{
	g_assert (stack != NULL && stack->type == CHECK);
	
	stack->v.check.status = PASSED;
	
	testsuite_check_pop ();
	
	stack->v.test.passed++;
	total_tests++;
}

int
testsuite_total_errors (void)
{
	return total_errors;
}


Exception *
exception_new (const char *fmt, ...)
{
	Exception *ex;
	va_list ap;
	
	ex = g_new (Exception, 1);
	va_start (ap, fmt);
	ex->message = g_strdup_vprintf (fmt, ap);
	va_end (ap);
	
	return ex;
}

void
exception_free (Exception *ex)
{
	g_free (ex->message);
	g_free (ex);
}

void
g_try (ExceptionEnv *env)
{
	ExceptionEnv *penv = exception_env_get ();
	
	env->parent = penv;
	env->ex = NULL;
	
	exception_env_set (env);
}

void
g_throw (Exception *ex)
{
	ExceptionEnv *env;
	
	if (!(env = exception_env_get ())) {
		fprintf (stderr, "Uncaught exception: %s\n", ex->message);
		abort ();
	}
	
	env->ex = ex;
	exception_env_set (env->parent);
	longjmp (env->env, 1);
}

/* in versions of gpg before 2.1.16, the only mechanism to override
   the session key was --override-session-key, which leaks its
   argument to the process table.

   in 2.1.16, gpg introduced --override-session-key-fd, which is what
   gmime uses to be safe.
*/

#define v2_1_16 ((2 << 24) | (1 << 16) | (16 << 8))

static int
get_gpg_version (const char *path)
{
	const char vheader[] = " (GnuPG) ";
	int v, n = 0, version = 0;
	const char *inptr;
	char buffer[128];
	char *command;
	FILE *gpg;
	
	g_return_val_if_fail (path != NULL, -1);
	
	command = g_strdup_printf ("%s --version", path);
	gpg = popen (command, "r");
	g_free (command);
	
	if (gpg == NULL)
		return -1;
	
	inptr = fgets (buffer, 128, gpg);
	pclose (gpg);

	if (!(inptr = strstr (inptr, vheader)))
		return -1;
	
	inptr += sizeof (vheader) - 1;
	while (*inptr >= '0' && *inptr <= '9' && n < 4) {
		v = 0;
		
		while (*inptr >= '0' && *inptr <= '9' && (v < 25 || (v == 25 && *inptr < '6'))) {
			v = (v * 10) + (*inptr - '0');
			inptr++;
		}
		
		version = (version << 8) + v;
		n++;
		
		if (*inptr != '.')
			break;
		
		inptr++;
	}
	
	if (n == 0)
		return -1;
	
	if (n < 4)
		version = version << ((4 - n) * 8);
	
	return version;
}

gboolean
testsuite_can_safely_override_session_key (const char *gpg)
{
	return get_gpg_version (gpg) >= v2_1_16;
}

int
testsuite_setup_gpghome (const char *gpg)
{
	char *command;
	FILE *fp;
	
	/* reset .gnupg config directory */
	if (system ("/bin/rm -rf ./tmp") != 0)
		return EXIT_FAILURE;
	
	if (g_mkdir ("./tmp", 0755) != 0)
		return EXIT_FAILURE;
	
	g_setenv ("GNUPGHOME", "./tmp/.gnupg", 1);
	
	/* disable environment variables that gpg-agent uses for pinentry */
	g_unsetenv ("DBUS_SESSION_BUS_ADDRESS");
	g_unsetenv ("DISPLAY");
	g_unsetenv ("GPG_TTY");
	
        command = g_strdup_printf ("%s --list-keys > /dev/null 2>&1", gpg);
	if (system (command) != 0) {
		g_free (command);
		return EXIT_FAILURE;
	}
	
	g_free (command);
	
	if (get_gpg_version (gpg) >= ((2 << 24) | (1 << 16))) {
		if (!(fp = fopen ("./tmp/.gnupg/gpg.conf", "a")))
			return EXIT_FAILURE;
		
		if (fprintf (fp, "pinentry-mode loopback\n") == -1) {
			fclose (fp);
			return EXIT_FAILURE;
		}
		
		if (fclose (fp))
			return EXIT_FAILURE;
	}
	
	if (!(fp = fopen ("./tmp/.gnupg/gpgsm.conf", "a")))
		return EXIT_FAILURE;
	
	if (fprintf (fp, "disable-crl-checks\n") == -1) {
		fclose (fp);
		return EXIT_FAILURE;
	}
	
	if (fclose (fp))
		return EXIT_FAILURE;
	
#if DEBUG_GNUPG
	if (TRUE) {
		const char *files[] = { "./tmp/.gnupg/gpg.conf", "./tmp/.gnupg/gpgsm.conf", "./tmp/.gnupg/gpg-agent.conf", "./tmp/.gnupg/dirmngr.conf", NULL };
		const char debug[] = "log-file socket://%s/tmp/.gnupg/S.log\ndebug 1024\nverbose\n";
		char *cwd = g_get_current_dir ();
		int i;
		
		for (i = 0; files[i]; i++) {
			if (!(fp = fopen (files[i], "a")))
				return EXIT_FAILURE;
			
			if (fprintf (fp, debug, cwd) == -1) {
				g_free (cwd);
				fclose (fp);
				return EXIT_FAILURE;
			}
			
			if (fclose (fp)) {
				g_free (cwd);
				return EXIT_FAILURE;
			}
		}
		
		g_free (cwd);
	}
#endif
	
	return EXIT_SUCCESS;
}

int
testsuite_destroy_gpghome (void)
{
	if (system ("/bin/rm -rf ./tmp") != 0)
		return EXIT_FAILURE;
	return EXIT_SUCCESS;
}


static void test_stream_onebyte_class_init (TestStreamOneByteClass *klass);
static void test_stream_onebyte_init (TestStreamOneByte *stream, TestStreamOneByteClass *klass);
static void test_stream_onebyte_finalize (GObject *object);

static ssize_t stream_read (GMimeStream *stream, char *buf, size_t len);
static ssize_t stream_write (GMimeStream *stream, const char *buf, size_t len);
static int stream_flush (GMimeStream *stream);
static int stream_close (GMimeStream *stream);
static gboolean stream_eos (GMimeStream *stream);
static int stream_reset (GMimeStream *stream);
static gint64 stream_seek (GMimeStream *stream, gint64 offset, GMimeSeekWhence whence);
static gint64 stream_tell (GMimeStream *stream);
static gint64 stream_length (GMimeStream *stream);
static GMimeStream *stream_substream (GMimeStream *stream, gint64 start, gint64 end);

static GMimeStreamClass *parent_class = NULL;

GType
test_stream_onebyte_get_type (void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof (TestStreamOneByteClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc) test_stream_onebyte_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (TestStreamOneByte),
			0,    /* n_preallocs */
			(GInstanceInitFunc) test_stream_onebyte_init,
		};
		
		type = g_type_register_static (GMIME_TYPE_STREAM, "TestStreamOneByte", &info, 0);
	}
	
	return type;
}


static void
test_stream_onebyte_class_init (TestStreamOneByteClass *klass)
{
	GMimeStreamClass *stream_class = GMIME_STREAM_CLASS (klass);
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	
	parent_class = g_type_class_ref (GMIME_TYPE_STREAM);
	
	object_class->finalize = test_stream_onebyte_finalize;
	
	stream_class->read = stream_read;
	stream_class->write = stream_write;
	stream_class->flush = stream_flush;
	stream_class->close = stream_close;
	stream_class->eos = stream_eos;
	stream_class->reset = stream_reset;
	stream_class->seek = stream_seek;
	stream_class->tell = stream_tell;
	stream_class->length = stream_length;
	stream_class->substream = stream_substream;
}

static void
test_stream_onebyte_init (TestStreamOneByte *stream, TestStreamOneByteClass *klass)
{
	stream->source = NULL;
}

static void
test_stream_onebyte_finalize (GObject *object)
{
	TestStreamOneByte *stream = (TestStreamOneByte *) object;
	
	if (stream->source)
		g_object_unref (stream->source);
	
	G_OBJECT_CLASS (parent_class)->finalize (object);
}


static ssize_t
stream_read (GMimeStream *stream, char *buf, size_t len)
{
	TestStreamOneByte *test = (TestStreamOneByte *) stream;
	
	return g_mime_stream_read (test->source, buf, MIN (len, 1));
}

static ssize_t
stream_write (GMimeStream *stream, const char *buf, size_t len)
{
	TestStreamOneByte *test = (TestStreamOneByte *) stream;
	size_t nwritten = 0;
	
	while (nwritten < len) {
		if (g_mime_stream_write (test->source, buf + nwritten, 1) > 0)
			nwritten++;
	}
	
	return (ssize_t) nwritten;
}

static int
stream_flush (GMimeStream *stream)
{
	TestStreamOneByte *test = (TestStreamOneByte *) stream;
	
	return g_mime_stream_flush (test->source);
}

static int
stream_close (GMimeStream *stream)
{
	TestStreamOneByte *test = (TestStreamOneByte *) stream;
	
	return g_mime_stream_close (test->source);
}

static gboolean
stream_eos (GMimeStream *stream)
{
	TestStreamOneByte *test = (TestStreamOneByte *) stream;
	
	return g_mime_stream_eos (test->source);
}

static int
stream_reset (GMimeStream *stream)
{
	TestStreamOneByte *test = (TestStreamOneByte *) stream;
	
	return g_mime_stream_reset (test->source);
}

static gint64
stream_seek (GMimeStream *stream, gint64 offset, GMimeSeekWhence whence)
{
	TestStreamOneByte *test = (TestStreamOneByte *) stream;
	
	return g_mime_stream_seek (test->source, offset, whence);
}

static gint64
stream_tell (GMimeStream *stream)
{
	TestStreamOneByte *test = (TestStreamOneByte *) stream;
	
	return g_mime_stream_tell (test->source);
}

static gint64
stream_length (GMimeStream *stream)
{
	TestStreamOneByte *test = (TestStreamOneByte *) stream;
	
	return g_mime_stream_length (test->source);
}

static GMimeStream *
stream_substream (GMimeStream *stream, gint64 start, gint64 end)
{
	TestStreamOneByte *test = (TestStreamOneByte *) stream;
	
	return g_mime_stream_substream (test->source, start, end);
}


/**
 * test_stream_onebyte_new:
 * @source: source stream
 *
 * Creates a new TestStreamOneByte object.
 *
 * Returns: a new test stream with source @source.
 **/
GMimeStream *
test_stream_onebyte_new (GMimeStream *source)
{
	TestStreamOneByte *test;
	
	g_return_val_if_fail (GMIME_IS_STREAM (source), NULL);
	
	test = g_object_new (TEST_TYPE_STREAM_ONEBYTE, NULL);
	
	test->source = source;
	g_object_ref (source);
	
	g_mime_stream_construct ((GMimeStream *) test,
				 source->bound_start,
				 source->bound_end);
	
	return (GMimeStream *) test;
}


#ifdef BUILD
int main (int argc, char **argv)
{
	testsuite_init (argc, argv);
	
	testsuite_start ("test-suite implementation");
	
	testsuite_check ("exceptions work");
	try {
		if (TRUE)
			throw (exception_new ("ApplicationException"));
		testsuite_check_passed ();
	} catch (ex) {
		testsuite_check_failed ("Caught %s", ex->message);
	} finally;
	
#if 0
	/* try */ {
		ExceptionEnv __env = { NULL, };
		if (setjmp (__env.env) == 0) {
			if (TRUE) {
				__env.ex = exception_new ("ApplicationException");
				longjmp (__env.env, 1);
			}
			testsuite_check_passed ();
		} /* catch */ else {
			Exception *ex = __env.ex;
			if (ex != NULL) {
				testsuite_check_failed (ex->message);
			} /* finally */ }
		exception_free (__env.ex);
	};
#endif
	
	testsuite_end ();
	
	return testsuite_exit ();
}
#endif
