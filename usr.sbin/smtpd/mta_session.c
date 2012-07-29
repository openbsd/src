/*	$OpenBSD: mta_session.c,v 1.5 2012/07/29 13:56:24 eric Exp $	*/

/*
 * Copyright (c) 2008 Pierre-Yves Ritschard <pyr@openbsd.org>
 * Copyright (c) 2008 Gilles Chehade <gilles@openbsd.org>
 * Copyright (c) 2009 Jacek Masiulaniec <jacekm@dobremiasto.net>
 * Copyright (c) 2012 Eric Faurot <eric@openbsd.org>
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

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <event.h>
#include <imsg.h>
#include <inttypes.h>
#include <netdb.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "smtpd.h"
#include "log.h"

struct mta_relay {
	TAILQ_ENTRY(mta_relay)	 entry;
	struct sockaddr_storage	 sa;
	char			 fqdn[MAXHOSTNAMELEN];
	int			 used;
};

struct mta_session {
	SPLAY_ENTRY(mta_session) entry;
	u_int64_t		 id;
	enum mta_state		 state;
	char			*host;
	int			 port;
	int			 flags;
	TAILQ_HEAD(,mta_relay)	 relays;
	char			*authmap;
	char			*secret;
	FILE			*datafp;

	TAILQ_HEAD(,mta_task)	 tasks;

	struct envelope		*currevp;
	struct iobuf		 iobuf;
	struct io		 io;
	int			 ext; /* extension */
	struct ssl		*ssl;
};

struct mta_task {
	SPLAY_ENTRY(mta_task)	 entry;
	uint64_t		 id;

	struct mta_session	*session;
	TAILQ_ENTRY(mta_task)	 list;

	struct mailaddr		 sender;
	TAILQ_HEAD(,envelope)	 envelopes;
};

SPLAY_HEAD(mta_session_tree, mta_session);
SPLAY_HEAD(mta_task_tree, mta_task);

static void mta_io(struct io *, int);
static struct mta_session *mta_session_lookup(u_int64_t);
static void mta_enter_state(struct mta_session *, int);
static void mta_status(struct mta_session *, int, const char *, ...);
static int mta_envelope_done(struct mta_task *, struct envelope *,
    const char *);
static void mta_send(struct mta_session *, char *, ...);
static ssize_t mta_queue_data(struct mta_session *);
static void mta_response(struct mta_session *, char *);
static int mta_session_cmp(struct mta_session *, struct mta_session *);

static int mta_task_cmp(struct mta_task *, struct mta_task *);
static struct mta_task * mta_task_lookup(uint64_t, int);
static struct mta_task * mta_task_create(uint64_t, struct mta_session *);
static void mta_task_free(struct mta_task *);

SPLAY_PROTOTYPE(mta_session_tree, mta_session, entry, mta_session_cmp);
SPLAY_PROTOTYPE(mta_task_tree, mta_task, entry, mta_task_cmp);

static const char * mta_strstate(int);

#define MTA_HIWAT	65535

static struct mta_session_tree mta_sessions = SPLAY_INITIALIZER(&mta_sessions);
static struct mta_task_tree mta_tasks = SPLAY_INITIALIZER(&mta_tasks);

