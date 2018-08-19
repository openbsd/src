/*	$OpenBSD: slaacd.c,v 1.31 2018/08/19 12:29:03 florian Exp $	*/

/*
 * Copyright (c) 2017 Florian Obser <florian@openbsd.org>
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
#include <sys/ioctl.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/syslog.h>
#include <sys/sysctl.h>
#include <sys/uio.h>
#include <sys/wait.h>

#include <net/if.h>
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <netinet6/in6_var.h>
#include <netinet/icmp6.h>

#include <err.h>
#include <errno.h>
#include <event.h>
#include <imsg.h>
#include <netdb.h>
#include <pwd.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

#include "log.h"
#include "slaacd.h"
#include "frontend.h"
#include "engine.h"
#include "control.h"

__dead void	usage(void);
__dead void	main_shutdown(void);

void	main_sig_handler(int, short, void *);

static pid_t	start_child(int, char *, int, int, int);

void	main_dispatch_frontend(int, short, void *);
void	main_dispatch_engine(int, short, void *);
void	handle_proposal(struct imsg_proposal *);
void	configure_interface(struct imsg_configure_address *);
void	configure_gateway(struct imsg_configure_dfr *, uint8_t);
void	add_gateway(struct imsg_configure_dfr *);
void	delete_gateway(struct imsg_configure_dfr *);
int	get_soiikey(uint8_t *);

static int	main_imsg_send_ipc_sockets(struct imsgbuf *, struct imsgbuf *);
int		main_imsg_compose_frontend(int, pid_t, void *, uint16_t);
int		main_imsg_compose_frontend_fd(int, pid_t, int);
int		main_imsg_compose_engine(int, pid_t, void *, uint16_t);

struct imsgev		*iev_frontend;
struct imsgev		*iev_engine;

pid_t	 frontend_pid;
pid_t	 engine_pid;

int	 routesock, ioctl_sock, rtm_seq = 0;

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
	case SIGHUP:
		log_debug("sighub received");
		break;
	default:
		fatalx("unexpected signal");
	}
}

__dead void
usage(void)
{
	extern char *__progname;

	fprintf(stderr, "usage: %s [-dv] [-s socket]\n",
	    __progname);
	exit(1);
}

int
main(int argc, char *argv[])
{
	struct event		 ev_sigint, ev_sigterm, ev_sighup;
	struct icmp6_filter	 filt;
	int			 ch;
	int			 debug = 0, engine_flag = 0, frontend_flag = 0;
	int			 verbose = 0;
	char			*saved_argv0;
	int			 pipe_main2frontend[2];
	int			 pipe_main2engine[2];
	int			 icmp6sock, on = 1;
	int			 frontend_routesock, rtfilter;
	char			*csock = SLAACD_SOCKET;
#ifndef SMALL
	int			 control_fd;
#endif /* SMALL */

	log_init(1, LOG_DAEMON);	/* Log to stderr until daemonized. */
	log_setverbose(1);

	saved_argv0 = argv[0];
	if (saved_argv0 == NULL)
		saved_argv0 = "slaacd";

	while ((ch = getopt(argc, argv, "dEFs:v")) != -1) {
		switch (ch) {
		case 'd':
			debug = 1;
			break;
		case 'E':
			engine_flag = 1;
			break;
		case 'F':
			frontend_flag = 1;
			break;
		case 's':
			csock = optarg;
			break;
		case 'v':
			verbose++;
			break;
		default:
			usage();
		}
	}

	argc -= optind;
	argv += optind;
	if (argc > 0 || (engine_flag && frontend_flag))
		usage();

	if (engine_flag)
		engine(debug, verbose);
	else if (frontend_flag)
		frontend(debug, verbose);

	/* Check for root privileges. */
	if (geteuid())
		errx(1, "need root privileges");

	/* Check for assigned daemon user */
	if (getpwnam(SLAACD_USER) == NULL)
		errx(1, "unknown user %s", SLAACD_USER);

	log_init(debug, LOG_DAEMON);
	log_setverbose(verbose);

	if (!debug)
		daemon(0, 0);

	log_info("startup");

	if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK,
	    PF_UNSPEC, pipe_main2frontend) == -1)
		fatal("main2frontend socketpair");
	if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK,
	    PF_UNSPEC, pipe_main2engine) == -1)
		fatal("main2engine socketpair");

	/* Start children. */
	engine_pid = start_child(PROC_ENGINE, saved_argv0, pipe_main2engine[1],
	    debug, verbose);
	frontend_pid = start_child(PROC_FRONTEND, saved_argv0,
	    pipe_main2frontend[1], debug, verbose);

	slaacd_process = PROC_MAIN;

	log_procinit(log_procnames[slaacd_process]);

	if ((routesock = socket(PF_ROUTE, SOCK_RAW | SOCK_CLOEXEC |
	    SOCK_NONBLOCK, AF_INET6)) < 0)
		fatal("route socket");
	shutdown(SHUT_RD, routesock);

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
	    (iev_engine = malloc(sizeof(struct imsgev))) == NULL)
		fatal(NULL);
	imsg_init(&iev_frontend->ibuf, pipe_main2frontend[0]);
	iev_frontend->handler = main_dispatch_frontend;
	imsg_init(&iev_engine->ibuf, pipe_main2engine[0]);
	iev_engine->handler = main_dispatch_engine;

	/* Setup event handlers for pipes to engine & frontend. */
	iev_frontend->events = EV_READ;
	event_set(&iev_frontend->ev, iev_frontend->ibuf.fd,
	    iev_frontend->events, iev_frontend->handler, iev_frontend);
	event_add(&iev_frontend->ev, NULL);

	iev_engine->events = EV_READ;
	event_set(&iev_engine->ev, iev_engine->ibuf.fd, iev_engine->events,
	    iev_engine->handler, iev_engine);
	event_add(&iev_engine->ev, NULL);

	if (main_imsg_send_ipc_sockets(&iev_frontend->ibuf, &iev_engine->ibuf))
		fatal("could not establish imsg links");

	if ((ioctl_sock = socket(AF_INET6, SOCK_DGRAM | SOCK_CLOEXEC, 0)) < 0)
		fatal("socket");

	if ((icmp6sock = socket(AF_INET6, SOCK_RAW | SOCK_CLOEXEC,
	    IPPROTO_ICMPV6)) < 0)
		fatal("ICMPv6 socket");

	if (setsockopt(icmp6sock, IPPROTO_IPV6, IPV6_RECVPKTINFO, &on,
	    sizeof(on)) < 0)
		fatal("IPV6_RECVPKTINFO");

	if (setsockopt(icmp6sock, IPPROTO_IPV6, IPV6_RECVHOPLIMIT, &on,
	    sizeof(on)) < 0)
		fatal("IPV6_RECVHOPLIMIT");

	/* only router advertisements */
	ICMP6_FILTER_SETBLOCKALL(&filt);
	ICMP6_FILTER_SETPASS(ND_ROUTER_ADVERT, &filt);
	if (setsockopt(icmp6sock, IPPROTO_ICMPV6, ICMP6_FILTER, &filt,
	    sizeof(filt)) == -1)
		fatal("ICMP6_FILTER");

	if ((frontend_routesock = socket(PF_ROUTE, SOCK_RAW | SOCK_CLOEXEC,
	    AF_INET6)) < 0)
		fatal("route socket");

	rtfilter = ROUTE_FILTER(RTM_IFINFO) | ROUTE_FILTER(RTM_NEWADDR) |
	    ROUTE_FILTER(RTM_DELADDR) | ROUTE_FILTER(RTM_PROPOSAL) |
	    ROUTE_FILTER(RTM_DELETE) | ROUTE_FILTER(RTM_CHGADDRATTR);
	if (setsockopt(frontend_routesock, PF_ROUTE, ROUTE_MSGFILTER,
	    &rtfilter, sizeof(rtfilter)) < 0)
		fatal("setsockopt(ROUTE_MSGFILTER)");

