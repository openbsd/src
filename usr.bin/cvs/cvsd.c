/*	$OpenBSD: cvsd.c,v 1.18 2005/02/22 22:33:01 jfb Exp $	*/
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
#include "repo.h"
#include "cvsd.h"


static void  cvsd_parent_loop (void);
static void  cvsd_report      (void);


extern char *__progname;


int    cvsd_fg = 0;
uid_t  cvsd_uid = 0;
gid_t  cvsd_gid = 0;

volatile sig_atomic_t cvsd_running = 1;
volatile sig_atomic_t cvsd_restart = 0;

static char  *cvsd_user = NULL;
static char  *cvsd_group = NULL;
static char  *cvsd_root = NULL;
static char  *cvsd_conffile = CVSD_PATH_CONF;
static char  *cvsd_moddir = NULL;
static int    cvsd_privfd = -1;

static CVSREPO *cvsd_repo;


static TAILQ_HEAD(,cvsd_child) cvsd_children;
static volatile sig_atomic_t   cvsd_chnum = 0;
static volatile sig_atomic_t   cvsd_chmax = CVSD_CHILD_DEFMAX;
static volatile sig_atomic_t   cvsd_sigchld = 0;
static volatile sig_atomic_t   cvsd_siginfo = 0;


void   usage	 (void);
void   cvsd_sighdlr  (int);
int    cvsd_msghdlr  (struct cvsd_child *, int);


/*
 * cvsd_sighdlr()
 *
 * Generic signal handler.
 */
void
cvsd_sighdlr(int signo)
{
	switch (signo) {
	case SIGHUP:
		cvsd_restart = 1;
		break;
	case SIGCHLD:
		cvsd_sigchld = 1;
		break;
	case SIGINT:
	case SIGTERM:
	case SIGQUIT:
		cvsd_running = 0;
		break;
	case SIGINFO:
		cvsd_siginfo = 1;
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
	    "Usage: %s [-dfhpv] [-c config] [-g group] [-r root] "
	    "[-s path] [-u user]\n"
	    "\t-c config\tUse <config> as the configuration file\n"
	    "\t-d\t\tStart the server in debugging mode (very verbose)\n"
	    "\t-f\t\tStay in foreground instead of becoming a daemon\n"
	    "\t-g group\tUse group <group> for privilege revocation\n"
	    "\t-h\t\tPrint the usage and exit\n"
	    "\t-p\t\tPerform repository sanity check on startup\n"
	    "\t-r root\t\tUse <root> as the root directory of the repository\n"
	    "\t-s path\t\tUse <path> as the path for the CVS server socket\n"
	    "\t-u user\t\tUse user <user> for privilege revocation\n"
	    "\t-v\t\tBe verbose\n",
	    __progname);
}


