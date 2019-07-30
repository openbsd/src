/*	$OpenBSD: ntp.c,v 1.146 2017/05/30 23:30:48 benno Exp $ */

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
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <paths.h>
#include <poll.h>
#include <pwd.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>
#include <err.h>

#include "ntpd.h"

#define	PFD_PIPE_MAIN	0
#define	PFD_PIPE_DNS	1
#define	PFD_SOCK_CTL	2
#define	PFD_MAX		3

volatile sig_atomic_t	 ntp_quit = 0;
struct imsgbuf		*ibuf_main;
struct imsgbuf		*ibuf_dns;
struct ntpd_conf	*conf;
struct ctl_conns	 ctl_conns;
u_int			 peer_cnt;
u_int			 sensors_cnt;
extern u_int		 constraint_cnt;

void	ntp_sighdlr(int);
int	ntp_dispatch_imsg(void);
int	ntp_dispatch_imsg_dns(void);
void	peer_add(struct ntp_peer *);
void	peer_remove(struct ntp_peer *);

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

void
ntp_main(struct ntpd_conf *nconf, struct passwd *pw, int argc, char **argv)
{
	int			 a, b, nfds, i, j, idx_peers, timeout;
	int			 nullfd, pipe_dns[2], idx_clients;
	int			 ctls;
	int			 fd_ctl;
	u_int			 pfd_elms = 0, idx2peer_elms = 0;
	u_int			 listener_cnt, new_cnt, sent_cnt, trial_cnt;
	u_int			 ctl_cnt;
	struct pollfd		*pfd = NULL;
	struct servent		*se;
	struct listen_addr	*la;
	struct ntp_peer		*p;
	struct ntp_peer		**idx2peer = NULL;
	struct ntp_sensor	*s, *next_s;
	struct constraint	*cstr;
	struct timespec		 tp;
	struct stat		 stb;
	struct ctl_conn		*cc;
	time_t			 nextaction, last_sensor_scan = 0, now;
	void			*newp;

	if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, PF_UNSPEC,
	    pipe_dns) == -1)
		fatal("socketpair");

	start_child(NTPDNS_PROC_NAME, pipe_dns[1], argc, argv);

	/* in this case the parent didn't init logging and didn't daemonize */
	if (nconf->settime && !nconf->debug) {
		log_init(nconf->debug, LOG_DAEMON);
		if (setsid() == -1)
			fatal("setsid");
	}
	log_procinit("ntp");

	if ((se = getservbyname("ntp", "udp")) == NULL)
		fatal("getservbyname");

	/* Start control socket. */
	if ((fd_ctl = control_init(CTLSOCKET)) == -1)
		fatalx("control socket init failed");
	if (control_listen(fd_ctl) == -1)
		fatalx("control socket listen failed");
	if ((nullfd = open("/dev/null", O_RDWR, 0)) == -1)
		fatal(NULL);

	if (stat(pw->pw_dir, &stb) == -1) {
		fatal("privsep dir %s could not be opened", pw->pw_dir);
	}
	if (stb.st_uid != 0 || (stb.st_mode & (S_IWGRP|S_IWOTH)) != 0) {
		fatalx("bad privsep dir %s permissions: %o",
		    pw->pw_dir, stb.st_mode);
	}
	if (chroot(pw->pw_dir) == -1)
		fatal("chroot");
	if (chdir("/") == -1)
		fatal("chdir(\"/\")");

	if (!nconf->debug) {
		dup2(nullfd, STDIN_FILENO);
		dup2(nullfd, STDOUT_FILENO);
		dup2(nullfd, STDERR_FILENO);
	}
	close(nullfd);

	setproctitle("ntp engine");

	conf = nconf;
	setup_listeners(se, conf, &listener_cnt);

	if (setgroups(1, &pw->pw_gid) ||
	    setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) ||
	    setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid))
		fatal("can't drop privileges");

	endservent();

	/* The ntp process will want to open NTP client sockets -> "inet" */
	if (pledge("stdio inet", NULL) == -1)
		err(1, "pledge");

	signal(SIGTERM, ntp_sighdlr);
	signal(SIGINT, ntp_sighdlr);
	signal(SIGPIPE, SIG_IGN);
	signal(SIGHUP, SIG_IGN);
	signal(SIGCHLD, SIG_DFL);

	if ((ibuf_main = malloc(sizeof(struct imsgbuf))) == NULL)
		fatal(NULL);
	imsg_init(ibuf_main, PARENT_SOCK_FILENO);
	if ((ibuf_dns = malloc(sizeof(struct imsgbuf))) == NULL)
		fatal(NULL);
	imsg_init(ibuf_dns, pipe_dns[0]);

	constraint_cnt = 0;
	conf->constraint_median = 0;
	conf->constraint_last = getmonotime();
	TAILQ_FOREACH(cstr, &conf->constraints, entry)
		constraint_cnt += constraint_init(cstr);

	TAILQ_FOREACH(p, &conf->ntp_peers, entry)
		client_peer_init(p);

	memset(&conf->status, 0, sizeof(conf->status));

	conf->freq.num = 0;
	conf->freq.samples = 0;
	conf->freq.x = 0.0;
	conf->freq.xx = 0.0;
	conf->freq.xy = 0.0;
	conf->freq.y = 0.0;
	conf->freq.overall_offset = 0.0;

	conf->status.synced = 0;
	clock_getres(CLOCK_REALTIME, &tp);
	b = 1000000000 / tp.tv_nsec;	/* convert to Hz */
	for (a = 0; b > 1; a--, b >>= 1)
		;
	conf->status.precision = a;
	conf->scale = 1;

	TAILQ_INIT(&ctl_conns);
	sensor_init();

	log_info("ntp engine ready");

	ctl_cnt = 0;
	peer_cnt = 0;
	TAILQ_FOREACH(p, &conf->ntp_peers, entry)
		peer_cnt++;

	while (ntp_quit == 0) {
		if (peer_cnt > idx2peer_elms) {
			if ((newp = reallocarray(idx2peer, peer_cnt,
			    sizeof(*idx2peer))) == NULL) {
				/* panic for now */
				log_warn("could not resize idx2peer from %u -> "
				    "%u entries", idx2peer_elms, peer_cnt);
				fatalx("exiting");
			}
			idx2peer = newp;
			idx2peer_elms = peer_cnt;
		}

		new_cnt = PFD_MAX +
		    peer_cnt + listener_cnt + ctl_cnt;
		if (new_cnt > pfd_elms) {
			if ((newp = reallocarray(pfd, new_cnt,
			    sizeof(*pfd))) == NULL) {
				/* panic for now */
				log_warn("could not resize pfd from %u -> "
				    "%u entries", pfd_elms, new_cnt);
				fatalx("exiting");
			}
			pfd = newp;
			pfd_elms = new_cnt;
		}

		memset(pfd, 0, sizeof(*pfd) * pfd_elms);
		memset(idx2peer, 0, sizeof(*idx2peer) * idx2peer_elms);
		nextaction = getmonotime() + 3600;
		pfd[PFD_PIPE_MAIN].fd = ibuf_main->fd;
		pfd[PFD_PIPE_MAIN].events = POLLIN;
		pfd[PFD_PIPE_DNS].fd = ibuf_dns->fd;
		pfd[PFD_PIPE_DNS].events = POLLIN;
		pfd[PFD_SOCK_CTL].fd = fd_ctl;
		pfd[PFD_SOCK_CTL].events = POLLIN;

		i = PFD_MAX;
		TAILQ_FOREACH(la, &conf->listen_addrs, entry) {
			pfd[i].fd = la->fd;
			pfd[i].events = POLLIN;
			i++;
		}

		idx_peers = i;
		sent_cnt = trial_cnt = 0;
		TAILQ_FOREACH(p, &conf->ntp_peers, entry) {
			if (constraint_cnt && conf->constraint_median == 0)
				continue;

			if (p->next > 0 && p->next <= getmonotime()) {
				if (p->state > STATE_DNS_INPROGRESS)
					trial_cnt++;
				if (client_query(p) == 0)
					sent_cnt++;
			}
			if (p->deadline > 0 && p->deadline <= getmonotime()) {
				timeout = 300;
				log_debug("no reply from %s received in time, "
				    "next query %ds", log_sockaddr(
				    (struct sockaddr *)&p->addr->ss), timeout);
				if (p->trustlevel >= TRUSTLEVEL_BADPEER &&
				    (p->trustlevel /= 2) < TRUSTLEVEL_BADPEER)
					log_info("peer %s now invalid",
					    log_sockaddr(
					    (struct sockaddr *)&p->addr->ss));
				client_nextaddr(p);
				set_next(p, timeout);
			}
			if (p->senderrors > MAX_SEND_ERRORS) {
				log_debug("failed to send query to %s, "
				    "next query %ds", log_sockaddr(
				    (struct sockaddr *)&p->addr->ss),
				    INTERVAL_QUERY_PATHETIC);
				p->senderrors = 0;
				client_nextaddr(p);
				set_next(p, INTERVAL_QUERY_PATHETIC);
			}
			if (p->next > 0 && p->next < nextaction)
				nextaction = p->next;
			if (p->deadline > 0 && p->deadline < nextaction)
				nextaction = p->deadline;

			if (p->state == STATE_QUERY_SENT &&
			    p->query->fd != -1) {
				pfd[i].fd = p->query->fd;
				pfd[i].events = POLLIN;
				idx2peer[i - idx_peers] = p;
				i++;
			}
		}
		idx_clients = i;

		if (!TAILQ_EMPTY(&conf->ntp_conf_sensors)) {
			if (last_sensor_scan == 0 ||
			    last_sensor_scan + SENSOR_SCAN_INTERVAL <= getmonotime()) {
				sensors_cnt = sensor_scan();
				last_sensor_scan = getmonotime();
			}
			if (sensors_cnt == 0 &&
			    nextaction > last_sensor_scan + SENSOR_SCAN_INTERVAL)
				nextaction = last_sensor_scan + SENSOR_SCAN_INTERVAL;
			sensors_cnt = 0;
			TAILQ_FOREACH(s, &conf->ntp_sensors, entry) {
				if (conf->settime && s->offsets[0].offset)
					priv_settime(s->offsets[0].offset);
				sensors_cnt++;
				if (s->next > 0 && s->next < nextaction)
					nextaction = s->next;
			}
		}

		if (conf->settime &&
		    ((trial_cnt > 0 && sent_cnt == 0) ||
		    (peer_cnt == 0 && sensors_cnt == 0)))
			priv_settime(0);	/* no good peers, don't wait */

		if (ibuf_main->w.queued > 0)
			pfd[PFD_PIPE_MAIN].events |= POLLOUT;
		if (ibuf_dns->w.queued > 0)
			pfd[PFD_PIPE_DNS].events |= POLLOUT;

		TAILQ_FOREACH(cc, &ctl_conns, entry) {
			pfd[i].fd = cc->ibuf.fd;
			pfd[i].events = POLLIN;
			if (cc->ibuf.w.queued > 0)
				pfd[i].events |= POLLOUT;
			i++;
		}
		ctls = i;

		TAILQ_FOREACH(cstr, &conf->constraints, entry) {
			if (constraint_query(cstr) == -1)
				continue;
		}

		now = getmonotime();
		if (constraint_cnt)
			nextaction = now + 1;

		timeout = nextaction - now;
		if (timeout < 0)
			timeout = 0;

		if ((nfds = poll(pfd, i, timeout * 1000)) == -1)
			if (errno != EINTR) {
				log_warn("poll error");
				ntp_quit = 1;
			}

		if (nfds > 0 && (pfd[PFD_PIPE_MAIN].revents & POLLOUT))
			if (msgbuf_write(&ibuf_main->w) <= 0 &&
			    errno != EAGAIN) {
				log_warn("pipe write error (to parent)");
				ntp_quit = 1;
			}

		if (nfds > 0 && pfd[PFD_PIPE_MAIN].revents & (POLLIN|POLLERR)) {
			nfds--;
			if (ntp_dispatch_imsg() == -1) {
				log_warn("pipe write error (from main)");
				ntp_quit = 1;
			}
		}

		if (nfds > 0 && (pfd[PFD_PIPE_DNS].revents & POLLOUT))
			if (msgbuf_write(&ibuf_dns->w) <= 0 &&
			    errno != EAGAIN) {
				log_warn("pipe write error (to dns engine)");
				ntp_quit = 1;
			}

		if (nfds > 0 && pfd[PFD_PIPE_DNS].revents & (POLLIN|POLLERR)) {
			nfds--;
			if (ntp_dispatch_imsg_dns() == -1) {
				log_warn("pipe write error (from dns engine)");
				ntp_quit = 1;
			}
		}

		if (nfds > 0 && pfd[PFD_SOCK_CTL].revents & (POLLIN|POLLERR)) {
			nfds--;
			ctl_cnt += control_accept(fd_ctl);
		}

		for (j = PFD_MAX; nfds > 0 && j < idx_peers; j++)
			if (pfd[j].revents & (POLLIN|POLLERR)) {
				nfds--;
				if (server_dispatch(pfd[j].fd, conf) == -1) {
					log_warn("pipe write error (conf)");
					ntp_quit = 1;
				}
			}

		for (; nfds > 0 && j < idx_clients; j++) {
			if (pfd[j].revents & (POLLIN|POLLERR)) {
				nfds--;
				if (client_dispatch(idx2peer[j - idx_peers],
				    conf->settime) == -1) {
					log_warn("pipe write error (settime)");
					ntp_quit = 1;
				}
			}
		}

		for (; nfds > 0 && j < ctls; j++) {
			nfds -= control_dispatch_msg(&pfd[j], &ctl_cnt);
		}

		for (s = TAILQ_FIRST(&conf->ntp_sensors); s != NULL;
		    s = next_s) {
			next_s = TAILQ_NEXT(s, entry);
			if (s->next <= getmonotime())
				sensor_query(s);
		}
	}

	msgbuf_write(&ibuf_main->w);
	msgbuf_clear(&ibuf_main->w);
	free(ibuf_main);
	msgbuf_write(&ibuf_dns->w);
	msgbuf_clear(&ibuf_dns->w);
	free(ibuf_dns);

	log_info("ntp engine exiting");
	exit(0);
}

