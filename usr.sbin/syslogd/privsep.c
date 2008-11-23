/*	$OpenBSD: privsep.c,v 1.34 2008/11/23 04:29:42 brad Exp $	*/

/*
 * Copyright (c) 2003 Anil Madhavapeddy <anil@recoil.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/queue.h>
#include <sys/uio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
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
#include <unistd.h>
#include <util.h>
#include <utmp.h>
#include "syslogd.h"

/*
 * syslogd can only go forward in these states; each state should represent
 * less privilege.   After STATE_INIT, the child is allowed to parse its
 * config file once, and communicate the information regarding what logfiles
 * it needs access to back to the parent.  When that is done, it sends a
 * message to the priv parent revoking this access, moving to STATE_RUNNING.
 * In this state, any log-files not in the access list are rejected.
 *
 * This allows a HUP signal to the child to reopen its log files, and
 * the config file to be parsed if it hasn't been changed (this is still
 * useful to force resolution of remote syslog servers again).
 * If the config file has been modified, then the child dies, and
 * the priv parent restarts itself.
 */
enum priv_state {
	STATE_INIT,		/* just started up */
	STATE_CONFIG,		/* parsing config file for first time */
	STATE_RUNNING,		/* running and accepting network traffic */
	STATE_QUIT		/* shutting down */
};

enum cmd_types {
	PRIV_OPEN_TTY,		/* open terminal or console device */
	PRIV_OPEN_LOG,		/* open logfile for appending */
	PRIV_OPEN_PIPE,		/* fork & exec child that gets logs on stdin */
	PRIV_OPEN_UTMP,		/* open utmp for reading only */
	PRIV_OPEN_CONFIG,	/* open config file for reading only */
	PRIV_CONFIG_MODIFIED,	/* check if config file has been modified */
	PRIV_GETHOSTSERV,	/* resolve host/service names */
	PRIV_GETHOSTBYADDR,	/* resolve numeric address into hostname */
	PRIV_DONE_CONFIG_PARSE	/* signal that the initial config parse is done */
};

static int priv_fd = -1;
static volatile pid_t child_pid = -1;
static char config_file[MAXPATHLEN];
static struct stat cf_info;
static int allow_gethostbyaddr = 0;
static volatile sig_atomic_t cur_state = STATE_INIT;

/* Queue for the allowed logfiles */
struct logname {
	char path[MAXPATHLEN];
	TAILQ_ENTRY(logname) next;
};
static TAILQ_HEAD(, logname) lognames;

static void check_log_name(char *, size_t);
static int open_file(char *);
static int open_pipe(char *);
static void check_tty_name(char *, size_t);
static void increase_state(int);
static void sig_pass_to_chld(int);
static void sig_got_chld(int);
static void must_read(int, void *, size_t);
static void must_write(int, void *, size_t);
static int  may_read(int, void *, size_t);