#ifndef SMALL
	if ((control_fd = control_init(csock)) == -1)
		fatalx("control socket setup failed");
#endif /* SMALL */

	if (pledge("stdio sendfd wroute", NULL) == -1)
		fatal("pledge");

	main_imsg_compose_frontend_fd(IMSG_ICMP6SOCK, 0, icmp6sock);

	main_imsg_compose_frontend_fd(IMSG_ROUTESOCK, 0, frontend_routesock);

#ifndef SMALL
	main_imsg_compose_frontend_fd(IMSG_CONTROLFD, 0, control_fd);
#endif /* SMALL */

	main_imsg_compose_frontend(IMSG_STARTUP, 0, NULL, 0);

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
	msgbuf_clear(&iev_engine->ibuf.w);
	close(iev_engine->ibuf.fd);

	log_debug("waiting for children to terminate");
	do {
		pid = wait(&status);
		if (pid == -1) {
			if (errno != EINTR && errno != ECHILD)
				fatal("wait");
		} else if (WIFSIGNALED(status))
			log_warnx("%s terminated; signal %d",
			    (pid == engine_pid) ? "engine" :
			    "frontend", WTERMSIG(status));
	} while (pid != -1 || (pid == -1 && errno == EINTR));

	free(iev_frontend);
	free(iev_engine);

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
	case PROC_ENGINE:
		argv[argc++] = "-E";
		break;
	case PROC_FRONTEND:
		argv[argc++] = "-F";
		break;
	}
	if (debug)
		argv[argc++] = "-d";
	if (verbose)
		argv[argc++] = "-v";
	if (verbose > 1)
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
	struct imsg_ifinfo	 imsg_ifinfo;
	ssize_t			 n;
	int			 shut = 0;
