/*	$OpenBSD: ntpd.c,v 1.86 2015/01/14 21:14:27 naddy Exp $ */

/*
 * Copyright (c) 2003, 2004 Henning Brauer <henning@openbsd.org>
 * Copyright (c) 2012 Mike Miller <mmiller@mgm51.com>
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
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <errno.h>
#include <poll.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <err.h>

#include "ntpd.h"

void		sighdlr(int);
__dead void	usage(void);
int		main(int, char *[]);
int		check_child(pid_t, const char *);
int		dispatch_imsg(struct ntpd_conf *);
int		dispatch_imsg_ctl(struct ntpd_conf *);
void		reset_adjtime(void);
int		ntpd_adjtime(double);
void		ntpd_adjfreq(double, int);
void		ntpd_settime(double);
void		readfreq(void);
int		writefreq(double);
void		ctl_main(int, char*[]);
const char     *ctl_lookup_option(char *, const char **);
void		show_status_msg(struct imsg *);
void		show_peer_msg(struct imsg *, int);
void		show_sensor_msg(struct imsg *, int);

volatile sig_atomic_t	 quit = 0;
volatile sig_atomic_t	 reconfig = 0;
volatile sig_atomic_t	 sigchld = 0;
struct imsgbuf		*ibuf;
int			 timeout = INFTIM;

const char		*showopt;

static const char *ctl_showopt_list[] = {
	"peers", "Sensors", "status", "all", NULL
};

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

__dead void
usage(void)
{
	extern char *__progname;

	if (strcmp(__progname, "ntpctl") == 0)
		fprintf(stderr, "usage: ntpctl [-s all | peers | Sensors | status]\n");
	else
		fprintf(stderr, "usage: %s [-dnSsv] [-f file]\n",
		    __progname);
	exit(1);
}

#define POLL_MAX		8
#define PFD_PIPE		0

int
main(int argc, char *argv[])
{
	struct ntpd_conf	 lconf;
	struct pollfd		 pfd[POLL_MAX];
	pid_t			 chld_pid = 0, pid;
	const char		*conffile;
	int			 fd_ctl, ch, nfds;
	int			 pipe_chld[2];
	struct passwd		*pw;
	extern char		*__progname;

	if (strcmp(__progname, "ntpctl") == 0) {
		ctl_main (argc, argv);
		/* NOTREACHED */
	}

	conffile = CONFFILE;

	bzero(&lconf, sizeof(lconf));

	log_init(1);		/* log to stderr until daemonized */

	while ((ch = getopt(argc, argv, "df:nsSv")) != -1) {
		switch (ch) {
		case 'd':
			lconf.debug = 1;
			log_verbose(1);
			break;
		case 'f':
			conffile = optarg;
			break;
		case 'n':
			lconf.noaction = 1;
			break;
		case 's':
			lconf.settime = 1;
			break;
		case 'S':
			lconf.settime = 0;
			break;
		case 'v':
			log_verbose(1);
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

	if (parse_config(conffile, &lconf))
		exit(1);

	if (lconf.noaction) {
		fprintf(stderr, "configuration OK\n");
		exit(0);
	}

	if (geteuid())
		errx(1, "need root privileges");

	if ((pw = getpwnam(NTPD_USER)) == NULL)
		errx(1, "unknown user %s", NTPD_USER);

	if (setpriority(PRIO_PROCESS, 0, -20) == -1)
		warn("can't set priority");

	reset_adjtime();
	if (!lconf.settime) {
		log_init(lconf.debug);
		if (!lconf.debug)
			if (daemon(1, 0))
				fatal("daemon");
	} else
		timeout = SETTIME_TIMEOUT * 1000;

	if (socketpair(AF_UNIX, SOCK_STREAM, PF_UNSPEC, pipe_chld) == -1)
		fatal("socketpair");

	if ((fd_ctl = control_init(CTLSOCKET)) == -1)
		fatalx("control socket init failed");
	if (control_listen(fd_ctl) == -1)
		fatalx("control socket listen failed");

	signal(SIGCHLD, sighdlr);
	/* fork child process */
	chld_pid = ntp_main(pipe_chld, fd_ctl, &lconf, pw);

	setproctitle("[priv]");
	readfreq();

	signal(SIGTERM, sighdlr);
	signal(SIGINT, sighdlr);
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

		if (nfds == 0 && lconf.settime) {
			lconf.settime = 0;
			timeout = INFTIM;
			log_init(lconf.debug);
			log_warnx("no reply received in time, skipping initial "
			    "time setting");
			if (!lconf.debug)
				if (daemon(1, 0))
					fatal("daemon");
		}

		if (nfds > 0 && (pfd[PFD_PIPE].revents & POLLOUT))
			if (msgbuf_write(&ibuf->w) <= 0 && errno != EAGAIN) {
				log_warn("pipe write error (to child)");
				quit = 1;
			}

		if (nfds > 0 && pfd[PFD_PIPE].revents & POLLIN) {
			nfds--;
			if (dispatch_imsg(&lconf) == -1)
				quit = 1;
		}

		if (sigchld) {
			if (check_child(chld_pid, "child")) {
				quit = 1;
				chld_pid = 0;
			}
			sigchld = 0;
		}

	}

	signal(SIGCHLD, SIG_DFL);

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
	int	 status, sig;
	char	*signame;

	if (waitpid(pid, &status, WNOHANG) > 0) {
		if (WIFEXITED(status)) {
			log_warnx("Lost child: %s exited", pname);
			return (1);
		}
		if (WIFSIGNALED(status)) {
			sig = WTERMSIG(status);
			signame = strsignal(sig) ? strsignal(sig) : "unknown";
			log_warnx("Lost child: %s terminated; signal %d (%s)",
			    pname, sig, signame);
			return (1);
		}
	}

	return (0);
}

