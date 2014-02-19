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

#include <glib.h>
#include <stdlib.h>
#ifdef ENABLE_THREADS
#include <pthread.h>
#endif

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
testsuite_vprintf (FILE *out, int verbosity, const char *fmt, va_list args)
{
	if (verbose < verbosity)
		return;
	
	vfprintf (out, fmt, args);
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
