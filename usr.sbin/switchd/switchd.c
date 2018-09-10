/*	$OpenBSD: switchd.c,v 1.16 2018/09/10 13:21:39 akoshibe Exp $	*/

/*
 * Copyright (c) 2013-2016 Reyk Floeter <reyk@openbsd.org>
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
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/un.h>
#include <sys/queue.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <syslog.h>
#include <fcntl.h>
#include <errno.h>
#include <pwd.h>
#include <event.h>

#include "switchd.h"

void	 parent_shutdown(struct switchd *);
void	 parent_sig_handler(int, short, void *);
int	 parent_dispatch_ofp(int, struct privsep_proc *, struct imsg *);
int	 parent_dispatch_control(int, struct privsep_proc *, struct imsg *);
int	 parent_configure(struct switchd *);
int	 parent_reload(struct switchd *);
void	 parent_connect(struct privsep *, struct switch_client *);
void	 parent_connected(int, short, void *);
void	 parent_disconnect(struct privsep *, struct switch_client *);

__dead void	usage(void);

static struct privsep_proc procs[] = {
	{ "ofp",	PROC_OFP,	NULL, ofp },
	{ "control",	PROC_CONTROL,	parent_dispatch_control, control },
	{ "ofcconn",	PROC_OFCCONN,	NULL, ofcconn }
};

__dead void
usage(void)
{
	extern const char	*__progname;
	fprintf(stderr, "usage: %s [-dnv] [-c cachesize]  [-D macro=value] "
	    "[-f file] [-t timeout]\n",
	    __progname);
	exit(1);
}

int
main(int argc, char *argv[])
{
	struct switchd		*sc = NULL;
	struct privsep		*ps = NULL;
	struct switch_server	*srv;
	const char		*errstr = NULL;
	int			 c;
	int			 debug = 0, verbose = 0;
	uint32_t		 opts = 0;
	unsigned int		 cache = SWITCHD_CACHE_MAX;
	unsigned int		 timeout = SWITCHD_CACHE_TIMEOUT;
	const char		*conffile = SWITCHD_CONFIG;
	const char		*errp, *title = NULL;
	enum privsep_procid	 proc_id = PROC_PARENT;
	int			 argc0 = argc, proc_instance = 0;

	log_init(1, LOG_DAEMON);

	while ((c = getopt(argc, argv, "c:dD:f:hI:nP:t:v")) != -1) {
		switch (c) {
		case 'c':
			cache = strtonum(optarg, 1, UINT32_MAX, &errstr);
			if (errstr != NULL) {
				log_warn("max cache size: %s", errstr);
				usage();
			}
			break;
		case 'd':
			debug++;
			break;
		case 'D':
			if (cmdline_symset(optarg) < 0)
				log_warnx("could not parse macro definition %s",
				    optarg);
			break;
		case 'f':
			conffile = optarg;
			break;
		case 'I':
			proc_instance = strtonum(optarg, 0,
			    PROC_MAX_INSTANCES, &errp);
			if (errp)
				fatalx("invalid process instance");
			break;
		case 'n':
			opts |= SWITCHD_OPT_NOACTION;
			break;
		case 'P':
			title = optarg;
			proc_id = proc_getid(procs, nitems(procs), title);
			if (proc_id == PROC_MAX)
				fatalx("invalid process name");
			break;
		case 't':
			timeout = strtonum(optarg, 0, UINT32_MAX, &errstr);
			if (errstr != NULL) {
				log_warn("cache timeout: %s", errstr);
				usage();
			}
			break;
		case 'v':
			verbose++;
			opts |= SWITCHD_OPT_VERBOSE;
			break;
		default:
			usage();
		}
	}

	if ((sc = calloc(1, sizeof(*sc))) == NULL)
		fatal("calloc");

	if (strlcpy(sc->sc_conffile, conffile, PATH_MAX) >= PATH_MAX)
		fatal("config file exceeds PATH_MAX");

	sc->sc_cache_max = cache;
	sc->sc_cache_timeout = timeout;
	sc->sc_opts = opts;

	srv = &sc->sc_server;
	srv->srv_sc = sc;

	ps = &sc->sc_ps;
	ps->ps_env = sc;
	TAILQ_INIT(&ps->ps_rcsocks);
	TAILQ_INIT(&sc->sc_clients);

	if (parse_config(sc->sc_conffile, sc) == -1) {
		proc_kill(&sc->sc_ps);
		exit(1);
	}

	if (opts & SWITCHD_OPT_NOACTION) {
		fprintf(stderr, "configuration OK\n");
		proc_kill(&sc->sc_ps);
		exit(0);
	}

	/* check for root privileges */
	if (geteuid())
		fatalx("need root privileges");

	if ((ps->ps_pw =  getpwnam(SWITCHD_USER)) == NULL)
		fatalx("unknown user " SWITCHD_USER);

	log_init(debug, LOG_DAEMON);
	log_setverbose(verbose);

	/* Configure the control socket */
	ps->ps_csock.cs_name = SWITCHD_SOCKET;
	ps->ps_instance = proc_instance;
	if (title)
		ps->ps_title[proc_id] = title;

	/* Only the parent returns. */
	proc_init(ps, procs, nitems(procs), debug, argc0, argv, proc_id);

	if (!debug && daemon(0, 0) == -1)
		fatal("failed to daemonize");

	log_procinit("parent");

	/*
	 * pledge in the parent process:
	 * stdio - for malloc and basic I/O including events.
	 * rpath - for reload to open and read the configuration files.
	 * wpath - for accessing the /dev/switch device.
	 * inet - for opening OpenFlow and device sockets.
	 * dns - for resolving host in the configuration files.
	 * sendfd - send sockets to child processes on reload.
	 */
	if (pledge("stdio rpath wpath inet dns sendfd", NULL) == -1)
		fatal("pledge");

	event_init();

	signal_set(&ps->ps_evsigint, SIGINT, parent_sig_handler, ps);
	signal_set(&ps->ps_evsigterm, SIGTERM, parent_sig_handler, ps);
	signal_set(&ps->ps_evsighup, SIGHUP, parent_sig_handler, ps);
	signal_set(&ps->ps_evsigpipe, SIGPIPE, parent_sig_handler, ps);
	signal_set(&ps->ps_evsigusr1, SIGUSR1, parent_sig_handler, ps);

	signal_add(&ps->ps_evsigint, NULL);
	signal_add(&ps->ps_evsigterm, NULL);
	signal_add(&ps->ps_evsighup, NULL);
	signal_add(&ps->ps_evsigpipe, NULL);
	signal_add(&ps->ps_evsigusr1, NULL);

	proc_connect(ps);

	if (parent_configure(sc) == -1)
		fatalx("configuration failed");

	event_dispatch();

	log_debug("%d parent exiting", getpid());

	return (0);
}

