/*
 * libss7: An implementation of Signalling System 7
 *
 * Written by Matthew Fredrickson <creslin@digium.com>
 *
 * scheduling routines taken from libpri by Mark Spencer
 *
 * Copyright (C) 2005, Digium, Inc
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA. 
 *
 */

#include "libss7.h"
#include "ss7_internal.h"
#include "mtp3.h"
#include <stdio.h>


/* Scheduler routines */
int ss7_schedule_event(struct ss7 *ss7, int ms, void (*function)(void *data), void *data)
{
	int x;
	struct timeval tv;
	for (x=1;x<MAX_SCHED;x++)
		if (!ss7->ss7_sched[x].callback)
			break;
	if (x == MAX_SCHED) {
		ss7_error(ss7, "No more room in scheduler\n");
		return -1;
	}
	gettimeofday(&tv, NULL);
	tv.tv_sec += ms / 1000;
	tv.tv_usec += (ms % 1000) * 1000;
	if (tv.tv_usec > 1000000) {
		tv.tv_usec -= 1000000;
		tv.tv_sec += 1;
	}
	ss7->ss7_sched[x].when = tv;
	ss7->ss7_sched[x].callback = function;
	ss7->ss7_sched[x].data = data;
	return x;
}

struct timeval *ss7_schedule_next(struct ss7 *ss7)
{
	struct timeval *closest = NULL;
	int x;
	/* Check subchannels */
	for (x=1;x<MAX_SCHED;x++) {
		if (ss7->ss7_sched[x].callback && 
			(!closest || (closest->tv_sec > ss7->ss7_sched[x].when.tv_sec) ||
				((closest->tv_sec == ss7->ss7_sched[x].when.tv_sec) && 
				 (closest->tv_usec > ss7->ss7_sched[x].when.tv_usec))))
				 	closest = &ss7->ss7_sched[x].when;
	}
	return closest;
}

static int __ss7_schedule_run(struct ss7 *ss7, struct timeval *tv)
{
	int x;
	void (*callback)(void *);
	void *data;
	for (x=1;x<MAX_SCHED;x++) {
		if (ss7->ss7_sched[x].callback &&
			((ss7->ss7_sched[x].when.tv_sec < tv->tv_sec) ||
			 ((ss7->ss7_sched[x].when.tv_sec == tv->tv_sec) &&
			  (ss7->ss7_sched[x].when.tv_usec <= tv->tv_usec)))) {
			  	callback = ss7->ss7_sched[x].callback;
				data = ss7->ss7_sched[x].data;
				ss7->ss7_sched[x].callback = NULL;
				ss7->ss7_sched[x].data = NULL;
				callback(data);
		}
	}
	return 0;
}

int ss7_schedule_run(struct ss7 *ss7)
{
	int res;

	struct timeval tv;
	gettimeofday(&tv, NULL);

	res =  __ss7_schedule_run(ss7, &tv);

	return res;
}

void ss7_schedule_del(struct ss7 *ss7, int *id)
{
	if ((*id >= MAX_SCHED)) 
		ss7_error(ss7, "Asked to delete sched id %d???\n", *id);

	if (*id < 0) /* Item already deleted */
		return;

	ss7->ss7_sched[*id].callback = NULL;
	*id = -1; /* "Delete" the event */
}
