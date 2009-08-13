/*
 * Copyright (c) 2008 Damien Miller <djm@mindrot.org>
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
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/socketvar.h>

#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/tcp_timer.h>
#include <netinet/tcp_fsm.h>
#include <netinet/in_pcb.h>
#include <netinet/tcp_var.h>

#include <arpa/inet.h>

#include <unistd.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <netdb.h>
#include <signal.h>
#include <err.h>
#include <fcntl.h>
#include <poll.h>

#include <kvm.h>
#include <nlist.h>

#define DEFAULT_PORT		"12345"
#define DEFAULT_STATS_INTERVAL	1000		/* ms */
#define DEFAULT_BUF 		256 * 1024

sig_atomic_t done = 0;
sig_atomic_t print_stats = 0;

u_int rdomain;

struct statctx {
	struct timeval t_start, t_last, t_cur;
	unsigned long long bytes;
	pid_t pid;
	u_long tcbaddr;
	kvm_t *kh;
	char **kvars;
};

/* When adding variables, also add to stats_display() */
static const char *allowed_kvars[] = {
	"inpcb.inp_flags",
	"sockb.so_rcv.sb_cc",
	"sockb.so_rcv.sb_hiwat",
	"sockb.so_snd.sb_cc",
	"sockb.so_snd.sb_hiwat",
	"tcpcb.snd_una",
	"tcpcb.snd_nxt",
	"tcpcb.snd_wl1",
	"tcpcb.snd_wl2",
	"tcpcb.snd_wnd",
	"tcpcb.rcv_wnd",
	"tcpcb.rcv_nxt",
	"tcpcb.rcv_adv",
	"tcpcb.snd_max",
	"tcpcb.snd_cwnd",
	"tcpcb.snd_ssthresh",
	"tcpcb.t_rcvtime",
	"tcpcb.t_rtttime",
	"tcpcb.t_rtseq",
	"tcpcb.t_srtt",
	"tcpcb.t_rttvar",
	"tcpcb.t_rttmin",
	"tcpcb.max_sndwnd",
	"tcpcb.snd_scale",
	"tcpcb.rcv_scale",
	"tcpcb.last_ack_sent",
	NULL
};

static void
exitsighand(int signo)
{
	done = signo;
}

static void
alarmhandler(int signo)
{
	print_stats = 1;
	signal(signo, alarmhandler);
}

static void __dead
usage(void)
{
	fprintf(stderr,
	    "usage: tcpbench -l\n"
	    "       tcpbench [-v] [-B buf] [-k kvars] [-n connections]"
	    " [-p port] [-r rate] [-V rdomain]\n"
	    "                [-S space] hostname\n"
	    "       tcpbench -s [-v] [-B buf] [-k kvars] [-p port] [-r rate]"
	    " [-S space] [-V rdomain]\n");
	exit(1);
}

static void
saddr_ntop(const struct sockaddr *addr, socklen_t alen, char *buf, size_t len)
{
	char hbuf[NI_MAXHOST], pbuf[NI_MAXSERV];
	int herr;

	if ((herr = getnameinfo(addr, alen, hbuf, sizeof(hbuf),
	    pbuf, sizeof(pbuf), NI_NUMERICHOST|NI_NUMERICSERV)) != 0) {
		if (herr == EAI_SYSTEM)
			err(1, "getnameinfo");
		else
			errx(1, "getnameinfo: %s", gai_strerror(herr));
	}
	snprintf(buf, len, "[%s]:%s", hbuf, pbuf);
}

static void
kget(kvm_t *kh, u_long addr, void *buf, int size)
{
	if (kvm_read(kh, addr, buf, size) != size)
		errx(1, "kvm_read: %s", kvm_geterr(kh));
}

