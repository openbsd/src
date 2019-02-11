/*	$OpenBSD: unwind.c,v 1.14 2019/02/11 13:38:53 florian Exp $	*/

/*
 * Copyright (c) 2018 Florian Obser <florian@openbsd.org>
 * Copyright (c) 2005 Claudio Jeker <claudio@openbsd.org>
 * Copyright (c) 2004 Esben Norby <norby@openbsd.org>
 * Copyright (c) 2003, 2004 Henning Brauer <henning@openbsd.org>
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
#include <sys/syslog.h>
#include <sys/wait.h>

#include <net/if.h>
#include <net/route.h>

#include <err.h>
#include <errno.h>
#include <event.h>
#include <fcntl.h>
#include <imsg.h>
#include <netdb.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

#include "uw_log.h"
#include "unwind.h"
#include "frontend.h"
#include "resolver.h"
#include "control.h"
#include "captiveportal.h"

#define	LEASE_DB_DIR	"/var/db/"
#define	_PATH_LEASE_DB	"/var/db/dhclient.leases."

#define	TRUST_ANCHOR_FILE	"/var/db/unwind.key"

__dead void	usage(void);
__dead void	main_shutdown(void);

void	main_sig_handler(int, short, void *);

static pid_t	start_child(int, char *, int, int, int);

void	main_dispatch_frontend(int, short, void *);
void	main_dispatch_resolver(int, short, void *);
void	main_dispatch_captiveportal(int, short, void *);

static int	main_imsg_send_ipc_sockets(struct imsgbuf *, struct imsgbuf *,
		    struct imsgbuf *);
static int	main_imsg_send_config(struct unwind_conf *);

int	main_reload(void);
int	main_sendall(enum imsg_type, void *, uint16_t);
void	open_dhcp_lease(int);
void	open_ports(void);

struct unwind_conf	*main_conf;
struct imsgev		*iev_frontend;
struct imsgev		*iev_resolver;
struct imsgev		*iev_captiveportal;
char			*conffile;

pid_t	 frontend_pid;
pid_t	 resolver_pid;
pid_t	 captiveportal_pid;

uint32_t cmd_opts;

void
main_sig_handler(int sig, short event, void *arg)
{
	/*
	 * Normal signal handler rules don't apply because libevent
	 * decouples for us.
	 */

	switch (sig) {
	case SIGTERM:
	case SIGINT:
		main_shutdown();
		break;
	case SIGHUP:
		if (main_reload() == -1)
			log_warnx("configuration reload failed");
		else
			log_debug("configuration reloaded");
		break;
	default:
		fatalx("unexpected signal");
	}
}

__dead void
usage(void)
{
	extern char *__progname;

	fprintf(stderr, "usage: %s [-dnv] [-f file] [-s socket]\n",
	    __progname);
	exit(1);
}

