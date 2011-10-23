/*	$OpenBSD: mta.c,v 1.115 2011/10/23 09:30:07 gilles Exp $	*/

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

#include <errno.h>
#include <event.h>
#include <imsg.h>
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
#include "log.h"

static void mta_imsg(struct imsgev *, struct imsg *);
static void mta_shutdown(void);
static void mta_sig_handler(int, short, void *);
static struct mta_session *mta_lookup(u_int64_t);
static void mta_enter_state(struct mta_session *, int, void *);
static void mta_pickup(struct mta_session *, void *);
static void mta_event(int, short, void *);
static void mta_status(struct mta_session *, const char *, ...);
static void mta_message_status(struct envelope *, char *);
static void mta_message_log(struct mta_session *, struct envelope *);
static void mta_message_done(struct mta_session *, struct envelope *);
static void mta_connect_done(int, short, void *);
static void mta_request_datafd(struct mta_session *);

static void
mta_imsg(struct imsgev *iev, struct imsg *imsg)
{
	struct ramqueue_batch  	*rq_batch;
	struct mta_session	*s;
	struct mta_relay	*relay;
	struct envelope		*e;
	struct secret		*secret;
	struct dns		*dns;
	struct ssl		*ssl;

	log_imsg(PROC_MTA, iev->proc, imsg);

	if (iev->proc == PROC_QUEUE) {
		switch (imsg->hdr.type) {
		case IMSG_BATCH_CREATE:
			rq_batch = imsg->data;

			s = calloc(1, sizeof *s);
			if (s == NULL)
				fatal(NULL);
			s->id = rq_batch->b_id;
			s->state = MTA_INIT;
			s->batch = rq_batch;

			/* establish host name */
			if (rq_batch->rule.r_action == A_RELAYVIA) {
				s->host = strdup(rq_batch->rule.r_value.relayhost.hostname);
				s->flags |= MTA_FORCE_MX;
			}
			else
				s->host = NULL;

			/* establish port */
			s->port = ntohs(rq_batch->rule.r_value.relayhost.port); /* XXX */

			/* have cert? */
			s->cert = strdup(rq_batch->rule.r_value.relayhost.cert);
			if (s->cert == NULL)
				fatal(NULL);
			else if (s->cert[0] == '\0') {
				free(s->cert);
				s->cert = NULL;
			}

			/* use auth? */
			if ((rq_batch->rule.r_value.relayhost.flags & F_SSL) &&
			    (rq_batch->rule.r_value.relayhost.flags & F_AUTH)) {
				s->flags |= MTA_USE_AUTH;
				s->secmapid = rq_batch->rule.r_value.relayhost.secmapid;
			}

			/* force a particular SSL mode? */
			switch (rq_batch->rule.r_value.relayhost.flags & F_SSL) {
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
			return;


		case IMSG_BATCH_APPEND:
			e = imsg->data;
			s = mta_lookup(e->batch_id);
			e = malloc(sizeof *e);
			if (e == NULL)
				fatal(NULL);
			*e = *(struct envelope *)imsg->data;
			strlcpy(e->errorline, "000 init",
			    sizeof(e->errorline));

			if (s->host == NULL) {
				s->host = strdup(e->dest.domain);
				if (s->host == NULL)
					fatal("strdup");
			}
 			TAILQ_INSERT_TAIL(&s->recipients, e, entry);
			return;

		case IMSG_BATCH_CLOSE:
			rq_batch = imsg->data;
			mta_pickup(mta_lookup(rq_batch->b_id), NULL);
			return;

		case IMSG_QUEUE_MESSAGE_FD:
			rq_batch = imsg->data;
			mta_pickup(mta_lookup(rq_batch->b_id), &imsg->fd);
			return;
		}
	}

	if (iev->proc == PROC_LKA) {
		switch (imsg->hdr.type) {
		case IMSG_LKA_SECRET:
			secret = imsg->data;
			mta_pickup(mta_lookup(secret->id), secret->secret);
			return;

		case IMSG_DNS_HOST:
			dns = imsg->data;
			s = mta_lookup(dns->id);
			relay = calloc(1, sizeof *relay);
			if (relay == NULL)
				fatal(NULL);
			relay->sa = dns->ss;
 			TAILQ_INSERT_TAIL(&s->relays, relay, entry);
			return;

		case IMSG_DNS_HOST_END:
			dns = imsg->data;
			mta_pickup(mta_lookup(dns->id), &dns->error);
			return;

		case IMSG_DNS_PTR:
			dns = imsg->data;
			s = mta_lookup(dns->id);
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

static void
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

static void
mta_shutdown(void)
{
	log_info("mail transfer agent exiting");
	_exit(0);
}

pid_t
mta(void)
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
	purge_config(PURGE_EVERYTHING);

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

	signal_set(&ev_sigint, SIGINT, mta_sig_handler, NULL);
	signal_set(&ev_sigterm, SIGTERM, mta_sig_handler, NULL);
	signal_add(&ev_sigint, NULL);
	signal_add(&ev_sigterm, NULL);
	signal(SIGPIPE, SIG_IGN);
	signal(SIGHUP, SIG_IGN);

	config_pipes(peers, nitems(peers));
	config_peers(peers, nitems(peers));

	ramqueue_init(&env->sc_rqueue);
	SPLAY_INIT(&env->mta_sessions);

	if (event_dispatch() < 0)
		fatal("event_dispatch");
	mta_shutdown();

	return (0);
}

static struct mta_session *
mta_lookup(u_int64_t id)
{
	struct mta_session	 key, *res;

	key.id = id;
	if ((res = SPLAY_FIND(mtatree, &env->mta_sessions, &key)) == NULL)
		fatalx("mta_lookup: session not found");
	return (res);
}

static void
mta_enter_state(struct mta_session *s, int newstate, void *p)
{
	struct secret		 secret;
	struct mta_relay	*relay;
	struct sockaddr		*sa;
	struct envelope		*e;
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
		secret.secmapid = s->secmapid;
		strlcpy(secret.host, s->host, sizeof(secret.host));
		imsg_compose_event(env->sc_ievs[PROC_LKA], IMSG_LKA_SECRET,
		    0, 0, -1, &secret, sizeof(secret));  
		break;

	case MTA_MX:
		/*
		 * Lookup MX record.
		 */
		if (s->flags & MTA_FORCE_MX)
			dns_query_host(s->host, s->port, s->id);
		else
			dns_query_mx(s->host, 0, s->id);
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
		dns_query_ptr(&relay->sa, s->id);
		break;

	case MTA_PROTOCOL:
		/*
		 * Start protocol engine.
		 */
		log_debug("mta: entering smtp phase");

		pcb = client_init(s->fd, s->datafp, env->sc_hostname, 1);

		/* lookup SSL certificate */
		if (s->cert) {
			struct ssl	 key, *res;

			strlcpy(key.ssl_name, s->cert, sizeof(key.ssl_name));
			res = SPLAY_FIND(ssltree, env->sc_ssl, &key);
			if (res == NULL) {
				client_close(pcb);
				s->pcb = NULL;
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
		e = TAILQ_FIRST(&s->recipients);
		if (e->sender.user[0] && e->sender.domain[0])
			client_sender(pcb, "%s@%s", e->sender.user,
			    e->sender.domain);
		else
			client_sender(pcb, "");
			
		/* set envelope recipients */
		TAILQ_FOREACH(e, &s->recipients, entry)
			client_rcpt(pcb, e, "%s@%s", e->dest.user,
			    e->dest.domain);

		s->pcb = pcb;
		event_set(&s->ev, s->fd, EV_READ|EV_WRITE, mta_event, s);
		event_add(&s->ev, &pcb->timeout);
		break;

	case MTA_DONE:
		/*
		 * Kill mta session.
		 */

		/* update queue status */
		while ((e = TAILQ_FIRST(&s->recipients)))
			mta_message_done(s, e);

		imsg_compose_event(env->sc_ievs[PROC_QUEUE],
		    IMSG_BATCH_DONE, 0, 0, -1, NULL, 0);

		/* deallocate resources */
		SPLAY_REMOVE(mtatree, &env->mta_sessions, s);
		while ((relay = TAILQ_FIRST(&s->relays))) {
			TAILQ_REMOVE(&s->relays, relay, entry);
			free(relay);
		}

		if (s->datafp)
			fclose(s->datafp);

		free(s->secret);
		free(s->host);
		free(s->cert);
		free(s);
		break;

	default:
		fatal("mta_enter_state: unknown state");
	}
}

static void
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
		if ((error = *(int *)p)) {
			if (error == DNS_RETRY)
				mta_status(s, "100 MX lookup failed temporarily");
			else if (error == DNS_EINVAL)
				mta_status(s, "600 Invalid domain name");
			else if (error == DNS_ENONAME)
				mta_status(s, "600 Domain does not exist");
			else if (error == DNS_ENOTFOUND)
				mta_status(s, "600 No MX address found for domain");
			mta_enter_state(s, MTA_DONE, NULL);
		} else
			mta_enter_state(s, MTA_DATA, NULL);
		break;

	case MTA_DATA:
		/* QUEUE replied to body fd request. */
		if (*(int *)p == -1)
			fatalx("mta cannot obtain msgfd");
		s->datafp = fdopen(*(int *)p, "r");
		if (s->datafp == NULL)
			fatal("fdopen");
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

static void
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

static void
mta_status(struct mta_session *s, const char *fmt, ...)
{
	char			*status;
	struct envelope		*e, *next;
	va_list			 ap;

	va_start(ap, fmt);
	if (vasprintf(&status, fmt, ap) == -1)
		fatal("vasprintf");
	va_end(ap);

	for (e = TAILQ_FIRST(&s->recipients); e; e = next) {
		next = TAILQ_NEXT(e, entry);

		/* save new status */
		mta_message_status(e, status);

		/* remove queue entry */
		if (*status == '2' || *status == '5' || *status == '6') {
			mta_message_log(s, e);
			mta_message_done(s, e);
		}
	}

	free(status);
}

static void
mta_message_status(struct envelope *e, char *status)
{
	/*
	 * Previous delivery attempts might have assigned an errorline of
	 * higher status (eg. 5yz is of higher status than 4yz), so check
	 * this before deciding to overwrite existing status with a new one.
	 */
	if (*status != '2' && strncmp(e->errorline, status, 3) > 0)
		return;

	/* change status */
	log_debug("mta: new status for %s@%s: %s", e->dest.user,
	    e->dest.domain, status);
	strlcpy(e->errorline, status, sizeof(e->errorline));
}

static void
mta_message_log(struct mta_session *s, struct envelope *e)
{
	struct mta_relay	*relay = TAILQ_FIRST(&s->relays);
	char			*status = e->errorline;

	log_info("%016llx: to=<%s@%s>, delay=%lld, relay=%s [%s], stat=%s (%s)",
	    e->id, e->dest.user,
	    e->dest.domain,
	    (long long int) (time(NULL) - e->creation),
	    relay ? relay->fqdn : "(none)",
	    relay ? ss_to_text(&relay->sa) : "",
	    *status == '2' ? "Sent" :
	    *status == '5' ? "RemoteError" :
	    *status == '4' ? "RemoteError" : "LocalError",
	    status + 4);
}

static void
mta_message_done(struct mta_session *s, struct envelope *e)
{
	switch (e->errorline[0]) {
	case '6':
	case '5':
		e->status = DS_PERMFAILURE;
		break;
	case '2':
		e->status = DS_ACCEPTED;
		break;
	default:
		e->status = DS_TEMPFAILURE;
		break;
	}
	imsg_compose_event(env->sc_ievs[PROC_QUEUE],
	    IMSG_QUEUE_MESSAGE_UPDATE, 0, 0, -1, e, sizeof(*e));
	TAILQ_REMOVE(&s->recipients, e, entry);
	free(e);
}

static void
mta_connect_done(int fd, short event, void *p)
{
	mta_pickup(p, NULL);
}

static void
mta_request_datafd(struct mta_session *s)
{
	struct ramqueue_batch	rq_batch;
	struct envelope	*e;

	e = TAILQ_FIRST(&s->recipients);

	rq_batch.b_id = s->id;
	rq_batch.msgid = evpid_to_msgid(e->id);
	imsg_compose_event(env->sc_ievs[PROC_QUEUE], IMSG_QUEUE_MESSAGE_FD,
	    0, 0, -1, &rq_batch, sizeof(rq_batch));
}

int
mta_session_cmp(struct mta_session *a, struct mta_session *b)
{
	return (a->id < b->id ? -1 : a->id > b->id);
}

SPLAY_GENERATE(mtatree, mta_session, entry, mta_session_cmp);
