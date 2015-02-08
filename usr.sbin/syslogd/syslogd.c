/*	$OpenBSD: syslogd.c,v 1.151 2015/02/08 15:17:30 bluhm Exp $	*/

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
 * MAXLINE -- the maximum line length that can be handled.
 * DEFUPRI -- the default priority for user messages
 * DEFSPRI -- the default priority for kernel messages
 *
 * Author: Eric Allman
 * extensive changes by Ralph Campbell
 * more extensive changes by Eric Allman (again)
 * memory buffer logging by Damien Miller
 * IPv6, libevent, sending over TCP and TLS by Alexander Bluhm
 */

#define MAXLINE		8192		/* maximum line length */
#define MAX_UDPMSG	1180		/* maximum UDP send size */
#define MIN_MEMBUF	(MAXLINE * 4)	/* Minimum memory buffer size */
#define MAX_MEMBUF	(256 * 1024)	/* Maximum memory buffer size */
#define MAX_MEMBUF_NAME	64		/* Max length of membuf log name */
#define MAX_TCPBUF	(256 * 1024)	/* Maximum tcp event buffer size */
#define	MAXSVLINE	120		/* maximum saved line length */
#define DEFUPRI		(LOG_USER|LOG_NOTICE)
#define DEFSPRI		(LOG_KERN|LOG_CRIT)
#define TIMERINTVL	30		/* interval for checking flush, mark */
#define TTYMSGTIME	1		/* timeout passed to ttymsg */
#define ERRBUFSIZE	256

#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/msgbuf.h>
#include <sys/queue.h>
#include <sys/uio.h>
#include <sys/sysctl.h>
#include <sys/un.h>
#include <sys/time.h>
#include <sys/resource.h>

#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <event.h>
#include <fcntl.h>
#include <limits.h>
#include <paths.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tls.h>
#include <unistd.h>
#include <utmp.h>
#include <vis.h>

#define MAXIMUM(a, b)	(((a) > (b)) ? (a) : (b))
#define MINIMUM(a, b)	(((a) < (b)) ? (a) : (b))

#define SYSLOG_NAMES
#include <sys/syslog.h>

#include "syslogd.h"
#include "evbuffer_tls.h"

char *ConfFile = _PATH_LOGCONF;
const char ctty[] = _PATH_CONSOLE;

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
	SIMPLEQ_ENTRY(filed) f_next;	/* next in linked list */
	int	f_type;			/* entry type, see below */
	int	f_file;			/* file descriptor */
	time_t	f_time;			/* time this was last written */
	u_char	f_pmask[LOG_NFACILITIES+1];	/* priority mask */
	char	*f_program;		/* program this applies to */
	union {
		char	f_uname[MAXUNAMES][UT_NAMESIZE+1];
		struct {
			char	f_loghost[1+4+3+1+NI_MAXHOST+1+NI_MAXSERV];
				/* @proto46://[hostname]:servname\0 */
			struct sockaddr_storage	 f_addr;
			struct buffertls	 f_buftls;
			struct bufferevent	*f_bufev;
			struct tls		*f_ctx;
			char			*f_host;
			int			 f_reconnectwait;
			int			 f_dropped;
		} f_forw;		/* forwarding address */
		char	f_fname[PATH_MAX];
		struct {
			char	f_mname[MAX_MEMBUF_NAME];
			struct ringbuf *f_rb;
			int	f_overflow;
			int	f_attached;
			size_t	f_len;
		} f_mb;		/* Memory buffer */
	} f_un;
	char	f_prevline[MAXSVLINE];		/* last message logged */
	char	f_lasttime[16];			/* time of last occurrence */
	char	f_prevhost[HOST_NAME_MAX+1];	/* host from which recd. */
	int	f_prevpri;			/* pri of f_prevline */
	int	f_prevlen;			/* length of f_prevline */
	int	f_prevcount;			/* repetition cnt of prevline */
	unsigned int f_repeatcount;		/* number of "repeated" msgs */
	int	f_quick;			/* abort when matched */
	time_t	f_lasterrtime;			/* last error was reported */
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
#define F_FORWUDP	4		/* remote machine via UDP */
#define F_USERS		5		/* list of users */
#define F_WALL		6		/* everyone logged on */
#define F_MEMBUF	7		/* memory buffer */
#define F_PIPE		8		/* pipe to external program */
#define F_FORWTCP	9		/* remote machine via TCP */
#define F_FORWTLS	10		/* remote machine via TLS */

char	*TypeNames[] = {
	"UNUSED",	"FILE",		"TTY",		"CONSOLE",
	"FORWUDP",	"USERS",	"WALL",		"MEMBUF",
	"PIPE",		"FORWTCP",	"FORWTLS",
};

SIMPLEQ_HEAD(filed_list, filed) Files;
struct	filed consfile;

int	nunix = 1;		/* Number of Unix domain sockets requested */
char	*path_unix[MAXUNIX] = { _PATH_LOG }; /* Paths to Unix domain sockets */
int	Debug;			/* debug flag */
int	Startup = 1;		/* startup flag */
char	LocalHostName[HOST_NAME_MAX+1];	/* our hostname */
char	*LocalDomain;		/* our local domain name */
int	Initialized = 0;	/* set when we have initialized ourselves */

int	MarkInterval = 20 * 60;	/* interval between marks in seconds */
int	MarkSeq = 0;		/* mark sequence number */
int	SecureMode = 1;		/* when true, speak only unix domain socks */
int	NoDNS = 0;		/* when true, will refrain from doing DNS lookups */
int	IPv4Only = 0;		/* when true, disable IPv6 */
int	IPv6Only = 0;		/* when true, disable IPv4 */
int	IncludeHostname = 0;	/* include RFC 3164 style hostnames when forwarding */

char	*path_ctlsock = NULL;	/* Path to control socket */

struct	tls_config *tlsconfig;
const char *CAfile = "/etc/ssl/cert.pem"; /* file containing CA certificates */
int	NoVerify = 0;		/* do not verify TLS server x509 certificate */
int	tcpbuf_dropped = 0;	/* count messages dropped from TCP or TLS */

#define CTL_READING_CMD		1
#define CTL_WRITING_REPLY	2
#define CTL_WRITING_CONT_REPLY	3
int	ctl_state = 0;		/* What the control socket is up to */
int	membuf_drop = 0;	/* logs were dropped in continuous membuf read */

/*
 * Client protocol NB. all numeric fields in network byte order
 */
#define CTL_VERSION		2

/* Request */
struct	{
	u_int32_t	version;
#define CMD_READ	1	/* Read out log */
#define CMD_READ_CLEAR	2	/* Read and clear log */
#define CMD_CLEAR	3	/* Clear log */
#define CMD_LIST	4	/* List available logs */
#define CMD_FLAGS	5	/* Query flags only */
#define CMD_READ_CONT	6	/* Read out log continuously */
	u_int32_t	cmd;
	u_int32_t	lines;
	char		logname[MAX_MEMBUF_NAME];
}	ctl_cmd;

size_t	ctl_cmd_bytes = 0;	/* number of bytes of ctl_cmd read */

/* Reply */
struct ctl_reply_hdr {
	u_int32_t	version;
#define CTL_HDR_FLAG_OVERFLOW	0x01
	u_int32_t	flags;
	/* Reply text follows, up to MAX_MEMBUF long */
};

#define CTL_HDR_LEN		(sizeof(struct ctl_reply_hdr))
#define CTL_REPLY_MAXSIZE	(CTL_HDR_LEN + MAX_MEMBUF)
#define CTL_REPLY_SIZE		(strlen(reply_text) + CTL_HDR_LEN)

char	*ctl_reply = NULL;	/* Buffer for control connection reply */
char	*reply_text;		/* Start of reply text in buffer */
size_t	ctl_reply_size = 0;	/* Number of bytes used in reply */
size_t	ctl_reply_offset = 0;	/* Number of bytes of reply written so far */

char	*linebuf;
int	 linesize;

int		 fd_ctlsock, fd_ctlconn, fd_klog, fd_sendsys,
		 fd_udp, fd_udp6, fd_unix[MAXUNIX];
struct event	 ev_ctlaccept, ev_ctlread, ev_ctlwrite, ev_klog, ev_sendsys,
		 ev_udp, ev_udp6, ev_unix[MAXUNIX],
		 ev_hup, ev_int, ev_quit, ev_term, ev_mark;

void	 klog_readcb(int, short, void *);
void	 udp_readcb(int, short, void *);
void	 unix_readcb(int, short, void *);
int	 tcp_socket(struct filed *);
void	 tcp_readcb(struct bufferevent *, void *);
void	 tcp_writecb(struct bufferevent *, void *);
void	 tcp_errorcb(struct bufferevent *, short, void *);
void	 tcp_connectcb(int, short, void *);
struct tls *tls_socket(struct filed *);
int	 tcpbuf_countmsg(struct bufferevent *bufev);
void	 die_signalcb(int, short, void *);
void	 mark_timercb(int, short, void *);
void	 init_signalcb(int, short, void *);
void	 ctlsock_acceptcb(int, short, void *);
void	 ctlconn_readcb(int, short, void *);
void	 ctlconn_writecb(int, short, void *);
void	 ctlconn_logto(char *);
void	 ctlconn_cleanup(void);

