/*	$OpenBSD: cvsd.c,v 1.2 2004/07/16 01:46:16 jfb Exp $	*/
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
#include "sock.h"
#include "cvs.h"
#include "cvsd.h"


static void  cvsd_parent_loop (void);
static void  cvsd_child_loop  (void);
static int   cvsd_privdrop    (void);


extern char *__progname;



int foreground = 0;

volatile sig_atomic_t restart = 0;


uid_t cvsd_uid = -1;
gid_t cvsd_gid = -1;


char *cvsd_root = NULL;
char *cvsd_aclfile = NULL;


static int    cvsd_privfd = -1;



static TAILQ_HEAD(,cvsd_child) cvsd_children;
static volatile sig_atomic_t   cvsd_chnum = 0;
static volatile sig_atomic_t   cvsd_chmin = CVSD_CHILD_DEFMIN;
static volatile sig_atomic_t   cvsd_chmax = CVSD_CHILD_DEFMAX;


/*
 * sighup_handler()
 *
 * Handler for the SIGHUP signal.
 */

void
sighup_handler(int signo)
{
	restart = 1;
}


/*
 * sigint_handler()
 *
 * Handler for the SIGINT signal.
 */

void
sigint_handler(int signo)
{
	cvs_sock_doloop = 0;
}


/*
 * sigchld_handler()
 *
 */

void
sigchld_handler(int signo)
{
	int status;

	wait(&status);

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
	    "Usage: %s [-dfpv] [-a aclfile] [-c config] [-r root] [-s path]\n"
	    "\t-a aclfile\tUse the file <aclfile> for ACL ruleset\n"
	    "\t-d\t\tStart the server in debugging mode (very verbose)\n"
	    "\t-f\t\tStay in foreground instead of becoming a daemon\n"
	    "\t-p\t\tPerform permission and ownership check on the repository\n"
	    "\t-r root\tUse <root> as the root directory of the repository\n"
	    "\t-s path\tUse <path> as the path for the CVS server socket\n"
	    "\t-v\t\tBe verbose\n",
	    __progname);
}


