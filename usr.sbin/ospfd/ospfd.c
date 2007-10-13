/*	$OpenBSD: ospfd.c,v 1.53 2007/10/13 16:35:22 deraadt Exp $ */

/*
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

#include "ospfd.h"
#include "ospf.h"
#include "ospfe.h"
#include "control.h"
#include "log.h"
#include "rde.h"

void		main_sig_handler(int, short, void *);
__dead void	usage(void);
void		ospfd_shutdown(void);
int		check_child(pid_t, const char *);

void	main_dispatch_ospfe(int, short, void *);
void	main_dispatch_rde(int, short, void *);

void	ospf_redistribute_default(int);

int	ospf_reload(void);
int	ospf_sendboth(enum imsg_type, void *, u_int16_t);
int	merge_interfaces(struct area *, struct area *);
struct iface *iface_lookup(struct area *, struct iface *);

int	pipe_parent2ospfe[2];
int	pipe_parent2rde[2];
int	pipe_ospfe2rde[2];

struct ospfd_conf	*ospfd_conf = NULL;
struct imsgbuf		*ibuf_ospfe;
struct imsgbuf		*ibuf_rde;
char			*conffile;

pid_t			 ospfe_pid = 0;
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
		if (check_child(ospfe_pid, "ospf engine")) {
			ospfe_pid = 0;
			die = 1;
		}
		if (check_child(rde_pid, "route decision engine")) {
			rde_pid = 0;
			die = 1;
		}
		if (die)
			ospfd_shutdown();
		break;
	case SIGHUP:
		if (ospf_reload() == -1)
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

	fprintf(stderr, "usage: %s [-cdnv] [-f file]\n", __progname);
	exit(1);
}

int
main(int argc, char *argv[])
{
	struct event		 ev_sigint, ev_sigterm, ev_sigchld, ev_sighup;
	struct area		*a;
	int			 ch, opts = 0;
	int			 debug = 0;
	int			 ipforwarding;
	int			 mib[4];
	size_t			 len;

	conffile = CONF_FILE;
	ospfd_process = PROC_MAIN;

	while ((ch = getopt(argc, argv, "cdf:nv")) != -1) {
		switch (ch) {
		case 'c':
			opts |= OSPFD_OPT_FORCE_DEMOTE;
			break;
		case 'd':
			debug = 1;
			break;
		case 'f':
			conffile = optarg;
			break;
		case 'n':
			opts |= OSPFD_OPT_NOACTION;
			break;
		case 'v':
			if (opts & OSPFD_OPT_VERBOSE)
				opts |= OSPFD_OPT_VERBOSE2;
			opts |= OSPFD_OPT_VERBOSE;
			break;

		default:
			usage();
			/* NOTREACHED */
		}
	}

	/* start logging */
	log_init(debug);

	mib[0] = CTL_NET;
	mib[1] = PF_INET;
	mib[2] = IPPROTO_IP;
	mib[3] = IPCTL_FORWARDING;
	len = sizeof(ipforwarding);
	if (sysctl(mib, 4, &ipforwarding, &len, NULL, 0) == -1)
		err(1, "sysctl");

	if (ipforwarding != 1) {
		log_warnx("WARNING: IP forwarding NOT enabled, "
		    "running as stub router");
		opts |= OSPFD_OPT_STUB_ROUTER;
	}


	/* fetch interfaces early */
	kif_init();

	/* parse config file */
	if ((ospfd_conf = parse_config(conffile, opts)) == NULL )
		exit(1);

	if (ospfd_conf->opts & OSPFD_OPT_NOACTION) {
		if (ospfd_conf->opts & OSPFD_OPT_VERBOSE)
			print_config(ospfd_conf);
		else
			fprintf(stderr, "configuration OK\n");
		exit(0);
	}

	/* check for root privileges  */
	if (geteuid())
		errx(1, "need root privileges");

	/* check for ospfd user */
	if (getpwnam(OSPFD_USER) == NULL)
		errx(1, "unknown user %s", OSPFD_USER);

	if (!debug)
		daemon(1, 0);

	log_info("startup");

	if (socketpair(AF_UNIX, SOCK_STREAM, PF_UNSPEC,
	    pipe_parent2ospfe) == -1)
		fatal("socketpair");
	if (socketpair(AF_UNIX, SOCK_STREAM, PF_UNSPEC, pipe_parent2rde) == -1)
		fatal("socketpair");
	if (socketpair(AF_UNIX, SOCK_STREAM, PF_UNSPEC, pipe_ospfe2rde) == -1)
		fatal("socketpair");
	session_socket_blockmode(pipe_parent2ospfe[0], BM_NONBLOCK);
	session_socket_blockmode(pipe_parent2ospfe[1], BM_NONBLOCK);
	session_socket_blockmode(pipe_parent2rde[0], BM_NONBLOCK);
	session_socket_blockmode(pipe_parent2rde[1], BM_NONBLOCK);
	session_socket_blockmode(pipe_ospfe2rde[0], BM_NONBLOCK);
	session_socket_blockmode(pipe_ospfe2rde[1], BM_NONBLOCK);

	/* start children */
	rde_pid = rde(ospfd_conf, pipe_parent2rde, pipe_ospfe2rde,
	    pipe_parent2ospfe);
	ospfe_pid = ospfe(ospfd_conf, pipe_parent2ospfe, pipe_ospfe2rde,
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
	close(pipe_parent2ospfe[1]);
	close(pipe_parent2rde[1]);
	close(pipe_ospfe2rde[0]);
	close(pipe_ospfe2rde[1]);

	if ((ibuf_ospfe = malloc(sizeof(struct imsgbuf))) == NULL ||
	    (ibuf_rde = malloc(sizeof(struct imsgbuf))) == NULL)
		fatal(NULL);
	imsg_init(ibuf_ospfe, pipe_parent2ospfe[0], main_dispatch_ospfe);
	imsg_init(ibuf_rde, pipe_parent2rde[0], main_dispatch_rde);

	/* setup event handler */
	ibuf_ospfe->events = EV_READ;
	event_set(&ibuf_ospfe->ev, ibuf_ospfe->fd, ibuf_ospfe->events,
	    ibuf_ospfe->handler, ibuf_ospfe);
	event_add(&ibuf_ospfe->ev, NULL);

	ibuf_rde->events = EV_READ;
	event_set(&ibuf_rde->ev, ibuf_rde->fd, ibuf_rde->events,
	    ibuf_rde->handler, ibuf_rde);
	event_add(&ibuf_rde->ev, NULL);

	if (kr_init(!(ospfd_conf->flags & OSPFD_FLAG_NO_FIB_UPDATE)) == -1)
		fatalx("kr_init failed");

	/* remove unneded stuff from config */
	while ((a = LIST_FIRST(&ospfd_conf->area_list)) != NULL) {
		LIST_REMOVE(a, entry);
		area_del(a);
	}

	/* redistribute default */
	ospf_redistribute_default(IMSG_NETWORK_ADD);

	event_dispatch();

	ospfd_shutdown();
	/* NOTREACHED */
	return (0);
}