#ifndef	SMALL
	struct imsg_addrinfo	 imsg_addrinfo;
	struct imsg_link_state	 imsg_link_state;
	int			 verbose;
#endif	/* SMALL */

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
			if (pledge("stdio wroute", NULL) == -1)
				fatal("pledge");
			break;
#ifndef	SMALL
		case IMSG_CTL_LOG_VERBOSE:
			/* Already checked by frontend. */
			memcpy(&verbose, imsg.data, sizeof(verbose));
			log_setverbose(verbose);
			break;
		case IMSG_UPDATE_ADDRESS:
			if (imsg.hdr.len != IMSG_HEADER_SIZE +
			    sizeof(imsg_addrinfo))
				fatal("%s: IMSG_UPDATE_ADDRESS wrong length: "
				    "%d", __func__, imsg.hdr.len);
			memcpy(&imsg_addrinfo, imsg.data,
			    sizeof(imsg_addrinfo));
			main_imsg_compose_engine(IMSG_UPDATE_ADDRESS, 0,
			    &imsg_addrinfo, sizeof(imsg_addrinfo));
			break;
		case IMSG_UPDATE_LINK_STATE:
			if (imsg.hdr.len != IMSG_HEADER_SIZE +
			    sizeof(imsg_link_state))
				fatal("%s: IMSG_UPDATE_LINK_STATE wrong "
				    "length: %d", __func__, imsg.hdr.len);
			memcpy(&imsg_link_state, imsg.data,
			    sizeof(imsg_link_state));
			main_imsg_compose_engine(IMSG_UPDATE_LINK_STATE, 0,
			    &imsg_link_state, sizeof(imsg_link_state));
			break;
