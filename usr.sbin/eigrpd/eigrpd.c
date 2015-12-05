/*	$OpenBSD: eigrpd.c,v 1.4 2015/12/05 15:49:01 claudio Exp $ */

/*
 * Copyright (c) 2015 Renato Westphal <renato@openbsd.org>
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

#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/sysctl.h>
#include <arpa/inet.h>
#include <err.h>
#include <errno.h>
#include <pwd.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

#include "eigrpd.h"
#include "eigrp.h"
#include "eigrpe.h"
#include "control.h"
#include "log.h"
#include "rde.h"

void		main_sig_handler(int, short, void *);
__dead void	usage(void);
void		eigrpd_shutdown(void);
int		check_child(pid_t, const char *);

void	main_dispatch_eigrpe(int, short, void *);
void	main_dispatch_rde(int, short, void *);

int	eigrp_reload(void);
int	eigrp_sendboth(enum imsg_type, void *, uint16_t);
void	merge_instances(struct eigrpd_conf *, struct eigrp *, struct eigrp *);

int	pipe_parent2eigrpe[2];
int	pipe_parent2rde[2];
int	pipe_eigrpe2rde[2];

struct eigrpd_conf	*eigrpd_conf = NULL;
struct imsgev		*iev_eigrpe;
struct imsgev		*iev_rde;
char			*conffile;

pid_t			 eigrpe_pid = 0;
pid_t			 rde_pid = 0;

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
		if (check_child(eigrpe_pid, "eigrp engine")) {
			eigrpe_pid = 0;
			die = 1;
		}
		if (check_child(rde_pid, "route decision engine")) {
			rde_pid = 0;
			die = 1;
		}
		if (die)
			eigrpd_shutdown();
		break;
	case SIGHUP:
		if (eigrp_reload() == -1)
			log_warnx("configuration reload failed");
		else
			log_debug("configuration reloaded");
		break;
	default:
		fatalx("unexpected signal");
		/* NOTREACHED */
	}
}

__dead void
usage(void)
{
	extern char *__progname;

	fprintf(stderr, "usage: %s [-dnv] [-D macro=value]"
	    " [-f file] [-s socket]\n",
	    __progname);
	exit(1);
}

int
main(int argc, char *argv[])
{
	struct event		 ev_sigint, ev_sigterm, ev_sigchld, ev_sighup;
	int			 ch, opts = 0;
	int			 debug = 0;
	int			 ipforwarding;
	int			 mib[4];
	size_t			 len;
	char			*sockname;

	conffile = CONF_FILE;
	eigrpd_process = PROC_MAIN;
	sockname = EIGRPD_SOCKET;

	log_init(1);	/* log to stderr until daemonized */
	log_verbose(1);

	while ((ch = getopt(argc, argv, "dD:f:ns:v")) != -1) {
		switch (ch) {
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
			opts |= EIGRPD_OPT_NOACTION;
			break;
		case 's':
			sockname = optarg;
			break;
		case 'v':
			if (opts & EIGRPD_OPT_VERBOSE)
				opts |= EIGRPD_OPT_VERBOSE2;
			opts |= EIGRPD_OPT_VERBOSE;
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

	if (ipforwarding != 1)
		log_warnx("WARNING: IP forwarding NOT enabled");

	/* fetch interfaces early */
	kif_init();

	/* parse config file */
	if ((eigrpd_conf = parse_config(conffile, opts)) == NULL) {
		kif_clear();
		exit(1);
	}
	eigrpd_conf->csock = sockname;

	if (eigrpd_conf->opts & EIGRPD_OPT_NOACTION) {
		if (eigrpd_conf->opts & EIGRPD_OPT_VERBOSE)
			print_config(eigrpd_conf);
		else
			fprintf(stderr, "configuration OK\n");
		kif_clear();
		exit(0);
	}

	/* check for root privileges  */
	if (geteuid())
		errx(1, "need root privileges");

	/* check for eigrpd user */
	if (getpwnam(EIGRPD_USER) == NULL)
		errx(1, "unknown user %s", EIGRPD_USER);

	log_init(debug);
	log_verbose(eigrpd_conf->opts & EIGRPD_OPT_VERBOSE);

	if (!debug)
		daemon(1, 0);

	log_info("startup");

	if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK,
	    PF_UNSPEC, pipe_parent2eigrpe) == -1)
		fatal("socketpair");
	if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK,
	    PF_UNSPEC, pipe_parent2rde) == -1)
		fatal("socketpair");
	if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK,
	    PF_UNSPEC, pipe_eigrpe2rde) == -1)
		fatal("socketpair");

	/* start children */
	rde_pid = rde(eigrpd_conf, pipe_parent2rde, pipe_eigrpe2rde,
	    pipe_parent2eigrpe);
	eigrpe_pid = eigrpe(eigrpd_conf, pipe_parent2eigrpe, pipe_eigrpe2rde,
	    pipe_parent2rde);

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
	close(pipe_parent2eigrpe[1]);
	close(pipe_parent2rde[1]);
	close(pipe_eigrpe2rde[0]);
	close(pipe_eigrpe2rde[1]);

	if ((iev_eigrpe = malloc(sizeof(struct imsgev))) == NULL ||
	    (iev_rde = malloc(sizeof(struct imsgev))) == NULL)
		fatal(NULL);
	imsg_init(&iev_eigrpe->ibuf, pipe_parent2eigrpe[0]);
	iev_eigrpe->handler = main_dispatch_eigrpe;
	imsg_init(&iev_rde->ibuf, pipe_parent2rde[0]);
	iev_rde->handler = main_dispatch_rde;

	/* setup event handler */
	iev_eigrpe->events = EV_READ;
	event_set(&iev_eigrpe->ev, iev_eigrpe->ibuf.fd, iev_eigrpe->events,
	    iev_eigrpe->handler, iev_eigrpe);
	event_add(&iev_eigrpe->ev, NULL);

	iev_rde->events = EV_READ;
	event_set(&iev_rde->ev, iev_rde->ibuf.fd, iev_rde->events,
	    iev_rde->handler, iev_rde);
	event_add(&iev_rde->ev, NULL);

	/* notify eigrpe about existing interfaces and addresses */
	kif_redistribute();

	if (kr_init(!(eigrpd_conf->flags & EIGRPD_FLAG_NO_FIB_UPDATE),
	    eigrpd_conf->rdomain) == -1)
		fatalx("kr_init failed");

	if (pledge("stdio proc", NULL) == -1)
		fatal("pledge");

	event_dispatch();

	eigrpd_shutdown();
	/* NOTREACHED */
	return (0);
}