int
main(int argc, char *argv[])
{
	struct event		 ev_sigint, ev_sigterm, ev_sighup;
	int			 ch;
	int			 debug = 0, resolver_flag = 0;
	int			 frontend_flag = 0, captiveportal_flag = 0;
	char			*saved_argv0;
	int			 pipe_main2frontend[2];
	int			 pipe_main2resolver[2];
	int			 pipe_main2captiveportal[2];
	int			 frontend_routesock, rtfilter;
	int			 control_fd, ta_fd;
	char			*csock;

	conffile = CONF_FILE;
	csock = UNWIND_SOCKET;

	log_init(1, LOG_DAEMON);	/* Log to stderr until daemonized. */
	log_setverbose(1);

	saved_argv0 = argv[0];
	if (saved_argv0 == NULL)
		saved_argv0 = "unwind";

	while ((ch = getopt(argc, argv, "CdEFf:ns:v")) != -1) {
		switch (ch) {
		case 'C':
			captiveportal_flag = 1;
			break;
		case 'd':
			debug = 1;
			break;
		case 'E':
			resolver_flag = 1;
			break;
		case 'F':
			frontend_flag = 1;
			break;
		case 'f':
			conffile = optarg;
			break;
		case 'n':
			cmd_opts |= OPT_NOACTION;
			break;
		case 's':
			csock = optarg;
			break;
		case 'v':
			if (cmd_opts & OPT_VERBOSE)
				cmd_opts |= OPT_VERBOSE2;
			cmd_opts |= OPT_VERBOSE;
			break;
		default:
			usage();
		}
	}

	argc -= optind;
	argv += optind;
	if (argc > 0 || (resolver_flag && frontend_flag && captiveportal_flag))
		usage();

	if (resolver_flag)
		resolver(debug, cmd_opts & (OPT_VERBOSE | OPT_VERBOSE2));
	else if (frontend_flag)
		frontend(debug, cmd_opts & (OPT_VERBOSE | OPT_VERBOSE2));
	else if (captiveportal_flag)
		captiveportal(debug, cmd_opts & (OPT_VERBOSE | OPT_VERBOSE2));

	if ((main_conf = parse_config(conffile)) == NULL)
		exit(1);

	if (cmd_opts & OPT_NOACTION) {
		if (cmd_opts & OPT_VERBOSE)
			print_config(main_conf);
		else
			fprintf(stderr, "configuration OK\n");
		exit(0);
	}

	/* Check for root privileges. */
	if (geteuid())
		errx(1, "need root privileges");

	/* Check for assigned daemon user */
	if (getpwnam(UNWIND_USER) == NULL)
		errx(1, "unknown user %s", UNWIND_USER);

	log_init(debug, LOG_DAEMON);
	log_setverbose(cmd_opts & OPT_VERBOSE);

	if (!debug)
		daemon(1, 0);

	log_info("startup");

	if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK,
	    PF_UNSPEC, pipe_main2frontend) == -1)
		fatal("main2frontend socketpair");
	if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK,
	    PF_UNSPEC, pipe_main2resolver) == -1)
		fatal("main2resolver socketpair");
	if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK,
	    PF_UNSPEC, pipe_main2captiveportal) == -1)
		fatal("main2captiveportal socketpair");

	/* Start children. */
	resolver_pid = start_child(PROC_RESOLVER, saved_argv0,
	    pipe_main2resolver[1], debug, cmd_opts & (OPT_VERBOSE |
	    OPT_VERBOSE2));
	frontend_pid = start_child(PROC_FRONTEND, saved_argv0,
	    pipe_main2frontend[1], debug, cmd_opts & (OPT_VERBOSE |
	    OPT_VERBOSE2));
	captiveportal_pid = start_child(PROC_CAPTIVEPORTAL, saved_argv0,
	    pipe_main2captiveportal[1], debug, cmd_opts & (OPT_VERBOSE |
	    OPT_VERBOSE2));

	unwind_process = PROC_MAIN;
	log_procinit(log_procnames[unwind_process]);

	event_init();

	/* Setup signal handler. */
	signal_set(&ev_sigint, SIGINT, main_sig_handler, NULL);
	signal_set(&ev_sigterm, SIGTERM, main_sig_handler, NULL);
	signal_set(&ev_sighup, SIGHUP, main_sig_handler, NULL);
	signal_add(&ev_sigint, NULL);
	signal_add(&ev_sigterm, NULL);
	signal_add(&ev_sighup, NULL);
	signal(SIGPIPE, SIG_IGN);

	/* Setup pipes to children. */

	if ((iev_frontend = malloc(sizeof(struct imsgev))) == NULL ||
	    (iev_captiveportal = malloc(sizeof(struct imsgev))) == NULL ||
	    (iev_resolver = malloc(sizeof(struct imsgev))) == NULL)
		fatal(NULL);
	imsg_init(&iev_frontend->ibuf, pipe_main2frontend[0]);
	iev_frontend->handler = main_dispatch_frontend;
	imsg_init(&iev_resolver->ibuf, pipe_main2resolver[0]);
	iev_resolver->handler = main_dispatch_resolver;
	imsg_init(&iev_captiveportal->ibuf, pipe_main2captiveportal[0]);
	iev_captiveportal->handler = main_dispatch_captiveportal;

	/* Setup event handlers for pipes. */
	iev_frontend->events = EV_READ;
	event_set(&iev_frontend->ev, iev_frontend->ibuf.fd,
	    iev_frontend->events, iev_frontend->handler, iev_frontend);
	event_add(&iev_frontend->ev, NULL);

	iev_resolver->events = EV_READ;
	event_set(&iev_resolver->ev, iev_resolver->ibuf.fd, iev_resolver->events,
	    iev_resolver->handler, iev_resolver);
	event_add(&iev_resolver->ev, NULL);

	iev_captiveportal->events = EV_READ;
	event_set(&iev_captiveportal->ev, iev_captiveportal->ibuf.fd,
	    iev_captiveportal->events, iev_captiveportal->handler,
	    iev_captiveportal);
	event_add(&iev_captiveportal->ev, NULL);

	if (main_imsg_send_ipc_sockets(&iev_frontend->ibuf,
	    &iev_resolver->ibuf, &iev_captiveportal->ibuf))
		fatal("could not establish imsg links");

	if ((control_fd = control_init(csock)) == -1)
		fatalx("control socket setup failed");

	if ((frontend_routesock = socket(AF_ROUTE, SOCK_RAW | SOCK_CLOEXEC,
	    AF_INET)) < 0)
		fatal("route socket");

	rtfilter = ROUTE_FILTER(RTM_IFINFO) | ROUTE_FILTER(RTM_PROPOSAL) |
	    ROUTE_FILTER(RTM_GET);
	if (setsockopt(frontend_routesock, AF_ROUTE, ROUTE_MSGFILTER,
	    &rtfilter, sizeof(rtfilter)) < 0)
		fatal("setsockopt(ROUTE_MSGFILTER)");

	if ((ta_fd = open(TRUST_ANCHOR_FILE, O_RDWR | O_CREAT, 0644)) == -1)
		log_warn("%s", TRUST_ANCHOR_FILE);

	/* receiver handles failed open correctly */
	main_imsg_compose_frontend_fd(IMSG_TAFD, 0, ta_fd);

	main_imsg_compose_frontend_fd(IMSG_CONTROLFD, 0, control_fd);
	main_imsg_compose_frontend_fd(IMSG_ROUTESOCK, 0, frontend_routesock);
	main_imsg_send_config(main_conf);

	if (unveil(LEASE_DB_DIR, "r") == -1 && errno != ENOENT)
		err(1, "unveil");

	if (pledge("stdio inet dns rpath sendfd", NULL) == -1)
		fatal("pledge");

	main_imsg_compose_frontend(IMSG_STARTUP, 0, NULL, 0);
	main_imsg_compose_resolver(IMSG_STARTUP, 0, NULL, 0);

	event_dispatch();

	main_shutdown();
	return (0);
}

