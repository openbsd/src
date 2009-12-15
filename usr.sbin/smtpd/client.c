/*	$OpenBSD: client.c,v 1.20 2009/12/15 11:45:51 jacekm Exp $	*/

/*
 * Copyright (c) 2009 Jacek Masiulaniec <jacekm@dobremiasto.net>
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
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/uio.h>

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <netdb.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <openssl/ssl.h>

#include "imsg.h"
#include "client.h"

int		 client_read(struct smtp_client *);
int		 client_write(struct smtp_client *);
int		 client_next_state(struct smtp_client *);
void		 client_status(struct smtp_client *, char *, ...);

int		 client_getln(struct smtp_client *);
void		 client_putln(struct smtp_client *, char *, ...);
void		 client_body(struct msgbuf *, FILE *);

#ifndef CLIENT_NO_SSL
int		 client_ssl_connect(struct smtp_client *);
SSL		*ssl_client_init(int, char *, size_t, char *, size_t);
int		 ssl_buf_read(SSL *, struct buf_read *);
int		 ssl_buf_write(SSL *, struct msgbuf *);
#endif

char		*buf_getln(struct buf_read *);
int		 buf_read(int, struct buf_read *);

void		 log_debug(const char *, ...);	/* XXX */
void		 fatal(const char *);	/* XXX */
void		 fatalx(const char *);	/* XXX */

/*
 * Initialize SMTP session.
 */
struct smtp_client *
client_init(int fd, int body, char *ehlo, int verbose)
{
	struct smtp_client	*sp = NULL;

	if ((sp = calloc(1, sizeof(*sp))) == NULL)
		fatal(NULL);
	if (ehlo == NULL || *ehlo == '\0') {
		char			 buf[NI_MAXHOST];
		struct sockaddr_storage	 sa;
		socklen_t		 len;

		len = sizeof(sa);
		if (getsockname(fd, (struct sockaddr *)&sa, &len))
			fatal("client_init: getsockname");
		if (getnameinfo((struct sockaddr *)&sa, len, buf, sizeof(buf),
		    NULL, 0, NI_NUMERICHOST))
			fatalx("client_init: getnameinfo");
		if (asprintf(&sp->ehlo, "[%s]", buf) == -1)
			fatal("client_init: asprintf");
	} else if ((sp->ehlo = strdup(ehlo)) == NULL)
		fatal(NULL);
	if ((sp->sender = strdup("")) == NULL)
		fatal(NULL);
	if (verbose)
		sp->verbose = stdout;
	else if ((sp->verbose = fopen("/dev/null", "a")) == NULL)
		fatal("client_init: fopen");
	if ((sp->body = fdopen(body, "r")) == NULL)
		fatal("client_init: fdopen");
	sp->state = CLIENT_INIT;
	sp->handler = client_write;
	sp->timeout.tv_sec = 300;
	msgbuf_init(&sp->w);
	sp->w.fd = fd;
	TAILQ_INIT(&sp->recipients);

	sp->exts[CLIENT_EXT_STARTTLS].want = 1;
	sp->exts[CLIENT_EXT_STARTTLS].must = 1;
#ifdef CLIENT_NO_SSL
	sp->exts[CLIENT_EXT_STARTTLS].want = 0;
	sp->exts[CLIENT_EXT_STARTTLS].must = 0;
#endif
	sp->exts[CLIENT_EXT_STARTTLS].state = CLIENT_STARTTLS;
	sp->exts[CLIENT_EXT_STARTTLS].name = "STARTTLS";

	sp->exts[CLIENT_EXT_AUTH].want = 0;
	sp->exts[CLIENT_EXT_AUTH].must = 0;
	sp->exts[CLIENT_EXT_AUTH].state = CLIENT_AUTH;
	sp->exts[CLIENT_EXT_AUTH].name = "AUTH";

	return (sp);
}

/*
 * Request that connection be secured using SSL from the start.
 */
void
client_ssl_smtps(struct smtp_client *sp)
{
	sp->state = CLIENT_SSL_INIT;
	sp->exts[CLIENT_EXT_STARTTLS].want = 0;
	sp->exts[CLIENT_EXT_STARTTLS].must = 0;
}

