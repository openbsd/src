/*	$OpenBSD: mta.c,v 1.50 2009/05/27 13:14:18 jacekm Exp $	*/

/*
 * Copyright (c) 2008 Pierre-Yves Ritschard <pyr@openbsd.org>
 * Copyright (c) 2008 Gilles Chehade <gilles@openbsd.org>
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

#include <ssl/ssl.h>

#include <errno.h>
#include <event.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "smtpd.h"

__dead void	mta_shutdown(void);
void		mta_sig_handler(int, short, void *);
void		mta_dispatch_parent(int, short, void *);
void		mta_dispatch_queue(int, short, void *);
void		mta_dispatch_runner(int, short, void *);
void		mta_dispatch_lka(int, short, void *);
void		mta_setup_events(struct smtpd *);
void		mta_disable_events(struct smtpd *);
void		mta_write(int, short, void *);
int		mta_connect(struct session *);
void		mta_read_handler(struct bufferevent *, void *);
void		mta_write_handler(struct bufferevent *, void *);
void		mta_error_handler(struct bufferevent *, short, void *);
int		mta_reply_handler(struct bufferevent *, void *);
void		mta_batch_update_queue(struct batch *);
void		mta_mxlookup(struct smtpd *, struct session *, char *, struct rule *);
void		ssl_client_init(struct session *);

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
	struct imsgbuf		*ibuf;
	struct imsg		 imsg;
	ssize_t			 n;

	ibuf = env->sc_ibufs[PROC_PARENT];
	switch (event) {
	case EV_READ:
		if ((n = imsg_read(ibuf)) == -1)
			fatal("imsg_read_error");
		if (n == 0) {
			/* this pipe is dead, so remove the event handler */
			event_del(&ibuf->ev);
			event_loopexit(NULL);
			return;
		}
		break;
	case EV_WRITE:
		if (msgbuf_write(&ibuf->w) == -1)
			fatal("msgbuf_write");
		imsg_event_add(ibuf);
		return;
	default:
		fatalx("unknown event");
	}

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatalx("mta_dispatch_parent: imsg_get error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		default:
			log_warnx("mta_dispatch_parent: got imsg %d",
			    imsg.hdr.type);
			fatalx("mta_dispatch_parent: unexpected imsg");
		}
		imsg_free(&imsg);
	}
	imsg_event_add(ibuf);
}

void
mta_dispatch_lka(int sig, short event, void *p)
{
	struct smtpd		*env = p;
	struct imsgbuf		*ibuf;
	struct imsg		 imsg;
	ssize_t			 n;

	ibuf = env->sc_ibufs[PROC_LKA];
	switch (event) {
	case EV_READ:
		if ((n = imsg_read(ibuf)) == -1)
			fatal("imsg_read_error");
		if (n == 0) {
			/* this pipe is dead, so remove the event handler */
			event_del(&ibuf->ev);
			event_loopexit(NULL);
			return;
		}
		break;
	case EV_WRITE:
		if (msgbuf_write(&ibuf->w) == -1)
			fatal("msgbuf_write");
		imsg_event_add(ibuf);
		return;
	default:
		fatalx("unknown event");
	}

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatalx("mta_dispatch_lka: imsg_get error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_DNS_A: {
			struct session key;
			struct dns *reply = imsg.data;
			struct session *s;
			struct mxhost *mxhost;

			IMSG_SIZE_CHECK(reply);

			key.s_id = reply->id;

			s = SPLAY_FIND(sessiontree, &env->sc_sessions, &key);
			if (s == NULL)
				fatal("mta_dispatch_lka: session is gone");

			mxhost = calloc(1, sizeof(struct mxhost));
			if (mxhost == NULL)
				fatal("mta_dispatch_lka: calloc");

			mxhost->ss = reply->ss;

 			TAILQ_INSERT_TAIL(&s->mxhosts, mxhost, entry);

			break;
		}

		case IMSG_DNS_A_END: {
			struct session key;
			struct dns *reply = imsg.data;
			struct session *s;
			int ret;

			IMSG_SIZE_CHECK(reply);

			key.s_id = reply->id;

			s = SPLAY_FIND(sessiontree, &env->sc_sessions, &key);
			if (s == NULL)
				fatal("smtp_dispatch_parent: session is gone");

			s->batch->flags |= F_BATCH_RESOLVED;

			do {
				ret = mta_connect(s);
			} while (ret == 0);

			if (ret < 0) {
				mta_batch_update_queue(s->batch);
				session_destroy(s);
			}

			break;
		}

		case IMSG_LKA_SECRET: {
			struct secret	*reply = imsg.data;
			struct session	 key, *s;

			IMSG_SIZE_CHECK(reply);

			key.s_id = reply->id;

			s = SPLAY_FIND(sessiontree, &env->sc_sessions, &key);
			if (s == NULL)
				fatal("smtp_dispatch_parent: session is gone");

			strlcpy(s->credentials, reply->secret,
			    sizeof(s->credentials));

			mta_mxlookup(env, s, s->batch->hostname,
			    &s->batch->rule);
			break;
		}

		default:
			log_warnx("mta_dispatch_parent: got imsg %d",
			    imsg.hdr.type);
			fatalx("mta_dispatch_lka: unexpected imsg");
		}
		imsg_free(&imsg);
	}
	imsg_event_add(ibuf);
}

