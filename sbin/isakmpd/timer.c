/*	$OpenBSD: timer.c,v 1.2 1998/11/15 00:44:03 niklas Exp $	*/

/*
 * Copyright (c) 1998 Niklas Hallqvist.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Ericsson Radio Systems.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * This code was written under funding by Ericsson Radio Systems.
 */

#include <sys/queue.h>
#include <stdlib.h>
#include <string.h>

#include "log.h"
#include "timer.h"

static TAILQ_HEAD (event_list, event) events;

void
timer_init ()
{
  TAILQ_INIT (&events);
}

void
timer_next_event (struct timeval **timeout)
{
  struct timeval now;

  if (TAILQ_FIRST (&events))
    {
      gettimeofday (&now, 0);
      if (timercmp (&now, &TAILQ_FIRST (&events)->expiration, >=))
	timerclear (*timeout);
      else
	timersub (&TAILQ_FIRST (&events)->expiration, &now, *timeout);
    }
  else
    *timeout = 0;
}

void
timer_handle_expirations ()
{
  struct timeval now;
  struct event *n;

  gettimeofday (&now, 0);
  for (n = TAILQ_FIRST (&events); n && timercmp (&now, &n->expiration, >=);
       n = TAILQ_FIRST (&events))
    {
      log_debug (LOG_TIMER, 10,
		 "timer_handle_expirations: event %s(%p)", n->name, n->arg);
      TAILQ_REMOVE (&events, n, link);
      (*n->func) (n->arg);
      free (n);
    }
}

struct event *
timer_add_event (char *name, void (*func) (void *), void *arg,
		 struct timeval *expiration)
{
  struct event *ev = (struct event *)malloc (sizeof *ev);
  struct event *n;

  if (!ev)
    return 0;
  ev->name = name;
  ev->func = func;
  ev->arg = arg;
  memcpy (&ev->expiration, expiration, sizeof *expiration);
  for (n = TAILQ_FIRST (&events);
       n && timercmp (expiration, &n->expiration, >=);
       n = TAILQ_NEXT (n, link))
    ;
  if (n)
    {
      log_debug (LOG_TIMER, 10,
		 "timer_add_event: event %s(%p) added before %s(%p)", name,
		 arg, n->name, n->arg);
      TAILQ_INSERT_BEFORE (n, ev, link);
    }
  else
    {
      log_debug (LOG_TIMER, 10, "timer_add_event: event %s(%p) added last",
		 name, arg);
      TAILQ_INSERT_TAIL (&events, ev, link);
    }
  return ev;
}

void
timer_remove_event (struct event *ev)
{
  log_debug (LOG_TIMER, 10, "timer_remove_event: removing event %s(%p)",
	     ev->name, ev->arg);
  TAILQ_REMOVE (&events, ev, link);
  free (ev);
}
