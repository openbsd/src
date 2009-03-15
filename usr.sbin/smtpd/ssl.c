/*	$OpenBSD: ssl.c,v 1.11 2009/03/15 19:32:11 gilles Exp $	*/

/*
 * Copyright (c) 2008 Pierre-Yves Ritschard <pyr@openbsd.org>
 * Copyright (c) 2008 Reyk Floeter <reyk@openbsd.org>
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
#include <sys/time.h>

#include <ctype.h>
#include <event.h>
#include <fcntl.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <openssl/ssl.h>
#include <openssl/engine.h>
#include <openssl/err.h>

#include "smtpd.h"

#define SSL_CIPHERS	"HIGH:!ADH"

void	 ssl_error(const char *);
char	*ssl_load_file(const char *, off_t *);
SSL_CTX	*ssl_ctx_create(void);
void	 ssl_session_accept(int, short, void *);
void	 ssl_read(int, short, void *);
void	 ssl_write(int, short, void *);
int	 ssl_bufferevent_add(struct event *, int);
void	 ssl_connect(int, short, void *);
void	 ssl_client_init(struct session *);

extern void	bufferevent_read_pressure_cb(struct evbuffer *, size_t,
		    size_t, void *);

extern struct s_session	s_smtp;

void
ssl_connect(int fd, short event, void *p)
{
	struct session	*s = p;
	int		 ret;
	int		 retry_flag;
	int		 ssl_err;

	if (event == EV_TIMEOUT) {
		log_debug("ssl_session_accept: session timed out");
		session_destroy(s);
		return;
	}

	ret = SSL_connect(s->s_ssl);
	if (ret <= 0) {
		ssl_err = SSL_get_error(s->s_ssl, ret);

		switch (ssl_err) {
		case SSL_ERROR_WANT_READ:
			retry_flag = EV_READ;
			goto retry;
		case SSL_ERROR_WANT_WRITE:
			retry_flag = EV_WRITE;
			goto retry;
		case SSL_ERROR_ZERO_RETURN:
		case SSL_ERROR_SYSCALL:
			if (ret == 0) {
				log_debug("session destroy in MTA #1");
				session_destroy(s);
				return;
			}
			/* FALLTHROUGH */
		default:
			ssl_error("ssl_session_connect");
			session_destroy(s);
			return;
		}
	}

	event_set(&s->s_bev->ev_read, s->s_fd, EV_READ, ssl_read, s->s_bev);
	event_set(&s->s_bev->ev_write, s->s_fd, EV_WRITE, ssl_write, s->s_bev);

	log_info("ssl_connect: connected to remote ssl server");
	bufferevent_enable(s->s_bev, EV_READ|EV_WRITE);
	s->s_flags |= F_SECURE;

	if (s->s_flags & F_PEERHASTLS) {
		session_respond(s, "EHLO %s", s->s_env->sc_hostname);
	}

	return;
retry:
	event_add(&s->s_ev, &s->s_tv);
}

void
ssl_read(int fd, short event, void *p)
{
	struct bufferevent	*bufev = p;
	struct session		*s = bufev->cbarg;
	int			 ret;
	int			 ssl_err;
	short			 what;
	size_t			 len;
	char			 rbuf[READ_BUF_SIZE];
	int			 howmuch = READ_BUF_SIZE;

	what = EVBUFFER_READ;
	ret = ssl_err = 0;

	if (event == EV_TIMEOUT) {
		what |= EVBUFFER_TIMEOUT;
		goto err;
	}

	if (bufev->wm_read.high != 0)
		howmuch = MIN(sizeof(rbuf), bufev->wm_read.high);

	ret = SSL_read(s->s_ssl, rbuf, howmuch);
	if (ret <= 0) {
		ssl_err = SSL_get_error(s->s_ssl, ret);

		switch (ssl_err) {
		case SSL_ERROR_WANT_READ:
			goto retry;
		case SSL_ERROR_WANT_WRITE:
			goto retry;
		default:
			if (ret == 0)
				what |= EVBUFFER_EOF;
			else {
				ssl_error("ssl_read");
				what |= EVBUFFER_ERROR;
			}
			goto err;
		}
	}

	if (evbuffer_add(bufev->input, rbuf, ret) == -1) {
		what |= EVBUFFER_ERROR;
		goto err;
	}

	ssl_bufferevent_add(&bufev->ev_read, bufev->timeout_read);

	len = EVBUFFER_LENGTH(bufev->input);
	if (bufev->wm_read.low != 0 && len < bufev->wm_read.low)
		return;
	if (bufev->wm_read.high != 0 && len > bufev->wm_read.high) {
		struct evbuffer *buf = bufev->input;
		event_del(&bufev->ev_read);
		evbuffer_setcb(buf, bufferevent_read_pressure_cb, bufev);
		return;
	}

	if (bufev->readcb != NULL)
		(*bufev->readcb)(bufev, bufev->cbarg);
	return;

retry:
	ssl_bufferevent_add(&bufev->ev_read, bufev->timeout_read);
	return;

err:
	(*bufev->errorcb)(bufev, what, bufev->cbarg);
}


