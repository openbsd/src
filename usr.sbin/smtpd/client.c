/*
 * Copyright (c) 2009 Jacek Masiulaniec <jacekm@dobremiasto.net>
 * Copyright (c) 2002, 2003 Niels Provos <provos@citi.umich.edu>
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
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/uio.h>

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "imsg.h"
#include "client.h"

int		 client_readln(struct smtp_client *, char **);
int		 client_writeln(struct smtp_client *, char *, ...);
int		 client_error(struct smtp_client *, char **, char *);
int		 client_data_add(struct smtp_client *, char *, size_t);
char		*buf_getln(struct buf_read *);

struct smtp_client *
client_init(int fd, char *ehlo)
{
	struct smtp_client	*sp = NULL;
	int			 rv = -1;

	if ((sp = calloc(1, sizeof(*sp))) == NULL)
		goto done;
	if ((sp->ehlo = strdup(ehlo)) == NULL)
		goto done;
	if ((sp->data = buf_dynamic(0, SIZE_T_MAX)) == NULL)
		goto done;
	sp->state = CLIENT_INIT;
	msgbuf_init(&sp->w);
	sp->w.fd = fd;
	TAILQ_INIT(&sp->recipients);
	
	rv = 0;
done:
	if (rv < 0) {
		free(sp->ehlo);
		if (sp->data)
			buf_free(sp->data);
		free(sp);
		sp = NULL;
	}
	return (sp);
}

void
client_verbose(struct smtp_client *sp, int fd)
{
	sp->verbose = fdopen(fd, "w");
}

int
client_sender(struct smtp_client *sp, char *fmt, ...)
{
	va_list		 ap;
	char		*mbox = NULL;
	int		 rv = -1;

	va_start(ap, fmt);

	/* check if too late */
	switch (sp->state) {
	case CLIENT_INIT:
	case CLIENT_EHLO:
	case CLIENT_HELO:
		break;
	default:
		goto done;
	}

	if (vasprintf(&mbox, fmt, ap) == -1)
		goto done;
	if (sp->sender)
		free(sp->sender);
	sp->sender = mbox;

	rv = 0;
done:
	va_end(ap);
	if (rv < 0)
		free(mbox);
	return (rv);
}

int
client_rcpt(struct smtp_client *sp, char *fmt, ...)
{
	va_list		 ap;
	struct rcpt	*rp = NULL;
	int		 rv = -1;

	va_start(ap, fmt);

	/* check if too late */
	switch (sp->state) {
	case CLIENT_INIT:
	case CLIENT_EHLO:
	case CLIENT_HELO:
	case CLIENT_MAILFROM:
		break;
	default:
		goto done;
	}

	if ((rp = calloc(1, sizeof(*rp))) == NULL)
		goto done;
	if (vasprintf(&rp->mbox, fmt, ap) == -1)
		goto done;
	TAILQ_INSERT_TAIL(&sp->recipients, rp, entry);

	rv = 0;
done:
	va_end(ap);
	if (rv < 0) {
		if (rp)
			free(rp->mbox);
		free(rp);
	}
	return (rv);
}

int
client_data_fd(struct smtp_client *sp, int fd)
{
	struct stat	 sb;
	char		*map = NULL;
	int		 rv = -1;

	if (fstat(fd, &sb) == -1)
		goto done;
	if (sb.st_size > SIZE_T_MAX)
		goto done;
	if (!S_ISREG(sb.st_mode))
		goto done;
	map = mmap(NULL, sb.st_size, PROT_READ|PROT_WRITE, MAP_PRIVATE, fd,
	    (off_t)0);
	if (map == MAP_FAILED)
		goto done;
	madvise(map, sb.st_size, MADV_SEQUENTIAL);

	if (client_data_add(sp, map, sb.st_size) < 0)
		goto done;

	rv = 0;
done:
	close(fd);
	if (map)
		munmap(map, sb.st_size);
	return (rv);
}

int
client_data_printf(struct smtp_client *sp, char *fmt, ...)
{
	va_list	 ap;
	char	*p = NULL;
	int	 len, rv = -1;

	va_start(ap, fmt);
	if ((len = vasprintf(&p, fmt, ap)) == -1)
		goto done;

	if (client_data_add(sp, p, len) < 0)
		goto done;

	rv = 0;
done:
	va_end(ap);
	free(p);
	return (rv);
}

