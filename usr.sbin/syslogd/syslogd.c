/*
 * Copyright (c) 1983, 1988, 1993, 1994
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
"@(#) Copyright (c) 1983, 1988, 1993, 1994\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
/*static char sccsid[] = "@(#)syslogd.c	8.3 (Berkeley) 4/4/94";*/
static char rcsid[] = "$NetBSD: syslogd.c,v 1.5 1996/01/02 17:48:41 perry Exp $";
#endif /* not lint */

/*
 *  syslogd -- log system messages
 *
 * This program implements a system log. It takes a series of lines.
 * Each line may have a priority, signified as "<n>" as
 * the first characters of the line.  If this is
 * not present, a default priority is used.
 *
 * To kill syslogd, send a signal 15 (terminate).  A signal 1 (hup) will
 * cause it to reread its configuration file.
 *
 * Defined Constants:
 *
 * MAXLINE -- the maximimum line length that can be handled.
 * DEFUPRI -- the default priority for user messages
 * DEFSPRI -- the default priority for kernel messages
 *
 * Author: Eric Allman
 * extensive changes by Ralph Campbell
 * more extensive changes by Eric Allman (again)
 */

#define	MAXLINE		1024		/* maximum line length */
#define	MAXSVLINE	120		/* maximum saved line length */
#define DEFUPRI		(LOG_USER|LOG_NOTICE)
#define DEFSPRI		(LOG_KERN|LOG_CRIT)
#define TIMERINTVL	30		/* interval for checking flush, mark */
#define TTYMSGTIME	1		/* timeout passed to ttymsg */

#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/msgbuf.h>
#include <sys/uio.h>
#include <sys/un.h>
#include <sys/time.h>
#include <sys/resource.h>

#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <utmp.h>
#include "pathnames.h"

#define SYSLOG_NAMES
#include <sys/syslog.h>

char	*ConfFile = _PATH_LOGCONF;
char	*PidFile = _PATH_LOGPID;
char	ctty[] = _PATH_CONSOLE;

#define	dprintf		if (Debug) printf

#define MAXUNAMES	20	/* maximum number of user names */

/*
 * Flags to logmsg().
 */

#define IGN_CONS	0x001	/* don't print on console */
#define SYNC_FILE	0x002	/* do fsync on file after printing */
#define ADDDATE		0x004	/* add a date to the message */
#define MARK		0x008	/* this message is a mark */

/*
 * This structure represents the files that will have log
 * copies printed.
 */

struct filed {
	struct	filed *f_next;		/* next in linked list */
	short	f_type;			/* entry type, see below */
	short	f_file;			/* file descriptor */
	time_t	f_time;			/* time this was last written */
	u_char	f_pmask[LOG_NFACILITIES+1];	/* priority mask */
	char	*f_program;		/* program this applies to */
	union {
		char	f_uname[MAXUNAMES][UT_NAMESIZE+1];
		struct {
			char	f_hname[MAXHOSTNAMELEN];
			struct sockaddr_in	f_addr;
		} f_forw;		/* forwarding address */
		char	f_fname[MAXPATHLEN];
	} f_un;
	char	f_prevline[MAXSVLINE];		/* last message logged */
	char	f_lasttime[16];			/* time of last occurrence */
	char	f_prevhost[MAXHOSTNAMELEN];	/* host from which recd. */
	int	f_prevpri;			/* pri of f_prevline */
	int	f_prevlen;			/* length of f_prevline */
	int	f_prevcount;			/* repetition cnt of prevline */
	int	f_repeatcount;			/* number of "repeated" msgs */
};

/*
 * Intervals at which we flush out "message repeated" messages,
 * in seconds after previous message is logged.  After each flush,
 * we move to the next interval until we reach the largest.
 */
int	repeatinterval[] = { 30, 120, 600 };	/* # of secs before flush */
#define	MAXREPEAT ((sizeof(repeatinterval) / sizeof(repeatinterval[0])) - 1)
#define	REPEATTIME(f)	((f)->f_time + repeatinterval[(f)->f_repeatcount])
#define	BACKOFF(f)	{ if (++(f)->f_repeatcount > MAXREPEAT) \
				 (f)->f_repeatcount = MAXREPEAT; \
			}

/* values for f_type */
#define F_UNUSED	0		/* unused entry */
#define F_FILE		1		/* regular file */
#define F_TTY		2		/* terminal */
#define F_CONSOLE	3		/* console terminal */
#define F_FORW		4		/* remote machine */
#define F_USERS		5		/* list of users */
#define F_WALL		6		/* everyone logged on */

