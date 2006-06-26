/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  ZenTimer
 *  Copyright (C) 2001-2006 Jeffrey Stedfast
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
 */


#ifndef __ZENTINER_H__
#define __ZENTIMER_H__

#ifdef ENABLE_ZENTIMER

#include <stdio.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

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

#define ZTIME_USEC_PER_SEC 1000000

typedef struct {
	uint32_t sec;
	uint32_t usec;
} ztime_t;

#ifdef G_OS_WIN32
#include <windows.h>

static void
ztime (ztime_t *ztimep)
{
	uint64_t systime;
	
	/* systime represents the number of 100-nanoseconds since Jan 1, 1601 */
	GetSystemTimeAsFileTime ((FILETIME *) &systime);
	
	/* convert to microseconds */
	systime /= 10;
	
	ztimep->sec = systime / ZTIME_USEC_PER_SEC;
	ztimep->usec = systime % ZTIME_USEC_PER_SEC;
}

#else /* POSIX OS */
#include <sys/time.h>

static void
ztime (ztime_t *ztimep)
{
	struct timeval tv;
	
	gettimeofday (&tv, NULL);
	ztimep->sec = tv.tv_sec;
	ztimep->usec = tv.tv_usec;
	
	if (ztimep->usec >= ZTIME_USEC_PER_SEC) {
		/* this is probably unneccessary */
		ztimep->sec += ztimep->usec / ZTIME_USEC_PER_SEC;
		ztimep->usec = ztimep->usec % ZTIME_USEC_PER_SEC;
	}
}

#endif /* OS */

static void
ztime_add (ztime_t *ztime, ztime_t *adj)
{
	ztime->sec += adj->sec;
	ztime->usec += adj->usec;
	ztime->sec += ztime->usec / ZTIME_USEC_PER_SEC;
	ztime->usec = ztime->usec % ZTIME_USEC_PER_SEC;
}

static void
ztime_delta (ztime_t *start, ztime_t *stop, ztime_t *delta)
{
	delta->sec = stop->sec - start->sec;
	if (stop->usec < start->usec) {
		delta->usec = (stop->usec + ZTIME_USEC_PER_SEC) - start->usec;
		delta->sec--;
	} else {
		delta->usec = stop->usec - start->usec;
	}
}

enum {
	ZTIMER_INACTIVE = 0,
	ZTIMER_ACTIVE   = (1 << 0),
	ZTIMER_PAUSED   = (1 << 1),
};

typedef uint8_t zstate_t;

typedef struct {
	zstate_t state;
	ztime_t start;
	ztime_t stop;
} ztimer_t;

#define ZTIMER_INITIALIZER { ZTIMER_INACTIVE, { 0, 0 }, { 0, 0 } }

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
	
	/* calculate time since paused */
	ztime (&now);
	ztime_delta (&ztimer->stop, &now, &delta);
	
	/* adjust start time to account for time elapsed since paused */
	ztime_add (&ztimer->start, &delta);
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
	
	ztime_delta (&ztimer->start, &ztimer->stop, &delta);
	
	fprintf (stderr, "ZenTimer: %s took %u.%06u seconds\n", oper, delta.sec, delta.usec);
	
	if (paused)
		ZenTimerResume (ztimer);
}

#ifdef __cplusplus
}
#endif /* __cplusplus */

#else /* ! ENABLE_ZENTIMER */

#define ZenTimerStart(ztimerp)
#define ZenTimerStop(ztimerp)
#define ZenTimerReport(ztimerp, oper)

#endif /* ENABLE_ZENTIMER */

#endif /* __ZENTIMER_H__ */
