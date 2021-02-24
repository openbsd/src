/*
 * Copyright (c) 2021 Florian Obser <florian@openbsd.org>
 * Copyright (c) 2013 David Gwynne <dlg@openbsd.org>
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
#include <sys/event.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/syslog.h>
#include <sys/time.h>
#include <sys/un.h>

#include <arpa/inet.h>
#include <net/if.h>
#include <net/route.h>

#include <err.h>
#include <errno.h>
#include <event.h>
#include <poll.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define	ROUTE_SOCKET_BUF_SIZE	16384
#define	ASR_MAXNS		5
#define	ASR_RESCONF_SIZE	4096	/* maximum size asr will handle */
#define	UNWIND_SOCKET		"/dev/unwind.sock"
#define	RESOLV_CONF		"/etc/resolv.conf"
#define	RESOLVD_STATE		"/etc/resolvd.state"
#define	STARTUP_WAIT_TIMO	1

#ifndef nitems
#define	nitems(_a) (sizeof((_a)) / sizeof((_a)[0]))
#endif

__dead void	usage(void);

void		route_receive(int);
void		handle_route_message(struct rt_msghdr *, struct sockaddr **);
void		get_rtaddrs(int, struct sockaddr *, struct sockaddr **);
void		solicit_dns_proposals(int);
void		gen_resolvconf(void);
int		cmp(const void *, const void *);
int		parse_resolv_conf(void);
void		find_markers(void);

struct rdns_proposal {
	uint32_t	 if_index;
	int		 src;
	char		 ip[INET6_ADDRSTRLEN];
};

struct rdns_proposal	 active_proposal[ASR_MAXNS], new_proposal[ASR_MAXNS];
int			 startup_wait = 1, resolv_conf_kq = -1;
int			 resolvd_state_kq = -1;
uint8_t			 rsock_buf[ROUTE_SOCKET_BUF_SIZE];
FILE			*f_resolv_conf, *f_resolvd_state;
char			 resolv_conf[ASR_RESCONF_SIZE];
char			 new_resolv_conf[ASR_RESCONF_SIZE];
char			 resolvd_state[ASR_RESCONF_SIZE];
char			*resolv_conf_pre = resolv_conf;
char			*resolv_conf_our, *resolv_conf_post;

#ifndef SMALL
int			 open_unwind_ctl(void);
int			 check_unwind = 1, unwind_running = 0;

struct loggers {
	__dead void (*err)(int, const char *, ...)
	    __attribute__((__format__ (printf, 2, 3)));
	__dead void (*errx)(int, const char *, ...)
	    __attribute__((__format__ (printf, 2, 3)));
	void (*warn)(const char *, ...)
	    __attribute__((__format__ (printf, 1, 2)));
	void (*warnx)(const char *, ...)
	    __attribute__((__format__ (printf, 1, 2)));
	void (*info)(const char *, ...)
	    __attribute__((__format__ (printf, 1, 2)));
	void (*debug)(const char *, ...)
	    __attribute__((__format__ (printf, 1, 2)));
};

const struct loggers conslogger = {
	err,
	errx,
	warn,
	warnx,
	warnx, /* info */
	warnx /* debug */
};

__dead void	syslog_err(int, const char *, ...)
		    __attribute__((__format__ (printf, 2, 3)));
__dead void	syslog_errx(int, const char *, ...)
		    __attribute__((__format__ (printf, 2, 3)));
void		syslog_warn(const char *, ...)
		    __attribute__((__format__ (printf, 1, 2)));
void		syslog_warnx(const char *, ...)
		    __attribute__((__format__ (printf, 1, 2)));
void		syslog_info(const char *, ...)
		    __attribute__((__format__ (printf, 1, 2)));
void		syslog_debug(const char *, ...)
		    __attribute__((__format__ (printf, 1, 2)));
void		syslog_vstrerror(int, int, const char *, va_list)
		    __attribute__((__format__ (printf, 3, 0)));

