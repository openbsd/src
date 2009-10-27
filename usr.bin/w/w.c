/*	$OpenBSD: w.c,v 1.47 2009/10/27 23:59:49 deraadt Exp $	*/

/*-
 * Copyright (c) 1980, 1991, 1993, 1994
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
 * 3. Neither the name of the University nor the names of its contributors
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

/*
 * w - print system status (who and what)
 *
 * This program is similar to the systat command on Tenex/Tops 10/20
 *
 */
#include <sys/param.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/tty.h>

#include <machine/cpu.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <kvm.h>
#include <netdb.h>
#include <nlist.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tzfile.h>
#include <unistd.h>
#include <limits.h>
#include <utmp.h>
#include <vis.h>

#include "extern.h"

struct timeval	boottime;
struct utmp	utmp;
struct winsize	ws;
kvm_t	       *kd;
time_t		now;		/* the current time of day */
int		ttywidth;	/* width of tty */
int		argwidth;	/* width of tty */
int		header = 1;	/* true if -h flag: don't print heading */
int		nflag = 1;	/* true if -n flag: don't convert addrs */
int		sortidle;	/* sort bu idle time */
char	       *sel_user;	/* login of particular user selected */
char		domain[MAXHOSTNAMELEN];

#define	NAME_WIDTH	8
#define HOST_WIDTH	16

/*
 * One of these per active utmp entry.
 */
struct	entry {
	struct	entry *next;
	struct	utmp utmp;
	dev_t	tdev;			/* dev_t of terminal */
	time_t	idle;			/* idle time of terminal in seconds */
	struct	kinfo_proc2 *kp;	/* `most interesting' proc */
} *ep, *ehead = NULL, **nextp = &ehead;

static void	 pr_args(struct kinfo_proc2 *);
static void	 pr_header(time_t *, int);
static struct stat
		*ttystat(char *);
static void	 usage(int);

