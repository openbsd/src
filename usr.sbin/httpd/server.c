/*	$OpenBSD: server.c,v 1.48 2014/12/12 14:45:59 reyk Exp $	*/

/*
 * Copyright (c) 2006 - 2014 Reyk Floeter <reyk@openbsd.org>
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
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/uio.h>
#include <sys/tree.h>

#include <net/if.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <stdio.h>
#include <err.h>
#include <pwd.h>
#include <event.h>
#include <fnmatch.h>
#include <tls.h>

#include "httpd.h"

int		 server_dispatch_parent(int, struct privsep_proc *,
		    struct imsg *);
int		 server_dispatch_logger(int, struct privsep_proc *,
		    struct imsg *);
void		 server_shutdown(void);

void		 server_init(struct privsep *, struct privsep_proc *p, void *);
void		 server_launch(void);
int		 server_socket(struct sockaddr_storage *, in_port_t,
		    struct server_config *, int, int);
int		 server_socket_listen(struct sockaddr_storage *, in_port_t,
		    struct server_config *);

int		 server_tls_init(struct server *);
void		 server_tls_readcb(int, short, void *);
void		 server_tls_writecb(int, short, void *);

void		 server_accept(int, short, void *);
void		 server_accept_tls(int, short, void *);
void		 server_input(struct client *);

extern void	 bufferevent_read_pressure_cb(struct evbuffer *, size_t,
		    size_t, void *);

volatile int server_clients;
volatile int server_inflight = 0;
u_int32_t server_cltid;

static struct httpd		*env = NULL;
int				 proc_id;

static struct privsep_proc procs[] = {
	{ "parent",	PROC_PARENT,	server_dispatch_parent },
	{ "logger",	PROC_LOGGER,	server_dispatch_logger }
};

pid_t
server(struct privsep *ps, struct privsep_proc *p)
{
	pid_t	 pid;
	env = ps->ps_env;
	pid = proc_run(ps, p, procs, nitems(procs), server_init, NULL);
	server_http(env);
	return (pid);
}

void
server_shutdown(void)
{
	config_purge(env, CONFIG_ALL);
	usleep(200);	/* XXX server needs to shutdown last */
}

int
server_privinit(struct server *srv)
{
	if (srv->srv_conf.flags & SRVFLAG_LOCATION)
		return (0);

	log_debug("%s: adding server %s", __func__, srv->srv_conf.name);

	if ((srv->srv_s = server_socket_listen(&srv->srv_conf.ss,
	    srv->srv_conf.port, &srv->srv_conf)) == -1)
		return (-1);

	return (0);
}

static char *
server_load_file(const char *filename, off_t *len)
{
	struct stat		 st;
	off_t			 size;
	char			*buf = NULL;
	int			 fd;

	if ((fd = open(filename, O_RDONLY)) == -1)
		return (NULL);
	if (fstat(fd, &st) != 0)
		goto fail;
	size = st.st_size;
	if ((buf = calloc(1, size + 1)) == NULL)
		goto fail;
	if (read(fd, buf, size) != size)
		goto fail;

	close(fd);

	*len = size;
	return (buf);

 fail:
	free(buf);
	close(fd);

	return (NULL);
}

int
server_tls_load_keypair(struct server *srv)
{
	if ((srv->srv_conf.flags & SRVFLAG_TLS) == 0)
		return (0);

	if ((srv->srv_conf.tls_cert = server_load_file(
	    srv->srv_conf.tls_cert_file, &srv->srv_conf.tls_cert_len)) == NULL)
		return (-1);
	log_debug("%s: using certificate %s", __func__,
	    srv->srv_conf.tls_cert_file);

	if ((srv->srv_conf.tls_key = server_load_file(
	    srv->srv_conf.tls_key_file, &srv->srv_conf.tls_key_len)) == NULL)
		return (-1);
	log_debug("%s: using private key %s", __func__,
	    srv->srv_conf.tls_key_file);

	return (0);
}

