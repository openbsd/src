/*	$OpenBSD: child.c,v 1.1 2005/02/22 22:33:01 jfb Exp $	*/
/*
 * Copyright (c) 2004 Jean-Francois Brousseau <jfb@openbsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL  DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * cvsd-child
 * ----------
 *
 * This is the process taking care of cvs(1) repository requests
 * This program is not meant to be run standalone and should only be started
 * by the cvsd(8) process.
 *
 */

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/uio.h>

#include <err.h>
#include <pwd.h>
#include <grp.h>
#include <poll.h>
#include <fcntl.h>
#include <dirent.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <sysexits.h>

#include "log.h"
#include "cvs.h"
#include "cvsd.h"
#include "cvspr.h"



extern char *__progname;


int    cvsd_fg = 0;

volatile sig_atomic_t cvsd_running = 1;

static int    cvsd_privfd = -1;
static char   cvsd_root[MAXPATHLEN];
static char  *cvsd_motd;
static uid_t  cvsd_uid = -1;
static gid_t  cvsd_gid = -1;


/* session info */
static uid_t  cvsd_sess_ruid = 0;	/* UID of the cvs issuing requests */
static gid_t  cvsd_sess_rgid = 0;	/* UID of the cvs issuing requests */
static int    cvsd_sess_fd = -1;


void   usage         (void);
void   cvsd_sighdlr  (int);
int    cvsd_child_getreq (struct cvsd_req *);


/*
 * cvsd_sighdlr()
 *
 * Generic signal handler.
 */
void
cvsd_sighdlr(int signo)
{
	switch (signo) {
	case SIGINT:
	case SIGTERM:
	case SIGQUIT:
		cvsd_running = 0;
		break;
	}
}


/*
 * usage()
 *
 * Display program usage.
 */
void
usage(void)
{
	fprintf(stderr,
	    "Usage: %s [-dfhv] [-g group] "
	    "[-u user]\n"
	    "\t-d\t\tStart the server in debugging mode (very verbose)\n"
	    "\t-u user\t\tUse user <user> for privilege revocation\n"
	    "\t-v\t\tBe verbose\n",
	    __progname);
}


int
main(int argc, char **argv)
{
	int ret;
	struct cvsd_req req;

	if (cvs_log_init(LD_STD|LD_SYSLOG, LF_PID) < 0)
		err(1, "failed to initialize logging mechanism");

	cvsd_sess_fd = CVSD_CHILD_SOCKFD;
	if (getpeereid(cvsd_sess_fd, &cvsd_sess_ruid, &cvsd_sess_rgid) == -1) {
		cvs_log(LP_ERRNO, "failed to get remote credentials");
		exit(EX_OSERR);
	}

	while ((ret = getopt(argc, argv, "dfg:hr:u:v")) != -1) {
		switch (ret) {
		case 'd':
			cvs_log_filter(LP_FILTER_UNSET, LP_DEBUG);
			cvs_log_filter(LP_FILTER_UNSET, LP_INFO);
			break;
		case 'f':
			cvsd_fg = 1;
			break;
		case 'g':
			cvsd_gid = atoi(optarg);
			break;
		case 'h':
			usage();
			exit(0);
			/* NOTREACHED */
			break;
		case 'r':
			strlcpy(cvsd_root, optarg, sizeof(cvsd_root));
			break;
		case 'u':
			cvsd_uid = atoi(optarg);
			break;
		case 'v':
			cvs_log_filter(LP_FILTER_UNSET, LP_INFO);
			break;
		default:
			usage();
			exit(EX_USAGE);
		}
	}

	argc -= optind;
	argv += optind;
	if (argc > 0)
		errx(EX_USAGE, "unrecognized trailing arguments");

	/* Before getting any further, chroot to the CVS repository's root
	 * directory and drop all privileges to the appropriate user and
	 * group so we can't cause damage outside of the CVS data.
	 */
	if (chroot(cvsd_root) == -1) {
		cvs_log(LP_ERRNO, "failed to chroot to %s", cvsd_root);
		exit(EX_OSERR);
	}
	(void)chdir("/");
	cvs_log(LP_INFO, "dropping privileges to %d:%d", cvsd_uid, cvsd_gid);
	if (setgid(cvsd_gid) == -1) {
		cvs_log(LP_ERRNO, "failed to drop group privileges to %s",
		    CVSD_GROUP);
		return (-1);
	}

	if (setuid(cvsd_uid) == -1) {
		cvs_log(LP_ERRNO, "failed to drop user privileges to %s",
		    CVSD_USER);
		return (-1);
	}

	signal(SIGINT, cvsd_sighdlr);
	signal(SIGQUIT, cvsd_sighdlr);
	signal(SIGTERM, cvsd_sighdlr);
	signal(SIGPIPE, SIG_IGN);

	setproctitle("%s [child %d]", __progname, getpid());

	for (;;) {
		ret = cvsd_child_getreq(&req);
		if (ret <= 0)
			break;

		switch (req.cr_op) {
		case CVS_OP_DIFF:
		case CVS_OP_UPDATE:
		default:
		}
		printf("request ID: %d, nfiles = %d\n", req.cr_op,
		    req.cr_nfiles);
	}

	close(cvsd_sess_fd);

	cvs_log_cleanup();

	return (0);
}


/*
 * cvsd_child_getreq()
 *
 * Read the next request available on the session socket.
 * Returns 1 if a request was received, 0 if there are no more requests to
 * serve, and -1 in case of failure.
 */
int
cvsd_child_getreq(struct cvsd_req *reqp)
{
	ssize_t ret;
	if ((ret = read(cvsd_sess_fd, reqp, sizeof(*reqp))) == -1) {
		cvs_log(LP_ERRNO, "failed to read request");
	} else if (ret > 0) {
		printf("reqlen = %d\n", ret);
		ret = 1;
	}

	return ((int)ret);
}


/*
 * cvsd_child_reqhdlr()
 *
 */
int
cvsd_child_reqhdlr(struct cvsp_req *req)
{
	int ret;

	switch (req->req_code) {
	case CVSP_REQ_MOTD:
		ret = cvs_proto_sendresp(cvsd_sess_fd, CVSP_RESP_DONE,
		    cvsd_motd, strlen(cvsd_motd) + 1);
		break;
	case CVSP_REQ_VERSION:
	case CVSP_REQ_GETMSG:
	case CVSP_REQ_SETMSG:
	default:
		ret = cvs_proto_sendresp(cvsd_sess_fd, CVSP_RESP_INVREQ,
		    req->req_seq, NULL, 0);
	}

	return (ret);
}
