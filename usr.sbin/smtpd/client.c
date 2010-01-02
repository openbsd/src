/*	$OpenBSD: client.c,v 1.25 2010/01/02 13:42:42 jacekm Exp $	*/

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

struct client_cmd *cmd_new(int, char *, ...);
void		 cmd_free(struct client_cmd *);
int		 client_read(struct smtp_client *);
void		 client_get_reply(struct smtp_client *, struct client_cmd *,
		     int *);
int		 client_write(struct smtp_client *);
int		 client_use_extensions(struct smtp_client *);
void		 client_status(struct smtp_client *, char *, ...);
int		 client_getln(struct smtp_client *, int);
void		 client_putln(struct smtp_client *, char *, ...);
struct buf	*client_content_read(FILE *, size_t);
int		 client_poll(struct smtp_client *);
void		 client_quit(struct smtp_client *);

int		 client_socket_read(struct smtp_client *);
int		 client_socket_write(struct smtp_client *);

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
	struct client_cmd	*c;

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
	if (verbose)
		sp->verbose = stdout;
	else if ((sp->verbose = fopen("/dev/null", "a")) == NULL)
		fatal("client_init: fopen");
	if ((sp->body = fdopen(body, "r")) == NULL)
		fatal("client_init: fdopen");
	sp->timeout.tv_sec = 300;
	msgbuf_init(&sp->w);
	sp->w.fd = fd;
	sp->sndlowat = -1;

	sp->exts[CLIENT_EXT_STARTTLS].want = 1;
	sp->exts[CLIENT_EXT_STARTTLS].must = 1;
#ifdef CLIENT_NO_SSL
	sp->exts[CLIENT_EXT_STARTTLS].want = 0;
	sp->exts[CLIENT_EXT_STARTTLS].must = 0;
#endif
	sp->exts[CLIENT_EXT_STARTTLS].name = "STARTTLS";

	sp->exts[CLIENT_EXT_AUTH].want = 0;
	sp->exts[CLIENT_EXT_AUTH].must = 0;
	sp->exts[CLIENT_EXT_AUTH].name = "AUTH";

	sp->exts[CLIENT_EXT_PIPELINING].want = 1;
	sp->exts[CLIENT_EXT_PIPELINING].must = 0;
	sp->exts[CLIENT_EXT_PIPELINING].name = "PIPELINING";

	TAILQ_INIT(&sp->cmdsendq);
	TAILQ_INIT(&sp->cmdrecvq);
	sp->cmdi = 1;
	sp->cmdw = 1;

	c = cmd_new(CLIENT_BANNER, "<banner>");
	TAILQ_INSERT_HEAD(&sp->cmdrecvq, c, entry);

	c = cmd_new(CLIENT_EHLO, "EHLO %s", sp->ehlo);
	TAILQ_INSERT_TAIL(&sp->cmdsendq, c, entry);

	return (sp);
}

/*
 * Create SMTP command.
 */
struct client_cmd *
cmd_new(int type, char *fmt, ...)
{
	struct client_cmd	*cmd;
	va_list			 ap;

	va_start(ap, fmt);
	if ((cmd = calloc(1, sizeof(*cmd))) == NULL)
		fatal(NULL);
	cmd->type = type;
	if (vasprintf(&cmd->action, fmt, ap) == -1)
		fatal(NULL);
	va_end(ap);
	return (cmd);
}

void
cmd_free(struct client_cmd *cmd)
{
	free(cmd->action);
	free(cmd);
}

/*
 * Request that connection be secured using SSL from the start.
 */