int
client_read(struct smtp_client *sp, char **ep)
{
	char	*reply;

	*ep = NULL;

	/* read server reply */
	if (client_readln(sp, &reply) < 0)
		return client_error(sp, ep, reply);
	if (reply == NULL)
		return (CLIENT_WANT_READ);

	/* send command */
	switch (sp->state) {
	case CLIENT_INIT:
		if (*reply != '2')
			client_error(sp, ep, reply);
		else
			sp->state = CLIENT_EHLO;
		break;

	case CLIENT_EHLO:
		if (*reply != '2')
			sp->state = CLIENT_HELO;
		else
			sp->state = CLIENT_MAILFROM;
		break;

	case CLIENT_HELO:
		if (*reply != '2')
			client_error(sp, ep, reply);
		else
			sp->state = CLIENT_MAILFROM;
		break;

	case CLIENT_MAILFROM:
		if (*reply != '2')
			client_error(sp, ep, reply);
		else
			sp->state = CLIENT_RCPTTO;
		break;

	case CLIENT_RCPTTO:
		if (*reply != '2')
			client_error(sp, ep, reply);
		else if (TAILQ_EMPTY(&sp->recipients))
			sp->state = CLIENT_DATA;
		break;

	case CLIENT_DATA:
		if (*reply != '3')
			client_error(sp, ep, reply);
		else
			sp->state = CLIENT_DATA_BODY;
		break;

	case CLIENT_DATA_BODY:
		if (*reply != '2')
			client_error(sp, ep, reply);
		else
			return (CLIENT_DONE);
		break;

	default:
		client_error(sp, ep, "invalid state");
	}
	free(reply);

	/* may have been set by client_error */
	if (*ep)
		return (CLIENT_ERROR);

	return (CLIENT_WANT_WRITE);
}

int
client_write(struct smtp_client *sp, char **ep)
{
	struct rcpt	*rp;

	*ep = NULL;

	/* complete any pending write */
	if (sp->w.queued)
		goto write;

	switch (sp->state) {
	case CLIENT_EHLO:
		if (client_writeln(sp, "EHLO %s", sp->ehlo) < 0)
			return client_error(sp, ep, NULL);
		break;

	case CLIENT_HELO:
		if (client_writeln(sp, "HELO %s", sp->ehlo) < 0)
			return client_error(sp, ep, NULL);
		break;

	case CLIENT_MAILFROM:
		if (client_writeln(sp, "MAIL FROM:<%s>", sp->sender) < 0)
			return client_error(sp, ep, NULL);
		break;

	case CLIENT_RCPTTO:
		rp = TAILQ_FIRST(&sp->recipients);
		TAILQ_REMOVE(&sp->recipients, rp, entry);
		if (client_writeln(sp, "RCPT TO:<%s>", rp->mbox) < 0)
			return client_error(sp, ep, NULL);
		free(rp->mbox);
		free(rp);
		break;

	case CLIENT_DATA:
		if (client_writeln(sp, "DATA") < 0)
			return client_error(sp, ep, NULL);
		break;

	case CLIENT_DATA_BODY:
		if (buf_add(sp->data, ".\r\n", 3) == -1)
			return client_error(sp, ep, NULL);
		buf_close(&sp->w, sp->data);
		sp->data = NULL;
		break;

	default:
		return client_error(sp, ep, "invalid state");
	}

write:
	if (buf_write(&sp->w) < 0)
		return client_error(sp, ep, NULL);

	return (sp->w.queued ? CLIENT_WANT_WRITE : CLIENT_WANT_READ);
}

void
client_close(struct smtp_client *sp)
{
	struct rcpt	*rp;

	free(sp->ehlo);
	free(sp->sender);
	if (sp->data)
		buf_free(sp->data);
	msgbuf_clear(&sp->w);
	while ((rp = TAILQ_FIRST(&sp->recipients))) {
		TAILQ_REMOVE(&sp->recipients, rp, entry);
		free(rp->mbox);
		free(rp);
	}
	close(sp->w.fd);
	free(sp);
}

