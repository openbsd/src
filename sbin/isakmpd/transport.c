/*	$OpenBSD: transport.c,v 1.2 1998/11/15 00:44:03 niklas Exp $	*/

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

#include <sys/param.h>
#include <sys/queue.h>

#include "conf.h"
#include "exchange.h"
#include "log.h"
#include "message.h"
#include "sa.h"
#include "timer.h"
#include "transport.h"

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
  TAILQ_INIT (&t->sendq);
  LIST_INSERT_HEAD (&transport_list, t, link);
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
	n = (*t->vtbl->fd_set) (t, fds);
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
	  n = (*t->vtbl->fd_set) (t, fds);
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
 * Send the first queued message on the transports whose file descriptor
 * is in FDS.
 */
void
transport_send_messages (fd_set *fds)
{
  struct transport *t;
  struct message *msg;
  struct timeval expiration;
  struct sa *sa, *next_sa;
  int expiry;

  for (t = LIST_FIRST (&transport_list); t; t = LIST_NEXT (t, link))
    if (TAILQ_FIRST (&t->sendq) && (*t->vtbl->fd_isset) (t, fds))
      {
	msg = TAILQ_FIRST (&t->sendq);
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
	    if (msg->xmits > conf_get_num ("General", "retransmits"))
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
		return;
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
