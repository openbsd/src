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
#include <sys/resource.h>

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

#define DEFAULT_PORT "12345"
#define DEFAULT_STATS_INTERVAL 1000 /* ms */
#define DEFAULT_BUF 256 * 1024
#define MAX_FD 1024

sig_atomic_t done = 0;
sig_atomic_t proc_slice = 0;

static u_int  rtableid;
static char **kflag;
static size_t Bflag;
static int    Sflag;
static int    rflag;
static int    sflag;
static int    vflag;

/* stats for a single connection */
struct statctx {
	struct timeval t_start, t_last;
	unsigned long long bytes;
	u_long tcbaddr;
	char **kvars;
	kvm_t *kh;
};

/*
 * We account the mainstats here, that is the stats
 * for all connections, all variables starting with slice
 * are used to account information for the timeslice
 * between each output. Peak variables record the highest
 * between all slices so far.
 */
static struct {
	unsigned long long slice_bytes; /* bytes for last slice */
	struct timeval t_start;	        /* when we started counting */
	long double peak_mbps;		/* peak mbps so far */
	int nconns; 		        /* connected clients */
} mainstats;

/* When adding variables, also add to stats_display() */
static const char *allowed_kvars[] = {
	"inpcb.inp_flags",
	"sockb.so_rcv.sb_cc",
	"sockb.so_rcv.sb_wat",
	"sockb.so_rcv.sb_hiwat",
	"sockb.so_snd.sb_cc",
	"sockb.so_snd.sb_wat",
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
	"tcpcb.rfbuf_cnt",
	"tcpcb.rfbuf_ts",
	"tcpcb.ts_recent_age",
	"tcpcb.ts_recent",
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
	proc_slice = 1;
	signal(signo, alarmhandler);
}

static void __dead
usage(void)
{
	fprintf(stderr,
	    "usage: tcpbench -l\n"
	    "       tcpbench [-v] [-B buf] [-k kvars] [-n connections] [-p port]\n"
	    "                [-r rate] [-S space] [-V rtable] hostname\n"
	    "       tcpbench -s [-v] [-B buf] [-k kvars] [-p port]\n"
	    "                [-r rate] [-S space] [-V rtable]\n");
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
set_timer(int toggle)
{
	struct itimerval itv;

	if (rflag <= 0)
		return;

	if (toggle) {
		itv.it_interval.tv_sec = rflag / 1000;
		itv.it_interval.tv_usec = (rflag % 1000) * 1000;
		itv.it_value = itv.it_interval;
	}
	else
		bzero(&itv, sizeof(itv));
		
	setitimer(ITIMER_REAL, &itv, NULL);
}

static void
print_header(void)
{
	char **kv;
	
	printf("%12s %14s %12s %8s ", "elapsed_ms", "bytes", "mbps",
	    "bwidth");
	
	for (kv = kflag;  kflag != NULL && *kv != NULL; kv++) 
		printf("%s%s", kv != kflag ? "," : "", *kv);
	
	printf("\n");
}

static void
kget(kvm_t *kh, u_long addr, void *buf, size_t size)
{
	if (kvm_read(kh, addr, buf, size) != (ssize_t)size)
		errx(1, "kvm_read: %s", kvm_geterr(kh));
}

static u_long
kfind_tcb(kvm_t *kh, u_long ktcbtab, int sock)
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
	int nretry;

	nretry = 10;
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
retry:
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
		if (CIRCLEQ_PREV(&inpcb, inp_queue) != prev) {
			if (nretry--) {
				warnx("pcb prev pointer insane");
				goto retry;
			}
			else
				errx(1, "pcb prev pointer insane,"
				     " all attempts exausted");
		}
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
	u_int i;

	for (i = 0; allowed_kvars[i] != NULL; i++)
		if (strcmp(allowed_kvars[i], var) == 0)
			return;
	errx(1, "Unrecognised kvar: %s", var);
}

static void
list_kvars(void)
{
	u_int i;

	fprintf(stderr, "Supported kernel variables:\n");
	for (i = 0; allowed_kvars[i] != NULL; i++)
		fprintf(stderr, "\t%s\n", allowed_kvars[i]);
}