int
dispatch_imsg(struct ntpd_conf *lconf)
{
	struct imsg		 imsg;
	int			 n, cnt;
	double			 d;
	char			*name;
	struct ntp_addr		*h, *hn;
	struct ibuf		*buf;

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
				fatalx("invalid IMSG_ADJTIME received");
			memcpy(&d, imsg.data, sizeof(d));
			n = ntpd_adjtime(d);
			imsg_compose(ibuf, IMSG_ADJTIME, 0, 0, -1,
			     &n, sizeof(n));
			break;
		case IMSG_ADJFREQ:
			if (imsg.hdr.len != IMSG_HEADER_SIZE + sizeof(d))
				fatalx("invalid IMSG_ADJFREQ received");
			memcpy(&d, imsg.data, sizeof(d));
			ntpd_adjfreq(d, 1);
			break;
		case IMSG_SETTIME:
			if (imsg.hdr.len != IMSG_HEADER_SIZE + sizeof(d))
				fatalx("invalid IMSG_SETTIME received");
			if (!lconf->settime)
				break;
			log_init(lconf->debug);
			memcpy(&d, imsg.data, sizeof(d));
			ntpd_settime(d);
			/* daemonize now */
			if (!lconf->debug)
				if (daemon(1, 0))
					fatal("daemon");
			lconf->settime = 0;
			timeout = INFTIM;
			break;
		case IMSG_HOST_DNS:
			name = imsg.data;
			if (imsg.hdr.len < 1 + IMSG_HEADER_SIZE)
				fatalx("invalid IMSG_HOST_DNS received");
			imsg.hdr.len -= 1 + IMSG_HEADER_SIZE;
			if (name[imsg.hdr.len] != '\0' ||
			    strlen(name) != imsg.hdr.len)
				fatalx("invalid IMSG_HOST_DNS received");
			if ((cnt = host_dns(name, &hn)) == -1)
				break;
			buf = imsg_create(ibuf, IMSG_HOST_DNS,
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
					if (buf)
						imsg_close(ibuf, buf);
				}
				host_dns_free(hn);
				hn = NULL;
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
reset_adjtime(void)
{
	struct timeval	tv;

	timerclear(&tv);
	if (adjtime(&tv, NULL) == -1)
		log_warn("reset adjtime failed");
}

int
ntpd_adjtime(double d)
{
	struct timeval	tv, olddelta;
	int		synced = 0;
	static int	firstadj = 1;

	d += getoffset();
	if (d >= (double)LOG_NEGLIGIBLE_ADJTIME / 1000 ||
	    d <= -1 * (double)LOG_NEGLIGIBLE_ADJTIME / 1000)
		log_info("adjusting local clock by %fs", d);
	else
		log_debug("adjusting local clock by %fs", d);
	d_to_tv(d, &tv);
	if (adjtime(&tv, &olddelta) == -1)
		log_warn("adjtime failed");
	else if (!firstadj && olddelta.tv_sec == 0 && olddelta.tv_usec == 0)
		synced = 1;
	firstadj = 0;
	return (synced);
}

void
ntpd_adjfreq(double relfreq, int wrlog)
{
	int64_t curfreq;
	double ppmfreq;
	int r;

	if (adjfreq(NULL, &curfreq) == -1) {
		log_warn("adjfreq failed");
		return;
	}

	/*
	 * adjfreq's unit is ns/s shifted left 32; convert relfreq to
	 * that unit before adding. We log values in part per million.
	 */
	curfreq += relfreq * 1e9 * (1LL << 32);
	r = writefreq(curfreq / 1e9 / (1LL << 32));
	ppmfreq = relfreq * 1e6;
	if (wrlog) {
		if (ppmfreq >= LOG_NEGLIGIBLE_ADJFREQ ||
		    ppmfreq <= -LOG_NEGLIGIBLE_ADJFREQ)
			log_info("adjusting clock frequency by %f to %fppm%s",
			    ppmfreq, curfreq / 1e3 / (1LL << 32),
			    r ? "" : " (no drift file)");
		else
			log_debug("adjusting clock frequency by %f to %fppm%s",
			    ppmfreq, curfreq / 1e3 / (1LL << 32),
			    r ? "" : " (no drift file)");
	}

	if (adjfreq(&curfreq, NULL) == -1)
		log_warn("adjfreq failed");
}

void
ntpd_settime(double d)
{
	struct timeval	tv, curtime;
	char		buf[80];
	time_t		tval;

	if (gettimeofday(&curtime, NULL) == -1) {
		log_warn("gettimeofday");
		return;
	}
	d_to_tv(d, &tv);
	curtime.tv_usec += tv.tv_usec + 1000000;
	curtime.tv_sec += tv.tv_sec - 1 + (curtime.tv_usec / 1000000);
	curtime.tv_usec %= 1000000;

	if (settimeofday(&curtime, NULL) == -1) {
		log_warn("settimeofday");
		return;
	}
	tval = curtime.tv_sec;
	strftime(buf, sizeof(buf), "%a %b %e %H:%M:%S %Z %Y",
	    localtime(&tval));
	log_info("set local clock to %s (offset %fs)", buf, d);
}

void
readfreq(void)
{
	FILE *fp;
	int64_t current;
	double d;

	fp = fopen(DRIFTFILE, "r");
	if (fp == NULL) {
		/* if the drift file has been deleted by the user, reset */
		current = 0;
		if (adjfreq(&current, NULL) == -1)
			log_warn("adjfreq reset failed");
		return;
	}

	/* if we're adjusting frequency already, don't override */
	if (adjfreq(NULL, &current) == -1)
		log_warn("adjfreq failed");
	else if (current == 0) {
		if (fscanf(fp, "%lf", &d) == 1) {
			d /= 1e6;	/* scale from ppm */
			ntpd_adjfreq(d, 0);
		} else
			log_warnx("can't read %s", DRIFTFILE);
	}
	fclose(fp);
}

int
writefreq(double d)
{
	int r;
	FILE *fp;
	static int warnonce = 1;

	fp = fopen(DRIFTFILE, "w");
	if (fp == NULL) {
		if (warnonce) {
			log_warn("can't open %s", DRIFTFILE);
			warnonce = 0;
		}
		return 0;
	}

	fprintf(fp, "%.3f\n", d * 1e6);		/* scale to ppm */
	r = ferror(fp);
	if (fclose(fp) != 0 || r != 0) {
		if (warnonce) {
			log_warnx("can't write %s", DRIFTFILE);
			warnonce = 0;
		}
		unlink(DRIFTFILE);
		return 0;
	}
	return 1;
}

void
ctl_main(int argc, char *argv[])
{
	struct sockaddr_un	 sa;
	struct imsg		 imsg;
	struct imsgbuf		*ibuf_ctl;
	int			 fd, n, done, ch, action;
	char			*sockname;

	sockname = CTLSOCKET;

	if (argc < 2) {
		usage();
		/* NOTREACHED */
	}

	while ((ch = getopt(argc, argv, "s:")) != -1) {
		switch (ch) {
		case 's':
			showopt = ctl_lookup_option(optarg, ctl_showopt_list);
			if (showopt == NULL) {
				warnx("Unknown show modifier '%s'", optarg);
				usage();
			}
			break;
		default:
			usage();
			/* NOTREACHED */
		}
	}

	action = -1;
	if (showopt != NULL) {
		switch (*showopt) {
		case 'p':
			action = CTL_SHOW_PEERS;
			break;
		case 's':
			action = CTL_SHOW_STATUS;
			break;
		case 'S':
			action = CTL_SHOW_SENSORS;
			break;
		case 'a':
			action = CTL_SHOW_ALL;
			break;
		default:
			usage();
			/* NOTREACHED */
		}
	} else
		usage();

	if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
		err(1, "ntpctl: socket");

	bzero(&sa, sizeof(sa));
	sa.sun_family = AF_UNIX;
	if (strlcpy(sa.sun_path, sockname, sizeof(sa.sun_path)) >=
	    sizeof(sa.sun_path))
		errx(1, "ctl socket name too long");
	if (connect(fd, (struct sockaddr *)&sa, sizeof(sa)) == -1)
		err(1, "connect: %s", sockname);

	if ((ibuf_ctl = malloc(sizeof(struct imsgbuf))) == NULL)
		err(1, NULL);
	imsg_init(ibuf_ctl, fd);

	switch (action) {
	case CTL_SHOW_STATUS:
		imsg_compose(ibuf_ctl, IMSG_CTL_SHOW_STATUS,
		    0, 0, -1, NULL, 0);
		break;
	case CTL_SHOW_PEERS:
		imsg_compose(ibuf_ctl, IMSG_CTL_SHOW_PEERS,
		    0, 0, -1, NULL, 0);
		break;
	case CTL_SHOW_SENSORS:
		imsg_compose(ibuf_ctl, IMSG_CTL_SHOW_SENSORS,
		    0, 0, -1, NULL, 0);
		break;
	case CTL_SHOW_ALL:
		imsg_compose(ibuf_ctl, IMSG_CTL_SHOW_ALL,
		    0, 0, -1, NULL, 0);
		break;
	default:
		errx(1, "invalid action");
		break; /* NOTREACHED */
	}

	while (ibuf_ctl->w.queued)
		if (msgbuf_write(&ibuf_ctl->w) <= 0 && errno != EAGAIN)
			err(1, "ibuf_ctl: msgbuf_write error");

	done = 0;
	while (!done) {
		if ((n = imsg_read(ibuf_ctl)) == -1)
			err(1, "ibuf_ctl: imsg_read error");
		if (n == 0)
			errx(1, "ntpctl: pipe closed");

		while (!done) {
			if ((n = imsg_get(ibuf_ctl, &imsg)) == -1)
				err(1, "ibuf_ctl: imsg_get error");
			if (n == 0)
				break;

			switch (action) {
			case CTL_SHOW_STATUS:
				show_status_msg(&imsg);
				done = 1;
				break;
			case CTL_SHOW_PEERS:
				show_peer_msg(&imsg, 0);
				if (imsg.hdr.type ==
				    IMSG_CTL_SHOW_PEERS_END)
					done = 1;
				break;
			case CTL_SHOW_SENSORS:
				show_sensor_msg(&imsg, 0);
				if (imsg.hdr.type ==
				    IMSG_CTL_SHOW_SENSORS_END)
					done = 1;
				break;
			case CTL_SHOW_ALL:
				switch (imsg.hdr.type) {
				case IMSG_CTL_SHOW_STATUS:
					show_status_msg(&imsg);
					break;
				case IMSG_CTL_SHOW_PEERS:
					show_peer_msg(&imsg, 1);
					break;
				case IMSG_CTL_SHOW_SENSORS:
					show_sensor_msg(&imsg, 1);
					break;
				case IMSG_CTL_SHOW_PEERS_END:
				case IMSG_CTL_SHOW_SENSORS_END:
					/* do nothing */
					break;
				case IMSG_CTL_SHOW_ALL_END:
					done=1;
					break;
				default:
					/* no action taken */
					break;
				}
			default:
				/* no action taken */
				break;
			}
			imsg_free(&imsg);
		}
	}
	close(fd);
	free(ibuf_ctl);
	exit (0);
}

const char *
ctl_lookup_option(char *cmd, const char **list)
{
	const char *item = NULL;
	if (cmd != NULL && *cmd)
		for (; *list; list++)
			if (!strncmp(cmd, *list, strlen(cmd))) {
				if (item == NULL)
					item = *list;
				else
					errx(1, "%s is ambiguous", cmd);
			}
	return (item);
}

void
show_status_msg(struct imsg *imsg)
{
	struct ctl_show_status	*cstatus;
	double			 clock_offset;

	if (imsg->hdr.len != IMSG_HEADER_SIZE + sizeof(struct ctl_show_status))
		fatalx("invalid IMSG_CTL_SHOW_STATUS received");

	cstatus = (struct ctl_show_status *)imsg->data;

	if (cstatus->peercnt > 0)
		printf("%d/%d peers valid, ",
		    cstatus->valid_peers, cstatus->peercnt);

	if (cstatus->sensorcnt > 0)
		printf("%d/%d sensors valid, ",
		    cstatus->valid_sensors, cstatus->sensorcnt);

	if (cstatus->peercnt + cstatus->sensorcnt == 0)
		printf("no peers and no sensors configured\n");

	if (cstatus->synced == 1)
		printf("clock synced, stratum %u\n", cstatus->stratum);
	else {
		printf("clock unsynced");
		clock_offset = cstatus->clock_offset < 0 ?
		    -1.0 * cstatus->clock_offset : cstatus->clock_offset;
		if (clock_offset > 5e-7)
			printf(", clock offset is %.3fms\n",
			    cstatus->clock_offset);
		else
			printf("\n");
	}
}

void
show_peer_msg(struct imsg *imsg, int calledfromshowall)
{
	struct ctl_show_peer	*cpeer;
	int			 cnt;
	char			 stratum[3];
	static int		 firsttime = 1;

	if (imsg->hdr.type == IMSG_CTL_SHOW_PEERS_END) {
		if (imsg->hdr.len != IMSG_HEADER_SIZE + sizeof(cnt))
			fatalx("invalid IMSG_CTL_SHOW_PEERS_END received");
		memcpy(&cnt, imsg->data, sizeof(cnt));
		if (cnt == 0)
			printf("no peers configured\n");
		return;
	}

	if (imsg->hdr.len != IMSG_HEADER_SIZE + sizeof(struct ctl_show_peer))
		fatalx("invalid IMSG_CTL_SHOW_PEERS received");

	cpeer = (struct ctl_show_peer *)imsg->data;

	if (strlen(cpeer->peer_desc) > MAX_DISPLAY_WIDTH - 1)
		fatalx("peer_desc is too long");

	if (firsttime) {
		firsttime = 0;
		if (calledfromshowall)
			printf("\n");
		printf("peer\n   wt tl st  next  poll          "
		    "offset       delay      jitter\n");
	}

	if (cpeer->stratum > 0)
		snprintf(stratum, sizeof(stratum), "%2u", cpeer->stratum);
	else
		strlcpy (stratum, " -", sizeof (stratum));

	printf("%s\n %1s %2u %2u %2s %4llds %4llds",
	    cpeer->peer_desc, cpeer->syncedto == 1 ? "*" : " ",
	    cpeer->weight, cpeer->trustlevel, stratum,
	    (long long)cpeer->next, (long long)cpeer->poll);

	if (cpeer->trustlevel >= TRUSTLEVEL_BADPEER)
		printf("  %12.3fms %9.3fms  %8.3fms\n", cpeer->offset,
		    cpeer->delay, cpeer->jitter);
	else
		printf("             ---- peer not valid ----\n");

}

void
show_sensor_msg(struct imsg *imsg, int calledfromshowall)
{
	struct ctl_show_sensor	*csensor;
	int			 cnt;
	static int		 firsttime = 1;

	if (imsg->hdr.type == IMSG_CTL_SHOW_SENSORS_END) {
		if (imsg->hdr.len != IMSG_HEADER_SIZE + sizeof(cnt))
			fatalx("invalid IMSG_CTL_SHOW_SENSORS_END received");
		memcpy(&cnt, imsg->data, sizeof(cnt));
		if (cnt == 0)
			printf("no sensors configured\n");
		return;
	}

	if (imsg->hdr.len != IMSG_HEADER_SIZE + sizeof(struct ctl_show_sensor))
		fatalx("invalid IMSG_CTL_SHOW_SENSORS received");

	csensor = (struct ctl_show_sensor *)imsg->data;

	if (strlen(csensor->sensor_desc) > MAX_DISPLAY_WIDTH - 1)
		fatalx("sensor_desc is too long");

	if (firsttime) {
		firsttime = 0;
		if (calledfromshowall)
			printf("\n");
		printf("sensor\n   wt gd st  next  poll          "
		    "offset  correction\n");
	}

	printf("%s\n %1s %2u %2u %2u %4llds %4llds",
	    csensor->sensor_desc, csensor->syncedto == 1 ? "*" : " ",
	    csensor->weight, csensor->good, csensor->stratum,
	    (long long)csensor->next, (long long)csensor->poll);

	if (csensor->good == 1)
		printf("   %11.3fms %9.3fms\n",
		    csensor->offset, csensor->correction);
	else
		printf("         - sensor not valid -\n");

}