const struct loggers syslogger = {
	syslog_err,
	syslog_errx,
	syslog_warn,
	syslog_warnx,
	syslog_info,
	syslog_debug
};

const struct loggers *logger = &conslogger;

#define lerr(_e, _f...) logger->err((_e), _f)
#define lerrx(_e, _f...) logger->errx((_e), _f)
#define lwarn(_f...) logger->warn(_f)
#define lwarnx(_f...) logger->warnx(_f)
#define linfo(_f...) logger->info(_f)
#define ldebug(_f...) logger->debug(_f)
#else
#define lerr(x...) do {} while(0)
#define lerrx(x...) do {} while(0)
#define lwarn(x...) do {} while(0)
#define lwarnx(x...) do {} while(0)
#define linfo(x...) do {} while(0)
#define ldebug(x...) do {} while(0)
#endif /* SMALL */

int
main(int argc, char *argv[])
{
	struct pollfd		 pfd[4];
	struct kevent		 ke;
	struct timespec		 now, stop, *timop = NULL, zero = {0, 0};
	struct timespec		 timeout = {STARTUP_WAIT_TIMO, 0};
	struct timespec		 one = {1, 0};
	int			 ch, debug = 0, verbose = 0, routesock;
	int			 rtfilter, nready, first_poll =1;

	while ((ch = getopt(argc, argv, "dEFs:v")) != -1) {
		switch (ch) {
		case 'd':
			debug = 1;
			break;
		case 'v':
			verbose++;
			break;
		default:
			usage();
		}
	}

	argc -= optind;
	argv += optind;
	if (argc > 0)
		usage();

	/* Check for root privileges. */
	if (geteuid())
		errx(1, "need root privileges");

	if (!debug)
		daemon(0, 0);

	if (!debug) {
		openlog("resolvd", LOG_PID|LOG_NDELAY, LOG_DAEMON);
		logger = &syslogger;
	}


	if ((routesock = socket(AF_ROUTE, SOCK_RAW, 0)) == -1)
		lerr(1, "route socket");

	rtfilter = ROUTE_FILTER(RTM_PROPOSAL) | ROUTE_FILTER(RTM_IFANNOUNCE);
	if (setsockopt(routesock, AF_ROUTE, ROUTE_MSGFILTER, &rtfilter,
	    sizeof(rtfilter)) == -1)
		lerr(1, "setsockopt(ROUTE_MSGFILTER)");

	if (unveil("/etc", "rwc") == -1)
		lerr(1, "unveil");
	if (unveil("/dev", "r") == -1)
		lerr(1, "unveil");

	if (pledge("stdio unix rpath wpath cpath fattr", NULL) == -1)
		lerr(1, "pledge");

	if ((resolv_conf_kq = kqueue()) == -1)
		lerr(1, "kqueue");
	if ((resolvd_state_kq = kqueue()) == -1)
		lerr(1, "kqueue");

	solicit_dns_proposals(routesock);
	pfd[0].fd = routesock;
	pfd[0].events = POLLIN;
	pfd[1].fd = resolv_conf_kq;
	pfd[1].events = POLLIN;
	pfd[2].fd = resolvd_state_kq;
	pfd[2].events = POLLIN;
	pfd[3].fd = -1;
	pfd[3].events = POLLIN;

	parse_resolv_conf();

	for(;;) {
#ifndef SMALL
		if (!unwind_running && check_unwind) {
			check_unwind = 0;
			pfd[3].fd = open_unwind_ctl();
			unwind_running = pfd[3].fd != -1;
			if (unwind_running)
				gen_resolvconf();
		}
#endif /* SMALL */
		nready = ppoll(pfd, nitems(pfd), timop, NULL);
		if (first_poll) {
			first_poll = 0;
			clock_gettime(CLOCK_REALTIME, &now);
			timespecadd(&now, &timeout, &stop);
			timop = &timeout;
		}
		if (timop != NULL) {
			clock_gettime(CLOCK_REALTIME, &now);
			if (timespeccmp(&stop, &now, <=)) {
				startup_wait = 0;
				timop = NULL;
				gen_resolvconf();
			} else
				timespecsub(&stop, &now, &timeout);
		}
		if (nready == -1) {
			if (errno == EINTR)
				continue;
			lerr(1, "poll");
		}

		if (nready == 0)
			continue;

		if ((pfd[0].revents & (POLLIN|POLLHUP)))
			route_receive(routesock);
		if ((pfd[1].revents & (POLLIN|POLLHUP))) {
			if ((nready = kevent(resolv_conf_kq, NULL, 0, &ke, 1,
			    &zero)) == -1) {
				close(resolv_conf_kq);
				fclose(f_resolv_conf);
				f_resolv_conf = NULL;
			}
			if (nready > 0) {
				if (ke.fflags & (NOTE_DELETE | NOTE_RENAME)) {
					fclose(f_resolv_conf);
					f_resolv_conf = NULL;
					parse_resolv_conf();
					gen_resolvconf();
				}
				if (ke.fflags & (NOTE_TRUNCATE | NOTE_WRITE)) {
					/* some editors truncate and write */
					if (ke.fflags & NOTE_TRUNCATE)
						nanosleep(&one, NULL);
					parse_resolv_conf();
					gen_resolvconf();
				}
			}
		}
		if ((pfd[2].revents & (POLLIN|POLLHUP))) {
			if ((nready = kevent(resolvd_state_kq, NULL, 0, &ke, 1,
			    &zero)) == -1) {
				close(resolvd_state_kq);
				fclose(f_resolv_conf);
				f_resolv_conf = NULL;
			}
			if (nready > 0) {

				if (ke.fflags & (NOTE_DELETE | NOTE_RENAME)) {
					fclose(f_resolvd_state);
					f_resolvd_state = NULL;
					parse_resolv_conf();
					gen_resolvconf();
				}
				if (ke.fflags & (NOTE_TRUNCATE | NOTE_WRITE)) {
					parse_resolv_conf();
					gen_resolvconf();
				}
			}
		}
#ifndef SMALL
		if ((pfd[3].revents & (POLLIN|POLLHUP))) {
			ssize_t	 n;
			if ((n = read(pfd[3].fd, rsock_buf, sizeof(rsock_buf))
			    == -1)) {
				if (errno == EAGAIN || errno == EINTR)
					continue;
			}
			if (n == 0 || n == -1) {
				if (n == -1)
					check_unwind = 1;
				close(pfd[3].fd);
				pfd[3].fd = -1;
				unwind_running = 0;
				gen_resolvconf();
			} else
				lwarnx("read %ld from unwind ctl", n);
		}
#endif /* SMALL */
	}
	return (0);
}

