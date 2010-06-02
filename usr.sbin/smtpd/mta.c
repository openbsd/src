/*	$OpenBSD: mta.c,v 1.92 2010/06/02 19:16:53 chl Exp $	*/

/*
 * Copyright (c) 2008 Pierre-Yves Ritschard <pyr@openbsd.org>
 * Copyright (c) 2008 Gilles Chehade <gilles@openbsd.org>
 * Copyright (c) 2009-2010 Jacek Masiulaniec <jacekm@dobremiasto.net>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/tree.h>
#include <sys/param.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <errno.h>
#include <event.h>
#include <netdb.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "smtpd.h"
#include "client.h"
#include "queue_backend.h"

void			 mta_imsg(struct smtpd *, struct imsgev *, struct imsg *);

__dead void		 mta_shutdown(void);
void			 mta_sig_handler(int, short, void *);

struct mta_session	*mta_lookup(struct smtpd *, u_int32_t);
void			 mta_enter_state(struct mta_session *, int, void *);
void			 mta_pickup(struct mta_session *, void *);
void			 mta_event(int, short, void *);

void			 mta_status(struct mta_session *, const char *, ...);
void			 mta_rcpt_status(struct recipient *, char *);
void			 mta_rcpt_log(struct mta_session *, struct recipient *);
void			 mta_rcpt_done(struct mta_session *, struct recipient *);
void			 mta_connect_done(int, short, void *);

void
mta_imsg(struct smtpd *env, struct imsgev *iev, struct imsg *imsg)
{
	struct aux		 aux;
	struct mta_session	*s;
	struct recipient	*rcpt;
	struct mta_relay	*relay;
	struct action		*action;
	struct dns		*dns;
	struct ssl		*ssl;

	if (iev->proc == PROC_QUEUE) {
		switch (imsg->hdr.type) {
		case IMSG_BATCH_CREATE:
			if (imsg->fd < 0)
				fatalx("mta: fd pass fail");
			s = calloc(1, sizeof *s);
			if (s == NULL)
				fatal(NULL);
			TAILQ_INIT(&s->recipients);
			TAILQ_INIT(&s->relays);
			s->id = imsg->hdr.peerid;
			s->state = MTA_INIT;
			s->env = env;
			s->datafd = imsg->fd;
			memcpy(&s->content_id, imsg->data,
			    sizeof s->content_id);
			SPLAY_INSERT(mtatree, &env->mta_sessions, s);
			return;

		case IMSG_BATCH_APPEND:
			action = imsg->data;
			s = mta_lookup(env, imsg->hdr.peerid);
			if (s->auxraw == NULL) {
				/*
				 * XXX: queue can batch together actions with
				 * different relay params.
				 */
				s->auxraw = strdup(action->data);
				if (s->auxraw == NULL)
					fatal(NULL);
				auxsplit(&s->aux, s->auxraw);
			}
			auxsplit(&aux, action->data);
			rcpt = malloc(sizeof *rcpt + strlen(aux.rcpt));
			if (rcpt == NULL)
				fatal(NULL);
			rcpt->action_id = action->id;
			strlcpy(rcpt->address, aux.rcpt, strlen(aux.rcpt) + 1);
			strlcpy(rcpt->status, "000 init", sizeof rcpt->status);
 			TAILQ_INSERT_TAIL(&s->recipients, rcpt, entry);
			return;

		case IMSG_BATCH_CLOSE:
			s = mta_lookup(env, imsg->hdr.peerid);
			memcpy(&s->birth, imsg->data, sizeof s->birth);
			mta_pickup(s, NULL);
			return;
		}
	}

	if (iev->proc == PROC_LKA) {
		switch (imsg->hdr.type) {
		case IMSG_LKA_SECRET:
			mta_pickup(mta_lookup(env, imsg->hdr.peerid), imsg->data);
			return;

		case IMSG_DNS_A:
			dns = imsg->data;
			s = mta_lookup(env, dns->id);
			relay = calloc(1, sizeof *relay);
			if (relay == NULL)
				fatal(NULL);
			relay->sa = dns->ss;
 			TAILQ_INSERT_TAIL(&s->relays, relay, entry);
			return;

		case IMSG_DNS_A_END:
			dns = imsg->data;
			mta_pickup(mta_lookup(env, dns->id), &dns->error);
			return;

		case IMSG_DNS_PTR:
			dns = imsg->data;
			s = mta_lookup(env, dns->id);
			relay = TAILQ_FIRST(&s->relays);
			if (dns->error)
				strlcpy(relay->fqdn, "<unknown>",
				    sizeof relay->fqdn);
			else
				strlcpy(relay->fqdn, dns->host,
				    sizeof relay->fqdn);
			mta_pickup(s, NULL);
			return;
		}
	}

	if (iev->proc == PROC_PARENT) {
		switch (imsg->hdr.type) {
		case IMSG_CONF_START:
			if (env->sc_flags & SMTPD_CONFIGURING)
				return;
			env->sc_flags |= SMTPD_CONFIGURING;
			env->sc_ssl = calloc(1, sizeof *env->sc_ssl);
			if (env->sc_ssl == NULL)
				fatal(NULL);
			return;

		case IMSG_CONF_SSL:
			if (!(env->sc_flags & SMTPD_CONFIGURING))
				return;
			ssl = calloc(1, sizeof *ssl);
			if (ssl == NULL)
				fatal(NULL);
			*ssl = *(struct ssl *)imsg->data;
			ssl->ssl_cert = strdup((char *)imsg->data +
			     sizeof *ssl);
			if (ssl->ssl_cert == NULL)
				fatal(NULL);
			ssl->ssl_key = strdup((char *)imsg->data +
			    sizeof *ssl + ssl->ssl_cert_len);
			if (ssl->ssl_key == NULL)
				fatal(NULL);
			SPLAY_INSERT(ssltree, env->sc_ssl, ssl);
			return;

		case IMSG_CONF_END:
			if (!(env->sc_flags & SMTPD_CONFIGURING))
				return;
			env->sc_flags &= ~SMTPD_CONFIGURING;
			return;

		case IMSG_CTL_VERBOSE:
			log_verbose(*(int *)imsg->data);
			return;
		}
	}

	fatalx("mta_imsg: unexpected imsg");
}

