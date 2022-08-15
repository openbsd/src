/*	$OpenBSD: tcpbench.c,v 1.67 2022/08/15 09:06:54 claudio Exp $	*/

/*
 * Copyright (c) 2008 Damien Miller <djm@mindrot.org>
 * Copyright (c) 2011 Christiano F. Haesbaert <haesbaert@haesbaert.org>
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
#include <sys/queue.h>
#include <sys/un.h>

#include <net/route.h>

#include <netinet/in.h>
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
#include <event.h>
#include <netdb.h>
#include <signal.h>
#include <err.h>
#include <fcntl.h>
#include <poll.h>
#include <paths.h>
#include <math.h>

#define DEFAULT_PORT "12345"
#define DEFAULT_STATS_INTERVAL 1000 /* ms */
#define DEFAULT_BUF (256 * 1024)
#define DEFAULT_UDP_PKT (1500 - 28) /* TODO don't hardcode this */
#define TCP_MODE !ptb->uflag
#define UDP_MODE ptb->uflag
#define MAX_FD 1024

/* Our tcpbench globals */
struct {
	int	  Dflag;	/* Socket debug */
	int	  Sflag;	/* Socket buffer size */
	u_int	  rflag;	/* Report rate (ms) */
	int	  sflag;	/* True if server */
	int	  Tflag;	/* ToS if != -1 */
	int	  vflag;	/* Verbose */
	int	  uflag;	/* UDP mode */
	int	  Uflag;	/* UNIX (AF_LOCAL) mode */
	int	  Rflag;	/* randomize client write size */
	char	**kvars;	/* Kvm enabled vars */
	char	 *dummybuf;	/* IO buffer */
	size_t	  dummybuf_len;	/* IO buffer len */
} tcpbench, *ptb;

struct tcpservsock {
	struct event ev;
	struct event evt;
	int fd;
};

/* stats for a single tcp connection, udp uses only one  */
struct statctx {
	TAILQ_ENTRY(statctx) entry;
	struct timeval t_start, t_last;
	unsigned long long bytes;
	int fd;
	char *buf;
	size_t buflen;
	struct event ev;
	/* TCP only */
	struct tcpservsock *tcp_ts;
	/* UDP only */
	u_long udp_slice_pkts;
};

struct statctx *udp_sc; /* singleton */

static void	signal_handler(int, short, void *);
static void	saddr_ntop(const struct sockaddr *, socklen_t, char *, size_t);
static void	set_slice_timer(int);
static void	print_tcp_header(void);
static void	list_kvars(void);
static void	check_kvar(const char *);
static char **	check_prepare_kvars(char *);
static void	stats_prepare(struct statctx *);
static void	summary_display(void);
static void	tcp_stats_display(unsigned long long, long double, float,
    struct statctx *, struct tcp_info *);
static void	tcp_process_slice(int, short, void *);
static void	tcp_server_handle_sc(int, short, void *);
static void	tcp_server_accept(int, short, void *);
static void	server_init(struct addrinfo *);
static void	client_handle_sc(int, short, void *);
static void	client_init(struct addrinfo *, int, struct addrinfo *);
static int	clock_gettime_tv(clockid_t, struct timeval *);
static void	udp_server_handle_sc(int, short, void *);
static void	udp_process_slice(int, short, void *);
static int	map_tos(char *, int *);
static void	quit(int, short, void *);
static void	wrapup(int);

/*
 * We account the mainstats here, that is the stats
 * for all connections, all variables starting with slice
 * are used to account information for the timeslice
 * between each output. Peak variables record the highest
 * between all slices so far.
 */
static struct {
	struct timeval t_first;		/* first connect / packet */
 	unsigned long long total_bytes; /* bytes since t_first */
	unsigned long long n_slices;	/* slices since start */
	unsigned long long slice_bytes; /* bytes since slice reset */
	long double peak_mbps;		/* peak mbps so far */
	long double floor_mbps;		/* floor mbps so far */
	long double mean_mbps;		/* mean mbps so far */
	long double nvariance_mbps;     /* for online std dev */
	int nconns;		        /* connected clients */
	struct event timer;		/* process timer */
	const char *host;               /* remote server for display */
} mainstats;

/* When adding variables, also add to tcp_stats_display() */
static const char *allowed_kvars[] = {
	"last_ack_recv",
	"last_ack_sent",
	"last_data_recv",
	"last_data_sent",
	"max_sndwnd",
	"options",
	"rcv_adv",
	"rcv_mss",
	"rcv_nxt",
	"rcv_ooopack",
	"rcv_space",
	"rcv_up",
	"rcv_wscale",
	"rcv_wscale",
	"rfbuf_cnt",
	"rfbuf_ts",
	"rtt",
	"rttmin",
	"rttvar",
	"snd_cwnd",
	"snd_max",
	"snd_mss",
	"snd_nxt",
	"snd_rexmitpack",
	"snd_ssthresh",
	"snd_una",
	"snd_wl1",
	"snd_wl2",
	"snd_wnd",
	"snd_wnd",
	"snd_wscale",
	"snd_wscale",
	"snd_zerowin",
	"so_rcv_sb_cc",
	"so_rcv_sb_hiwat",
	"so_rcv_sb_lowat",
	"so_rcv_sb_wat",
	"so_snd_sb_cc",
	"so_snd_sb_hiwat",
	"so_snd_sb_lowat",
	"so_snd_sb_wat",
	"ts_recent",
	"ts_recent_age",
	NULL
};