__dead void
usage(void)
{
	fprintf(stderr, "usage: resolvd [-dv]\n");
	exit(1);
}

void
route_receive(int fd)
{
	struct rt_msghdr	*rtm;
	struct sockaddr		*sa, *rti_info[RTAX_MAX];
	ssize_t			 n;

	rtm = (struct rt_msghdr *) rsock_buf;
	if ((n = read(fd, rsock_buf, sizeof(rsock_buf))) == -1) {
		if (errno == EAGAIN || errno == EINTR)
			return;
		lwarn("dispatch_rtmsg: read error");
		return;
	}

	if (n == 0)
		lerr(1, "routing socket closed");

	if (n < (ssize_t)sizeof(rtm->rtm_msglen) || n < rtm->rtm_msglen) {
		lwarnx("partial rtm of %zd in buffer", n);
		return;
	}

	if (rtm->rtm_version != RTM_VERSION)
		return;

	sa = (struct sockaddr *)(rsock_buf + rtm->rtm_hdrlen);
	get_rtaddrs(rtm->rtm_addrs, sa, rti_info);

	handle_route_message(rtm, rti_info);
}

void
handle_route_message(struct rt_msghdr *rtm, struct sockaddr **rti_info)
{
	struct sockaddr_rtdns		*rtdns;
	struct if_announcemsghdr	*ifan;
	int				 rdns_count, af, i, new = 0;
	char				*src;

	if (rtm->rtm_pid == getpid())
		return;

	memset(&new_proposal, 0, sizeof(new_proposal));

	switch (rtm->rtm_type) {
	case RTM_IFANNOUNCE:
		ifan = (struct if_announcemsghdr *)rtm;
		if (ifan->ifan_what == IFAN_ARRIVAL)
			return;
		for (i = 0; i < ASR_MAXNS; i++) {
			if (active_proposal[i].src == 0)
				continue;
			if (active_proposal[i].if_index == ifan->ifan_index)
				continue;
			memcpy(&new_proposal[new++], &active_proposal[i],
			    sizeof(struct rdns_proposal));
		}
		break;
	case RTM_PROPOSAL:
		if (rtm->rtm_priority == RTP_PROPOSAL_SOLICIT) {
#ifndef SMALL
			check_unwind = 1;
#endif /* SMALL */
			return;
		}
		if (!(rtm->rtm_addrs & RTA_DNS))
			return;

		rtdns = (struct sockaddr_rtdns*)rti_info[RTAX_DNS];
		src = rtdns->sr_dns;
		af = rtdns->sr_family;

		for (i = 0; i < ASR_MAXNS; i++) {
			if (active_proposal[i].src == 0)
				continue;
			/* rtm_index of zero means drop all proposals */
			if (active_proposal[i].src == rtm->rtm_priority &&
			    (active_proposal[i].if_index == rtm->rtm_index ||
			    rtm->rtm_index == 0))
				continue;
			memcpy(&new_proposal[new++], &active_proposal[i],
			    sizeof(struct rdns_proposal));
		}
		switch (af) {
		case AF_INET:
			if ((rtdns->sr_len - 2) % sizeof(struct in_addr) != 0) {
				lwarnx("ignoring invalid RTM_PROPOSAL");
				return;
			}
			rdns_count = (rtdns->sr_len - offsetof(struct
			    sockaddr_rtdns, sr_dns)) / sizeof(struct in_addr);
			break;
		case AF_INET6:
			if ((rtdns->sr_len - 2) % sizeof(struct in6_addr) != 0) {
				lwarnx("ignoring invalid RTM_PROPOSAL");
				return;
			}
			rdns_count = (rtdns->sr_len - offsetof(struct
			    sockaddr_rtdns, sr_dns)) / sizeof(struct in6_addr);
			break;
		default:
			lwarnx("ignoring invalid RTM_PROPOSAL");
			return;
		}

		for (i = 0; i < rdns_count && new < ASR_MAXNS; i++) {
			struct in_addr addr4;
			struct in6_addr addr6;
			switch (af) {
			case AF_INET:
				memcpy(&addr4, src, sizeof(struct in_addr));
				src += sizeof(struct in_addr);
				if (addr4.s_addr == INADDR_LOOPBACK)
					continue;
				if(inet_ntop(af, &addr4, new_proposal[new].ip,
				    INET6_ADDRSTRLEN) != NULL) {
					new_proposal[new].src =
					    rtm->rtm_priority;
					new_proposal[new].if_index =
					    rtm->rtm_index;
					new++;
				} else
					lwarn("inet_ntop");
				break;
			case AF_INET6:
				memcpy(&addr6, src, sizeof(struct in6_addr));
				src += sizeof(struct in6_addr);
				if (IN6_IS_ADDR_LOOPBACK(&addr6))
					continue;
				if(inet_ntop(af, &addr6, new_proposal[new].ip,
				    INET6_ADDRSTRLEN) != NULL) {
					new_proposal[new].src =
					    rtm->rtm_priority;
					new_proposal[new].if_index =
					    rtm->rtm_index;
					new++;
				} else
					lwarn("inet_ntop");
			}
		}
		break;
	default:
		return;
	}

	/* normalize to avoid fs churn when only the order changes */
	qsort(new_proposal, ASR_MAXNS, sizeof(new_proposal[0]), cmp);

	if (memcmp(new_proposal, active_proposal, sizeof(new_proposal)) !=
	    0) {
		memcpy(active_proposal, new_proposal, sizeof(new_proposal));
#ifndef SMALL
		if (!unwind_running)
#endif /* SMALL */
			gen_resolvconf();
	}

}