static u_long
kfind_tcb(kvm_t *kh, u_long ktcbtab, int sock, int vflag)
{
	struct inpcbtable tcbtab;
	struct inpcb *head, *next, *prev;
	struct inpcb inpcb;
	struct tcpcb tcpcb;

	struct sockaddr_storage me, them;
	socklen_t melen, themlen;
	struct sockaddr_in *in4;
	struct sockaddr_in6 *in6;
	char tmp1[64], tmp2[64];

	melen = themlen = sizeof(struct sockaddr_storage);
	if (getsockname(sock, (struct sockaddr *)&me, &melen) == -1)
		err(1, "getsockname");
	if (getpeername(sock, (struct sockaddr *)&them, &themlen) == -1)
		err(1, "getpeername");
	if (me.ss_family != them.ss_family)
		errx(1, "%s: me.ss_family != them.ss_family", __func__);
	if (me.ss_family != AF_INET && me.ss_family != AF_INET6)
		errx(1, "%s: unknown socket family", __func__);
	if (vflag >= 2) {
		saddr_ntop((struct sockaddr *)&me, me.ss_len,
		    tmp1, sizeof(tmp1));
		saddr_ntop((struct sockaddr *)&them, them.ss_len,
		    tmp2, sizeof(tmp2));
		fprintf(stderr, "Our socket local %s remote %s\n", tmp1, tmp2);
	}
	if (vflag >= 2)
		fprintf(stderr, "Using PCB table at %lu\n", ktcbtab);

	kget(kh, ktcbtab, &tcbtab, sizeof(tcbtab));
	prev = head = (struct inpcb *)&CIRCLEQ_FIRST(
	    &((struct inpcbtable *)ktcbtab)->inpt_queue);
	next = CIRCLEQ_FIRST(&tcbtab.inpt_queue);

	if (vflag >= 2)
		fprintf(stderr, "PCB head at %p\n", head);
	while (next != head) {
		if (vflag >= 2)
			fprintf(stderr, "Checking PCB %p\n", next);
		kget(kh, (u_long)next, &inpcb, sizeof(inpcb));
		if (CIRCLEQ_PREV(&inpcb, inp_queue) != prev)
			errx(1, "pcb prev pointer insane");
		prev = next;
		next = CIRCLEQ_NEXT(&inpcb, inp_queue);

		if (me.ss_family == AF_INET) {
			if ((inpcb.inp_flags & INP_IPV6) != 0) {
				if (vflag >= 2)
					fprintf(stderr, "Skip: INP_IPV6");
				continue;
			}
			if (vflag >= 2) {
				inet_ntop(AF_INET, &inpcb.inp_laddr,
				    tmp1, sizeof(tmp1));
				inet_ntop(AF_INET, &inpcb.inp_faddr,
				    tmp2, sizeof(tmp2));
				fprintf(stderr, "PCB %p local: [%s]:%d "
				    "remote: [%s]:%d\n", prev,
				    tmp1, inpcb.inp_lport,
				    tmp2, inpcb.inp_fport);
			}
			in4 = (struct sockaddr_in *)&me;
			if (memcmp(&in4->sin_addr, &inpcb.inp_laddr,
			    sizeof(struct in_addr)) != 0 ||
			    in4->sin_port != inpcb.inp_lport)
				continue;
			in4 = (struct sockaddr_in *)&them;
			if (memcmp(&in4->sin_addr, &inpcb.inp_faddr,
			    sizeof(struct in_addr)) != 0 ||
			    in4->sin_port != inpcb.inp_fport)
				continue;
		} else {
			if ((inpcb.inp_flags & INP_IPV6) == 0)
				continue;
			if (vflag >= 2) {
				inet_ntop(AF_INET6, &inpcb.inp_laddr6,
				    tmp1, sizeof(tmp1));
				inet_ntop(AF_INET6, &inpcb.inp_faddr6,
				    tmp2, sizeof(tmp2));
				fprintf(stderr, "PCB %p local: [%s]:%d "
				    "remote: [%s]:%d\n", prev,
				    tmp1, inpcb.inp_lport,
				    tmp2, inpcb.inp_fport);
			}
			in6 = (struct sockaddr_in6 *)&me;
			if (memcmp(&in6->sin6_addr, &inpcb.inp_laddr6,
			    sizeof(struct in6_addr)) != 0 ||
			    in6->sin6_port != inpcb.inp_lport)
				continue;
			in6 = (struct sockaddr_in6 *)&them;
			if (memcmp(&in6->sin6_addr, &inpcb.inp_faddr6,
			    sizeof(struct in6_addr)) != 0 ||
			    in6->sin6_port != inpcb.inp_fport)
				continue;
		}
		kget(kh, (u_long)inpcb.inp_ppcb, &tcpcb, sizeof(tcpcb));
		if (tcpcb.t_state != TCPS_ESTABLISHED) {
			if (vflag >= 2)
				fprintf(stderr, "Not established\n");
			continue;
		}
		if (vflag >= 2)
			fprintf(stderr, "Found PCB at %p\n", prev);
		return (u_long)prev;
	}

	errx(1, "No matching PCB found");
}

