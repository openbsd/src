/*	$OpenBSD: ntpd.c,v 1.18 2004/09/18 20:01:38 henning Exp $ */

/*
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
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER
 * IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ntpd.h"

void	sighdlr(int);
void	usage(void);
int	main(int, char *[]);
int	check_child(pid_t, const char *);
int	dispatch_imsg(struct ntpd_conf *);
void	ntpd_adjtime(double);
void	ntpd_settime(double);

volatile sig_atomic_t	 quit = 0;
volatile sig_atomic_t	 reconfig = 0;
volatile sig_atomic_t	 sigchld = 0;
struct imsgbuf		*ibuf;

void
sighdlr(int sig)
{
	switch (sig) {
	case SIGTERM:
	case SIGINT:
		quit = 1;
		break;
	case SIGCHLD:
		sigchld = 1;
		break;
	case SIGHUP:
		reconfig = 1;
		break;
	}
}

void
usage(void)
{
	extern char *__progname;

	fprintf(stderr, "usage: %s [-d] [-f file] [-s]\n", __progname);
	exit(1);
}

#define POLL_MAX		8
#define PFD_PIPE		0

int
main(int argc, char *argv[])
{
	struct ntpd_conf	 conf;
	struct pollfd		 pfd[POLL_MAX];
	pid_t			 chld_pid = 0, pid;
	char			*conffile;
	int			 ch, nfds, timeout = INFTIM;
	int			 pipe_chld[2];

	conffile = CONFFILE;

	bzero(&conf, sizeof(conf));

	log_init(1);		/* log to stderr until daemonized */

	while ((ch = getopt(argc, argv, "df:s")) != -1) {
		switch (ch) {
		case 'd':
			conf.debug = 1;
			break;
		case 'f':
			conffile = optarg;
			break;
		case 's':
			conf.settime = 1;
			break;
		default:
			usage();
			/* NOTREACHED */
		}
	}

	if (parse_config(conffile, &conf))
		exit(1);

	if (geteuid()) {
		fprintf(stderr, "ntpd: need root privileges\n");
		exit(1);
	}

	if (getpwnam(NTPD_USER) == NULL) {
		fprintf(stderr, "ntpd: unknown user %s\n", NTPD_USER);
		exit(1);
	}
	endpwent();

	if (!conf.settime) {
		log_init(conf.debug);
		if (!conf.debug)
			daemon(1, 0);
	} else
		timeout = 15 * 1000;

	if (socketpair(AF_UNIX, SOCK_STREAM, PF_UNSPEC, pipe_chld) == -1)
		fatal("socketpair");

	/* fork children */
	chld_pid = ntp_main(pipe_chld, &conf);

	setproctitle("[priv]");

	signal(SIGTERM, sighdlr);
	signal(SIGINT, sighdlr);
	signal(SIGCHLD, sighdlr);
	signal(SIGHUP, sighdlr);

	close(pipe_chld[1]);

	if ((ibuf = malloc(sizeof(struct imsgbuf))) == NULL)
		fatal(NULL);
	imsg_init(ibuf, pipe_chld[0]);

	while (quit == 0) {
		pfd[PFD_PIPE].fd = ibuf->fd;
		pfd[PFD_PIPE].events = POLLIN;
		if (ibuf->w.queued)
			pfd[PFD_PIPE].events |= POLLOUT;

		if ((nfds = poll(pfd, 1, timeout)) == -1)
			if (errno != EINTR) {
				log_warn("poll error");
				quit = 1;
			}

		if (nfds == 0 && conf.settime) {
			log_debug("no reply received, skipping initial time"
			    "setting");
			conf.settime = 0;
			timeout = INFTIM;
			log_init(conf.debug);
			if (!conf.debug)
				daemon(1, 0);
		}

		if (nfds > 0 && (pfd[PFD_PIPE].revents & POLLOUT))
			if (msgbuf_write(&ibuf->w) < 0) {
				log_warn("pipe write error (to child");
				quit = 1;
			}

		if (nfds > 0 && pfd[PFD_PIPE].revents & POLLIN) {
			nfds--;
			if (dispatch_imsg(&conf) == -1)
				quit = 1;
		}

		if (sigchld) {
			if (check_child(chld_pid, "child"))
				quit = 1;
			sigchld = 0;
		}

	}

	signal(SIGCHLD, SIG_IGN);

	if (chld_pid)
		kill(chld_pid, SIGTERM);

	do {
		if ((pid = wait(NULL)) == -1 &&
		    errno != EINTR && errno != ECHILD)
			fatal("wait");
	} while (pid != -1 || (pid == -1 && errno == EINTR));

	msgbuf_clear(&ibuf->w);
	free(ibuf);
	log_info("Terminating");
	return (0);
}