int
server_tls_init(struct server *srv)
{
	if ((srv->srv_conf.flags & SRVFLAG_TLS) == 0)
		return (0);

	log_debug("%s: setting up TLS for %s", __func__, srv->srv_conf.name);

	if (tls_init() != 0) {
		log_warn("%s: failed to initialise tls", __func__);
		return (-1);
	}
	if ((srv->srv_tls_config = tls_config_new()) == NULL) {
		log_warn("%s: failed to get tls config", __func__);
		return (-1);
	}
	if ((srv->srv_tls_ctx = tls_server()) == NULL) {
		log_warn("%s: failed to get tls server", __func__);
		return (-1);
	}

	if (tls_config_set_ciphers(srv->srv_tls_config,
	    srv->srv_conf.tls_ciphers) != 0) {
		log_warn("%s: failed to set tls ciphers", __func__);
		return (-1);
	}
	if (tls_config_set_cert_mem(srv->srv_tls_config,
	    srv->srv_conf.tls_cert, srv->srv_conf.tls_cert_len) != 0) {
		log_warn("%s: failed to set tls cert", __func__);
		return (-1);
	}
	if (tls_config_set_key_mem(srv->srv_tls_config,
	    srv->srv_conf.tls_key, srv->srv_conf.tls_key_len) != 0) {
		log_warn("%s: failed to set tls key", __func__);
		return (-1);
	}

	if (tls_configure(srv->srv_tls_ctx, srv->srv_tls_config) != 0) {
		log_warn("%s: failed to configure TLS - %s", __func__,
		    tls_error(srv->srv_tls_ctx));
		return (-1);
	}

	/* We're now done with the public/private key... */
	tls_config_clear_keys(srv->srv_tls_config);
	explicit_bzero(srv->srv_conf.tls_cert, srv->srv_conf.tls_cert_len);
	explicit_bzero(srv->srv_conf.tls_key, srv->srv_conf.tls_key_len);
	free(srv->srv_conf.tls_cert);
	free(srv->srv_conf.tls_key);
	srv->srv_conf.tls_cert = NULL;
	srv->srv_conf.tls_key = NULL;
	srv->srv_conf.tls_cert_len = 0;
	srv->srv_conf.tls_key_len = 0;

	return (0);
}

void
server_init(struct privsep *ps, struct privsep_proc *p, void *arg)
{
	server_http(ps->ps_env);

	if (config_init(ps->ps_env) == -1)
		fatal("failed to initialize configuration");

	/* Set to current prefork id */
	proc_id = p->p_instance;

	/* We use a custom shutdown callback */
	p->p_shutdown = server_shutdown;

	/* Unlimited file descriptors (use system limits) */
	socket_rlimit(-1);

#if 0
	/* Schedule statistics timer */
	evtimer_set(&env->sc_statev, server_statistics, NULL);
	memcpy(&tv, &env->sc_statinterval, sizeof(tv));
	evtimer_add(&env->sc_statev, &tv);
#endif
}

void
server_launch(void)
{
	struct server		*srv;

	TAILQ_FOREACH(srv, env->sc_servers, srv_entry) {
		server_tls_init(srv);
		server_http_init(srv);

		log_debug("%s: running server %s", __func__,
		    srv->srv_conf.name);

		event_set(&srv->srv_ev, srv->srv_s, EV_READ,
		    server_accept, srv);
		event_add(&srv->srv_ev, NULL);
		evtimer_set(&srv->srv_evt, server_accept, srv);
	}
}

void
server_purge(struct server *srv)
{
	struct client		*clt;
	struct server_config	*srv_conf;

	/* shutdown and remove server */
	if (event_initialized(&srv->srv_ev))
		event_del(&srv->srv_ev);
	if (evtimer_initialized(&srv->srv_evt))
		evtimer_del(&srv->srv_evt);

	close(srv->srv_s);
	TAILQ_REMOVE(env->sc_servers, srv, srv_entry);

	/* cleanup sessions */
	while ((clt =
	    SPLAY_ROOT(&srv->srv_clients)) != NULL)
		server_close(clt, NULL);

	/* cleanup hosts */
	while ((srv_conf =
	    TAILQ_FIRST(&srv->srv_hosts)) != NULL) {
		TAILQ_REMOVE(&srv->srv_hosts, srv_conf, entry);

		/* It might point to our own "default" entry */
		if (srv_conf != &srv->srv_conf) {
			serverconfig_free(srv_conf);
			free(srv_conf);
		}
	}

	tls_config_free(srv->srv_tls_config);
	tls_free(srv->srv_tls_ctx);

	free(srv);
}

void
serverconfig_free(struct server_config *srv_conf)
{
	free(srv_conf->tls_cert_file);
	free(srv_conf->tls_cert);
	free(srv_conf->tls_key_file);
	free(srv_conf->tls_key);
}

void
serverconfig_reset(struct server_config *srv_conf)
{
	srv_conf->tls_cert_file = srv_conf->tls_cert =
	    srv_conf->tls_key_file = srv_conf->tls_key = NULL;
}

