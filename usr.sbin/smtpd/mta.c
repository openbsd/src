/*	$OpenBSD: mta.c,v 1.83 2009/12/23 17:16:03 jacekm Exp $	*/

/*
 * Copyright (c) 2008 Pierre-Yves Ritschard <pyr@openbsd.org>
 * Copyright (c) 2008 Gilles Chehade <gilles@openbsd.org>
 * Copyright (c) 2009 Jacek Masiulaniec <jacekm@dobremiasto.net>
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

__dead void		 mta_shutdown(void);
void			 mta_sig_handler(int, short, void *);

void			 mta_dispatch_parent(int, short, void *);
void			 mta_dispatch_runner(int, short, void *);
void			 mta_dispatch_queue(int, short, void *);
void			 mta_dispatch_lka(int, short, void *);

struct mta_session	*mta_lookup(struct smtpd *, u_int64_t);
void			 mta_enter_state(struct mta_session *, int, void *);
void			 mta_pickup(struct mta_session *, void *);
void			 mta_event(int, short, void *);

void			 mta_status(struct mta_session *, const char *, ...);
void			 mta_message_status(struct message *, char *);
void			 mta_message_log(struct mta_session *, struct message *);
void			 mta_message_done(struct mta_session *, struct message *);
void			 mta_connect_done(int, short, void *);
void			 mta_request_datafd(struct mta_session *);

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
mta_dispatch_parent(int sig, short event, void *p)
{
	struct smtpd		*env = p;
	struct imsgev		*iev;
	struct imsgbuf		*ibuf;
	struct imsg		 imsg;
	ssize_t			 n;

	iev = env->sc_ievs[PROC_PARENT];
	ibuf = &iev->ibuf;

	if (event & EV_READ) {
		if ((n = imsg_read(ibuf)) == -1)
			fatal("imsg_read_error");
		if (n == 0) {
			/* this pipe is dead, so remove the event handler */
			event_del(&iev->ev);
			event_loopexit(NULL);
			return;
		}
	}

	if (event & EV_WRITE) {
		if (msgbuf_write(&ibuf->w) == -1)
			fatal("msgbuf_write");
	}

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatal("mta_dispatch_parent: imsg_get error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_CONF_START:
			if (env->sc_flags & SMTPD_CONFIGURING)
				break;
			env->sc_flags |= SMTPD_CONFIGURING;

			if ((env->sc_ssl = calloc(1, sizeof(*env->sc_ssl))) == NULL)
				fatal("mta_dispatch_parent: calloc");
			break;
		case IMSG_CONF_SSL: {
			struct ssl	*s;
			struct ssl	*x_ssl;

			if (!(env->sc_flags & SMTPD_CONFIGURING))
				break;

			if ((s = calloc(1, sizeof(*s))) == NULL)
				fatal(NULL);
			x_ssl = imsg.data;
			(void)strlcpy(s->ssl_name, x_ssl->ssl_name,
			    sizeof(s->ssl_name));
			s->ssl_cert_len = x_ssl->ssl_cert_len;
			if ((s->ssl_cert =
			    strdup((char *)imsg.data + sizeof(*s))) == NULL)
				fatal(NULL);
			s->ssl_key_len = x_ssl->ssl_key_len;
			if ((s->ssl_key = strdup((char *)imsg.data +
			    (sizeof(*s) + s->ssl_cert_len))) == NULL)
				fatal(NULL);

			SPLAY_INSERT(ssltree, env->sc_ssl, s);
			break;
		}
		case IMSG_CONF_END:
			if (!(env->sc_flags & SMTPD_CONFIGURING))
				break;
			env->sc_flags &= ~SMTPD_CONFIGURING;
			break;
		default:
			log_warnx("mta_dispatch_parent: got imsg %d",
			    imsg.hdr.type);
			fatalx("mta_dispatch_parent: unexpected imsg");
		}
		imsg_free(&imsg);
	}
	imsg_event_add(iev);
}