/*
 * Allow session to progress in plaintext if STARTTLS fails.
 */
void
client_ssl_optional(struct smtp_client *sp)
{
	sp->exts[CLIENT_EXT_STARTTLS].must = 0;
}

/*
 * Use the provided certificate during SSL handshake.
 */
void
client_certificate(struct smtp_client *sp, char *cert, size_t certsz,
    char *key, size_t keysz)
{
	if ((sp->auth.cert = malloc(certsz)) == NULL)
		fatal(NULL);
	if ((sp->auth.key = malloc(keysz)) == NULL)
		fatal(NULL);
	memcpy(sp->auth.cert, cert, certsz);
	memcpy(sp->auth.key, key, keysz);
	sp->auth.certsz = certsz;
	sp->auth.keysz = keysz;
}

/*
 * Use the AUTH extension.
 */
void
client_auth(struct smtp_client *sp, char *secret)
{
	if ((sp->auth.plain = strdup(secret)) == NULL)
		fatal(NULL);

	sp->exts[CLIENT_EXT_AUTH].want = 1;
	sp->exts[CLIENT_EXT_AUTH].must = 1;
}

/*
 * Set envelope sender.  If not called, the sender is assumed to be "<>".
 */
void
client_sender(struct smtp_client *sp, char *fmt, ...)
{
	va_list		 ap;

	free(sp->sender);

	va_start(ap, fmt);
	if (vasprintf(&sp->sender, fmt, ap) == -1)
		fatal("client_sender: vasprintf");
	va_end(ap);
}

/*
 * Add mail recipient.
 */
void
client_rcpt(struct smtp_client *sp, void *p, char *fmt, ...)
{
	va_list		 ap;
	struct rcpt	*rp = NULL;

	if ((rp = calloc(1, sizeof(*rp))) == NULL)
		fatal(NULL);
	rp->p = p;

	va_start(ap, fmt);
	if (vasprintf(&rp->mbox, fmt, ap) == -1)
		fatal("client_rcpt: vasprintf");
	va_end(ap);

	TAILQ_INSERT_TAIL(&sp->recipients, rp, entry);
	sp->rcpt = TAILQ_FIRST(&sp->recipients);
}

/*
 * Append string to the data buffer.
 */
void
client_printf(struct smtp_client *sp, char *fmt, ...)
{
	va_list	 ap;
	char	*p, *ln, *tmp;
	int	 len;

	if (sp->head == NULL)
		sp->head = buf_dynamic(0, SIZE_T_MAX);
	if (sp->head == NULL)
		fatal(NULL);

	va_start(ap, fmt);
	if ((len = vasprintf(&p, fmt, ap)) == -1)
		fatal("client_data_printf: vasprintf");
	va_end(ap);

	/* must end with a newline */
	if (len == 0 || p[len - 1] != '\n')
		fatalx("client_printf: invalid use");
	p[len - 1] = '\0';

	/* split into lines, deal with dot escaping etc. */
	tmp = p;
	while ((ln = strsep(&tmp, "\n"))) {
		if (*ln == '.' && buf_add(sp->head, ".", 1))
			fatal(NULL);
		if (buf_add(sp->head, ln, strlen(ln)))
			fatal(NULL);
		if (buf_add(sp->head, "\r\n", 2))
			fatal(NULL);
	}

	free(p);
}

/*
 * Routine called by the user to progress the session.
 */
int
client_talk(struct smtp_client *sp)
{
	int ret;

	ret = sp->handler(sp);

	if (ret == CLIENT_WANT_READ)
		sp->handler = client_read;
	else if (ret == CLIENT_WANT_WRITE)
		sp->handler = client_write;

	return (ret);
}

/*
 * Handler to be called when socket becomes readable.
 */