TAILQ_HEAD(, statctx) sc_queue;

static void __dead
usage(void)
{
	fprintf(stderr,
	    "usage: tcpbench -l\n"
	    "       tcpbench [-46DRUuv] [-B buf] [-b sourceaddr] [-k kvars] [-n connections]\n"
	    "                [-p port] [-r interval] [-S space] [-T toskeyword]\n"
	    "                [-t secs] [-V rtable] hostname\n"
	    "       tcpbench -s [-46DUuv] [-B buf] [-k kvars] [-p port] [-r interval]\n"
	    "                [-S space] [-T toskeyword] [-V rtable] [hostname]\n");
	exit(1);
}

static void
signal_handler(int sig, short event, void *bula)
{
	/*
	 * signal handler rules don't apply, libevent decouples for us
	 */
	switch (sig) {
	case SIGINFO:
		printf("\n");
		wrapup(-1);
		break;
	case SIGINT:
		printf("\n");
		wrapup(0);
		break;		/* NOTREACHED */
	case SIGTERM:
	case SIGHUP:
		warnx("Terminated by signal %d", sig);
		wrapup(0);
		break;		/* NOTREACHED */
	default:
		errx(1, "unexpected signal %d", sig);
		break;		/* NOTREACHED */
	}
}

static void
saddr_ntop(const struct sockaddr *addr, socklen_t alen, char *buf, size_t len)
{
	char hbuf[NI_MAXHOST], pbuf[NI_MAXSERV];
	int herr;

	if (addr->sa_family == AF_UNIX) {
		struct sockaddr_un *sun = (struct sockaddr_un *)addr;
		snprintf(buf, len, "%s", sun->sun_path);
		return;
	}
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
set_slice_timer(int on)
{
	struct timeval tv;

	if (ptb->rflag == 0)
		return;

	if (on) {
		if (evtimer_pending(&mainstats.timer, NULL))
			return;
		/* XXX Is there a better way to do this ? */
		tv.tv_sec = ptb->rflag / 1000;
		tv.tv_usec = (ptb->rflag % 1000) * 1000;

		evtimer_add(&mainstats.timer, &tv);
	} else if (evtimer_pending(&mainstats.timer, NULL))
		evtimer_del(&mainstats.timer);
}

static int
clock_gettime_tv(clockid_t clock_id, struct timeval *tv)
{
	struct timespec ts;

	if (clock_gettime(clock_id, &ts) == -1)
		return (-1);

	TIMESPEC_TO_TIMEVAL(tv, &ts);

	return (0);
}

static void
print_tcp_header(void)
{
	char **kv;

	if (ptb->rflag == 0)
		return;

	printf("%12s %14s %12s %8s ", "elapsed_ms", "bytes", "mbps",
	    "bwidth");
	for (kv = ptb->kvars;  ptb->kvars != NULL && *kv != NULL; kv++)
		printf("%s%s", kv != ptb->kvars ? "," : "", *kv);
	printf("\n");
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

	printf("Supported kernel variables:\n");
	for (i = 0; allowed_kvars[i] != NULL; i++)
		printf("\t%s\n", allowed_kvars[i]);
}

static char **
check_prepare_kvars(char *list)
{
	char *item, **ret = NULL;
	u_int n = 0;

	while ((item = strsep(&list, ", \t\n")) != NULL) {
		check_kvar(item);
		if ((ret = reallocarray(ret, (++n + 1), sizeof(*ret))) == NULL)
			err(1, "reallocarray(kvars)");
		if ((ret[n - 1] = strdup(item)) == NULL)
			err(1, "strdup");
		ret[n] = NULL;
	}
	return (ret);
}

static void
stats_prepare(struct statctx *sc)
{
	sc->buf = ptb->dummybuf;
	sc->buflen = ptb->dummybuf_len;

	if (clock_gettime_tv(CLOCK_MONOTONIC, &sc->t_start) == -1)
		err(1, "clock_gettime_tv");
	sc->t_last = sc->t_start;
	if (!timerisset(&mainstats.t_first))
		mainstats.t_first = sc->t_start;
}

static void
summary_display(void)
{
	struct timeval t_cur, t_diff;
	long double std_dev;
	unsigned long long total_elapsed;
	char *direction;

	if (!ptb->sflag) {
		direction = "sent";
		printf("--- %s tcpbench statistics ---\n", mainstats.host);
	} else {
		direction = "received";
		printf("--- tcpbench server statistics ---\n");
	}

	std_dev = sqrtl(mainstats.nvariance_mbps / mainstats.n_slices);

	if (clock_gettime_tv(CLOCK_MONOTONIC, &t_cur) == -1)
		err(1, "clock_gettime_tv");
	timersub(&t_cur, &mainstats.t_first, &t_diff);
	total_elapsed = t_diff.tv_sec * 1000 + t_diff.tv_usec / 1000;

	printf("%llu bytes %s over %.3Lf seconds\n",
	    mainstats.total_bytes, direction, total_elapsed/1000.0L);
	printf("bandwidth min/avg/max/std-dev = %.3Lf/%.3Lf/%.3Lf/%.3Lf Mbps\n",
	    mainstats.floor_mbps, mainstats.mean_mbps, mainstats.peak_mbps,
	    std_dev);
}

static void
tcp_stats_display(unsigned long long total_elapsed, long double mbps,
    float bwperc, struct statctx *sc, struct tcp_info *tcpi)
{
	int j;

	printf("%12llu %14llu %12.3Lf %7.2f%% ", total_elapsed, sc->bytes,
	    mbps, bwperc);

	if (ptb->kvars != NULL) {
		for (j = 0; ptb->kvars[j] != NULL; j++) {
#define S(a) #a
#define P(b, v, f)							\
			if (strcmp(ptb->kvars[j], S(v)) == 0) {		\
				printf("%s"f, j > 0 ? "," : "", b->tcpi_##v); \
				continue;				\
			}
			P(tcpi, last_ack_recv, "%u")
			P(tcpi, last_ack_sent, "%u")
			P(tcpi, last_data_recv, "%u")
			P(tcpi, last_data_sent, "%u")
			P(tcpi, max_sndwnd, "%u")
			P(tcpi, options, "%hhu")
			P(tcpi, rcv_adv, "%u")
			P(tcpi, rcv_mss, "%u")
			P(tcpi, rcv_nxt, "%u")
			P(tcpi, rcv_ooopack, "%u")
			P(tcpi, rcv_space, "%u")
			P(tcpi, rcv_up, "%u")
			P(tcpi, rcv_wscale, "%hhu")
			P(tcpi, rfbuf_cnt, "%u")
			P(tcpi, rfbuf_ts, "%u")
			P(tcpi, rtt, "%u")
			P(tcpi, rttmin, "%u")
			P(tcpi, rttvar, "%u")
			P(tcpi, snd_cwnd, "%u")
			P(tcpi, snd_max, "%u")
			P(tcpi, snd_mss, "%u")
			P(tcpi, snd_nxt, "%u")
			P(tcpi, snd_rexmitpack, "%u")
			P(tcpi, snd_ssthresh, "%u")
			P(tcpi, snd_una, "%u")
			P(tcpi, snd_wl1, "%u")
			P(tcpi, snd_wl2, "%u")
			P(tcpi, snd_wnd, "%u")
			P(tcpi, snd_wnd, "%u")
			P(tcpi, snd_wscale, "%hhu")
			P(tcpi, snd_zerowin, "%u")
			P(tcpi, so_rcv_sb_cc, "%u")
			P(tcpi, so_rcv_sb_hiwat, "%u")
			P(tcpi, so_rcv_sb_lowat, "%u")
			P(tcpi, so_rcv_sb_wat, "%u")
			P(tcpi, so_snd_sb_cc, "%u")
			P(tcpi, so_snd_sb_hiwat, "%u")
			P(tcpi, so_snd_sb_lowat, "%u")
			P(tcpi, so_snd_sb_wat, "%u")
			P(tcpi, ts_recent, "%u")
			P(tcpi, ts_recent_age, "%u")
#undef S
#undef P
		}
	}
	printf("\n");
}

static void
tcp_process_slice(int fd, short event, void *bula)
{
	unsigned long long total_elapsed, since_last;
	long double mbps, old_mean_mbps, slice_mbps = 0;
	float bwperc;
	struct statctx *sc;
	struct timeval t_cur, t_diff;
	struct tcp_info tcpi;
	socklen_t tcpilen;

	if (TAILQ_EMPTY(&sc_queue))
		return; /* don't pollute stats */

	mainstats.n_slices++;

	TAILQ_FOREACH(sc, &sc_queue, entry) {
		if (clock_gettime_tv(CLOCK_MONOTONIC, &t_cur) == -1)
			err(1, "clock_gettime_tv");
		if (ptb->kvars != NULL) { /* process kernel stats */
			tcpilen = sizeof(tcpi);
			if (getsockopt(sc->fd, IPPROTO_TCP, TCP_INFO,
			    &tcpi, &tcpilen) == -1)
				err(1, "get tcp_info");
		}

		timersub(&t_cur, &sc->t_start, &t_diff);
		total_elapsed = t_diff.tv_sec * 1000 + t_diff.tv_usec / 1000;
		timersub(&t_cur, &sc->t_last, &t_diff);
		since_last = t_diff.tv_sec * 1000 + t_diff.tv_usec / 1000;
		if (since_last == 0)
			continue;
		bwperc = (sc->bytes * 100.0) / mainstats.slice_bytes;
		mbps = (sc->bytes * 8) / (since_last * 1000.0);
		slice_mbps += mbps;

		tcp_stats_display(total_elapsed, mbps, bwperc, sc, &tcpi);

		sc->t_last = t_cur;
		sc->bytes = 0;
	}

	/* process stats for this slice */
	if (slice_mbps > mainstats.peak_mbps)
		mainstats.peak_mbps = slice_mbps;
	if (slice_mbps < mainstats.floor_mbps)
		mainstats.floor_mbps = slice_mbps;
	old_mean_mbps = mainstats.mean_mbps;
	mainstats.mean_mbps += (slice_mbps - mainstats.mean_mbps) /
				mainstats.n_slices;

	/* "Welford's method" for online variance
	 * see Knuth, TAoCP Volume 2, 3rd edn., p232 */
	mainstats.nvariance_mbps += (slice_mbps - old_mean_mbps) *
				    (slice_mbps - mainstats.mean_mbps);

	printf("Conn: %3d Mbps: %12.3Lf Peak Mbps: %12.3Lf Avg Mbps: %12.3Lf\n",
	    mainstats.nconns, slice_mbps, mainstats.peak_mbps,
	    mainstats.nconns ? slice_mbps / mainstats.nconns : 0);

	mainstats.slice_bytes = 0;
	set_slice_timer(mainstats.nconns > 0);
}

static void
udp_process_slice(int fd, short event, void *bula)
{
	unsigned long long total_elapsed, since_last, pps;
	long double old_mean_mbps, slice_mbps;
	struct timeval t_cur, t_diff;

	mainstats.n_slices++;

	if (clock_gettime_tv(CLOCK_MONOTONIC, &t_cur) == -1)
		err(1, "clock_gettime_tv");

	timersub(&t_cur, &udp_sc->t_start, &t_diff);
	total_elapsed = t_diff.tv_sec * 1000 + t_diff.tv_usec / 1000;

	timersub(&t_cur, &udp_sc->t_last, &t_diff);
	since_last = t_diff.tv_sec * 1000 + t_diff.tv_usec / 1000;
	if (since_last == 0)
		return;

	slice_mbps = (udp_sc->bytes * 8) / (since_last * 1000.0);
	pps = (udp_sc->udp_slice_pkts * 1000) / since_last;

	if (slice_mbps > mainstats.peak_mbps)
		mainstats.peak_mbps = slice_mbps;
	if (slice_mbps < mainstats.floor_mbps)
		mainstats.floor_mbps = slice_mbps;
	old_mean_mbps = mainstats.mean_mbps;
	mainstats.mean_mbps += (slice_mbps - mainstats.mean_mbps) /
				mainstats.n_slices;

	/* "Welford's method" for online variance
	 * see Knuth, TAoCP Volume 2, 3rd edn., p232 */
	mainstats.nvariance_mbps += (slice_mbps - old_mean_mbps) *
				    (slice_mbps - mainstats.mean_mbps);

	printf("Elapsed: %11llu Mbps: %11.3Lf Peak Mbps: %11.3Lf %s PPS: %7llu\n",
	    total_elapsed, slice_mbps, mainstats.peak_mbps,
	    ptb->sflag ? "Rx" : "Tx", pps);

	/* Clean up this slice time */
	udp_sc->t_last = t_cur;
	udp_sc->bytes = 0;
	udp_sc->udp_slice_pkts = 0;

	mainstats.slice_bytes = 0;
	set_slice_timer(1);
}

static void
udp_server_handle_sc(int fd, short event, void *bula)
{
	static int first_read = 1;
	ssize_t n;

	n = read(fd, ptb->dummybuf, ptb->dummybuf_len);
	if (n == 0)
		return;
	else if (n == -1) {
		if (errno != EINTR && errno != EWOULDBLOCK)
			warn("fd %d read error", fd);
		return;
	}

	if (ptb->vflag >= 3)
		fprintf(stderr, "read: %zd bytes\n", n);
	if (first_read) {
		first_read = 0;
		stats_prepare(udp_sc);
		set_slice_timer(1);
	}
	/* Account packet */
	udp_sc->udp_slice_pkts++;
	udp_sc->bytes += n;
	mainstats.slice_bytes += n;
	mainstats.total_bytes += n;
}

static void
tcp_server_handle_sc(int fd, short event, void *v_sc)
{
	struct statctx *sc = v_sc;
	ssize_t n;

	n = read(sc->fd, sc->buf, sc->buflen);
	if (n == -1) {
		if (errno != EINTR && errno != EWOULDBLOCK)
			warn("fd %d read error", sc->fd);
		return;
	} else if (n == 0) {
		if (ptb->vflag)
			fprintf(stderr, "%8d closed by remote end\n", sc->fd);

		TAILQ_REMOVE(&sc_queue, sc, entry);

		event_del(&sc->ev);
		close(sc->fd);

		/* Some file descriptors are available again. */
		if (evtimer_pending(&sc->tcp_ts->evt, NULL)) {
			evtimer_del(&sc->tcp_ts->evt);
			event_add(&sc->tcp_ts->ev, NULL);
		}

		free(sc);
		mainstats.nconns--;
		return;
	}
	if (ptb->vflag >= 3)
		fprintf(stderr, "read: %zd bytes\n", n);
	sc->bytes += n;
	mainstats.slice_bytes += n;
	mainstats.total_bytes += n;
}

static void
tcp_server_accept(int fd, short event, void *arg)
{
	struct tcpservsock *ts = arg;
	int sock;
	struct statctx *sc;
	struct sockaddr_storage ss;
	socklen_t sslen;
	char tmp[NI_MAXHOST + 2 + NI_MAXSERV];

	sslen = sizeof(ss);

	event_add(&ts->ev, NULL);
	if (event & EV_TIMEOUT)
		return;
	if ((sock = accept4(fd, (struct sockaddr *)&ss, &sslen, SOCK_NONBLOCK))
	    == -1) {
		/*
		 * Pause accept if we are out of file descriptors, or
		 * libevent will haunt us here too.
		 */
		if (errno == ENFILE || errno == EMFILE) {
			struct timeval evtpause = { 1, 0 };

			event_del(&ts->ev);
			evtimer_add(&ts->evt, &evtpause);
		} else if (errno != EWOULDBLOCK && errno != EINTR &&
		    errno != ECONNABORTED)
			warn("accept");
		return;
	}
	saddr_ntop((struct sockaddr *)&ss, sslen,
	    tmp, sizeof(tmp));
	if (ptb->Tflag != -1 && ss.ss_family == AF_INET) {
		if (setsockopt(sock, IPPROTO_IP, IP_TOS,
		    &ptb->Tflag, sizeof(ptb->Tflag)))
			err(1, "setsockopt IP_TOS");
	}
	if (ptb->Tflag != -1 && ss.ss_family == AF_INET6) {
		if (setsockopt(sock, IPPROTO_IPV6, IPV6_TCLASS,
		    &ptb->Tflag, sizeof(ptb->Tflag)))
			err(1, "setsockopt IPV6_TCLASS");
	}
	/* Alloc client structure and register reading callback */
	if ((sc = calloc(1, sizeof(*sc))) == NULL)
		err(1, "calloc");
	sc->tcp_ts = ts;
	sc->fd = sock;
	stats_prepare(sc);

	event_set(&sc->ev, sc->fd, EV_READ | EV_PERSIST,
	    tcp_server_handle_sc, sc);
	event_add(&sc->ev, NULL);
	TAILQ_INSERT_TAIL(&sc_queue, sc, entry);

	mainstats.nconns++;
	if (mainstats.nconns == 1)
		set_slice_timer(1);
	if (ptb->vflag)
		fprintf(stderr, "Accepted connection from %s, fd = %d\n",
		    tmp, sc->fd);
}

static void
server_init(struct addrinfo *aitop)
{
	int sock, on = 1;
	struct addrinfo *ai;
	struct event *ev;
	struct tcpservsock *ts;
	nfds_t lnfds;

	lnfds = 0;
	for (ai = aitop; ai != NULL; ai = ai->ai_next) {
		char tmp[NI_MAXHOST + 2 + NI_MAXSERV];

		saddr_ntop(ai->ai_addr, ai->ai_addrlen, tmp, sizeof(tmp));
		if (ptb->vflag)
			fprintf(stderr, "Try to bind to %s\n", tmp);
		if ((sock = socket(ai->ai_family, ai->ai_socktype,
		    ai->ai_protocol)) == -1) {
			if (ai->ai_next == NULL)
				err(1, "socket");
			if (ptb->vflag)
				warn("socket");
			continue;
		}
		if (ptb->Dflag) {
			if (setsockopt(sock, SOL_SOCKET, SO_DEBUG,
			    &ptb->Dflag, sizeof(ptb->Dflag)))
				err(1, "setsockopt SO_DEBUG");
		}
		if (ptb->Tflag != -1 && ai->ai_family == AF_INET) {
			if (setsockopt(sock, IPPROTO_IP, IP_TOS,
			    &ptb->Tflag, sizeof(ptb->Tflag)))
				err(1, "setsockopt IP_TOS");
		}
		if (ptb->Tflag != -1 && ai->ai_family == AF_INET6) {
			if (setsockopt(sock, IPPROTO_IPV6, IPV6_TCLASS,
			    &ptb->Tflag, sizeof(ptb->Tflag)))
				err(1, "setsockopt IPV6_TCLASS");
		}
		if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,
		    &on, sizeof(on)) == -1)
			warn("reuse port");
		if (bind(sock, ai->ai_addr, ai->ai_addrlen) != 0) {
			if (ai->ai_next == NULL)
				err(1, "bind");
			if (ptb->vflag)
				warn("bind");
			close(sock);
			continue;
		}
		if (ptb->Sflag) {
			if (setsockopt(sock, SOL_SOCKET, SO_RCVBUF,
			    &ptb->Sflag, sizeof(ptb->Sflag)) == -1)
				warn("set receive socket buffer size");
		}
		if (TCP_MODE) {
			if (listen(sock, 64) == -1) {
				if (ai->ai_next == NULL)
					err(1, "listen");
				if (ptb->vflag)
					warn("listen");
				close(sock);
				continue;
			}
		}
		if (UDP_MODE) {
			if ((ev = calloc(1, sizeof(*ev))) == NULL)
				err(1, "calloc");
			event_set(ev, sock, EV_READ | EV_PERSIST,
			    udp_server_handle_sc, NULL);
			event_add(ev, NULL);
		} else {
			if ((ts = calloc(1, sizeof(*ts))) == NULL)
				err(1, "calloc");

			ts->fd = sock;
			evtimer_set(&ts->evt, tcp_server_accept, ts);
			event_set(&ts->ev, ts->fd, EV_READ,
			    tcp_server_accept, ts);
			event_add(&ts->ev, NULL);
		}
		if (ptb->vflag >= 3)
			fprintf(stderr, "bound to fd %d\n", sock);
		lnfds++;
	}
	if (!ptb->Uflag)
		freeaddrinfo(aitop);
	if (lnfds == 0)
		errx(1, "No working listen addresses found");
}