int
main(int argc, char *argv[])
{
	extern char *__progname;
	struct kinfo_proc2 *kp;
	struct hostent *hp;
	struct stat *stp;
	FILE *ut;
	struct in_addr addr;
	int ch, i, nentries, nusers, wcmd;
	char *memf, *nlistf, *p, *x;
	char buf[MAXHOSTNAMELEN], errbuf[_POSIX2_LINE_MAX];

	/* Are we w(1) or uptime(1)? */
	p = __progname;
	if (*p == '-')
		p++;
	if (p[0] == 'w' && p[1] == '\0') {
		wcmd = 1;
		p = "hiflM:N:asuw";
	} else if (!strcmp(p, "uptime")) {
		wcmd = 0;
		p = "";
	} else
		errx(1,
		 "this program should be invoked only as \"w\" or \"uptime\"");

	memf = nlistf = NULL;
	while ((ch = getopt(argc, argv, p)) != -1)
		switch (ch) {
		case 'h':
			header = 0;
			break;
		case 'i':
			sortidle = 1;
			break;
		case 'M':
			header = 0;
			memf = optarg;
			break;
		case 'N':
			nlistf = optarg;
			break;
		case 'a':
			nflag = 0;
			break;
		case 'f': case 'l': case 's': case 'u': case 'w':
			warnx("[-flsuw] no longer supported");
			/* FALLTHROUGH */
		case '?':
		default:
			usage(wcmd);
		}
	argc -= optind;
	argv += optind;

	if (nlistf == NULL && memf == NULL) {
		if ((kd = kvm_openfiles(nlistf, memf, NULL, KVM_NO_FILES,
		    errbuf)) == NULL)
			errx(1, "%s", errbuf);
	} else {
		if ((kd = kvm_openfiles(nlistf, memf, NULL, O_RDONLY, errbuf)) == NULL)
			errx(1, "%s", errbuf);
	}

	(void)time(&now);
	if ((ut = fopen(_PATH_UTMP, "r")) == NULL)
		err(1, "%s", _PATH_UTMP);

	if (*argv)
		sel_user = *argv;

	for (nusers = 0; fread(&utmp, sizeof(utmp), 1, ut);) {
		if (utmp.ut_name[0] == '\0')
			continue;
		++nusers;
		if (wcmd == 0 || (sel_user &&
		    strncmp(utmp.ut_name, sel_user, UT_NAMESIZE) != 0))
			continue;
		if ((ep = calloc(1, sizeof(*ep))) == NULL)
			err(1, NULL);
		*nextp = ep;
		nextp = &(ep->next);
		memcpy(&(ep->utmp), &utmp, sizeof(utmp));
		if (!(stp = ttystat(ep->utmp.ut_line)))
			continue;
		ep->tdev = stp->st_rdev;
#ifdef CPU_CONSDEV
		/*
		 * If this is the console device, attempt to ascertain
		 * the true console device dev_t.
		 */
		if (ep->tdev == 0) {
			int mib[2];
			size_t size;

			mib[0] = CTL_MACHDEP;
			mib[1] = CPU_CONSDEV;
			size = sizeof(dev_t);
			(void) sysctl(mib, 2, &ep->tdev, &size, NULL, 0);
		}
#endif
		if ((ep->idle = now - stp->st_atime) < 0)
			ep->idle = 0;
	}
	(void)fclose(ut);

	if (header || wcmd == 0) {
		pr_header(&now, nusers);
		if (wcmd == 0)
			exit (0);
	}

#define HEADER	"USER    TTY FROM              LOGIN@  IDLE WHAT"
#define WUSED	(sizeof(HEADER) - sizeof("WHAT"))
	(void)puts(HEADER);

	kp = kvm_getproc2(kd, KERN_PROC_ALL, 0, sizeof(*kp), &nentries);
	if (kp == NULL)
		errx(1, "%s", kvm_geterr(kd));
	for (i = 0; i < nentries; i++, kp++) {
		if (kp->p_stat == SIDL || kp->p_stat == SZOMB)
			continue;
		for (ep = ehead; ep != NULL; ep = ep->next) {
			/* ftp is a special case. */
			if (strncmp(ep->utmp.ut_line, "ftp", 3) == 0) {
				char pidstr[UT_LINESIZE-2];
				pid_t fp;

				(void)strncpy(pidstr, &ep->utmp.ut_line[3],
				    sizeof(pidstr) - 1);
				pidstr[sizeof(pidstr) - 1] = '\0';
				fp = (pid_t)strtol(pidstr, NULL, 10);
				if (kp->p_pid == fp) {
					ep->kp = kp;
					break;
				}
			} else if (ep->tdev == kp->p_tdev &&
			    kp->p__pgid == kp->p_tpgid) {
				/*
				 * Proc is in foreground of this terminal
				 */
				if (proc_compare(ep->kp, kp))
					ep->kp = kp;
				break;
			}
		}
	}
	if ((ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 &&
	    ioctl(STDERR_FILENO, TIOCGWINSZ, &ws) == -1 &&
	    ioctl(STDIN_FILENO, TIOCGWINSZ, &ws) == -1) || ws.ws_col == 0)
		ttywidth = 79;
	else
		ttywidth = ws.ws_col - 1;
	argwidth = ttywidth - WUSED;
	if (argwidth < 4)
		argwidth = 8;
	/* sort by idle time */
	if (sortidle && ehead != NULL) {
		struct entry *from = ehead, *save;

		ehead = NULL;
		while (from != NULL) {
			for (nextp = &ehead;
			    (*nextp) && from->idle >= (*nextp)->idle;
			    nextp = &(*nextp)->next)
				continue;
			save = from;
			from = from->next;
			save->next = *nextp;
			*nextp = save;
		}
	}

	if (!nflag) {
		if (gethostname(domain, sizeof(domain)) < 0 ||
		    (p = strchr(domain, '.')) == 0)
			domain[0] = '\0';
		else {
			domain[sizeof(domain) - 1] = '\0';
			memmove(domain, p, strlen(p) + 1);
		}
	}

	for (ep = ehead; ep != NULL; ep = ep->next) {
		p = *ep->utmp.ut_host ? ep->utmp.ut_host : "-";
		for (x = NULL, i = 0; p[i] != '\0' && i < UT_HOSTSIZE; i++)
			if (p[i] == ':') {
				x = &p[i];
				*x++ = '\0';
				break;
			}
		if (!nflag && inet_aton(p, &addr) &&
		    (hp = gethostbyaddr((char *)&addr, sizeof(addr), AF_INET))) {
			if (domain[0] != '\0') {
				p = hp->h_name;
				p += strlen(hp->h_name);
				p -= strlen(domain);
				if (p > hp->h_name &&
				    strcasecmp(p, domain) == 0)
					*p = '\0';
			}
			p = hp->h_name;
		}
		if (x) {
			(void)snprintf(buf, sizeof(buf), "%s:%.*s", p,
			    (int)(ep->utmp.ut_host + UT_HOSTSIZE - x), x);
			p = buf;
		}
		(void)printf("%-*.*s %-2.2s %-*.*s ",
		    NAME_WIDTH, UT_NAMESIZE, ep->utmp.ut_name,
		    strncmp(ep->utmp.ut_line, "tty", 3) ?
		    ep->utmp.ut_line : ep->utmp.ut_line + 3,
		    HOST_WIDTH, HOST_WIDTH, *p ? p : "-");
		pr_attime(&ep->utmp.ut_time, &now);
		pr_idle(ep->idle);
		pr_args(ep->kp);
		printf("\n");
	}
	exit(0);
}

