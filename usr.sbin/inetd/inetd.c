/*	$OpenBSD: inetd.c,v 1.124 2007/09/11 16:30:59 gilles Exp $	*/

/*
 * Copyright (c) 1983,1991 The Regents of the University of California.
 * All rights reserved.
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

#ifndef lint
char copyright[] =
"@(#) Copyright (c) 1983 Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
/*static const char sccsid[] = "from: @(#)inetd.c	5.30 (Berkeley) 6/3/91";*/
static const char rcsid[] = "$OpenBSD: inetd.c,v 1.124 2007/09/11 16:30:59 gilles Exp $";
#endif /* not lint */

/*
 * Inetd - Internet super-server
 *
 * This program invokes all internet services as needed.
 * connection-oriented services are invoked each time a
 * connection is made, by creating a process.  This process
 * is passed the connection as file descriptor 0 and is
 * expected to do a getpeername to find out the source host
 * and port.
 *
 * Datagram oriented services are invoked when a datagram
 * arrives; a process is created and passed a pending message
 * on file descriptor 0.  Datagram servers may either connect
 * to their peer, freeing up the original socket for inetd
 * to receive further messages on, or ``take over the socket'',
 * processing all arriving datagrams and, eventually, timing
 * out.	 The first type of server is said to be ``multi-threaded'';
 * the second type of server ``single-threaded''.
 *
 * Inetd uses a configuration file which is read at startup
 * and, possibly, at some later time in response to a hangup signal.
 * The configuration file is ``free format'' with fields given in the
 * order shown below.  Continuation lines for an entry must begin with
 * a space or tab.  All fields must be present in each entry.
 *
 *	service name			must be in /etc/services
 *	socket type			stream/dgram/raw/rdm/seqpacket
 *	protocol			must be in /etc/protocols
 *	wait/nowait[.max]		single-threaded/multi-threaded, max #
 *	user[.group] or user[:group]	user/group to run daemon as
 *	server program			full path name
 *	server program arguments	maximum of MAXARGS (20)
 *
 * For RPC services
 *      service name/version            must be in /etc/rpc
 *	socket type			stream/dgram/raw/rdm/seqpacket
 *	protocol			must be in /etc/protocols
 *	wait/nowait[.max]		single-threaded/multi-threaded
 *	user[.group] or user[:group]	user to run daemon as
 *	server program			full path name
 *	server program arguments	maximum of MAXARGS (20)
 *
 * For non-RPC services, the "service name" can be of the form
 * hostaddress:servicename, in which case the hostaddress is used
 * as the host portion of the address to listen on.  If hostaddress
 * consists of a single `*' character, INADDR_ANY is used.
 *
 * A line can also consist of just
 *      hostaddress:
 * where hostaddress is as in the preceding paragraph.  Such a line must
 * have no further fields; the specified hostaddress is remembered and
 * used for all further lines that have no hostaddress specified,
 * until the next such line (or EOF).  (This is why * is provided to
 * allow explicit specification of INADDR_ANY.)  A line
 *      *:
 * is implicitly in effect at the beginning of the file.
 *
 * The hostaddress specifier may (and often will) contain dots;
 * the service name must not.
 *
 * For RPC services, host-address specifiers are accepted and will
 * work to some extent; however, because of limitations in the
 * portmapper interface, it will not work to try to give more than
 * one line for any given RPC service, even if the host-address
 * specifiers are different.
 *
 * Comment lines are indicated by a `#' in column 1.
 */

/*
 * Here's the scoop concerning the user[.:]group feature:
 *
 * 1) set-group-option off.
 *
 *	a) user = root:	NO setuid() or setgid() is done
 *
 *	b) other:	setgid(primary group as found in passwd)
 *			initgroups(name, primary group)
 *			setuid()
 *
 * 2) set-group-option on.
 *
 *	a) user = root:	setgid(specified group)
 *			NO initgroups()
 *			NO setuid()
 *
 *	b) other:	setgid(specified group)
 *			initgroups(name, specified group)
 *			setuid()
 *
 */

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/file.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/resource.h>

#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <errno.h>
#include <ctype.h>
#include <signal.h>
#include <netdb.h>
#include <syslog.h>
#include <pwd.h>
#include <grp.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <login_cap.h>
#include <ifaddrs.h>
#include <rpc/rpc.h>
#include <rpc/pmap_clnt.h>
#include <rpcsvc/nfs_prot.h>
#include "pathnames.h"

#define	TOOMANY		256		/* don't start more than TOOMANY */
#define	CNT_INTVL	60		/* servers in CNT_INTVL sec. */
#define	RETRYTIME	(60*10)		/* retry after bind or server fail */

int	 debug = 0;
int	 nsock, maxsock;
fd_set	*allsockp;
int	 allsockn;
int	 toomany = TOOMANY;
int	 options;
int	 timingout;
struct	 servent *sp;
uid_t	 uid;
sigset_t blockmask;
sigset_t emptymask;

#ifndef OPEN_MAX
#define OPEN_MAX	64
#endif

/* Reserve some descriptors, 3 stdio + at least: 1 log, 1 conf. file */
#define FD_MARGIN	(8)
rlim_t	rlim_nofile_cur = OPEN_MAX;

struct rlimit	rlim_nofile;

struct	servtab {
	char	*se_hostaddr;		/* host address to listen on */
	char	*se_service;		/* name of service */
	int	se_socktype;		/* type of socket to use */
	int	se_family;		/* address family */
	char	*se_proto;		/* protocol used */
	int	se_rpcprog;		/* rpc program number */
	int	se_rpcversl;		/* rpc program lowest version */
	int	se_rpcversh;		/* rpc program highest version */
#define isrpcservice(sep)	((sep)->se_rpcversl != 0)
	pid_t	se_wait;		/* single threaded server */
	short	se_checked;		/* looked at during merge */
	char	*se_user;		/* user name to run as */
	char	*se_group;		/* group name to run as */
	struct	biltin *se_bi;		/* if built-in, description */
	char	*se_server;		/* server program */
#define	MAXARGV 20
	char	*se_argv[MAXARGV+1];	/* program arguments */
	int	se_fd;			/* open descriptor */
	union {
		struct	sockaddr se_un_ctrladdr;
		struct	sockaddr_in se_un_ctrladdr_in;
		struct	sockaddr_in6 se_un_ctrladdr_in6;
		struct	sockaddr_un se_un_ctrladdr_un;
		struct	sockaddr_storage se_un_ctrladdr_storage;
	} se_un;			/* bound address */
#define se_ctrladdr	se_un.se_un_ctrladdr
#define se_ctrladdr_in	se_un.se_un_ctrladdr_in
#define se_ctrladdr_in6	se_un.se_un_ctrladdr_in6
#define se_ctrladdr_un	se_un.se_un_ctrladdr_un
#define se_ctrladdr_storage	se_un.se_un_ctrladdr_storage
	int	se_ctrladdr_size;
	int	se_max;			/* max # of instances of this service */
	int	se_count;		/* number started since se_time */
	struct	timeval se_time;	/* start of se_count */
	struct	servtab *se_next;
} *servtab;

void echo_stream(int, struct servtab *);
void discard_stream(int, struct servtab *);
void machtime_stream(int, struct servtab *);
void daytime_stream(int, struct servtab *);
void chargen_stream(int, struct servtab *);
void echo_dg(int, struct servtab *);
void discard_dg(int, struct servtab *);
void machtime_dg(int, struct servtab *);
void daytime_dg(int, struct servtab *);
void chargen_dg(int, struct servtab *);

