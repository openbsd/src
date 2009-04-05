/*	$OpenBSD: enqueue.c,v 1.11 2009/04/05 16:10:42 gilles Exp $	*/

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
#include <sys/queue.h>
#include <sys/tree.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <event.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "smtpd.h"

extern struct imsgbuf	*ibuf;

__dead void	usage(void);
int		enqueue(int, char **);
int		enqueue_init(struct message *);
int		enqueue_add_recipient(struct message *, char *);
int		enqueue_messagefd(struct message *);
int		enqueue_write_message(FILE *, FILE *);
int		enqueue_commit(struct message *);

int
enqueue(int argc, char *argv[])
{
	int		ch;
	int		fd;
	FILE		*fpout;
	struct message	message;
	char		sender[MAX_PATH_SIZE];

	uid_t uid;
	char *username;
	char hostname[MAXHOSTNAMELEN];
	struct passwd *pw;

	uid = getuid();
	pw = safe_getpwuid(uid);
	if (pw == NULL)
		errx(1, "you don't exist, go away.");

	username = pw->pw_name;
	gethostname(hostname, sizeof(hostname));

	if (! bsnprintf(sender, sizeof(sender), "%s@%s", username, hostname))
		errx(1, "sender address too long.");

	while ((ch = getopt(argc, argv, "f:i")) != -1) {
		switch (ch) {
		case 'f':
			if (strlcpy(sender, optarg, sizeof(sender))
			    >= sizeof(sender))
				errx(1, "sender address too long.");
			break;
		case 'i': /* ignore, interface compatibility */
		case 'o':
			break;
		default:
			usage();
		}
	}

	argc -= optind;
	argv += optind;

	bzero(&message, sizeof(struct message));

	strlcpy(message.session_helo, "localhost",
	    sizeof(message.session_helo));
	strlcpy(message.session_hostname, hostname,
	    sizeof(message.session_hostname));
	
	/* build sender */
	if (! recipient_to_path(&message.sender, sender))
		errx(1, "invalid sender address.");
	
	if (! enqueue_init(&message))
		errx(1, "failed to initialize enqueue message.");
	
	if (argc == 0)
		errx(1, "no recipient.");

	while (argc--) {
		if (! enqueue_add_recipient(&message, *argv))
			errx(1, "invalid recipient.");
		++argv;
	}

	fd = enqueue_messagefd(&message);
	if (fd == -1 || (fpout = fdopen(fd, "w")) == NULL)
		errx(1, "failed to open message file for writing.");

	if (! enqueue_write_message(stdin, fpout))
		errx(1, "failed to write message to message file.");

	if (! safe_fclose(fpout))
		errx(1, "error while writing to message file.");

	if (! enqueue_commit(&message))
		errx(1, "failed to commit message to queue.");

	return 0;
}

int
enqueue_add_recipient(struct message *messagep, char *recipient)
{
	char buffer[MAX_PATH_SIZE];
	struct message_recipient mr;
	struct sockaddr_in6 *ssin6;
	struct sockaddr_in *ssin;
	struct message message;
	int done = 0;
	int n;
	struct imsg imsg;

	bzero(&mr, sizeof(mr));

	message = *messagep;

	if (strlcpy(buffer, recipient, sizeof(buffer)) >= sizeof(buffer))
		errx(1, "recipient address too long.");

	if (strchr(buffer, '@') == NULL) {
		if (! bsnprintf(buffer, sizeof(buffer), "%s@%s",
			buffer, messagep->sender.domain))
			errx(1, "recipient address too long.");
	}
	
	if (! recipient_to_path(&message.recipient, buffer))
		errx(1, "invalid recipient address.");

	message.session_rcpt = message.recipient;

	mr.ss.ss_family = AF_INET6;
	mr.ss.ss_len = sizeof(*ssin6);
	ssin6 = (struct sockaddr_in6 *)&mr.ss;
	if (inet_pton(AF_INET6, "::1", &ssin6->sin6_addr) != 1) {
		mr.ss.ss_family = AF_INET;
		mr.ss.ss_len = sizeof(*ssin);
		ssin = (struct sockaddr_in *)&mr.ss;
		if (inet_pton(AF_INET, "127.0.0.1", &ssin->sin_addr) != 1)
			return 0;
	}
	message.session_ss = mr.ss;

	mr.path = message.recipient;
	mr.id = message.session_id;
	mr.msg = message;
	mr.msg.flags |= F_MESSAGE_ENQUEUED;

	imsg_compose(ibuf, IMSG_MFA_RCPT, 0, 0, -1, &mr, sizeof (mr));
	while (ibuf->w.queued)
		if (msgbuf_write(&ibuf->w) < 0)
			err(1, "write error");	

	while (!done) {
		if ((n = imsg_read(ibuf)) == -1)
			errx(1, "imsg_read error");
		if (n == 0)
			errx(1, "pipe closed");

		if ((n = imsg_get(ibuf, &imsg)) == -1)
			errx(1, "imsg_get error");

		if (n == 0)
			continue;

		done = 1;
		switch (imsg.hdr.type) {
		case IMSG_CTL_OK: {
			return 1;
		}
		case IMSG_CTL_FAIL:
			return 0;
		default:
			errx(1, "unexpected reply (%d)", imsg.hdr.type);
		}
		imsg_free(&imsg);
	}

	return 1;
}

