/*	$OpenBSD: relay.c,v 1.33 2007/06/07 07:19:50 pyr Exp $	*/

/*
 * Copyright (c) 2006, 2007 Reyk Floeter <reyk@openbsd.org>
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

#include <sys/queue.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/tree.h>
#include <sys/hash.h>

#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <net/if.h>
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

#include <openssl/ssl.h>

#include "hoststated.h"

void		 relay_sig_handler(int sig, short, void *);
void		 relay_statistics(int, short, void *);
void		 relay_dispatch_pfe(int, short, void *);
void		 relay_dispatch_parent(int, short, void *);
void		 relay_shutdown(void);

void		 relay_privinit(void);
void		 relay_protodebug(struct relay *);
void		 relay_init(void);
void		 relay_launch(void);
int		 relay_socket(struct sockaddr_storage *, in_port_t,
		    struct protocol *);
int		 relay_socket_listen(struct sockaddr_storage *, in_port_t,
		    struct protocol *);
int		 relay_socket_connect(struct sockaddr_storage *, in_port_t,
		    struct protocol *);

void		 relay_accept(int, short, void *);
void		 relay_input(struct session *);
void		 relay_close(struct session *, const char *);
void		 relay_session(struct session *);
void		 relay_natlook(int, short, void *);

int		 relay_connect(struct session *);
void		 relay_connected(int, short, void *);

const char	*relay_host(struct sockaddr_storage *, char *, size_t);
u_int32_t	 relay_hash_addr(struct sockaddr_storage *, u_int32_t);
int		 relay_from_table(struct session *);

void		 relay_write(struct bufferevent *, void *);
void		 relay_read(struct bufferevent *, void *);
void		 relay_error(struct bufferevent *, short, void *);

int		 relay_handle_http(struct ctl_relay_event *,
		    struct protonode *, struct protonode *, int);
void		 relay_read_http(struct bufferevent *, void *);
void		 relay_read_httpcontent(struct bufferevent *, void *);
void		 relay_read_httpchunks(struct bufferevent *, void *);
char		*relay_expand_http(struct ctl_relay_event *, char *,
		    char *, size_t);

SSL_CTX		*relay_ssl_ctx_create(struct relay *);
void		 relay_ssl_transaction(struct session *);
void		 relay_ssl_accept(int, short, void *);
void		 relay_ssl_connected(struct ctl_relay_event *);
void		 relay_ssl_readcb(int, short, void *);
void		 relay_ssl_writecb(int, short, void *);

int		 relay_bufferevent_add(struct event *, int);
#ifdef notyet
int		 relay_bufferevent_printf(struct ctl_relay_event *,
		    const char *, ...);
#endif
int		 relay_bufferevent_print(struct ctl_relay_event *, char *);
int		 relay_bufferevent_write_buffer(struct ctl_relay_event *,
		    struct evbuffer *);
int		 relay_bufferevent_write_chunk(struct ctl_relay_event *,
		    struct evbuffer *, size_t);
int		 relay_bufferevent_write(struct ctl_relay_event *,
		    void *, size_t);
static __inline int
		 relay_proto_cmp(struct protonode *, struct protonode *);
extern void	 bufferevent_read_pressure_cb(struct evbuffer *, size_t,
		    size_t, void *);

volatile sig_atomic_t relay_sessions;
objid_t relay_conid;

static struct hoststated	*env = NULL;
struct imsgbuf			*ibuf_pfe;
struct imsgbuf			*ibuf_main;
int				 proc_id;

#if DEBUG > 1
#define DPRINTF		log_debug
#else
#define DPRINTF(x...)	do { } while(0)
#endif

void
relay_sig_handler(int sig, short event, void *arg)
{
	switch (sig) {
	case SIGTERM:
	case SIGINT:
		(void)event_loopexit(NULL);
	}
}

pid_t
relay(struct hoststated *x_env, int pipe_parent2pfe[2], int pipe_parent2hce[2],
    int pipe_parent2relay[RELAY_MAXPROC][2], int pipe_pfe2hce[2],
    int pipe_pfe2relay[RELAY_MAXPROC][2])
{
	pid_t		 pid;
	struct passwd	*pw;
	struct event	 ev_sigint;
	struct event	 ev_sigterm;
	int		 i;

	switch (pid = fork()) {
	case -1:
		fatal("relay: cannot fork");
	case 0:
		break;
	default:
		return (pid);
	}

	env = x_env;
	purge_config(env, PURGE_SERVICES);

	/* Need root privileges for relay initialization */
	relay_privinit();

	if ((pw = getpwnam(HOSTSTATED_USER)) == NULL)
		fatal("relay: getpwnam");

#ifndef DEBUG
	if (chroot(pw->pw_dir) == -1)
		fatal("relay: chroot");
	if (chdir("/") == -1)
		fatal("relay: chdir(\"/\")");

#else
#warning disabling privilege revocation and chroot in DEBUG mode
#endif

	setproctitle("socket relay engine");
	hoststated_process = PROC_RELAY;

#ifndef DEBUG
	if (setgroups(1, &pw->pw_gid) ||
	    setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) ||
	    setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid))
		fatal("relay: can't drop privileges");
#endif

	/* Fork child handlers */
	for (i = 1; i < env->prefork_relay; i++) {
		if (fork() == 0) {
			proc_id = i;
			break;
		}
	}

	event_init();

	/* Per-child initialization */
	relay_init();

	signal_set(&ev_sigint, SIGINT, relay_sig_handler, NULL);
	signal_set(&ev_sigterm, SIGTERM, relay_sig_handler, NULL);
	signal_add(&ev_sigint, NULL);
	signal_add(&ev_sigterm, NULL);
	signal(SIGHUP, SIG_IGN);
	signal(SIGPIPE, SIG_IGN);

	/* setup pipes */
	close(pipe_pfe2hce[0]);
	close(pipe_pfe2hce[1]);
	close(pipe_parent2hce[0]);
	close(pipe_parent2hce[1]);
	close(pipe_parent2pfe[0]);
	close(pipe_parent2pfe[1]);
	for (i = 0; i < env->prefork_relay; i++) {
		if (i == proc_id)
			continue;
		close(pipe_parent2relay[i][0]);
		close(pipe_parent2relay[i][1]);
		close(pipe_pfe2relay[i][0]);
		close(pipe_pfe2relay[i][1]);
	}
	close(pipe_parent2relay[proc_id][1]);
	close(pipe_pfe2relay[proc_id][1]);

	if ((ibuf_pfe = calloc(1, sizeof(struct imsgbuf))) == NULL ||
	    (ibuf_main = calloc(1, sizeof(struct imsgbuf))) == NULL)
		fatal("relay");
	imsg_init(ibuf_main, pipe_parent2relay[proc_id][0],
	    relay_dispatch_parent);
	imsg_init(ibuf_pfe, pipe_pfe2relay[proc_id][0], relay_dispatch_pfe);

	ibuf_pfe->events = EV_READ;
	event_set(&ibuf_pfe->ev, ibuf_pfe->fd, ibuf_pfe->events,
	    ibuf_pfe->handler, ibuf_pfe);
	event_add(&ibuf_pfe->ev, NULL);

	ibuf_main->events = EV_READ;
	event_set(&ibuf_main->ev, ibuf_main->fd, ibuf_main->events,
	    ibuf_main->handler, ibuf_main);
	event_add(&ibuf_main->ev, NULL);

	relay_launch();

	event_dispatch();
	relay_shutdown();

	return (0);
}

void
relay_shutdown(void)
{
	struct session	*con;

	struct relay	*rlay;
	TAILQ_FOREACH(rlay, &env->relays, entry) {
		if (rlay->conf.flags & F_DISABLE)
			continue;
		close(rlay->s);
		while ((con = TAILQ_FIRST(&rlay->sessions)) != NULL)
			relay_close(con, "shutdown");
	}
	usleep(200);	/* XXX relay needs to shutdown last */
	log_info("socket relay engine exiting");
	_exit(0);
}

void
relay_protodebug(struct relay *rlay)
{
	struct protocol *proto = rlay->proto;
	struct protonode *pn;
	struct proto_tree *tree;
	const char *name;

	fprintf(stderr, "protocol %d: name %s\n", proto->id, proto->name);
	fprintf(stderr, "\tflags: 0x%04x\n", proto->flags);
	if (proto->cache != -1)
		fprintf(stderr, "\tssl session cache: %d\n", proto->cache);
	fprintf(stderr, "\ttype: ");
	switch (proto->type) {
	case RELAY_PROTO_TCP:
		fprintf(stderr, "tcp\n");
		break;
	case RELAY_PROTO_HTTP:
		fprintf(stderr, "http\n");
		break;
	}

	name = "request";
	tree = &proto->request_tree;
 show:
	RB_FOREACH(pn, proto_tree, tree) {
		fprintf(stderr, "\t\t");

		fprintf(stderr, "%s ", name);

		switch (pn->type) {
		case NODE_TYPE_HEADER:
			break;
		case NODE_TYPE_URL:
			fprintf(stderr, "url ");
			break;
		case NODE_TYPE_COOKIE:
			fprintf(stderr, "cookie ");
			break;
		case NODE_TYPE_PATH:
			fprintf(stderr, "path ");
			break;
		}

		switch (pn->action) {
		case NODE_ACTION_APPEND:
			fprintf(stderr, "append \"%s\" to \"%s\"",
			    pn->value, pn->key);
			break;
		case NODE_ACTION_CHANGE:
			fprintf(stderr, "change \"%s\" to \"%s\"",
			    pn->key, pn->value);
			break;
		case NODE_ACTION_REMOVE:
			fprintf(stderr, "remove \"%s\"",
			    pn->key);
			break;
		case NODE_ACTION_EXPECT:
			fprintf(stderr, "expect \"%s\" from \"%s\"",
			    pn->value, pn->key);
			break;
		case NODE_ACTION_FILTER:
			fprintf(stderr, "filter \"%s\" from \"%s\"",
			    pn->value, pn->key);
			break;
		case NODE_ACTION_HASH:
			fprintf(stderr, "hash \"%s\"", pn->key);
			break;
		case NODE_ACTION_LOG:
			fprintf(stderr, "log \"%s\"", pn->key);
			break;
		case NODE_ACTION_NONE:
			fprintf(stderr, "none \"%s\"", pn->key);
			break;
		}
		fprintf(stderr, "\n");
	}
	if (tree == &proto->request_tree) {
		name = "response";
		tree = &proto->response_tree;
		goto show;
	}
}