#endif	/* SMALL */
		case IMSG_UPDATE_IF:
			if (imsg.hdr.len != IMSG_HEADER_SIZE +
			    sizeof(imsg_ifinfo))
				fatal("%s: IMSG_UPDATE_IF wrong length: %d",
				    __func__, imsg.hdr.len);
			memcpy(&imsg_ifinfo, imsg.data, sizeof(imsg_ifinfo));
			if (get_soiikey(imsg_ifinfo.soiikey) == -1)
				log_warn("get_soiikey");
			else
				main_imsg_compose_engine(IMSG_UPDATE_IF, 0,
				    &imsg_ifinfo, sizeof(imsg_ifinfo));
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
main_dispatch_engine(int fd, short event, void *bula)
{
	struct imsgev			*iev = bula;
	struct imsgbuf			*ibuf;
	struct imsg			 imsg;
	struct imsg_proposal		 proposal;
	struct imsg_configure_address	 address;
	struct imsg_configure_dfr	 dfr;
	ssize_t				 n;
	int				 shut = 0;

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
		case IMSG_PROPOSAL:
			if (imsg.hdr.len != IMSG_HEADER_SIZE + sizeof(proposal))
				fatal("%s: IMSG_PROPOSAL wrong "
				    "length: %d", __func__, imsg.hdr.len);
			memcpy(&proposal, imsg.data, sizeof(proposal));
			handle_proposal(&proposal);
			break;
		case IMSG_CONFIGURE_ADDRESS:
			if (imsg.hdr.len != IMSG_HEADER_SIZE + sizeof(address))
				fatal("%s: IMSG_CONFIGURE_ADDRESS wrong "
				    "length: %d", __func__, imsg.hdr.len);
			memcpy(&address, imsg.data, sizeof(address));
			configure_interface(&address);
			break;
		case IMSG_CONFIGURE_DFR:
			if (imsg.hdr.len != IMSG_HEADER_SIZE + sizeof(dfr))
				fatal("%s: IMSG_CONFIGURE_DFR wrong "
				    "length: %d", __func__, imsg.hdr.len);
			memcpy(&dfr, imsg.data, sizeof(dfr));
			add_gateway(&dfr);
			break;
		case IMSG_WITHDRAW_DFR:
			if (imsg.hdr.len != IMSG_HEADER_SIZE + sizeof(dfr))
				fatal("%s: IMSG_WITHDRAW_DFR wrong "
				    "length: %d", __func__, imsg.hdr.len);
			memcpy(&dfr, imsg.data, sizeof(dfr));
			delete_gateway(&dfr);
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

int
main_imsg_compose_frontend(int type, pid_t pid, void *data, uint16_t datalen)
{
	if (iev_frontend)
		return (imsg_compose_event(iev_frontend, type, 0, pid, -1, data,
		    datalen));
	else
		return (-1);
}

int
main_imsg_compose_frontend_fd(int type, pid_t pid, int fd)
{
	if (iev_frontend)
		return (imsg_compose_event(iev_frontend, type, 0, pid, fd,
		    NULL, 0));
	else
		return (-1);
}

int
main_imsg_compose_engine(int type, pid_t pid, void *data, uint16_t datalen)
{
	if (iev_engine)
		return(imsg_compose_event(iev_engine, type, 0, pid, -1, data,
		    datalen));
	else
		return (-1);
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
    struct imsgbuf *engine_buf)
{
	int pipe_frontend2engine[2];

	if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK,
	    PF_UNSPEC, pipe_frontend2engine) == -1)
		return (-1);

	if (imsg_compose(frontend_buf, IMSG_SOCKET_IPC, 0, 0,
	    pipe_frontend2engine[0], NULL, 0) == -1)
		return (-1);
	imsg_flush(frontend_buf);
	if (imsg_compose(engine_buf, IMSG_SOCKET_IPC, 0, 0,
	    pipe_frontend2engine[1], NULL, 0) == -1)
		return (-1);
	imsg_flush(engine_buf);
	return (0);
}

#define	ROUNDUP(a)	\
    (((a) & (sizeof(long) - 1)) ? (1 + ((a) | (sizeof(long) - 1))) : (a))

void
handle_proposal(struct imsg_proposal *proposal)
{
	struct rt_msghdr		 rtm;
	struct sockaddr_in6		 ifa, mask;
	struct sockaddr_rtlabel		 rl;
	struct iovec			 iov[13];
	long				 pad = 0;
	int				 iovcnt = 0, padlen;

	memset(&rtm, 0, sizeof(rtm));

	rtm.rtm_version = RTM_VERSION;
	rtm.rtm_type = RTM_PROPOSAL;
	rtm.rtm_msglen = sizeof(rtm);
	rtm.rtm_tableid = 0; /* XXX imsg->rdomain; */
	rtm.rtm_index = proposal->if_index;
	rtm.rtm_seq = ++rtm_seq;
	rtm.rtm_priority = RTP_PROPOSAL_SLAAC;
	rtm.rtm_addrs = (proposal->rtm_addrs & (RTA_NETMASK | RTA_IFA)) |
	    RTA_LABEL;
	rtm.rtm_flags = RTF_UP;

	iov[iovcnt].iov_base = &rtm;
	iov[iovcnt++].iov_len = sizeof(rtm);

	if (rtm.rtm_addrs & RTA_NETMASK) {
		memset(&mask, 0, sizeof(mask));
		mask.sin6_family = AF_INET6;
		mask.sin6_len = sizeof(struct sockaddr_in6);
		mask.sin6_addr = proposal->mask;

		iov[iovcnt].iov_base = &mask;
		iov[iovcnt++].iov_len = sizeof(mask);
		rtm.rtm_msglen += sizeof(mask);
		padlen = ROUNDUP(sizeof(mask)) - sizeof(mask);
		if (padlen > 0) {
			iov[iovcnt].iov_base = &pad;
			iov[iovcnt++].iov_len = padlen;
			rtm.rtm_msglen += padlen;
		}
	}

	if (rtm.rtm_addrs & RTA_IFA) {
		memcpy(&ifa, &proposal->addr, sizeof(ifa));

		if (ifa.sin6_family != AF_INET6 || ifa.sin6_len !=
		    sizeof(struct sockaddr_in6)) {
			log_warnx("%s: invalid address", __func__);
			return;
		}

		iov[iovcnt].iov_base = &ifa;
		iov[iovcnt++].iov_len = sizeof(ifa);
		rtm.rtm_msglen += sizeof(ifa);
		padlen = ROUNDUP(sizeof(ifa)) - sizeof(ifa);
		if (padlen > 0) {
			iov[iovcnt].iov_base = &pad;
			iov[iovcnt++].iov_len = padlen;
			rtm.rtm_msglen += padlen;
		}
	}

	rl.sr_len = sizeof(rl);
	rl.sr_family = AF_UNSPEC;
	if (snprintf(rl.sr_label, sizeof(rl.sr_label), "%s: %lld %d",
	    SLAACD_RTA_LABEL, proposal->id, (int32_t)proposal->pid) >=
	    (ssize_t)sizeof(rl.sr_label))
		log_warnx("route label truncated");

	iov[iovcnt].iov_base = &rl;
	iov[iovcnt++].iov_len = sizeof(rl);
	rtm.rtm_msglen += sizeof(rl);
	padlen = ROUNDUP(sizeof(rl)) - sizeof(rl);
	if (padlen > 0) {
		iov[iovcnt].iov_base = &pad;
		iov[iovcnt++].iov_len = padlen;
		rtm.rtm_msglen += padlen;
	}

	if (writev(routesock, iov, iovcnt) == -1)
		log_warn("failed to send proposal");
}