void
mta_dispatch_queue(int sig, short event, void *p)
{
	struct smtpd		*env = p;
	struct imsgbuf		*ibuf;
	struct imsg		 imsg;
	ssize_t			 n;

	ibuf = env->sc_ibufs[PROC_QUEUE];
	switch (event) {
	case EV_READ:
		if ((n = imsg_read(ibuf)) == -1)
			fatal("imsg_read_error");
		if (n == 0) {
			/* this pipe is dead, so remove the event handler */
			event_del(&ibuf->ev);
			event_loopexit(NULL);
			return;
		}
		break;
	case EV_WRITE:
		if (msgbuf_write(&ibuf->w) == -1)
			fatal("msgbuf_write");
		imsg_event_add(ibuf);
		return;
	default:
		fatalx("unknown event");
	}

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatalx("mta_dispatch_queue: imsg_get error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_QUEUE_MESSAGE_FD: {
			struct batch	*batchp = imsg.data;
			struct session	*sessionp;
			int fd;

			IMSG_SIZE_CHECK(batchp);

			if ((fd = imsg_get_fd(ibuf, &imsg)) == -1) {
				/* NEEDS_FIX - unsure yet how it must be handled */
				fatalx("mta_dispatch_queue: imsg_get_fd");
			}

			batchp = batch_by_id(env, batchp->id);
			sessionp = batchp->sessionp;

			if ((batchp->messagefp = fdopen(fd, "r")) == NULL)
				fatal("mta_dispatch_queue: fdopen");

			session_respond(sessionp, "DATA");
			bufferevent_enable(sessionp->s_bev, EV_READ);
			break;
		}
		default:
			log_warnx("mta_dispatch_queue: got imsg %d",
			    imsg.hdr.type);
			fatalx("mta_dispatch_queue: unexpected imsg");
		}
		imsg_free(&imsg);
	}
	imsg_event_add(ibuf);
}