int
priv_init(char *conf, int numeric, int lockfd, int nullfd, char *argv[])
{
	int i, fd, socks[2], cmd, addr_len, addr_af, result, restart;
	size_t path_len, hostname_len, servname_len;
	char path[MAXPATHLEN], hostname[MAXHOSTNAMELEN];
	char servname[MAXHOSTNAMELEN];
	struct stat cf_stat;
	struct hostent *hp;
	struct passwd *pw;
	struct addrinfo hints, *res0;
	struct sigaction sa;

	memset(&sa, 0, sizeof(sa));
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	sa.sa_handler = SIG_DFL;
	for (i = 1; i < _NSIG; i++)
		sigaction(i, &sa, NULL);

	/* Create sockets */
	if (socketpair(AF_LOCAL, SOCK_STREAM, PF_UNSPEC, socks) == -1)
		err(1, "socketpair() failed");

	pw = getpwnam("_syslogd");
	if (pw == NULL)
		errx(1, "unknown user _syslogd");

	child_pid = fork();
	if (child_pid < 0)
		err(1, "fork() failed");

	if (!child_pid) {
		/* Child - drop privileges and return */
		if (chroot(pw->pw_dir) != 0)
			err(1, "unable to chroot");
		if (chdir("/") != 0)
			err(1, "unable to chdir");

		if (setgroups(1, &pw->pw_gid) == -1)
			err(1, "setgroups() failed");
		if (setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) == -1)
			err(1, "setresgid() failed");
		if (setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid) == -1)
			err(1, "setresuid() failed");
		close(socks[0]);
		priv_fd = socks[1];
		return 0;
	}

	if (!Debug) {
		close(lockfd);
		dup2(nullfd, STDIN_FILENO);
		dup2(nullfd, STDOUT_FILENO);
		dup2(nullfd, STDERR_FILENO);
	}

	if (nullfd > 2)
		close(nullfd);

	/* Father */
	/* Pass TERM/HUP/INT/QUIT through to child, and accept CHLD */
	sa.sa_handler = sig_pass_to_chld;
	sigaction(SIGTERM, &sa, NULL);
	sigaction(SIGHUP, &sa, NULL);
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGQUIT, &sa, NULL);
	sa.sa_handler = sig_got_chld;
	sa.sa_flags |= SA_NOCLDSTOP;
	sigaction(SIGCHLD, &sa, NULL);

	setproctitle("[priv]");
	close(socks[1]);

	/* Close descriptors that only the unpriv child needs */
	for (i = 0; i < nfunix; i++)
		if (pfd[PFD_UNIX_0 + i].fd != -1)
			close(pfd[PFD_UNIX_0 + i].fd);
	if (pfd[PFD_INET].fd != -1)
		close(pfd[PFD_INET].fd);
	if (pfd[PFD_CTLSOCK].fd != -1)
		close(pfd[PFD_CTLSOCK].fd);
	if (pfd[PFD_CTLCONN].fd != -1)
		close(pfd[PFD_CTLCONN].fd);
	if (pfd[PFD_KLOG].fd)
		close(pfd[PFD_KLOG].fd);

	/* Save the config file specified by the child process */
	if (strlcpy(config_file, conf, sizeof config_file) >= sizeof(config_file))
		errx(1, "config_file truncation");

	if (stat(config_file, &cf_info) < 0)
		err(1, "stat config file failed");

	/* Save whether or not the child can have access to gethostbyaddr(3) */
	if (numeric > 0)
		allow_gethostbyaddr = 0;
	else
		allow_gethostbyaddr = 1;

	TAILQ_INIT(&lognames);
	increase_state(STATE_CONFIG);
	restart = 0;

	while (cur_state < STATE_QUIT) {
		if (may_read(socks[0], &cmd, sizeof(int)))
			break;
		switch (cmd) {
		case PRIV_OPEN_TTY:
			dprintf("[priv]: msg PRIV_OPEN_TTY received\n");
			/* Expecting: length, path */
			must_read(socks[0], &path_len, sizeof(size_t));
			if (path_len == 0 || path_len > sizeof(path))
				_exit(0);
			must_read(socks[0], &path, path_len);
			path[path_len - 1] = '\0';
			check_tty_name(path, path_len);
			fd = open(path, O_WRONLY|O_NONBLOCK, 0);
			send_fd(socks[0], fd);
			if (fd < 0)
				warnx("priv_open_tty failed");
			else
				close(fd);
			break;

		case PRIV_OPEN_LOG:
		case PRIV_OPEN_PIPE:
			dprintf("[priv]: msg PRIV_OPEN_%s received\n",
			    cmd == PRIV_OPEN_PIPE ? "PIPE" : "LOG");
			/* Expecting: length, path */
			must_read(socks[0], &path_len, sizeof(size_t));
			if (path_len == 0 || path_len > sizeof(path))
				_exit(0);
			must_read(socks[0], &path, path_len);
			path[path_len - 1] = '\0';
			check_log_name(path, path_len);

			if (cmd == PRIV_OPEN_LOG)
				fd = open_file(path);
			else if (cmd == PRIV_OPEN_PIPE)
				fd = open_pipe(path);
			else
				errx(1, "invalid cmd");

			send_fd(socks[0], fd);
			if (fd < 0)
				warnx("priv_open_log failed");
			else
				close(fd);
			break;

		case PRIV_OPEN_UTMP:
			dprintf("[priv]: msg PRIV_OPEN_UTMP received\n");
			fd = open(_PATH_UTMP, O_RDONLY|O_NONBLOCK, 0);
			send_fd(socks[0], fd);
			if (fd < 0)
				warnx("priv_open_utmp failed");
			else
				close(fd);
			break;

		case PRIV_OPEN_CONFIG:
			dprintf("[priv]: msg PRIV_OPEN_CONFIG received\n");
			stat(config_file, &cf_info);
			fd = open(config_file, O_RDONLY|O_NONBLOCK, 0);
			send_fd(socks[0], fd);
			if (fd < 0)
				warnx("priv_open_config failed");
			else
				close(fd);
			break;

		case PRIV_CONFIG_MODIFIED:
			dprintf("[priv]: msg PRIV_CONFIG_MODIFIED received\n");
			if (stat(config_file, &cf_stat) < 0 ||
			    timespeccmp(&cf_info.st_mtimespec,
			    &cf_stat.st_mtimespec, <) ||
			    cf_info.st_size != cf_stat.st_size) {
				dprintf("config file modified: restarting\n");
				restart = result = 1;
				must_write(socks[0], &result, sizeof(int));
			} else {
				result = 0;
				must_write(socks[0], &result, sizeof(int));
			}
			break;

		case PRIV_DONE_CONFIG_PARSE:
			dprintf("[priv]: msg PRIV_DONE_CONFIG_PARSE received\n");
			increase_state(STATE_RUNNING);
			break;

		case PRIV_GETHOSTSERV:
			dprintf("[priv]: msg PRIV_GETHOSTSERV received\n");
			/* Expecting: len, hostname, len, servname */
			must_read(socks[0], &hostname_len, sizeof(size_t));
			if (hostname_len == 0 || hostname_len > sizeof(hostname))
				_exit(0);
			must_read(socks[0], &hostname, hostname_len);
			hostname[hostname_len - 1] = '\0';

			must_read(socks[0], &servname_len, sizeof(size_t));
			if (servname_len == 0 || servname_len > sizeof(servname))
				_exit(0);
			must_read(socks[0], &servname, servname_len);
			servname[servname_len - 1] = '\0';

			memset(&hints, '\0', sizeof(hints));
			hints.ai_family = AF_INET;
			hints.ai_socktype = SOCK_DGRAM;
			i = getaddrinfo(hostname, servname, &hints, &res0);
			if (i != 0 || res0 == NULL) {
				addr_len = 0;
				must_write(socks[0], &addr_len, sizeof(int));
			} else {
				/* Just send the first address */
				i = res0->ai_addrlen;
				must_write(socks[0], &i, sizeof(int));
				must_write(socks[0], res0->ai_addr, i);
				freeaddrinfo(res0);
			}
			break;

		case PRIV_GETHOSTBYADDR:
			dprintf("[priv]: msg PRIV_GETHOSTBYADDR received\n");
			if (!allow_gethostbyaddr)
				errx(1, "rejected attempt to gethostbyaddr");
			/* Expecting: length, address, address family */
			must_read(socks[0], &addr_len, sizeof(int));
			if (addr_len <= 0 || addr_len > sizeof(hostname))
				_exit(0);
			must_read(socks[0], hostname, addr_len);
			must_read(socks[0], &addr_af, sizeof(int));
			hp = gethostbyaddr(hostname, addr_len, addr_af);
			if (hp == NULL) {
				addr_len = 0;
				must_write(socks[0], &addr_len, sizeof(int));
			} else {
				addr_len = strlen(hp->h_name) + 1;
				must_write(socks[0], &addr_len, sizeof(int));
				must_write(socks[0], hp->h_name, addr_len);
			}
			break;
		default:
			errx(1, "unknown command %d", cmd);
			break;
		}
	}

	close(socks[0]);

	/* Unlink any domain sockets that have been opened */
	for (i = 0; i < nfunix; i++)
		if (funixn[i] != NULL && pfd[PFD_UNIX_0 + i].fd != -1)
			(void)unlink(funixn[i]);
	if (ctlsock_path != NULL && pfd[PFD_CTLSOCK].fd != -1)
		(void)unlink(ctlsock_path);

	if (restart) {
		int r;

		wait(&r);
		execvp(argv[0], argv);
	}
	unlink(_PATH_LOGPID);
	_exit(1);
}

