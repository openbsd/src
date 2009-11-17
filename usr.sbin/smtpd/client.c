/*	$OpenBSD: client.c,v 1.17 2009/11/17 09:22:19 jacekm Exp $	*/

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

int		 client_next_state(struct smtp_client *);

int		 client_getln(struct smtp_client *);
int		 client_putln(struct smtp_client *, char *, ...);
int		 client_data_add(struct smtp_client *, char *, size_t);

#ifndef CLIENT_NO_SSL
int		 client_ssl_connect(struct smtp_client *);
SSL		*ssl_client_init(int, char *, size_t, char *, size_t);
int		 ssl_buf_read(SSL *, struct buf_read *);
int		 ssl_buf_write(SSL *, struct msgbuf *);
#endif

char		*buf_getln(struct buf_read *);
int		 buf_read(int, struct buf_read *);

/*
 * client_init()
 *
 * Initialize SMTP session.
 */
struct smtp_client *
client_init(int fd, char *ehlo)
{
	struct smtp_client	*sp = NULL;
	int			 rv = -1;

	if ((sp = calloc(1, sizeof(*sp))) == NULL)
		goto done;
	if (ehlo == NULL || *ehlo == '\0') {
		char			 buf[NI_MAXHOST];
		struct sockaddr_storage	 sa;
		socklen_t		 len;

		len = sizeof(sa);
		if (getsockname(fd, (struct sockaddr *)&sa, &len))
			goto done;
		if (getnameinfo((struct sockaddr *)&sa, len, buf, sizeof(buf),
		    NULL, 0, NI_NUMERICHOST))
			goto done;
		if (asprintf(&sp->ehlo, "[%s]", buf) == -1)
			goto done;
	} else if ((sp->ehlo = strdup(ehlo)) == NULL)
		goto done;
	if ((sp->sender = strdup("")) == NULL)
		goto done;
	if ((sp->data = buf_dynamic(0, SIZE_T_MAX)) == NULL)
		goto done;
	sp->state = CLIENT_INIT;
	msgbuf_init(&sp->w);
	sp->w.fd = fd;
	TAILQ_INIT(&sp->recipients);

#ifndef CLIENT_NO_SSL
	sp->exts[CLIENT_EXT_STARTTLS].want = 1;
	sp->exts[CLIENT_EXT_STARTTLS].must = 1;
	sp->exts[CLIENT_EXT_STARTTLS].state = CLIENT_STARTTLS;
	sp->exts[CLIENT_EXT_STARTTLS].name = "STARTTLS";
#endif

	sp->exts[CLIENT_EXT_AUTH].want = 0;
	sp->exts[CLIENT_EXT_AUTH].must = 0;
	sp->exts[CLIENT_EXT_AUTH].state = CLIENT_AUTH;
	sp->exts[CLIENT_EXT_AUTH].name = "AUTH";

	rv = 0;
done:
	if (rv && sp != NULL) {
		free(sp->ehlo);
		free(sp->sender);
		if (sp->data)
			buf_free(sp->data);
		free(sp);
		sp = NULL;
	}
	return (sp);
}

/*
 * client_verbose()
 *
 * Enable logging of SMTP commands.
 */
void
client_verbose(struct smtp_client *sp, FILE *fp)
{
	sp->verbose = fp;
}

/*
 * client_ssl_smtps()
 *
 * Request that connection be secured using SSL from the start.
 */
int
client_ssl_smtps(struct smtp_client *sp)
{
	/* check if too late */
	if (sp->state != CLIENT_INIT)
		return (-1);

#ifndef CLIENT_NO_SSL
	sp->state = CLIENT_SSL_INIT;
	sp->exts[CLIENT_EXT_STARTTLS].want = 0;
	sp->exts[CLIENT_EXT_STARTTLS].must = 0;

	return (0);
#else
	return (-1);
#endif
}

/*
 * client_ssl_optional()
 *
 * Allow session to progress in plaintext if STARTTLS fails.
 */
int
client_ssl_optional(struct smtp_client *sp)
{
	if (sp->state != CLIENT_INIT)
		return (-1);

#ifndef CLIENT_NO_SSL
	sp->exts[CLIENT_EXT_STARTTLS].must = 0;

	return (0);
#else
	return (-1);
#endif
}

/*
 * client_certificate()
 *
 * Use the provided certificate during SSL handshake.
 */
int
client_certificate(struct smtp_client *sp, char *cert, size_t certsz,
    char *key, size_t keysz)
{
	int	 rv = -1;

	if (sp->state != CLIENT_INIT && sp->state != CLIENT_SSL_INIT)
		goto done;

	if ((sp->auth.cert = malloc(certsz)) == NULL)
		goto done;
	if ((sp->auth.key = malloc(keysz)) == NULL)
		goto done;
	memcpy(sp->auth.cert, cert, certsz);
	memcpy(sp->auth.key, key, keysz);
	sp->auth.certsz = certsz;
	sp->auth.keysz = keysz;

	rv = 0;
done:
	if (rv) {
		free(sp->auth.cert);
		free(sp->auth.key);
		sp->auth.cert = NULL;
		sp->auth.certsz = 0;
		sp->auth.key = NULL;
		sp->auth.keysz = 0;
	}
	return (rv);
}

