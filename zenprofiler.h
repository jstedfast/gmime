/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  ZenProfiler
 *  Copyright (C) 2001-2007 Jeffrey Stedfast
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public License
 *  as published by the Free Software Foundation; either version 2.1
 *  of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, 51 Franklin Street, Fifth Floor, Boston, MA
 *  02110-1301, USA.
 */


#ifndef __ZENPROFILER_H__
#define __ZENPROFILER_H__

#ifdef ENABLE_ZENPROFILER

#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#ifdef ENABLE_THREADS
#include <pthread.h>
#endif

#ifndef ENABLE_ZENTIMER
#ifdef ENABLE_ZENPROFILER
#define ENABLE_ZENTIMER
#endif
#endif /* ENABLE_ZENTIMER */

#include "zentimer.h"

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

typedef struct _zfunc_t {
	struct _zfunc_t *next;
	uint32_t ncalls;
	ztime_t total;
	char *name;
} zfunc_t;

static struct {
	GHashTable *hash;
	zfunc_t *functions, *tail;
	uint32_t nfuncs;
	ztimer_t ztimer;
	FILE *log;
} zprof = { NULL, NULL, (zfunc_t *) &zprof.functions, 0, ZTIMER_INITIALIZER, NULL };

#ifdef ENABLE_THREADS
static pthread_mutex_t zlock = PTHREAD_MUTEX_INITIALIZER;
#define ZENPROFILER_LOCK()   pthread_mutex_lock (&zlock)
#define ZENPROFILER_UNLOCK() pthread_mutex_unlock (&zlock)
#else
#define ZENPROFILER_LOCK()
#define ZENPROFILER_UNLOCK()
#endif /* ENABLE_THREADS */

static char *
zfuncname (const char *func)
{
	register const const char *inptr = func;
	const char *name;
	
	while (*inptr == ' ' || *inptr == '\t')
		inptr++;
	
	name = inptr;
	
	while (*inptr && *inptr != ' ' && *inptr != '(')
		inptr++;
	
	return g_strndup (name, inptr - name);
}


/**
 * ZenProfilerInit:
 * @logfile: File to save profiling information (or %NULL to log to stderr)
 *
 * Initializes the Zen Profiler.
 **/
static void
ZenProfilerInit (const char *logfile)
{
	zprof.hash = g_hash_table_new (g_str_hash, g_str_equal);
	
	if (logfile)
		zprof.log = fopen (logfile, "wt");
	
	ZenTimerStart (&zprof.ztimer);
}


static void
zprofile (char *name, ztimer_t *ztimer)
{
	zfunc_t *zfunc;
	ztime_t delta;
	char *key;
	
	ZENPROFILER_LOCK();
	
	if (!g_hash_table_lookup_extended (zprof.hash, name, (gpointer *) &key, (gpointer *) &zfunc)) {
		zfunc = g_new (zfunc_t, 1);
		zfunc->next = NULL;
		zfunc->name = name;
		zfunc->ncalls = 0;
		zfunc->total.sec = 0;
		zfunc->total.usec = 0;
		
		zprof.tail->next = zfunc;
		zprof.tail = zfunc;
		
		g_hash_table_insert (zprof.hash, zfunc->name, zfunc);
		zprof.nfuncs++;
	} else
		g_free (name);
	
	ztime_delta (&ztimer->start, &ztimer->stop, &delta);
	ztime_add (&zfunc->total, &delta);
	zfunc->ncalls++;
	
	ZENPROFILER_UNLOCK();
}


/**
 * ZenProfilerLazy:
 * @func: Function call to profile.
 *
 * Profiles the provided function call. This is basically the lazy way out.
 *
 * Sample usage:  ZenProfilerLazy(printf ("how long does this take??\n"));
 **/
#define ZenProfilerLazy(func) G_STMT_START {                  \
	ztimer_t ztimer;                                      \
	                                                      \
	ZenTimerStart (&ztimer);                              \
	func;                                                 \
	ZenTimerStop (&ztimer);                               \
	                                                      \
	zprofile (zfuncname (#func), &ztimer);                \
} G_STMT_END


/**
 * ZenProfilerStart:
 * @ztimerp: pointer to a ztimer_t
 *
 * Initializes @zstart.
 **/
#define ZenProfilerStart(ztimerp) ZenTimerStart (ztimerp)


/**
 * ZenProfilerStop:
 * @ztimerp: pointer to a ztimer_t
 *
 * Initializes @zstop.
 **/
#define ZenProfilerStop(ztimerp) ZenTimerStop (ztimerp)


/**
 * ZenProfilerReport:
 * @ztimerp: pointer to a ztimer_t
 *
 * Cache the results for later reporting.
 **/
#define ZenProfilerReport(ztimerp) zprofile (g_strdup (G_GNUC_PRETTY_FUNCTION), ztimerp)


/**
 * ZenProfiler_return:
 * @ztimer: a ztimer_t
 *
 * Stop the timer, report the results, and then return from the function.
 *
 * Note: equivalent to: ZenProfilerStop(@ztimer); ZenProfilerReport(@ztimer); return;
 **/
#define ZenProfiler_return(ztimerp) G_STMT_START {             \
	ZenProfilerStop (ztimerp);                             \
	ZenProfilerReport (ztimerp);                           \
	return;                                                \
} G_STMT_END


