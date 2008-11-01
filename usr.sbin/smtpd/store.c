/*
 * Copyright (c) 2008 Gilles Chehade <gilles@openbsd.org>
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
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/queue.h>
#include <sys/tree.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <event.h>
#include <fcntl.h>
#include <paths.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <util.h>
#include <unistd.h>

#include "smtpd.h"

int fd_copy(int, int, size_t);
int fd_append(int, int);

int
fd_copy(int dest, int src, size_t len)
{
	char buffer[8192];
	size_t rlen;

	for (; len;) {

		rlen = len < sizeof(buffer) ? len : sizeof(buffer);
		if (atomic_read(src, buffer, rlen) == -1)
			return 0;

		if (atomic_write(dest, buffer, rlen) == -1)
			return 0;

		len -= rlen;
	}

	return 1;
}

int
fd_append(int dest, int src)
{
	struct stat sb;
	size_t srcsz;

	if (fstat(src, &sb) == -1)
		return 0;
	srcsz = sb.st_size;

	if (! fd_copy(dest, src, srcsz))
		return 0;

	return 1;
}

int
store_write_header(struct batch *batchp, struct message *messagep)
{
	time_t tm;
	char timebuf[26];	/* current time	 */
	char ctimebuf[26];	/* creation time */
	void *p;
	char addrbuf[INET6_ADDRSTRLEN];

	tm = time(NULL);
	ctime_r(&tm, timebuf);
	timebuf[strcspn(timebuf, "\n")] = '\0';

	tm = time(&messagep->creation);
	ctime_r(&tm, ctimebuf);
	ctimebuf[strcspn(ctimebuf, "\n")] = '\0';

	if (messagep->session_ss.ss_family == PF_INET) {
		struct sockaddr_in *ssin = (struct sockaddr_in *)&messagep->session_ss;
		p = &ssin->sin_addr.s_addr;
	}
	if (messagep->session_ss.ss_family == PF_INET6) {
		struct sockaddr_in6 *ssin6 = (struct sockaddr_in6 *)&messagep->session_ss;
		p = &ssin6->sin6_addr.s6_addr;
	}

	bzero(addrbuf, sizeof (addrbuf));
	inet_ntop(messagep->session_ss.ss_family, p, addrbuf, sizeof (addrbuf));

	if (batchp->type & T_DAEMON_BATCH) {
		if (atomic_printfd(messagep->mboxfd, "From %s@%s %s\n",
			"MAILER-DAEMON", batchp->env->sc_hostname, timebuf) == -1)
			return 0;
		if (atomic_printfd(messagep->mboxfd,
			"Received: from %s (%s [%s%s])\n"
			"\tby %s with ESMTP id %s\n"
			"\tfor <%s@%s>; %s\n\n",
			messagep->session_helo, messagep->session_hostname,
			messagep->session_ss.ss_family == PF_INET ? "" : "IPv6:", addrbuf,
			batchp->env->sc_hostname, messagep->message_id,
			messagep->sender.user, messagep->sender.domain, ctimebuf) == -1) {
			return 0;
		}
		return 1;
	}

	if (atomic_printfd(messagep->mboxfd,
		"From %s@%s %s\n"
		"Received: from %s (%s [%s%s])\n"
		"\tby %s with ESMTP id %s\n"
		"\tfor <%s@%s>; %s\n",
		messagep->sender.user, messagep->sender.domain, timebuf,
		messagep->session_helo, messagep->session_hostname,
		messagep->session_ss.ss_family == PF_INET ? "" : "IPv6:", addrbuf,
		batchp->env->sc_hostname, batchp->message_id,
		messagep->recipient.user, messagep->recipient.domain, ctimebuf) == -1) {
		return 0;
	}

	return 1;
}

int
store_write_daemon(struct batch *batchp, struct message *messagep)
{
	u_int32_t i;
	struct message *recipient;

	if (! store_write_header(batchp, messagep))
		return 0;

	if (atomic_printfd(messagep->mboxfd,
		"Hi !\n\n"
		"This is the MAILER-DAEMON, please DO NOT REPLY to this e-mail it is\n"
		"just a notification to let you know that an error has occured.\n\n") == -1) {
		return 0;
	}

	if ((batchp->status & S_BATCH_PERMFAILURE) && atomic_printfd(messagep->mboxfd,
		"You ran into a PERMANENT FAILURE, which means that the e-mail can't\n"
		"be delivered to the remote host no matter how much I'll try.\n\n"
		"Diagnostic:\n"
		"%s\n\n", batchp->errorline) == -1) {
		return 0;
	}

	if ((batchp->status & S_BATCH_TEMPFAILURE) && atomic_printfd(messagep->mboxfd,
		"You ran into a TEMPORARY FAILURE, which means that the e-mail can't\n"
		"be delivered right now, but could be deliverable at a later time. I\n"
		"will attempt until it succeeds for the next four days, then let you\n"
		"know if it didn't work out.\n\n"
		"Diagnostic:\n"
		"%s\n\n", batchp->errorline) == -1) {
		return 0;
	}

	if (! (batchp->status & S_BATCH_TEMPFAILURE)) {
		/* First list the temporary failures */
		i = 0;
		TAILQ_FOREACH(recipient, &batchp->messages, entry) {
			if (recipient->status & S_MESSAGE_TEMPFAILURE) {
				if (i == 0) {
					if (atomic_printfd(messagep->mboxfd,
						"The following recipients caused a temporary failure:\n") == -1)
						return 0;
					++i;
				}
				if (atomic_printfd(messagep->mboxfd,
					"\t<%s@%s>:\n%s\n\n", recipient->recipient.user, recipient->recipient.domain,
					recipient->session_errorline) == -1) {
					return 0;
				}
			}
		}

		/* Then list the permanent failures */
		i = 0;
		TAILQ_FOREACH(recipient, &batchp->messages, entry) {
			if (recipient->status & S_MESSAGE_PERMFAILURE) {
				if (i == 0) {
					if (atomic_printfd(messagep->mboxfd,
						"The following recipients caused a permanent failure:\n") == -1)
						return 0;
					++i;
				}
				if (atomic_printfd(messagep->mboxfd,
					"\t<%s@%s>:\n%s\n\n", recipient->recipient.user, recipient->recipient.domain,
					recipient->session_errorline) == -1) {
					return 0;
				}
			}
		}
	}

	if (atomic_printfd(messagep->mboxfd,
		"Below is a copy of the original message:\n\n") == -1)
		return 0;

	if (! fd_append(messagep->mboxfd, messagep->messagefd))
		return 0;

	if (atomic_printfd(messagep->mboxfd, "\n") == -1)
		return 0;

	return 1;
}

int
store_write_message(struct batch *batchp, struct message *messagep)
{
	if (! store_write_header(batchp, messagep))
		return 0;

	if (! fd_append(messagep->mboxfd, messagep->messagefd))
		return 0;

	if (atomic_printfd(messagep->mboxfd, "\n") == -1)
		return 0;

	return 1;
}

int
store_message(struct batch *batchp, struct message *messagep,
    int (*writer)(struct batch *, struct message *))
{
	struct stat sb;

	if (fstat(messagep->mboxfd, &sb) == -1)
		return 0;

	if (! writer(batchp, messagep)) {
		if (S_ISREG(sb.st_mode)) {
			ftruncate(messagep->mboxfd, sb.st_size);
			return 0;
		}
		return 0;
	}

	return 1;
}
