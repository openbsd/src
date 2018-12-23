/*	$OpenBSD: pony.c,v 1.26 2018/12/23 16:37:53 eric Exp $	*/

/*
 * Copyright (c) 2014 Gilles Chehade <gilles@poolp.org>
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
#include <sys/socket.h>

#include <ctype.h>
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
#include <time.h>
#include <unistd.h>
#include <limits.h>
#include <grp.h>

#include "smtpd.h"
#include "log.h"

void mda_imsg(struct mproc *, struct imsg *);
void mta_imsg(struct mproc *, struct imsg *);
void smtp_imsg(struct mproc *, struct imsg *);

static void pony_shutdown(void);

void
pony_imsg(struct mproc *p, struct imsg *imsg)
{
	struct msg	m;
	int		v;

	if (imsg == NULL)
		pony_shutdown();

	switch (imsg->hdr.type) {

	case IMSG_GETADDRINFO:
	case IMSG_GETADDRINFO_END:
	case IMSG_GETNAMEINFO:
		resolver_dispatch_result(p, imsg);
		return;

	case IMSG_CERT_INIT:
	case IMSG_CERT_VERIFY:
		cert_dispatch_result(p, imsg);
		return;

	case IMSG_CONF_START:
		return;
	case IMSG_CONF_END:
		smtp_configure();
		return;
	case IMSG_CTL_VERBOSE:
		m_msg(&m, imsg);
		m_get_int(&m, &v);
		m_end(&m);
		log_trace_verbose(v);
		return;
	case IMSG_CTL_PROFILE:
		m_msg(&m, imsg);
		m_get_int(&m, &v);
		m_end(&m);
		profiling = v;
		return;

	/* smtp imsg */
	case IMSG_SMTP_CHECK_SENDER:
	case IMSG_SMTP_EXPAND_RCPT:
	case IMSG_SMTP_LOOKUP_HELO:
	case IMSG_SMTP_AUTHENTICATE:
	case IMSG_SMTP_MESSAGE_COMMIT:
	case IMSG_SMTP_MESSAGE_CREATE:
	case IMSG_SMTP_MESSAGE_OPEN:
	case IMSG_FILTER_SMTP_PROTOCOL:
	case IMSG_FILTER_SMTP_DATA_BEGIN:
	case IMSG_QUEUE_ENVELOPE_SUBMIT:
	case IMSG_QUEUE_ENVELOPE_COMMIT:
	case IMSG_QUEUE_SMTP_SESSION:
	case IMSG_CTL_SMTP_SESSION:
	case IMSG_CTL_PAUSE_SMTP:
	case IMSG_CTL_RESUME_SMTP:
		smtp_imsg(p, imsg);
		return;

        /* mta imsg */
	case IMSG_QUEUE_TRANSFER:
	case IMSG_MTA_OPEN_MESSAGE:
	case IMSG_MTA_LOOKUP_CREDENTIALS:
	case IMSG_MTA_LOOKUP_SMARTHOST:
	case IMSG_MTA_LOOKUP_SOURCE:
	case IMSG_MTA_LOOKUP_HELO:
	case IMSG_MTA_DNS_HOST:
	case IMSG_MTA_DNS_HOST_END:
	case IMSG_MTA_DNS_MX_PREFERENCE:
	case IMSG_CTL_RESUME_ROUTE:
	case IMSG_CTL_MTA_SHOW_HOSTS:
	case IMSG_CTL_MTA_SHOW_RELAYS:
	case IMSG_CTL_MTA_SHOW_ROUTES:
	case IMSG_CTL_MTA_SHOW_HOSTSTATS:
	case IMSG_CTL_MTA_BLOCK:
	case IMSG_CTL_MTA_UNBLOCK:
	case IMSG_CTL_MTA_SHOW_BLOCK:
		mta_imsg(p, imsg);
		return;

        /* mda imsg */
	case IMSG_MDA_LOOKUP_USERINFO:
	case IMSG_QUEUE_DELIVER:
	case IMSG_MDA_OPEN_MESSAGE:
	case IMSG_MDA_FORK:
	case IMSG_MDA_DONE:
		mda_imsg(p, imsg);
		return;
	default:
		break;
	}

	errx(1, "session_imsg: unexpected %s imsg", imsg_to_str(imsg->hdr.type));
}

static void
pony_shutdown(void)
{
	log_debug("debug: pony agent exiting");
	_exit(0);
}

int
pony(void)
{
	struct passwd	*pw;

	mda_postfork();
	mta_postfork();
	smtp_postfork();

	/* do not purge listeners and pki, they are purged
	 * in smtp_configure()
	 */
	purge_config(PURGE_TABLES|PURGE_RULES);

	if ((pw = getpwnam(SMTPD_USER)) == NULL)
		fatalx("unknown user " SMTPD_USER);

	if (chroot(PATH_CHROOT) == -1)
		fatal("pony: chroot");
	if (chdir("/") == -1)
		fatal("pony: chdir(\"/\")");

	config_process(PROC_PONY);

	if (setgroups(1, &pw->pw_gid) ||
	    setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) ||
	    setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid))
		fatal("pony: cannot drop privileges");

	imsg_callback = pony_imsg;
	event_init();

	mda_postprivdrop();
	mta_postprivdrop();
	smtp_postprivdrop();

	signal(SIGINT, SIG_IGN);
	signal(SIGTERM, SIG_IGN);
	signal(SIGPIPE, SIG_IGN);
	signal(SIGHUP, SIG_IGN);

	config_peer(PROC_PARENT);
	config_peer(PROC_QUEUE);
	config_peer(PROC_LKA);
	config_peer(PROC_CONTROL);
	config_peer(PROC_CA);

	ca_engine_init();

	if (pledge("stdio inet unix recvfd sendfd", NULL) == -1)
		err(1, "pledge");

	event_dispatch();
	fatalx("exited event loop");

	return (0);
}