void
mta_sig_handler(int sig, short event, void *p)
{
	switch (sig) {
	case SIGINT:
	case SIGTERM:
		mta_shutdown();
		break;
	default:
		fatalx("mta_sig_handler: unexpected signal");
	}
}

void
mta_shutdown(void)
{
	log_info("mail transfer agent exiting");
	_exit(0);
}

pid_t
mta(struct smtpd *env)
{
	pid_t		 pid;

	struct passwd	*pw;
	struct event	 ev_sigint;
	struct event	 ev_sigterm;

	struct peer peers[] = {
		{ PROC_PARENT,	imsg_dispatch },
		{ PROC_QUEUE,	imsg_dispatch },
		{ PROC_LKA,	imsg_dispatch }
	};

	switch (pid = fork()) {
	case -1:
		fatal("mta: cannot fork");
	case 0:
		break;
	default:
		return (pid);
	}

	ssl_init();
	purge_config(env, PURGE_EVERYTHING);

	pw = env->sc_pw;
	if (chroot(pw->pw_dir) == -1)
		fatal("mta: chroot");
	if (chdir("/") == -1)
		fatal("mta: chdir(\"/\")");

	smtpd_process = PROC_MTA;
	setproctitle("%s", env->sc_title[smtpd_process]);

	if (setgroups(1, &pw->pw_gid) ||
	    setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) ||
	    setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid))
		fatal("mta: cannot drop privileges");

	imsg_callback = mta_imsg;
	event_init();

	signal_set(&ev_sigint, SIGINT, mta_sig_handler, env);
	signal_set(&ev_sigterm, SIGTERM, mta_sig_handler, env);
	signal_add(&ev_sigint, NULL);
	signal_add(&ev_sigterm, NULL);
	signal(SIGPIPE, SIG_IGN);
	signal(SIGHUP, SIG_IGN);

	config_pipes(env, peers, nitems(peers));
	config_peers(env, peers, nitems(peers));

	SPLAY_INIT(&env->mta_sessions);

	if (event_dispatch() < 0)
		fatal("event_dispatch");
	mta_shutdown();

	return (0);
}

