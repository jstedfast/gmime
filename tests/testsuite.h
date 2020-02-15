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


#ifndef __TEST_SUITE_H__
#define __TEST_SUITE_H__

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <setjmp.h>

#include <gmime/gmime.h>

G_BEGIN_DECLS

void testsuite_init (int argc, char **argv);
int testsuite_exit (void);

void testsuite_printf (FILE *out, int verbosity, const char *fmt, ...) G_GNUC_PRINTF (3, 4);

/* start/end a test collection */
void testsuite_start (const char *test);
void testsuite_end (void);

void testsuite_check (const char *checking, ...) G_GNUC_PRINTF (1, 2);
void testsuite_check_failed (const char *fmt, ...) G_GNUC_PRINTF (1, 2);
void testsuite_check_warn (const char *fmt, ...) G_GNUC_PRINTF (1, 2);
void testsuite_check_passed (void);

int testsuite_total_errors (void);

/* GnuPG test suite utility functions */
int testsuite_setup_gpghome (const char *gpg);
int testsuite_destroy_gpghome (void);
gboolean testsuite_can_safely_override_session_key (const char *gpg);

/*#if __GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ >= 96)
#define G_GNUC_NORETURN __attribute__((noreturn))
#else
#define G_GNUC_NORETURN
#endif*/

typedef struct _Exception {
	char *message;
} Exception;

Exception *exception_new (const char *fmt, ...) G_GNUC_PRINTF (1, 2);
void exception_free (Exception *ex);

/* PRIVATE: do not use! */
typedef struct _ExceptionEnv {
	struct _ExceptionEnv *parent;
	Exception *ex;
	jmp_buf env;
} ExceptionEnv;

void g_try (ExceptionEnv *env);
void g_throw (Exception *ex) G_GNUC_NORETURN;

/* PUBLIC: try/throw/catch/finally - similar to c++, etc */
#define try { ExceptionEnv __env; g_try (&__env); if (setjmp (__env.env) == 0)
#define catch(e) else { Exception *e = __env.ex; if (e != NULL)
#define throw(e) g_throw (e)
#define finally } if (__env.ex != NULL) exception_free (__env.ex); }


/* A test stream that reads/writes 1 byte at a time */
#define TEST_TYPE_STREAM_ONEBYTE            (test_stream_onebyte_get_type ())
#define TEST_STREAM_ONEBYTE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), TEST_TYPE_STREAM_ONEBYTE, TestStreamOneByte))
#define TEST_STREAM_ONEBYTE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), TEST_TYPE_STREAM_ONEBYTE, TestStreamOneByteClass))
#define TEST_IS_STREAM_ONEBYTE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TEST_TYPE_STREAM_ONEBYTE))
#define TEST_IS_STREAM_ONEBYTE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), TEST_TYPE_STREAM_ONEBYTE))
#define TEST_STREAM_ONEBYTE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), TEST_TYPE_STREAM_ONEBYTE, TestStreamOneByteClass))

typedef struct _TestStreamOneByte TestStreamOneByte;
typedef struct _TestStreamOneByteClass TestStreamOneByteClass;

struct _TestStreamOneByte {
	GMimeStream parent_object;
	GMimeStream *source;
};

struct _TestStreamOneByteClass {
	GMimeStreamClass parent_class;
};

GType test_stream_onebyte_get_type (void);
GMimeStream *test_stream_onebyte_new (GMimeStream *source);

G_END_DECLS

#endif /* __TEST_SUITE_H__ */