void
mta_session_imsg(struct imsgev *iev, struct imsg *imsg)
{
	struct mta_batch	*mta_batch;
	struct mta_task		*task;
	struct mta_session	*s;
	struct mta_relay	*relay;
	struct envelope		*e;
	struct secret		*secret;
	struct dns		*dns;
	struct ssl		 key;
	char			*cert;
	void			*ptr;

	switch (imsg->hdr.type) {
	case IMSG_BATCH_CREATE:
		mta_batch = imsg->data;

		s = calloc(1, sizeof *s);
		if (s == NULL)
			fatal(NULL);
		s->id = mta_batch->id;
		s->state = MTA_INIT;

		/* establish host name */
		if (mta_batch->relay.hostname[0]) {
			s->host = strdup(mta_batch->relay.hostname);
			s->flags |= MTA_FORCE_MX;
		}

		/* establish port */
		s->port = mta_batch->relay.port;

		/* use auth? */
		if ((mta_batch->relay.flags & F_SSL) &&
		    (mta_batch->relay.flags & F_AUTH)) {
			s->flags |= MTA_USE_AUTH;
			s->authmap = strdup(mta_batch->relay.authmap);
			if (s->authmap == NULL)
				fatalx("mta: strdup authmap");
		}

		/* force a particular SSL mode? */
		switch (mta_batch->relay.flags & F_SSL) {
		case F_SSL:
			s->flags |= MTA_FORCE_ANYSSL;
			break;
		case F_SMTPS:
			s->flags |= MTA_FORCE_SMTPS;
			break;
		case F_STARTTLS:
			/* STARTTLS is tried by default */
			break;
		default:
			s->flags |= MTA_ALLOW_PLAIN;
		}

		/* have cert? */
		cert = mta_batch->relay.cert;
		if (cert[0] != '\0') {
			s->flags |= MTA_USE_CERT;
			strlcpy(key.ssl_name, cert, sizeof(key.ssl_name));
			s->ssl = SPLAY_FIND(ssltree, env->sc_ssl, &key);
		}

		TAILQ_INIT(&s->relays);
		TAILQ_INIT(&s->tasks);
		SPLAY_INSERT(mta_session_tree, &mta_sessions, s);

		if (mta_task_create(s->id, s) == NULL)
			fatalx("mta_session_imsg: mta_task_create");

		log_debug("mta: %p: new session for batch %" PRIu64, s, s->id);

		return;

	case IMSG_BATCH_APPEND:
		e = imsg->data;
		task = mta_task_lookup(e->batch_id, 1);
		e = malloc(sizeof *e);
		if (e == NULL)
			fatal(NULL);
		*e = *(struct envelope *)imsg->data;
		s = task->session;
		if (s->host == NULL) {
			s->host = strdup(e->dest.domain);
			if (s->host == NULL)
				fatal("strdup");
		}
		if (TAILQ_FIRST(&task->envelopes) == NULL)
			task->sender = e->sender;
		log_debug("mta: %p: adding <%s@%s> from envelope %016" PRIx64,
		    s, e->dest.user, e->dest.domain, e->id);
		TAILQ_INSERT_TAIL(&task->envelopes, e, entry);
		return;

	case IMSG_BATCH_CLOSE:
		mta_batch = imsg->data;
		task = mta_task_lookup(mta_batch->id, 1);
		s = task->session;
		if (s->flags & MTA_USE_CERT && s->ssl == NULL) {
			mta_status(s, 1, "190 certificate not found");
			mta_enter_state(s, MTA_DONE);
		} else
			mta_enter_state(s, MTA_INIT);
		return;

	case IMSG_QUEUE_MESSAGE_FD:
		mta_batch = imsg->data;
		if (imsg->fd == -1)
			fatalx("mta: cannot obtain msgfd");
		s = mta_session_lookup(mta_batch->id);
		s->datafp = fdopen(imsg->fd, "r");
		if (s->datafp == NULL)
			fatal("mta: fdopen");
		mta_enter_state(s, MTA_CONNECT);
		return;

	case IMSG_LKA_SECRET:
		/* LKA responded to AUTH lookup. */
		secret = imsg->data;
		s = mta_session_lookup(secret->id);
		s->secret = strdup(secret->secret);
		if (s->secret == NULL)
			fatal(NULL);
		else if (s->secret[0] == '\0') {
			mta_status(s, 1, "190 secrets lookup failed");
			mta_enter_state(s, MTA_DONE);
		} else
			mta_enter_state(s, MTA_MX);
		return;

	case IMSG_DNS_HOST:
		dns = imsg->data;
		s = mta_session_lookup(dns->id);
		relay = calloc(1, sizeof *relay);
		if (relay == NULL)
			fatal(NULL);
		relay->sa = dns->ss;
 		TAILQ_INSERT_TAIL(&s->relays, relay, entry);
		return;

	case IMSG_DNS_HOST_END:
		/* LKA responded to DNS lookup. */
		dns = imsg->data;
		s = mta_session_lookup(dns->id);
		if (dns->error) {
			if (dns->error == DNS_RETRY)
				mta_status(s, 1, "100 MX lookup failed temporarily");
			else if (dns->error == DNS_EINVAL)
				mta_status(s, 1, "600 Invalid domain name");
			else if (dns->error == DNS_ENONAME)
				mta_status(s, 1, "600 Domain does not exist");
			else if (dns->error == DNS_ENOTFOUND)
				mta_status(s, 1, "600 No MX address found for domain");
			mta_enter_state(s, MTA_DONE);
		} else
			mta_enter_state(s, MTA_DATA);
		return;

	case IMSG_DNS_PTR:
		dns = imsg->data;
		s = mta_session_lookup(dns->id);
		relay = TAILQ_FIRST(&s->relays);
		if (dns->error)
			strlcpy(relay->fqdn, "<unknown>", sizeof relay->fqdn);
		else
			strlcpy(relay->fqdn, dns->host, sizeof relay->fqdn);
		log_debug("mta: %p: connected to %s", s, relay->fqdn);

		/* check if we need to start tls now... */
		if (((s->flags & MTA_FORCE_ANYSSL) && relay->used == 1) ||
		    (s->flags & MTA_FORCE_SMTPS)) {
			log_debug("mta: %p: trying smtps (ssl=%p)...", s, s->ssl);
			ptr = ssl_mta_init(s->ssl);
			if (ptr == NULL)
				fatalx("mta: ssl_mta_init");

			io_start_tls(&s->io, ptr);
		} else {
			mta_enter_state(s, MTA_SMTP_BANNER);
		}
		break;
	default:
		errx(1, "mta_session_imsg: unexpected %s imsg",
		    imsg_to_str(imsg->hdr.type));
	}
}