void
relay_privinit(void)
{
	struct relay	*rlay;
	extern int	 debug;

	if (env->flags & F_SSL)
		ssl_init(env);

	TAILQ_FOREACH(rlay, &env->relays, entry) {
		log_debug("relay_init: adding relay %s", rlay->conf.name);

		if (debug)
			relay_protodebug(rlay);

		if ((rlay->conf.flags & F_SSL) &&
		    (rlay->ctx = relay_ssl_ctx_create(rlay)) == NULL)
			fatal("relay_launch: failed to create SSL context");

		if ((rlay->s = relay_socket_listen(&rlay->conf.ss,
		    rlay->conf.port, rlay->proto)) == -1)
			fatal("relay_launch: failed to listen");
	}
}

void
relay_init(void)
{
	struct relay	*rlay;
	struct host	*host;
	struct timeval	 tv;

	TAILQ_FOREACH(rlay, &env->relays, entry) {
		if (rlay->dsttable != NULL) {
			switch (rlay->conf.dstmode) {
			case RELAY_DSTMODE_ROUNDROBIN:
				rlay->dstkey = 0;
				break;
			case RELAY_DSTMODE_LOADBALANCE:
			case RELAY_DSTMODE_HASH:
				rlay->dstkey =
				    hash32_str(rlay->conf.name, HASHINIT);
				rlay->dstkey =
				    hash32_str(rlay->dsttable->conf.name,
				    rlay->dstkey);
				break;
			}
			rlay->dstnhosts = 0;
			TAILQ_FOREACH(host, &rlay->dsttable->hosts, entry) {
				if (rlay->dstnhosts >= RELAY_MAXHOSTS)
					fatal("relay_init: "
					    "too many hosts in table");
				rlay->dsthost[rlay->dstnhosts++] = host;
			}
			log_info("adding %d hosts from table %s%s",
			    rlay->dstnhosts, rlay->dsttable->conf.name,
			    rlay->conf.dstcheck ? "" : " (no check)");
		}
	}

	/* Schedule statistics timer */
	evtimer_set(&env->statev, relay_statistics, NULL);
	bcopy(&env->statinterval, &tv, sizeof(tv));
	evtimer_add(&env->statev, &tv);
}

void
relay_statistics(int fd, short events, void *arg)
{
	struct relay		*rlay;
	struct ctl_stats	 crs, *cur;
	struct timeval		 tv, tv_now;
	int			 resethour = 0, resetday = 0;
	struct session		*con, *next_con;

	/*
	 * This is a hack to calculate some average statistics.
	 * It doesn't try to be very accurate, but could be improved...
	 */

	timerclear(&tv);
	if (gettimeofday(&tv_now, NULL))
		fatal("relay_init: gettimeofday");

	TAILQ_FOREACH(rlay, &env->relays, entry) {
		bzero(&crs, sizeof(crs));
		resethour = resetday = 0;

		cur = &rlay->stats[proc_id];
		cur->cnt += cur->last;
		cur->tick++;
		cur->avg = (cur->last + cur->avg) / 2;
		cur->last_hour += cur->last;
		if ((cur->tick % (3600 / env->statinterval.tv_sec)) == 0) {
			cur->avg_hour = (cur->last_hour + cur->avg_hour) / 2;
			resethour++;
		}
		cur->last_day += cur->last;
		if ((cur->tick % (86400 / env->statinterval.tv_sec)) == 0) {
			cur->avg_day = (cur->last_day + cur->avg_day) / 2;
			resethour++;
		}
		bcopy(cur, &crs, sizeof(crs));

		cur->last = 0;
		if (resethour)
			cur->last_hour = 0;
		if (resetday)
			cur->last_day = 0;

		crs.id = rlay->conf.id;
		crs.proc = proc_id;
		imsg_compose(ibuf_pfe, IMSG_STATISTICS, 0, 0,
		    &crs, sizeof(crs));

		for (con = TAILQ_FIRST(&rlay->sessions);
		    con != NULL; con = next_con) {
			next_con = TAILQ_NEXT(con, entry);
			timersub(&tv_now, &con->tv_last, &tv);
			if (timercmp(&tv, &rlay->conf.timeout, >=))
				relay_close(con, "hard timeout");
		}
	}

	/* Schedule statistics timer */
	evtimer_set(&env->statev, relay_statistics, NULL);
	bcopy(&env->statinterval, &tv, sizeof(tv));
	evtimer_add(&env->statev, &tv);
}

void
relay_launch(void)
{
	struct relay	*rlay;

	TAILQ_FOREACH(rlay, &env->relays, entry) {
		log_debug("relay_launch: running relay %s", rlay->conf.name);

		rlay->up = HOST_UP;

		event_set(&rlay->ev, rlay->s, EV_READ|EV_PERSIST,
		    relay_accept, rlay);
		event_add(&rlay->ev, NULL);
	}
}

