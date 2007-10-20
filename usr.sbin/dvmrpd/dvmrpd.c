/*	$OpenBSD: dvmrpd.c,v 1.5 2007/10/20 13:26:50 pyr Exp $ */

/*
 * Copyright (c) 2005 Claudio Jeker <claudio@openbsd.org>
 * Copyright (c) 2005, 2006 Esben Norby <norby@openbsd.org>
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
#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/wait.h>

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
#include <util.h>

#include "igmp.h"
#include "dvmrpd.h"
#include "dvmrp.h"
#include "dvmrpe.h"
#include "control.h"
#include "log.h"
#include "rde.h"

void		main_sig_handler(int, short, void *);
__dead void	usage(void);
void		dvmrpd_shutdown(void);
int		check_child(pid_t, const char *);

void	main_dispatch_dvmrpe(int, short, void *);
void	main_dispatch_rde(int, short, void *);
void	main_imsg_compose_dvmrpe(int, pid_t, void *, u_int16_t);
void	main_imsg_compose_rde(int, pid_t, void *, u_int16_t);

int	pipe_parent2dvmrpe[2];
int	pipe_parent2rde[2];
int	pipe_dvmrpe2rde[2];

struct dvmrpd_conf	*conf = NULL;
struct imsgbuf		*ibuf_dvmrpe;
struct imsgbuf		*ibuf_rde;

pid_t			 dvmrpe_pid;
pid_t			 rde_pid;

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
		if (check_child(dvmrpe_pid, "dvmrp engine")) {
			dvmrpe_pid = 0;
			die = 1;
		}
		if (check_child(rde_pid, "route decision engine")) {
			rde_pid = 0;
			die = 1;
		}
		if (die)
			dvmrpd_shutdown();
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

__dead void
usage(void)
{
	extern char *__progname;

	fprintf(stderr, "usage: %s [-dnv] [-f file]\n", __progname);
	exit(1);
}

int
main(int argc, char *argv[])
{
	struct event	 ev_sigint, ev_sigterm, ev_sigchld, ev_sighup;
	char		*conffile;
	int		 ch, opts = 0;
	int		 debug = 0;
	int		 ipmforwarding;
	int		 mib[4];
	size_t		 len;

	conffile = CONF_FILE;
	dvmrpd_process = PROC_MAIN;

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
			opts |= DVMRPD_OPT_NOACTION;
			break;
		case 'v':
			if (opts & DVMRPD_OPT_VERBOSE)
				opts |= DVMRPD_OPT_VERBOSE2;
			opts |= DVMRPD_OPT_VERBOSE;
			break;
		default:
			usage();
			/* NOTREACHED */
		}
	}

	log_init(debug);

	/* multicast IP forwarding must be enabled */
	mib[0] = CTL_NET;
	mib[1] = PF_INET;
	mib[2] = IPPROTO_IP;
	mib[3] = IPCTL_MFORWARDING;
	len = sizeof(ipmforwarding);
	if (sysctl(mib, 4, &ipmforwarding, &len, NULL, 0) == -1)
		err(1, "sysctl");

	if (!ipmforwarding)
		errx(1, "multicast IP forwarding not enabled");

	/* fetch interfaces early */
	kif_init();

	/* parse config file */
	if ((conf = parse_config(conffile, opts)) == NULL )
		exit(1);

	if (conf->opts & DVMRPD_OPT_NOACTION) {
		if (conf->opts & DVMRPD_OPT_VERBOSE)
			print_config(conf);
		else
			fprintf(stderr, "configuration OK\n");
		exit(0);
	}

	/* check for root privileges  */
	if (geteuid())
		errx(1, "need root privileges");

	/* check for dvmrpd user */
	if (getpwnam(DVMRPD_USER) == NULL)
		errx(1, "unknown user %s", DVMRPD_USER);

	endpwent();

	/* start logging */
	log_init(1);

	if (!debug)
		daemon(1, 0);

	log_info("startup");

	if (socketpair(AF_UNIX, SOCK_STREAM, PF_UNSPEC,
	    pipe_parent2dvmrpe) == -1)
		fatal("socketpair");
	if (socketpair(AF_UNIX, SOCK_STREAM, PF_UNSPEC, pipe_parent2rde) == -1)
		fatal("socketpair");
	if (socketpair(AF_UNIX, SOCK_STREAM, PF_UNSPEC, pipe_dvmrpe2rde) == -1)
		fatal("socketpair");
	session_socket_blockmode(pipe_parent2dvmrpe[0], BM_NONBLOCK);
	session_socket_blockmode(pipe_parent2dvmrpe[1], BM_NONBLOCK);
	session_socket_blockmode(pipe_parent2rde[0], BM_NONBLOCK);
	session_socket_blockmode(pipe_parent2rde[1], BM_NONBLOCK);
	session_socket_blockmode(pipe_dvmrpe2rde[0], BM_NONBLOCK);
	session_socket_blockmode(pipe_dvmrpe2rde[1], BM_NONBLOCK);

	/* start children */
	rde_pid = rde(conf, pipe_parent2rde, pipe_dvmrpe2rde,
	    pipe_parent2dvmrpe);
	dvmrpe_pid = dvmrpe(conf, pipe_parent2dvmrpe, pipe_dvmrpe2rde,
	    pipe_parent2rde);

	/* create the raw ip socket */
	if ((conf->mroute_socket = socket(AF_INET, SOCK_RAW,
	    IPPROTO_IGMP)) == -1)
		fatal("error creating raw socket");

	if_set_recvbuf(conf->mroute_socket);

	if (mrt_init(conf->mroute_socket))
		fatal("multicast routing not enabled in kernel");

	/* show who we are */
	setproctitle("parent");

	event_init();

	/* setup signal handler */
	signal_set(&ev_sigint, SIGINT, main_sig_handler, NULL);
	signal_set(&ev_sigterm, SIGTERM, main_sig_handler, NULL);
	signal_set(&ev_sigchld, SIGINT, main_sig_handler, NULL);
	signal_set(&ev_sighup, SIGTERM, main_sig_handler, NULL);
	signal_add(&ev_sigint, NULL);
	signal_add(&ev_sigterm, NULL);
	signal_add(&ev_sigchld, NULL);
	signal_add(&ev_sighup, NULL);
	signal(SIGPIPE, SIG_IGN);

	/* setup pipes to children */
	close(pipe_parent2dvmrpe[1]);
	close(pipe_parent2rde[1]);
	close(pipe_dvmrpe2rde[0]);
	close(pipe_dvmrpe2rde[1]);

	if ((ibuf_dvmrpe = malloc(sizeof(struct imsgbuf))) == NULL ||
	    (ibuf_rde = malloc(sizeof(struct imsgbuf))) == NULL)
		fatal(NULL);
	imsg_init(ibuf_dvmrpe, pipe_parent2dvmrpe[0], main_dispatch_dvmrpe);
	imsg_init(ibuf_rde, pipe_parent2rde[0], main_dispatch_rde);

	/* setup event handler */
	ibuf_dvmrpe->events = EV_READ;
	event_set(&ibuf_dvmrpe->ev, ibuf_dvmrpe->fd, ibuf_dvmrpe->events,
	    ibuf_dvmrpe->handler, ibuf_dvmrpe);
	event_add(&ibuf_dvmrpe->ev, NULL);

	ibuf_rde->events = EV_READ;
	event_set(&ibuf_rde->ev, ibuf_rde->fd, ibuf_rde->events,
	    ibuf_rde->handler, ibuf_rde);
	event_add(&ibuf_rde->ev, NULL);

	if (kmr_init(!(conf->flags & DVMRPD_FLAG_NO_FIB_UPDATE)) == -1)
		dvmrpd_shutdown();
	if (kr_init() == -1)
		dvmrpd_shutdown();

	event_set(&conf->ev, conf->mroute_socket, EV_READ|EV_PERSIST,
	    kmr_recv_msg, conf);
	event_add(&conf->ev, NULL);

	event_dispatch();

	dvmrpd_shutdown();
	/* NOTREACHED */
	return (0);
}

