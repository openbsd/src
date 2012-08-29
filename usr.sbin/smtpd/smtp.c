/*	$OpenBSD: smtp.c,v 1.109 2012/08/29 16:26:17 gilles Exp $	*/

/*
 * Copyright (c) 2008 Gilles Chehade <gilles@openbsd.org>
 * Copyright (c) 2008 Pierre-Yves Ritschard <pyr@openbsd.org>
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

#include <err.h>
#include <errno.h>
#include <event.h>
#include <imsg.h>
#include <netdb.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "smtpd.h"
#include "log.h"

static void smtp_imsg(struct imsgev *, struct imsg *);
static void smtp_shutdown(void);
static void smtp_sig_handler(int, short, void *);
static void smtp_setup_events(void);
static void smtp_pause(void);
static int smtp_enqueue(uid_t *);
static void smtp_accept(int, short, void *);
static struct session *smtp_new(struct listener *);
static struct session *session_lookup(uint64_t);

#define	SMTP_FD_RESERVE	5

static void
smtp_imsg(struct imsgev *iev, struct imsg *imsg)
{
	struct session		 skey;
	struct submit_status	*ss;
	struct listener		*l;
	struct session		*s;
	struct auth		*auth;
	struct ssl		*ssl;
	struct dns		*dns;

	log_imsg(PROC_SMTP, iev->proc, imsg);

	if (iev->proc == PROC_LKA) {
		switch (imsg->hdr.type) {
		case IMSG_DNS_PTR:
			dns = imsg->data;
			s = session_lookup(dns->id);
			if (s == NULL)
				fatalx("smtp: impossible quit");
			strlcpy(s->s_hostname,
			    dns->error ? "<unknown>" : dns->host,
			    sizeof s->s_hostname);
			strlcpy(s->s_msg.hostname, s->s_hostname,
			    sizeof s->s_msg.hostname);
			session_pickup(s, NULL);
			return;
		}
	}

	if (iev->proc == PROC_MFA) {
		switch (imsg->hdr.type) {
		case IMSG_MFA_CONNECT:
		case IMSG_MFA_HELO:
		case IMSG_MFA_MAIL:
		case IMSG_MFA_RCPT:
		case IMSG_MFA_DATALINE:
		case IMSG_MFA_QUIT:
		case IMSG_MFA_RSET:
			ss = imsg->data;
			s = session_lookup(ss->id);
			if (s == NULL)
				return;
			session_pickup(s, ss);
			return;
		case IMSG_MFA_CLOSE:
			return;
		}
	}

	if (iev->proc == PROC_QUEUE) {
		ss = imsg->data;

		switch (imsg->hdr.type) {
		case IMSG_QUEUE_CREATE_MESSAGE:
			s = session_lookup(ss->id);
			if (s == NULL)
				return;
			s->s_msg.id = ((uint64_t)ss->u.msgid) << 32;
			session_pickup(s, ss);
			return;

		case IMSG_QUEUE_MESSAGE_FILE:
			s = session_lookup(ss->id);
			if (s == NULL) {
				close(imsg->fd);
				return;
			}
			s->datafp = fdopen(imsg->fd, "w");
			if (s->datafp == NULL) {
				/* queue may have experienced tempfail. */
				if (ss->code != 421)
					fatalx("smtp: fdopen");
				close(imsg->fd);
			}
			session_pickup(s, ss);
			return;

		case IMSG_QUEUE_TEMPFAIL:
			skey.s_id = ss->id;
			/* do not use lookup since this is not a expected imsg -- eric@ */
			s = SPLAY_FIND(sessiontree, &env->sc_sessions, &skey);
			if (s == NULL)
				fatalx("smtp: session is gone");
			s->s_dstatus |= DS_TEMPFAILURE;
			return;

		case IMSG_QUEUE_COMMIT_ENVELOPES:
			s = session_lookup(ss->id);
			if (s == NULL)
				return;
			session_pickup(s, ss);
			return;

		case IMSG_QUEUE_COMMIT_MESSAGE:
			s = session_lookup(ss->id);
			if (s == NULL)
				return;
			session_pickup(s, ss);
			return;

		case IMSG_SMTP_ENQUEUE:
			imsg_compose_event(iev, IMSG_SMTP_ENQUEUE, 0, 0,
			    smtp_enqueue(NULL), imsg->data,
			    imsg->hdr.len - sizeof imsg->hdr);
			return;
		}
	}

	if (iev->proc == PROC_PARENT) {
		switch (imsg->hdr.type) {

		case IMSG_CONF_START:
			if (env->sc_flags & SMTPD_CONFIGURING)
				return;
			env->sc_flags |= SMTPD_CONFIGURING;
			env->sc_listeners = calloc(1, sizeof *env->sc_listeners);
			env->sc_ssl = calloc(1, sizeof *env->sc_ssl);
			if (env->sc_listeners == NULL || env->sc_ssl == NULL)
				fatal(NULL);
			TAILQ_INIT(env->sc_listeners);
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
			ssl->ssl_key = strdup((char *)imsg->data + sizeof *ssl +
			    ssl->ssl_cert_len);
			if (ssl->ssl_key == NULL)
				fatal(NULL);
			if (ssl->ssl_dhparams_len) {
				ssl->ssl_dhparams = strdup((char *)imsg->data
				    + sizeof *ssl + ssl->ssl_cert_len +
				    ssl->ssl_key_len);
				if (ssl->ssl_dhparams == NULL)
					fatal(NULL);
			}
			if (ssl->ssl_ca_len) {
				ssl->ssl_ca = strdup((char *)imsg->data
				    + sizeof *ssl + ssl->ssl_cert_len +
				    ssl->ssl_key_len + ssl->ssl_dhparams_len);
				if (ssl->ssl_ca == NULL)
					fatal(NULL);
			}

			SPLAY_INSERT(ssltree, env->sc_ssl, ssl);
			return;

		case IMSG_CONF_LISTENER:
			if (!(env->sc_flags & SMTPD_CONFIGURING))
				return;
			l = calloc(1, sizeof *l);
			if (l == NULL)
				fatal(NULL);
			*l = *(struct listener *)imsg->data;
			l->fd = imsg->fd;
			if (l->fd < 0)
				fatalx("smtp: listener pass failed");
			if (l->flags & F_SSL) {
				struct ssl key;

				strlcpy(key.ssl_name, l->ssl_cert_name,
				    sizeof key.ssl_name);
				l->ssl = SPLAY_FIND(ssltree, env->sc_ssl, &key);
				if (l->ssl == NULL)
					fatalx("smtp: ssltree out of sync");
			}
			TAILQ_INSERT_TAIL(env->sc_listeners, l, entry);
			return;

		case IMSG_CONF_END:
			if (!(env->sc_flags & SMTPD_CONFIGURING))
				return;
			smtp_setup_events();
			env->sc_flags &= ~SMTPD_CONFIGURING;
			return;

		case IMSG_PARENT_AUTHENTICATE:
			auth = imsg->data;
			s = session_lookup(auth->id);
			if (s == NULL)
				return;
			if (auth->success) {
				s->s_flags |= F_AUTHENTICATED;
				s->s_msg.flags |= DF_AUTHENTICATED;
			} else {
				s->s_flags &= ~F_AUTHENTICATED;
				s->s_msg.flags &= ~DF_AUTHENTICATED;
			}
			session_pickup(s, NULL);
			return;

		case IMSG_CTL_VERBOSE:
			log_verbose(*(int *)imsg->data);
			return;
		}
	}

	if (iev->proc == PROC_CONTROL) {
		switch (imsg->hdr.type) {
		case IMSG_SMTP_ENQUEUE:
			imsg_compose_event(iev, IMSG_SMTP_ENQUEUE,
			    imsg->hdr.peerid, 0, smtp_enqueue(imsg->data),
			    NULL, 0);
			return;

		case IMSG_SMTP_PAUSE:
			log_debug("smtp: pausing listening sockets");
			smtp_pause();
			env->sc_flags |= SMTPD_SMTP_PAUSED;
			return;

		case IMSG_SMTP_RESUME:
			log_debug("smtp: resuming listening sockets");
			env->sc_flags &= ~SMTPD_SMTP_PAUSED;
			smtp_resume();
			return;
		}
	}

	errx(1, "smtp_imsg: unexpected %s imsg", imsg_to_str(imsg->hdr.type));
}

