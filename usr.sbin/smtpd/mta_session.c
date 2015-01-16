/*	$OpenBSD: mta_session.c,v 1.70 2015/01/16 06:40:20 deraadt Exp $	*/

/*
 * Copyright (c) 2008 Pierre-Yves Ritschard <pyr@openbsd.org>
 * Copyright (c) 2008 Gilles Chehade <gilles@poolp.org>
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
#include <sys/socket.h>
#include <sys/uio.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <event.h>
#include <imsg.h>
#include <inttypes.h>
#include <netdb.h>
#include <openssl/ssl.h>
#include <pwd.h>
#include <resolv.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "smtpd.h"
#include "log.h"
#include "ssl.h"

#define MAX_TRYBEFOREDISABLE	10

#define MTA_HIWAT		65535

enum mta_state {
	MTA_INIT,
	MTA_BANNER,
	MTA_EHLO,
	MTA_HELO,
	MTA_LHLO,
	MTA_STARTTLS,
	MTA_AUTH,
	MTA_AUTH_PLAIN,
	MTA_AUTH_LOGIN,
	MTA_AUTH_LOGIN_USER,
	MTA_AUTH_LOGIN_PASS,
	MTA_READY,
	MTA_MAIL,
	MTA_RCPT,
	MTA_DATA,
	MTA_BODY,
	MTA_EOM,
	MTA_LMTP_EOM,
	MTA_RSET,
	MTA_QUIT,
};

#define MTA_FORCE_ANYSSL	0x0001
#define MTA_FORCE_SMTPS		0x0002
#define MTA_FORCE_TLS     	0x0004
#define MTA_FORCE_PLAIN		0x0008
#define MTA_WANT_SECURE		0x0010
#define MTA_USE_AUTH		0x0020
#define MTA_USE_CERT		0x0040
#define MTA_DOWNGRADE_PLAIN    	0x0080

#define MTA_TLS_TRIED		0x0080

#define MTA_TLS			0x0100
#define MTA_VERIFIED   		0x0200

#define MTA_FREE		0x0400
#define MTA_LMTP		0x0800
#define MTA_WAIT		0x1000
#define MTA_HANGON		0x2000
#define MTA_RECONN		0x4000

#define MTA_EXT_STARTTLS	0x01
#define MTA_EXT_PIPELINING	0x02
#define MTA_EXT_AUTH		0x04
#define MTA_EXT_AUTH_PLAIN     	0x08
#define MTA_EXT_AUTH_LOGIN     	0x10

struct mta_session {
	uint64_t		 id;
	struct mta_relay	*relay;
	struct mta_route	*route;
	char			*helo;

	int			 flags;

	int			 attempt;
	int			 use_smtps;
	int			 use_starttls;
	int			 use_smtp_tls;
	int			 ready;

	struct iobuf		 iobuf;
	struct io		 io;
	int			 ext;

	size_t			 msgtried;
	size_t			 msgcount;
	size_t			 rcptcount;
	int			 hangon;

	enum mta_state		 state;
	struct mta_task		*task;
	struct mta_envelope	*currevp;
	FILE			*datafp;

	size_t			 failures;
};

static void mta_session_init(void);
static void mta_start(int fd, short ev, void *arg);
static void mta_io(struct io *, int);
static void mta_free(struct mta_session *);
static void mta_on_ptr(void *, void *, void *);
static void mta_on_timeout(struct runq *, void *);
static void mta_connect(struct mta_session *);
static void mta_enter_state(struct mta_session *, int);
static void mta_flush_task(struct mta_session *, int, const char *, size_t, int);
static void mta_error(struct mta_session *, const char *, ...);
static void mta_send(struct mta_session *, char *, ...);
static ssize_t mta_queue_data(struct mta_session *);
static void mta_response(struct mta_session *, char *);
static const char * mta_strstate(int);
static void mta_start_tls(struct mta_session *);
static int mta_verify_certificate(struct mta_session *);
static struct mta_session *mta_tree_pop(struct tree *, uint64_t);
static const char * dsn_strret(enum dsn_ret);
static const char * dsn_strnotify(uint8_t);

void mta_hoststat_update(const char *, const char *);
void mta_hoststat_reschedule(const char *);
void mta_hoststat_cache(const char *, uint64_t);
void mta_hoststat_uncache(const char *, uint64_t);

static struct tree wait_helo;
static struct tree wait_ptr;
static struct tree wait_fd;
static struct tree wait_ssl_init;
static struct tree wait_ssl_verify;

static struct runq *hangon;

static void
mta_session_init(void)
{
	static int init = 0;

	if (!init) {
		tree_init(&wait_helo);
		tree_init(&wait_ptr);
		tree_init(&wait_fd);
		tree_init(&wait_ssl_init);
		tree_init(&wait_ssl_verify);
		runq_init(&hangon, mta_on_timeout);
		init = 1;
	}
}

void
mta_session(struct mta_relay *relay, struct mta_route *route)
{
	struct mta_session	*s;
	struct timeval		 tv;

	mta_session_init();

	s = xcalloc(1, sizeof *s, "mta_session");
	s->id = generate_uid();
	s->relay = relay;
	s->route = route;
	s->io.sock = -1;

	if (relay->flags & RELAY_SSL && relay->flags & RELAY_AUTH)
		s->flags |= MTA_USE_AUTH;
	if (relay->pki_name)
		s->flags |= MTA_USE_CERT;
	if (relay->flags & RELAY_LMTP)
		s->flags |= MTA_LMTP;
	switch (relay->flags & (RELAY_SSL|RELAY_TLS_OPTIONAL)) {
		case RELAY_SSL:
			s->flags |= MTA_FORCE_ANYSSL;
			s->flags |= MTA_WANT_SECURE;
			break;
		case RELAY_SMTPS:
			s->flags |= MTA_FORCE_SMTPS;
			s->flags |= MTA_WANT_SECURE;
			break;
		case RELAY_STARTTLS:
			s->flags |= MTA_FORCE_TLS;
			s->flags |= MTA_WANT_SECURE;
			break;
		case RELAY_TLS_OPTIONAL:
			/* do not force anything, try tls then smtp */
			break;
		default:
			s->flags |= MTA_FORCE_PLAIN;
	}

	if (relay->flags & RELAY_BACKUP)
		s->flags &= ~MTA_FORCE_PLAIN;

	log_debug("debug: mta: %p: spawned for relay %s", s,
	    mta_relay_to_text(relay));
	stat_increment("mta.session", 1);

	if (route->dst->ptrname || route->dst->lastptrquery) {
		/* We want to delay the connection since to always notify
		 * the relay asynchronously.
		 */
		tv.tv_sec = 0;
		tv.tv_usec = 0;
		evtimer_set(&s->io.ev, mta_start, s);
		evtimer_add(&s->io.ev, &tv);
	} else if (waitq_wait(&route->dst->ptrname, mta_on_ptr, s)) {
		m_create(p_lka,  IMSG_MTA_DNS_PTR, 0, 0, -1);
		m_add_id(p_lka, s->id);
		m_add_sockaddr(p_lka, s->route->dst->sa);
		m_close(p_lka);
		tree_xset(&wait_ptr, s->id, s);
		s->flags |= MTA_WAIT;
	}
}