int
relay_socket(struct sockaddr_storage *ss, in_port_t port,
    struct protocol *proto)
{
	int s = -1, val;
	struct linger lng;

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
	}

	if ((s = socket(ss->ss_family, SOCK_STREAM, IPPROTO_TCP)) == -1)
		goto bad;

	/*
	 * Socket options
	 */
	bzero(&lng, sizeof(lng));
	if (setsockopt(s, SOL_SOCKET, SO_LINGER, &lng, sizeof(lng)) == -1)
		goto bad;
	val = 1;
	if (setsockopt(s, SOL_SOCKET, SO_REUSEPORT, &val, sizeof(int)) == -1)
		goto bad;
	if (fcntl(s, F_SETFL, O_NONBLOCK) == -1)
		goto bad;
	if (proto->tcpflags & TCPFLAG_BUFSIZ) {
		val = proto->tcpbufsiz;
		if (setsockopt(s, SOL_SOCKET, SO_RCVBUF,
		    &val, sizeof(val)) == -1)
			goto bad;
		val = proto->tcpbufsiz;
		if (setsockopt(s, SOL_SOCKET, SO_SNDBUF,
		    &val, sizeof(val)) == -1)
			goto bad;
	}

	/*
	 * IP options
	 */
	if (proto->tcpflags & TCPFLAG_IPTTL) {
		val = (int)proto->tcpipttl;
		if (setsockopt(s, IPPROTO_IP, IP_TTL,
		    &val, sizeof(val)) == -1)
			goto bad;
	}
	if (proto->tcpflags & TCPFLAG_IPMINTTL) {
		val = (int)proto->tcpipminttl;
		if (setsockopt(s, IPPROTO_IP, IP_MINTTL,
		    &val, sizeof(val)) == -1)
			goto bad;
	}

	/*
	 * TCP options
	 */
	if (proto->tcpflags & (TCPFLAG_NODELAY|TCPFLAG_NNODELAY)) {
		if (proto->tcpflags & TCPFLAG_NNODELAY)
			val = 0;
		else
			val = 1;
		if (setsockopt(s, IPPROTO_TCP, TCP_NODELAY,
		    &val, sizeof(val)) == -1)
			goto bad;
	}
	if (proto->tcpflags & (TCPFLAG_SACK|TCPFLAG_NSACK)) {
		if (proto->tcpflags & TCPFLAG_NSACK)
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
relay_socket_connect(struct sockaddr_storage *ss, in_port_t port,
    struct protocol *proto)
{
	int s;

	if ((s = relay_socket(ss, port, proto)) == -1)
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

int
relay_socket_listen(struct sockaddr_storage *ss, in_port_t port,
    struct protocol *proto)
{
	int s;

	if ((s = relay_socket(ss, port, proto)) == -1)
		return (-1);

	if (bind(s, (struct sockaddr *)ss, ss->ss_len) == -1)
		goto bad;
	if (listen(s, proto->tcpbacklog) == -1)
		goto bad;

	return (s);

 bad:
	close(s);
	return (-1);
}

void
relay_connected(int fd, short sig, void *arg)
{
	struct session		*con = (struct session *)arg;
	struct relay		*rlay = (struct relay *)con->relay;
	struct protocol		*proto = rlay->proto;
	evbuffercb		 outrd = relay_read;
	evbuffercb		 outwr = relay_write;
	struct bufferevent	*bev;

	if (sig == EV_TIMEOUT) {
		relay_close(con, "connect timeout");
		return;
	}

	DPRINTF("relay_connected: session %d: %ssuccessful",
	    con->id, rlay->proto->lateconnect ? "late connect " : "");

	switch (rlay->proto->type) {
	case RELAY_PROTO_HTTP:
		/* Check the servers's HTTP response */
		if (!RB_EMPTY(&rlay->proto->response_tree)) {
			outrd = relay_read_http;
			if ((con->out.nodes = calloc(proto->response_nodes,
			    sizeof(u_int8_t))) == NULL) {
				relay_close(con, "failed to allocate nodes");
				return;
			}
		}
		break;
	case RELAY_PROTO_TCP:
		/* Use defaults */
		break;
	default:
		fatalx("relay_input: unknown protocol");
	}

	/*
	 * Relay <-> Server
	 */
	bev = bufferevent_new(fd, outrd, outwr, relay_error, &con->out);
	if (bev == NULL) {
		relay_close(con, "failed to allocate output buffer event");
		return;
	}
	evbuffer_free(bev->output);
	bev->output = con->out.output;
	if (bev->output == NULL)
		fatal("relay_connected: invalid output buffer");

	con->out.bev = bev;
	bufferevent_settimeout(bev,
	    rlay->conf.timeout.tv_sec, rlay->conf.timeout.tv_sec);
	bufferevent_enable(bev, EV_READ|EV_WRITE);
}

void
relay_input(struct session *con)
{
	struct relay	*rlay = (struct relay *)con->relay;
	struct protocol *proto = rlay->proto;
	evbuffercb	 inrd = relay_read;
	evbuffercb	 inwr = relay_write;

	switch (rlay->proto->type) {
	case RELAY_PROTO_HTTP:
		/* Check the client's HTTP request */
		if (!RB_EMPTY(&rlay->proto->request_tree) ||
		    proto->lateconnect) {
			inrd = relay_read_http;
			if ((con->in.nodes = calloc(proto->request_nodes,
			    sizeof(u_int8_t))) == NULL) {
				relay_close(con, "failed to allocate nodes");
				return;
			}
		}
		break;
	case RELAY_PROTO_TCP:
		/* Use defaults */
		break;
	default:
		fatalx("relay_input: unknown protocol");
	}

	/*
	 * Client <-> Relay
	 */
	con->in.bev = bufferevent_new(con->in.s, inrd, inwr,
	    relay_error, &con->in);
	if (con->in.bev == NULL) {
		relay_close(con, "failed to allocate input buffer event");
		return;
	}

	/* Initialize the SSL wrapper */
	if ((rlay->conf.flags & F_SSL) && con->in.ssl != NULL)
		relay_ssl_connected(&con->in);

	bufferevent_settimeout(con->in.bev,
	    rlay->conf.timeout.tv_sec, rlay->conf.timeout.tv_sec);
	bufferevent_enable(con->in.bev, EV_READ|EV_WRITE);
}

void
relay_write(struct bufferevent *bev, void *arg)
{
	struct ctl_relay_event	*cre = (struct ctl_relay_event *)arg;
	struct session		*con = (struct session *)cre->con;
	if (gettimeofday(&con->tv_last, NULL))
		con->done = 1;
	if (con->done)
		relay_close(con, "last write (done)");
}

void
relay_read(struct bufferevent *bev, void *arg)
{
	struct ctl_relay_event	*cre = (struct ctl_relay_event *)arg;
	struct session		*con = (struct session *)cre->con;
	struct evbuffer		*src = EVBUFFER_INPUT(bev);

	if (gettimeofday(&con->tv_last, NULL))
		goto done;
	if (!EVBUFFER_LENGTH(src))
		return;
	if (relay_bufferevent_write_buffer(cre->dst, src) == -1)
		goto fail;
	if (con->done)
		goto done;
	bufferevent_enable(con->in.bev, EV_READ);
	return;
 done:
	relay_close(con, "last read (done)");
	return;
 fail:
	relay_close(con, strerror(errno));
}

char *
relay_expand_http(struct ctl_relay_event *cre, char *val, char *buf, size_t len)
{
	struct session	*con = (struct session *)cre->con;
	struct relay	*rlay = (struct relay *)con->relay;
	char		 ibuf[128];

	(void)strlcpy(buf, val, len);

	if (strstr(val, "$REMOTE_") != NULL) {
		if (strstr(val, "$REMOTE_ADDR") != NULL) {
			if (relay_host(&cre->ss, ibuf, sizeof(ibuf)) == NULL)
				return (NULL);
			if (expand_string(buf, len,
			    "$REMOTE_ADDR", ibuf) != 0)
				return (NULL);
		}
		if (strstr(val, "$REMOTE_PORT") != NULL) {
			snprintf(ibuf, sizeof(ibuf), "%u", ntohs(cre->port));
			if (expand_string(buf, len,
			    "$REMOTE_PORT", ibuf) != 0)
				return (NULL);
		}
	}
	if (strstr(val, "$SERVER_") != NULL) {
		if (strstr(val, "$SERVER_ADDR") != NULL) {
			if (relay_host(&rlay->conf.ss,
			    ibuf, sizeof(ibuf)) == NULL)
				return (NULL);
			if (expand_string(buf, len,
			    "$SERVER_ADDR", ibuf) != 0)
				return (NULL);
		}
		if (strstr(val, "$SERVER_PORT") != NULL) {
			snprintf(ibuf, sizeof(ibuf), "%u",
			    ntohs(rlay->conf.port));
			if (expand_string(buf, len,
			    "$SERVER_PORT", ibuf) != 0)
				return (NULL);
		}
	}
	if (strstr(val, "$TIMEOUT") != NULL) {
		snprintf(ibuf, sizeof(ibuf), "%lu", rlay->conf.timeout.tv_sec);
		if (expand_string(buf, len, "$TIMEOUT", ibuf) != 0)
			return (NULL);
	}

	return (buf);
}


int
relay_handle_http(struct ctl_relay_event *cre, struct protonode *pn,
    struct protonode *pk, int header)
{
	struct session		*con = (struct session *)cre->con;
	char			 buf[READ_BUF_SIZE], *ptr;
	int			 ret = PN_DROP;

	switch (pn->action) {
	case NODE_ACTION_APPEND:
		if (!header || ((pn->flags & PNFLAG_MARK) && cre->marked == 0))
			return (PN_PASS);
		ptr = pn->value;
		if ((pn->flags & PNFLAG_MACRO) &&
		    (ptr = relay_expand_http(cre, pn->value,
		    buf, sizeof(buf))) == NULL)
			break;
		if (relay_bufferevent_print(cre->dst, pn->key) == -1 ||
		    relay_bufferevent_print(cre->dst, ": ") == -1 ||
		    relay_bufferevent_print(cre->dst, pk->value) == -1 ||
		    relay_bufferevent_print(cre->dst, ", ") == -1 ||
		    relay_bufferevent_print(cre->dst, ptr) == -1 ||
		    relay_bufferevent_print(cre->dst, "\r\n") == -1)
			goto fail;
		cre->nodes[pn->id] = 1;
		DPRINTF("relay_handle_http: append '%s: %s, %s'",
		    pk->key, pk->value, ptr);
		break;
	case NODE_ACTION_CHANGE:
	case NODE_ACTION_REMOVE:
		if (!header || ((pn->flags & PNFLAG_MARK) && cre->marked == 0))
			return (PN_PASS);
		DPRINTF("relay_handle_http: change/remove '%s: %s'",
		    pk->key, pk->value);
		break;
	case NODE_ACTION_EXPECT:
		DPRINTF("relay_handle_http: expect '%s: %s'",
		    pn->key, pn->value);
		if (fnmatch(pn->value, pk->value, FNM_CASEFOLD) == 0) {
			if (pn->flags & PNFLAG_MARK)
				cre->marked++;
			cre->nodes[pn->id] = 1;
		}
		ret = PN_PASS;
		break;
	case NODE_ACTION_FILTER:
		DPRINTF("relay_handle_http: filter '%s: %s'",
		    pn->key, pn->value);
		if (fnmatch(pn->value, pk->value, FNM_CASEFOLD) == 0) {
			if (pn->flags & PNFLAG_MARK)
				cre->marked++;
			cre->nodes[pn->id] = 1;
		}
		break;
	case NODE_ACTION_HASH:
		if ((pn->flags & PNFLAG_MARK) && cre->marked == 0)
			return (PN_PASS);
		DPRINTF("relay_handle_http: hash '%s: %s'",
		    pn->key, pk->value);
		con->outkey = hash32_str(pk->value, con->outkey);
		ret = PN_PASS;
		break;
	case NODE_ACTION_LOG:
		if ((pn->flags & PNFLAG_MARK) && cre->marked == 0)
			return (PN_PASS);
		DPRINTF("relay_handle_http: log '%s: %s'",
		    pn->key, pk->value);
		ret = PN_PASS;
		break;
	case NODE_ACTION_NONE:
		return (PN_PASS);
	}
	if (pn->flags & PNFLAG_LOG) {
		bzero(buf, sizeof(buf));
		if (snprintf(buf, sizeof(buf), " [%s: %s]",
		    pk->key, pk->value) == -1 ||
		    evbuffer_add(con->log, buf, strlen(buf)) == -1)
			goto fail;
	}

	return (ret);
 fail:
	relay_close(con, strerror(errno));
	return (PN_FAIL);
}

void
relay_read_httpcontent(struct bufferevent *bev, void *arg)
{
	struct ctl_relay_event	*cre = (struct ctl_relay_event *)arg;
	struct session		*con = (struct session *)cre->con;
	struct evbuffer		*src = EVBUFFER_INPUT(bev);
	size_t			 size;

	if (gettimeofday(&con->tv_last, NULL))
		goto done;
	size = EVBUFFER_LENGTH(src);
	DPRINTF("relay_read_httpcontent: size %d, to read %d",
	    size, cre->toread);
	if (!size)
		return;
	if (relay_bufferevent_write_buffer(cre->dst, src) == -1)
		goto fail;
	if (size >= cre->toread)
		bev->readcb = relay_read_http;
	cre->toread -= size;
	DPRINTF("relay_read_httpcontent: done, size %d, to read %d",
	    size, cre->toread);
	if (con->done)
		goto done;
	if (EVBUFFER_LENGTH(src) && bev->readcb != relay_read_httpcontent)
		bev->readcb(bev, arg);
	bufferevent_enable(bev, EV_READ);
	return;
 done:
	relay_close(con, "last http content read (done)");
	return;
 fail:
	relay_close(con, strerror(errno));
}

void
relay_read_httpchunks(struct bufferevent *bev, void *arg)
{
	struct ctl_relay_event	*cre = (struct ctl_relay_event *)arg;
	struct session		*con = (struct session *)cre->con;
	struct evbuffer		*src = EVBUFFER_INPUT(bev);
	char			*line, *ep;
	long			 lval;
	size_t			 size;

	if (gettimeofday(&con->tv_last, NULL))
		goto done;
	size = EVBUFFER_LENGTH(src);
	DPRINTF("relay_read_httpchunks: size %d, to read %d",
	    size, cre->toread);
	if (!size)
		return;

	if (!cre->toread) {
		line = evbuffer_readline(src);
		if (line == NULL) {
			relay_close(con, "invalid chunk");
			return;
		}

		/* Read prepended chunk size in hex */
		errno = 0;
		lval = strtol(line, &ep, 16);
		if (line[0] == '\0' || *ep != '\0') {
			free(line);
			relay_close(con, "invalid chunk size");
			return;
		}
		if (errno == ERANGE &&
		    (lval == LONG_MAX || lval == LONG_MIN)) {
			free(line);
			relay_close(con, "chunk size out of range");
			return;
		}
		if (relay_bufferevent_print(cre->dst, line) == -1 ||
		    relay_bufferevent_print(cre->dst, "\r\n") == -1)
			goto fail;
		free(line);

		/* Last chunk is 0 bytes followed by an empty newline */
		if ((cre->toread = lval) == 0) {
			line = evbuffer_readline(src);
			if (line == NULL) {
				relay_close(con, "invalid chunk");
				return;
			}
			free(line);
			if (relay_bufferevent_print(cre->dst, "\r\n") == -1)
				goto fail;

			/* Switch to HTTP header mode */
			bev->readcb = relay_read_http;
		}
	} else {
		/* Read chunk data */
		if (size > cre->toread)
			size = cre->toread;
		if (relay_bufferevent_write_chunk(cre->dst, src, size) == -1)
			goto fail;
		cre->toread -= size;
		DPRINTF("relay_read_httpchunks: done, size %d, to read %d",
		    size, cre->toread);

		if (cre->toread == 0) {
			/* Chunk is terminated by an empty newline */
			line = evbuffer_readline(src);
			if (line == NULL || strlen(line)) {
				if (line != NULL)
					free(line);
				relay_close(con, "invalid chunk");
				return;
			}
			free(line);
			if (relay_bufferevent_print(cre->dst, "\r\n\r\n") == -1)
				goto fail;
		}
	}

	if (con->done)
		goto done;
	if (EVBUFFER_LENGTH(src) && bev->readcb != relay_read_httpchunks)
		bev->readcb(bev, arg);
	bufferevent_enable(bev, EV_READ);
	return;

 done:
	relay_close(con, "last http chunk read (done)");
	return;
 fail:
	relay_close(con, strerror(errno));
}

void
relay_read_http(struct bufferevent *bev, void *arg)
{
	struct ctl_relay_event	*cre = (struct ctl_relay_event *)arg;
	struct session		*con = (struct session *)cre->con;
	struct relay		*rlay = (struct relay *)con->relay;
	struct protocol		*proto = rlay->proto;
	struct evbuffer		*src = EVBUFFER_INPUT(bev);
	struct protonode	*pn, pk, *pnv, pkv;
	char			*line, buf[READ_BUF_SIZE], *ptr, *val;
	int			 header = 0, ret;
	const char		*errstr;
	size_t			 size;

	if (gettimeofday(&con->tv_last, NULL))
		goto done;
	size = EVBUFFER_LENGTH(src);
	DPRINTF("relay_read_http: size %d, to read %d", size, cre->toread);
	if (!size)
		return;

	pk.type = NODE_TYPE_HEADER;

	while (!cre->done && (line = evbuffer_readline(src)) != NULL) {
		/*
		 * An empty line indicates the end of the request.
		 * libevent already stripped the \r\n for us.
		 */
		if (!strlen(line)) {
			cre->done = 1;
			free(line);
			break;
		}
		pk.key = line;

		/*
		 * The first line is the GET/POST/PUT/... request,
		 * subsequent lines are HTTP headers.
		 */
		if (++cre->line == 1) {
			pk.value = strchr(pk.key, ' ');
		} else
			pk.value = strchr(pk.key, ':');
		if (pk.value == NULL || strlen(pk.value) < 3) {
			DPRINTF("relay_read_http: request '%s'", line);
			/* Append line to the output buffer */
			if (relay_bufferevent_print(cre->dst, line) == -1 ||
			    relay_bufferevent_print(cre->dst, "\r\n") == -1) {
				free(line);
				goto fail;
			}
			free(line);
			continue;
		}
		if (*pk.value == ':') {
			*pk.value++ = '\0';
			pk.value++;
			header = 1;
		} else {
			*pk.value++ = '\0';
			header = 0;
		}

		DPRINTF("relay_read_http: header '%s: %s'", pk.key, pk.value);

		/*
		 * Identify and handle specific HTTP request methods
		 */
		if (cre->line == 1) {
			if (cre->dir == RELAY_DIR_RESPONSE) {
				cre->method = HTTP_METHOD_RESPONSE;
				goto lookup;
			} else if (strcmp("GET", pk.key) == 0)
				cre->method = HTTP_METHOD_GET;
			else if (strcmp("HEAD", pk.key) == 0)
				cre->method = HTTP_METHOD_HEAD;
			else if (strcmp("POST", pk.key) == 0)
				cre->method = HTTP_METHOD_POST;
			else if (strcmp("PUT", pk.key) == 0)
				cre->method = HTTP_METHOD_PUT;
			else if (strcmp("DELETE", pk.key) == 0)
				cre->method = HTTP_METHOD_DELETE;
			else if (strcmp("OPTIONS", pk.key) == 0)
				cre->method = HTTP_METHOD_OPTIONS;
			else if (strcmp("TRACE", pk.key) == 0)
				cre->method = HTTP_METHOD_TRACE;
			else if (strcmp("CONNECT", pk.key) == 0)
				cre->method = HTTP_METHOD_CONNECT;

			/*
			 * Decode the URL
			 */
			cre->path = strdup(pk.value);
			if (cre->path == NULL) {
				free(line);
				goto fail;
			}
			cre->version = strchr(cre->path, ' ');
			if (cre->version != NULL)
				*cre->version++ = '\0';
			cre->args = strchr(cre->path, '?');
			if (cre->args != NULL)
				*cre->args++ = '\0';
#ifdef DEBUG
			if (snprintf(buf, sizeof(buf), " \"%s\"",
			    cre->path) == -1 ||
			    evbuffer_add(con->log, buf, strlen(buf)) == -1) {
				free(line);
				goto fail;
			}
#endif

			/*
			 * Lookup protocol handlers in the URL path
			 */
			if ((proto->flags & F_LOOKUP_PATH) == 0)
				goto lookup;

			pkv.key = cre->path;
			pkv.type = NODE_TYPE_PATH;
			pkv.value = cre->args == NULL ? "" : cre->args;

			DPRINTF("relay_read_http: "
			    "lookup path '%s: %s'", pkv.key, pkv.value);

			if ((pnv = RB_FIND(proto_tree,
			    cre->tree, &pkv)) == NULL)
				goto lookup;

			ret = relay_handle_http(cre, pnv, &pkv, 0);
			if (ret == PN_FAIL) {
				free(line);
				goto fail;
			}
		} else if ((cre->method == HTTP_METHOD_POST ||
		    cre->method == HTTP_METHOD_PUT ||
		    cre->method == HTTP_METHOD_RESPONSE) &&
		    strcasecmp("Content-Length", pk.key) == 0) {
			/*
			 * Need to read data from the client after the
			 * HTTP header.
			 * XXX What about non-standard clients not using
			 * the carriage return? And some browsers seem to
			 * include the line length in the content-length.
			 */
			cre->toread = strtonum(pk.value, 1, INT_MAX, &errstr);

			if (errstr) {
				relay_close(con, errstr);
				free(line);
				return;
			}
		}
 lookup:
		if (strcasecmp("Transfer-Encoding", pk.key) == 0 &&
		    strcasecmp("chunked", pk.value) == 0)
			cre->chunked = 1;

		/* Match the HTTP header */
		if ((pn = RB_FIND(proto_tree, cre->tree, &pk)) == NULL)
			goto next;

		if (cre->dir == RELAY_DIR_RESPONSE)
			goto handle;

		if (pn->flags & PNFLAG_LOOKUP_URL) {
			if (cre->path == NULL || cre->args == NULL ||
			    strlen(cre->args) < 2 ||
			    (val = strdup(cre->args)) == NULL)
				goto next;
			ptr = val;
			while (ptr != NULL && strlen(ptr)) {
				pkv.key = ptr;
				pkv.type = NODE_TYPE_URL;
				if ((ptr = strchr(ptr, '&')) != NULL)
					*ptr++ = '\0';
				if ((pkv.value =
				    strchr(pkv.key, '=')) == NULL ||
				    strlen(pkv.value) < 1)
					continue;
				*pkv.value++ = '\0';

				if ((pnv = RB_FIND(proto_tree,
				    cre->tree, &pkv)) == NULL)
					continue;
				ret = relay_handle_http(cre, pnv, &pkv, 0);
				if (ret == PN_PASS)
					continue;
				else if (ret == PN_FAIL) {
					free(val);
					free(line);
					return;
				}
			}
			free(val);
		} else if (pn->flags & PNFLAG_LOOKUP_COOKIE) {
			/*
			 * Decode the HTTP cookies
			 */
			val = strdup(pk.value);
			if (val == NULL)
				goto next;

			for (ptr = val; ptr != NULL && strlen(ptr);) {
				if (*ptr == ' ')
					*ptr++ = '\0';
				pkv.key = ptr;
				pkv.type = NODE_TYPE_COOKIE;
				if ((ptr = strchr(ptr, ';')) != NULL)
					*ptr++ = '\0';
				/*
				 * XXX We do not handle attributes
				 * ($Path, $Domain, or $Port)
				 */
				if (*pkv.key == '$')
					continue;

				if ((pkv.value =
				    strchr(pkv.key, '=')) == NULL ||
				    strlen(pkv.value) < 1)
					continue;
				*pkv.value++ = '\0';
				if (*pkv.value == '"')
					*pkv.value++ = '\0';
				if (pkv.value[strlen(pkv.value) - 1] == '"')
					pkv.value[strlen(pkv.value) - 1] = '\0';

				if ((pnv = RB_FIND(proto_tree,
				    cre->tree, &pkv)) == NULL)
					continue;
				ret = relay_handle_http(cre, pnv, &pkv, 0);
				if (ret == PN_PASS)
					continue;
				else if (ret == PN_FAIL) {
					free(val);
					free(line);
					return;
				}
			}
			free(val);
		}

 handle:
		ret = relay_handle_http(cre, pn, &pk, header);
		if (ret == PN_PASS)
			goto next;
		free(line);
		if (ret == PN_FAIL)
			return;
		continue;

 next:
		if (relay_bufferevent_print(cre->dst, pk.key) == -1 ||
		    relay_bufferevent_print(cre->dst,
		    header ? ": " : " ") == -1 ||
		    relay_bufferevent_print(cre->dst, pk.value) == -1 ||
		    relay_bufferevent_print(cre->dst, "\r\n") == -1) {
			free(line);
			goto fail;
		}
		free(line);
		continue;
	}
	if (cre->done) {
		RB_FOREACH(pn, proto_tree, cre->tree) {
			switch (pn->action) {
			case NODE_ACTION_FILTER:
				if (!cre->nodes[pn->id])
					continue;
				cre->nodes[pn->id] = 0;
				break;
			default:
				if (cre->nodes[pn->id]) {
					cre->nodes[pn->id] = 0;
					continue;
				}
				break;
			}
			switch (pn->action) {
			case NODE_ACTION_APPEND:
			case NODE_ACTION_CHANGE:
				ptr = pn->value;
				if ((pn->flags & PNFLAG_MARK) &&
				    cre->marked == 0)
					break;
				if ((pn->flags & PNFLAG_MACRO) &&
				    (ptr = relay_expand_http(cre, pn->value,
				    buf, sizeof(buf))) == NULL)
					break;
				if (relay_bufferevent_print(cre->dst,
				    pn->key) == -1 ||
				    relay_bufferevent_print(cre->dst,
				    ": ") == -1 ||
				    relay_bufferevent_print(cre->dst,
				    ptr) == -1 ||
				    relay_bufferevent_print(cre->dst,
				    "\r\n") == -1)
					goto fail;
				DPRINTF("relay_read_http: add '%s: %s'",
				    pn->key, ptr);
				break;
			case NODE_ACTION_EXPECT:
				if (pn->flags & PNFLAG_MARK)
					break;
				DPRINTF("relay_read_http: missing '%s: %s'",
				    pn->key, pn->value);
				relay_close(con, "incomplete header (done)");
				return;
			case NODE_ACTION_FILTER:
				if (pn->flags & PNFLAG_MARK)
					break;
				DPRINTF("relay_read_http: filtered '%s: %s'",
				    pn->key, pn->value);
				relay_close(con, "rejecting header (done)");
				return;
			default:
				break;
			}
		}

		switch (cre->method) {
		case HTTP_METHOD_CONNECT:
			/* Data stream */
			bev->readcb = relay_read;
			break;
		case HTTP_METHOD_POST:
		case HTTP_METHOD_PUT:
		case HTTP_METHOD_RESPONSE:
			/* HTTP request payload */
			if (cre->toread) {
				bev->readcb = relay_read_httpcontent;
				break;
			}
			/* FALLTHROUGH */
		default:
			/* HTTP handler */
			bev->readcb = relay_read_http;
			break;
		}
		if (cre->chunked) {
			/* Chunked transfer encoding */
			cre->toread = 0;
			bev->readcb = relay_read_httpchunks;
		}

		/* Write empty newline and switch to relay mode */
		if (relay_bufferevent_print(cre->dst, "\r\n") == -1)
			goto fail;
		cre->line = 0;
		cre->method = 0;
		cre->marked = 0;
		cre->done = 0;
		cre->chunked = 0;

		if (cre->dir == RELAY_DIR_REQUEST &&
		    proto->lateconnect && cre->dst->bev == NULL &&
		    relay_connect(con) == -1) {
			relay_close(con, "session failed");
			return;
		}
	}
	if (con->done)
		goto done;
	if (EVBUFFER_LENGTH(src) && bev->readcb != relay_read_http)
		bev->readcb(bev, arg);
	bufferevent_enable(bev, EV_READ);
	return;
 done:
	relay_close(con, "last http read (done)");
	return;
 fail:
	relay_close(con, strerror(errno));
}

void
relay_error(struct bufferevent *bev, short error, void *arg)
{
	struct ctl_relay_event *cre = (struct ctl_relay_event *)arg;
	struct session *con = (struct session *)cre->con;
	struct evbuffer *dst;

	if (error & EVBUFFER_TIMEOUT) {
		relay_close(con, "buffer event timeout");
		return;
	}
	if (error & (EVBUFFER_READ|EVBUFFER_WRITE|EVBUFFER_EOF)) {
		bufferevent_disable(bev, EV_READ|EV_WRITE);

		con->done = 1;
		if (cre->dst->bev != NULL) {
			dst = EVBUFFER_OUTPUT(cre->dst->bev);
			if (EVBUFFER_LENGTH(dst))
				return;
		}

		relay_close(con, "done");
		return;
	}
	relay_close(con, "buffer event error");
}

const char *
relay_host(struct sockaddr_storage *ss, char *buf, size_t len)
{
	int af = ss->ss_family;
	void *ptr;

	bzero(buf, len);
	if (af == AF_INET)
		ptr = &((struct sockaddr_in *)ss)->sin_addr;
	else
		ptr = &((struct sockaddr_in6 *)ss)->sin6_addr;
	return (inet_ntop(af, ptr, buf, len));
}

void
relay_accept(int fd, short sig, void *arg)
{
	struct relay *rlay = (struct relay *)arg;
	struct protocol *proto = rlay->proto;
	struct session *con = NULL;
	struct ctl_natlook *cnl = NULL;
	socklen_t slen;
	struct timeval tv;
	struct sockaddr_storage ss;
	int s = -1;

	slen = sizeof(ss);
	if ((s = accept(fd, (struct sockaddr *)&ss, (socklen_t *)&slen)) == -1)
		return;

	if (relay_sessions >= RELAY_MAX_SESSIONS ||
	    rlay->conf.flags & F_DISABLE)
		goto err;

	if ((con = (struct session *)
	    calloc(1, sizeof(struct session))) == NULL)
		goto err;

	con->in.s = s;
	con->in.ssl = NULL;
	con->out.s = -1;
	con->out.ssl = NULL;
	con->in.dst = &con->out;
	con->out.dst = &con->in;
	con->in.con = con;
	con->out.con = con;
	con->relay = rlay;
	con->id = ++relay_conid;
	con->outkey = rlay->dstkey;
	con->in.tree = &proto->request_tree;
	con->out.tree = &proto->response_tree;
	con->in.dir = RELAY_DIR_REQUEST;
	con->out.dir = RELAY_DIR_RESPONSE;
	con->retry = rlay->conf.dstretry;
	if (gettimeofday(&con->tv_start, NULL))
		goto err;
	bcopy(&con->tv_start, &con->tv_last, sizeof(con->tv_last));
	bcopy(&ss, &con->in.ss, sizeof(con->in.ss));

	/* Pre-allocate output buffer */
	con->out.output = evbuffer_new();
	if (con->out.output == NULL) {
		relay_close(con, "failed to allocate output buffer");
		return;
	}

	/* Pre-allocate log buffer */
	con->log = evbuffer_new();
	if (con->log == NULL) {
		relay_close(con, "failed to allocate log buffer");
		return;
	}

	if (rlay->conf.flags & F_NATLOOK) {
		if ((cnl = (struct ctl_natlook *)
		    calloc(1, sizeof(struct ctl_natlook))) == NULL)
			goto err;
	}

	relay_sessions++;
	TAILQ_INSERT_HEAD(&rlay->sessions, con, entry);

	/* Increment the per-relay session counter */
	rlay->stats[proc_id].last++;

	if (rlay->conf.flags & F_NATLOOK && cnl != NULL) {
		con->cnl = cnl;;
		bzero(cnl, sizeof(*cnl));
		cnl->in = -1;
		cnl->id = con->id;
		cnl->proc = proc_id;
		bcopy(&con->in.ss, &cnl->src, sizeof(cnl->src));
		bcopy(&rlay->conf.ss, &cnl->dst, sizeof(cnl->dst));
		imsg_compose(ibuf_pfe, IMSG_NATLOOK, 0, 0, cnl, sizeof(*cnl));

		/* Schedule timeout */
		evtimer_set(&con->ev, relay_natlook, con);
		bcopy(&rlay->conf.timeout, &tv, sizeof(tv));
		evtimer_add(&con->ev, &tv);
		return;
	}

	relay_session(con);
	return;
 err:
	if (s != -1) {
		close(s);
		if (con != NULL)
			free(con);
	}
}

u_int32_t
relay_hash_addr(struct sockaddr_storage *ss, u_int32_t p)
{
	struct sockaddr_in	*sin4;
	struct sockaddr_in6	*sin6;

	if (ss->ss_family == AF_INET) {
		sin4 = (struct sockaddr_in *)ss;
		p = hash32_buf(&sin4->sin_addr,
		    sizeof(struct in_addr), p);
	} else {
		sin6 = (struct sockaddr_in6 *)ss;
		p = hash32_buf(&sin6->sin6_addr,
		    sizeof(struct in6_addr), p);
	}

	return (p);
}

int
relay_from_table(struct session *con)
{
	struct relay		*rlay = (struct relay *)con->relay;
	struct host		*host;
	struct table		*table = rlay->dsttable;
	u_int32_t		 p = con->outkey;
	int			 idx = 0;

	if (rlay->conf.dstcheck && !table->up) {
		log_debug("relay_from_table: no active hosts");
		return (-1);
	}

	switch (rlay->conf.dstmode) {
	case RELAY_DSTMODE_ROUNDROBIN:
		if ((int)rlay->dstkey >= rlay->dstnhosts)
			rlay->dstkey = 0;
		idx = (int)rlay->dstkey++;
		break;
	case RELAY_DSTMODE_LOADBALANCE:
		p = relay_hash_addr(&con->in.ss, p);
		/* FALLTHROUGH */
	case RELAY_DSTMODE_HASH:
		p = relay_hash_addr(&rlay->conf.ss, p);
		p = hash32_buf(&rlay->conf.port, sizeof(rlay->conf.port), p);
		if ((idx = p % rlay->dstnhosts) >= RELAY_MAXHOSTS)
			return (-1);
	}
	host = rlay->dsthost[idx];
	DPRINTF("relay_from_table: host %s, p 0x%08x, idx %d",
	    host->conf.name, p, idx);
	while (host != NULL) {
		DPRINTF("relay_from_table: host %s", host->conf.name);
		if (!rlay->conf.dstcheck || host->up == HOST_UP)
			goto found;
		host = TAILQ_NEXT(host, entry);
	}
	TAILQ_FOREACH(host, &rlay->dsttable->hosts, entry) {
		DPRINTF("relay_from_table: next host %s", host->conf.name);
		if (!rlay->conf.dstcheck || host->up == HOST_UP)
			goto found;
	}

	/* Should not happen */
	fatalx("relay_from_table: no active hosts, desynchronized");

 found:
	con->retry = host->conf.retry;
	con->out.port = table->conf.port;
	bcopy(&host->conf.ss, &con->out.ss, sizeof(con->out.ss));

	return (0);
}

void
relay_natlook(int fd, short event, void *arg)
{
	struct session		*con = (struct session *)arg;
	struct relay		*rlay = (struct relay *)con->relay;
	struct ctl_natlook	*cnl = con->cnl;

	if (cnl == NULL)
		fatalx("invalid NAT lookup");

	if (con->out.ss.ss_family == AF_UNSPEC && cnl->in == -1 &&
	    rlay->conf.dstss.ss_family == AF_UNSPEC && rlay->dsttable == NULL) {
		relay_close(con, "session NAT lookup failed");
		return;
	}
	if (cnl->in != -1) {
		bcopy(&cnl->rdst, &con->out.ss, sizeof(con->out.ss));
		con->out.port = cnl->rdport;
	}
	free(con->cnl);
	con->cnl = NULL;

	relay_session(con);
}

void
relay_session(struct session *con)
{
	struct relay	*rlay = (struct relay *)con->relay;

	if (bcmp(&rlay->conf.ss, &con->out.ss, sizeof(con->out.ss)) == 0 &&
	    con->out.port == rlay->conf.port) {
		log_debug("relay_session: session %d: looping",
		    con->id);
		relay_close(con, "session aborted");
		return;
	}

	if ((rlay->conf.flags & F_SSL) && (con->in.ssl == NULL)) {
		relay_ssl_transaction(con);
		return;
	}

	if (!rlay->proto->lateconnect && relay_connect(con) == -1) {
		relay_close(con, "session failed");
		return;
	}

	relay_input(con);
}

int
relay_connect(struct session *con)
{
	struct relay	*rlay = (struct relay *)con->relay;

	if (gettimeofday(&con->tv_start, NULL))
		return (-1);

	if (rlay->dsttable != NULL) {
		if (relay_from_table(con) != 0)
			return (-1);
	} else if (con->out.ss.ss_family == AF_UNSPEC) {
		bcopy(&rlay->conf.dstss, &con->out.ss, sizeof(con->out.ss));
		con->out.port = rlay->conf.dstport;
	}

 retry:
	if ((con->out.s = relay_socket_connect(&con->out.ss, con->out.port,
	    rlay->proto)) == -1) {
		if (con->retry) {
			con->retry--;
			log_debug("relay_connect: session %d: "
			    "forward failed: %s, %s",
			    con->id, strerror(errno),
			    con->retry ? "next retry" : "last retry");
			goto retry;
		}
		log_debug("relay_connect: session %d: forward failed: %s",
		    con->id, strerror(errno));
		return (-1);
	}

	if (errno == EINPROGRESS)
		event_again(&con->ev, con->out.s, EV_WRITE|EV_TIMEOUT,
		    relay_connected, &con->tv_start, &env->timeout, con);
	else
		relay_connected(con->out.s, EV_WRITE, con);

	return (0);
}

void
relay_close(struct session *con, const char *msg)
{
	struct relay	*rlay = (struct relay *)con->relay;
	char		 ibuf[128], obuf[128], *ptr = NULL;

	TAILQ_REMOVE(&rlay->sessions, con, entry);

	event_del(&con->ev);
	if (con->in.bev != NULL)
		bufferevent_disable(con->in.bev, EV_READ|EV_WRITE);
	if (con->out.bev != NULL)
		bufferevent_disable(con->out.bev, EV_READ|EV_WRITE);

	if (env->opts & HOSTSTATED_OPT_LOGUPDATE) {
		bzero(&ibuf, sizeof(ibuf));
		bzero(&obuf, sizeof(obuf));
		(void)relay_host(&con->in.ss, ibuf, sizeof(ibuf));
		(void)relay_host(&con->out.ss, obuf, sizeof(obuf));
		if (EVBUFFER_LENGTH(con->log) &&
		    evbuffer_add_printf(con->log, "\r\n") != -1)
			ptr = evbuffer_readline(con->log);
		log_info("relay %s, session %d (%d active), %s -> %s:%d, "
		    "%s%s%s", rlay->conf.name, con->id, relay_sessions,
		    ibuf, obuf, ntohs(con->out.port), msg,
		    ptr == NULL ? "" : ",", ptr == NULL ? "" : ptr);
		if (ptr != NULL)
			free(ptr);
	}

	if (con->in.bev != NULL)
		bufferevent_free(con->in.bev);
	else if (con->in.output != NULL)
		evbuffer_free(con->in.output);
	if (con->in.ssl != NULL) {
		/* XXX handle non-blocking shutdown */
		if (SSL_shutdown(con->in.ssl) == 0)
			SSL_shutdown(con->in.ssl);
		SSL_free(con->in.ssl);
	}
	if (con->in.s != -1)
		close(con->in.s);
	if (con->in.path != NULL)
		free(con->in.path);
	if (con->in.buf != NULL)
		free(con->in.buf);
	if (con->in.nodes != NULL)
		free(con->in.nodes);

	if (con->out.bev != NULL)
		bufferevent_free(con->out.bev);
	else if (con->out.output != NULL)
		evbuffer_free(con->out.output);
	if (con->out.s != -1)
		close(con->out.s);
	if (con->out.path != NULL)
		free(con->out.path);
	if (con->out.buf != NULL)
		free(con->out.buf);
	if (con->out.nodes != NULL)
		free(con->out.nodes);

	if (con->log != NULL)
		evbuffer_free(con->log);

	if (con->cnl != NULL) {
#if 0
		imsg_compose(ibuf_pfe, IMSG_KILLSTATES, 0, 0,
		    cnl, sizeof(*cnl));
#endif
		free(con->cnl);
	}

	free(con);
	relay_sessions--;
}

void
relay_dispatch_pfe(int fd, short event, void *ptr)
{
	struct imsgbuf		*ibuf;
	struct imsg		 imsg;
	ssize_t			 n;
	struct session		*con;
	struct ctl_natlook	 cnl;
	struct timeval		 tv;
	struct host		*host;
	struct table		*table;
	struct ctl_status	 st;
	objid_t			 id;

	ibuf = ptr;
	switch (event) {
	case EV_READ:
		if ((n = imsg_read(ibuf)) == -1)
			fatal("relay_dispatch_pfe: imsg_read_error");
		if (n == 0)
			fatalx("relay_dispatch_pfe: pipe closed");
		break;
	case EV_WRITE:
		if (msgbuf_write(&ibuf->w) == -1)
			fatal("relay_dispatch_pfe: msgbuf_write");
		imsg_event_add(ibuf);
		return;
	default:
		fatalx("relay_dispatch_pfe: unknown event");
	}

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatal("relay_dispatch_pfe: imsg_read error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_HOST_DISABLE:
			memcpy(&id, imsg.data, sizeof(id));
			if ((host = host_find(env, id)) == NULL)
				fatalx("relay_dispatch_pfe: desynchronized");
			if ((table = table_find(env, host->conf.tableid)) ==
			    NULL)
				fatalx("relay_dispatch_pfe: invalid table id");
			if (host->up == HOST_UP)
				table->up--;
			host->flags |= F_DISABLE;
			host->up = HOST_UNKNOWN;
			break;
		case IMSG_HOST_ENABLE:
			memcpy(&id, imsg.data, sizeof(id));
			if ((host = host_find(env, id)) == NULL)
				fatalx("relay_dispatch_pfe: desynchronized");
			host->flags &= ~(F_DISABLE);
			host->up = HOST_UNKNOWN;
			break;
		case IMSG_HOST_STATUS:
			if (imsg.hdr.len - IMSG_HEADER_SIZE != sizeof(st))
				fatalx("relay_dispatch_pfe: invalid request");
			memcpy(&st, imsg.data, sizeof(st));
			if ((host = host_find(env, st.id)) == NULL)
				fatalx("relay_dispatch_pfe: invalid host id");
			if (host->flags & F_DISABLE)
				break;
			if (host->up == st.up) {
				log_debug("relay_dispatch_pfe: host %d => %d",
				    host->conf.id, host->up);
				fatalx("relay_dispatch_pfe: desynchronized");
			}

			if ((table = table_find(env, host->conf.tableid))
			    == NULL)
				fatalx("relay_dispatch_pfe: invalid table id");

			DPRINTF("relay_dispatch_pfe: [%d] state %d for "
			    "host %u %s", proc_id, st.up,
			    host->conf.id, host->conf.name);

			if ((st.up == HOST_UNKNOWN && host->up == HOST_DOWN) ||
			    (st.up == HOST_DOWN && host->up == HOST_UNKNOWN)) {
				host->up = st.up;
				break;
			}
			if (st.up == HOST_UP)
				table->up++;
			else
				table->up--;
			host->up = st.up;
			break;
		case IMSG_NATLOOK:
			bcopy(imsg.data, &cnl, sizeof(cnl));
			if ((con = session_find(env, cnl.id)) == NULL ||
			    con->cnl == NULL) {
				log_debug("relay_dispatch_pfe: "
				    "session expired");
				break;
			}
			bcopy(&cnl, con->cnl, sizeof(*con->cnl));
			evtimer_del(&con->ev);
			evtimer_set(&con->ev, relay_natlook, con);
			bzero(&tv, sizeof(tv));
			evtimer_add(&con->ev, &tv);
			break;
		default:
			log_debug("relay_dispatch_msg: unexpected imsg %d",
			    imsg.hdr.type);
			break;
		}
		imsg_free(&imsg);
	}
	imsg_event_add(ibuf);
}

void
relay_dispatch_parent(int fd, short event, void * ptr)
{
	struct imsgbuf	*ibuf;
	struct imsg	 imsg;
	ssize_t		 n;

	ibuf = ptr;
	switch (event) {
	case EV_READ:
		if ((n = imsg_read(ibuf)) == -1)
			fatal("relay_dispatch_parent: imsg_read error");
		if (n == 0)
			fatalx("relay_dispatch_parent: pipe closed");
		break;
	case EV_WRITE:
		if (msgbuf_write(&ibuf->w) == -1)
			fatal("relay_dispatch_parent: msgbuf_write");
		imsg_event_add(ibuf);
		return;
	default:
		fatalx("relay_dispatch_parent: unknown event");
	}

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatal("relay_dispatch_parent: imsg_read error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		default:
			log_debug("relay_dispatch_parent: unexpected imsg %d",
			    imsg.hdr.type);
			break;
		}
		imsg_free(&imsg);
	}
	imsg_event_add(ibuf);
}

