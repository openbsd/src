/* $OpenBSD: transport.c,v 1.24 2004/04/15 18:39:26 deraadt Exp $	 */
/* $EOM: transport.c,v 1.43 2000/10/10 12:36:39 provos Exp $	 */

/*
 * Copyright (c) 1998, 1999 Niklas Hallqvist.  All rights reserved.
 * Copyright (c) 2001 Håkan Olsson.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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
#include <string.h>

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

LIST_HEAD(transport_list, transport) transport_list;
LIST_HEAD(transport_method_list, transport_vtbl) transport_method_list;

/* Call the reinit function of the various transports.  */
	void
	                transport_reinit(void)
{
	struct transport_vtbl *method;

	for (method = LIST_FIRST(&transport_method_list); method;
	     method = LIST_NEXT(method, link))
		method->reinit();
}

/* Initialize the transport maintenance module.  */
void
transport_init(void)
{
	LIST_INIT(&transport_list);
	LIST_INIT(&transport_method_list);
}

/* Register another transport T.  */
void
transport_add(struct transport * t)
{
	LOG_DBG((LOG_TRANSPORT, 70, "transport_add: adding %p", t));
	TAILQ_INIT(&t->sendq);
	TAILQ_INIT(&t->prio_sendq);
	LIST_INSERT_HEAD(&transport_list, t, link);
	t->flags = 0;
	t->refcnt = 0;
}

/* Add a referer to transport T.  */
void
transport_reference(struct transport * t)
{
	t->refcnt++;
	LOG_DBG((LOG_TRANSPORT, 95,
	       "transport_reference: transport %p now has %d references", t,
		 t->refcnt));
}

/*
 * Remove a referer from transport T, removing all of T when no referers left.
 */
void
transport_release(struct transport * t)
{
	LOG_DBG((LOG_TRANSPORT, 95,
		 "transport_release: transport %p had %d references", t,
		 t->refcnt));
	if (--t->refcnt)
		return;

	LOG_DBG((LOG_TRANSPORT, 70, "transport_release: freeing %p", t));
	LIST_REMOVE(t, link);
	t->vtbl->remove(t);
}

void
transport_report(void)
{
	struct transport *t;
	struct message *msg;

	for (t = LIST_FIRST(&transport_list); t; t = LIST_NEXT(t, link)) {
		LOG_DBG((LOG_REPORT, 0,
		     "transport_report: transport %p flags %x refcnt %d", t,
			 t->flags, t->refcnt));

		t->vtbl->report(t);

		/*
		 * This is the reason message_dump_raw lives outside
		 * message.c.
		 */
		for (msg = TAILQ_FIRST(&t->prio_sendq); msg;
		     msg = TAILQ_NEXT(msg, link))
			message_dump_raw("udp_report", msg, LOG_REPORT);

		for (msg = TAILQ_FIRST(&t->sendq); msg; msg = TAILQ_NEXT(msg, link))
			message_dump_raw("udp_report", msg, LOG_REPORT);
	}
}

int
transport_prio_sendqs_empty(void)
{
	struct transport *t;

	for (t = LIST_FIRST(&transport_list); t; t = LIST_NEXT(t, link))
		if (TAILQ_FIRST(&t->prio_sendq))
			return 0;
	return 1;
}

/* Register another transport method T.  */
void
transport_method_add(struct transport_vtbl * t)
{
	LIST_INSERT_HEAD(&transport_method_list, t, link);
}

/* Apply a function FUNC on all registered transports.  */
void
transport_map(void (*func) (struct transport *))
{
	struct transport *t;

	for (t = LIST_FIRST(&transport_list); t; t = LIST_NEXT(t, link))
		(*func) (t);
}

/*
 * Build up a file descriptor set FDS with all transport descriptors we want
 * to read from.  Return the number of file descriptors select(2) needs to
 * check in order to cover the ones we setup in here.
 */
int
transport_fd_set(fd_set * fds)
{
	int             n;
	int             max = -1;
	struct transport *t;

	for (t = LIST_FIRST(&transport_list); t; t = LIST_NEXT(t, link))
		if (t->flags & TRANSPORT_LISTEN) {
			n = t->vtbl->fd_set(t, fds, 1);
			if (n > max)
				max = n;
		}
	return max + 1;
}

/*
 * Build up a file descriptor set FDS with all the descriptors belonging to
 * transport where messages are queued for transmittal.  Return the number
 * of file descriptors select(2) needs to check in order to cover the ones
 * we setup in here.
 */