void
configure_interface(struct imsg_configure_address *address)
{

	struct in6_aliasreq	 in6_addreq;
	time_t			 t;
	char			*if_name;

	memset(&in6_addreq, 0, sizeof(in6_addreq));

	if_name = if_indextoname(address->if_index, in6_addreq.ifra_name);
	if (if_name == NULL) {
		log_warnx("%s: cannot find interface %d", __func__,
		    address->if_index);
		return;
	}

	memcpy(&in6_addreq.ifra_addr, &address->addr,
	    sizeof(in6_addreq.ifra_addr));
	memcpy(&in6_addreq.ifra_prefixmask.sin6_addr, &address->mask,
	    sizeof(in6_addreq.ifra_prefixmask.sin6_addr));
	in6_addreq.ifra_prefixmask.sin6_family = AF_INET6;
	in6_addreq.ifra_prefixmask.sin6_len =
	    sizeof(in6_addreq.ifra_prefixmask);

	t = time(NULL);

	in6_addreq.ifra_lifetime.ia6t_expire = t + address->vltime;
	in6_addreq.ifra_lifetime.ia6t_vltime = address->vltime;

	in6_addreq.ifra_lifetime.ia6t_preferred = t + address->pltime;
	in6_addreq.ifra_lifetime.ia6t_pltime = address->pltime;

	in6_addreq.ifra_flags |= IN6_IFF_AUTOCONF;

	if (address->privacy)
		in6_addreq.ifra_flags |= IN6_IFF_PRIVACY;

	log_debug("%s: %s", __func__, if_name);

	if (ioctl(ioctl_sock, SIOCAIFADDR_IN6, &in6_addreq) < 0)
		fatal("SIOCAIFADDR_IN6");

	if (address->mtu) {
		struct ifreq	 ifr;

		(void)strlcpy(ifr.ifr_name, in6_addreq.ifra_name,
		    sizeof(ifr.ifr_name));
		ifr.ifr_mtu = address->mtu;
		log_debug("Setting MTU to %d", ifr.ifr_mtu);

		if (ioctl(ioctl_sock, SIOCSIFMTU, &ifr) < 0)
		    log_warn("failed to set MTU");
	}
}

