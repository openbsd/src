/*
 * Copyright (c) 1983, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
static char copyright[] =
"@(#) Copyright (c) 1983, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
/*static char sccsid[] = "@(#)rwhod.c	8.1 (Berkeley) 6/6/93";*/
static char rcsid[] = "$OpenBSD: rwhod.c,v 1.25 2002/09/06 19:46:52 deraadt Exp $";
#endif /* not lint */

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/signal.h>
#include <sys/ioctl.h>
#include <sys/sysctl.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/route.h>
#include <netinet/in.h>
#include <protocols/rwhod.h>
#include <arpa/inet.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <netdb.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <utmp.h>

/*
 * Alarm interval. Don't forget to change the down time check in ruptime
 * if this is changed.
 */
#define AL_INTERVAL (3 * 60)

char	myname[MAXHOSTNAMELEN];

int	debug;

/*
 * We communicate with each neighbor in a list constructed at the time we're
 * started up.  Neighbors are currently directly connected via a hardware
 * interface.
 */
struct	neighbor {
	struct	neighbor *n_next;
	char	*n_name;		/* interface name */
	struct	sockaddr *n_addr;	/* who to send to */
	int	n_addrlen;		/* size of address */
	int	n_flags;		/* should forward?, interface flags */
};

struct	neighbor *neighbors;
struct	whod mywd;
struct	servent *sp;
int	s, utmpf;

volatile sig_atomic_t gothup;

#define	WHDRSIZE	(sizeof(mywd) - sizeof(mywd.wd_we))

int	 configure(int);
void	 getboottime(void);
void	 hup(int);
void	 timer(void);
void	 quit(char *);
void	 rt_xaddrs(caddr_t, caddr_t, struct rt_addrinfo *);
int	 verify(char *);
void	 handleread(int s);
int	 Sendto(int, const void *, size_t, int, const struct sockaddr *,
	    socklen_t);
char	*interval(int, char *);

void
hup(int signo)
{
	gothup = 1;
}

void
usage(void)
{
	fprintf(stderr, "usage: rwhod [-d]\n");
	exit(1);
}

