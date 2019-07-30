/*	$OpenBSD: ntp_dns.c,v 1.20 2017/04/17 16:03:15 otto Exp $ */

/*
 * Copyright (c) 2003-2008 Henning Brauer <henning@openbsd.org>
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
#include <sys/resource.h>
#include <sys/time.h>

#include <err.h>
#include <errno.h>
#include <poll.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include "ntpd.h"

volatile sig_atomic_t	 quit_dns = 0;
struct imsgbuf		*ibuf_dns;

void	sighdlr_dns(int);
int	dns_dispatch_imsg(void);

void
sighdlr_dns(int sig)
{
	switch (sig) {
	case SIGTERM:
	case SIGINT:
		quit_dns = 1;
		break;
	}
}

void
ntp_dns(struct ntpd_conf *nconf, struct passwd *pw)
{
	struct pollfd		 pfd[1];
	int			 nfds, nullfd;

	if (setpriority(PRIO_PROCESS, 0, 0) == -1)
		log_warn("could not set priority");

	/* in this case the parent didn't init logging and didn't daemonize */
	if (nconf->settime && !nconf->debug) {
		log_init(nconf->debug, LOG_DAEMON);
		if (setsid() == -1)
			fatal("setsid");
	}
	log_procinit("dns");

	if ((nullfd = open("/dev/null", O_RDWR, 0)) == -1)
		fatal(NULL);

	if (!nconf->debug) {
		dup2(nullfd, STDIN_FILENO);
		dup2(nullfd, STDOUT_FILENO);
		dup2(nullfd, STDERR_FILENO);
	}
	close(nullfd);

	setproctitle("dns engine");

	if (setgroups(1, &pw->pw_gid) ||
	    setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) ||
	    setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid))
		fatal("can't drop privileges");

	signal(SIGTERM, sighdlr_dns);
	signal(SIGINT, sighdlr_dns);
	signal(SIGHUP, SIG_IGN);

	if ((ibuf_dns = malloc(sizeof(struct imsgbuf))) == NULL)
		fatal(NULL);
	imsg_init(ibuf_dns, PARENT_SOCK_FILENO);

	if (pledge("stdio dns", NULL) == -1)
		err(1, "pledge");

	while (quit_dns == 0) {
		pfd[0].fd = ibuf_dns->fd;
		pfd[0].events = POLLIN;
		if (ibuf_dns->w.queued)
			pfd[0].events |= POLLOUT;

		if ((nfds = poll(pfd, 1, INFTIM)) == -1)
			if (errno != EINTR) {
				log_warn("poll error");
				quit_dns = 1;
			}

		if (nfds > 0 && (pfd[0].revents & POLLOUT))
			if (msgbuf_write(&ibuf_dns->w) <= 0 &&
			    errno != EAGAIN) {
				log_warn("pipe write error (to ntp engine)");
				quit_dns = 1;
			}

		if (nfds > 0 && pfd[0].revents & POLLIN) {
			nfds--;
			if (dns_dispatch_imsg() == -1)
				quit_dns = 1;
		}
	}

	msgbuf_clear(&ibuf_dns->w);
	free(ibuf_dns);
	exit(0);
}

int
dns_dispatch_imsg(void)
{
	struct imsg		 imsg;
	int			 n, cnt;
	char			*name;
	struct ntp_addr		*h, *hn;
	struct ibuf		*buf;
	const char		*str;
	size_t			 len;

	if (((n = imsg_read(ibuf_dns)) == -1 && errno != EAGAIN) || n == 0)
		return (-1);

	for (;;) {
		if ((n = imsg_get(ibuf_dns, &imsg)) == -1)
			return (-1);

		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_HOST_DNS:
		case IMSG_CONSTRAINT_DNS:
			if (imsg.hdr.type == IMSG_HOST_DNS)
				str = "IMSG_HOST_DNS";
			else
				str = "IMSG_CONSTRAINT_DNS";
			name = imsg.data;
			if (imsg.hdr.len < 1 + IMSG_HEADER_SIZE)
				fatalx("invalid %s received", str);
			len = imsg.hdr.len - 1 - IMSG_HEADER_SIZE;
			if (name[len] != '\0' ||
			    strlen(name) != len)
				fatalx("invalid %s received", str);
			if ((cnt = host_dns(name, &hn)) == -1)
				break;
			buf = imsg_create(ibuf_dns, imsg.hdr.type,
			    imsg.hdr.peerid, 0,
			    cnt * sizeof(struct sockaddr_storage));
			if (cnt > 0) {
				if (buf) {
					for (h = hn; h != NULL; h = h->next)
						if (imsg_add(buf, &h->ss,
						    sizeof(h->ss)) == -1) {
							buf = NULL;
							break;
						}
				}
				host_dns_free(hn);
				hn = NULL;
			}
			if (buf)
				imsg_close(ibuf_dns, buf);
			break;
		default:
			break;
		}
		imsg_free(&imsg);
	}
	return (0);
}
