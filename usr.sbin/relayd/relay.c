/*	$OpenBSD: relay.c,v 1.134 2011/04/24 10:07:43 bluhm Exp $	*/

/*
 * Copyright (c) 2006, 2007, 2008 Reyk Floeter <reyk@openbsd.org>
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

#include <openssl/ssl.h>

#include "relayd.h"

void		 relay_sig_handler(int sig, short, void *);
void		 relay_statistics(int, short, void *);
void		 relay_dispatch_pfe(int, short, void *);
void		 relay_dispatch_parent(int, short, void *);
void		 relay_shutdown(void);

void		 relay_privinit(void);
void		 relay_nodedebug(const char *, struct protonode *);
void		 relay_protodebug(struct relay *);
void		 relay_init(void);
void		 relay_launch(void);
int		 relay_socket(struct sockaddr_storage *, in_port_t,
		    struct protocol *, int, int);
int		 relay_socket_listen(struct sockaddr_storage *, in_port_t,
		    struct protocol *);
int		 relay_socket_connect(struct sockaddr_storage *, in_port_t,
		    struct protocol *, int);

void		 relay_accept(int, short, void *);
void		 relay_input(struct rsession *);

int		 relay_connect(struct rsession *);
void		 relay_connected(int, short, void *);
void		 relay_bindanyreq(struct rsession *, in_port_t, int);
void		 relay_bindany(int, short, void *);

u_int32_t	 relay_hash_addr(struct sockaddr_storage *, u_int32_t);

void		 relay_write(struct bufferevent *, void *);
void		 relay_read(struct bufferevent *, void *);
int		 relay_splicelen(struct ctl_relay_event *);
void		 relay_error(struct bufferevent *, short, void *);
void		 relay_dump(struct ctl_relay_event *, const void *, size_t);

int		 relay_resolve(struct ctl_relay_event *,
		    struct protonode *, struct protonode *);
int		 relay_handle_http(struct ctl_relay_event *,
		    struct protonode *, struct protonode *,
		    struct protonode *, int);
int		 relay_lognode(struct rsession *,
		    struct protonode *, struct protonode *, char *, size_t);
void		 relay_read_http(struct bufferevent *, void *);
static int	_relay_lookup_url(struct ctl_relay_event *, char *, char *,
		    char *, enum digest_type);
int		 relay_lookup_url(struct ctl_relay_event *,
		    const char *, enum digest_type);
int		 relay_lookup_query(struct ctl_relay_event *);
int		 relay_lookup_cookie(struct ctl_relay_event *, const char *);
void		 relay_read_httpcontent(struct bufferevent *, void *);
void		 relay_read_httpchunks(struct bufferevent *, void *);
char		*relay_expand_http(struct ctl_relay_event *, char *,
		    char *, size_t);
void		 relay_close_http(struct rsession *, u_int, const char *,
		    u_int16_t);
void		 relay_http_request_close(struct ctl_relay_event *);

SSL_CTX		*relay_ssl_ctx_create(struct relay *);
void		 relay_ssl_transaction(struct rsession *,
		    struct ctl_relay_event *);
void		 relay_ssl_accept(int, short, void *);
void		 relay_ssl_connect(int, short, void *);
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
char		*relay_load_file(const char *, off_t *);
static __inline int
		 relay_proto_cmp(struct protonode *, struct protonode *);
extern void	 bufferevent_read_pressure_cb(struct evbuffer *, size_t,
		    size_t, void *);

volatile sig_atomic_t relay_sessions;
objid_t relay_conid;

static struct relayd		*env = NULL;
struct imsgev			*iev_pfe;
struct imsgev			*iev_main;
int				 proc_id;

void
relay_sig_handler(int sig, short event, void *arg)
{
	switch (sig) {
	case SIGTERM:
	case SIGINT:
		(void)event_loopexit(NULL);
		break;
	case SIGCHLD:
	case SIGHUP:
	case SIGPIPE:
		/* ignore */
		break;
	default:
		fatalx("relay_sig_handler: unexpected signal");
	}
}

pid_t
relay(struct relayd *x_env, int pipe_parent2pfe[2], int pipe_parent2hce[2],
    int pipe_parent2relay[RELAY_MAXPROC][2], int pipe_pfe2hce[2],
    int pipe_pfe2relay[RELAY_MAXPROC][2])
{
	pid_t		 pid;
	struct passwd	*pw;
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
	purge_config(env, PURGE_RDRS);

	/* Need root privileges for relay initialization */
	relay_privinit();

	if ((pw = getpwnam(RELAYD_USER)) == NULL)
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
	relayd_process = PROC_RELAY;

#ifndef DEBUG
	if (setgroups(1, &pw->pw_gid) ||
	    setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) ||
	    setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid))
		fatal("relay: can't drop privileges");
#endif

	/* Fork child handlers */
	for (i = 1; i < env->sc_prefork_relay; i++) {
		if (fork() == 0) {
			proc_id = i;
			break;
		}
	}

	event_init();

	/* Per-child initialization */
	relay_init();

	signal_set(&env->sc_evsigint, SIGINT, relay_sig_handler, env);
	signal_set(&env->sc_evsigterm, SIGTERM, relay_sig_handler, env);
	signal_set(&env->sc_evsigchld, SIGCHLD, relay_sig_handler, env);
	signal_set(&env->sc_evsighup, SIGHUP, relay_sig_handler, env);
	signal_set(&env->sc_evsigpipe, SIGPIPE, relay_sig_handler, env);

	signal_add(&env->sc_evsigint, NULL);
	signal_add(&env->sc_evsigterm, NULL);
	signal_add(&env->sc_evsigchld, NULL);
	signal_add(&env->sc_evsighup, NULL);
	signal_add(&env->sc_evsigpipe, NULL);

	/* setup pipes */
	close(pipe_pfe2hce[0]);
	close(pipe_pfe2hce[1]);
	close(pipe_parent2hce[0]);
	close(pipe_parent2hce[1]);
	close(pipe_parent2pfe[0]);
	close(pipe_parent2pfe[1]);
	for (i = 0; i < env->sc_prefork_relay; i++) {
		if (i == proc_id)
			continue;
		close(pipe_parent2relay[i][0]);
		close(pipe_parent2relay[i][1]);
		close(pipe_pfe2relay[i][0]);
		close(pipe_pfe2relay[i][1]);
	}
	close(pipe_parent2relay[proc_id][1]);
	close(pipe_pfe2relay[proc_id][1]);

	if ((iev_pfe = calloc(1, sizeof(struct imsgev))) == NULL ||
	    (iev_main = calloc(1, sizeof(struct imsgev))) == NULL)
		fatal("relay");
	imsg_init(&iev_pfe->ibuf, pipe_pfe2relay[proc_id][0]);
	imsg_init(&iev_main->ibuf, pipe_parent2relay[proc_id][0]);
	iev_pfe->handler = relay_dispatch_pfe;
	iev_main->handler = relay_dispatch_parent;

	iev_pfe->events = EV_READ;
	event_set(&iev_pfe->ev, iev_pfe->ibuf.fd, iev_pfe->events,
	    iev_pfe->handler, iev_pfe);
	event_add(&iev_pfe->ev, NULL);

	iev_main->events = EV_READ;
	event_set(&iev_main->ev, iev_main->ibuf.fd, iev_main->events,
	    iev_main->handler, iev_main);
	event_add(&iev_main->ev, NULL);

	relay_launch();

	event_dispatch();
	relay_shutdown();

	return (0);
}

void
relay_shutdown(void)
{
	struct rsession	*con;

	struct relay	*rlay;
	TAILQ_FOREACH(rlay, env->sc_relays, rl_entry) {
		if (rlay->rl_conf.flags & F_DISABLE)
			continue;
		close(rlay->rl_s);
		while ((con = SPLAY_ROOT(&rlay->rl_sessions)) != NULL)
			relay_close(con, "shutdown");
	}
	usleep(200);	/* XXX relay needs to shutdown last */
	log_info("socket relay engine exiting");
	_exit(0);
}

void
relay_nodedebug(const char *name, struct protonode *pn)
{
	const char	*s;
	int		 digest;

	if (pn->action == NODE_ACTION_NONE)
		return;

	fprintf(stderr, "\t\t");
	fprintf(stderr, "%s ", name);

	switch (pn->type) {
	case NODE_TYPE_HEADER:
		break;
	case NODE_TYPE_QUERY:
		fprintf(stderr, "query ");
		break;
	case NODE_TYPE_COOKIE:
		fprintf(stderr, "cookie ");
		break;
	case NODE_TYPE_PATH:
		fprintf(stderr, "path ");
		break;
	case NODE_TYPE_URL:
		fprintf(stderr, "url ");
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
	case NODE_ACTION_FILTER:
		s = pn->action == NODE_ACTION_EXPECT ? "expect" : "filter";
		digest = pn->flags & PNFLAG_LOOKUP_URL_DIGEST;
		if (strcmp(pn->value, "*") == 0)
			fprintf(stderr, "%s %s\"%s\"", s,
			    digest ? "digest " : "", pn->key);
		else
			fprintf(stderr, "%s \"%s\" from \"%s\"", s,
			    pn->value, pn->key);
		break;
	case NODE_ACTION_HASH:
		fprintf(stderr, "hash \"%s\"", pn->key);
		break;
	case NODE_ACTION_LOG:
		fprintf(stderr, "log \"%s\"", pn->key);
		break;
	case NODE_ACTION_MARK:
		if (strcmp(pn->value, "*") == 0)
			fprintf(stderr, "mark \"%s\"", pn->key);
		else
			fprintf(stderr, "mark \"%s\" from \"%s\"",
			    pn->value, pn->key);
		break;
	case NODE_ACTION_NONE:
		break;
	}
	fprintf(stderr, "\n");
}

