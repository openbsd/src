/*	$OpenBSD: rshd.c,v 1.56 2014/04/16 19:14:57 okan Exp $	*/

/*-
 * Copyright (c) 1988, 1989, 1992, 1993, 1994
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
 * remote shell server:
 *	[port]\0
 *	remuser\0
 *	locuser\0
 *	command\0
 *	data
 */
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/socket.h>

#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <arpa/inet.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <paths.h>
#include <poll.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <stdarg.h>
#include <login_cap.h>
#include <bsd_auth.h>

int	keepalive = 1;
int	check_all;
int	log_success;		/* If TRUE, log all successful accesses */
int	sent_null;
login_cap_t *lc;

void	 doit(struct sockaddr *);
void	 error(const char *, ...);
void	 getstr(char *, int, char *);
int	 local_domain(char *);
char	*topdomain(char *);
void	 usage(void);

#define	P_SOCKREAD	0
#define	P_PIPEREAD	1

int
main(int argc, char *argv[])
{
	extern int __check_rhosts_file;
	struct linger linger;
	int ch, on = 1;
	socklen_t fromlen;
	struct sockaddr_storage from;

	openlog("rshd", LOG_PID | LOG_ODELAY, LOG_DAEMON);

	opterr = 0;
	while ((ch = getopt(argc, argv, "aLln")) != -1)
		switch (ch) {
		case 'a':
			check_all = 1;
			break;
		case 'L':
			log_success = 1;
			break;
		case 'l':
			__check_rhosts_file = 0;
			break;
		case 'n':
			keepalive = 0;
			break;
		case '?':
		default:
			usage();
			break;
		}

	argc -= optind;
	argv += optind;

	fromlen = sizeof (from);
	if (getpeername(STDIN_FILENO, (struct sockaddr *)&from, &fromlen) < 0) {
		/* syslog(LOG_ERR, "getpeername: %m"); */
		exit(1);
	}
	if (keepalive &&
	    setsockopt(STDIN_FILENO, SOL_SOCKET, SO_KEEPALIVE, (char *)&on,
	    sizeof(on)) < 0)
		syslog(LOG_WARNING, "setsockopt (SO_KEEPALIVE): %m");
	linger.l_onoff = 1;
	linger.l_linger = 60;			/* XXX */
	if (setsockopt(STDIN_FILENO, SOL_SOCKET, SO_LINGER, (char *)&linger,
	    sizeof (linger)) < 0)
		syslog(LOG_WARNING, "setsockopt (SO_LINGER): %m");
	doit((struct sockaddr *)&from);
	/* NOTREACHED */
	exit(0);
}

char	*envinit[1] = { 0 };
extern char **environ;