struct biltin {
	char	*bi_service;		/* internally provided service name */
	int	bi_socktype;		/* type of socket supported */
	short	bi_fork;		/* 1 if should fork before call */
	short	bi_wait;		/* 1 if should wait for child */
	void	(*bi_fn)(int, struct servtab *);
} biltins[] = {
	/* Echo received data */
	{ "echo",	SOCK_STREAM,	1, 0,	echo_stream },
	{ "echo",	SOCK_DGRAM,	0, 0,	echo_dg },

	/* Internet /dev/null */
	{ "discard",	SOCK_STREAM,	1, 0,	discard_stream },
	{ "discard",	SOCK_DGRAM,	0, 0,	discard_dg },

	/* Return 32 bit time since 1900 */
	{ "time",	SOCK_STREAM,	0, 0,	machtime_stream },
	{ "time",	SOCK_DGRAM,	0, 0,	machtime_dg },

	/* Return human-readable time */
	{ "daytime",	SOCK_STREAM,	0, 0,	daytime_stream },
	{ "daytime",	SOCK_DGRAM,	0, 0,	daytime_dg },

	/* Familiar character generator */
	{ "chargen",	SOCK_STREAM,	1, 0,	chargen_stream },
	{ "chargen",	SOCK_DGRAM,	0, 0,	chargen_dg },

	{ 0 }
};

volatile sig_atomic_t wantretry;
volatile sig_atomic_t wantconfig;
volatile sig_atomic_t wantreap;
volatile sig_atomic_t wantdie;

void	config(int);
void	doconfig(void);
void	reap(int);
void	doreap(void);
void	retry(int);
void	doretry(void);
void	die(int);
void	dodie(void);
void	logpid(void);
void	spawn(struct servtab *, int);
int	gettcp(struct servtab *);
int	setconfig(void);
void	endconfig(void);
void	register_rpc(struct servtab *);
void	unregister_rpc(struct servtab *);
void	freeconfig(struct servtab *);
void	print_service(char *, struct servtab *);
void	setup(struct servtab *);
struct servtab *getconfigent(void);
int	bump_nofile(void);
struct servtab *enter(struct servtab *);
int	matchconf(struct servtab *, struct servtab *);
int	dg_broadcast(struct in_addr *in);

#define NUMINT	(sizeof(intab) / sizeof(struct inent))
char	*CONFIG = _PATH_INETDCONF;
char	*progname;

void
fd_grow(fd_set **fdsp, int *bytes, int fd)
{
	caddr_t new;
	int newbytes;

	newbytes = howmany(fd+1, NFDBITS) * sizeof(fd_mask);
	if (newbytes > *bytes) {
		newbytes *= 2;			/* optimism */
		new = realloc(*fdsp, newbytes);
		if (new == NULL) {
			syslog(LOG_ERR, "Out of memory.");
			exit(1);
		}
		memset(new + *bytes, 0, newbytes - *bytes);
		*fdsp = (fd_set *)new;
		*bytes = newbytes;
	}
}

struct sigaction sa, sapipe;

int
main(int argc, char *argv[])
{
	fd_set *fdsrp = NULL;
	int readablen = 0, ch;
	struct servtab *sep;
	extern char *optarg;
	extern int optind;

	progname = strrchr(argv[0], '/');
	progname = progname ? progname + 1 : argv[0];

	while ((ch = getopt(argc, argv, "dR:")) != -1)
		switch (ch) {
		case 'd':
			debug = 1;
			options |= SO_DEBUG;
			break;
		case 'R': {	/* invocation rate */
			char *p;
			int val;

			val = strtoul(optarg, &p, 0);
			if (val >= 1 && *p == '\0') {
				toomany = val;
				break;
			}
			syslog(LOG_ERR,
			    "-R %s: bad value for service invocation rate",
			    optarg);
			break;
		}
		case '?':
		default:
			fprintf(stderr, "usage: %s [-R rate] [-d] [conf]\n",
			    progname);
			exit(1);
		}
	argc -= optind;
	argv += optind;

	uid = getuid();
	if (uid != 0)
		CONFIG = NULL;
	if (argc > 0)
		CONFIG = argv[0];
	if (CONFIG == NULL) {
		fprintf(stderr, "%s: non-root must specify a config file\n",
		    progname);
		exit(1);
	}

	umask(022);
	if (debug == 0) {
		daemon(0, 0);
		if (uid == 0)
			(void) setlogin("");
	}

	if (uid == 0) {
		gid_t gid = getgid();

		/* If run by hand, ensure groups vector gets trashed */
		setgroups(1, &gid);
	}

	openlog(progname, LOG_PID | LOG_NOWAIT, LOG_DAEMON);
	logpid();

	if (getrlimit(RLIMIT_NOFILE, &rlim_nofile) < 0) {
		syslog(LOG_ERR, "getrlimit: %m");
	} else {
		rlim_nofile_cur = rlim_nofile.rlim_cur;
		if (rlim_nofile_cur == RLIM_INFINITY)	/* ! */
			rlim_nofile_cur = OPEN_MAX;
	}

	sigemptyset(&emptymask);
	sigemptyset(&blockmask);
	sigaddset(&blockmask, SIGCHLD);
	sigaddset(&blockmask, SIGHUP);
	sigaddset(&blockmask, SIGALRM);

	memset(&sa, 0, sizeof(sa));
	sigemptyset(&sa.sa_mask);
	sigaddset(&sa.sa_mask, SIGALRM);
	sigaddset(&sa.sa_mask, SIGCHLD);
	sigaddset(&sa.sa_mask, SIGHUP);
	sa.sa_handler = retry;
	sigaction(SIGALRM, &sa, NULL);
	doconfig();
	sa.sa_handler = config;
	sigaction(SIGHUP, &sa, NULL);
	sa.sa_handler = reap;
	sigaction(SIGCHLD, &sa, NULL);
	sa.sa_handler = die;
	sigaction(SIGTERM, &sa, NULL);
	sa.sa_handler = die;
	sigaction(SIGINT, &sa, NULL);
	sa.sa_handler = SIG_IGN;
	sigaction(SIGPIPE, &sa, &sapipe);

	for (;;) {
		int n, ctrl = -1;

	    restart:
		if (nsock == 0) {
			(void) sigprocmask(SIG_BLOCK, &blockmask, NULL);
			while (nsock == 0) {
				if (wantretry || wantconfig || wantreap || wantdie)
					break;
				sigsuspend(&emptymask);
			}
			(void) sigprocmask(SIG_SETMASK, &emptymask, NULL);
		}

		while (wantretry || wantconfig || wantreap || wantdie) {
			if (wantretry) {
				wantretry = 0;
				doretry();
			}
			if (wantconfig) {
				wantconfig = 0;
				doconfig();
			}
			if (wantreap) {
				wantreap = 0;
				doreap();
			}
			if (wantdie)
				dodie();
			goto restart;
		}

		if (readablen != allsockn) {
			if (fdsrp)
				free(fdsrp);
			fdsrp = (fd_set *)calloc(allsockn, 1);
			if (fdsrp == NULL) {
				syslog(LOG_ERR, "Out of memory.");
				exit(1);
			}
			readablen = allsockn;
		}
		bcopy(allsockp, fdsrp, allsockn);

		if ((n = select(maxsock + 1, fdsrp, NULL, NULL, NULL)) <= 0) {
			if (n < 0 && errno != EINTR) {
				syslog(LOG_WARNING, "select: %m");
				sleep(1);
			}
			continue;
		}

		for (sep = servtab; n && sep; sep = sep->se_next) {
			if (sep->se_fd != -1 &&
			    FD_ISSET(sep->se_fd, fdsrp)) {
				n--;
				if (debug)
					fprintf(stderr, "someone wants %s\n",
					    sep->se_service);
				if (!sep->se_wait &&
				    sep->se_socktype == SOCK_STREAM) {
					ctrl = gettcp(sep);
					if (ctrl == -1)
						continue;
				} else
					ctrl = sep->se_fd;
				(void) sigprocmask(SIG_BLOCK, &blockmask, NULL);
				spawn(sep, ctrl);	/* spawn will unblock */
			}
		}
	}
}