struct server *
server_byaddr(struct sockaddr *addr, in_port_t port)
{
	struct server	*srv;

	TAILQ_FOREACH(srv, env->sc_servers, srv_entry) {
		if (port == srv->srv_conf.port &&
		    sockaddr_cmp((struct sockaddr *)&srv->srv_conf.ss,
		    addr, srv->srv_conf.prefixlen) == 0)
			return (srv);
	}

	return (NULL);
}

struct server_config *
serverconfig_byid(u_int32_t id)
{
	struct server		*srv;
	struct server_config	*srv_conf;

	TAILQ_FOREACH(srv, env->sc_servers, srv_entry) {
		if (srv->srv_conf.id == id)
			return (&srv->srv_conf);
		TAILQ_FOREACH(srv_conf, &srv->srv_hosts, entry) {
			if (srv_conf->id == id)
				return (srv_conf);
		}
	}

	return (NULL);
}

int
server_foreach(int (*srv_cb)(struct server *,
    struct server_config *, void *), void *arg)
{
	struct server		*srv;
	struct server_config	*srv_conf;

	TAILQ_FOREACH(srv, env->sc_servers, srv_entry) {
		if ((srv_cb)(srv, &srv->srv_conf, arg) == -1)
			return (-1);
		TAILQ_FOREACH(srv_conf, &srv->srv_hosts, entry) {
			if ((srv_cb)(srv, srv_conf, arg) == -1)
				return (-1);
		}
	}

	return (0);
}

int
server_socket_af(struct sockaddr_storage *ss, in_port_t port)
{
	switch (ss->ss_family) {
	case AF_INET:
		((struct sockaddr_in *)ss)->sin_port = port;
		((struct sockaddr_in *)ss)->sin_len =
		    sizeof(struct sockaddr_in);
		break;
	case AF_INET6:
		((struct sockaddr_in6 *)ss)->sin6_port = port;
		((struct sockaddr_in6 *)ss)->sin6_len =
		    sizeof(struct sockaddr_in6);
		break;
	default:
		return (-1);
	}

	return (0);
}

in_port_t
server_socket_getport(struct sockaddr_storage *ss)
{
	switch (ss->ss_family) {
	case AF_INET:
		return (((struct sockaddr_in *)ss)->sin_port);
	case AF_INET6:
		return (((struct sockaddr_in6 *)ss)->sin6_port);
	default:
		return (0);
	}

	/* NOTREACHED */
	return (0);
}

int
server_socket(struct sockaddr_storage *ss, in_port_t port,
    struct server_config *srv_conf, int fd, int reuseport)
{
	struct linger	lng;
	int		s = -1, val;

	if (server_socket_af(ss, port) == -1)
		goto bad;

	s = fd == -1 ? socket(ss->ss_family, SOCK_STREAM, IPPROTO_TCP) : fd;
	if (s == -1)
		goto bad;

	/*
	 * Socket options
	 */
	memset(&lng, 0, sizeof(lng));
	if (setsockopt(s, SOL_SOCKET, SO_LINGER, &lng, sizeof(lng)) == -1)
		goto bad;
	if (reuseport) {
		val = 1;
		if (setsockopt(s, SOL_SOCKET, SO_REUSEPORT, &val,
		    sizeof(int)) == -1)
			goto bad;
	}
	if (fcntl(s, F_SETFL, O_NONBLOCK) == -1)
		goto bad;
	if (srv_conf->tcpflags & TCPFLAG_BUFSIZ) {
		val = srv_conf->tcpbufsiz;
		if (setsockopt(s, SOL_SOCKET, SO_RCVBUF,
		    &val, sizeof(val)) == -1)
			goto bad;
		val = srv_conf->tcpbufsiz;
		if (setsockopt(s, SOL_SOCKET, SO_SNDBUF,
		    &val, sizeof(val)) == -1)
			goto bad;
	}

	/*
	 * IP options
	 */
	if (srv_conf->tcpflags & TCPFLAG_IPTTL) {
		val = (int)srv_conf->tcpipttl;
		if (setsockopt(s, IPPROTO_IP, IP_TTL,
		    &val, sizeof(val)) == -1)
			goto bad;
	}
	if (srv_conf->tcpflags & TCPFLAG_IPMINTTL) {
		val = (int)srv_conf->tcpipminttl;
		if (setsockopt(s, IPPROTO_IP, IP_MINTTL,
		    &val, sizeof(val)) == -1)
			goto bad;
	}