int
check_child(pid_t pid, const char *pname)
{
	int	status;

	if (waitpid(pid, &status, WNOHANG) > 0) {
		if (WIFEXITED(status)) {
			log_warnx("Lost child: %s exited", pname);
			return (1);
		}
		if (WIFSIGNALED(status)) {
			log_warnx("Lost child: %s terminated; signal %d",
			    pname, WTERMSIG(status));
			return (1);
		}
	}

	return (0);
}

int
dispatch_imsg(struct ntpd_conf *conf)
{
	struct imsg		 imsg;
	int			 n, cnt;
	double			 d;
	char			*name;
	struct ntp_addr		*h, *hn;
	struct buf		*buf;

	if ((n = imsg_read(ibuf)) == -1)
		return (-1);

	if (n == 0) {	/* connection closed */
		log_warnx("dispatch_imsg in main: pipe closed");
		return (-1);
	}

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			return (-1);

		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_ADJTIME:
			if (imsg.hdr.len != IMSG_HEADER_SIZE + sizeof(d))
				fatal("invalid IMSG_ADJTIME received");
			memcpy(&d, imsg.data, sizeof(d));
			ntpd_adjtime(d);
			break;
		case IMSG_SETTIME:
			if (imsg.hdr.len != IMSG_HEADER_SIZE + sizeof(d))
				fatal("invalid IMSG_SETTIME received");
			if (!conf->settime)
				break;
			memcpy(&d, imsg.data, sizeof(d));
			ntpd_settime(d);
			/* daemonize now */
			log_init(conf->debug);
			if (!conf->debug)
				daemon(1, 0);
			conf->settime = 0;
			break;
		case IMSG_HOST_DNS:
			name = imsg.data;
			if (imsg.hdr.len != strlen(name) + 1 + IMSG_HEADER_SIZE)
				fatal("invalid IMSG_HOST_DNS received");
			if ((cnt = host_dns(name, &hn)) > 0) {
				buf = imsg_create(ibuf, IMSG_HOST_DNS,
				    imsg.hdr.peerid, 0,
				    cnt * sizeof(struct sockaddr_storage));
				if (buf == NULL)
					break;
				for (h = hn; h != NULL; h = h->next) {
					imsg_add(buf, &h->ss, sizeof(h->ss));
				}
				imsg_close(ibuf, buf);
			}
			break;
		default:
			break;
		}
		imsg_free(&imsg);
	}
	return (0);
}

void
ntpd_adjtime(double d)
{
	struct timeval	tv;

	d_to_tv(d, &tv);
	log_info("adjusting local clock by %fs", d);
	if (adjtime(&tv, NULL) == -1)
		log_warn("adjtime failed");
}

void
ntpd_settime(double d)
{
	struct timeval	tv, curtime;
	char		buf[80];
	time_t		tval;

	d_to_tv(d, &tv);
	if (gettimeofday(&curtime, NULL) == -1)
		log_warn("gettimeofday");
	curtime.tv_sec += tv.tv_sec;
	curtime.tv_usec += tv.tv_usec;
	if (curtime.tv_usec > 1000000) {
		curtime.tv_sec++;
		curtime.tv_usec -= 1000000;
	}
	if (settimeofday(&curtime, NULL) == -1)
		log_warn("settimeofday");
	tval = curtime.tv_sec;
	strftime(buf, sizeof(buf), "%a %b %e %H:%M:%S %Z %Y",
	    localtime(&tval));
	log_info("set local clock to %s (offset %fs)", buf, d);
}