void
mta_dispatch_runner(int sig, short event, void *p)
{
	struct smtpd		*env = p;
	struct imsgbuf		*ibuf;
	struct imsg		 imsg;
	ssize_t			 n;

	ibuf = env->sc_ibufs[PROC_RUNNER];
	switch (event) {
	case EV_READ:
		if ((n = imsg_read(ibuf)) == -1)
			fatal("imsg_read_error");
		if (n == 0) {
			/* this pipe is dead, so remove the event handler */
			event_del(&ibuf->ev);
			event_loopexit(NULL);
			return;
		}
		break;
	case EV_WRITE:
		if (msgbuf_write(&ibuf->w) == -1)
			fatal("msgbuf_write");
		imsg_event_add(ibuf);
		return;
	default:
		fatalx("unknown event");
	}

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatalx("mta_dispatch_runner: imsg_get error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_BATCH_CREATE: {
			struct batch *request = imsg.data;
			struct batch *batchp;
			struct session *s;

			IMSG_SIZE_CHECK(request);

			/* create a client session */
			if ((s = calloc(1, sizeof(*s))) == NULL)
				fatal(NULL);
			s->s_state = S_INIT;
			s->s_env = env;
			s->s_id = queue_generate_id();
			TAILQ_INIT(&s->mxhosts);
			SPLAY_INSERT(sessiontree, &s->s_env->sc_sessions, s);

			/* create the batch for this session */
			batchp = calloc(1, sizeof (struct batch));
			if (batchp == NULL)
				fatal("mta_dispatch_runner: calloc");

			*batchp = *request;
			batchp->session_id = s->s_id;
			batchp->env = env;
			batchp->flags = 0;
			batchp->sessionp = s;

			s->batch = batchp;

			TAILQ_INIT(&batchp->messages);
			SPLAY_INSERT(batchtree, &env->batch_queue, batchp);

			break;
		}
		case IMSG_BATCH_APPEND: {
			struct message	*append = imsg.data;
			struct message	*messagep;
			struct batch	*batchp;

			IMSG_SIZE_CHECK(append);

			messagep = calloc(1, sizeof (struct message));
			if (messagep == NULL)
				fatal("mta_dispatch_runner: calloc");

			*messagep = *append;

			batchp = batch_by_id(env, messagep->batch_id);
			if (batchp == NULL)
				fatalx("mta_dispatch_runner: internal inconsistency.");

			batchp->session_ss = messagep->session_ss;
			strlcpy(batchp->session_hostname,
			    messagep->session_hostname,
			    sizeof(batchp->session_hostname));
			strlcpy(batchp->session_helo, messagep->session_helo,
			    sizeof(batchp->session_helo));

 			TAILQ_INSERT_TAIL(&batchp->messages, messagep, entry);
			break;
		}
		case IMSG_BATCH_CLOSE: {
			struct batch		*batchp = imsg.data;
			struct session		*s;

			IMSG_SIZE_CHECK(batchp);

			batchp = batch_by_id(env, batchp->id);
			if (batchp == NULL)
				fatalx("mta_dispatch_runner: internal inconsistency.");

			batchp->flags |= F_BATCH_COMPLETE;

			/* assume temporary failure by default, safest choice */
			batchp->status = S_BATCH_TEMPFAILURE;

			log_debug("batch ready, we can initiate a session");

			s = batchp->sessionp;

			if (batchp->rule.r_value.relayhost.flags & F_AUTH) {
				struct secret	query;

				bzero(&query, sizeof(query));
				query.id = s->s_id;
				strlcpy(query.host,
				    batchp->rule.r_value.relayhost.hostname,
				    sizeof(query.host));

				imsg_compose(env->sc_ibufs[PROC_LKA],
				    IMSG_LKA_SECRET, 0, 0, -1, &query,
				    sizeof(query));
			} else
				mta_mxlookup(env, s, batchp->hostname,
				    &batchp->rule);
			break;
		}
		default:
			log_warnx("mta_dispatch_runner: got imsg %d",
			    imsg.hdr.type);
			fatalx("mta_dispatch_runner: unexpected imsg");
		}
		imsg_free(&imsg);
	}
	imsg_event_add(ibuf);
}

void
mta_shutdown(void)
{
	log_info("mail transfer agent exiting");
	_exit(0);
}

void
mta_setup_events(struct smtpd *env)
{
}

void
mta_disable_events(struct smtpd *env)
{
}