/*
 * client_auth()
 *
 * Use the AUTH extension.
 */
int
client_auth(struct smtp_client *sp, char *secret)
{
	int	 rv = -1;

	if (sp->state != CLIENT_INIT && sp->state != CLIENT_SSL_INIT)
		goto done;

	if ((sp->auth.plain = strdup(secret)) == NULL)
		goto done;

	sp->exts[CLIENT_EXT_AUTH].want = 1;
	sp->exts[CLIENT_EXT_AUTH].must = 1;

	rv = 0;
done:
	if (rv) {
		free(sp->auth.plain);
		sp->auth.plain = NULL;
	}
	return (rv);
}

/*
 * client_sender()
 *
 * Set envelope sender.  If not called, the sender is assumed to be "<>".
 */
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
	case CLIENT_SSL_INIT:
	case CLIENT_EHLO:
	case CLIENT_HELO:
		break;
	default:
		goto done;
	}

	if (vasprintf(&mbox, fmt, ap) == -1)
		goto done;
	free(sp->sender);
	sp->sender = mbox;

	rv = 0;
done:
	va_end(ap);
	if (rv)
		free(mbox);
	return (rv);
}

/*
 * client_rcpt()
 *
 * Add mail recipient.
 */
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
	case CLIENT_SSL_INIT:
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
	if (rv) {
		if (rp)
			free(rp->mbox);
		free(rp);
	}
	return (rv);
}

/*
 * client_data_fd()
 *
 * Append file referenced by fd to the data buffer.
 */
int
client_data_fd(struct smtp_client *sp, int fd)
{
	struct stat	 sb;
	char		*map = NULL;
	int		 rv = -1;

	if (fstat(fd, &sb) == -1)
		goto done;
	if ((size_t)sb.st_size > SIZE_T_MAX)
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
	if (map)
		munmap(map, sb.st_size);
	return (rv);
}

/*
 * client_data_printf()
 *
 * Append string to the data buffer.
 */
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

/*
 * client_udata_set()
 *
 * Associate user pointer with the most recently recorded recipient.
 */
void
client_udata_set(struct smtp_client *sp, void *p)
{
	struct rcpt *lastrcpt;

	lastrcpt = TAILQ_LAST(&sp->recipients, rlist);
	lastrcpt->udata = p;
}

/*
 * client_udata_get()
 *
 * Return user pointer associated with most recently sent recipient.
 */
void *
client_udata_get(struct smtp_client *sp)
{
	return (sp->rcptsent->udata);
}

/*
 * client_read()
 *
 * Handler to be called when socket becomes readable.
 */