void
dvmrpd_shutdown(void)
{
	struct iface	*iface;
	pid_t		 pid;

	if (dvmrpe_pid)
		kill(dvmrpe_pid, SIGTERM);

	if (rde_pid)
		kill(rde_pid, SIGTERM);

	control_cleanup();
	kmr_shutdown();
	kr_shutdown();

	LIST_FOREACH(iface, &conf->iface_list, entry) {
		if_del(iface);
	}

	mrt_done(conf->mroute_socket);

	do {
		if ((pid = wait(NULL)) == -1 &&
		    errno != EINTR && errno != ECHILD)
			fatal("wait");
	} while (pid != -1 || (pid == -1 && errno == EINTR));

	msgbuf_clear(&ibuf_dvmrpe->w);
	free(ibuf_dvmrpe);
	msgbuf_clear(&ibuf_rde->w);
	free(ibuf_rde);

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
void
main_dispatch_dvmrpe(int fd, short event, void *bula)
{
	struct imsgbuf  *ibuf = bula;
	struct imsg	 imsg;
	int		 n;

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
			log_debug("main_dispatch_dvmrpe: IMSG_CTL_RELOAD");
			/* reconfig */
			break;
		case IMSG_CTL_MFC_COUPLE:
			kmr_mfc_couple();
			break;
		case IMSG_CTL_MFC_DECOUPLE:
			kmr_mfc_decouple();
			break;
		default:
			log_debug("main_dispatch_dvmrpe: error handling "
			    "imsg %d", imsg.hdr.type);
			break;
		}
		imsg_free(&imsg);
	}
	imsg_event_add(ibuf);
}

void
main_dispatch_rde(int fd, short event, void *bula)
{
	struct mfc	 mfc;
	struct imsgbuf  *ibuf = bula;
	struct imsg	 imsg;
	int		 n;

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
		case IMSG_MFC_ADD:
			if (imsg.hdr.len - IMSG_HEADER_SIZE != sizeof(mfc))
				fatalx("invalid size of RDE request");
			memcpy(&mfc, imsg.data, sizeof(mfc));

			/* add to MFC */
			mrt_add_mfc(conf->mroute_socket, &mfc);
			break;
		case IMSG_MFC_DEL:
			if (imsg.hdr.len - IMSG_HEADER_SIZE != sizeof(mfc))
				fatalx("invalid size of RDE request");
			memcpy(&mfc, imsg.data, sizeof(mfc));

			/* remove from MFC */
			mrt_del_mfc(conf->mroute_socket, &mfc);
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
main_imsg_compose_dvmrpe(int type, pid_t pid, void *data, u_int16_t datalen)
{
	imsg_compose(ibuf_dvmrpe, type, 0, pid, data, datalen);
}

void
main_imsg_compose_rde(int type, pid_t pid, void *data, u_int16_t datalen)
{
	imsg_compose(ibuf_rde, type, 0, pid, data, datalen);
}

/* this needs to be added here so that dvmrpctl can be used without libevent */
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
