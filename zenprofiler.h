/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2001 Ximian, Inc. (www.ximian.com)
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


#ifndef __ZENPROFILER_H__
#define __ZENPROFILER_H__

#include <glib.h>
#include <stdio.h>
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

#ifdef ENABLE_ZENPROFILER

struct zenfunc_t {
	char *name;
	unsigned num;
	ztime_t total;
};

static struct {
	FILE *log;
	ztimer_t ztimer;
	ztime_t total;
	GHashTable *hash;
	GSList *list;
} zenprof = { NULL, ZTIMER_INITIALIZER, { 0, 0 }, NULL, NULL };

#ifdef ENABLE_THREADS
static pthread_mutex_t zen_lock = PTHREAD_MUTEX_INITIALIZER;
#define ZENPROFILER_LOCK()   pthread_mutex_lock (&zen_lock)
#define ZENPROFILER_UNLOCK() pthread_mutex_unlock (&zen_lock)
#else
#define ZENPROFILER_LOCK()
#define ZENPROFILER_UNLOCK()
#endif /* ENABLE_THREADS */

static char *
zengetname (char *func)
{
	char *ptr;
	
	for (ptr = func; *ptr && *ptr != '('; ptr++);
	
	return g_strstrip (g_strndup (func, ptr - func));
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
	zenprof.hash = g_hash_table_new (g_str_hash, g_str_equal);
	
	ZenTimerMTStart (zenprof.ztimer);
	
	if (logfile)
		zenprof.log = fopen (logfile, "w+");
}


static void
zen_report_internal (char *name, ztimer_t ztimer)
{
	struct zenfunc_t *zenfunc;
	ztime_t diff;
	char *key;
	
	ZENPROFILER_LOCK();
	
	if (!g_hash_table_lookup_extended (zenprof.hash, name, (gpointer *) &key, (gpointer *) &zenfunc)) {
		zenfunc = g_new0 (struct zenfunc_t, 1);
		zenfunc->name = name;
		zenfunc->total.sec = 0;
		zenfunc->total.msec = 0;
		zenfunc->num = 0;
		g_hash_table_insert (zenprof.hash, zenfunc->name, zenfunc);
		zenprof.list = g_slist_prepend (zenprof.list, zenfunc);
	} else
		g_free (name);
	
	ztime_diff (ztimer.start, ztimer.stop, &diff);
	
	zenfunc->total.sec += diff.sec;
	zenfunc->total.msec += diff.msec;
	while (zenfunc->total.msec >= 1000) {
		zenfunc->total.msec -= 1000;
		zenfunc->total.sec += 1;
	}
	zenfunc->num++;
	
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
	{                                                     \
		ztimer_t ztimer;                              \
		char *name;                                   \
		                                              \
		ZenTimerMTStart (ztimer);                     \
		func;                                         \
		ZenTimerMTStop (ztimer);                      \
		                                              \
		name = zengetname (#func);                    \
		                                              \
		zen_report_internal (name, ztimer);           \
	}                                                     \
} G_STMT_END

/**
 * ZenProfilerStart:
 * @ztimer: a ztimer_t
 *
 * Initializes @zstart.
 **/
#define ZenProfilerStart(ztimer) ZenTimerMTStart (ztimer)


/**
 * ZenProfilerStop:
 * @ztimer: a ztimer_t
 *
 * Initializes @zstop.
 **/
#define ZenProfilerStop(ztimer) ZenTimerMTStop (ztimer)


/**
 * ZenProfilerReport:
 * @ztimer: a ztimer_t
 *
 * Cache the results for later reporting.
 **/
#define ZenProfilerReport(ztimer) G_STMT_START {              \
	{                                                     \
		char *name;                                   \
		                                              \
		name = g_strdup (__FUNCTION__);               \
		                                              \
		zen_report_internal (name, ztimer);           \
	}                                                     \
} G_STMT_END


/**
 * ZenProfiler_return:
 * @ztimer: a ztimer_t
 *
 * Stop the timer, report the results, and then return from the function.
 *
 * Note: equivalent to: ZenProfilerStop(@ztimer); ZenProfilerReport(@ztimer); return;
 **/
#define ZenProfiler_return(ztimer) G_STMT_START {             \
	ZenProfilerStop (ztimer);                             \
	ZenProfilerReport (ztimer);                           \
	return;                                               \
} G_STMT_END


/**
 * ZenProfiler_return_val:
 * @ztimer: a ztimer_t
 *
 * Same as ZenProfiler_return() but returns the specified value.
 **/
#define ZenProfiler_return_val(ztimer, retval) G_STMT_START { \
	ZenProfilerStop (ztimer);                             \
	ZenProfilerReport (ztimer);                           \
	return retval;                                        \
} G_STMT_END