int
client_read(struct smtp_client *sp)
{
	int	 rv = CLIENT_ERROR;

#ifndef CLIENT_NO_SSL
	if (sp->state == CLIENT_SSL_CONNECT)
		return client_ssl_connect(sp);
#endif

	/* read data from the socket */
#ifndef CLIENT_NO_SSL
	if (sp->ssl_state) {
		switch (ssl_buf_read(sp->ssl_state, &sp->r)) {
		case SSL_ERROR_NONE:
			break;

		case SSL_ERROR_WANT_READ:
			return (CLIENT_WANT_READ);

		case SSL_ERROR_WANT_WRITE:
			return (CLIENT_WANT_WRITE);

		default:
			strlcpy(sp->ebuf, "130 ssl_buf_read error",
			    sizeof(sp->ebuf));
			return (CLIENT_ERROR);
		}
#else
	if (0) {
#endif
	} else {
		errno = 0;
		if (buf_read(sp->w.fd, &sp->r) == -1) {
			if (errno)
				snprintf(sp->ebuf, sizeof(sp->ebuf),
				    "130 buf_read: %s", strerror(errno));
			else
				snprintf(sp->ebuf, sizeof(sp->ebuf),
				    "130 buf_read: connection closed");
			return (CLIENT_ERROR);
		}
	}

	/* get server reply */
	if (client_getln(sp) < 0)
		return (CLIENT_ERROR);
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
		memcpy(sp->reply, "190", 3);
		goto done;
	}

	switch (sp->state) {
	case CLIENT_INIT:
		if (*sp->reply != '2')
			goto done;
		else
			sp->state = CLIENT_EHLO;
		break;

	case CLIENT_EHLO:
		if (*sp->reply != '2') {
			if (sp->exts[CLIENT_EXT_STARTTLS].must ||
			    sp->exts[CLIENT_EXT_AUTH].must)
				goto done;
			else
				sp->state = CLIENT_HELO;
			break;
		}
	
		if ((sp->state = client_next_state(sp)) == 0)
			return (CLIENT_ERROR);
		break;

	case CLIENT_HELO:
		if (*sp->reply != '2')
			goto done;
		else
			sp->state = CLIENT_MAILFROM;
		break;

	case CLIENT_STARTTLS:
		if (*sp->reply != '2') {
			sp->exts[CLIENT_EXT_STARTTLS].fail = 1;
			if ((sp->state = client_next_state(sp)) == 0)
				return (CLIENT_ERROR);
		} else
			sp->state = CLIENT_SSL_INIT;
		break;

	case CLIENT_AUTH:
		if (*sp->reply != '2')
			sp->exts[CLIENT_EXT_AUTH].fail = 1;
		else
			sp->exts[CLIENT_EXT_AUTH].done = 1;

		if ((sp->state = client_next_state(sp)) == 0)
			return (CLIENT_ERROR);
		break;

	case CLIENT_MAILFROM:
		if (*sp->reply != '2')
			goto done;
		else
			sp->state = CLIENT_RCPTTO;
		break;

	case CLIENT_RCPTTO:
		if (*sp->reply != '2') {
			rv = CLIENT_RCPT_FAIL;
			goto done;
		} else if (TAILQ_NEXT(sp->rcptsent, entry) == NULL)
			sp->state = CLIENT_DATA;
		break;

	case CLIENT_DATA:
		if (*sp->reply != '3')
			goto done;
		else
			sp->state = CLIENT_DATA_BODY;
		break;

	case CLIENT_DATA_BODY:
		if (*sp->reply == '2')
			rv = CLIENT_DONE;
		goto done;

	default:
		abort();
	}

	rv = CLIENT_WANT_WRITE;
done:
	if (rv == CLIENT_ERROR)
		strlcpy(sp->ebuf, sp->reply, sizeof(sp->ebuf));
	return (rv);
}

/*
 * client_write()
 *
 * Handler to be called when socket becomes writable.
 */