void
ospfd_shutdown(void)
{
	pid_t		 pid;

	if (ospfe_pid)
		kill(ospfe_pid, SIGTERM);

	if (rde_pid)
		kill(rde_pid, SIGTERM);

	control_cleanup();
	kr_shutdown();
	carp_demote_shutdown();

	do {
		if ((pid = wait(NULL)) == -1 &&
		    errno != EINTR && errno != ECHILD)
			fatal("wait");
	} while (pid != -1 || (pid == -1 && errno == EINTR));

	msgbuf_clear(&ibuf_ospfe->w);
	free(ibuf_ospfe);
	msgbuf_clear(&ibuf_rde->w);
	free(ibuf_rde);
	free(ospfd_conf);

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
main_dispatch_ospfe(int fd, short event, void *bula)
{
	struct imsgbuf		*ibuf = bula;
	struct imsg		 imsg;
	struct demote_msg	 dmsg;
	ssize_t			 n;
	int			 shut = 0;

	switch (event) {
	case EV_READ:
		if ((n = imsg_read(ibuf)) == -1)
			fatal("imsg_read error");
		if (n == 0)	/* connection closed */
			shut = 1;
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
			if (ospf_reload() == -1)
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
		default:
			log_debug("main_dispatch_ospfe: error handling imsg %d",
			    imsg.hdr.type);
			break;
		}
		imsg_free(&imsg);
	}
	if (!shut)
		imsg_event_add(ibuf);
	else {
		/* this pipe is dead, so remove the event handler */
		event_del(&ibuf->ev);
		event_loopexit(NULL);
	}
}

