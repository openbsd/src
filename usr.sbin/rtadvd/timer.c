/*	$OpenBSD: timer.c,v 1.13 2014/11/02 02:45:46 deraadt Exp $	*/
/*	$KAME: timer.c,v 1.7 2002/05/21 14:26:55 itojun Exp $	*/

/*
 * Copyright (C) 1998 WIDE Project.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/queue.h>
#include <sys/time.h>

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include "timer.h"
#include "log.h"

SLIST_HEAD(, rtadvd_timer) timer_head = SLIST_HEAD_INITIALIZER(timer_head);

struct rtadvd_timer *
rtadvd_add_timer(void (*timeout)(void *),
		void (*update)(void *, struct timeval *),
		 void *timeodata, void *updatedata)
{
	struct rtadvd_timer *newtimer;

	if ((newtimer = calloc(1, sizeof(*newtimer))) == NULL)
		fatal("calloc");

	if (timeout == NULL)
		fatalx("timeout function unspecified");
	if (update == NULL)
		fatalx("update function unspecified");
	newtimer->expire = timeout;
	newtimer->update = update;
	newtimer->expire_data = timeodata;
	newtimer->update_data = updatedata;

	/* link into chain */
	SLIST_INSERT_HEAD(&timer_head, newtimer, entries);

	return(newtimer);
}

void
rtadvd_remove_timer(struct rtadvd_timer **timer)
{
	SLIST_REMOVE(&timer_head, *timer, rtadvd_timer, entries);
	free(*timer);
	*timer = NULL;
}

void
rtadvd_set_timer(struct timeval *tm, struct rtadvd_timer *timer)
{
	struct timeval now;

	/* reset the timer */
	gettimeofday(&now, NULL);

	timeradd(&now, tm, &timer->tm);
}

/*
 * Check expiration for each timer. If a timer is expired,
 * call the expire function for the timer and update the timer.
 * Return the next interval.
 */
struct timeval *
rtadvd_check_timer()
{
	static struct timeval returnval;
	struct timeval now;
	struct rtadvd_timer *tm;
	int timers;

	timers = 0;
	gettimeofday(&now, NULL);

	SLIST_FOREACH(tm, &timer_head, entries) {
		if (timercmp(&tm->tm, &now, <=)) {
			(*tm->expire)(tm->expire_data);
			(*tm->update)(tm->update_data, &tm->tm);
			timeradd(&tm->tm, &now, &tm->tm);
		}
		if (timers == 0 || timercmp(&tm->tm, &returnval, <))
			returnval = tm->tm;
		timers ++;
	}

	if (timers == 0) {
		/* no need to timeout */
		return(NULL);
	} else if (timercmp(&returnval, &now, <)) {
		/* this may occur when the interval is too small */
		timerclear(&returnval);
	} else
		timersub(&returnval, &now, &returnval);
	return(&returnval);
}

struct timeval *
rtadvd_timer_rest(struct rtadvd_timer *timer)
{
	static struct timeval returnval, now;

	gettimeofday(&now, NULL);
	if (timercmp(&timer->tm, &now, <=)) {
		log_debug("a timer must be expired, but not yet");
		timerclear(&returnval);
	}
	else
		timersub(&timer->tm, &now, &returnval);

	return(&returnval);
}
