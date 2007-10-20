/*	$OpenBSD: ripd.c,v 1.6 2007/10/20 13:26:50 pyr Exp $ */

/*
 * Copyright (c) 2006 Michele Marchetto <mydecay@openbeer.it>
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
#include <sys/socket.h>
#include <sys/queue.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/param.h>
#include <sys/sysctl.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <event.h>
#include <err.h>
#include <errno.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

#include "rip.h"
#include "ripd.h"
#include "ripe.h"
#include "log.h"
#include "control.h"
#include "rde.h"

__dead void		 usage(void);
int			 check_child(pid_t, const char *);
void			 main_sig_handler(int, short, void *);
void			 ripd_shutdown(void);
void			 main_dispatch_ripe(int, short, void *);
void			 main_dispatch_rde(int, short, void *);
void			 ripd_redistribute_default(int);

int			 pipe_parent2ripe[2];
int			 pipe_parent2rde[2];
int			 pipe_ripe2rde[2];

struct ripd_conf	*conf = NULL;
struct imsgbuf		*ibuf_ripe;
struct imsgbuf		*ibuf_rde;

pid_t			 ripe_pid = 0;
pid_t			 rde_pid = 0;

__dead void
usage(void)
{
	extern char *__progname;

	fprintf(stderr, "usage: %s [-dnv] [-f file]\n", __progname);
	exit(1);
}

/* ARGSUSED */
void
main_sig_handler(int sig, short event, void *arg)
{
	/*
	 * signal handler rules don't apply, libevent decouples for us
	 */

	int	die = 0;

	switch (sig) {
	case SIGTERM:
	case SIGINT:
		die = 1;
		/* FALLTHROUGH */
	case SIGCHLD:
		if (check_child(ripe_pid, "rip engine")) {
			ripe_pid = 0;
			die = 1;
		}
		if (check_child(rde_pid, "route decision engine")) {
			rde_pid = 0;
			die = 1;
		}
		if (die)
			ripd_shutdown();
		break;
	case SIGHUP:
		/* reconfigure */
		/* ... */
		break;
	default:
		fatalx("unexpected signal");
		/* NOTREACHED */
	}
}

