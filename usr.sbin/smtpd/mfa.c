/*	$OpenBSD: mfa.c,v 1.64 2011/10/23 09:30:07 gilles Exp $	*/

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
#include <sys/wait.h>
#include <sys/queue.h>
#include <sys/tree.h>
#include <sys/param.h>
#include <sys/socket.h>

#include <errno.h>
#include <event.h>
#include <imsg.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "smtpd.h"
#include "log.h"

static void mfa_imsg(struct imsgev *, struct imsg *);
static void mfa_shutdown(void);
static void mfa_sig_handler(int, short, void *);
static void mfa_test_helo(struct envelope *);
static void mfa_test_mail(struct envelope *);
static void mfa_test_rcpt(struct envelope *);
static void mfa_test_rcpt_resume(struct submit_status *);
static void mfa_test_dataline(struct submit_status *);
static int mfa_strip_source_route(char *, size_t);
static int mfa_fork_filter(struct filter *);
void mfa_session(struct submit_status *, enum session_state);

static void
mfa_imsg(struct imsgev *iev, struct imsg *imsg)
{
	struct filter *filter;

	log_imsg(PROC_MFA, iev->proc, imsg);

	if (iev->proc == PROC_SMTP) {
		switch (imsg->hdr.type) {
		case IMSG_MFA_HELO:
			mfa_test_helo(imsg->data);
			return;
		case IMSG_MFA_MAIL:
			mfa_test_mail(imsg->data);
			return;
		case IMSG_MFA_RCPT:
			mfa_test_rcpt(imsg->data);
			return;
		case IMSG_MFA_DATALINE:
			mfa_test_dataline(imsg->data);
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
		case IMSG_CONF_START:
			env->sc_filters = calloc(1, sizeof *env->sc_filters);
			if (env->sc_filters == NULL)
				fatal(NULL);
			TAILQ_INIT(env->sc_filters);
			return;

		case IMSG_CONF_FILTER:
			filter = calloc(1, sizeof *filter);
			if (filter == NULL)
				fatal(NULL);
			memcpy(filter, (struct filter *)imsg->data, sizeof (*filter));
			TAILQ_INSERT_TAIL(env->sc_filters, filter, f_entry);
			return;

		case IMSG_CONF_END:
			TAILQ_FOREACH(filter, env->sc_filters, f_entry) {
				log_info("forking filter: %s", filter->name);
				if (! mfa_fork_filter(filter))
					fatalx("could not fork filter");
			}
			return;

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

	case SIGCHLD:
		fatalx("unexpected SIGCHLD");
		break;

	default:
		fatalx("mfa_sig_handler: unexpected signal");
	}
}

static void
mfa_shutdown(void)
{
	pid_t pid;
	struct filter *filter;

	TAILQ_FOREACH(filter, env->sc_filters, f_entry) {
		kill(filter->pid, SIGTERM);
	}

	do {
		pid = waitpid(WAIT_MYPGRP, NULL, 0);
	} while (pid != -1 || (pid == -1 && errno == EINTR));

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
	struct event	 ev_sigchld;

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

	if ((env->sc_pw =  getpwnam(SMTPD_FILTER_USER)) == NULL)
		if ((env->sc_pw =  getpwnam(SMTPD_USER)) == NULL)
			fatalx("unknown user " SMTPD_FILTER_USER);
	pw = env->sc_pw;

	smtpd_process = PROC_MFA;
	setproctitle("%s", env->sc_title[smtpd_process]);

	if (setgroups(1, &pw->pw_gid) ||
	    setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) ||
	    setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid))
		fatal("mfa: cannot drop privileges");

	imsg_callback = mfa_imsg;
	event_init();

	SPLAY_INIT(&env->mfa_sessions);
	TAILQ_INIT(env->sc_filters);

	signal_set(&ev_sigint, SIGINT, mfa_sig_handler, NULL);
	signal_set(&ev_sigterm, SIGTERM, mfa_sig_handler, NULL);
	signal_set(&ev_sigchld, SIGCHLD, mfa_sig_handler, NULL);
	signal_add(&ev_sigint, NULL);
	signal_add(&ev_sigterm, NULL);
	signal_add(&ev_sigchld, NULL);
	signal(SIGPIPE, SIG_IGN);
	signal(SIGHUP, SIG_IGN);

	config_pipes(peers, nitems(peers));
	config_peers(peers, nitems(peers));

	if (event_dispatch() < 0)
		fatal("event_dispatch");
	mfa_shutdown();

	return (0);
}

static void
mfa_test_helo(struct envelope *e)
{
	struct submit_status	 ss;

	ss.id = e->session_id;
	ss.code = 530;
	ss.envelope = *e;

	mfa_session(&ss, S_HELO);
	return;
}

static void
mfa_test_mail(struct envelope *e)
{
	struct submit_status	 ss;

	ss.id = e->session_id;
	ss.code = 530;
	ss.u.maddr = e->sender;

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

	mfa_session(&ss, S_MAIL_MFA);
	return;

refuse:
	imsg_compose_event(env->sc_ievs[PROC_SMTP], IMSG_MFA_MAIL, 0, 0, -1, &ss,
	    sizeof(ss));
	return;
}

static void
mfa_test_rcpt(struct envelope *e)
{
	struct submit_status	 ss;

	ss.id = e->session_id;
	ss.code = 530;
	ss.u.maddr = e->rcpt;
	ss.ss = e->ss;
	ss.envelope = *e;
	ss.envelope.dest = e->rcpt;
	ss.flags = e->flags;

	mfa_strip_source_route(ss.u.maddr.user, sizeof(ss.u.maddr.user));
	
	if (! valid_localpart(ss.u.maddr.user) ||
	    ! valid_domainpart(ss.u.maddr.domain))
		goto refuse;

	mfa_session(&ss, S_RCPT_MFA);
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

	ss->envelope.dest = ss->u.maddr;
	ss->envelope.expire = ss->envelope.rule.r_qexpire;
	imsg_compose_event(env->sc_ievs[PROC_LKA], IMSG_LKA_RCPT, 0, 0, -1,
	    ss, sizeof(*ss));
}

static void
mfa_test_dataline(struct submit_status *ss)
{
	ss->code = 250;

	mfa_session(ss, S_DATACONTENT);
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

static int
mfa_fork_filter(struct filter *filter)
{
	pid_t	pid;
	int	sockpair[2];

	if (socketpair(AF_UNIX, SOCK_STREAM, PF_UNSPEC, sockpair) < 0)
		return 0;

	session_socket_blockmode(sockpair[0], BM_NONBLOCK);
	session_socket_blockmode(sockpair[1], BM_NONBLOCK);

	filter->ibuf = calloc(1, sizeof(struct imsgbuf));
	if (filter->ibuf == NULL)
		goto err;

	pid = fork();
	if (pid == -1)
		goto err;

	if (pid == 0) {
		/* filter */
		dup2(sockpair[0], 0);
		
		if (closefrom(STDERR_FILENO + 1) < 0)
			exit(1);

		execl(filter->path, filter->name, NULL);
		exit(1);
	}

	/* in parent */
	close(sockpair[0]);
	imsg_init(filter->ibuf, sockpair[1]);

	return 1;

err:
	free(filter->ibuf);
	close(sockpair[0]);
	close(sockpair[1]);
	return 0;
}