	/*
	 * TCP options
	 */
	if (srv_conf->tcpflags & (TCPFLAG_NODELAY|TCPFLAG_NNODELAY)) {
		if (srv_conf->tcpflags & TCPFLAG_NNODELAY)
			val = 0;
		else
			val = 1;
		if (setsockopt(s, IPPROTO_TCP, TCP_NODELAY,
		    &val, sizeof(val)) == -1)
			goto bad;
	}
	if (srv_conf->tcpflags & (TCPFLAG_SACK|TCPFLAG_NSACK)) {
		if (srv_conf->tcpflags & TCPFLAG_NSACK)
			val = 0;
		else
			val = 1;
		if (setsockopt(s, IPPROTO_TCP, TCP_SACK_ENABLE,
		    &val, sizeof(val)) == -1)
			goto bad;
	}

	return (s);

 bad:
	if (s != -1)
		close(s);
	return (-1);
}

int
server_socket_listen(struct sockaddr_storage *ss, in_port_t port,
    struct server_config *srv_conf)
{
	int s;

	if ((s = server_socket(ss, port, srv_conf, -1, 1)) == -1)
		return (-1);

	if (bind(s, (struct sockaddr *)ss, ss->ss_len) == -1)
		goto bad;
	if (listen(s, srv_conf->tcpbacklog) == -1)
		goto bad;

	return (s);

 bad:
	close(s);
	return (-1);
}

int
server_socket_connect(struct sockaddr_storage *ss, in_port_t port,
    struct server_config *srv_conf)
{
	int	s;

	if ((s = server_socket(ss, port, srv_conf, -1, 0)) == -1)
		return (-1);

	if (connect(s, (struct sockaddr *)ss, ss->ss_len) == -1) {
		if (errno != EINPROGRESS)
			goto bad;
	}

	return (s);

 bad:
	close(s);
	return (-1);
}

void
server_tls_readcb(int fd, short event, void *arg)
{
	struct bufferevent	*bufev = arg;
	struct client		*clt = bufev->cbarg;
	char			 rbuf[IBUF_READ_SIZE];
	int			 what = EVBUFFER_READ;
	int			 howmuch = IBUF_READ_SIZE;
	int			 ret;
	size_t			 len;

	if (event == EV_TIMEOUT) {
		what |= EVBUFFER_TIMEOUT;
		goto err;
	}

	if (bufev->wm_read.high != 0)
		howmuch = MIN(sizeof(rbuf), bufev->wm_read.high);

	ret = tls_read(clt->clt_tls_ctx, rbuf, howmuch, &len);
	if (ret == TLS_READ_AGAIN || ret == TLS_WRITE_AGAIN) {
		goto retry;
	} else if (ret != 0) {
		what |= EVBUFFER_ERROR;
		goto err;
	}

	if (evbuffer_add(bufev->input, rbuf, len) == -1) {
		what |= EVBUFFER_ERROR;
		goto err;
	}

	server_bufferevent_add(&bufev->ev_read, bufev->timeout_read);

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
	server_bufferevent_add(&bufev->ev_read, bufev->timeout_read);
	return;

 err:
	(*bufev->errorcb)(bufev, what, bufev->cbarg);
}

void
server_tls_writecb(int fd, short event, void *arg)
{
	struct bufferevent	*bufev = arg;
	struct client		*clt = bufev->cbarg;
	int			 ret;
	short			 what = EVBUFFER_WRITE;
	size_t			 len;

	if (event == EV_TIMEOUT) {
		what |= EVBUFFER_TIMEOUT;
		goto err;
	}

	if (EVBUFFER_LENGTH(bufev->output)) {
		if (clt->clt_buf == NULL) {
			clt->clt_buflen = EVBUFFER_LENGTH(bufev->output);
			if ((clt->clt_buf = malloc(clt->clt_buflen)) == NULL) {
				what |= EVBUFFER_ERROR;
				goto err;
			}
			bcopy(EVBUFFER_DATA(bufev->output),
			    clt->clt_buf, clt->clt_buflen);
		}
		ret = tls_write(clt->clt_tls_ctx, clt->clt_buf,
		    clt->clt_buflen, &len);
		if (ret == TLS_READ_AGAIN || ret == TLS_WRITE_AGAIN) {
			goto retry;
		} else if (ret != 0) {
			what |= EVBUFFER_ERROR;
			goto err;
		}
		evbuffer_drain(bufev->output, len);
	}
	if (clt->clt_buf != NULL) {
		free(clt->clt_buf);
		clt->clt_buf = NULL;
		clt->clt_buflen = 0;
	}

	if (EVBUFFER_LENGTH(bufev->output) != 0)
		server_bufferevent_add(&bufev->ev_write, bufev->timeout_write);

	if (bufev->writecb != NULL &&
	    EVBUFFER_LENGTH(bufev->output) <= bufev->wm_write.low)
		(*bufev->writecb)(bufev, bufev->cbarg);
	return;

 retry:
	if (clt->clt_buflen != 0)
		server_bufferevent_add(&bufev->ev_write, bufev->timeout_write);
	return;

 err:
	if (clt->clt_buf != NULL) {
		free(clt->clt_buf);
		clt->clt_buf = NULL;
		clt->clt_buflen = 0;
	}
	(*bufev->errorcb)(bufev, what, bufev->cbarg);
}

