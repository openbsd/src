/*  This file is part of the program psim.

    Copyright (C) 1994-1995, Andrew Cagney <cagney@highland.com.au>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
 
    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 
    */


#ifndef _EVENTS_C_
#define _EVENTS_C_

#include "basics.h"
#include "events.h"


/* The event queue maintains a single absolute time using two
   variables.
   
   TIME_OF_EVENT: this holds the time at which the next event is ment
   to occure.  If no next event it will hold the time of the last
   event.  The first event occures at time 0 - system start.

   TIME_FROM_EVENT: The current distance from TIME_OF_EVENT.  If an
   event is pending, this will be positive.  If no future event is
   pending this will be negative.  This variable is decremented once
   for each iteration of a clock cycle.

   Clearly there is a bug in that this code assumes that the absolute
   time counter will never become greater than 2^62. */

typedef struct _event_entry event_entry;
struct _event_entry {
  void *data;
  event_handler *handler;
  signed64 time_of_event;  
  event_entry *next;
};

struct _event_queue {
  event_entry *queue;
  event_entry *volatile held;
  event_entry *volatile *volatile held_end;
  signed64 time_of_event;
  signed64 time_from_event;
};


INLINE_EVENTS\
(event_queue *)
event_queue_create(void)
{
  event_queue *new_event_queue = ZALLOC(event_queue);

  new_event_queue->queue = NULL;
  new_event_queue->held = NULL;
  new_event_queue->held_end = &new_event_queue->held;
  /* both times are already zero */
  return new_event_queue;
}


INLINE_EVENTS\
(void)
event_queue_init(event_queue *queue)
{
  event_entry *event;

  /* drain the interrupt queue */
  /*-LOCK-*/
  event = queue->held;
  while (event != NULL) {
    event_entry *dead = event;
    event = event->next;
    zfree(dead);
  }
  queue->held = NULL;
  queue->held_end = &queue->held;
  /*-UNLOCK-*/

  /* drain the normal queue */
  event = queue->queue;
  while (event != NULL) {
    event_entry *dead = event;
    event = event->next;
    zfree(dead);
  }
  queue->queue = NULL;
    
  /* wind time back to one */
  queue->time_of_event = 0;
  queue->time_from_event = -1;
}

INLINE_EVENTS\
(signed64)
event_queue_time(event_queue *queue)
{
  return queue->time_of_event - queue->time_from_event;
}

STATIC_INLINE_EVENTS\
(void)
update_time_from_event(event_queue *events)
{
  signed64 current_time = event_queue_time(events);
  if (events->queue != NULL) {
    events->time_from_event = (events->queue->time_of_event - current_time);
    events->time_of_event = events->queue->time_of_event;
  }
  else {
    events->time_of_event = current_time - 1;
    events->time_from_event = -1;
  }
  ASSERT(current_time == event_queue_time(events));
  ASSERT((events->time_from_event >= 0) == (events->queue != NULL));
}

STATIC_INLINE_EVENTS\
(void)
insert_event_entry(event_queue *events,
		   event_entry *new_event,
		   signed64 delta)
{
  event_entry *curr;
  event_entry **last;
  signed64 time_of_event;

  if (delta < 0)
    error("what is past is past!\n");

  /* compute when the event should occure */
  time_of_event = event_queue_time(events) + delta;

  /* find the queue insertion point - things are time ordered */
  last = &events->queue;
  curr = events->queue;
  while (curr != NULL && time_of_event >= curr->time_of_event) {
    last = &curr->next;
    curr = curr->next;
  }

  /* insert it */
  new_event->next = curr;
  *last = new_event;
  new_event->time_of_event = time_of_event;

  /* adjust the time until the first event */
  update_time_from_event(events);
}

INLINE_EVENTS\
(event_entry_tag)
event_queue_schedule(event_queue *events,
		     signed64 delta_time,
		     event_handler *handler,
		     void *data)
{
  event_entry *new_event = ZALLOC(event_entry);
  new_event->data = data;
  new_event->handler = handler;
  insert_event_entry(events, new_event, delta_time);
  return (event_entry_tag)new_event;
}


INLINE_EVENTS\
(event_entry_tag)
event_queue_schedule_after_signal(event_queue *events,
				  signed64 delta_time,
				  event_handler *handler,
				  void *data)
{
  event_entry *new_event = ZALLOC(event_entry);

  new_event->data = data;
  new_event->handler = handler;
  new_event->time_of_event = delta_time; /* work it out later */
  new_event->next = NULL;

  /*-LOCK-*/
  if (events->held == NULL) {
    events->held = new_event;
  }
  else {
    *events->held_end = new_event;
  }
  events->held_end = &new_event->next;
  /*-UNLOCK-*/

  return (event_entry_tag)new_event;
}


INLINE_EVENTS\
(void)
event_queue_deschedule(event_queue *events,
		       event_entry_tag event_to_remove)
{
  event_entry *to_remove = (event_entry*)event_to_remove;
  ASSERT((events->time_from_event >= 0) == (events->queue != NULL));
  if (event_to_remove != NULL) {
    event_entry *current;
    event_entry **ptr_to_current;
    for (ptr_to_current = &events->queue, current = *ptr_to_current;
	 current != NULL && current != to_remove;
	 ptr_to_current = &current->next, current = *ptr_to_current);
    if (current == to_remove) {
      *ptr_to_current = current->next;
      zfree(current);
      update_time_from_event(events);
    }
  }
  ASSERT((events->time_from_event >= 0) == (events->queue != NULL));
}




INLINE_EVENTS\
(int)
event_queue_tick(event_queue *events)
{
  signed64 time_from_event;

  /* remove things from the asynchronous event queue onto the real one */
  if (events->held != NULL) {
    event_entry *held_events;
    event_entry *curr_event;

    /*-LOCK-*/
    held_events = events->held;
    events->held = NULL;
    events->held_end = &events->held;
    /*-UNLOCK-*/

    do {
      curr_event = held_events;
      held_events = curr_event->next;
      insert_event_entry(events, curr_event, curr_event->time_of_event);
    } while (held_events != NULL);
  }

  /* advance time, checking to see if we've reached time zero which
     would indicate the time for the next event has arrived */
  time_from_event = events->time_from_event;
  events->time_from_event = time_from_event - 1;
  return time_from_event == 0;
}

INLINE_EVENTS\
(void)
event_queue_process(event_queue *events)
{
  ASSERT(events->time_from_event == -1); /* was just zero */
  ASSERT(events->queue != NULL); /* something to do */
  /* consume all events for this or earlier times.  Be careful to
     allow a new event to appear under our feet */
  do {
    event_entry *to_do = events->queue;
    events->queue = to_do->next;
    to_do->handler(to_do->data);
    zfree(to_do);
  } while (events->queue != NULL
	   && events->queue->time_of_event <= events->time_of_event);
  /* re-caculate time for new events */
  update_time_from_event(events);
  ASSERT(events->time_from_event != 0);
}


#endif /* _EVENTS_C_ */