char	*TypeNames[7] = {
	"UNUSED",	"FILE",		"TTY",		"CONSOLE",
	"FORW",		"USERS",	"WALL"
};

struct	filed *Files;
struct	filed consfile;

int	Debug;			/* debug flag */
char	LocalHostName[MAXHOSTNAMELEN];	/* our hostname */
char	*LocalDomain;		/* our local domain name */
int	InetInuse = 0;		/* non-zero if INET sockets are being used */
int	finet;			/* Internet datagram socket */
int	LogPort;		/* port number for INET connections */
int	Initialized = 0;	/* set when we have initialized ourselves */
int	MarkInterval = 20 * 60;	/* interval between marks in seconds */
int	MarkSeq = 0;		/* mark sequence number */
int	SecureMode = 1;		/* when true, speak only unix domain socks */

void	cfline __P((char *, struct filed *, char *));
char   *cvthname __P((struct sockaddr_in *));
int	decode __P((const char *, CODE *));
void	die __P((int));
void	domark __P((int));
void	fprintlog __P((struct filed *, int, char *));
void	init __P((int));
void	logerror __P((char *));
void	logmsg __P((int, char *, char *, int));
void	printline __P((char *, char *));
void	printsys __P((char *));
void	reapchild __P((int));
char   *ttymsg __P((struct iovec *, int, char *, int));
void	usage __P((void));
void	wallmsg __P((struct filed *, struct iovec *));

#define MAXFUNIX	21

int nfunix = 1;
char *funixn[MAXFUNIX] = { _PATH_LOG };
int funix[MAXFUNIX];

