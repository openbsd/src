/*	$OpenBSD: ldpd.c,v 1.22 2015/02/09 11:54:24 claudio Exp $ */

/*
 * Copyright (c) 2005 Claudio Jeker <claudio@openbsd.org>
 * Copyright (c) 2004, 2008 Esben Norby <norby@openbsd.org>
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

#include <netinet/in.h>
#include <arpa/inet.h>
#include <netmpls/mpls.h>

#include <event.h>
#include <err.h>
#include <errno.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

#include "ldpd.h"
#include "ldp.h"
#include "ldpe.h"
#include "control.h"
#include "log.h"
#include "lde.h"

void		main_sig_handler(int, short, void *);
__dead void	usage(void);
void		ldpd_shutdown(void);
int		check_child(pid_t, const char *);

void	main_dispatch_ldpe(int, short, void *);
void	main_dispatch_lde(int, short, void *);

/*int	ldp_reload(void); */
int	ldp_sendboth(enum imsg_type, void *, u_int16_t);
/*int	merge_interfaces(struct lspace *, struct lspace *); */
/*struct iface *iface_lookup(struct lspace *, struct iface *); */

int	pipe_parent2ldpe[2];
int	pipe_parent2lde[2];
int	pipe_ldpe2lde[2];

struct ldpd_conf	*ldpd_conf = NULL;
struct imsgev		*iev_ldpe;
struct imsgev		*iev_lde;
char			*conffile;

pid_t			 ldpe_pid = 0;
pid_t			 lde_pid = 0;

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
		if (check_child(ldpe_pid, "ldp engine")) {
			ldpe_pid = 0;
			die = 1;
		}
		if (check_child(lde_pid, "label decision engine")) {
			lde_pid = 0;
			die = 1;
		}
		if (die)
			ldpd_shutdown();
		break;
	case SIGHUP:
/*
		if (ldp_reload() == -1)
			log_warnx("configuration reload failed");
		else
			log_debug("configuration reloaded");
*/
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

	fprintf(stderr, "usage: %s [-dnv] [-D macro=value] [-f file]\n",
	    __progname);
	exit(1);
}

int
main(int argc, char *argv[])
{
	struct event		 ev_sigint, ev_sigterm, ev_sigchld, ev_sighup;
	int			 ch, opts = 0;
	int			 debug = 0;

	conffile = CONF_FILE;
	ldpd_process = PROC_MAIN;

	log_init(1);	/* log to stderr until daemonized */
	log_verbose(1);

	while ((ch = getopt(argc, argv, "dD:f:nv")) != -1) {
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
			opts |= LDPD_OPT_NOACTION;
			break;
		case 'v':
			if (opts & LDPD_OPT_VERBOSE)
				opts |= LDPD_OPT_VERBOSE2;
			opts |= LDPD_OPT_VERBOSE;
			break;
		default:
			usage();
			/* NOTREACHED */
		}
	}

	/* fetch interfaces early */
	kif_init();

	/* parse config file */
	if ((ldpd_conf = parse_config(conffile, opts)) == NULL )
		exit(1);

	if (ldpd_conf->opts & LDPD_OPT_NOACTION) {
		if (ldpd_conf->opts & LDPD_OPT_VERBOSE)
			print_config(ldpd_conf);
		else
			fprintf(stderr, "configuration OK\n");
		exit(0);
	}

	/* check for root privileges  */
	if (geteuid())
		errx(1, "need root privileges");

	/* check for ldpd user */
	if (getpwnam(LDPD_USER) == NULL)
		errx(1, "unknown user %s", LDPD_USER);

	log_init(debug);
	log_verbose(opts & (LDPD_OPT_VERBOSE | LDPD_OPT_VERBOSE2));

	if (!debug)
		daemon(1, 0);

	log_info("startup");

	if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC,
	    PF_UNSPEC, pipe_parent2ldpe) == -1)
		fatal("socketpair");
	if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC,
	    PF_UNSPEC, pipe_parent2lde) == -1)
		fatal("socketpair");
	if (socketpair(AF_UNIX, SOCK_STREAM |SOCK_NONBLOCK | SOCK_CLOEXEC,
	    PF_UNSPEC, pipe_ldpe2lde) == -1)
		fatal("socketpair");

	/* start children */
	lde_pid = lde(ldpd_conf, pipe_parent2lde, pipe_ldpe2lde,
	    pipe_parent2ldpe);
	ldpe_pid = ldpe(ldpd_conf, pipe_parent2ldpe, pipe_ldpe2lde,
	    pipe_parent2lde);

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
	close(pipe_parent2ldpe[1]);
	close(pipe_parent2lde[1]);
	close(pipe_ldpe2lde[0]);
	close(pipe_ldpe2lde[1]);

	if ((iev_ldpe = malloc(sizeof(struct imsgev))) == NULL ||
	    (iev_lde = malloc(sizeof(struct imsgev))) == NULL)
		fatal(NULL);
	imsg_init(&iev_ldpe->ibuf, pipe_parent2ldpe[0]);
	iev_ldpe->handler = main_dispatch_ldpe;
	imsg_init(&iev_lde->ibuf, pipe_parent2lde[0]);
	iev_lde->handler = main_dispatch_lde;

	/* setup event handler */
	iev_ldpe->events = EV_READ;
	event_set(&iev_ldpe->ev, iev_ldpe->ibuf.fd, iev_ldpe->events,
	    iev_ldpe->handler, iev_ldpe);
	event_add(&iev_ldpe->ev, NULL);

	iev_lde->events = EV_READ;
	event_set(&iev_lde->ev, iev_lde->ibuf.fd, iev_lde->events,
	    iev_lde->handler, iev_lde);
	event_add(&iev_lde->ev, NULL);

	/* notify ldpe about existing interfaces and addresses */
	kif_redistribute();

	if (kr_init(!(ldpd_conf->flags & LDPD_FLAG_NO_FIB_UPDATE)) == -1)
		fatalx("kr_init failed");

	/* remove unneded stuff from config */
		/* ... */

	event_dispatch();

	ldpd_shutdown();
	/* NOTREACHED */
	return (0);
}