static int
open_file(char *path)
{
	/* must not start with | */
	if (path[0] == '|')
		return (-1);

	return (open(path, O_WRONLY|O_APPEND|O_NONBLOCK, 0));
}

static int
open_pipe(char *cmd)
{
	char *argp[] = {"sh", "-c", NULL, NULL};
	struct passwd *pw;
	int fd[2];
	int bsize, flags;
	pid_t pid;

	/* skip over leading | and whitespace */
	if (cmd[0] != '|')
		return (-1);
	for(cmd++; *cmd && *cmd == ' '; cmd++)
		; /* nothing */
	if (!*cmd)
		return (-1);

	argp[2] = cmd;

	if (socketpair(AF_UNIX, SOCK_STREAM, PF_UNSPEC, fd) == -1) {
		warnx("open_pipe");
		return (-1);
	}

	/* make the fd on syslogd's side nonblocking */
	if ((flags = fcntl(fd[1], F_GETFL, 0)) == -1) {
		warnx("fcntl");
		return (-1);
	}
	flags |= O_NONBLOCK;
	if ((flags = fcntl(fd[1], F_SETFL, flags)) == -1) {
		warnx("fcntl");
		return (-1);
	}

	switch (pid = fork()) {
	case -1:
		warnx("fork error");
		return (-1);
	case 0:
		break;
	default:
		close(fd[0]);
		return (fd[1]);
	}

	close(fd[1]);

	/* grow receive buffer */
	bsize = 65535;
	while (bsize > 0 && setsockopt(fd[0], SOL_SOCKET, SO_RCVBUF,
	    &bsize, sizeof(bsize)) == -1)
		bsize /= 2;

	if ((pw = getpwnam("_syslogd")) == NULL)
		errx(1, "unknown user _syslogd");
	if (setgroups(1, &pw->pw_gid) == -1 ||
	    setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) == -1 ||
	    setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid) == -1)
		err(1, "failure dropping privs");
	endpwent();

	if (dup2(fd[0], STDIN_FILENO) == -1)
		err(1, "dup2 failed");
	if (execv("/bin/sh", argp) == -1)
		err(1, "execv %s", cmd);
	/* NOTREACHED */
	return (-1);
}