#define ROUNDUP(a) \
	((a) > 0 ? (1 + (((a) - 1) | (sizeof(long) - 1))) : sizeof(long))

void
get_rtaddrs(int addrs, struct sockaddr *sa, struct sockaddr **rti_info)
{
	int	i;

	for (i = 0; i < RTAX_MAX; i++) {
		if (addrs & (1 << i)) {
			rti_info[i] = sa;
			sa = (struct sockaddr *)((char *)(sa) +
			    ROUNDUP(sa->sa_len));
		} else
			rti_info[i] = NULL;
	}
}

void
solicit_dns_proposals(int routesock)
{
	struct rt_msghdr	 rtm;
	struct iovec		 iov[1];
	int			 iovcnt = 0;

	memset(&rtm, 0, sizeof(rtm));

	rtm.rtm_version = RTM_VERSION;
	rtm.rtm_type = RTM_PROPOSAL;
	rtm.rtm_msglen = sizeof(rtm);
	rtm.rtm_tableid = 0;
	rtm.rtm_index = 0;
	rtm.rtm_seq = arc4random();
	rtm.rtm_priority = RTP_PROPOSAL_SOLICIT;

	iov[iovcnt].iov_base = &rtm;
	iov[iovcnt++].iov_len = sizeof(rtm);

	if (writev(routesock, iov, iovcnt) == -1)
		lwarn("failed to send solicitation");
}

