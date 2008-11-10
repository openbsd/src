/*	$OpenBSD: store.c,v 1.4 2008/11/10 16:33:07 gilles Exp $	*/

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

int file_copy(FILE *, FILE *, size_t);
int file_append(FILE *, FILE *);

int
file_copy(FILE *dest, FILE *src, size_t len)
{
	char buffer[8192];
	size_t rlen;

	for (; len;) {

		rlen = len < sizeof(buffer) ? len : sizeof(buffer);

		if (fread(buffer, 1, rlen, src) != rlen)
			return 0;

		if (fwrite(buffer, 1, rlen, dest) != rlen)
			return 0;

		len -= rlen;
	}

	return 1;
}

int
file_append(FILE *dest, FILE *src)
{
	struct stat sb;
	size_t srcsz;

	if (fstat(fileno(src), &sb) == -1)
		return 0;
	srcsz = sb.st_size;

	if (! file_copy(dest, src, srcsz))
		return 0;

	return 1;
}

int
store_write_header(struct batch *batchp, struct message *messagep, FILE *fp)
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

		if (fprintf(fp, "From %s@%s %s\n", "MAILER-DAEMON",
			batchp->env->sc_hostname, timebuf) == -1) {
			return 0;
		}

		if (fprintf(fp, "Received: from %s (%s [%s%s])\n"
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

	if (fprintf(fp, "From %s@%s %s\n"
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
	FILE *mboxfp;
	FILE *messagefp;

	mboxfp = fdopen(messagep->mboxfd, "a");
	if (mboxfp == NULL)
		return 0;

	messagefp = fdopen(messagep->messagefd, "r");
	if (messagefp == NULL)
		goto bad;

	if (! store_write_header(batchp, messagep, mboxfp))
		goto bad;

	if (fprintf(mboxfp, "Hi !\n\n"
		"This is the MAILER-DAEMON, please DO NOT REPLY to this e-mail it is\n"
		"just a notification to let you know that an error has occured.\n\n") == -1)
		goto bad;

	if ((batchp->status & S_BATCH_PERMFAILURE) && fprintf(mboxfp,
		"You ran into a PERMANENT FAILURE, which means that the e-mail can't\n"
		"be delivered to the remote host no matter how much I'll try.\n\n"
		"Diagnostic:\n"
		"%s\n\n", batchp->errorline) == -1)
		goto bad;

	if ((batchp->status & S_BATCH_TEMPFAILURE) && fprintf(mboxfp,
		"You ran into a TEMPORARY FAILURE, which means that the e-mail can't\n"
		"be delivered right now, but could be deliverable at a later time. I\n"
		"will attempt until it succeeds for the next four days, then let you\n"
		"know if it didn't work out.\n\n"
		"Diagnostic:\n"
		"%s\n\n", batchp->errorline) == -1)
		goto bad;

	if (! (batchp->status & S_BATCH_TEMPFAILURE)) {
		/* First list the temporary failures */
		i = 0;
		TAILQ_FOREACH(recipient, &batchp->messages, entry) {
			if (recipient->status & S_MESSAGE_TEMPFAILURE) {
				if (i == 0) {
					if (fprintf(mboxfp,
						"The following recipients caused a temporary failure:\n") == -1)
						goto bad;
					++i;
				}
				if (fprintf(mboxfp,
					"\t<%s@%s>:\n%s\n\n", recipient->recipient.user, recipient->recipient.domain,
					recipient->session_errorline) == -1)
					goto bad;
			}
		}

		/* Then list the permanent failures */
		i = 0;
		TAILQ_FOREACH(recipient, &batchp->messages, entry) {
			if (recipient->status & S_MESSAGE_PERMFAILURE) {
				if (i == 0) {
					if (fprintf(mboxfp,
						"The following recipients caused a permanent failure:\n") == -1)
						goto bad;
					++i;
				}
				if (fprintf(mboxfp,
					"\t<%s@%s>:\n%s\n\n", recipient->recipient.user, recipient->recipient.domain,
					recipient->session_errorline) == -1)
					goto bad;
			}
		}
	}

	if (fprintf(mboxfp, "Below is a copy of the original message:\n\n") == -1)
		goto bad;

	if (! file_append(mboxfp, messagefp))
		goto bad;

	if (fprintf(mboxfp, "\n") == -1)
		goto bad;

	fflush(mboxfp);
	fsync(fileno(mboxfp));
	fclose(mboxfp);
	return 1;

bad:
	if (mboxfp != NULL)
		fclose(mboxfp);

	if (messagefp != NULL)
		fclose(messagefp);

	return 0;
}

int
store_write_message(struct batch *batchp, struct message *messagep)
{
	FILE *mboxfp;
	FILE *messagefp;

	mboxfp = fdopen(messagep->mboxfd, "a");
	if (mboxfp == NULL)
		return 0;

	messagefp = fdopen(messagep->messagefd, "r");
	if (messagefp == NULL)
		goto bad;

	if (! store_write_header(batchp, messagep, mboxfp))
		goto bad;

	if (! file_append(mboxfp, messagefp))
		goto bad;

	if (fprintf(mboxfp, "\n") == -1)
		goto bad;


	fflush(mboxfp);
	fsync(fileno(mboxfp));
	fclose(mboxfp);
	return 1;

bad:
	if (mboxfp != NULL)
		fclose(mboxfp);

	if (messagefp != NULL)
		fclose(messagefp);

	return 0;
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
