/*	$OpenBSD: altqd.c,v 1.7 2002/02/13 08:23:04 kjc Exp $	*/
/*	$KAME: altqd.c,v 1.9 2002/02/12 10:12:15 kjc Exp $	*/
/*
 * Copyright (c) 2001 Theo de Raadt
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Copyright (C) 1997-2002
 *	Sony Computer Science Laboratories, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY SONY CSL AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL SONY CSL OR CONTRIBUTORS BE LIABLE
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
#include <sys/un.h>
#include <sys/stat.h>
#include <net/if.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <syslog.h>
#include <err.h>

#include <altq/altq.h>
#include "altq_qop.h"
#include "quip_server.h"

#define MAX_CLIENT		10

volatile sig_atomic_t gotsig_hup, gotsig_int, gotsig_term;

void usage(void);
void sig_pipe(int);
void sig_hup(int);
void sig_int(int);
void sig_term(int);

void
usage(void)
{
	fprintf(stderr, "usage: altqd [-vd] [-f config]\n");
	exit(1);
}

void
sig_pipe(int sig)
{
	/*
	 * we have lost an API connection.
	 * a subsequent output operation will catch EPIPE.
	 */
}

void
sig_hup(int sig)
{
	gotsig_hup = 1;
}

void
sig_int(int sig)
{
	gotsig_int = 1;
}

void
sig_term(int sig)
{
	gotsig_term = 1;
}

int
main(int argc, char **argv)
{
	int	i, c, maxfd, rval, qpsock;
	fd_set	fds, rfds;
	FILE	*fp, *client[MAX_CLIENT];

	m_debug = 0;
	l_debug = LOG_INFO;
	fp = NULL;
	for (i = 0; i < MAX_CLIENT; i++)
		client[i] = NULL;

	while ((c = getopt(argc, argv, "f:vDdl:")) != -1) {
		switch (c) {
		case 'f':
			altqconfigfile = optarg;
			break;
		case 'D':	/* -D => dummy mode */
			Debug_mode = 1;
			printf("Debug mode set.\n");
			break;
		case 'v':
			l_debug = LOG_DEBUG;
			m_debug |= DEBUG_ALTQ;
			daemonize = 0;
			break;
		case 'd':
			daemonize = 0;
			break;
		case 'l':
			l_debug = atoi(optarg);
			break;
		default:
			usage();
		}
	}

	signal(SIGINT, sig_int);
	signal(SIGTERM, sig_term);
	signal(SIGHUP, sig_hup);
	signal(SIGPIPE, sig_pipe);

	if (daemonize)
		openlog("altqd", LOG_PID, LOG_DAEMON);

	if (qcmd_init() != 0) {
		if (daemonize)
			closelog();
		exit(1);
	}

	/*
	 * open a unix domain socket for altqd clients
	 */
	if ((qpsock = socket(AF_LOCAL, SOCK_STREAM, 0)) < 0)
		LOG(LOG_ERR, errno, "can't open unix domain socket");
	else {
		struct sockaddr_un addr;

		bzero(&addr, sizeof(addr));
		addr.sun_family = AF_LOCAL;
		strlcpy(addr.sun_path, QUIP_PATH, sizeof(addr.sun_path));
		unlink(QUIP_PATH);
		if (bind(qpsock, (struct sockaddr *)&addr,
		    sizeof(addr)) < 0) {
			LOG(LOG_ERR, errno, "can't bind to %s", QUIP_PATH);
			close(qpsock);
			qpsock = -1;
		}
		chmod(QUIP_PATH, 0666);
		if (listen(qpsock, SOMAXCONN) < 0) {
			LOG(LOG_ERR, errno, "can't listen to %s", QUIP_PATH);
			close(qpsock);
			qpsock = -1;
		}
	}

	if (daemonize) {
		daemon(0, 0);

		/* save pid to the pid file (/var/tmp/altqd.pid) */
		if (pidfile(NULL))
			LOG(LOG_WARNING, errno, "can't open pid file");
	} else {
		/* interactive mode */
		fp = stdin;
		printf("\nEnter ? or command:\n");
		printf("altqd %s> ", cur_ifname());
		fflush(stdout);
	}

	/*
	 * go into the command mode.
	 */
	FD_ZERO(&fds);
	maxfd = 0;
	if (fp != NULL) {
		FD_SET(fileno(fp), &fds);
		maxfd = MAX(maxfd, fileno(fp) + 1);
	}
	if (qpsock >= 0) {
		FD_SET(qpsock, &fds);
		maxfd = MAX(maxfd, qpsock + 1);
	}

	rval = 1;
	while (rval) {
		if (gotsig_hup) {
			qcmd_destroyall();
			gotsig_hup = 0;
			LOG(LOG_INFO, 0, "reinitializing altqd...");
			if (qcmd_init() != 0) {
				LOG(LOG_INFO, 0, "reinitialization failed");
				break;
			}
		}
		if (gotsig_term || gotsig_int) {
			LOG(LOG_INFO, 0, "Exiting on signal %d",
			    gotsig_term ? SIGTERM : SIGINT);
			break;
		}

		FD_COPY(&fds, &rfds);
		if (select(maxfd, &rfds, NULL, NULL, NULL) < 0) {
			if (errno != EINTR)
				err(1, "select");
			continue;
		}

		/*
		 * if there is command input, read the input line,
		 * parse it, and execute.
		 */
		if (fp && FD_ISSET(fileno(fp), &rfds)) {
			rval = do_command(fp);
			if (rval == 0) {
				/* quit command or eof on input */
				LOG(LOG_INFO, 0, "Exiting.");
			} else if (fp == stdin)
				printf("altqd %s> ", cur_ifname());
			fflush(stdout);
		} else if (qpsock >= 0 && FD_ISSET(qpsock, &rfds)) {
			/*
			 * quip connection request from client via unix
			 * domain socket; get a new socket for this
			 * connection and add it to the select list.
			 */
			int newsock = accept(qpsock, NULL, NULL);

			if (newsock == -1) {
				LOG(LOG_ERR, errno, "accept");
				continue;
			}
			FD_SET(newsock, &fds);
			for (i = 0; i < MAX_CLIENT; i++)
				if (client[i] == NULL) {
					client[i] = fdopen(newsock, "r+");
					break;
				}
			maxfd = MAX(maxfd, newsock + 1);
		} else {
			/*
			 * check input from a client via unix domain socket
			 */
			for (i = 0; i < MAX_CLIENT; i++) {
				int fd;

				if (client[i] == NULL)
					continue;
				fd = fileno(client[i]);
				if (FD_ISSET(fd, &rfds)) {
					if (quip_input(client[i]) != 0 ||
					    fflush(client[i]) != 0) {
						/* connection closed */
						fclose(client[i]);
						client[i] = NULL;
						FD_CLR(fd, &fds);
					}
				}
			}
		}
	}

	/* cleanup and exit */
	qcmd_destroyall();
	if (qpsock >= 0)
		(void)close(qpsock);
	unlink(QUIP_PATH);

	for (i = 0; i < MAX_CLIENT; i++)
		if (client[i] != NULL)
			(void)fclose(client[i]);
	if (daemonize) {
		closelog();
	}
	exit(0);
}