int
client_read(struct smtp_client *sp)
{
#ifndef CLIENT_NO_SSL
	if (sp->state == CLIENT_SSL_CONNECT)
		return client_ssl_connect(sp);

	/* read data from the socket */
	if (sp->ssl) {
		switch (ssl_buf_read(sp->ssl, &sp->r)) {
		case SSL_ERROR_NONE:
			break;

		case SSL_ERROR_WANT_READ:
			return (CLIENT_WANT_READ);

		case SSL_ERROR_WANT_WRITE:
			return (CLIENT_WANT_WRITE);

		default:
			client_status(sp, "130 ssl_buf_read error");
			return (CLIENT_DONE);
		}
	}
#endif
	if (sp->ssl == NULL) {
		errno = 0;
		if (buf_read(sp->w.fd, &sp->r) == -1) {
			if (errno)
				client_status(sp, "130 buf_read: %s",
				    strerror(errno));
			else
				client_status(sp, "130 buf_read: "
				    "connection closed");
			return (CLIENT_DONE);
		}
	}

	/* get server reply */
	if (client_getln(sp) < 0)
		goto quit2;
	if (*sp->reply == '\0')
		return (CLIENT_WANT_READ);

	/*
	 * Devalue untimely 5yz reply down to 1yz in order to protect
	 * the caller from dropping mail for trifle reason.
	 */ 
	if (*sp->reply == '5' &&
	    sp->state != CLIENT_EHLO &&
	    sp->state != CLIENT_AUTH &&
	    sp->state != CLIENT_MAILFROM &&
	    sp->state != CLIENT_RCPTTO &&
	    sp->state != CLIENT_DATA &&
	    sp->state != CLIENT_DATA_BODY) {
		client_status(sp, "190 untimely 5yz reply: %s", sp->reply);
		goto quit2;
	}

	switch (sp->state) {
	case CLIENT_INIT:
		if (*sp->reply != '2')
			goto quit;
		else
			sp->state = CLIENT_EHLO;
		break;

	case CLIENT_EHLO:
		if (*sp->reply != '2') {
			if (sp->exts[CLIENT_EXT_STARTTLS].must ||
			    sp->exts[CLIENT_EXT_AUTH].must)
				goto quit;
			else
				sp->state = CLIENT_HELO;
			break;
		}
	
		if ((sp->state = client_next_state(sp)) == 0)
			goto quit2;
		break;

	case CLIENT_HELO:
		if (*sp->reply != '2')
			goto quit;
		else
			sp->state = CLIENT_MAILFROM;
		break;

	case CLIENT_STARTTLS:
		if (*sp->reply != '2') {
			sp->exts[CLIENT_EXT_STARTTLS].fail = 1;
			if ((sp->state = client_next_state(sp)) == 0)
				goto quit2;
		} else
			sp->state = CLIENT_SSL_INIT;
		break;

	case CLIENT_AUTH:
		if (*sp->reply != '2')
			sp->exts[CLIENT_EXT_AUTH].fail = 1;
		else
			sp->exts[CLIENT_EXT_AUTH].done = 1;

		if ((sp->state = client_next_state(sp)) == 0)
			goto quit2;
		break;

	case CLIENT_MAILFROM:
		if (*sp->reply != '2')
			goto quit;
		else
			sp->state = CLIENT_RCPTTO;
		break;

	case CLIENT_RCPTTO:
		if (*sp->reply == '2') {
			sp->rcptokay++;
			sp->rcpt = TAILQ_NEXT(sp->rcpt, entry);
		} else {
			sp->rcptfail = sp->rcpt;
			sp->rcpt = TAILQ_NEXT(sp->rcpt, entry);
			return (CLIENT_RCPT_FAIL);
		}
		break;

	case CLIENT_DATA:
		if (*sp->reply != '3')
			goto quit;
		else
			sp->state = CLIENT_DATA_BODY;
		break;

	case CLIENT_DATA_BODY:
		goto quit;

	case CLIENT_QUIT:
		return (CLIENT_WANT_READ);

	default:
		fatalx("client_read: unexpected state");
	}

	return (CLIENT_WANT_WRITE);

quit:
	client_status(sp, "%s", sp->reply);
quit2:
	sp->state = CLIENT_QUIT;
	return (CLIENT_WANT_WRITE);
}