struct filed *cfline(char *, char *);
void	cvthname(struct sockaddr *, char *, size_t);
int	decode(const char *, const CODE *);
void	die(int);
void	markit(void);
void	fprintlog(struct filed *, int, char *);
void	init(void);
void	logerror(const char *);
void	logmsg(int, char *, char *, int);
struct filed *find_dup(struct filed *);
void	printline(char *, char *);
void	printsys(char *);
char   *ttymsg(struct iovec *, int, char *, int);
void	usage(void);
void	wallmsg(struct filed *, struct iovec *);
int	loghost(char *, char **, char **, char **);
int	getmsgbufsize(void);
int	unix_socket(char *, int, mode_t);
void	double_rbuf(int);
void	tailify_replytext(char *, int);

int
main(int argc, char *argv[])
{
	struct addrinfo	 hints, *res, *res0;
	struct timeval	 to;
	const char	*errstr;
	char		*p;
	int		 ch, i;
	int		 lockpipe[2] = { -1, -1}, pair[2], nullfd, fd;

	while ((ch = getopt(argc, argv, "46C:dhnuf:m:p:a:s:V")) != -1)
		switch (ch) {
		case '4':		/* disable IPv6 */
			IPv4Only = 1;
			IPv6Only = 0;
			break;
		case '6':		/* disable IPv4 */
			IPv6Only = 1;
			IPv4Only = 0;
			break;
		case 'C':		/* file containing CA certificates */
			CAfile = optarg;
			break;
		case 'd':		/* debug */
			Debug++;
			break;
		case 'f':		/* configuration file */
			ConfFile = optarg;
			break;
		case 'h':		/* RFC 3164 hostnames */
			IncludeHostname = 1;
			break;
		case 'm':		/* mark interval */
			MarkInterval = strtonum(optarg, 0, 365*24*60, &errstr);
			if (errstr)
				errx(1, "mark_interval: %s", errstr, optarg);
			MarkInterval *= 60;
			break;
		case 'n':		/* don't do DNS lookups */
			NoDNS = 1;
			break;
		case 'p':		/* path */
			path_unix[0] = optarg;
			break;
		case 'u':		/* allow udp input port */
			SecureMode = 0;
			break;
		case 'a':
			if (nunix >= MAXUNIX)
				fprintf(stderr, "syslogd: "
				    "out of descriptors, ignoring %s\n",
				    optarg);
			else
				path_unix[nunix++] = optarg;
			break;
		case 's':
			path_ctlsock = optarg;
			break;
		case 'V':		/* do not verify certificates */
			NoVerify = 1;
			break;
		default:
			usage();
		}
	if ((argc -= optind) != 0)
		usage();

	if (Debug)
		setvbuf(stdout, NULL, _IOLBF, 0);

	if ((nullfd = open(_PATH_DEVNULL, O_RDWR)) == -1) {
		logerror("Couldn't open /dev/null");
		die(0);
	}
	for (fd = nullfd + 1; fd <= 2; fd++) {
		if (fcntl(fd, F_GETFL, 0) == -1)
			if (dup2(nullfd, fd) == -1)
				logerror("dup2");
	}

	consfile.f_type = F_CONSOLE;
	(void)strlcpy(consfile.f_un.f_fname, ctty,
	    sizeof(consfile.f_un.f_fname));
	(void)gethostname(LocalHostName, sizeof(LocalHostName));
	if ((p = strchr(LocalHostName, '.')) != NULL) {
		*p++ = '\0';
		LocalDomain = p;
	} else
		LocalDomain = "";

	linesize = getmsgbufsize();
	if (linesize < MAXLINE)
		linesize = MAXLINE;
	linesize++;
	if ((linebuf = malloc(linesize)) == NULL) {
		logerror("Couldn't allocate line buffer");
		die(0);
	}

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_protocol = IPPROTO_UDP;
	hints.ai_flags = AI_PASSIVE;

	i = getaddrinfo(NULL, "syslog", &hints, &res0);
	if (i) {
		errno = 0;
		logerror("syslog/udp: unknown service");
		die(0);
	}

	fd_udp = fd_udp6 = -1;
	for (res = res0; res; res = res->ai_next) {
		int *fdp;

		switch (res->ai_family) {
		case AF_INET:
			if (IPv6Only)
				continue;
			fdp = &fd_udp;
			break;
		case AF_INET6:
			if (IPv4Only)
				continue;
			fdp = &fd_udp6;
			break;
		default:
			continue;
		}

		if (*fdp >= 0)
			continue;

		*fdp = socket(res->ai_family, res->ai_socktype,
		    res->ai_protocol);
		if (*fdp == -1)
			continue;

		if (bind(*fdp, res->ai_addr, res->ai_addrlen) < 0) {
			logerror("bind");
			close(*fdp);
			*fdp = -1;
			if (!Debug)
				die(0);
			continue;
		}

		if (SecureMode)
			shutdown(*fdp, SHUT_RD);
		else
			double_rbuf(*fdp);
	}

	freeaddrinfo(res0);

#ifndef SUN_LEN
#define SUN_LEN(unp) (strlen((unp)->sun_path) + 2)
#endif
	for (i = 0; i < nunix; i++) {
		fd_unix[i] = unix_socket(path_unix[i], SOCK_DGRAM, 0666);
		if (fd_unix[i] == -1) {
			if (i == 0 && !Debug)
				die(0);
			continue;
		}
		double_rbuf(fd_unix[i]);
	}

	if (socketpair(AF_UNIX, SOCK_DGRAM, PF_UNSPEC, pair) == -1)
		die(0);
	fd_sendsys = pair[0];
	double_rbuf(fd_sendsys);

	fd_ctlsock = fd_ctlconn = -1;
	if (path_ctlsock != NULL) {
		fd_ctlsock = unix_socket(path_ctlsock, SOCK_STREAM, 0600);
		if (fd_ctlsock == -1) {
			dprintf("can't open %s (%d)\n", path_ctlsock, errno);
			if (!Debug)
				die(0);
		} else {
			if (listen(fd_ctlsock, 16) == -1) {
				logerror("ctlsock listen");
				die(0);
			}
		}
	}

	fd_klog = open(_PATH_KLOG, O_RDONLY, 0);
	if (fd_klog == -1) {
		dprintf("can't open %s (%d)\n", _PATH_KLOG, errno);
	} else {
		if (ioctl(fd_klog, LIOCSFD, &pair[1]) == -1)
			dprintf("LIOCSFD errno %d\n", errno);
	}
	close(pair[1]);

	if (tls_init() == -1) {
		logerror("tls_init");
	} else if ((tlsconfig = tls_config_new()) == NULL) {
		logerror("tls_config_new");
	} else if (NoVerify) {
		tls_config_insecure_noverifyhost(tlsconfig);
		tls_config_insecure_noverifycert(tlsconfig);
	} else {
		struct stat sb;

		fd = -1;
		p = NULL;
		errno = 0;
		if ((fd = open(CAfile, O_RDONLY)) == -1) {
			logerror("open CAfile");
		} else if (fstat(fd, &sb) == -1) {
			logerror("fstat CAfile");
		} else if (sb.st_size > 1024*1024*1024) {
			logerror("CAfile larger than 1GB");
		} else if ((p = calloc(sb.st_size, 1)) == NULL) {
			logerror("calloc CAfile");
		} else if (read(fd, p, sb.st_size) != sb.st_size) {
			logerror("read CAfile");
		} else if (tls_config_set_ca_mem(tlsconfig, p, sb.st_size)
		    == -1) {
			logerror("tls_config_set_ca_mem");
		} else {
			dprintf("CAfile %s, size %lld\n", CAfile, sb.st_size);
		}
		free(p);
		close(fd);
	}

	dprintf("off & running....\n");

	chdir("/");

	tzset();

	if (!Debug) {
		char c;

		pipe(lockpipe);

		switch(fork()) {
		case -1:
			exit(1);
		case 0:
			setsid();
			close(lockpipe[0]);
			break;
		default:
			close(lockpipe[1]);
			read(lockpipe[0], &c, 1);
			_exit(0);
		}
	}

	/* tuck my process id away */
	if (!Debug) {
		FILE *fp;

		fp = fopen(_PATH_LOGPID, "w");
		if (fp != NULL) {
			fprintf(fp, "%ld\n", (long)getpid());
			(void) fclose(fp);
		}
	}

	/* Privilege separation begins here */
	if (priv_init(ConfFile, NoDNS, lockpipe[1], nullfd, argv) < 0)
		errx(1, "unable to privsep");

	/* Process is now unprivileged and inside a chroot */
	event_init();

	event_set(&ev_ctlaccept, fd_ctlsock, EV_READ|EV_PERSIST,
	    ctlsock_acceptcb, &ev_ctlaccept);
	event_set(&ev_ctlread, fd_ctlconn, EV_READ|EV_PERSIST,
	    ctlconn_readcb, &ev_ctlread);
	event_set(&ev_ctlwrite, fd_ctlconn, EV_WRITE|EV_PERSIST,
	    ctlconn_writecb, &ev_ctlwrite);
	event_set(&ev_klog, fd_klog, EV_READ|EV_PERSIST, klog_readcb, &ev_klog);
	event_set(&ev_sendsys, fd_sendsys, EV_READ|EV_PERSIST, unix_readcb,
	    &ev_sendsys);
	event_set(&ev_udp, fd_udp, EV_READ|EV_PERSIST, udp_readcb, &ev_udp);
	event_set(&ev_udp6, fd_udp6, EV_READ|EV_PERSIST, udp_readcb, &ev_udp6);
	for (i = 0; i < nunix; i++)
		event_set(&ev_unix[i], fd_unix[i], EV_READ|EV_PERSIST,
		    unix_readcb, &ev_unix[i]);

	signal_set(&ev_hup, SIGHUP, init_signalcb, &ev_hup);
	signal_set(&ev_int, SIGINT, die_signalcb, &ev_int);
	signal_set(&ev_quit, SIGQUIT, die_signalcb, &ev_quit);
	signal_set(&ev_term, SIGTERM, die_signalcb, &ev_term);

	evtimer_set(&ev_mark, mark_timercb, &ev_mark);

	init();

	Startup = 0;

	/* Allocate ctl socket reply buffer if we have a ctl socket */
	if (fd_ctlsock != -1 &&
	    (ctl_reply = malloc(CTL_REPLY_MAXSIZE)) == NULL) {
		logerror("Couldn't allocate ctlsock reply buffer");
		die(0);
	}
	reply_text = ctl_reply + CTL_HDR_LEN;

	if (!Debug) {
		dup2(nullfd, STDIN_FILENO);
		dup2(nullfd, STDOUT_FILENO);
		dup2(nullfd, STDERR_FILENO);
		close(lockpipe[1]);
	}
	if (nullfd > 2)
		close(nullfd);

	/*
	 * Signal to the priv process that the initial config parsing is done
	 * so that it will reject any future attempts to open more files
	 */
	priv_config_parse_done();

	if (fd_ctlsock != -1)
		event_add(&ev_ctlaccept, NULL);
	if (fd_klog != -1)
		event_add(&ev_klog, NULL);
	if (fd_sendsys != -1)
		event_add(&ev_sendsys, NULL);
	if (!SecureMode) {
		if (fd_udp != -1)
			event_add(&ev_udp, NULL);
		if (fd_udp6 != -1)
			event_add(&ev_udp6, NULL);
	}
	for (i = 0; i < nunix; i++)
		if (fd_unix[i] != -1)
			event_add(&ev_unix[i], NULL);

	signal_add(&ev_hup, NULL);
	signal_add(&ev_term, NULL);
	if (Debug) {
		signal_add(&ev_int, NULL);
		signal_add(&ev_quit, NULL);
	} else {
		(void)signal(SIGINT, SIG_IGN);
		(void)signal(SIGQUIT, SIG_IGN);
	}
	(void)signal(SIGCHLD, SIG_IGN);
	(void)signal(SIGPIPE, SIG_IGN);

	to.tv_sec = TIMERINTVL;
	to.tv_usec = 0;
	evtimer_add(&ev_mark, &to);

	logmsg(LOG_SYSLOG|LOG_INFO, "syslogd: start", LocalHostName, ADDDATE);
	dprintf("syslogd: started\n");

	event_dispatch();
	/* NOTREACHED */
	return (0);
}