void
gen_resolvconf()
{
	size_t	 buf_len, state_buf_len;
	int	 len, state_len, i, fd;
	char	*buf_p = NULL, *state_buf_p = NULL;
	char	 tmpl[] = RESOLV_CONF"-XXXXXXXXXX";
	char	 state_tmpl[] = RESOLVD_STATE"-XXXXXXXXXX";

	if (resolv_conf_our == NULL || startup_wait)
		return;

	buf_len = sizeof(new_resolv_conf);
	buf_p = new_resolv_conf;
	memset(buf_p, 0, buf_len);

	state_buf_len = sizeof(resolvd_state);
	state_buf_p = resolvd_state;
	memset(state_buf_p, 0, state_buf_len);

	len = snprintf(buf_p, buf_len, "%.*s", (int)(resolv_conf_our -
	    resolv_conf_pre), resolv_conf_pre);
	if (len < 0 || (size_t)len > buf_len) {
		lwarnx("failed to generate new resolv.conf");
		return;
	}
	buf_p += len;
	buf_len -= len;

#ifndef SMALL
	if (unwind_running) {
		state_len = snprintf(state_buf_p, state_buf_len,
		    "nameserver 127.0.0.1\n");
		if (state_len < 0 || (size_t)state_len > state_buf_len) {
			lwarnx("failed to generate new resolv.conf");
			return;
		}
		state_buf_p += state_len;
		state_buf_len -= state_len;
	} else
#endif /* SMALL */
		for (i = 0; i < ASR_MAXNS; i++) {
			if (active_proposal[i].if_index != 0) {
				state_len = snprintf(state_buf_p, state_buf_len,
				    "nameserver %s\n", active_proposal[i].ip);
				if (state_len < 0 || (size_t)state_len >
				    state_buf_len) {
					lwarnx("failed to generate new "
					    "resolv.conf");
					return;
				}
				state_buf_p += state_len;
				state_buf_len -= state_len;
			}
		}

	len = snprintf(buf_p, buf_len, "%s", resolvd_state);
	if (len < 0 || (size_t)len > buf_len) {
		lwarnx("failed to generate new resolv.conf");
		return;
	}
	buf_p += len;
	buf_len -= len;

	len = snprintf(buf_p, buf_len, "%s", resolv_conf_post);
	if (len < 0 || (size_t)len > buf_len) {
		lwarnx("failed to generate new resolv.conf");
		return;
	}
	if (strcmp(new_resolv_conf, resolv_conf) == 0)
		return;

	ldebug("new resolv.conf:\n%s", new_resolv_conf);

	memcpy(resolv_conf, new_resolv_conf, sizeof(resolv_conf));

	if ((fd = mkstemp(tmpl)) == -1) {
		lwarn("mkstemp");
		return;
	}

	if (write(fd, resolv_conf, strlen(resolv_conf)) == -1)
		goto err;

	if (fchmod(fd, 0644) == -1)
		goto err;

	if (rename(tmpl, RESOLV_CONF) == -1)
		goto err;
	if (f_resolv_conf != NULL)
		fclose(f_resolv_conf);
	f_resolv_conf = NULL;

	close(fd);

	if ((fd = mkstemp(state_tmpl)) == -1) {
		lwarn("mkstemp");
		return;
	}

	if (write(fd, resolvd_state, strlen(resolvd_state)) == -1)
		goto err;

	if (fchmod(fd, 0644) == -1)
		goto err;

	if (rename(state_tmpl, RESOLVD_STATE) == -1)
		goto err;
	if(f_resolvd_state)
		fclose(f_resolvd_state);
	f_resolvd_state = NULL;

	close(fd);
	parse_resolv_conf();

	return;
 err:
	if (fd != -1)
		close(fd);
	unlink(tmpl);
}

