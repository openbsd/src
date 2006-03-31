/*
 * Copyright (c) 1995, 1996, 1998 Theo de Raadt.  All rights reserved.
 * Copyright (c) 1983, 1993, 1994
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

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <signal.h>
#include <fcntl.h>
#include <netdb.h>
#include <unistd.h>
#include <pwd.h>
#include <errno.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <syslog.h>
#include <stdlib.h>
#include <netgroup.h>

int	__ivaliduser(FILE *, in_addr_t, const char *, const char *);
int	__ivaliduser_sa(FILE *, struct sockaddr *, socklen_t,
	    const char *, const char *);
static int __icheckhost(struct sockaddr *, socklen_t, const char *);
static char *__gethostloop(struct sockaddr *, socklen_t);

int
rcmd(char **ahost, int rport, const char *locuser, const char *remuser,
    const char *cmd, int *fd2p)
{
	return rcmd_af(ahost, rport, locuser, remuser, cmd, fd2p, AF_INET);
}

int
rcmd_af(char **ahost, int porta, const char *locuser, const char *remuser,
    const char *cmd, int *fd2p, int af)
{
	static char hbuf[MAXHOSTNAMELEN];
	char pbuf[NI_MAXSERV];
	struct addrinfo hints, *res, *r;
	int error;
	struct sockaddr_storage from;
	fd_set *readsp = NULL;
	sigset_t oldmask, mask;
	pid_t pid;
	int s, lport, timo;
	char c, *p;
	int refused;
	in_port_t rport = porta;

	/* call rcmdsh() with specified remote shell if appropriate. */
	if (!issetugid() && (p = getenv("RSH")) && *p) {
		struct servent *sp = getservbyname("shell", "tcp");

		if (sp && sp->s_port == rport)
			return (rcmdsh(ahost, rport, locuser, remuser,
			    cmd, p));
	}

	/* use rsh(1) if non-root and remote port is shell. */
	if (geteuid()) {
		struct servent *sp = getservbyname("shell", "tcp");

		if (sp && sp->s_port == rport)
			return (rcmdsh(ahost, rport, locuser, remuser,
			    cmd, NULL));
	}

	pid = getpid();
	snprintf(pbuf, sizeof(pbuf), "%u", ntohs(rport));
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = af;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_CANONNAME;
	error = getaddrinfo(*ahost, pbuf, &hints, &res);
	if (error) {
#if 0
		warnx("%s: %s", *ahost, gai_strerror(error));
#endif
		return (-1);
	}
	if (res->ai_canonname) {
		strlcpy(hbuf, res->ai_canonname, sizeof(hbuf));
		*ahost = hbuf;
	} else
		; /*XXX*/

	r = res;
	refused = 0;
	sigemptyset(&mask);
	sigaddset(&mask, SIGURG);
	sigprocmask(SIG_BLOCK, &mask, &oldmask);
	for (timo = 1, lport = IPPORT_RESERVED - 1;;) {
		s = rresvport_af(&lport, r->ai_family);
		if (s < 0) {
			if (errno == EAGAIN)
				(void)fprintf(stderr,
				    "rcmd: socket: All ports in use\n");
			else
				(void)fprintf(stderr, "rcmd: socket: %s\n",
				    strerror(errno));
			if (r->ai_next) {
				r = r->ai_next;
				continue;
			} else {
				sigprocmask(SIG_SETMASK, &oldmask, NULL);
				freeaddrinfo(res);
				return (-1);
			}
		}
		fcntl(s, F_SETOWN, pid);
		if (connect(s, r->ai_addr, r->ai_addrlen) >= 0)
			break;
		(void)close(s);
		if (errno == EADDRINUSE) {
			lport--;
			continue;
		}
		if (errno == ECONNREFUSED)
			refused++;
		if (r->ai_next) {
			int oerrno = errno;
			char hbuf[NI_MAXHOST];
			const int niflags = NI_NUMERICHOST;

			hbuf[0] = '\0';
			if (getnameinfo(r->ai_addr, r->ai_addrlen,
			    hbuf, sizeof(hbuf), NULL, 0, niflags) != 0)
				strlcpy(hbuf, "(invalid)", sizeof hbuf);
			(void)fprintf(stderr, "connect to address %s: ", hbuf);
			errno = oerrno;
			perror(0);
			r = r->ai_next;
			hbuf[0] = '\0';
			if (getnameinfo(r->ai_addr, r->ai_addrlen,
			    hbuf, sizeof(hbuf), NULL, 0, niflags) != 0)
				strlcpy(hbuf, "(invalid)", sizeof hbuf);
			(void)fprintf(stderr, "Trying %s...\n", hbuf);
			continue;
		}
		if (refused && timo <= 16) {
			(void)sleep(timo);
			timo *= 2;
			r = res;
			refused = 0;
			continue;
		}
		(void)fprintf(stderr, "%s: %s\n", res->ai_canonname,
		    strerror(errno));
		sigprocmask(SIG_SETMASK, &oldmask, NULL);
		freeaddrinfo(res);
		return (-1);
	}
	/* given "af" can be PF_UNSPEC, we need the real af for "s" */
	af = r->ai_family;
	freeaddrinfo(res);