SSL_CTX *
relay_ssl_ctx_create(struct relay *rlay)
{
	struct protocol *proto = rlay->proto;
	SSL_CTX *ctx;
	char certfile[PATH_MAX], hbuf[128];

	ctx = SSL_CTX_new(SSLv23_method());
	if (ctx == NULL)
		goto err;

	/* Modify session timeout and cache size*/
	SSL_CTX_set_timeout(ctx, rlay->conf.timeout.tv_sec);
	if (proto->cache < -1) {
		SSL_CTX_set_session_cache_mode(ctx, SSL_SESS_CACHE_OFF);
	} else if (proto->cache >= -1) {
		SSL_CTX_set_session_cache_mode(ctx, SSL_SESS_CACHE_SERVER);
		if (proto->cache >= 0)
			SSL_CTX_sess_set_cache_size(ctx, proto->cache);
	}

	/* Enable all workarounds and set SSL options */
	SSL_CTX_set_options(ctx, SSL_OP_ALL);
	SSL_CTX_set_options(ctx,
	    SSL_OP_NO_SESSION_RESUMPTION_ON_RENEGOTIATION);

	/* Set the allowed SSL protocols */
	if ((proto->sslflags & SSLFLAG_SSLV2) == 0)
		SSL_CTX_set_options(ctx, SSL_OP_NO_SSLv2);
	if ((proto->sslflags & SSLFLAG_SSLV3) == 0)
		SSL_CTX_set_options(ctx, SSL_OP_NO_SSLv3);
	if ((proto->sslflags & SSLFLAG_TLSV1) == 0)
		SSL_CTX_set_options(ctx, SSL_OP_NO_TLSv1);

	if (!SSL_CTX_set_cipher_list(ctx, proto->sslciphers))
		goto err;

	if (relay_host(&rlay->conf.ss, hbuf, sizeof(hbuf)) == NULL)
		goto err;

	/* Load the certificate */
	if (snprintf(certfile, sizeof(certfile),
	    "/etc/ssl/%s.crt", hbuf) == -1)
		goto err;
	log_debug("relay_ssl_ctx_create: using certificate %s", certfile);
	if (!SSL_CTX_use_certificate_file(ctx, certfile, SSL_FILETYPE_PEM))
		goto err;

	/* Load the private key */
	if (snprintf(certfile, sizeof(certfile),
	    "/etc/ssl/private/%s.key", hbuf) == -1) {
		goto err;
	}
	log_debug("relay_ssl_ctx_create: using private key %s", certfile);
	if (!SSL_CTX_use_PrivateKey_file(ctx, certfile,  SSL_FILETYPE_PEM))
		goto err;
	if (!SSL_CTX_check_private_key(ctx))
		goto err;

	/* Set session context to the local relay name */
	if (!SSL_CTX_set_session_id_context(ctx, rlay->conf.name,
	    strlen(rlay->conf.name)))
		goto err;

	return (ctx);

 err:
	if (ctx != NULL)
		SSL_CTX_free(ctx);
	ssl_error(rlay->conf.name, "relay_ssl_ctx_create");
	return (NULL);
}

