/*	$OpenBSD: altqd.c,v 1.1.1.1 2001/06/27 18:23:17 kjc Exp $	*/
/*	$KAME: altqd.c,v 1.2 2000/10/18 09:15:15 kjc Exp $	*/
/*
 * Copyright (C) 1997-2000
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
/*******************************************************************

  Copyright (c) 1996 by the University of Southern California
  All rights reserved.

  Permission to use, copy, modify, and distribute this software and its
  documentation in source and binary forms for any purpose and without
  fee is hereby granted, provided that both the above copyright notice
  and this permission notice appear in all copies. and that any
  documentation, advertising materials, and other materials related to
  such distribution and use acknowledge that the software was developed
  in part by the University of Southern California, Information
  Sciences Institute.  The name of the University may not be used to
  endorse or promote products derived from this software without
  specific prior written permission.

  THE UNIVERSITY OF SOUTHERN CALIFORNIA makes no representations about
  the suitability of this software for any purpose.  THIS SOFTWARE IS
  PROVIDED "AS IS" AND WITHOUT ANY EXPRESS OR IMPLIED WARRANTIES,
  INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.

  Other copyrights might apply to parts of this software and are so
  noted when applicable.

********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <syslog.h>
#include <err.h>

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>

#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <altq/altq.h>
#include "altq_qop.h"
#include "quip_server.h"

#define  ALTQD_PID_FILE	  "/var/run/altqd.pid"

static int altqd_socket = -1;
#define MAX_CLIENT		10
static FILE *client[MAX_CLIENT];

/* for command mode */
int             T;  		/* Current Thread number */
FILE		*infp;		/* Input file pointer */
char           *infile = NULL;  /* command input file.  stdin if NULL. */
fd_set		fds, t_fds;

#define  DEFAULT_DEBUG_MASK	0	
#define  DEFAULT_LOGGING_LEVEL	LOG_INFO

static void usage(void)
{
	fprintf(stderr, "usage: altqd [options]\n");
	fprintf(stderr, "    options:\n");
	fprintf(stderr, "    -f config_file	: set config file\n");
	fprintf(stderr, "    -v			: verbose (no daemonize)\n");
	fprintf(stderr, "    -d			: debug (no daemonize)\n");
}

static void
sig_handler(int sig)
{
	if (sig == SIGPIPE) {
		/*
		 * we have lost an API connection.
		 * a subsequent output operation will catch EPIPE.
		 */
		return;
	}

	qcmd_destroyall();

	if (sig == SIGHUP) {
		printf("reinitializing altqd...\n");
		qcmd_init();
		return;
	}

	fprintf(stderr, "Exiting on signal %d\n", sig);

	/* if we have a pid file, remove it */
	if (daemonize) {
		unlink(ALTQD_PID_FILE);
		closelog();
	}

	if (altqd_socket >= 0) {
		close(altqd_socket);
		altqd_socket = -1;
	}

	exit(0);
}

int main(int argc, char **argv)
{
	int		c;
	int		i, maxfd;
	extern char	*optarg;

	m_debug = DEFAULT_DEBUG_MASK;
	l_debug = DEFAULT_LOGGING_LEVEL;

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

	signal(SIGINT, sig_handler);
	signal(SIGTERM, sig_handler);
	signal(SIGHUP, sig_handler);
	signal(SIGPIPE, sig_handler);

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
	for (i = 0; i < MAX_CLIENT; i++)
		client[i] = NULL;
	if ((altqd_socket = socket(AF_LOCAL, SOCK_STREAM, 0)) < 0)
		LOG(LOG_ERR, errno, "can't open unix domain socket\n");
	else {
		struct sockaddr_un addr;

		unlink(QUIP_PATH);
		bzero(&addr, sizeof(addr));
		addr.sun_family = AF_LOCAL;
		strcpy(addr.sun_path, QUIP_PATH);
		if (bind(altqd_socket, (struct sockaddr *)&addr,
			 sizeof(addr)) < 0) {
			LOG(LOG_ERR, errno, "can't bind to %s\n",
			    QUIP_PATH);
			altqd_socket = -1;
		}
		chmod(QUIP_PATH, 0666);
		if (listen(altqd_socket, SOMAXCONN) < 0) {
			LOG(LOG_ERR, errno, "can't listen to %s\n",
			    QUIP_PATH);
			altqd_socket = -1;
		}
	}

	if (daemonize) {
		FILE *fp;

		daemon(0, 0);

		/* save pid to the pid file (/var/tmp/altqd.pid) */
		if ((fp = fopen(ALTQD_PID_FILE, "w")) != NULL) {
			fprintf(fp, "%d\n", getpid());
			fclose(fp);
		}
		else
			warn("can't open pid file: %s: %s",
			     ALTQD_PID_FILE, strerror(errno));
	} else {
		/* interactive mode */
		if (infile) {
			if (NULL == (infp = fopen(infile, "r"))) {
				perror("Cannot open input file");
				exit(1);
			}
		} else {
			infp = stdin;
			printf("\nEnter ? or command:\n");
			printf("altqd %s> ", cur_ifname());
		}
		fflush(stdout);
	}

	/*
	 * go into the command mode.
	 * the code below is taken from rtap of rsvpd.
	 */
	FD_ZERO(&fds);
	maxfd = 0;
	if (infp != NULL) {
		FD_SET(fileno(infp), &fds);
		maxfd = MAX(maxfd, fileno(infp) + 1);
	}
	if (altqd_socket >= 0) {
		FD_SET(altqd_socket, &fds);
		maxfd = MAX(maxfd, altqd_socket + 1);
	}
	while (1) {
		int rc;

		FD_COPY(&fds, &t_fds);
		rc = select(maxfd, &t_fds, NULL, NULL, NULL);
		if (rc < 0) {
			if (errno != EINTR) {	
				perror("select");
				exit(1);
			}
			continue;
		}
		
		/*
		 * If there is control input, read the input line,
		 * parse it, and execute.
		 */
		if (infp != NULL && FD_ISSET(fileno(infp), &t_fds)) {
			rc = DoCommand(infile, infp);
			if (rc == 0) {
				/*
				 * EOF on input.  If reading from file,
				 * go to stdin; else exit.
				 */
				if (infile) {
					infp = stdin;
					infile = NULL;
					printf("\nEnter ? or command:\n");	
				FD_SET(fileno(infp), &fds);
				} else {
					LOG(LOG_INFO, 0, "Exiting.\n");
					(void) qcmd_destroyall();
					exit(0);
				}
			} else if (infp == stdin)
				printf("altqd %s> ", cur_ifname());
			fflush(stdout);
		} else if (altqd_socket >= 0 && FD_ISSET(altqd_socket, &t_fds)) {
			/*
			 * quip connection request from client via unix
			 * domain socket; get a new socket for this
			 * connection and add it to the select list.
			 */
			int newsock = accept(altqd_socket, NULL, NULL);
			if (newsock == -1) {
				LOG(LOG_ERR, errno, "accept error\n");
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
			for (i = 0; i <= MAX_CLIENT; i++) {
				int fd;

				if (client[i] == NULL)
					continue;
				fd = fileno(client[i]);
				if (FD_ISSET(fd, &t_fds)) {
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
}