int
ntp_dispatch_imsg(void)
{
	struct imsg		 imsg;
	int			 n;

	if (((n = imsg_read(ibuf_main)) == -1 && errno != EAGAIN) || n == 0)
		return (-1);

	for (;;) {
		if ((n = imsg_get(ibuf_main, &imsg)) == -1)
			return (-1);

		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_ADJTIME:
			memcpy(&n, imsg.data, sizeof(n));
			if (n == 1 && !conf->status.synced) {
				log_info("clock is now synced");
				conf->status.synced = 1;
			} else if (n == 0 && conf->status.synced) {
				log_info("clock is now unsynced");
				conf->status.synced = 0;
			}
			break;
		case IMSG_CONSTRAINT_RESULT:
			constraint_msg_result(imsg.hdr.peerid,
			    imsg.data, imsg.hdr.len - IMSG_HEADER_SIZE);
			break;
		case IMSG_CONSTRAINT_CLOSE:
			constraint_msg_close(imsg.hdr.peerid,
			    imsg.data, imsg.hdr.len - IMSG_HEADER_SIZE);
			break;
		default:
			break;
		}
		imsg_free(&imsg);
	}
	return (0);
}

int
ntp_dispatch_imsg_dns(void)
{
	struct imsg		 imsg;
	struct ntp_peer		*peer, *npeer;
	u_int16_t		 dlen;
	u_char			*p;
	struct ntp_addr		*h;
	int			 n;

	if (((n = imsg_read(ibuf_dns)) == -1 && errno != EAGAIN) || n == 0)
		return (-1);

	for (;;) {
		if ((n = imsg_get(ibuf_dns, &imsg)) == -1)
			return (-1);

		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_HOST_DNS:
			TAILQ_FOREACH(peer, &conf->ntp_peers, entry)
				if (peer->id == imsg.hdr.peerid)
					break;
			if (peer == NULL) {
				log_warnx("IMSG_HOST_DNS with invalid peerID");
				break;
			}
			if (peer->addr != NULL) {
				log_warnx("IMSG_HOST_DNS but addr != NULL!");
				break;
			}

			dlen = imsg.hdr.len - IMSG_HEADER_SIZE;
			if (dlen == 0) {	/* no data -> temp error */
				peer->state = STATE_DNS_TEMPFAIL;
				break;
			}

			p = (u_char *)imsg.data;
			while (dlen >= sizeof(struct sockaddr_storage)) {
				if ((h = calloc(1, sizeof(struct ntp_addr))) ==
				    NULL)
					fatal(NULL);
				memcpy(&h->ss, p, sizeof(h->ss));
				p += sizeof(h->ss);
				dlen -= sizeof(h->ss);
				if (peer->addr_head.pool) {
					npeer = new_peer();
					npeer->weight = peer->weight;
					npeer->query_addr4 = peer->query_addr4;
					npeer->query_addr6 = peer->query_addr6;
					h->next = NULL;
					npeer->addr = h;
					npeer->addr_head.a = h;
					npeer->addr_head.name =
					    peer->addr_head.name;
					npeer->addr_head.pool = 1;
					client_peer_init(npeer);
					npeer->state = STATE_DNS_DONE;
					peer_add(npeer);
				} else {
					h->next = peer->addr;
					peer->addr = h;
					peer->addr_head.a = peer->addr;
					peer->state = STATE_DNS_DONE;
				}
			}
			if (dlen != 0)
				fatalx("IMSG_HOST_DNS: dlen != 0");
			if (peer->addr_head.pool)
				peer_remove(peer);
			else
				client_addr_init(peer);
			break;
		case IMSG_CONSTRAINT_DNS:
			constraint_msg_dns(imsg.hdr.peerid,
			    imsg.data, imsg.hdr.len - IMSG_HEADER_SIZE);
			break;
		default:
			break;
		}
		imsg_free(&imsg);
	}
	return (0);
}

