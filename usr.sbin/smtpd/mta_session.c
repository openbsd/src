/*	$OpenBSD: mta_session.c,v 1.16 2012/09/11 16:24:28 eric Exp $	*/

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

#define MTA_HIWAT	65535

enum mta_state {
	MTA_INIT,
	MTA_SECRET,
	MTA_DATA,
	MTA_MX,
	MTA_CONNECT,
	MTA_DONE,
	MTA_SMTP_READY,
	MTA_SMTP_BANNER,
	MTA_SMTP_EHLO,
	MTA_SMTP_HELO,
	MTA_SMTP_STARTTLS,
	MTA_SMTP_AUTH,
	MTA_SMTP_MAIL,
	MTA_SMTP_RCPT,
	MTA_SMTP_DATA,
	MTA_SMTP_QUIT,
	MTA_SMTP_BODY,
	MTA_SMTP_DONE,
	MTA_SMTP_RSET,
};

#define MTA_FORCE_ANYSSL	0x01
#define MTA_FORCE_SMTPS		0x02
#define MTA_ALLOW_PLAIN		0x04
#define MTA_USE_AUTH		0x08
#define MTA_FORCE_MX		0x10
#define MTA_USE_CERT		0x20
#define MTA_TLS			0x40

#define MTA_EXT_STARTTLS	0x01
#define MTA_EXT_AUTH		0x02
#define MTA_EXT_PIPELINING	0x04

struct mta_host {
	TAILQ_ENTRY(mta_host)	 entry;
	struct sockaddr_storage	 sa;
	char			 fqdn[MAXHOSTNAMELEN];
	int			 used;
};

struct mta_session {
	uint64_t		 id;
	struct mta_route	*route;

	char			*secret;
	int			 flags;
	int			 ready;

	int			 msgcount;
	enum mta_state		 state;
	TAILQ_HEAD(, mta_host)	 hosts;
	struct mta_task		*task;
	FILE			*datafp;
	struct envelope		*currevp;
	struct iobuf		 iobuf;
	struct io		 io;
	int			 is_reading; /* XXX remove this later */
	int			 ext;
	struct ssl		*ssl;
};

static void mta_io(struct io *, int);
static void mta_enter_state(struct mta_session *, int);
static void mta_status(struct mta_session *, int, const char *, ...);
static void mta_envelope_done(struct mta_task *, struct envelope *, const char *);
static void mta_send(struct mta_session *, char *, ...);
static ssize_t mta_queue_data(struct mta_session *);
static void mta_response(struct mta_session *, char *);
static const char * mta_strstate(int);
static int mta_check_loop(FILE *);

static struct tree sessions = SPLAY_INITIALIZER(&sessions);

void
mta_session(struct mta_route *route)
{
	struct mta_session	*session;

	session = xcalloc(1, sizeof *session, "mta_session");
	session->id = generate_uid();
	session->route = route;
	session->state = MTA_INIT;
	session->io.sock = -1;
	tree_xset(&sessions, session->id, session);
	TAILQ_INIT(&session->hosts);

	if (route->flags & ROUTE_MX)
		session->flags |= MTA_FORCE_MX;
	if (route->flags & ROUTE_SSL && route->flags & ROUTE_AUTH)
		session->flags |= MTA_USE_AUTH;
	if (route->cert)
		session->flags |= MTA_USE_CERT;
	switch (route->flags & ROUTE_SSL) {
		case ROUTE_SSL:
			session->flags |= MTA_FORCE_ANYSSL;
			break;
		case ROUTE_SMTPS:
			session->flags |= MTA_FORCE_SMTPS;
			break;
		case ROUTE_STARTTLS:
			/* STARTTLS is tried by default */
			break;
		default:
			session->flags |= MTA_ALLOW_PLAIN;
	}

	log_debug("mta: %p: spawned for %s", session, mta_route_to_text(route));
	stat_increment("mta.session", 1);
	mta_enter_state(session, MTA_INIT);
}