void
mta_session_imsg(struct mproc *p, struct imsg *imsg)
{
	struct ca_vrfy_resp_msg	*resp_ca_vrfy;
	struct ca_cert_resp_msg	*resp_ca_cert;
	struct mta_session	*s;
	struct mta_host		*h;
	struct msg		 m;
	uint64_t		 reqid;
	const char		*name;
	void			*ssl;
	int			 dnserror, status;
	char			*pkiname;

	switch (imsg->hdr.type) {

	case IMSG_MTA_OPEN_MESSAGE:
		m_msg(&m, imsg);
		m_get_id(&m, &reqid);
		m_end(&m);

		s = mta_tree_pop(&wait_fd, reqid);
		if (s == NULL) {
			if (imsg->fd != -1)
				close(imsg->fd);
			return;
		}

		if (imsg->fd == -1) {
			log_debug("debug: mta: failed to obtain msg fd");
			mta_flush_task(s, IMSG_MTA_DELIVERY_TEMPFAIL,
			    "Could not get message fd", 0, 0);
			mta_enter_state(s, MTA_READY);
			io_reload(&s->io);
			return;
		}

		s->datafp = fdopen(imsg->fd, "r");
		if (s->datafp == NULL)
			fatal("mta: fdopen");

		mta_enter_state(s, MTA_MAIL);
		io_reload(&s->io);
		return;

	case IMSG_MTA_DNS_PTR:
		m_msg(&m, imsg);
		m_get_id(&m, &reqid);
		m_get_int(&m, &dnserror);
		if (dnserror)
			name = NULL;
		else
			m_get_string(&m, &name);
		m_end(&m);
		s = mta_tree_pop(&wait_ptr, reqid);
		if (s == NULL)
			return;

		h = s->route->dst;
		h->lastptrquery = time(NULL);
		if (name)
			h->ptrname = xstrdup(name, "mta: ptr");
		waitq_run(&h->ptrname, h->ptrname);
		return;

	case IMSG_MTA_SSL_INIT:
		resp_ca_cert = imsg->data;
		s = mta_tree_pop(&wait_ssl_init, resp_ca_cert->reqid);
		if (s == NULL)
			return;

		if (resp_ca_cert->status == CA_FAIL) {
			if (s->relay->pki_name) {
				log_info("smtp-out: Disconnecting session %016"PRIx64
				    ": CA failure", s->id);
				mta_free(s);
				return;
			}
			else {
				ssl = ssl_mta_init(NULL, NULL, 0);
				if (ssl == NULL)
					fatal("mta: ssl_mta_init");
				io_start_tls(&s->io, ssl);
				return;
			}
		}

		resp_ca_cert = xmemdup(imsg->data, sizeof *resp_ca_cert, "mta:ca_cert");
		resp_ca_cert->cert = xstrdup((char *)imsg->data +
		    sizeof *resp_ca_cert, "mta:ca_cert");
		if (s->relay->pki_name)
			pkiname = s->relay->pki_name;
		else
			pkiname = s->helo;
		ssl = ssl_mta_init(pkiname,
		    resp_ca_cert->cert, resp_ca_cert->cert_len);
		if (ssl == NULL)
			fatal("mta: ssl_mta_init");
		io_start_tls(&s->io, ssl);

		explicit_bzero(resp_ca_cert->cert, resp_ca_cert->cert_len);
		free(resp_ca_cert->cert);
		free(resp_ca_cert);
		return;

	case IMSG_MTA_SSL_VERIFY:
		resp_ca_vrfy = imsg->data;
		s = mta_tree_pop(&wait_ssl_verify, resp_ca_vrfy->reqid);
		if (s == NULL)
			return;

		if (resp_ca_vrfy->status == CA_OK)
			s->flags |= MTA_VERIFIED;
		else if (s->relay->flags & F_TLS_VERIFY) {
			errno = 0;
			mta_error(s, "SSL certificate check failed");
			mta_free(s);
			return;
		}

		mta_io(&s->io, IO_TLSVERIFIED);
		io_resume(&s->io, IO_PAUSE_IN);
		io_reload(&s->io);
		return;

	case IMSG_MTA_LOOKUP_HELO:
		m_msg(&m, imsg);
		m_get_id(&m, &reqid);
		m_get_int(&m, &status);
		if (status == LKA_OK)
			m_get_string(&m, &name);
		m_end(&m);

		s = mta_tree_pop(&wait_helo, reqid);
		if (s == NULL)
			return;

		if (status == LKA_OK) {
			s->helo = xstrdup(name, "mta_session_imsg");
			mta_connect(s);
		} else {
			mta_source_error(s->relay, s->route,
			    "Failed to retrieve helo string");
			mta_free(s);
		}
		return;

	default:
		errx(1, "mta_session_imsg: unexpected %s imsg",
		    imsg_to_str(imsg->hdr.type));
	}
}

static struct mta_session *
mta_tree_pop(struct tree *wait, uint64_t reqid)
{
	struct mta_session *s;

	s = tree_xpop(wait, reqid);
	if (s->flags & MTA_FREE) {
		log_debug("debug: mta: %p: zombie session", s);
		mta_free(s);
		return (NULL);
	}
	s->flags &= ~MTA_WAIT;

	return (s);
}