void
peer_add(struct ntp_peer *p)
{
	TAILQ_INSERT_TAIL(&conf->ntp_peers, p, entry);
	peer_cnt++;
}

void
peer_remove(struct ntp_peer *p)
{
	TAILQ_REMOVE(&conf->ntp_peers, p, entry);
	free(p);
	peer_cnt--;
}

static void
priv_adjfreq(double offset)
{
	double curtime, freq;

	if (!conf->status.synced){
		conf->freq.samples = 0;
		return;
	}

	conf->freq.samples++;

	if (conf->freq.samples <= 0)
		return;

	conf->freq.overall_offset += offset;
	offset = conf->freq.overall_offset;

	curtime = gettime_corrected();
	conf->freq.xy += offset * curtime;
	conf->freq.x += curtime;
	conf->freq.y += offset;
	conf->freq.xx += curtime * curtime;

	if (conf->freq.samples % FREQUENCY_SAMPLES != 0)
		return;

	freq =
	    (conf->freq.xy - conf->freq.x * conf->freq.y / conf->freq.samples)
	    /
	    (conf->freq.xx - conf->freq.x * conf->freq.x / conf->freq.samples);

	if (freq > MAX_FREQUENCY_ADJUST)
		freq = MAX_FREQUENCY_ADJUST;
	else if (freq < -MAX_FREQUENCY_ADJUST)
		freq = -MAX_FREQUENCY_ADJUST;

	imsg_compose(ibuf_main, IMSG_ADJFREQ, 0, 0, -1, &freq, sizeof(freq));
	conf->filters |= FILTER_ADJFREQ;
	conf->freq.xy = 0.0;
	conf->freq.x = 0.0;
	conf->freq.y = 0.0;
	conf->freq.xx = 0.0;
	conf->freq.samples = 0;
	conf->freq.overall_offset = 0.0;
	conf->freq.num++;
}