static void
pr_args(struct kinfo_proc2 *kp)
{
	char **argv, *str;
	int left;

	if (kp == NULL)
		goto nothing;		/* XXX - can this happen? */
	left = argwidth;
	argv = kvm_getargv2(kd, kp, argwidth+60);  /* +60 for ftpd snip */
	if (argv == NULL)
		goto nothing;

	if (*argv == NULL || **argv == '\0') {
		/* Process has zeroed argv[0], display executable name. */
		fmt_putc('(', &left);
		fmt_puts(kp->p_comm, &left);
		fmt_putc(')', &left);
	}
	while (*argv) {
		/*
		 * ftp argv[0] is in the following format:
		 * ftpd: HOSTNAME: [USER/PASS: ]CMD args (ftpd)
		 */
		if (strncmp(*argv, "ftpd:", 5) == 0) {
			if ((str = strchr(*argv + 5, ':')) != NULL)
				str = strchr(str + 1, ':');
			if (str != NULL) {
				if ((str[0] == ':') && isspace(str[1]))
					str += 2;
				fmt_puts(str, &left);
			} else
				fmt_puts(*argv, &left);
		} else
			fmt_puts(*argv, &left);
		argv++;
		fmt_putc(' ', &left);
	}
	return;
nothing:
	putchar('-');
}

static void
pr_header(time_t *nowp, int nusers)
{
	double avenrun[3];
	time_t uptime;
	int days, hrs, i, mins;
	int mib[2];
	size_t size;
	char buf[256];

	/*
	 * Print time of day.
	 */
	(void)strftime(buf, sizeof(buf) - 1, "%l:%M%p", localtime(nowp));
	buf[sizeof(buf) - 1] = '\0';
	(void)printf("%s ", buf);

	/*
	 * Print how long system has been up.
	 * (Found by looking getting "boottime" from the kernel)
	 */
	mib[0] = CTL_KERN;
	mib[1] = KERN_BOOTTIME;
	size = sizeof(boottime);
	if (sysctl(mib, 2, &boottime, &size, NULL, 0) != -1) {
		uptime = now - boottime.tv_sec;
		if (uptime > 59) {
			uptime += 30;
			days = uptime / SECSPERDAY;
			uptime %= SECSPERDAY;
			hrs = uptime / SECSPERHOUR;
			uptime %= SECSPERHOUR;
			mins = uptime / SECSPERMIN;
			(void)printf(" up");
			if (days > 0)
				(void)printf(" %d day%s,", days,
				    days > 1 ? "s" : "");
			if (hrs > 0 && mins > 0)
				(void)printf(" %2d:%02d,", hrs, mins);
			else {
				if (hrs > 0)
					(void)printf(" %d hr%s,",
					    hrs, hrs > 1 ? "s" : "");
				if (mins > 0 || (days == 0 && hrs == 0))
					(void)printf(" %d min%s,",
					    mins, mins != 1 ? "s" : "");
			}
		} else
			printf(" %d secs,", uptime);
	}

	/* Print number of users logged in to system */
	(void)printf(" %d user%s", nusers, nusers != 1 ? "s" : "");

	/*
	 * Print 1, 5, and 15 minute load averages.
	 */
	if (getloadavg(avenrun, sizeof(avenrun) / sizeof(avenrun[0])) == -1)
		(void)printf(", no load average information available\n");
	else {
		(void)printf(", load averages:");
		for (i = 0; i < (sizeof(avenrun) / sizeof(avenrun[0])); i++) {
			if (i > 0)
				(void)printf(",");
			(void)printf(" %.2f", avenrun[i]);
		}
		(void)printf("\n");
	}
}

static struct stat *
ttystat(char *line)
{
	static struct stat sb;
	char ttybuf[sizeof(_PATH_DEV) + UT_LINESIZE];

	/* Note, line may not be NUL-terminated */
	(void)strlcpy(ttybuf, _PATH_DEV, sizeof(ttybuf));
	(void)strncat(ttybuf, line, sizeof(ttybuf) - 1 - strlen(ttybuf));
	if (stat(ttybuf, &sb))
		return (NULL);
	return (&sb);
}

static void
usage(int wcmd)
{
	if (wcmd)
		(void)fprintf(stderr,
		    "usage: w [-ahi] [-M core] [-N system] [user]\n");
	else
		(void)fprintf(stderr,
		    "usage: uptime\n");
	exit (1);
}