void
doit(struct sockaddr *fromp)
{
	extern char *__rcmd_errstr;	/* syslog hook from libc/net/rcmd.c. */
	struct addrinfo hints, *res, *res0;
	int gaierror;
	struct passwd *pwd;
	u_short port;
	in_port_t *portp;
	struct pollfd pfd[2];
	int cc, nfd, pv[2], s = 0, one = 1;
	pid_t pid;
	char *hostname, *errorstr, *errorhost = (char *) NULL;
	char *cp, sig, buf[BUFSIZ];
	char cmdbuf[NCARGS+1], locuser[_PW_NAME_LEN+1], remuser[_PW_NAME_LEN+1];
	char remotehost[2 * MAXHOSTNAMELEN + 1];
	char hostnamebuf[2 * MAXHOSTNAMELEN + 1];
	char naddr[NI_MAXHOST];
	char saddr[NI_MAXHOST];
	char raddr[NI_MAXHOST];
	char pbuf[NI_MAXSERV];
	auth_session_t *as;
	const int niflags = NI_NUMERICHOST | NI_NUMERICSERV;

	(void) signal(SIGINT, SIG_DFL);
	(void) signal(SIGQUIT, SIG_DFL);
	(void) signal(SIGTERM, SIG_DFL);
#ifdef DEBUG
	{ int t = open(_PATH_TTY, 2);
	  if (t >= 0) {
		ioctl(t, TIOCNOTTY, (char *)0);
		(void) close(t);
	  }
	}
#endif
	switch (fromp->sa_family) {
	case AF_INET:
		portp = &((struct sockaddr_in *)fromp)->sin_port;
		break;
	case AF_INET6:
		portp = &((struct sockaddr_in6 *)fromp)->sin6_port;
		break;
	default:
		syslog(LOG_ERR, "malformed \"from\" address (af %d)",
		    fromp->sa_family);
		exit(1);
	}
	if (getnameinfo(fromp, fromp->sa_len, naddr, sizeof(naddr),
	    pbuf, sizeof(pbuf), niflags) != 0) {
		syslog(LOG_ERR, "malformed \"from\" address (af %d)",
		    fromp->sa_family);
		exit(1);
	}

#ifdef IP_OPTIONS
	if (fromp->sa_family == AF_INET) {
		struct ipoption opts;
		socklen_t optsize = sizeof(opts);
		int ipproto, i;
		struct protoent *ip;

		if ((ip = getprotobyname("ip")) != NULL)
			ipproto = ip->p_proto;
		else
			ipproto = IPPROTO_IP;
		if (!getsockopt(STDIN_FILENO, ipproto, IP_OPTIONS,
		    (char *)&opts, &optsize) && optsize != 0) {
			for (i = 0; (void *)&opts.ipopt_list[i] - (void *)&opts <
			    optsize; ) {
				u_char c = (u_char)opts.ipopt_list[i];
				if (c == IPOPT_LSRR || c == IPOPT_SSRR)
					exit(1);
				if (c == IPOPT_EOL)
					break;
				i += (c == IPOPT_NOP) ? 1 :
				    (u_char)opts.ipopt_list[i+1];
			}
		}
	}
#endif

	if (ntohs(*portp) >= IPPORT_RESERVED ||
	    ntohs(*portp) < IPPORT_RESERVED/2) {
		syslog(LOG_NOTICE|LOG_AUTH,
		    "Connection from %s on illegal port %u",
		    naddr, ntohs(*portp));
		exit(1);
	}

	(void) alarm(60);
	port = 0;
	for (;;) {
		char c;
		if ((cc = read(STDIN_FILENO, &c, 1)) != 1) {
			if (cc < 0)
				syslog(LOG_NOTICE, "read: %m");
			shutdown(STDIN_FILENO, SHUT_RDWR);
			exit(1);
		}
		if (c == 0)
			break;
		port = port * 10 + c - '0';
	}

	(void) alarm(0);
	if (port != 0) {
		int lport;
		if (port >= IPPORT_RESERVED ||
		    port < IPPORT_RESERVED/2) {
			syslog(LOG_ERR, "2nd port not reserved");
			exit(1);
		}
		*portp = htons(port);
		lport = IPPORT_RESERVED - 1;
		s = rresvport_af(&lport, fromp->sa_family);
		if (s < 0) {
			syslog(LOG_ERR, "can't get stderr port: %m");
			exit(1);
		}
		if (connect(s, (struct sockaddr *)fromp, fromp->sa_len) < 0) {
			syslog(LOG_INFO, "connect second port %d: %m", port);
			exit(1);
		}
	}

#ifdef notdef
	/* from inetd, socket is already on 0, 1, 2 */
	dup2(f, 0);
	dup2(f, 1);
	dup2(f, 2);
#endif
	errorstr = NULL;
	if (getnameinfo(fromp, fromp->sa_len, saddr, sizeof(saddr),
			NULL, 0, NI_NAMEREQD)== 0) {
		/*
		 * If name returned by getnameinfo is in our domain,
		 * attempt to verify that we haven't been fooled by someone
		 * in a remote net; look up the name and check that this
		 * address corresponds to the name.
		 */
		hostname = saddr;
		res0 = NULL;
		if (check_all || local_domain(saddr)) {
			strlcpy(remotehost, saddr, sizeof(remotehost));
			errorhost = remotehost;
			memset(&hints, 0, sizeof(hints));
			hints.ai_family = fromp->sa_family;
			hints.ai_socktype = SOCK_STREAM;
			hints.ai_flags = AI_CANONNAME;
			gaierror = getaddrinfo(remotehost, pbuf, &hints, &res0);
			if (gaierror) {
				syslog(LOG_INFO,
				    "Couldn't look up address for %s: %s",
				    remotehost, gai_strerror(gaierror));
				errorstr =
				"Couldn't look up address for your host (%s)\n";
				hostname = naddr;
			} else {
				for (res = res0; res; res = res->ai_next) {
					if (res->ai_family != fromp->sa_family)
						continue;
					if (res->ai_addrlen != fromp->sa_len)
						continue;
					if (getnameinfo(res->ai_addr,
						res->ai_addrlen,
						raddr, sizeof(raddr), NULL, 0,
						niflags) == 0
					 && strcmp(naddr, raddr) == 0) {
						hostname = res->ai_canonname
							? res->ai_canonname
							: saddr;
						break;
					}
				}
				if (res == NULL) {
					syslog(LOG_NOTICE,
					  "Host addr %s not listed for host %s",
					    naddr, res0->ai_canonname
							? res0->ai_canonname
							: saddr);
					errorstr =
					    "Host address mismatch for %s\n";
					hostname = naddr;
				}
			}
		}
		strlcpy(hostnamebuf, hostname, sizeof(hostnamebuf));
		hostname = hostnamebuf;
		if (res0)
			freeaddrinfo(res0);
	} else {
		strlcpy(hostnamebuf, naddr, sizeof(hostnamebuf));
		errorhost = hostname = hostnamebuf;
	}

	getstr(remuser, sizeof(remuser), "remuser");
	getstr(locuser, sizeof(locuser), "locuser");
	getstr(cmdbuf, sizeof(cmdbuf), "command");
	pwd = getpwnam(locuser);
	if (pwd == NULL) {
		syslog(LOG_INFO|LOG_AUTH,
		    "%s@%s as %s: unknown login. cmd='%.80s'",
		    remuser, hostname, locuser, cmdbuf);
		if (errorstr == NULL)
			errorstr = "Permission denied.\n";
		goto fail;
	}
	lc = login_getclass(pwd->pw_class);
	if (lc == NULL) {
		syslog(LOG_INFO|LOG_AUTH,
		    "%s@%s as %s: unknown class. cmd='%.80s'",
		    remuser, hostname, locuser, cmdbuf);
		if (errorstr == NULL)
			errorstr = "Login incorrect.\n";
		goto fail;
	}
	as = auth_open();
	if (as == NULL || auth_setpwd(as, pwd) != 0) {
		syslog(LOG_INFO|LOG_AUTH,
		    "%s@%s as %s: unable to allocate memory. cmd='%.80s'",
		    remuser, hostname, locuser, cmdbuf);
		if (errorstr == NULL)
			errorstr = "Cannot allocate memory.\n";
		goto fail;
	}

	setegid(pwd->pw_gid);
	seteuid(pwd->pw_uid);
	if (chdir(pwd->pw_dir) < 0) {
		(void) chdir("/");
#ifdef notdef
		syslog(LOG_INFO|LOG_AUTH,
		    "%s@%s as %s: no home directory. cmd='%.80s'",
		    remuser, hostname, locuser, cmdbuf);
		error("No remote directory.\n");
		exit(1);
#endif
	}
	seteuid(0);
	setegid(0);	/* XXX use a saved gid instead? */

	if (errorstr ||
	    (pwd->pw_passwd != 0 && *pwd->pw_passwd != '\0' &&
	    iruserok_sa(fromp, fromp->sa_len, pwd->pw_uid == 0,
	    remuser, locuser) < 0)) {
		if (__rcmd_errstr)
			syslog(LOG_INFO|LOG_AUTH,
			    "%s@%s as %s: permission denied (%s). cmd='%.80s'",
			    remuser, hostname, locuser, __rcmd_errstr,
			    cmdbuf);
		else
			syslog(LOG_INFO|LOG_AUTH,
			    "%s@%s as %s: permission denied. cmd='%.80s'",
			    remuser, hostname, locuser, cmdbuf);
fail:
		if (errorstr == NULL)
			errorstr = "Permission denied.\n";
		error(errorstr, errorhost);
		exit(1);
	}

	if (pwd->pw_uid)
		auth_checknologin(lc);

	(void) write(STDERR_FILENO, "\0", 1);
	sent_null = 1;

	if (port) {
		if (pipe(pv) < 0) {
			error("Can't make pipe.\n");
			exit(1);
		}
		pid = fork();
		if (pid == -1)  {
			error("Can't fork; try again.\n");
			exit(1);
		}
		if (pid) {
			(void) close(STDIN_FILENO);
			(void) close(STDOUT_FILENO);
			(void) close(STDERR_FILENO);
			(void) close(pv[1]);

			pfd[P_SOCKREAD].fd = s;
			pfd[P_SOCKREAD].events = POLLIN;
			pfd[P_PIPEREAD].fd = pv[0];
			pfd[P_PIPEREAD].events = POLLIN;
			nfd = 2;
			ioctl(pv[0], FIONBIO, (char *)&one);

			/* should set s nbio! */
			do {
				if (poll(pfd, nfd, INFTIM) < 0)
					break;
				if (pfd[P_SOCKREAD].revents & POLLIN) {
					int	ret;
					ret = read(s, &sig, 1);
					if (ret <= 0)
						pfd[P_SOCKREAD].revents = 0;
					else
						killpg(pid, sig);
				}
				if (pfd[P_PIPEREAD].revents & POLLIN) {
					errno = 0;
					cc = read(pv[0], buf, sizeof(buf));
					if (cc <= 0) {
						shutdown(s, SHUT_RDWR);
						pfd[P_PIPEREAD].revents = 0;
					} else {
						(void) write(s, buf, cc);
					}
				}
			} while ((pfd[P_SOCKREAD].revents & POLLIN) ||
			    (pfd[P_PIPEREAD].revents & POLLIN));
			exit(0);
		}
		setsid();
		(void) close(s);
		(void) close(pv[0]);
		dup2(pv[1], 2);
		close(pv[1]);
	} else
		setsid();
	if (*pwd->pw_shell == '\0')
		pwd->pw_shell = _PATH_BSHELL;

	environ = envinit;
	if (setenv("HOME", pwd->pw_dir, 1) == -1 ||
	    setenv("SHELL", pwd->pw_shell, 1) == -1 ||
	    setenv("USER", pwd->pw_name, 1) == -1 ||
	    setenv("LOGNAME", pwd->pw_name, 1) == -1)
		errx(1, "cannot setup environment");

	if (setusercontext(lc, pwd, pwd->pw_uid, LOGIN_SETALL))
		errx(1, "cannot set user context");
	if (auth_approval(as, lc, pwd->pw_name, "rsh") <= 0)
		errx(1, "approval failure");
	auth_close(as);
	login_close(lc);

	cp = strrchr(pwd->pw_shell, '/');
	if (cp)
		cp++;
	else
		cp = pwd->pw_shell;
	endpwent();
	if (log_success || pwd->pw_uid == 0) {
		syslog(LOG_INFO|LOG_AUTH, "%s@%s as %s: cmd='%.80s'",
		    remuser, hostname, locuser, cmdbuf);
	}
	execl(pwd->pw_shell, cp, "-c", cmdbuf, (char *)NULL);
	perror(pwd->pw_shell);
	exit(1);
}

