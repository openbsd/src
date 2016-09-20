/*	$OpenBSD: switchd.c,v 1.10 2016/09/20 16:45:09 rzalamena Exp $	*/

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
void	 parent_device_connect(struct privsep *, struct switch_device *);
int	 switch_device_cmp(struct switch_device *, struct switch_device *);

__dead void	usage(void);

static struct privsep_proc procs[] = {
	{ "ofp",	PROC_OFP, NULL, ofp },
	{ "control",	PROC_CONTROL, parent_dispatch_control, control },
	{ "ofcconn",	PROC_OFCCONN, NULL, ofcconn }
};

__dead void
usage(void)
{
	extern const char	*__progname;
	fprintf(stderr, "usage: %s [-dnv] [-D macro=value] [-f file] "
	    "[-c mac-cache-size] [-t cache-timeout]\n",
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
	TAILQ_INIT(&sc->sc_conns);

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
	log_verbose(verbose);

	/* Configure the control socket */
	ps->ps_csock.cs_name = SWITCHD_SOCKET;
	ps->ps_instance = proc_instance;
	if (title)
		ps->ps_title[proc_id] = title;

	/* Only the parent returns. */
	proc_init(ps, procs, nitems(procs), argc0, argv, proc_id);

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
	int	 fd;
	if ((fd = open("/dev/tap0", O_WRONLY)) == -1)
		return (-1);
	return (fd);
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
	struct switch_device	*c;

	TAILQ_FOREACH(c, &sc->sc_conns, sdv_next) {
		parent_device_connect(&sc->sc_ps, c);
	}

	return (0);
}

int
parent_reload(struct switchd *sc)
{
	struct switchd			 newconf;
	struct switch_device		*sdv, *osdv, *sdvn;
	enum privsep_procid		 procid;

	memset(&newconf, 0, sizeof(newconf));
	TAILQ_INIT(&newconf.sc_conns);

	if (parse_config(sc->sc_conffile, &newconf) != -1) {
		TAILQ_FOREACH_SAFE(sdv, &sc->sc_conns, sdv_next, sdvn) {
			TAILQ_FOREACH(osdv, &newconf.sc_conns, sdv_next) {
				if (switch_device_cmp(osdv, sdv) == 0) {
					TAILQ_REMOVE(&newconf.sc_conns,
					    osdv, sdv_next);
					break;
				}
			}
			if (osdv == NULL) {
				/* Removed */
				TAILQ_REMOVE(&sc->sc_conns, sdv, sdv_next);
				procid = (sdv->sdv_swc.swc_type ==
				    SWITCH_CONN_LOCAL)
				    ? PROC_OFP : PROC_OFCCONN;
				proc_compose_imsg(&sc->sc_ps, procid, -1,
				    IMSG_CTL_DEVICE_DISCONNECT,
				    -1, -1, sdv, sizeof(*sdv));
			} else {
				/* Keep the existing one */
				TAILQ_REMOVE(&newconf.sc_conns, osdv, sdv_next);
				free(osdv);
			}
		}
		TAILQ_FOREACH(sdv, &newconf.sc_conns, sdv_next) {
			procid =
			    (sdv->sdv_swc.swc_type == SWITCH_CONN_LOCAL)
			    ? PROC_OFP : PROC_OFCCONN;
			TAILQ_INSERT_TAIL(&sc->sc_conns, sdv, sdv_next);
			parent_device_connect(&sc->sc_ps, sdv);
		}
	}

	return (0);
}

int
parent_dispatch_control(int fd, struct privsep_proc *p, struct imsg *imsg)
{
	switch (imsg->hdr.type) {
	case IMSG_CTL_DEVICE_CONNECT:
	case IMSG_CTL_DEVICE_DISCONNECT:
		if (IMSG_DATA_SIZE(imsg) <
		    sizeof(struct switch_device)) {
			log_warnx("%s: IMSG_CTL_DEVICE_CONNECT: "
			    "message size is wrong", __func__);
			return (0);
		}
		if (imsg->hdr.type == IMSG_CTL_DEVICE_CONNECT)
			parent_device_connect(p->p_ps, imsg->data);
		else {
			/*
			 * Since we don't know which the device was attached
			 * to, we send the message to the both.
			 */
			proc_compose(p->p_ps, PROC_OFP,
			    imsg->hdr.type, imsg->data, IMSG_DATA_SIZE(imsg));
			proc_compose(p->p_ps, PROC_OFCCONN,
			    imsg->hdr.type, imsg->data, IMSG_DATA_SIZE(imsg));
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
parent_device_connect(struct privsep *ps, struct switch_device *sdv)
{
	int	 fd;

	/* restrict the opening path to /dev/switch* */
	if (strncmp(sdv->sdv_device, "/dev/switch", 11) != 0) {
		log_warnx("%s: device path is wrong: %s", __func__,
		    sdv->sdv_device);
		goto on_error;
	}

	if ((fd = open(sdv->sdv_device, O_RDWR | O_NONBLOCK)) == -1) {
		log_warn("%s: open(%s) failed", __func__, sdv->sdv_device);
		goto on_error;
	}

	switch (sdv->sdv_swc.swc_type) {
	case SWITCH_CONN_LOCAL:
		proc_compose_imsg(ps, PROC_OFP, -1, IMSG_CTL_DEVICE_CONNECT,
		    -1, fd, sdv, sizeof(*sdv));
		break;
	case SWITCH_CONN_TLS:
	case SWITCH_CONN_TCP:
		proc_compose_imsg(ps, PROC_OFCCONN, -1, IMSG_CTL_DEVICE_CONNECT,
		    -1, fd, sdv, sizeof(struct switch_device));
		break;
	default:
		fatalx("not implemented");
	}
on_error:
	return;
}

int
switch_device_cmp(struct switch_device *a,
    struct switch_device *b)
{
	struct switch_controller	*ca = &a->sdv_swc;
	struct switch_controller	*cb = &b->sdv_swc;
	int				 c;

	if ((c = strcmp(a->sdv_device, b->sdv_device)) != 0)
		return (c);
	if ((c = cb->swc_type - ca->swc_type) != 0)
		return (c);

	return (sockaddr_cmp((struct sockaddr *)&ca->swc_addr,
	    (struct sockaddr *)&cb->swc_addr, -1));
}