void
relay_ssl_transaction(struct session *con)
{
	struct relay	*rlay = (struct relay *)con->relay;
	SSL		*ssl;

	ssl = SSL_new(rlay->ctx);
	if (ssl == NULL)
		goto err;

	if (!SSL_set_ssl_method(ssl, SSLv23_server_method()))
		goto err;
	if (!SSL_set_fd(ssl, con->in.s))
		goto err;
	SSL_set_accept_state(ssl);

	con->in.ssl = ssl;

	event_again(&con->ev, con->in.s, EV_TIMEOUT|EV_READ,
	    relay_ssl_accept, &con->tv_start, &env->timeout, con);
	return;

 err:
	if (ssl != NULL)
		SSL_free(ssl);
	ssl_error(rlay->conf.name, "relay_ssl_transaction");
}

void
relay_ssl_accept(int fd, short event, void *arg)
{
	struct session	*con = (struct session *)arg;
	struct relay	*rlay = (struct relay *)con->relay;
	int		 ret;
	int		 ssl_err;
	int		 retry_flag;

	if (event == EV_TIMEOUT) {
		relay_close(con, "SSL accept timeout");
		return;
	}

	retry_flag = ssl_err = 0;

	ret = SSL_accept(con->in.ssl);
	if (ret <= 0) {
		ssl_err = SSL_get_error(con->in.ssl, ret);

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
				relay_close(con, "closed");
				return;
			}
			/* FALLTHROUGH */
		default:
			ssl_error(rlay->conf.name, "relay_ssl_accept");
			relay_close(con, "SSL accept error");
			return;
		}
	}