int
gettcp(struct servtab *sep)
{
	int ctrl;

	ctrl = accept(sep->se_fd, NULL, NULL);
	if (debug)
		fprintf(stderr, "accept, ctrl %d\n", ctrl);
	if (ctrl < 0) {
		if (errno == EINTR)
			return -1;
		syslog(LOG_WARNING, "accept (for %s): %m", sep->se_service);
		return -1;
	}
	if ((sep->se_family == AF_INET || sep->se_family == AF_INET6) &&
	    sep->se_socktype == SOCK_STREAM) {
		struct sockaddr_storage peer;
		socklen_t plen = sizeof(peer);
		char sbuf[NI_MAXSERV];

		if (getpeername(ctrl, (struct sockaddr *)&peer, &plen) < 0) {
			syslog(LOG_WARNING, "could not getpeername");
			close(ctrl);
			return -1;
		}
		if (getnameinfo((struct sockaddr *)&peer, plen, NULL, 0,
		    sbuf, sizeof(sbuf), NI_NUMERICSERV) == 0 &&
		    atoi(sbuf) == 20) {
			/*
			 * ignore things that look like ftp bounce
			 */
			close(ctrl);
			return -1;
		}
	}
	return (ctrl);
}


int
dg_badinput(struct sockaddr *sa)
{
	struct in_addr in;
	struct in6_addr *in6;
	u_int16_t port;

	switch (sa->sa_family) {
	case AF_INET:
		in.s_addr = ntohl(((struct sockaddr_in *)sa)->sin_addr.s_addr);
		port = ntohs(((struct sockaddr_in *)sa)->sin_port);
	v4chk:
		if (IN_MULTICAST(in.s_addr))
			goto bad;
		switch ((in.s_addr & 0xff000000) >> 24) {
		case 0: case 127: case 255:
			goto bad;
		}
		if (dg_broadcast(&in))
			goto bad;
		break;
	case AF_INET6:
		in6 = &((struct sockaddr_in6 *)sa)->sin6_addr;
		port = ntohs(((struct sockaddr_in6 *)sa)->sin6_port);
		if (IN6_IS_ADDR_MULTICAST(in6) || IN6_IS_ADDR_UNSPECIFIED(in6))
			goto bad;
		/*
		 * OpenBSD does not support IPv4 mapped adderss (RFC2553
		 * inbound behavior) at all.  We should drop it.
		 */
		if (IN6_IS_ADDR_V4MAPPED(in6))
			goto bad;
		if (IN6_IS_ADDR_V4COMPAT(in6)) {
			memcpy(&in, &in6->s6_addr[12], sizeof(in));
			in.s_addr = ntohl(in.s_addr);
			goto v4chk;
		}
		break;
	default:
		/* XXX unsupported af, is it safe to assume it to be safe? */
		return 0;
	}

	if (port < IPPORT_RESERVED || port == NFS_PORT)
		goto bad;

	return (0);

bad:
	return (1);
}

int
dg_broadcast(struct in_addr *in)
{
	struct ifaddrs *ifa, *ifap;
	struct sockaddr_in *sin;

	if (getifaddrs(&ifap) < 0)
		return (0);
	for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
		if (ifa->ifa_addr->sa_family != AF_INET ||
		    (ifa->ifa_flags & IFF_BROADCAST) == 0)
			continue;
		sin = (struct sockaddr_in *)ifa->ifa_broadaddr;
		if (sin->sin_addr.s_addr == in->s_addr) {
			freeifaddrs(ifap);
			return (1);
		}
	}
	freeifaddrs(ifap);
	return (0);
}

/* ARGSUSED */
void
reap(int sig)
{
	wantreap = 1;
}

void
doreap(void)
{
	struct servtab *sep;
	int status;
	pid_t pid;

	if (debug)
		fprintf(stderr, "reaping asked for\n");

	for (;;) {
		if ((pid = wait3(&status, WNOHANG, NULL)) <= 0) {
			if (pid == -1 && errno == EINTR)
				continue;
			break;
		}
		if (debug)
			fprintf(stderr, "%ld reaped, status %x\n",
			    (long)pid, status);
		for (sep = servtab; sep; sep = sep->se_next)
			if (sep->se_wait == pid) {
				if (WIFEXITED(status) && WEXITSTATUS(status))
					syslog(LOG_WARNING,
					    "%s: exit status %d",
					    sep->se_server, WEXITSTATUS(status));
				else if (WIFSIGNALED(status))
					syslog(LOG_WARNING,
					    "%s: exit signal %d",
					    sep->se_server, WTERMSIG(status));
				sep->se_wait = 1;
				fd_grow(&allsockp, &allsockn, sep->se_fd);
				FD_SET(sep->se_fd, allsockp);
				nsock++;
				if (debug)
					fprintf(stderr, "restored %s, fd %d\n",
					    sep->se_service, sep->se_fd);
			}
	}
}

/* ARGSUSED */
void
config(int sig)
{
	wantconfig = 1;
}

