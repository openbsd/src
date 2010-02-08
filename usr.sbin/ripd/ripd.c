/*	$OpenBSD: ripd.c,v 1.20 2010/02/08 00:26:51 guenther Exp $ */

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

int			 pipe_parent2ripe[2];
int			 pipe_parent2rde[2];
int			 pipe_ripe2rde[2];

struct ripd_conf	*conf = NULL;
struct imsgev		*iev_ripe;
struct imsgev		*iev_rde;

pid_t			 ripe_pid = 0;
pid_t			 rde_pid = 0;

__dead void
usage(void)
{
	extern char *__progname;

	fprintf(stderr, "usage: %s [-dnv] [-D macro=value] [-f file]\n",
	    __progname);
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

	while ((ch = getopt(argc, argv, "cdD:f:nv")) != -1) {
		switch (ch) {
		case 'c':
			opts |= RIPD_OPT_FORCE_DEMOTE;
			break;
		case 'd':
			debug = 1;
			break;
		case 'D':
			if (cmdline_symset(optarg) < 0)
				log_warnx("could not parse macro definition %s",
				    optarg);
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

	argc -= optind;
	argv += optind;
	if (argc > 0)
		usage();

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
	signal_set(&ev_sigchld, SIGCHLD, main_sig_handler, NULL);
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

	if ((iev_ripe = malloc(sizeof(struct imsgev))) == NULL ||
	    (iev_rde = malloc(sizeof(struct imsgev))) == NULL)
		fatal(NULL);
	imsg_init(&iev_ripe->ibuf, pipe_parent2ripe[0]);
	iev_ripe->handler = main_dispatch_ripe;
	imsg_init(&iev_rde->ibuf, pipe_parent2rde[0]);
	iev_rde->handler = main_dispatch_rde;

	/* setup event handler */
	iev_ripe->events = EV_READ;
	event_set(&iev_ripe->ev, iev_ripe->ibuf.fd, iev_ripe->events,
	    iev_ripe->handler, iev_ripe);
	event_add(&iev_ripe->ev, NULL);

	iev_rde->events = EV_READ;
	event_set(&iev_rde->ev, iev_rde->ibuf.fd, iev_rde->events,
	    iev_rde->handler, iev_rde);
	event_add(&iev_rde->ev, NULL);

	if (kr_init(!(conf->flags & RIPD_FLAG_NO_FIB_UPDATE),
	    conf->rdomain) == -1)
		fatalx("kr_init failed");

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

	msgbuf_clear(&iev_ripe->ibuf.w);
	free(iev_ripe);
	msgbuf_clear(&iev_rde->ibuf.w);
	free(iev_rde);
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
	struct imsgev		*iev = bula;
	struct imsgbuf		*ibuf = &iev->ibuf;
	struct imsg		 imsg;
	struct demote_msg	 dmsg;
	ssize_t			 n;
	int			 shut = 0, verbose;

	if (event & EV_READ) {
		if ((n = imsg_read(ibuf)) == -1)
			fatal("imsg_read error");
		if (n == 0)	/* connection closed */
			shut = 1;
	}
	if (event & EV_WRITE) {
		if (msgbuf_write(&ibuf->w) == -1)
			fatal("msgbuf_write");
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
		case IMSG_DEMOTE:
			if (imsg.hdr.len - IMSG_HEADER_SIZE != sizeof(dmsg))
				fatalx("invalid size of OE request");
			memcpy(&dmsg, imsg.data, sizeof(dmsg));
			carp_demote_set(dmsg.demote_group, dmsg.level);
			break;
		case IMSG_CTL_LOG_VERBOSE:
			/* already checked by ripe */
			memcpy(&verbose, imsg.data, sizeof(verbose));
			log_verbose(verbose);
			break;
		default:
			log_debug("main_dispatch_ripe: error handling imsg %d",
			    imsg.hdr.type);
			break;
		}
		imsg_free(&imsg);
	}
	if (!shut)
		imsg_event_add(iev);
	else {
		/* this pipe is dead, so remove the event handler */  
		event_del(&iev->ev);
		event_loopexit(NULL);
	}
}

/* ARGSUSED */
void
main_dispatch_rde(int fd, short event, void *bula)
{
	struct imsgev	*iev = bula;
	struct imsgbuf	*ibuf = &iev->ibuf;
	struct imsg	 imsg;
	ssize_t		 n;
	int		 shut = 0;

	if (event & EV_READ) {
		if ((n = imsg_read(ibuf)) == -1)
			fatal("imsg_read error");
		if (n == 0)	/* connection closed */
			shut = 1;
	}
	if (event & EV_WRITE) {
		if (msgbuf_write(&ibuf->w) == -1)
			fatal("msgbuf_write");
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
	if (!shut)
		imsg_event_add(iev);
	else {
		/* this pipe is dead, so remove the event handler */  
		event_del(&iev->ev);
		event_loopexit(NULL);
	}
}

void
main_imsg_compose_ripe(int type, pid_t pid, void *data, u_int16_t datalen)
{
	imsg_compose_event(iev_ripe, type, 0, pid, -1, data, datalen);
}

void
main_imsg_compose_rde(int type, pid_t pid, void *data, u_int16_t datalen)
{
	imsg_compose_event(iev_rde, type, 0, pid, -1, data, datalen);
}

int
rip_redistribute(struct kroute *kr)
{
	struct redistribute	*r;
	u_int8_t		 is_default = 0;

	if (kr->flags & F_RIPD_INSERTED)
		return (1);

	/* only allow 0.0.0.0/0 via REDIST_DEFAULT */
	if (kr->prefix.s_addr == INADDR_ANY && kr->netmask.s_addr == INADDR_ANY)
		is_default = 1;

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
			if (is_default)
				continue;
			if (kr->flags & F_DYNAMIC)
				continue;
			if (kr->flags & F_STATIC)
				return (r->type & REDIST_NO ? 0 : 1);
			break;
		case REDIST_CONNECTED:
			if (is_default)
				continue;
			if (kr->flags & F_DYNAMIC)
				continue;
			if (kr->flags & F_CONNECTED)
				return (r->type & REDIST_NO ? 0 : 1);
			break;
		case REDIST_ADDR:
			if (kr->flags & F_DYNAMIC)
				continue;

			if (r->addr.s_addr == INADDR_ANY &&
			    r->mask.s_addr == INADDR_ANY) {
				if (is_default)
					return (r->type & REDIST_NO? 0 : 1);
				else
					return (0);
			}

			if ((kr->prefix.s_addr & r->mask.s_addr) ==
			    (r->addr.s_addr & r->mask.s_addr) &&
			    (kr->netmask.s_addr & r->mask.s_addr) ==
			    r->mask.s_addr)
				return (r->type & REDIST_NO? 0 : 1);
			break;
		case REDIST_DEFAULT:
			if (is_default)
				return (r->type & REDIST_NO? 0 : 1);
			break;
		}
	}

	return (0);
}

void
imsg_event_add(struct imsgev *iev)
{
	if (iev->handler == NULL) {
		imsg_flush(&iev->ibuf);
		return;
	}

	iev->events = EV_READ;
	if (iev->ibuf.w.queued)
		iev->events |= EV_WRITE;

	event_del(&iev->ev);
	event_set(&iev->ev, iev->ibuf.fd, iev->events, iev->handler, iev);
	event_add(&iev->ev, NULL);
}

int
imsg_compose_event(struct imsgev *iev, u_int16_t type,
    u_int32_t peerid, pid_t pid, int fd, void *data, u_int16_t datalen)
{
	int	ret;

	if ((ret = imsg_compose(&iev->ibuf, type, peerid,
	    pid, fd, data, datalen)) != -1)
		imsg_event_add(iev);
	return (ret);
}
