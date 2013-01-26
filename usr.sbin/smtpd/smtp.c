/*	$OpenBSD: smtp.c,v 1.123 2013/01/26 09:37:23 gilles Exp $	*/

/*
 * Copyright (c) 2008 Gilles Chehade <gilles@poolp.org>
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

#include <openssl/ssl.h>

#include "smtpd.h"
#include "log.h"
#include "ssl.h"

static void smtp_imsg(struct mproc *, struct imsg *);
static void smtp_shutdown(void);
static void smtp_sig_handler(int, short, void *);
static void smtp_setup_events(void);
static void smtp_pause(void);
static void smtp_resume(void);
static void smtp_accept(int, short, void *);
static int smtp_enqueue(uid_t *);
static int smtp_can_accept(void);

#define	SMTP_FD_RESERVE	5
static size_t	sessions;

static void
smtp_imsg(struct mproc *p, struct imsg *imsg)
{
	struct listener	*l;
	struct ssl	*ssl;
	struct msg	 m;
	int		 v;

	if (p->proc == PROC_LKA) {
		switch (imsg->hdr.type) {
		case IMSG_DNS_PTR:
		case IMSG_LKA_EXPAND_RCPT:
		case IMSG_LKA_AUTHENTICATE:
		case IMSG_LKA_SSL_INIT:
		case IMSG_LKA_SSL_VERIFY:
			smtp_session_imsg(p, imsg);
			return;
		}
	}

	if (p->proc == PROC_MFA) {
		switch (imsg->hdr.type) {
		case IMSG_MFA_SMTP_DATA:
		case IMSG_MFA_SMTP_RESPONSE:
			smtp_session_imsg(p, imsg);
			return;
		}
	}

	if (p->proc == PROC_QUEUE) {
		switch (imsg->hdr.type) {
		case IMSG_QUEUE_CREATE_MESSAGE:
		case IMSG_QUEUE_MESSAGE_FILE:
		case IMSG_QUEUE_SUBMIT_ENVELOPE:
		case IMSG_QUEUE_COMMIT_ENVELOPES:
		case IMSG_QUEUE_COMMIT_MESSAGE:
			smtp_session_imsg(p, imsg);
			return;

		case IMSG_SMTP_ENQUEUE_FD:
			m_compose(p, IMSG_SMTP_ENQUEUE_FD, 0, 0,
			    smtp_enqueue(NULL), imsg->data,
			    imsg->hdr.len - sizeof imsg->hdr);
			return;
		}
	}

	if (p->proc == PROC_PARENT) {
		switch (imsg->hdr.type) {

		case IMSG_CONF_START:
			if (env->sc_flags & SMTPD_CONFIGURING)
				return;
			env->sc_flags |= SMTPD_CONFIGURING;
			env->sc_listeners = calloc(1,
			    sizeof *env->sc_listeners);
			if (env->sc_listeners == NULL)
				fatal(NULL);
			env->sc_ssl_dict = calloc(1, sizeof *env->sc_ssl_dict);
			if (env->sc_ssl_dict == NULL)
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
			ssl->ssl_cert = xstrdup((char *)imsg->data +
			    sizeof *ssl, "smtp:ssl_cert");
			ssl->ssl_key = xstrdup((char *)imsg->data +
			    sizeof *ssl + ssl->ssl_cert_len, "smtp:ssl_key");
			if (ssl->ssl_dhparams_len) {
				ssl->ssl_dhparams = xstrdup((char *)imsg->data
				    + sizeof *ssl + ssl->ssl_cert_len +
				    ssl->ssl_key_len, "smtp:ssl_dhparams");
			}
			if (ssl->ssl_ca_len) {
				ssl->ssl_ca = xstrdup((char *)imsg->data
				    + sizeof *ssl + ssl->ssl_cert_len +
				    ssl->ssl_key_len + ssl->ssl_dhparams_len,
				    "smtp:ssl_ca");
			}
			dict_set(env->sc_ssl_dict, ssl->ssl_name, ssl);
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
				l->ssl = dict_get(env->sc_ssl_dict, l->ssl_cert_name);
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

		case IMSG_CTL_VERBOSE:
			m_msg(&m, imsg);
			m_get_int(&m, &v);
			m_end(&m);
			log_verbose(v);
			return;

		case IMSG_CTL_PROFILE:
			m_msg(&m, imsg);
			m_get_int(&m, &v);
			m_end(&m);
			profiling = v;
			return;
		}
	}

	if (p->proc == PROC_CONTROL) {
		switch (imsg->hdr.type) {
		case IMSG_SMTP_ENQUEUE_FD:
			m_compose(p, IMSG_SMTP_ENQUEUE_FD, imsg->hdr.peerid, 0,
			    smtp_enqueue(imsg->data), NULL, 0);
			return;

		case IMSG_CTL_PAUSE_SMTP:
			log_debug("debug: smtp: pausing listening sockets");
			smtp_pause();
			env->sc_flags |= SMTPD_SMTP_PAUSED;
			return;

		case IMSG_CTL_RESUME_SMTP:
			log_debug("debug: smtp: resuming listening sockets");
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
	log_info("info: smtp server exiting");
	_exit(0);
}

pid_t
smtp(void)
{
	pid_t		 pid;
	struct passwd	*pw;
	struct event	 ev_sigint;
	struct event	 ev_sigterm;

	switch (pid = fork()) {
	case -1:
		fatal("smtp: cannot fork");
	case 0:
		env->sc_pid = getpid();
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

	config_process(PROC_SMTP);

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

	config_peer(PROC_CONTROL);
	config_peer(PROC_PARENT);
	config_peer(PROC_LKA);
	config_peer(PROC_MFA);
	config_peer(PROC_QUEUE);
	config_done();

	if (event_dispatch() < 0)
		fatal("event_dispatch");
	smtp_shutdown();

	return (0);
}

static void
smtp_setup_events(void)
{
	struct listener *l;
	struct ssl	*ssl, key;

	TAILQ_FOREACH(l, env->sc_listeners, entry) {
		log_debug("debug: smtp: listen on %s port %d flags 0x%01x"
		    " cert \"%s\"", ss_to_text(&l->ss), ntohs(l->port),
		    l->flags, l->ssl_cert_name);

		session_socket_blockmode(l->fd, BM_NONBLOCK);
		if (listen(l->fd, SMTPD_BACKLOG) == -1)
			fatal("listen");
		event_set(&l->ev, l->fd, EV_READ|EV_PERSIST, smtp_accept, l);

		if (!(env->sc_flags & SMTPD_SMTP_PAUSED))
			event_add(&l->ev, NULL);

		if (!(l->flags & F_SSL))
			continue;

		if (strlcpy(key.ssl_name, l->ssl_cert_name, sizeof(key.ssl_name))
		    >= sizeof(key.ssl_name))
			fatal("smtp_setup_events: certificate name truncated");
		if ((ssl = dict_get(env->sc_ssl_dict, l->ssl_cert_name)) == NULL)
			fatal("smtp_setup_events: certificate tree corrupted");
		if (! ssl_setup((SSL_CTX **)&l->ssl_ctx, ssl))
			fatal("smtp_setup_events: ssl_setup failure");
	}

	purge_config(PURGE_SSL);

	log_debug("debug: smtp: will accept at most %d clients",
	    (getdtablesize() - getdtablecount())/2 - SMTP_FD_RESERVE);
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

static void
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
	static struct listener	 local, *listener = NULL;
	char			 buf[MAXHOSTNAMELEN], *hostname;
	int			 fd[2];

	if (listener == NULL) {
		listener = &local;
		strlcpy(listener->tag, "local", sizeof(listener->tag));
		listener->ss.ss_family = AF_LOCAL;
		listener->ss.ss_len = sizeof(struct sockaddr *);
	}

	/*
	 * Some enqueue requests buffered in IMSG may still arrive even after
	 * call to smtp_pause() because enqueue listener is not a real socket
	 * and thus cannot be paused properly.
	 */
	if (env->sc_flags & SMTPD_SMTP_PAUSED)
		return (-1);

	/* XXX dont' fatal here */
	if (socketpair(AF_UNIX, SOCK_STREAM, PF_UNSPEC, fd))
		fatal("socketpair");

	hostname = "localhost";
	if (euid) {
		snprintf(buf, sizeof(buf), "%d@localhost", *euid);
		hostname = buf;
	}

	if ((smtp_session(listener, fd[0], &listener->ss, hostname)) == -1) {
		close(fd[0]);
		close(fd[1]);
		return (-1);
	}

	sessions++;
	stat_increment("smtp.session", 1);
	stat_increment("smtp.session.local", 1);

	return (fd[1]);
}

