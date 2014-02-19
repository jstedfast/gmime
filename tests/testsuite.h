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


#ifndef __TEST_SUITE_H__
#define __TEST_SUITE_H__

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <setjmp.h>

G_BEGIN_DECLS

void testsuite_init (int argc, char **argv);
int testsuite_exit (void);

void testsuite_printf (FILE *out, int verbosity, const char *fmt, ...);
void testsuite_vprintf (FILE *out, int verbosity, const char *fmt, va_list args);

/* start/end a test collection */
void testsuite_start (const char *test);
void testsuite_end (void);

void testsuite_check (const char *checking, ...);
void testsuite_check_failed (const char *fmt, ...);
void testsuite_check_warn (const char *fmt, ...);
void testsuite_check_passed (void);

int testsuite_total_errors (void);


/*#if __GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ >= 96)
#define G_GNUC_NORETURN __attribute__((noreturn))
#else
#define G_GNUC_NORETURN
#endif*/

typedef struct _Exception {
	char *message;
} Exception;

Exception *exception_new (const char *fmt, ...);
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

G_END_DECLS

#endif /* __TEST_SUITE_H__ */