int
priv_adjtime(void)
{
	struct ntp_peer		 *p;
	struct ntp_sensor	 *s;
	int			  offset_cnt = 0, i = 0, j;
	struct ntp_offset	**offsets;
	double			  offset_median;

	TAILQ_FOREACH(p, &conf->ntp_peers, entry) {
		if (p->trustlevel < TRUSTLEVEL_BADPEER)
			continue;
		if (!p->update.good)
			return (1);
		offset_cnt += p->weight;
	}

	TAILQ_FOREACH(s, &conf->ntp_sensors, entry) {
		if (!s->update.good)
			continue;
		offset_cnt += s->weight;
	}

	if (offset_cnt == 0)
		return (1);

	if ((offsets = calloc(offset_cnt, sizeof(struct ntp_offset *))) == NULL)
		fatal("calloc priv_adjtime");

	TAILQ_FOREACH(p, &conf->ntp_peers, entry) {
		if (p->trustlevel < TRUSTLEVEL_BADPEER)
			continue;
		for (j = 0; j < p->weight; j++)
			offsets[i++] = &p->update;
	}

	TAILQ_FOREACH(s, &conf->ntp_sensors, entry) {
		if (!s->update.good)
			continue;
		for (j = 0; j < s->weight; j++)
			offsets[i++] = &s->update;
	}

	qsort(offsets, offset_cnt, sizeof(struct ntp_offset *), offset_compare);

	i = offset_cnt / 2;
	if (offset_cnt % 2 == 0)
		if (offsets[i - 1]->delay < offsets[i]->delay)
			i -= 1;
	offset_median = offsets[i]->offset;
	conf->status.rootdelay = offsets[i]->delay;
	conf->status.stratum = offsets[i]->status.stratum;
	conf->status.leap = offsets[i]->status.leap;

	imsg_compose(ibuf_main, IMSG_ADJTIME, 0, 0, -1,
	    &offset_median, sizeof(offset_median));

	priv_adjfreq(offset_median);

	conf->status.reftime = gettime();
	conf->status.stratum++;	/* one more than selected peer */
	if (conf->status.stratum > NTP_MAXSTRATUM)
		conf->status.stratum = NTP_MAXSTRATUM;
	update_scale(offset_median);

	conf->status.refid = offsets[i]->status.send_refid;

	free(offsets);

	TAILQ_FOREACH(p, &conf->ntp_peers, entry) {
		for (i = 0; i < OFFSET_ARRAY_SIZE; i++)
			p->reply[i].offset -= offset_median;
		p->update.good = 0;
	}
	TAILQ_FOREACH(s, &conf->ntp_sensors, entry) {
		for (i = 0; i < SENSOR_OFFSETS; i++)
			s->offsets[i].offset -= offset_median;
		s->update.offset -= offset_median;
	}

	return (0);
}

