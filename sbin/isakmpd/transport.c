/*	$OpenBSD: transport.c,v 1.5 1999/04/19 21:04:00 niklas Exp $	*/
/*	$EOM: transport.c,v 1.32 1999/04/13 20:00:42 ho Exp $	*/

/*
 * Copyright (c) 1998, 1999 Niklas Hallqvist.  All rights reserved.
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

#include <sys/param.h>
#include <sys/queue.h>

#include "sysdep.h"

#include "conf.h"
#include "exchange.h"
#include "log.h"
#include "message.h"
#include "sa.h"
#include "timer.h"
#include "transport.h"

/* If no retransmit limit is given, use this as a default.  */
#define RETRANSMIT_DEFAULT 10

LIST_HEAD (transport_list, transport) transport_list;
LIST_HEAD (transport_method_list, transport_vtbl) transport_method_list;

/* Initialize the transport maintenance module.  */
void
transport_init (void)
{
  LIST_INIT (&transport_list);
  LIST_INIT (&transport_method_list);
}

/* Register another transport T.  */
void
transport_add (struct transport *t)
{
  log_debug (LOG_TRANSPORT, 70, "transport_add: adding %p", t);
  TAILQ_INIT (&t->sendq);
  LIST_INSERT_HEAD (&transport_list, t, link);
  t->flags = 0;
  t->refcnt = 0;
}

/* Add a referer to transport T.  */
void
transport_reference (struct transport *t)
{
  t->refcnt++;
  log_debug (LOG_TRANSPORT, 90,
	     "transport_reference: transport %p now has %d references", t,
	     t->refcnt);
}

/*
 * Remove a referer from transport T, removing all of T when no referers left.
 */
void
transport_release (struct transport *t)
{
  log_debug (LOG_TRANSPORT, 90,
	     "transport_release: transport %p had %d references", t,
	     t->refcnt);
  if (--t->refcnt)
    return;

  log_debug (LOG_TRANSPORT, 70, "transport_release: freeing %p", t);
  LIST_REMOVE (t, link);
  t->vtbl->remove (t);
}

void
transport_report (void)
{
  struct transport *t;
  struct message *msg;

  for (t = LIST_FIRST (&transport_list); t; t = LIST_NEXT (t, link))
    { 
      log_debug (LOG_REPORT, 0, 
		 "transport_report: transport %p flags %x refcnt %d", t,
		 t->flags, t->refcnt);
      
      t->vtbl->report (t);
      
      /* This is the reason message_dump_raw lives outside message.c.  */
      for (msg = TAILQ_FIRST (&t->sendq); msg; msg = TAILQ_NEXT (msg, link))
        message_dump_raw("udp_report", msg, LOG_REPORT);
    }
}

/* Register another transport method T.  */
void
transport_method_add (struct transport_vtbl *t)
{
  LIST_INSERT_HEAD (&transport_method_list, t, link);
}

/* Apply a function FUNC on all registered transports.  */
void
transport_map (void (*func) (struct transport *))
{
  struct transport *t;

  for (t = LIST_FIRST (&transport_list); t; t = LIST_NEXT (t, link))
    (*func) (t);
}

/*
 * Build up a file desciptor set FDS with all transport descriptors we want
 * to read from.  Return the number of file descriptors select(2) needs to
 * check in order to cover the ones we setup in here.
 */
int
transport_fd_set (fd_set *fds)
{
  int n;
  int max = -1;
  struct transport *t;

  for (t = LIST_FIRST (&transport_list); t; t = LIST_NEXT (t, link))
    if (t->flags & TRANSPORT_LISTEN)
      {
	n = t->vtbl->fd_set (t, fds, 1);
	if (n > max)
	  max = n;
      }
  return max + 1;
}

/*
 * Build up a file desciptor set FDS with all the descriptors belonging to
 * transport where messages are queued for transmittal.  Return the number
 * of file descriptors select(2) needs to check in order to cover the ones
 * we setup in here.
 */