__dead void
main_shutdown(void)
{
	pid_t	 pid;
	int	 status;

	/* Close pipes. */
	msgbuf_clear(&iev_frontend->ibuf.w);
	close(iev_frontend->ibuf.fd);
	msgbuf_clear(&iev_resolver->ibuf.w);
	close(iev_resolver->ibuf.fd);
	msgbuf_clear(&iev_captiveportal->ibuf.w);
	close(iev_captiveportal->ibuf.fd);

	config_clear(main_conf);

	log_debug("waiting for children to terminate");
	do {
		pid = wait(&status);
		if (pid == -1) {
			if (errno != EINTR && errno != ECHILD)
				fatal("wait");
		} else if (WIFSIGNALED(status))
			log_warnx("%s terminated; signal %d",
			    (pid == resolver_pid) ? "resolver" :
			    "frontend", WTERMSIG(status));
	} while (pid != -1 || (pid == -1 && errno == EINTR));

	free(iev_frontend);
	free(iev_resolver);
	free(iev_captiveportal);

	log_info("terminating");
	exit(0);
}

static pid_t
start_child(int p, char *argv0, int fd, int debug, int verbose)
{
	char	*argv[7];
	int	 argc = 0;
	pid_t	 pid;

	switch (pid = fork()) {
	case -1:
		fatal("cannot fork");
	case 0:
		break;
	default:
		close(fd);
		return (pid);
	}

	if (dup2(fd, 3) == -1)
		fatal("cannot setup imsg fd");

	argv[argc++] = argv0;
	switch (p) {
	case PROC_MAIN:
		fatalx("Can not start main process");
	case PROC_RESOLVER:
		argv[argc++] = "-E";
		break;
	case PROC_FRONTEND:
		argv[argc++] = "-F";
		break;
	case PROC_CAPTIVEPORTAL:
		argv[argc++] = "-C";
		break;
	}
	if (debug)
		argv[argc++] = "-d";
	if (verbose & OPT_VERBOSE)
		argv[argc++] = "-v";
	if (verbose & OPT_VERBOSE2)
		argv[argc++] = "-v";
	argv[argc++] = NULL;

	execvp(argv0, argv);
	fatal("execvp");
}