void
server_input(struct client *clt)
{
	struct server_config	*srv_conf = clt->clt_srv_conf;
	evbuffercb		 inrd = server_read;
	evbuffercb		 inwr = server_write;
	socklen_t		 slen;

	if (server_httpdesc_init(clt) == -1) {
		server_close(clt, "failed to allocate http descriptor");
		return;
	}

	clt->clt_toread = TOREAD_HTTP_HEADER;
	inrd = server_read_http;

	slen = sizeof(clt->clt_sndbufsiz);
	if (getsockopt(clt->clt_s, SOL_SOCKET, SO_SNDBUF,
	    &clt->clt_sndbufsiz, &slen) == -1) {
		server_close(clt, "failed to get send buffer size");
		return;
	}

	/*
	 * Client <-> Server
	 */
	clt->clt_bev = bufferevent_new(clt->clt_s, inrd, inwr,
	    server_error, clt);
	if (clt->clt_bev == NULL) {
		server_close(clt, "failed to allocate input buffer event");
		return;
	}

	if (srv_conf->flags & SRVFLAG_TLS) {
		event_set(&clt->clt_bev->ev_read, clt->clt_s, EV_READ,
		    server_tls_readcb, clt->clt_bev);
		event_set(&clt->clt_bev->ev_write, clt->clt_s, EV_WRITE,
		    server_tls_writecb, clt->clt_bev);
	}

	/* Adjust write watermark to the socket buffer output size */
	bufferevent_setwatermark(clt->clt_bev, EV_WRITE,
	    clt->clt_sndbufsiz, 0);
	/* Read at most amount of data that fits in one fcgi record. */
	bufferevent_setwatermark(clt->clt_bev, EV_READ, 0, FCGI_CONTENT_SIZE);

	bufferevent_settimeout(clt->clt_bev,
	    srv_conf->timeout.tv_sec, srv_conf->timeout.tv_sec);
	bufferevent_enable(clt->clt_bev, EV_READ|EV_WRITE);
}

void
server_write(struct bufferevent *bev, void *arg)
{
	struct client		*clt = arg;
	struct evbuffer		*dst = EVBUFFER_OUTPUT(bev);

	if (EVBUFFER_LENGTH(dst) == 0 &&
	    clt->clt_toread == TOREAD_HTTP_NONE)
		goto done;

	getmonotime(&clt->clt_tv_last);

	if (clt->clt_done)
		goto done;

	bufferevent_enable(bev, EV_READ);
	return;
 done:
	(*bev->errorcb)(bev, EVBUFFER_WRITE|EVBUFFER_EOF, bev->cbarg);
	return;
}

void
server_dump(struct client *clt, const void *buf, size_t len)
{
	size_t			 outlen;

	if (!len)
		return;

	/*
	 * This function will dump the specified message directly
	 * to the underlying client, without waiting for success
	 * of non-blocking events etc. This is useful to print an
	 * error message before gracefully closing the client.
	 */
	if (clt->clt_tls_ctx != NULL)
		(void)tls_write(clt->clt_tls_ctx, buf, len, &outlen);
	else
		(void)write(clt->clt_s, buf, len);
}

void
server_read(struct bufferevent *bev, void *arg)
{
	struct client		*clt = arg;
	struct evbuffer		*src = EVBUFFER_INPUT(bev);

	getmonotime(&clt->clt_tv_last);

	if (!EVBUFFER_LENGTH(src))
		return;
	if (server_bufferevent_write_buffer(clt, src) == -1)
		goto fail;
	if (clt->clt_done)
		goto done;
	return;
 done:
	(*bev->errorcb)(bev, EVBUFFER_READ|EVBUFFER_EOF, bev->cbarg);
	return;
 fail:
	server_close(clt, strerror(errno));
}