static void
mta_free(struct mta_session *s)
{
	struct mta_relay *relay;
	struct mta_route *route;

	log_debug("debug: mta: %p: session done", s);

	if (s->ready)
		s->relay->nconn_ready -= 1;

	if (s->flags & MTA_HANGON) {
		log_debug("debug: mta: %p: cancelling hangon timer", s);
		runq_cancel(hangon, NULL, s);
	}

	io_clear(&s->io);
	iobuf_clear(&s->iobuf);

	if (s->task)
		fatalx("current task should have been deleted already");
	if (s->datafp)
		fclose(s->datafp);
	if (s->helo)
		free(s->helo);

	relay = s->relay;
	route = s->route;
	free(s);
	stat_decrement("mta.session", 1);
	mta_route_collect(relay, route);
}

static void
mta_on_timeout(struct runq *runq, void *arg)
{
	struct mta_session *s = arg;

	log_debug("mta: timeout for session hangon");

	s->flags &= ~MTA_HANGON;
	s->hangon++;

	mta_enter_state(s, MTA_READY);
	io_reload(&s->io);
}

static void
mta_on_ptr(void *tag, void *arg, void *data)
{
	struct mta_session *s = arg;

	mta_connect(s);
}

static void
mta_start(int fd, short ev, void *arg)
{
	struct mta_session *s = arg;

	mta_connect(s);
}

static void
mta_connect(struct mta_session *s)
{
	struct sockaddr_storage	 ss;
	struct sockaddr		*sa;
	int			 portno;
	const char		*schema = "smtp+tls://";

	if (s->helo == NULL) {
		if (s->relay->helotable && s->route->src->sa) {
			m_create(p_lka, IMSG_MTA_LOOKUP_HELO, 0, 0, -1);
			m_add_id(p_lka, s->id);
			m_add_string(p_lka, s->relay->helotable);
			m_add_sockaddr(p_lka, s->route->src->sa);
			m_close(p_lka);
			tree_xset(&wait_helo, s->id, s);
			s->flags |= MTA_WAIT;
			return;
		}
		else if (s->relay->heloname)
			s->helo = xstrdup(s->relay->heloname, "mta_connect");
		else
			s->helo = xstrdup(env->sc_hostname, "mta_connect");
	}

	io_clear(&s->io);
	iobuf_clear(&s->iobuf);

	s->use_smtps = s->use_starttls = s->use_smtp_tls = 0;

	switch (s->attempt) {
	case 0:
		if (s->flags & MTA_FORCE_SMTPS)
			s->use_smtps = 1;	/* smtps */
		else if (s->flags & (MTA_FORCE_TLS|MTA_FORCE_ANYSSL))
			s->use_starttls = 1;	/* tls, tls+smtps */
		else if (!(s->flags & MTA_FORCE_PLAIN))
			s->use_smtp_tls = 1;
		break;
	case 1:
		if (s->flags & MTA_FORCE_ANYSSL) {
			s->use_smtps = 1;	/* tls+smtps */
			break;
		}
		else if (s->flags & MTA_DOWNGRADE_PLAIN) {
			/* smtp+tls, with tls failure */
			break;
		}
	default:
		mta_free(s);
		return;
	}
	portno = s->use_smtps ? 465 : 25;

	/* Override with relay-specified port */
	if (s->relay->port)
		portno = s->relay->port;

	memmove(&ss, s->route->dst->sa, s->route->dst->sa->sa_len);
	sa = (struct sockaddr *)&ss;

	if (sa->sa_family == AF_INET)
		((struct sockaddr_in *)sa)->sin_port = htons(portno);
	else if (sa->sa_family == AF_INET6)
		((struct sockaddr_in6 *)sa)->sin6_port = htons(portno);

	s->attempt += 1;
	if (s->use_smtp_tls)
		schema = "smtp+tls://";
	else if (s->use_starttls)
		schema = "tls://";
	else if (s->use_smtps)
		schema = "smtps://";
	else if (s->flags & MTA_LMTP)
		schema = "lmtp://";
	else
		schema = "smtp://";

	log_info("smtp-out: Connecting to %s%s:%d (%s) on session"
	    " %016"PRIx64"...", schema, sa_to_text(s->route->dst->sa),
	    portno, s->route->dst->ptrname, s->id);

	mta_enter_state(s, MTA_INIT);
	iobuf_xinit(&s->iobuf, 0, 0, "mta_connect");
	io_init(&s->io, -1, s, mta_io, &s->iobuf);
	io_set_timeout(&s->io, 300000);
	if (io_connect(&s->io, sa, s->route->src->sa) == -1) {
		/*
		 * This error is most likely a "no route",
		 * so there is no need to try again.
		 */
		log_debug("debug: mta: io_connect failed: %s", s->io.error);
		if (errno == EADDRNOTAVAIL)
			mta_source_error(s->relay, s->route, s->io.error);
		else
			mta_error(s, "Connection failed: %s", s->io.error);
		mta_free(s);
	}
}

