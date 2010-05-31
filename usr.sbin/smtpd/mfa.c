/*	$OpenBSD: mfa.c,v 1.46 2010/05/31 23:38:56 jacekm Exp $	*/

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

void		mfa_imsg(struct smtpd *, struct imsgev *, struct imsg *);
__dead void	mfa_shutdown(void);
void		mfa_sig_handler(int, short, void *);
void		mfa_setup_events(struct smtpd *);
void		mfa_disable_events(struct smtpd *);

void		mfa_test_mail(struct smtpd *, struct message *);
void		mfa_test_rcpt(struct smtpd *, struct message *);

int		strip_source_route(char *, size_t);

struct rule    *ruleset_match(struct smtpd *, struct path *, struct sockaddr_storage *);

void
mfa_imsg(struct smtpd *env, struct imsgev *iev, struct imsg *imsg)
{
	if (iev->proc == PROC_SMTP) {
		switch (imsg->hdr.type) {
		case IMSG_MFA_MAIL:
			mfa_test_mail(env, imsg->data);
			return;

		case IMSG_MFA_RCPT:
			mfa_test_rcpt(env, imsg->data);
			return;
		}
	}

	if (iev->proc == PROC_LKA) {
		switch (imsg->hdr.type) {
		case IMSG_LKA_MAIL:
			imsg_compose_event(env->sc_ievs[PROC_SMTP],
			    IMSG_MFA_MAIL, imsg->hdr.peerid, 0, -1, imsg->data,
			    imsg->hdr.len - sizeof imsg->hdr);
			return;

		case IMSG_LKA_RCPT:
			imsg_compose_event(env->sc_ievs[PROC_SMTP],
			    IMSG_MFA_RCPT, imsg->hdr.peerid, 0, -1, imsg->data,
			    imsg->hdr.len - sizeof imsg->hdr);
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

	purge_config(env, PURGE_EVERYTHING);

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

void
mfa_test_mail(struct smtpd *env, struct message *m)
{
	int status;

	if (strip_source_route(m->sender.user, sizeof(m->sender.user)))
		goto refuse;

	if (! valid_localpart(m->sender.user) ||
	    ! valid_domainpart(m->sender.domain)) {
		/*
		 * "MAIL FROM:<>" is the exception we allow.
		 */
		if (!(m->sender.user[0] == '\0' && m->sender.domain[0] == '\0'))
			goto refuse;
	}

	/* Current policy is to allow all well-formed addresses. */
	goto accept;

refuse:
	status = S_MESSAGE_PERMFAILURE;
	imsg_compose_event(env->sc_ievs[PROC_SMTP], IMSG_MFA_MAIL, m->id, 0, -1,
	    &status, sizeof status);
	return;

accept:
	imsg_compose_event(env->sc_ievs[PROC_LKA], IMSG_LKA_MAIL, 0,
	    0, -1, m, sizeof *m);
}

void
mfa_test_rcpt(struct smtpd *env, struct message *m)
{
	int status;

	m->recipient = m->session_rcpt;

	strip_source_route(m->recipient.user, sizeof(m->recipient.user));

	if (! valid_localpart(m->recipient.user) ||
	    ! valid_domainpart(m->recipient.domain))
		goto refuse;

	if (m->flags & F_MESSAGE_AUTHENTICATED)
		m->recipient.flags |= F_PATH_AUTHENTICATED;

	imsg_compose_event(env->sc_ievs[PROC_LKA], IMSG_LKA_RCPT, 0, 0, -1,
	    m, sizeof *m);
	return;
refuse:
	status = S_MESSAGE_PERMFAILURE;
	imsg_compose_event(env->sc_ievs[PROC_SMTP], IMSG_MFA_RCPT, m->id, 0, -1,
	    &status, sizeof status);
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