void
ssl_write(int fd, short event, void *p)
{
	struct bufferevent	*bufev = p;
	struct session		*s = bufev->cbarg;
	int			 ret;
	int			 ssl_err;
	short			 what;

	ret = 0;
	what = EVBUFFER_WRITE;

	if (event == EV_TIMEOUT) {
		what |= EV_TIMEOUT;
		goto err;
	}

	if (EVBUFFER_LENGTH(bufev->output)) {
		if (s->s_buf == NULL) {
			s->s_buflen = EVBUFFER_LENGTH(bufev->output);
			if ((s->s_buf = malloc(s->s_buflen)) == NULL) {
				what |= EVBUFFER_ERROR;
				goto err;
			}
			memcpy(s->s_buf, EVBUFFER_DATA(bufev->output),
			    s->s_buflen);
		}

		ret = SSL_write(s->s_ssl, s->s_buf, s->s_buflen);
		if (ret <= 0) {
			ssl_err = SSL_get_error(s->s_ssl, ret);

			switch (ssl_err) {
			case SSL_ERROR_WANT_READ:
				goto retry;
			case SSL_ERROR_WANT_WRITE:
				goto retry;
			default:
				if (ret == 0)
					what |= EVBUFFER_EOF;
				else {
					ssl_error("ssl_write");
					what |= EVBUFFER_ERROR;
				}
				goto err;
			}
		}
		evbuffer_drain(bufev->output, ret);
	}
	if (s->s_buf != NULL) {
		free(s->s_buf);
		s->s_buf = NULL;
		s->s_buflen = 0;
	}
	if (EVBUFFER_LENGTH(bufev->output) != 0)
		ssl_bufferevent_add(&bufev->ev_write, bufev->timeout_write);

	if (bufev->writecb != NULL &&
	    EVBUFFER_LENGTH(bufev->output) <= bufev->wm_write.low)
		(*bufev->writecb)(bufev, bufev->cbarg);
	return;

retry:
	if (s->s_buflen != 0)
		ssl_bufferevent_add(&bufev->ev_write, bufev->timeout_write);
	return;

err:
	if (s->s_buf != NULL) {
		free(s->s_buf);
		s->s_buf = NULL;
		s->s_buflen = 0;
	}
	(*bufev->errorcb)(bufev, what, bufev->cbarg);
}

int
ssl_bufferevent_add(struct event *ev, int timeout)
{
	struct timeval	 tv;
	struct timeval	*ptv = NULL;

	if (timeout) {
		timerclear(&tv);
		tv.tv_sec = timeout;
		ptv = &tv;
	}

	return (event_add(ev, ptv));
}

int
ssl_cmp(struct ssl *s1, struct ssl *s2)
{
	return (strcmp(s1->ssl_name, s2->ssl_name));
}

SPLAY_GENERATE(ssltree, ssl, ssl_nodes, ssl_cmp);

char *
ssl_load_file(const char *name, off_t *len)
{
	struct stat	 st;
	off_t		 size;
	char		*buf = NULL;
	int		 fd;

	if ((fd = open(name, O_RDONLY)) == -1)
		return (NULL);
	if (fstat(fd, &st) != 0)
		goto fail;
	size = st.st_size;
	if ((buf = calloc(1, size + 1)) == NULL)
		goto fail;
	if (read(fd, buf, size) != size)
		goto fail;
	close(fd);

	*len = size + 1;
	return (buf);

fail:
	if (buf != NULL)
		free(buf);
	close(fd);
	return (NULL);
}