int
main(int argc, char *argv[])
{
	struct timeval start, next, delta, now;
	struct sockaddr_in sin;
	struct pollfd pfd[1];
	int on = 1, ch;
	char *cp;

	while ((ch = getopt(argc, argv, "d")) != -1) {
		switch (ch) {
		case 'd':
			debug = 1;
			break;
		default:
			usage();
		}
	}

	if (getuid()) {
		fprintf(stderr, "rwhod: not super user\n");
		exit(1);
	}
	sp = getservbyname("who", "udp");
	if (sp == NULL) {
		fprintf(stderr, "rwhod: udp/who: unknown service\n");
		exit(1);
	}
	if (chdir(_PATH_RWHODIR) < 0) {
		(void)fprintf(stderr, "rwhod: %s: %s\n",
		    _PATH_RWHODIR, strerror(errno));
		exit(1);
	}
	if (!debug)
		daemon(1, 0);

	(void) signal(SIGHUP, hup);
	openlog("rwhod", LOG_PID, LOG_DAEMON);
	/*
	 * Establish host name as returned by system.
	 */
	if (gethostname(myname, sizeof(myname)) < 0) {
		syslog(LOG_ERR, "gethostname: %m");
		exit(1);
	}
	if ((cp = strchr(myname, '.')) != NULL)
		*cp = '\0';
	strncpy(mywd.wd_hostname, myname, sizeof(mywd.wd_hostname) - 1);
	mywd.wd_hostname[sizeof(mywd.wd_hostname) - 1] = '\0';
	utmpf = open(_PATH_UTMP, O_RDONLY|O_CREAT, 0644);
	if (utmpf < 0) {
		syslog(LOG_ERR, "%s: %m", _PATH_UTMP);
		exit(1);
	}
	getboottime();
	if ((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		syslog(LOG_ERR, "socket: %m");
		exit(1);
	}
	if (setsockopt(s, SOL_SOCKET, SO_BROADCAST, &on, sizeof(on)) < 0) {
		syslog(LOG_ERR, "setsockopt SO_BROADCAST: %m");
		exit(1);
	}
	memset(&sin, 0, sizeof(sin));
	sin.sin_addr.s_addr = INADDR_ANY;
	sin.sin_family = AF_INET;
	sin.sin_port = sp->s_port;
	if (bind(s, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
		syslog(LOG_ERR, "bind: %m");
		exit(1);
	}
	if (!configure(s))
		exit(1);
	timer();

	gettimeofday(&start, NULL);
	delta.tv_sec = AL_INTERVAL;
	delta.tv_usec = 0;
	timeradd(&start, &delta, &next);

	pfd[0].fd = s;
	pfd[0].events = POLLIN;

	for (;;) {
		int n;

		n = poll(pfd, 1, 1000);

		if (n == 1)
			handleread(s);

		if (gothup) {
			gothup = 0;
			getboottime();
		}

		gettimeofday(&now, NULL);
		if (timercmp(&now, &next, >)) {
			timer();
			timeradd(&now, &delta, &next);
		}
	}
}

void
handleread(int s)
{
	struct sockaddr_in from;
	struct stat st;
	char path[64];
	struct whod wd;
	int cc, whod;
	socklen_t len = sizeof(from);

	cc = recvfrom(s, (char *)&wd, sizeof(struct whod), 0,
	    (struct sockaddr *)&from, &len);
	if (cc <= 0) {
		if (cc < 0 && errno != EINTR)
			syslog(LOG_WARNING, "recv: %m");
		return;
	}
	if (from.sin_port != sp->s_port) {
		syslog(LOG_WARNING, "%d: bad source port from %s",
		    ntohs(from.sin_port), inet_ntoa(from.sin_addr));
		return;
	}
	if (cc < WHDRSIZE) {
		syslog(LOG_WARNING, "short packet from %s",
		    inet_ntoa(from.sin_addr));
		return;
	}
	if (wd.wd_vers != WHODVERSION)
		return;
	if (wd.wd_type != WHODTYPE_STATUS)
		return;
	wd.wd_hostname[sizeof(wd.wd_hostname)-1] = '\0';
	if (!verify(wd.wd_hostname)) {
		syslog(LOG_WARNING, "malformed host name from %s",
		    inet_ntoa(from.sin_addr));
		return;
	}
	if (debug)
		printf("host %s\n", wd.wd_hostname);

	(void) snprintf(path, sizeof path, "whod.%s", wd.wd_hostname);
	/*
	 * Rather than truncating and growing the file each time,
	 * use ftruncate if size is less than previous size.
	 */
	whod = open(path, O_WRONLY | O_CREAT, 0644);
	if (whod < 0) {
		syslog(LOG_WARNING, "%s: %m", path);
		return;
	}
#if ENDIAN != BIG_ENDIAN
	{
		int i, n = (cc - WHDRSIZE)/sizeof(struct whoent);
		struct whoent *we;

		/* undo header byte swapping before writing to file */
		wd.wd_sendtime = ntohl(wd.wd_sendtime);
		for (i = 0; i < 3; i++)
			wd.wd_loadav[i] = ntohl(wd.wd_loadav[i]);
		wd.wd_boottime = ntohl(wd.wd_boottime);
		we = wd.wd_we;
		for (i = 0; i < n; i++) {
			we->we_idle = ntohl(we->we_idle);
			we->we_utmp.out_time =
			    ntohl(we->we_utmp.out_time);
			we++;
		}
	}
#endif
	(void) time((time_t *)&wd.wd_recvtime);
	(void) write(whod, (char *)&wd, cc);
	if (fstat(whod, &st) < 0 || st.st_size > cc)
		ftruncate(whod, cc);
	(void) close(whod);
}

/*
 * Check out host name for unprintables
 * and other funnies before allowing a file
 * to be created.  Sorry, but blanks aren't allowed.
 */
int
verify(char *p)
{
	char c;

	/*
	 * Many people do not obey RFC 822 and 1035.  The valid
	 * characters are a-z, A-Z, 0-9, '-' and . But the others
	 * tested for below can happen, and we must be more permissive
	 * than the resolver until those idiots clean up their act.
	 */
	if (*p == '.' || *p == '-')
		return 0;
	while ((c = *p++)) {
		if (('a' <= c && c <= 'z') ||
		    ('A' <= c && c <= 'Z') ||
		    ('0' <= c && c <= '9'))
			continue;
		if (c == '.' && *p == '.')
			return 0;
		if (c == '.' || c == '-')
			continue;
		return 0;
	}
	return 1;
}

int	utmptime;
int	utmpent;
int	utmpsize = 0;
struct	utmp *utmp;
int	alarmcount;

void
timer(void)
{
	struct neighbor *np;
	struct whoent *we = mywd.wd_we, *wlast;
	int i;
	struct stat stb;
	double avenrun[3];
	time_t now;
	struct	utmp *nutmp;
	int cc;

	now = time(NULL);
	if (alarmcount % 10 == 0)
		getboottime();
	alarmcount++;
	(void) fstat(utmpf, &stb);
	if ((stb.st_mtime != utmptime) || (stb.st_size > utmpsize)) {
		utmptime = stb.st_mtime;
		if (stb.st_size > utmpsize) {
			utmpsize = stb.st_size + 10 * sizeof(struct utmp);
			if (utmp)
				nutmp = (struct utmp *)realloc(utmp, utmpsize);
			else
				nutmp = (struct utmp *)malloc(utmpsize);
			if (!nutmp) {
				fprintf(stderr, "rwhod: malloc failed\n");
				if (utmp)
					free(utmp);
				utmpsize = 0;
				return;
			}
			utmp = nutmp;
		}
		(void) lseek(utmpf, (off_t)0, SEEK_SET);
		cc = read(utmpf, (char *)utmp, stb.st_size);
		if (cc < 0) {
			fprintf(stderr, "rwhod: %s: %s\n",
			    _PATH_UTMP, strerror(errno));
			return;
		}
		wlast = &mywd.wd_we[1024 / sizeof(struct whoent) - 1];
		utmpent = cc / sizeof(struct utmp);
		for (i = 0; i < utmpent; i++)
			if (utmp[i].ut_name[0]) {
				memcpy(we->we_utmp.out_line, utmp[i].ut_line,
				   sizeof(utmp[i].ut_line));
				memcpy(we->we_utmp.out_name, utmp[i].ut_name,
				   sizeof(utmp[i].ut_name));
				we->we_utmp.out_time = htonl(utmp[i].ut_time);
				if (we >= wlast)
					break;
				we++;
			}
		utmpent = we - mywd.wd_we;
	}

	/*
	 * The test on utmpent looks silly---after all, if no one is
	 * logged on, why worry about efficiency?---but is useful on
	 * (e.g.) compute servers.
	 */
	if (utmpent && chdir(_PATH_DEV)) {
		syslog(LOG_ERR, "chdir(%s): %m", _PATH_DEV);
		exit(1);
	}
	we = mywd.wd_we;
	for (i = 0; i < utmpent; i++) {
		if (stat(we->we_utmp.out_line, &stb) >= 0)
			we->we_idle = htonl(now - stb.st_atime);
		we++;
	}
	(void)getloadavg(avenrun, sizeof(avenrun)/sizeof(avenrun[0]));
	for (i = 0; i < 3; i++)
		mywd.wd_loadav[i] = htonl((u_long)(avenrun[i] * 100));
	cc = (char *)we - (char *)&mywd;
printf("sending cc = %d\n", cc);
	mywd.wd_sendtime = htonl(time(0));
	mywd.wd_vers = WHODVERSION;
	mywd.wd_type = WHODTYPE_STATUS;
	for (np = neighbors; np != NULL; np = np->n_next)
		(void)Sendto(s, &mywd, cc, 0, np->n_addr, np->n_addrlen);
	if (utmpent && chdir(_PATH_RWHODIR)) {
		syslog(LOG_ERR, "chdir(%s): %m", _PATH_RWHODIR);
		exit(1);
	}
}

void
getboottime(void)
{
	int mib[2];
	size_t size;
	struct timeval tm;

	mib[0] = CTL_KERN;
	mib[1] = KERN_BOOTTIME;
	size = sizeof(tm);
	if (sysctl(mib, 2, &tm, &size, NULL, 0) == -1) {
		syslog(LOG_ERR, "cannot get boottime: %m");
		exit(1);
	}
	mywd.wd_boottime = htonl(tm.tv_sec);
}

void
quit(char *msg)
{
	syslog(LOG_ERR, "%s", msg);
	exit(1);
}

#define ROUNDUP(a) \
	((a) > 0 ? (1 + (((a) - 1) | (sizeof(long) - 1))) : sizeof(long))
#define ADVANCE(x, n) (x += ROUNDUP((n)->sa_len))

void
rt_xaddrs(caddr_t cp, caddr_t cplim, struct rt_addrinfo *rtinfo)
{
	struct sockaddr *sa;
	int i;

	memset(rtinfo->rti_info, 0, sizeof(rtinfo->rti_info));
	for (i = 0; (i < RTAX_MAX) && (cp < cplim); i++) {
		if ((rtinfo->rti_addrs & (1 << i)) == 0)
			continue;
		rtinfo->rti_info[i] = sa = (struct sockaddr *)cp;
		ADVANCE(cp, sa);
	}
}

/*
 * Figure out device configuration and select
 * networks which deserve status information.
 */
int
configure(int s)
{
	struct neighbor *np;
	struct if_msghdr *ifm;
	struct ifa_msghdr *ifam;
	struct sockaddr_dl *sdl;
	size_t needed;
	int mib[6], flags = 0, len;
	char *buf, *lim, *next;
	struct rt_addrinfo info;

	mib[0] = CTL_NET;
	mib[1] = PF_ROUTE;
	mib[2] = 0;
	mib[3] = AF_INET;
	mib[4] = NET_RT_IFLIST;
	mib[5] = 0;
	if (sysctl(mib, 6, NULL, &needed, NULL, 0) < 0)
		quit("route-sysctl-estimate");
	if ((buf = malloc(needed)) == NULL)
		quit("malloc");
	if (sysctl(mib, 6, buf, &needed, NULL, 0) < 0)
		quit("actual retrieval of interface table");
	lim = buf + needed;

	sdl = NULL;		/* XXX just to keep gcc -Wall happy */
	for (next = buf; next < lim; next += ifm->ifm_msglen) {
		ifm = (struct if_msghdr *)next;
		if (ifm->ifm_type == RTM_IFINFO) {
			sdl = (struct sockaddr_dl *)(ifm + 1);
			flags = ifm->ifm_flags;
			continue;
		}
		if ((flags & IFF_UP) == 0 ||
		    (flags & (IFF_BROADCAST|IFF_POINTOPOINT)) == 0)
			continue;
		if (ifm->ifm_type != RTM_NEWADDR)
			quit("out of sync parsing NET_RT_IFLIST");
		ifam = (struct ifa_msghdr *)ifm;
		info.rti_addrs = ifam->ifam_addrs;
		rt_xaddrs((char *)(ifam + 1), ifam->ifam_msglen + (char *)ifam,
			&info);
		/* gag, wish we could get rid of Internet dependencies */
#define dstaddr	info.rti_info[RTAX_BRD]
#define IPADDR_SA(x) ((struct sockaddr_in *)(x))->sin_addr.s_addr
#define PORT_SA(x) ((struct sockaddr_in *)(x))->sin_port
		if (dstaddr == 0 || dstaddr->sa_family != AF_INET)
			continue;
		PORT_SA(dstaddr) = sp->s_port;
		for (np = neighbors; np != NULL; np = np->n_next)
			if (memcmp(sdl->sdl_data, np->n_name,
				   sdl->sdl_nlen) == 0 &&
			    IPADDR_SA(np->n_addr) == IPADDR_SA(dstaddr))
				break;
		if (np != NULL)
			continue;
		len = sizeof(*np) + dstaddr->sa_len + sdl->sdl_nlen + 1;
		np = (struct neighbor *)malloc(len);
		if (np == NULL)
			quit("malloc of neighbor structure");
		memset(np, 0, len);
		np->n_flags = flags;
		np->n_addr = (struct sockaddr *)(np + 1);
		np->n_addrlen = dstaddr->sa_len;
		np->n_name = np->n_addrlen + (char *)np->n_addr;
		np->n_next = neighbors;
		neighbors = np;
		memcpy((char *)np->n_addr, (char *)dstaddr, np->n_addrlen);
		memcpy(np->n_name, sdl->sdl_data, sdl->sdl_nlen);
	}
	free(buf);
	return (1);
}

int
Sendto(int s, const void *buf, size_t cc, int flags,
    const struct sockaddr *to, socklen_t tolen)
{
	struct whod *w = (struct whod *)buf;
	struct whoent *we;
	struct sockaddr_in *sin = (struct sockaddr_in *)to;
	int ret;

	ret = sendto(s, buf, cc, flags, to, tolen);
	if (debug) {
		printf("sendto %s.%d\n", inet_ntoa(sin->sin_addr),
		    ntohs(sin->sin_port));
		printf("hostname %s %s\n", w->wd_hostname,
		    interval(ntohl(w->wd_sendtime) - ntohl(w->wd_boottime),
		    "  up"));
		printf("load %4.2f, %4.2f, %4.2f\n",
		    ntohl(w->wd_loadav[0]) / 100.0,
		    ntohl(w->wd_loadav[1]) / 100.0,
		    ntohl(w->wd_loadav[2]) / 100.0);
		cc -= WHDRSIZE;
		for (we = w->wd_we, cc /= sizeof(struct whoent); cc > 0;
		    cc--, we++) {
			time_t t = ntohl(we->we_utmp.out_time);
			printf("%-8.8s %s:%s %.12s",
			    we->we_utmp.out_name,
			    w->wd_hostname, we->we_utmp.out_line,
			    ctime(&t)+4);
			we->we_idle = ntohl(we->we_idle) / 60;
			if (we->we_idle) {
				if (we->we_idle >= 100*60)
					we->we_idle = 100*60 - 1;
				if (we->we_idle >= 60)
					printf(" %2d", we->we_idle / 60);
				else
					printf("   ");
				printf(":%02u", we->we_idle % 60);
			}
			printf("\n");
		}
	}
	return (ret);
}

char *
interval(int time, char *updown)
{
	static char resbuf[32];
	int days, hours, minutes;

	if (time < 0 || time > 3*30*24*60*60) {
		(void) snprintf(resbuf, sizeof(resbuf),
		    "   %s ??:??", updown);
		return (resbuf);
	}
	minutes = (time + 59) / 60;		/* round to minutes */
	hours = minutes / 60; minutes %= 60;
	days = hours / 24; hours %= 24;
	if (days)
		(void) snprintf(resbuf, sizeof(resbuf),
		    "%s %2d+%02d:%02d", updown, days, hours, minutes);
	else
		(void) snprintf(resbuf, sizeof(resbuf),
		    "%s    %2d:%02d", updown, hours, minutes);
	return (resbuf);
}
