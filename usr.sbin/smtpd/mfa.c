/*	$OpenBSD: mfa.c,v 1.75 2013/01/26 09:37:23 gilles Exp $	*/

/*
 * Copyright (c) 2008 Gilles Chehade <gilles@poolp.org>
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

#include <err.h>
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

static void mfa_imsg(struct mproc *, struct imsg *);
static void mfa_shutdown(void);
static void mfa_sig_handler(int, short, void *);

static void
mfa_imsg(struct mproc *p, struct imsg *imsg)
{
	struct sockaddr_storage	 local, remote;
	struct mailaddr		 maddr;
	struct filter		*filter;
	struct msg		 m;
	const char		*line, *hostname;
	uint64_t		 reqid;
	int			 v;

	if (p->proc == PROC_SMTP) {
		switch (imsg->hdr.type) {
		case IMSG_MFA_REQ_CONNECT:
			m_msg(&m, imsg);
			m_get_id(&m, &reqid);
			m_get_sockaddr(&m, (struct sockaddr *)&local);
			m_get_sockaddr(&m, (struct sockaddr *)&remote);
			m_get_string(&m, &hostname);
			m_end(&m);
			mfa_filter_connect(reqid, (struct sockaddr *)&local,
			    (struct sockaddr *)&remote, hostname);
			return;

		case IMSG_MFA_REQ_HELO:
			m_msg(&m, imsg);
			m_get_id(&m, &reqid);
			m_get_string(&m, &line);
			m_end(&m);
			mfa_filter_line(reqid, HOOK_HELO, line);
			return;

		case IMSG_MFA_REQ_MAIL:
			m_msg(&m, imsg);
			m_get_id(&m, &reqid);
			m_get_mailaddr(&m, &maddr);
			m_end(&m);
			mfa_filter_mailaddr(reqid, HOOK_MAIL, &maddr);
			return;

		case IMSG_MFA_REQ_RCPT:
			m_msg(&m, imsg);
			m_get_id(&m, &reqid);
			m_get_mailaddr(&m, &maddr);
			m_end(&m);
			mfa_filter_mailaddr(reqid, HOOK_RCPT, &maddr);
			return;

		case IMSG_MFA_REQ_DATA:
			m_msg(&m, imsg);
			m_get_id(&m, &reqid);
			m_end(&m);
			mfa_filter(reqid, HOOK_DATA);
			return;

		case IMSG_MFA_REQ_EOM:
			m_msg(&m, imsg);
			m_get_id(&m, &reqid);
			m_end(&m);
			mfa_filter(reqid, HOOK_EOM);
			return;

		case IMSG_MFA_SMTP_DATA:
			m_msg(&m, imsg);
			m_get_id(&m, &reqid);
			m_get_string(&m, &line);
			m_end(&m);
			mfa_filter_data(reqid, line);
			return;

		case IMSG_MFA_EVENT_RSET:
			m_msg(&m, imsg);
			m_get_id(&m, &reqid);
			m_end(&m);
			mfa_filter_event(reqid, HOOK_RESET);
			return;

		case IMSG_MFA_EVENT_COMMIT:
			m_msg(&m, imsg);
			m_get_id(&m, &reqid);
			m_end(&m);
			mfa_filter_event(reqid, HOOK_COMMIT);
			return;

		case IMSG_MFA_EVENT_ROLLBACK:
			m_msg(&m, imsg);
			m_get_id(&m, &reqid);
			m_end(&m);
			mfa_filter_event(reqid, HOOK_ROLLBACK);
			return;

		case IMSG_MFA_EVENT_DISCONNECT:
			m_msg(&m, imsg);
			m_get_id(&m, &reqid);
			m_end(&m);
			mfa_filter_event(reqid, HOOK_DISCONNECT);
			return;
		}
	}

	if (p->proc == PROC_PARENT) {
		switch (imsg->hdr.type) {
		case IMSG_CONF_START:
			dict_init(&env->sc_filters);
			return;

		case IMSG_CONF_FILTER:
			filter = xmemdup(imsg->data, sizeof *filter,
			    "mfa_imsg");
			dict_set(&env->sc_filters, filter->name, filter);
			return;

		case IMSG_CONF_END:
			mfa_filter_init();
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

	errx(1, "mfa_imsg: unexpected %s imsg", imsg_to_str(imsg->hdr.type));
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

	do {
		pid = waitpid(WAIT_MYPGRP, NULL, 0);
	} while (pid != -1 || (pid == -1 && errno == EINTR));

	log_info("info: mail filter exiting");
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

	switch (pid = fork()) {
	case -1:
		fatal("mfa: cannot fork");
	case 0:
		env->sc_pid = getpid();
		break;
	default:
		return (pid);
	}

	purge_config(PURGE_EVERYTHING);

	if ((env->sc_pw =  getpwnam(SMTPD_FILTER_USER)) == NULL)
		if ((env->sc_pw =  getpwnam(SMTPD_USER)) == NULL)
			fatalx("unknown user " SMTPD_USER);
	pw = env->sc_pw;

	config_process(PROC_MFA);

	if (setgroups(1, &pw->pw_gid) ||
	    setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) ||
	    setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid))
		fatal("mfa: cannot drop privileges");

	imsg_callback = mfa_imsg;
	event_init();

	signal_set(&ev_sigint, SIGINT, mfa_sig_handler, NULL);
	signal_set(&ev_sigterm, SIGTERM, mfa_sig_handler, NULL);
	signal_set(&ev_sigchld, SIGCHLD, mfa_sig_handler, NULL);
	signal_add(&ev_sigint, NULL);
	signal_add(&ev_sigterm, NULL);
	signal_add(&ev_sigchld, NULL);
	signal(SIGPIPE, SIG_IGN);
	signal(SIGHUP, SIG_IGN);

	config_peer(PROC_PARENT);
	config_peer(PROC_SMTP);
	config_peer(PROC_CONTROL);
	config_done();

	mproc_disable(p_smtp);

	if (event_dispatch() < 0)
		fatal("event_dispatch");
	mfa_shutdown();

	return (0);
}

void
mfa_ready(void)
{
	log_debug("debug: mfa ready");
	mproc_enable(p_smtp);
}