SSL_CTX *
ssl_ctx_create(void)
{
	SSL_CTX	*ctx;

	ctx = SSL_CTX_new(SSLv23_method());
	if (ctx == NULL) {
		ssl_error("ssl_ctx_create");
		fatal("ssl_ctx_create: could not create SSL context");
	}

	SSL_CTX_set_session_cache_mode(ctx, SSL_SESS_CACHE_OFF);
	SSL_CTX_set_timeout(ctx, SMTPD_SESSION_TIMEOUT);
	SSL_CTX_set_options(ctx, SSL_OP_ALL);
	SSL_CTX_set_options(ctx,
	    SSL_OP_NO_SESSION_RESUMPTION_ON_RENEGOTIATION);

	if (!SSL_CTX_set_cipher_list(ctx, SSL_CIPHERS)) {
		ssl_error("ssl_ctx_create");
		fatal("ssl_ctx_create: could not set cipher list");
	}
	return (ctx);
}

int
ssl_load_certfile(struct smtpd *env, const char *name)
{
	struct ssl	*s;
	struct ssl	 key;
	char		 certfile[PATH_MAX];

	if (strlcpy(key.ssl_name, name, sizeof(key.ssl_name))
	    >= sizeof(key.ssl_name)) {
		log_warn("ssl_load_certfile: certificate name truncated");
		return -1;
	}

	s = SPLAY_FIND(ssltree, &env->sc_ssl, &key);
	if (s != NULL)
		return 0;

	if ((s = calloc(1, sizeof(*s))) == NULL)
		fatal(NULL);

	(void)strlcpy(s->ssl_name, key.ssl_name, sizeof(s->ssl_name));

	if (! bsnprintf(certfile, sizeof(certfile),
		"/etc/mail/certs/%s.crt", name)) {
		free(s);
		return (-1);
	}

	if ((s->ssl_cert = ssl_load_file(certfile, &s->ssl_cert_len)) == NULL) {
		free(s);
		return (-1);
	}

	if (! bsnprintf(certfile, sizeof(certfile),
		"/etc/mail/certs/%s.key", name)) {
		free(s->ssl_cert);
		free(s);
		return -1;
	}

	if ((s->ssl_key = ssl_load_file(certfile, &s->ssl_key_len)) == NULL) {
		free(s->ssl_cert);
		free(s);
		return (-1);
	}

	if (s->ssl_cert == NULL || s->ssl_key == NULL)
		fatal("invalid certificates");

	SPLAY_INSERT(ssltree, &env->sc_ssl, s);

	return (0);
}

void
ssl_init(void)
{
	SSL_library_init();
	SSL_load_error_strings();

	OpenSSL_add_all_algorithms();

	/* Init hardware crypto engines. */
	ENGINE_load_builtin_engines();
	ENGINE_register_all_complete();
}

void
ssl_setup(struct smtpd *env, struct listener *l)
{
	struct ssl	key;

	if (!(l->flags & F_SSL))
		return;

	if (strlcpy(key.ssl_name, l->ssl_cert_name, sizeof(key.ssl_name))
	    >= sizeof(key.ssl_name))
		fatal("ssl_setup: certificate name truncated");

	if ((l->ssl = SPLAY_FIND(ssltree, &env->sc_ssl, &key)) == NULL)
		fatal("ssl_setup: certificate tree corrupted");

	l->ssl_ctx = ssl_ctx_create();

	if (!ssl_ctx_use_certificate_chain(l->ssl_ctx,
	    l->ssl->ssl_cert, l->ssl->ssl_cert_len))
		goto err;
	if (!ssl_ctx_use_private_key(l->ssl_ctx,
	    l->ssl->ssl_key, l->ssl->ssl_key_len))
		goto err;

	if (!SSL_CTX_check_private_key(l->ssl_ctx))
		goto err;
	if (!SSL_CTX_set_session_id_context(l->ssl_ctx,
		(const unsigned char *)l->ssl_cert_name, strlen(l->ssl_cert_name) + 1))
		goto err;

	log_debug("ssl_setup: ssl setup finished for listener: %p", l);
	return;

err:
	if (l->ssl_ctx != NULL)
		SSL_CTX_free(l->ssl_ctx);
	ssl_error("ssl_setup");
	fatal("ssl_setup: cannot set SSL up");
	return;
}

void
ssl_error(const char *where)
{
	unsigned long	code;
	char		errbuf[128];
	extern int	debug;

	if (!debug)
		return;
	for (; (code = ERR_get_error()) != 0 ;) {
		ERR_error_string_n(code, errbuf, sizeof(errbuf));
		log_debug("SSL library error: %s: %s", where, errbuf);
	}
}