void
main_dispatch_frontend(int fd, short event, void *bula)
{
	struct imsgev		*iev = bula;
	struct imsgbuf		*ibuf;
	struct imsg		 imsg;
	ssize_t			 n;
	int			 shut = 0, verbose;
	u_short			 rtm_index;

	ibuf = &iev->ibuf;

	if (event & EV_READ) {
		if ((n = imsg_read(ibuf)) == -1 && errno != EAGAIN)
			fatal("imsg_read error");
		if (n == 0)	/* Connection closed. */
			shut = 1;
	}
	if (event & EV_WRITE) {
		if ((n = msgbuf_write(&ibuf->w)) == -1 && errno != EAGAIN)
			fatal("msgbuf_write");
		if (n == 0)	/* Connection closed. */
			shut = 1;
	}

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatal("imsg_get");
		if (n == 0)	/* No more messages. */
			break;

		switch (imsg.hdr.type) {
		case IMSG_STARTUP_DONE:
			break;
		case IMSG_CTL_RELOAD:
			if (main_reload() == -1)
				log_warnx("configuration reload failed");
			else
				log_warnx("configuration reloaded");
			break;
		case IMSG_CTL_LOG_VERBOSE:
			if (imsg.hdr.len != IMSG_HEADER_SIZE +
			    sizeof(verbose))
				fatalx("%s: IMSG_CTL_LOG_VERBOSE wrong length: "
				    "%d", __func__, imsg.hdr.len);
			memcpy(&verbose, imsg.data, sizeof(verbose));
			log_setverbose(verbose);
			break;
		case IMSG_OPEN_DHCP_LEASE:
			if (imsg.hdr.len != IMSG_HEADER_SIZE +
			    sizeof(rtm_index))
				fatalx("%s: IMSG_OPEN_DHCP_LEASE wrong length: "
				    "%d", __func__, imsg.hdr.len);
			memcpy(&rtm_index, imsg.data, sizeof(rtm_index));
			open_dhcp_lease(rtm_index);
			break;
		case IMSG_OPEN_PORTS:
			open_ports();
			break;
		default:
			log_debug("%s: error handling imsg %d", __func__,
			    imsg.hdr.type);
			break;
		}
		imsg_free(&imsg);
	}
	if (!shut)
		imsg_event_add(iev);
	else {
		/* This pipe is dead. Remove its event handler */
		event_del(&iev->ev);
		event_loopexit(NULL);
	}
}

void
main_dispatch_resolver(int fd, short event, void *bula)
{
	struct imsgev		*iev = bula;
	struct imsgbuf		*ibuf;
	struct imsg		 imsg;
	struct sockaddr_in	 sin;
	ssize_t			 n;
	int			 shut = 0, httpsock;

	ibuf = &iev->ibuf;

	if (event & EV_READ) {
		if ((n = imsg_read(ibuf)) == -1 && errno != EAGAIN)
			fatal("imsg_read error");
		if (n == 0)	/* Connection closed. */
			shut = 1;
	}
	if (event & EV_WRITE) {
		if ((n = msgbuf_write(&ibuf->w)) == -1 && errno != EAGAIN)
			fatal("msgbuf_write");
		if (n == 0)	/* Connection closed. */
			shut = 1;
	}

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatal("imsg_get");
		if (n == 0)	/* No more messages. */
			break;

		switch (imsg.hdr.type) {
		case IMSG_OPEN_HTTP_PORT:
			if (imsg.hdr.len != IMSG_HEADER_SIZE +
			    sizeof(sin))
				fatalx("%s: IMSG_OPEN_HTTP_PORT wrong length: "
				    "%d", __func__, imsg.hdr.len);
			memcpy(&sin, imsg.data, sizeof(sin));

			if ((httpsock = socket(AF_INET, SOCK_STREAM |
			    SOCK_CLOEXEC | SOCK_NONBLOCK, 0)) == -1) {
				log_warn("%s: socket", __func__);
				break;
			}
			if (connect(httpsock, (struct sockaddr *)&sin,
			    sin.sin_len) == -1) {
				if (errno != EINPROGRESS) {
					log_warn("%s: connect", __func__);
					close(httpsock);
					break;
				}
			}
			main_imsg_compose_captiveportal_fd(IMSG_HTTPSOCK, 0,
			    httpsock);
			break;
		default:
			log_debug("%s: error handling imsg %d", __func__,
			    imsg.hdr.type);
			break;
		}
		imsg_free(&imsg);
	}
	if (!shut)
		imsg_event_add(iev);
	else {
		/* This pipe is dead. Remove its event handler. */
		event_del(&iev->ev);
		event_loopexit(NULL);
	}
}