static void
mta_enter_state(struct mta_session *s, int newstate)
{
	struct mta_envelope	 *e;
	size_t			 envid_sz;
	int			 oldstate;
	ssize_t			 q;
	char			 ibuf[SMTPD_MAXLINESIZE];
	char			 obuf[SMTPD_MAXLINESIZE];
	int			 offset;

    again:
	oldstate = s->state;

	log_trace(TRACE_MTA, "mta: %p: %s -> %s", s,
	    mta_strstate(oldstate),
	    mta_strstate(newstate));

	s->state = newstate;

	/* don't try this at home! */
#define mta_enter_state(_s, _st) do { newstate = _st; goto again; } while (0)

	switch (s->state) {
	case MTA_INIT:
	case MTA_BANNER:
		break;

	case MTA_EHLO:
		s->ext = 0;
		mta_send(s, "EHLO %s", s->helo);
		break;

	case MTA_HELO:
		s->ext = 0;
		mta_send(s, "HELO %s", s->helo);
		break;

	case MTA_LHLO:
		s->ext = 0;
		mta_send(s, "LHLO %s", s->helo);
		break;

	case MTA_STARTTLS:
		if (s->flags & MTA_DOWNGRADE_PLAIN)
			mta_enter_state(s, MTA_AUTH);
		if (s->flags & MTA_TLS) /* already started */
			mta_enter_state(s, MTA_AUTH);
		else if ((s->ext & MTA_EXT_STARTTLS) == 0) {
			if (s->flags & MTA_FORCE_TLS || s->flags & MTA_WANT_SECURE) {
				mta_error(s, "TLS required but not supported by remote host");
				s->flags |= MTA_RECONN;
			}
			else
				/* server doesn't support starttls, do not use it */
				mta_enter_state(s, MTA_AUTH);
		}
		else
			mta_send(s, "STARTTLS");
		break;

	case MTA_AUTH:
		if (s->relay->secret && s->flags & MTA_TLS) {
			if (s->ext & MTA_EXT_AUTH) {
				if (s->ext & MTA_EXT_AUTH_PLAIN) {
					mta_enter_state(s, MTA_AUTH_PLAIN);
					break;
				}
				if (s->ext & MTA_EXT_AUTH_LOGIN) {
					mta_enter_state(s, MTA_AUTH_LOGIN);
					break;
				}
				log_debug("debug: mta: %p: no supported AUTH method on session", s);
				mta_error(s, "no supported AUTH method");
			}
			else {
				log_debug("debug: mta: %p: AUTH not advertised on session", s);
				mta_error(s, "AUTH not advertised");
			}
		}
		else if (s->relay->secret) {
			log_debug("debug: mta: %p: not using AUTH on non-TLS "
			    "session", s);
			mta_error(s, "Refuse to AUTH over unsecure channel");
			mta_connect(s);
		} else {
			mta_enter_state(s, MTA_READY);
		}
		break;

	case MTA_AUTH_PLAIN:
		mta_send(s, "AUTH PLAIN %s", s->relay->secret);
		break;

	case MTA_AUTH_LOGIN:
		mta_send(s, "AUTH LOGIN");
		break;

	case MTA_AUTH_LOGIN_USER:
		memset(ibuf, 0, sizeof ibuf);
		if (base64_decode(s->relay->secret, (unsigned char *)ibuf,
				  sizeof(ibuf)-1) == -1) {
			log_debug("debug: mta: %p: credentials too large on session", s);
			mta_error(s, "Credentials too large");
			break;
		}

		memset(obuf, 0, sizeof obuf);
		base64_encode((unsigned char *)ibuf + 1, strlen(ibuf + 1), obuf, sizeof obuf);
		mta_send(s, "%s", obuf);

		memset(ibuf, 0, sizeof ibuf);
		memset(obuf, 0, sizeof obuf);
		break;

	case MTA_AUTH_LOGIN_PASS:
		memset(ibuf, 0, sizeof ibuf);
		if (base64_decode(s->relay->secret, (unsigned char *)ibuf,
				  sizeof(ibuf)-1) == -1) {
			log_debug("debug: mta: %p: credentials too large on session", s);
			mta_error(s, "Credentials too large");
			break;
		}

		offset = strlen(ibuf+1)+2;
		memset(obuf, 0, sizeof obuf);
		base64_encode((unsigned char *)ibuf + offset, strlen(ibuf + offset), obuf, sizeof obuf);
		mta_send(s, "%s", obuf);

		memset(ibuf, 0, sizeof ibuf);
		memset(obuf, 0, sizeof obuf);
		break;

	case MTA_READY:
		/* Ready to send a new mail */
		if (s->ready == 0) {
			s->ready = 1;
			s->relay->nconn_ready += 1;
			mta_route_ok(s->relay, s->route);
		}

		if (s->msgtried >= MAX_TRYBEFOREDISABLE) {
			log_info("smtp-out: Remote host seems to reject all mails on session %016"PRIx64,
			    s->id);
			mta_route_down(s->relay, s->route);
			mta_enter_state(s, MTA_QUIT);
			break;
		}

		if (s->msgcount >= s->relay->limits->max_mail_per_session) {
			log_debug("debug: mta: "
			    "%p: cannot send more message to relay %s", s,
			    mta_relay_to_text(s->relay));
			mta_enter_state(s, MTA_QUIT);
			break;
		}

		/*
		 * When downgrading from opportunistic TLS, clear flag and
		 * possibly reuse the same task (forbidden in other cases).
		 */
		if (s->flags & MTA_DOWNGRADE_PLAIN)
			s->flags &= ~MTA_DOWNGRADE_PLAIN;
		else if (s->task)
			fatalx("task should be NULL at this point");

		if (s->task == NULL)
			s->task = mta_route_next_task(s->relay, s->route);
		if (s->task == NULL) {
			log_debug("debug: mta: %p: no task for relay %s",
			    s, mta_relay_to_text(s->relay));

			if (s->relay->nconn > 1 ||
			    s->hangon >= s->relay->limits->sessdelay_keepalive) {
				mta_enter_state(s, MTA_QUIT);
				break;
			}

			log_debug("mta: debug: last connection: hanging on for %llds",
			    (long long)(s->relay->limits->sessdelay_keepalive -
			    s->hangon));
			s->flags |= MTA_HANGON;
			runq_schedule(hangon, time(NULL) + 1, NULL, s);
			break;
		}

		log_debug("debug: mta: %p: handling next task for relay %s", s,
			    mta_relay_to_text(s->relay));

		stat_increment("mta.task.running", 1);

		m_create(p_queue, IMSG_MTA_OPEN_MESSAGE, 0, 0, -1);
		m_add_id(p_queue, s->id);
		m_add_msgid(p_queue, s->task->msgid);
		m_close(p_queue);

		tree_xset(&wait_fd, s->id, s);
		s->flags |= MTA_WAIT;
		break;

	case MTA_MAIL:
		s->currevp = TAILQ_FIRST(&s->task->envelopes);

		e = s->currevp;
		s->hangon = 0;
		s->msgtried++;
		envid_sz = strlen(e->dsn_envid);
		if (s->ext & MTA_EXT_DSN) {
			mta_send(s, "MAIL FROM:<%s>%s%s%s%s",
			    s->task->sender,
			    e->dsn_ret ? " RET=" : "",
			    e->dsn_ret ? dsn_strret(e->dsn_ret) : "",
			    envid_sz ? " ENVID=" : "",
			    envid_sz ? e->dsn_envid : "");
		} else
			mta_send(s, "MAIL FROM:<%s>", s->task->sender);
		break;

	case MTA_RCPT:
		if (s->currevp == NULL)
			s->currevp = TAILQ_FIRST(&s->task->envelopes);

		e = s->currevp;
		if (s->ext & MTA_EXT_DSN) {
			mta_send(s, "RCPT TO:<%s>%s%s%s%s",
			    e->dest,
			    e->dsn_notify ? " NOTIFY=" : "",
			    e->dsn_notify ? dsn_strnotify(e->dsn_notify) : "",
			    e->dsn_orcpt ? " ORCPT=" : "",
			    e->dsn_orcpt ? e->dsn_orcpt : "");
		} else
			mta_send(s, "RCPT TO:<%s>", e->dest);

		s->rcptcount++;
		break;

	case MTA_DATA:
		fseek(s->datafp, 0, SEEK_SET);
		mta_send(s, "DATA");
		break;

	case MTA_BODY:
		if (s->datafp == NULL) {
			log_trace(TRACE_MTA, "mta: %p: end-of-file", s);
			mta_enter_state(s, MTA_EOM);
			break;
		}

		if ((q = mta_queue_data(s)) == -1) {
			s->flags |= MTA_FREE;
			break;
		}
		if (q == 0) {
			mta_enter_state(s, MTA_BODY);
			break;
		}

		log_trace(TRACE_MTA, "mta: %p: >>> [...%zd bytes...]", s, q);
		break;

	case MTA_EOM:
		mta_send(s, ".");
		break;

	case MTA_LMTP_EOM:
		/* LMTP reports status of each delivery, so enable read */
		io_set_read(&s->io);
		break;

	case MTA_RSET:
		if (s->datafp) {
			fclose(s->datafp);
			s->datafp = NULL;
		}
		mta_send(s, "RSET");
		break;

	case MTA_QUIT:
		mta_send(s, "QUIT");
		break;

	default:
		fatalx("mta_enter_state: unknown state");
	}
#undef mta_enter_state
}