static void
client_handle_sc(int fd, short event, void *v_sc)
{
	struct statctx *sc = v_sc;
	ssize_t n;
	size_t blen = sc->buflen;

	if (ptb->Rflag)
		blen = arc4random_uniform(blen) + 1;
	if ((n = write(sc->fd, sc->buf, blen)) == -1) {
		if (errno == EINTR || errno == EWOULDBLOCK ||
		    (UDP_MODE && errno == ENOBUFS))
			return;
		warn("write");
		wrapup(1);
	}
	if (TCP_MODE && n == 0) {
		fprintf(stderr, "Remote end closed connection");
		wrapup(1);
	}
	if (ptb->vflag >= 3)
		fprintf(stderr, "write: %zd bytes\n", n);
	sc->bytes += n;
	mainstats.slice_bytes += n;
	mainstats.total_bytes += n;
	if (UDP_MODE)
		sc->udp_slice_pkts++;
}

static void
client_init(struct addrinfo *aitop, int nconn, struct addrinfo *aib)
{
	struct statctx *sc;
	struct addrinfo *ai;
	int i, r, sock;

	for (i = 0; i < nconn; i++) {
		for (sock = -1, ai = aitop; ai != NULL; ai = ai->ai_next) {
			char tmp[NI_MAXHOST + 2 + NI_MAXSERV];

			saddr_ntop(ai->ai_addr, ai->ai_addrlen, tmp,
			    sizeof(tmp));
			if (ptb->vflag && i == 0)
				fprintf(stderr, "Trying %s\n", tmp);
			if ((sock = socket(ai->ai_family, ai->ai_socktype,
			    ai->ai_protocol)) == -1) {
				if (ai->ai_next == NULL)
					err(1, "socket");
				if (ptb->vflag)
					warn("socket");
				continue;
			}
			if (ptb->Dflag) {
				if (setsockopt(sock, SOL_SOCKET, SO_DEBUG,
				    &ptb->Dflag, sizeof(ptb->Dflag)))
					err(1, "setsockopt SO_DEBUG");
			}
			if (aib != NULL) {
				saddr_ntop(aib->ai_addr, aib->ai_addrlen,
				    tmp, sizeof(tmp));
				if (ptb->vflag)
					fprintf(stderr,
					    "Try to bind to %s\n", tmp);
				if (bind(sock, (struct sockaddr *)aib->ai_addr,
				    aib->ai_addrlen) == -1)
					err(1, "bind");
			}
			if (ptb->Tflag != -1 && ai->ai_family == AF_INET) {
				if (setsockopt(sock, IPPROTO_IP, IP_TOS,
				    &ptb->Tflag, sizeof(ptb->Tflag)))
					err(1, "setsockopt IP_TOS");
			}
			if (ptb->Tflag != -1 && ai->ai_family == AF_INET6) {
				if (setsockopt(sock, IPPROTO_IPV6, IPV6_TCLASS,
				    &ptb->Tflag, sizeof(ptb->Tflag)))
					err(1, "setsockopt IPV6_TCLASS");
			}
			if (ptb->Sflag) {
				if (setsockopt(sock, SOL_SOCKET, SO_SNDBUF,
				    &ptb->Sflag, sizeof(ptb->Sflag)) == -1)
					warn("set send socket buffer size");
			}
			if (connect(sock, ai->ai_addr, ai->ai_addrlen) != 0) {
				if (ai->ai_next == NULL)
					err(1, "connect");
				if (ptb->vflag)
					warn("connect");
				close(sock);
				sock = -1;
				continue;
			}
			break;
		}
		if (sock == -1)
			errx(1, "No host found");
		if ((r = fcntl(sock, F_GETFL)) == -1)
			err(1, "fcntl(F_GETFL)");
		r |= O_NONBLOCK;
		if (fcntl(sock, F_SETFL, r) == -1)
			err(1, "fcntl(F_SETFL, O_NONBLOCK)");
		/* Alloc and prepare stats */
		if (TCP_MODE) {
			if ((sc = calloc(1, sizeof(*sc))) == NULL)
				err(1, "calloc");
		} else
			sc = udp_sc;

		sc->fd = sock;
		stats_prepare(sc);

		event_set(&sc->ev, sc->fd, EV_WRITE | EV_PERSIST,
		    client_handle_sc, sc);
		event_add(&sc->ev, NULL);
		TAILQ_INSERT_TAIL(&sc_queue, sc, entry);

		mainstats.nconns++;
		if (mainstats.nconns == 1)
			set_slice_timer(1);
	}
	if (!ptb->Uflag)
		freeaddrinfo(aitop);
	if (aib != NULL)
		freeaddrinfo(aib);

	if (ptb->vflag && nconn > 1)
		fprintf(stderr, "%d connections established\n",
		    mainstats.nconns);
}