static void
mta_enter_state(struct mta_session *s, int newstate)
{
	int			 oldstate;
	struct secret		 secret;
	struct mta_relay	*relay;
	struct mta_task		*task;
	struct sockaddr		*sa;
	struct envelope		*e;
	int			 max_reuse;
	ssize_t			 q;
	struct mta_batch	 batch;

    again:
	task = TAILQ_FIRST(&s->tasks);
	oldstate = s->state;

	log_trace(TRACE_MTA, "mta: %p: %s -> %s", s,
	    mta_strstate(oldstate),
	    mta_strstate(newstate));

	s->state = newstate;

	/* don't try this at home! */
#define mta_enter_state(_s, _st) do { newstate = _st; goto again; } while(0)

	switch (s->state) {
	case MTA_INIT:
		if (s->flags & MTA_USE_AUTH)
			mta_enter_state(s, MTA_SECRET);
		else
			mta_enter_state(s, MTA_MX);
		break;

	case MTA_DATA:
		/*
		 * Obtain message body fd.
		 */
		e = TAILQ_FIRST(&task->envelopes);
		batch.id = s->id;
		batch.msgid = evpid_to_msgid(e->id);
		imsg_compose_event(env->sc_ievs[PROC_QUEUE],
		    IMSG_QUEUE_MESSAGE_FD, 0, 0, -1, &batch, sizeof(batch));
		break;

	case MTA_SECRET:
		/*
		 * Lookup AUTH secret.
		 */
		bzero(&secret, sizeof(secret));
		secret.id = s->id;
		strlcpy(secret.mapname, s->authmap, sizeof(secret.mapname));
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

	case MTA_CONNECT:
		/*
		 * Connect to the MX.
		 */
		if (oldstate == MTA_CONNECT) {
			/* previous connection failed. clean it up */
			iobuf_clear(&s->iobuf);
			io_clear(&s->io);
		}

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

			log_debug("mta: %p: connecting to %s...", s,
				ss_to_text(&relay->sa));
			sa = (struct sockaddr *)&relay->sa;

			if (s->port)
				sa_set_port(sa, s->port);
			else if ((s->flags & MTA_FORCE_ANYSSL) && relay->used == 1)
				sa_set_port(sa, 465);
			else if (s->flags & MTA_FORCE_SMTPS)
				sa_set_port(sa, 465);
			else
				sa_set_port(sa, 25);

			iobuf_init(&s->iobuf, 0, 0);
			io_init(&s->io, -1, s, mta_io, &s->iobuf);
			io_set_timeout(&s->io, 10000);
			if (io_connect(&s->io, sa) == -1) {
				log_debug("mta: %p: connection failed: %s", s,
				    strerror(errno));
				iobuf_clear(&s->iobuf);
				/*
				 * This error is most likely a "no route",
				 * so there is no need to try the same
				 * relay again.
				 */
				TAILQ_REMOVE(&s->relays, relay, entry);
				free(relay);
				continue;
			}
			return;
		}
		/* tried them all? */
		mta_status(s, 1, "150 Can not connect to MX");
		mta_enter_state(s, MTA_DONE);
		break;

	case MTA_DONE:
		/*
		 * Kill the mta session.
		 */

		log_debug("mta: %p: deleting session...", s);
		io_clear(&s->io);
		iobuf_clear(&s->iobuf);

		if (TAILQ_FIRST(&s->tasks))
			fatalx("all tasks should have been deleted already");

		imsg_compose_event(env->sc_ievs[PROC_QUEUE],
		    IMSG_BATCH_DONE, 0, 0, -1, NULL, 0);

		/* deallocate resources */
		SPLAY_REMOVE(mta_session_tree, &mta_sessions, s);
		while ((relay = TAILQ_FIRST(&s->relays))) {
			TAILQ_REMOVE(&s->relays, relay, entry);
			free(relay);
		}

		if (s->datafp)
			fclose(s->datafp);

		free(s->authmap);
		free(s->secret);
		free(s->host);
		free(s);
		break;

	case MTA_SMTP_BANNER:
		/* just wait for banner */
		io_set_read(&s->io);
		break;

	case MTA_SMTP_EHLO:
		s->ext = 0;
		mta_send(s, "EHLO %s", env->sc_hostname);
		break;

	case MTA_SMTP_HELO:
		s->ext = 0;
		mta_send(s, "HELO %s", env->sc_hostname);
		break;

	case MTA_SMTP_STARTTLS:
		if (s->flags & MTA_TLS) /* already started */
			mta_enter_state(s, MTA_SMTP_AUTH);
		else if ((s->ext & MTA_EXT_STARTTLS) == 0)
			/* server doesn't support starttls, do not use it */
			mta_enter_state(s, MTA_SMTP_AUTH);
		else
			mta_send(s, "STARTTLS");
		break;

	case MTA_SMTP_AUTH:
		if (s->secret && s->flags & MTA_TLS)
			mta_send(s, "AUTH PLAIN %s", s->secret);
		else if (s->secret) {
			log_debug("mta: %p: not using AUTH on non-TLS session",
			    s);
			mta_enter_state(s, MTA_CONNECT);
		} else
			mta_enter_state(s, MTA_SMTP_READY);
		break;

	case MTA_SMTP_READY:
		/* ready to send a new mail */
		if (task)
			mta_enter_state(s, MTA_SMTP_MAIL);
		else
			mta_enter_state(s, MTA_SMTP_QUIT);
		break;

	case MTA_SMTP_MAIL:
		if (task->sender.user[0] &&
		    task->sender.domain[0])
			mta_send(s, "MAIL FROM: <%s@%s>",
			    task->sender.user,
			    task->sender.domain);
		else
			mta_send(s, "MAIL FROM: <>");
		break;

	case MTA_SMTP_RCPT:
		if (s->currevp == NULL)
			s->currevp = TAILQ_FIRST(&task->envelopes);
		mta_send(s, "RCPT TO: <%s@%s>",
		    s->currevp->dest.user,
		    s->currevp->dest.domain);
		break;

	case MTA_SMTP_DATA:
		fseek(s->datafp, 0, SEEK_SET);
		mta_send(s, "DATA");
		break;

	case MTA_SMTP_BODY:
		if (s->datafp == NULL) {
			log_debug("mta: %p: end-of-file", s);
			mta_enter_state(s, MTA_SMTP_DONE);
			break;
		}

		if ((q = mta_queue_data(s)) == -1) {
			mta_enter_state(s, MTA_DONE);
			break;
		}

		log_trace(TRACE_MTA, "mta: %p: >>> [...%zi bytes...]", s, q);
		break;

	case MTA_SMTP_DONE:
		mta_send(s, ".");
		break;

	case MTA_SMTP_QUIT:
		mta_send(s, "QUIT");
		break;

	case MTA_SMTP_RSET:
		if (task == NULL)
			mta_enter_state(s, MTA_SMTP_QUIT);
		else
			mta_send(s, "RSET");
		break;

	default:
		fatal("mta_enter_state: unknown state");
	}