#define ZENLOG (zenprof.log ? zenprof.log : stderr)

static void
zen_log (gpointer key, gpointer value, gpointer user_data)
{
	struct zenfunc_t *zenfunc = (struct zenfunc_t *) value;
	char percent[7], total[20], calls[20], average[20];
	unsigned long secs, millisecs;
	double pcnt = 0.0;
	
	if (zenfunc) {
		millisecs = zenfunc->total.sec * 1000 + zenfunc->total.msec;
		pcnt = ((double) millisecs) / ((double) zenprof.total.sec * 1000 + zenprof.total.msec) * 100;
		millisecs = (unsigned long) (millisecs / zenfunc->num);
		
		secs = 0;
		while (millisecs >= 1000) {
			millisecs -= 1000;
			secs++;
		}
		
		sprintf (percent, "%2.2f", pcnt);
		sprintf (calls, "%d", zenfunc->num);
		sprintf (total, "%lu.%03d", zenfunc->total.sec, zenfunc->total.msec);
		sprintf (average, "%lu.%03d", secs, millisecs);
		
		fprintf (ZENLOG, "%6s %9s  %7s  %7s %7s %7s  %s\n",
			 percent, total, "n/a", calls, "n/a", average, zenfunc->name);
		
		g_free (zenfunc->name);
		g_free (zenfunc);
	}
}


static int
zenfunccmp (gconstpointer a, gconstpointer b)
{
	const struct zenfunc_t *afunc = (const struct zenfunc_t *) a;
	const struct zenfunc_t *bfunc = (const struct zenfunc_t *) b;
	unsigned long amsec, bmsec;
	
	amsec = afunc->total.sec * 1000 + afunc->total.msec;
	bmsec = bfunc->total.sec * 1000 + bfunc->total.msec;
	
	return bmsec - amsec;
}


/**
 * ZenProfilerShutdown:
 *
 * Write out our profiling info and cleanup the mess we made.
 **/
static void
ZenProfilerShutdown (void)
{
	GSList *slist;
	
	ZenTimerMTStop (zenprof.ztimer);
	
	ztime_diff (zenprof.ztimer.start, zenprof.ztimer.stop, &zenprof.total);
	
	fprintf (ZENLOG, "ZenProfiler\n\n");
	fprintf (ZENLOG, "Flat profile:\n\n");
	fprintf (ZENLOG, "  %%   cumulative   self              self   total\n");
	fprintf (ZENLOG, " time   seconds   seconds    calls  s/call  s/call  name\n");
	fprintf (ZENLOG, "------ ---------  -------  ------- ------- -------  ----\n");
	zenprof.list = g_slist_sort (zenprof.list, zenfunccmp);
	for (slist = zenprof.list; slist; slist = slist->next)
		zen_log (NULL, slist->data, NULL);
	g_hash_table_destroy (zenprof.hash);
	g_slist_free (zenprof.list);
	fprintf (ZENLOG, "\n %%         the percentage of the total running time of the\n");
	fprintf (ZENLOG, "time       program used by this function.\n\n");
	fprintf (ZENLOG, "cumulative a running sum of the number of seconds accounted\n");
	fprintf (ZENLOG, " seconds   for by this function and those listed above it.\n\n");
	fprintf (ZENLOG, " self      the number of seconds accounted for by this\n");
	fprintf (ZENLOG, "seconds    function alone.  This is the major sort for this\n");
	fprintf (ZENLOG, "           listing.\n\n");
	fprintf (ZENLOG, "calls      the number of times this function was invoked, if\n");
	fprintf (ZENLOG, "           this function is profiled, else blank.\n\n");
	fprintf (ZENLOG, " self      the average number of seconds spent in this\n");
	fprintf (ZENLOG, "s/call     function per call, if this function is profiled,\n");
	fprintf (ZENLOG, "           else blank.\n\n");
	fprintf (ZENLOG, " total     the average number of seconds spent in this\n");
	fprintf (ZENLOG, " s/call    function and its descendents per call, if this \n");
	fprintf (ZENLOG, "           function is profiled, else blank.\n\n");
	fprintf (ZENLOG, "name       the name of the function.\n\n");
	
	if (zenprof.log)
		fclose (zenprof.log);
}

#else /* ENABLE_ZENPROFILER */

#define ZenProfilerInit(logfile)
#define ZenProfilerLazy(func) func
#define ZenProfilerStart(zstart)
#define ZenProfilerStop(zstop)
#define ZenProfilerReport(ztimer)
#define ZenProfiler_return(ztimer) return
#define ZenProfiler_return_val(ztimer, retval) return (retval)
#define ZenShutdown()

#endif /* ENABLE_ZENPROFILER */

#define zen(func) ZenProfilerLazy(func)

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __ZENPROFILER_H__ */