void
server_error(struct bufferevent *bev, short error, void *arg)
{
	struct client		*clt = arg;
	struct evbuffer		*dst;

	if (error & EVBUFFER_TIMEOUT) {
		server_close(clt, "buffer event timeout");
		return;
	}
	if (error & EVBUFFER_ERROR) {
		if (errno == EFBIG) {
			bufferevent_enable(bev, EV_READ);
			return;
		}
		server_close(clt, "buffer event error");
		return;
	}
	if (error & (EVBUFFER_READ|EVBUFFER_WRITE|EVBUFFER_EOF)) {
		bufferevent_disable(bev, EV_READ|EV_WRITE);

		clt->clt_done = 1;

		dst = EVBUFFER_OUTPUT(clt->clt_bev);
		if (EVBUFFER_LENGTH(dst)) {
			/* Finish writing all data first */
			bufferevent_enable(clt->clt_bev, EV_WRITE);
			return;
		}

		server_close(clt, "done");
		return;
	}
	server_close(clt, "unknown event error");
	return;
}

void
server_accept(int fd, short event, void *arg)
{
	struct server		*srv = arg;
	struct client		*clt = NULL;
	socklen_t		 slen;
	struct sockaddr_storage	 ss;
	int			 s = -1;

	event_add(&srv->srv_ev, NULL);
	if ((event & EV_TIMEOUT))
		return;

	slen = sizeof(ss);
	if ((s = accept_reserve(fd, (struct sockaddr *)&ss,
	    &slen, FD_RESERVE, &server_inflight)) == -1) {
		/*
		 * Pause accept if we are out of file descriptors, or
		 * libevent will haunt us here too.
		 */
		if (errno == ENFILE || errno == EMFILE) {
			struct timeval evtpause = { 1, 0 };

			event_del(&srv->srv_ev);
			evtimer_add(&srv->srv_evt, &evtpause);
			log_debug("%s: deferring connections", __func__);
		}
		return;
	}
	if (server_clients >= SERVER_MAX_CLIENTS)
		goto err;

	if (fcntl(s, F_SETFL, O_NONBLOCK) == -1)
		goto err;

	if ((clt = calloc(1, sizeof(*clt))) == NULL)
		goto err;

	clt->clt_s = s;
	clt->clt_fd = -1;
	clt->clt_toread = TOREAD_UNLIMITED;
	clt->clt_srv = srv;
	clt->clt_srv_conf = &srv->srv_conf;
	clt->clt_id = ++server_cltid;
	clt->clt_srv_id = srv->srv_conf.id;
	clt->clt_pid = getpid();
	clt->clt_inflight = 1;

	/* get local address */
	slen = sizeof(clt->clt_srv_ss);
	if (getsockname(s, (struct sockaddr *)&clt->clt_srv_ss,
	    &slen) == -1) {
		server_close(clt, "listen address lookup failed");
		return;
	}

	/* get client address */
	memcpy(&clt->clt_ss, &ss, sizeof(clt->clt_ss));

	/* get ports */
	switch (ss.ss_family) {
	case AF_INET:
		clt->clt_port = ((struct sockaddr_in *)&ss)->sin_port;
		break;
	case AF_INET6:
		clt->clt_port = ((struct sockaddr_in6 *)&ss)->sin6_port;
		break;
	}

	getmonotime(&clt->clt_tv_start);
	memcpy(&clt->clt_tv_last, &clt->clt_tv_start, sizeof(clt->clt_tv_last));

	server_clients++;
	SPLAY_INSERT(client_tree, &srv->srv_clients, clt);

	/* Increment the per-relay client counter */
	//srv->srv_stats[proc_id].last++;

	/* Pre-allocate output buffer */
	clt->clt_output = evbuffer_new();
	if (clt->clt_output == NULL) {
		server_close(clt, "failed to allocate output buffer");
		return;
	}

	/* Pre-allocate log buffer */
	clt->clt_log = evbuffer_new();
	if (clt->clt_log == NULL) {
		server_close(clt, "failed to allocate log buffer");
		return;
	}

	if (srv->srv_conf.flags & SRVFLAG_TLS) {
		event_again(&clt->clt_ev, clt->clt_s, EV_TIMEOUT|EV_READ,
		    server_accept_tls, &clt->clt_tv_start,
		    &srv->srv_conf.timeout, clt);
		return;
	}

	server_input(clt);
	return;

 err:
	if (s != -1) {
		close(s);
		if (clt != NULL)
			free(clt);
		/*
		 * the client struct was not completly set up, but still
		 * counted as an inflight client. account for this.
		 */
		server_inflight_dec(clt, __func__);
	}
}