int
mta_session_cmp(struct mta_session *a, struct mta_session *b)
{
	return (a->id < b->id ? -1 : a->id > b->id);
}

struct mta_session *
mta_lookup(struct smtpd *env, u_int32_t id)
{
	struct mta_session	 key, *res;

	key.id = id;
	res = SPLAY_FIND(mtatree, &env->mta_sessions, &key);
	if (res == NULL)
		fatalx("mta_lookup: session not found");
	return res;
}

void
mta_enter_state(struct mta_session *s, int newstate, void *p)
{
	struct mta_relay	*relay;
	struct sockaddr		*sa;
	struct recipient	*rcpt;
	struct smtp_client	*pcb;
	char			*host;
	int			 max_reuse;

	s->state = newstate;

	switch (s->state) {
	case MTA_SECRET:
		/*
		 * Lookup AUTH secret.
		 */
		if (s->aux.relay_via[0])
			host = s->aux.relay_via;
		else {
			rcpt = TAILQ_FIRST(&s->recipients);
			host = strchr(rcpt->address, '@') + 1;
		}
		imsg_compose_event(s->env->sc_ievs[PROC_LKA], IMSG_LKA_SECRET,
		    s->id, 0, -1, host, strlen(host) + 1);
		break;

	case MTA_MX:
		/*
		 * Lookup MX record.
		 */
		if (s->aux.relay_via[0])
			host = s->aux.relay_via;
		else {
			rcpt = TAILQ_FIRST(&s->recipients);
			host = strchr(rcpt->address, '@') + 1;
		}
		dns_query_mx(s->env, host, 0, s->id);
		break;

	case MTA_CONNECT:
		/*
		 * Connect to the MX.
		 */
		if (strcmp(s->aux.ssl, "ssl") == 0)
			max_reuse = 2;
		else
			max_reuse = 1;

		/* pick next mx */
		while ((relay = TAILQ_FIRST(&s->relays))) {
			if (relay->used == max_reuse) {
				TAILQ_REMOVE(&s->relays, relay, entry);
				free(relay);
				continue;
			}
			relay->used++;

			log_debug("mta: connect %s", ss_to_text(&relay->sa));
			sa = (struct sockaddr *)&relay->sa;

			if (s->aux.port[0])
				sa_set_port(sa, s->aux.port);
			else if (strcmp(s->aux.ssl, "ssl") == 0 &&
			    relay->used == 1)
				sa_set_port(sa, "465");
			else if (strcmp(s->aux.ssl, "smtps") == 0)
				sa_set_port(sa, "465");
			else
				sa_set_port(sa, "25");

			s->fd = socket(sa->sa_family, SOCK_STREAM, 0);
			if (s->fd == -1)
				fatal("socket");
			session_socket_blockmode(s->fd, BM_NONBLOCK);
			session_socket_no_linger(s->fd);

			if (connect(s->fd, sa, sa->sa_len) == -1) {
				if (errno != EINPROGRESS) {
					mta_status(s, "110 connect: %s",
					    strerror(errno));
					close(s->fd);
					continue;
				}
			}
			event_once(s->fd, EV_WRITE, mta_connect_done, s, NULL);
			break;
		}

		/* tried them all? */
		if (relay == NULL)
			mta_enter_state(s, MTA_DONE, NULL);
		break;

	case MTA_PTR:
		/*
		 * Lookup PTR record of the connected host.
		 */
		relay = TAILQ_FIRST(&s->relays);
		dns_query_ptr(s->env, &relay->sa, s->id);
		break;

	case MTA_PROTOCOL:
		/*
		 * Start protocol engine.
		 */
		log_debug("mta: entering smtp phase");

		pcb = client_init(s->fd, s->datafd, s->env->sc_hostname, 1);

		/* lookup SSL certificate */
		if (s->aux.cert[0]) {
			struct ssl key, *res;

			strlcpy(key.ssl_name, s->aux.cert, sizeof key.ssl_name);
			res = SPLAY_FIND(ssltree, s->env->sc_ssl, &key);
			if (res == NULL) {
				client_close(pcb);
				mta_status(s, "190 certificate not found");
				mta_enter_state(s, MTA_DONE, NULL);
				break;
			}
			client_certificate(pcb,
			    res->ssl_cert, res->ssl_cert_len,
			    res->ssl_key, res->ssl_key_len);
		}

		/* choose SMTPS vs. STARTTLS */
		relay = TAILQ_FIRST(&s->relays);
		if (strcmp(s->aux.ssl, "ssl") == 0 && relay->used == 1)
			client_ssl_smtps(pcb);
		else if (strcmp(s->aux.ssl, "smtps") == 0)
			client_ssl_smtps(pcb);
		else if (s->aux.ssl[0] == '\0')
			client_ssl_optional(pcb);

		/* enable AUTH */
		if (s->secret)
			client_auth(pcb, s->secret);

		/* set envelope sender */
		if (s->aux.mail_from[0])
			client_sender(pcb, "%s", s->aux.mail_from);
		else
			client_sender(pcb, "");
			
		/* set envelope recipients */
		TAILQ_FOREACH(rcpt, &s->recipients, entry)
			client_rcpt(pcb, rcpt, "%s", rcpt->address);

		s->pcb = pcb;
		event_set(&s->ev, s->fd, EV_READ|EV_WRITE, mta_event, s);
		event_add(&s->ev, &pcb->timeout);
		break;

	case MTA_DONE:
		/*
		 * Kill mta session.
		 */

		/* update queue status */
		while ((rcpt = TAILQ_FIRST(&s->recipients)))
			mta_rcpt_done(s, rcpt);

		imsg_compose_event(s->env->sc_ievs[PROC_QUEUE],
		    IMSG_BATCH_DONE, s->id, 0, -1, NULL, 0);

		/* deallocate resources */
		SPLAY_REMOVE(mtatree, &s->env->mta_sessions, s);
		while ((relay = TAILQ_FIRST(&s->relays))) {
			TAILQ_REMOVE(&s->relays, relay, entry);
			free(relay);
		}
		close(s->datafd);
		free(s->auxraw);
		free(s->secret);
		free(s);
		break;

	default:
		fatal("mta_enter_state: unknown state");
	}
}