void
ssl_session_accept(int fd, short event, void *p)
{
	struct session	*s = p;
	int		 ret;
	int		 retry_flag;
	int		 ssl_err;

	if (event == EV_TIMEOUT) {
		log_debug("ssl_session_accept: session timed out");
		session_destroy(s);
		return;
	}

	retry_flag = ssl_err = 0;

	log_debug("ssl_session_accept: accepting client");
	ret = SSL_accept(s->s_ssl);
	if (ret <= 0) {
		ssl_err = SSL_get_error(s->s_ssl, ret);

		switch (ssl_err) {
		case SSL_ERROR_WANT_READ:
			retry_flag = EV_READ;
			goto retry;
		case SSL_ERROR_WANT_WRITE:
			retry_flag = EV_WRITE;
			goto retry;
		case SSL_ERROR_ZERO_RETURN:
		case SSL_ERROR_SYSCALL:
			if (ret == 0) {
				session_destroy(s);
				return;
			}
			/* FALLTHROUGH */
		default:
			ssl_error("ssl_session_accept");
			session_destroy(s);
			return;
		}
	}

	event_set(&s->s_bev->ev_read, s->s_fd, EV_READ, ssl_read, s->s_bev);
	event_set(&s->s_bev->ev_write, s->s_fd, EV_WRITE, ssl_write, s->s_bev);

	log_info("ssl_session_accept: accepted ssl client");
	s->s_flags |= F_SECURE;

	if (s->s_l->flags & F_SSMTP) {
		s_smtp.ssmtp++;
		s_smtp.ssmtp_active++;
	}
	if (s->s_l->flags & F_STARTTLS) {
		s_smtp.starttls++;
		s_smtp.starttls_active++;
	}

	session_pickup(s, NULL);
	return;
retry:
	event_add(&s->s_ev, &s->s_tv);
}

void
ssl_session_init(struct session *s)
{
	struct listener	*l;
	SSL             *ssl;

	l = s->s_l;

	if (!(l->flags & F_SSL))
		return;

	log_debug("ssl_session_init: switching to SSL");
	ssl = SSL_new(l->ssl_ctx);
	if (ssl == NULL)
		goto err;

	if (!SSL_set_ssl_method(ssl, SSLv23_server_method()))
		goto err;
	if (!SSL_set_fd(ssl, s->s_fd))
		goto err;
	SSL_set_accept_state(ssl);

	s->s_ssl = ssl;

	s->s_tv.tv_sec = SMTPD_SESSION_TIMEOUT;
	s->s_tv.tv_usec = 0;
	event_set(&s->s_ev, s->s_fd, EV_READ|EV_TIMEOUT, ssl_session_accept, s);
	event_add(&s->s_ev, &s->s_tv);
	return;

 err:
	if (ssl != NULL)
		SSL_free(ssl);
	ssl_error("ssl_session_init");
}

void
ssl_client_init(struct session *s)
{
	SSL_CTX		*ctx;

	log_debug("ssl_client_init: preparing SSL");
	ctx = ssl_ctx_create();

	s->s_ssl = SSL_new(ctx);
	if (s->s_ssl == NULL)
		goto err;

	if (!SSL_set_ssl_method(s->s_ssl, SSLv23_client_method()))
		goto err;
	if (!SSL_set_fd(s->s_ssl, s->s_fd))
		goto err;
	SSL_set_connect_state(s->s_ssl);

	s->s_tv.tv_sec = SMTPD_SESSION_TIMEOUT;
	s->s_tv.tv_usec = 0;

	event_set(&s->s_ev, s->s_fd, EV_WRITE|EV_TIMEOUT, ssl_connect, s);
	event_add(&s->s_ev, &s->s_tv);
	return;

 err:
	if (s->s_ssl != NULL)
		SSL_free(s->s_ssl);
	ssl_error("ssl_client_init");
}

void
ssl_session_destroy(struct session *s)
{
	SSL_free(s->s_ssl);

	if (s->s_l == NULL) {
		/* called from mta */
		return;
	}

	if (s->s_l->flags & F_SSMTP) {
		if (s->s_flags & F_SECURE)
			s_smtp.ssmtp_active--;
	}
	if (s->s_l->flags & F_STARTTLS) {
		if (s->s_flags & F_SECURE)
			s_smtp.starttls_active--;
	}
}