/* Check that the terminal device is ok, and if not, rewrite to /dev/null.
 * Either /dev/console or /dev/tty* are allowed.
 */
static void
check_tty_name(char *tty, size_t ttylen)
{
	const char ttypre[] = "/dev/tty";
	char *p;

	/* Any path containing '..' is invalid.  */
	for (p = tty; *p && (p - tty) < ttylen; p++)
		if (*p == '.' && *(p + 1) == '.')
			goto bad_path;

	if (strcmp(_PATH_CONSOLE, tty) && strncmp(tty, ttypre, strlen(ttypre)))
		goto bad_path;
	return;

bad_path:
	warnx ("%s: invalid attempt to open %s: rewriting to /dev/null",
	    "check_tty_name", tty);
	strlcpy(tty, "/dev/null", ttylen);
}

/* If we are in the initial configuration state, accept a logname and add
 * it to the list of acceptable logfiles.  Otherwise, check against this list
 * and rewrite to /dev/null if it's a bad path.
 */
static void
check_log_name(char *lognam, size_t loglen)
{
	struct logname *lg;
	char *p;

	/* Any path containing '..' is invalid.  */
	for (p = lognam; *p && (p - lognam) < loglen; p++)
		if (*p == '.' && *(p + 1) == '.')
			goto bad_path;

	switch (cur_state) {
	case STATE_CONFIG:
		lg = malloc(sizeof(struct logname));
		if (!lg)
			err(1, "check_log_name() malloc");
		strlcpy(lg->path, lognam, MAXPATHLEN);
		TAILQ_INSERT_TAIL(&lognames, lg, next);
		break;
	case STATE_RUNNING:
		TAILQ_FOREACH(lg, &lognames, next)
			if (!strcmp(lg->path, lognam))
				return;
		goto bad_path;
		break;
	default:
		/* Any other state should just refuse the request */
		goto bad_path;
		break;
	}
	return;

bad_path:
	warnx("%s: invalid attempt to open %s: rewriting to /dev/null",
	    "check_log_name", lognam);
	strlcpy(lognam, "/dev/null", loglen);
}

/* Crank our state into less permissive modes */
static void
increase_state(int state)
{
	if (state <= cur_state)
		errx(1, "attempt to decrease or match current state");
	if (state < STATE_INIT || state > STATE_QUIT)
		errx(1, "attempt to switch to invalid state");
	cur_state = state;
}