void
mta_pickup(struct mta_session *s, void *p)
{
	int	 error;

	switch (s->state) {
	case MTA_INIT:
		if (s->aux.auth[0])
			mta_enter_state(s, MTA_SECRET, NULL);
		else
			mta_enter_state(s, MTA_MX, NULL);
		break;

	case MTA_SECRET:
		/* LKA responded to AUTH lookup. */
		s->secret = strdup(p);
		if (s->secret == NULL)
			fatal(NULL);
		else if (s->secret[0] == '\0') {
			mta_status(s, "190 secrets lookup failed");
			mta_enter_state(s, MTA_DONE, NULL);
		} else
			mta_enter_state(s, MTA_MX, NULL);
		break;

	case MTA_MX:
		/* LKA responded to DNS lookup. */
		error = *(int *)p;
		if (error == EAI_AGAIN) {
			mta_status(s, "100 MX lookup failed temporarily");
			mta_enter_state(s, MTA_DONE, NULL);
		} else if (error == EAI_NONAME) {
			mta_status(s, "600 Domain does not exist");
			mta_enter_state(s, MTA_DONE, NULL);
		} else if (error) {
			mta_status(s, "600 Unable to resolve DNS for domain");
			mta_enter_state(s, MTA_DONE, NULL);
		} else
			mta_enter_state(s, MTA_CONNECT, NULL);
		break;

	case MTA_CONNECT:
		/* Remote accepted/rejected connection. */
		error = session_socket_error(s->fd);
		if (error) {
			mta_status(s, "110 connect: %s", strerror(error));
			close(s->fd);
			mta_enter_state(s, MTA_CONNECT, NULL);
		} else
			mta_enter_state(s, MTA_PTR, NULL);
		break;

	case MTA_PTR:
		mta_enter_state(s, MTA_PROTOCOL, NULL);
		break;

	default:
		fatalx("mta_pickup: unexpected state");
	}
}