#if 0
	/*
	 * try to rresvport() to the same port. This will make rresvport()
	 * fail it's first bind, resulting in it choosing a random port.
	 */
	lport--;
#endif
	if (fd2p == 0) {
		write(s, "", 1);
		lport = 0;
	} else {
		char num[8];
		int s2 = rresvport_af(&lport, af), s3;
		socklen_t len = sizeof(from);
		int fdssize = howmany(MAX(s, s2)+1, NFDBITS) * sizeof(fd_mask);

		if (s2 < 0)
			goto bad;
		readsp = (fd_set *)malloc(fdssize);
		if (readsp == NULL) {
			close(s2);
			goto bad;
		}
		listen(s2, 1);
		(void)snprintf(num, sizeof(num), "%d", lport);
		if (write(s, num, strlen(num)+1) != strlen(num)+1) {
			(void)fprintf(stderr,
			    "rcmd: write (setting up stderr): %s\n",
			    strerror(errno));
			(void)close(s2);
			goto bad;
		}
again:
		bzero(readsp, fdssize);
		FD_SET(s, readsp);
		FD_SET(s2, readsp);
		errno = 0;
		if (select(MAX(s, s2) + 1, readsp, 0, 0, 0) < 1 ||
		    !FD_ISSET(s2, readsp)) {
			if (errno != 0)
				(void)fprintf(stderr,
				    "rcmd: select (setting up stderr): %s\n",
				    strerror(errno));
			else
				(void)fprintf(stderr,
				"select: protocol failure in circuit setup\n");
			(void)close(s2);
			goto bad;
		}
		s3 = accept(s2, (struct sockaddr *)&from, &len);
		if (s3 < 0) {
			(void)fprintf(stderr,
			    "rcmd: accept: %s\n", strerror(errno));
			lport = 0;
			close(s2);
			goto bad;
		}

		/*
		 * XXX careful for ftp bounce attacks. If discovered, shut them
		 * down and check for the real auxiliary channel to connect.
		 */
		switch (from.ss_family) {
		case AF_INET:
		case AF_INET6:
			if (getnameinfo((struct sockaddr *)&from, len,
			    NULL, 0, num, sizeof(num), NI_NUMERICSERV) == 0 &&
			    atoi(num) != 20) {
				break;
			}
			close(s3);
			goto again;
		default:
			break;
		}
		(void)close(s2);

		*fd2p = s3;
		switch (from.ss_family) {
		case AF_INET:
		case AF_INET6:
			if (getnameinfo((struct sockaddr *)&from, len,
			    NULL, 0, num, sizeof(num), NI_NUMERICSERV) != 0 ||
			    (atoi(num) >= IPPORT_RESERVED ||
			     atoi(num) < IPPORT_RESERVED / 2)) {
				(void)fprintf(stderr,
				    "socket: protocol failure in circuit setup.\n");
				goto bad2;
			}
			break;
		default:
			break;
		}
	}
	(void)write(s, locuser, strlen(locuser)+1);
	(void)write(s, remuser, strlen(remuser)+1);
	(void)write(s, cmd, strlen(cmd)+1);
	if (read(s, &c, 1) != 1) {
		(void)fprintf(stderr,
		    "rcmd: %s: %s\n", *ahost, strerror(errno));
		goto bad2;
	}
	if (c != 0) {
		while (read(s, &c, 1) == 1) {
			(void)write(STDERR_FILENO, &c, 1);
			if (c == '\n')
				break;
		}
		goto bad2;
	}
	sigprocmask(SIG_SETMASK, &oldmask, NULL);
	free(readsp);
	return (s);