static void
kupdate_stats(kvm_t *kh, u_long tcbaddr,
    struct inpcb *inpcb, struct tcpcb *tcpcb, struct socket *sockb)
{
	kget(kh, tcbaddr, inpcb, sizeof(*inpcb));
	kget(kh, (u_long)inpcb->inp_ppcb, tcpcb, sizeof(*tcpcb));
	kget(kh, (u_long)inpcb->inp_socket, sockb, sizeof(*sockb));
}

static void
check_kvar(const char *var)
{
	size_t i;

	for (i = 0; allowed_kvars[i] != NULL; i++)
		if (strcmp(allowed_kvars[i], var) == 0)
			return;
	errx(1, "Unrecognised kvar: %s", var);
}

static void
list_kvars(void)
{
	size_t i;

	fprintf(stderr, "Supported kernel variables:\n");
	for (i = 0; allowed_kvars[i] != NULL; i++)
		fprintf(stderr, "\t%s\n", allowed_kvars[i]);
}

static char **
check_prepare_kvars(char *list)
{
	char *item, **ret = NULL;
	size_t n = 0;

	while ((item = strsep(&list, ", \t\n")) != NULL) {
		check_kvar(item);
		if ((ret = realloc(ret, sizeof(*ret) * (++n + 1))) == NULL)
			errx(1, "realloc(kvars)");
		if ((ret[n - 1] = strdup(item)) == NULL)
			errx(1, "strdup");
		ret[n] = NULL;
	}
	return ret;
}

static void
stats_prepare(struct statctx *sc, int fd, kvm_t *kh, u_long ktcbtab,
    int rflag, int vflag, char **kflag)
{
	struct itimerval itv;
	int i;

	if (rflag <= 0)
		return;
	sc->kh = kh;
	sc->kvars = kflag;
	if (kflag)
		sc->tcbaddr = kfind_tcb(kh, ktcbtab, fd, vflag);
	gettimeofday(&sc->t_start, NULL);
	sc->t_last = sc->t_start;
	signal(SIGALRM, alarmhandler);
	itv.it_interval.tv_sec = rflag / 1000;
	itv.it_interval.tv_usec = (rflag % 1000) * 1000;
	itv.it_value = itv.it_interval;
	setitimer(ITIMER_REAL, &itv, NULL);
	sc->bytes = 0;
	sc->pid = getpid();

	printf("%8s %12s %14s %12s ", "pid", "elapsed_ms", "bytes", "Mbps");
	if (sc->kvars != NULL) {
		for (i = 0; sc->kvars[i] != NULL; i++)
			printf("%s%s", i > 0 ? "," : "", sc->kvars[i]);
	}
	printf("\n");
	fflush(stdout);
}

static void
stats_update(struct statctx *sc, ssize_t n)
{
	sc->bytes += n;
}