void
configure_gateway(struct imsg_configure_dfr *dfr, uint8_t rtm_type)
{
	struct rt_msghdr		 rtm;
	struct sockaddr_rtlabel		 rl;
	struct sockaddr_in6		 dst, gw, mask;
	struct iovec			 iov[10];
	long				 pad = 0;
	int				 iovcnt = 0, padlen;

	memset(&rtm, 0, sizeof(rtm));

	rtm.rtm_version = RTM_VERSION;
	rtm.rtm_type = rtm_type;
	rtm.rtm_msglen = sizeof(rtm);
	rtm.rtm_tableid = 0; /* XXX imsg->rdomain; */
	rtm.rtm_index = dfr->if_index;
	rtm.rtm_seq = ++rtm_seq;
	rtm.rtm_priority = RTP_DEFAULT;
	rtm.rtm_addrs = RTA_DST | RTA_GATEWAY | RTA_NETMASK | RTA_LABEL;
	rtm.rtm_flags = RTF_UP | RTF_GATEWAY | RTF_STATIC | RTF_MPATH;

	iov[iovcnt].iov_base = &rtm;
	iov[iovcnt++].iov_len = sizeof(rtm);

	memset(&dst, 0, sizeof(mask));
	dst.sin6_family = AF_INET6;
	dst.sin6_len = sizeof(struct sockaddr_in6);

	iov[iovcnt].iov_base = &dst;
	iov[iovcnt++].iov_len = sizeof(dst);
	rtm.rtm_msglen += sizeof(dst);
	padlen = ROUNDUP(sizeof(dst)) - sizeof(dst);
	if (padlen > 0) {
		iov[iovcnt].iov_base = &pad;
		iov[iovcnt++].iov_len = padlen;
		rtm.rtm_msglen += padlen;
	}

	memcpy(&gw, &dfr->addr, sizeof(gw));
	/* from route(8) getaddr()*/
	*(u_int16_t *)& gw.sin6_addr.s6_addr[2] = htons(gw.sin6_scope_id);
	gw.sin6_scope_id = 0;
	iov[iovcnt].iov_base = &gw;
	iov[iovcnt++].iov_len = sizeof(gw);
	rtm.rtm_msglen += sizeof(gw);
	padlen = ROUNDUP(sizeof(gw)) - sizeof(gw);
	if (padlen > 0) {
		iov[iovcnt].iov_base = &pad;
		iov[iovcnt++].iov_len = padlen;
		rtm.rtm_msglen += padlen;
	}

	memset(&mask, 0, sizeof(mask));
	mask.sin6_family = AF_INET6;
	mask.sin6_len = sizeof(struct sockaddr_in6);
	iov[iovcnt].iov_base = &mask;
	iov[iovcnt++].iov_len = sizeof(mask);
	rtm.rtm_msglen += sizeof(mask);
	padlen = ROUNDUP(sizeof(mask)) - sizeof(mask);
	if (padlen > 0) {
		iov[iovcnt].iov_base = &pad;
		iov[iovcnt++].iov_len = padlen;
		rtm.rtm_msglen += padlen;
	}

	memset(&rl, 0, sizeof(rl));
	rl.sr_len = sizeof(rl);
	rl.sr_family = AF_UNSPEC;
	(void)snprintf(rl.sr_label, sizeof(rl.sr_label), "%s",
	    SLAACD_RTA_LABEL);
	iov[iovcnt].iov_base = &rl;
	iov[iovcnt++].iov_len = sizeof(rl);
	rtm.rtm_msglen += sizeof(rl);
	padlen = ROUNDUP(sizeof(rl)) - sizeof(rl);
	if (padlen > 0) {
		iov[iovcnt].iov_base = &pad;
		iov[iovcnt++].iov_len = padlen;
		rtm.rtm_msglen += padlen;
	}

	if (writev(routesock, iov, iovcnt) == -1)
		log_warn("failed to send route message");
}

void
add_gateway(struct imsg_configure_dfr *dfr)
{
	configure_gateway(dfr, RTM_ADD);
}

void
delete_gateway(struct imsg_configure_dfr *dfr)
{
	configure_gateway(dfr, RTM_DELETE);
}

#ifndef	SMALL
const char*
sin6_to_str(struct sockaddr_in6 *sin6)
{
	static char hbuf[NI_MAXHOST];
	int error;

	error = getnameinfo((struct sockaddr *)sin6, sin6->sin6_len, hbuf,
	    sizeof(hbuf), NULL, 0, NI_NUMERICHOST | NI_NUMERICSERV);
	if (error) {
		log_warnx("%s", gai_strerror(error));
		strlcpy(hbuf, "unknown", sizeof(hbuf));
	}
	return hbuf;
}
#endif	/* SMALL */

int
get_soiikey(uint8_t *key)
{
	int	 mib[4] = {CTL_NET, PF_INET6, IPPROTO_IPV6, IPV6CTL_SOIIKEY};
	size_t	 size = SLAACD_SOIIKEY_LEN;

	return sysctl(mib, sizeof(mib) / sizeof(mib[0]), key, &size, NULL, 0);
}
