/*	$OpenBSD: ospfd.c,v 1.4 2005/02/07 05:51:00 david Exp $ */

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

#include <netinet/in.h>
#include <arpa/inet.h>

#include <event.h>
#include <err.h>
#include <errno.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <util.h>

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
void	main_imsg_compose_ospfe(int, pid_t, void *, u_int16_t);
void	main_imsg_compose_rde(int, pid_t, void *, u_int16_t);

int	check_file_secrecy(int, const char *);

int	pipe_parent2ospfe[2];
int	pipe_parent2rde[2];
int	pipe_ospfe2rde[2];

volatile sig_atomic_t	 main_quit = 0;

struct ospfd_conf	*conf = NULL;
struct imsgbuf		*ibuf_ospfe;
struct imsgbuf		*ibuf_rde;

pid_t			 ospfe_pid;
pid_t			 rde_pid;

void
main_sig_handler(int sig, short event, void *arg)
{
	/*
	 * signal handler rules don't apply, libevent decouples for us
	 */

	switch (sig) {
	case SIGTERM:
	case SIGINT:
		ospfd_shutdown();
		/* NOTREACHED */
	case SIGCHLD:
		if (check_child(ospfe_pid, "ospfe engine")) {
			ospfe_pid = 0;
			ospfd_shutdown();
		}
		if (check_child(rde_pid, "route decision engine")) {
			rde_pid = 0;
			ospfd_shutdown();
		}
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

	fprintf(stderr, "usage: %s [-d] [-f file]\n", __progname);
	exit(1);
}

int
main(int argc, char *argv[])
{
	struct event		 ev_sigint, ev_sigterm, ev_sigchld, ev_sighup;
	char			*conffile;
	int			 ch;
	int			 debug = 0;

	conffile = CONF_FILE;
	ospfd_process = PROC_MAIN;

	/* start logging */
	log_init(1);

	while ((ch = getopt(argc, argv, "dhf:")) != -1) {
		switch (ch) {
		case 'd':
			debug = 1;
			break;
		case 'f':
			conffile = optarg;
			break;
		default:
			usage();
			/* NOTREACHED */
		}
	}

	log_init(debug);

	/* parse config file */
	if ((conf = parse_config(conffile, OSPFD_OPT_VERBOSE)) == NULL )
		exit(1);

	/* check for root privileges  */
	if (geteuid())
		errx(1, "need root privileges");

	/* check for ospfd user */
	if (getpwnam(OSPFD_USER) == NULL)
		errx(1, "unknown user %s", OSPFD_USER);

	endpwent();

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

	event_init();

	if (if_init(conf))
		log_info("error initializing interfaces");
	else
		log_debug("interfaces initialized");

	/* start children */
	rde_pid = rde(conf, pipe_parent2rde, pipe_ospfe2rde, pipe_parent2ospfe);
	ospfe_pid = ospfe(conf, pipe_parent2ospfe, pipe_ospfe2rde,
	    pipe_parent2rde);

	/* show who we are */
	setproctitle("parent");

	/* setup signal handler */
	signal_set(&ev_sigint, SIGINT, main_sig_handler, NULL);
	signal_set(&ev_sigterm, SIGTERM, main_sig_handler, NULL);
	signal_set(&ev_sigchld, SIGINT, main_sig_handler, NULL);
	signal_set(&ev_sighup, SIGTERM, main_sig_handler, NULL);
	signal_add(&ev_sigint, NULL);
	signal_add(&ev_sigterm, NULL);
	signal_add(&ev_sigchld, NULL);
	signal_add(&ev_sighup, NULL);

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

	if (kr_init(!(conf->flags & OSPFD_FLAG_NO_FIB_UPDATE)) == -1)
		ospfd_shutdown();

	show_config(conf);

	event_dispatch();

	ospfd_shutdown();
	/* NOTREACHED */
	return (0);
}

void
ospfd_shutdown(void)
{
	pid_t	pid;

	if (ospfe_pid)
		kill(ospfe_pid, SIGTERM);

	if (rde_pid)
		kill(rde_pid, SIGTERM);

	control_cleanup();
	kr_shutdown();
	if_shutdown(conf);

	do {
		if ((pid = wait(NULL)) == -1 &&
		    errno != EINTR && errno != ECHILD)
			fatal("wait");
	} while (pid != -1 || (pid == -1 && errno == EINTR));

	msgbuf_clear(&ibuf_ospfe->w);
	free(ibuf_ospfe);
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
main_dispatch_ospfe(int fd, short event, void *bula)
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
			log_debug("main_dispatch_ospfe: IMSG_CTL_RELOAD");
			/* reconfig */
			break;
		case IMSG_CTL_SHOW_INTERFACE:
			kr_show_route(&imsg);
			break;
		case IMSG_CTL_FIB_COUPLE:
			kr_fib_couple();
			break;
		case IMSG_CTL_FIB_DECOUPLE:
			kr_fib_decouple();
			break;
		default:
			log_debug("main_dispatch_ospfe: error handling imsg %d",
			    imsg.hdr.type);
			break;
		}
		imsg_free(&imsg);
	}
	imsg_event_add(ibuf);
}

void
main_dispatch_rde(int fd, short event, void *bula)
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
main_imsg_compose_ospfe(int type, pid_t pid, void *data, u_int16_t datalen)
{

	imsg_compose(ibuf_ospfe, type, 0, pid, -1, data, datalen);
}

void
main_imsg_compose_rde(int type, pid_t pid, void *data, u_int16_t datalen)
{

	imsg_compose(ibuf_rde, type, 0, pid, -1, data, datalen);
}


void
send_nexthop_update(struct kroute_nexthop *msg)
{
	char	*gw = NULL;

	if (msg->gateway.s_addr)
		if (asprintf(&gw, ": via %s",
		    inet_ntoa(msg->gateway)) == -1) {
			log_warn("send_nexthop_update");
			main_quit = 1;
		}

	log_info("nexthop %s now %s%s%s", inet_ntoa(msg->nexthop),
	    msg->valid ? "valid" : "invalid",
	    msg->connected ? ": directly connected" : "",
	    msg->gateway.s_addr ? gw : "");

	free(gw);

	if (imsg_compose(ibuf_rde, IMSG_NEXTHOP_UPDATE, 0, 0, -1,
	    msg, sizeof(struct kroute_nexthop)) == -1)
		main_quit = 1;
}

int
check_file_secrecy(int fd, const char *fname)
{
	struct stat	st;

	if (fstat(fd, &st)) {
		log_warn("cannot stat %s", fname);
		return (-1);
	}

	if (st.st_uid != 0 && st.st_uid != getuid()) {
		log_warnx("%s: owner not root or current user", fname);
		return (-1);
	}

	if (st.st_mode & (S_IRWXG | S_IRWXO)) {
		log_warnx("%s: group/world readable/writeable", fname);
		return (-1);
	}

	return (0);
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
