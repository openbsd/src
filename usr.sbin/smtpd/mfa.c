/*	$OpenBSD: mfa.c,v 1.59 2011/05/16 21:05:52 gilles Exp $	*/

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

#include <event.h>
#include <imsg.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "smtpd.h"
#include "log.h"

static void mfa_imsg(struct imsgev *, struct imsg *);
static void mfa_shutdown(void);
static void mfa_sig_handler(int, short, void *);
static void mfa_test_mail(struct envelope *);
static void mfa_test_rcpt(struct envelope *);
static void mfa_test_rcpt_resume(struct submit_status *);
static int mfa_strip_source_route(char *, size_t);

static void
mfa_imsg(struct imsgev *iev, struct imsg *imsg)
{
	if (iev->proc == PROC_SMTP) {
		switch (imsg->hdr.type) {
		case IMSG_MFA_MAIL:
			mfa_test_mail(imsg->data);
			return;

		case IMSG_MFA_RCPT:
			mfa_test_rcpt(imsg->data);
			return;
		}
	}

	if (iev->proc == PROC_LKA) {
		switch (imsg->hdr.type) {
		case IMSG_LKA_MAIL:
		case IMSG_LKA_RCPT:
			imsg_compose_event(env->sc_ievs[PROC_SMTP],
			    IMSG_MFA_MAIL, 0, 0, -1, imsg->data,
			    sizeof(struct submit_status));
			return;

		case IMSG_LKA_RULEMATCH:
			mfa_test_rcpt_resume(imsg->data);
			return;
		}
	}

	if (iev->proc == PROC_PARENT) {
		switch (imsg->hdr.type) {
		case IMSG_CTL_VERBOSE:
			log_verbose(*(int *)imsg->data);
			return;
		}
	}

	fatalx("mfa_imsg: unexpected imsg");
}

static void
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

static void
mfa_shutdown(void)
{
	log_info("mail filter exiting");
	_exit(0);
}


pid_t
mfa(void)
{
	pid_t		 pid;
	struct passwd	*pw;

	struct event	 ev_sigint;
	struct event	 ev_sigterm;

	struct peer peers[] = {
		{ PROC_PARENT,	imsg_dispatch },
		{ PROC_SMTP,	imsg_dispatch },
		{ PROC_LKA,	imsg_dispatch },
		{ PROC_CONTROL,	imsg_dispatch }
	};

	switch (pid = fork()) {
	case -1:
		fatal("mfa: cannot fork");
	case 0:
		break;
	default:
		return (pid);
	}

	purge_config(PURGE_EVERYTHING);

	pw = env->sc_pw;

	if (chroot(pw->pw_dir) == -1)
		fatal("mfa: chroot");
	if (chdir("/") == -1)
		fatal("mfa: chdir(\"/\")");

	smtpd_process = PROC_MFA;
	setproctitle("%s", env->sc_title[smtpd_process]);

	if (setgroups(1, &pw->pw_gid) ||
	    setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) ||
	    setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid))
		fatal("mfa: cannot drop privileges");

	imsg_callback = mfa_imsg;
	event_init();

	signal_set(&ev_sigint, SIGINT, mfa_sig_handler, NULL);
	signal_set(&ev_sigterm, SIGTERM, mfa_sig_handler, NULL);
	signal_add(&ev_sigint, NULL);
	signal_add(&ev_sigterm, NULL);
	signal(SIGPIPE, SIG_IGN);
	signal(SIGHUP, SIG_IGN);

	config_pipes(peers, nitems(peers));
	config_peers(peers, nitems(peers));

	if (event_dispatch() < 0)
		fatal("event_dispatch");
	mfa_shutdown();

	return (0);
}

void
mfa_test_mail(struct envelope *e)
{
	struct submit_status	 ss;

	ss.id = e->session_id;
	ss.code = 530;
	ss.u.maddr = e->delivery.from;

	if (mfa_strip_source_route(ss.u.maddr.user, sizeof(ss.u.maddr.user)))
		goto refuse;

	if (! valid_localpart(ss.u.maddr.user) ||
	    ! valid_domainpart(ss.u.maddr.domain)) {
		/*
		 * "MAIL FROM:<>" is the exception we allow.
		 */
		if (!(ss.u.maddr.user[0] == '\0' && ss.u.maddr.domain[0] == '\0'))
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

static void
mfa_test_rcpt(struct envelope *e)
{
	struct submit_status	 ss;

	ss.id = e->session_id;
	ss.code = 530;
	ss.u.maddr = e->delivery.rcpt_orig;
	ss.ss = e->delivery.ss;
	ss.envelope = *e;
	ss.envelope.delivery.rcpt = e->delivery.rcpt_orig;
	ss.flags = e->delivery.flags;

	mfa_strip_source_route(ss.u.maddr.user, sizeof(ss.u.maddr.user));
	
	if (! valid_localpart(ss.u.maddr.user) ||
	    ! valid_domainpart(ss.u.maddr.domain))
		goto refuse;

	imsg_compose_event(env->sc_ievs[PROC_LKA], IMSG_LKA_RULEMATCH, 0, 0, -1,
	    &ss, sizeof(ss));

	return;
refuse:
	imsg_compose_event(env->sc_ievs[PROC_SMTP], IMSG_MFA_RCPT, 0, 0, -1, &ss,
	    sizeof(ss));
}

static void
mfa_test_rcpt_resume(struct submit_status *ss) {
	if (ss->code != 250) {
		imsg_compose_event(env->sc_ievs[PROC_SMTP], IMSG_MFA_RCPT, 0, 0, -1, ss,
		    sizeof(*ss));
		return;
	}

	ss->envelope.delivery.rcpt = ss->u.maddr;
	ss->envelope.delivery.expire = ss->envelope.rule.r_qexpire;
	imsg_compose_event(env->sc_ievs[PROC_LKA], IMSG_LKA_RCPT, 0, 0, -1,
	    ss, sizeof(*ss));
}

static int
mfa_strip_source_route(char *buf, size_t len)
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