void
mta_dispatch_runner(int sig, short event, void *p)
{
	struct smtpd		*env = p;
	struct imsgev		*iev;
	struct imsgbuf		*ibuf;
	struct imsg		 imsg;
	ssize_t			 n;

	iev = env->sc_ievs[PROC_RUNNER];
	ibuf = &iev->ibuf;

	if (event & EV_READ) {
		if ((n = imsg_read(ibuf)) == -1)
			fatal("imsg_read_error");
		if (n == 0) {
			/* this pipe is dead, so remove the event handler */
			event_del(&iev->ev);
			event_loopexit(NULL);
			return;
		}
	}

	if (event & EV_WRITE) {
		if (msgbuf_write(&ibuf->w) == -1)
			fatal("msgbuf_write");
	}

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatal("mta_dispatch_runner: imsg_get error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_BATCH_CREATE: {
			struct batch		*b = imsg.data;
			struct mta_session	*s;

			IMSG_SIZE_CHECK(b);

			if ((s = calloc(1, sizeof(*s))) == NULL)
				fatal(NULL);
			s->id = b->id;
			s->state = MTA_INIT;
			s->env = env;
			s->datafd = -1;

			/* establish host name */
			if (b->rule.r_action == A_RELAYVIA)
				s->host = strdup(b->rule.r_value.relayhost.hostname);
			else
				s->host = strdup(b->hostname);
			if (s->host == NULL)
				fatal(NULL);

			/* establish port */
			s->port = ntohs(b->rule.r_value.relayhost.port); /* XXX */

			/* have cert? */
			s->cert = strdup(b->rule.r_value.relayhost.cert);
			if (s->cert == NULL)
				fatal(NULL);
			else if (s->cert[0] == '\0') {
				free(s->cert);
				s->cert = NULL;
			}

			/* use auth? */
			if ((b->rule.r_value.relayhost.flags & F_SSL) &&
			    (b->rule.r_value.relayhost.flags & F_AUTH))
				s->flags |= MTA_USE_AUTH;

			/* force a particular SSL mode? */
			switch (b->rule.r_value.relayhost.flags & F_SSL) {
			case F_SSL:
				s->flags |= MTA_FORCE_ANYSSL;
				break;

			case F_SMTPS:
				s->flags |= MTA_FORCE_SMTPS;

			case F_STARTTLS:
				/* client_* API by default requires STARTTLS */
				break;

			default:
				s->flags |= MTA_ALLOW_PLAIN;
			}

			TAILQ_INIT(&s->recipients);
			TAILQ_INIT(&s->relays);
			SPLAY_INSERT(mtatree, &env->mta_sessions, s);
			break;
		}

		case IMSG_BATCH_APPEND: {
			struct message		*append = imsg.data, *m;
			struct mta_session	*s;

			IMSG_SIZE_CHECK(append);

			s = mta_lookup(env, append->batch_id);
			if ((m = malloc(sizeof(*m))) == NULL)
				fatal(NULL);
			*m = *append;
			strlcpy(m->session_errorline, "000 init",
			    sizeof(m->session_errorline));
 			TAILQ_INSERT_TAIL(&s->recipients, m, entry);
			break;
		}

		case IMSG_BATCH_CLOSE: {
			struct batch		*b = imsg.data;

			IMSG_SIZE_CHECK(b);
			mta_pickup(mta_lookup(env, b->id), NULL);
			break;
		}

		default:
			log_warnx("mta_dispatch_runner: got imsg %d",
			    imsg.hdr.type);
			fatalx("mta_dispatch_runner: unexpected imsg");
		}
		imsg_free(&imsg);
	}
	imsg_event_add(iev);
}