int
switchd_socket(struct sockaddr *sock, int reuseport)
{
	int s = -1, val;
	struct linger lng;

	if ((s = socket(sock->sa_family, SOCK_STREAM | SOCK_NONBLOCK,
	    IPPROTO_TCP)) == -1)
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

	/*
	 * TCP options
	 */
	val = 1;
	if (setsockopt(s, IPPROTO_TCP, TCP_NODELAY,
	    &val, sizeof(val)) == -1)
		goto bad;

	return (s);

 bad:
	if (s != -1)
		close(s);
	return (-1);
}

int
switchd_listen(struct sockaddr *sock)
{
	int s;

	if ((s = switchd_socket(sock, 1)) == -1)
		return (-1);

	if (bind(s, sock, sock->sa_len) == -1)
		goto bad;
	if (listen(s, 10) == -1)
		goto bad;

	return (s);

 bad:
	close(s);
	return (-1);
}

int
switchd_tap(void)
{
	char	 path[PATH_MAX];
	int	 i, fd;

	for (i = 0; i < SWITCHD_MAX_TAP; i++) {
		snprintf(path, PATH_MAX, "/dev/tap%d", i);
		fd = open(path, O_RDWR | O_NONBLOCK);
		if (fd != -1)
			return (fd);
	}

	return (-1);
}

struct switch_connection *
switchd_connbyid(struct switchd *sc, unsigned int id, unsigned int instance)
{
	struct switch_connection	*con;

	TAILQ_FOREACH(con, &sc->sc_conns, con_entry) {
		if (con->con_id == id && con->con_instance == instance)
			return (con);
	}

	return (NULL);
}

struct switch_connection *
switchd_connbyaddr(struct switchd *sc, struct sockaddr *sa)
{
	struct switch_connection	*con;

	TAILQ_FOREACH(con, &sc->sc_conns, con_entry) {
		if (sockaddr_cmp((struct sockaddr *)
		    &con->con_peer, sa, -1) == 0)
			return (con);
	}

	return (NULL);
}

