/*	$OpenBSD: server.c,v 1.3 2014/07/13 14:46:52 reyk Exp $	*/

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
#include <sys/tree.h>
#include <sys/hash.h>

#include <net/if.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <err.h>
#include <pwd.h>
#include <event.h>
#include <fnmatch.h>

#include <openssl/dh.h>
#include <openssl/ssl.h>

#include "httpd.h"

int		 server_dispatch_parent(int, struct privsep_proc *,
		    struct imsg *);
void		 server_shutdown(void);

void		 server_init(struct privsep *, struct privsep_proc *p, void *);
void		 server_launch(void);
int		 server_socket(struct sockaddr_storage *, in_port_t,
		    struct server *, int, int);
int		 server_socket_listen(struct sockaddr_storage *, in_port_t,
		    struct server *);

void		 server_accept(int, short, void *);
void		 server_input(struct client *);

extern void	 bufferevent_read_pressure_cb(struct evbuffer *, size_t,
		    size_t, void *);

volatile int server_clients;
volatile int server_inflight = 0;
u_int32_t server_cltid;

static struct httpd		*env = NULL;
int				 proc_id;

static struct privsep_proc procs[] = {
	{ "parent",	PROC_PARENT,	server_dispatch_parent }
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
	log_debug("%s: adding server %s", __func__, srv->srv_conf.name);

	if ((srv->srv_s = server_socket_listen(&srv->srv_conf.ss,
	    srv->srv_conf.port, srv)) == -1)
		return (-1);

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
		server_http_init(srv);

		log_debug("%s: running server %s", __func__,
		    srv->srv_conf.name);

		event_set(&srv->srv_ev, srv->srv_s, EV_READ,
		    server_accept, srv);
		event_add(&srv->srv_ev, NULL);
		evtimer_set(&srv->srv_evt, server_accept, srv);
	}
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
    struct server *srv, int fd, int reuseport)
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
	if (srv->srv_tcpflags & TCPFLAG_BUFSIZ) {
		val = srv->srv_tcpbufsiz;
		if (setsockopt(s, SOL_SOCKET, SO_RCVBUF,
		    &val, sizeof(val)) == -1)
			goto bad;
		val = srv->srv_tcpbufsiz;
		if (setsockopt(s, SOL_SOCKET, SO_SNDBUF,
		    &val, sizeof(val)) == -1)
			goto bad;
	}

	/*
	 * IP options
	 */
	if (srv->srv_tcpflags & TCPFLAG_IPTTL) {
		val = (int)srv->srv_tcpipttl;
		if (setsockopt(s, IPPROTO_IP, IP_TTL,
		    &val, sizeof(val)) == -1)
			goto bad;
	}
	if (srv->srv_tcpflags & TCPFLAG_IPMINTTL) {
		val = (int)srv->srv_tcpipminttl;
		if (setsockopt(s, IPPROTO_IP, IP_MINTTL,
		    &val, sizeof(val)) == -1)
			goto bad;
	}

	/*
	 * TCP options
	 */
	if (srv->srv_tcpflags & (TCPFLAG_NODELAY|TCPFLAG_NNODELAY)) {
		if (srv->srv_tcpflags & TCPFLAG_NNODELAY)
			val = 0;
		else
			val = 1;
		if (setsockopt(s, IPPROTO_TCP, TCP_NODELAY,
		    &val, sizeof(val)) == -1)
			goto bad;
	}
	if (srv->srv_tcpflags & (TCPFLAG_SACK|TCPFLAG_NSACK)) {
		if (srv->srv_tcpflags & TCPFLAG_NSACK)
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
    struct server *srv)
{
	int s;

	if ((s = server_socket(ss, port, srv, -1, 1)) == -1)
		return (-1);

	if (bind(s, (struct sockaddr *)ss, ss->ss_len) == -1)
		goto bad;
	if (listen(s, srv->srv_tcpbacklog) == -1)
		goto bad;

	return (s);

 bad:
	close(s);
	return (-1);
}

void
server_input(struct client *clt)
{
	struct server	*srv = clt->clt_server;
	evbuffercb	 inrd = server_read;
	evbuffercb	 inwr = server_write;

	if (server_httpdesc_init(clt) == -1) {
		server_close(clt,
		    "failed to allocate http descriptor");
		return;
	}

	clt->clt_toread = TOREAD_HTTP_HEADER;
	inrd = server_read_http;

	/*
	 * Client <-> Server
	 */
	clt->clt_bev = bufferevent_new(clt->clt_s, inrd, inwr,
	    server_error, clt);
	if (clt->clt_bev == NULL) {
		server_close(clt, "failed to allocate input buffer event");
		return;
	}

	bufferevent_settimeout(clt->clt_bev,
	    srv->srv_conf.timeout.tv_sec, srv->srv_conf.timeout.tv_sec);
	bufferevent_enable(clt->clt_bev, EV_READ|EV_WRITE);
}