static void
stats_display(struct statctx *sc)
{
	struct timeval t_diff;
	unsigned long long total_elapsed, since_last;
	size_t i;
	struct inpcb inpcb;
	struct tcpcb tcpcb;
	struct socket sockb;

	gettimeofday(&sc->t_cur, NULL);
	timersub(&sc->t_cur, &sc->t_start, &t_diff);
	total_elapsed = t_diff.tv_sec * 1000 + t_diff.tv_usec / 1000;
	timersub(&sc->t_cur, &sc->t_last, &t_diff);
	since_last = t_diff.tv_sec * 1000 + t_diff.tv_usec / 1000;
	printf("%8ld %12llu %14llu %12.3Lf ", (long)sc->pid,
	    total_elapsed, sc->bytes,
	    (long double)(sc->bytes * 8) / (since_last * 1000.0));
	sc->t_last = sc->t_cur;
	sc->bytes = 0;

	if (sc->kvars != NULL) {
		kupdate_stats(sc->kh, sc->tcbaddr, &inpcb, &tcpcb, &sockb);
		for (i = 0; sc->kvars[i] != NULL; i++) {
#define P(v, f) \
	if (strcmp(sc->kvars[i], #v) == 0) { \
		printf("%s"f, i > 0 ? "," : "", v); \
		continue; \
	}
			P(inpcb.inp_flags, "0x%08x")
			P(sockb.so_rcv.sb_cc, "%lu")
			P(sockb.so_rcv.sb_hiwat, "%lu")
			P(sockb.so_snd.sb_cc, "%lu")
			P(sockb.so_snd.sb_hiwat, "%lu")
			P(tcpcb.snd_una, "%u")
			P(tcpcb.snd_nxt, "%u")
			P(tcpcb.snd_wl1, "%u")
			P(tcpcb.snd_wl2, "%u")
			P(tcpcb.snd_wnd, "%lu")
			P(tcpcb.rcv_wnd, "%lu")
			P(tcpcb.rcv_nxt, "%u")
			P(tcpcb.rcv_adv, "%u")
			P(tcpcb.snd_max, "%u")
			P(tcpcb.snd_cwnd, "%lu")
			P(tcpcb.snd_ssthresh, "%lu")
			P(tcpcb.t_rcvtime, "%u")
			P(tcpcb.t_rtttime, "%u")
			P(tcpcb.t_rtseq, "%u")
			P(tcpcb.t_srtt, "%hu")
			P(tcpcb.t_rttvar, "%hu")
			P(tcpcb.t_rttmin, "%hu")
			P(tcpcb.max_sndwnd, "%lu")
			P(tcpcb.snd_scale, "%u")
			P(tcpcb.rcv_scale, "%u")
			P(tcpcb.last_ack_sent, "%u")
#undef P
		}
	}
	printf("\n");
	fflush(stdout);
}

static void
stats_finish(struct statctx *sc)
{
	struct itimerval itv;

	signal(SIGALRM, SIG_DFL);
	bzero(&itv, sizeof(itv));
	setitimer(ITIMER_REAL, &itv, NULL);
}

static void __dead
handle_connection(kvm_t *kvmh, u_long ktcbtab, int sock, int vflag,
    int rflag, char **kflag, int Bflag)
{
	char *buf;
	struct pollfd pfd;
	ssize_t n;
	int r;
	struct statctx sc;

	if ((buf = malloc(Bflag)) == NULL)
		err(1, "malloc");
	if ((r = fcntl(sock, F_GETFL, 0)) == -1)
		err(1, "fcntl(F_GETFL)");
	r |= O_NONBLOCK;
	if (fcntl(sock, F_SETFL, r) == -1)
		err(1, "fcntl(F_SETFL, O_NONBLOCK)");

	signal(SIGINT, exitsighand);
	signal(SIGTERM, exitsighand);
	signal(SIGHUP, exitsighand);
	signal(SIGPIPE, SIG_IGN);

	bzero(&pfd, sizeof(pfd));
	pfd.fd = sock;
	pfd.events = POLLIN;

	stats_prepare(&sc, sock, kvmh, ktcbtab, rflag, vflag, kflag);

	while (!done) {
		if (print_stats) {
			stats_display(&sc);
			print_stats = 0;
		}
		if (poll(&pfd, 1, INFTIM) == -1) {
			if (errno == EINTR)
				continue;
			err(1, "poll");
		}
		if ((n = read(pfd.fd, buf, Bflag)) == -1) {
			if (errno == EINTR || errno == EAGAIN)
				continue;
			err(1, "read");
		}
		if (n == 0) {
			fprintf(stderr, "%8ld closed by remote end\n",
			    (long)getpid());
			done = -1;
			break;
		}
		if (vflag >= 3)
			fprintf(stderr, "read: %zd bytes\n", n);
		stats_update(&sc, n);
	}
	stats_finish(&sc);

	free(buf);
	close(sock);
	exit(1);
}

static void __dead
serverloop(kvm_t *kvmh, u_long ktcbtab, struct addrinfo *aitop,
    int vflag, int rflag, char **kflag, int Sflag, int Bflag)
{
	char tmp[128];
	int r, sock, client_id, on = 1;
	struct addrinfo *ai;
	struct pollfd *pfd;
	struct sockaddr_storage ss;
	socklen_t sslen;
	size_t nfds, i, j;

	pfd = NULL;
	nfds = 0;
	for (ai = aitop; ai != NULL; ai = ai->ai_next) {
		saddr_ntop(ai->ai_addr, ai->ai_addrlen, tmp, sizeof(tmp));
		if (vflag)
			fprintf(stderr, "Try listen on %s\n", tmp);
		if ((sock = socket(ai->ai_family, ai->ai_socktype,
		    ai->ai_protocol)) == -1) {
			if (ai->ai_next == NULL)
				err(1, "socket");
			if (vflag)
				warn("socket");
			continue;
		}
		if (rdomain && ai->ai_family == AF_INET) {
			if (setsockopt(sock, IPPROTO_IP, SO_RDOMAIN,
			    &rdomain, sizeof(rdomain)) == -1)
				err(1, "setsockopt SO_RDOMAIN");
		}
		if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,
		    &on, sizeof(on)) == -1)
			warn("reuse port");
		if (bind(sock, ai->ai_addr, ai->ai_addrlen) != 0) {
			if (ai->ai_next == NULL)
				err(1, "bind");
			if (vflag)
				warn("bind");
			close(sock);
			continue;
		}
		if (Sflag) {
			if (setsockopt(sock, SOL_SOCKET, SO_RCVBUF,
			    &Sflag, sizeof(Sflag)) == -1)
				warn("set TCP receive buffer size");
		}
		if (listen(sock, 64) == -1) {
			if (ai->ai_next == NULL)
				err(1, "listen");
			if (vflag)
				warn("listen");
			close(sock);
			continue;
		}
		if (nfds > 128)
			break;
		if ((pfd = realloc(pfd, ++nfds * sizeof(*pfd))) == NULL)
			errx(1, "realloc(pfd * %zu)", nfds);
		pfd[nfds - 1].fd = sock;
		pfd[nfds - 1].events = POLLIN;
	}
	freeaddrinfo(aitop);
	if (nfds == 0)
		errx(1, "No working listen addresses found");

	signal(SIGINT, exitsighand);
	signal(SIGTERM, exitsighand);
	signal(SIGHUP, exitsighand);
	signal(SIGPIPE, SIG_IGN);
	signal(SIGCHLD, SIG_IGN);

	if (setpgid(0, 0) == -1)
		err(1, "setpgid");

	client_id = 0;
	while (!done) {		
		if ((r = poll(pfd, nfds, INFTIM)) == -1) {
			if (errno == EINTR)
				continue;
			warn("poll");
			break;
		}
		if (vflag >= 3)
			fprintf(stderr, "poll: %d\n", r);
		for (i = 0 ; r > 0 && i < nfds; i++) {
			if ((pfd[i].revents & POLLIN) == 0)
				continue;
			if (vflag >= 3)
				fprintf(stderr, "fd %d active\n", pfd[i].fd);
			r--;
			sslen = sizeof(ss);
			if ((sock = accept(pfd[i].fd, (struct sockaddr *)&ss,
			    &sslen)) == -1) {
				if (errno == EINTR)
					continue;
				warn("accept");
				break;
			}
			saddr_ntop((struct sockaddr *)&ss, sslen,
			    tmp, sizeof(tmp));
			if (vflag)
				fprintf(stderr, "Accepted connection %d from "
				    "%s, fd = %d\n", client_id++, tmp, sock);
			switch (fork()) {
			case -1:
				warn("fork");
				done = -1;
				break;
			case 0:
				for (j = 0; j < nfds; j++)
					if (j != i)
						close(pfd[j].fd);
				handle_connection(kvmh, ktcbtab, sock,
				    vflag, rflag, kflag, Bflag);
				/* NOTREACHED */
				_exit(1);
			default:
				close(sock);
				break;
			}
			if (done == -1)
				break;
		}
	}
	for (i = 0; i < nfds; i++)
		close(pfd[i].fd);
	if (done > 0)
		warnx("Terminated by signal %d", done);
	signal(SIGTERM, SIG_IGN);
	killpg(0, SIGTERM);
	exit(1);
}