int
transport_pending_wfd_set (fd_set *fds)
{
  int n;
  int max = -1;
  struct transport *t;

  for (t = LIST_FIRST (&transport_list); t; t = LIST_NEXT (t, link))
    {
      if (TAILQ_FIRST (&t->sendq))
	{
	  n = t->vtbl->fd_set (t, fds, 1);
	  if (n > max)
	    max = n;
	}
    }
  return max + 1;
}

/*
 * For each transport with a file descriptor in FDS, try to get an
 * incoming message and start processing it.
 */
void
transport_handle_messages (fd_set *fds)
{
  struct transport *t;

  for (t = LIST_FIRST (&transport_list); t; t = LIST_NEXT (t, link))
    if ((t->flags & TRANSPORT_LISTEN) && (*t->vtbl->fd_isset) (t, fds))
      (*t->vtbl->handle_message) (t);
}

/*
 * Send the first queued message on the first transport found whose file
 * descriptor is in FDS and has messages queued.  For fairness always try
 * the transport right after the last one which got a message sent on it.
 * XXX Would this perhaps be nicer done with CIRCLEQ chaining?
 */
void
transport_send_messages (fd_set *fds)
{
  struct transport *t = 0;
  struct message *msg;
  struct timeval expiration;
  struct sa *sa, *next_sa;
  int expiry;

  for (t = LIST_FIRST (&transport_list); t; t = LIST_NEXT (t, link))
    if (TAILQ_FIRST (&t->sendq) && t->vtbl->fd_isset (t, fds))
      {
	t->vtbl->fd_set (t, fds, 0);
	msg = TAILQ_FIRST (&t->sendq);
	msg->flags &= ~MSG_IN_TRANSIT;
	TAILQ_REMOVE (&t->sendq, msg, link);

	/*
	 * We disregard the potential error message here, hoping that the
	 * retransmit will go better.
	 * XXX Consider a retry/fatal error discriminator.
	 */
	t->vtbl->send_message (msg);

	/*
	 * If this is not a retransmit call post-send functions that allows
	 * parallel work to be done while the network and peer does their
	 * share of the job.
	 */
	if (msg->xmits == 0)
	  message_post_send (msg);

	msg->xmits++;

	if ((msg->flags & MSG_NO_RETRANS) == 0)
	  {
	    /* XXX make this a configurable parameter.  */
	    if (msg->xmits
		> conf_get_num ("General", "retransmits", RETRANSMIT_DEFAULT))
	      {
		log_print ("transport_send_messages: giving up on message %p",
			   msg);
		msg->exchange->last_sent = 0;

		/*
		 * As this exchange never went to a normal end, remove the
		 * SA's being negotiated too.
		 */
		for (sa = TAILQ_FIRST (&msg->exchange->sa_list); sa;
		     sa = next_sa)
		  {
		    next_sa = TAILQ_NEXT (sa, next);
		    sa_free (sa);
		  }

		exchange_free (msg->exchange);
		message_free (msg);
		continue;
	      };

	    gettimeofday (&expiration, 0);
	    /* XXX Calculate from round trip timings and a backoff func.  */
	    expiry = msg->xmits * 2 + 5;
	    expiration.tv_sec += expiry;
	    log_debug (LOG_TRANSPORT, 30,
		       "transport_send_messages: "
		       "message %p scheduled for retransmission %d in %d secs",
		       msg, msg->xmits, expiry);
	    msg->retrans = timer_add_event ("message_send",
					    (void (*) (void *))message_send,
					    msg, &expiration);
	    if (!msg->retrans)
	      {
		/* If we can make no retransmission, we can't.... */
		message_free (msg);
		return;
	      }

	    msg->exchange->last_sent = msg;
	  }
	else if ((msg->flags & MSG_KEEP) == 0)
	  message_free (msg);
      }
}

/*
 * Textual search after the transport method denoted by NAME, then create
 * a transport connected to the peer with address ADDR, given in a transport-
 * specific string format.
 */
struct transport *
transport_create (char *name, char *addr)
{
  struct transport_vtbl *method;

  for (method = LIST_FIRST (&transport_method_list); method;
       method = LIST_NEXT (method, link))
    if (strcmp (method->name, name) == 0)
      return (*method->create) (addr);
  return 0;
}