static int
map_tos(char *s, int *val)
{
	/* DiffServ Codepoints and other TOS mappings */
	const struct toskeywords {
		const char	*keyword;
		int		 val;
	} *t, toskeywords[] = {
		{ "af11",		IPTOS_DSCP_AF11 },
		{ "af12",		IPTOS_DSCP_AF12 },
		{ "af13",		IPTOS_DSCP_AF13 },
		{ "af21",		IPTOS_DSCP_AF21 },
		{ "af22",		IPTOS_DSCP_AF22 },
		{ "af23",		IPTOS_DSCP_AF23 },
		{ "af31",		IPTOS_DSCP_AF31 },
		{ "af32",		IPTOS_DSCP_AF32 },
		{ "af33",		IPTOS_DSCP_AF33 },
		{ "af41",		IPTOS_DSCP_AF41 },
		{ "af42",		IPTOS_DSCP_AF42 },
		{ "af43",		IPTOS_DSCP_AF43 },
		{ "critical",		IPTOS_PREC_CRITIC_ECP },
		{ "cs0",		IPTOS_DSCP_CS0 },
		{ "cs1",		IPTOS_DSCP_CS1 },
		{ "cs2",		IPTOS_DSCP_CS2 },
		{ "cs3",		IPTOS_DSCP_CS3 },
		{ "cs4",		IPTOS_DSCP_CS4 },
		{ "cs5",		IPTOS_DSCP_CS5 },
		{ "cs6",		IPTOS_DSCP_CS6 },
		{ "cs7",		IPTOS_DSCP_CS7 },
		{ "ef",			IPTOS_DSCP_EF },
		{ "inetcontrol",	IPTOS_PREC_INTERNETCONTROL },
		{ "lowdelay",		IPTOS_LOWDELAY },
		{ "netcontrol",		IPTOS_PREC_NETCONTROL },
		{ "reliability",	IPTOS_RELIABILITY },
		{ "throughput",		IPTOS_THROUGHPUT },
		{ NULL,			-1 },
	};

	for (t = toskeywords; t->keyword != NULL; t++) {
		if (strcmp(s, t->keyword) == 0) {
			*val = t->val;
			return (1);
		}
	}

	return (0);
}