int
main(int argc, char *argv[])
{
	struct event	 ev_sigint, ev_sigterm, ev_sigchld, ev_sighup;
	int		 mib[4];
	int		 debug = 0;
	int		 ipforwarding;
	int		 ch;
	int		 opts = 0;
	char		*conffile;
	size_t		 len;

	conffile = CONF_FILE;
	ripd_process = PROC_MAIN;

	log_init(1);	/* log to stderr until daemonized */

	while ((ch = getopt(argc, argv, "df:nv")) != -1) {
		switch (ch) {
		case 'd':
			debug = 1;
			break;
		case 'f':
			conffile = optarg;
			break;
		case 'n':
			opts |= RIPD_OPT_NOACTION;
			break;
		case 'v':
			if (opts & RIPD_OPT_VERBOSE)
				opts |= RIPD_OPT_VERBOSE2;
			opts |= RIPD_OPT_VERBOSE;
			break;
		default:
			usage();
			/* NOTREACHED */
		}
	}

	mib[0] = CTL_NET;
	mib[1] = PF_INET;
	mib[2] = IPPROTO_IP;
	mib[3] = IPCTL_FORWARDING;
	len = sizeof(ipforwarding);
	if (sysctl(mib, 4, &ipforwarding, &len, NULL, 0) == -1)
		err(1, "sysctl");

	if (!ipforwarding)
		log_warnx("WARNING: IP forwarding NOT enabled");

	/* fetch interfaces early */
	kif_init();

	/* parse config file */
	if ((conf = parse_config(conffile, opts)) == NULL )
		exit(1);

	if (conf->opts & RIPD_OPT_NOACTION) {
		if (conf->opts & RIPD_OPT_VERBOSE)
			print_config(conf);
		else
			fprintf(stderr, "configuration OK\n");
		exit(0);
	}

	/* check for root privileges */
	if (geteuid())
		errx(1, "need root privileges");

	/* check for ripd user */
	if (getpwnam(RIPD_USER) == NULL)
		errx(1, "unknown user %s", RIPD_USER);

	log_init(debug);

	if (!debug)
		daemon(1, 0);

	log_info("startup");

	if (socketpair(AF_UNIX, SOCK_STREAM, PF_UNSPEC,
	    pipe_parent2ripe) == -1)
		fatal("socketpair");
	if (socketpair(AF_UNIX, SOCK_STREAM, PF_UNSPEC, pipe_parent2rde) == -1)
		fatal("socketpair");
	if (socketpair(AF_UNIX, SOCK_STREAM, PF_UNSPEC, pipe_ripe2rde) == -1)
		fatal("socketpair");
	session_socket_blockmode(pipe_parent2ripe[0], BM_NONBLOCK);
	session_socket_blockmode(pipe_parent2ripe[1], BM_NONBLOCK);
	session_socket_blockmode(pipe_parent2rde[0], BM_NONBLOCK);
	session_socket_blockmode(pipe_parent2rde[1], BM_NONBLOCK);
	session_socket_blockmode(pipe_ripe2rde[0], BM_NONBLOCK);
	session_socket_blockmode(pipe_ripe2rde[1], BM_NONBLOCK);

	/* start children */
	rde_pid = rde(conf, pipe_parent2rde, pipe_ripe2rde, pipe_parent2ripe);
	ripe_pid = ripe(conf, pipe_parent2ripe, pipe_ripe2rde, pipe_parent2rde);

	/* show who we are */
	setproctitle("parent");

	event_init();

	/* setup signal handler */
	signal_set(&ev_sigint, SIGINT, main_sig_handler, NULL);
	signal_set(&ev_sigterm, SIGTERM, main_sig_handler, NULL);
	signal_set(&ev_sigchld, SIGINT, main_sig_handler, NULL);
	signal_set(&ev_sighup, SIGHUP, main_sig_handler, NULL);
	signal_add(&ev_sigint, NULL);
	signal_add(&ev_sigterm, NULL);
	signal_add(&ev_sigchld, NULL);
	signal_add(&ev_sighup, NULL);
	signal(SIGPIPE, SIG_IGN);

	/* setup pipes to children */
	close(pipe_parent2ripe[1]);
	close(pipe_parent2rde[1]);
	close(pipe_ripe2rde[0]);
	close(pipe_ripe2rde[1]);

	if ((ibuf_ripe = malloc(sizeof(struct imsgbuf))) == NULL ||
	    (ibuf_rde = malloc(sizeof(struct imsgbuf))) == NULL)
		fatal(NULL);
	imsg_init(ibuf_ripe, pipe_parent2ripe[0], main_dispatch_ripe);
	imsg_init(ibuf_rde, pipe_parent2rde[0], main_dispatch_rde);

	/* setup event handler */
	ibuf_ripe->events = EV_READ;
	event_set(&ibuf_ripe->ev, ibuf_ripe->fd, ibuf_ripe->events,
	    ibuf_ripe->handler, ibuf_ripe);
	event_add(&ibuf_ripe->ev, NULL);

	ibuf_rde->events = EV_READ;
	event_set(&ibuf_rde->ev, ibuf_rde->fd, ibuf_rde->events,
	    ibuf_rde->handler, ibuf_rde);
	event_add(&ibuf_rde->ev, NULL);

	if (kr_init(!(conf->flags & RIPD_FLAG_NO_FIB_UPDATE)) == -1)
		fatalx("kr_init failed");

	/* redistribute default */
	ripd_redistribute_default(IMSG_NETWORK_ADD);

	event_dispatch();

	ripd_shutdown();
	/* NOTREACHED */
	return (0);
}

void
ripd_shutdown(void)
{
	struct iface	*i;
	pid_t		 pid;

	if (ripe_pid)
		kill(ripe_pid, SIGTERM);

	if (rde_pid)
		kill(rde_pid, SIGTERM);

	while ((i = LIST_FIRST(&conf->iface_list)) != NULL) {
		LIST_REMOVE(i, entry);
		if_del(i);
	}

	control_cleanup();
	kr_shutdown();

	do {
		if ((pid = wait(NULL)) == -1 &&
		    errno != EINTR && errno != ECHILD)
			fatal("wait");
	} while (pid != -1 || (pid == -1 && errno == EINTR));

	msgbuf_clear(&ibuf_ripe->w);
	free(ibuf_ripe);
	msgbuf_clear(&ibuf_rde->w);
	free(ibuf_rde);
	free(conf);

	log_info("terminating");
	exit(0);
}

int
check_child(pid_t pid, const char *pname)
{
	int	status;

	if (waitpid(pid, &status, WNOHANG) > 0) {
		if (WIFEXITED(status)) {
			log_warnx("lost child: %s exited", pname);
			return (1);
		}
		if (WIFSIGNALED(status)) {
			log_warnx("lost child: %s terminated; signal %d",
			    pname, WTERMSIG(status));
			return (1);
		}
	}

	return (0);
}