int
main(int argc, char **argv)
{
	int ret, repo_flags;
	struct passwd *pwd;
	struct group *grp;

	repo_flags = 0;
	cvsd_set(CVSD_SET_SOCK, CVSD_SOCK_PATH);
	cvsd_set(CVSD_SET_USER, CVSD_USER);
	cvsd_set(CVSD_SET_GROUP, CVSD_GROUP);

	if (cvs_log_init(LD_STD|LD_SYSLOG, LF_PID) < 0)
		err(1, "failed to initialize logging mechanism");

	while ((ret = getopt(argc, argv, "c:dfg:hpr:s:u:v")) != -1) {
		switch (ret) {
		case 'c':
			cvsd_conffile = optarg;
			break;
		case 'd':
			cvs_log_filter(LP_FILTER_UNSET, LP_DEBUG);
			cvs_log_filter(LP_FILTER_UNSET, LP_INFO);
			break;
		case 'f':
			cvsd_fg = 1;
			break;
		case 'g':
			cvsd_set(CVSD_SET_GROUP, optarg);
			break;
		case 'h':
			usage();
			exit(0);
			/* NOTREACHED */
			break;
		case 'p':
			repo_flags |= CVS_REPO_CHKPERM;
			break;
		case 'r':
			cvsd_set(CVSD_SET_ROOT, optarg);
			break;
		case 's':
			cvsd_set(CVSD_SET_SOCK, optarg);
			break;
		case 'u':
			cvsd_set(CVSD_SET_USER, optarg);
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

	if (cvs_conf_read(cvsd_conffile) < 0)
		errx(1, "error parsing configuration file `%s'", cvsd_conffile);

	if (cvsd_root == NULL)
		errx(1, "no CVS root directory specified");

	if (argc > 0)
		errx(EX_USAGE, "unrecognized trailing arguments");

	TAILQ_INIT(&cvsd_children);

	pwd = getpwnam(cvsd_user);
	if (pwd == NULL)
		err(EX_NOUSER, "failed to get user `%s'", cvsd_user);

	grp = getgrnam(cvsd_group);
	if (grp == NULL)
		err(EX_NOUSER, "failed to get group `%s'", cvsd_group);

	endpwent();
	endgrent();

	cvsd_uid = pwd->pw_uid;
	cvsd_gid = grp->gr_gid;

	signal(SIGHUP, cvsd_sighdlr);
	signal(SIGINT, cvsd_sighdlr);
	signal(SIGQUIT, cvsd_sighdlr);
	signal(SIGTERM, cvsd_sighdlr);
	signal(SIGCHLD, cvsd_sighdlr);
	signal(SIGPIPE, SIG_IGN);

	if (!cvsd_fg && daemon(0, 0) == -1) {
		cvs_log(LP_ERRNO, "failed to become a daemon");
		exit(EX_OSERR);
	}

	if ((cvsd_repo = cvs_repo_load(cvsd_root, repo_flags)) == NULL) {
		cvs_log(LP_ERR, "failed to load repository");
		exit(EX_OSERR);
	};

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

	signal(SIGINFO, cvsd_sighdlr);
	cvsd_parent_loop();

	cvsd_sock_close();

	cvs_repo_free(cvsd_repo);

	cvs_log(LP_NOTICE, "shutting down");
	cvs_log_cleanup();
	return (0);
}


/*
 * cvsd_child_fork()
 *
 * Fork a child process which chroots to the CVS repository's root directory,
 * drops all privileges, and then executes the cvsd-child process, which will
 * handle the incoming CVS requests.
 * On success, returns a pointer to the new child structure,
 * or NULL on failure.
 */
struct cvsd_child*
cvsd_child_fork(int sock)
{
	int argc, svec[2];
	pid_t pid;
	char *argv[16], ubuf[8], gbuf[8];
	struct cvsd_child *chp;

	if (cvsd_chnum == cvsd_chmax) {
		cvs_log(LP_WARN, "child pool reached limit of processes");
		return (NULL);
	}

	if (socketpair(AF_LOCAL, SOCK_STREAM, PF_UNSPEC, svec) == -1) {
		cvs_log(LP_ERRNO, "failed to create socket pair");
		return (NULL);
	}

	/*
	 * We need to temporarily regain original privileges in order for the
	 * child to chroot().
	 */
	if (seteuid(0) == -1) {
		cvs_log(LP_ERRNO, "failed to regain privileges");
		return (NULL);
	}

	pid = fork();
	if (pid == -1) {
		cvs_log(LP_ERRNO, "failed to fork child");
		(void)close(svec[0]);
		(void)close(svec[1]);
		return (NULL);
	}

	if (pid == 0) {
		cvsd_privfd = svec[1];
		(void)close(svec[0]);

		/*
		 * Move the accepted socket to descriptor 3, where the child
		 * expects it to be.  This could become troublesome if the
		 * descriptor is already taken, but then again, the child
		 * shouldn't have access to other descriptors except the
		 * connection and its side of the socket pair it shares with
		 * the parent.
		 */
		if (dup2(sock, CVSD_CHILD_SOCKFD) == -1) {
			cvs_log(LP_ERRNO, "failed to dup child socket");
			exit(EX_OSERR);
		}
		(void)close(sock);

		argc = 0;
		argv[argc++] = CVSD_PATH_CHILD;
		argv[argc++] = "-r";
		argv[argc++] = cvsd_root;
		if (cvsd_uid != 0) {
			snprintf(ubuf, sizeof(ubuf), "%d", cvsd_uid);
			argv[argc++] = "-u";
			argv[argc++] = ubuf;
		}
		if (cvsd_gid != 0) {
			snprintf(gbuf, sizeof(gbuf), "%d", cvsd_gid);
			argv[argc++] = "-g";
			argv[argc++] = gbuf;
		}
		argv[argc] = NULL;

		execv(CVSD_PATH_CHILD, argv);
		err(1, "FUCK");
		exit(EX_OSERR);
	}

	cvs_log(LP_INFO, "spawning child %d", pid);

	(void)close(svec[1]);

	if (seteuid(cvsd_uid) == -1)
		cvs_log(LP_ERRNO, "failed to redrop privs");

	chp = (struct cvsd_child *)malloc(sizeof(*chp));
	if (chp == NULL) {
		/* XXX kill child */
		cvs_log(LP_ERRNO, "failed to allocate child data");
		return (NULL);
	}

	chp->ch_pid = pid;
	chp->ch_sock = svec[0];
	chp->ch_state = CVSD_ST_IDLE;

	TAILQ_INSERT_TAIL(&cvsd_children, chp, ch_list);
	cvsd_chnum++;

	return (chp);
}


/*
 * cvsd_child_reap()
 *
 * Wait for a child's status and perform the proper actions depending on it.
 * If the child has exited or has been terminated by a signal, it will be
 * removed from the list.
 * Returns 0 on success, or -1 on failure.
 */
int
cvsd_child_reap(void)
{
	pid_t pid;
	int status;
	struct cvsd_child *ch;

	pid = wait(&status);
	if (pid == -1) {
		cvs_log(LP_ERRNO, "failed to wait for child");
		return (-1);
	}

	TAILQ_FOREACH(ch, &cvsd_children, ch_list) {
		if (ch->ch_pid == pid) {
			if (WIFEXITED(status)) {
				cvs_log(LP_WARN,
				    "child %d exited with status %d",
				    pid, WEXITSTATUS(status));
			} else if (WIFSIGNALED(status)) {
				cvs_log(LP_WARN,
				    "child %d terminated with signal %d",
				    pid, WTERMSIG(status));
			} else {
				cvs_log(LP_ERR, "HOLY SHIT!");
			}

			signal(SIGCHLD, SIG_IGN);
			TAILQ_REMOVE(&cvsd_children, ch, ch_list);
			cvsd_chnum--;
			signal(SIGCHLD, cvsd_sighdlr);

			break;
		}
	}

	return (0);
}


/*
 * cvsd_parent_loop()
 *
 * Main loop of the parent cvsd process, which listens on its end of the
 * local socket for requests from the cvs(1) program and on any outstanding
 * messages from the children.
 */
static void
cvsd_parent_loop(void)
{
	int cfd, timeout, ret;
	nfds_t nfds, i;
	struct pollfd *pfd;
	struct cvsd_child *chp;

	nfds = 0;
	timeout = INFTIM;
	pfd = NULL;

	for (;;) {
		if (!cvsd_running)
			break;

		if (cvsd_restart) {
			/* restart server */
		}

		if (cvsd_sigchld) {
			cvsd_sigchld = 0;
			cvsd_child_reap();
		}
		if (cvsd_siginfo) {
			cvsd_siginfo = 0;
			cvsd_report();
		}

		nfds = cvsd_chnum + 1;
		pfd = (struct pollfd *)realloc(pfd,
		    nfds * sizeof(struct pollfd));
		if (pfd == NULL) {
			cvs_log(LP_ERRNO, "failed to reallocate polling data");
			return;
		}

		pfd[0].fd = cvsd_sock;
		pfd[0].events = POLLIN;
		pfd[0].revents = 0;
		i = 1;
		TAILQ_FOREACH(chp, &cvsd_children, ch_list) {
			pfd[i].fd = chp->ch_sock;
			pfd[i].events = POLLIN;
			pfd[i].revents = 0;
			i++;

			if (i == nfds)   /* just a precaution */
				break;
		}

		ret = poll(pfd, nfds, timeout);
		if (ret == -1) {
			if (errno == EINTR)
				continue;
			cvs_log(LP_ERRNO, "poll error");
			break;
		}

		if (pfd[0].revents & (POLLERR|POLLNVAL)) {
			cvs_log(LP_ERR, "poll error on request socket");
		} else if (pfd[0].revents & POLLIN) {
			uid_t uid;
			gid_t gid;

			if ((cfd = cvsd_sock_accept(pfd[0].fd)) == -1)
				continue;

			if ((chp = cvsd_child_fork(cfd)) == NULL) {
				cvs_log(LP_ALERT,
				    "request queue not implemented");
				break;
			}

			if (getpeereid(cfd, &uid, &gid) < 0)
				err(1, "failed to get UID");
			if (cvsd_sendmsg(chp->ch_sock, CVSD_MSG_PASSFD,
			    &cfd, sizeof(cfd)) < 0)
				break;

			/* mark the child as busy */
			chp->ch_state = CVSD_ST_BUSY;
		}

		chp = TAILQ_FIRST(&cvsd_children);
		for (i = 1; i < nfds; i++) {
			if (pfd[i].revents & (POLLERR|POLLNVAL)) {
				cvs_log(LP_ERR,
				    "poll error on child socket (PID %d)",
				    chp->ch_pid);
			} else if (pfd[i].revents & POLLIN)
				cvsd_msghdlr(chp, pfd[i].fd);

			chp = TAILQ_NEXT(chp, ch_list);
		}

	}

	/* broadcast a shutdown message to children */
	TAILQ_FOREACH(chp, &cvsd_children, ch_list) {
		(void)cvsd_sendmsg(chp->ch_sock, CVSD_MSG_SHUTDOWN, NULL, 0);
	}
}


/*
 * cvsd_msghdlr()
 *
 * Handler for messages received from child processes.
 * Returns 0 on success, or -1 on failure.
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
	} else if (ret == 0) {
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
	case CVSD_MSG_GETGID:
		rbuf[ret] = '\0';
		cvs_log(LP_INFO, "getting GID for `%s'", rbuf);

		gr = getgrnam(rbuf);
		if (gr != NULL) {
			msg.cm_type = CVSD_MSG_GID;
			msg.cm_len = sizeof(gid_t);
			iov[1].iov_len = msg.cm_len;
			iov[1].iov_base = &(gr->gr_gid);
		}
		break;
	case CVSD_MSG_SETIDLE:
		child->ch_state = CVSD_ST_IDLE;
		break;
	default:
		cvs_log(LP_ERR, "unknown command type %u", msg.cm_type);
		return (-1);
	}

	ret = writev(fd, iov, 2);

	return (ret);
}


/*
 * cvsd_set()
 *
 * Generic interface to set some of the parameters of the cvs server.
 * When a string is set using cvsd_set(), the original string is copied into
 * a new buffer.
 * Returns 0 on success, or -1 on failure.
 */
int
cvsd_set(int what, ...)
{
	char *str;
	int error = 0;
	va_list vap;

	str = NULL;

	va_start(vap, what);

	if ((what == CVSD_SET_ROOT) || (what == CVSD_SET_SOCK) ||
	    (what == CVSD_SET_USER) || (what == CVSD_SET_GROUP) ||
	    (what == CVSD_SET_MODDIR)) {
		str = strdup(va_arg(vap, char *));
		if (str == NULL) {
			cvs_log(LP_ERRNO, "failed to set string");
			va_end(vap);
			return (-1);
		}
	}

	switch (what) {
	case CVSD_SET_ROOT:
		if (cvsd_root != NULL)
			free(cvsd_root);
		cvsd_root = str;
		break;
	case CVSD_SET_SOCK:
		if (cvsd_sock_path != NULL)
			free(cvsd_sock_path);
		cvsd_sock_path = str;
		break;
	case CVSD_SET_USER:
		if (cvsd_user != NULL)
			free(cvsd_user);
		cvsd_user = str;
		break;
	case CVSD_SET_GROUP:
		if (cvsd_group != NULL)
			free(cvsd_group);
		cvsd_group = str;
		break;
	case CVSD_SET_MODDIR:
		if (cvsd_moddir != NULL)
			free(cvsd_moddir);
		cvsd_moddir = str;
		break;
	case CVSD_SET_CHMAX:
		cvsd_chmax = va_arg(vap, int);
		/* we should decrease the number of children accordingly */
		break;
	case CVSD_SET_ADDR:
		/* this is more like an add than a set */
		break;
	default:
		cvs_log(LP_ERR, "invalid field to set");
		error = -1;
		break;
	}

	va_end(vap);

	return (error);
}


/*
 * cvsd_report()
 *
 * Report about the current state of child processes on the repository.
 */
static void
cvsd_report(void)
{
	u_int nb_idle, nb_busy, nb_unknown;
	struct cvsd_child *ch;

	nb_idle = 0;
	nb_busy = 0;
	nb_unknown = 0;

	signal(SIGCHLD, SIG_IGN);
	TAILQ_FOREACH(ch, &cvsd_children, ch_list) {
		if (ch->ch_state == CVSD_ST_IDLE)
			nb_idle++;
		else if (ch->ch_state == CVSD_ST_BUSY)
			nb_busy++;
		else if (ch->ch_state == CVSD_ST_UNKNOWN)
			nb_unknown++;
	}

	cvs_log(LP_WARN, "%u children, %u idle, %u busy, %u unknown",
	    cvsd_chnum, nb_idle, nb_busy, nb_unknown);

	TAILQ_FOREACH(ch, &cvsd_children, ch_list)
		cvs_log(LP_WARN, "");
	signal(SIGCHLD, cvsd_sighdlr);
}