void
server_accept_tls(int fd, short event, void *arg)
{
	struct client *clt = (struct client *)arg;
	struct server *srv = (struct server *)clt->clt_srv;
	int ret;

	if (event == EV_TIMEOUT) {
		server_close(clt, "TLS accept timeout");
		return;
	}

	if (srv->srv_tls_ctx == NULL)
		fatalx("NULL tls context");

	ret = tls_accept_socket(srv->srv_tls_ctx, &clt->clt_tls_ctx,
	    clt->clt_s);
	if (ret == TLS_READ_AGAIN) {
		event_again(&clt->clt_ev, clt->clt_s, EV_TIMEOUT|EV_READ,
		    server_accept_tls, &clt->clt_tv_start,
		    &srv->srv_conf.timeout, clt);
	} else if (ret == TLS_WRITE_AGAIN) {
		event_again(&clt->clt_ev, clt->clt_s, EV_TIMEOUT|EV_WRITE,
		    server_accept_tls, &clt->clt_tv_start,
		    &srv->srv_conf.timeout, clt);
	} else if (ret != 0) {
		log_warnx("%s: TLS accept failed - %s", __func__,
		    tls_error(srv->srv_tls_ctx));
		return;
	}

	server_input(clt);
	return;
}

void
server_inflight_dec(struct client *clt, const char *why)
{
	if (clt != NULL) {
		/* the flight already left inflight mode. */
		if (clt->clt_inflight == 0)
			return;
		clt->clt_inflight = 0;
	}

	/* the file was never opened, thus this was an inflight client. */
	server_inflight--;
	DPRINTF("%s: inflight decremented, now %d, %s",
	    __func__, server_inflight, why);
}

void
server_sendlog(struct server_config *srv_conf, int cmd, const char *emsg, ...)
{
	va_list		 ap;
	char		*msg;
	int		 ret;
	struct iovec	 iov[2];

	if (srv_conf->flags & SRVFLAG_SYSLOG) {
		va_start(ap, emsg);
		if (cmd == IMSG_LOG_ACCESS)
			vlog(LOG_INFO, emsg, ap);
		else
			vlog(LOG_DEBUG, emsg, ap);
		va_end(ap);
		return;
	}

	va_start(ap, emsg);
	ret = vasprintf(&msg, emsg, ap);
	va_end(ap);
	if (ret == -1) {
		log_warn("%s: vasprintf", __func__);
		return;
	}

	iov[0].iov_base = &srv_conf->id;
	iov[0].iov_len = sizeof(srv_conf->id);
	iov[1].iov_base = msg;
	iov[1].iov_len = strlen(msg) + 1;

	proc_composev_imsg(env->sc_ps, PROC_LOGGER, -1, cmd, -1, iov, 2);
}

void
server_log(struct client *clt, const char *msg)
{
	char			 ibuf[MAXHOSTNAMELEN], obuf[MAXHOSTNAMELEN];
	struct server_config	*srv_conf = clt->clt_srv_conf;
	char			*ptr = NULL;
	int			 debug_cmd = -1;
	extern int		 verbose;

	switch (srv_conf->logformat) {
	case LOG_FORMAT_CONNECTION:
		debug_cmd = IMSG_LOG_ACCESS;
		break;
	default:
		if (verbose > 1)
			debug_cmd = IMSG_LOG_ERROR;
		if (EVBUFFER_LENGTH(clt->clt_log)) {
			while ((ptr =
			    evbuffer_readline(clt->clt_log)) != NULL) {
				server_sendlog(srv_conf,
				    IMSG_LOG_ACCESS, "%s", ptr);
				free(ptr);
			}
		}
		break;
	}

	if (debug_cmd != -1 && msg != NULL) {
		memset(ibuf, 0, sizeof(ibuf));
		memset(obuf, 0, sizeof(obuf));
		(void)print_host(&clt->clt_ss, ibuf, sizeof(ibuf));
		(void)server_http_host(&clt->clt_srv_ss, obuf, sizeof(obuf));
		if (EVBUFFER_LENGTH(clt->clt_log) &&
		    evbuffer_add_printf(clt->clt_log, "\n") != -1)
			ptr = evbuffer_readline(clt->clt_log);
		server_sendlog(srv_conf, debug_cmd, "server %s, "
		    "client %d (%d active), %s:%u -> %s, "
		    "%s%s%s", srv_conf->name, clt->clt_id, server_clients,
		    ibuf, ntohs(clt->clt_port), obuf, msg,
		    ptr == NULL ? "" : ",", ptr == NULL ? "" : ptr);
		if (ptr != NULL)
			free(ptr);
	}
}