/* ARGSUSED */
void
main_dispatch_rde(int fd, short event, void *bula)
{
	struct imsgbuf  *ibuf = bula;
	struct imsg	 imsg;
	ssize_t		 n;
	int		 count, shut = 0;

	switch (event) {
	case EV_READ:
		if ((n = imsg_read(ibuf)) == -1)
			fatal("imsg_read error");
		if (n == 0)	/* connection closed */
			shut = 1;
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
			count = (imsg.hdr.len - IMSG_HEADER_SIZE) /
			    sizeof(struct kroute);
			if (kr_change(imsg.data, count))
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
		imsg_event_add(ibuf);
	else {
		/* this pipe is dead, so remove the event handler */
		event_del(&ibuf->ev);
		event_loopexit(NULL);
	}
}

void
main_imsg_compose_ospfe(int type, pid_t pid, void *data, u_int16_t datalen)
{
	imsg_compose(ibuf_ospfe, type, 0, pid, data, datalen);
}

void
main_imsg_compose_rde(int type, pid_t pid, void *data, u_int16_t datalen)
{
	imsg_compose(ibuf_rde, type, 0, pid, data, datalen);
}

/* this needs to be added here so that ospfctl can be used without libevent */
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

int
ospf_redistribute(struct kroute *kr, u_int32_t *metric)
{
	struct redistribute	*r;

	/* only allow 0.0.0.0/0 via REDISTRIBUTE_DEFAULT */
	if (kr->prefix.s_addr == INADDR_ANY && kr->prefixlen == 0)
		return (0);

	SIMPLEQ_FOREACH(r, &ospfd_conf->redist_list, entry) {
		switch (r->type & ~REDIST_NO) {
		case REDIST_LABEL:
			if (kr->rtlabel == r->label) {
				*metric = r->metric;
				return (r->type & REDIST_NO ? 0 : 1);
			}
			break;
		case REDIST_STATIC:
			/*
			 * Dynamic routes are not redistributable. Placed here
			 * so that link local addresses can be redistributed
			 * via a rtlabel.
			 */
			if (kr->flags & F_DYNAMIC)
				continue;
			if (kr->flags & F_STATIC) {
				*metric = r->metric;
				return (r->type & REDIST_NO ? 0 : 1);
			}
			break;
		case REDIST_CONNECTED:
			if (kr->flags & F_DYNAMIC)
				continue;
			if (kr->flags & F_CONNECTED) {
				*metric = r->metric;
				return (r->type & REDIST_NO ? 0 : 1);
			}
			break;
		case REDIST_ADDR:
			if (kr->flags & F_DYNAMIC)
				continue;
			if ((kr->prefix.s_addr & r->mask.s_addr) ==
			    (r->addr.s_addr & r->mask.s_addr) &&
			    kr->prefixlen >= mask2prefixlen(r->mask.s_addr)) {
				*metric = r->metric;
				return (r->type & REDIST_NO? 0 : 1);
			}
			break;
		}
	}

	return (0);
}

void
ospf_redistribute_default(int type)
{
	struct rroute	rr;

	if (!(ospfd_conf->redistribute & REDISTRIBUTE_DEFAULT))
		return;

	bzero(&rr, sizeof(rr));
	rr.metric = ospfd_conf->defaultmetric;
	main_imsg_compose_rde(type, 0, &rr, sizeof(struct rroute));
}

int
ospf_reload(void)
{
	struct area		*area;
	struct iface		*iface;
	struct ospfd_conf	*xconf;

	if ((xconf = parse_config(conffile, ospfd_conf->opts)) == NULL)
		return (-1);

	/* send config to childs */
	if (ospf_sendboth(IMSG_RECONF_CONF, xconf, sizeof(*xconf)) == -1)
		return (-1);

	/* send interfaces */
	LIST_FOREACH(area, &xconf->area_list, entry) {
		if (ospf_sendboth(IMSG_RECONF_AREA, area, sizeof(*area)) == -1)
			return (-1);

		LIST_FOREACH(iface, &area->iface_list, entry) {
			if (ospf_sendboth(IMSG_RECONF_IFACE, iface,
			    sizeof(*iface)) == -1)
				return (-1);
			if (iface->auth_type == AUTH_CRYPT)
				if (md_list_send(&iface->auth_md_list,
				    ibuf_ospfe) == -1)
					return (-1);
		}
	}

	if (ospf_sendboth(IMSG_RECONF_END, NULL, 0) == -1)
		return (-1);

	merge_config(ospfd_conf, xconf);
	/* update redistribute lists */
	kr_reload();
	return (0);
}

int
ospf_sendboth(enum imsg_type type, void *buf, u_int16_t len)
{
	if (imsg_compose(ibuf_ospfe, type, 0, 0, buf, len) == -1)
		return (-1);
	if (imsg_compose(ibuf_rde, type, 0, 0, buf, len) == -1)
		return (-1);
	return (0);
}

void
merge_config(struct ospfd_conf *conf, struct ospfd_conf *xconf)
{
	struct area		*a, *xa, *na;
	struct iface		*iface;
	struct redistribute	*r;
	int			 rchange = 0;

	/* change of rtr_id needs a restart */
	conf->flags = xconf->flags;
	conf->spf_delay = xconf->spf_delay;
	conf->spf_hold_time = xconf->spf_hold_time;
	if ((conf->redistribute & REDISTRIBUTE_ON) !=
	    (xconf->redistribute & REDISTRIBUTE_ON))
		rchange = 1;
	conf->redistribute = xconf->redistribute;
	conf->rfc1583compat = xconf->rfc1583compat;

	if (ospfd_process == PROC_MAIN) {
		/* main process does neither use areas nor interfaces */
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

	/* merge areas and interfaces */
	for (a = LIST_FIRST(&conf->area_list); a != NULL; a = na) {
		na = LIST_NEXT(a, entry);
		/* find deleted areas */
		if ((xa = area_find(xconf, a->id)) == NULL) {
			if (ospfd_process == PROC_OSPF_ENGINE) {
				LIST_FOREACH(iface, &a->iface_list, entry)
					if_fsm(iface, IF_EVT_DOWN);
			}
			LIST_REMOVE(a, entry);
			area_del(a);
		}
	}

	for (xa = LIST_FIRST(&xconf->area_list); xa != NULL; xa = na) {
		na = LIST_NEXT(xa, entry);
		if ((a = area_find(conf, xa->id)) == NULL) {
			LIST_REMOVE(xa, entry);
			LIST_INSERT_HEAD(&conf->area_list, xa, entry);
			if (ospfd_process == PROC_OSPF_ENGINE) {
				/* start interfaces */
				ospfe_demote_area(xa, 0);
				LIST_FOREACH(iface, &xa->iface_list, entry) {
					if_init(conf, iface);
					if (if_fsm(iface, IF_EVT_UP)) {
						log_debug("error starting "
						    "interface %s",
						    iface->name);
					}
				}
			}
			/* no need to merge interfaces */
			continue;
		}
		/*
		 * stub is not yet used but switching between stub and normal
		 * will be another painful job.
		 */
		a->stub = xa->stub;
		a->stub_default_cost = xa->stub_default_cost;
		if (ospfd_process == PROC_RDE_ENGINE)
			a->dirty = 1; /* force SPF tree recalculation */

		/* merge interfaces */
		if (merge_interfaces(a, xa) &&
		    ospfd_process == PROC_OSPF_ENGINE)
			a->dirty = 1; /* force rtr LSA update */
	}

	if (ospfd_process == PROC_OSPF_ENGINE) {
		LIST_FOREACH(a, &conf->area_list, entry) {
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
			if (a->dirty || rchange) {
				a->dirty = 0;
				orig_rtr_lsa(a);
			}
		}
	}

done:
	while ((a = LIST_FIRST(&xconf->area_list)) != NULL) {
		LIST_REMOVE(a, entry);
		area_del(a);
	}
	free(xconf);
}

int
merge_interfaces(struct area *a, struct area *xa)
{
	struct iface	*i, *xi, *ni;
	int		 dirty = 0;

	/* problems:
	 * - new interfaces (easy)
	 * - deleted interfaces (needs to be done via fsm?)
	 * - changing passive (painful?)
	 */
	for (i = LIST_FIRST(&a->iface_list); i != NULL; i = ni) {
		ni = LIST_NEXT(i, entry);
		if (iface_lookup(xa, i) == NULL) {
			log_debug("merge_config: proc %d area %s removing "
			    "interface %s", ospfd_process, inet_ntoa(a->id),
			    i->name);
			if (ospfd_process == PROC_OSPF_ENGINE)
				if_fsm(i, IF_EVT_DOWN);
			LIST_REMOVE(i, entry);
			if_del(i);
		}
	}

	for (xi = LIST_FIRST(&xa->iface_list); xi != NULL; xi = ni) {
		ni = LIST_NEXT(xi, entry);
		if ((i = iface_lookup(a, xi)) == NULL) {
			/* new interface but delay initialisation */
			log_debug("merge_config: proc %d area %s adding "
			    "interface %s", ospfd_process, inet_ntoa(a->id),
			    xi->name);
			LIST_REMOVE(xi, entry);
			LIST_INSERT_HEAD(&a->iface_list, xi, entry);
			xi->area = a;
			if (ospfd_process == PROC_OSPF_ENGINE)
				xi->state = IF_STA_NEW;
			continue;
		}
		log_debug("merge_config: proc %d area %s merging interface %s",
		    ospfd_process, inet_ntoa(a->id), i->name);
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
		i->flags = xi->flags; /* needed? */
		i->type = xi->type; /* needed? */
		i->media_type = xi->media_type; /* needed? */
		i->linkstate = xi->linkstate; /* needed? */

		i->auth_type = xi->auth_type;
		strncpy(i->auth_key, xi->auth_key, MAX_SIMPLE_AUTH_LEN);
		md_list_clr(&i->auth_md_list);
		md_list_copy(&i->auth_md_list, &xi->auth_md_list);

		if (i->passive != xi->passive) {
			/* need to restart interface to cope with this change */
			if (ospfd_process == PROC_OSPF_ENGINE)
				if_fsm(i, IF_EVT_DOWN);
			i->passive = xi->passive;
			if (ospfd_process == PROC_OSPF_ENGINE)
				if_fsm(i, IF_EVT_UP);
		}
	}
	return (dirty);
}

struct iface *
iface_lookup(struct area *area, struct iface *iface)
{
	struct iface	*i;

	LIST_FOREACH(i, &area->iface_list, entry)
		if (i->ifindex == iface->ifindex)
			return (i);
	return (NULL);
}
