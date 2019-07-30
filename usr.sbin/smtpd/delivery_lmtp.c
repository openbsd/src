/* $OpenBSD: delivery_lmtp.c,v 1.17 2016/06/05 12:10:28 gilles Exp $ */

/*
 * Copyright (c) 2013 Ashish SHUKLA <ashish.is@lostca.se>
 * Copyright (c) 2015 Sunil Nimmagadda <sunil@nimmagadda.net>
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

#include <sys/socket.h>
#include <sys/tree.h>
#include <sys/un.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <event.h>
#include <fcntl.h>
#include <imsg.h>
#include <limits.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "smtpd.h"

/* should be more than enough for any LMTP server */
#define	MAX_CONTINUATIONS	100

static int	inet_socket(char *);
static int	lmtp_banner(char **buf, size_t *, int, FILE *);
static int	lmtp_cmd(char **buf, size_t *, int, FILE *, const char *, ...)
		    __attribute__((__format__ (printf, 5, 6)))
		    __attribute__((__nonnull__ (5)));
static void	lmtp_open(struct deliver *);
static int	unix_socket(char *);

struct delivery_backend delivery_backend_lmtp = {
	 0, lmtp_open
};

static int
inet_socket(char *address)
{
	 struct addrinfo	 hints, *res, *res0;
	 char			*hostname, *servname;
	 const char		*cause = NULL;
	 int			 n, s = -1, save_errno;

	 if ((servname = strchr(address, ':')) == NULL)
		 errx(1, "invalid address: %s", address);

	 *servname++ = '\0';
	 hostname = address;
	 memset(&hints, 0, sizeof(hints));
	 hints.ai_family = PF_UNSPEC;
	 hints.ai_socktype = SOCK_STREAM;
	 hints.ai_flags = AI_NUMERICSERV;
	 n = getaddrinfo(hostname, servname, &hints, &res0);
	 if (n)
		 errx(1, "%s", gai_strerror(n));

	 for (res = res0; res; res = res->ai_next) {
		s = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
		if (s == -1) {
			 cause = "socket";
			 continue;
		 }

		 if (connect(s, res->ai_addr, res->ai_addrlen) == -1) {
			 cause = "connect";
			 save_errno = errno;
			 close(s);
			 errno = save_errno;
			 s = -1;
			 continue;
		 }

		 break;
	 }

	 freeaddrinfo(res0);
	 if (s == -1)
		errx(1, "%s", cause);

	 return s;
}

static int
unix_socket(char *path)
{
	 struct sockaddr_un	addr;
	 int			s;

	 if ((s = socket(PF_LOCAL, SOCK_STREAM, 0)) == -1)
		 err(1, "socket");

	 memset(&addr, 0, sizeof(addr));
	 addr.sun_family = AF_UNIX;
	 if (strlcpy(addr.sun_path, path, sizeof(addr.sun_path))
	     >= sizeof(addr.sun_path))
		 errx(1, "socket path too long");

	 if (connect(s, (struct sockaddr *)&addr, sizeof(addr)) == -1)
		 err(1, "connect");

	 return s;
}

static void
lmtp_open(struct deliver *deliver)
{
	FILE		*fp;
	char		*buf = NULL, hn[HOST_NAME_MAX + 1],
			*rcpt = deliver->to, *to = deliver->to;
	size_t		 sz = 0;
	ssize_t		 len;
	int		 s;

	strsep(&rcpt, " ");
	s = (to[0] == '/') ? unix_socket(to) : inet_socket(to);
	if ((fp = fdopen(s, "r+")) == NULL)
		err(1, "fdopen");

	if (lmtp_banner(&buf, &sz, '2', fp) != 0)
		errx(1, "Invalid LHLO reply: %s", buf);

	if (gethostname(hn, sizeof hn) == -1)
		err(1, "gethostname");

	if (lmtp_cmd(&buf, &sz, '2', fp, "LHLO %s", hn) != 0)
		errx(1, "Invalid LHLO reply: %s", buf);

	if (lmtp_cmd(&buf, &sz, '2', fp, "MAIL FROM:<%s>", deliver->from) != 0)
		errx(1, "MAIL FROM rejected: %s", buf);

	if (lmtp_cmd(&buf, &sz, '2', fp, "RCPT TO:<%s>",
	    rcpt ? deliver->dest : deliver->user) != 0)
		errx(1, "RCPT TO rejected: %s", buf);

	if (lmtp_cmd(&buf, &sz, '3', fp, "DATA") != 0)
		errx(1, "Invalid DATA reply: %s", buf);

	while ((len = getline(&buf, &sz, stdin)) != -1) {
		if (buf[len - 1] == '\n')
			buf[len - 1] = '\0';

		if (fprintf(fp, "%s%s\r\n", buf[0] == '.' ? "." : "", buf) < 0)
			errx(1, "fprintf failed");
	}

	if (lmtp_cmd(&buf, &sz, '2', fp, ".") != 0)
		errx(1, "Delivery error: %s", buf);

	if (lmtp_cmd(&buf, &sz, '2', fp, "QUIT") != 0)
		errx(1, "Error on QUIT: %s", buf);

	exit(0);
}

static int
lmtp_banner(char **buf, size_t *sz, int code, FILE *fp)
{
	char	*bufp;
	ssize_t	 len;
	size_t	 counter;

	counter = 0;
	do {
		if ((len = getline(buf, sz, fp)) == -1)
			err(1, "getline");
		if (len < 4)
			err(1, "line too short");

		bufp = *buf;
		if (len >= 2 && bufp[len - 2] == '\r')
			bufp[len - 2] = '\0';
		else if (bufp[len - 1] == '\n')
			bufp[len - 1] = '\0';

		if (bufp[3] == '\0' || bufp[3] == ' ')
			break;
		else if (bufp[3] == '-') {
			if (counter == MAX_CONTINUATIONS)
				errx(1, "LMTP server is sending too many continuations");
			counter++;
			continue;
		}
		else
			errx(1, "invalid line");
	} while (1);

	return bufp[0] != code;
}

static int
lmtp_cmd(char **buf, size_t *sz, int code, FILE *fp, const char *fmt, ...)
{
	va_list	 ap;
	char	*bufp;
	ssize_t	 len;
	size_t	 counter;

	va_start(ap, fmt);
	if (vfprintf(fp, fmt, ap) < 0)
		errx(1, "vfprintf failed");

	va_end(ap);
	if (fprintf(fp, "\r\n") < 0)
		errx(1, "fprintf failed");

	if (fflush(fp) != 0)
		err(1, "fflush");

	counter = 0;
	do {
		if ((len = getline(buf, sz, fp)) == -1)
			err(1, "getline");
		if (len < 4)
			err(1, "line too short");

		bufp = *buf;
		if (len >= 2 && bufp[len - 2] == '\r')
			bufp[len - 2] = '\0';
		else if (bufp[len - 1] == '\n')
			bufp[len - 1] = '\0';

		if (bufp[3] == '\0' || bufp[3] == ' ')
			break;
		else if (bufp[3] == '-') {
			if (counter == MAX_CONTINUATIONS)
				errx(1, "LMTP server is sending too many continuations");
			counter++;
			continue;
		}
		else
			errx(1, "invalid line");
	} while (1);

	return bufp[0] != code;
}