void
mta_dispatch_lka(int sig, short event, void *p)
{
	struct smtpd		*env = p;
	struct imsgev		*iev;
	struct imsgbuf		*ibuf;
	struct imsg		 imsg;
	ssize_t			 n;

	iev = env->sc_ievs[PROC_LKA];
	ibuf = &iev->ibuf;

	if (event & EV_READ) {
		if ((n = imsg_read(ibuf)) == -1)
			fatal("imsg_read_error");
		if (n == 0) {
			/* this pipe is dead, so remove the event handler */
			event_del(&iev->ev);
			event_loopexit(NULL);
			return;
		}
	}

	if (event & EV_WRITE) {
		if (msgbuf_write(&ibuf->w) == -1)
			fatal("msgbuf_write");
	}

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatal("mta_dispatch_lka: imsg_get error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_LKA_SECRET: {
			struct secret		*reply = imsg.data;

			IMSG_SIZE_CHECK(reply);

			mta_pickup(mta_lookup(env, reply->id), reply->secret);
			break;
		}

		case IMSG_DNS_A: {
			struct dns		*reply = imsg.data;
			struct mta_relay	*relay;
			struct mta_session	*s;

			IMSG_SIZE_CHECK(reply);

			s = mta_lookup(env, reply->id);
			if ((relay = calloc(1, sizeof(*relay))) == NULL)
				fatal(NULL);
			relay->sa = reply->ss;
 			TAILQ_INSERT_TAIL(&s->relays, relay, entry);
			break;
		}

		case IMSG_DNS_A_END: {
			struct dns		*reply = imsg.data;

			IMSG_SIZE_CHECK(reply);

			mta_pickup(mta_lookup(env, reply->id), &reply->error);
			break;
		}

		case IMSG_DNS_PTR: {
			struct dns		*reply = imsg.data;
			struct mta_session	*s;
			struct mta_relay	*r;

			IMSG_SIZE_CHECK(reply);

			s = mta_lookup(env, reply->id);
			r = TAILQ_FIRST(&s->relays);
			if (reply->error)
				strlcpy(r->fqdn, "<unknown>", sizeof(r->fqdn));
			else
				strlcpy(r->fqdn, reply->host, sizeof(r->fqdn));
			mta_pickup(s, NULL);
			break;
		}

		default:
			log_warnx("mta_dispatch_parent: got imsg %d",
			    imsg.hdr.type);
			fatalx("mta_dispatch_lka: unexpected imsg");
		}
		imsg_free(&imsg);
	}
	imsg_event_add(iev);
}

void
mta_dispatch_queue(int sig, short event, void *p)
{
	struct smtpd		*env = p;
	struct imsgev		*iev;
	struct imsgbuf		*ibuf;
	struct imsg		 imsg;
	ssize_t			 n;

	iev = env->sc_ievs[PROC_QUEUE];
	ibuf = &iev->ibuf;

	if (event & EV_READ) {
		if ((n = imsg_read(ibuf)) == -1)
			fatal("imsg_read_error");
		if (n == 0) {
			/* this pipe is dead, so remove the event handler */
			event_del(&iev->ev);
			event_loopexit(NULL);
			return;
		}
	}

	if (event & EV_WRITE) {
		if (msgbuf_write(&ibuf->w) == -1)
			fatal("msgbuf_write");
	}

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatal("mta_dispatch_queue: imsg_get error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_QUEUE_MESSAGE_FD: {
			struct batch	*b = imsg.data;

			IMSG_SIZE_CHECK(b);

			mta_pickup(mta_lookup(env, b->id), &imsg.fd);
			break;
		}

		default:
			log_warnx("mta_dispatch_queue: got imsg %d",
			    imsg.hdr.type);
			fatalx("mta_dispatch_queue: unexpected imsg");
		}
		imsg_free(&imsg);
	}
	imsg_event_add(iev);
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
		{ PROC_PARENT,	mta_dispatch_parent },
		{ PROC_QUEUE,	mta_dispatch_queue },
		{ PROC_RUNNER,	mta_dispatch_runner },
		{ PROC_LKA,	mta_dispatch_lka }
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
#ifndef DEBUG
	if (chroot(pw->pw_dir) == -1)
		fatal("mta: chroot");
	if (chdir("/") == -1)
		fatal("mta: chdir(\"/\")");
#else
#warning disabling privilege revocation and chroot in DEBUG MODE
#endif

	smtpd_process = PROC_MTA;
	setproctitle("%s", env->sc_title[smtpd_process]);

#ifndef DEBUG
	if (setgroups(1, &pw->pw_gid) ||
	    setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) ||
	    setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid))
		fatal("mta: cannot drop privileges");
#endif

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

	event_dispatch();
	mta_shutdown();

	return (0);
}

int
mta_session_cmp(struct mta_session *a, struct mta_session *b)
{
	return (a->id < b->id ? -1 : a->id > b->id);
}