void
doconfig(void)
{
	struct servtab *sep, *cp, **sepp;
	int add;
	char protoname[10];
	sigset_t omask;

	if (!setconfig()) {
		syslog(LOG_ERR, "%s: %m", CONFIG);
		exit(1);
	}
	for (sep = servtab; sep; sep = sep->se_next)
		sep->se_checked = 0;
	cp = getconfigent();
	while (cp != NULL) {
		for (sep = servtab; sep; sep = sep->se_next)
			if (matchconf(sep, cp))
				break;
		add = 0;
		if (sep != NULL) {
			int i;

#define SWAP(type, a, b) {type c=(type)a; a=(type)b; b=(type)c;}

			sigprocmask(SIG_BLOCK, &blockmask, &omask);
			/*
			 * sep->se_wait may be holding the pid of a daemon
			 * that we're waiting for.  If so, don't overwrite
			 * it unless the config file explicitly says don't
			 * wait.
			 */
			if (cp->se_bi == 0 &&
			    (sep->se_wait == 1 || cp->se_wait == 0))
				sep->se_wait = cp->se_wait;
			SWAP(int, cp->se_max, sep->se_max);
			SWAP(char *, sep->se_user, cp->se_user);
			SWAP(char *, sep->se_group, cp->se_group);
			SWAP(char *, sep->se_server, cp->se_server);
			for (i = 0; i < MAXARGV; i++)
				SWAP(char *, sep->se_argv[i], cp->se_argv[i]);
#undef SWAP
			if (isrpcservice(sep))
				unregister_rpc(sep);
			sep->se_rpcversl = cp->se_rpcversl;
			sep->se_rpcversh = cp->se_rpcversh;
			sigprocmask(SIG_SETMASK, &omask, NULL);
			freeconfig(cp);
			add = 1;
		} else {
			sep = enter(cp);
		}
		sep->se_checked = 1;

		switch (sep->se_family) {
		case AF_UNIX:
			if (sep->se_fd != -1)
				break;
			sep->se_ctrladdr_size =
			    strlcpy(sep->se_ctrladdr_un.sun_path,
			    sep->se_service,
			    sizeof sep->se_ctrladdr_un.sun_path);
			if (sep->se_ctrladdr_size >=
			    sizeof sep->se_ctrladdr_un.sun_path) {
				syslog(LOG_WARNING, "%s/%s: UNIX domain socket "
				    "path too long", sep->se_service,
				    sep->se_proto);
				goto serv_unknown;
			}
			sep->se_ctrladdr_un.sun_family = AF_UNIX;
			sep->se_ctrladdr_size +=
			    1 + sizeof sep->se_ctrladdr_un.sun_family;
			(void)unlink(sep->se_service);
			setup(sep);
			break;
		case AF_INET:
			sep->se_ctrladdr_in.sin_family = AF_INET;
			/* se_ctrladdr_in was set in getconfigent */
			sep->se_ctrladdr_size = sizeof sep->se_ctrladdr_in;

			if (isrpcservice(sep)) {
				struct rpcent *rp;

				sep->se_rpcprog = atoi(sep->se_service);
				if (sep->se_rpcprog == 0) {
					rp = getrpcbyname(sep->se_service);
					if (rp == 0) {
						syslog(LOG_ERR,
						    "%s: unknown rpc service",
						    sep->se_service);
						goto serv_unknown;
					}
					sep->se_rpcprog = rp->r_number;
				}
				if (sep->se_fd == -1)
					setup(sep);
				if (sep->se_fd != -1)
					register_rpc(sep);
			} else {
				u_short port = htons(atoi(sep->se_service));

				if (!port) {
					/* XXX */
					strncpy(protoname, sep->se_proto,
						sizeof(protoname));
					if (isdigit(protoname[strlen(protoname) - 1]))
						protoname[strlen(protoname) - 1] = '\0';
					sp = getservbyname(sep->se_service,
					    protoname);
					if (sp == 0) {
						syslog(LOG_ERR,
						    "%s/%s: unknown service",
						    sep->se_service, sep->se_proto);
						goto serv_unknown;
					}
					port = sp->s_port;
				}
				if (port != sep->se_ctrladdr_in.sin_port) {
					sep->se_ctrladdr_in.sin_port = port;
					if (sep->se_fd != -1) {
						FD_CLR(sep->se_fd, allsockp);
						nsock--;
						(void) close(sep->se_fd);
					}
					sep->se_fd = -1;
				}
				if (sep->se_fd == -1)
					setup(sep);
			}
			break;
		case AF_INET6:
			sep->se_ctrladdr_in6.sin6_family = AF_INET6;
			/* se_ctrladdr_in was set in getconfigent */
			sep->se_ctrladdr_size = sizeof sep->se_ctrladdr_in6;

			if (isrpcservice(sep)) {
				struct rpcent *rp;

				sep->se_rpcprog = atoi(sep->se_service);
				if (sep->se_rpcprog == 0) {
					rp = getrpcbyname(sep->se_service);
					if (rp == 0) {
						syslog(LOG_ERR,
						    "%s: unknown rpc service",
						    sep->se_service);
						goto serv_unknown;
					}
					sep->se_rpcprog = rp->r_number;
				}
				if (sep->se_fd == -1)
					setup(sep);
				if (sep->se_fd != -1)
					register_rpc(sep);
			} else {
				u_short port = htons(atoi(sep->se_service));

				if (!port) {
					/* XXX */
					strncpy(protoname, sep->se_proto,
						sizeof(protoname));
					if (isdigit(protoname[strlen(protoname) - 1]))
						protoname[strlen(protoname) - 1] = '\0';
					sp = getservbyname(sep->se_service,
					    protoname);
					if (sp == 0) {
						syslog(LOG_ERR,
						    "%s/%s: unknown service",
						    sep->se_service, sep->se_proto);
						goto serv_unknown;
					}
					port = sp->s_port;
				}
				if (port != sep->se_ctrladdr_in6.sin6_port) {
					sep->se_ctrladdr_in6.sin6_port = port;
					if (sep->se_fd != -1) {
						FD_CLR(sep->se_fd, allsockp);
						nsock--;
						(void) close(sep->se_fd);
					}
					sep->se_fd = -1;
				}
				if (sep->se_fd == -1)
					setup(sep);
			}
			break;
		}
	serv_unknown:
		if (cp->se_next != NULL) {
			struct servtab *tmp = cp;

			cp = cp->se_next;
			free(tmp);
		} else {
			free(cp);
			cp = getconfigent();
		}
		if (debug)
			print_service(add ? "REDO" : "ADD", sep);
	}
	endconfig();
	/*
	 * Purge anything not looked at above.
	 */
	sigprocmask(SIG_BLOCK, &blockmask, &omask);
	sepp = &servtab;
	while ((sep = *sepp)) {
		if (sep->se_checked) {
			sepp = &sep->se_next;
			continue;
		}
		*sepp = sep->se_next;
		if (sep->se_fd != -1) {
			FD_CLR(sep->se_fd, allsockp);
			nsock--;
			(void) close(sep->se_fd);
		}
		if (isrpcservice(sep))
			unregister_rpc(sep);
		if (sep->se_family == AF_UNIX)
			(void)unlink(sep->se_service);
		if (debug)
			print_service("FREE", sep);
		freeconfig(sep);
		free(sep);
	}
	sigprocmask(SIG_SETMASK, &omask, NULL);
}

/* ARGSUSED */
void
retry(int sig)
{
	wantretry = 1;
}

void
doretry(void)
{
	struct servtab *sep;

	timingout = 0;
	for (sep = servtab; sep; sep = sep->se_next) {
		if (sep->se_fd == -1) {
			switch (sep->se_family) {
			case AF_UNIX:
			case AF_INET:
			case AF_INET6:
				setup(sep);
				if (sep->se_fd != -1 && isrpcservice(sep))
					register_rpc(sep);
				break;
			}
		}
	}
}

/* ARGSUSED */
void
die(int sig)
{
	wantdie = 1;
}

void
dodie(void)
{
	struct servtab *sep;

	for (sep = servtab; sep; sep = sep->se_next) {
		if (sep->se_fd == -1)
			continue;

		switch (sep->se_family) {
		case AF_UNIX:
			(void)unlink(sep->se_service);
			break;
		case AF_INET:
		case AF_INET6:
			if (sep->se_wait == 1 && isrpcservice(sep))
				unregister_rpc(sep);
			break;
		}
		(void)close(sep->se_fd);
	}
	(void)unlink(_PATH_INETDPID);
	exit(0);
}

void
setup(struct servtab *sep)
{
	int on = 1;
	int r;
	mode_t mask = 0;

	if ((sep->se_fd = socket(sep->se_family, sep->se_socktype, 0)) < 0) {
		syslog(LOG_ERR, "%s/%s: socket: %m",
		    sep->se_service, sep->se_proto);
		return;
	}
#define	turnon(fd, opt) \
setsockopt(fd, SOL_SOCKET, opt, &on, sizeof (on))
	if (strncmp(sep->se_proto, "tcp", 3) == 0 && (options & SO_DEBUG) &&
	    turnon(sep->se_fd, SO_DEBUG) < 0)
		syslog(LOG_ERR, "setsockopt (SO_DEBUG): %m");
	if (turnon(sep->se_fd, SO_REUSEADDR) < 0)
		syslog(LOG_ERR, "setsockopt (SO_REUSEADDR): %m");
#undef turnon
	if (isrpcservice(sep)) {
		struct passwd *pwd;

		/*
		 * for RPC services, attempt to use a reserved port
		 * if they are going to be running as root.
		 *
		 * Also, zero out the port for all RPC services; let bind()
		 * find one.
		 */
		sep->se_ctrladdr_in.sin_port = 0;
		if (sep->se_user && (pwd = getpwnam(sep->se_user)) &&
		    pwd->pw_uid == 0 && uid == 0)
			r = bindresvport(sep->se_fd, &sep->se_ctrladdr_in);
		else {
			r = bind(sep->se_fd, &sep->se_ctrladdr,
			    sep->se_ctrladdr_size);
			if (r == 0) {
				socklen_t len = sep->se_ctrladdr_size;
				int saveerrno = errno;

				/* update se_ctrladdr_in.sin_port */
				r = getsockname(sep->se_fd, &sep->se_ctrladdr,
				    &len);
				if (r <= 0)
					errno = saveerrno;
			}
		}
	} else {
		if (sep->se_family == AF_UNIX)
			mask = umask(0111);
		r = bind(sep->se_fd, &sep->se_ctrladdr, sep->se_ctrladdr_size);
		if (sep->se_family == AF_UNIX)
			umask(mask);
	}
	if (r < 0) {
		syslog(LOG_ERR, "%s/%s: bind: %m",
		    sep->se_service, sep->se_proto);
		(void) close(sep->se_fd);
		sep->se_fd = -1;
		if (!timingout) {
			timingout = 1;
			alarm(RETRYTIME);
		}
		return;
	}
	if (sep->se_socktype == SOCK_STREAM)
		listen(sep->se_fd, 10);

	fd_grow(&allsockp, &allsockn, sep->se_fd);
	FD_SET(sep->se_fd, allsockp);
	nsock++;
	if (sep->se_fd > maxsock) {
		maxsock = sep->se_fd;
		if (maxsock > rlim_nofile_cur - FD_MARGIN)
			bump_nofile();
	}
}