int
main(argc, argv)
	int argc;
	char *argv[];
{
	int ch, i, fklog, len;
	struct sockaddr_un sunx, fromunix;
	struct sockaddr_in sin, frominet;
	FILE *fp;
	char *p, line[MSG_BSIZE + 1];

	while ((ch = getopt(argc, argv, "duf:m:p:a:")) != -1)
		switch (ch) {
		case 'd':		/* debug */
			Debug++;
			break;
		case 'f':		/* configuration file */
			ConfFile = optarg;
			break;
		case 'm':		/* mark interval */
			MarkInterval = atoi(optarg) * 60;
			break;
		case 'p':		/* path */
			funixn[0] = optarg;
			break;
		case 'u':		/* allow udp input port */
			SecureMode = 0;
			break;
		case 'a':
			if (nfunix < MAXFUNIX)
				funixn[nfunix++] = optarg;
			else
				fprintf(stderr,
				    "syslogd: out of descriptors, ignoring %s\n",
				    optarg);
			break;
		case '?':
		default:
			usage();
		}
	if ((argc -= optind) != 0)
		usage();

	if (!Debug)
		(void)daemon(0, 0);
	else
		setlinebuf(stdout);

	consfile.f_type = F_CONSOLE;
	(void)strcpy(consfile.f_un.f_fname, ctty);
	(void)gethostname(LocalHostName, sizeof(LocalHostName));
	if ((p = strchr(LocalHostName, '.')) != NULL) {
		*p++ = '\0';
		LocalDomain = p;
	} else
		LocalDomain = "";
	(void)signal(SIGTERM, die);
	(void)signal(SIGINT, Debug ? die : SIG_IGN);
	(void)signal(SIGQUIT, Debug ? die : SIG_IGN);
	(void)signal(SIGCHLD, reapchild);
	(void)signal(SIGALRM, domark);
	(void)alarm(TIMERINTVL);

#ifndef SUN_LEN
#define SUN_LEN(unp) (strlen((unp)->sun_path) + 2)
#endif
	for (i = 0; i < nfunix; i++) {
		(void)unlink(funixn[i]);

		memset(&sunx, 0, sizeof(sunx));
		sunx.sun_family = AF_UNIX;
		(void)strncpy(sunx.sun_path, funixn[i], sizeof(sunx.sun_path));
		funix[i] = socket(AF_UNIX, SOCK_DGRAM, 0);
		if (funix[i] < 0 ||
		    bind(funix[i], (struct sockaddr *)&sunx, SUN_LEN(&sunx)) < 0 ||
		    chmod(funixn[i], 0666) < 0) {
			(void) snprintf(line, sizeof line, "cannot create %s",
			    funixn[i]);
			logerror(line);
			dprintf("cannot create %s (%d)\n", funixn[i], errno);
			if (i == 0)
				die(0);
		}
	}
	finet = socket(AF_INET, SOCK_DGRAM, 0);
	if (finet >= 0) {
		struct servent *sp;

		sp = getservbyname("syslog", "udp");
		if (sp == NULL) {
			errno = 0;
			logerror("syslog/udp: unknown service");
			die(0);
		}
		memset(&sin, 0, sizeof(sin));
		sin.sin_len = sizeof(sin);
		sin.sin_family = AF_INET;
		sin.sin_port = LogPort = sp->s_port;
		if (bind(finet, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
			logerror("bind");
			if (!Debug)
				die(0);
		} else {
			InetInuse = 1;
		}
	}
	if ((fklog = open(_PATH_KLOG, O_RDONLY, 0)) < 0)
		dprintf("can't open %s (%d)\n", _PATH_KLOG, errno);

	/* tuck my process id away */
	if (!Debug) {
		fp = fopen(PidFile, "w");
		if (fp != NULL) {
			fprintf(fp, "%d\n", getpid());
			(void) fclose(fp);
		}
	}

	dprintf("off & running....\n");

	init(0);
	(void)signal(SIGHUP, init);

	for (;;) {
		fd_set readfds;
		int nfds = 0;

		FD_ZERO(&readfds);
		if (fklog != -1) {
			FD_SET(fklog, &readfds);
			if (fklog > nfds)
				nfds = fklog;
		}
		if (finet != -1) {
			FD_SET(finet, &readfds);
			if (finet > nfds)
				nfds = finet;
		}
		for (i = 0; i < nfunix; i++) {
			if (funix[i] != -1) {
				FD_SET(funix[i], &readfds);
				if (funix[i] > nfds)
					nfds = funix[i];
			}
		}

		/*dprintf("readfds = %#x\n", readfds);*/
		nfds = select(nfds+1, &readfds, (fd_set *)NULL,
		    (fd_set *)NULL, (struct timeval *)NULL);
		if (nfds == 0)
			continue;
		if (nfds < 0) {
			if (errno != EINTR)
				logerror("select");
			continue;
		}
		/*dprintf("got a message (%d, %#x)\n", nfds, readfds);*/
		if (fklog != -1 && FD_ISSET(fklog, &readfds)) {
			i = read(fklog, line, sizeof(line) - 1);
			if (i > 0) {
				line[i] = '\0';
				printsys(line);
			} else if (i < 0 && errno != EINTR) {
				logerror("klog");
				fklog = -1;
			}
		}
		if (finet != -1 && FD_ISSET(finet, &readfds)) {
			len = sizeof(frominet);
			i = recvfrom(finet, line, MAXLINE, 0,
			    (struct sockaddr *)&frominet, &len);
			if (SecureMode) {
				/* silently drop it */
			} else {
				if (i > 0) {
					line[i] = '\0';
					printline(cvthname(&frominet), line);
				} else if (i < 0 && errno != EINTR)
					logerror("recvfrom inet");
			}
		} 
		for (i = 0; i < nfunix; i++) {
			if (funix[i] != -1 && FD_ISSET(funix[i], &readfds)) {
				len = sizeof(fromunix);
				len = recvfrom(funix[i], line, MAXLINE, 0,
				    (struct sockaddr *)&fromunix, &len);
				if (len > 0) {
					line[len] = '\0';
					printline(LocalHostName, line);
				} else if (len < 0 && errno != EINTR)
					logerror("recvfrom unix");
			}
		}
	}
}

void
usage()
{

	(void)fprintf(stderr,
	    "usage: syslogd [-u] [-f conffile] [-m markinterval] [-p logpath] [-a logpath]\n");
	exit(1);
}

/*
 * Take a raw input line, decode the message, and print the message
 * on the appropriate log files.
 */
void
printline(hname, msg)
	char *hname;
	char *msg;
{
	int c, pri;
	char *p, *q, line[MAXLINE + 1];

	/* test for special codes */
	pri = DEFUPRI;
	p = msg;
	if (*p == '<') {
		pri = 0;
		while (isdigit(*++p))
			pri = 10 * pri + (*p - '0');
		if (*p == '>')
			++p;
	}
	if (pri &~ (LOG_FACMASK|LOG_PRIMASK))
		pri = DEFUPRI;

	/* don't allow users to log kernel messages */
	if (LOG_FAC(pri) == LOG_KERN)
		pri = LOG_MAKEPRI(LOG_USER, LOG_PRI(pri));

	q = line;

	while ((c = *p++ & 0177) != '\0' &&
	    q < &line[sizeof(line) - 1])
		if (iscntrl(c))
			if (c == '\n')
				*q++ = ' ';
			else if (c == '\t')
				*q++ = '\t';
			else {
				*q++ = '^';
				*q++ = c ^ 0100;
			}
		else
			*q++ = c;
	*q = '\0';

	logmsg(pri, line, hname, 0);
}

/*
 * Take a raw input line from /dev/klog, split and format similar to syslog().
 */
void
printsys(msg)
	char *msg;
{
	int c, pri, flags;
	char *lp, *p, *q, line[MAXLINE + 1];

	(void)strcpy(line, _PATH_UNIX);
	(void)strcat(line, ": ");
	lp = line + strlen(line);
	for (p = msg; *p != '\0'; ) {
		flags = SYNC_FILE | ADDDATE;	/* fsync file after write */
		pri = DEFSPRI;
		if (*p == '<') {
			pri = 0;
			while (isdigit(*++p))
				pri = 10 * pri + (*p - '0');
			if (*p == '>')
				++p;
		} else {
			/* kernel printf's come out on console */
			flags |= IGN_CONS;
		}
		if (pri &~ (LOG_FACMASK|LOG_PRIMASK))
			pri = DEFSPRI;
		q = lp;
		while (*p != '\0' && (c = *p++) != '\n' &&
		    q < &line[MAXLINE])
			*q++ = c;
		*q = '\0';
		logmsg(pri, line, LocalHostName, flags);
	}
}

time_t	now;

/*
 * Log a message to the appropriate log files, users, etc. based on
 * the priority.
 */
void
logmsg(pri, msg, from, flags)
	int pri;
	char *msg, *from;
	int flags;
{
	struct filed *f;
	int fac, msglen, prilev, i;
	sigset_t mask, omask;
	char *timestamp;
 	char prog[NAME_MAX+1];

	dprintf("logmsg: pri 0%o, flags 0x%x, from %s, msg %s\n",
	    pri, flags, from, msg);

	sigemptyset(&mask);
	sigaddset(&mask, SIGALRM);
	sigaddset(&mask, SIGHUP);
	sigprocmask(SIG_BLOCK, &mask, &omask);

	/*
	 * Check to see if msg looks non-standard.
	 */
	msglen = strlen(msg);
	if (msglen < 16 || msg[3] != ' ' || msg[6] != ' ' ||
	    msg[9] != ':' || msg[12] != ':' || msg[15] != ' ')
		flags |= ADDDATE;

	(void)time(&now);
	if (flags & ADDDATE)
		timestamp = ctime(&now) + 4;
	else {
		timestamp = msg;
		msg += 16;
		msglen -= 16;
	}

	/* extract facility and priority level */
	if (flags & MARK)
		fac = LOG_NFACILITIES;
	else
		fac = LOG_FAC(pri);
	prilev = LOG_PRI(pri);

	/* extract program name */
	for(i = 0; i < NAME_MAX; i++) {
		if (!isalnum(msg[i]))
			break;
		prog[i] = msg[i];
	}
	prog[i] = 0;

	/* log the message to the particular outputs */
	if (!Initialized) {
		f = &consfile;
		f->f_file = open(ctty, O_WRONLY, 0);

		if (f->f_file >= 0) {
			fprintlog(f, flags, msg);
			(void)close(f->f_file);
			f->f_file = -1;
		}
		(void)sigprocmask(SIG_SETMASK, &omask, NULL);
		return;
	}
	for (f = Files; f; f = f->f_next) {
		/* skip messages that are incorrect priority */
		if (f->f_pmask[fac] < prilev ||
		    f->f_pmask[fac] == INTERNAL_NOPRI)
			continue;

		/* skip messages with the incorrect program name */
		if (f->f_program)
			if (strcmp(prog, f->f_program) != 0)
				continue;

		if (f->f_type == F_CONSOLE && (flags & IGN_CONS))
			continue;

		/* don't output marks to recently written files */
		if ((flags & MARK) && (now - f->f_time) < MarkInterval / 2)
			continue;

		/*
		 * suppress duplicate lines to this file
		 */
		if ((flags & MARK) == 0 && msglen == f->f_prevlen &&
		    !strcmp(msg, f->f_prevline) &&
		    !strcmp(from, f->f_prevhost)) {
			(void)strncpy(f->f_lasttime, timestamp, 15);
			f->f_prevcount++;
			dprintf("msg repeated %d times, %ld sec of %d\n",
			    f->f_prevcount, (long)(now - f->f_time),
			    repeatinterval[f->f_repeatcount]);
			/*
			 * If domark would have logged this by now,
			 * flush it now (so we don't hold isolated messages),
			 * but back off so we'll flush less often
			 * in the future.
			 */
			if (now > REPEATTIME(f)) {
				fprintlog(f, flags, (char *)NULL);
				BACKOFF(f);
			}
		} else {
			/* new line, save it */
			if (f->f_prevcount)
				fprintlog(f, 0, (char *)NULL);
			f->f_repeatcount = 0;
			f->f_prevpri = pri;
			(void)strncpy(f->f_lasttime, timestamp, 15);
			(void)strncpy(f->f_prevhost, from,
					sizeof(f->f_prevhost)-1);
			f->f_prevhost[sizeof(f->f_prevhost)-1] = '\0';
			if (msglen < MAXSVLINE) {
				f->f_prevlen = msglen;
				(void)strcpy(f->f_prevline, msg);
				fprintlog(f, flags, (char *)NULL);
			} else {
				f->f_prevline[0] = 0;
				f->f_prevlen = 0;
				fprintlog(f, flags, msg);
			}
		}
	}
	(void)sigprocmask(SIG_SETMASK, &omask, NULL);
}

void
fprintlog(f, flags, msg)
	struct filed *f;
	int flags;
	char *msg;
{
	struct iovec iov[6];
	struct iovec *v;
	int l;
	char line[MAXLINE + 1], repbuf[80], greetings[500];

	v = iov;
	if (f->f_type == F_WALL) {
		v->iov_base = greetings;
		v->iov_len = snprintf(greetings, sizeof(greetings),
		    "\r\n\7Message from syslogd@%s at %.24s ...\r\n",
		    f->f_prevhost, ctime(&now));
		if (v->iov_len >= sizeof(greetings))
			v->iov_len = sizeof(greetings) - 1;
		v++;
		v->iov_base = "";
		v->iov_len = 0;
		v++;
	} else {
		v->iov_base = f->f_lasttime;
		v->iov_len = 15;
		v++;
		v->iov_base = " ";
		v->iov_len = 1;
		v++;
	}
	v->iov_base = f->f_prevhost;
	v->iov_len = strlen(v->iov_base);
	v++;
	v->iov_base = " ";
	v->iov_len = 1;
	v++;

	if (msg) {
		v->iov_base = msg;
		v->iov_len = strlen(msg);
	} else if (f->f_prevcount > 1) {
		v->iov_base = repbuf;
		v->iov_len = sprintf(repbuf, "last message repeated %d times",
		    f->f_prevcount);
	} else {
		v->iov_base = f->f_prevline;
		v->iov_len = f->f_prevlen;
	}
	v++;

	dprintf("Logging to %s", TypeNames[f->f_type]);
	f->f_time = now;

	switch (f->f_type) {
	case F_UNUSED:
		dprintf("\n");
		break;

	case F_FORW:
		dprintf(" %s\n", f->f_un.f_forw.f_hname);
		l = snprintf(line, sizeof(line) - 1, "<%d>%.15s %s", f->f_prevpri,
		    iov[0].iov_base, iov[4].iov_base);
		if (l > MAXLINE)
			l = MAXLINE;
		if (sendto(finet, line, l, 0,
		    (struct sockaddr *)&f->f_un.f_forw.f_addr,
		    sizeof(f->f_un.f_forw.f_addr)) != l) {
			f->f_type = F_UNUSED;
			logerror("sendto");
		}
		break;

	case F_CONSOLE:
		if (flags & IGN_CONS) {
			dprintf(" (ignored)\n");
			break;
		}
		/* FALLTHROUGH */

	case F_TTY:
	case F_FILE:
		dprintf(" %s\n", f->f_un.f_fname);
		if (f->f_type != F_FILE) {
			v->iov_base = "\r\n";
			v->iov_len = 2;
		} else {
			v->iov_base = "\n";
			v->iov_len = 1;
		}
	again:
		if (writev(f->f_file, iov, 6) < 0) {
			int e = errno;
			(void)close(f->f_file);
			/*
			 * Check for errors on TTY's due to loss of tty
			 */
			if ((e == EIO || e == EBADF) && f->f_type != F_FILE) {
				f->f_file = open(f->f_un.f_fname,
				    O_WRONLY|O_APPEND, 0);
				if (f->f_file < 0) {
					f->f_type = F_UNUSED;
					logerror(f->f_un.f_fname);
				} else
					goto again;
			} else {
				f->f_type = F_UNUSED;
				f->f_file = -1;
				errno = e;
				logerror(f->f_un.f_fname);
			}
		} else if (flags & SYNC_FILE)
			(void)fsync(f->f_file);
		break;

	case F_USERS:
	case F_WALL:
		dprintf("\n");
		v->iov_base = "\r\n";
		v->iov_len = 2;
		wallmsg(f, iov);
		break;
	}
	f->f_prevcount = 0;
}

/*
 *  WALLMSG -- Write a message to the world at large
 *
 *	Write the specified message to either the entire
 *	world, or a list of approved users.
 */
void
wallmsg(f, iov)
	struct filed *f;
	struct iovec *iov;
{
	static int reenter;			/* avoid calling ourselves */
	FILE *uf;
	struct utmp ut;
	int i;
	char *p;
	char line[sizeof(ut.ut_line) + 1];

	if (reenter++)
		return;
	if ((uf = fopen(_PATH_UTMP, "r")) == NULL) {
		logerror(_PATH_UTMP);
		reenter = 0;
		return;
	}
	/* NOSTRICT */
	while (fread((char *)&ut, sizeof(ut), 1, uf) == 1) {
		if (ut.ut_name[0] == '\0')
			continue;
		strncpy(line, ut.ut_line, sizeof(ut.ut_line));
		line[sizeof(ut.ut_line)] = '\0';
		if (f->f_type == F_WALL) {
			if ((p = ttymsg(iov, 6, line, TTYMSGTIME)) != NULL) {
				errno = 0;	/* already in msg */
				logerror(p);
			}
			continue;
		}
		/* should we send the message to this user? */
		for (i = 0; i < MAXUNAMES; i++) {
			if (!f->f_un.f_uname[i][0])
				break;
			if (!strncmp(f->f_un.f_uname[i], ut.ut_name,
			    UT_NAMESIZE)) {
				if ((p = ttymsg(iov, 6, line, TTYMSGTIME))
								!= NULL) {
					errno = 0;	/* already in msg */
					logerror(p);
				}
				break;
			}
		}
	}
	(void)fclose(uf);
	reenter = 0;
}

void
reapchild(signo)
	int signo;
{
	union wait status;
	int save_errno = errno;

	while (wait3((int *)&status, WNOHANG, (struct rusage *)NULL) > 0)
		;
	errno = save_errno;
}

/*
 * Return a printable representation of a host address.
 */
char *
cvthname(f)
	struct sockaddr_in *f;
{
	struct hostent *hp;
	sigset_t omask, nmask;
	char *p;

	dprintf("cvthname(%s)\n", inet_ntoa(f->sin_addr));

	if (f->sin_family != AF_INET) {
		dprintf("Malformed from address\n");
		return ("???");
	}
	sigemptyset(&nmask);
	sigaddset(&nmask, SIGHUP);
	sigprocmask(SIG_BLOCK, &nmask, &omask);
	hp = gethostbyaddr((char *)&f->sin_addr,
	    sizeof(struct in_addr), f->sin_family);
	sigprocmask(SIG_SETMASK, &omask, NULL);
	if (hp == 0) {
		dprintf("Host name for your address (%s) unknown\n",
			inet_ntoa(f->sin_addr));
		return (inet_ntoa(f->sin_addr));
	}
	if ((p = strchr(hp->h_name, '.')) && strcmp(p + 1, LocalDomain) == 0)
		*p = '\0';
	return (hp->h_name);
}

void
domark(signo)
	int signo;
{
	struct filed *f;

	now = time((time_t *)NULL);
	MarkSeq += TIMERINTVL;
	if (MarkSeq >= MarkInterval) {
		logmsg(LOG_INFO, "-- MARK --", LocalHostName, ADDDATE|MARK);
		MarkSeq = 0;
	}

	for (f = Files; f; f = f->f_next) {
		if (f->f_prevcount && now >= REPEATTIME(f)) {
			dprintf("flush %s: repeated %d times, %d sec.\n",
			    TypeNames[f->f_type], f->f_prevcount,
			    repeatinterval[f->f_repeatcount]);
			fprintlog(f, 0, (char *)NULL);
			BACKOFF(f);
		}
	}
	(void)alarm(TIMERINTVL);
}

/*
 * Print syslogd errors some place.
 */
void
logerror(type)
	char *type;
{
	char buf[100];

	if (errno)
		(void)snprintf(buf, sizeof(buf), "syslogd: %s: %s",
		    type, strerror(errno));
	else
		(void)snprintf(buf, sizeof(buf), "syslogd: %s", type);
	errno = 0;
	dprintf("%s\n", buf);
	logmsg(LOG_SYSLOG|LOG_ERR, buf, LocalHostName, ADDDATE);
}

void
die(signo)
	int signo;
{
	struct filed *f;
	int was_initialized = Initialized;
	char buf[100];
	int i;

	Initialized = 0;		/* Don't log SIGCHLDs */
	for (f = Files; f != NULL; f = f->f_next) {
		/* flush any pending output */
		if (f->f_prevcount)
			fprintlog(f, 0, (char *)NULL);
	}
	Initialized = was_initialized;
	if (signo) {
		dprintf("syslogd: exiting on signal %d\n", signo);
		(void)sprintf(buf, "exiting on signal %d", signo);
		errno = 0;
		logerror(buf);
	}
	for (i = 0; i < nfunix; i++)
		if (funixn[i] && funix[i] != -1)
			(void)unlink(funixn[i]);
	exit(0);
}

/*
 *  INIT -- Initialize syslogd from configuration table
 */
void
init(signo)
	int signo;
{
	int i;
	FILE *cf;
	struct filed *f, *next, **nextp;
	char *p;
	char cline[LINE_MAX];
 	char prog[NAME_MAX+1];

	dprintf("init\n");

	/*
	 *  Close all open log files.
	 */
	Initialized = 0;
	for (f = Files; f != NULL; f = next) {
		/* flush any pending output */
		if (f->f_prevcount)
			fprintlog(f, 0, (char *)NULL);

		switch (f->f_type) {
		case F_FILE:
		case F_TTY:
		case F_CONSOLE:
			(void)close(f->f_file);
			break;
		case F_FORW:
			break;
		}
		next = f->f_next;
		if (f->f_program)
			free(f->f_program);
		free((char *)f);
	}
	Files = NULL;
	nextp = &Files;

	/* open the configuration file */
	if ((cf = fopen(ConfFile, "r")) == NULL) {
		dprintf("cannot open %s\n", ConfFile);
		*nextp = (struct filed *)calloc(1, sizeof(*f));
		cfline("*.ERR\t/dev/console", *nextp, "*");
		(*nextp)->f_next = (struct filed *)calloc(1, sizeof(*f));
		cfline("*.PANIC\t*", (*nextp)->f_next, "*");
		Initialized = 1;
		return;
	}

	/*
	 *  Foreach line in the conf table, open that file.
	 */
	f = NULL;
	strcpy(prog, "*");
	while (fgets(cline, sizeof(cline), cf) != NULL) {
		/*
		 * check for end-of-section, comments, strip off trailing
		 * spaces and newline character. #!prog  and !prog are treated
		 * specially: the following lines apply only to that program.
		 */
		for (p = cline; isspace(*p); ++p)
			continue;
		if (*p == '\0')
			continue;
		if (*p == '#') {
			p++;
			if (*p != '!')
				continue;
		}
		if (*p == '!') {
			p++;
			while (isspace(*p))
				p++;
			if (!*p) {
				strcpy(prog, "*");
				continue;
			}
			for (i = 0; i < NAME_MAX; i++) {
				if (!isalnum(p[i]))
					break;
				prog[i] = p[i];
			}
			prog[i] = 0;
			continue;
		}
		p = cline + strlen(cline);
		while (p > cline)
			if (!isspace(*--p)) {
				p++;
				break;
			}
		*p = '\0';
		f = (struct filed *)calloc(1, sizeof(*f));
		*nextp = f;
		nextp = &f->f_next;
		cfline(cline, f, prog);
	}

	/* close the configuration file */
	(void)fclose(cf);

	Initialized = 1;

	if (Debug) {
		for (f = Files; f; f = f->f_next) {
			for (i = 0; i <= LOG_NFACILITIES; i++)
				if (f->f_pmask[i] == INTERNAL_NOPRI)
					printf("X ");
				else
					printf("%d ", f->f_pmask[i]);
			printf("%s: ", TypeNames[f->f_type]);
			switch (f->f_type) {
			case F_FILE:
			case F_TTY:
			case F_CONSOLE:
				printf("%s", f->f_un.f_fname);
				break;

			case F_FORW:
				printf("%s", f->f_un.f_forw.f_hname);
				break;

			case F_USERS:
				for (i = 0; i < MAXUNAMES && *f->f_un.f_uname[i]; i++)
					printf("%s, ", f->f_un.f_uname[i]);
				break;
			}
			if (f->f_program)
				printf(" (%s)", f->f_program);
			printf("\n");
		}
	}

	logmsg(LOG_SYSLOG|LOG_INFO, "syslogd: restart", LocalHostName, ADDDATE);
	dprintf("syslogd: restarted\n");
}

/*
 * Crack a configuration file line
 */
void
cfline(line, f, prog)
	char *line;
	struct filed *f;
	char *prog;
{
	struct hostent *hp;
	int i, pri;
	char *bp, *p, *q;
	char buf[MAXLINE], ebuf[100];

	dprintf("cfline(\"%s\", f, \"%s\")\n", line, prog);

	errno = 0;	/* keep strerror() stuff out of logerror messages */

	/* clear out file entry */
	memset(f, 0, sizeof(*f));
	for (i = 0; i <= LOG_NFACILITIES; i++)
		f->f_pmask[i] = INTERNAL_NOPRI;

	/* save program name if any */
	if (!strcmp(prog, "*"))
		prog = NULL;
	else {
		f->f_program = calloc(1, strlen(prog)+1);
		if (f->f_program)
			strcpy(f->f_program, prog);
	}

	/* scan through the list of selectors */
	for (p = line; *p && *p != '\t';) {

		/* find the end of this facility name list */
		for (q = p; *q && *q != '\t' && *q++ != '.'; )
			continue;

		/* collect priority name */
		for (bp = buf; *q && !strchr("\t,;", *q); )
			*bp++ = *q++;
		*bp = '\0';

		/* skip cruft */
		while (strchr(", ;", *q))
			q++;

		/* decode priority name */
		if (*buf == '*')
			pri = LOG_PRIMASK + 1;
		else {
			/* ignore trailing spaces */
			int i;
			for (i=strlen(buf)-1; i >= 0 && buf[i] == ' '; i--) {
				buf[i]='\0';
			}

			pri = decode(buf, prioritynames);
			if (pri < 0) {
				(void)snprintf(ebuf, sizeof ebuf,
				    "unknown priority name \"%s\"", buf);
				logerror(ebuf);
				return;
			}
		}

		/* scan facilities */
		while (*p && !strchr("\t.;", *p)) {
			for (bp = buf; *p && !strchr("\t,;.", *p); )
				*bp++ = *p++;
			*bp = '\0';
			if (*buf == '*')
				for (i = 0; i < LOG_NFACILITIES; i++)
					f->f_pmask[i] = pri;
			else {
				i = decode(buf, facilitynames);
				if (i < 0) {
					(void)snprintf(ebuf, sizeof(ebuf),
					    "unknown facility name \"%s\"",
					    buf);
					logerror(ebuf);
					return;
				}
				f->f_pmask[i >> 3] = pri;
			}
			while (*p == ',' || *p == ' ')
				p++;
		}

		p = q;
	}

	/* skip to action part */
	while (*p == '\t')
		p++;

	switch (*p)
	{
	case '@':
		if (!InetInuse)
			break;
		(void)strncpy(f->f_un.f_forw.f_hname, ++p,
		    sizeof(f->f_un.f_forw.f_hname)-1);
		f->f_un.f_forw.f_hname[sizeof(f->f_un.f_forw.f_hname)-1] = '\0';
		hp = gethostbyname(f->f_un.f_forw.f_hname);
		if (hp == NULL) {
			extern int h_errno;

			logerror((char *)hstrerror(h_errno));
			break;
		}
		memset(&f->f_un.f_forw.f_addr, 0,
		    sizeof(f->f_un.f_forw.f_addr));
		f->f_un.f_forw.f_addr.sin_len = sizeof(f->f_un.f_forw.f_addr);
		f->f_un.f_forw.f_addr.sin_family = AF_INET;
		f->f_un.f_forw.f_addr.sin_port = LogPort;
		memmove(&f->f_un.f_forw.f_addr.sin_addr, hp->h_addr,
		    hp->h_length);
		f->f_type = F_FORW;
		break;

	case '/':
		(void)strcpy(f->f_un.f_fname, p);
		if ((f->f_file = open(p, O_WRONLY|O_APPEND, 0)) < 0) {
			f->f_type = F_UNUSED;
			logerror(p);
			break;
		}
		if (isatty(f->f_file))
			f->f_type = F_TTY;
		else
			f->f_type = F_FILE;
		if (strcmp(p, ctty) == 0)
			f->f_type = F_CONSOLE;
		break;

	case '*':
		f->f_type = F_WALL;
		break;

	default:
		for (i = 0; i < MAXUNAMES && *p; i++) {
			for (q = p; *q && *q != ','; )
				q++;
			(void)strncpy(f->f_un.f_uname[i], p, UT_NAMESIZE);
			if ((q - p) > UT_NAMESIZE)
				f->f_un.f_uname[i][UT_NAMESIZE] = '\0';
			else
				f->f_un.f_uname[i][q - p] = '\0';
			while (*q == ',' || *q == ' ')
				q++;
			p = q;
		}
		f->f_type = F_USERS;
		break;
	}
}


/*
 *  Decode a symbolic name to a numeric value
 */
int
decode(name, codetab)
	const char *name;
	CODE *codetab;
{
	CODE *c;
	char *p, buf[40];

	if (isdigit(*name))
		return (atoi(name));

	for (p = buf; *name && p < &buf[sizeof(buf) - 1]; p++, name++) {
		if (isupper(*name))
			*p = tolower(*name);
		else
			*p = *name;
	}
	*p = '\0';
	for (c = codetab; c->c_name; c++)
		if (!strcmp(buf, c->c_name))
			return (c->c_val);

	return (-1);
}