void
eigrpd_shutdown(void)
{
	pid_t		 	 pid;

	if (eigrpe_pid)
		kill(eigrpe_pid, SIGTERM);

	if (rde_pid)
		kill(rde_pid, SIGTERM);

	kr_shutdown();

	do {
		if ((pid = wait(NULL)) == -1 &&
		    errno != EINTR && errno != ECHILD)
			fatal("wait");
	} while (pid != -1 || (pid == -1 && errno == EINTR));

	config_clear(eigrpd_conf);

	msgbuf_clear(&iev_eigrpe->ibuf.w);
	free(iev_eigrpe);
	msgbuf_clear(&iev_rde->ibuf.w);
	free(iev_rde);

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
main_dispatch_eigrpe(int fd, short event, void *bula)
{
	struct imsgev		*iev = bula;
	struct imsgbuf		*ibuf;
	struct imsg		 imsg;
	ssize_t			 n;
	int			 shut = 0, verbose;

	ibuf = &iev->ibuf;

	if (event & EV_READ) {
		if ((n = imsg_read(ibuf)) == -1 && errno != EAGAIN)
			fatal("imsg_read error");
		if (n == 0)	/* connection closed */
			shut = 1;
	}
	if (event & EV_WRITE) {
		if ((n = msgbuf_write(&ibuf->w)) == -1 && errno != EAGAIN)
			fatal("msgbuf_write");
		if (n == 0)	/* connection closed */
			shut = 1;
	}

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatal("imsg_get");

		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_CTL_RELOAD:
			if (eigrp_reload() == -1)
				log_warnx("configuration reload failed");
			else
				log_debug("configuration reloaded");
			break;
		case IMSG_CTL_FIB_COUPLE:
			kr_fib_couple();
			break;
		case IMSG_CTL_FIB_DECOUPLE:
			kr_fib_decouple();
			break;
		case IMSG_CTL_KROUTE:
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
		case IMSG_CTL_LOG_VERBOSE:
			/* already checked by eigrpe */
			memcpy(&verbose, imsg.data, sizeof(verbose));
			log_verbose(verbose);
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
	struct imsgbuf  *ibuf;
	struct imsg	 imsg;
	ssize_t		 n;
	int		 shut = 0;

	ibuf = &iev->ibuf;

	if (event & EV_READ) {
		if ((n = imsg_read(ibuf)) == -1 && errno != EAGAIN)
			fatal("imsg_read error");
		if (n == 0)	/* connection closed */
			shut = 1;
	}
	if (event & EV_WRITE) {
		if ((n = msgbuf_write(&ibuf->w)) == -1 && errno != EAGAIN)
			fatal("msgbuf_write");
		if (n == 0)	/* connection closed */
			shut = 1;
	}

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatal("imsg_get");

		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_KROUTE_CHANGE:
			if (imsg.hdr.len - IMSG_HEADER_SIZE !=
			    sizeof(struct kroute))
				fatalx("invalid size of IMSG_KROUTE_CHANGE");
			if (kr_change(imsg.data))
				log_warnx("%s: error changing route", __func__);
			break;
		case IMSG_KROUTE_DELETE:
			if (imsg.hdr.len - IMSG_HEADER_SIZE !=
			    sizeof(struct kroute))
				fatalx("invalid size of IMSG_KROUTE_DELETE");
			if (kr_delete(imsg.data))
				log_warnx("%s: error deleting route", __func__);
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
		/* this pipe is dead, so remove the event handler */
		event_del(&iev->ev);
		event_loopexit(NULL);
	}
}