#undef mta_enter_state
}

/*
 * Handle a response to an SMTP command
 */
static void
mta_response(struct mta_session *s, char *line)
{
	void		*ssl;
	struct envelope	*evp;
	struct mta_task	*task = TAILQ_FIRST(&s->tasks);

	switch (s->state) {

	case MTA_SMTP_BANNER:
		mta_enter_state(s, MTA_SMTP_EHLO);
		break;

	case MTA_SMTP_EHLO:
		if (line[0] != '2') {
			if ((s->flags & MTA_USE_AUTH) ||
			    !(s->flags & MTA_ALLOW_PLAIN)) {
				mta_status(s, 1, line);
				mta_enter_state(s, MTA_DONE);
				return;
			}
			mta_enter_state(s, MTA_SMTP_HELO);
			return;
		}
		mta_enter_state(s, MTA_SMTP_STARTTLS);
		break;

	case MTA_SMTP_HELO:
		if (line[0] != '2') {
			mta_status(s, 1, line);
			mta_enter_state(s, MTA_DONE);
			return;
		}
		mta_enter_state(s, MTA_SMTP_READY);
		break;

	case MTA_SMTP_STARTTLS:
		if (line[0] != '2') {
			if (s->flags & MTA_ALLOW_PLAIN) {
				mta_enter_state(s, MTA_SMTP_AUTH);
				return;
			}
			/* stop here if ssl can't be used */
			mta_status(s, 1, line);
			mta_enter_state(s, MTA_DONE);
			return;
		}
		ssl = ssl_mta_init(s->ssl);
		if (ssl == NULL)
			fatal("mta: ssl_mta_init");
		io_set_write(&s->io);
		io_start_tls(&s->io, ssl);
		break;

	case MTA_SMTP_AUTH:
		if (line[0] != '2') {
			mta_status(s, 1, line);
			mta_enter_state(s, MTA_DONE);
			return;
		}
		mta_enter_state(s, MTA_SMTP_READY);
		break;

	case MTA_SMTP_MAIL:
		if (line[0] != '2') {
			mta_status(s, 0, line);
			mta_enter_state(s, MTA_SMTP_RSET);
			return;
		}
		mta_enter_state(s, MTA_SMTP_RCPT);
		break;

	case MTA_SMTP_RCPT:
		evp = s->currevp;
		s->currevp = TAILQ_NEXT(s->currevp, entry);
		if (line[0] != '2')
			mta_envelope_done(task, evp, line);
		if (s->currevp == NULL)
			mta_enter_state(s, MTA_SMTP_DATA);
		else
			mta_enter_state(s, MTA_SMTP_RCPT);
		break;

	case MTA_SMTP_DATA:
		if (line[0] != '2' && line[0] != '3') {
			mta_status(s, 0, line);
			mta_enter_state(s, MTA_SMTP_RSET);
			return;
		}
		mta_enter_state(s, MTA_SMTP_BODY);
		break;

	case MTA_SMTP_DONE:
		mta_status(s, 0, line);
		mta_enter_state(s, MTA_SMTP_READY);
		break;

	case MTA_SMTP_RSET:
		mta_enter_state(s, MTA_SMTP_READY);
		break;

	default:
		fatal("mta_response() bad state");
	}
}