void
klog_readcb(int fd, short event, void *arg)
{
	struct event		*ev = arg;
	ssize_t			 n;

	n = read(fd, linebuf, linesize - 1);
	if (n > 0) {
		linebuf[n] = '\0';
		printsys(linebuf);
	} else if (n < 0 && errno != EINTR) {
		logerror("klog");
		event_del(ev);
	}
}

void
udp_readcb(int fd, short event, void *arg)
{
	struct sockaddr_storage	 sa;
	socklen_t		 salen;
	ssize_t			 n;

	salen = sizeof(sa);
	n = recvfrom(fd, linebuf, MAXLINE, 0, (struct sockaddr *)&sa, &salen);
	if (n > 0) {
		char	 resolve[NI_MAXHOST];

		linebuf[n] = '\0';
		cvthname((struct sockaddr *)&sa, resolve, sizeof(resolve));
		dprintf("cvthname res: %s\n", resolve);
		printline(resolve, linebuf);
	} else if (n < 0 && errno != EINTR)
		logerror("recvfrom udp");
}

void
unix_readcb(int fd, short event, void *arg)
{
	struct sockaddr_un	 sa;
	socklen_t		 salen;
	ssize_t			 n;

	salen = sizeof(sa);
	n = recvfrom(fd, linebuf, MAXLINE, 0, (struct sockaddr *)&sa, &salen);
	if (n > 0) {
		linebuf[n] = '\0';
		printline(LocalHostName, linebuf);
	} else if (n == -1 && errno != EINTR)
		logerror("recvfrom unix");
}

int
tcp_socket(struct filed *f)
{
	int	 s, flags;
	char	 ebuf[ERRBUFSIZE];

	if ((s = socket(f->f_un.f_forw.f_addr.ss_family, SOCK_STREAM,
	    IPPROTO_TCP)) == -1) {
		snprintf(ebuf, sizeof(ebuf), "socket \"%s\"",
		    f->f_un.f_forw.f_loghost);
		logerror(ebuf);
		return (-1);
	}
	/* Connect must not block the process. */
	if ((flags = fcntl(s, F_GETFL)) == -1 ||
	    fcntl(s, F_SETFL, flags | O_NONBLOCK) == -1) {
		snprintf(ebuf, sizeof(ebuf), "fcntl \"%s\" O_NONBLOCK",
		    f->f_un.f_forw.f_loghost);
		logerror(ebuf);
		close(s);
		return (-1);
	}
	if (connect(s, (struct sockaddr *)&f->f_un.f_forw.f_addr,
	    f->f_un.f_forw.f_addr.ss_len) == -1 && errno != EINPROGRESS) {
		snprintf(ebuf, sizeof(ebuf), "connect \"%s\"",
		    f->f_un.f_forw.f_loghost);
		logerror(ebuf);
		close(s);
		return (-1);
	}
	return (s);
}

void
tcp_readcb(struct bufferevent *bufev, void *arg)
{
	struct filed	*f = arg;

	/*
	 * Drop data received from the forward log server.
	 */
	dprintf("loghost \"%s\" did send %zu bytes back\n",
	    f->f_un.f_forw.f_loghost, EVBUFFER_LENGTH(bufev->input));
	evbuffer_drain(bufev->input, -1);
}

void
tcp_writecb(struct bufferevent *bufev, void *arg)
{
	struct filed	*f = arg;
	char		 ebuf[ERRBUFSIZE];

	/*
	 * Successful write, connection to server is good, reset wait time.
	 */
	dprintf("loghost \"%s\" successful write\n", f->f_un.f_forw.f_loghost);
	f->f_un.f_forw.f_reconnectwait = 0;

	if (f->f_un.f_forw.f_dropped > 0 &&
	    EVBUFFER_LENGTH(f->f_un.f_forw.f_bufev->output) < MAX_TCPBUF) {
		snprintf(ebuf, sizeof(ebuf),
		    "syslogd: dropped %d messages to loghost \"%s\"",
		    f->f_un.f_forw.f_dropped, f->f_un.f_forw.f_loghost);
		f->f_un.f_forw.f_dropped = 0;
		logmsg(LOG_SYSLOG|LOG_WARNING, ebuf, LocalHostName, ADDDATE);
	}
}

void
tcp_errorcb(struct bufferevent *bufev, short event, void *arg)
{
	struct filed	*f = arg;
	char		*p, *buf, *end;
	int		 l;
	char		 ebuf[ERRBUFSIZE];

	if (event & EVBUFFER_EOF)
		snprintf(ebuf, sizeof(ebuf),
		    "syslogd: loghost \"%s\" connection close",
		    f->f_un.f_forw.f_loghost);
	else
		snprintf(ebuf, sizeof(ebuf),
		    "syslogd: loghost \"%s\" connection error: %s",
		    f->f_un.f_forw.f_loghost, f->f_un.f_forw.f_ctx ?
		    tls_error(f->f_un.f_forw.f_ctx) : strerror(errno));
	dprintf("%s\n", ebuf);

	/* The SIGHUP handler may also close the socket, so invalidate it. */
	if (f->f_un.f_forw.f_ctx) {
		tls_close(f->f_un.f_forw.f_ctx);
		tls_free(f->f_un.f_forw.f_ctx);
		f->f_un.f_forw.f_ctx = NULL;
	}
	close(f->f_file);
	f->f_file = -1;

	/*
	 * The messages in the output buffer may be out of sync.
	 * Check that the buffer starts with "1234 <1234 octets>\n".
	 * Otherwise remove the partial message from the beginning.
	 */
	buf = EVBUFFER_DATA(bufev->output);
	end = buf + EVBUFFER_LENGTH(bufev->output);
	for (p = buf; p < end && p < buf + 4; p++) {
		if (!isdigit(*p))
			break;
	}
	if (buf < end && !(buf + 1 <= p && p < end && *p == ' ' &&
	    (l = atoi(buf)) > 0 && buf + l < end && buf[l] == '\n')) {
		for (p = buf; p < end; p++) {
			if (*p == '\n') {
				evbuffer_drain(bufev->output, p - buf + 1);
				break;
			}
		}
		/* Without '\n' discard everything. */
		if (p == end)
			evbuffer_drain(bufev->output, p - buf);
		dprintf("loghost \"%s\" dropped partial message\n",
		    f->f_un.f_forw.f_loghost);
		f->f_un.f_forw.f_dropped++;
	}

	tcp_connectcb(-1, 0, f);

	/* Log the connection error to the fresh buffer after reconnecting. */
	logmsg(LOG_SYSLOG|LOG_WARNING, ebuf, LocalHostName, ADDDATE);
}