int
enqueue_write_message(FILE *fpin, FILE *fpout)
{
	char *buf, *lbuf;
	size_t len;
	
	lbuf = NULL;
	while ((buf = fgetln(fpin, &len))) {
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
		if (fprintf(fpout, "%s\n", buf) != (int)len + 1)
			return 0;
	}
	free(lbuf);
	return 1;
}

int
enqueue_init(struct message *messagep)
{
	int done = 0;
	int n;
	struct imsg imsg;

	imsg_compose(ibuf, IMSG_QUEUE_CREATE_MESSAGE, 0, 0, -1, messagep, sizeof(*messagep));
	while (ibuf->w.queued)
		if (msgbuf_write(&ibuf->w) < 0)
			err(1, "write error");	

	while (!done) {
		if ((n = imsg_read(ibuf)) == -1)
			errx(1, "imsg_read error");
		if (n == 0)
			errx(1, "pipe closed");

		if ((n = imsg_get(ibuf, &imsg)) == -1)
			errx(1, "imsg_get error");

		if (n == 0)
			continue;

		done = 1;
		switch (imsg.hdr.type) {
		case IMSG_CTL_OK: {
			struct message *mp;
			
			mp = imsg.data;
			messagep->session_id = mp->session_id;
			strlcpy(messagep->message_id, mp->message_id,
			    sizeof(messagep->message_id));

			return 1;
		}
		case IMSG_CTL_FAIL:
			return 0;
		default:
			err(1, "unexpected reply (%d)", imsg.hdr.type);
		}
		imsg_free(&imsg);
	}

	return 0;
}

int
enqueue_messagefd(struct message *messagep)
{
	int done = 0;
	int n;
	struct imsg imsg;

	imsg_compose(ibuf, IMSG_QUEUE_MESSAGE_FILE, 0, 0, -1, messagep, sizeof(*messagep));
	while (ibuf->w.queued)
		if (msgbuf_write(&ibuf->w) < 0)
			err(1, "write error");	

	while (!done) {
		if ((n = imsg_read(ibuf)) == -1)
			errx(1, "imsg_read error");
		if (n == 0)
			errx(1, "pipe closed");

		if ((n = imsg_get(ibuf, &imsg)) == -1)
			errx(1, "imsg_get error");

		if (n == 0)
			continue;

		done = 1;
		switch (imsg.hdr.type) {
		case IMSG_CTL_OK:
			return imsg_get_fd(ibuf, &imsg);
		case IMSG_CTL_FAIL:
			return -1;
		default:
			err(1, "unexpected reply (%d)", imsg.hdr.type);
		}
		imsg_free(&imsg);
	}

	return -1;
}


int
enqueue_commit(struct message *messagep)
{
	int done = 0;
	int n;
	struct imsg imsg;

	imsg_compose(ibuf, IMSG_QUEUE_COMMIT_MESSAGE, 0, 0, -1, messagep, sizeof(*messagep));
	while (ibuf->w.queued)
		if (msgbuf_write(&ibuf->w) < 0)
			err(1, "write error");	

	while (!done) {
		if ((n = imsg_read(ibuf)) == -1)
			errx(1, "imsg_read error");
		if (n == 0)
			errx(1, "pipe closed");

		if ((n = imsg_get(ibuf, &imsg)) == -1)
			errx(1, "imsg_get error");

		if (n == 0)
			continue;

		done = 1;
		switch (imsg.hdr.type) {
		case IMSG_CTL_OK: {
			return 1;
		}
		case IMSG_CTL_FAIL: {
			return 0;
		}
		default:
			err(1, "unexpected reply (%d)", imsg.hdr.type);
		}
		imsg_free(&imsg);
	}

	return 0;
}