void
register_rpc(struct servtab *sep)
{
	socklen_t n;
	struct sockaddr_in sin;
	struct protoent *pp;

	if ((pp = getprotobyname(sep->se_proto+4)) == NULL) {
		syslog(LOG_ERR, "%s: getproto: %m",
		    sep->se_proto);
		return;
	}
	n = sizeof sin;
	if (getsockname(sep->se_fd, (struct sockaddr *)&sin, &n) < 0) {
		syslog(LOG_ERR, "%s/%s: getsockname: %m",
		    sep->se_service, sep->se_proto);
		return;
	}

	for (n = sep->se_rpcversl; n <= sep->se_rpcversh; n++) {
		if (debug)
			fprintf(stderr, "pmap_set: %u %u %u %u\n",
			    sep->se_rpcprog, n, pp->p_proto,
			    ntohs(sin.sin_port));
		(void)pmap_unset(sep->se_rpcprog, n);
		if (!pmap_set(sep->se_rpcprog, n, pp->p_proto, ntohs(sin.sin_port)))
			syslog(LOG_ERR, "%s %s: pmap_set: %u %u %u %u: %m",
			    sep->se_service, sep->se_proto,
			    sep->se_rpcprog, n, pp->p_proto,
			    ntohs(sin.sin_port));
	}
}

void
unregister_rpc(struct servtab *sep)
{
	int n;

	for (n = sep->se_rpcversl; n <= sep->se_rpcversh; n++) {
		if (debug)
			fprintf(stderr, "pmap_unset(%u, %u)\n",
			    sep->se_rpcprog, n);
		if (!pmap_unset(sep->se_rpcprog, n))
			syslog(LOG_ERR, "pmap_unset(%u, %u)",
			    sep->se_rpcprog, n);
	}
}


struct servtab *
enter(struct servtab *cp)
{
	struct servtab *sep;
	sigset_t omask;

	sep = (struct servtab *)malloc(sizeof (*sep));
	if (sep == NULL) {
		syslog(LOG_ERR, "Out of memory.");
		exit(1);
	}
	*sep = *cp;
	sep->se_fd = -1;
	sep->se_rpcprog = -1;
	sigprocmask(SIG_BLOCK, &blockmask, &omask);
	sep->se_next = servtab;
	servtab = sep;
	sigprocmask(SIG_SETMASK, &omask, NULL);
	return (sep);
}

int
matchconf(struct servtab *old, struct servtab *new)
{
	if (strcmp(old->se_service, new->se_service) != 0)
		return (0);

	if (strcmp(old->se_hostaddr, new->se_hostaddr) != 0)
		return (0);

	if (strcmp(old->se_proto, new->se_proto) != 0)
		return (0);

	/*
	 * If the new servtab is bound to a specific address, check that the
	 * old servtab is bound to the same entry. If the new service is not
	 * bound to a specific address then the check of se_hostaddr above
	 * is sufficient.
	 */

	if (old->se_family == AF_INET && new->se_family == AF_INET &&
	    bcmp(&old->se_ctrladdr_in.sin_addr,
	    &new->se_ctrladdr_in.sin_addr,
	    sizeof(new->se_ctrladdr_in.sin_addr)) != 0)
		return (0);

	if (old->se_family == AF_INET6 && new->se_family == AF_INET6 &&
	    bcmp(&old->se_ctrladdr_in6.sin6_addr,
	    &new->se_ctrladdr_in6.sin6_addr,
	    sizeof(new->se_ctrladdr_in6.sin6_addr)) != 0)
		return (0);
	if (old->se_family == AF_INET6 && new->se_family == AF_INET6 &&
	    old->se_ctrladdr_in6.sin6_scope_id !=
	    new->se_ctrladdr_in6.sin6_scope_id)
		return (0);

	return (1);
}

FILE		*fconfig = NULL;
char		line[1024];
char		*defhost;
char		*skip(char **, int);
char		*nextline(FILE *);
char		*newstr(char *);
struct servtab	*dupconfig(struct servtab *);

int
setconfig(void)
{
	if (defhost)
		free(defhost);
	defhost = newstr("*");
	if (fconfig != NULL) {
		fseek(fconfig, 0L, SEEK_SET);
		return (1);
	}
	fconfig = fopen(CONFIG, "r");
	return (fconfig != NULL);
}

void
endconfig(void)
{
	if (fconfig) {
		(void) fclose(fconfig);
		fconfig = NULL;
	}
	if (defhost) {
		free(defhost);
		defhost = 0;
	}
}