bad2:
	if (lport)
		(void)close(*fd2p);
bad:
	if (readsp)
		free(readsp);
	(void)close(s);
	sigprocmask(SIG_SETMASK, &oldmask, NULL);
	return (-1);
}

int	__check_rhosts_file = 1;
char	*__rcmd_errstr;

int
ruserok(const char *rhost, int superuser, const char *ruser, const char *luser)
{
	struct addrinfo hints, *res, *r;
	int error;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;	/*dummy*/
	error = getaddrinfo(rhost, "0", &hints, &res);
	if (error)
		return (-1);

	for (r = res; r; r = r->ai_next) {
		if (iruserok_sa(r->ai_addr, r->ai_addrlen, superuser, ruser,
		    luser) == 0) {
			freeaddrinfo(res);
			return (0);
		}
	}
	freeaddrinfo(res);
	return (-1);
}

/*
 * New .rhosts strategy: We are passed an ip address. We spin through
 * hosts.equiv and .rhosts looking for a match. When the .rhosts only
 * has ip addresses, we don't have to trust a nameserver.  When it
 * contains hostnames, we spin through the list of addresses the nameserver
 * gives us and look for a match.
 *
 * Returns 0 if ok, -1 if not ok.
 */
int
iruserok(u_int32_t raddr, int superuser, const char *ruser, const char *luser)
{
	struct sockaddr_in sin;

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_len = sizeof(struct sockaddr_in);
	memcpy(&sin.sin_addr, &raddr, sizeof(sin.sin_addr));
	return iruserok_sa(&sin, sizeof(struct sockaddr_in), superuser, ruser,
		    luser);
}

int
iruserok_sa(const void *raddr, int rlen, int superuser, const char *ruser,
    const char *luser)
{
	struct sockaddr *sa;
	char *cp;
	struct stat sbuf;
	struct passwd *pwd;
	FILE *hostf;
	uid_t uid;
	int first;
	char pbuf[MAXPATHLEN];

	sa = (struct sockaddr *)raddr;
	first = 1;
	hostf = superuser ? NULL : fopen(_PATH_HEQUIV, "r");
again:
	if (hostf) {
		if (__ivaliduser_sa(hostf, sa, rlen, luser, ruser) == 0) {
			(void)fclose(hostf);
			return (0);
		}
		(void)fclose(hostf);
	}
	if (first == 1 && (__check_rhosts_file || superuser)) {
		first = 0;
		if ((pwd = getpwnam(luser)) == NULL)
			return (-1);
		snprintf(pbuf, sizeof pbuf, "%s/.rhosts", pwd->pw_dir);

		/*
		 * Change effective uid while opening .rhosts.  If root and
		 * reading an NFS mounted file system, can't read files that
		 * are protected read/write owner only.
		 */
		uid = geteuid();
		(void)seteuid(pwd->pw_uid);
		hostf = fopen(pbuf, "r");
		(void)seteuid(uid);

		if (hostf == NULL)
			return (-1);
		/*
		 * If not a regular file, or is owned by someone other than
		 * user or root or if writeable by anyone but the owner, quit.
		 */
		cp = NULL;
		if (lstat(pbuf, &sbuf) < 0)
			cp = ".rhosts lstat failed";
		else if (!S_ISREG(sbuf.st_mode))
			cp = ".rhosts not regular file";
		else if (fstat(fileno(hostf), &sbuf) < 0)
			cp = ".rhosts fstat failed";
		else if (sbuf.st_uid && sbuf.st_uid != pwd->pw_uid)
			cp = "bad .rhosts owner";
		else if (sbuf.st_mode & (S_IWGRP|S_IWOTH))
			cp = ".rhosts writable by other than owner";
		/* If there were any problems, quit. */
		if (cp) {
			__rcmd_errstr = cp;
			(void)fclose(hostf);
			return (-1);
		}
		goto again;
	}
	return (-1);
}