static void
smtp_sig_handler(int sig, short event, void *p)
{
	switch (sig) {
	case SIGINT:
	case SIGTERM:
		smtp_shutdown();
		break;
	default:
		fatalx("smtp_sig_handler: unexpected signal");
	}
}

static void
smtp_shutdown(void)
{
	log_info("smtp server exiting");
	_exit(0);
}

pid_t
smtp(void)
{
	pid_t		 pid;
	struct passwd	*pw;

	struct event	 ev_sigint;
	struct event	 ev_sigterm;

	struct peer peers[] = {
		{ PROC_PARENT,	imsg_dispatch },
		{ PROC_MFA,	imsg_dispatch },
		{ PROC_QUEUE,	imsg_dispatch },
		{ PROC_LKA,	imsg_dispatch },
		{ PROC_CONTROL,	imsg_dispatch }
	};

	switch (pid = fork()) {
	case -1:
		fatal("smtp: cannot fork");
	case 0:
		break;
	default:
		return (pid);
	}

	purge_config(PURGE_EVERYTHING);

	pw = env->sc_pw;

	if (chroot(pw->pw_dir) == -1)
		fatal("smtp: chroot");
	if (chdir("/") == -1)
		fatal("smtp: chdir(\"/\")");

	smtpd_process = PROC_SMTP;
	setproctitle("%s", env->sc_title[smtpd_process]);

	if (setgroups(1, &pw->pw_gid) ||
	    setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) ||
	    setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid))
		fatal("smtp: cannot drop privileges");

	imsg_callback = smtp_imsg;
	event_init();

	signal_set(&ev_sigint, SIGINT, smtp_sig_handler, NULL);
	signal_set(&ev_sigterm, SIGTERM, smtp_sig_handler, NULL);
	signal_add(&ev_sigint, NULL);
	signal_add(&ev_sigterm, NULL);
	signal(SIGPIPE, SIG_IGN);
	signal(SIGHUP, SIG_IGN);

	fdlimit(1.0);

	config_pipes(peers, nitems(peers));
	config_peers(peers, nitems(peers));

	if (event_dispatch() < 0)
		fatal("event_dispatch");
	smtp_shutdown();

	return (0);
}

