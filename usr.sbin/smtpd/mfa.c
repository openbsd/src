/*	$OpenBSD: mfa.c,v 1.82 2014/04/04 16:10:42 eric Exp $	*/

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
#include <sys/socket.h>

#include <err.h>
#include <errno.h>
#include <event.h>
#include <imsg.h>
#include <inttypes.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "smtpd.h"
#include "log.h"

struct mfa_tx {
	uint64_t	 reqid;
	struct io	 io;
	struct iobuf	 iobuf;
	FILE		*ofile;
	size_t		 datain;
	size_t		 datalen;
	int		 eom;
	int		 error;
};

static void mfa_imsg(struct mproc *, struct imsg *);
static void mfa_shutdown(void);
static void mfa_sig_handler(int, short, void *);
static void mfa_tx_io(struct io *, int);
static int mfa_tx(uint64_t, int);
static void mfa_tx_done(struct mfa_tx *);

struct tree tx_tree;

static void
mfa_imsg(struct mproc *p, struct imsg *imsg)
{
	struct sockaddr_storage	 local, remote;
	struct mailaddr		 maddr;
	struct msg		 m;
	const char		*line, *hostname;
	uint64_t		 reqid;
	uint32_t		 datalen; /* XXX make it off_t? */
	int			 v, success, fdout;

	if (p->proc == PROC_PONY) {
		switch (imsg->hdr.type) {
		case IMSG_SMTP_REQ_CONNECT:
			m_msg(&m, imsg);
			m_get_id(&m, &reqid);
			m_get_sockaddr(&m, (struct sockaddr *)&local);
			m_get_sockaddr(&m, (struct sockaddr *)&remote);
			m_get_string(&m, &hostname);
			m_end(&m);
			mfa_filter_connect(reqid, (struct sockaddr *)&local,
			    (struct sockaddr *)&remote, hostname);
			return;

		case IMSG_SMTP_REQ_HELO:
			m_msg(&m, imsg);
			m_get_id(&m, &reqid);
			m_get_string(&m, &line);
			m_end(&m);
			mfa_filter_line(reqid, HOOK_HELO, line);
			return;

		case IMSG_SMTP_REQ_MAIL:
			m_msg(&m, imsg);
			m_get_id(&m, &reqid);
			m_get_mailaddr(&m, &maddr);
			m_end(&m);
			mfa_filter_mailaddr(reqid, HOOK_MAIL, &maddr);
			return;

		case IMSG_SMTP_REQ_RCPT:
			m_msg(&m, imsg);
			m_get_id(&m, &reqid);
			m_get_mailaddr(&m, &maddr);
			m_end(&m);
			mfa_filter_mailaddr(reqid, HOOK_RCPT, &maddr);
			return;

		case IMSG_SMTP_REQ_DATA:
			m_msg(&m, imsg);
			m_get_id(&m, &reqid);
			m_end(&m);
			mfa_filter(reqid, HOOK_DATA);
			return;

		case IMSG_SMTP_REQ_EOM:
			m_msg(&m, imsg);
			m_get_id(&m, &reqid);
			m_get_u32(&m, &datalen);
			m_end(&m);
			mfa_filter_eom(reqid, HOOK_EOM, datalen);
			return;

		case IMSG_SMTP_EVENT_RSET:
			m_msg(&m, imsg);
			m_get_id(&m, &reqid);
			m_end(&m);
			mfa_filter_event(reqid, HOOK_RESET);
			return;

		case IMSG_SMTP_EVENT_COMMIT:
			m_msg(&m, imsg);
			m_get_id(&m, &reqid);
			m_end(&m);
			mfa_filter_event(reqid, HOOK_COMMIT);
			return;

		case IMSG_SMTP_EVENT_ROLLBACK:
			m_msg(&m, imsg);
			m_get_id(&m, &reqid);
			m_end(&m);
			mfa_filter_event(reqid, HOOK_ROLLBACK);
			return;

		case IMSG_SMTP_EVENT_DISCONNECT:
			m_msg(&m, imsg);
			m_get_id(&m, &reqid);
			m_end(&m);
			mfa_filter_event(reqid, HOOK_DISCONNECT);
			return;
		}
	}

	if (p->proc == PROC_QUEUE) {
		switch (imsg->hdr.type) {
		case IMSG_SMTP_MESSAGE_OPEN: /* XXX bogus */
			m_msg(&m, imsg);
			m_get_id(&m, &reqid);
			m_get_int(&m, &success);
			m_end(&m);

			fdout = mfa_tx(reqid, imsg->fd);
			mfa_build_fd_chain(reqid, fdout);
			return;
		}
	}

	if (p->proc == PROC_PARENT) {
		switch (imsg->hdr.type) {
		case IMSG_CONF_START:
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
		fatal("filter: cannot fork");
	case 0:
		post_fork(PROC_MFA);
		break;
	default:
		return (pid);
	}

	mfa_filter_prepare();

	purge_config(PURGE_EVERYTHING);

	if ((pw =  getpwnam(SMTPD_USER)) == NULL)
		fatalx("unknown user " SMTPD_USER);

	config_process(PROC_MFA);

	if (chroot(PATH_CHROOT) == -1)
		fatal("scheduler: chroot");
	if (chdir("/") == -1)
		fatal("scheduler: chdir(\"/\")");

	if (setgroups(1, &pw->pw_gid) ||
	    setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) ||
	    setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid))
		fatal("filter: cannot drop privileges");

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
	config_peer(PROC_CONTROL);
	config_peer(PROC_PONY);
	config_done();

	mproc_disable(p_pony);

	if (event_dispatch() < 0)
		fatal("event_dispatch");
	mfa_shutdown();

	return (0);
}