/*
 * Handle a response to an SMTP command
 */
static void
mta_response(struct mta_session *s, char *line)
{
	struct mta_envelope	*e;
	struct sockaddr_storage	 ss;
	struct sockaddr		*sa;
	const char		*domain;
	socklen_t		 sa_len;
	char			 buf[SMTPD_MAXLINESIZE];
	int			 delivery;

	switch (s->state) {

	case MTA_BANNER:
		if (line[0] != '2') {
			mta_error(s, "BANNER rejected: %s", line);
			s->flags |= MTA_FREE;
			return;
		}
		if (s->flags & MTA_LMTP)
			mta_enter_state(s, MTA_LHLO);
		else
			mta_enter_state(s, MTA_EHLO);
		break;

	case MTA_EHLO:
		if (line[0] != '2') {
			/* rejected at ehlo state */
			if ((s->flags & MTA_USE_AUTH) ||
			    (s->flags & MTA_WANT_SECURE)) {
				mta_error(s, "EHLO rejected: %s", line);
				s->flags |= MTA_FREE;
				return;
			}
			mta_enter_state(s, MTA_HELO);
			return;
		}
		if (!(s->flags & MTA_FORCE_PLAIN))
			mta_enter_state(s, MTA_STARTTLS);
		else
			mta_enter_state(s, MTA_READY);
		break;

	case MTA_HELO:
		if (line[0] != '2') {
			mta_error(s, "HELO rejected: %s", line);
			s->flags |= MTA_FREE;
			return;
		}
		mta_enter_state(s, MTA_READY);
		break;

	case MTA_LHLO:
		if (line[0] != '2') {
			mta_error(s, "LHLO rejected: %s", line);
			s->flags |= MTA_FREE;
			return;
		}
		mta_enter_state(s, MTA_READY);
		break;

	case MTA_STARTTLS:
		if (line[0] != '2') {
			if (!(s->flags & MTA_WANT_SECURE)) {
				mta_enter_state(s, MTA_AUTH);
				return;
			}
			/* XXX mark that the MX doesn't support STARTTLS */
			mta_error(s, "STARTTLS rejected: %s", line);
			s->flags |= MTA_FREE;
			return;
		}

		mta_start_tls(s);
		break;

	case MTA_AUTH_PLAIN:
		if (line[0] != '2') {
			mta_error(s, "AUTH rejected: %s", line);
			s->flags |= MTA_FREE;
			return;
		}
		mta_enter_state(s, MTA_READY);
		break;

	case MTA_AUTH_LOGIN:
		if (strncmp(line, "334 ", 4) != 0) {
			mta_error(s, "AUTH rejected: %s", line);
			s->flags |= MTA_FREE;
			return;
		}
		mta_enter_state(s, MTA_AUTH_LOGIN_USER);
		break;

	case MTA_AUTH_LOGIN_USER:
		if (strncmp(line, "334 ", 4) != 0) {
			mta_error(s, "AUTH rejected: %s", line);
			s->flags |= MTA_FREE;
			return;
		}
		mta_enter_state(s, MTA_AUTH_LOGIN_PASS);
		break;

	case MTA_AUTH_LOGIN_PASS:
		if (line[0] != '2') {
			mta_error(s, "AUTH rejected: %s", line);
			s->flags |= MTA_FREE;
			return;
		}
		mta_enter_state(s, MTA_READY);
		break;

	case MTA_MAIL:
		if (line[0] != '2') {
			if (line[0] == '5')
				delivery = IMSG_MTA_DELIVERY_PERMFAIL;
			else
				delivery = IMSG_MTA_DELIVERY_TEMPFAIL;
			mta_flush_task(s, delivery, line, 0, 0);
			mta_enter_state(s, MTA_RSET);
			return;
		}
		mta_enter_state(s, MTA_RCPT);
		break;

	case MTA_RCPT:
		e = s->currevp;

		/* remove envelope from hosttat cache if there */
		if ((domain = strchr(e->dest, '@')) != NULL) {
			domain++;
			mta_hoststat_uncache(domain, e->id);
		}

		s->currevp = TAILQ_NEXT(s->currevp, entry);
		if (line[0] == '2') {
			s->failures = 0;
			/*
			 * this host is up, reschedule envelopes that
			 * were cached for reschedule.
			 */
			if (domain)
				mta_hoststat_reschedule(domain);
		}
		else {
			if (line[0] == '5')
				delivery = IMSG_MTA_DELIVERY_PERMFAIL;
			else
				delivery = IMSG_MTA_DELIVERY_TEMPFAIL;
			s->failures++;

			/* remove failed envelope from task list */
			TAILQ_REMOVE(&s->task->envelopes, e, entry);
			stat_decrement("mta.envelope", 1);

			/* log right away */
			(void)snprintf(buf, sizeof(buf), "%s",
			    mta_host_to_text(s->route->dst));

			e->session = s->id;
			/* XXX */
			/*
			 * getsockname() can only fail with ENOBUFS here
			 * best effort, don't log source ...
			 */
			sa_len = sizeof(ss);
			sa = (struct sockaddr *)&ss;
			if (getsockname(s->io.sock, sa, &sa_len) < 0)
				mta_delivery_log(e, NULL, buf, delivery, line);
			else
				mta_delivery_log(e, sa_to_text(sa),
				    buf, delivery, line);

			if (domain)
				mta_hoststat_update(domain, e->status);
			mta_delivery_notify(e);

			if (s->relay->limits->max_failures_per_session &&
			    s->failures == s->relay->limits->max_failures_per_session) {
					mta_flush_task(s, IMSG_MTA_DELIVERY_TEMPFAIL,
					    "Too many consecutive errors, closing connection", 0, 1);
					mta_enter_state(s, MTA_QUIT);
					break;
				}

			/*
			 * if no more envelopes, flush failed queue
			 */
			if (TAILQ_EMPTY(&s->task->envelopes)) {
				mta_flush_task(s, IMSG_MTA_DELIVERY_OK,
				    "No envelope", 0, 0);
				mta_enter_state(s, MTA_RSET);
				break;
			}
		}

		if (s->currevp == NULL)
			mta_enter_state(s, MTA_DATA);
		else
			mta_enter_state(s, MTA_RCPT);
		break;

	case MTA_DATA:
		if (line[0] == '2' || line[0] == '3') {
			mta_enter_state(s, MTA_BODY);
			break;
		}
		if (line[0] == '5')
			delivery = IMSG_MTA_DELIVERY_PERMFAIL;
		else
			delivery = IMSG_MTA_DELIVERY_TEMPFAIL;
		mta_flush_task(s, delivery, line, 0, 0);
		mta_enter_state(s, MTA_RSET);
		break;

	case MTA_LMTP_EOM:
	case MTA_EOM:
		if (line[0] == '2') {
			delivery = IMSG_MTA_DELIVERY_OK;
			s->msgtried = 0;
			s->msgcount++;
		}
		else if (line[0] == '5')
			delivery = IMSG_MTA_DELIVERY_PERMFAIL;
		else
			delivery = IMSG_MTA_DELIVERY_TEMPFAIL;
		mta_flush_task(s, delivery, line, (s->flags & MTA_LMTP) ? 1 : 0, 0);
		if (s->task) {
			s->rcptcount--;
			mta_enter_state(s, MTA_LMTP_EOM);
		} else {
			s->rcptcount = 0;
			if (s->relay->limits->sessdelay_transaction) {
				log_debug("debug: mta: waiting for %llds before next transaction",
				    (long long int)s->relay->limits->sessdelay_transaction);
				s->hangon = s->relay->limits->sessdelay_transaction -1;
				s->flags |= MTA_HANGON;
				runq_schedule(hangon, time(NULL)
				    + s->relay->limits->sessdelay_transaction,
				    NULL, s);
			}
			else
				mta_enter_state(s, MTA_READY);
		}
		break;

	case MTA_RSET:
		s->rcptcount = 0;
		if (s->relay->limits->sessdelay_transaction) {
			log_debug("debug: mta: waiting for %llds after reset",
			    (long long int)s->relay->limits->sessdelay_transaction);
			s->hangon = s->relay->limits->sessdelay_transaction -1;
			s->flags |= MTA_HANGON;
			runq_schedule(hangon, time(NULL)
			    + s->relay->limits->sessdelay_transaction,
			    NULL, s);
		}
		else
			mta_enter_state(s, MTA_READY);
		break;

	default:
		fatalx("mta_response() bad state");
	}
}