void
mta_event(int fd, short event, void *p)
{
	struct mta_session	*s = p;
	struct smtp_client	*pcb = s->pcb;
	struct recipient	*rcpt;

	if (event & EV_TIMEOUT) {
		mta_status(s, "150 timeout");
		goto out;
	}

	switch (client_talk(pcb, event & EV_WRITE)) {
	case CLIENT_WANT_WRITE:
		goto rw;
	case CLIENT_STOP_WRITE:
		goto ro;
	case CLIENT_RCPT_FAIL:
		rcpt = pcb->rcptfail;
		mta_rcpt_status(pcb->rcptfail, pcb->reply);
		mta_rcpt_log(s, pcb->rcptfail);
		mta_rcpt_done(s, pcb->rcptfail);
		goto rw;
	case CLIENT_DONE:
		mta_status(s, "%s", pcb->status);
		break;
	default:
		fatalx("mta_event: unexpected code");
	}

out:
	client_close(pcb);
	s->pcb = NULL;

	if (TAILQ_EMPTY(&s->recipients)) {
		log_debug("%s: leaving", __func__);
		mta_enter_state(s, MTA_DONE, NULL);
	} else {
		log_debug("%s: connecting to next", __func__);
		mta_enter_state(s, MTA_CONNECT, NULL);
	}
	return;

rw:
	event_set(&s->ev, fd, EV_READ|EV_WRITE, mta_event, s);
	event_add(&s->ev, &pcb->timeout);
	return;

ro:
	event_set(&s->ev, fd, EV_READ, mta_event, s);
	event_add(&s->ev, &pcb->timeout);
}

void
mta_status(struct mta_session *s, const char *fmt, ...)
{
	char			*status;
	struct recipient	*rcpt, *next;
	va_list			 ap;

	va_start(ap, fmt);
	if (vasprintf(&status, fmt, ap) == -1)
		fatal("vasprintf");
	va_end(ap);

	for (rcpt = TAILQ_FIRST(&s->recipients); rcpt; rcpt = next) {
		next = TAILQ_NEXT(rcpt, entry);

		/* save new status */
		mta_rcpt_status(rcpt, status);

		/* remove queue entry */
		if (*status == '2' || *status == '5' || *status == '6') {
			mta_rcpt_log(s, rcpt);
			mta_rcpt_done(s, rcpt);
		}
	}

	free(status);
}

void
mta_rcpt_status(struct recipient *rcpt, char *status)
{
	/*
	 * Previous delivery attempts might have assigned an errorline of
	 * higher status (eg. 5yz is of higher status than 4yz), so check
	 * this before deciding to overwrite existing status with a new one.
	 */
	if (*status != '2' && strncmp(rcpt->status, status, 3) > 0)
		return;

	/* change status */
	log_debug("mta: new status for %s: %s", rcpt->address, status);
	strlcpy(rcpt->status, status, sizeof rcpt->status);
}

void
mta_rcpt_log(struct mta_session *s, struct recipient *rcpt)
{
	struct mta_relay *relay;

	relay = TAILQ_FIRST(&s->relays);

	log_info("%s: to=%s, delay=%d, relay=%s [%s], stat=%s (%s)",
	    queue_be_decode(s->content_id), rcpt->address,
	    time(NULL) - s->birth,
	    relay ? relay->fqdn : "(none)",
	    relay ? ss_to_text(&relay->sa) : "",
	    rcpt->status[0] == '2' ? "Sent" :
	    rcpt->status[0] == '5' ? "RemoteError" :
	    rcpt->status[0] == '4' ? "RemoteError" : "LocalError",
	    rcpt->status + 4);
}

void
mta_rcpt_done(struct mta_session *s, struct recipient *rcpt)
{
	struct action *action;

	action = malloc(sizeof *action + strlen(rcpt->status));
	if (action == NULL)
		fatal(NULL);
	action->id = rcpt->action_id;
	strlcpy(action->data, rcpt->status, strlen(rcpt->status) + 1);
	imsg_compose_event(s->env->sc_ievs[PROC_QUEUE], IMSG_BATCH_UPDATE,
	    s->id, 0, -1, action, sizeof *action + strlen(rcpt->status));
	TAILQ_REMOVE(&s->recipients, rcpt, entry);
	free(action);
	free(rcpt);
}

void
mta_connect_done(int fd, short event, void *p)
{
	mta_pickup(p, NULL);
}

SPLAY_GENERATE(mtatree, mta_session, entry, mta_session_cmp);
