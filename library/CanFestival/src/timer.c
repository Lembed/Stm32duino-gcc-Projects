/*
This file is part of CanFestival, a library implementing CanOpen Stack.

Copyright (C): Edouard TISSERANT and Francis DUPIN

See COPYING file for copyrights details.

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

/*
 * MD: Timer scheduling was modified to support periodic calling from
 * Systick handler on STM32.
 *
 * getElapsedTime semantic was changed to return timer period, instead of
 * difference between consequent calls.
 */

/*!
** @file   timer.c
** @author Edouard TISSERANT and Francis DUPIN
** @date   Tue Jun  5 09:32:32 2007
**
** @brief
**
**
*/

/* #define DEBUG_WAR_CONSOLE_ON */
/* #define DEBUG_ERR_CONSOLE_ON */

#include <applicfg.h>
#include "timer.h"

/*  ---------  The timer table --------- */
s_timer_entry timers[MAX_NB_TIMER] = {{TIMER_FREE, NULL, NULL, 0, 0, 0},};

TIMEVAL total_sleep_time = TIMEVAL_MAX;
TIMER_HANDLE last_timer_raw = -1;

#define min_val(a,b) ((a<b)?a:b)

/*!
** -------  Use this to declare a new alarm ------
**
** @param d
** @param id
** @param callback
** @param value
** @param period
**
** @return
**/
TIMER_HANDLE SetAlarm(CO_Data* d, UNS32 id, TimerCallback_t callback, TIMEVAL value, TIMEVAL period)
{
	TIMER_HANDLE row_number;
	s_timer_entry *row;

	/* in order to decide new timer setting we have to run over all timer rows */
	for (row_number = 0, row = timers; row_number <= last_timer_raw + 1 && row_number < MAX_NB_TIMER; row_number++, row++) {
		if (callback && 	/* if something to store */
		    row->state == TIMER_FREE) { /* and empty row */
			/* just store */
			TIMEVAL real_timer_value;
			TIMEVAL elapsed_time;

			if (row_number == last_timer_raw + 1) last_timer_raw++;

//			elapsed_time = getElapsedTime();
			/* set next wakeup alarm if new entry is sooner than others, or if it is alone */
			real_timer_value = value;
			real_timer_value = min_val(real_timer_value, TIMEVAL_MAX);

			/* MD: no need to reschedule system timer
						if (total_sleep_time > elapsed_time && total_sleep_time - elapsed_time > real_timer_value)
						{
							total_sleep_time = elapsed_time + real_timer_value;
							setTimer(real_timer_value);
						}
			*/
			row->callback = callback;
			row->d = d;
			row->id = id;
			row->val = real_timer_value;// + elapsed_time;
			row->interval = period;
			row->state = TIMER_ARMED;
			return row_number;
		}
	}

	return TIMER_NONE;
}

/*!
**  -----  Use this to remove an alarm ----
**
** @param handle
**
** @return
**/
TIMER_HANDLE DelAlarm(TIMER_HANDLE handle)
{
	/* Quick and dirty. system timer will continue to be trigged, but no action will be preformed. */
	MSG_WAR(0x3320, "DelAlarm. handle = ", handle);
	if (handle != TIMER_NONE) {
		if (handle == last_timer_raw)
			last_timer_raw--;
		timers[handle].state = TIMER_FREE;
	}
	return TIMER_NONE;
}

/*!
** ------  TimeDispatch is called on each timer expiration ----
**
**/
int tdcount = 0;
void TimeDispatch(void)
{
	TIMER_HANDLE i;

	TIMEVAL period = getTimerPeriod();

	s_timer_entry *row;

	for (i = 0, row = timers; i <= last_timer_raw; i++, row++) {
		if (row->state & TIMER_ARMED) { /* if row is active */
			if (row->val < period) { /* to be trigged */
				if (!row->interval) { /* if simply outdated */
					row->state = TIMER_TRIG; /* ask for trig */
				} else { /* or period have expired */
					/* set val as interval, with overrun correction */
					row->val = row->interval - (period % row->interval);
					row->state = TIMER_TRIG_PERIOD; /* ask for trig, periodic */
				}
			} else {
				/* Each armed timer value in decremented. */
				row->val -= period;
			}
		}
	}

	/* Then trig them or not. */
	for (i = 0, row = timers; i <= last_timer_raw; i++, row++) {
		if (row->state & TIMER_TRIG) {
			row->state &= ~TIMER_TRIG; /* reset trig state (will be free if not periodic) */
			if (row->callback)
				(*row->callback)(row->d, row->id); /* trig ! */
		}
	}
}