struct servtab *
getconfigent(void)
{
	struct servtab *sep, *tsep;
	char *arg, *cp, *hostdelim, *s;
	int argc;

	sep = (struct servtab *) malloc(sizeof(struct servtab));
	if (sep == NULL) {
		syslog(LOG_ERR, "malloc: %m");
		exit(1);
	}

	memset(sep, 0, sizeof *sep);
more:
	freeconfig(sep);

	while ((cp = nextline(fconfig)) && *cp == '#')
		;
	if (cp == NULL) {
		free(sep);
		return (NULL);
	}

	memset(sep, 0, sizeof *sep);
	arg = skip(&cp, 0);
	if (arg == NULL) {
		/* A blank line. */
		goto more;
	}

	/* Check for a host name. */
	hostdelim = strrchr(arg, ':');
	if (hostdelim) {
		*hostdelim = '\0';
		if (arg[0] == '[' && hostdelim > arg && hostdelim[-1] == ']') {
			hostdelim[-1] = '\0';
			sep->se_hostaddr = newstr(arg + 1);
		} else if (hostdelim == arg)
			sep->se_hostaddr = newstr("*");
		else
			sep->se_hostaddr = newstr(arg);
		arg = hostdelim + 1;
		/*
		 * If the line is of the form `host:', then just change the
		 * default host for the following lines.
		 */
		if (*arg == '\0') {
			arg = skip(&cp, 0);
			if (cp == NULL) {
				free(defhost);
				defhost = newstr(sep->se_hostaddr);
				goto more;
			}
		}
	} else
		sep->se_hostaddr = newstr(defhost);

	sep->se_service = newstr(arg);
	if ((arg = skip(&cp, 1)) == NULL)
		goto more;

	if (strcmp(arg, "stream") == 0)
		sep->se_socktype = SOCK_STREAM;
	else if (strcmp(arg, "dgram") == 0)
		sep->se_socktype = SOCK_DGRAM;
	else if (strcmp(arg, "rdm") == 0)
		sep->se_socktype = SOCK_RDM;
	else if (strcmp(arg, "seqpacket") == 0)
		sep->se_socktype = SOCK_SEQPACKET;
	else if (strcmp(arg, "raw") == 0)
		sep->se_socktype = SOCK_RAW;
	else
		sep->se_socktype = -1;

	if ((arg = skip(&cp, 1)) == NULL)
		goto more;

	sep->se_proto = newstr(arg);

	if (strcmp(sep->se_proto, "unix") == 0) {
		sep->se_family = AF_UNIX;
	} else {
		int s;

		sep->se_family = AF_INET;
		if (sep->se_proto[strlen(sep->se_proto) - 1] == '6')
			sep->se_family = AF_INET6;

		/* check if the family is supported */
		s = socket(sep->se_family, SOCK_DGRAM, 0);
		if (s < 0) {
			syslog(LOG_WARNING, "%s/%s: %s: the address family is "
			    "not supported by the kernel", sep->se_service,
			    sep->se_proto, sep->se_hostaddr);
			goto more;
		}
		close(s);

		if (strncmp(sep->se_proto, "rpc/", 4) == 0) {
			char *cp, *ccp;
			long l;

			cp = strchr(sep->se_service, '/');
			if (cp == 0) {
				syslog(LOG_ERR, "%s: no rpc version",
				    sep->se_service);
				goto more;
			}
			*cp++ = '\0';
			l = strtol(cp, &ccp, 0);
			if (ccp == cp || l < 0 || l > INT_MAX) {
		badafterall:
				syslog(LOG_ERR, "%s/%s: bad rpc version",
				    sep->se_service, cp);
				goto more;
			}
			sep->se_rpcversl = sep->se_rpcversh = l;
			if (*ccp == '-') {
				cp = ccp + 1;
				l = strtol(cp, &ccp, 0);
				if (ccp == cp || l < 0 || l > INT_MAX ||
				    l < sep->se_rpcversl || *ccp)
					goto badafterall;
				sep->se_rpcversh = l;
			} else if (*ccp != '\0')
				goto badafterall;
		}
	}
	arg = skip(&cp, 1);
	if (arg == NULL)
		goto more;

	s = strchr(arg, '.');
	if (s) {
		char *p;

		*s++ = '\0';
		sep->se_max = strtoul(s, &p, 0);
		if (sep->se_max < 1 || *p) {
			syslog(LOG_ERR,
			    "%s: illegal max field \"%s\", setting to %d",
			    sep->se_service, s, toomany);
			sep->se_max = toomany;
		}
	} else
		sep->se_max = toomany;

	sep->se_wait = strcmp(arg, "wait") == 0;
	if ((arg = skip(&cp, 1)) == NULL)
		goto more;
	sep->se_user = newstr(arg);
	arg = strchr(sep->se_user, '.');
	if (arg == NULL)
		arg = strchr(sep->se_user, ':');
	if (arg) {
		*arg++ = '\0';
		sep->se_group = newstr(arg);
	}
	if ((arg = skip(&cp, 1)) == NULL)
		goto more;

	sep->se_server = newstr(arg);
	if (strcmp(sep->se_server, "internal") == 0) {
		struct biltin *bi;

		for (bi = biltins; bi->bi_service; bi++)
			if (bi->bi_socktype == sep->se_socktype &&
			    strcmp(bi->bi_service, sep->se_service) == 0)
				break;
		if (bi->bi_service == 0) {
			syslog(LOG_ERR, "internal service %s unknown",
			    sep->se_service);
			goto more;
		}
		sep->se_bi = bi;
		sep->se_wait = bi->bi_wait;
	} else
		sep->se_bi = NULL;
	argc = 0;
	for (arg = skip(&cp, 0); cp; arg = skip(&cp, 0)) {
		if (argc < MAXARGV)
			sep->se_argv[argc++] = newstr(arg);
	}
	if (argc == 0 && sep->se_bi == NULL) {
		if ((arg = strrchr(sep->se_server, '/')) != NULL)
			arg++;
		else
			arg = sep->se_server;
		sep->se_argv[argc++] = newstr(arg);
	}
	while (argc <= MAXARGV)
		sep->se_argv[argc++] = NULL;

	/*
	 * Resolve each hostname in the se_hostaddr list (if any)
	 * and create a new entry for each resolved address.
	 */
	if (sep->se_hostaddr != NULL && strcmp(sep->se_proto, "unix") != 0) {
		struct addrinfo hints, *res0, *res;
		char *host, *hostlist0, *hostlist, *port;
		int error;

		hostlist = hostlist0 = sep->se_hostaddr;
		sep->se_hostaddr = NULL;
		sep->se_checked = -1;
		while ((host = strsep(&hostlist, ",")) != NULL) {
			if (*host == '\0')
				continue;

			memset(&hints, 0, sizeof(hints));
			hints.ai_family = sep->se_family;
			hints.ai_socktype = sep->se_socktype;
			hints.ai_flags = AI_PASSIVE;
			port = "0";
			/* XXX shortened IPv4 syntax is now forbidden */
			error = getaddrinfo(strcmp(host, "*") ? host : NULL,
			    port, &hints, &res0);
			if (error) {
				syslog(LOG_ERR, "%s/%s: %s: %s",
				    sep->se_service, sep->se_proto,
				    host, gai_strerror(error));
				continue;
			}
			for (res = res0; res; res = res->ai_next) {
				if (res->ai_addrlen >
				    sizeof(sep->se_ctrladdr_storage))
					continue;
				/*
				 * If sep is unused, store host in there.
				 * Otherwise, dup a new entry and prepend it.
				 */
				if (sep->se_checked == -1) {
					sep->se_checked = 0;
				} else {
					tsep = dupconfig(sep);
					tsep->se_next = sep;
					sep = tsep;
				}
				sep->se_hostaddr = newstr(host);
				memcpy(&sep->se_ctrladdr_storage,
				    res->ai_addr, res->ai_addrlen);
				sep->se_ctrladdr_size = res->ai_addrlen;
			}
			freeaddrinfo(res0);
		}
		free(hostlist0);
		if (sep->se_checked == -1)
			goto more;	/* no resolvable names/addresses */
	}

	return (sep);
}

void
freeconfig(struct servtab *cp)
{
	int i;

	free(cp->se_hostaddr);
	cp->se_hostaddr = NULL;
	free(cp->se_service);
	cp->se_service = NULL;
	free(cp->se_proto);
	cp->se_proto = NULL;
	free(cp->se_user);
	cp->se_user = NULL;
	free(cp->se_group);
	cp->se_group = NULL;
	free(cp->se_server);
	cp->se_server = NULL;
	for (i = 0; i < MAXARGV; i++) {
		free(cp->se_argv[i]);
		cp->se_argv[i] = NULL;
	}
}

char *
skip(char **cpp, int report)
{
	char *cp = *cpp;
	char *start;

erp:
	if (*cpp == NULL) {
		if (report)
			syslog(LOG_ERR, "syntax error in inetd config file");
		return (NULL);
	}

again:
	while (*cp == ' ' || *cp == '\t')
		cp++;
	if (*cp == '\0') {
		int c;

		c = getc(fconfig);
		(void) ungetc(c, fconfig);
		if (c == ' ' || c == '\t')
			if ((cp = nextline(fconfig)))
				goto again;
		*cpp = NULL;
		goto erp;
	}
	start = cp;
	while (*cp && *cp != ' ' && *cp != '\t')
		cp++;
	if (*cp != '\0')
		*cp++ = '\0';
	if ((*cpp = cp) == NULL)
		goto erp;

	return (start);
}