void
client_ssl_smtps(struct smtp_client *sp)
{
	sp->ssl_handshake = 1;
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
 * Set envelope sender.
 */
void
client_sender(struct smtp_client *sp, char *fmt, ...)
{
	struct client_cmd	*c;
	char			*s;
	va_list			 ap;

	va_start(ap, fmt);
	if (vasprintf(&s, fmt, ap) == -1)
		fatal("client_sender: vasprintf");
	va_end(ap);
	c = cmd_new(CLIENT_MAILFROM, "MAIL FROM:<%s>", s);
	TAILQ_INSERT_TAIL(&sp->cmdsendq, c, entry);
	free(s);
}

/*
 * Add envelope recipient.
 */
void
client_rcpt(struct smtp_client *sp, void *data, char *fmt, ...)
{
	struct client_cmd	*c;
	char			*r;
	va_list			 ap;

	va_start(ap, fmt);
	if (vasprintf(&r, fmt, ap) == -1)
		fatal("client_rcpt: vasprintf");
	va_end(ap);
	c = cmd_new(CLIENT_RCPTTO, "RCPT TO:<%s>", r);
	c->data = data;
	TAILQ_INSERT_TAIL(&sp->cmdsendq, c, entry);
	free(r);
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
client_talk(struct smtp_client *sp, int writable)
{
	struct client_cmd	*c;
	socklen_t		 len;

	/* first call -> complete the initialisation */
	if (sp->sndlowat == -1) {
		len = sizeof(sp->sndlowat);
		if (getsockopt(sp->w.fd, SOL_SOCKET, SO_SNDLOWAT,
		    &sp->sndlowat, &len) == -1)
			fatal("client_talk: getsockopt");

		c = cmd_new(CLIENT_DATA, "DATA");
		TAILQ_INSERT_TAIL(&sp->cmdsendq, c, entry);

		c = cmd_new(CLIENT_DOT, ".");
		TAILQ_INSERT_TAIL(&sp->cmdsendq, c, entry);

		c = cmd_new(CLIENT_QUIT, "QUIT");
		TAILQ_INSERT_TAIL(&sp->cmdsendq, c, entry);

		/* prepare for the banner */
		writable = 0;
	}

#ifndef CLIENT_NO_SSL
	/* transition to ssl requested? */
	if (sp->ssl_handshake) {
		if (sp->ssl == NULL) {
			log_debug("client: ssl handshake started");
			sp->ssl = ssl_client_init(sp->w.fd,
			    sp->auth.cert, sp->auth.certsz,
			    sp->auth.key, sp->auth.keysz);
			if (sp->ssl == NULL) {
				client_status(sp, "130 SSL init failed");
				return (CLIENT_DONE);
			}
			return (CLIENT_WANT_WRITE);
		} else
			return client_ssl_connect(sp);
	}
#endif

	/* regular handlers */
	return (writable ? client_write(sp) : client_read(sp));
}

/*
 * Handler to be called when socket becomes readable.
 */
int
client_read(struct smtp_client *sp)
{
	struct client_cmd	*cmd;
	int			 ret;

	if ((ret = client_socket_read(sp)))
		return (ret);

	while ((cmd = TAILQ_FIRST(&sp->cmdrecvq))) {
		if (client_getln(sp, cmd->type) < 0)
			return (CLIENT_DONE);
		if (*sp->reply == '\0')
			return client_poll(sp);

		/* reply fully received */
		TAILQ_REMOVE(&sp->cmdrecvq, cmd, entry);
		sp->cmdi--;

		/* if dying, ignore all replies as we wait for an EOF. */
		if (!sp->dying)
			client_get_reply(sp, cmd, &ret);

		cmd_free(cmd);

		/* handle custom return code, e.g. CLIENT_RCPT_FAIL */
		if (ret)
			return (ret);
	}

	return client_poll(sp);
}

/*
 * Parse reply to previously sent command.
 */
void
client_get_reply(struct smtp_client *sp, struct client_cmd *cmd, int *ret)
{
	switch (cmd->type) {
	case CLIENT_BANNER:
	case CLIENT_HELO:
	case CLIENT_MAILFROM:
		if (*sp->reply != '2') {
			client_status(sp, "%s", sp->reply);
			client_quit(sp);
		}
		return;

	case CLIENT_EHLO:
		if (*sp->reply != '2') {
			if (sp->exts[CLIENT_EXT_STARTTLS].must ||
			    sp->exts[CLIENT_EXT_AUTH].must) {
				client_status(sp, "%s", sp->reply);
				client_quit(sp);
			} else {
				cmd = cmd_new(CLIENT_HELO, "HELO %s", sp->ehlo);
				TAILQ_INSERT_HEAD(&sp->cmdsendq, cmd, entry);
			}
			return;
		}

		if (client_use_extensions(sp) < 0)
			client_quit(sp);
		return;

	case CLIENT_STARTTLS:
		if (*sp->reply != '2') {
			sp->exts[CLIENT_EXT_STARTTLS].fail = 1;
			if (client_use_extensions(sp) < 0)
				client_quit(sp);
		} else
			sp->ssl_handshake = 1;
		return;

	case CLIENT_AUTH:
		if (*sp->reply != '2')
			sp->exts[CLIENT_EXT_AUTH].fail = 1;
		else
			sp->exts[CLIENT_EXT_AUTH].done = 1;

		if (client_use_extensions(sp) < 0)
			client_quit(sp);
		return;

	case CLIENT_RCPTTO:
		if (*sp->reply == '2')
			sp->rcptokay++;
		else {
			sp->rcptfail = cmd->data;
			*ret = CLIENT_RCPT_FAIL;
		}
		return;

	case CLIENT_DATA:
		if (*sp->reply != '3') {
			client_status(sp, "%s", sp->reply);
			client_quit(sp);
		} else if (sp->rcptokay > 0) {
			sp->content = sp->head;
			sp->head = NULL;
			if (sp->content == NULL)
				sp->content = client_content_read(sp->body,
				    sp->sndlowat);
		} else {
			/*
			 * Leaving content pointer at NULL will make us proceed
			 * straight to "." as required by RFC 2920.
			 */
			client_status(sp, "600 all recipients refused");
		}
		return;

	case CLIENT_DOT:
		client_status(sp, "%s", sp->reply);
		client_quit(sp);
		return;

	default:
		fatalx("client_get_reply: unexpected type");
	}
}

/*
 * Handler to be called when socket becomes writable.
 */
int
client_write(struct smtp_client *sp)
{
	struct client_cmd	*cmd;
	int			 ret;

	if (sp->content) {
		buf_close(&sp->w, sp->content);
		sp->content = client_content_read(sp->body, sp->sndlowat);
	} else {
		while (sp->cmdi < sp->cmdw) {
			if ((cmd = TAILQ_FIRST(&sp->cmdsendq)) == NULL)
				fatalx("client_write: empty sendq");
			TAILQ_REMOVE(&sp->cmdsendq, cmd, entry);
			TAILQ_INSERT_TAIL(&sp->cmdrecvq, cmd, entry);
			client_putln(sp, "%s", cmd->action);
			sp->cmdi++;

			if (cmd->type == CLIENT_EHLO || cmd->type == CLIENT_HELO){
				sp->exts[CLIENT_EXT_STARTTLS].have = 0;
				sp->exts[CLIENT_EXT_STARTTLS].fail = 0;
				sp->exts[CLIENT_EXT_AUTH].have = 0;
				sp->exts[CLIENT_EXT_AUTH].fail = 0;
				sp->exts[CLIENT_EXT_PIPELINING].have = 0;
				sp->exts[CLIENT_EXT_PIPELINING].fail = 0;
			}

			if (cmd->type == CLIENT_DATA) {
				sp->timeout.tv_sec = 180;
				sp->cmdw = 1; /* stop pipelining */
			}

			if (cmd->type == CLIENT_DOT)
				sp->timeout.tv_sec = 600;

			if (cmd->type == CLIENT_QUIT) {
				sp->timeout.tv_sec = 300;
				sp->cmdw = 0; /* stop all output */
			}
		}
	}

	if ((ret = client_socket_write(sp)))
		return (ret);

	return client_poll(sp);
}

#ifndef CLIENT_NO_SSL
/*
 * Progress SSL handshake.
 */
int
client_ssl_connect(struct smtp_client *sp)
{
	struct client_cmd	*c;
	int			 ret;

	ret = SSL_connect(sp->ssl);

	switch (SSL_get_error(sp->ssl, ret)) {
	case SSL_ERROR_WANT_READ:
		return (CLIENT_STOP_WRITE);

	case SSL_ERROR_WANT_WRITE:
		return (CLIENT_WANT_WRITE);

	case SSL_ERROR_NONE:
		sp->ssl_handshake = 0;
		break;

	default:
		log_debug("client: ssl handshake failed");
		sp->ssl_handshake = 0;

		if (sp->exts[CLIENT_EXT_STARTTLS].want) {
			sp->exts[CLIENT_EXT_STARTTLS].fail = 1;
			SSL_free(sp->ssl);
			sp->ssl = NULL;
			if (client_use_extensions(sp) == 0)
				return client_poll(sp);
		} else
			client_status(sp, "130 SSL_connect error");

		return (CLIENT_DONE);
	}

	log_debug("client: ssl handshake completed");

	if (sp->exts[CLIENT_EXT_STARTTLS].want) {
		c = cmd_new(CLIENT_EHLO, "EHLO %s", sp->ehlo);
		TAILQ_INSERT_HEAD(&sp->cmdsendq, c, entry);
	}

	sp->exts[CLIENT_EXT_STARTTLS].done = 1;

	return client_poll(sp);
}
#endif

/*
 * Deinitialization routine.
 */
void
client_close(struct smtp_client *sp)
{
	struct client_cmd	*cmd;

	free(sp->ehlo);
	free(sp->auth.plain);
	free(sp->auth.cert);
	free(sp->auth.key);
	if (sp->head)
		buf_free(sp->head);
	if (sp->content)
		buf_free(sp->content);
	msgbuf_clear(&sp->w);
	while ((cmd = TAILQ_FIRST(&sp->cmdsendq))) {
		TAILQ_REMOVE(&sp->cmdsendq, cmd, entry);
		cmd_free(cmd);
	}
	while ((cmd = TAILQ_FIRST(&sp->cmdrecvq))) {
		TAILQ_REMOVE(&sp->cmdrecvq, cmd, entry);
		cmd_free(cmd);
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
client_use_extensions(struct smtp_client *sp)
{
	struct client_ext	*e;
	struct client_cmd	*c;
	size_t			 i;

	for (i = 0; i < nitems(sp->exts); i++) {
		e = &sp->exts[i];
		if (!e->want || e->done)
			continue;
		if (e->have && !e->fail) {
			if (i == CLIENT_EXT_STARTTLS) {
				c = cmd_new(CLIENT_STARTTLS, "STARTTLS");
				TAILQ_INSERT_HEAD(&sp->cmdsendq, c, entry);
				break;
			}
			if (i == CLIENT_EXT_AUTH) {
				c = cmd_new(CLIENT_AUTH, "AUTH PLAIN %s",
				    sp->auth.plain);
				TAILQ_INSERT_HEAD(&sp->cmdsendq, c, entry);
				break;
			}
			if (i == CLIENT_EXT_PIPELINING) {
				sp->cmdw = SIZE_T_MAX;
				sp->exts[i].done = 1;
				continue;
			}
			fatalx("client_use_extensions: invalid extension");
		} else if (e->must) {
			client_status(sp, "600 %s %s", e->name,
			     e->fail ? "failed" : "not available");
			return (-1);
		}
	}

	return (0);
}

/*
 * Update status field which the caller uses to check if any errors were
 * encountered.
 */
void
client_status(struct smtp_client *sp, char *fmt, ...)
{
	va_list ap;

	if (sp->dying)
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
client_getln(struct smtp_client *sp, int type)
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

		if (type == CLIENT_EHLO) {
			if (strcmp(ln + 4, "STARTTLS") == 0)
				sp->exts[CLIENT_EXT_STARTTLS].have = 1;
			else if (strncmp(ln + 4, "AUTH", 4) == 0)
				sp->exts[CLIENT_EXT_AUTH].have = 1;
			else if (strcmp(ln + 4, "PIPELINING") == 0)
				sp->exts[CLIENT_EXT_PIPELINING].have = 1;
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
struct buf *
client_content_read(FILE *fp, size_t max)
{
	struct buf	*b;
	char		*ln;
	size_t		 len;

	if ((b = buf_dynamic(0, SIZE_T_MAX)) == NULL)
		fatal(NULL);

	while (buf_size(b) < max) {
		if ((ln = fgetln(fp, &len)) == NULL)
			break;
		if (ln[len - 1] == '\n')
			len--;
		if (*ln == '.' && buf_add(b, ".", 1))
			fatal(NULL);
		if (buf_add(b, ln, len))
			fatal(NULL);
		if (buf_add(b, "\r\n", 2))
			fatal(NULL);
	}
	if (ferror(fp))
		fatal("client_body: fgetln");
	if (feof(fp) && buf_size(b) == 0) {
		buf_free(b);
		b = NULL;
	}

	return (b);
}

/*
 * Inform the caller what kind of polling should be done.
 */
int
client_poll(struct smtp_client *sp)
{
	if (sp->cmdi < sp->cmdw || sp->w.queued)
		return (CLIENT_WANT_WRITE);
	else
		return (CLIENT_STOP_WRITE);
}

/*
 * Move to dying stage.
 */
void
client_quit(struct smtp_client *sp)
{
	struct client_cmd *cmd;

	while ((cmd = TAILQ_FIRST(&sp->cmdsendq))) {
		if (cmd->type == CLIENT_QUIT)
			break;
		TAILQ_REMOVE(&sp->cmdsendq, cmd, entry);
		cmd_free(cmd);
	}
	sp->dying = 1; 
}

/*
 * Receive data from socket to internal buffer.
 */
int
client_socket_read(struct smtp_client *sp)
{
#ifndef CLIENT_NO_SSL
	if (sp->ssl) {
		switch (ssl_buf_read(sp->ssl, &sp->r)) {
		case SSL_ERROR_NONE:
			break;
		case SSL_ERROR_WANT_READ:
			return (CLIENT_STOP_WRITE);
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
	return (0);
}

/*
 * Send data to socket from the msgbuf.
 */
int
client_socket_write(struct smtp_client *sp)
{
#ifndef CLIENT_NO_SSL
	if (sp->ssl) {
		switch (ssl_buf_write(sp->ssl, &sp->w)) {
		case SSL_ERROR_NONE:
			break;
		case SSL_ERROR_WANT_READ:
			return (CLIENT_STOP_WRITE);
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

	return (0);
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