/*
 * XXX
 * Don't make static, used by lpd(8).
 *
 * Returns 0 if ok, -1 if not ok.
 */
int
__ivaliduser(FILE *hostf, in_addr_t raddrl, const char *luser,
    const char *ruser)
{
	struct sockaddr_in sin;

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_len = sizeof(struct sockaddr_in);
	memcpy(&sin.sin_addr, &raddrl, sizeof(sin.sin_addr));
	return __ivaliduser_sa(hostf, (struct sockaddr *)&sin, sin.sin_len,
		    luser, ruser);
}

int
__ivaliduser_sa(FILE *hostf, struct sockaddr *raddr, socklen_t salen,
    const char *luser, const char *ruser)
{
	char *user, *p;
	char *buf;
	const char *auser, *ahost;
	int hostok, userok;
	char *rhost = (char *)-1;
	char domain[MAXHOSTNAMELEN];
	size_t buflen;

	getdomainname(domain, sizeof(domain));

	while ((buf = fgetln(hostf, &buflen))) {
		p = buf;
		if (*p == '#')
			continue;
		while (p < buf + buflen && *p != '\n' && *p != ' ' && *p != '\t') {
			if (!isprint(*p))
				goto bail;
			*p = isupper(*p) ? tolower(*p) : *p;
			p++;
		}
		if (p >= buf + buflen)
			continue;
		if (*p == ' ' || *p == '\t') {
			*p++ = '\0';
			while (p < buf + buflen && (*p == ' ' || *p == '\t'))
				p++;
			if (p >= buf + buflen)
				continue;
			user = p;
			while (p < buf + buflen && *p != '\n' && *p != ' ' &&
			    *p != '\t') {
				if (!isprint(*p))
					goto bail;
				p++;
			}
		} else
			user = p;
		*p = '\0';

		if (p == buf)
			continue;

		auser = *user ? user : luser;
		ahost = buf;

		if (strlen(ahost) >= MAXHOSTNAMELEN)
			continue;

		/*
		 * innetgr() must lookup a hostname (we do not attempt
		 * to change the semantics so that netgroups may have
		 * #.#.#.# addresses in the list.)
		 */
		if (ahost[0] == '+')
			switch (ahost[1]) {
			case '\0':
				hostok = 1;
				break;
			case '@':
				if (rhost == (char *)-1)
					rhost = __gethostloop(raddr, salen);
				hostok = 0;
				if (rhost)
					hostok = innetgr(&ahost[2], rhost,
					    NULL, domain);
				break;
			default:
				hostok = __icheckhost(raddr, salen, &ahost[1]);
				break;
			}
		else if (ahost[0] == '-')
			switch (ahost[1]) {
			case '\0':
				hostok = -1;
				break;
			case '@':
				if (rhost == (char *)-1)
					rhost = __gethostloop(raddr, salen);
				hostok = 0;
				if (rhost)
					hostok = -innetgr(&ahost[2], rhost,
					    NULL, domain);
				break;
			default:
				hostok = -__icheckhost(raddr, salen, &ahost[1]);
				break;
			}
		else
			hostok = __icheckhost(raddr, salen, ahost);


		if (auser[0] == '+')
			switch (auser[1]) {
			case '\0':
				userok = 1;
				break;
			case '@':
				userok = innetgr(&auser[2], NULL, ruser,
				    domain);
				break;
			default:
				userok = strcmp(ruser, &auser[1]) ? 0 : 1;
				break;
			}
		else if (auser[0] == '-')
			switch (auser[1]) {
			case '\0':
				userok = -1;
				break;
			case '@':
				userok = -innetgr(&auser[2], NULL, ruser,
				    domain);
				break;
			default:
				userok = strcmp(ruser, &auser[1]) ? 0 : -1;
				break;
			}
		else
			userok = strcmp(ruser, auser) ? 0 : 1;

		/* Check if one component did not match */
		if (hostok == 0 || userok == 0)
			continue;

		/* Check if we got a forbidden pair */
		if (userok <= -1 || hostok <= -1)
			return (-1);

		/* Check if we got a valid pair */
		if (hostok >= 1 && userok >= 1)
			return (0);
	}
bail:
	return (-1);
}