static void
quit(int sig, short event, void *arg)
{
	wrapup(0);
}

static void
wrapup(int err)
{
	const int transfers = timerisset(&mainstats.t_first);
	const int stats = (mainstats.floor_mbps != INFINITY);

	if (transfers) {
		if (!stats) {
			if (UDP_MODE)
				udp_process_slice(0, 0, NULL);
			else
				tcp_process_slice(0, 0, NULL);
		}

		summary_display();
	}

	if (err != -1)
		exit(err);
}

int
main(int argc, char **argv)
{
	struct timeval tv;
	unsigned int secs, rtable;

	char kerr[_POSIX2_LINE_MAX], *tmp;
	struct addrinfo *aitop, *aib, hints;
	const char *errstr;
	struct rlimit rl;
	int ch, herr, nconn;
	int family = PF_UNSPEC;
	const char *host = NULL, *port = DEFAULT_PORT, *srcbind = NULL;
	struct event ev_sigint, ev_sigterm, ev_sighup, ev_siginfo, ev_progtimer;
	struct sockaddr_un sock_un;

	/* Init world */
	setvbuf(stdout, NULL, _IOLBF, 0);
	ptb = &tcpbench;
	ptb->dummybuf_len = 0;
	ptb->Dflag = 0;
	ptb->Sflag = ptb->sflag = ptb->vflag = ptb->Rflag = ptb->Uflag = 0;
	ptb->kvars = NULL;
	ptb->rflag = DEFAULT_STATS_INTERVAL;
	ptb->Tflag = -1;
	nconn = 1;
	aib = NULL;
	secs = 0;

	while ((ch = getopt(argc, argv, "46b:B:Dhlk:n:p:Rr:sS:t:T:uUvV:"))
	    != -1) {
		switch (ch) {
		case '4':
			family = PF_INET;
			break;
		case '6':
			family = PF_INET6;
			break;
		case 'b':
			srcbind = optarg;
			break;
		case 'D':
			ptb->Dflag = 1;
			break;
		case 'l':
			list_kvars();
			exit(0);
		case 'k':
			if ((tmp = strdup(optarg)) == NULL)
				err(1, "strdup");
			ptb->kvars = check_prepare_kvars(tmp);
			free(tmp);
			break;
		case 'R':
			ptb->Rflag = 1;
			break;
		case 'r':
			ptb->rflag = strtonum(optarg, 0, 60 * 60 * 24 * 1000,
			    &errstr);
			if (errstr != NULL)
				errx(1, "statistics interval is %s: %s",
				    errstr, optarg);
			break;
		case 'p':
			port = optarg;
			break;
		case 's':
			ptb->sflag = 1;
			break;
		case 'S':
			ptb->Sflag = strtonum(optarg, 0, 1024*1024*1024,
			    &errstr);
			if (errstr != NULL)
				errx(1, "socket buffer size is %s: %s",
				    errstr, optarg);
			break;
		case 'B':
			ptb->dummybuf_len = strtonum(optarg, 0, 1024*1024*1024,
			    &errstr);
			if (errstr != NULL)
				errx(1, "read/write buffer size is %s: %s",
				    errstr, optarg);
			break;
		case 'v':
			ptb->vflag++;
			break;
		case 'V':
			rtable = (unsigned int)strtonum(optarg, 0,
			    RT_TABLEID_MAX, &errstr);
			if (errstr)
				errx(1, "rtable value is %s: %s",
				    errstr, optarg);
			if (setrtable(rtable) == -1)
				err(1, "setrtable");
			break;
		case 'n':
			nconn = strtonum(optarg, 0, 65535, &errstr);
			if (errstr != NULL)
				errx(1, "number of connections is %s: %s",
				    errstr, optarg);
			break;
		case 'u':
			ptb->uflag = 1;
			break;
		case 'U':
			ptb->Uflag = 1;
			break;
		case 'T':
			if (map_tos(optarg, &ptb->Tflag))
				break;
			errstr = NULL;
			if (strlen(optarg) > 1 && optarg[0] == '0' &&
			    optarg[1] == 'x')
				ptb->Tflag = (int)strtol(optarg, NULL, 16);
			else
				ptb->Tflag = (int)strtonum(optarg, 0, 255,
				    &errstr);
			if (ptb->Tflag == -1 || ptb->Tflag > 255 || errstr)
				errx(1, "illegal tos value %s", optarg);
			break;
		case 't':
			secs = strtonum(optarg, 1, UINT_MAX, &errstr);
			if (errstr != NULL)
				errx(1, "secs is %s: %s",
				    errstr, optarg);
			break;
		case 'h':
		default:
			usage();
		}
	}

	if (pledge("stdio unveil rpath dns inet unix id", NULL) == -1)
		err(1, "pledge");

	argv += optind;
	argc -= optind;
	if ((argc != (ptb->sflag && !ptb->Uflag ? 0 : 1)) ||
	    (UDP_MODE && (ptb->kvars || nconn != 1)))
		usage();

	if (!ptb->sflag || ptb->Uflag)
		mainstats.host = host = argv[0];

	if (ptb->Uflag)
		if (unveil(host, "rwc") == -1)
			err(1, "unveil %s", host);

	if (pledge("stdio id dns inet unix", NULL) == -1)
		err(1, "pledge");

	/*
	 * Rationale,
	 * If TCP, use a big buffer with big reads/writes.
	 * If UDP, use a big buffer in server and a buffer the size of a
	 * ethernet packet.
	 */
	if (!ptb->dummybuf_len) {
		if (ptb->sflag || TCP_MODE)
			ptb->dummybuf_len = DEFAULT_BUF;
		else
			ptb->dummybuf_len = DEFAULT_UDP_PKT;
	}

	bzero(&hints, sizeof(hints));
	hints.ai_family = family;
	if (UDP_MODE) {
		hints.ai_socktype = SOCK_DGRAM;
		hints.ai_protocol = IPPROTO_UDP;
	} else {
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_protocol = IPPROTO_TCP;
	}
	if (ptb->Uflag) {
		hints.ai_family = AF_UNIX;
		hints.ai_protocol = 0;
		sock_un.sun_family = AF_UNIX;
		if (strlcpy(sock_un.sun_path, host, sizeof(sock_un.sun_path)) >=
		    sizeof(sock_un.sun_path))
			errx(1, "socket name '%s' too long", host);
		hints.ai_addr = (struct sockaddr *)&sock_un;
		hints.ai_addrlen = sizeof(sock_un);
		aitop = &hints;
	} else {
		if (ptb->sflag)
			hints.ai_flags = AI_PASSIVE;
		if (srcbind != NULL) {
			hints.ai_flags |= AI_NUMERICHOST;
			herr = getaddrinfo(srcbind, NULL, &hints, &aib);
			hints.ai_flags &= ~AI_NUMERICHOST;
			if (herr != 0) {
				if (herr == EAI_SYSTEM)
					err(1, "getaddrinfo");
				else
					errx(1, "getaddrinfo: %s",
					    gai_strerror(herr));
			}
		}
		if ((herr = getaddrinfo(host, port, &hints, &aitop)) != 0) {
			if (herr == EAI_SYSTEM)
				err(1, "getaddrinfo");
			else
				errx(1, "getaddrinfo: %s", gai_strerror(herr));
		}
	}

	if (pledge("stdio id inet unix", NULL) == -1)
		err(1, "pledge");

	if (getrlimit(RLIMIT_NOFILE, &rl) == -1)
		err(1, "getrlimit");
	if (rl.rlim_cur < MAX_FD)
		rl.rlim_cur = MAX_FD;
	if (setrlimit(RLIMIT_NOFILE, &rl))
		err(1, "setrlimit");
	if (getrlimit(RLIMIT_NOFILE, &rl) == -1)
		err(1, "getrlimit");

	if (pledge("stdio inet unix", NULL) == -1)
		err(1, "pledge");

	/* Init world */
	TAILQ_INIT(&sc_queue);
	if ((ptb->dummybuf = malloc(ptb->dummybuf_len)) == NULL)
		err(1, "malloc");
	arc4random_buf(ptb->dummybuf, ptb->dummybuf_len);

	timerclear(&mainstats.t_first);
	mainstats.floor_mbps = INFINITY;

	/* Setup libevent and signals */
	event_init();
	signal_set(&ev_sigterm, SIGTERM, signal_handler, NULL);
	signal_set(&ev_sighup, SIGHUP, signal_handler, NULL);
	signal_set(&ev_sigint, SIGINT, signal_handler, NULL);
	signal_set(&ev_siginfo, SIGINFO, signal_handler, NULL);
	signal_add(&ev_sigint, NULL);
	signal_add(&ev_sigterm, NULL);
	signal_add(&ev_sighup, NULL);
	signal_add(&ev_siginfo, NULL);
	signal(SIGPIPE, SIG_IGN);

	if (UDP_MODE) {
		if ((udp_sc = calloc(1, sizeof(*udp_sc))) == NULL)
			err(1, "calloc");
		udp_sc->fd = -1;
		evtimer_set(&mainstats.timer, udp_process_slice, NULL);
	} else {
		print_tcp_header();
		evtimer_set(&mainstats.timer, tcp_process_slice, NULL);
	}

	if (ptb->sflag)
		server_init(aitop);
	else {
		if (secs > 0) {
			timerclear(&tv);
			tv.tv_sec = secs + 1;
			evtimer_set(&ev_progtimer, quit, NULL);
			evtimer_add(&ev_progtimer, &tv);
		}
		client_init(aitop, nconn, aib);

		if (pledge("stdio inet", NULL) == -1)
			err(1, "pledge");
	}

	/* libevent main loop*/
	event_dispatch();

	return (0);
}