pid_t
mta(struct smtpd *env)
{
	pid_t		 pid;

	struct passwd	*pw;
	struct event	 ev_sigint;
	struct event	 ev_sigterm;

	struct peer peers[] = {
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

	SPLAY_INIT(&env->batch_queue);

	mta_setup_events(env);
	event_dispatch();
	mta_shutdown();

	return (0);
}

void
mta_mxlookup(struct smtpd *env, struct session *sessionp, char *hostname, struct rule *rule)
{
	int	 port;

	switch (rule->r_value.relayhost.flags & F_SSL) {
	case F_SMTPS:
		port = 465;
		break;
	case F_SSL:
		port = 465;
		rule->r_value.relayhost.flags &= ~F_STARTTLS;
		break;
	default:
		port = 25;
	}
	
	if (rule->r_value.relayhost.port)
		port = ntohs(rule->r_value.relayhost.port);

	if (rule->r_action == A_RELAYVIA)
		dns_query_mx(env, rule->r_value.relayhost.hostname, port,
		    sessionp->s_id);
	else
		dns_query_mx(env, hostname, port, sessionp->s_id);
}

/* shamelessly ripped usr.sbin/relayd/check_tcp.c ;) */
int
mta_connect(struct session *sessionp)
{
	int s;
	int type;
	struct linger lng;
	struct mxhost *mxhost;

	mxhost = TAILQ_FIRST(&sessionp->mxhosts);
	if (mxhost == NULL)
		return -1;

	if ((s = socket(mxhost->ss.ss_family, SOCK_STREAM, 0)) == -1)
		goto bad;

	bzero(&lng, sizeof(lng));
	if (setsockopt(s, SOL_SOCKET, SO_LINGER, &lng, sizeof (lng)) == -1)
		goto bad;

	type = 1;
	if (setsockopt(s, SOL_SOCKET, SO_REUSEPORT, &type, sizeof (type)) == -1)
		goto bad;

	session_socket_blockmode(s, BM_NONBLOCK);

	if (connect(s, (struct sockaddr *)&mxhost->ss, mxhost->ss.ss_len) == -1)
		if (errno != EINPROGRESS)
			goto bad;

	sessionp->s_tv.tv_sec = SMTPD_CONNECT_TIMEOUT;
	sessionp->s_tv.tv_usec = 0;
	sessionp->s_fd = s;
	event_set(&sessionp->s_ev, s, EV_TIMEOUT|EV_WRITE, mta_write, sessionp);
	event_add(&sessionp->s_ev, &sessionp->s_tv);

	return 1;

bad:
	if (mxhost) {
		TAILQ_REMOVE(&sessionp->mxhosts, mxhost, entry);
		free(mxhost);
	}
	close(s);
	return 0;
}

void
mta_write(int s, short event, void *arg)
{
	struct session *sessionp = arg;
	struct batch *batchp = sessionp->batch;
	struct mxhost *mxhost;
	int ret;

	mxhost = TAILQ_FIRST(&sessionp->mxhosts);

	if (event == EV_TIMEOUT) {

		if (mxhost) {
			TAILQ_REMOVE(&sessionp->mxhosts, mxhost, entry);
			free(mxhost);
		}
		close(s);

		if (sessionp->s_bev) {
			bufferevent_free(sessionp->s_bev);
			sessionp->s_bev = NULL;
		}
		strlcpy(batchp->errorline, "connection timed-out.",
		    sizeof(batchp->errorline));

		do {
			ret = mta_connect(sessionp);
		} while (ret == 0);

		if (ret < 0) {
			mta_batch_update_queue(batchp);
			session_destroy(sessionp);
		}

		return;
	}

	sessionp->s_bev = bufferevent_new(s, mta_read_handler, mta_write_handler,
	    mta_error_handler, sessionp);

	if (sessionp->s_bev == NULL) {
		mta_batch_update_queue(batchp);
		session_destroy(sessionp);
		return;
	}

	if (sessionp->batch->rule.r_value.relayhost.flags & F_SMTPS) {
		log_debug("mta_write: initializing ssl");
		ssl_client_init(sessionp);
		return;
	}
	bufferevent_enable(sessionp->s_bev, EV_READ);
}

void
mta_read_handler(struct bufferevent *bev, void *arg)
{
	while (mta_reply_handler(bev, arg))
		;
}

int
mta_reply_handler(struct bufferevent *bev, void *arg)
{
	struct session *sessionp = arg;
	struct batch *batchp = sessionp->batch;
	struct smtpd *env = batchp->env;
	struct message *messagep = NULL;
	char *line;
	int i;
	int code;
#define F_ISINFO	0x1
#define F_ISPROTOERROR	0x2
	char codebuf[4];
	const char *errstr;
	int flags = 0;

	line = evbuffer_readline(bev->input);
	if (line == NULL)
		return 0;

	log_debug("remote server sent: [%s]", line);

	strlcpy(codebuf, line, sizeof(codebuf));
	code = strtonum(codebuf, 0, UINT16_MAX, &errstr);
	if (errstr || code < 100) {
		/* Server sent invalid line, protocol error */
		batchp->status = S_BATCH_PERMFAILURE;
		strlcpy(batchp->errorline, line, sizeof(batchp->errorline));
		mta_batch_update_queue(batchp);
		session_destroy(sessionp);
		return 0;
	}

	if (line[3] == '-') {
		if (strcasecmp(&line[4], "STARTTLS") == 0)
			sessionp->s_flags |= F_PEERHASTLS;
		else if (strncasecmp(&line[4], "AUTH ", 5) == 0 ||
		    strncasecmp(&line[4], "AUTH-", 5) == 0)
			sessionp->s_flags |= F_PEERHASAUTH;
		return 1;
	}

	switch (code) {
	case 250:
		if (sessionp->s_state == S_DONE) {
			batchp->status = S_BATCH_ACCEPTED;
			mta_batch_update_queue(batchp);
			session_destroy(sessionp);
			return 0;
		}

		if (sessionp->s_state == S_GREETED &&
		    (sessionp->s_flags & F_PEERHASTLS) &&
		    !(sessionp->s_flags & F_SECURE)) {
			session_respond(sessionp, "STARTTLS");
			sessionp->s_state = S_TLS;
			return 0;
		}

		if (sessionp->s_state == S_GREETED &&
		    (sessionp->s_flags & F_PEERHASAUTH) &&
		    (sessionp->s_flags & F_SECURE) &&
		    (sessionp->batch->rule.r_value.relayhost.flags & F_AUTH) &&
		    (sessionp->credentials[0] != '\0')) {
			log_debug("AUTH PLAIN %s", sessionp->credentials);
			session_respond(sessionp, "AUTH PLAIN %s",
			    sessionp->credentials);
			sessionp->s_state = S_AUTH_INIT;
			return 0;
		}

		if (sessionp->s_state == S_GREETED &&
		    !(sessionp->s_flags & F_PEERHASTLS) &&
		    (sessionp->batch->rule.r_value.relayhost.flags&F_STARTTLS)){
			/* PERM - we want TLS but it is not advertised */
			batchp->status = S_BATCH_PERMFAILURE;
			mta_batch_update_queue(batchp);
			session_destroy(sessionp);
			return 0;
		}

		if (sessionp->s_state == S_GREETED &&
		    !(sessionp->s_flags & F_PEERHASAUTH) &&
		    (sessionp->batch->rule.r_value.relayhost.flags & F_AUTH)) {
			/* PERM - we want AUTH but it is not advertised */
			batchp->status = S_BATCH_PERMFAILURE;
			mta_batch_update_queue(batchp);
			session_destroy(sessionp);
			return 0;
		}

		break;

	case 220:
		if (sessionp->s_state == S_TLS) {
			ssl_client_init(sessionp);
			bufferevent_disable(bev, EV_READ|EV_WRITE);
			sessionp->s_state = S_GREETED;
			return 0;
		}

		session_respond(sessionp, "EHLO %s", env->sc_hostname);
		sessionp->s_state = S_GREETED;
		return 1;

	case 235:
		if (sessionp->s_state == S_AUTH_INIT) {
			sessionp->s_flags |= F_AUTHENTICATED;
			sessionp->s_state = S_GREETED;
			break;
		}
		return 0;
	case 421:
	case 450:
	case 451:
		strlcpy(batchp->errorline, line, sizeof(batchp->errorline));
		mta_batch_update_queue(batchp);
		session_destroy(sessionp);
		return 0;

		/* The following codes are state dependant and will cause
		 * a batch rejection if returned at the wrong state.
		 */
	case 530:
	case 550:
		if (sessionp->s_state == S_RCPT) {
			batchp->messagep->status = (S_MESSAGE_REJECTED|S_MESSAGE_PERMFAILURE);
			strlcpy(batchp->messagep->session_errorline, line,
			    sizeof(batchp->messagep->session_errorline));
			break;
		}
	case 354:
		if (sessionp->s_state == S_RCPT && batchp->messagep == NULL) {
			sessionp->s_state = S_DATA;
			break;
		}

	case 221:
		if (sessionp->s_state == S_DONE) {
			batchp->status = S_BATCH_ACCEPTED;
			mta_batch_update_queue(batchp);
			session_destroy(sessionp);
			return 0;
		}

	case 535:
		/* Authentication failed*/
	case 552:
	case 553:
		flags |= F_ISPROTOERROR;
	default:
		/* Server sent code we know nothing about, error */
		if (!(flags & F_ISPROTOERROR))
			log_warn("SMTP session returned unknown status %d.", code);

		strlcpy(batchp->errorline, line, sizeof(batchp->errorline));
		mta_batch_update_queue(batchp);
		session_destroy(sessionp);
		return 0;
	}


	switch (sessionp->s_state) {
	case S_GREETED: {
		char *user;
		char *domain;

		if (batchp->type & T_DAEMON_BATCH) {
			user = "MAILER-DAEMON";
			domain = env->sc_hostname;
		}
		else {
			messagep = TAILQ_FIRST(&batchp->messages);
			user = messagep->sender.user;
			domain = messagep->sender.domain;
		}

		if (user[0] == '\0' && domain[0] == '\0')
			session_respond(sessionp, "MAIL FROM:<>");
		else
			session_respond(sessionp,  "MAIL FROM:<%s@%s>", user, domain);

		sessionp->s_state = S_MAIL;

		break;
	}

	case S_MAIL:
		sessionp->s_state = S_RCPT;

	case S_RCPT: {
		char *user;
		char *domain;

		/* Is this the first RCPT ? */
		if (batchp->messagep == NULL)
			messagep = TAILQ_FIRST(&batchp->messages);
		else {
			/* We already had a RCPT, mark is as accepted and
			 * fetch next one from queue if we aren't dealing
			 * with a daemon batch.
			 */
			if (batchp->type & T_DAEMON_BATCH)
				messagep = NULL;
			else {
				messagep = batchp->messagep;
				if ((messagep->status & S_MESSAGE_REJECTED) == 0)
					messagep->status = S_MESSAGE_ACCEPTED;
				messagep = TAILQ_NEXT(batchp->messagep, entry);
			}
		}
		batchp->messagep = messagep;

		if (messagep) {
			if (batchp->type & T_DAEMON_BATCH) {
				user = messagep->sender.user;
				domain = messagep->sender.domain;
			}
			else {
				user = messagep->recipient.user;
				domain = messagep->recipient.domain;
			}
			session_respond(sessionp, "RCPT TO:<%s@%s>", user, domain);
		}
		else {
			/* Do we have at least one accepted recipient ? */
			if ((batchp->type & T_DAEMON_BATCH) == 0) {
				TAILQ_FOREACH(messagep, &batchp->messages, entry) {
					if (messagep->status & S_MESSAGE_ACCEPTED)
						break;
				}
				if (messagep == NULL) {
					batchp->status = S_BATCH_PERMFAILURE;
					mta_batch_update_queue(batchp);
					session_destroy(sessionp);
					return 0;
				}
			}

			imsg_compose(env->sc_ibufs[PROC_QUEUE], IMSG_QUEUE_MESSAGE_FD,
			    0, 0, -1, batchp, sizeof(*batchp));
			bufferevent_disable(sessionp->s_bev, EV_READ);
		}
		break;
	}

	case S_DATA: {
		if (sessionp->s_flags & F_SECURE) {
			log_info("%s: version=%s cipher=%s bits=%d",
			batchp->message_id,
			SSL_get_cipher_version(sessionp->s_ssl),
			SSL_get_cipher_name(sessionp->s_ssl),
			SSL_get_cipher_bits(sessionp->s_ssl, NULL));
		}

		TAILQ_FOREACH(messagep, &batchp->messages, entry) {
			session_respond(sessionp, "X-OpenSMTPD-Loop: %s@%s",
			    messagep->session_rcpt.user,
			    messagep->session_rcpt.domain);
		}

		if (batchp->type & T_DAEMON_BATCH) {
			session_respond(sessionp, "Hi !");
			session_respond(sessionp, "%s", "");
			session_respond(sessionp, "This is the MAILER-DAEMON, please DO NOT REPLY to this e-mail it is");
			session_respond(sessionp, "just a notification to let you know that an error has occured.");
			session_respond(sessionp, "%s", "");

			if (batchp->status == S_BATCH_PERMFAILURE) {
				session_respond(sessionp, "You ran into a PERMANENT FAILURE, which means that the e-mail can't");
				session_respond(sessionp, "be delivered to the remote host no matter how much I'll try.");
				session_respond(sessionp, "%s", "");
				session_respond(sessionp, "Diagnostic:");
				session_respond(sessionp, "%s", batchp->errorline);
				session_respond(sessionp, "%s", "");
			}

			if (batchp->status == S_BATCH_TEMPFAILURE) {
				session_respond(sessionp, "You ran into a TEMPORARY FAILURE, which means that the e-mail can't");
				session_respond(sessionp, "be delivered right now, but could be deliberable at a later time. I");
				session_respond(sessionp, "will attempt until it succeeds for the next four days, then let you");
				session_respond(sessionp, "know if it didn't work out.");
				session_respond(sessionp, "%s", "");
				session_respond(sessionp, "Diagnostic:");
				session_respond(sessionp, "%s", batchp->errorline);
				session_respond(sessionp, "%s", "");
			}

			i = 0;
			TAILQ_FOREACH(messagep, &batchp->messages, entry) {
				if (messagep->status & S_MESSAGE_TEMPFAILURE) {
					if (i == 0) {
						session_respond(sessionp, "The following recipients caused a temporary failure:");
						++i;
					}

					session_respond(sessionp,
					    "\t<%s@%s>:", messagep->recipient.user, messagep->recipient.domain);
					session_respond(sessionp,
					    "%s", messagep->session_errorline);
					session_respond(sessionp, "%s", "");
				}
			}

			i = 0;
			TAILQ_FOREACH(messagep, &batchp->messages, entry) {
				if (messagep->status & S_MESSAGE_PERMFAILURE) {
					if (i == 0) {
						session_respond(sessionp,
						    "The following recipients caused a permanent failure:");
						++i;
					}

					session_respond(sessionp,
					    "\t<%s@%s>:", messagep->recipient.user, messagep->recipient.domain);
					session_respond(sessionp,
					    "%s", messagep->session_errorline);
					session_respond(sessionp, "%s", "");
				}
			}

			session_respond(sessionp, "Below is a copy of the original message:");
			session_respond(sessionp, "%s", "");
		}

		break;
	}
	case S_DONE:
		session_respond(sessionp, "QUIT");
		sessionp->s_state = S_QUIT;
		break;

	default:
		log_info("unknown command: %d", sessionp->s_state);
	}

	return 1;
}

void
mta_write_handler(struct bufferevent *bev, void *arg)
{
	struct session *sessionp = arg;
	struct batch *batchp = sessionp->batch;
	char *buf, *lbuf;
	size_t len;
	
	if (sessionp->s_state == S_QUIT) {
		bufferevent_disable(bev, EV_READ|EV_WRITE);
		log_debug("closing connection because of QUIT");
		close(sessionp->s_fd);
		return;
	}

	/* Progressively fill the output buffer with data */
	if (sessionp->s_state == S_DATA) {

		lbuf = NULL;
		if ((buf = fgetln(batchp->messagefp, &len))) {
			if (buf[len - 1] == '\n')
				buf[len - 1] = '\0';
			else {
				if ((lbuf = malloc(len + 1)) == NULL)
					fatal("mta_write_handler: malloc");
				memcpy(lbuf, buf, len);
				lbuf[len] = '\0';
				buf = lbuf;
			}

			/* "If first character of the line is a period, one
			 *  additional period is inserted at the beginning."
			 * [4.5.2]
			 */
			if (*buf == '.')
				evbuffer_add_printf(sessionp->s_bev->output, ".");

			session_respond(sessionp, "%s", buf);
			free(lbuf);
			lbuf = NULL;
		}
		else {
			session_respond(sessionp, ".");
			sessionp->s_state = S_DONE;
			fclose(batchp->messagefp);
			batchp->messagefp = NULL;
		}
	}
}

void
mta_error_handler(struct bufferevent *bev, short error, void *arg)
{
	struct session *sessionp = arg;

	if (error & (EVBUFFER_TIMEOUT|EVBUFFER_EOF)) {
		bufferevent_disable(bev, EV_READ|EV_WRITE);
		log_debug("closing connection because of an error");
		close(sessionp->s_fd);
		return;
	}
}

void
mta_batch_update_queue(struct batch *batchp)
{
	struct smtpd *env = batchp->env;
	struct message *messagep;

	while ((messagep = TAILQ_FIRST(&batchp->messages)) != NULL) {

		if (batchp->status == S_BATCH_PERMFAILURE) {
			messagep->status |= S_MESSAGE_PERMFAILURE;
		}
		
		if (batchp->status == S_BATCH_TEMPFAILURE) {
			if (messagep->status != S_MESSAGE_PERMFAILURE)
				messagep->status |= S_MESSAGE_TEMPFAILURE;
		}

		if ((messagep->status & S_MESSAGE_TEMPFAILURE) == 0 &&
		    (messagep->status & S_MESSAGE_PERMFAILURE) == 0) {
			log_info("%s: to=<%s@%s>, delay=%d, stat=Sent",
			    messagep->message_uid,
			    messagep->recipient.user,
			    messagep->recipient.domain,
			    time(NULL) - messagep->creation);
		}

		imsg_compose(env->sc_ibufs[PROC_QUEUE],
		    IMSG_QUEUE_MESSAGE_UPDATE, 0, 0, -1, messagep,
		    sizeof(struct message));
		TAILQ_REMOVE(&batchp->messages, messagep, entry);
		free(messagep);
	}

	SPLAY_REMOVE(batchtree, &env->batch_queue, batchp);

	if (batchp->messagefp)
		fclose(batchp->messagefp);

	free(batchp);

}