static void
mta_io(struct io *io, int evt)
{
	struct mta_session	*s = io->arg;
	char			*line, *msg, *p;
	size_t			 len;
	const char		*error;
	int			 cont;
	X509			*x;

	log_trace(TRACE_IO, "mta: %p: %s %s", s, io_strevent(evt),
	    io_strio(io));

	switch (evt) {

	case IO_CONNECTED:
		log_info("smtp-out: Connected on session %016"PRIx64, s->id);

		if (s->use_smtps) {
			io_set_write(io);
			mta_start_tls(s);
		}
		else {
			mta_enter_state(s, MTA_BANNER);
			io_set_read(io);
		}
		break;

	case IO_TLSREADY:
		log_info("smtp-out: Started TLS on session %016"PRIx64": %s",
		    s->id, ssl_to_text(s->io.ssl));
		s->flags |= MTA_TLS;

		if (mta_verify_certificate(s)) {
			io_pause(&s->io, IO_PAUSE_IN);
			break;
		}

	case IO_TLSVERIFIED:
		x = SSL_get_peer_certificate(s->io.ssl);
		if (x) {
			log_info("smtp-out: Server certificate verification %s "
			    "on session %016"PRIx64,
			    (s->flags & MTA_VERIFIED) ? "succeeded" : "failed",
			    s->id);
			X509_free(x);
		}

		if (s->use_smtps) {
			mta_enter_state(s, MTA_BANNER);
			io_set_read(io);
		}
		else
			mta_enter_state(s, MTA_EHLO);
		break;

	case IO_DATAIN:
	    nextline:
		line = iobuf_getline(&s->iobuf, &len);
		if (line == NULL) {
			if (iobuf_len(&s->iobuf) >= SMTPD_MAXLINESIZE) {
				mta_error(s, "Input too long");
				mta_free(s);
				return;
			}
			iobuf_normalize(&s->iobuf);
			break;
		}

		log_trace(TRACE_MTA, "mta: %p: <<< %s", s, line);

		if ((error = parse_smtp_response(line, len, &msg, &cont))) {
			mta_error(s, "Bad response: %s", error);
			mta_free(s);
			return;
		}

		/* read extensions */
		if (s->state == MTA_EHLO) {
			if (strcmp(msg, "STARTTLS") == 0)
				s->ext |= MTA_EXT_STARTTLS;
			else if (strncmp(msg, "AUTH ", 5) == 0) {
                                s->ext |= MTA_EXT_AUTH;
                                if ((p = strstr(msg, " PLAIN")) &&
				    (*(p+6) == '\0' || *(p+6) == ' '))
                                        s->ext |= MTA_EXT_AUTH_PLAIN;
                                if ((p = strstr(msg, " LOGIN")) &&
				    (*(p+6) == '\0' || *(p+6) == ' '))
                                        s->ext |= MTA_EXT_AUTH_LOGIN;
			}
			else if (strcmp(msg, "PIPELINING") == 0)
				s->ext |= MTA_EXT_PIPELINING;
			else if (strcmp(msg, "DSN") == 0)
				s->ext |= MTA_EXT_DSN;
		}

		if (cont)
			goto nextline;

		if (s->state == MTA_QUIT) {
			log_info("smtp-out: Closing session %016"PRIx64
			    ": %zu message%s sent.", s->id, s->msgcount,
			    (s->msgcount > 1) ? "s" : "");
			mta_free(s);
			return;
		}
		io_set_write(io);
		mta_response(s, line);
		if (s->flags & MTA_FREE) {
			mta_free(s);
			return;
		}
		if (s->flags & MTA_RECONN) {
			s->flags &= ~MTA_RECONN;
			mta_connect(s);
			return;
		}

		iobuf_normalize(&s->iobuf);

		if (iobuf_len(&s->iobuf)) {
			log_debug("debug: mta: remaining data in input buffer");
			mta_error(s, "Remote host sent too much data");
			if (s->flags & MTA_WAIT)
				s->flags |= MTA_FREE;
			else
				mta_free(s);
		}
		break;

	case IO_LOWAT:
		if (s->state == MTA_BODY) {
			mta_enter_state(s, MTA_BODY);
			if (s->flags & MTA_FREE) {
				mta_free(s);
				return;
			}
		}

		if (iobuf_queued(&s->iobuf) == 0)
			io_set_read(io);
		break;

	case IO_TIMEOUT:
		log_debug("debug: mta: %p: connection timeout", s);
		mta_error(s, "Connection timeout");
		if (!s->ready)
			mta_connect(s);
		else
			mta_free(s);
		break;

	case IO_ERROR:
		log_debug("debug: mta: %p: IO error: %s", s, io->error);
		if (!s->ready) {
			mta_error(s, "IO Error: %s", io->error);
			mta_connect(s);
			break;
		}
		else if (!(s->flags & (MTA_FORCE_TLS|MTA_FORCE_ANYSSL))) {
			/* error in non-strict SSL negotiation, downgrade to plain */
			if (s->flags & MTA_TLS) {
				log_info("smtp-out: Error on session %016"PRIx64
				    ": opportunistic TLS failed, "
				    "downgrading to plain", s->id);
				s->flags &= ~MTA_TLS;
				s->flags |= MTA_DOWNGRADE_PLAIN;
				mta_connect(s);
				break;
			}
		}
		mta_error(s, "IO Error: %s", io->error);
		mta_free(s);
		break;

	case IO_TLSERROR:
		log_debug("debug: mta: %p: TLS IO error: %s", s, io->error);
		if (!(s->flags & (MTA_FORCE_TLS|MTA_FORCE_ANYSSL))) {
			/* error in non-strict SSL negotiation, downgrade to plain */
			log_info("smtp-out: TLS Error on session %016"PRIx64
			    ": TLS failed, "
			    "downgrading to plain", s->id);
			s->flags &= ~MTA_TLS;
			s->flags |= MTA_DOWNGRADE_PLAIN;
			mta_connect(s);
			break;
		}
		mta_error(s, "IO Error: %s", io->error);
		mta_free(s);
		break;

	case IO_DISCONNECTED:
		log_debug("debug: mta: %p: disconnected in state %s",
		    s, mta_strstate(s->state));
		mta_error(s, "Connection closed unexpectedly");
		if (!s->ready)
			mta_connect(s);
		else
			mta_free(s);
		break;

	default:
		fatalx("mta_io() bad event");
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

	iobuf_xfqueue(&s->iobuf, "mta_send", "%s\r\n", p);

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
			ln[len - 1] = '\0';
		iobuf_xfqueue(&s->iobuf, "mta_queue_data", "%s%s\r\n",
		    *ln == '.' ? "." : "", ln);
	}

	if (ferror(s->datafp)) {
		mta_flush_task(s, IMSG_MTA_DELIVERY_TEMPFAIL,
		    "Error reading content file", 0, 0);
		return (-1);
	}

	if (feof(s->datafp)) {
		fclose(s->datafp);
		s->datafp = NULL;
	}

	return (iobuf_queued(&s->iobuf) - q);
}