/*
 * Report error to client.  Note: can't be used until second socket has
 * connected to client, or older clients will hang waiting for that
 * connection first.
 */
void
error(const char *fmt, ...)
{
	va_list ap;
	int len;
	char *bp, buf[BUFSIZ];

	va_start(ap, fmt);
	bp = buf;
	if (sent_null == 0) {
		*bp++ = 1;
		len = 1;
	} else
		len = 0;
	(void)vsnprintf(bp, sizeof(buf) - len, fmt, ap);
	(void)write(STDERR_FILENO, buf, len + strlen(bp));
	va_end(ap);
}

void
getstr(char *buf, int cnt, char *err)
{
	char c;

	do {
		if (read(STDIN_FILENO, &c, 1) != 1)
			exit(1);
		*buf++ = c;
		if (--cnt == 0) {
			error("%s too long\n", err);
			exit(1);
		}
	} while (c != 0);
}

/*
 * Check whether host h is in our local domain,
 * defined as sharing the last two components of the domain part,
 * or the entire domain part if the local domain has only one component.
 * If either name is unqualified (contains no '.'),
 * assume that the host is local, as it will be
 * interpreted as such.
 */
int
local_domain(char *h)
{
	char localhost[MAXHOSTNAMELEN];
	char *p1, *p2;

	localhost[0] = 0;
	(void) gethostname(localhost, sizeof(localhost));
	p1 = topdomain(localhost);
	p2 = topdomain(h);
	if (p1 == NULL || p2 == NULL || !strcasecmp(p1, p2))
		return (1);
	return (0);
}

char *
topdomain(char *h)
{
	char *p, *maybe = NULL;
	int dots = 0;

	for (p = h + strlen(h); p >= h; p--) {
		if (*p == '.') {
			if (++dots == 2)
				return (p);
			maybe = p;
		}
	}
	return (maybe);
}

void
usage(void)
{
	syslog(LOG_ERR, "usage: rshd [-aLln]");
	exit(2);
}