static void
smtp_accept(int fd, short event, void *p)
{
	struct listener		*listener = p;
	struct sockaddr_storage	 ss;
	socklen_t		 len;
	int			 sock;

	if (env->sc_flags & SMTPD_SMTP_PAUSED)
		fatalx("smtp_session: unexpected client");

	if (! smtp_can_accept()) {
		log_warnx("warn: Disabling incoming SMTP connections: "
		    "Client limit reached");
		goto pause;
	}

	len = sizeof(ss);
	if ((sock = accept(fd, (struct sockaddr *)&ss, &len)) == -1) {
		if (errno == ENFILE || errno == EMFILE) {
			log_warn("warn: Disabling incoming SMTP connections");
			goto pause;
		}
		if (errno == EINTR || errno == ECONNABORTED)
			return;
		fatal("smtp_accept");
	}

	if (smtp_session(listener, sock, &ss, NULL) == -1) {
		log_warn("warn: Failed to create SMTP session");
		close(sock);
		return;
	}

	sessions++;
	stat_increment("smtp.session", 1);
	if (listener->ss.ss_family == AF_LOCAL)
		stat_increment("smtp.session.local", 1);
	if (listener->ss.ss_family == AF_INET)
		stat_increment("smtp.session.inet4", 1);
	if (listener->ss.ss_family == AF_INET6)
		stat_increment("smtp.session.inet6", 1);
	return;

pause:
	smtp_pause();
	env->sc_flags |= SMTPD_SMTP_DISABLED;
	return;
}

static int
smtp_can_accept(void)
{
	size_t max;

	max = (getdtablesize() - getdtablecount()) / 2 - SMTP_FD_RESERVE;

	return (sessions < max);
}

void
smtp_collect(void)
{
	sessions--;
	stat_decrement("smtp.session", 1);

	if (!smtp_can_accept())
		return;

	if (env->sc_flags & SMTPD_SMTP_DISABLED) {
		log_warnx("warn: smtp: "
		    "fd exaustion over, re-enabling incoming connections");
		env->sc_flags &= ~SMTPD_SMTP_DISABLED;
		smtp_resume();
	}
}