static void
mta_flush_task(struct mta_session *s, int delivery, const char *error, size_t count,
	int cache)
{
	struct mta_envelope	*e;
	char			 relay[SMTPD_MAXLINESIZE];
	size_t			 n;
	struct sockaddr_storage	 ss;
	struct sockaddr		*sa;
	socklen_t		 sa_len;
	const char		*domain;

	(void)snprintf(relay, sizeof relay, "%s", mta_host_to_text(s->route->dst));
	n = 0;
	while ((e = TAILQ_FIRST(&s->task->envelopes))) {

		if (count && n == count) {
			stat_decrement("mta.envelope", n);
			return;
		}

		TAILQ_REMOVE(&s->task->envelopes, e, entry);

		/* we're about to log, associate session to envelope */
		e->session = s->id;
		e->ext = s->ext;

		/* XXX */
		/*
		 * getsockname() can only fail with ENOBUFS here
		 * best effort, don't log source ...
		 */
		sa = (struct sockaddr *)&ss;
		sa_len = sizeof(ss);
		if (getsockname(s->io.sock, sa, &sa_len) < 0)
			mta_delivery_log(e, NULL, relay, delivery, error);
		else
			mta_delivery_log(e, sa_to_text(sa),
			    relay, delivery, error);

		mta_delivery_notify(e);

		domain = strchr(e->dest, '@');
		if (domain) {
			domain++;
			mta_hoststat_update(domain, error);
			if (cache)
				mta_hoststat_cache(domain, e->id);
		}

		n++;
	}

	free(s->task->sender);
	free(s->task);
	s->task = NULL;

	if (s->datafp) {
		fclose(s->datafp);
		s->datafp = NULL;
	}

	stat_decrement("mta.envelope", n);
	stat_decrement("mta.task.running", 1);
	stat_decrement("mta.task", 1);
}