int
client_write(struct smtp_client *sp)
{
	int	 rv = CLIENT_ERROR;

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
		if (sp->verbose)
			fprintf(sp->verbose, "client: ssl handshake started\n");
		sp->ssl_state = ssl_client_init(sp->w.fd,
		    sp->auth.cert, sp->auth.certsz,
		    sp->auth.key, sp->auth.keysz);
		if (sp->ssl_state == NULL) {
			strlcpy(sp->ebuf, "130 SSL init failed",
			    sizeof(sp->ebuf));
			return (CLIENT_ERROR);
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

		if (client_putln(sp, "%sLO %s",
		    sp->state == CLIENT_EHLO ? "EH" : "HE", sp->ehlo) < 0)
			goto done;
		break;

	case CLIENT_AUTH:
		if (client_putln(sp, "AUTH PLAIN %s", sp->auth.plain) < 0)
			goto done;
		break;

	case CLIENT_STARTTLS:
		if (client_putln(sp, "STARTTLS") < 0)
			goto done;
		break;

	case CLIENT_MAILFROM:
		if (client_putln(sp, "MAIL FROM:<%s>", sp->sender) < 0)
			goto done;
		break;

	case CLIENT_RCPTTO:
		if (sp->rcptsent == NULL)
			sp->rcptsent = TAILQ_FIRST(&sp->recipients);
		else
			sp->rcptsent = TAILQ_NEXT(sp->rcptsent, entry);

		if (sp->rcptsent == NULL)
			goto done;

		if (client_putln(sp, "RCPT TO:<%s>", sp->rcptsent->mbox) < 0)
			goto done;
		break;

	case CLIENT_DATA:
		if (client_putln(sp, "DATA") < 0)
			goto done;
		break;

	case CLIENT_DATA_BODY:
		if (buf_add(sp->data, ".\r\n", 3) < 0) {
			strlcpy(sp->ebuf, "190 buf_add error",
			    sizeof(sp->ebuf));
			goto done;
		}
		buf_close(&sp->w, sp->data);
		sp->data = NULL;
		break;

	default:
		abort();
	}

write:
#ifndef CLIENT_NO_SSL
	if (sp->ssl_state) {
		switch (ssl_buf_write(sp->ssl_state, &sp->w)) {
		case SSL_ERROR_NONE:
			break;

		case SSL_ERROR_WANT_READ:
			rv = CLIENT_WANT_READ;
			goto done;

		case SSL_ERROR_WANT_WRITE:
			rv = CLIENT_WANT_WRITE;
			goto done;

		default:
			strlcpy(sp->ebuf, "130 ssl_buf_write error",
			    sizeof(sp->ebuf));
			goto done;
		}
#else
	if (0) {
#endif
	} else {
		if (buf_write(&sp->w) < 0) {
			strlcpy(sp->ebuf, "130 buf_write error",
			    sizeof(sp->ebuf));
			goto done;
		}
	}

	rv = sp->w.queued ? CLIENT_WANT_WRITE : CLIENT_WANT_READ;
done:
	return (rv);
}

#ifndef CLIENT_NO_SSL
/*
 * client_ssl_connect()
 *
 * Progress SSL handshake.
 */
int
client_ssl_connect(struct smtp_client *sp)
{
	int	 ret;

	ret = SSL_connect(sp->ssl_state);

	switch (SSL_get_error(sp->ssl_state, ret)) {
	case SSL_ERROR_NONE:
		break;

	case SSL_ERROR_WANT_READ:
		return (CLIENT_WANT_READ);

	case SSL_ERROR_WANT_WRITE:
		return (CLIENT_WANT_WRITE);

	default:
		if (sp->verbose)
			fprintf(sp->verbose, "client: ssl handshake failed\n");

		if (sp->exts[CLIENT_EXT_STARTTLS].want) {
			sp->exts[CLIENT_EXT_STARTTLS].fail = 1;

			SSL_free(sp->ssl_state);
			sp->ssl_state = NULL;

			if ((sp->state = client_next_state(sp)) == 0)
				return (CLIENT_ERROR);
			else
				return (CLIENT_WANT_WRITE);
		} else {
			strlcpy(sp->ebuf, "130 SSL_connect error", sizeof(sp->ebuf));
			return (CLIENT_ERROR);
		}
	}

	if (sp->verbose)
		fprintf(sp->verbose, "client: ssl handshake completed\n");

	if (sp->exts[CLIENT_EXT_STARTTLS].want)
		sp->state = CLIENT_EHLO;
	else
		sp->state = CLIENT_INIT;

	sp->exts[CLIENT_EXT_STARTTLS].done = 1;

	return (CLIENT_WANT_WRITE);
}
#endif

/*
 * client_strerror()
 *
 * Access error string explaining most recent client_{read,write} failure.
 */
char *
client_strerror(struct smtp_client *sp)
{
	if (sp->ebuf[0] == '\0')
		return (NULL);
	else
		return (sp->ebuf);
}

/*
 * client_reply()
 *
 * Access string containing most recent server reply.
 */
char *
client_reply(struct smtp_client *sp)
{
	return (sp->reply);
}

/*
 * client_close()
 *
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
	if (sp->data)
		buf_free(sp->data);
	msgbuf_clear(&sp->w);
	while ((rp = TAILQ_FIRST(&sp->recipients))) {
		TAILQ_REMOVE(&sp->recipients, rp, entry);
		free(rp->mbox);
		free(rp);
	}
#ifndef CLIENT_NO_SSL
	if (sp->ssl_state)
		SSL_free(sp->ssl_state);
#endif
	close(sp->w.fd);
	free(sp);
}

/*
 * client_next_state()
 *
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
				snprintf(sp->ebuf, sizeof(sp->ebuf),
				     "%s %s", e->name,
				     e->fail ? "failed" : "not available");
				return (0);
			}
		}
	}

	return (CLIENT_MAILFROM);
}

/*
 * client_timeout()
 *
 * Return a timeout that applies to the current session state.
 */
struct timeval *
client_timeout(struct smtp_client *sp)
{
	switch (sp->state) {
	case CLIENT_DATA:
		sp->timeout.tv_sec = 120;
		break;

	case CLIENT_DATA_BODY:
		if (sp->w.queued)
			sp->timeout.tv_sec = 180;
		else
			sp->timeout.tv_sec = 600;
		break;

	default:
		sp->timeout.tv_sec = 300;
	}

	sp->timeout.tv_usec = 0;

	return (&sp->timeout);
}

/*
 * client_getln()
 *
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

		if (sp->verbose)
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
		strlcpy(sp->ebuf, cause, sizeof(sp->ebuf));
	free(ln);
	return (rv);
}

/*
 * client_putln()
 *
 * Add a line to the output buffer.
 */
int
client_putln(struct smtp_client *sp, char *fmt, ...)
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
	if (rv)
		snprintf(sp->ebuf, sizeof(sp->ebuf), "190 %s", strerror(errno));
	va_end(ap);
	free(p);
	if (cmd)
		buf_free(cmd);
	return (rv);
}

/*
 * client_data_add()
 *
 * Append buffer to the data buffer, performing necessary transformations.
 */
int
client_data_add(struct smtp_client *sp, char *buf, size_t len)
{
	char	*ln;

	/* check if too late */
	switch (sp->state) {
	case CLIENT_INIT:
	case CLIENT_SSL_INIT:
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

/*
 * buf_getln()
 *
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
 * buf_read()
 *
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