void
mfa_ready(void)
{
	log_debug("debug: mfa ready");
	mproc_enable(p_pony);
}

static int
mfa_tx(uint64_t reqid, int fdout)
{
	struct mfa_tx	*tx;
	int		 sp[2];

	if (socketpair(AF_UNIX, SOCK_STREAM, PF_UNSPEC, sp) == -1) {
		log_warn("warn: mfa: socketpair");
		return (-1);
	}

	tx = xcalloc(1, sizeof(*tx), "mfa_tx");

	if ((tx->ofile = fdopen(fdout, "w")) == NULL) {
		log_warn("warn: mfa: fdopen");
		free(tx);
		close(sp[0]);
		close(sp[1]);
		return (-1);
	}

	iobuf_init(&tx->iobuf, 0, 0);
	io_init(&tx->io, sp[0], tx, mfa_tx_io, &tx->iobuf);
	io_set_read(&tx->io);
	tx->reqid = reqid;
	tree_xset(&tx_tree, reqid, tx);

	return (sp[1]);
}

static void
mfa_tx_io(struct io *io, int evt)
{
	struct mfa_tx	*tx = io->arg;
	size_t		 len, n;
	char		*data;

	switch (evt) {
	case IO_DATAIN:
		data = iobuf_data(&tx->iobuf);
		len = iobuf_len(&tx->iobuf);
		log_debug("debug: mfa: tx data (%zu) for req %016"PRIx64,
		    len, tx->reqid);
		n = fwrite(data, 1, len, tx->ofile);
		if (n != len) {
			tx->error = 1;
			break;
		}
		tx->datain += n;
		iobuf_drop(&tx->iobuf, n);
		iobuf_normalize(&tx->iobuf);
		return;

	case IO_DISCONNECTED:
		log_debug("debug: mfa: tx done for req %016"PRIx64,
		    tx->reqid);
		break;

	default:
		log_debug("debug: mfa: tx error for req %016"PRIx64,
		    tx->reqid);
		tx->error = 1;
		break;
	}

	io_clear(&tx->io);
	iobuf_clear(&tx->iobuf);
	fclose(tx->ofile);
	tx->ofile = NULL;
	if (tx->eom)
		mfa_tx_done(tx);
}

static void
mfa_tx_done(struct mfa_tx *tx)
{
	log_debug("debug: mfa: tx done for %016"PRIx64, tx->reqid);

	if (!tx->error && tx->datain != tx->datalen) {
		log_debug("debug: mfa: tx datalen mismatch: %zu/%zu",
		    tx->datain, tx->datalen);
		tx->error = 1;
	}

	if (tx->error) {
		log_debug("debug: mfa: tx error");

		m_create(p_pony, IMSG_MFA_SMTP_RESPONSE, 0, 0, -1);
		m_add_id(p_pony, tx->reqid);
		m_add_int(p_pony, MFA_FAIL);
		m_add_u32(p_pony, 0);
		m_add_string(p_pony, "Internal server error");
		m_close(p_pony);
	}
#if 0
	else
		mfa_filter(tx->reqid, HOOK_EOM);
#endif
	free(tx);
}