static char **
check_prepare_kvars(char *list)
{
	char *item, **ret = NULL;
	u_int n = 0;

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
stats_prepare(struct statctx *sc, int fd, kvm_t *kh, u_long ktcbtab)
{
	if (rflag <= 0)
		return;
	sc->kh = kh;
	sc->kvars = kflag;
	if (kflag)
		sc->tcbaddr = kfind_tcb(kh, ktcbtab, fd);
	if (gettimeofday(&sc->t_start, NULL) == -1)
		err(1, "gettimeofday");
	sc->t_last = sc->t_start;
	sc->bytes = 0;
}

static void
stats_update(struct statctx *sc, ssize_t n)
{
	sc->bytes += n;
	mainstats.slice_bytes += n;
}

static void
stats_cleanslice(void)
{
	mainstats.slice_bytes = 0;
}

static void
stats_display(unsigned long long total_elapsed, long double mbps,
    float bwperc, struct statctx *sc, struct inpcb *inpcb,
    struct tcpcb *tcpcb, struct socket *sockb)
{
	int j;
	
	printf("%12llu %14llu %12.3Lf %7.2f%% ", total_elapsed, sc->bytes,
	    mbps, bwperc);
	
	if (sc->kvars != NULL) {
		kupdate_stats(sc->kh, sc->tcbaddr, inpcb, tcpcb,
		    sockb);

		for (j = 0; sc->kvars[j] != NULL; j++) {
#define S(a) #a
#define P(b, v, f)							\
			if (strcmp(sc->kvars[j], S(b.v)) == 0) {	\
				printf("%s"f, j > 0 ? "," : "", b->v);	\
				continue;				\
			}
			P(inpcb, inp_flags, "0x%08x")
			P(sockb, so_rcv.sb_cc, "%lu")
			P(sockb, so_rcv.sb_wat, "%lu")
			P(sockb, so_rcv.sb_hiwat, "%lu")
			P(sockb, so_snd.sb_cc, "%lu")
			P(sockb, so_snd.sb_wat, "%lu")
			P(sockb, so_snd.sb_hiwat, "%lu")
			P(tcpcb, snd_una, "%u")
			P(tcpcb, snd_nxt, "%u")
			P(tcpcb, snd_wl1, "%u")
			P(tcpcb, snd_wl2, "%u")
			P(tcpcb, snd_wnd, "%lu")
			P(tcpcb, rcv_wnd, "%lu")
			P(tcpcb, rcv_nxt, "%u")
			P(tcpcb, rcv_adv, "%u")
			P(tcpcb, snd_max, "%u")
			P(tcpcb, snd_cwnd, "%lu")
			P(tcpcb, snd_ssthresh, "%lu")
			P(tcpcb, t_rcvtime, "%u")
			P(tcpcb, t_rtttime, "%u")
			P(tcpcb, t_rtseq, "%u")
			P(tcpcb, t_srtt, "%hu")
			P(tcpcb, t_rttvar, "%hu")
			P(tcpcb, t_rttmin, "%hu")
			P(tcpcb, max_sndwnd, "%lu")
			P(tcpcb, snd_scale, "%u")
			P(tcpcb, rcv_scale, "%u")
			P(tcpcb, last_ack_sent, "%u")
			P(tcpcb, rfbuf_cnt, "%u")
			P(tcpcb, rfbuf_ts, "%u")
			P(tcpcb, ts_recent_age, "%u")
			P(tcpcb, ts_recent, "%u")
#undef S			    
#undef P
		}
	}
	printf("\n");
}

static void
mainstats_display(long double slice_mbps, long double avg_mbps)
{
	printf("Conn: %3d Mbps: %12.3Lf Peak Mbps: %12.3Lf Avg Mbps: %12.3Lf\n",
	    mainstats.nconns, slice_mbps, mainstats.peak_mbps, avg_mbps); 
}

static void
process_slice(struct statctx *sc, size_t nsc)
{
	unsigned long long total_elapsed, since_last;
	long double mbps, slice_mbps = 0;
	float bwperc;
	nfds_t i;
	struct timeval t_cur, t_diff;
	struct inpcb inpcb;
	struct tcpcb tcpcb;
	struct socket sockb;
	
	for (i = 0; i < nsc; i++, sc++) {
		if (gettimeofday(&t_cur, NULL) == -1)
			err(1, "gettimeofday");
		if (sc->kvars != NULL) /* process kernel stats */
			kupdate_stats(sc->kh, sc->tcbaddr, &inpcb, &tcpcb,
			    &sockb);
		timersub(&t_cur, &sc->t_start, &t_diff);
		total_elapsed = t_diff.tv_sec * 1000 + t_diff.tv_usec / 1000;
		timersub(&t_cur, &sc->t_last, &t_diff);
		since_last = t_diff.tv_sec * 1000 + t_diff.tv_usec / 1000;
		bwperc = (sc->bytes * 100.0) / mainstats.slice_bytes;
		mbps = (sc->bytes * 8) / (since_last * 1000.0);
		slice_mbps += mbps;
		
		stats_display(total_elapsed, mbps, bwperc, sc,
		    &inpcb, &tcpcb, &sockb);
		
		sc->t_last = t_cur;
		sc->bytes = 0;

	}

	/* process stats for this slice */
	if (slice_mbps > mainstats.peak_mbps)
		mainstats.peak_mbps = slice_mbps;
	mainstats_display(slice_mbps, slice_mbps / mainstats.nconns);
}

static int
handle_connection(struct statctx *sc, int fd, char *buf, size_t buflen)
{
	ssize_t n;

again:
	n = read(fd, buf, buflen);
	if (n == -1) {
		if (errno == EINTR)
			goto again;
		else if (errno == EWOULDBLOCK) 
			return 0;
		warn("fd %d read error", fd);
		
		return -1;
	}
	else if (n == 0) {
		if (vflag)
			fprintf(stderr, "%8d closed by remote end\n", fd);
		close(fd);
		return -1;
	}
	if (vflag >= 3)
		fprintf(stderr, "read: %zd bytes\n", n);
	
	stats_update(sc, n);
	return 0;
}

static nfds_t
serverbind(struct pollfd *pfd, nfds_t max_nfds, struct addrinfo *aitop)
{
	char tmp[128];
	int sock, on = 1;
	struct addrinfo *ai;
	nfds_t lnfds;

	lnfds = 0;
	for (ai = aitop; ai != NULL; ai = ai->ai_next) {
		if (lnfds == max_nfds) {
			fprintf(stderr,
			    "maximum number of listening fds reached\n");
			break;
		}
		saddr_ntop(ai->ai_addr, ai->ai_addrlen, tmp, sizeof(tmp));
		if (vflag)
			fprintf(stderr, "Try to listen on %s\n", tmp);
		if ((sock = socket(ai->ai_family, ai->ai_socktype,
		    ai->ai_protocol)) == -1) {
			if (ai->ai_next == NULL)
				err(1, "socket");
			if (vflag)
				warn("socket");
			continue;
		}
		if (rtableid && ai->ai_family == AF_INET) {
			if (setsockopt(sock, IPPROTO_IP, SO_RTABLE,
			    &rtableid, sizeof(rtableid)) == -1)
				err(1, "setsockopt SO_RTABLE");
		} else if (rtableid)
			warnx("rtable only supported on AF_INET");
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
		if (vflag >= 3)
			fprintf(stderr, "listening on fd %d\n", sock);
		lnfds++;
		pfd[lnfds - 1].fd = sock;
		pfd[lnfds - 1].events = POLLIN;

	}
	freeaddrinfo(aitop);
	if (lnfds == 0)
		errx(1, "No working listen addresses found");

	return lnfds;
}	

static void
set_listening(struct pollfd *pfd, nfds_t lfds, int toggle) {
	int i;

	for (i = 0; i < (int)lfds; i++) {
		if (toggle)
			pfd[i].events = POLLIN;
		else
			pfd[i].events = 0;
	}
			
}
static void __dead
serverloop(kvm_t *kvmh, u_long ktcbtab, struct addrinfo *aitop)
{
	socklen_t sslen;
	struct pollfd *pfd;
	char tmp[128], *buf;
	struct statctx *psc;
	struct sockaddr_storage ss;
	nfds_t i, nfds, lfds;
	size_t nalloc;
	int r, sock, client_id;

	sslen = sizeof(ss);
	nalloc = 128;
	if ((pfd = calloc(sizeof(*pfd), nalloc)) == NULL)
		err(1, "calloc");
	if ((psc = calloc(sizeof(*psc), nalloc)) == NULL)
		err(1, "calloc");
	if ((buf = malloc(Bflag)) == NULL)
		err(1, "malloc");
	lfds = nfds = serverbind(pfd, nalloc - 1, aitop);
	if (vflag >= 3)
		fprintf(stderr, "listening on %d fds\n", lfds);
	if (setpgid(0, 0) == -1)
		err(1, "setpgid");
	
	print_header();
	
	client_id = 0;
	while (!done) {
		if (proc_slice) { 
			process_slice(psc + lfds, nfds - lfds);
			stats_cleanslice();
			proc_slice = 0;
		}
		if (vflag >= 3) 
			fprintf(stderr, "mainstats.nconns = %u\n",
			    mainstats.nconns);
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
			if (pfd[i].fd == -1)
				errx(1, "pfd insane");
			r--;
			if (vflag >= 3)
				fprintf(stderr, "fd %d active i = %d\n",
				    pfd[i].fd, i);
			/* new connection */
			if (i < lfds) {
				if ((sock = accept(pfd[i].fd,
				    (struct sockaddr *)&ss,
				    &sslen)) == -1) {
					if (errno == EINTR)
						continue;
					else if (errno == EMFILE ||
					    errno == ENFILE)
						set_listening(pfd, lfds, 0);
					warn("accept");
					continue;
				}
				if ((r = fcntl(sock, F_GETFL, 0)) == -1)
					err(1, "fcntl(F_GETFL)");
				r |= O_NONBLOCK;
				if (fcntl(sock, F_SETFL, r) == -1)
					err(1, "fcntl(F_SETFL, O_NONBLOCK)");
				saddr_ntop((struct sockaddr *)&ss, sslen,
				    tmp, sizeof(tmp));
				if (vflag)
					fprintf(stderr,
					    "Accepted connection %d from "
					    "%s, fd = %d\n", client_id++, tmp,
					     sock);
				/* alloc more space if we're full */
				if (nfds == nalloc) {
					nalloc *= 2;
					if ((pfd = realloc(pfd,
					    sizeof(*pfd) * nalloc)) == NULL)
						err(1, "realloc");
					if ((psc = realloc(psc,
					    sizeof(*psc) * nalloc)) == NULL)
						err(1, "realloc");
				}
				pfd[nfds].fd = sock;
				pfd[nfds].events = POLLIN;
				stats_prepare(&psc[nfds], sock, kvmh, ktcbtab);
				nfds++;
				if (!mainstats.nconns++)
					set_timer(1);
				continue;
			}
			/* event in fd */
			if (vflag >= 3)
				fprintf(stderr,
				    "fd %d active", pfd[i].fd);
			while (handle_connection(&psc[i], pfd[i].fd,
			    buf, Bflag) == -1) {
				pfd[i] = pfd[nfds - 1];
				pfd[nfds - 1].fd = -1;
				psc[i] = psc[nfds - 1];
				mainstats.nconns--;
				nfds--;
				/* stop display if no clients */
				if (!mainstats.nconns) {
					proc_slice = 1;
					set_timer(0);
				}
				/* if we were full */
				set_listening(pfd, lfds, 1);

				/* is there an event pending on the last fd? */
				if (pfd[i].fd == -1 ||
				    (pfd[i].revents & POLLIN) == 0)
					break;
			}
		}
	}
	exit(1);
}

