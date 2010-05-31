/*	$OpenBSD: ldapd.c,v 1.2 2010/05/31 18:29:04 martinh Exp $ */

/*
 * Copyright (c) 2009, 2010 Martin Hedenfalk <martin@bzero.se>
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

#include <sys/queue.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <assert.h>
#include <bsd_auth.h>
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <event.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "ldapd.h"

void			 usage(void);
void			 ldapd_sig_handler(int fd, short why, void *data);
void			 ldapd_sigchld_handler(int sig, short why, void *data);
void			 ldapd_dispatch_ldape(int fd, short event, void *ptr);

struct ldapd_stats	 stats;
pid_t			 ldape_pid;

void
usage(void)
{
	extern char	*__progname;

	fprintf(stderr, "usage: %s [-dnv] [-D macro=value] "
	    "[-f file] [-s file]\n", __progname);
	exit(1);
}

void
ldapd_sig_handler(int sig, short why, void *data)
{
	log_info("ldapd: got signal %d", sig);
	if (sig == SIGINT || sig == SIGTERM)
		event_loopexit(NULL);
}

void
ldapd_sigchld_handler(int sig, short why, void *data)
{
	pid_t		 pid;
	int		 status;

	while ((pid = waitpid(WAIT_ANY, &status, WNOHANG)) != 0) {
		if (pid == -1) {
			if (errno == EINTR)
				continue;
			if (errno != ECHILD)
				log_warn("waitpid");
			break;
		}

		if (WIFEXITED(status))
			log_debug("child %d exited with status %d",
			    pid, WEXITSTATUS(status));
		else if (WIFSIGNALED(status))
			log_debug("child %d exited due to signal %d",
			    pid, WTERMSIG(status));
		else
			log_debug("child %d terminated abnormally", pid);

		if (pid == ldape_pid) {
			log_info("ldapd: lost ldap server");
			event_loopexit(NULL);
			break;
		}
	}
}

/* set socket non-blocking */
void
fd_nonblock(int fd)
{
	int flags = fcntl(fd, F_GETFL, 0);
	int rc = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
	if (rc == -1) {
		log_warn("failed to set fd %i non-blocking", fd);
	}
}