static void
smtp_setup_events(void)
{
	struct listener *l;

	TAILQ_FOREACH(l, env->sc_listeners, entry) {
		log_debug("smtp: listen on %s port %d flags 0x%01x"
		    " cert \"%s\"", ss_to_text(&l->ss), ntohs(l->port),
		    l->flags, l->ssl_cert_name);

		session_socket_blockmode(l->fd, BM_NONBLOCK);
		if (listen(l->fd, SMTPD_BACKLOG) == -1)
			fatal("listen");
		event_set(&l->ev, l->fd, EV_READ|EV_PERSIST, smtp_accept, l);

		if (!(env->sc_flags & SMTPD_SMTP_PAUSED))
			event_add(&l->ev, NULL);

		ssl_setup(l);
	}

	log_debug("smtp: will accept at most %d clients",
	    getdtablesize() - getdtablecount() - SMTP_FD_RESERVE + 1);
}

static void
smtp_pause(void)
{
	struct listener *l;

	if (env->sc_flags & (SMTPD_SMTP_DISABLED|SMTPD_SMTP_PAUSED))
		return;

	TAILQ_FOREACH(l, env->sc_listeners, entry)
		event_del(&l->ev);
}

void
smtp_resume(void)
{
	struct listener *l;

	if (env->sc_flags & (SMTPD_SMTP_DISABLED|SMTPD_SMTP_PAUSED))
		return;

	TAILQ_FOREACH(l, env->sc_listeners, entry)
		event_add(&l->ev, NULL);
}