static void __dead
clientloop(kvm_t *kvmh, u_long ktcbtab, const char *host, const char *port,
    int vflag, int rflag, char **kflag, int Sflag, int Bflag, int nconn)
{
	char tmp[128];
	char *buf;
	int r, sock, herr;
	struct addrinfo *aitop, *ai, hints;
	struct pollfd *pfd;
	ssize_t n;
	struct statctx sc;
	u_int i, scnt = 0;

	if ((buf = malloc(Bflag)) == NULL)
		err(1, "malloc");

	if ((pfd = calloc(nconn, sizeof(struct pollfd))) == NULL)
		err(1, "clientloop pfd calloc");

	for (i = 0; i < nconn; i++) {
		bzero(&hints, sizeof(hints));
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_flags = 0;
		if ((herr = getaddrinfo(host, port, &hints, &aitop)) != 0) {
			if (herr == EAI_SYSTEM)
				err(1, "getaddrinfo");
			else
				errx(1, "c getaddrinfo: %s", gai_strerror(herr));
		}

		for (sock = -1, ai = aitop; ai != NULL; ai = ai->ai_next) {
			saddr_ntop(ai->ai_addr, ai->ai_addrlen, tmp,
			    sizeof(tmp));
			if (vflag && scnt == 0)
				fprintf(stderr, "Trying %s\n", tmp);
			if ((sock = socket(ai->ai_family, ai->ai_socktype,
			    ai->ai_protocol)) == -1) {
				if (ai->ai_next == NULL)
					err(1, "socket");
				if (vflag)
					warn("socket");
				continue;
			}
			if (rdomain && ai->ai_family == AF_INET) {
				if (setsockopt(sock, IPPROTO_IP, SO_RDOMAIN,
				    &rdomain, sizeof(rdomain)) == -1)
					err(1, "setsockopt SO_RDOMAIN");
			}
			if (Sflag) {
				if (setsockopt(sock, SOL_SOCKET, SO_SNDBUF,
				    &Sflag, sizeof(Sflag)) == -1)
					warn("set TCP send buffer size");
			}
			if (connect(sock, ai->ai_addr, ai->ai_addrlen) != 0) {
				if (ai->ai_next == NULL)
					err(1, "connect");
				if (vflag)
					warn("connect");
				close(sock);
				sock = -1;
				continue;
			}
			break;
		}
		freeaddrinfo(aitop);
		if (sock == -1)
			errx(1, "No host found");

		if ((r = fcntl(sock, F_GETFL, 0)) == -1)
			err(1, "fcntl(F_GETFL)");
		r |= O_NONBLOCK;
		if (fcntl(sock, F_SETFL, r) == -1)
			err(1, "fcntl(F_SETFL, O_NONBLOCK)");

		pfd[i].fd = sock;
		pfd[i].events = POLLOUT;
		scnt++;
	}

	if (vflag && scnt > 1)
		fprintf(stderr, "%u connections established\n", scnt);
	arc4random_buf(buf, Bflag);

	signal(SIGINT, exitsighand);
	signal(SIGTERM, exitsighand);
	signal(SIGHUP, exitsighand);
	signal(SIGPIPE, SIG_IGN);

	stats_prepare(&sc, sock, kvmh, ktcbtab, rflag, vflag, kflag);

	while (!done) {
		if (print_stats) {
			stats_display(&sc);
			print_stats = 0;
		}
		if (poll(pfd, nconn, INFTIM) == -1) {
			if (errno == EINTR)
				continue;
			err(1, "poll");
		}
		for (i = 0; i < nconn; i++) {
			if (pfd[i].revents & POLLOUT) {
				if ((n = write(pfd[i].fd, buf, Bflag)) == -1) {
					if (errno == EINTR || errno == EAGAIN)
						continue;
					err(1, "write");
				}
				if (n == 0) {
					warnx("Remote end closed connection");
					done = -1;
					break;
				}
				if (vflag >= 3)
					fprintf(stderr, "write: %zd bytes\n",
					    n);
				stats_update(&sc, n);
			}
		}
	}
	stats_finish(&sc);

	if (done > 0)
		warnx("Terminated by signal %d", done);

	free(buf);
	close(sock);
	exit(0);
}

