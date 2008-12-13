/*	$OpenBSD: mfa.c,v 1.3 2008/12/13 23:19:34 jacekm Exp $	*/

/*
 * Copyright (c) 2008 Gilles Chehade <gilles@openbsd.org>
 * Copyright (c) 2008 Pierre-Yves Ritschard <pyr@openbsd.org>
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
#include <sys/time.h>

#include <event.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "smtpd.h"

__dead void	mfa_shutdown(void);
void		mfa_sig_handler(int, short, void *);
void		mfa_dispatch_parent(int, short, void *);
void		mfa_dispatch_smtp(int, short, void *);
void		mfa_dispatch_lka(int, short, void *);
void		mfa_setup_events(struct smtpd *);
void		mfa_disable_events(struct smtpd *);
void		mfa_timeout(int, short, void *);

void
mfa_sig_handler(int sig, short event, void *p)
{
	switch (sig) {
	case SIGINT:
	case SIGTERM:
		mfa_shutdown();
		break;
	default:
		fatalx("mfa_sig_handler: unexpected signal");
	}
}

void
mfa_dispatch_parent(int sig, short event, void *p)
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
			fatal("parent_dispatch_mfa: imsg_read error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		default:
			log_debug("parent_dispatch_mfa: unexpected imsg %d",
			    imsg.hdr.type);
			break;
		}
		imsg_free(&imsg);
	}
	imsg_event_add(ibuf);
}

void
mfa_dispatch_smtp(int sig, short event, void *p)
{
	struct smtpd		*env = p;
	struct imsgbuf		*ibuf;
	struct imsg		 imsg;
	ssize_t			 n;

	ibuf = env->sc_ibufs[PROC_SMTP];
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
			fatal("mfa_dispatch_smtp: imsg_read error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_MFA_MAIL: {
			struct message		*m;
			struct submit_status	 ss;

			m = imsg.data;
			log_debug("mfa_dispatch_smtp: testing return path");
			ss.id = m->id;
			ss.code = 250;
			ss.u.path = m->sender;

			imsg_compose(env->sc_ibufs[PROC_LKA], IMSG_LKA_MAIL, 0,
			    0, -1, &ss, sizeof(ss));
			break;
		}
		case IMSG_MFA_RCPT: {
			struct message_recipient *mr;
			struct submit_status	 ss;

			mr = imsg.data;
			log_debug("mfa_dispatch_smtp: testing forward path");
			ss.id = mr->id;
			ss.code = 250;
			ss.u.path = mr->path;
			ss.ss = mr->ss;

			imsg_compose(env->sc_ibufs[PROC_LKA], IMSG_LKA_RCPT, 0,
			    0, -1, &ss, sizeof(ss));
			break;
		}
		default:
			log_debug("mfa_dispatch_smtp: unexpected imsg %d",
			    imsg.hdr.type);
			break;
		}
		imsg_free(&imsg);
	}
	imsg_event_add(ibuf);
}

void
mfa_dispatch_lka(int sig, short event, void *p)
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
			fatal("mfa_dispatch_lka: imsg_read error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_LKA_MAIL: {
			struct submit_status	 *ss;

			ss = imsg.data;
			imsg_compose(env->sc_ibufs[PROC_SMTP], IMSG_MFA_MAIL,
			    0, 0, -1, ss, sizeof(*ss));
			break;
		}
		case IMSG_LKA_RCPT: {
			struct submit_status	 *ss;

			ss = imsg.data;
			imsg_compose(env->sc_ibufs[PROC_SMTP], IMSG_MFA_RCPT,
			    0, 0, -1, ss, sizeof(*ss));
			break;
		}
		default:
			log_debug("mfa_dispatch_lka: unexpected imsg %d",
			    imsg.hdr.type);
			break;
		}
		imsg_free(&imsg);
	}
	imsg_event_add(ibuf);
}

void
mfa_shutdown(void)
{
	log_info("mail filter exiting");
	_exit(0);
}

void
mfa_setup_events(struct smtpd *env)
{
	struct timeval	 tv;

	evtimer_set(&env->sc_ev, mfa_timeout, env);
	tv.tv_sec = 3;
	tv.tv_usec = 0;
	evtimer_add(&env->sc_ev, &tv);
}

void
mfa_disable_events(struct smtpd *env)
{
	evtimer_del(&env->sc_ev);
}

void
mfa_timeout(int fd, short event, void *p)
{
	struct smtpd		*env = p;
	struct timeval		 tv;

	tv.tv_sec = 3;
	tv.tv_usec = 0;
	evtimer_add(&env->sc_ev, &tv);
}

pid_t
mfa(struct smtpd *env)
{
	pid_t		 pid;
	struct passwd	*pw;

	struct event	 ev_sigint;
	struct event	 ev_sigterm;

	struct peer peers[] = {
		{ PROC_PARENT,	mfa_dispatch_parent },
		{ PROC_SMTP,	mfa_dispatch_smtp },
		{ PROC_LKA,	mfa_dispatch_lka },
	};

	switch (pid = fork()) {
	case -1:
		fatal("mfa: cannot fork");
	case 0:
		break;
	default:
		return (pid);
	}

//	purge_config(env, PURGE_EVERYTHING);

	pw = env->sc_pw;

#ifndef DEBUG
	if (chroot(pw->pw_dir) == -1)
		fatal("mfa: chroot");
	if (chdir("/") == -1)
		fatal("mfa: chdir(\"/\")");
#else
#warning disabling privilege revocation and chroot in DEBUG MODE
#endif

	setproctitle("mail filter agent");
	smtpd_process = PROC_MFA;

#ifndef DEBUG
	if (setgroups(1, &pw->pw_gid) ||
	    setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) ||
	    setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid))
		fatal("mfa: cannot drop privileges");
#endif

	event_init();

	signal_set(&ev_sigint, SIGINT, mfa_sig_handler, env);
	signal_set(&ev_sigterm, SIGTERM, mfa_sig_handler, env);
	signal_add(&ev_sigint, NULL);
	signal_add(&ev_sigterm, NULL);
	signal(SIGPIPE, SIG_IGN);
	signal(SIGHUP, SIG_IGN);

	config_peers(env, peers, 3);

	mfa_setup_events(env);
	event_dispatch();
	mfa_shutdown();

	return (0);
}

int
msg_cmp(struct message *m1, struct message *m2)
{
	/*
	 * do not return u_int64_t's
	 */
	if (m1->id - m2->id > 0)
		return (1);
	else if (m1->id - m2->id < 0)
		return (-1);
	else
		return (0);
}

SPLAY_GENERATE(msgtree, message, nodes, msg_cmp);
