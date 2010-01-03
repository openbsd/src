/*	$OpenBSD: mfa.c,v 1.43 2010/01/03 14:37:37 chl Exp $	*/

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

#include <ctype.h>
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
void		mfa_dispatch_control(int, short, void *);
void		mfa_setup_events(struct smtpd *);
void		mfa_disable_events(struct smtpd *);

void		mfa_test_mail(struct smtpd *, struct message *);
void		mfa_test_rcpt(struct smtpd *, struct message *);
void		mfa_test_rcpt_resume(struct smtpd *, struct submit_status *);

int		strip_source_route(char *, size_t);

struct rule    *ruleset_match(struct smtpd *, struct path *, struct sockaddr_storage *);

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
			fatal("mfa_dispatch_parent: imsg_get error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_CTL_VERBOSE: {
			int verbose;

			IMSG_SIZE_CHECK(&verbose);

			memcpy(&verbose, imsg.data, sizeof(verbose));
			log_verbose(verbose);
			break;
		}
		default:
			log_warnx("mfa_dispatch_parent: got imsg %d",
			    imsg.hdr.type);
			fatalx("mfa_dispatch_parent: unexpected imsg");
		}
		imsg_free(&imsg);
	}
	imsg_event_add(iev);
}

void
mfa_dispatch_smtp(int sig, short event, void *p)
{
	struct smtpd		*env = p;
	struct imsgev		*iev;
	struct imsgbuf		*ibuf;
	struct imsg		 imsg;
	ssize_t			 n;

	iev = env->sc_ievs[PROC_SMTP];
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
			fatal("mfa_dispatch_smtp: imsg_get error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_MFA_MAIL: {
			struct message	*m = imsg.data;

			IMSG_SIZE_CHECK(m);

			mfa_test_mail(env, m);
			break;
		}

		case IMSG_MFA_RCPT: {
			struct message	*m = imsg.data;

			IMSG_SIZE_CHECK(m);

			mfa_test_rcpt(env, m);
			break;
		}

		default:
			log_warnx("mfa_dispatch_smtp: got imsg %d",
			    imsg.hdr.type);
			fatalx("mfa_dispatch_smtp: unexpected imsg");
		}
		imsg_free(&imsg);
	}
	imsg_event_add(iev);
}

void
mfa_dispatch_lka(int sig, short event, void *p)
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
			fatal("mfa_dispatch_lka: imsg_get error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_LKA_MAIL: {
			struct submit_status	 *ss = imsg.data;

			IMSG_SIZE_CHECK(ss);

			imsg_compose_event(env->sc_ievs[PROC_SMTP], IMSG_MFA_MAIL,
			    0, 0, -1, ss, sizeof(*ss));
			break;
		}
		case IMSG_LKA_RCPT: {
			struct submit_status	 *ss = imsg.data;

			IMSG_SIZE_CHECK(ss);

			imsg_compose_event(env->sc_ievs[PROC_SMTP], IMSG_MFA_RCPT,
			    0, 0, -1, ss, sizeof(*ss));
			break;
		}
		case IMSG_LKA_RULEMATCH: {
			struct submit_status	 *ss = imsg.data;

			IMSG_SIZE_CHECK(ss);

			mfa_test_rcpt_resume(env, ss);
			break;
		}
		default:
			log_warnx("mfa_dispatch_lka: got imsg %d",
			    imsg.hdr.type);
			fatalx("mfa_dispatch_lka: unexpected imsg");
		}
		imsg_free(&imsg);
	}
	imsg_event_add(iev);
}

void
mfa_dispatch_control(int sig, short event, void *p)
{
	struct smtpd		*env = p;
	struct imsgev		*iev;
	struct imsgbuf		*ibuf;
	struct imsg		 imsg;
	ssize_t			 n;

	iev = env->sc_ievs[PROC_CONTROL];
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
			fatal("mfa_dispatch_control: imsg_get error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		default:
			log_warnx("mfa_dispatch_control: got imsg %d",
			    imsg.hdr.type);
			fatalx("mfa_dispatch_control: unexpected imsg");
		}
		imsg_free(&imsg);
	}
	imsg_event_add(iev);
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
}