int
transport_pending_wfd_set(fd_set * fds)
{
	int             n;
	int             max = -1;
	struct transport *t;

	for (t = LIST_FIRST(&transport_list); t; t = LIST_NEXT(t, link)) {
		if (TAILQ_FIRST(&t->sendq) || TAILQ_FIRST(&t->prio_sendq)) {
			n = t->vtbl->fd_set(t, fds, 1);
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
transport_handle_messages(fd_set * fds)
{
	struct transport *t;

	for (t = LIST_FIRST(&transport_list); t; t = LIST_NEXT(t, link))
		if ((t->flags & TRANSPORT_LISTEN) && (*t->vtbl->fd_isset) (t, fds))
			(*t->vtbl->handle_message) (t);
}

/*
 * Send the first queued message on the transports found whose file
 * descriptor is in FDS and has messages queued.  Remove the fd bit from
 * FDS as soon as one message has been sent on it so other transports
 * sharing the socket won't get service without an intervening select
 * call.  Perhaps a fairness strategy should be implemented between
 * such transports.  Now early transports in the list will potentially
 * be favoured to later ones sharing the file descriptor.
 */
void
transport_send_messages(fd_set * fds)
{
	struct transport *t, *next;
	struct message *msg;
	struct exchange *exchange;
	struct timeval  expiration;
	int             expiry, ok_to_drop_message;

	/*
	 * Reference all transports first so noone will disappear while in
	 * use.
	 */
	for (t = LIST_FIRST(&transport_list); t; t = LIST_NEXT(t, link))
		transport_reference(t);

	for (t = LIST_FIRST(&transport_list); t; t = LIST_NEXT(t, link)) {
		if ((TAILQ_FIRST(&t->sendq) || TAILQ_FIRST(&t->prio_sendq))
		    && t->vtbl->fd_isset(t, fds)) {
			t->vtbl->fd_set(t, fds, 0);

			/* Prefer a message from the prioritized sendq.  */
			if (TAILQ_FIRST(&t->prio_sendq)) {
				msg = TAILQ_FIRST(&t->prio_sendq);
				TAILQ_REMOVE(&t->prio_sendq, msg, link);
			} else {
				msg = TAILQ_FIRST(&t->sendq);
				TAILQ_REMOVE(&t->sendq, msg, link);
			}

			msg->flags &= ~MSG_IN_TRANSIT;
			exchange = msg->exchange;
			exchange->in_transit = 0;

			/*
			 * We disregard the potential error message here, hoping that the
			 * retransmit will go better.
			 * XXX Consider a retry/fatal error discriminator.
		         */
			t->vtbl->send_message(msg);
			msg->xmits++;

			/*
			 * This piece of code has been proven to be quite delicate.
			 * Think twice for before altering.  Here's an outline:
		         *
			 * If this message is not the one which finishes an exchange,
			 * check if we have reached the number of retransmit before
			 * queuing it up for another.
		         *
			 * If it is a finishing message we still may have to keep it
			 * around for an on-demand retransmit when seeing a duplicate
			 * of our peer's previous message.
		         *
			 * If we have no previous message from our peer, we need not
			 * to keep the message around.
		         */
			if ((msg->flags & MSG_LAST) == 0) {
				if (msg->xmits > conf_get_num("General", "retransmits",
						      RETRANSMIT_DEFAULT)) {
					log_print("transport_send_messages: giving up on "
					     "message %p, exchange %s", msg,
						  exchange->name ? exchange->name : "<unnamed>");
					/* Be more verbose here.  */
					if (exchange->phase == 1) {
						log_print("transport_send_messages: either this "
							  "message did not reach the other peer");
						if (exchange->initiator)
							log_print("transport_send_messages: or the response"
								  "message did not reach us back");
						else
							log_print("transport_send_messages: or this is "
								  "an attempted IKE scan");
					}
					exchange->last_sent = 0;
				} else {
					gettimeofday(&expiration, 0);

					/*
					 * XXX Calculate from round trip timings and a backoff func.
				         */
					expiry = msg->xmits * 2 + 5;
					expiration.tv_sec += expiry;
					LOG_DBG((LOG_TRANSPORT, 30,
						 "transport_send_messages: message %p "
						 "scheduled for retransmission %d in %d secs",
						 msg, msg->xmits, expiry));
					if (msg->retrans)
						timer_remove_event(msg->retrans);
					msg->retrans
						= timer_add_event("message_send_expire",
								  (void (*) (void *)) message_send_expire,
							  msg, &expiration);
					/*
					 * If we cannot retransmit, we
					 * cannot...
					 */
					exchange->last_sent = msg->retrans ? msg : 0;
				}
			} else
				exchange->last_sent = exchange->last_received ? msg : 0;

			/*
			 * If this message is not referred to for later retransmission
			 * it will be ok for us to drop it after the post-send function.
			 * But as the post-send function may remove the exchange, we need
			 * to remember this fact here.
		         */
			ok_to_drop_message = exchange->last_sent == 0;

			/*
			 * If this is not a retransmit call post-send functions that allows
			 * parallel work to be done while the network and peer does their
			 * share of the job.  Note that a post-send function may take
			 * away the exchange we belong to, but only if no retransmits
			 * are possible.
		         */
			if (msg->xmits == 1)
				message_post_send(msg);

			if (ok_to_drop_message)
				message_free(msg);
		}
	}

	for (t = LIST_FIRST(&transport_list); t; t = next) {
		next = LIST_NEXT(t, link);
		transport_release(t);
	}
}

/*
 * Textual search after the transport method denoted by NAME, then create
 * a transport connected to the peer with address ADDR, given in a transport-
 * specific string format.
 */
struct transport *
transport_create(char *name, char *addr)
{
	struct transport_vtbl *method;

	for (method = LIST_FIRST(&transport_method_list); method;
	     method = LIST_NEXT(method, link))
		if (strcmp(method->name, name) == 0)
			return (*method->create) (addr);
	return 0;
}
