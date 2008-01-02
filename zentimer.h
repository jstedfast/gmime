/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  ZenTimer
 *  Copyright (C) 2001-2008 Jeffrey Stedfast
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
#include <sys/time.h>
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

static void
ztime (ztime_t *ztimep)
{
	struct timeval tv;
	
	gettimeofday (&tv, NULL);
	
	*ztimep = ((uint64_t) tv.tv_sec * ZTIME_USEC_PER_SEC) + tv.tv_usec;
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

static void
ZenTimerReport (ztimer_t *ztimer, const char *oper)
{
	ztime_t delta;
	int paused;
	
	ztimer = ztimer ? ztimer : &__ztimer;
	
	if (ztimer->state == ZTIMER_ACTIVE) {
		ZenTimerPause (ztimer);
		paused = 1;
	} else {
		paused = 0;
	}
	
	delta = ztimer->stop - ztimer->start;
	
	fprintf (stderr, "ZenTimer: %s took %.6f seconds\n", oper,
		 (double) delta / (double) ZTIME_USEC_PER_SEC);
	
	if (paused)
		ZenTimerResume (ztimer);
}

#ifdef __cplusplus
}
#endif /* __cplusplus */

#else /* ! ENABLE_ZENTIMER */

#define ZenTimerStart(ztimerp)
#define ZenTimerStop(ztimerp)
#define ZenTimerPause(ztimerp)
#define ZenTimerResume(ztimerp)
#define ZenTimerReport(ztimerp, oper)

#endif /* ENABLE_ZENTIMER */

#endif /* __ZENTIMER_H__ */
