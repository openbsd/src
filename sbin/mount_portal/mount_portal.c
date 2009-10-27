/*	$OpenBSD: mount_portal.c,v 1.29 2009/10/27 23:59:33 deraadt Exp $	*/
/*	$NetBSD: mount_portal.c,v 1.8 1996/04/13 01:31:54 jtc Exp $	*/

/*
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software donated to Berkeley by
 * Jan-Simon Pendry.
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
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/syslog.h>
#include <sys/mount.h>

#include <err.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mntopts.h"
#include "pathnames.h"
#include "portald.h"

const struct mntopt mopts[] = {
	MOPT_STDOPTS,
	{ NULL }
};

extern char *__progname;	/* from crt0.o */

static char mountpt[MAXPATHLEN];	/* made available to signal handler */

static void usage(void);

static volatile sig_atomic_t readcf;	/* Set when SIGHUP received */

static void
sigchld(int sig)
{
	int save_errno = errno;
	struct syslog_data sdata = SYSLOG_DATA_INIT;
	pid_t pid;

	while ((pid = waitpid((pid_t) -1, NULL, WNOHANG)) > 0)
		;
	if (pid < 0 && errno != ECHILD)
		syslog_r(LOG_WARNING, &sdata, "waitpid: %m");
	errno = save_errno;
}

static void
sighup(int sig)
{

	readcf = 1;
}

static void
sigterm(int sig)
{
	struct syslog_data sdata = SYSLOG_DATA_INIT;

	if (unmount(mountpt, MNT_FORCE) < 0)
		syslog_r(LOG_WARNING, &sdata,
		    "sigterm: unmounting %s failed: %m", mountpt);
	_exit(1);
}

int
main(int argc, char *argv[])
{
	struct portal_args args;
	struct sockaddr_un un;
	char *conf;
	int mntflags = 0;
	char tag[32];
	fd_set *fdsp;
	int fdssize;

	qelem q;
	int so;
	int error = 0;

	/*
	 * Crack command line args
	 */
	int ch;

	while ((ch = getopt(argc, argv, "o:")) != -1) {
		switch (ch) {
		case 'o':
			getmntopts(optarg, mopts, &mntflags);
			break;
		default:
			error = 1;
			break;
		}
	}

	if (optind != (argc - 2))
		error = 1;

	if (error)
		usage();

	/*
	 * Get config file and mount point
	 */
	conf = argv[optind];
	if (realpath(argv[optind+1], mountpt) == NULL)
		err(1, "realpath %s", argv[optind+1]);

	/*
	 * Construct the listening socket
	 */
	un.sun_family = AF_UNIX;
	if (sizeof(_PATH_TMPPORTAL) >= sizeof(un.sun_path))
		errx(1, "portal socket name too long");
	(void)strlcpy(un.sun_path, _PATH_TMPPORTAL, sizeof un.sun_path);
	so = mkstemp(un.sun_path);
	if (so < 0)
		err(1, "can't create portal socket name: %s", un.sun_path);
	un.sun_len = strlen(un.sun_path);
	(void)close(so);

	so = socket(AF_UNIX, SOCK_STREAM, 0);
	if (so < 0)
		err(1, "socket(2)");

	(void)unlink(un.sun_path);
	/* XXX teeny race? */
	if (bind(so, (struct sockaddr *) &un, sizeof(un)) < 0)
		err(1, "bind(2)");

	(void)listen(so, 5);

	args.pa_socket = so;

	/*
	 * Must fork before mount to get pid in name right.
	 */
	daemon(0, 0);

	(void)snprintf(tag, sizeof(tag), "portal:%ld", (long)getpid());
	args.pa_config = tag;

	/*
	 * Start logging (and change name)
	 */
	openlog("portald", LOG_CONS|LOG_PID, LOG_DAEMON);

	if (mount(MOUNT_PORTAL, mountpt, mntflags, &args)) {
		if (errno == EOPNOTSUPP)
			syslog(LOG_ERR,
			    "mount: Filesystem not supported by kernel");
		else
			syslog(LOG_ERR, "mount: %m");
		exit(1);
	}

	q.q_forw = q.q_back = &q;
	readcf = 1;

	(void)signal(SIGCHLD, sigchld);
	(void)signal(SIGHUP, sighup);
	(void)signal(SIGTERM, sigterm);

	fdssize = howmany(so+1, NFDBITS) * sizeof(fd_mask);
	fdsp = (fd_set *)malloc(fdssize);
	if (fdsp == NULL)
		err(1, "malloc");

	/*
	 * Just loop waiting for new connections and activating them
	 */
	for (;;) {
		struct sockaddr_un un2;
		socklen_t salen = sizeof(un2);
		int so2;
		pid_t pid;
		int rc;

		/*
		 * Check whether we need to re-read the configuration file
		 */
		if (readcf) {
			readcf = 0;
			conf_read(&q, conf);
			continue;
		}

		/*
		 * Accept a new connection
		 * Will get EINTR if a signal has arrived, so just
		 * ignore that error code
		 */
		memset(fdsp, 0, fdssize);
		FD_SET(so, fdsp);
		rc = select(so+1, fdsp, NULL, NULL, NULL);
		if (rc < 0) {
			if (errno == EINTR)
				continue;
			syslog(LOG_ERR, "select: %m");
			exit(1);
		}
		if (rc == 0)
			break;
		so2 = accept(so, (struct sockaddr *) &un2, &salen);
		if (so2 < 0) {
			/*
			 * The unmount function does a shutdown on the socket
			 * which will generated ECONNABORTED on the accept.
			 */
			if (errno == ECONNABORTED)
				break;
			if (errno != EINTR) {
				syslog(LOG_ERR, "accept: %m");
				exit(1);
			}
			continue;
		}

		/*
		 * Now fork a new child to deal with the connection
		 */
	eagain:;
		switch (pid = fork()) {
		case -1:
			if (errno == EAGAIN) {
				sleep(1);
				goto eagain;
			}
			syslog(LOG_ERR, "fork: %m");
			break;
		case 0:
			(void)close(so);
			activate(&q, so2);
			exit(0);
		default:
			(void)close(so2);
			break;
		}
	}
	free(fdsp);
	syslog(LOG_INFO, "%s unmounted", mountpt);
	exit(0);
}

static void
usage(void)
{
	(void)fprintf(stderr,
		"usage: %s [-o options] config mount_point\n", __progname);
	exit(1);
}
