/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  ZenTimer
 *  Copyright (C) 2001-2009 Jeffrey Stedfast
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


#ifndef __ZENTINER_H__
#define __ZENTIMER_H__

#ifdef ENABLE_ZENTIMER

#include <stdio.h>
#ifdef WINDOWS
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

#ifdef WINDOWS
static uint64_t ztimer_freq = 0;
#endif

static void
ztime (ztime_t *ztimep)
{
#ifdef WINDOWS
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

typedef uint32_t zstate_t;

typedef struct {
	zstate_t state; /* 32bit for alignment reasons */
	ztime_t start;
	ztime_t stop;
} ztimer_t;

#define ZTIMER_INITIALIZER { ZTIMER_INACTIVE, 0, 0 }

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
#ifdef WINDOWS
	static uint64_t freq = 0;
	ztime_t delta, stop;
	
	if (freq == 0)
		QueryPerformanceFrequency ((LARGE_INTEGER *) &freq);
#else
#define freq ZTIME_USEC_PER_SEC
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