/* imsg handling */
/* ARGSUSED */
void
main_dispatch_ripe(int fd, short event, void *bula)
{
	struct imsgbuf	*ibuf = bula;
	struct imsg	 imsg;
	ssize_t		 n;

	switch (event) {
	case EV_READ:
		if ((n = imsg_read(ibuf)) == -1)
			fatal("imsg_read error");
		if (n == 0)	/* connection closed */
			fatalx("pipe closed");
		break;
	case EV_WRITE:
		if (msgbuf_write(&ibuf->w) == -1)
			fatal("msgbuf_write");
		imsg_event_add(ibuf);
		return;
	default:
		fatalx("unknown event");
	}

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatal("imsg_get");

		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_CTL_RELOAD:
			/* XXX reconfig */
			break;
		case IMSG_CTL_FIB_COUPLE:
			kr_fib_couple();
			break;
		case IMSG_CTL_FIB_DECOUPLE:
			kr_fib_decouple();
			break;
		case IMSG_CTL_KROUTE:
		case IMSG_CTL_KROUTE_ADDR:
			kr_show_route(&imsg);
			break;
		case IMSG_CTL_IFINFO:
			if (imsg.hdr.len == IMSG_HEADER_SIZE)
				kr_ifinfo(NULL, imsg.hdr.pid);
			else if (imsg.hdr.len == IMSG_HEADER_SIZE + IFNAMSIZ)
				kr_ifinfo(imsg.data, imsg.hdr.pid);
			else
				log_warnx("IFINFO request with wrong len");
			break;
		default:
			log_debug("main_dispatch_ripe: error handling imsg %d",
			    imsg.hdr.type);
			break;
		}
		imsg_free(&imsg);
	}
	imsg_event_add(ibuf);
}

/* ARGSUSED */
void
main_dispatch_rde(int fd, short event, void *bula)
{
	struct imsgbuf	*ibuf = bula;
	struct imsg	 imsg;
	ssize_t		 n;

	switch (event) {
	case EV_READ:
		if ((n = imsg_read(ibuf)) == -1)
			fatal("imsg_read error");
		if (n == 0)	/* connection closed */
			fatalx("pipe closed");
		break;
	case EV_WRITE:
		if (msgbuf_write(&ibuf->w) == -1)
			fatal("msgbuf_write");
		imsg_event_add(ibuf);
		return;
	default:
		fatalx("unknown event");
	}

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatal("imsg_get");

		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_KROUTE_CHANGE:
			if (kr_change(imsg.data))
				log_warn("main_dispatch_rde: error changing "
				    "route");
			break;
		case IMSG_KROUTE_DELETE:
			if (kr_delete(imsg.data))
				log_warn("main_dispatch_rde: error deleting "
				    "route");
			break;
		default:
			log_debug("main_dispatch_rde: error handling imsg %d",
			    imsg.hdr.type);
			break;
		}
		imsg_free(&imsg);
	}
	imsg_event_add(ibuf);
}

void
main_imsg_compose_ripe(int type, pid_t pid, void *data, u_int16_t datalen)
{
	imsg_compose(ibuf_ripe, type, 0, pid, data, datalen);
}

void
main_imsg_compose_rde(int type, pid_t pid, void *data, u_int16_t datalen)
{
	imsg_compose(ibuf_rde, type, 0, pid, data, datalen);
}

int
rip_redistribute(struct kroute *kr)
{
	struct redistribute	*r;

	if (kr->flags & F_RIPD_INSERTED)
		return (1);

	/* only allow 0.0.0.0/0 via REDISTRIBUTE_DEFAULT */
	if (kr->prefix.s_addr == INADDR_ANY && kr->netmask.s_addr == INADDR_ANY)
		return (0);

	SIMPLEQ_FOREACH(r, &conf->redist_list, entry) {
		switch (r->type & ~REDIST_NO) {
		case REDIST_LABEL:
			if (kr->rtlabel == r->label)
				return (r->type & REDIST_NO ? 0 : 1);
			break;
		case REDIST_STATIC:
			/*
			 * Dynamic routes are not redistributable. Placed here
			 * so that link local addresses can be redistributed
			 * via a rtlabel.
			 */
			if (kr->flags & F_DYNAMIC)
				continue;
			if (kr->flags & F_STATIC)
				return (r->type & REDIST_NO ? 0 : 1);
			break;
		case REDIST_CONNECTED:
			if (kr->flags & F_DYNAMIC)
				continue;
			if (kr->flags & F_CONNECTED)
				return (r->type & REDIST_NO ? 0 : 1);
			break;
		case REDIST_ADDR:
			if (kr->flags & F_DYNAMIC)
				continue;
			if ((kr->prefix.s_addr & r->mask.s_addr) ==
			    (r->addr.s_addr & r->mask.s_addr) &&
			    (kr->netmask.s_addr & r->mask.s_addr) ==
			    r->mask.s_addr)
				return (r->type & REDIST_NO? 0 : 1);
			break;
		}
	}

	return (0);
}

void
ripd_redistribute_default(int type)
{
	struct kroute	kr;

	if (!(conf->redistribute & REDISTRIBUTE_DEFAULT))
		return;

	bzero(&kr, sizeof(kr));
	main_imsg_compose_rde(type, 0, &kr, sizeof(struct kroute));
}

/* this needs to be added here so that ripctl can be used without libevent */
void
imsg_event_add(struct imsgbuf *ibuf)
{
	ibuf->events = EV_READ;
	if (ibuf->w.queued)
		ibuf->events |= EV_WRITE;

	event_del(&ibuf->ev);
	event_set(&ibuf->ev, ibuf->fd, ibuf->events, ibuf->handler, ibuf);
	event_add(&ibuf->ev, NULL);
}