void
clientconnect(struct addrinfo *aitop, struct pollfd *pfd, int nconn)
{
	char tmp[128];
	struct addrinfo *ai;
	int i, r, sock;

	for (i = 0; i < nconn; i++) {
		for (sock = -1, ai = aitop; ai != NULL; ai = ai->ai_next) {
			saddr_ntop(ai->ai_addr, ai->ai_addrlen, tmp,
			    sizeof(tmp));
			if (vflag && i == 0)
				fprintf(stderr, "Trying %s\n", tmp);
			if ((sock = socket(ai->ai_family, ai->ai_socktype,
			    ai->ai_protocol)) == -1) {
				if (ai->ai_next == NULL)
					err(1, "socket");
				if (vflag)
					warn("socket");
				continue;
			}
			if (rtableid && ai->ai_family == AF_INET) {
				if (setsockopt(sock, IPPROTO_IP, SO_RTABLE,
				    &rtableid, sizeof(rtableid)) == -1)
					err(1, "setsockopt SO_RTABLE");
			} else if (rtableid)
				warnx("rtable only supported on AF_INET");
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
		if (sock == -1)
			errx(1, "No host found");

		if ((r = fcntl(sock, F_GETFL, 0)) == -1)
			err(1, "fcntl(F_GETFL)");
		r |= O_NONBLOCK;
		if (fcntl(sock, F_SETFL, r) == -1)
			err(1, "fcntl(F_SETFL, O_NONBLOCK)");

		pfd[i].fd = sock;
		pfd[i].events = POLLOUT;
	}
	freeaddrinfo(aitop);

	if (vflag && nconn > 1)
		fprintf(stderr, "%u connections established\n", nconn);
}

static void __dead
clientloop(kvm_t *kvmh, u_long ktcbtab, struct addrinfo *aitop, int nconn)
{
	struct statctx *psc;
	struct pollfd *pfd;
	char *buf;
	int i;
	ssize_t n;

	if ((pfd = calloc(nconn, sizeof(*pfd))) == NULL)
		err(1, "clientloop pfd calloc");
	if ((psc = calloc(nconn, sizeof(*psc))) == NULL)
		err(1, "clientloop psc calloc");
	
	clientconnect(aitop, pfd, nconn);

	for (i = 0; i < nconn; i++) {
		stats_prepare(psc + i, pfd[i].fd, kvmh, ktcbtab);
		mainstats.nconns++;
	}

	if ((buf = malloc(Bflag)) == NULL)
		err(1, "malloc");
	arc4random_buf(buf, Bflag);

	print_header();
	set_timer(1);

	while (!done) {
		if (proc_slice) {
			process_slice(psc, nconn);
			stats_cleanslice();
			proc_slice = 0;
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
				stats_update(psc + i, n);
			}
		}
	}
	
	if (done > 0)
		warnx("Terminated by signal %d", done);

	free(buf);
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
	struct addrinfo *aitop, hints;
	const char *errstr;
	kvm_t *kvmh = NULL;
	struct rlimit rl;
	int ch, herr;
	struct nlist nl[] = { { "_tcbtable" }, { "" } };
	const char *host = NULL, *port = DEFAULT_PORT;
	int nconn = 1;

	Bflag = DEFAULT_BUF;
	Sflag = sflag = vflag = rtableid = 0;
	kflag = NULL;
	rflag = DEFAULT_STATS_INTERVAL;

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
			rtableid = (unsigned int)strtonum(optarg, 0,
			    RT_TABLEID_MAX, &errstr);
			if (errstr)
				errx(1, "rtable value is %s: %s",
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

	if (!sflag)
		host = argv[0];

	bzero(&hints, sizeof(hints));
	hints.ai_socktype = SOCK_STREAM;
	if (sflag)
		hints.ai_flags = AI_PASSIVE;
	if ((herr = getaddrinfo(host, port, &hints, &aitop)) != 0) {
		if (herr == EAI_SYSTEM)
			err(1, "getaddrinfo");
		else
			errx(1, "getaddrinfo: %s", gai_strerror(herr));
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

	signal(SIGINT, exitsighand);
	signal(SIGTERM, exitsighand);
	signal(SIGHUP, exitsighand);
	signal(SIGPIPE, SIG_IGN);
	signal(SIGALRM, alarmhandler);

	if (getrlimit(RLIMIT_NOFILE, &rl) == -1)
		err(1, "getrlimit");
	if (rl.rlim_cur < MAX_FD)
		rl.rlim_cur = MAX_FD;
	if (setrlimit(RLIMIT_NOFILE, &rl))
		err(1, "setrlimit");
	if (getrlimit(RLIMIT_NOFILE, &rl) == -1)
		err(1, "getrlimit");
	
	if (sflag)
		serverloop(kvmh, nl[0].n_value, aitop);
	else
		clientloop(kvmh, nl[0].n_value, aitop, nconn);

	return 0;
}
