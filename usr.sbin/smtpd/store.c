/*	$OpenBSD: store.c,v 1.19 2009/06/05 21:55:40 jacekm Exp $	*/

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
#include <sys/socket.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <err.h>
#include <errno.h>
#include <event.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "smtpd.h"

int file_copy(FILE *, FILE *, enum action_type);

int
file_copy(FILE *dest, FILE *src, enum action_type type)
{
	char *buf, *lbuf;
	size_t len;
	char *escape;
	
	lbuf = NULL;
	while ((buf = fgetln(src, &len))) {
		if (buf[len - 1] == '\n') {
			buf[len - 1] = '\0';
			len--;
		}
		else {
			/* EOF without EOL, copy and add the NUL */
			if ((lbuf = malloc(len + 1)) == NULL)
				err(1, NULL);
			memcpy(lbuf, buf, len);
			lbuf[len] = '\0';
			buf = lbuf;
		}

		if (type == A_MBOX) {
			escape = buf;
			while (*escape == '>')
				++escape;
			if (strncmp("From ", escape, 5) == 0) {
				if (fprintf(dest, ">") != 1)
					return 0;
			}
		}

		if (fprintf(dest, "%s\n", buf) != (int)len + 1)
			return 0;
	}
	free(lbuf);

	if (type == A_MBOX) {
		if (fprintf(dest, "\n") != 1)
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

	mboxfp = fdopen(batchp->sessionp->mboxfd, "a");
	if (mboxfp == NULL)
		return 0;

	messagefp = fdopen(batchp->sessionp->messagefd, "r");
	if (messagefp == NULL)
		goto bad;

	if (fprintf(mboxfp, "Hi !\n\n"
		"This is the MAILER-DAEMON, please DO NOT REPLY to this e-mail it is\n"
		"just a notification to let you know that an error has occurred.\n\n") == -1)
		goto bad;

	if ((batchp->status & S_BATCH_PERMFAILURE) && fprintf(mboxfp,
		"You ran into a PERMANENT FAILURE, which means that the e-mail can't\n"
		"be delivered to the remote host no matter how much I try.\n\n"
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

	if (! file_copy(mboxfp, messagefp, messagep->recipient.rule.r_action))
		goto bad;

	fflush(mboxfp);
	fsync(fileno(mboxfp));
	fclose(mboxfp);
	fclose(messagefp);
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

	mboxfp = fdopen(batchp->sessionp->mboxfd, "a");
	if (mboxfp == NULL)
		return 0;

	messagefp = fdopen(batchp->sessionp->messagefd, "r");
	if (messagefp == NULL)
		goto bad;

	if (! file_copy(mboxfp, messagefp, messagep->recipient.rule.r_action))
		goto bad;

	fflush(mboxfp);
	fsync(fileno(mboxfp));
	fclose(mboxfp);
	fclose(messagefp);
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

	if (fstat(batchp->sessionp->mboxfd, &sb) == -1)
		return 0;

	if (! writer(batchp, messagep)) {
		if (S_ISREG(sb.st_mode)) {
			ftruncate(batchp->sessionp->mboxfd, sb.st_size);
			return 0;
		}
		return 0;
	}
	return 1;
}