static void
mta_io(struct io *io, int evt)
{
	struct mta_session	*s = io->arg;
	char			*line, *msg;
	ssize_t			 len;
	struct mta_relay	*relay;
	const char		*error;
	int			 cont;

	log_trace(TRACE_IO, "mta: %p: %s %s", s, io_strevent(evt), io_strio(io));

	switch (evt) {

	case IO_CONNECTED:
		io_set_timeout(io, 300000);
		io_set_write(io);
		relay = TAILQ_FIRST(&s->relays);
		dns_query_ptr(&relay->sa, s->id);
		break;

	case IO_TLSREADY:
		s->flags |= MTA_TLS;
		if (s->state == MTA_CONNECT) /* smtps */
			mta_enter_state(s, MTA_SMTP_BANNER);
		else
			mta_enter_state(s, MTA_SMTP_EHLO);
		break;

	case IO_DATAIN:
	    nextline:

		line = iobuf_getline(&s->iobuf, &len);
		if (line == NULL) {
			if (iobuf_len(&s->iobuf) >= SMTP_LINE_MAX) {
				mta_status(s, 1, "150 Input too long");
				mta_enter_state(s, MTA_DONE);
				return;
			}
			iobuf_normalize(&s->iobuf);
			break;
		}

		log_trace(TRACE_MTA, "mta: %p: <<< %s", s, line);

		if ((error = parse_smtp_response(line, len, &msg, &cont))) {
			mta_status(s, 1, "150 Bad response: %s", error);
			mta_enter_state(s, MTA_DONE);
			return;
		}

		/* read extensions */
		if (s->state == MTA_SMTP_EHLO) {
			if (strcmp(msg, "STARTTLS") == 0)
				s->ext |= MTA_EXT_STARTTLS;
			else if (strncmp(msg, "AUTH", 4) == 0)
				s->ext |= MTA_EXT_AUTH;
			else if (strcmp(msg, "PIPELINING") == 0)
				s->ext |= MTA_EXT_PIPELINING;
		}

		if (cont)
			goto nextline;

		if (s->state == MTA_SMTP_QUIT) {
			mta_enter_state(s, MTA_DONE);
			return;
		}

		mta_response(s, line);

		iobuf_normalize(&s->iobuf);
		if (iobuf_queued(&s->iobuf))
			io_set_write(io);
		break;

	case IO_LOWAT:
		if (s->state == MTA_SMTP_BODY)
			mta_enter_state(s, MTA_SMTP_BODY);

		if (iobuf_queued(&s->iobuf) == 0)
			io_set_read(io);
		break;

	case IO_TIMEOUT:
		log_debug("mta: %p: connection timeout", s);
		if (s->state == MTA_CONNECT) {
			/* try the next MX */
			mta_enter_state(s, MTA_CONNECT);
			break;
		}
		mta_status(s, 1, "150 connection timeout");
		mta_enter_state(s, MTA_DONE);
		break;

	case IO_ERROR:
		log_debug("mta: %p: IO error: %s", s, strerror(errno));
		if (s->state == MTA_CONNECT) {
			mta_enter_state(s, MTA_CONNECT);
			break;
		}
		mta_status(s, 1, "150 IO error");
		mta_enter_state(s, MTA_DONE);
		break;

	case IO_DISCONNECTED:
		log_debug("mta: %p: disconnected in state %s", s, mta_strstate(s->state));
		if (s->state == MTA_CONNECT) {
			mta_enter_state(s, MTA_CONNECT);
			break;
		}
		mta_status(s, 1, "150 connection closed unexpectedly");
		mta_enter_state(s, MTA_DONE);
		break;

	default:
		fatal("mta_io()");
	}
}