int
offset_compare(const void *aa, const void *bb)
{
	const struct ntp_offset * const *a;
	const struct ntp_offset * const *b;

	a = aa;
	b = bb;

	if ((*a)->offset < (*b)->offset)
		return (-1);
	else if ((*a)->offset > (*b)->offset)
		return (1);
	else
		return (0);
}

void
priv_settime(double offset)
{
	imsg_compose(ibuf_main, IMSG_SETTIME, 0, 0, -1,
	    &offset, sizeof(offset));
	conf->settime = 0;
}

void
priv_dns(int cmd, char *name, u_int32_t peerid)
{
	u_int16_t	dlen;

	dlen = strlen(name) + 1;
	imsg_compose(ibuf_dns, cmd, peerid, 0, -1, name, dlen);
}

void
update_scale(double offset)
{
	offset += getoffset();
	if (offset < 0)
		offset = -offset;

	if (offset > QSCALE_OFF_MAX || !conf->status.synced ||
	    conf->freq.num < 3)
		conf->scale = 1;
	else if (offset < QSCALE_OFF_MIN)
		conf->scale = QSCALE_OFF_MAX / QSCALE_OFF_MIN;
	else
		conf->scale = QSCALE_OFF_MAX / offset;
}

time_t
scale_interval(time_t requested)
{
	time_t interval, r;

	interval = requested * conf->scale;
	r = arc4random_uniform(MAXIMUM(5, interval / 10));
	return (interval + r);
}

time_t
error_interval(void)
{
	time_t interval, r;

	interval = INTERVAL_QUERY_PATHETIC * QSCALE_OFF_MAX / QSCALE_OFF_MIN;
	r = arc4random_uniform(interval / 10);
	return (interval + r);
}