void
mta_session_imsg(struct imsgev *iev, struct imsg *imsg)
{
	uint64_t		 id;
	struct mta_session	*s;
	struct mta_host		*host;
	struct secret		*secret;
	struct dns		*dns;
	const char		*error;
	void			*ptr;

	switch(imsg->hdr.type) {

	case IMSG_QUEUE_MESSAGE_FD:
		id = *(uint64_t*)(imsg->data);
		if (imsg->fd == -1)
			fatalx("mta: cannot obtain msgfd");
		s = tree_xget(&sessions, id);
		s->datafp = fdopen(imsg->fd, "r");
		if (s->datafp == NULL)
			fatal("mta: fdopen");

		if (mta_check_loop(s->datafp)) {
			log_debug("mta: loop detected");
			fclose(s->datafp);
			s->datafp = NULL;
			mta_status(s, 0, "646 Loop detected");
			mta_enter_state(s, MTA_SMTP_READY);
		} else {
			mta_enter_state(s, MTA_SMTP_MAIL);
		}
		return;

	case IMSG_LKA_SECRET:
		/* LKA responded to AUTH lookup. */
		secret = imsg->data;
		s = tree_xget(&sessions, secret->id);
		s->secret = xstrdup(secret->secret, "mta: secret");
		if (s->secret[0] == '\0') {
			mta_route_error(s->route, "secrets lookup failed");
			mta_enter_state(s, MTA_DONE);
		} else
			mta_enter_state(s, MTA_MX);
		return;

	case IMSG_DNS_HOST:
		dns = imsg->data;
		s = tree_xget(&sessions, dns->id);
		host = xcalloc(1, sizeof *host, "mta: host");
		host->sa = dns->ss;
		TAILQ_INSERT_TAIL(&s->hosts, host, entry);
		return;

	case IMSG_DNS_HOST_END:
		/* LKA responded to DNS lookup. */
		dns = imsg->data;
		s = tree_xget(&sessions, dns->id);
		if (!dns->error) {
			mta_enter_state(s, MTA_CONNECT);
			return;
		}
		if (dns->error == DNS_RETRY)
			error = "100 MX lookup failed temporarily";
		else if (dns->error == DNS_EINVAL)
			error = "600 Invalid domain name";
		else if (dns->error == DNS_ENONAME)
			error = "600 Domain does not exist";
		else if (dns->error == DNS_ENOTFOUND)
			error = "600 No MX address found for domain";
		else
			error = "100 Weird error";
		mta_route_error(s->route, error);
		mta_enter_state(s, MTA_CONNECT);
		return;

	case IMSG_DNS_PTR:
		dns = imsg->data;
		s = tree_xget(&sessions, dns->id);
		host = TAILQ_FIRST(&s->hosts);
		if (dns->error)
			strlcpy(host->fqdn, "<unknown>", sizeof host->fqdn);
		else
			strlcpy(host->fqdn, dns->host, sizeof host->fqdn);
		log_debug("mta: %p: connected to %s", s, host->fqdn);

		/* check if we need to start tls now... */
		if (((s->flags & MTA_FORCE_ANYSSL) && host->used == 1) ||
		    (s->flags & MTA_FORCE_SMTPS)) {
			log_debug("mta: %p: trying smtps (ssl=%p)...", s, s->ssl);
			if ((ptr = ssl_mta_init(s->ssl)) == NULL)
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
	struct mta_route	*route;
	struct mta_host		*host;
	struct sockaddr		*sa;
	int			 max_reuse;
	ssize_t			 q;

    again:
	oldstate = s->state;

	log_trace(TRACE_MTA, "mta: %p: %s -> %s", s,
	    mta_strstate(oldstate),
	    mta_strstate(newstate));

	s->state = newstate;

	/* don't try this at home! */
#define mta_enter_state(_s, _st) do { newstate = _st; goto again; } while(0)

	switch (s->state) {
	case MTA_INIT:
		if (s->route->auth)
			mta_enter_state(s, MTA_SECRET);
		else
			mta_enter_state(s, MTA_MX);
		break;

	case MTA_DATA:
		/*
		 * Obtain message body fd.
		 */
		imsg_compose_event(env->sc_ievs[PROC_QUEUE],
		    IMSG_QUEUE_MESSAGE_FD, s->task->msgid, 0, -1,
		    &s->id, sizeof(s->id));
		break;

	case MTA_SECRET:
		/*
		 * Lookup AUTH secret.
		 */
		bzero(&secret, sizeof(secret));
		secret.id = s->id;
		strlcpy(secret.mapname, s->route->auth, sizeof(secret.mapname));
		strlcpy(secret.host, s->route->hostname, sizeof(secret.host));
		imsg_compose_event(env->sc_ievs[PROC_LKA], IMSG_LKA_SECRET,
		    0, 0, -1, &secret, sizeof(secret));  
		break;

	case MTA_MX:
		/*
		 * Lookup MX record.
		 */
		if (s->flags & MTA_FORCE_MX) /* XXX */
			dns_query_host(s->route->hostname, s->route->port, s->id);
		else
			dns_query_mx(s->route->hostname, s->route->backupname, 0, s->id);
		break;

	case MTA_CONNECT:
		/*
		 * Connect to the MX.
		 */
	
		/* cleanup previous connection if any */
		iobuf_clear(&s->iobuf);
		io_clear(&s->io);

		if (s->flags & MTA_FORCE_ANYSSL)
			max_reuse = 2;
		else
			max_reuse = 1;

		/* pick next mx */
		while ((host = TAILQ_FIRST(&s->hosts))) {
			if (host->used == max_reuse) {
				TAILQ_REMOVE(&s->hosts, host, entry);
				free(host);
				continue;
			}
			host->used++;

			log_debug("mta: %p: connecting to %s...", s,
				ss_to_text(&host->sa));
			sa = (struct sockaddr *)&host->sa;

			if (s->route->port)
				sa_set_port(sa, s->route->port);
			else if ((s->flags & MTA_FORCE_ANYSSL) && host->used == 1)
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
				TAILQ_REMOVE(&s->hosts, host, entry);
				free(host);
				continue;
			}
			return;
		}
		/* tried them all? */
		mta_route_error(s->route, "150 Can not connect to MX");
		mta_enter_state(s, MTA_DONE);
		break;

	case MTA_DONE:
		/*
		 * Kill the mta session.
		 */
		log_debug("mta: %p: session done", s);
		io_clear(&s->io);
		iobuf_clear(&s->iobuf);
		if (s->task)
			fatalx("current task should have been deleted already");
		if (s->datafp)
			fclose(s->datafp);
		s->datafp = NULL;
		while ((host = TAILQ_FIRST(&s->hosts))) {
			TAILQ_REMOVE(&s->hosts, host, entry);
			free(host);
		}
		route = s->route;
		tree_xpop(&sessions, s->id);
		free(s);
		stat_decrement("mta.session", 1);
		mta_route_collect(route);
		break;

	case MTA_SMTP_BANNER:
		/* just wait for banner */
		s->is_reading = 1;
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
		} else {
			mta_enter_state(s, MTA_SMTP_READY);
		}
		break;

	case MTA_SMTP_READY:
		/* ready to send a new mail */
		if (s->ready == 0) {
			s->ready = 1;
			mta_route_ok(s->route);
		}
		if (s->msgcount >= s->route->maxmail) {
			log_debug("mta: %p: cannot send more message to %s", s,
			    mta_route_to_text(s->route));
			mta_enter_state(s, MTA_SMTP_QUIT);
		} else if ((s->task = TAILQ_FIRST(&s->route->tasks))) {
			log_debug("mta: %p: handling next task for %s", s,
			    mta_route_to_text(s->route));
			TAILQ_REMOVE(&s->route->tasks, s->task, entry);
			s->route->ntask -= 1;
			s->task->session = s;
			stat_decrement("mta.task", 1);
			stat_increment("mta.task.running", 1);
			mta_enter_state(s, MTA_DATA);
		} else {
			log_debug("mta: %p: no pending task for %s", s,
			    mta_route_to_text(s->route));
			/* XXX stay open for a while? */
			mta_enter_state(s, MTA_SMTP_QUIT);
		}
		break;

	case MTA_SMTP_MAIL:
		if (s->task->sender.user[0] && s->task->sender.domain[0])
			mta_send(s, "MAIL FROM: <%s@%s>",
			    s->task->sender.user, s->task->sender.domain);
		else
			mta_send(s, "MAIL FROM: <>");
		break;

	case MTA_SMTP_RCPT:
		if (s->currevp == NULL)
			s->currevp = TAILQ_FIRST(&s->task->envelopes);
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
			log_trace(TRACE_MTA, "mta: %p: end-of-file", s);
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

	switch (s->state) {

	case MTA_SMTP_BANNER:
		mta_enter_state(s, MTA_SMTP_EHLO);
		break;

	case MTA_SMTP_EHLO:
		if (line[0] != '2') {
			if ((s->flags & MTA_USE_AUTH) ||
			    !(s->flags & MTA_ALLOW_PLAIN)) {
				mta_route_error(s->route, line);
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
			mta_route_error(s->route, line);
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
			mta_route_error(s->route, line);
			mta_enter_state(s, MTA_DONE);
			return;
		}
		ssl = ssl_mta_init(s->ssl);
		if (ssl == NULL)
			fatal("mta: ssl_mta_init");
		s->is_reading = 0;
		io_set_write(&s->io);
		io_start_tls(&s->io, ssl);
		break;

	case MTA_SMTP_AUTH:
		if (line[0] != '2') {
			mta_route_error(s->route, line);
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
		if (line[0] != '2') {
			mta_envelope_done(s->task, evp, line);
			if (TAILQ_EMPTY(&s->task->envelopes)) {
				free(s->task);
				s->task = NULL;
				stat_decrement("mta.task.running", 1);
				mta_enter_state(s, MTA_SMTP_RSET);
				break;
			}
		}
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
		if (line[0] == '2')
			s->msgcount++;
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
	struct mta_host		*host;
	const char		*error;
	int			 cont;

	log_trace(TRACE_IO, "mta: %p: %s %s", s, io_strevent(evt), io_strio(io));

	switch (evt) {

	case IO_CONNECTED:
		s->is_reading = 0;
		io_set_timeout(io, 300000);
		io_set_write(io);
		host = TAILQ_FIRST(&s->hosts);
		dns_query_ptr(&host->sa, s->id);
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
		break;

	case IO_LOWAT:
		if (s->state == MTA_SMTP_BODY)
			mta_enter_state(s, MTA_SMTP_BODY);

		if (iobuf_queued(&s->iobuf) == 0) {
			s->is_reading = 1;
			io_set_read(io);
		}
		break;

	case IO_TIMEOUT:
		log_debug("mta: %p: connection timeout", s);
		if (!s->ready) {
			mta_enter_state(s, MTA_CONNECT);
			break;
		}
		mta_status(s, 1, "150 connection timeout");
		mta_enter_state(s, MTA_DONE);
		break;

	case IO_ERROR:
		log_debug("mta: %p: IO error: %s", s, strerror(errno));
		if (!s->ready) {
			mta_enter_state(s, MTA_CONNECT);
			break;
		}
		mta_status(s, 1, "150 IO error");
		mta_enter_state(s, MTA_DONE);
		break;

	case IO_DISCONNECTED:
		log_debug("mta: %p: disconnected in state %s", s, mta_strstate(s->state));
		if (!s->ready) {
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
	char	*p;
	int	 len;

	va_start(ap, fmt);
	if ((len = vasprintf(&p, fmt, ap)) == -1)
		fatal("mta: vasprintf");
	va_end(ap);

	log_trace(TRACE_MTA, "mta: %p: >>> %s", s, p);

	iobuf_fqueue(&s->iobuf, "%s\r\n", p);

	free(p);

	if (s->is_reading) {
		s->is_reading = 0;
		io_set_write(&s->io);
	}
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

	if (s->is_reading) {
		s->is_reading = 0;
		io_set_write(&s->io);
	}

	return (iobuf_queued(&s->iobuf) - q);
}

static void
mta_status(struct mta_session *s, int connerr, const char *fmt, ...)
{
	struct envelope		*e;
	char			*status;
	va_list			 ap;

	va_start(ap, fmt);
	if (vasprintf(&status, fmt, ap) == -1)
		fatal("vasprintf");
	va_end(ap);

	if (s->task) {
		while((e = TAILQ_FIRST(&s->task->envelopes)))
			mta_envelope_done(s->task, e, status);
		free(s->task);
		s->task = NULL;
		stat_decrement("mta.task.running", 1);
	}

	if (connerr)
		mta_route_error(s->route, status);

	free(status);
}

static void
mta_envelope_done(struct mta_task *task, struct envelope *e, const char *status)
{
	struct	mta_host	*host = TAILQ_FIRST(&task->session->hosts);

	envelope_set_errormsg(e, "%s", status);

	log_info("%016" PRIx64 ": to=<%s@%s>, delay=%s, relay=%s [%s], stat=%s (%s)",
	    e->id, e->dest.user,
	    e->dest.domain,
	    duration_to_text(time(NULL) - e->creation),
	    host->fqdn,
	    ss_to_text(&host->sa),
	    mta_response_status(e->errorline),
	    mta_response_text(e->errorline));

	imsg_compose_event(env->sc_ievs[PROC_QUEUE],
	    mta_response_delivery(e->errorline), 0, 0, -1, e, sizeof(*e));

	TAILQ_REMOVE(&task->envelopes, e, entry);
	free(e);
	stat_decrement("mta.envelope", 1);
}

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

static int
mta_check_loop(FILE *fp)
{
	char	*buf, *lbuf;
	size_t	 len;
	uint32_t rcvcount = 0;
	int	 ret = 0;

	lbuf = NULL;
	while ((buf = fgetln(fp, &len))) {
		if (buf[len - 1] == '\n')
			buf[len - 1] = '\0';
		else {
			/* EOF without EOL, copy and add the NUL */
			if ((lbuf = malloc(len + 1)) == NULL)
				err(1, NULL);
			memcpy(lbuf, buf, len);
			lbuf[len] = '\0';
			buf = lbuf;
		}

		if (strchr(buf, ':') == NULL && !isspace((int)*buf))
			break;

		if (strncasecmp("Received: ", buf, 10) == 0) {
			rcvcount++;
			if (rcvcount == MAX_HOPS_COUNT) {
				ret = 1;
				break;
			}
		}
		if (lbuf) {
			free(lbuf);
			lbuf  = NULL;
		}
	}
	if (lbuf)
		free(lbuf);

	fseek(fp, SEEK_SET, 0);
	return ret;
}