int
client_readln(struct smtp_client *sp, char **reply)
{
	struct buf_read	*r = &sp->r;
	size_t		 i;
	char		*ln = NULL;
	int		 n, rv = -1;

	*reply = NULL;

	/* read data from the socket */
	n = read(sp->w.fd, r->buf + r->wpos, sizeof(r->buf) - r->wpos);
	if (n == -1) {
		if (errno == EAGAIN || errno == EINTR)
			rv = 0;
		else
			*reply = strerror(errno);
		goto done;
	}
	r->wpos += n;

	/* get a reply, dealing with multiline responses */
	for (;;) {
		errno = 0;
		if ((ln = buf_getln(r)) == NULL) {
			if (errno)
				*reply = strerror(errno);
			else if (r->wpos >= sizeof(r->buf))
				*reply = "reply too big";
			else
				rv = 0;
			goto done;
		}

		if (sp->verbose)
			fprintf(sp->verbose, "<<< %s\n", ln);

		if (strlen(ln) == 3 || ln[3] == ' ')
			break;
		else if (ln[3] != '-') {
			*reply = "garbled multiline reply";
			goto done;
		}
	}

	if (ln[0] < '2' || ln[0] > '5' || !isdigit(ln[1]) || !isdigit(ln[2])) {
		*reply = "reply code out of range"; 
		goto done;
	}

	for (i = 0; ln[i] != '\0'; i++) {
		if (!isprint(ln[i])) {
			*reply = "non-printable character in server reply";
			goto done;
		}
	}

	*reply = ln;

	rv = 0;
done:
	if (rv < 0)
		free(ln);
	return (rv);
}

int
client_writeln(struct smtp_client *sp, char *fmt, ...)
{
	struct buf	*cmd = NULL;
	char		*p = NULL;
	int		 len, rv = -1;
	va_list		 ap;

	va_start(ap, fmt);
	if ((len = vasprintf(&p, fmt, ap)) == -1)
		goto done;

	if (sp->verbose)
		fprintf(sp->verbose, ">>> %s\n", p);

	if ((cmd = buf_open(len + 2)) == NULL)
		goto done;
	if (buf_add(cmd, p, len) < 0)
		goto done;
	if (buf_add(cmd, "\r\n", 2) < 0)
		goto done;
	buf_close(&sp->w, cmd);
	cmd = NULL;


	rv = 0;
done:
	va_end(ap);
	free(p);
	if (cmd)
		buf_free(cmd);
	return (rv);
}

int
client_data_add(struct smtp_client *sp, char *buf, size_t len)
{
	char	*ln;

	/* check if too late */
	switch (sp->state) {
	case CLIENT_INIT:
	case CLIENT_EHLO:
	case CLIENT_HELO:
	case CLIENT_MAILFROM:
	case CLIENT_RCPTTO:
		break;
	default:
		return (-1);
	}

	/* must end with a newline */
	if (len == 0 || buf[len - 1] != '\n')
		return (-1);
	buf[len - 1] = '\0';

	/* split into lines, deal with dot escaping etc. */
	while ((ln = strsep(&buf, "\n"))) {
		if (*ln == '.' && buf_add(sp->data, ".", 1) < 0)
			return (-1);
		if (buf_add(sp->data, ln, strlen(ln)) < 0)
			return (-1);
		if (buf_add(sp->data, "\r\n", 2) < 0)
			return (-1);
	}

	return (0);
}

int
client_error(struct smtp_client *sp, char **ep, char *errormsg)
{
	if (errormsg == NULL)
		errormsg = strerror(errno);
	strlcpy(sp->errbuf, errormsg, sizeof(sp->errbuf));
	*ep = sp->errbuf;
	sp->state = -1;

	return (CLIENT_ERROR);
}

char *
buf_getln(struct buf_read *r)
{
	char	*line;
	size_t	 i;

	/* look for terminating newline */
	for (i = 0; i < r->wpos; i++)
		if (r->buf[i] == '\r' || r->buf[i] == '\n')
			break;
	if (i == r->wpos)
		return (NULL);

	/* make a copy of the line */
        if ((line = malloc(i + 1)) == NULL)
		return (NULL);
        memcpy(line, r->buf, i);
        line[i] = '\0';

	/* drain the buffer */
	if (i < r->wpos - 1) {
		char fch = r->buf[i], sch = r->buf[i + 1];

		if ((sch == '\r' || sch == '\n') && sch != fch)
			i += 1;
	}
	memmove(r->buf, r->buf + i + 1, r->wpos - i - 1);
	r->wpos -= i + 1;

	return (line);
}
