/*	$OpenBSD: ntp.c,v 1.2 2004/05/31 15:11:56 henning Exp $ */

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
#include <netdb.h>
#include <poll.h>
#include <pwd.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ntpd.h"
#include "ntp.h"

#define	PFD_LISTEN	0
#define	PFD_LISTEN6	1
#define	PFD_PIPE_MAIN	2
#define	PFD_MAX		3

volatile sig_atomic_t	 ntp_quit = 0;
struct imsgbuf		 ibuf_main;
struct l_fixedpt	 ref_ts;

void	ntp_sighdlr(int);
int	setup_listener(struct servent *);
int	setup_listener6(struct servent *);
int	ntp_dispatch_imsg(void);
int	ntp_dispatch(int fd);
int	parse_ntp_msg(char *, ssize_t, struct ntp_msg *);
int	ntp_reply(int, struct sockaddr *, struct ntp_msg *, int);
int	ntp_send(int, struct sockaddr *, struct ntp_msg *, ssize_t, int);
void	get_ts(struct l_fixedpt *);

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
ntp_main(int pipe_prnt[2])
{
	int			 nfds;
	int			 sock = -1;
	int			 sock6 = -1;
	pid_t			 pid;
	struct pollfd		 pfd[PFD_MAX];
	struct passwd		*pw;
	struct servent		*se;

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

	sock = setup_listener(se);
	sock6 = setup_listener6(se);

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
		get_ts(&ref_ts);	/* XXX */
		bzero(&pfd, sizeof(pfd));
		pfd[PFD_LISTEN].fd = sock;
		pfd[PFD_LISTEN].events = POLLIN;
		pfd[PFD_LISTEN6].fd = sock6;
		pfd[PFD_LISTEN6].events = POLLIN;
		pfd[PFD_PIPE_MAIN].fd = ibuf_main.fd;
		pfd[PFD_PIPE_MAIN].events = POLLIN;
		if (ibuf_main.w.queued > 0)
			pfd[PFD_PIPE_MAIN].events |= POLLOUT;

		if ((nfds = poll(pfd, PFD_MAX, INFTIM)) == -1)
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

		if (nfds > 0 && pfd[PFD_LISTEN].revents & POLLIN) {
			nfds--;
			if (ntp_dispatch(sock) == -1)
				ntp_quit = 1;
		}

		if (nfds > 0 && pfd[PFD_LISTEN6].revents & POLLIN) {
			nfds--;
			if (ntp_dispatch(sock6) == -1)
				ntp_quit = 1;
		}
	}

	msgbuf_write(&ibuf_main.w);
	msgbuf_clear(&ibuf_main.w);

	log_info("ntp engine exiting");
	_exit(0);
}

int
setup_listener(struct servent *se)
{
	struct sockaddr_in	 sa_in;
	int			 fd;

	if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
		fatal("socket");

	bzero(&sa_in, sizeof(sa_in));
	sa_in.sin_len = sizeof(sa_in);
	sa_in.sin_family = AF_INET;
	sa_in.sin_addr.s_addr = htonl(INADDR_ANY);
	sa_in.sin_port = se->s_port;

	if (bind(fd, (struct sockaddr *)&sa_in, sizeof(sa_in)) == -1)
		fatal("bind");

	return (fd);
}

int
setup_listener6(struct servent *se)
{
	struct sockaddr_in6	 sa_in6;
	int			 fd;

	if ((fd = socket(AF_INET6, SOCK_DGRAM, 0)) == -1)
		fatal("socket");

	bzero(&sa_in6, sizeof(sa_in6));
	sa_in6.sin6_len = sizeof(sa_in6);
	sa_in6.sin6_family = AF_INET6;
	sa_in6.sin6_port = se->s_port;

	if (bind(fd, (struct sockaddr *)&sa_in6, sizeof(sa_in6)) == -1)
		fatal("bind");

	return (fd);
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

	parse_ntp_msg(buf, size, &msg);
	ntp_reply(fd, (struct sockaddr *)&fsa, &msg, 0);

	return (0);
}

int
parse_ntp_msg(char *p, ssize_t len, struct ntp_msg *msg)
{
	int		 auth, i;

	if (len == NTP_MSGSIZE)
		auth = 1;
	else if (len == NTP_MSGSIZE_NOAUTH)
		auth = 0;
	else {
		log_warnx("malformed packet received");
		return (-1);
	}

	memcpy(&msg->status, p, sizeof(msg->status));
	p += sizeof(msg->status);
	memcpy(&msg->stratum, p, sizeof(msg->stratum));
	p += sizeof(msg->stratum);
	memcpy(&msg->ppoll, p, sizeof(msg->ppoll));
	p += sizeof(msg->ppoll);
	memcpy(&msg->precision, p, sizeof(msg->precision));
	p += sizeof(msg->precision);
	memcpy(&msg->distance.int_part, p, sizeof(msg->distance.int_part));
	p += sizeof(msg->distance.int_part);
	memcpy(&msg->distance.fraction, p, sizeof(msg->distance.fraction));
	p += sizeof(msg->distance.fraction);
	memcpy(&msg->dispersion.int_part, p, sizeof(msg->dispersion.int_part));
	p += sizeof(msg->dispersion.int_part);
	memcpy(&msg->dispersion.fraction, p, sizeof(msg->dispersion.fraction));
	p += sizeof(msg->dispersion.fraction);
	memcpy(&msg->refid, p, sizeof(msg->refid));
	p += sizeof(msg->refid);
	memcpy(&msg->reftime.int_part, p, sizeof(msg->reftime.int_part));
	p += sizeof(msg->reftime.int_part);
	memcpy(&msg->reftime.fraction, p, sizeof(msg->reftime.fraction));
	p += sizeof(msg->reftime.fraction);
	memcpy(&msg->orgtime.int_part, p, sizeof(msg->orgtime.int_part));
	p += sizeof(msg->orgtime.int_part);
	memcpy(&msg->orgtime.fraction, p, sizeof(msg->orgtime.fraction));
	p += sizeof(msg->orgtime.fraction);
	memcpy(&msg->rectime.int_part, p, sizeof(msg->rectime.int_part));
	p += sizeof(msg->rectime.int_part);
	memcpy(&msg->rectime.fraction, p, sizeof(msg->rectime.fraction));
	p += sizeof(msg->rectime.fraction);
	memcpy(&msg->xmttime.int_part, p, sizeof(msg->xmttime.int_part));
	p += sizeof(msg->xmttime.int_part);
	memcpy(&msg->xmttime.fraction, p, sizeof(msg->xmttime.fraction));
	p += sizeof(msg->xmttime.fraction);

	if (auth) {
		memcpy(&msg->keyid, p, sizeof(msg->keyid));
		p += sizeof(msg->refid);
		for (i = 0; i < NTP_DIGESTSIZE; i++) {
			memcpy(&msg->digest[i], p, sizeof(msg->digest[i]));
			p += sizeof(msg->digest[i]);
		}

		/* XXX check auth */
	}

	return (0);
}

