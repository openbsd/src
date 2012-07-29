/*	$OpenBSD: mta.c,v 1.133 2012/07/29 13:56:24 eric Exp $	*/

/*
 * Copyright (c) 2008 Pierre-Yves Ritschard <pyr@openbsd.org>
 * Copyright (c) 2008 Gilles Chehade <gilles@openbsd.org>
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
#include <sys/param.h>
#include <sys/socket.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <event.h>
#include <imsg.h>
#include <inttypes.h>
#include <netdb.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "smtpd.h"
#include "log.h"

static void mta_imsg(struct imsgev *, struct imsg *);
static void mta_shutdown(void);
static void mta_sig_handler(int, short, void *);

static void
mta_imsg(struct imsgev *iev, struct imsg *imsg)
{
	struct ssl		 *ssl;

	log_imsg(PROC_MTA, iev->proc, imsg);

	if (iev->proc == PROC_QUEUE) {
		switch (imsg->hdr.type) {
		case IMSG_BATCH_CREATE:
		case IMSG_BATCH_APPEND:
		case IMSG_BATCH_CLOSE:
		case IMSG_QUEUE_MESSAGE_FD:
			mta_session_imsg(iev, imsg);
			return;
		}
	}

	if (iev->proc == PROC_LKA) {
		switch (imsg->hdr.type) {
		case IMSG_LKA_SECRET:
		case IMSG_DNS_HOST:
		case IMSG_DNS_HOST_END:
		case IMSG_DNS_PTR:
			mta_session_imsg(iev, imsg);
			return;
		}
	}

	if (iev->proc == PROC_PARENT) {
		switch (imsg->hdr.type) {
		case IMSG_CONF_START:
			if (env->sc_flags & SMTPD_CONFIGURING)
				return;
			env->sc_flags |= SMTPD_CONFIGURING;
			env->sc_ssl = calloc(1, sizeof *env->sc_ssl);
			if (env->sc_ssl == NULL)
				fatal(NULL);
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
			ssl->ssl_key = strdup((char *)imsg->data +
			    sizeof *ssl + ssl->ssl_cert_len);
			if (ssl->ssl_key == NULL)
				fatal(NULL);
			SPLAY_INSERT(ssltree, env->sc_ssl, ssl);
			return;

		case IMSG_CONF_END:
			if (!(env->sc_flags & SMTPD_CONFIGURING))
				return;
			env->sc_flags &= ~SMTPD_CONFIGURING;
			return;

		case IMSG_CTL_VERBOSE:
			log_verbose(*(int *)imsg->data);
			return;
		}
	}

	errx(1, "mta_imsg: unexpected %s imsg", imsg_to_str(imsg->hdr.type));
}

static void
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

static void
mta_shutdown(void)
{
	log_info("mail transfer agent exiting");
	_exit(0);
}

pid_t
mta(void)
{
	pid_t		 pid;

	struct passwd	*pw;
	struct event	 ev_sigint;
	struct event	 ev_sigterm;

	struct peer peers[] = {
		{ PROC_PARENT,	imsg_dispatch },
		{ PROC_QUEUE,	imsg_dispatch },
		{ PROC_LKA,	imsg_dispatch }
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
	purge_config(PURGE_EVERYTHING);

	pw = env->sc_pw;
	if (chroot(pw->pw_dir) == -1)
		fatal("mta: chroot");
	if (chdir("/") == -1)
		fatal("mta: chdir(\"/\")");

	smtpd_process = PROC_MTA;
	setproctitle("%s", env->sc_title[smtpd_process]);

	if (setgroups(1, &pw->pw_gid) ||
	    setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) ||
	    setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid))
		fatal("mta: cannot drop privileges");

	imsg_callback = mta_imsg;
	event_init();

	signal_set(&ev_sigint, SIGINT, mta_sig_handler, NULL);
	signal_set(&ev_sigterm, SIGTERM, mta_sig_handler, NULL);
	signal_add(&ev_sigint, NULL);
	signal_add(&ev_sigterm, NULL);
	signal(SIGPIPE, SIG_IGN);
	signal(SIGHUP, SIG_IGN);

	config_pipes(peers, nitems(peers));
	config_peers(peers, nitems(peers));

	if (event_dispatch() < 0)
		fatal("event_dispatch");
	mta_shutdown();

	return (0);
}