char *
nextline(FILE *fd)
{
	if (fgets(line, sizeof (line), fd) == NULL)
		return (NULL);
	line[strcspn(line, "\n")] = '\0';
	return (line);
}

char *
newstr(char *cp)
{
	if ((cp = strdup(cp ? cp : "")))
		return(cp);
	syslog(LOG_ERR, "strdup: %m");
	exit(1);
}

struct servtab *
dupconfig(struct servtab *sep)
{
	struct servtab *newtab;
	int argc;

	newtab = (struct servtab *) malloc(sizeof(struct servtab));

	if (newtab == NULL) {
		syslog(LOG_ERR, "malloc: %m");
		exit(1);
	}

	memset(newtab, 0, sizeof(struct servtab));

	newtab->se_service = sep->se_service ? newstr(sep->se_service) : NULL;
	newtab->se_socktype = sep->se_socktype;
	newtab->se_family = sep->se_family;
	newtab->se_proto = sep->se_proto ? newstr(sep->se_proto) : NULL;
	newtab->se_rpcprog = sep->se_rpcprog;
	newtab->se_rpcversl = sep->se_rpcversl;
	newtab->se_rpcversh = sep->se_rpcversh;
	newtab->se_wait = sep->se_wait;
	newtab->se_user = sep->se_user ? newstr(sep->se_user) : NULL;
	newtab->se_group = sep->se_group ? newstr(sep->se_group) : NULL;
	newtab->se_bi = sep->se_bi;
	newtab->se_server = sep->se_server ? newstr(sep->se_server) : 0;

	for (argc = 0; argc <= MAXARGV; argc++)
		newtab->se_argv[argc] = sep->se_argv[argc] ?
		    newstr(sep->se_argv[argc]) : NULL;
	newtab->se_max = sep->se_max;

	return (newtab);
}

void
inetd_setproctitle(char *a, int s)
{
	socklen_t size;
	struct sockaddr_storage ss;
	char hbuf[NI_MAXHOST];

	size = sizeof(ss);
	if (getpeername(s, (struct sockaddr *)&ss, &size) == 0) {
		if (getnameinfo((struct sockaddr *)&ss, size, hbuf,
		    sizeof(hbuf), NULL, 0, NI_NUMERICHOST) == 0)
			setproctitle("-%s [%s]", a, hbuf);
		else
			setproctitle("-%s [?]", a);
	} else
		setproctitle("-%s", a);
}

void
logpid(void)
{
	FILE *fp;

	if ((fp = fopen(_PATH_INETDPID, "w")) != NULL) {
		fprintf(fp, "%ld\n", (long)getpid());
		(void)fclose(fp);
	}
}

int
bump_nofile(void)
{
#define FD_CHUNK	32

	struct rlimit rl;

	if (getrlimit(RLIMIT_NOFILE, &rl) < 0) {
		syslog(LOG_ERR, "getrlimit: %m");
		return -1;
	}
	rl.rlim_cur = MIN(rl.rlim_max, rl.rlim_cur + FD_CHUNK);
	rl.rlim_cur = MIN(FD_SETSIZE, rl.rlim_cur + FD_CHUNK);
	if (rl.rlim_cur <= rlim_nofile_cur) {
		syslog(LOG_ERR,
		    "bump_nofile: cannot extend file limit, max = %d",
		    (int)rl.rlim_cur);
		return -1;
	}

	if (setrlimit(RLIMIT_NOFILE, &rl) < 0) {
		syslog(LOG_ERR, "setrlimit: %m");
		return -1;
	}

	rlim_nofile_cur = rl.rlim_cur;
	return 0;
}

/*
 * Internet services provided internally by inetd:
 */
#define	BUFSIZE	4096

/* ARGSUSED */
void
echo_stream(int s, struct servtab *sep)
{
	char buffer[BUFSIZE];
	int i;

	inetd_setproctitle(sep->se_service, s);
	while ((i = read(s, buffer, sizeof(buffer))) > 0 &&
	    write(s, buffer, i) > 0)
		;
	exit(0);
}

/* ARGSUSED */
void
echo_dg(int s, struct servtab *sep)
{
	char buffer[BUFSIZE];
	int i;
	socklen_t size;
	struct sockaddr_storage ss;

	size = sizeof(ss);
	if ((i = recvfrom(s, buffer, sizeof(buffer), 0,
	    (struct sockaddr *)&ss, &size)) < 0)
		return;
	if (dg_badinput((struct sockaddr *)&ss))
		return;
	(void) sendto(s, buffer, i, 0, (struct sockaddr *)&ss, size);
}

/* ARGSUSED */
void
discard_stream(int s, struct servtab *sep)
{
	char buffer[BUFSIZE];

	inetd_setproctitle(sep->se_service, s);
	while ((errno = 0, read(s, buffer, sizeof(buffer)) > 0) ||
	    errno == EINTR)
		;
	exit(0);
}

/* ARGSUSED */
void
discard_dg(int s, struct servtab *sep)
{
	char buffer[BUFSIZE];

	(void) read(s, buffer, sizeof(buffer));
}

#include <ctype.h>
#define LINESIZ 72
char ring[128];
char *endring;

void
initring(void)
{
	int i;

	endring = ring;

	for (i = 0; i <= sizeof ring; ++i)
		if (isprint(i))
			*endring++ = i;
}

/* ARGSUSED */
void
chargen_stream(int s, struct servtab *sep)
{
	char *rs;
	int len;
	char text[LINESIZ+2];

	inetd_setproctitle(sep->se_service, s);

	if (!endring) {
		initring();
		rs = ring;
	}

	text[LINESIZ] = '\r';
	text[LINESIZ + 1] = '\n';
	for (rs = ring;;) {
		if ((len = endring - rs) >= LINESIZ)
			memmove(text, rs, LINESIZ);
		else {
			memmove(text, rs, len);
			memmove(text + len, ring, LINESIZ - len);
		}
		if (++rs == endring)
			rs = ring;
		if (write(s, text, sizeof(text)) != sizeof(text))
			break;
	}
	exit(0);
}

/* ARGSUSED */
void
chargen_dg(int s, struct servtab *sep)
{
	struct sockaddr_storage ss;
	static char *rs;
	int len;
	socklen_t size;
	char text[LINESIZ+2];

	if (endring == 0) {
		initring();
		rs = ring;
	}

	size = sizeof(ss);
	if (recvfrom(s, text, sizeof(text), 0, (struct sockaddr *)&ss,
	    &size) < 0)
		return;
	if (dg_badinput((struct sockaddr *)&ss))
		return;

	if ((len = endring - rs) >= LINESIZ)
		memmove(text, rs, LINESIZ);
	else {
		memmove(text, rs, len);
		memmove(text + len, ring, LINESIZ - len);
	}
	if (++rs == endring)
		rs = ring;
	text[LINESIZ] = '\r';
	text[LINESIZ + 1] = '\n';
	(void) sendto(s, text, sizeof(text), 0, (struct sockaddr *)&ss, size);
}

/*
 * Return a machine readable date and time, in the form of the
 * number of seconds since midnight, Jan 1, 1900.  Since gettimeofday
 * returns the number of seconds since midnight, Jan 1, 1970,
 * we must add 2208988800 seconds to this figure to make up for
 * some seventy years Bell Labs was asleep.
 */
u_int32_t
machtime(void)
{
	struct timeval tv;

	if (gettimeofday(&tv, NULL) < 0)
		return (0L);

	return (htonl((u_int32_t)tv.tv_sec + 2208988800UL));
}

/* ARGSUSED */
void
machtime_stream(s, sep)
	int s;
	struct servtab *sep;
{
	u_int32_t result;