void
relay_protodebug(struct relay *rlay)
{
	struct protocol		*proto = rlay->rl_proto;
	struct protonode	*proot, *pn;
	struct proto_tree	*tree;
	const char		*name;
	int			 i;

	fprintf(stderr, "protocol %d: name %s\n", proto->id, proto->name);
	fprintf(stderr, "\tflags: %s, relay flags: %s\n",
	    printb_flags(proto->flags, F_BITS),
	    printb_flags(rlay->rl_conf.flags, F_BITS));
	if (proto->tcpflags)
		fprintf(stderr, "\ttcp flags: %s\n",
		    printb_flags(proto->tcpflags, TCPFLAG_BITS));
	if ((rlay->rl_conf.flags & (F_SSL|F_SSLCLIENT)) && proto->sslflags)
		fprintf(stderr, "\tssl flags: %s\n",
		    printb_flags(proto->sslflags, SSLFLAG_BITS));
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
	case RELAY_PROTO_DNS:
		fprintf(stderr, "dns\n");
		break;
	}

	name = "request";
	tree = &proto->request_tree;
 show:
	i = 0;
	RB_FOREACH(proot, proto_tree, tree) {
#if DEBUG > 1
		i = 0;
#endif
		PROTONODE_FOREACH(pn, proot, entry) {
#if DEBUG > 1
			i = 0;
#endif
			if (++i > 100)
				break;
			relay_nodedebug(name, pn);
		}
		/* Limit the number of displayed lines */
		if (++i > 100) {
			fprintf(stderr, "\t\t...\n");
			break;
		}
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

	if (env->sc_flags & (F_SSL|F_SSLCLIENT))
		ssl_init(env);

	TAILQ_FOREACH(rlay, env->sc_relays, rl_entry) {
		log_debug("relay_privinit: adding relay %s",
		    rlay->rl_conf.name);

		if (debug)
			relay_protodebug(rlay);

		switch (rlay->rl_proto->type) {
		case RELAY_PROTO_DNS:
			relay_udp_privinit(env, rlay);
			break;
		case RELAY_PROTO_TCP:
		case RELAY_PROTO_HTTP:
			/* Use defaults */
			break;
		}

		if (rlay->rl_conf.flags & F_UDP)
			rlay->rl_s = relay_udp_bind(&rlay->rl_conf.ss,
			    rlay->rl_conf.port, rlay->rl_proto);
		else
			rlay->rl_s = relay_socket_listen(&rlay->rl_conf.ss,
			    rlay->rl_conf.port, rlay->rl_proto);
		if (rlay->rl_s == -1)
			fatal("relay_privinit: failed to listen");
	}
}

void
relay_init(void)
{
	struct relay	*rlay;
	struct host	*host;
	struct timeval	 tv;

	/* Unlimited file descriptors (use system limits) */
	socket_rlimit(-1);

	TAILQ_FOREACH(rlay, env->sc_relays, rl_entry) {
		if ((rlay->rl_conf.flags & (F_SSL|F_SSLCLIENT)) &&
		    (rlay->rl_ssl_ctx = relay_ssl_ctx_create(rlay)) == NULL)
			fatal("relay_init: failed to create SSL context");

		if (rlay->rl_dsttable != NULL) {
			switch (rlay->rl_conf.dstmode) {
			case RELAY_DSTMODE_ROUNDROBIN:
				rlay->rl_dstkey = 0;
				break;
			case RELAY_DSTMODE_LOADBALANCE:
			case RELAY_DSTMODE_HASH:
				rlay->rl_dstkey =
				    hash32_str(rlay->rl_conf.name, HASHINIT);
				rlay->rl_dstkey =
				    hash32_str(rlay->rl_dsttable->conf.name,
				    rlay->rl_dstkey);
				break;
			}
			rlay->rl_dstnhosts = 0;
			TAILQ_FOREACH(host, &rlay->rl_dsttable->hosts, entry) {
				if (rlay->rl_dstnhosts >= RELAY_MAXHOSTS)
					fatal("relay_init: "
					    "too many hosts in table");
				host->idx = rlay->rl_dstnhosts;
				rlay->rl_dsthost[rlay->rl_dstnhosts++] = host;
			}
			log_info("adding %d hosts from table %s%s",
			    rlay->rl_dstnhosts, rlay->rl_dsttable->conf.name,
			    rlay->rl_dsttable->conf.check ? "" : " (no check)");
		}

		switch (rlay->rl_proto->type) {
		case RELAY_PROTO_DNS:
			relay_udp_init(rlay);
			break;
		case RELAY_PROTO_TCP:
		case RELAY_PROTO_HTTP:
			/* Use defaults */
			break;
		}
	}

	/* Schedule statistics timer */
	evtimer_set(&env->sc_statev, relay_statistics, NULL);
	bcopy(&env->sc_statinterval, &tv, sizeof(tv));
	evtimer_add(&env->sc_statev, &tv);
}

void
relay_statistics(int fd, short events, void *arg)
{
	struct relay		*rlay;
	struct ctl_stats	 crs, *cur;
	struct timeval		 tv, tv_now;
	int			 resethour = 0, resetday = 0;
	struct rsession		*con, *next_con;

	/*
	 * This is a hack to calculate some average statistics.
	 * It doesn't try to be very accurate, but could be improved...
	 */

	timerclear(&tv);
	if (gettimeofday(&tv_now, NULL) == -1)
		fatal("relay_init: gettimeofday");

	TAILQ_FOREACH(rlay, env->sc_relays, rl_entry) {
		bzero(&crs, sizeof(crs));
		resethour = resetday = 0;

		cur = &rlay->rl_stats[proc_id];
		cur->cnt += cur->last;
		cur->tick++;
		cur->avg = (cur->last + cur->avg) / 2;
		cur->last_hour += cur->last;
		if ((cur->tick % (3600 / env->sc_statinterval.tv_sec)) == 0) {
			cur->avg_hour = (cur->last_hour + cur->avg_hour) / 2;
			resethour++;
		}
		cur->last_day += cur->last;
		if ((cur->tick % (86400 / env->sc_statinterval.tv_sec)) == 0) {
			cur->avg_day = (cur->last_day + cur->avg_day) / 2;
			resethour++;
		}
		bcopy(cur, &crs, sizeof(crs));

		cur->last = 0;
		if (resethour)
			cur->last_hour = 0;
		if (resetday)
			cur->last_day = 0;

		crs.id = rlay->rl_conf.id;
		crs.proc = proc_id;
		imsg_compose_event(iev_pfe, IMSG_STATISTICS, 0, 0, -1,
		    &crs, sizeof(crs));

		for (con = SPLAY_ROOT(&rlay->rl_sessions);
		    con != NULL; con = next_con) {
			next_con = SPLAY_NEXT(session_tree,
			    &rlay->rl_sessions, con);
			timersub(&tv_now, &con->se_tv_last, &tv);
			if (timercmp(&tv, &rlay->rl_conf.timeout, >=))
				relay_close(con, "hard timeout");
		}
	}

	/* Schedule statistics timer */
	evtimer_set(&env->sc_statev, relay_statistics, NULL);
	bcopy(&env->sc_statinterval, &tv, sizeof(tv));
	evtimer_add(&env->sc_statev, &tv);
}

void
relay_launch(void)
{
	struct relay	*rlay;
	void		(*callback)(int, short, void *);

	TAILQ_FOREACH(rlay, env->sc_relays, rl_entry) {
		log_debug("relay_launch: running relay %s", rlay->rl_conf.name);

		rlay->rl_up = HOST_UP;

		if (rlay->rl_conf.flags & F_UDP)
			callback = relay_udp_server;
		else
			callback = relay_accept;

		event_set(&rlay->rl_ev, rlay->rl_s, EV_READ|EV_PERSIST,
		    callback, rlay);
		event_add(&rlay->rl_ev, NULL);
	}
}

int
relay_socket_af(struct sockaddr_storage *ss, in_port_t port)
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
relay_socket_getport(struct sockaddr_storage *ss)
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
relay_socket(struct sockaddr_storage *ss, in_port_t port,
    struct protocol *proto, int fd, int reuseport)
{
	int s = -1, val;
	struct linger lng;

	if (relay_socket_af(ss, port) == -1)
		goto bad;

	s = fd == -1 ? socket(ss->ss_family, SOCK_STREAM, IPPROTO_TCP) : fd;
	if (s == -1)
		goto bad;

	/*
	 * Socket options
	 */
	bzero(&lng, sizeof(lng));
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
    struct protocol *proto, int fd)
{
	int	s;

	if ((s = relay_socket(ss, port, proto, fd, 0)) == -1)
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

	if ((s = relay_socket(ss, port, proto, -1, 1)) == -1)
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
	struct rsession		*con = (struct rsession *)arg;
	struct relay		*rlay = (struct relay *)con->se_relay;
	struct protocol		*proto = rlay->rl_proto;
	evbuffercb		 outrd = relay_read;
	evbuffercb		 outwr = relay_write;
	struct bufferevent	*bev;
	struct ctl_relay_event	*out = &con->se_out;

	if (sig == EV_TIMEOUT) {
		relay_close_http(con, 504, "connect timeout", 0);
		return;
	}

	if ((rlay->rl_conf.flags & F_SSLCLIENT) && (out->ssl == NULL)) {
		relay_ssl_transaction(con, out);
		return;
	}

	DPRINTF("relay_connected: session %d: %ssuccessful",
	    con->se_id, rlay->rl_proto->lateconnect ? "late connect " : "");

	switch (rlay->rl_proto->type) {
	case RELAY_PROTO_HTTP:
		/* Check the servers's HTTP response */
		if (!RB_EMPTY(&rlay->rl_proto->response_tree)) {
			outrd = relay_read_http;
			if ((con->se_out.nodes = calloc(proto->response_nodes,
			    sizeof(u_int8_t))) == NULL) {
				relay_close_http(con, 500,
				    "failed to allocate nodes", 0);
				return;
			}
		}
		break;
	case RELAY_PROTO_TCP:
		if ((proto->tcpflags & TCPFLAG_NSPLICE) ||
		    (rlay->rl_conf.flags & (F_SSL|F_SSLCLIENT)))
			break;
		if (setsockopt(con->se_in.s, SOL_SOCKET, SO_SPLICE,
		    &con->se_out.s, sizeof(int)) == -1) {
			log_debug("relay_connect: session %d: splice forward "
			    "failed: %s", con->se_id, strerror(errno));
			return;
		}
		con->se_in.splicelen = 0;
		if (setsockopt(con->se_out.s, SOL_SOCKET, SO_SPLICE,
		    &con->se_in.s, sizeof(int)) == -1) {
			log_debug("relay_connect: session %d: splice backward "
			    "failed: %s", con->se_id, strerror(errno));
			return;
		}
		con->se_out.splicelen = 0;
		break;
	default:
		fatalx("relay_input: unknown protocol");
	}