int
cmp(const void *a, const void *b)
{
	const struct rdns_proposal	*rpa, *rpb;

	rpa = a;
	rpb = b;

	if (rpa->src == rpb->src)
		return strcmp(rpa->ip, rpb->ip);
	else
		return rpa->src < rpb->src ? -1 : 1;
}

int
parse_resolv_conf(void)
{
	struct kevent	 ke;
	ssize_t		 n;

	resolv_conf_our = NULL;
	resolv_conf_post = NULL;

	if (f_resolvd_state != NULL) {
		if (fseek(f_resolvd_state, 0, SEEK_SET) == -1) {
			lwarn("fseek");
			fclose(f_resolvd_state);
			f_resolvd_state = NULL;
		}
	}
	if (f_resolvd_state == NULL) {
		if ((f_resolvd_state = fopen(RESOLVD_STATE, "r")) == NULL)
			resolvd_state[0] = '\0';
		else {
			EV_SET(&ke, fileno(f_resolvd_state), EVFILT_VNODE,
			    EV_ENABLE | EV_ADD | EV_CLEAR, NOTE_DELETE |
			    NOTE_RENAME | NOTE_TRUNCATE | NOTE_WRITE, 0, NULL);
			if (kevent(resolvd_state_kq, &ke, 1, NULL, 0, NULL)
			    == -1)
				lwarn("kevent");
			n = fread(resolvd_state, 1, sizeof(resolvd_state) - 1,
			    f_resolvd_state);
			if (!feof(f_resolvd_state)) {
				fclose(f_resolvd_state);
				f_resolvd_state = NULL;
				lwarnx("/etc/resolvd.state too big");
				return -1;
			}
			if (ferror(f_resolvd_state)) {
				fclose(f_resolvd_state);
				f_resolvd_state = NULL;
				return -1;
			}

			resolvd_state[n] = '\0';
		}
	}

	/* -------------------- */

	if (f_resolv_conf != NULL) {
		if (fseek(f_resolv_conf, 0, SEEK_SET) == -1) {
			lwarn("fseek");
			fclose(f_resolv_conf);
			f_resolv_conf = NULL;
		}
	}
	if (f_resolv_conf == NULL) {
		if ((f_resolv_conf = fopen(RESOLV_CONF, "r")) == NULL) {
			if (errno == ENOENT) {
				/* treat as empty, which makes it ours */
				resolv_conf[0] = '\0';
				find_markers();
				return 0;
			}
			return -1;
		}

		EV_SET(&ke, fileno(f_resolv_conf), EVFILT_VNODE,
		    EV_ENABLE | EV_ADD | EV_CLEAR, NOTE_DELETE | NOTE_RENAME |
		    NOTE_TRUNCATE | NOTE_WRITE, 0, NULL);
		if (kevent(resolv_conf_kq, &ke, 1, NULL, 0, NULL) == -1)
			lwarn("kevent");
	}

	n = fread(resolv_conf, 1, sizeof(resolv_conf) - 1, f_resolv_conf);

	if (!feof(f_resolv_conf)) {
		fclose(f_resolv_conf);
		f_resolv_conf = NULL;
		lwarnx("/etc/resolv.conf too big");
		return -1;
	}
	if (ferror(f_resolv_conf)) {
		fclose(f_resolv_conf);
		f_resolv_conf = NULL;
		return -1;
	}

	resolv_conf[n] = '\0';

	find_markers();

	return 0;
}