#ifdef DEBUG
	log_info("relay %s, session %d established (%d active)",
	    rlay->conf.name, con->id, relay_sessions);
#else
	log_debug("relay %s, session %d established (%d active)",
	    rlay->conf.name, con->id, relay_sessions);
#endif
	relay_session(con);
	return;

retry:
	DPRINTF("relay_ssl_accept: session %d: scheduling on %s", con->id,
	    (retry_flag == EV_READ) ? "EV_READ" : "EV_WRITE");
	event_again(&con->ev, fd, EV_TIMEOUT|retry_flag, relay_ssl_accept,
	    &con->tv_start, &env->timeout, con);
}

void
relay_ssl_connected(struct ctl_relay_event *cre)
{
	/*
	 * Hack libevent - we overwrite the internal bufferevent I/O
	 * functions to handle the SSL abstraction.
	 */
	event_set(&cre->bev->ev_read, cre->s, EV_READ,
	    relay_ssl_readcb, cre->bev);
	event_set(&cre->bev->ev_write, cre->s, EV_WRITE,
	    relay_ssl_writecb, cre->bev);
}

void
relay_ssl_readcb(int fd, short event, void *arg)
{
	struct bufferevent *bufev = arg;
	struct ctl_relay_event *cre = (struct ctl_relay_event *)bufev->cbarg;
	struct session *con = (struct session *)cre->con;
	struct relay *rlay = (struct relay *)con->relay;
	int ret = 0, ssl_err = 0;
	short what = EVBUFFER_READ;
	size_t len;
	char rbuf[READ_BUF_SIZE];
	int howmuch = READ_BUF_SIZE;

	if (event == EV_TIMEOUT) {
		what |= EVBUFFER_TIMEOUT;
		goto err;
	}

	if (bufev->wm_read.high != 0)
		howmuch = MIN(sizeof(rbuf), bufev->wm_read.high);

	ret = SSL_read(cre->ssl, rbuf, howmuch);
	if (ret <= 0) {
		ssl_err = SSL_get_error(cre->ssl, ret);

		switch (ssl_err) {
		case SSL_ERROR_WANT_READ:
			DPRINTF("relay_ssl_readcb: session %d: "
			    "want read", con->id);
			goto retry;
		case SSL_ERROR_WANT_WRITE:
			DPRINTF("relay_ssl_readcb: session %d: "
			    "want write", con->id);
			goto retry;
		default:
			if (ret == 0)
				what |= EVBUFFER_EOF;
			else {
				ssl_error(rlay->conf.name, "relay_ssl_readcb");
				what |= EVBUFFER_ERROR;
			}
			goto err;
		}
	}

	if (evbuffer_add(bufev->input, rbuf, ret) == -1) {
		what |= EVBUFFER_ERROR;
		goto err;
	}

	relay_bufferevent_add(&bufev->ev_read, bufev->timeout_read);

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
	relay_bufferevent_add(&bufev->ev_read, bufev->timeout_read);
	return;

 err:
	(*bufev->errorcb)(bufev, what, bufev->cbarg);
}