/*
 * Handler to be called when socket becomes writable.
 */
int
client_write(struct smtp_client *sp)
{

#ifndef CLIENT_NO_SSL
	if (sp->state == CLIENT_SSL_CONNECT)
		return client_ssl_connect(sp);
#endif

	/* complete any pending write */
	if (sp->w.queued)
		goto write;

	switch (sp->state) {
#ifndef CLIENT_NO_SSL
	case CLIENT_SSL_INIT:
		log_debug("client: ssl handshake started");
		sp->ssl = ssl_client_init(sp->w.fd,
		    sp->auth.cert, sp->auth.certsz,
		    sp->auth.key, sp->auth.keysz);
		if (sp->ssl == NULL) {
			client_status(sp, "130 SSL init failed");
			return (CLIENT_DONE);
		} else {
			sp->state = CLIENT_SSL_CONNECT;
			return (CLIENT_WANT_WRITE);
		}
		break;
#endif

	case CLIENT_INIT:
		/* read the banner */
		return (CLIENT_WANT_READ);

	case CLIENT_EHLO:
	case CLIENT_HELO:
		sp->exts[CLIENT_EXT_STARTTLS].have = 0;
		sp->exts[CLIENT_EXT_STARTTLS].fail = 0;

		sp->exts[CLIENT_EXT_AUTH].have = 0;
		sp->exts[CLIENT_EXT_AUTH].fail = 0;

		client_putln(sp, "%s %s", sp->state == CLIENT_EHLO ? "EHLO" :
		    "HELO", sp->ehlo);
		break;

	case CLIENT_AUTH:
		client_putln(sp, "AUTH PLAIN %s", sp->auth.plain);
		break;

	case CLIENT_STARTTLS:
		client_putln(sp, "STARTTLS");
		break;

	case CLIENT_MAILFROM:
		client_putln(sp, "MAIL FROM:<%s>", sp->sender);
		break;

	case CLIENT_RCPTTO:
		if (sp->rcpt == NULL) {
			if (sp->rcptokay > 0)
				sp->state = CLIENT_DATA;
			else
				sp->state = CLIENT_QUIT;
			return (CLIENT_WANT_WRITE);
		}
		client_putln(sp, "RCPT TO:<%s>", sp->rcpt->mbox);
		break;

	case CLIENT_DATA:
		sp->timeout.tv_sec = 120;
		client_putln(sp, "DATA");
		break;

	case CLIENT_DATA_BODY:
		sp->timeout.tv_sec = 180;
		if (sp->head) {
			buf_close(&sp->w, sp->head);
			sp->head = NULL;
		}
		client_body(&sp->w, sp->body);
		break;

	case CLIENT_QUIT:
		sp->timeout.tv_sec = 300;
		client_putln(sp, "QUIT");
		break;

	default:
		fatalx("client_write: unexpected state");
	}

write:
#ifndef CLIENT_NO_SSL
	if (sp->ssl) {
		switch (ssl_buf_write(sp->ssl, &sp->w)) {
		case SSL_ERROR_NONE:
			break;

		case SSL_ERROR_WANT_READ:
			return (CLIENT_WANT_READ);

		case SSL_ERROR_WANT_WRITE:
			return (CLIENT_WANT_WRITE);

		default:
			client_status(sp, "130 ssl_buf_write error");
			return (CLIENT_DONE);
		}
	}
#endif
	if (sp->ssl == NULL) {
		if (buf_write(&sp->w) < 0) {
			client_status(sp, "130 buf_write error");
			return (CLIENT_DONE);
		}
	}

	if (sp->state == CLIENT_DATA_BODY) {
		if (!feof(sp->body))
			return (CLIENT_WANT_WRITE);
		else
			sp->timeout.tv_sec = 600;
	}

	return (sp->w.queued ? CLIENT_WANT_WRITE : CLIENT_WANT_READ);
}

#ifndef CLIENT_NO_SSL
/*
 * Progress SSL handshake.
 */