void
main_dispatch_captiveportal(int fd, short event, void *bula)
{
	struct imsgev	*iev = bula;
	struct imsgbuf  *ibuf;
	struct imsg	 imsg;
	ssize_t		 n;
	int		 shut = 0;

	ibuf = &iev->ibuf;

	if (event & EV_READ) {
		if ((n = imsg_read(ibuf)) == -1 && errno != EAGAIN)
			fatal("imsg_read error");
		if (n == 0)	/* Connection closed. */
			shut = 1;
	}
	if (event & EV_WRITE) {
		if ((n = msgbuf_write(&ibuf->w)) == -1 && errno != EAGAIN)
			fatal("msgbuf_write");
		if (n == 0)	/* Connection closed. */
			shut = 1;
	}

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatal("imsg_get");
		if (n == 0)	/* No more messages. */
			break;

		switch (imsg.hdr.type) {
		default:
			log_debug("%s: error handling imsg %d", __func__,
			    imsg.hdr.type);
			break;
		}
		imsg_free(&imsg);
	}
	if (!shut)
		imsg_event_add(iev);
	else {
		/* This pipe is dead. Remove its event handler. */
		event_del(&iev->ev);
		event_loopexit(NULL);
	}
}

void
main_imsg_compose_frontend(int type, pid_t pid, void *data, uint16_t datalen)
{
	if (iev_frontend)
		imsg_compose_event(iev_frontend, type, 0, pid, -1, data,
		    datalen);
}

void
main_imsg_compose_frontend_fd(int type, pid_t pid, int fd)
{
	if (iev_frontend)
		imsg_compose_event(iev_frontend, type, 0, pid, fd, NULL, 0);
}

void
main_imsg_compose_resolver(int type, pid_t pid, void *data, uint16_t datalen)
{
	if (iev_resolver)
		imsg_compose_event(iev_resolver, type, 0, pid, -1, data,
		    datalen);
}

void
main_imsg_compose_captiveportal(int type, pid_t pid, void *data,
    uint16_t datalen)
{
	if (iev_captiveportal)
		imsg_compose_event(iev_captiveportal, type, 0, pid, -1, data,
		    datalen);
}

void
main_imsg_compose_captiveportal_fd(int type, pid_t pid, int fd)
{
	if (iev_frontend)
		imsg_compose_event(iev_captiveportal, type, 0, pid, fd, NULL,
		    0);
}

void
imsg_event_add(struct imsgev *iev)
{
	iev->events = EV_READ;
	if (iev->ibuf.w.queued)
		iev->events |= EV_WRITE;

	event_del(&iev->ev);
	event_set(&iev->ev, iev->ibuf.fd, iev->events, iev->handler, iev);
	event_add(&iev->ev, NULL);
}

int
imsg_compose_event(struct imsgev *iev, uint16_t type, uint32_t peerid,
    pid_t pid, int fd, void *data, uint16_t datalen)
{
	int	ret;

	if ((ret = imsg_compose(&iev->ibuf, type, peerid, pid, fd, data,
	    datalen)) != -1)
		imsg_event_add(iev);

	return (ret);
}

static int
main_imsg_send_ipc_sockets(struct imsgbuf *frontend_buf,
    struct imsgbuf *resolver_buf, struct imsgbuf *captiveportal_buf)
{
	int pipe_frontend2resolver[2];
	int pipe_frontend2captiveportal[2];
	int pipe_resolver2captiveportal[2];

	if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK,
	    PF_UNSPEC, pipe_frontend2resolver) == -1)
		return (-1);

	if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK,
	    PF_UNSPEC, pipe_frontend2captiveportal) == -1)
		return (-1);

	if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK,
	    PF_UNSPEC, pipe_resolver2captiveportal) == -1)
		return (-1);

	if (imsg_compose(frontend_buf, IMSG_SOCKET_IPC_RESOLVER, 0, 0,
	    pipe_frontend2resolver[0], NULL, 0) == -1)
		return (-1);
	if (imsg_compose(resolver_buf, IMSG_SOCKET_IPC_FRONTEND, 0, 0,
	    pipe_frontend2resolver[1], NULL, 0) == -1)
		return (-1);

	if (imsg_compose(frontend_buf, IMSG_SOCKET_IPC_CAPTIVEPORTAL, 0, 0,
	    pipe_frontend2captiveportal[0], NULL, 0) == -1)
		return (-1);
	if (imsg_compose(captiveportal_buf, IMSG_SOCKET_IPC_FRONTEND, 0, 0,
	    pipe_frontend2captiveportal[1], NULL, 0) == -1)
		return (-1);

	if (imsg_compose(resolver_buf, IMSG_SOCKET_IPC_CAPTIVEPORTAL, 0, 0,
	    pipe_resolver2captiveportal[0], NULL, 0) == -1)
		return (-1);
	if (imsg_compose(captiveportal_buf, IMSG_SOCKET_IPC_RESOLVER, 0, 0,
	    pipe_resolver2captiveportal[1], NULL, 0) == -1)
		return (-1);

	return (0);
}

