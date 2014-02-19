/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  ZenTimer
 *  Copyright (C) 2000-2014 Jeffrey Stedfast
 *
 *  Permission is hereby granted, free of charge, to any person
 *  obtaining a copy of this software and associated documentation
 *  files (the "Software"), to deal in the Software without
 *  restriction, including without limitation the rights to use, copy,
 *  modify, merge, publish, distribute, sublicense, and/or sell copies
 *  of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be
 *  included in all copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 *  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 *  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 *  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 *  HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 *  WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 *  DEALINGS IN THE SOFTWARE.
 */


#ifndef __ZENTIMER_H__
#define __ZENTIMER_H__

#ifdef ENABLE_ZENTIMER

#include <stdio.h>
#ifdef WIN32
#include <windows.h>
#else
#include <sys/time.h>
#endif
#ifdef HAVE_STDINT_H
#include <stdint.h>
#elif HAVE_INTTYPES_H
#include <inttypes.h>
#else
typedef unsigned char uint8_t;
typedef unsigned long int uint32_t;
typedef unsigned long long uint64_t;
#endif

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#define ZTIME_USEC_PER_SEC 1000000

/* ztime_t represents usec */
typedef uint64_t ztime_t;

#ifdef WIN32
static uint64_t ztimer_freq = 0;
#endif

static void
ztime (ztime_t *ztimep)
{
#ifdef WIN32
	QueryPerformanceCounter ((LARGE_INTEGER *) ztimep);
#else
	struct timeval tv;
	
	gettimeofday (&tv, NULL);
	
	*ztimep = ((uint64_t) tv.tv_sec * ZTIME_USEC_PER_SEC) + tv.tv_usec;
#endif
}

enum {
	ZTIMER_INACTIVE = 0,
	ZTIMER_ACTIVE   = (1 << 0),
	ZTIMER_PAUSED   = (1 << 1),
};

typedef struct {
	ztime_t start;
	ztime_t stop;
	int state;
} ztimer_t;

#define ZTIMER_INITIALIZER { 0, 0, 0 }

/* default timer */
static ztimer_t __ztimer = ZTIMER_INITIALIZER;

static void
ZenTimerStart (ztimer_t *ztimer)
{
	ztimer = ztimer ? ztimer : &__ztimer;
	
	ztimer->state = ZTIMER_ACTIVE;
	ztime (&ztimer->start);
}

static void
ZenTimerStop (ztimer_t *ztimer)
{
	ztimer = ztimer ? ztimer : &__ztimer;
	
	ztime (&ztimer->stop);
	ztimer->state = ZTIMER_INACTIVE;
}

static void
ZenTimerPause (ztimer_t *ztimer)
{
	ztimer = ztimer ? ztimer : &__ztimer;
	
	ztime (&ztimer->stop);
	ztimer->state |= ZTIMER_PAUSED;
}

static void
ZenTimerResume (ztimer_t *ztimer)
{
	ztime_t now, delta;
	
	ztimer = ztimer ? ztimer : &__ztimer;
	
	/* unpause */
	ztimer->state &= ~ZTIMER_PAUSED;
	
	ztime (&now);
	
	/* calculate time since paused */
	delta = now - ztimer->stop;
	
	/* adjust start time to account for time elapsed since paused */
	ztimer->start += delta;
}

static double
ZenTimerElapsed (ztimer_t *ztimer, uint64_t *usec)
{
#ifdef WIN32
	static uint64_t freq = 0;
	ztime_t delta, stop;
	
	if (freq == 0)
		QueryPerformanceFrequency ((LARGE_INTEGER *) &freq);
#else
	static uint64_t freq = ZTIME_USEC_PER_SEC;
	ztime_t delta, stop;
#endif
	
	ztimer = ztimer ? ztimer : &__ztimer;
	
	if (ztimer->state != ZTIMER_ACTIVE)
		stop = ztimer->stop;
	else
		ztime (&stop);
	
	delta = stop - ztimer->start;
	
	if (usec != NULL)
		*usec = (uint64_t) (delta * ((double) ZTIME_USEC_PER_SEC / (double) freq));
	
	return (double) delta / (double) freq;
}

static void
ZenTimerReport (ztimer_t *ztimer, const char *oper)
{
	fprintf (stderr, "ZenTimer: %s took %.6f seconds\n", oper, ZenTimerElapsed (ztimer, NULL));
}

#ifdef __cplusplus
}
#endif /* __cplusplus */

#else /* ! ENABLE_ZENTIMER */

#define ZenTimerStart(ztimerp)
#define ZenTimerStop(ztimerp)
#define ZenTimerPause(ztimerp)
#define ZenTimerResume(ztimerp)
#define ZenTimerElapsed(ztimerp, usec)
#define ZenTimerReport(ztimerp, oper)

#endif /* ENABLE_ZENTIMER */

#endif /* __ZENTIMER_H__ */