void
mfa_disable_events(struct smtpd *env)
{
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
		{ PROC_CONTROL,	mfa_dispatch_control},
	};

	switch (pid = fork()) {
	case -1:
		fatal("mfa: cannot fork");
	case 0:
		break;
	default:
		return (pid);
	}

	purge_config(env, PURGE_EVERYTHING);

	pw = env->sc_pw;

#ifndef DEBUG
	if (chroot(pw->pw_dir) == -1)
		fatal("mfa: chroot");
	if (chdir("/") == -1)
		fatal("mfa: chdir(\"/\")");
#else
#warning disabling privilege revocation and chroot in DEBUG MODE
#endif

	smtpd_process = PROC_MFA;
	setproctitle("%s", env->sc_title[smtpd_process]);

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

	config_pipes(env, peers, nitems(peers));
	config_peers(env, peers, nitems(peers));

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

void
mfa_test_mail(struct smtpd *env, struct message *m)
{
	struct submit_status	 ss;

	ss.id = m->id;
	ss.code = 530;
	ss.u.path = m->sender;

	if (strip_source_route(ss.u.path.user, sizeof(ss.u.path.user)))
		goto refuse;

	if (! valid_localpart(ss.u.path.user) ||
	    ! valid_domainpart(ss.u.path.domain)) {
		/*
		 * "MAIL FROM:<>" is the exception we allow.
		 */
		if (!(ss.u.path.user[0] == '\0' && ss.u.path.domain[0] == '\0'))
			goto refuse;
	}

	/* Current policy is to allow all well-formed addresses. */
	goto accept;

refuse:
	imsg_compose_event(env->sc_ievs[PROC_SMTP], IMSG_MFA_MAIL, 0, 0, -1, &ss,
	    sizeof(ss));
	return;

accept:
	ss.code = 250;
	imsg_compose_event(env->sc_ievs[PROC_LKA], IMSG_LKA_MAIL, 0,
	    0, -1, &ss, sizeof(ss));
}

void
mfa_test_rcpt(struct smtpd *env, struct message *m)
{
	struct submit_status	 ss;

	if (! valid_message_id(m->message_id))
		fatalx("mfa_test_rcpt: received corrupted message_id");

	ss.id = m->session_id;
	ss.code = 530;
	ss.u.path = m->session_rcpt;
	ss.ss = m->session_ss;
	ss.msg = *m;
	ss.msg.recipient = m->session_rcpt;
	ss.flags = m->flags;

	strip_source_route(ss.u.path.user, sizeof(ss.u.path.user));

	if (! valid_localpart(ss.u.path.user) ||
	    ! valid_domainpart(ss.u.path.domain))
		goto refuse;

	if (ss.flags & F_MESSAGE_AUTHENTICATED)
		ss.u.path.flags |= F_PATH_AUTHENTICATED;

	imsg_compose_event(env->sc_ievs[PROC_LKA], IMSG_LKA_RULEMATCH, 0, 0, -1,
	    &ss, sizeof(ss));

	return;
refuse:
	imsg_compose_event(env->sc_ievs[PROC_SMTP], IMSG_MFA_RCPT, 0, 0, -1, &ss,
	    sizeof(ss));
}

void
mfa_test_rcpt_resume(struct smtpd *env, struct submit_status *ss) {
	if (ss->code != 250) {
		imsg_compose_event(env->sc_ievs[PROC_SMTP], IMSG_MFA_RCPT, 0, 0, -1, ss,
		    sizeof(*ss));
		return;
	}

	ss->msg.recipient = ss->u.path;
	imsg_compose_event(env->sc_ievs[PROC_LKA], IMSG_LKA_RCPT, 0, 0, -1,
	    ss, sizeof(*ss));
}

int
strip_source_route(char *buf, size_t len)
{
	char *p;

	p = strchr(buf, ':');
	if (p != NULL) {
		p++;
		memmove(buf, p, strlen(p) + 1);
		return 1;
	}

	return 0;
}