void
relay_ssl_writecb(int fd, short event, void *arg)
{
	struct bufferevent *bufev = arg;
	struct ctl_relay_event *cre = (struct ctl_relay_event *)bufev->cbarg;
	struct session *con = (struct session *)cre->con;
	struct relay *rlay = (struct relay *)con->relay;
	int ret = 0, ssl_err;
	short what = EVBUFFER_WRITE;

	if (event == EV_TIMEOUT) {
		what |= EVBUFFER_TIMEOUT;
		goto err;
	}

	if (EVBUFFER_LENGTH(bufev->output)) {
		if (cre->buf == NULL) {
			cre->buflen = EVBUFFER_LENGTH(bufev->output);
			if ((cre->buf = malloc(cre->buflen)) == NULL) {
				what |= EVBUFFER_ERROR;
				goto err;
			}
			bcopy(EVBUFFER_DATA(bufev->output),
			    cre->buf, cre->buflen);
		}

		ret = SSL_write(cre->ssl, cre->buf, cre->buflen);
		if (ret <= 0) {
			ssl_err = SSL_get_error(cre->ssl, ret);

			switch (ssl_err) {
			case SSL_ERROR_WANT_READ:
				DPRINTF("relay_ssl_writecb: session %d: "
				    "want read", con->id);
				goto retry;
			case SSL_ERROR_WANT_WRITE:
				DPRINTF("relay_ssl_writecb: session %d: "
				    "want write", con->id);
				goto retry;
			default:
				if (ret == 0)
					what |= EVBUFFER_EOF;
				else {
					ssl_error(rlay->conf.name,
					    "relay_ssl_writecb");
					what |= EVBUFFER_ERROR;
				}
				goto err;
			}
		}
		evbuffer_drain(bufev->output, ret);
	}
	if (cre->buf != NULL) {
		free(cre->buf);
		cre->buf = NULL;
		cre->buflen = 0;
	}

	if (EVBUFFER_LENGTH(bufev->output) != 0)
		relay_bufferevent_add(&bufev->ev_write, bufev->timeout_write);

	if (bufev->writecb != NULL &&
	    EVBUFFER_LENGTH(bufev->output) <= bufev->wm_write.low)
		(*bufev->writecb)(bufev, bufev->cbarg);
	return;

 retry:
	if (cre->buflen != 0)
		relay_bufferevent_add(&bufev->ev_write, bufev->timeout_write);
	return;

 err:
	if (cre->buf != NULL) {
		free(cre->buf);
		cre->buf = NULL;
		cre->buflen = 0;
	}
	(*bufev->errorcb)(bufev, what, bufev->cbarg);
}