int
client_ssl_connect(struct smtp_client *sp)
{
	int	 ret;

	ret = SSL_connect(sp->ssl);

	switch (SSL_get_error(sp->ssl, ret)) {
	case SSL_ERROR_WANT_READ:
		return (CLIENT_WANT_READ);

	case SSL_ERROR_WANT_WRITE:
		return (CLIENT_WANT_WRITE);

	case SSL_ERROR_NONE:
		break;

	default:
		log_debug("client: ssl handshake failed");

		if (sp->exts[CLIENT_EXT_STARTTLS].want) {
			sp->exts[CLIENT_EXT_STARTTLS].fail = 1;
			SSL_free(sp->ssl);
			sp->ssl = NULL;
			if ((sp->state = client_next_state(sp)) != 0)
				return (CLIENT_WANT_WRITE);
		} else
			client_status(sp, "130 SSL_connect error");

		return (CLIENT_DONE);
	}

	log_debug("client: ssl handshake completed");

	if (sp->exts[CLIENT_EXT_STARTTLS].want)
		sp->state = CLIENT_EHLO;
	else
		sp->state = CLIENT_INIT;

	sp->exts[CLIENT_EXT_STARTTLS].done = 1;

	return (CLIENT_WANT_WRITE);
}
#endif

/*
 * Deinitialization routine.
 */
void
client_close(struct smtp_client *sp)
{
	struct rcpt	*rp;

	free(sp->ehlo);
	free(sp->sender);
	free(sp->auth.plain);
	free(sp->auth.cert);
	free(sp->auth.key);
	if (sp->head)
		buf_free(sp->head);
	msgbuf_clear(&sp->w);
	while ((rp = TAILQ_FIRST(&sp->recipients))) {
		TAILQ_REMOVE(&sp->recipients, rp, entry);
		free(rp->mbox);
		free(rp);
	}
#ifndef CLIENT_NO_SSL
	if (sp->ssl)
		SSL_free(sp->ssl);
#endif
	close(sp->w.fd);
	free(sp);
}

/*
 * Decide if any extensions need to be requested before proceeding to
 * the MAIL FROM command.
 */
int
client_next_state(struct smtp_client *sp)
{
	struct client_ext	*e;
	size_t			 i;

	/* Request extensions that require use of a verb. */
	for (i = 0; i < nitems(sp->exts); i++) {
		e = &sp->exts[i];
		if (e->want && !e->done) {
			if (e->have && !e->fail)
				return (e->state);
			else if (e->must) {
				client_status(sp, "600 %s %s", e->name,
				     e->fail ? "failed" : "not available");
				return (0);
			}
		}
	}

	return (CLIENT_MAILFROM);
}

/*
 * Update status field which the caller uses to check if any errors were
 * encountered.
 */
void
client_status(struct smtp_client *sp, char *fmt, ...)
{
	va_list ap;

	/* Don't record errors that occurred at QUIT. */
	if (sp->state == CLIENT_QUIT)
		return;

	va_start(ap, fmt);
	if (vsnprintf(sp->status, sizeof(sp->status), fmt, ap) == -1)
		fatal("client_status: vnprintf");
	va_end(ap);
}

/*
 * Read and validate next line from the input buffer.
 */