void
server_close(struct client *clt, const char *msg)
{
	struct server		*srv = clt->clt_srv;

	SPLAY_REMOVE(client_tree, &srv->srv_clients, clt);

	/* free the HTTP descriptors incl. headers */
	server_close_http(clt);

	event_del(&clt->clt_ev);
	if (clt->clt_bev != NULL)
		bufferevent_disable(clt->clt_bev, EV_READ|EV_WRITE);
	if (clt->clt_srvbev != NULL)
		bufferevent_disable(clt->clt_srvbev, EV_READ|EV_WRITE);

	server_log(clt, msg);

	if (clt->clt_bev != NULL)
		bufferevent_free(clt->clt_bev);
	if (clt->clt_output != NULL)
		evbuffer_free(clt->clt_output);
	if (clt->clt_srvevb != NULL)
		evbuffer_free(clt->clt_srvevb);

	if (clt->clt_srvbev != NULL)
		bufferevent_free(clt->clt_srvbev);
	if (clt->clt_fd != -1)
		close(clt->clt_fd);
	if (clt->clt_s != -1)
		close(clt->clt_s);

	if (clt->clt_tls_ctx != NULL)
		tls_close(clt->clt_tls_ctx);
	tls_free(clt->clt_tls_ctx);

	server_inflight_dec(clt, __func__);

	if (clt->clt_log != NULL)
		evbuffer_free(clt->clt_log);

	free(clt);
	server_clients--;
}

int
server_dispatch_parent(int fd, struct privsep_proc *p, struct imsg *imsg)
{
	switch (imsg->hdr.type) {
	case IMSG_CFG_MEDIA:
		config_getmedia(env, imsg);
		break;
	case IMSG_CFG_SERVER:
		config_getserver(env, imsg);
		break;
	case IMSG_CFG_DONE:
		config_getcfg(env, imsg);
		break;
	case IMSG_CTL_START:
		server_launch();
		break;
	case IMSG_CTL_RESET:
		config_getreset(env, imsg);
		break;
	default:
		return (-1);
	}

	return (0);
}

int
server_dispatch_logger(int fd, struct privsep_proc *p, struct imsg *imsg)
{
	switch (imsg->hdr.type) {
	default:
		return (-1);
	}

	return (0);
}

int
server_bufferevent_add(struct event *ev, int timeout)
{
	struct timeval tv, *ptv = NULL;

	if (timeout) {
		timerclear(&tv);
		tv.tv_sec = timeout;
		ptv = &tv;
	}

	return (event_add(ev, ptv));
}

int
server_bufferevent_printf(struct client *clt, const char *fmt, ...)
{
	int	 ret;
	va_list	 ap;
	char	*str;

	va_start(ap, fmt);
	ret = vasprintf(&str, fmt, ap);
	va_end(ap);

	if (ret == -1)
		return (ret);

	ret = server_bufferevent_print(clt, str);
	free(str);

	return (ret);
}

int
server_bufferevent_print(struct client *clt, const char *str)
{
	if (clt->clt_bev == NULL)
		return (evbuffer_add(clt->clt_output, str, strlen(str)));
	return (bufferevent_write(clt->clt_bev, str, strlen(str)));
}

int
server_bufferevent_write_buffer(struct client *clt, struct evbuffer *buf)
{
	if (clt->clt_bev == NULL)
		return (evbuffer_add_buffer(clt->clt_output, buf));
	return (bufferevent_write_buffer(clt->clt_bev, buf));
}

int
server_bufferevent_write_chunk(struct client *clt,
    struct evbuffer *buf, size_t size)
{
	int ret;
	ret = server_bufferevent_write(clt, buf->buffer, size);
	if (ret != -1)
		evbuffer_drain(buf, size);
	return (ret);
}

int
server_bufferevent_write(struct client *clt, void *data, size_t size)
{
	if (clt->clt_bev == NULL)
		return (evbuffer_add(clt->clt_output, data, size));
	return (bufferevent_write(clt->clt_bev, data, size));
}

int
server_client_cmp(struct client *a, struct client *b)
{
	return ((int)a->clt_id - b->clt_id);
}

SPLAY_GENERATE(client_tree, client, clt_nodes, server_client_cmp);