static void
mta_send(struct mta_session *s, char *fmt, ...)
{
        va_list  ap;
        char    *p;
        int      len;

        va_start(ap, fmt);
        if ((len = vasprintf(&p, fmt, ap)) == -1)
                fatal("mta: vasprintf");
        va_end(ap);

	log_trace(TRACE_MTA, "mta: %p: >>> %s", s, p);

	iobuf_fqueue(&s->iobuf, "%s\r\n", p);

        free(p);
}

/*
 * Queue some data into the input buffer
 */
static ssize_t
mta_queue_data(struct mta_session *s)
{
	char	*ln;
        size_t	 len, q;

	q = iobuf_queued(&s->iobuf);

	while (iobuf_queued(&s->iobuf) < MTA_HIWAT) {
                if ((ln = fgetln(s->datafp, &len)) == NULL)
                        break;
                if (ln[len - 1] == '\n')
                        len--;
		if (*ln == '.')
			iobuf_queue(&s->iobuf, ".", 1);
		iobuf_queue(&s->iobuf, ln, len);
		iobuf_queue(&s->iobuf, "\r\n", 2);
	}

        if (ferror(s->datafp)) {
		mta_status(s, 1, "460 Error reading content file");
		return (-1);
	}

        if (feof(s->datafp)) {
		fclose(s->datafp);
		s->datafp = NULL;
	}

	return (iobuf_queued(&s->iobuf) - q);
}

static void
mta_status(struct mta_session *s, int alltasks, const char *fmt, ...)
{
	char			*status;
	struct envelope		*e;
	struct mta_task		*task;
	va_list			 ap;

	va_start(ap, fmt);
	if (vasprintf(&status, fmt, ap) == -1)
		fatal("vasprintf");
	va_end(ap);

	log_debug("mta: %p: new status for %s: %s", s,
	    alltasks ? "all tasks" : "current task", status);
	while ((task = TAILQ_FIRST(&s->tasks))) {
		while((e = TAILQ_FIRST(&task->envelopes)))
			if (mta_envelope_done(task, e, status))
				break;
		if (!alltasks)
			break;
	}

	free(status);
}