static int
smtp_enqueue(uid_t *euid)
{
	static struct listener		 local, *l;
	static struct sockaddr_storage	 sa;
	struct session			*s;
	int				 fd[2];

	if (l == NULL) {
		struct addrinfo hints, *res;

		l = &local;
		strlcpy(l->tag, "local", sizeof(l->tag));

		bzero(&hints, sizeof(hints));
		hints.ai_family = PF_UNSPEC;
		hints.ai_flags = AI_NUMERICHOST;

		if (getaddrinfo("::1", NULL, &hints, &res))
			fatal("getaddrinfo");
		memcpy(&sa, res->ai_addr, res->ai_addrlen);
		freeaddrinfo(res);
	}

	/*
	 * Some enqueue requests buffered in IMSG may still arrive even after
	 * call to smtp_pause() because enqueue listener is not a real socket
	 * and thus cannot be paused properly.
	 */
	if (env->sc_flags & SMTPD_SMTP_PAUSED)
		return (-1);

	if ((s = smtp_new(l)) == NULL)
		return (-1);

	if (socketpair(AF_UNIX, SOCK_STREAM, PF_UNSPEC, fd))
		fatal("socketpair");

	s->s_io.sock = fd[0];
	s->s_ss = sa;
	s->s_msg.flags |= DF_ENQUEUED;

	if (euid)
		bsnprintf(s->s_hostname, sizeof(s->s_hostname), "%d@localhost",
		    *euid);
	else {
		strlcpy(s->s_hostname, "localhost", sizeof(s->s_hostname));
		s->s_msg.flags |= DF_BOUNCE;
	}

	strlcpy(s->s_msg.hostname, s->s_hostname,
	    sizeof(s->s_msg.hostname));

	session_pickup(s, NULL);

	return (fd[1]);
}

static void
smtp_accept(int fd, short event, void *p)
{
	struct listener		*l = p;
	struct session		*s;
	socklen_t		 len;

	if ((s = smtp_new(l)) == NULL) {
		log_warnx("smtp: client limit hit, disabling incoming connections");
		goto pause;
	}

	len = sizeof(s->s_ss);
	if ((s->s_io.sock = accept(fd, (struct sockaddr *)&s->s_ss, &len)) == -1) {
		if (errno == ENFILE || errno == EMFILE) {
			log_warnx("smtp: fd exhaustion, disabling incoming connections");
			goto pause;
		}
		if (errno == EINTR || errno == ECONNABORTED)
			return;
		fatal("smtp_accept");
	}

	io_set_timeout(&s->s_io, SMTPD_SESSION_TIMEOUT * 1000);
	io_set_write(&s->s_io);
	dns_query_ptr(&s->s_ss, s->s_id);
	return;

pause:
	smtp_pause();
	env->sc_flags |= SMTPD_SMTP_DISABLED;
	return;
}


static struct session *
smtp_new(struct listener *l)
{
	struct session	*s;

	log_debug("smtp: new client on listener: %p", l);

	if (env->sc_flags & SMTPD_SMTP_PAUSED)
		fatalx("smtp_new: unexpected client");

	if ((s = calloc(1, sizeof(*s))) == NULL)
		fatal(NULL);
	s->s_id = generate_uid();
	s->s_l = l;
	strlcpy(s->s_msg.tag, l->tag, sizeof(s->s_msg.tag));
	SPLAY_INSERT(sessiontree, &env->sc_sessions, s);

	stat_increment("smtp.session", 1);
	
	if (getdtablesize() - getdtablecount() < SMTP_FD_RESERVE) {
		return NULL;
	}

	if (s->s_l->ss.ss_family == AF_INET)
		stat_increment("smtp.session.inet4", 1);
	if (s->s_l->ss.ss_family == AF_INET6)
		stat_increment("smtp.session.inet6", 1);

	iobuf_init(&s->s_iobuf, MAX_LINE_SIZE, MAX_LINE_SIZE);
	io_init(&s->s_io, -1, s, session_io, &s->s_iobuf);
	s->s_state = S_CONNECTED;

	return (s);
}

void
smtp_destroy(struct session *session)
{
	if (getdtablesize() - getdtablecount() < SMTP_FD_RESERVE)
		return;

	if (env->sc_flags & SMTPD_SMTP_DISABLED) {
		log_warnx("smtp: fd exaustion over, re-enabling incoming connections");
		env->sc_flags &= ~SMTPD_SMTP_DISABLED;
	}
	smtp_resume();
}


/*
 * Helper function for handling IMSG replies.
 */
static struct session *
session_lookup(uint64_t id)
{
	struct session	 key;
	struct session	*s;

	key.s_id = id;
	s = SPLAY_FIND(sessiontree, &env->sc_sessions, &key);
	if (s == NULL)
		fatalx("session_lookup: session is gone");

	if (s->s_flags & F_ZOMBIE) {
		session_destroy(s, "(finalizing)");
		s = NULL;
	}

	return (s);
}