/* Open console or a terminal device for writing */
int
priv_open_tty(const char *tty)
{
	char path[MAXPATHLEN];
	int cmd, fd;
	size_t path_len;

	if (priv_fd < 0)
		errx(1, "%s: called from privileged portion", "priv_open_tty");

	if (strlcpy(path, tty, sizeof path) >= sizeof(path))
		return -1;
	path_len = strlen(path) + 1;

	cmd = PRIV_OPEN_TTY;
	must_write(priv_fd, &cmd, sizeof(int));
	must_write(priv_fd, &path_len, sizeof(size_t));
	must_write(priv_fd, path, path_len);
	fd = receive_fd(priv_fd);
	return fd;
}

/* Open log-file */
int
priv_open_log(const char *lognam)
{
	char path[MAXPATHLEN];
	int cmd, fd;
	size_t path_len;

	if (priv_fd < 0)
		errx(1, "%s: called from privileged child", "priv_open_log");

	if (strlcpy(path, lognam, sizeof path) >= sizeof(path))
		return -1;
	path_len = strlen(path) + 1;

	if (lognam[0] == '|')
		cmd = PRIV_OPEN_PIPE;
	else
		cmd = PRIV_OPEN_LOG;
	must_write(priv_fd, &cmd, sizeof(int));
	must_write(priv_fd, &path_len, sizeof(size_t));
	must_write(priv_fd, path, path_len);
	fd = receive_fd(priv_fd);
	return fd;
}

/* Open utmp for reading */
FILE *
priv_open_utmp(void)
{
	int cmd, fd;
	FILE *fp;

	if (priv_fd < 0)
		errx(1, "%s: called from privileged portion", "priv_open_utmp");

	cmd = PRIV_OPEN_UTMP;
	must_write(priv_fd, &cmd, sizeof(int));
	fd = receive_fd(priv_fd);
	if (fd < 0)
		return NULL;

	fp = fdopen(fd, "r");
	if (!fp) {
		warn("priv_open_utmp: fdopen() failed");
		close(fd);
		return NULL;
	}

	return fp;
}

/* Open syslog config file for reading */
FILE *
priv_open_config(void)
{
	int cmd, fd;
	FILE *fp;

	if (priv_fd < 0)
		errx(1, "%s: called from privileged portion", "priv_open_config");

	cmd = PRIV_OPEN_CONFIG;
	must_write(priv_fd, &cmd, sizeof(int));
	fd = receive_fd(priv_fd);
	if (fd < 0)
		return NULL;

	fp = fdopen(fd, "r");
	if (!fp) {
		warn("priv_open_config: fdopen() failed");
		close(fd);
		return NULL;
	}

	return fp;
}

/* Ask if config file has been modified since last attempt to read it */
int
priv_config_modified(void)
{
	int cmd, res;

	if (priv_fd < 0)
		errx(1, "%s: called from privileged portion",
		    "priv_config_modified");

	cmd = PRIV_CONFIG_MODIFIED;
	must_write(priv_fd, &cmd, sizeof(int));

	/* Expect back integer signalling 1 for modification */
	must_read(priv_fd, &res, sizeof(int));
	return res;
}

/* Child can signal that its initial parsing is done, so that parent
 * can revoke further logfile permissions.  This call only works once. */
void
priv_config_parse_done(void)
{
	int cmd;

	if (priv_fd < 0)
		errx(1, "%s: called from privileged portion",
		    "priv_config_parse_done");

	cmd = PRIV_DONE_CONFIG_PARSE;
	must_write(priv_fd, &cmd, sizeof(int));
}

/* Name/service to address translation.  Response is placed into addr, and
 * the length is returned (zero on error) */