int
ntp_reply(int fd, struct sockaddr *sa, struct ntp_msg *query, int auth)
{
	ssize_t			 len;
	struct l_fixedpt	 t;
	struct ntp_msg		 reply;

	if (auth)
		len = NTP_MSGSIZE;
	else
		len = NTP_MSGSIZE_NOAUTH;

	bzero(&reply, sizeof(reply));
	reply.status = 0 | (query->status & VERSIONMASK);
	if ((query->status & MODEMASK) == MODE_CLIENT)
		reply.status |= MODE_SERVER;
	else
		reply.status |= MODE_SYM_PAS;

	reply.stratum =	2;
	reply.ppoll = query->ppoll;
	reply.precision = 0;			/* XXX */
	reply.refid = htonl(t.fraction);	/* XXX */
	reply.reftime.int_part = htonl(ref_ts.int_part);
	reply.reftime.fraction = htonl(ref_ts.fraction);
	get_ts(&t);
	reply.rectime.int_part = htonl(t.int_part);
	reply.rectime.fraction = htonl(t.fraction);
	reply.xmttime.int_part = htonl(t.int_part);
	reply.xmttime.fraction = htonl(t.fraction);
	reply.orgtime.int_part = query->xmttime.int_part;
	reply.orgtime.fraction = query->xmttime.fraction;

	return (ntp_send(fd, sa, &reply, len, auth));
}

int
ntp_send(int fd, struct sockaddr *sa, struct ntp_msg *reply, ssize_t len,
    int auth)
{
	char	 buf[NTP_MSGSIZE];
	char	*p;

	p = buf;
	memcpy(p, &reply->status, sizeof(reply->status));
	p += sizeof(reply->status);
	memcpy(p, &reply->stratum, sizeof(reply->stratum));
	p += sizeof(reply->stratum);
	memcpy(p, &reply->ppoll, sizeof(reply->ppoll));
	p += sizeof(reply->ppoll);
	memcpy(p, &reply->precision, sizeof(reply->precision));
	p += sizeof(reply->precision);
	memcpy(p, &reply->distance.int_part, sizeof(reply->distance.int_part));
	p += sizeof(reply->distance.int_part);
	memcpy(p, &reply->distance.fraction, sizeof(reply->distance.fraction));
	p += sizeof(reply->distance.fraction);
	memcpy(p, &reply->dispersion.int_part,
	    sizeof(reply->dispersion.int_part));
	p += sizeof(reply->dispersion.int_part);
	memcpy(p, &reply->dispersion.fraction,
	    sizeof(reply->dispersion.fraction));
	p += sizeof(reply->dispersion.fraction);
	memcpy(p, &reply->refid, sizeof(reply->refid));
	p += sizeof(reply->refid);
	memcpy(p, &reply->reftime.int_part, sizeof(reply->reftime.int_part));
	p += sizeof(reply->reftime.int_part);
	memcpy(p, &reply->reftime.fraction, sizeof(reply->reftime.fraction));
	p += sizeof(reply->reftime.fraction);
	memcpy(p, &reply->orgtime.int_part, sizeof(reply->orgtime.int_part));
	p += sizeof(reply->orgtime.int_part);
	memcpy(p, &reply->orgtime.fraction, sizeof(reply->orgtime.fraction));
	p += sizeof(reply->orgtime.fraction);
	memcpy(p, &reply->rectime.int_part, sizeof(reply->rectime.int_part));
	p += sizeof(reply->rectime.int_part);
	memcpy(p, &reply->rectime.fraction, sizeof(reply->rectime.fraction));
	p += sizeof(reply->rectime.fraction);
	memcpy(p, &reply->xmttime.int_part, sizeof(reply->xmttime.int_part));
	p += sizeof(reply->xmttime.int_part);
	memcpy(p, &reply->xmttime.fraction, sizeof(reply->xmttime.fraction));
	p += sizeof(reply->xmttime.fraction);

	if (auth) {
		/* XXX */
	}

	if (sendto(fd, &buf, len, 0, sa, sa->sa_len) != len)
		fatal("sendto");

	return (0);
}

void
get_ts(struct l_fixedpt *t)
{
	struct timeval		 tv;

	gettimeofday(&tv, NULL);
	t->int_part = tv.tv_sec + JAN_1970;
	t->fraction = ((float)tv.tv_usec)/1000000 * UINT_MAX;
}