struct mta_session *
mta_lookup(struct smtpd *env, u_int64_t id)
{
	struct mta_session	 key, *res;

	key.id = id;
	if ((res = SPLAY_FIND(mtatree, &env->mta_sessions, &key)) == NULL)
		fatalx("mta_lookup: session not found");
	return (res);
}

void
mta_enter_state(struct mta_session *s, int newstate, void *p)
{
	struct secret		 secret;
	struct mta_relay	*relay;
	struct sockaddr		*sa;
	struct message		*m;
	struct smtp_client	*pcb;
	int			 max_reuse;

	s->state = newstate;

	switch (s->state) {
	case MTA_SECRET:
		/*
		 * Lookup AUTH secret.
		 */
		bzero(&secret, sizeof(secret));
		secret.id = s->id;
		strlcpy(secret.host, s->host, sizeof(secret.host));
		imsg_compose_event(s->env->sc_ievs[PROC_LKA], IMSG_LKA_SECRET,
		    0, 0, -1, &secret, sizeof(secret));  
		break;

	case MTA_MX:
		/*
		 * Lookup MX record.
		 */
		dns_query_mx(s->env, s->host, 0, s->id);
		break;

	case MTA_DATA:
		/*
		 * Obtain message body fd.
		 */
		log_debug("mta: getting datafd");
		mta_request_datafd(s);
		break;

	case MTA_CONNECT:
		/*
		 * Connect to the MX.
		 */
		if (s->flags & MTA_FORCE_ANYSSL)
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

			if (s->port)
				sa_set_port(sa, s->port);
			else if ((s->flags & MTA_FORCE_ANYSSL) && relay->used == 1)
				sa_set_port(sa, 465);
			else if (s->flags & MTA_FORCE_SMTPS)
				sa_set_port(sa, 465);
			else
				sa_set_port(sa, 25);

			s->fd = socket(sa->sa_family, SOCK_STREAM, 0);
			if (s->fd == -1)
				fatal("mta cannot create socket");
			session_socket_blockmode(s->fd, BM_NONBLOCK);
			session_socket_no_linger(s->fd);

			if (connect(s->fd, sa, sa->sa_len) == -1) {
				if (errno != EINPROGRESS) {
					mta_status(s, "110 connect error: %s",
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
		if (s->cert) {
			struct ssl	 key, *res;

			strlcpy(key.ssl_name, s->cert, sizeof(key.ssl_name));
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
		if ((s->flags & MTA_FORCE_ANYSSL) && relay->used == 1)
			client_ssl_smtps(pcb);
		else if (s->flags & MTA_FORCE_SMTPS)
			client_ssl_smtps(pcb);
		else if (s->flags & MTA_ALLOW_PLAIN)
			client_ssl_optional(pcb);

		/* enable AUTH */
		if (s->secret)
			client_auth(pcb, s->secret);

		/* set envelope sender */
		m = TAILQ_FIRST(&s->recipients);
		if (m->sender.user[0] && m->sender.domain[0])
			client_sender(pcb, "%s@%s", m->sender.user,
			    m->sender.domain);
		else
			client_sender(pcb, "");
			
		/* set envelope recipients */
		TAILQ_FOREACH(m, &s->recipients, entry)
			client_rcpt(pcb, m, "%s@%s", m->recipient.user,
			    m->recipient.domain);

		s->pcb = pcb;
		event_set(&s->ev, s->fd, EV_READ|EV_WRITE, mta_event, s);
		event_add(&s->ev, &pcb->timeout);
		break;

	case MTA_DONE:
		/*
		 * Kill mta session.
		 */

		/* update queue status */
		while ((m = TAILQ_FIRST(&s->recipients)))
			mta_message_done(s, m);

		imsg_compose_event(s->env->sc_ievs[PROC_RUNNER],
		    IMSG_BATCH_DONE, 0, 0, -1, NULL, 0);

		/* deallocate resources */
		SPLAY_REMOVE(mtatree, &s->env->mta_sessions, s);
		while ((relay = TAILQ_FIRST(&s->relays))) {
			TAILQ_REMOVE(&s->relays, relay, entry);
			free(relay);
		}
		close(s->datafd);
		free(s->secret);
		free(s->host);
		free(s->cert);
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
		if (s->flags & MTA_USE_AUTH)
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
			mta_enter_state(s, MTA_DATA, NULL);
		break;

	case MTA_DATA:
		/* QUEUE replied to body fd request. */
		s->datafd = *(int *)p;
		if (s->datafd == -1)
			fatalx("mta cannot obtain msgfd");
		else
			mta_enter_state(s, MTA_CONNECT, NULL);
		break;

	case MTA_CONNECT:
		/* Remote accepted/rejected connection. */
		error = session_socket_error(s->fd);
		if (error) {
			mta_status(s, "110 connect error: %s", strerror(error));
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
		mta_message_status(pcb->rcptfail, pcb->reply);
		mta_message_log(s, pcb->rcptfail);
		mta_message_done(s, pcb->rcptfail);
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

	if (TAILQ_EMPTY(&s->recipients))
		mta_enter_state(s, MTA_DONE, NULL);
	else
		mta_enter_state(s, MTA_CONNECT, NULL);
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
	struct message		*m, *next;
	va_list			 ap;

	va_start(ap, fmt);
	if (vasprintf(&status, fmt, ap) == -1)
		fatal("vasprintf");
	va_end(ap);

	for (m = TAILQ_FIRST(&s->recipients); m; m = next) {
		next = TAILQ_NEXT(m, entry);

		/* save new status */
		mta_message_status(m, status);

		/* remove queue entry */
		if (*status == '2' || *status == '5' || *status == '6') {
			mta_message_log(s, m);
			mta_message_done(s, m);
		}
	}

	free(status);
}

void
mta_message_status(struct message *m, char *status)
{
	/*
	 * Previous delivery attempts might have assigned an errorline of
	 * higher status (eg. 5yz is of higher status than 4yz), so check
	 * this before deciding to overwrite existing status with a new one.
	 */
	if (*status != '2' && strncmp(m->session_errorline, status, 3) > 0)
		return;

	/* change status */
	log_debug("mta: new status for %s@%s: %s", m->recipient.user,
	    m->recipient.domain, status);
	strlcpy(m->session_errorline, status, sizeof(m->session_errorline));
}

void
mta_message_log(struct mta_session *s, struct message *m)
{
	struct mta_relay	*relay = TAILQ_FIRST(&s->relays);
	char			*status = m->session_errorline;

	log_info("%s: to=<%s@%s>, delay=%d, relay=%s [%s], stat=%s (%s)",
	    m->message_id, m->recipient.user,
	    m->recipient.domain, time(NULL) - m->creation,
	    relay ? relay->fqdn : "(none)",
	    relay ? ss_to_text(&relay->sa) : "",
	    *status == '2' ? "Sent" :
	    *status == '5' ? "RemoteError" :
	    *status == '4' ? "RemoteError" : "LocalError",
	    status + 4);
}

void
mta_message_done(struct mta_session *s, struct message *m)
{
	switch (m->session_errorline[0]) {
	case '6':
	case '5':
		m->status = S_MESSAGE_PERMFAILURE;
		break;
	case '2':
		m->status = S_MESSAGE_ACCEPTED;
		break;
	default:
		m->status = S_MESSAGE_TEMPFAILURE;
		break;
	}
	imsg_compose_event(s->env->sc_ievs[PROC_QUEUE],
	    IMSG_QUEUE_MESSAGE_UPDATE, 0, 0, -1, m, sizeof(*m));
	TAILQ_REMOVE(&s->recipients, m, entry);
	free(m);
}

void
mta_connect_done(int fd, short event, void *p)
{
	mta_pickup(p, NULL);
}

void
mta_request_datafd(struct mta_session *s)
{
	struct batch	 b;
	struct message	*m;

	b.id = s->id;
	m = TAILQ_FIRST(&s->recipients);
	strlcpy(b.message_id, m->message_id, sizeof(b.message_id));
	imsg_compose_event(s->env->sc_ievs[PROC_QUEUE], IMSG_QUEUE_MESSAGE_FD,
	    0, 0, -1, &b, sizeof(b));
}

SPLAY_GENERATE(mtatree, mta_session, entry, mta_session_cmp);