int
main(int argc, char *argv[])
{
	int			 c;
	int			 debug = 0, verbose = 0;
	int			 configtest = 0, skip_chroot = 0;
	int			 pipe_parent2ldap[2];
	char			*conffile = CONFFILE;
	char			*csockpath = LDAPD_SOCKET;
	struct passwd		*pw = NULL;
	struct imsgev		*iev_ldape;
	struct event		 ev_sigint;
	struct event		 ev_sigterm;
	struct event		 ev_sigchld;
	struct event		 ev_sighup;

	log_init(1);		/* log to stderr until daemonized */

	while ((c = getopt(argc, argv, "dhvD:f:ns:")) != -1) {
		switch (c) {
		case 'd':
			debug = 1;
			break;
		case 'D':
			if (cmdline_symset(optarg) < 0) {
				warnx("could not parse macro definition %s",
				    optarg);
			}
			break;
		case 'f':
			conffile = optarg;
			break;
		case 'h':
			usage();
			/* NOTREACHED */
		case 'n':
			configtest = 1;
			break;
		case 's':
			csockpath = optarg;
			break;
		case 'v':
			verbose = 1;
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

	log_verbose(verbose);
	stats.started_at = time(0);
	ssl_init();

	if (parse_config(conffile) != 0)
		exit(2);

	if (configtest) {
		fprintf(stderr, "configuration ok\n");
		exit(0);
	}

	if (geteuid()) {
		if (!debug)
			errx(1, "need root privileges");
		skip_chroot = 1;
	}

	if (!skip_chroot && (pw = getpwnam(LDAPD_USER)) == NULL)
		err(1, "%s", LDAPD_USER);

	if (!debug) {
		if (daemon(1, 0) == -1)
			err(1, "failed to daemonize");
	}

	log_init(debug);
	log_info("startup");

	if (socketpair(AF_UNIX, SOCK_STREAM, PF_UNSPEC, pipe_parent2ldap) != 0)
		fatal("socketpair");

	fd_nonblock(pipe_parent2ldap[0]);
	fd_nonblock(pipe_parent2ldap[1]);

	ldape_pid = ldape(pw, csockpath, pipe_parent2ldap);

	setproctitle("auth");
	event_init();

	signal_set(&ev_sigint, SIGINT, ldapd_sig_handler, NULL);
	signal_set(&ev_sigterm, SIGTERM, ldapd_sig_handler, NULL);
	signal_set(&ev_sigchld, SIGCHLD, ldapd_sigchld_handler, NULL);
	signal_set(&ev_sighup, SIGHUP, ldapd_sig_handler, NULL);
	signal_add(&ev_sigint, NULL);
	signal_add(&ev_sigterm, NULL);
	signal_add(&ev_sigchld, NULL);
	signal_add(&ev_sighup, NULL);
	signal(SIGPIPE, SIG_IGN);

	close(pipe_parent2ldap[1]);

	if ((iev_ldape = calloc(1, sizeof(struct imsgev))) == NULL)
		fatal("calloc");
	imsg_init(&iev_ldape->ibuf, pipe_parent2ldap[0]);
	iev_ldape->handler = ldapd_dispatch_ldape;
	imsg_event_add(iev_ldape);

	event_dispatch();
	log_debug("ldapd: exiting");

	return 0;
}

void
imsg_event_add(struct imsgev *iev)
{
	iev->events = EV_READ;
	if (iev->ibuf.w.queued)
		iev->events |= EV_WRITE;

	if (event_initialized(&iev->ev))
		event_del(&iev->ev);
	event_set(&iev->ev, iev->ibuf.fd, iev->events, iev->handler, iev);
	event_add(&iev->ev, NULL);
}

int
imsg_compose_event(struct imsgev *iev, u_int16_t type,
    u_int32_t peerid, pid_t pid, int fd, void *data, u_int16_t datalen)
{
	int     ret;

	if ((ret = imsg_compose(&iev->ibuf, type, peerid,
	    pid, fd, data, datalen)) != -1)
		imsg_event_add(iev);
	return (ret);
}

int
imsg_event_handle(struct imsgev *iev, short event)
{
	ssize_t			 n;

	switch (event) {
	case EV_READ:
		if ((n = imsg_read(&iev->ibuf)) == -1)
			fatal("imsg_read error");
		if (n == 0) {
			/* this pipe is dead, so remove the event handler */
			event_del(&iev->ev);
			event_loopexit(NULL);
			return -1;
		}
		break;
	case EV_WRITE:
		if (msgbuf_write(&iev->ibuf.w) == -1)
			fatal("msgbuf_write");
		imsg_event_add(iev);
		return 1;
	default:
		fatalx("unknown event");
	}

	return 0;
}

void
ldapd_dispatch_ldape(int fd, short event, void *ptr)
{
	struct imsgev		*iev = ptr;
	struct imsgbuf		*ibuf;
	struct imsg		 imsg;
	ssize_t			 n;
	int			 verbose;

	if (imsg_event_handle(iev, event) != 0)
		return;

	ibuf = &iev->ibuf;
	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatal("ldapd_dispatch_ldape: imsg_read error");
		if (n == 0)
			break;

		log_debug("ldapd_dispatch_ldape: imsg type %u", imsg.hdr.type);

		switch (imsg.hdr.type) {
		case IMSG_LDAPD_AUTH: {
			struct auth_req		*areq = imsg.data;
			struct auth_res		 ares;

			log_debug("authenticating [%s]", areq->name);
			ares.ok = auth_userokay(areq->name, NULL, "auth-ldap",
			    areq->password);
			ares.fd = areq->fd;
			ares.msgid = areq->msgid;
			bzero(areq, sizeof(*areq));
			imsg_compose(ibuf, IMSG_LDAPD_AUTH_RESULT, 0, 0, -1,
			    &ares, sizeof(ares));
			imsg_event_add(iev);
			break;
		}
		case IMSG_CTL_LOG_VERBOSE:
			memcpy(&verbose, imsg.data, sizeof(verbose));
			log_verbose(verbose);
			break;
		default:
			log_debug("ldapd_dispatch_ldape: unexpected imsg %d",
			    imsg.hdr.type);
			break;
		}
		imsg_free(&imsg);
	}
	imsg_event_add(iev);
}