void
ldpd_shutdown(void)
{
	pid_t		 pid;

	if (ldpe_pid)
		kill(ldpe_pid, SIGTERM);

	if (lde_pid)
		kill(lde_pid, SIGTERM);

	control_cleanup();
	kr_shutdown();

	do {
		if ((pid = wait(NULL)) == -1 &&
		    errno != EINTR && errno != ECHILD)
			fatal("wait");
	} while (pid != -1 || (pid == -1 && errno == EINTR));

	msgbuf_clear(&iev_ldpe->ibuf.w);
	free(iev_ldpe);
	msgbuf_clear(&iev_lde->ibuf.w);
	free(iev_lde);
	free(ldpd_conf);

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
main_dispatch_ldpe(int fd, short event, void *bula)
{
	struct imsgev		*iev = bula;
	struct imsgbuf		*ibuf = &iev->ibuf;
	struct imsg		 imsg;
	ssize_t			 n;
	int			 shut = 0, verbose;

	if (event & EV_READ) {
		if ((n = imsg_read(ibuf)) == -1)
			fatal("imsg_read error");
		if (n == 0)	/* connection closed */
			shut = 1;
	}
	if (event & EV_WRITE) {
		if ((n = msgbuf_write(&ibuf->w)) == -1 && errno != EAGAIN)
			fatal("msgbuf_write");
		if (n == 0)
			shut = 1;
	}

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatal("imsg_get");

		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_CTL_RELOAD:
/*
			if (ldp_reload() == -1)
				log_warnx("configuration reload failed");
			else
				log_debug("configuration reloaded");
			break;
*/
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
		case IMSG_CTL_LOG_VERBOSE:
			/* already checked by ldpe */
			memcpy(&verbose, imsg.data, sizeof(verbose));
			log_verbose(verbose);
			break;
		default:
			log_debug("main_dispatch_ldpe: error handling imsg %d",
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
main_dispatch_lde(int fd, short event, void *bula)
{
	struct imsgev  *iev = bula;
	struct imsgbuf *ibuf = &iev->ibuf;
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
		if ((n = msgbuf_write(&ibuf->w)) == -1 && errno != EAGAIN)
			fatal("msgbuf_write");
		if (n == 0)
			shut = 1;
	}

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatal("imsg_get");

		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_KLABEL_CHANGE:
			if (imsg.hdr.len - IMSG_HEADER_SIZE !=
			    sizeof(struct kroute))
				fatalx("invalid size of IMSG_KLABEL_CHANGE");
			if (kr_change(imsg.data))
				log_warn("main_dispatch_lde: error changing "
				    "route");
			break;
		case IMSG_KLABEL_DELETE:
			if (imsg.hdr.len - IMSG_HEADER_SIZE !=
			    sizeof(struct kroute))
				fatalx("invalid size of IMSG_KLABEL_DELETE");
			if (kr_delete(imsg.data))
				log_warn("main_dispatch_lde: error deleting "
				    "route");
			break;
		default:
			log_debug("main_dispatch_lde: error handling imsg %d",
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
main_imsg_compose_ldpe(int type, pid_t pid, void *data, u_int16_t datalen)
{
	if (iev_ldpe == NULL)
		return;
	imsg_compose_event(iev_ldpe, type, 0, pid, -1, data, datalen);
}

void
main_imsg_compose_lde(int type, pid_t pid, void *data, u_int16_t datalen)
{
	imsg_compose_event(iev_lde, type, 0, pid, -1, data, datalen);
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
imsg_compose_event(struct imsgev *iev, u_int16_t type,
    u_int32_t peerid, pid_t pid, int fd, void *data, u_int16_t datalen)
{
	int	ret;

	if ((ret = imsg_compose(&iev->ibuf, type, peerid,
	    pid, fd, data, datalen)) != -1)
		imsg_event_add(iev);
	return (ret);
}

void
evbuf_enqueue(struct evbuf *eb, struct ibuf *buf)
{
	ibuf_close(&eb->wbuf, buf);
	evbuf_event_add(eb);
}

void
evbuf_event_add(struct evbuf *eb)
{
	if (eb->wbuf.queued)
		event_add(&eb->ev, NULL);
}

void
evbuf_init(struct evbuf *eb, int fd, void (*handler)(int, short, void *),
    void *arg)
{
	msgbuf_init(&eb->wbuf);
	eb->wbuf.fd = fd;
	event_set(&eb->ev, eb->wbuf.fd, EV_WRITE, handler, arg);
}

void
evbuf_clear(struct evbuf *eb)
{
	event_del(&eb->ev);
	msgbuf_clear(&eb->wbuf);
	eb->wbuf.fd = -1;
}

/*
int
ldp_reload(void)
{
	struct lspace		*lspace;
	struct iface		*iface;
	struct ldpd_conf	*xconf;

	if ((xconf = parse_config(conffile, ldpd_conf->opts)) == NULL)
		return (-1);

	if (ldp_sendboth(IMSG_RECONF_CONF, xconf, sizeof(*xconf)) == -1)
		return (-1);

	LIST_FOREACH(lspace, &xconf->lspace_list, entry) {
		if (ldp_sendboth(IMSG_RECONF_AREA, lspace, sizeof(*lspace)) == -1)
			return (-1);

		LIST_FOREACH(iface, &lspace->iface_list, entry) {
			if (ldp_sendboth(IMSG_RECONF_IFACE, iface,
			    sizeof(*iface)) == -1)
				return (-1);
		}
	}

	if (ldp_sendboth(IMSG_RECONF_END, NULL, 0) == -1)
		return (-1);

	merge_config(ldpd_conf, xconf);

	kr_reload();
	return (0);
}

int
ldp_sendboth(enum imsg_type type, void *buf, u_int16_t len)
{
	if (imsg_compose_event(iev_ldpe, type, 0, 0, -1, buf, len) == -1)
		return (-1);
	if (imsg_compose_event(iev_lde, type, 0, 0, -1, buf, len) == -1)
		return (-1);
	return (0);
}

void
merge_config(struct ldpd_conf *conf, struct ldpd_conf *xconf)
{
	struct lspace		*a, *xa, *na;
	struct iface		*iface;
	struct redistribute	*r;
	int			 rchange = 0;

	conf->flags = xconf->flags;
	conf->spf_delay = xconf->spf_delay;
	conf->spf_hold_time = xconf->spf_hold_time;
	if ((conf->redistribute & REDISTRIBUTE_ON) !=
	    (xconf->redistribute & REDISTRIBUTE_ON))
		rchange = 1;
	conf->redistribute = xconf->redistribute;
	conf->rfc1583compat = xconf->rfc1583compat;

	if (ldpd_process == PROC_MAIN) {
		while ((r = SIMPLEQ_FIRST(&conf->redist_list)) != NULL) {
			SIMPLEQ_REMOVE_HEAD(&conf->redist_list, entry);
			free(r);
		}
		while ((r = SIMPLEQ_FIRST(&xconf->redist_list)) != NULL) {
			SIMPLEQ_REMOVE_HEAD(&xconf->redist_list, entry);
			SIMPLEQ_INSERT_TAIL(&conf->redist_list, r, entry);
		}
		goto done;
	}

	for (a = LIST_FIRST(&conf->lspace_list); a != NULL; a = na) {
		na = LIST_NEXT(a, entry);
		if ((xa = lspace_find(xconf, a->id)) == NULL) {
			if (ldpd_process == PROC_LDP_ENGINE) {
				LIST_FOREACH(iface, &a->iface_list, entry)
					if_fsm(iface, IF_EVT_DOWN);
			}
			LIST_REMOVE(a, entry);
			lspace_del(a);
		}
	}

	for (xa = LIST_FIRST(&xconf->lspace_list); xa != NULL; xa = na) {
		na = LIST_NEXT(xa, entry);
		if ((a = lspace_find(conf, xa->id)) == NULL) {
			LIST_REMOVE(xa, entry);
			LIST_INSERT_HEAD(&conf->lspace_list, xa, entry);
			if (ldpd_process == PROC_LDP_ENGINE) {
				LIST_FOREACH(iface, &xa->iface_list, entry) {
					if_init(conf, iface);
					if (if_fsm(iface, IF_EVT_UP)) {
						log_debug("error starting "
						    "interface %s",
						    iface->name);
					}
				}
			}
			continue;
		}
		a->stub = xa->stub;
		a->stub_default_cost = xa->stub_default_cost;
		if (ldpd_process == PROC_LDE_ENGINE)
			a->dirty = 1;

		if (merge_interfaces(a, xa) &&
		    ldpd_process == PROC_LDP_ENGINE)
			a->dirty = 1;
	}

	if (ldpd_process == PROC_LDP_ENGINE) {
		LIST_FOREACH(a, &conf->lspace_list, entry) {
			LIST_FOREACH(iface, &a->iface_list, entry) {
				if (iface->state == IF_STA_NEW) {
					iface->state = IF_STA_DOWN;
					if_init(conf, iface);
					if (if_fsm(iface, IF_EVT_UP)) {
						log_debug("error starting "
						    "interface %s",
						    iface->name);
					}
				}
			}
		}
	}

done:
	while ((a = LIST_FIRST(&xconf->lspace_list)) != NULL) {
		LIST_REMOVE(a, entry);
		lspace_del(a);
	}
	free(xconf);
}

int
merge_interfaces(struct lspace *a, struct lspace *xa)
{
	struct iface	*i, *xi, *ni;
	int		 dirty = 0;

	for (i = LIST_FIRST(&a->iface_list); i != NULL; i = ni) {
		ni = LIST_NEXT(i, entry);
		if (iface_lookup(xa, i) == NULL) {
			log_debug("merge_config: proc %d label space %s removing "
			    "interface %s", ldpd_process, inet_ntoa(a->id),
			    i->name);
			if (ldpd_process == PROC_LDP_ENGINE)
				if_fsm(i, IF_EVT_DOWN);
			LIST_REMOVE(i, entry);
			if_del(i);
		}
	}

	for (xi = LIST_FIRST(&xa->iface_list); xi != NULL; xi = ni) {
		ni = LIST_NEXT(xi, entry);
		if ((i = iface_lookup(a, xi)) == NULL) {
			log_debug("merge_config: proc %d label space %s adding "
			    "interface %s", ldpd_process, inet_ntoa(a->id),
			    xi->name);
			LIST_REMOVE(xi, entry);
			LIST_INSERT_HEAD(&a->iface_list, xi, entry);
			xi->lspace = a;
			if (ldpd_process == PROC_LDP_ENGINE)
				xi->state = IF_STA_NEW;
			continue;
		}
		log_debug("merge_config: proc %d label space %s merging interface %s",
		    ldpd_process, inet_ntoa(a->id), i->name);
		i->addr = xi->addr;
		i->dst = xi->dst;
		i->mask = xi->mask;
		i->abr_id = xi->abr_id;
		i->baudrate = xi->baudrate;
		i->dead_interval = xi->dead_interval;
		i->mtu = xi->mtu;
		i->transmit_delay = xi->transmit_delay;
		i->hello_interval = xi->hello_interval;
		i->rxmt_interval = xi->rxmt_interval;
		if (i->metric != xi->metric)
			dirty = 1;
		i->metric = xi->metric;
		i->priority = xi->priority;
		i->flags = xi->flags;
		i->type = xi->type;
		i->media_type = xi->media_type;
		i->linkstate = xi->linkstate;

		if (i->passive != xi->passive) {
			if (ldpd_process == PROC_LDP_ENGINE)
				if_fsm(i, IF_EVT_DOWN);
			i->passive = xi->passive;
			if (ldpd_process == PROC_LDP_ENGINE)
				if_fsm(i, IF_EVT_UP);
		}
	}
	return (dirty);
}

struct iface *
iface_lookup(struct lspace *lspace, struct iface *iface)
{
	struct iface	*i;

	LIST_FOREACH(i, &lspace->iface_list, entry)
		if (i->ifindex == iface->ifindex)
			return (i);
	return (NULL);
}
*/