int
client_getln(struct smtp_client *sp)
{
	char	*ln = NULL, *cause = "";
	int	 i, rv = -1;

	sp->reply[0] = '\0';

	/* get a reply, dealing with multiline responses */
	for (;;) {
		errno = 0;
		if ((ln = buf_getln(&sp->r)) == NULL) {
			if (errno)
				cause = "150 buf_getln error";
			else
				rv = 0;
			goto done;
		}

		fprintf(sp->verbose, "<<< %s\n", ln);

		/* 3-char replies are invalid on their own, append space */
		if (strlen(ln) == 3) {
			char buf[5];

			strlcpy(buf, ln, sizeof(buf));
			strlcat(buf, " ", sizeof(buf));
			free(ln);
			if ((ln = strdup(buf)) == NULL) {
				cause = "150 strdup error";
				goto done;
			}
		}

		if (strlen(ln) < 4 || (ln[3] != ' ' && ln[3] != '-')) {
			cause = "150 garbled smtp reply";
			goto done;
		}

		if (sp->state == CLIENT_EHLO) {
			if (strcmp(ln + 4, "STARTTLS") == 0)
				sp->exts[CLIENT_EXT_STARTTLS].have = 1;
			else if (strncmp(ln + 4, "AUTH", 4) == 0)
				sp->exts[CLIENT_EXT_AUTH].have = 1;
		}

		if (ln[3] == ' ')
			break;
	}

	/* validate reply code */
	if (ln[0] < '2' || ln[0] > '5' || !isdigit(ln[1]) || !isdigit(ln[2])) {
		cause = "150 reply code out of range";
		goto done;
	}

	/* validate reply message */
	for (i = 0; ln[i] != '\0'; i++) {
		if (!isprint(ln[i])) {
			cause = "150 non-printable character in reply";
			goto done;
		}
	}

	strlcpy(sp->reply, ln, sizeof(sp->reply));

	rv = 0;
done:
	if (rv)
		client_status(sp, cause);
	free(ln);
	return (rv);
}

/*
 * Add a line to the output buffer.
 */
void
client_putln(struct smtp_client *sp, char *fmt, ...)
{
	struct buf	*cmd = NULL;
	char		*p = NULL;
	int		 len;
	va_list		 ap;

	va_start(ap, fmt);
	if ((len = vasprintf(&p, fmt, ap)) == -1)
		fatal("client_putln: vasprintf");
	va_end(ap);

	fprintf(sp->verbose, ">>> %s\n", p);

	if ((cmd = buf_open(len + 2)) == NULL)
		fatal(NULL);
	if (buf_add(cmd, p, len))
		fatal(NULL);
	if (buf_add(cmd, "\r\n", 2))
		fatal(NULL);
	buf_close(&sp->w, cmd);

	free(p);
}

/*
 * Put chunk of message content to output buffer.
 */
void
client_body(struct msgbuf *out, FILE *fp)
{
	struct buf	*b;
	char		*ln;
	size_t		 len, total = 0;

	if ((b = buf_dynamic(0, SIZE_T_MAX)) == NULL)
		fatal(NULL);

	while ((ln = fgetln(fp, &len)) && total < 4096) {
		if (ln[len - 1] == '\n')
			len--;
		if (*ln == '.' && buf_add(b, ".", 1))
			fatal(NULL);
		if (buf_add(b, ln, len))
			fatal(NULL);
		if (buf_add(b, "\r\n", 2))
			fatal(NULL);
		total += len;
	}
	if (ferror(fp))
		fatal("client_body: fgetln");
	if (feof(fp) && buf_add(b, ".\r\n", 3))
		fatal(NULL);

	buf_close(out, b);
}

/*
 * Read a full line from the read buffer.
 */
char *
buf_getln(struct buf_read *r)
{
	char	*buf = r->buf, *line;
	size_t	 bufsz = r->wpos, i;

	/* look for terminating newline */
	for (i = 0; i < bufsz; i++)
		if (buf[i] == '\n')
			break;
	if (i == bufsz)
		return (NULL);

	/* make a copy of the line */
	if ((line = calloc(i + 1, 1)) == NULL)
		return (NULL);
	memcpy(line, buf, i);

	/* handle CRLF */
	if (i != 0 && line[i - 1] == '\r')
		line[i - 1] = '\0';

	/* drain the buffer */
	memmove(buf, buf + i + 1, bufsz - i - 1);
	r->wpos -= i + 1;

	return (line);
}

/*
 * I/O routine for reading UNIX socket.
 */
int
buf_read(int fd, struct buf_read *r)
{
	char		*buf = r->buf + r->wpos;
	size_t		 bufsz = sizeof(r->buf) - r->wpos;
	ssize_t		 n;

	if (bufsz == 0) {
		errno = EMSGSIZE;
		return (-1);
	}

	if ((n = read(fd, buf, bufsz)) == -1) {
		if (errno == EAGAIN || errno == EINTR)
			return (-2);
		return (-1);
	} else if (n == 0)
		return (-1);

	r->wpos += n;

	return (0);
}