int
relay_bufferevent_add(struct event *ev, int timeout)
{
	struct timeval tv, *ptv = NULL;

	if (timeout) {
		timerclear(&tv);
		tv.tv_sec = timeout;
		ptv = &tv;
	}

	return (event_add(ev, ptv));
}

#ifdef notyet
int
relay_bufferevent_printf(struct ctl_relay_event *cre, const char *fmt, ...)
{
	int ret;
	va_list ap;

	va_start(ap, fmt);
	ret = evbuffer_add_vprintf(cre->output, fmt, ap);
	va_end(ap);

	if (cre->bev != NULL &&
	    ret != -1 && EVBUFFER_LENGTH(cre->output) > 0 &&
	    (cre->bev->enabled & EV_WRITE))
		bufferevent_enable(cre->bev, EV_WRITE);

	return (ret);
}
#endif

int
relay_bufferevent_print(struct ctl_relay_event *cre, char *str)
{
	if (cre->bev == NULL)
		return (evbuffer_add(cre->output, str, strlen(str)));
	return (bufferevent_write(cre->bev, str, strlen(str)));
}

int
relay_bufferevent_write_buffer(struct ctl_relay_event *cre,
    struct evbuffer *buf)
{
	if (cre->bev == NULL)
		return (evbuffer_add_buffer(cre->output, buf));
	return (bufferevent_write_buffer(cre->bev, buf));
}

int
relay_bufferevent_write_chunk(struct ctl_relay_event *cre,
    struct evbuffer *buf, size_t size)
{
	int ret;
	ret = relay_bufferevent_write(cre, buf->buffer, size);
	if (ret != -1)
		evbuffer_drain(buf, size);
	return (ret);
}

int
relay_bufferevent_write(struct ctl_relay_event *cre, void *data, size_t size)
{
	if (cre->bev == NULL)
		return (evbuffer_add(cre->output, data, size));
	return (bufferevent_write(cre->bev, data, size));
}

static __inline int
relay_proto_cmp(struct protonode *a, struct protonode *b)
{
	return (strcasecmp(a->key, b->key) +
	    a->type == b->type ? 0 : (a->type > b->type ? 1 : -1));
}

RB_GENERATE(proto_tree, protonode, nodes, relay_proto_cmp);