	/*
	 * Relay <-> Server
	 */
	bev = bufferevent_new(fd, outrd, outwr, relay_error, &con->se_out);
	if (bev == NULL) {
		relay_close_http(con, 500,
		    "failed to allocate output buffer event", 0);
		return;
	}
	evbuffer_free(bev->output);
	bev->output = con->se_out.output;
	if (bev->output == NULL)
		fatal("relay_connected: invalid output buffer");
	con->se_out.bev = bev;

	/* Initialize the SSL wrapper */
	if ((rlay->rl_conf.flags & F_SSLCLIENT) && (out->ssl != NULL))
		relay_ssl_connected(out);

	bufferevent_settimeout(bev,
	    rlay->rl_conf.timeout.tv_sec, rlay->rl_conf.timeout.tv_sec);
	bufferevent_enable(bev, EV_READ|EV_WRITE);
}

void
relay_input(struct rsession *con)
{
	struct relay	*rlay = (struct relay *)con->se_relay;
	struct protocol *proto = rlay->rl_proto;
	evbuffercb	 inrd = relay_read;
	evbuffercb	 inwr = relay_write;

	switch (rlay->rl_proto->type) {
	case RELAY_PROTO_HTTP:
		/* Check the client's HTTP request */
		if (!RB_EMPTY(&rlay->rl_proto->request_tree) ||
		    proto->lateconnect) {
			inrd = relay_read_http;
			if ((con->se_in.nodes = calloc(proto->request_nodes,
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
	con->se_in.bev = bufferevent_new(con->se_in.s, inrd, inwr,
	    relay_error, &con->se_in);
	if (con->se_in.bev == NULL) {
		relay_close(con, "failed to allocate input buffer event");
		return;
	}

	/* Initialize the SSL wrapper */
	if ((rlay->rl_conf.flags & F_SSL) && con->se_in.ssl != NULL)
		relay_ssl_connected(&con->se_in);

	bufferevent_settimeout(con->se_in.bev,
	    rlay->rl_conf.timeout.tv_sec, rlay->rl_conf.timeout.tv_sec);
	bufferevent_enable(con->se_in.bev, EV_READ|EV_WRITE);
}

void
relay_write(struct bufferevent *bev, void *arg)
{
	struct ctl_relay_event	*cre = (struct ctl_relay_event *)arg;
	struct rsession		*con = cre->con;
	if (gettimeofday(&con->se_tv_last, NULL) == -1)
		con->se_done = 1;
	if (con->se_done)
		relay_close(con, "last write (done)");
}

void
relay_dump(struct ctl_relay_event *cre, const void *buf, size_t len)
{
	if (!len)
		return;

	/*
	 * This function will dump the specified message directly
	 * to the underlying session, without waiting for success
	 * of non-blocking events etc. This is useful to print an
	 * error message before gracefully closing the session.
	 */
	if (cre->ssl != NULL)
		(void)SSL_write(cre->ssl, buf, len);
	else
		(void)write(cre->s, buf, len);
}

void
relay_read(struct bufferevent *bev, void *arg)
{
	struct ctl_relay_event	*cre = (struct ctl_relay_event *)arg;
	struct rsession		*con = cre->con;
	struct evbuffer		*src = EVBUFFER_INPUT(bev);

	if (gettimeofday(&con->se_tv_last, NULL) == -1)
		goto fail;
	if (!EVBUFFER_LENGTH(src))
		return;
	if (relay_bufferevent_write_buffer(cre->dst, src) == -1)
		goto fail;
	if (con->se_done)
		goto done;
	if (cre->dst->bev)
		bufferevent_enable(cre->dst->bev, EV_READ);
	return;
 done:
	relay_close(con, "last read (done)");
	return;
 fail:
	relay_close(con, strerror(errno));
}

int
relay_resolve(struct ctl_relay_event *cre,
    struct protonode *proot, struct protonode *pn)
{
	struct rsession		*con = cre->con;
	char			 buf[IBUF_READ_SIZE], *ptr;
	int			 id;

	if (pn->mark && (pn->mark != con->se_mark))
		return (0);

	switch (pn->action) {
	case NODE_ACTION_FILTER:
		id = cre->nodes[proot->id];
		if (SIMPLEQ_NEXT(pn, entry) == NULL)
			cre->nodes[proot->id] = 0;
		if (id <= 1)
			return (0);
		break;
	case NODE_ACTION_EXPECT:
		id = cre->nodes[proot->id];
		if (SIMPLEQ_NEXT(pn, entry) == NULL)
			cre->nodes[proot->id] = 0;
		if (id > 1)
			return (0);
		break;
	default:
		if (cre->nodes[pn->id]) {
			cre->nodes[pn->id] = 0;
			return (0);
		}
		break;
	}
	switch (pn->action) {
	case NODE_ACTION_APPEND:
	case NODE_ACTION_CHANGE:
		ptr = pn->value;
		if ((pn->flags & PNFLAG_MACRO) &&
		    (ptr = relay_expand_http(cre, pn->value,
		    buf, sizeof(buf))) == NULL)
			break;
		if (relay_bufferevent_print(cre->dst, pn->key) == -1 ||
		    relay_bufferevent_print(cre->dst, ": ") == -1 ||
		    relay_bufferevent_print(cre->dst, ptr) == -1 ||
		    relay_bufferevent_print(cre->dst, "\r\n") == -1) {
			relay_close_http(con, 500,
			    "failed to modify header", 0);
			return (-1);
		}
		DPRINTF("relay_resolve: add '%s: %s'",
		    pn->key, ptr);
		break;
	case NODE_ACTION_EXPECT:
		DPRINTF("relay_resolve: missing '%s: %s'",
		    pn->key, pn->value);
		relay_close_http(con, 403, "incomplete request", pn->label);
		return (-1);
	case NODE_ACTION_FILTER:
		DPRINTF("relay_resolve: filtered '%s: %s'",
		    pn->key, pn->value);
		relay_close_http(con, 403, "rejecting request", pn->label);
		return (-1);
	default:
		break;
	}
	return (0);
}

char *
relay_expand_http(struct ctl_relay_event *cre, char *val, char *buf, size_t len)
{
	struct rsession	*con = cre->con;
	struct relay	*rlay = (struct relay *)con->se_relay;
	char		 ibuf[128];

	(void)strlcpy(buf, val, len);

	if (strstr(val, "$REMOTE_") != NULL) {
		if (strstr(val, "$REMOTE_ADDR") != NULL) {
			if (print_host(&cre->ss, ibuf, sizeof(ibuf)) == NULL)
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
			if (print_host(&rlay->rl_conf.ss,
			    ibuf, sizeof(ibuf)) == NULL)
				return (NULL);
			if (expand_string(buf, len,
			    "$SERVER_ADDR", ibuf) != 0)
				return (NULL);
		}
		if (strstr(val, "$SERVER_PORT") != NULL) {
			snprintf(ibuf, sizeof(ibuf), "%u",
			    ntohs(rlay->rl_conf.port));
			if (expand_string(buf, len,
			    "$SERVER_PORT", ibuf) != 0)
				return (NULL);
		}
		if (strstr(val, "$SERVER_NAME") != NULL) {
			if (expand_string(buf, len,
			    "$SERVER_NAME", RELAYD_SERVERNAME) != 0)
				return (NULL);
		}
	}
	if (strstr(val, "$TIMEOUT") != NULL) {
		snprintf(ibuf, sizeof(ibuf), "%lu",
		    rlay->rl_conf.timeout.tv_sec);
		if (expand_string(buf, len, "$TIMEOUT", ibuf) != 0)
			return (NULL);
	}

	return (buf);
}

int
relay_lognode(struct rsession *con, struct protonode *pn, struct protonode *pk,
    char *buf, size_t len)
{
	const char		*label = NULL;

	if ((pn->flags & PNFLAG_LOG) == 0)
		return (0);
	bzero(buf, len);
	if (pn->label != 0)
		label = pn_id2name(pn->label);
	if (snprintf(buf, len, " [%s%s%s: %s]",
	    label == NULL ? "" : label,
	    label == NULL ? "" : ", ",
	    pk->key, pk->value) == -1 ||
	    evbuffer_add(con->se_log, buf, strlen(buf)) == -1)
		return (-1);
	return (0);
}

int
relay_handle_http(struct ctl_relay_event *cre, struct protonode *proot,
    struct protonode *pn, struct protonode *pk, int header)
{
	struct rsession		*con = cre->con;
	char			 buf[IBUF_READ_SIZE], *ptr;
	int			 ret = PN_DROP, mark = 0;
	struct protonode	*next;

	/* Check if this action depends on a marked session */
	if (pn->mark != 0)
		mark = pn->mark == con->se_mark ? 1 : -1;

	switch (pn->action) {
	case NODE_ACTION_EXPECT:
	case NODE_ACTION_FILTER:
	case NODE_ACTION_MARK:
		break;
	default:
		if (mark == -1)
			return (PN_PASS);
		break;
	}

	switch (pn->action) {
	case NODE_ACTION_APPEND:
		if (!header)
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
		if (!header)
			return (PN_PASS);
		DPRINTF("relay_handle_http: change/remove '%s: %s'",
		    pk->key, pk->value);
		break;
	case NODE_ACTION_EXPECT:
		/*
		 * A client may specify the header line for multiple times
		 * trying to circumvent the filter.
		 */
		if (cre->nodes[proot->id] > 1) {
			relay_close_http(con, 400, "repeated header line", 0);
			return (PN_FAIL);
		}
		/* FALLTHROUGH */
	case NODE_ACTION_FILTER:
		DPRINTF("relay_handle_http: %s '%s: %s'",
		    (pn->action == NODE_ACTION_EXPECT) ? "expect" : "filter",
		    pn->key, pn->value);

		/* Do not drop the entity */
		ret = PN_PASS;

		if (mark != -1 &&
		    fnmatch(pn->value, pk->value, FNM_CASEFOLD) == 0) {
			cre->nodes[proot->id] = 1;

			/* Fail instantly */
			if (pn->action == NODE_ACTION_FILTER) {
				(void)relay_lognode(con, pn, pk,
				    buf, sizeof(buf));
				relay_close_http(con, 403,
				    "rejecting request", pn->label);
				return (PN_FAIL);
			}
		}
		next = SIMPLEQ_NEXT(pn, entry);
		if (next == NULL || next->action != pn->action)
			cre->nodes[proot->id]++;
		break;
	case NODE_ACTION_HASH:
		DPRINTF("relay_handle_http: hash '%s: %s'",
		    pn->key, pk->value);
		con->se_hashkey = hash32_str(pk->value, con->se_hashkey);
		ret = PN_PASS;
		break;
	case NODE_ACTION_LOG:
		DPRINTF("relay_handle_http: log '%s: %s'",
		    pn->key, pk->value);
		ret = PN_PASS;
		break;
	case NODE_ACTION_MARK:
		DPRINTF("relay_handle_http: mark '%s: %s'",
		    pn->key, pk->value);
		if (fnmatch(pn->value, pk->value, FNM_CASEFOLD) == 0)
			con->se_mark = pn->mark;
		ret = PN_PASS;
		break;
	case NODE_ACTION_NONE:
		return (PN_PASS);
	}
	if (mark != -1 && relay_lognode(con, pn, pk, buf, sizeof(buf)) == -1)
		goto fail;

	return (ret);
 fail:
	relay_close_http(con, 500, strerror(errno), 0);
	return (PN_FAIL);
}

void
relay_read_httpcontent(struct bufferevent *bev, void *arg)
{
	struct ctl_relay_event	*cre = (struct ctl_relay_event *)arg;
	struct rsession		*con = cre->con;
	struct evbuffer		*src = EVBUFFER_INPUT(bev);
	size_t			 size;

	if (gettimeofday(&con->se_tv_last, NULL) == -1)
		goto fail;
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
	if (con->se_done)
		goto done;
	if (bev->readcb != relay_read_httpcontent)
		bev->readcb(bev, arg);
	bufferevent_enable(bev, EV_READ);
	return;
 done:
	relay_close(con, "last http content read");
	return;
 fail:
	relay_close(con, strerror(errno));
}

void
relay_read_httpchunks(struct bufferevent *bev, void *arg)
{
	struct ctl_relay_event	*cre = (struct ctl_relay_event *)arg;
	struct rsession		*con = cre->con;
	struct evbuffer		*src = EVBUFFER_INPUT(bev);
	char			*line;
	long			 lval;
	size_t			 size;

	if (gettimeofday(&con->se_tv_last, NULL) == -1)
		goto fail;
	size = EVBUFFER_LENGTH(src);
	DPRINTF("relay_read_httpchunks: size %d, to read %d",
	    size, cre->toread);
	if (!size)
		return;

	if (!cre->toread) {
		line = evbuffer_readline(src);
		if (line == NULL) {
			/* Ignore empty line, continue */
			bufferevent_enable(bev, EV_READ);
			return;
		}
		if (!strlen(line)) {
			free(line);
			goto next;
		}

		/* Read prepended chunk size in hex, ingore the trailer */
		if (sscanf(line, "%lx", &lval) != 1) {
			free(line);
			relay_close(con, "invalid chunk size");
			return;
		}

		if (relay_bufferevent_print(cre->dst, line) == -1 ||
		    relay_bufferevent_print(cre->dst, "\r\n") == -1) {
			free(line);
			goto fail;
		}
		free(line);

		/* Last chunk is 0 bytes followed by an empty newline */
		if ((cre->toread = lval) == 0) {
			DPRINTF("relay_read_httpchunks: last chunk");

			line = evbuffer_readline(src);
			if (line == NULL) {
				relay_close(con, "invalid last chunk");
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
			/* Chunk is terminated by an empty (empty) newline */
			line = evbuffer_readline(src);
			if (line != NULL)
				free(line);
			if (relay_bufferevent_print(cre->dst, "\r\n\r\n") == -1)
				goto fail;
		}
	}

 next:
	if (con->se_done)
		goto done;
	if (EVBUFFER_LENGTH(src))
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
relay_http_request_close(struct ctl_relay_event *cre)
{
	if (cre->path != NULL) {
		free(cre->path);
		cre->path = NULL;
	}

	cre->args = NULL;
	cre->version = NULL;

	if (cre->buf != NULL) {
		free(cre->buf);
		cre->buf = NULL;
		cre->buflen = 0;
	}

	cre->line = 0;
	cre->method = 0;
	cre->done = 0;
	cre->chunked = 0;
}

void
relay_read_http(struct bufferevent *bev, void *arg)
{
	struct ctl_relay_event	*cre = (struct ctl_relay_event *)arg;
	struct rsession		*con = cre->con;
	struct relay		*rlay = (struct relay *)con->se_relay;
	struct protocol		*proto = rlay->rl_proto;
	struct evbuffer		*src = EVBUFFER_INPUT(bev);
	struct protonode	*pn, pk, *proot, *pnv = NULL, pkv;
	char			*line;
	int			 header = 0, ret, pass = 0;
	const char		*errstr;
	size_t			 size;

	if (gettimeofday(&con->se_tv_last, NULL) == -1)
		goto fail;
	size = EVBUFFER_LENGTH(src);
	DPRINTF("relay_read_http: size %d, to read %d", size, cre->toread);
	if (!size) {
		if (cre->dir == RELAY_DIR_RESPONSE)
			return;
		cre->toread = 0;
		goto done;
	}

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
			if (cre->line == 1) {
				free(line);
				relay_close_http(con, 400, "malformed", 0);
				return;
			}

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
			pk.value += strspn(pk.value, " \t\r\n");
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
			} else if (strcmp("HEAD", pk.key) == 0)
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
			else {
				/* Use GET method as the default */
				cre->method = HTTP_METHOD_GET;
			}

			/*
			 * Decode the path and query
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
			char	 buf[BUFSIZ];
			if (snprintf(buf, sizeof(buf), " \"%s\"",
			    cre->path) == -1 ||
			    evbuffer_add(con->se_log, buf, strlen(buf)) == -1) {
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

			if ((proot = RB_FIND(proto_tree,
			    cre->tree, &pkv)) == NULL)
				goto lookup;

			PROTONODE_FOREACH(pnv, proot, entry) {
				ret = relay_handle_http(cre, proot,
				    pnv, &pkv, 0);
				if (ret == PN_FAIL)
					goto abort;
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
			cre->toread = strtonum(pk.value, 0, INT_MAX, &errstr);
			if (errstr) {
				relay_close_http(con, 500, errstr, 0);
				goto abort;
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
			/*
			 * Lookup the URL of type example.com/path?args.
			 * Either as a plain string or SHA1/MD5 digest.
			 */
			if ((pn->flags & PNFLAG_LOOKUP_DIGEST(0)) &&
			    relay_lookup_url(cre, pk.value,
			    DIGEST_NONE) == PN_FAIL)
				goto abort;
			if ((pn->flags & PNFLAG_LOOKUP_DIGEST(DIGEST_SHA1)) &&
			    relay_lookup_url(cre, pk.value,
			    DIGEST_SHA1) == PN_FAIL)
				goto abort;
			if ((pn->flags & PNFLAG_LOOKUP_DIGEST(DIGEST_MD5)) &&
			    relay_lookup_url(cre, pk.value,
			    DIGEST_MD5) == PN_FAIL)
				goto abort;
		} else if (pn->flags & PNFLAG_LOOKUP_QUERY) {
			/* Lookup the HTTP query arguments */
			if (relay_lookup_query(cre) == PN_FAIL)
				goto abort;
		} else if (pn->flags & PNFLAG_LOOKUP_COOKIE) {
			/* Lookup the HTTP cookie */
			if (relay_lookup_cookie(cre, pk.value) == PN_FAIL)
				goto abort;
		}

 handle:
		pass = 0;
		PROTONODE_FOREACH(pnv, pn, entry) {
			ret = relay_handle_http(cre, pn, pnv, &pk, header);
			if (ret == PN_PASS)
				pass = 1;
			else if (ret == PN_FAIL)
				goto abort;
		}

		if (pass) {
 next:
			if (relay_bufferevent_print(cre->dst, pk.key) == -1 ||
			    relay_bufferevent_print(cre->dst,
			    header ? ": " : " ") == -1 ||
			    relay_bufferevent_print(cre->dst, pk.value) == -1 ||
			    relay_bufferevent_print(cre->dst, "\r\n") == -1) {
				free(line);
				goto fail;
			}
		}
		free(line);
	}
	if (cre->done) {
		RB_FOREACH(proot, proto_tree, cre->tree) {
			PROTONODE_FOREACH(pn, proot, entry)
				if (relay_resolve(cre, proot, pn) != 0)
					return;
		}

		switch (cre->method) {
		case HTTP_METHOD_NONE:
			relay_close_http(con, 406, "no method", 0);
			return;
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

			/* Single-pass HTTP response */
			bev->readcb = relay_read;
			break;
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

		relay_http_request_close(cre);

 done:
		if (cre->dir == RELAY_DIR_REQUEST && !cre->toread &&
		    proto->lateconnect && cre->dst->bev == NULL) {
			if (rlay->rl_conf.fwdmode == FWD_TRANS) {
				relay_bindanyreq(con, 0, IPPROTO_TCP);
				return;
			}
			if (relay_connect(con) == -1)
				relay_close_http(con, 502, "session failed", 0);
			return;
		}
	}
	if (con->se_done) {
		relay_close(con, "last http read (done)");
		return;
	}
	if (EVBUFFER_LENGTH(src) && bev->readcb != relay_read_http)
		bev->readcb(bev, arg);
	bufferevent_enable(bev, EV_READ);
	return;
 fail:
	relay_close_http(con, 500, strerror(errno), 0);
	return;
 abort:
	free(line);
}

static int
_relay_lookup_url(struct ctl_relay_event *cre, char *host, char *path,
    char *query, enum digest_type type)
{
	struct rsession		*con = cre->con;
	struct protonode	*proot, *pnv, pkv;
	char			*val, *md = NULL;
	int			 ret = PN_FAIL;

	if (asprintf(&val, "%s%s%s%s",
	    host, path,
	    query == NULL ? "" : "?",
	    query == NULL ? "" : query) == -1) {
		relay_close_http(con, 500, "failed to allocate URL", 0);
		return (PN_FAIL);
	}

	DPRINTF("_relay_lookup_url: %s", val);

	switch (type) {
	case DIGEST_SHA1:
	case DIGEST_MD5:
		if ((md = digeststr(type, val, strlen(val), NULL)) == NULL) {
			relay_close_http(con, 500,
			    "failed to allocate digest", 0);
			goto fail;
		}
		pkv.key = md;
		break;
	case DIGEST_NONE:
		pkv.key = val;
		break;
	}
	pkv.type = NODE_TYPE_URL;
	pkv.value = "";

	if ((proot = RB_FIND(proto_tree, cre->tree, &pkv)) == NULL)
		goto done;

	PROTONODE_FOREACH(pnv, proot, entry) {
		ret = relay_handle_http(cre, proot, pnv, &pkv, 0);
		if (ret == PN_FAIL)
			goto fail;
	}

 done:
	ret = PN_PASS;
 fail:
	if (md != NULL)
		free(md);
	free(val);
	return (ret);
}

int
relay_lookup_url(struct ctl_relay_event *cre, const char *str,
    enum digest_type type)
{
	struct rsession	*con = cre->con;
	int		 i, j, dots;
	char		*hi[RELAY_MAXLOOKUPLEVELS], *p, *pp, *c, ch;
	char		 ph[MAXHOSTNAMELEN];
	int		 ret;

	if (cre->path == NULL)
		return (PN_PASS);

	/*
	 * This is an URL lookup algorithm inspired by
	 * http://code.google.com/apis/safebrowsing/
	 *     developers_guide.html#PerformingLookups
	 */

	DPRINTF("relay_lookup_url: host: '%s', path: '%s', query: '%s'",
	    str, cre->path, cre->args == NULL ? "" : cre->args);

	if (canonicalize_host(str, ph, sizeof(ph)) == NULL) {
		relay_close_http(con, 400, "invalid host name", 0);
		return (PN_FAIL);
	}

	bzero(hi, sizeof(hi));
	for (dots = -1, i = strlen(ph) - 1; i > 0; i--) {
		if (ph[i] == '.' && ++dots)
			hi[dots - 1] = &ph[i + 1];
		if (dots > (RELAY_MAXLOOKUPLEVELS - 2))
			break;
	}
	if (dots == -1)
		dots = 0;
	hi[dots] = ph;

	if ((pp = strdup(cre->path)) == NULL) {
		relay_close_http(con, 500, "failed to allocate path", 0);
		return (PN_FAIL);
	}
	for (i = (RELAY_MAXLOOKUPLEVELS - 1); i >= 0; i--) {
		if (hi[i] == NULL)
			continue;

		/* 1. complete path with query */
		if (cre->args != NULL)
			if ((ret = _relay_lookup_url(cre, hi[i],
			    pp, cre->args, type)) != PN_PASS)
				goto done;

		/* 2. complete path without query */
		if ((ret = _relay_lookup_url(cre, hi[i],
		    pp, NULL, type)) != PN_PASS)
			goto done;

		/* 3. traverse path */
		for (j = 0, p = strchr(pp, '/');
		    p != NULL; p = strchr(p, '/'), j++) {
			if (j > (RELAY_MAXLOOKUPLEVELS - 2) || ++p == '\0')
				break;
			c = &pp[p - pp];
			ch = *c;
			*c = '\0';
			if ((ret = _relay_lookup_url(cre, hi[i],
			    pp, NULL, type)) != PN_PASS)
				goto done;
			*c = ch;
		}
	}

	ret = PN_PASS;
 done:
	free(pp);
	return (ret);
}

int
relay_lookup_query(struct ctl_relay_event *cre)
{
	struct rsession		*con = cre->con;
	struct protonode	*proot, *pnv, pkv;
	char			*val, *ptr;
	int			 ret;

	if (cre->path == NULL || cre->args == NULL || strlen(cre->args) < 2)
		return (PN_PASS);
	if ((val = strdup(cre->args)) == NULL) {
		relay_close_http(con, 500, "failed to allocate query", 0);
		return (PN_FAIL);
	}

	ptr = val;
	while (ptr != NULL && strlen(ptr)) {
		pkv.key = ptr;
		pkv.type = NODE_TYPE_QUERY;
		if ((ptr = strchr(ptr, '&')) != NULL)
			*ptr++ = '\0';
		if ((pkv.value =
		    strchr(pkv.key, '=')) == NULL ||
		    strlen(pkv.value) < 1)
			continue;
		*pkv.value++ = '\0';

		if ((proot = RB_FIND(proto_tree, cre->tree, &pkv)) == NULL)
			continue;
		PROTONODE_FOREACH(pnv, proot, entry) {
			ret = relay_handle_http(cre, proot,
			    pnv, &pkv, 0);
			if (ret == PN_FAIL)
				goto done;
		}
	}

	ret = PN_PASS;
 done:
	free(val);
	return (ret);
}

int
relay_lookup_cookie(struct ctl_relay_event *cre, const char *str)
{
	struct rsession		*con = cre->con;
	struct protonode	*proot, *pnv, pkv;
	char			*val, *ptr;
	int			 ret;

	if ((val = strdup(str)) == NULL) {
		relay_close_http(con, 500, "failed to allocate cookie", 0);
		return (PN_FAIL);
	}

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
		if ((proot = RB_FIND(proto_tree, cre->tree, &pkv)) == NULL)
			continue;
		PROTONODE_FOREACH(pnv, proot, entry) {
			ret = relay_handle_http(cre, proot, pnv, &pkv, 0);
			if (ret == PN_FAIL)
				goto done;
		}
	}

	ret = PN_PASS;
 done:
	free(val);
	return (ret);
}

void
relay_close_http(struct rsession *con, u_int code, const char *msg,
    u_int16_t labelid)
{
	struct relay		*rlay = (struct relay *)con->se_relay;
	struct bufferevent	*bev = con->se_in.bev;
	const char		*httperr = print_httperror(code), *text = "";
	char			*httpmsg;
	time_t			 t;
	struct tm		*lt;
	char			 tmbuf[32], hbuf[128];
	const char		*style, *label = NULL;

	/* In some cases this function may be called from generic places */
	if (rlay->rl_proto->type != RELAY_PROTO_HTTP ||
	    (rlay->rl_proto->flags & F_RETURN) == 0) {
		relay_close(con, msg);
		return;
	}

	if (bev == NULL)
		goto done;

	/* Some system information */
	if (print_host(&rlay->rl_conf.ss, hbuf, sizeof(hbuf)) == NULL)
		goto done;

	/* RFC 2616 "tolerates" asctime() */
	time(&t);
	lt = localtime(&t);
	tmbuf[0] = '\0';
	if (asctime_r(lt, tmbuf) != NULL)
		tmbuf[strlen(tmbuf) - 1] = '\0';	/* skip final '\n' */

	/* Do not send details of the Internal Server Error */
	if (code != 500)
		text = msg;
	if (labelid != 0)
		label = pn_id2name(labelid);

	/* A CSS stylesheet allows minimal customization by the user */
	if ((style = rlay->rl_proto->style) == NULL)
		style = "body { background-color: #a00000; color: white; }";

	/* Generate simple HTTP+HTML error document */
	if (asprintf(&httpmsg,
	    "HTTP/1.x %03d %s\r\n"
	    "Date: %s\r\n"
	    "Server: %s\r\n"
	    "Connection: close\r\n"
	    "Content-Type: text/html\r\n"
	    "\r\n"
	    "<!DOCTYPE HTML PUBLIC "
	    "\"-//W3C//DTD HTML 4.01 Transitional//EN\">\n"
	    "<html>\n"
	    "<head>\n"
	    "<title>%03d %s</title>\n"
	    "<style type=\"text/css\"><!--\n%s\n--></style>\n"
	    "</head>\n"
	    "<body>\n"
	    "<h1>%s</h1>\n"
	    "<div id='m'>%s</div>\n"
	    "<div id='l'>%s</div>\n"
	    "<hr><address>%s at %s port %d</address>\n"
	    "</body>\n"
	    "</html>\n",
	    code, httperr, tmbuf, RELAYD_SERVERNAME,
	    code, httperr, style, httperr, text,
	    label == NULL ? "" : label,
	    RELAYD_SERVERNAME, hbuf, ntohs(rlay->rl_conf.port)) == -1)
		goto done;

	/* Dump the message without checking for success */
	relay_dump(&con->se_in, httpmsg, strlen(httpmsg));
	free(httpmsg);

 done:
	if (asprintf(&httpmsg, "%s (%03d %s)", msg, code, httperr) == -1)
		relay_close(con, msg);
	else {
		relay_close(con, httpmsg);
		free(httpmsg);
	}
}

int
relay_splicelen(struct ctl_relay_event *cre)
{
	struct rsession *con = cre->con;
	off_t len;
	socklen_t optlen;

	optlen = sizeof(len);
	if (getsockopt(cre->s, SOL_SOCKET, SO_SPLICE, &len, &optlen) == -1) {
		relay_close(con, strerror(errno));
		return (0);
	}
	if (len > cre->splicelen) {
		cre->splicelen = len;
		return (1);
	}
	return (0);
}

void
relay_error(struct bufferevent *bev, short error, void *arg)
{
	struct ctl_relay_event *cre = (struct ctl_relay_event *)arg;
	struct rsession *con = cre->con;
	struct evbuffer *dst;
	struct timeval tv, tv_now;

	if (error & EVBUFFER_TIMEOUT) {
		if (gettimeofday(&tv_now, NULL) == -1) {
			relay_close(con, strerror(errno));
			return;
		}
		if (cre->splicelen >= 0 && relay_splicelen(cre))
			con->se_tv_last = tv_now;
		if (cre->dst->splicelen >= 0 && relay_splicelen(cre->dst))
			con->se_tv_last = tv_now;
		timersub(&tv_now, &con->se_tv_last, &tv);
		if (timercmp(&tv, &con->se_relay->rl_conf.timeout, >=))
			relay_close(con, "buffer event timeout");
		else
			bufferevent_enable(cre->bev, EV_READ);
		return;
	}
	if (error & (EVBUFFER_READ|EVBUFFER_WRITE|EVBUFFER_EOF)) {
		bufferevent_disable(bev, EV_READ|EV_WRITE);

		con->se_done = 1;
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

void
relay_accept(int fd, short sig, void *arg)
{
	struct relay *rlay = (struct relay *)arg;
	struct protocol *proto = rlay->rl_proto;
	struct rsession *con = NULL;
	struct ctl_natlook *cnl = NULL;
	socklen_t slen;
	struct timeval tv;
	struct sockaddr_storage ss;
	int s = -1;

	slen = sizeof(ss);
	if ((s = accept(fd, (struct sockaddr *)&ss, (socklen_t *)&slen)) == -1)
		return;

	if (relay_sessions >= RELAY_MAX_SESSIONS ||
	    rlay->rl_conf.flags & F_DISABLE)
		goto err;

	if (fcntl(s, F_SETFL, O_NONBLOCK) == -1)
		goto err;

	if ((con = calloc(1, sizeof(*con))) == NULL)
		goto err;

	con->se_in.s = s;
	con->se_in.ssl = NULL;
	con->se_out.s = -1;
	con->se_out.ssl = NULL;
	con->se_in.dst = &con->se_out;
	con->se_out.dst = &con->se_in;
	con->se_in.con = con;
	con->se_out.con = con;
	con->se_in.splicelen = -1;
	con->se_out.splicelen = -1;
	con->se_relay = rlay;
	con->se_id = ++relay_conid;
	con->se_relayid = rlay->rl_conf.id;
	con->se_hashkey = rlay->rl_dstkey;
	con->se_in.tree = &proto->request_tree;
	con->se_out.tree = &proto->response_tree;
	con->se_in.dir = RELAY_DIR_REQUEST;
	con->se_out.dir = RELAY_DIR_RESPONSE;
	con->se_retry = rlay->rl_conf.dstretry;
	con->se_bnds = -1;
	if (gettimeofday(&con->se_tv_start, NULL) == -1)
		goto err;
	bcopy(&con->se_tv_start, &con->se_tv_last, sizeof(con->se_tv_last));
	bcopy(&ss, &con->se_in.ss, sizeof(con->se_in.ss));
	con->se_out.port = rlay->rl_conf.dstport;
	switch (ss.ss_family) {
	case AF_INET:
		con->se_in.port = ((struct sockaddr_in *)&ss)->sin_port;
		break;
	case AF_INET6:
		con->se_in.port = ((struct sockaddr_in6 *)&ss)->sin6_port;
		break;
	}

	relay_sessions++;
	SPLAY_INSERT(session_tree, &rlay->rl_sessions, con);

	/* Increment the per-relay session counter */
	rlay->rl_stats[proc_id].last++;

	/* Pre-allocate output buffer */
	con->se_out.output = evbuffer_new();
	if (con->se_out.output == NULL) {
		relay_close(con, "failed to allocate output buffer");
		return;
	}

	/* Pre-allocate log buffer */
	con->se_log = evbuffer_new();
	if (con->se_log == NULL) {
		relay_close(con, "failed to allocate log buffer");
		return;
	}

	if (rlay->rl_conf.flags & F_DIVERT) {
		slen = sizeof(con->se_out.ss);
		if (getsockname(s, (struct sockaddr *)&con->se_out.ss,
		    &slen) == -1) {
			relay_close(con, "peer lookup failed");
			return;
		}
		con->se_out.port = relay_socket_getport(&con->se_out.ss);

		/* Detect loop and fall back to the alternate forward target */
		if (bcmp(&rlay->rl_conf.ss, &con->se_out.ss,
		    sizeof(con->se_out.ss)) == 0 &&
		    con->se_out.port == rlay->rl_conf.port)
			con->se_out.ss.ss_family = AF_UNSPEC;
	} else if (rlay->rl_conf.flags & F_NATLOOK) {
		if ((cnl = (struct ctl_natlook *)
		    calloc(1, sizeof(struct ctl_natlook))) == NULL) {
			relay_close(con, "failed to allocate nat lookup");
			return;
		}

		con->se_cnl = cnl;
		bzero(cnl, sizeof(*cnl));
		cnl->in = -1;
		cnl->id = con->se_id;
		cnl->proc = proc_id;
		cnl->proto = IPPROTO_TCP;

		bcopy(&con->se_in.ss, &cnl->src, sizeof(cnl->src));
		slen = sizeof(cnl->dst);
		if (getsockname(s,
		    (struct sockaddr *)&cnl->dst, &slen) == -1) {
			relay_close(con, "failed to get local address");
			return;
		}

		imsg_compose_event(iev_pfe, IMSG_NATLOOK, 0, 0, -1, cnl,
		    sizeof(*cnl));

		/* Schedule timeout */
		evtimer_set(&con->se_ev, relay_natlook, con);
		bcopy(&rlay->rl_conf.timeout, &tv, sizeof(tv));
		evtimer_add(&con->se_ev, &tv);
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
relay_from_table(struct rsession *con)
{
	struct relay		*rlay = (struct relay *)con->se_relay;
	struct host		*host;
	struct table		*table = rlay->rl_dsttable;
	u_int32_t		 p = con->se_hashkey;
	int			 idx = 0;

	if (table->conf.check && !table->up && !rlay->rl_backuptable->up) {
		log_debug("relay_from_table: no active hosts");
		return (-1);
	} else if (!table->up && rlay->rl_backuptable->up) {
		table = rlay->rl_backuptable;
	}

	switch (rlay->rl_conf.dstmode) {
	case RELAY_DSTMODE_ROUNDROBIN:
		if ((int)rlay->rl_dstkey >= rlay->rl_dstnhosts)
			rlay->rl_dstkey = 0;
		idx = (int)rlay->rl_dstkey;
		break;
	case RELAY_DSTMODE_LOADBALANCE:
		p = relay_hash_addr(&con->se_in.ss, p);
		/* FALLTHROUGH */
	case RELAY_DSTMODE_HASH:
		p = relay_hash_addr(&rlay->rl_conf.ss, p);
		p = hash32_buf(&rlay->rl_conf.port,
		    sizeof(rlay->rl_conf.port), p);
		if ((idx = p % rlay->rl_dstnhosts) >= RELAY_MAXHOSTS)
			return (-1);
	}
	host = rlay->rl_dsthost[idx];
	DPRINTF("relay_from_table: host %s, p 0x%08x, idx %d",
	    host->conf.name, p, idx);
	while (host != NULL) {
		DPRINTF("relay_from_table: host %s", host->conf.name);
		if (!table->conf.check || host->up == HOST_UP)
			goto found;
		host = TAILQ_NEXT(host, entry);
	}
	TAILQ_FOREACH(host, &table->hosts, entry) {
		DPRINTF("relay_from_table: next host %s", host->conf.name);
		if (!table->conf.check || host->up == HOST_UP)
			goto found;
	}

	/* Should not happen */
	fatalx("relay_from_table: no active hosts, desynchronized");

 found:
	if (rlay->rl_conf.dstmode == RELAY_DSTMODE_ROUNDROBIN)
		rlay->rl_dstkey = host->idx + 1;
	con->se_retry = host->conf.retry;
	con->se_out.port = table->conf.port;
	bcopy(&host->conf.ss, &con->se_out.ss, sizeof(con->se_out.ss));

	return (0);
}

void
relay_natlook(int fd, short event, void *arg)
{
	struct rsession		*con = (struct rsession *)arg;
	struct relay		*rlay = (struct relay *)con->se_relay;
	struct ctl_natlook	*cnl = con->se_cnl;

	if (cnl == NULL)
		fatalx("invalid NAT lookup");

	if (con->se_out.ss.ss_family == AF_UNSPEC && cnl->in == -1 &&
	    rlay->rl_conf.dstss.ss_family == AF_UNSPEC &&
	    rlay->rl_dsttable == NULL) {
		relay_close(con, "session NAT lookup failed");
		return;
	}
	if (cnl->in != -1) {
		bcopy(&cnl->rdst, &con->se_out.ss, sizeof(con->se_out.ss));
		con->se_out.port = cnl->rdport;
	}
	free(con->se_cnl);
	con->se_cnl = NULL;

	relay_session(con);
}

void
relay_session(struct rsession *con)
{
	struct relay		*rlay = (struct relay *)con->se_relay;
	struct ctl_relay_event	*in = &con->se_in, *out = &con->se_out;

	if (bcmp(&rlay->rl_conf.ss, &out->ss, sizeof(out->ss)) == 0 &&
	    out->port == rlay->rl_conf.port) {
		log_debug("relay_session: session %d: looping",
		    con->se_id);
		relay_close(con, "session aborted");
		return;
	}

	if (rlay->rl_conf.flags & F_UDP) {
		/*
		 * Call the UDP protocol-specific handler
		 */
		if (rlay->rl_proto->request == NULL)
			fatalx("invalide UDP session");
		if ((*rlay->rl_proto->request)(con) == -1)
			relay_close(con, "session failed");
		return;
	}

	if ((rlay->rl_conf.flags & F_SSL) && (in->ssl == NULL)) {
		relay_ssl_transaction(con, in);
		return;
	}

	if (!rlay->rl_proto->lateconnect) {
		if (rlay->rl_conf.fwdmode == FWD_TRANS)
			relay_bindanyreq(con, 0, IPPROTO_TCP);
		else if (relay_connect(con) == -1) {
			relay_close(con, "session failed");
			return;
		}
	}

	relay_input(con);
}

void
relay_bindanyreq(struct rsession *con, in_port_t port, int proto)
{
	struct relay		*rlay = (struct relay *)con->se_relay;
	struct ctl_bindany	 bnd;
	struct timeval		 tv;

	bzero(&bnd, sizeof(bnd));
	bnd.bnd_id = con->se_id;
	bnd.bnd_proc = proc_id;
	bnd.bnd_port = port;
	bnd.bnd_proto = proto;
	bcopy(&con->se_in.ss, &bnd.bnd_ss, sizeof(bnd.bnd_ss));
	imsg_compose_event(iev_main, IMSG_BINDANY,
	    0, 0, -1, &bnd, sizeof(bnd));

	/* Schedule timeout */
	evtimer_set(&con->se_ev, relay_bindany, con);
	bcopy(&rlay->rl_conf.timeout, &tv, sizeof(tv));
	evtimer_add(&con->se_ev, &tv);
}

void
relay_bindany(int fd, short event, void *arg)
{
	struct rsession	*con = (struct rsession *)arg;

	if (con->se_bnds == -1) {
		relay_close(con, "bindany failed, invalid socket");
		return;
	}

	if (relay_connect(con) == -1)
		relay_close(con, "session failed");
}

int
relay_connect(struct rsession *con)
{
	struct relay	*rlay = (struct relay *)con->se_relay;
	int		 bnds = -1, ret;

	if (gettimeofday(&con->se_tv_start, NULL) == -1)
		return (-1);

	if (rlay->rl_dsttable != NULL) {
		if (relay_from_table(con) != 0)
			return (-1);
	} else if (con->se_out.ss.ss_family == AF_UNSPEC) {
		bcopy(&rlay->rl_conf.dstss, &con->se_out.ss,
		    sizeof(con->se_out.ss));
		con->se_out.port = rlay->rl_conf.dstport;
	}

	if (rlay->rl_conf.fwdmode == FWD_TRANS) {
		if (con->se_bnds == -1) {
			log_debug("relay_connect: could not bind any sock");
			return (-1);
		}
		bnds = con->se_bnds;
	}

	/* Do the IPv4-to-IPv6 or IPv6-to-IPv4 translation if requested */
	if (rlay->rl_conf.dstaf.ss_family != AF_UNSPEC) {
		if (con->se_out.ss.ss_family == AF_INET &&
		    rlay->rl_conf.dstaf.ss_family == AF_INET6)
			ret = map4to6(&con->se_out.ss, &rlay->rl_conf.dstaf);
		else if (con->se_out.ss.ss_family == AF_INET6 &&
		    rlay->rl_conf.dstaf.ss_family == AF_INET)
			ret = map6to4(&con->se_out.ss);
		else
			ret = 0;
		if (ret != 0) {
			log_debug("relay_connect: mapped to invalid address");
			return (-1);
		}
	}

 retry:
	if ((con->se_out.s = relay_socket_connect(&con->se_out.ss,
	    con->se_out.port, rlay->rl_proto, bnds)) == -1) {
		if (con->se_retry) {
			con->se_retry--;
			log_debug("relay_connect: session %d: "
			    "forward failed: %s, %s",
			    con->se_id, strerror(errno),
			    con->se_retry ? "next retry" : "last retry");
			goto retry;
		}
		log_debug("relay_connect: session %d: forward failed: %s",
		    con->se_id, strerror(errno));
		return (-1);
	}

	if (errno == EINPROGRESS)
		event_again(&con->se_ev, con->se_out.s, EV_WRITE|EV_TIMEOUT,
		    relay_connected, &con->se_tv_start, &env->sc_timeout, con);
	else
		relay_connected(con->se_out.s, EV_WRITE, con);

	return (0);
}

void
relay_close(struct rsession *con, const char *msg)
{
	struct relay	*rlay = (struct relay *)con->se_relay;
	char		 ibuf[128], obuf[128], *ptr = NULL;

	SPLAY_REMOVE(session_tree, &rlay->rl_sessions, con);

	event_del(&con->se_ev);
	if (con->se_in.bev != NULL)
		bufferevent_disable(con->se_in.bev, EV_READ|EV_WRITE);
	if (con->se_out.bev != NULL)
		bufferevent_disable(con->se_out.bev, EV_READ|EV_WRITE);

	if (env->sc_opts & RELAYD_OPT_LOGUPDATE) {
		bzero(&ibuf, sizeof(ibuf));
		bzero(&obuf, sizeof(obuf));
		(void)print_host(&con->se_in.ss, ibuf, sizeof(ibuf));
		(void)print_host(&con->se_out.ss, obuf, sizeof(obuf));
		if (EVBUFFER_LENGTH(con->se_log) &&
		    evbuffer_add_printf(con->se_log, "\r\n") != -1)
			ptr = evbuffer_readline(con->se_log);
		log_info("relay %s, session %d (%d active), %d, %s -> %s:%d, "
		    "%s%s%s", rlay->rl_conf.name, con->se_id, relay_sessions,
		    con->se_mark, ibuf, obuf, ntohs(con->se_out.port), msg,
		    ptr == NULL ? "" : ",", ptr == NULL ? "" : ptr);
		if (ptr != NULL)
			free(ptr);
	}

	if (con->se_priv != NULL)
		free(con->se_priv);
	if (con->se_in.bev != NULL)
		bufferevent_free(con->se_in.bev);
	else if (con->se_in.output != NULL)
		evbuffer_free(con->se_in.output);
	if (con->se_in.ssl != NULL) {
		/* XXX handle non-blocking shutdown */
		if (SSL_shutdown(con->se_in.ssl) == 0)
			SSL_shutdown(con->se_in.ssl);
		SSL_free(con->se_in.ssl);
	}
	if (con->se_in.s != -1)
		close(con->se_in.s);
	if (con->se_in.path != NULL)
		free(con->se_in.path);
	if (con->se_in.buf != NULL)
		free(con->se_in.buf);
	if (con->se_in.nodes != NULL)
		free(con->se_in.nodes);

	if (con->se_out.bev != NULL)
		bufferevent_free(con->se_out.bev);
	else if (con->se_out.output != NULL)
		evbuffer_free(con->se_out.output);
	if (con->se_out.ssl != NULL) {
		/* XXX handle non-blocking shutdown */
		if (SSL_shutdown(con->se_out.ssl) == 0)
			SSL_shutdown(con->se_out.ssl);
		SSL_free(con->se_out.ssl);
	}
	if (con->se_out.s != -1)
		close(con->se_out.s);
	if (con->se_out.path != NULL)
		free(con->se_out.path);
	if (con->se_out.buf != NULL)
		free(con->se_out.buf);
	if (con->se_out.nodes != NULL)
		free(con->se_out.nodes);

	if (con->se_log != NULL)
		evbuffer_free(con->se_log);

	if (con->se_cnl != NULL) {
#if 0
		imsg_compose_event(iev_pfe, IMSG_KILLSTATES, 0, 0, -1,
		    cnl, sizeof(*cnl));
#endif
		free(con->se_cnl);
	}

	free(con);
	relay_sessions--;
}

void
relay_dispatch_pfe(int fd, short event, void *ptr)
{
	struct imsgev		*iev;
	struct imsgbuf		*ibuf;
	struct imsg		 imsg;
	ssize_t			 n;
	struct relay		*rlay;
	struct rsession		*con;
	struct ctl_natlook	 cnl;
	struct timeval		 tv;
	struct host		*host;
	struct table		*table;
	struct ctl_status	 st;
	objid_t			 id;
	int			 verbose;

	iev = ptr;
	ibuf = &iev->ibuf;

	if (event & EV_READ) {
		if ((n = imsg_read(ibuf)) == -1)
			fatal("relay_dispatch_pfe: imsg_read_error");
		if (n == 0) {
			/* this pipe is dead, so remove the event handler */
			event_del(&iev->ev);
			event_loopexit(NULL);
			return;
		}
	}

	if (event & EV_WRITE) {
		if (msgbuf_write(&ibuf->w) == -1)
			fatal("relay_dispatch_pfe: msgbuf_write");
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
		case IMSG_TABLE_DISABLE:
			memcpy(&id, imsg.data, sizeof(id));
			if ((table = table_find(env, id)) == NULL)
				fatalx("relay_dispatch_pfe: desynchronized");
			table->conf.flags |= F_DISABLE;
			table->up = 0;
			TAILQ_FOREACH(host, &table->hosts, entry)
				host->up = HOST_UNKNOWN;
			break;
		case IMSG_TABLE_ENABLE:
			memcpy(&id, imsg.data, sizeof(id));
			if ((table = table_find(env, id)) == NULL)
				fatalx("relay_dispatch_pfe: desynchronized");
			table->conf.flags &= ~(F_DISABLE);
			table->up = 0;
			TAILQ_FOREACH(host, &table->hosts, entry)
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
			    con->se_cnl == NULL) {
				log_debug("relay_dispatch_pfe: "
				    "session expired");
				break;
			}
			bcopy(&cnl, con->se_cnl, sizeof(*con->se_cnl));
			evtimer_del(&con->se_ev);
			evtimer_set(&con->se_ev, relay_natlook, con);
			bzero(&tv, sizeof(tv));
			evtimer_add(&con->se_ev, &tv);
			break;
		case IMSG_CTL_SESSION:
			TAILQ_FOREACH(rlay, env->sc_relays, rl_entry)
				SPLAY_FOREACH(con, session_tree,
				    &rlay->rl_sessions)
					imsg_compose_event(iev,
					    IMSG_CTL_SESSION,
					    0, 0, -1, con, sizeof(*con));
			imsg_compose_event(iev, IMSG_CTL_END,
			    0, 0, -1, NULL, 0);
			break;
		case IMSG_CTL_LOG_VERBOSE:
			memcpy(&verbose, imsg.data, sizeof(verbose));
			log_verbose(verbose);
			break;
		default:
			log_debug("relay_dispatch_msg: unexpected imsg %d",
			    imsg.hdr.type);
			break;
		}
		imsg_free(&imsg);
	}
	imsg_event_add(iev);
}

void
relay_dispatch_parent(int fd, short event, void * ptr)
{
	struct rsession		*con;
	struct imsgev		*iev;
	struct imsgbuf		*ibuf;
	struct imsg		 imsg;
	ssize_t			 n;
	struct timeval		 tv;
	objid_t			 id;

	iev = ptr;
	ibuf = &iev->ibuf;

	if (event & EV_READ) {
		if ((n = imsg_read(ibuf)) == -1)
			fatal("relay_dispatch_parent: imsg_read error");
		if (n == 0) {
			/* this pipe is dead, so remove the event handler */
			event_del(&iev->ev);
			event_loopexit(NULL);
			return;
		}
	}

	if (event & EV_WRITE) {
		if (msgbuf_write(&ibuf->w) == -1)
			fatal("relay_dispatch_parent: msgbuf_write");
	}

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatal("relay_dispatch_parent: imsg_read error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_BINDANY:
			bcopy(imsg.data, &id, sizeof(id));
			if ((con = session_find(env, id)) == NULL) {
				log_debug("relay_dispatch_parent: "
				    "session expired");
				break;
			}

			/* Will validate the result later */
			con->se_bnds = imsg.fd;

			evtimer_del(&con->se_ev);
			evtimer_set(&con->se_ev, relay_bindany, con);
			bzero(&tv, sizeof(tv));
			evtimer_add(&con->se_ev, &tv);
			break;
		default:
			log_debug("relay_dispatch_parent: unexpected imsg %d",
			    imsg.hdr.type);
			break;
		}
		imsg_free(&imsg);
	}
	imsg_event_add(iev);
}

SSL_CTX *
relay_ssl_ctx_create(struct relay *rlay)
{
	struct protocol *proto = rlay->rl_proto;
	SSL_CTX *ctx;

	ctx = SSL_CTX_new(SSLv23_method());
	if (ctx == NULL)
		goto err;

	/* Modify session timeout and cache size*/
	SSL_CTX_set_timeout(ctx, rlay->rl_conf.timeout.tv_sec);
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

	/* Verify the server certificate if we have a CA chain */
	if ((rlay->rl_conf.flags & F_SSLCLIENT) &&
	    (rlay->rl_ssl_ca != NULL)) {
		if (!ssl_ctx_load_verify_memory(ctx,
		    rlay->rl_ssl_ca, rlay->rl_ssl_ca_len))
			goto err;
		SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, NULL);
	}

	if ((rlay->rl_conf.flags & F_SSL) == 0)
		return (ctx);

	log_debug("relay_ssl_ctx_create: loading certificate");
	if (!ssl_ctx_use_certificate_chain(ctx,
	    rlay->rl_ssl_cert, rlay->rl_ssl_cert_len))
		goto err;

	log_debug("relay_ssl_ctx_create: loading private key");
	if (!ssl_ctx_use_private_key(ctx, rlay->rl_ssl_key,
	    rlay->rl_ssl_key_len))
		goto err;
	if (!SSL_CTX_check_private_key(ctx))
		goto err;

	/* Set session context to the local relay name */
	if (!SSL_CTX_set_session_id_context(ctx, rlay->rl_conf.name,
	    strlen(rlay->rl_conf.name)))
		goto err;

	return (ctx);

 err:
	if (ctx != NULL)
		SSL_CTX_free(ctx);
	ssl_error(rlay->rl_conf.name, "relay_ssl_ctx_create");
	return (NULL);
}

void
relay_ssl_transaction(struct rsession *con, struct ctl_relay_event *cre)
{
	struct relay		*rlay = (struct relay *)con->se_relay;
	SSL			*ssl;
	const SSL_METHOD	*method;
	void			(*cb)(int, short, void *);
	u_int			 flags = EV_TIMEOUT;

	ssl = SSL_new(rlay->rl_ssl_ctx);
	if (ssl == NULL)
		goto err;

	if (cre->dir == RELAY_DIR_REQUEST) {
		cb = relay_ssl_accept;
		method = SSLv23_server_method();
		flags |= EV_READ;
	} else {
		cb = relay_ssl_connect;
		method = SSLv23_client_method();
		flags |= EV_WRITE;
	}

	if (!SSL_set_ssl_method(ssl, method))
		goto err;
	if (!SSL_set_fd(ssl, cre->s))
		goto err;

	if (cre->dir == RELAY_DIR_REQUEST)
		SSL_set_accept_state(ssl);
	else
		SSL_set_connect_state(ssl);

	cre->ssl = ssl;

	event_again(&con->se_ev, cre->s, EV_TIMEOUT|flags,
	    cb, &con->se_tv_start, &env->sc_timeout, con);
	return;

 err:
	if (ssl != NULL)
		SSL_free(ssl);
	ssl_error(rlay->rl_conf.name, "relay_ssl_transaction");
	relay_close(con, "session ssl failed");
}

void
relay_ssl_accept(int fd, short event, void *arg)
{
	struct rsession	*con = (struct rsession *)arg;
	struct relay	*rlay = (struct relay *)con->se_relay;
	int		 ret;
	int		 ssl_err;
	int		 retry_flag;

	if (event == EV_TIMEOUT) {
		relay_close(con, "SSL accept timeout");
		return;
	}

	retry_flag = ssl_err = 0;

	ret = SSL_accept(con->se_in.ssl);
	if (ret <= 0) {
		ssl_err = SSL_get_error(con->se_in.ssl, ret);

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
			ssl_error(rlay->rl_conf.name, "relay_ssl_accept");
			relay_close(con, "SSL accept error");
			return;
		}
	}


#ifdef DEBUG
	log_info("relay %s, session %d established (%d active)",
	    rlay->rl_conf.name, con->se_id, relay_sessions);
#else
	log_debug("relay %s, session %d established (%d active)",
	    rlay->rl_conf.name, con->se_id, relay_sessions);
#endif
	relay_session(con);
	return;

retry:
	DPRINTF("relay_ssl_accept: session %d: scheduling on %s", con->se_id,
	    (retry_flag == EV_READ) ? "EV_READ" : "EV_WRITE");
	event_again(&con->se_ev, fd, EV_TIMEOUT|retry_flag, relay_ssl_accept,
	    &con->se_tv_start, &env->sc_timeout, con);
}

void
relay_ssl_connect(int fd, short event, void *arg)
{
	struct rsession	*con = (struct rsession *)arg;
	struct relay	*rlay = (struct relay *)con->se_relay;
	int		 ret;
	int		 ssl_err;
	int		 retry_flag;

	if (event == EV_TIMEOUT) {
		relay_close(con, "SSL connect timeout");
		return;
	}

	retry_flag = ssl_err = 0;

	ret = SSL_connect(con->se_out.ssl);
	if (ret <= 0) {
		ssl_err = SSL_get_error(con->se_out.ssl, ret);

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
			ssl_error(rlay->rl_conf.name, "relay_ssl_connect");
			relay_close(con, "SSL connect error");
			return;
		}
	}

#ifdef DEBUG
	log_info("relay %s, session %d connected (%d active)",
	    rlay->rl_conf.name, con->se_id, relay_sessions);
#else
	log_debug("relay %s, session %d connected (%d active)",
	    rlay->rl_conf.name, con->se_id, relay_sessions);
#endif
	relay_connected(fd, EV_WRITE, con);
	return;

retry:
	DPRINTF("relay_ssl_connect: session %d: scheduling on %s", con->se_id,
	    (retry_flag == EV_READ) ? "EV_READ" : "EV_WRITE");
	event_again(&con->se_ev, fd, EV_TIMEOUT|retry_flag, relay_ssl_connect,
	    &con->se_tv_start, &env->sc_timeout, con);
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
	struct rsession *con = cre->con;
	struct relay *rlay = (struct relay *)con->se_relay;
	int ret = 0, ssl_err = 0;
	short what = EVBUFFER_READ;
	size_t len;
	char rbuf[IBUF_READ_SIZE];
	int howmuch = IBUF_READ_SIZE;

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
			    "want read", con->se_id);
			goto retry;
		case SSL_ERROR_WANT_WRITE:
			DPRINTF("relay_ssl_readcb: session %d: "
			    "want write", con->se_id);
			goto retry;
		default:
			if (ret == 0)
				what |= EVBUFFER_EOF;
			else {
				ssl_error(rlay->rl_conf.name,
				    "relay_ssl_readcb");
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
	struct rsession *con = cre->con;
	struct relay *rlay = (struct relay *)con->se_relay;
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
				    "want read", con->se_id);
				goto retry;
			case SSL_ERROR_WANT_WRITE:
				DPRINTF("relay_ssl_writecb: session %d: "
				    "want write", con->se_id);
				goto retry;
			default:
				if (ret == 0)
					what |= EVBUFFER_EOF;
				else {
					ssl_error(rlay->rl_conf.name,
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

int
relay_cmp_af(struct sockaddr_storage *a, struct sockaddr_storage *b)
{
	int ret = -1;
	struct sockaddr_in ia, ib;
	struct sockaddr_in6 ia6, ib6;

	switch (a->ss_family) {
	case AF_INET:
		bcopy(a, &ia, sizeof(struct sockaddr_in));
		bcopy(b, &ib, sizeof(struct sockaddr_in));

		ret = memcmp(&ia.sin_addr, &ib.sin_addr,
		    sizeof(ia.sin_addr));
		if (ret == 0)
			ret = memcmp(&ia.sin_port, &ib.sin_port,
			    sizeof(ia.sin_port));
		break;
	case AF_INET6:
		bcopy(a, &ia6, sizeof(struct sockaddr_in6));
		bcopy(b, &ib6, sizeof(struct sockaddr_in6));

		ret = memcmp(&ia6.sin6_addr, &ib6.sin6_addr,
		    sizeof(ia6.sin6_addr));
		if (ret == 0)
			ret = memcmp(&ia6.sin6_port, &ib6.sin6_port,
			    sizeof(ia6.sin6_port));
		break;
	default:
		break;
	}

	return (ret);
}

char *
relay_load_file(const char *name, off_t *len)
{
	struct stat	 st;
	off_t		 size;
	u_int8_t	*buf = NULL;
	int		 fd;

	if ((fd = open(name, O_RDONLY)) == -1)
		return (NULL);
	if (fstat(fd, &st) != 0)
		goto fail;
	size = st.st_size;
	if ((buf = (char *)calloc(1, size + 1)) == NULL)
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

int
relay_load_certfiles(struct relay *rlay)
{
	struct protocol *proto = rlay->rl_proto;
	char	 certfile[PATH_MAX];
	char	 hbuf[sizeof("ffff:ffff:ffff:ffff:ffff:ffff:255.255.255.255")];

	if ((rlay->rl_conf.flags & F_SSLCLIENT) && (proto->sslca != NULL)) {
		if ((rlay->rl_ssl_ca = relay_load_file(proto->sslca,
		    &rlay->rl_ssl_ca_len)) == NULL)
			return (-1);
		log_debug("relay_load_certfiles: using ca %s", proto->sslca);
	}

	if ((rlay->rl_conf.flags & F_SSL) == 0)
		return (0);

	if (print_host(&rlay->rl_conf.ss, hbuf, sizeof(hbuf)) == NULL)
		return (-1);

	if (snprintf(certfile, sizeof(certfile),
	    "/etc/ssl/%s.crt", hbuf) == -1)
		return (-1);
	if ((rlay->rl_ssl_cert = relay_load_file(certfile,
	    &rlay->rl_ssl_cert_len)) == NULL)
		return (-1);
	log_debug("relay_load_certfiles: using certificate %s", certfile);

	if (snprintf(certfile, sizeof(certfile),
	    "/etc/ssl/private/%s.key", hbuf) == -1)
		return -1;
	if ((rlay->rl_ssl_key = relay_load_file(certfile,
	    &rlay->rl_ssl_key_len)) == NULL)
		return (-1);
	log_debug("relay_load_certfiles: using private key %s", certfile);

	return (0);
}

static __inline int
relay_proto_cmp(struct protonode *a, struct protonode *b)
{
	int ret;
	ret = strcasecmp(a->key, b->key);
	if (ret == 0)
		ret = (int)a->type - b->type;
	return (ret);
}

RB_GENERATE(proto_tree, protonode, nodes, relay_proto_cmp);

int
relay_session_cmp(struct rsession *a, struct rsession *b)
{
	struct relay	*rlay = (struct relay *)b->se_relay;
	struct protocol	*proto = rlay->rl_proto;

	if (proto != NULL && proto->cmp != NULL)
		return ((*proto->cmp)(a, b));

	return ((int)a->se_id - b->se_id);
}

SPLAY_GENERATE(session_tree, rsession, se_nodes, relay_session_cmp);
