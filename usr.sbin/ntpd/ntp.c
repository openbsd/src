/*	$OpenBSD: ntp.c,v 1.6 2004/06/02 10:08:59 henning Exp $ */

/*
 * Copyright (c) 2003, 2004 Henning Brauer <henning@openbsd.org>
 * Copyright (c) 2004 Alexander Guy <alexander.guy@andern.org>
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

#include <sys/param.h>
#include <errno.h>
#include <poll.h>
#include <pwd.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ntpd.h"
#include "ntp.h"

#define	PFD_PIPE_MAIN	0

volatile sig_atomic_t	 ntp_quit = 0;
struct imsgbuf		 ibuf_main;
struct l_fixedpt	 ref_ts;

void	ntp_sighdlr(int);
int	ntp_dispatch_imsg(void);
int	ntp_dispatch(int fd);

void
ntp_sighdlr(int sig)
{
	switch (sig) {
	case SIGINT:
	case SIGTERM:
		ntp_quit = 1;
		break;
	}
}

pid_t
ntp_main(int pipe_prnt[2], struct ntpd_conf *conf)
{
	int			 nfds, i, j;
	pid_t			 pid;
	struct pollfd		 pfd[OPEN_MAX];
	struct passwd		*pw;
	struct servent		*se;
	struct listen_addr	*la;

	switch (pid = fork()) {
	case -1:
		fatal("cannot fork");
	case 0:
		break;
	default:
		return (pid);
	}

	if ((se = getservbyname("ntp", "udp")) == NULL)
		fatal("getservbyname");

	if ((pw = getpwnam(NTPD_USER)) == NULL)
		fatal(NULL);

	if (chroot(pw->pw_dir) == -1)
		fatal("chroot");
	if (chdir("/") == -1)
		fatal("chdir(\"/\")");

	setproctitle("ntp engine");

	setup_listeners(se, conf);

	if (setgroups(1, &pw->pw_gid) ||
	    setegid(pw->pw_gid) || setgid(pw->pw_gid) ||
	    seteuid(pw->pw_uid) || setuid(pw->pw_uid))
		fatal("can't drop privileges");

	endpwent();
	endservent();

	signal(SIGTERM, ntp_sighdlr);
	signal(SIGINT, ntp_sighdlr);
	signal(SIGPIPE, SIG_IGN);

	close(pipe_prnt[0]);
	imsg_init(&ibuf_main, pipe_prnt[1]);

	log_info("ntp engine ready");

	while (ntp_quit == 0) {
		bzero(&pfd, sizeof(pfd));
		pfd[PFD_PIPE_MAIN].fd = ibuf_main.fd;
		pfd[PFD_PIPE_MAIN].events = POLLIN;
		if (ibuf_main.w.queued > 0)
			pfd[PFD_PIPE_MAIN].events |= POLLOUT;

		i = 1;
		TAILQ_FOREACH(la, &conf->listen_addrs, entry) {
			pfd[i].fd = la->fd;
			pfd[i].events = POLLIN;
			i++;
		}

		if ((nfds = poll(pfd, i, INFTIM)) == -1)
			if (errno != EINTR) {
				log_warn("poll error");
				ntp_quit = 1;
			}

		if (nfds > 0 && (pfd[PFD_PIPE_MAIN].revents & POLLOUT))
			if (msgbuf_write(&ibuf_main.w) < 0) {
				log_warn("pipe write error (to parent)");
				ntp_quit = 1;
			}

		if (nfds > 0 && pfd[PFD_PIPE_MAIN].revents & POLLIN) {
			nfds--;
			if (ntp_dispatch_imsg() == -1)
				ntp_quit = 1;
		}

		for (j = 1; nfds > 0 && j < i; j++)
			if (pfd[j].revents & POLLIN) {
				nfds--;
				if (ntp_dispatch(pfd[j].fd) == -1)
					ntp_quit = 1;
			}
	}

	msgbuf_write(&ibuf_main.w);
	msgbuf_clear(&ibuf_main.w);

	log_info("ntp engine exiting");
	_exit(0);
}

int
ntp_dispatch_imsg(void)
{
	struct imsg		 imsg;
	int			 n;

	if ((n = imsg_read(&ibuf_main)) == -1)
		return (-1);

	if (n == 0) {	/* connection closed */
		log_warnx("ntp_dispatch_imsg in ntp engine: pipe closed");
		return (-1);
	}

	for (;;) {
		if ((n = imsg_get(&ibuf_main, &imsg)) == -1)
			return (-1);

		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		default:
			break;
		}
		imsg_free(&imsg);
	}
	return (0);
}

int
ntp_dispatch(int fd)
{
	struct sockaddr_storage	 fsa;
	socklen_t		 fsa_len;
	char			 buf[NTP_MSGSIZE];
	ssize_t			 size;
	struct ntp_msg		 msg;

	fsa_len = sizeof(fsa);
	if ((size = recvfrom(fd, &buf, sizeof(buf), 0,
	    (struct sockaddr *)&fsa, &fsa_len)) == -1)
		fatal("recvfrom");

	ntp_getmsg(buf, size, &msg);
	ntp_reply(fd, (struct sockaddr *)&fsa, &msg, 0);

	return (0);
}