void
server_write(struct bufferevent *bev, void *arg)
{
	struct client		*clt = arg;

	getmonotime(&clt->clt_tv_last);

	if (clt->clt_done)
		goto done;
	return;
 done:
	server_close(clt, "done");
	return;
}

void
server_dump(struct client *clt, const void *buf, size_t len)
{
	if (!len)
		return;

	/*
	 * This function will dump the specified message directly
	 * to the underlying client, without waiting for success
	 * of non-blocking events etc. This is useful to print an
	 * error message before gracefully closing the client.
	 */
#if 0
	if (cre->ssl != NULL)
		(void)SSL_write(cre->ssl, buf, len);
	else
#endif
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
	bufferevent_enable(bev, EV_READ);
	return;
 done:
	server_close(clt, "done");
	return;
 fail:
	server_close(clt, strerror(errno));
}

void
server_error(struct bufferevent *bev, short error, void *arg)
{
	struct client		*clt = arg;

	if (error & EVBUFFER_TIMEOUT) {
		server_close(clt, "buffer event timeout");
		return;
	}
	if (error & EVBUFFER_ERROR && errno == EFBIG) {
		bufferevent_enable(bev, EV_READ);
		return;
	}
	if (error & (EVBUFFER_READ|EVBUFFER_WRITE|EVBUFFER_EOF)) {
		bufferevent_disable(bev, EV_READ|EV_WRITE);

		clt->clt_done = 1;
		server_close(clt, "done");

		return;
	}
	server_close(clt, "buffer event error");
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
	clt->clt_server = srv;
	clt->clt_id = ++server_cltid;
	clt->clt_serverid = srv->srv_conf.id;
	clt->clt_pid = getpid();
	switch (ss.ss_family) {
	case AF_INET:
		clt->clt_port = ((struct sockaddr_in *)&ss)->sin_port;
		break;
	case AF_INET6:
		clt->clt_port = ((struct sockaddr_in6 *)&ss)->sin6_port;
		break;
	}
	memcpy(&clt->clt_ss, &ss, sizeof(clt->clt_ss));

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
		server_inflight--;
		log_debug("%s: inflight decremented, now %d",
		    __func__, server_inflight);
	}
}

void
server_close(struct client *clt, const char *msg)
{
	char		 ibuf[128], obuf[128], *ptr = NULL;
	struct server	*srv = clt->clt_server;

	SPLAY_REMOVE(client_tree, &srv->srv_clients, clt);

	event_del(&clt->clt_ev);
	if (clt->clt_bev != NULL)
		bufferevent_disable(clt->clt_bev, EV_READ|EV_WRITE);
	if (clt->clt_file != NULL)
		bufferevent_disable(clt->clt_file, EV_READ|EV_WRITE);

	if ((env->sc_opts & HTTPD_OPT_LOGUPDATE) && msg != NULL) {
		memset(&ibuf, 0, sizeof(ibuf));
		memset(&obuf, 0, sizeof(obuf));
		(void)print_host(&clt->clt_ss, ibuf, sizeof(ibuf));
		(void)print_host(&srv->srv_conf.ss, obuf, sizeof(obuf));
		if (EVBUFFER_LENGTH(clt->clt_log) &&
		    evbuffer_add_printf(clt->clt_log, "\r\n") != -1)
			ptr = evbuffer_readline(clt->clt_log);
		log_info("server %s, "
		    "client %d (%d active), %s -> %s:%d, "
		    "%s%s%s", srv->srv_conf.name, clt->clt_id, server_clients,
		    ibuf, obuf, ntohs(clt->clt_port), msg,
		    ptr == NULL ? "" : ",", ptr == NULL ? "" : ptr);
		if (ptr != NULL)
			free(ptr);
	}

	if (clt->clt_bev != NULL)
		bufferevent_free(clt->clt_bev);
	else if (clt->clt_output != NULL)
		evbuffer_free(clt->clt_output);

	if (clt->clt_file != NULL)
		bufferevent_free(clt->clt_file);
	if (clt->clt_fd != -1)
		close(clt->clt_fd);

	if (clt->clt_s != -1) {
		close(clt->clt_s);
		if (/* XXX */ -1) {
			/*
			 * the output was never connected,
			 * thus this was an inflight client.
			 */
			server_inflight--;
			log_debug("%s: clients inflight decremented, now %d",
			    __func__, server_inflight);
		}
	}

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