/**
 * ZenProfiler_return_val:
 * @ztimer: a ztimer_t
 *
 * Same as ZenProfiler_return() but returns the specified value.
 **/
#define ZenProfiler_return_val(ztimerp, retval) G_STMT_START { \
	ZenProfilerStop (ztimerp);                             \
	ZenProfilerReport (ztimerp);                           \
	return retval;                                         \
} G_STMT_END


static void
zen_log (FILE *log, zfunc_t *zfunc, uint32_t usec)
{
	char percent[7], total[20], calls[20], average[20];
	uint32_t us, avgus;
	double pcnt = 0.0;
	
	us = (zfunc->total.sec * ZTIME_USEC_PER_SEC) + zfunc->total.usec;
	pcnt = ((double) us) / ((double) usec) * 100.0;
	avgus = us / zfunc->ncalls;
	
	sprintf (percent, "%2.2f", pcnt);
	sprintf (calls, "%u", zfunc->ncalls);
	sprintf (total, "%u.%03u", zfunc->total.sec, zfunc->total.usec / 1000);
	sprintf (average, "%u.%03u", avgus / ZTIME_USEC_PER_SEC, (avgus % ZTIME_USEC_PER_SEC) / 1000);
	
	fprintf (log, "%6s %9s  %7s  %7s %7s %7s  %s\n",
		 percent, total, "n/a", calls, "n/a", average, zfunc->name);
}


static int
zfunccmp (const void *v1, const void *v2)
{
	const zfunc_t *func1 = (const zfunc_t *) v1;
	const zfunc_t *func2 = (const zfunc_t *) v2;
	uint32_t usec1, usec2;
	
	usec1 = (func1->total.sec * ZTIME_USEC_PER_SEC) + func1->total.usec;
	usec2 = (func2->total.sec * ZTIME_USEC_PER_SEC) + func2->total.usec;
	
	return usec2 - usec1;
}


/**
 * ZenProfilerShutdown:
 *
 * Write out our profiling info and cleanup the mess we made.
 **/
static void
ZenProfilerShutdown (void)
{
	FILE *log = zprof.log ? zprof.log : stderr;
	uint32_t usec, i;
	zfunc_t *zfunc;
	ztime_t delta;
	void **pdata;
	
	ZenTimerStop (&zprof.ztimer);
	
	ztime_delta (&zprof.ztimer.start, &zprof.ztimer.stop, &delta);
	
	fprintf (log, "ZenProfiler\n\n");
	fprintf (log, "Flat profile:\n\n");
	fprintf (log, "  %%   cumulative   self                self       total\n");
	fprintf (log, " time   seconds   seconds    calls    s/call      s/call    name\n");
	fprintf (log, "------ ---------  -------  ------- ----------- -----------  ----\n");
	
	pdata = g_malloc (sizeof (void *) * zprof.nfuncs);
	for (i = 0, zfunc = zprof.functions; i < zprof.nfuncs; i++, zfunc = zfunc->next)
		pdata[i] = zfunc;
	qsort (pdata, zprof.nfuncs, sizeof (void *), zfunccmp);
	
	usec = (delta.sec * ZTIME_USEC_PER_SEC) + delta.usec;
	for (i = 0; i < zprof.nfuncs; i++) {
		zfunc = pdata[i];
		zen_log (log, zfunc, usec);
		g_free (zfunc->name);
		g_free (zfunc);
	}
	g_hash_table_destroy (zprof.hash);
	g_free (pdata);
	
	fprintf (log, "\n %%         the percentage of the total running time of the\n");
	fprintf (log, "time       program used by this function.\n\n");
	fprintf (log, "cumulative a running sum of the number of seconds accounted\n");
	fprintf (log, " seconds   for by this function and those listed above it.\n\n");
	fprintf (log, " self      the number of seconds accounted for by this\n");
	fprintf (log, "seconds    function alone.  This is the major sort for this\n");
	fprintf (log, "           listing.\n\n");
	fprintf (log, "calls      the number of times this function was invoked, if\n");
	fprintf (log, "           this function is profiled, else blank.\n\n");
	fprintf (log, " self      the average number of seconds spent in this\n");
	fprintf (log, "s/call     function per call, if this function is profiled,\n");
	fprintf (log, "           else blank.\n\n");
	fprintf (log, " total     the average number of seconds spent in this\n");
	fprintf (log, " s/call    function and its descendents per call, if this \n");
	fprintf (log, "           function is profiled, else blank.\n\n");
	fprintf (log, "name       the name of the function.\n\n");
	
	if (zprof.log)
		fclose (zprof.log);
}

#ifdef __cplusplus
}
#endif /* __cplusplus */

#else /* ENABLE_ZENPROFILER */

#define ZenProfilerInit(logfile)
#define ZenProfilerLazy(func) func
#define ZenProfilerStart(ztimerp)
#define ZenProfilerStop(ztimerp)
#define ZenProfilerReport(ztimerp)
#define ZenProfiler_return(ztimerp) return
#define ZenProfiler_return_val(ztimerp, retval) return (retval)
#define ZenProfilerShutdown()

#endif /* ENABLE_ZENPROFILER */

#define Z(func) ZenProfilerLazy(func)

#endif /* __ZENPROFILER_H__ */