static void
drop_gid(void)
{
	gid_t gid;

	gid = getgid();
	if (setresgid(gid, gid, gid) == -1)
		err(1, "setresgid");
}

int
main(int argc, char **argv)
{
	extern int optind;
	extern char *optarg;

	char kerr[_POSIX2_LINE_MAX], *tmp;
	const char *errstr;
	int ch, herr;
	struct addrinfo *aitop, hints;
	kvm_t *kvmh = NULL;

	const char *host = NULL, *port = DEFAULT_PORT;
	char **kflag = NULL;
	int sflag = 0, vflag = 0, rflag = DEFAULT_STATS_INTERVAL, Sflag = 0;
	int Bflag = DEFAULT_BUF;
	int nconn = 1;

	struct nlist nl[] = { { "_tcbtable" }, { "" } };

	while ((ch = getopt(argc, argv, "B:hlk:n:p:r:sS:vV:")) != -1) {
		switch (ch) {
		case 'l':
			list_kvars();
			exit(0);
		case 'k':
			if ((tmp = strdup(optarg)) == NULL)
				errx(1, "strdup");
			kflag = check_prepare_kvars(tmp);
			free(tmp);
			break;
		case 'r':
			rflag = strtonum(optarg, 0, 60 * 60 * 24 * 1000,
			    &errstr);
			if (errstr != NULL)
				errx(1, "statistics interval is %s: %s",
				    errstr, optarg);
			break;
		case 'p':
			port = optarg;
			break;
		case 's':
			sflag = 1;
			break;
		case 'S':
			Sflag = strtonum(optarg, 0, 1024*1024*1024,
			    &errstr);
			if (errstr != NULL)
				errx(1, "receive space interval is %s: %s",
				    errstr, optarg);
			break;
		case 'B':
			Bflag = strtonum(optarg, 0, 1024*1024*1024,
			    &errstr);
			if (errstr != NULL)
				errx(1, "read/write buffer size is %s: %s",
				    errstr, optarg);
			break;
		case 'v':
			vflag++;
			break;
		case 'V':
			rdomain = (unsigned int)strtonum(optarg, 0,
			    RT_TABLEID_MAX, &errstr);
			if (errstr)
				errx(1, "rdomain value is %s: %s",
				    errstr, optarg);
			break;
		case 'n':
			nconn = strtonum(optarg, 0, 65535, &errstr);
			if (errstr != NULL)
				errx(1, "number of connections is %s: %s",
				    errstr, optarg);
			break;
		case 'h':
		default:
			usage();
		}
	}

	argv += optind;
	argc -= optind;
	if (argc != (sflag ? 0 : 1))
		usage();

	if (kflag != NULL && nconn > 1)
		errx(1, "-k currently only works with a single tcp connection");

	if (!sflag)
		host = argv[0];

	if (sflag) {
		bzero(&hints, sizeof(hints));
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_flags = AI_PASSIVE;
		if ((herr = getaddrinfo(host, port, &hints, &aitop)) != 0) {
			if (herr == EAI_SYSTEM)
				err(1, "getaddrinfo");
			else
				errx(1, "s getaddrinfo: %s", gai_strerror(herr));
		}
	}

	if (kflag) {
		if ((kvmh = kvm_openfiles(NULL, NULL, NULL,
		    O_RDONLY, kerr)) == NULL)
			errx(1, "kvm_open: %s", kerr);
		drop_gid();
		if (kvm_nlist(kvmh, nl) < 0 || nl[0].n_type == 0)
			errx(1, "kvm: no namelist");
	} else
		drop_gid();

	if (sflag)
		serverloop(kvmh, nl[0].n_value, aitop, vflag, rflag, kflag,
		    Sflag, Bflag);
	else
		clientloop(kvmh, nl[0].n_value, host, port, vflag, rflag, kflag,
		    Sflag, Bflag, nconn);

	return 0;
}