void
find_markers(void)
{
	resolv_conf_post = NULL;
	if (*resolvd_state ==  '\0' && *resolv_conf_pre != '\0') {
		resolv_conf_our = NULL;
		return;
	}
	if (*resolv_conf_pre == '\0') {
		/* empty file, it's ours */
		resolv_conf_our = resolv_conf_post = resolv_conf_pre;
		return;
	}
	if ((resolv_conf_our = strstr(resolv_conf_pre, resolvd_state)) == NULL)
		return;

	resolv_conf_post = resolv_conf_our + strlen(resolvd_state);
}

#ifndef SMALL
int
open_unwind_ctl(void)
{
	static struct sockaddr_un	 sun;
	int				 s;

	if (sun.sun_family == 0) {
		sun.sun_family = AF_UNIX;
		strlcpy(sun.sun_path, UNWIND_SOCKET, sizeof(sun.sun_path));
	}

	if ((s = socket(AF_UNIX, SOCK_STREAM, 0)) != -1) {
		if (connect(s, (struct sockaddr *)&sun, sizeof(sun)) == -1) {
			close(s);
			s = -1;
		}
	}
	return s;
}

void
syslog_vstrerror(int e, int priority, const char *fmt, va_list ap)
{
	char *s;

	if (vasprintf(&s, fmt, ap) == -1) {
		syslog(LOG_EMERG, "unable to alloc in syslog_vstrerror");
		exit(1);
	}
	syslog(priority, "%s: %s", s, strerror(e));
	free(s);
}

__dead void
syslog_err(int ecode, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	syslog_vstrerror(errno, LOG_CRIT, fmt, ap);
	va_end(ap);
	exit(ecode);
}

__dead void
syslog_errx(int ecode, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vsyslog(LOG_CRIT, fmt, ap);
	va_end(ap);
	exit(ecode);
}

void
syslog_warn(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	syslog_vstrerror(errno, LOG_ERR, fmt, ap);
	va_end(ap);
}

void
syslog_warnx(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vsyslog(LOG_ERR, fmt, ap);
	va_end(ap);
}

void
syslog_info(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vsyslog(LOG_INFO, fmt, ap);
	va_end(ap);
}

void
syslog_debug(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vsyslog(LOG_DEBUG, fmt, ap);
	va_end(ap);
}
#endif /* SMALL */