void
main_imsg_compose_eigrpe(int type, pid_t pid, void *data, uint16_t datalen)
{
	if (iev_eigrpe == NULL)
		return;
	imsg_compose_event(iev_eigrpe, type, 0, pid, -1, data, datalen);
}

void
main_imsg_compose_rde(int type, pid_t pid, void *data, uint16_t datalen)
{
	imsg_compose_event(iev_rde, type, 0, pid, -1, data, datalen);
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

	if ((ret = imsg_compose(&iev->ibuf, type, peerid,
	    pid, fd, data, datalen)) != -1)
		imsg_event_add(iev);
	return (ret);
}

uint32_t
eigrp_router_id(struct eigrpd_conf *xconf)
{
	return (xconf->rtr_id.s_addr);
}

struct eigrp *
eigrp_find(struct eigrpd_conf *xconf, int af, uint16_t as)
{
	struct eigrp	*eigrp;

	TAILQ_FOREACH(eigrp, &xconf->instances, entry)
		if (eigrp->af == af && eigrp->as == as)
			return (eigrp);

	return (NULL);
}

int
eigrp_reload(void)
{
	struct eigrp		*eigrp;
	struct eigrp_iface	*ei;
	struct eigrpd_conf	*xconf;

	if ((xconf = parse_config(conffile, eigrpd_conf->opts)) == NULL)
		return (-1);

	if (eigrp_sendboth(IMSG_RECONF_CONF, xconf, sizeof(*xconf)) == -1)
		return (-1);

	TAILQ_FOREACH(eigrp, &xconf->instances, entry) {
		if (eigrp_sendboth(IMSG_RECONF_INSTANCE, eigrp,
		    sizeof(*eigrp)) == -1)
			return (-1);

		TAILQ_FOREACH(ei, &eigrp->ei_list, e_entry) {
			if (eigrp_sendboth(IMSG_RECONF_IFACE, ei->iface,
			    sizeof(struct iface)) == -1)
				return (-1);

			if (eigrp_sendboth(IMSG_RECONF_EIGRP_IFACE, ei,
			    sizeof(*ei)) == -1)
				return (-1);
		}
	}

	if (eigrp_sendboth(IMSG_RECONF_END, NULL, 0) == -1)
		return (-1);

	merge_config(eigrpd_conf, xconf);

	return (0);
}

int
eigrp_sendboth(enum imsg_type type, void *buf, uint16_t len)
{
	if (imsg_compose_event(iev_eigrpe, type, 0, 0, -1, buf, len) == -1)
		return (-1);
	if (imsg_compose_event(iev_rde, type, 0, 0, -1, buf, len) == -1)
		return (-1);
	return (0);
}

void
merge_config(struct eigrpd_conf *conf, struct eigrpd_conf *xconf)
{
	struct eigrp		*eigrp, *etmp, *xe;

	/* change of rtr_id needs a restart */
	conf->flags = xconf->flags;
	conf->rdomain= xconf->rdomain;
	conf->fib_priority_internal = xconf->fib_priority_internal;
	conf->fib_priority_external = xconf->fib_priority_external;
	conf->fib_priority_summary = xconf->fib_priority_summary;

	/* merge instances */
	TAILQ_FOREACH_SAFE(eigrp, &conf->instances, entry, etmp) {
		/* find deleted instances */
		if ((xe = eigrp_find(xconf, eigrp->af, eigrp->as)) == NULL) {
			TAILQ_REMOVE(&conf->instances, eigrp, entry);

			switch (eigrpd_process) {
			case PROC_RDE_ENGINE:
				rde_instance_del(eigrp);
				break;
			case PROC_EIGRP_ENGINE:
				eigrpe_instance_del(eigrp);
				break;
			case PROC_MAIN:
				free(eigrp);
				break;
			}
		}
	}
	TAILQ_FOREACH_SAFE(xe, &xconf->instances, entry, etmp) {
		/* find new instances */
		if ((eigrp = eigrp_find(conf, xe->af, xe->as)) == NULL) {
			TAILQ_REMOVE(&xconf->instances, xe, entry);
			TAILQ_INSERT_TAIL(&conf->instances, xe, entry);

			switch (eigrpd_process) {
			case PROC_RDE_ENGINE:
				rde_instance_init(xe);
				break;
			case PROC_EIGRP_ENGINE:
				eigrpe_instance_init(xe);
				break;
			case PROC_MAIN:
				break;
			}
			continue;
		}

		/* update existing instances */
		merge_instances(conf, eigrp, xe);
	}
	/* resend addresses to activate new interfaces */
	if (eigrpd_process == PROC_MAIN)
		kif_redistribute();

	free(xconf);
}

void
merge_instances(struct eigrpd_conf *xconf, struct eigrp *eigrp, struct eigrp *xe)
{
	/* TODO */
}

void
config_clear(struct eigrpd_conf *conf)
{
	struct eigrpd_conf	*xconf;

	/* merge current config with an empty config */
	xconf = malloc(sizeof(*xconf));
	memcpy(xconf, conf, sizeof(*xconf));
	TAILQ_INIT(&xconf->instances);
	merge_config(conf, xconf);

	free(conf);
}