int
main_reload(void)
{
	struct unwind_conf *xconf;

	if ((xconf = parse_config(conffile)) == NULL)
		return (-1);

	if (main_imsg_send_config(xconf) == -1)
		return (-1);

	merge_config(main_conf, xconf);

	return (0);
}

int
main_imsg_send_config(struct unwind_conf *xconf)
{
	struct unwind_forwarder	*unwind_forwarder;

	/* Send fixed part of config to children. */
	if (main_sendall(IMSG_RECONF_CONF, xconf, sizeof(*xconf)) == -1)
		return (-1);
	if (xconf->captive_portal_host != NULL) {
		if (main_sendall(IMSG_RECONF_CAPTIVE_PORTAL_HOST,
		    xconf->captive_portal_host,
		    strlen(xconf->captive_portal_host) + 1) == -1)
			return (-1);
	}

	if (xconf->captive_portal_path != NULL) {
		if (main_sendall(IMSG_RECONF_CAPTIVE_PORTAL_PATH,
		    xconf->captive_portal_path,
		    strlen(xconf->captive_portal_path) + 1) == -1)
			return (-1);
	}

	if (xconf->captive_portal_expected_response != NULL) {
		if (main_sendall(IMSG_RECONF_CAPTIVE_PORTAL_EXPECTED_RESPONSE,
		    xconf->captive_portal_expected_response,
		    strlen(xconf->captive_portal_expected_response) + 1)
		    == -1)
			return (-1);
	}

	/* send static forwarders to children */
	SIMPLEQ_FOREACH(unwind_forwarder, &xconf->unwind_forwarder_list, entry) {
		if (main_sendall(IMSG_RECONF_FORWARDER, unwind_forwarder,
		    sizeof(*unwind_forwarder)) == -1)
			return (-1);
	}

	/* send static DoT forwarders to children */
	SIMPLEQ_FOREACH(unwind_forwarder, &xconf->unwind_dot_forwarder_list,
	    entry) {
		if (main_sendall(IMSG_RECONF_DOT_FORWARDER, unwind_forwarder,
		    sizeof(*unwind_forwarder)) == -1)
			return (-1);
	}

	/* Tell children the revised config is now complete. */
	if (main_sendall(IMSG_RECONF_END, NULL, 0) == -1)
		return (-1);

	return (0);
}

int
main_sendall(enum imsg_type type, void *buf, uint16_t len)
{
	if (imsg_compose_event(iev_frontend, type, 0, 0, -1, buf, len) == -1)
		return (-1);
	if (imsg_compose_event(iev_resolver, type, 0, 0, -1, buf, len) == -1)
		return (-1);
	if (imsg_compose_event(iev_captiveportal, type, 0, 0, -1, buf, len) ==
	    -1)
		return (-1);
	return (0);
}