/*
 * Returns "true" if match, 0 if no match.  If we do not find any
 * semblance of an A->PTR->A loop, allow a simple #.#.#.# match to work.
 */
static int
__icheckhost(struct sockaddr *raddr, socklen_t salen, const char *lhost)
{
	struct addrinfo hints, *res, *r;
	char h1[NI_MAXHOST], h2[NI_MAXHOST];
	int error;
	const int niflags = NI_NUMERICHOST;

	h1[0] = '\0';
	if (getnameinfo(raddr, salen, h1, sizeof(h1), NULL, 0,
	    niflags) != 0)
		return (0);

	/* Resolve laddr into sockaddr */
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = raddr->sa_family;
	hints.ai_socktype = SOCK_DGRAM;	/*dummy*/
	res = NULL;
	error = getaddrinfo(lhost, "0", &hints, &res);
	if (error)
		return (0);

	/*
	 * Try string comparisons between raddr and laddr.
	 */
	for (r = res; r; r = r->ai_next) {
		h2[0] = '\0';
		if (getnameinfo(r->ai_addr, r->ai_addrlen, h2, sizeof(h2),
		    NULL, 0, niflags) != 0)
			continue;
		if (strcmp(h1, h2) == 0) {
			freeaddrinfo(res);
			return (1);
		}
	}

	/* No match. */
	freeaddrinfo(res);
	return (0);
}

/*
 * Return the hostname associated with the supplied address.
 * Do a reverse lookup as well for security. If a loop cannot
 * be found, pack the result of inet_ntoa() into the string.
 */
static char *
__gethostloop(struct sockaddr *raddr, socklen_t salen)
{
	static char remotehost[NI_MAXHOST];
	char h1[NI_MAXHOST], h2[NI_MAXHOST];
	struct addrinfo hints, *res, *r;
	int error;
	const int niflags = NI_NUMERICHOST;

	h1[0] = remotehost[0] = '\0';
	if (getnameinfo(raddr, salen, remotehost, sizeof(remotehost),
	    NULL, 0, NI_NAMEREQD) != 0)
		return (NULL);
	if (getnameinfo(raddr, salen, h1, sizeof(h1), NULL, 0,
	    niflags) != 0)
		return (NULL);

	/*
	 * Look up the name and check that the supplied
	 * address is in the list
	 */
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = raddr->sa_family;
	hints.ai_socktype = SOCK_DGRAM;	/*dummy*/
	hints.ai_flags = AI_CANONNAME;
	res = NULL;
	error = getaddrinfo(remotehost, "0", &hints, &res);
	if (error)
		return (NULL);

	for (r = res; r; r = r->ai_next) {
		h2[0] = '\0';
		if (getnameinfo(r->ai_addr, r->ai_addrlen, h2, sizeof(h2),
		    NULL, 0, niflags) != 0)
			continue;
		if (strcmp(h1, h2) == 0) {
			freeaddrinfo(res);
			return (remotehost);
		}
	}

	/*
	 * either the DNS adminstrator has made a configuration
	 * mistake, or someone has attempted to spoof us
	 */
	syslog(LOG_NOTICE, "rcmd: address %s not listed for host %s",
	    h1, res->ai_canonname ? res->ai_canonname : remotehost);
	freeaddrinfo(res);
	return (NULL);
}