int
priv_gethostserv(char *host, char *serv, struct sockaddr *addr,
    size_t addr_len)
{
	char hostcpy[MAXHOSTNAMELEN], servcpy[MAXHOSTNAMELEN];
	int cmd, ret_len;
	size_t hostname_len, servname_len;

	if (priv_fd < 0)
		errx(1, "%s: called from privileged portion", "priv_gethostserv");

	if (strlcpy(hostcpy, host, sizeof hostcpy) >= sizeof(hostcpy))
		errx(1, "%s: overflow attempt in hostname", "priv_gethostserv");
	hostname_len = strlen(hostcpy) + 1;
	if (strlcpy(servcpy, serv, sizeof servcpy) >= sizeof(servcpy))
		errx(1, "%s: overflow attempt in servname", "priv_gethostserv");
	servname_len = strlen(servcpy) + 1;

	cmd = PRIV_GETHOSTSERV;
	must_write(priv_fd, &cmd, sizeof(int));
	must_write(priv_fd, &hostname_len, sizeof(size_t));
	must_write(priv_fd, hostcpy, hostname_len);
	must_write(priv_fd, &servname_len, sizeof(size_t));
	must_write(priv_fd, servcpy, servname_len);

	/* Expect back an integer size, and then a string of that length */
	must_read(priv_fd, &ret_len, sizeof(int));

	/* Check there was no error (indicated by a return of 0) */
	if (!ret_len)
		return 0;

	/* Make sure we aren't overflowing the passed in buffer */
	if (addr_len < ret_len)
		errx(1, "%s: overflow attempt in return", "priv_gethostserv");

	/* Read the resolved address and make sure we got all of it */
	memset(addr, '\0', addr_len);
	must_read(priv_fd, addr, ret_len);

	return ret_len;
}

/* Reverse address resolution; response is placed into res, and length of
 * response is returned (zero on error) */
int
priv_gethostbyaddr(char *addr, int addr_len, int af, char *res, size_t res_len)
{
	int cmd, ret_len;

	if (priv_fd < 0)
		errx(1, "%s called from privileged portion", "priv_gethostbyaddr");

	cmd = PRIV_GETHOSTBYADDR;
	must_write(priv_fd, &cmd, sizeof(int));
	must_write(priv_fd, &addr_len, sizeof(int));
	must_write(priv_fd, addr, addr_len);
	must_write(priv_fd, &af, sizeof(int));

	/* Expect back an integer size, and then a string of that length */
	must_read(priv_fd, &ret_len, sizeof(int));

	/* Check there was no error (indicated by a return of 0) */
	if (!ret_len)
		return 0;

	/* Check we don't overflow the passed in buffer */
	if (res_len < ret_len)
		errx(1, "%s: overflow attempt in return", "priv_gethostbyaddr");

	/* Read the resolved hostname */
	must_read(priv_fd, res, ret_len);
	return ret_len;
}

/* Pass the signal through to child */
static void
sig_pass_to_chld(int sig)
{
	int save_errno = errno;

	if (child_pid != -1)
		kill(child_pid, sig);
	errno = save_errno;
}

/* When child dies, move into the shutdown state */
/* ARGSUSED */
static void
sig_got_chld(int sig)
{
	int save_errno = errno;
	pid_t	pid;

	do {
		pid = waitpid(WAIT_ANY, NULL, WNOHANG);
		if (pid == child_pid && cur_state < STATE_QUIT)
			cur_state = STATE_QUIT;
	} while (pid > 0 || (pid == -1 && errno == EINTR));
	errno = save_errno;
}

/* Read all data or return 1 for error.  */
static int
may_read(int fd, void *buf, size_t n)
{
	char *s = buf;
	ssize_t res, pos = 0;

	while (n > pos) {
		res = read(fd, s + pos, n - pos);
		switch (res) {
		case -1:
			if (errno == EINTR || errno == EAGAIN)
				continue;
		case 0:
			return (1);
		default:
			pos += res;
		}
	}
	return (0);
}

/* Read data with the assertion that it all must come through, or
 * else abort the process.  Based on atomicio() from openssh. */
static void
must_read(int fd, void *buf, size_t n)
{
	char *s = buf;
	ssize_t res, pos = 0;

	while (n > pos) {
		res = read(fd, s + pos, n - pos);
		switch (res) {
		case -1:
			if (errno == EINTR || errno == EAGAIN)
				continue;
		case 0:
			_exit(0);
		default:
			pos += res;
		}
	}
}

/* Write data with the assertion that it all has to be written, or
 * else abort the process.  Based on atomicio() from openssh. */
static void
must_write(int fd, void *buf, size_t n)
{
	char *s = buf;
	ssize_t res, pos = 0;

	while (n > pos) {
		res = write(fd, s + pos, n - pos);
		switch (res) {
		case -1:
			if (errno == EINTR || errno == EAGAIN)
				continue;
		case 0:
			_exit(0);
		default:
			pos += res;
		}
	}
}