void
merge_config(struct unwind_conf *conf, struct unwind_conf *xconf)
{
	struct unwind_forwarder	*unwind_forwarder;

	/* Remove & discard existing forwarders. */
	while ((unwind_forwarder =
	    SIMPLEQ_FIRST(&conf->unwind_forwarder_list)) != NULL) {
		SIMPLEQ_REMOVE_HEAD(&conf->unwind_forwarder_list, entry);
		free(unwind_forwarder);
	}
	while ((unwind_forwarder =
	    SIMPLEQ_FIRST(&conf->unwind_dot_forwarder_list)) != NULL) {
		SIMPLEQ_REMOVE_HEAD(&conf->unwind_dot_forwarder_list, entry);
		free(unwind_forwarder);
	}

	conf->unwind_options = xconf->unwind_options;

	free(conf->captive_portal_host);
	conf->captive_portal_host = xconf->captive_portal_host;

	free(conf->captive_portal_path);
	conf->captive_portal_path = xconf->captive_portal_path;

	free(conf->captive_portal_expected_response);
	conf->captive_portal_expected_response =
	    xconf->captive_portal_expected_response;

	conf->captive_portal_expected_status =
	    xconf->captive_portal_expected_status;

	conf->captive_portal_auto = xconf->captive_portal_auto;

	/* Add new forwarders. */
	while ((unwind_forwarder =
	    SIMPLEQ_FIRST(&xconf->unwind_forwarder_list)) != NULL) {
		SIMPLEQ_REMOVE_HEAD(&xconf->unwind_forwarder_list, entry);
		SIMPLEQ_INSERT_TAIL(&conf->unwind_forwarder_list,
		    unwind_forwarder, entry);
	}
	while ((unwind_forwarder =
	    SIMPLEQ_FIRST(&xconf->unwind_dot_forwarder_list)) != NULL) {
		SIMPLEQ_REMOVE_HEAD(&xconf->unwind_dot_forwarder_list, entry);
		SIMPLEQ_INSERT_TAIL(&conf->unwind_dot_forwarder_list,
		    unwind_forwarder, entry);
	}

	free(xconf);
}

struct unwind_conf *
config_new_empty(void)
{
	struct unwind_conf	*xconf;

	xconf = calloc(1, sizeof(*xconf));
	if (xconf == NULL)
		fatal(NULL);

	SIMPLEQ_INIT(&xconf->unwind_forwarder_list);
	SIMPLEQ_INIT(&xconf->unwind_dot_forwarder_list);

	if ((xconf->captive_portal_expected_response = strdup("")) == NULL)
		fatal(NULL);
	xconf->captive_portal_expected_status = 200;
	xconf->captive_portal_auto = 1;

	return (xconf);
}

void
config_clear(struct unwind_conf *conf)
{
	struct unwind_conf	*xconf;

	/* Merge current config with an empty config. */
	xconf = config_new_empty();
	merge_config(conf, xconf);

	free(conf);
}

void
open_dhcp_lease(int if_idx)
{
	static	char	 lease_filename[sizeof(_PATH_LEASE_DB) + IF_NAMESIZE] =
			     _PATH_LEASE_DB;

	int		 fd;
	char		*bufp;

	bufp = lease_filename + sizeof(_PATH_LEASE_DB) - 1;
	bufp = if_indextoname(if_idx, bufp);

	if (bufp == NULL) {
		log_debug("cannot find interface %d", if_idx);
		return;
	}

	log_debug("lease file name: %s", lease_filename);

	if ((fd = open(lease_filename, O_RDONLY)) == -1) {
		log_warn(NULL);
		return;
	}


	main_imsg_compose_frontend_fd(IMSG_LEASEFD, 0, fd);
}

void
open_ports(void)
{
	struct addrinfo		 hints, *res0;
	int			 udp4sock = -1, udp6sock = -1, error;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = AI_PASSIVE;

	error = getaddrinfo("127.0.0.1", "domain", &hints, &res0);
	if (!error && res0) {
		if ((udp4sock = socket(res0->ai_family, res0->ai_socktype,
		    res0->ai_protocol)) != -1) {
			if (bind(udp4sock, res0->ai_addr, res0->ai_addrlen)
			    == -1) {
				close(udp4sock);
				udp4sock = -1;
			}
		}
	}
	if (res0)
		freeaddrinfo(res0);

	hints.ai_family = AF_INET6;
	error = getaddrinfo("::1", "domain", &hints, &res0);
	if (!error && res0) {
		if ((udp6sock = socket(res0->ai_family, res0->ai_socktype,
		    res0->ai_protocol)) != -1) {
			if (bind(udp6sock, res0->ai_addr, res0->ai_addrlen)
			    == -1) {
				close(udp6sock);
				udp6sock = -1;
			}
		}
	}
	if (res0)
		freeaddrinfo(res0);

	if (udp4sock == -1 && udp6sock == -1)
		fatal("could not bind to 127.0.0.1 or ::1 on port 53");

	if (udp4sock != -1)
		main_imsg_compose_frontend_fd(IMSG_UDP4SOCK, 0, udp4sock);
	if (udp6sock != -1)
		main_imsg_compose_frontend_fd(IMSG_UDP6SOCK, 0, udp6sock);
}