void
parent_sig_handler(int sig, short event, void *arg)
{
	struct privsep	*ps = arg;

	switch (sig) {
	case SIGHUP:
		log_info("%s: reload requested with SIGHUP", __func__);

		/*
		 * This is safe because libevent uses async signal handlers
		 * that run in the event loop and not in signal context.
		 */
		parent_reload(ps->ps_env);
		break;
	case SIGPIPE:
		log_info("%s: ignoring SIGPIPE", __func__);
		break;
	case SIGUSR1:
		log_info("%s: ignoring SIGUSR1", __func__);
		break;
	case SIGTERM:
	case SIGINT:
		parent_shutdown(ps->ps_env);
		break;
	default:
		fatalx("unexpected signal");
	}
}

int
parent_configure(struct switchd *sc)
{
	struct switch_client	*swc, *swcn;
	int			 fd;

	if ((fd = switchd_tap()) == -1)
		fatal("%s: tap", __func__);
	proc_compose_imsg(&sc->sc_ps, PROC_OFP, -1,
	    IMSG_TAPFD, -1, fd, NULL, 0);

	TAILQ_FOREACH_SAFE(swc, &sc->sc_clients, swc_next, swcn) {
		parent_connect(&sc->sc_ps, swc);
	}

	return (0);
}

int
parent_reload(struct switchd *sc)
{
	struct switchd			 newconf;
	struct switch_client		*swc, *oswc, *swcn;

	memset(&newconf, 0, sizeof(newconf));
	TAILQ_INIT(&newconf.sc_clients);
	TAILQ_INIT(&newconf.sc_conns);

	if (parse_config(sc->sc_conffile, &newconf) != -1) {
		TAILQ_FOREACH_SAFE(swc, &sc->sc_clients, swc_next, swcn) {
			TAILQ_FOREACH(oswc, &newconf.sc_clients, swc_next) {
				if (sockaddr_cmp((struct sockaddr *)
				    &oswc->swc_addr.swa_addr,
				    (struct sockaddr *)
				    &swc->swc_addr.swa_addr, -1) == 0) {
					TAILQ_REMOVE(&newconf.sc_clients,
					    oswc, swc_next);
					break;
				}
			}
			if (oswc == NULL) {
				/* Removed */
				parent_disconnect(&sc->sc_ps, swc);
			} else {
				/* Keep the existing one */
				TAILQ_REMOVE(&newconf.sc_clients,
				    oswc, swc_next);
				free(oswc);
			}
		}
		TAILQ_FOREACH_SAFE(swc, &newconf.sc_clients, swc_next, swcn) {
			TAILQ_REMOVE(&newconf.sc_clients, swc, swc_next);
			TAILQ_INSERT_TAIL(&sc->sc_clients, swc, swc_next);

			parent_connect(&sc->sc_ps, swc);
		}
	}

	return (0);
}

int
parent_dispatch_control(int fd, struct privsep_proc *p, struct imsg *imsg)
{
	struct switch_client	*swc, *oswc;
	struct privsep		*ps = p->p_ps;
	struct switchd		*sc = ps->ps_env;

	switch (imsg->hdr.type) {
	case IMSG_CTL_CONNECT:
	case IMSG_CTL_DISCONNECT:
		IMSG_SIZE_CHECK(imsg, swc);

		/* Need to allocate it in case it is reused */
		if ((swc = calloc(1, sizeof(*swc))) == NULL) {
			log_warnx("%s: calloc", __func__);
			return (0);
		}
		memcpy(swc, imsg->data, sizeof(*swc));
		memset(&swc->swc_ev, 0, sizeof(swc->swc_ev));

		if (imsg->hdr.type == IMSG_CTL_CONNECT) {
			TAILQ_INSERT_TAIL(&sc->sc_clients, swc, swc_next);
			parent_connect(p->p_ps, swc);
		} else {
			TAILQ_FOREACH(oswc, &sc->sc_clients, swc_next) {
				if (sockaddr_cmp((struct sockaddr *)
				    &oswc->swc_addr.swa_addr,
				    (struct sockaddr *)
				    &swc->swc_addr.swa_addr, -1) == 0) {
					parent_disconnect(ps, oswc);
					break;
				}
			}
			if (oswc == NULL)
				log_warnx("client %s is not connected",
				    print_host(&swc->swc_addr.swa_addr,
				    NULL, 0));
			free(swc);
		}
		return (0);
	default:
		break;
	}

	return (-1);
}

void
parent_shutdown(struct switchd *sc)
{
	proc_kill(&sc->sc_ps);

	free(sc);

	log_warnx("parent terminating");
	exit(0);
}