static void
mta_error(struct mta_session *s, const char *fmt, ...)
{
	va_list  ap;
	char	*error;
	int	 len;

	va_start(ap, fmt);
	if ((len = vasprintf(&error, fmt, ap)) == -1)
		fatal("mta: vasprintf");
	va_end(ap);

	if (s->msgcount)
		log_info("smtp-out: Error on session %016"PRIx64
		    " after %zu message%s sent: %s", s->id, s->msgcount,
		    (s->msgcount > 1) ? "s" : "", error);
	else
		log_info("smtp-out: Error on session %016"PRIx64 ": %s",
		    s->id, error);
	/*
	 * If not connected yet, and the error is not local, just ignore it
	 * and try to reconnect.
	 */
	if (s->state == MTA_INIT && 
	    (errno == ETIMEDOUT || errno == ECONNREFUSED)) {
		log_debug("debug: mta: not reporting route error yet");
		free(error);
		return;
	}

	mta_route_error(s->relay, s->route);

	if (s->task)
		mta_flush_task(s, IMSG_MTA_DELIVERY_TEMPFAIL, error, 0, 0);

	free(error);
}

static void
mta_start_tls(struct mta_session *s)
{
	struct ca_cert_req_msg	req_ca_cert;
	const char	       *certname;

	if (s->relay->pki_name)
		certname = s->relay->pki_name;
	else
		certname = s->helo;

	req_ca_cert.reqid = s->id;
	(void)strlcpy(req_ca_cert.name, certname, sizeof req_ca_cert.name);
	m_compose(p_lka, IMSG_MTA_SSL_INIT, 0, 0, -1,
	    &req_ca_cert, sizeof(req_ca_cert));
	tree_xset(&wait_ssl_init, s->id, s);
	s->flags |= MTA_WAIT;
	return;
}

static int
mta_verify_certificate(struct mta_session *s)
{
	struct ca_vrfy_req_msg	req_ca_vrfy;
	struct iovec		iov[2];
	X509		       *x;
	STACK_OF(X509)	       *xchain;
	int			i;
	const char	       *pkiname;

	x = SSL_get_peer_certificate(s->io.ssl);
	if (x == NULL)
		return 0;
	xchain = SSL_get_peer_cert_chain(s->io.ssl);

	/*
	 * Client provided a certificate and possibly a certificate chain.
	 * SMTP can't verify because it does not have the information that
	 * it needs, instead it will pass the certificate and chain to the
	 * lookup process and wait for a reply.
	 *
	 */

	tree_xset(&wait_ssl_verify, s->id, s);
	s->flags |= MTA_WAIT;

	/* Send the client certificate */
	memset(&req_ca_vrfy, 0, sizeof req_ca_vrfy);
	if (s->relay->pki_name)
		pkiname = s->relay->pki_name;
	else
		pkiname = s->helo;
	if (strlcpy(req_ca_vrfy.pkiname, pkiname, sizeof req_ca_vrfy.pkiname)
	    >= sizeof req_ca_vrfy.pkiname)
		return 0;

	req_ca_vrfy.reqid = s->id;
	req_ca_vrfy.cert_len = i2d_X509(x, &req_ca_vrfy.cert);
	if (xchain)
		req_ca_vrfy.n_chain = sk_X509_num(xchain);
	iov[0].iov_base = &req_ca_vrfy;
	iov[0].iov_len = sizeof(req_ca_vrfy);
	iov[1].iov_base = req_ca_vrfy.cert;
	iov[1].iov_len = req_ca_vrfy.cert_len;
	m_composev(p_lka, IMSG_MTA_SSL_VERIFY_CERT, 0, 0, -1,
	    iov, nitems(iov));
	free(req_ca_vrfy.cert);
	X509_free(x);

	if (xchain) {		
		/* Send the chain, one cert at a time */
		for (i = 0; i < sk_X509_num(xchain); ++i) {
			memset(&req_ca_vrfy, 0, sizeof req_ca_vrfy);
			req_ca_vrfy.reqid = s->id;
			x = sk_X509_value(xchain, i);
			req_ca_vrfy.cert_len = i2d_X509(x, &req_ca_vrfy.cert);
			iov[0].iov_base = &req_ca_vrfy;
			iov[0].iov_len  = sizeof(req_ca_vrfy);
			iov[1].iov_base = req_ca_vrfy.cert;
			iov[1].iov_len  = req_ca_vrfy.cert_len;
			m_composev(p_lka, IMSG_MTA_SSL_VERIFY_CHAIN, 0, 0, -1,
			    iov, nitems(iov));
			free(req_ca_vrfy.cert);
		}
	}

	/* Tell lookup process that it can start verifying, we're done */
	memset(&req_ca_vrfy, 0, sizeof req_ca_vrfy);
	req_ca_vrfy.reqid = s->id;
	m_compose(p_lka, IMSG_MTA_SSL_VERIFY, 0, 0, -1,
	    &req_ca_vrfy, sizeof req_ca_vrfy);

	return 1;
}

static const char *
dsn_strret(enum dsn_ret ret)
{
	if (ret == DSN_RETHDRS)
		return "HDRS";
	else if (ret == DSN_RETFULL)
		return "FULL";
	else {
		log_debug("mta: invalid ret %d", ret);
		return "???";
	}
}

static const char *
dsn_strnotify(uint8_t arg)
{
	static char	buf[32];
	size_t		sz;

	buf[0] = '\0';
	if (arg & DSN_SUCCESS)
		(void)strlcat(buf, "SUCCESS,", sizeof(buf));

	if (arg & DSN_FAILURE)
		(void)strlcat(buf, "FAILURE,", sizeof(buf));

	if (arg & DSN_DELAY)
		(void)strlcat(buf, "DELAY,", sizeof(buf));

	if (arg & DSN_NEVER)
		(void)strlcat(buf, "NEVER,", sizeof(buf));

	/* trim trailing comma */
	sz = strlen(buf);
	if (sz)
		buf[sz - 1] = '\0';

	return (buf);
}

#define CASE(x) case x : return #x

static const char *
mta_strstate(int state)
{
	switch (state) {
	CASE(MTA_INIT);
	CASE(MTA_BANNER);
	CASE(MTA_EHLO);
	CASE(MTA_HELO);
	CASE(MTA_STARTTLS);
	CASE(MTA_AUTH);
	CASE(MTA_AUTH_PLAIN);
	CASE(MTA_AUTH_LOGIN);
	CASE(MTA_AUTH_LOGIN_USER);
	CASE(MTA_AUTH_LOGIN_PASS);
	CASE(MTA_READY);
	CASE(MTA_MAIL);
	CASE(MTA_RCPT);
	CASE(MTA_DATA);
	CASE(MTA_BODY);
	CASE(MTA_EOM);
	CASE(MTA_LMTP_EOM);
	CASE(MTA_RSET);
	CASE(MTA_QUIT);
	default:
		return "MTA_???";
	}
}