	result = machtime();
	(void) write(s, &result, sizeof(result));
}

/* ARGSUSED */
void
machtime_dg(int s, struct servtab *sep)
{
	u_int32_t result;
	struct sockaddr_storage ss;
	socklen_t size;

	size = sizeof(ss);
	if (recvfrom(s, &result, sizeof(result), 0,
	    (struct sockaddr *)&ss, &size) < 0)
		return;
	if (dg_badinput((struct sockaddr *)&ss))
		return;
	result = machtime();
	(void) sendto(s, &result, sizeof(result), 0,
	    (struct sockaddr *)&ss, size);
}

/* Return human-readable time of day */
/* ARGSUSED */
void
daytime_stream(int s, struct servtab *sep)
{
	char buffer[256];
	time_t clock;

	clock = time(NULL);

	(void) snprintf(buffer, sizeof buffer, "%.24s\r\n", ctime(&clock));
	(void) write(s, buffer, strlen(buffer));
}

/* Return human-readable time of day */
/* ARGSUSED */
void
daytime_dg(int s, struct servtab *sep)
{
	char buffer[256];
	time_t clock;
	struct sockaddr_storage ss;
	socklen_t size;

	clock = time(NULL);

	size = sizeof(ss);
	if (recvfrom(s, buffer, sizeof(buffer), 0, (struct sockaddr *)&ss,
	    &size) < 0)
		return;
	if (dg_badinput((struct sockaddr *)&ss))
		return;
	(void) snprintf(buffer, sizeof buffer, "%.24s\r\n", ctime(&clock));
	(void) sendto(s, buffer, strlen(buffer), 0, (struct sockaddr *)&ss,
	    size);
}

/*
 * print_service:
 *	Dump relevant information to stderr
 */
void
print_service(char *action, struct servtab *sep)
{
	if (strcmp(sep->se_hostaddr, "*") == 0)
		fprintf(stderr, "%s: %s ", action, sep->se_service);
	else
		fprintf(stderr, "%s: %s:%s ", action, sep->se_hostaddr,
		    sep->se_service);

	if (isrpcservice(sep))
		fprintf(stderr, "rpcprog=%d, rpcvers=%d/%d, proto=%s,",
		    sep->se_rpcprog, sep->se_rpcversh,
		    sep->se_rpcversl, sep->se_proto);
	else
		fprintf(stderr, "proto=%s,", sep->se_proto);

	fprintf(stderr,
	    " wait.max=%hd.%d user:group=%s:%s builtin=%lx server=%s\n",
	    sep->se_wait, sep->se_max, sep->se_user,
	    sep->se_group ? sep->se_group : "wheel",
	    (long)sep->se_bi, sep->se_server);
}

void
spawn(struct servtab *sep, int ctrl)
{
	struct passwd *pwd;
	int tmpint, dofork;
	struct group *grp = NULL;
	char buf[50];
	pid_t pid;

	pid = 0;
	dofork = (sep->se_bi == 0 || sep->se_bi->bi_fork);
	if (dofork) {
		if (sep->se_count++ == 0)
		    (void)gettimeofday(&sep->se_time, NULL);
		else if (sep->se_count >= sep->se_max) {
			struct timeval now;

			(void)gettimeofday(&now, NULL);
			if (now.tv_sec - sep->se_time.tv_sec >
			    CNT_INTVL) {
				sep->se_time = now;
				sep->se_count = 1;
			} else {
				if (!sep->se_wait &&
				    sep->se_socktype == SOCK_STREAM)
					close(ctrl);
				if (sep->se_family == AF_INET &&
				    ntohs(sep->se_ctrladdr_in.sin_port) >=
				    IPPORT_RESERVED) {
					/*
					 * Cannot close it -- there are
					 * thieves on the system.
					 * Simply ignore the connection.
					 */
					--sep->se_count;
					sigprocmask(SIG_SETMASK, &emptymask,
					    NULL);
					return;
				}
				syslog(LOG_ERR,
				    "%s/%s server failing (looping), service terminated",
				    sep->se_service, sep->se_proto);
				if (!sep->se_wait &&
				    sep->se_socktype == SOCK_STREAM)
					close(ctrl);
				FD_CLR(sep->se_fd, allsockp);
				(void) close(sep->se_fd);
				sep->se_fd = -1;
				sep->se_count = 0;
				nsock--;
				sigprocmask(SIG_SETMASK, &emptymask,
				    NULL);
				if (!timingout) {
					timingout = 1;
					alarm(RETRYTIME);
				}
				return;
			}
		}
		pid = fork();
	}
	if (pid < 0) {
		syslog(LOG_ERR, "fork: %m");
		if (!sep->se_wait && sep->se_socktype == SOCK_STREAM)
			close(ctrl);
		sigprocmask(SIG_SETMASK, &emptymask, NULL);
		sleep(1);
		return;
	}
	if (pid && sep->se_wait) {
		sep->se_wait = pid;
		FD_CLR(sep->se_fd, allsockp);
		nsock--;
	}
	sigprocmask(SIG_SETMASK, &emptymask, NULL);
	if (pid == 0) {
		if (sep->se_bi)
			(*sep->se_bi->bi_fn)(ctrl, sep);
		else {
			if ((pwd = getpwnam(sep->se_user)) == NULL) {
				syslog(LOG_ERR,
				    "getpwnam: %s: No such user",
				    sep->se_user);
				if (sep->se_socktype != SOCK_STREAM)
					recv(0, buf, sizeof (buf), 0);
				exit(1);
			}
			if (setsid() <0)
				syslog(LOG_ERR, "%s: setsid: %m",
				    sep->se_service);
			if (sep->se_group &&
			    (grp = getgrnam(sep->se_group)) == NULL) {
				syslog(LOG_ERR,
				    "getgrnam: %s: No such group",
				    sep->se_group);
				if (sep->se_socktype != SOCK_STREAM)
					recv(0, buf, sizeof (buf), 0);
				exit(1);
			}
			if (uid != 0) {
				/* a user running private inetd */
				if (uid != pwd->pw_uid)
					exit(1);
			} else {
				tmpint = LOGIN_SETALL &
				    ~(LOGIN_SETGROUP|LOGIN_SETLOGIN);
				if (pwd->pw_uid)
					tmpint |= LOGIN_SETGROUP|LOGIN_SETLOGIN;
				if (sep->se_group) {
					pwd->pw_gid = grp->gr_gid;
					tmpint |= LOGIN_SETGROUP;
				}
				if (setusercontext(NULL, pwd, pwd->pw_uid,
				    tmpint) < 0) {
					syslog(LOG_ERR,
					    "%s/%s: setusercontext: %m",
					    sep->se_service, sep->se_proto);
					exit(1);
				}
			}
			if (debug)
				fprintf(stderr, "%ld execv %s\n",
				    (long)getpid(), sep->se_server);
			if (ctrl != STDIN_FILENO) {
				dup2(ctrl, STDIN_FILENO);
				close(ctrl);
			}
			dup2(STDIN_FILENO, STDOUT_FILENO);
			dup2(STDIN_FILENO, STDERR_FILENO);
			closelog();
			for (tmpint = rlim_nofile_cur-1; --tmpint > 2; )
				(void)close(tmpint);
			sigaction(SIGPIPE, &sapipe, NULL);
			execv(sep->se_server, sep->se_argv);
			if (sep->se_socktype != SOCK_STREAM)
				recv(0, buf, sizeof (buf), 0);
			syslog(LOG_ERR, "execv %s: %m", sep->se_server);
			exit(1);
		}
	}
	if (!sep->se_wait && sep->se_socktype == SOCK_STREAM)
		close(ctrl);
}