void
parent_connect(struct privsep *ps, struct switch_client *swc)
{
	struct switchd		*sc = ps->ps_env;
	struct sockaddr_storage	*ss;
	struct sockaddr_un	*un;
	struct sockaddr_in	*sin4;
	struct sockaddr_in6	*sin6;
	int			 fd = -1;
	struct timeval		 tv;

	ss = &swc->swc_addr.swa_addr;

	if (ss->ss_len == 0) {
		log_warnx("%s: invalid address", __func__);
		goto fail;
	}
	swc->swc_arg = ps;
	memset(&swc->swc_ev, 0, sizeof(swc->swc_ev));

	switch (ss->ss_family) {
	case AF_LOCAL:
		un = (struct sockaddr_un *)ss;

		/* restrict the opening path to /dev/switch* */
		if (strncmp(un->sun_path, "/dev/switch",
		    strlen("/dev/switch")) != 0) {
			log_warnx("%s: device path is wrong: %s", __func__,
			    un->sun_path);
			goto fail;
		}

		if ((fd = open(un->sun_path, O_RDWR | O_NONBLOCK)) == -1) {
			log_warn("%s: failed to open %s",
			    __func__, un->sun_path);
			goto fail;
		}
		break;
	case AF_INET:
	case AF_INET6:
		if (ss->ss_family == AF_INET) {
			sin4 = (struct sockaddr_in *)ss;
			if (sin4->sin_port == 0)
				sin4->sin_port = htons(SWITCHD_CTLR_PORT);
		} else if (ss->ss_family == AF_INET6) {
			sin6 = (struct sockaddr_in6 *)ss;
			if (sin6->sin6_port == 0)
				sin6->sin6_port = htons(SWITCHD_CTLR_PORT);
		}

		if ((fd = switchd_socket((struct sockaddr *)ss, 0)) == -1) {
			log_debug("%s: failed to get socket for %s", __func__,
			    print_host(ss, NULL, 0));
			goto fail;
		}

 retry:
		if (connect(fd, (struct sockaddr *)ss, ss->ss_len) == -1) {
			if (errno == EINTR)
				goto retry;
			if (errno == EINPROGRESS) {
				tv.tv_sec = SWITCHD_CONNECT_TIMEOUT;
				tv.tv_usec = 0;
				event_set(&swc->swc_ev, fd, EV_WRITE|EV_TIMEOUT,
				    parent_connected, swc);
				event_add(&swc->swc_ev, &tv);
				return;
			}

			log_warn("%s: failed to connect to %s, fd %d", __func__,
			    print_host(ss, NULL, 0), fd);
			goto fail;
		}

		break;
	}

	parent_connected(fd, 0, swc);
	return;

 fail:
	TAILQ_REMOVE(&sc->sc_clients, swc, swc_next);
	free(swc);
}

void
parent_connected(int fd, short event, void *arg)
{
	struct switch_client	*swc = arg;
	struct privsep		*ps = swc->swc_arg;
	struct switchd		*sc = ps->ps_env;

	if (event & EV_TIMEOUT) {
		log_debug("%s: failed to connect to %s", __func__,
		    print_host(&swc->swc_addr.swa_addr, NULL, 0));
		TAILQ_REMOVE(&sc->sc_clients, swc, swc_next);
		free(swc);
		return;
	}

	switch (swc->swc_target.swa_type) {
	case SWITCH_CONN_LOCAL:
		proc_compose_imsg(ps, PROC_OFP, -1, IMSG_CTL_CONNECT,
		    -1, fd, swc, sizeof(*swc));
		break;
	case SWITCH_CONN_TLS:
	case SWITCH_CONN_TCP:
		proc_compose_imsg(ps, PROC_OFCCONN, -1, IMSG_CTL_CONNECT,
		    -1, fd, swc, sizeof(*swc));
		break;
	default:
		fatalx("not implemented");
	}
}

void
parent_disconnect(struct privsep *ps, struct switch_client *swc)
{
	struct switchd		*sc = ps->ps_env;
	enum privsep_procid	 target;

	TAILQ_REMOVE(&sc->sc_clients, swc, swc_next);

	target = swc->swc_target.swa_type == SWITCH_CONN_LOCAL ?
	    PROC_OFP : PROC_OFCCONN;
	proc_compose(ps, target, IMSG_CTL_DISCONNECT, swc, sizeof(*swc));

	free(swc);
}