void
tcp_connectcb(int fd, short event, void *arg)
{
	struct filed		*f = arg;
	struct bufferevent	*bufev = f->f_un.f_forw.f_bufev;
	struct tls		*ctx;
	struct timeval		 to;
	int			 s;

	if ((event & EV_TIMEOUT) == 0 && f->f_un.f_forw.f_reconnectwait > 0)
		goto retry;

	/* Avoid busy reconnect loop, delay until successful write. */
	if (f->f_un.f_forw.f_reconnectwait == 0)
		f->f_un.f_forw.f_reconnectwait = 1;

	if ((s = tcp_socket(f)) == -1)
		goto retry;
	dprintf("tcp connect callback: socket success, event %#x\n", event);
	f->f_file = s;

	bufferevent_setfd(bufev, s);
	bufferevent_setcb(bufev, tcp_readcb, tcp_writecb, tcp_errorcb, f);
	/*
	 * Although syslog is a write only protocol, enable reading from
	 * the socket to detect connection close and errors.
	 */
	bufferevent_enable(bufev, EV_READ|EV_WRITE);

	if (f->f_type == F_FORWTLS) {
		if ((ctx = tls_socket(f)) == NULL) {
			close(f->f_file);
			f->f_file = -1;
			goto retry;
		}
		dprintf("tcp connect callback: TLS context success\n");
		f->f_un.f_forw.f_ctx = ctx;

		buffertls_set(&f->f_un.f_forw.f_buftls, bufev, ctx, s);
		buffertls_connect(&f->f_un.f_forw.f_buftls, s,
		    f->f_un.f_forw.f_host);
	}

	return;

 retry:
	f->f_un.f_forw.f_reconnectwait <<= 1;
	if (f->f_un.f_forw.f_reconnectwait > 600)
		f->f_un.f_forw.f_reconnectwait = 600;
	to.tv_sec = f->f_un.f_forw.f_reconnectwait;
	to.tv_usec = 0;

	dprintf("tcp connect callback: retry, event %#x, wait %d\n",
	    event, f->f_un.f_forw.f_reconnectwait);
	bufferevent_setfd(bufev, -1);
	/* We can reuse the write event as bufferevent is disabled. */
	evtimer_set(&bufev->ev_write, tcp_connectcb, f);
	evtimer_add(&bufev->ev_write, &to);
}

struct tls *
tls_socket(struct filed *f)
{
	struct tls	*ctx;
	char		 ebuf[ERRBUFSIZE];

	if ((ctx = tls_client()) == NULL) {
		snprintf(ebuf, sizeof(ebuf), "tls_client \"%s\"",
		    f->f_un.f_forw.f_loghost);
		logerror(ebuf);
		return (NULL);
	}
	if (tlsconfig) {
		if (tls_configure(ctx, tlsconfig) < 0) {
			snprintf(ebuf, sizeof(ebuf), "tls_configure \"%s\": %s",
			    f->f_un.f_forw.f_loghost, tls_error(ctx));
			logerror(ebuf);
			tls_free(ctx);
			return (NULL);
		}
	}
	return (ctx);
}

int
tcpbuf_countmsg(struct bufferevent *bufev)
{
	char	*p, *buf, *end;
	int	 i = 0;

	buf = EVBUFFER_DATA(bufev->output);
	end = buf + EVBUFFER_LENGTH(bufev->output);
	for (p = buf; p < end; p++) {
		if (*p == '\n')
			i++;
	}
	return (i);
}

void
usage(void)
{

	(void)fprintf(stderr,
	    "usage: syslogd [-46dhnuV] [-a path] [-C CAfile] [-f config_file]\n"
	    "               [-m mark_interval] [-p log_socket] [-s reporting_socket]\n");
	exit(1);
}

/*
 * Take a raw input line, decode the message, and print the message
 * on the appropriate log files.
 */
void
printline(char *hname, char *msg)
{
	int pri;
	char *p, *q, line[MAXLINE + 4 + 1];  /* message, encoding, NUL */

	/* test for special codes */
	pri = DEFUPRI;
	p = msg;
	if (*p == '<') {
		pri = 0;
		while (isdigit((unsigned char)*++p))
			pri = 10 * pri + (*p - '0');
		if (*p == '>')
			++p;
	}
	if (pri &~ (LOG_FACMASK|LOG_PRIMASK))
		pri = DEFUPRI;

	/*
	 * Don't allow users to log kernel messages.
	 * NOTE: since LOG_KERN == 0 this will also match
	 * messages with no facility specified.
	 */
	if (LOG_FAC(pri) == LOG_KERN)
		pri = LOG_USER | LOG_PRI(pri);

	for (q = line; *p && q < &line[MAXLINE]; p++) {
		if (*p == '\n')
			*q++ = ' ';
		else
			q = vis(q, *p, 0, 0);
	}
	line[MAXLINE] = *q = '\0';

	logmsg(pri, line, hname, 0);
}

/*
 * Take a raw input line from /dev/klog, split and format similar to syslog().
 */
void
printsys(char *msg)
{
	int c, pri, flags;
	char *lp, *p, *q, line[MAXLINE + 1];

	(void)snprintf(line, sizeof line, "%s: ", _PATH_UNIX);
	lp = line + strlen(line);
	for (p = msg; *p != '\0'; ) {
		flags = SYNC_FILE | ADDDATE;	/* fsync file after write */
		pri = DEFSPRI;
		if (*p == '<') {
			pri = 0;
			while (isdigit((unsigned char)*++p))
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
		while (*p && (c = *p++) != '\n' && q < &line[sizeof(line) - 4])
			q = vis(q, c, 0, 0);

		logmsg(pri, line, LocalHostName, flags);
	}
}

time_t	now;

/*
 * Log a message to the appropriate log files, users, etc. based on
 * the priority.
 */
void
logmsg(int pri, char *msg, char *from, int flags)
{
	struct filed *f;
	int fac, msglen, prilev, i;
	char *timestamp;
	char prog[NAME_MAX+1];

	dprintf("logmsg: pri 0%o, flags 0x%x, from %s, msg %s\n",
	    pri, flags, from, msg);

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
	else {
		fac = LOG_FAC(pri);
		if (fac >= LOG_NFACILITIES || fac < 0)
			fac = LOG_USER;
	}
	prilev = LOG_PRI(pri);

	/* extract program name */
	while (isspace((unsigned char)*msg))
		msg++;
	for (i = 0; i < NAME_MAX; i++) {
		if (!isalnum((unsigned char)msg[i]) && msg[i] != '-')
			break;
		prog[i] = msg[i];
	}
	prog[i] = 0;

	/* log the message to the particular outputs */
	if (!Initialized) {
		f = &consfile;
		f->f_file = priv_open_tty(ctty);

		if (f->f_file >= 0) {
			fprintlog(f, flags, msg);
			(void)close(f->f_file);
			f->f_file = -1;
		}
		return;
	}
	SIMPLEQ_FOREACH(f, &Files, f_next) {
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
			strlcpy(f->f_lasttime, timestamp, 16);
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
			strlcpy(f->f_lasttime, timestamp, 16);
			strlcpy(f->f_prevhost, from,
			    sizeof(f->f_prevhost));
			if (msglen < MAXSVLINE) {
				f->f_prevlen = msglen;
				strlcpy(f->f_prevline, msg, sizeof(f->f_prevline));
				fprintlog(f, flags, (char *)NULL);
			} else {
				f->f_prevline[0] = 0;
				f->f_prevlen = 0;
				fprintlog(f, flags, msg);
			}
		}

		if (f->f_quick)
			break;
	}
}

