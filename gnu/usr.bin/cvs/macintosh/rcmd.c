/*	$NetBSD: rcmd.c,v 1.12 1995/06/03 22:33:34 mycroft Exp $	*/

/*
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

#include "mac_config.h"
#if defined(LIBC_SCCS) && !defined(lint)
#if 0
static char sccsid[] = "@(#)rcmd.c	8.3 (Berkeley) 3/26/94";
#else
static char *rcsid = "$NetBSD: rcmd.c,v 1.12 1995/06/03 22:33:34 mycroft Exp $";
#endif
#endif /* LIBC_SCCS and not lint */

#define MAXPATHLEN 1024
#ifndef MAXHOSTNAMELEN
#define	MAXHOSTNAMELEN	256
#endif

#include <GUSI.h>
#include <sys/errno.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include <netinet/in.h>

#include <signal.h>
#include <fcntl.h>
#include <netdb.h>
#include <unistd.h>
#include <pwd.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>

int
rcmd(ahost, rport, locuser, remuser, cmd, fd2p)
	char **ahost;
	u_short rport;
	const char *locuser, *remuser, *cmd;
	int *fd2p;
{
	struct hostent *hp;
	struct sockaddr_in sin, from;
	fd_set reads;
	long oldmask;
	pid_t pid;
	int s, lport, timo;
	char c;

	pid = getpid();
	hp = gethostbyname(*ahost);
	if (hp == NULL) {
		/*herror(*ahost);*/
		(void)fprintf(stderr,
				    "rcmd: socket: gethostbyname failed for %s\n", *ahost);
		return (-1);
	}
	*ahost = hp->h_name;
	/* oldmask = sigblock(sigmask(SIGURG)); */
	for (timo = 1, lport = IPPORT_RESERVED - 1;;) {
		s = rresvport(&lport);
		if (s < 0) {
			if (errno == EDEADLK) /* EDEADLK */
				(void)fprintf(stderr,
				    "rcmd: socket: All ports in use\n");
			else
				(void)fprintf(stderr, "rcmd: socket: %s\n",
				    strerror(errno));
			/* sigsetmask(oldmask); */
			return (-1);
		}
		fcntl(s, F_SETOWN, pid);
		sin.sin_len = sizeof(struct sockaddr_in);
		sin.sin_family = hp->h_addrtype;
		sin.sin_port = rport;
		bcopy(hp->h_addr_list[0], &sin.sin_addr, hp->h_length);
		if (connect(s, (struct sockaddr *)&sin, sizeof(sin)) >= 0)
			break;
		(void)close(s);
		if (errno == EADDRINUSE) {
			lport--;
			continue;
		}
		if (errno == ECONNREFUSED && timo <= 16) {
			(void)sleep(timo);
			timo *= 2;
			continue;
		}
		if (hp->h_addr_list[1] != NULL) {
			int oerrno = errno;

			(void)fprintf(stderr, "connect to address %s: ",
			    inet_ntoa(sin.sin_addr));
			errno = oerrno;
			perror(0);
			hp->h_addr_list++;
			bcopy(hp->h_addr_list[0], &sin.sin_addr, hp->h_length);
			(void)fprintf(stderr, "Trying %s...\n",
			    inet_ntoa(sin.sin_addr));
			continue;
		}
		(void)fprintf(stderr, "%s: %s\n", hp->h_name, strerror(errno));
		/* sigsetmask(oldmask); */
		return (-1);
	}
	lport--;
	if (fd2p == 0) {
		write(s, "", 1);
		lport = 0;
	} else {
		char num[8];
		int s2 = rresvport(&lport), s3;
		int len = sizeof(from);
		if (s2 < 0)
			goto bad;
		listen(s2, 1);
		/*(void)snprintf(num, sizeof(num), "%d", lport);*/
		sprintf(num,"%d", lport);
		if (write(s, num, strlen(num)+1) != strlen(num)+1) {
			(void)fprintf(stderr,
			    "rcmd: write (setting up stderr): %s\n",
			    strerror(errno));
			(void)close(s2);
			goto bad;
		}
		FD_ZERO(&reads);
		FD_SET(s, &reads);
		FD_SET(s2, &reads);
		errno = 0;
		if (select(MAX(s, s2) + 1, &reads, 0, 0, 0) < 1 ||
		    !FD_ISSET(s2, &reads)) {
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
		(void)close(s2);
		if (s3 < 0) {
			(void)fprintf(stderr,
			    "rcmd: accept: %s\n", strerror(errno));
			lport = 0;
			goto bad;
		}
		*fd2p = s3;
		from.sin_port = ntohs(from.sin_port);
		if (from.sin_family != AF_INET ||
		    from.sin_port >= IPPORT_RESERVED ||
		    from.sin_port < IPPORT_RESERVED / 2) {
			(void)fprintf(stderr,
			    "socket: protocol failure in circuit setup.\n");
			goto bad2;
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
	/* sigsetmask(oldmask); */
	return (s);
bad2:
	if (lport)
		(void)close(*fd2p);
bad:
	(void)close(s);
	/* sigsetmask(oldmask); */
	return (-1);
}

int
rresvport(alport)
	int *alport;
{
	struct sockaddr_in sin;
	int s;

	sin.sin_len = sizeof(struct sockaddr_in);
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = INADDR_ANY;
	s = socket(AF_INET, SOCK_STREAM, 0);
	if (s < 0)
		return (-1);
	for (;;) {
		sin.sin_port = htons((u_short)*alport);
		if (bind(s, (struct sockaddr *)&sin, sizeof(sin)) >= 0)
			return (s);
		if (errno != EADDRINUSE) {
			(void)close(s);
			(void)fprintf(stderr, "rresvport: bind failed with %d\n", errno);
			return (-1);
		}
		(*alport)--;
		if (*alport == IPPORT_RESERVED/2) {
			(void)close(s);
			errno = EDEADLK;		/* close */
			(void)fprintf(stderr, "rresvport: bind failed with EDEADLCK\n");
			return (-1);
		}
	}
}

