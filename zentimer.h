/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2001-2004 Ximian, Inc. (www.ximian.com)
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

#ifndef __ZENTINER_H__
#define __ZENTIMER_H__

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#include <stdio.h>
#ifdef USE_FTIME
/* ftime() has been obsoleted by gettimeofday() */
#include <sys/timeb.h>
#else
#include <sys/time.h>
#endif /* USE_FTIME */

typedef struct {
	time_t sec;
	unsigned short msec;
} ztime_t;

typedef struct _ztimer_t {
	ztime_t start;
	ztime_t stop;
} ztimer_t;

#define ZTIMER_INITIALIZER { { 0, 0 }, { 0, 0 } }

#ifdef ENABLE_ZENTIMER

/* G_STMT_START and G_STMT_END stolen from glib.h */
/* Provide simple macro statement wrappers (adapted from Perl):
 *  G_STMT_START { statements; } G_STMT_END;
 *  can be used as a single statement, as in
 *  if (x) G_STMT_START { ... } G_STMT_END; else ...
 *
 *  For gcc we will wrap the statements within `({' and `})' braces.
 *  For SunOS they will be wrapped within `if (1)' and `else (void) 0',
 *  and otherwise within `do' and `while (0)'.
 */
#if !(defined (G_STMT_START) && defined (G_STMT_END))
#  if defined (__GNUC__) && !defined (__STRICT_ANSI__) && !defined (__cplusplus)
#    define G_STMT_START	(void)(
#    define G_STMT_END		)
#  else
#    if (defined (sun) || defined (__sun__))
#      define G_STMT_START	if (1)
#      define G_STMT_END	else (void)0
#    else
#      define G_STMT_START	do
#      define G_STMT_END	while (0)
#    endif
#  endif
#endif

#ifdef USE_FTIME
static void
ztime (ztime_t *ztimep)
{
	struct timeb tb;
	
	ftime (&tb);
	ztimep->sec = tb.time;
	ztimep->msec = tb.millitm;
}
#else /* use gettimeofday instead */
static void
ztime (ztime_t *ztimep)
{
	struct timezone tz;
	struct timeval tv;
	
	gettimeofday (&tv, &tz);
	ztimep->sec = (time_t) tv.tv_sec;
	ztimep->msec = (unsigned short) (tv.tv_usec > 1000 ? tv.tv_usec / 1000 : 0);
}
#endif /* USE_FTIME */

static void
ztime_diff (ztime_t start, ztime_t stop, ztime_t *diff)
{
	if (stop.msec < start.msec) {
		stop.msec += 1000;
		stop.sec -= 1;
	}
	
	diff->sec = stop.sec - start.sec;
	diff->msec = stop.msec - start.msec;
}

static ztimer_t zen_ztimer = ZTIMER_INITIALIZER;

#define ZenTimerStart() ztime(&zen_ztimer.start)
#define ZenTimerStop()  ztime(&zen_ztimer.stop)

#define ZenTimerReport(oper) G_STMT_START {                            \
	if (zen_ztimer.stop.msec < zen_ztimer.start.msec) {            \
		zen_ztimer.stop.msec += 1000;                          \
		zen_ztimer.stop.sec--;                                 \
	}                                                              \
	                                                               \
	fprintf (stderr, "ZenTimer: %s took %lu.%03d seconds\n", oper, \
		 zen_ztimer.stop.sec - zen_ztimer.start.sec,           \
		 zen_ztimer.stop.msec - zen_ztimer.start.msec);        \
} G_STMT_END

/* Thread-safe version */

#define ZenTimerMTStart(ztimer) (ztime (&ztimer.start))
#define ZenTimerMTStop(ztimer)  (ztime (&ztimer.stop))

#define ZenTimerMTReport(oper, ztimer) G_STMT_START {                  \
	if (ztimer.stop.msec < ztimer.start.msec) {                    \
		ztimer.stop.msec += 1000;                              \
		ztimer.stop.sec--;                                     \
	}                                                              \
	                                                               \
	fprintf (stderr, "ZenTimer: %s took %lu.%03d seconds\n", oper, \
		 ztimer.stop.sec - ztimer.start.sec,                   \
		 ztimer.stop.msec - ztimer.start.msec);                \
} G_STMT_END

#else

#define ztime(x)
#define ztime_diff(start, stop, diff)
#define ZenTimerStart()
#define ZenTimerStop()
#define ZenTimerReport(oper)
#define ZenTimerMTStart(ztimer)
#define ZenTimerMTStop(ztimer)
#define ZenTimerMTReport(oper, ztimer)

#endif /* ENABLE_ZENTIMER */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __ZENTIMER_H__ */