static int
mta_envelope_done(struct mta_task *task, struct envelope *e, const char *status)
{
	struct mta_session	*s = task->session;
	struct mta_relay	*relay = TAILQ_FIRST(&s->relays);
	u_int16_t		msg;

	envelope_set_errormsg(e, "%s", status);

	log_info("%016" PRIx64 ": to=<%s@%s>, delay=%" PRId64 ", relay=%s [%s], stat=%s (%s)",
	    e->id, e->dest.user,
	    e->dest.domain,
	    (int64_t) (time(NULL) - e->creation),
	    relay ? relay->fqdn : "(none)",
	    relay ? ss_to_text(&relay->sa) : "",
	    *status == '2' ? "Sent" :
	    *status == '5' ? "RemoteError" :
	    *status == '4' ? "RemoteError" : "LocalError",
	    status + 4);

	switch (e->errorline[0]) {
	case '2':
		msg = IMSG_QUEUE_DELIVERY_OK;
		break;
	case '5':
	case '6':
		msg = IMSG_QUEUE_DELIVERY_PERMFAIL;
		break;
	default:
		msg = IMSG_QUEUE_DELIVERY_TEMPFAIL;
		break;
	}
	imsg_compose_event(env->sc_ievs[PROC_QUEUE], msg,
	    0, 0, -1, e, sizeof(*e));
	TAILQ_REMOVE(&task->envelopes, e, entry);
	free(e);

	if (TAILQ_FIRST(&task->envelopes))
		return (0);

	mta_task_free(task);
	return (1);

}

static int
mta_session_cmp(struct mta_session *a, struct mta_session *b)
{
	return (a->id < b->id ? -1 : a->id > b->id);
}

static struct mta_session *
mta_session_lookup(u_int64_t id)
{
	struct mta_session	 key, *res;

	key.id = id;
	if ((res = SPLAY_FIND(mta_session_tree, &mta_sessions, &key)) == NULL)
		fatalx("mta_session_lookup: session not found");
	return (res);
}

SPLAY_GENERATE(mta_session_tree, mta_session, entry, mta_session_cmp);

static struct mta_task *
mta_task_create(u_int64_t id, struct mta_session *s)
{
	struct mta_task	 *t;

	if ((t = mta_task_lookup(id, 0)))
		fatalx("mta_task_create: duplicate task id");
	if ((t = calloc(1, sizeof(*t))) == NULL)
		return (NULL);

	t->id = id;
	t->session = s;
	SPLAY_INSERT(mta_task_tree, &mta_tasks, t);
	TAILQ_INIT(&t->envelopes);
	if (s)
		TAILQ_INSERT_TAIL(&s->tasks, t, list);

	return (t);
}

static int
mta_task_cmp(struct mta_task *a, struct mta_task *b)
{
	return (a->id < b->id ? -1 : a->id > b->id);
}

static struct mta_task *
mta_task_lookup(u_int64_t id, int strict)
{
	struct mta_task	 key, *res;

	key.id = id;
	res = SPLAY_FIND(mta_task_tree, &mta_tasks, &key);
	if (res == NULL && strict)
		fatalx("mta_task_lookup: task not found");
	return (res);
}

static void
mta_task_free(struct mta_task *t)
{
	struct envelope	*e;

	if (t->session)
		TAILQ_REMOVE(&t->session->tasks, t, list);

	SPLAY_REMOVE(mta_task_tree, &mta_tasks, t);

	while ((e = TAILQ_FIRST(&t->envelopes))) {
		TAILQ_REMOVE(&t->envelopes, e, entry);
		free(e);
	}

	free(t);
}

SPLAY_GENERATE(mta_task_tree, mta_task, entry, mta_task_cmp);

#define CASE(x) case x : return #x

static const char *
mta_strstate(int state)
{
	switch (state) {
	CASE(MTA_INIT);
	CASE(MTA_SECRET);
	CASE(MTA_DATA);
	CASE(MTA_MX);
	CASE(MTA_CONNECT);
	CASE(MTA_DONE);
	CASE(MTA_SMTP_READY);
	CASE(MTA_SMTP_BANNER);  
	CASE(MTA_SMTP_EHLO);
	CASE(MTA_SMTP_HELO);
	CASE(MTA_SMTP_STARTTLS);
	CASE(MTA_SMTP_AUTH);                             
	CASE(MTA_SMTP_MAIL);
	CASE(MTA_SMTP_RCPT);
	CASE(MTA_SMTP_DATA);
	CASE(MTA_SMTP_QUIT);
	CASE(MTA_SMTP_BODY);
	CASE(MTA_SMTP_DONE);
	CASE(MTA_SMTP_RSET);
	default:
		return "MTA_???";
	}
}