void
fprintlog(struct filed *f, int flags, char *msg)
{
	struct iovec iov[6];
	struct iovec *v;
	int l, retryonce;
	char line[MAXLINE + 1], repbuf[80], greetings[500];

	v = iov;
	if (f->f_type == F_WALL) {
		l = snprintf(greetings, sizeof(greetings),
		    "\r\n\7Message from syslogd@%s at %.24s ...\r\n",
		    f->f_prevhost, ctime(&now));
		if (l < 0 || (size_t)l >= sizeof(greetings))
			l = strlen(greetings);
		v->iov_base = greetings;
		v->iov_len = l;
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
		l = snprintf(repbuf, sizeof(repbuf),
		    "last message repeated %d times", f->f_prevcount);
		if (l < 0 || (size_t)l >= sizeof(repbuf))
			l = strlen(repbuf);
		v->iov_base = repbuf;
		v->iov_len = l;
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

	case F_FORWUDP:
		dprintf(" %s\n", f->f_un.f_forw.f_loghost);
		l = snprintf(line, MINIMUM(MAX_UDPMSG + 1, sizeof(line)),
		    "<%d>%.15s %s%s%s", f->f_prevpri, (char *)iov[0].iov_base,
		    IncludeHostname ? LocalHostName : "",
		    IncludeHostname ? " " : "",
		    (char *)iov[4].iov_base);
		if (l < 0 || (size_t)l > MINIMUM(MAX_UDPMSG, sizeof(line)))
			l = MINIMUM(MAX_UDPMSG, sizeof(line));
		if (sendto(f->f_file, line, l, 0,
		    (struct sockaddr *)&f->f_un.f_forw.f_addr,
		    f->f_un.f_forw.f_addr.ss_len) != l) {
			switch (errno) {
			case EHOSTDOWN:
			case EHOSTUNREACH:
			case ENETDOWN:
			case ENETUNREACH:
			case ENOBUFS:
				/* silently dropped */
				break;
			default:
				f->f_type = F_UNUSED;
				logerror("sendto");
				break;
			}
		}
		break;

	case F_FORWTCP:
	case F_FORWTLS:
		dprintf(" %s", f->f_un.f_forw.f_loghost);
		if (EVBUFFER_LENGTH(f->f_un.f_forw.f_bufev->output) >=
		    MAX_TCPBUF) {
			dprintf(" (dropped)\n");
			f->f_un.f_forw.f_dropped++;
			break;
		}
		/*
		 * Syslog over TLS  RFC 5425  4.3.  Sending Data
		 * Syslog over TCP  RFC 6587  3.4.1.  Octet Counting
		 * Use an additional '\n' to split messages.  This allows
		 * buffer synchronisation, helps legacy implementations,
		 * and makes line based testing easier.
		 */
		l = snprintf(line, sizeof(line), "<%d>%.15s %s%s\n",
		    f->f_prevpri, (char *)iov[0].iov_base,
		    IncludeHostname ? LocalHostName : "",
		    IncludeHostname ? " " : "");
		if (l < 0) {
			dprintf(" (dropped snprintf)\n");
			f->f_un.f_forw.f_dropped++;
			break;
		}
		l = evbuffer_add_printf(f->f_un.f_forw.f_bufev->output,
		    "%zu <%d>%.15s %s%s%s\n",
		    (size_t)l + strlen(iov[4].iov_base),
		    f->f_prevpri, (char *)iov[0].iov_base,
		    IncludeHostname ? LocalHostName : "",
		    IncludeHostname ? " " : "",
		    (char *)iov[4].iov_base);
		if (l < 0) {
			dprintf(" (dropped evbuffer_add_printf)\n");
			f->f_un.f_forw.f_dropped++;
			break;
		}
		bufferevent_enable(f->f_un.f_forw.f_bufev, EV_WRITE);
		dprintf("\n");
		break;

	case F_CONSOLE:
		if (flags & IGN_CONS) {
			dprintf(" (ignored)\n");
			break;
		}
		/* FALLTHROUGH */

	case F_TTY:
	case F_FILE:
	case F_PIPE:
		dprintf(" %s\n", f->f_un.f_fname);
		if (f->f_type != F_FILE && f->f_type != F_PIPE) {
			v->iov_base = "\r\n";
			v->iov_len = 2;
		} else {
			v->iov_base = "\n";
			v->iov_len = 1;
		}
		retryonce = 0;
	again:
		if (writev(f->f_file, iov, 6) < 0) {
			int e = errno;

			/* pipe is non-blocking. log and drop message if full */
			if (e == EAGAIN && f->f_type == F_PIPE) {
				if (now - f->f_lasterrtime > 120) {
					f->f_lasterrtime = now;
					logerror(f->f_un.f_fname);
				}
				break;
			}

			(void)close(f->f_file);
			/*
			 * Check for errors on TTY's or program pipes.
			 * Errors happen due to loss of tty or died programs.
			 */
			if (e == EAGAIN) {
				/*
				 * Silently drop messages on blocked write.
				 * This can happen when logging to a locked tty.
				 */
				break;
			} else if ((e == EIO || e == EBADF) &&
			    f->f_type != F_FILE && f->f_type != F_PIPE &&
			    !retryonce) {
				f->f_file = priv_open_tty(f->f_un.f_fname);
				retryonce = 1;
				if (f->f_file < 0) {
					f->f_type = F_UNUSED;
					logerror(f->f_un.f_fname);
				} else
					goto again;
			} else if ((e == EPIPE || e == EBADF) &&
			    f->f_type == F_PIPE && !retryonce) {
				f->f_file = priv_open_log(f->f_un.f_fname);
				retryonce = 1;
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

	case F_MEMBUF:
		dprintf("\n");
		snprintf(line, sizeof(line), "%.15s %s %s",
		    (char *)iov[0].iov_base, (char *)iov[2].iov_base,
		    (char *)iov[4].iov_base);
		if (ringbuf_append_line(f->f_un.f_mb.f_rb, line) == 1)
			f->f_un.f_mb.f_overflow = 1;
		if (f->f_un.f_mb.f_attached)
			ctlconn_logto(line);
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
wallmsg(struct filed *f, struct iovec *iov)
{
	struct utmp ut;
	char line[sizeof(ut.ut_line) + 1], *p;
	static int reenter;			/* avoid calling ourselves */
	FILE *uf;
	int i;

	if (reenter++)
		return;
	if ((uf = priv_open_utmp()) == NULL) {
		logerror(_PATH_UTMP);
		reenter = 0;
		return;
	}
	/* NOSTRICT */
	while (fread((char *)&ut, sizeof(ut), 1, uf) == 1) {
		if (ut.ut_name[0] == '\0')
			continue;
		/* must use strncpy since ut_* may not be NUL terminated */
		strncpy(line, ut.ut_line, sizeof(line) - 1);
		line[sizeof(line) - 1] = '\0';
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

/*
 * Return a printable representation of a host address.
 */
void
cvthname(struct sockaddr *f, char *result, size_t res_len)
{
	if (getnameinfo(f, f->sa_len, result, res_len, NULL, 0,
	    NI_NUMERICHOST|NI_NUMERICSERV|NI_DGRAM) != 0) {
		dprintf("Malformed from address\n");
		strlcpy(result, "???", res_len);
		return;
	}
	dprintf("cvthname(%s)\n", result);
	if (NoDNS)
		return;

	if (priv_getnameinfo(f, f->sa_len, result, res_len) != 0)
		dprintf("Host name for your address (%s) unknown\n", result);
}

void
die_signalcb(int signum, short event, void *arg)
{
	die(signum);
}

void
mark_timercb(int unused, short event, void *arg)
{
	markit();
}

void
init_signalcb(int signum, short event, void *arg)
{
	char	 ebuf[ERRBUFSIZE];

	init();

	logmsg(LOG_SYSLOG|LOG_INFO, "syslogd: restart",
	    LocalHostName, ADDDATE);
	dprintf("syslogd: restarted\n");

	if (tcpbuf_dropped > 0) {
		snprintf(ebuf, sizeof(ebuf),
		    "syslogd: dropped %d messages to remote loghost",
		    tcpbuf_dropped);
		tcpbuf_dropped = 0;
		logmsg(LOG_SYSLOG|LOG_WARNING, ebuf, LocalHostName, ADDDATE);
	}
}

/*
 * Print syslogd errors some place.
 */
void
logerror(const char *type)
{
	char ebuf[ERRBUFSIZE];

	if (errno)
		(void)snprintf(ebuf, sizeof(ebuf), "syslogd: %s: %s",
		    type, strerror(errno));
	else
		(void)snprintf(ebuf, sizeof(ebuf), "syslogd: %s", type);
	errno = 0;
	dprintf("%s\n", ebuf);
	if (Startup)
		fprintf(stderr, "%s\n", ebuf);
	else
		logmsg(LOG_SYSLOG|LOG_ERR, ebuf, LocalHostName, ADDDATE);
}

void
die(int signo)
{
	struct filed *f;
	int was_initialized = Initialized;
	char ebuf[ERRBUFSIZE];

	Initialized = 0;		/* Don't log SIGCHLDs */
	SIMPLEQ_FOREACH(f, &Files, f_next) {
		/* flush any pending output */
		if (f->f_prevcount)
			fprintlog(f, 0, (char *)NULL);
		if (f->f_type == F_FORWTLS || f->f_type == F_FORWTCP) {
			tcpbuf_dropped += f->f_un.f_forw.f_dropped +
			    tcpbuf_countmsg(f->f_un.f_forw.f_bufev);
			f->f_un.f_forw.f_dropped = 0;
		}
	}
	Initialized = was_initialized;

	if (tcpbuf_dropped > 0) {
		snprintf(ebuf, sizeof(ebuf),
		    "syslogd: dropped %d messages to remote loghost",
		    tcpbuf_dropped);
		tcpbuf_dropped = 0;
		logmsg(LOG_SYSLOG|LOG_WARNING, ebuf, LocalHostName, ADDDATE);
	}

	if (signo) {
		dprintf("syslogd: exiting on signal %d\n", signo);
		(void)snprintf(ebuf, sizeof(ebuf), "exiting on signal %d",
		    signo);
		errno = 0;
		logerror(ebuf);
	}
	dprintf("[unpriv] syslogd child about to exit\n");
	exit(0);
}

/*
 *  INIT -- Initialize syslogd from configuration table
 */
void
init(void)
{
	char cline[LINE_MAX], prog[NAME_MAX+1], *p;
	struct filed_list mb;
	struct filed *f, *m;
	FILE *cf;
	int i;

	dprintf("init\n");

	/* If config file has been modified, then just die to restart */
	if (priv_config_modified()) {
		dprintf("config file changed: dying\n");
		die(0);
	}

	/*
	 *  Close all open log files.
	 */
	Initialized = 0;
	SIMPLEQ_INIT(&mb);
	while (!SIMPLEQ_EMPTY(&Files)) {
		f = SIMPLEQ_FIRST(&Files);
		SIMPLEQ_REMOVE_HEAD(&Files, f_next);
		/* flush any pending output */
		if (f->f_prevcount)
			fprintlog(f, 0, (char *)NULL);

		switch (f->f_type) {
		case F_FORWTLS:
			if (f->f_un.f_forw.f_ctx) {
				tls_close(f->f_un.f_forw.f_ctx);
				tls_free(f->f_un.f_forw.f_ctx);
			}
			free(f->f_un.f_forw.f_host);
			/* FALLTHROUGH */
		case F_FORWTCP:
			tcpbuf_dropped += f->f_un.f_forw.f_dropped +
			     tcpbuf_countmsg(f->f_un.f_forw.f_bufev);
			bufferevent_free(f->f_un.f_forw.f_bufev);
			/* FALLTHROUGH */
		case F_FILE:
		case F_TTY:
		case F_CONSOLE:
		case F_PIPE:
			(void)close(f->f_file);
			break;
		}
		if (f->f_program)
			free(f->f_program);
		if (f->f_type == F_MEMBUF) {
			f->f_program = NULL;
			dprintf("add %p to mb\n", f);
			SIMPLEQ_INSERT_HEAD(&mb, f, f_next);
		} else
			free(f);
	}
	SIMPLEQ_INIT(&Files);

	/* open the configuration file */
	if ((cf = priv_open_config()) == NULL) {
		dprintf("cannot open %s\n", ConfFile);
		SIMPLEQ_INSERT_TAIL(&Files, cfline("*.ERR\t/dev/console", "*"),
		    f_next);
		SIMPLEQ_INSERT_TAIL(&Files, cfline("*.PANIC\t*", "*"), f_next);
		Initialized = 1;
		return;
	}

	/*
	 *  Foreach line in the conf table, open that file.
	 */
	f = NULL;
	strlcpy(prog, "*", sizeof(prog));
	while (fgets(cline, sizeof(cline), cf) != NULL) {
		/*
		 * check for end-of-section, comments, strip off trailing
		 * spaces and newline character. !prog is treated
		 * specially: the following lines apply only to that program.
		 */
		for (p = cline; isspace((unsigned char)*p); ++p)
			continue;
		if (*p == '\0' || *p == '#')
			continue;
		if (*p == '!') {
			p++;
			while (isspace((unsigned char)*p))
				p++;
			if (!*p || (*p == '*' && (!p[1] ||
			    isspace((unsigned char)p[1])))) {
				strlcpy(prog, "*", sizeof(prog));
				continue;
			}
			for (i = 0; i < NAME_MAX; i++) {
				if (!isalnum((unsigned char)p[i]) &&
				    p[i] != '-' && p[i] != '!')
					break;
				prog[i] = p[i];
			}
			prog[i] = 0;
			continue;
		}
		p = cline + strlen(cline);
		while (p > cline)
			if (!isspace((unsigned char)*--p)) {
				p++;
				break;
			}
		*p = '\0';
		f = cfline(cline, prog);
		if (f != NULL)
			SIMPLEQ_INSERT_TAIL(&Files, f, f_next);
	}

	/* Match and initialize the memory buffers */
	SIMPLEQ_FOREACH(f, &Files, f_next) {
		if (f->f_type != F_MEMBUF)
			continue;
		dprintf("Initialize membuf %s at %p\n", f->f_un.f_mb.f_mname, f);

		SIMPLEQ_FOREACH(m, &mb, f_next) {
			if (m->f_un.f_mb.f_rb == NULL)
				continue;
			if (strcmp(m->f_un.f_mb.f_mname,
			    f->f_un.f_mb.f_mname) == 0)
				break;
		}
		if (m == NULL) {
			dprintf("Membuf no match\n");
			f->f_un.f_mb.f_rb = ringbuf_init(f->f_un.f_mb.f_len);
			if (f->f_un.f_mb.f_rb == NULL) {
				f->f_type = F_UNUSED;
				logerror("Failed to allocate membuf");
			}
		} else {
			dprintf("Membuf match f:%p, m:%p\n", f, m);
			f->f_un = m->f_un;
			m->f_un.f_mb.f_rb = NULL;
		}
	}

	/* make sure remaining buffers are freed */
	while (!SIMPLEQ_EMPTY(&mb)) {
		m = SIMPLEQ_FIRST(&mb);
		SIMPLEQ_REMOVE_HEAD(&mb, f_next);
		if (m->f_un.f_mb.f_rb != NULL) {
			logerror("Mismatched membuf");
			ringbuf_free(m->f_un.f_mb.f_rb);
		}
		dprintf("Freeing membuf %p\n", m);

		free(m);
	}

	/* close the configuration file */
	(void)fclose(cf);

	Initialized = 1;

	if (Debug) {
		SIMPLEQ_FOREACH(f, &Files, f_next) {
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
			case F_PIPE:
				printf("%s", f->f_un.f_fname);
				break;

			case F_FORWUDP:
			case F_FORWTCP:
			case F_FORWTLS:
				printf("%s", f->f_un.f_forw.f_loghost);
				break;

			case F_USERS:
				for (i = 0; i < MAXUNAMES && *f->f_un.f_uname[i]; i++)
					printf("%s, ", f->f_un.f_uname[i]);
				break;

			case F_MEMBUF:
				printf("%s", f->f_un.f_mb.f_mname);
				break;

			}
			if (f->f_program)
				printf(" (%s)", f->f_program);
			printf("\n");
		}
	}
}

#define progmatches(p1, p2) \
	(p1 == p2 || (p1 != NULL && p2 != NULL && strcmp(p1, p2) == 0))

/*
 * Spot a line with a duplicate file, pipe, console, tty, or membuf target.
 */
struct filed *
find_dup(struct filed *f)
{
	struct filed *list;

	SIMPLEQ_FOREACH(list, &Files, f_next) {
		if (list->f_quick || f->f_quick)
			continue;
		switch (list->f_type) {
		case F_FILE:
		case F_TTY:
		case F_CONSOLE:
		case F_PIPE:
			if (strcmp(list->f_un.f_fname, f->f_un.f_fname) == 0 &&
			    progmatches(list->f_program, f->f_program))
				return (list);
			break;
		case F_MEMBUF:
			if (strcmp(list->f_un.f_mb.f_mname,
			    f->f_un.f_mb.f_mname) == 0 &&
			    progmatches(list->f_program, f->f_program))
				return (list);
			break;
		}
	}
	return (NULL);
}

/*
 * Crack a configuration file line
 */
struct filed *
cfline(char *line, char *prog)
{
	int i, pri;
	size_t rb_len;
	char *bp, *p, *q, *proto, *host, *port, *ipproto;
	char buf[MAXLINE], ebuf[ERRBUFSIZE];
	struct filed *xf, *f, *d;
	struct timeval to;

	dprintf("cfline(\"%s\", f, \"%s\")\n", line, prog);

	errno = 0;	/* keep strerror() stuff out of logerror messages */

	if ((f = calloc(1, sizeof(*f))) == NULL) {
		logerror("Couldn't allocate struct filed");
		die(0);
	}
	for (i = 0; i <= LOG_NFACILITIES; i++)
		f->f_pmask[i] = INTERNAL_NOPRI;

	/* save program name if any */
	if (*prog == '!') {
		f->f_quick = 1;
		prog++;
	} else
		f->f_quick = 0;
	if (!strcmp(prog, "*"))
		prog = NULL;
	else
		f->f_program = strdup(prog);

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
		while (*q && strchr(", ;", *q))
			q++;

		/* decode priority name */
		if (*buf == '*')
			pri = LOG_PRIMASK + 1;
		else {
			/* ignore trailing spaces */
			for (i=strlen(buf)-1; i >= 0 && buf[i] == ' '; i--) {
				buf[i]='\0';
			}

			pri = decode(buf, prioritynames);
			if (pri < 0) {
				(void)snprintf(ebuf, sizeof ebuf,
				    "unknown priority name \"%s\"", buf);
				logerror(ebuf);
				free(f);
				return (NULL);
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
					free(f);
					return (NULL);
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

	switch (*p) {
	case '@':
		if ((strlcpy(f->f_un.f_forw.f_loghost, p,
		    sizeof(f->f_un.f_forw.f_loghost)) >=
		    sizeof(f->f_un.f_forw.f_loghost))) {
			snprintf(ebuf, sizeof(ebuf), "loghost too long \"%s\"",
			    p);
			logerror(ebuf);
			break;
		}
		if (loghost(++p, &proto, &host, &port) == -1) {
			snprintf(ebuf, sizeof(ebuf), "bad loghost \"%s\"",
			    f->f_un.f_forw.f_loghost);
			logerror(ebuf);
			break;
		}
		if (proto == NULL)
			proto = "udp";
		ipproto = proto;
		if (strcmp(proto, "udp") == 0) {
			if (fd_udp == -1)
				proto = "udp6";
			if (fd_udp6 == -1)
				proto = "udp4";
			ipproto = proto;
		} else if (strcmp(proto, "udp4") == 0) {
			if (fd_udp == -1) {
				snprintf(ebuf, sizeof(ebuf), "no udp4 \"%s\"",
				    f->f_un.f_forw.f_loghost);
				logerror(ebuf);
				break;
			}
		} else if (strcmp(proto, "udp6") == 0) {
			if (fd_udp6 == -1) {
				snprintf(ebuf, sizeof(ebuf), "no udp6 \"%s\"",
				    f->f_un.f_forw.f_loghost);
				logerror(ebuf);
				break;
			}
		} else if (strcmp(proto, "tcp") == 0 ||
		    strcmp(proto, "tcp4") == 0 || strcmp(proto, "tcp6") == 0) {
			;
		} else if (strcmp(proto, "tls") == 0) {
			ipproto = "tcp";
		} else if (strcmp(proto, "tls4") == 0) {
			ipproto = "tcp4";
		} else if (strcmp(proto, "tls6") == 0) {
			ipproto = "tcp6";
		} else {
			snprintf(ebuf, sizeof(ebuf), "bad protocol \"%s\"",
			    f->f_un.f_forw.f_loghost);
			logerror(ebuf);
			break;
		}
		if (strlen(host) >= NI_MAXHOST) {
			snprintf(ebuf, sizeof(ebuf), "host too long \"%s\"",
			    f->f_un.f_forw.f_loghost);
			logerror(ebuf);
			break;
		}
		if (port == NULL)
			port = strncmp(proto, "tls", 3) == 0 ?
			    "syslog-tls" : "syslog";
		if (strlen(port) >= NI_MAXSERV) {
			snprintf(ebuf, sizeof(ebuf), "port too long \"%s\"",
			    f->f_un.f_forw.f_loghost);
			logerror(ebuf);
			break;
		}
		if (priv_getaddrinfo(ipproto, host, port,
		    (struct sockaddr*)&f->f_un.f_forw.f_addr,
		    sizeof(f->f_un.f_forw.f_addr)) != 0) {
			snprintf(ebuf, sizeof(ebuf), "bad hostname \"%s\"",
			    f->f_un.f_forw.f_loghost);
			logerror(ebuf);
			break;
		}
		f->f_file = -1;
		if (strncmp(proto, "udp", 3) == 0) {
			switch (f->f_un.f_forw.f_addr.ss_family) {
			case AF_INET:
				f->f_file = fd_udp;
				break;
			case AF_INET6:
				f->f_file = fd_udp6;
				break;
			}
			f->f_type = F_FORWUDP;
		} else if (strncmp(ipproto, "tcp", 3) == 0) {
			if ((f->f_un.f_forw.f_bufev = bufferevent_new(-1,
			    tcp_readcb, tcp_writecb, tcp_errorcb, f)) == NULL) {
				snprintf(ebuf, sizeof(ebuf),
				    "bufferevent \"%s\"",
				    f->f_un.f_forw.f_loghost);
				logerror(ebuf);
				break;
			}
			if (strncmp(proto, "tls", 3) == 0) {
				f->f_un.f_forw.f_host = strdup(host);
				f->f_type = F_FORWTLS;
			} else {
				f->f_type = F_FORWTCP;
			}
			/*
			 * If we try to connect to a TLS server immediately
			 * syslogd gets an SIGPIPE as the signal handlers have
			 * not been set up.  Delay the connection until the
			 * event loop is started.  We can reuse the write event
			 * for that as bufferevent is still disabled.
			 */
			to.tv_sec = 0;
			to.tv_usec = 1;
			evtimer_set(&f->f_un.f_forw.f_bufev->ev_write,
			    tcp_connectcb, f);
			evtimer_add(&f->f_un.f_forw.f_bufev->ev_write, &to);
		}
		break;

	case '/':
	case '|':
		(void)strlcpy(f->f_un.f_fname, p, sizeof(f->f_un.f_fname));
		d = find_dup(f);
		if (d != NULL) {
			for (i = 0; i <= LOG_NFACILITIES; i++)
				if (f->f_pmask[i] != INTERNAL_NOPRI)
					d->f_pmask[i] = f->f_pmask[i];
			free(f);
			return (NULL);
		}
		if (strcmp(p, ctty) == 0)
			f->f_file = priv_open_tty(p);
		else
			f->f_file = priv_open_log(p);
		if (f->f_file < 0) {
			f->f_type = F_UNUSED;
			logerror(p);
			break;
		}
		if (isatty(f->f_file)) {
			if (strcmp(p, ctty) == 0)
				f->f_type = F_CONSOLE;
			else
				f->f_type = F_TTY;
		} else {
			if (*p == '|')
				f->f_type = F_PIPE;
			else {
				f->f_type = F_FILE;

				/* Clear O_NONBLOCK flag on f->f_file */
				if ((i = fcntl(f->f_file, F_GETFL, 0)) != -1) {
					i &= ~O_NONBLOCK;
					fcntl(f->f_file, F_SETFL, i);
				}
			}
		}
		break;

	case '*':
		f->f_type = F_WALL;
		break;

	case ':':
		f->f_type = F_MEMBUF;

		/* Parse buffer size (in kb) */
		errno = 0;
		rb_len = strtoul(++p, &q, 0);
		if (*p == '\0' || (errno == ERANGE && rb_len == ULONG_MAX) ||
		    *q != ':' || rb_len == 0) {
			f->f_type = F_UNUSED;
			logerror(p);
			break;
		}
		q++;
		rb_len *= 1024;

		/* Copy buffer name */
		for(i = 0; (size_t)i < sizeof(f->f_un.f_mb.f_mname) - 1; i++) {
			if (!isalnum((unsigned char)q[i]))
				break;
			f->f_un.f_mb.f_mname[i] = q[i];
		}

		/* Make sure buffer name is unique */
		xf = find_dup(f);

		/* Error on missing or non-unique name, or bad buffer length */
		if (i == 0 || rb_len > MAX_MEMBUF || xf != NULL) {
			f->f_type = F_UNUSED;
			logerror(p);
			break;
		}

		/* Set buffer length */
		rb_len = MAXIMUM(rb_len, MIN_MEMBUF);
		f->f_un.f_mb.f_len = rb_len;
		f->f_un.f_mb.f_overflow = 0;
		f->f_un.f_mb.f_attached = 0;
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
	return (f);
}

/*
 * Parse the host and port parts from a loghost string.
 */
int
loghost(char *str, char **proto, char **host, char **port)
{
	*proto = NULL;
	if ((*host = strchr(str, ':')) &&
	    (*host)[1] == '/' && (*host)[2] == '/') {
		*proto = str;
		**host = '\0';
		str = *host + 3;
	}
	*host = str;
	if (**host == '[') {
		(*host)++;
		str = strchr(*host, ']');
		if (str == NULL)
			return (-1);
		*str++ = '\0';
	}
	*port = strrchr(str, ':');
	if (*port != NULL)
		*(*port)++ = '\0';

	return (0);
}

/*
 * Retrieve the size of the kernel message buffer, via sysctl.
 */
int
getmsgbufsize(void)
{
	int msgbufsize, mib[2];
	size_t size;

	mib[0] = CTL_KERN;
	mib[1] = KERN_MSGBUFSIZE;
	size = sizeof msgbufsize;
	if (sysctl(mib, 2, &msgbufsize, &size, NULL, 0) == -1) {
		dprintf("couldn't get kern.msgbufsize\n");
		return (0);
	}
	return (msgbufsize);
}

/*
 *  Decode a symbolic name to a numeric value
 */
int
decode(const char *name, const CODE *codetab)
{
	const CODE *c;
	char *p, buf[40];

	for (p = buf; *name && p < &buf[sizeof(buf) - 1]; p++, name++) {
		if (isupper((unsigned char)*name))
			*p = tolower((unsigned char)*name);
		else
			*p = *name;
	}
	*p = '\0';
	for (c = codetab; c->c_name; c++)
		if (!strcmp(buf, c->c_name))
			return (c->c_val);

	return (-1);
}

void
markit(void)
{
	struct filed *f;

	now = time(NULL);
	MarkSeq += TIMERINTVL;
	if (MarkSeq >= MarkInterval) {
		logmsg(LOG_INFO, "-- MARK --",
		    LocalHostName, ADDDATE|MARK);
		MarkSeq = 0;
	}

	SIMPLEQ_FOREACH(f, &Files, f_next) {
		if (f->f_prevcount && now >= REPEATTIME(f)) {
			dprintf("flush %s: repeated %d times, %d sec.\n",
			    TypeNames[f->f_type], f->f_prevcount,
			    repeatinterval[f->f_repeatcount]);
			fprintlog(f, 0, (char *)NULL);
			BACKOFF(f);
		}
	}
}

int
unix_socket(char *path, int type, mode_t mode)
{
	struct sockaddr_un s_un;
	char ebuf[512];
	int fd, optval;
	mode_t old_umask;

	memset(&s_un, 0, sizeof(s_un));
	s_un.sun_family = AF_UNIX;
	if (strlcpy(s_un.sun_path, path, sizeof(s_un.sun_path)) >=
	    sizeof(s_un.sun_path)) {
		snprintf(ebuf, sizeof(ebuf), "socket path too long: %s", path);
		logerror(ebuf);
		die(0);
	}

	if ((fd = socket(AF_UNIX, type, 0)) == -1) {
		logerror("socket");
		return (-1);
	}

	if (Debug) {
		if (connect(fd, (struct sockaddr *)&s_un, sizeof(s_un)) == 0 ||
		    errno == EPROTOTYPE) {
			close(fd);
			errno = EISCONN;
			logerror("connect");
			return (-1);
		}
	}

	old_umask = umask(0177);

	unlink(path);
	if (bind(fd, (struct sockaddr *)&s_un, SUN_LEN(&s_un)) == -1) {
		snprintf(ebuf, sizeof(ebuf), "cannot bind %s", path);
		logerror(ebuf);
		umask(old_umask);
		close(fd);
		return (-1);
	}

	umask(old_umask);

	if (chmod(path, mode) == -1) {
		snprintf(ebuf, sizeof(ebuf), "cannot chmod %s", path);
		logerror(ebuf);
		close(fd);
		unlink(path);
		return (-1);
	}

	optval = MAXLINE + PATH_MAX;
	if (setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &optval, sizeof(optval))
	    == -1)
		logerror("cannot setsockopt unix");

	return (fd);
}

void
double_rbuf(int fd)
{
	socklen_t slen, len;

	slen = sizeof(len);
	if (getsockopt(fd, SOL_SOCKET, SO_RCVBUF, &len, &slen) == 0) {
		len *= 2;
		setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &len, slen);
	}
}

void
ctlconn_cleanup(void)
{
	struct filed *f;

	if (close(fd_ctlconn) == -1)
		logerror("close ctlconn");
	fd_ctlconn = -1;
	event_del(&ev_ctlread);
	event_del(&ev_ctlwrite);
	event_add(&ev_ctlaccept, NULL);

	if (ctl_state == CTL_WRITING_CONT_REPLY)
		SIMPLEQ_FOREACH(f, &Files, f_next)
			if (f->f_type == F_MEMBUF)
				f->f_un.f_mb.f_attached = 0;

	ctl_state = ctl_cmd_bytes = ctl_reply_offset = ctl_reply_size = 0;
}

void
ctlsock_acceptcb(int fd, short event, void *arg)
{
	struct event		*ev = arg;
	int			 flags;

	dprintf("Accepting control connection\n");
	fd = accept(fd, NULL, NULL);
	if (fd == -1) {
		if (errno != EINTR && errno != EWOULDBLOCK &&
		    errno != ECONNABORTED)
			logerror("accept ctlsock");
		return;
	}

	if (fd_ctlconn != -1)
		ctlconn_cleanup();

	/* Only one connection at a time */
	event_del(ev);

	if ((flags = fcntl(fd, F_GETFL)) == -1 ||
	    fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
		logerror("fcntl ctlconn O_NONBLOCK");
		close(fd);
		return;
	}

	fd_ctlconn = fd;
	/* file descriptor has changed, reset event */
	event_set(&ev_ctlread, fd_ctlconn, EV_READ|EV_PERSIST,
	    ctlconn_readcb, &ev_ctlread);
	event_set(&ev_ctlwrite, fd_ctlconn, EV_WRITE|EV_PERSIST,
	    ctlconn_writecb, &ev_ctlwrite);
	event_add(&ev_ctlread, NULL);
	ctl_state = CTL_READING_CMD;
	ctl_cmd_bytes = 0;
}

static struct filed
*find_membuf_log(const char *name)
{
	struct filed *f;

	SIMPLEQ_FOREACH(f, &Files, f_next) {
		if (f->f_type == F_MEMBUF &&
		    strcmp(f->f_un.f_mb.f_mname, name) == 0)
			break;
	}
	return (f);
}

void
ctlconn_readcb(int fd, short event, void *arg)
{
	struct filed		*f;
	struct ctl_reply_hdr	*reply_hdr = (struct ctl_reply_hdr *)ctl_reply;
	ssize_t			 n;
	u_int32_t		 flags = 0;

	if (ctl_state == CTL_WRITING_REPLY ||
	    ctl_state == CTL_WRITING_CONT_REPLY) {
		/* client has closed the connection */
		ctlconn_cleanup();
		return;
	}

 retry:
	n = read(fd, (char*)&ctl_cmd + ctl_cmd_bytes,
	    sizeof(ctl_cmd) - ctl_cmd_bytes);
	switch (n) {
	case -1:
		if (errno == EINTR)
			goto retry;
		logerror("ctlconn read");
		/* FALLTHROUGH */
	case 0:
		ctlconn_cleanup();
		return;
	default:
		ctl_cmd_bytes += n;
	}
	if (ctl_cmd_bytes < sizeof(ctl_cmd))
		return;

	if (ntohl(ctl_cmd.version) != CTL_VERSION) {
		logerror("Unknown client protocol version");
		ctlconn_cleanup();
		return;
	}

	/* Ensure that logname is \0 terminated */
	if (memchr(ctl_cmd.logname, '\0', sizeof(ctl_cmd.logname)) == NULL) {
		logerror("Corrupt ctlsock command");
		ctlconn_cleanup();
		return;
	}

	*reply_text = '\0';

	ctl_reply_size = ctl_reply_offset = 0;
	memset(reply_hdr, '\0', sizeof(*reply_hdr));

	ctl_cmd.cmd = ntohl(ctl_cmd.cmd);
	dprintf("ctlcmd %x logname \"%s\"\n", ctl_cmd.cmd, ctl_cmd.logname);

	switch (ctl_cmd.cmd) {
	case CMD_READ:
	case CMD_READ_CLEAR:
	case CMD_READ_CONT:
	case CMD_FLAGS:
		f = find_membuf_log(ctl_cmd.logname);
		if (f == NULL) {
			strlcpy(reply_text, "No such log\n", MAX_MEMBUF);
		} else {
			if (ctl_cmd.cmd != CMD_FLAGS) {
				ringbuf_to_string(reply_text, MAX_MEMBUF,
				    f->f_un.f_mb.f_rb);
			}
			if (f->f_un.f_mb.f_overflow)
				flags |= CTL_HDR_FLAG_OVERFLOW;
			if (ctl_cmd.cmd == CMD_READ_CLEAR) {
				ringbuf_clear(f->f_un.f_mb.f_rb);
				f->f_un.f_mb.f_overflow = 0;
			}
			if (ctl_cmd.cmd == CMD_READ_CONT) {
				f->f_un.f_mb.f_attached = 1;
				tailify_replytext(reply_text,
				    ctl_cmd.lines > 0 ? ctl_cmd.lines : 10);
			} else if (ctl_cmd.lines > 0) {
				tailify_replytext(reply_text, ctl_cmd.lines);
			}
		}
		break;
	case CMD_CLEAR:
		f = find_membuf_log(ctl_cmd.logname);
		if (f == NULL) {
			strlcpy(reply_text, "No such log\n", MAX_MEMBUF);
		} else {
			ringbuf_clear(f->f_un.f_mb.f_rb);
			if (f->f_un.f_mb.f_overflow)
				flags |= CTL_HDR_FLAG_OVERFLOW;
			f->f_un.f_mb.f_overflow = 0;
			strlcpy(reply_text, "Log cleared\n", MAX_MEMBUF);
		}
		break;
	case CMD_LIST:
		SIMPLEQ_FOREACH(f, &Files, f_next) {
			if (f->f_type == F_MEMBUF) {
				strlcat(reply_text, f->f_un.f_mb.f_mname,
				    MAX_MEMBUF);
				if (f->f_un.f_mb.f_overflow) {
					strlcat(reply_text, "*", MAX_MEMBUF);
					flags |= CTL_HDR_FLAG_OVERFLOW;
				}
				strlcat(reply_text, " ", MAX_MEMBUF);
			}
		}
		strlcat(reply_text, "\n", MAX_MEMBUF);
		break;
	default:
		logerror("Unsupported ctlsock command");
		ctlconn_cleanup();
		return;
	}
	reply_hdr->version = htonl(CTL_VERSION);
	reply_hdr->flags = htonl(flags);

	ctl_reply_size = CTL_REPLY_SIZE;
	dprintf("ctlcmd reply length %lu\n", (u_long)ctl_reply_size);

	/* Otherwise, set up to write out reply */
	ctl_state = (ctl_cmd.cmd == CMD_READ_CONT) ?
	    CTL_WRITING_CONT_REPLY : CTL_WRITING_REPLY;

	event_add(&ev_ctlwrite, NULL);

	/* another syslogc can kick us out */
	if (ctl_state == CTL_WRITING_CONT_REPLY)
		event_add(&ev_ctlaccept, NULL);
}

void
ctlconn_writecb(int fd, short event, void *arg)
{
	struct event		*ev = arg;
	ssize_t			 n;

	if (!(ctl_state == CTL_WRITING_REPLY ||
	    ctl_state == CTL_WRITING_CONT_REPLY)) {
		/* Shouldn't be here! */
		logerror("ctlconn_write with bad ctl_state");
		ctlconn_cleanup();
		return;
	}

 retry:
	n = write(fd, ctl_reply + ctl_reply_offset,
	    ctl_reply_size - ctl_reply_offset);
	switch (n) {
	case -1:
		if (errno == EINTR)
			goto retry;
		if (errno != EPIPE)
			logerror("ctlconn write");
		/* FALLTHROUGH */
	case 0:
		ctlconn_cleanup();
		return;
	default:
		ctl_reply_offset += n;
	}
	if (ctl_reply_offset < ctl_reply_size)
		return;

	if (ctl_state != CTL_WRITING_CONT_REPLY) {
		ctlconn_cleanup();
		return;
	}

	/*
	 * Make space in the buffer for continous writes.
	 * Set offset behind reply header to skip it
	 */
	*reply_text = '\0';
	ctl_reply_offset = ctl_reply_size = CTL_REPLY_SIZE;

	/* Now is a good time to report dropped lines */
	if (membuf_drop) {
		strlcat(reply_text, "<ENOBUFS>\n", MAX_MEMBUF);
		ctl_reply_size = CTL_REPLY_SIZE;
		membuf_drop = 0;
	} else {
		/* Nothing left to write */
		event_del(ev);
	}
}

/* Shorten replytext to number of lines */
void
tailify_replytext(char *replytext, int lines)
{
	char *start, *nl;
	int count = 0;
	start = nl = replytext;

	while ((nl = strchr(nl, '\n')) != NULL) {
		nl++;
		if (++count > lines) {
			start = strchr(start, '\n');
			start++;
		}
	}
	if (start != replytext) {
		int len = strlen(start);
		memmove(replytext, start, len);
		*(replytext + len) = '\0';
	}
}

void
ctlconn_logto(char *line)
{
	size_t l;

	if (membuf_drop)
		return;

	l = strlen(line);
	if (l + 2 > (CTL_REPLY_MAXSIZE - ctl_reply_size)) {
		/* remember line drops for later report */
		membuf_drop = 1;
		return;
	}
	memcpy(ctl_reply + ctl_reply_size, line, l);
	memcpy(ctl_reply + ctl_reply_size + l, "\n", 2);
	ctl_reply_size += l + 1;
	event_add(&ev_ctlwrite, NULL);
}