int
main(int argc, char **argv)
{
	u_int i;
	int ret, checkrepo;
	uid_t uid;
	struct passwd *pwd;
	struct group *grp;

	checkrepo = 0;

	if (cvs_log_init(LD_STD|LD_SYSLOG, LF_PID) < 0)
		err(1, "failed to initialize logging mechanism");

	while ((ret = getopt(argc, argv, "a:dfpr:s:v")) != -1) {
		switch (ret) {
		case 'a':
			cvsd_aclfile = optarg;
			break;
		case 'd':
			cvs_log_filter(LP_FILTER_UNSET, LP_DEBUG);
			cvs_log_filter(LP_FILTER_UNSET, LP_INFO);
			break;
		case 'f':
			foreground = 1;
			break;
		case 'p':
			checkrepo = 1;
			break;
		case 'r':
			cvsd_root = optarg;
			break;
		case 's':
			cvsd_sock_path = optarg;
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

	if ((cvsd_aclfile != NULL) && (cvs_acl_parse(cvsd_aclfile) < 0))
		errx(1, "error while parsing the ACL file `%s'", cvsd_aclfile);

	if (cvsd_root == NULL)
		errx(1, "no CVS root directory specified");

	if (argc > 0)
		errx(EX_USAGE, "unrecognized trailing arguments");

	TAILQ_INIT(&cvsd_children);

	pwd = getpwnam(CVSD_USER);
	if (pwd == NULL)
		err(EX_NOUSER, "failed to get user `%s'", CVSD_USER);

	grp = getgrnam(CVSD_GROUP);
	if (grp == NULL)
		err(EX_NOUSER, "failed to get group `%s'", CVSD_GROUP);

	cvsd_uid = pwd->pw_uid;
	cvsd_gid = grp->gr_gid;

	signal(SIGHUP, sighup_handler);
	signal(SIGINT, sigint_handler);
	signal(SIGQUIT, sigint_handler);
	signal(SIGTERM, sigint_handler);

	if (!foreground && daemon(0, 0) == -1) {
		cvs_log(LP_ERRNO, "failed to become a daemon");
		exit(EX_OSERR);
	}

	if (cvsd_sock_open() < 0) {
		exit(1);
	}

	if (setegid(cvsd_gid) == -1) {
		cvs_log(LP_ERRNO, "failed to drop group privileges");
		exit(EX_OSERR);
	}
	if (seteuid(cvsd_uid) == -1) {
		cvs_log(LP_ERRNO, "failed to drop user privileges");
		exit(EX_OSERR);
	}

	if (checkrepo && cvsd_checkperms("/") != 0) {
		cvs_log(LP_ERR,
		    "exiting due to permission errors on repository");
		exit(1);
	}

	/* spawn the initial pool of children */
	for (i = 0; i < cvsd_chmin; i++) {
		ret = cvsd_forkchild();
		if (ret == -1) {
			cvs_log(LP_ERR, "failed to spawn child");
			exit(EX_OSERR);
		}
	}

	cvsd_sock_loop();

	cvs_log(LP_NOTICE, "shutting down");
	cvs_log_cleanup();

	cvsd_sock_close();

	return (0);
}


/*
 * cvsd_privdrop()
 *
 * Drop privileges.
 */

int
cvsd_privdrop(void)
{
	cvs_log(LP_INFO, "dropping privileges to %s:%s", CVSD_USER,
	    CVSD_GROUP);
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

	return (0);
}


/*
 * cvsd_checkperms()
 *
 * Check permissions on the CVS repository and log warnings for any
 * weird of loose permissions.
 * Returns the number of warnings on success, or -1 on failure.
 */

int
cvsd_checkperms(const char *path)
{
	int fd, nbwarn, ret;
	mode_t fmode;
	long base;
	void *dp, *endp;
	char buf[1024], spath[MAXPATHLEN];
	struct stat st;
	struct dirent *dep;

	nbwarn = 0;

	cvs_log(LP_DEBUG, "checking permissions on `%s'", path);

	if (stat(path, &st) == -1) {
		cvs_log(LP_ERRNO, "failed to stat `%s'", path);
		return (-1);
	}

	if (S_ISDIR(st.st_mode))
		fmode = CVSD_DPERM;
	else
		fmode = CVSD_FPERM;

	if (st.st_uid != cvsd_uid) {
		cvs_log(LP_WARN, "owner of `%s' is not %s", path, CVSD_USER);
		nbwarn++;
	}

	if (st.st_gid != cvsd_gid) {
		cvs_log(LP_WARN, "group of `%s' is not %s", path, CVSD_GROUP);
		nbwarn++;
	}

	if (st.st_mode & S_IWGRP) {
		cvs_log(LP_WARN, "file `%s' is group-writable", path,
		    fmode);
		nbwarn++;
	}

	if (st.st_mode & S_IWOTH) {
		cvs_log(LP_WARN, "file `%s' is world-writable", path,
		    fmode);
		nbwarn++;
	}

	if (S_ISDIR(st.st_mode)) {
		fd = open(path, O_RDONLY, 0);
		if (fd == -1) {
			cvs_log(LP_ERRNO, "failed to open directory `%s'",
			    path);
			return (nbwarn);
		}
		/* recurse */
		ret = getdirentries(fd, buf, sizeof(buf), &base);
		if (ret == -1) {
			cvs_log(LP_ERRNO,
			    "failed to get directory entries for `%s'", path);
			(void)close(fd);
			return (nbwarn);
		}

		dp = buf;
		endp = buf + ret;

		while (dp < endp) {
			dep = (struct dirent *)dp;
			dp += dep->d_reclen;

			if ((dep->d_namlen == 1) && (dep->d_name[0] == '.'))
				continue;
			if ((dep->d_namlen == 2) && (dep->d_name[0] == '.') &&
			    (dep->d_name[1] == '.'))
				continue;

			/* skip the CVSROOT directory */
			if (strcmp(dep->d_name, CVS_PATH_ROOT) == 0)
				continue;

			snprintf(spath, sizeof(spath), "%s/%s", path,
			    dep->d_name);
			ret = cvsd_checkperms(spath);
			if (ret == -1)
				nbwarn++;
			else
				nbwarn += ret;
		}
		(void)close(fd);
	}


	return (nbwarn);
}


/*
 * cvsd_forkchild()
 *
 * Fork a child process which chroots to the CVS repository's root directory.
 * We need to temporarily regain privileges in order to chroot.
 * On success, returns 0 if this is the child process, 1 if this is the
 * parent, or -1 on failure.
 */

int
cvsd_forkchild(void)
{
	int svec[2];
	pid_t pid;
	struct cvsd_child *chp;

	if (socketpair(AF_LOCAL, SOCK_STREAM, PF_UNSPEC, svec) == -1) {
		cvs_log(LP_ERRNO, "failed to create socket pair");
		return (-1);
	}

	/*
	 * We need to temporarily regain original privileges in order for the
	 * child to chroot().
	 */
	if (seteuid(0) == -1) {
		cvs_log(LP_ERRNO, "failed to regain privileges");
		return (-1);
	}

	pid = fork();
	if (pid == -1) {
		cvs_log(LP_ERRNO, "failed to fork child");
		(void)close(svec[0]);
		(void)close(svec[1]);
		return (-1);
	}

	if (pid == 0) {
		cvsd_privfd = svec[1];
		(void)close(svec[0]);

		cvs_log(LP_INFO, "changing root to %s", cvsd_root);
		if (chroot(cvsd_root) == -1) {
			cvs_log(LP_ERRNO, "failed to chroot to `%s'",
			    cvsd_root);
			exit(EX_OSERR);
		}
		(void)chdir("/");

		if (cvsd_privdrop() < 0)
			exit(EX_OSERR);

		setproctitle("%s [child %d]", __progname, getpid());

		cvsd_child_loop();

		return (0);
	}

	cvs_log(LP_INFO, "spawning child %d", pid);

	if (seteuid(cvsd_uid) == -1) {
		cvs_log(LP_ERRNO, "failed to redrop privs");
		return (-1);
	}

	chp = (struct cvsd_child *)malloc(sizeof(*chp));
	if (chp == NULL) {
		cvs_log(LP_ERRNO, "failed to allocate child data");
		return (-1);
	}

	chp->ch_pid = pid;
	chp->ch_sock = svec[0];
	(void)close(svec[1]);

	return (1);
}


/*
 * cvsd_parent_loop()
 *
 * Main loop of the parent cvsd process, which listens on its end of the
 * socket pair for requests from the chrooted child.
 */

static void
cvsd_parent_loop(void)
{
	uid_t uid;
	int timeout, ret;
	nfds_t nfds, i;
	struct pollfd *pfd;
	struct cvsd_child *chp;

	nfds = 0;
	timeout = INFTIM;

	for (;;) {
		nfds = cvsd_chnum;
		pfd = (struct pollfd *)realloc(pfd,
		    nfds * sizeof(struct pollfd));
		if (pfd == NULL) {
			cvs_log(LP_ERRNO, "failed to reallocate polling data");
			return;
		}

		i = 0;
		TAILQ_FOREACH(chp, &cvsd_children, ch_list) {
			pfd[i].fd = chp->ch_sock;
			pfd[i].events = POLLIN;
			pfd[i].revents = 0;
			i++;

			if (i == nfds)
				break;
		}

		ret = poll(pfd, nfds, timeout);
		if (ret == -1) {
			cvs_log(LP_ERRNO, "poll error");
			break;
		}

		chp = TAILQ_FIRST(&cvsd_children);
		for (i = 0; i < nfds; i++) {
			if (pfd[i].revents & (POLLERR|POLLNVAL)) {
				cvs_log(LP_ERR,
				    "poll error on child socket (PID %d)",
				    chp->ch_pid);
			}
			else
				cvsd_msghdlr(chp, pfd[i].fd);

			chp = TAILQ_NEXT(chp, ch_list);
		}

	}
}


/*
 * cvsd_child_loop()
 *
 */

static void
cvsd_child_loop(void)
{
	int ret, timeout;
	struct pollfd pfd[1];

	pfd[0].fd = cvsd_privfd;
	pfd[0].events = POLLIN;
	timeout = INFTIM;

	for (;;) {
		ret = poll(pfd, 1, timeout);
		if (ret == -1) {
		}
		else if (ret == 0) {
			cvs_log(LP_INFO, "parent process closed descriptor");
			break;
		}
		cvs_log(LP_INFO, "polling");

	}

	exit(0);
}


/*
 * cvsd_msghdlr()
 */

int
cvsd_msghdlr(struct cvsd_child *child, int fd)
{
	uid_t uid;
	ssize_t ret;
	char rbuf[CVSD_MSG_MAXLEN];
	struct group *gr;
	struct passwd *pw;
	struct iovec iov[2];
	struct cvsd_msg msg;

	ret = read(fd, &msg, sizeof(msg));
	if (ret == -1) {
		cvs_log(LP_ERRNO, "failed to read CVS message");
		return (-1);
	}
	else if (ret == 0) {
		cvs_log(LP_WARN, "child closed socket pair");
		return (0);
	}

	if (msg.cm_len > 0) {
		ret = read(fd, rbuf, msg.cm_len);
		if (ret != (ssize_t)msg.cm_len) {
			cvs_log(LP_ERR, "failed to read entire msg");
			return (-1);
		}
	}

	/* setup the I/O vector for the reply */
	iov[0].iov_base = &msg;
	iov[0].iov_len = sizeof(msg);

	msg.cm_type = CVSD_MSG_ERROR;
	msg.cm_len = 0;

	switch (msg.cm_type) {
	case CVSD_MSG_GETUID:
		rbuf[ret] = '\0';
		cvs_log(LP_INFO, "getting UID for `%s'", rbuf); 

		pw = getpwnam(rbuf);
		if (pw != NULL) {
			msg.cm_type = CVSD_MSG_UID;
			msg.cm_len = sizeof(uid_t);
			iov[1].iov_len = msg.cm_len;
			iov[1].iov_base = &(pw->pw_uid);
		}
		break;
	case CVSD_MSG_GETUNAME:
		memcpy(&uid, rbuf, sizeof(uid));
		cvs_log(LP_INFO, "getting username for UID %u", uid); 
		pw = getpwuid(uid);
		if (pw != NULL) {
			msg.cm_type = CVSD_MSG_UNAME;
			msg.cm_len = strlen(pw->pw_name);
			iov[1].iov_len = msg.cm_len;
			iov[1].iov_base = pw->pw_name;
		}
		break;
	default:
		cvs_log(LP_ERR, "unknown command type %u", msg.cm_type);
		return (-1);
	}

	ret = writev(fd, iov, 2);

	return (ret);
}
