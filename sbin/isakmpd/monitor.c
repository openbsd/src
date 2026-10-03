/* $OpenBSD: monitor.c,v 1.85 2026/10/03 01:31:30 deraadt Exp $	 */

/*
 * Copyright (c) 2003 Håkan Olsson.  All rights reserved.
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
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <netinet/in.h>

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>

#include <regex.h>
#include <keynote.h>

#include "conf.h"
#include "log.h"
#include "monitor.h"
#include "policy.h"
#include "ui.h"
#include "util.h"
#include "pf_key_v2.h"

struct monitor_state {
	pid_t           pid;
	int             s;
	char            root[PATH_MAX];
} m_state;

extern void	set_slave_signals(void);

/* Private functions.  */
static void	must_read(void *, size_t);
static void	must_write(const void *, size_t);

static void	m_priv_getfd(void);
static void	m_priv_setsockopt(void);
static void	m_priv_req_readdir(void);
static void	m_priv_bind(void);
static void	m_priv_pfkey_open(void);
static int	m_priv_check_sockopt(int, int);
static int	m_priv_check_bind(const struct sockaddr *, socklen_t);

static void	set_monitor_signals(void);
static void	sig_pass_to_chld(int);

/*
 * Public functions, unprivileged.
 */

/* Setup monitor context, fork, drop child privs.  */
pid_t
monitor_init(int debug)
{
	struct passwd  *pw;
	int             p[2];

	bzero(&m_state, sizeof m_state);

	if (socketpair(AF_UNIX, SOCK_STREAM, PF_UNSPEC, p) != 0)
		log_fatal("monitor_init: socketpair() failed");

	pw = getpwnam(ISAKMPD_PRIVSEP_USER);
	if (pw == NULL)
		log_fatalx("monitor_init: getpwnam(\"%s\") failed",
		    ISAKMPD_PRIVSEP_USER);
	strlcpy(m_state.root, pw->pw_dir, sizeof m_state.root);

	set_monitor_signals();

	switch ((m_state.pid = fork())) {
	case -1:
		log_fatal("monitor_init: fork of unprivileged child failed");
		break;
	case 0:
		/* The child process drops privileges. */
		set_slave_signals();

		if (chroot(pw->pw_dir) != 0 || chdir("/") != 0)
			log_fatal("monitor_init: chroot in child failed");

		if (setgroups(1, &pw->pw_gid) == -1 ||
		    setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) ||
		    setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid))
			log_fatal("monitor_init: can't drop privileges");

		if (pledge("stdio sendfd route recvfd inet", NULL) == -1)
			log_fatal("monitor_init: pledge in child failed");

		m_state.s = p[0];
		close(p[1]);

		LOG_DBG((LOG_MISC, 10,
		    "monitor_init: privileges dropped for child process"));
		break;
	default:
		/* Privileged monitor. */
		setproctitle("monitor [priv]");

		if (unveil(ISAKMPD_ROOT, "r") == -1)
			log_fatal("monitor_init: unveil %s", ISAKMPD_ROOT);
		if (unveil(NULL, NULL) == -1)
			log_fatal("monitor_init: unveil");

		m_state.s = p[1];
		close(p[0]);
		break;
	}
	return m_state.pid;
}

void
monitor_exit(int code)
{
	int status = 0, gotstatus = 0;
	pid_t pid;

	if (m_state.pid != 0) {
		/* When called from the monitor, kill slave and wait for it  */
		kill(m_state.pid, SIGTERM);

		do {
			pid = waitpid(m_state.pid, &status, 0);
		} while (pid == -1 && errno == EINTR);
		if (pid != -1)
			gotstatus = 1;

		/* Remove FIFO and pid files.  */
		unlink(ui_fifo);
	}

	close(m_state.s);
	if (code == 0 && gotstatus)
		exit(WIFEXITED(status)? WEXITSTATUS(status) : 1);
	else
		exit(code);
}

int
monitor_pf_key_v2_open(void)
{
	int	err, cmd;

	cmd = MONITOR_PFKEY_OPEN;
	must_write(&cmd, sizeof cmd);

	must_read(&err, sizeof err);
	if (err < 0) {
		log_error("monitor_pf_key_v2_open: parent could not create "
		    "PF_KEY socket");
		return -1;
	}
	pf_key_v2_socket = mm_receive_fd(m_state.s);
	if (pf_key_v2_socket < 0) {
		log_error("monitor_pf_key_v2_open: mm_receive_fd() failed");
		return -1;
	}

	return pf_key_v2_socket;
}

int
monitor_open(const char *path, int flags)
{
	size_t	len;
	int	fd, err, cmd;

	cmd = MONITOR_GET_FD;
	must_write(&cmd, sizeof cmd);

	len = strlen(path);
	must_write(&len, sizeof len);
	must_write(path, len);

	must_write(&flags, sizeof flags);

	must_read(&err, sizeof err);
	if (err != 0) {
		errno = err;
		return -1;
	}

	fd = mm_receive_fd(m_state.s);
	if (fd < 0) {
		log_error("monitor_open: mm_receive_fd () failed");
		return -1;
	}

	return fd;
}

int
monitor_setsockopt(int s, int level, int optname, const void *optval,
    socklen_t optlen)
{
	int	ret, err, cmd;

	cmd = MONITOR_SETSOCKOPT;
	must_write(&cmd, sizeof cmd);
	if (mm_send_fd(m_state.s, s)) {
		log_print("monitor_setsockopt: read/write error");
		return -1;
	}

	must_write(&level, sizeof level);
	must_write(&optname, sizeof optname);
	must_write(&optlen, sizeof optlen);
	must_write(optval, optlen);

	must_read(&err, sizeof err);
	must_read(&ret, sizeof ret);
	if (err != 0)
		errno = err;
	return ret;
}

int
monitor_bind(int s, const struct sockaddr *name, socklen_t namelen)
{
	int	ret, err, cmd;

	cmd = MONITOR_BIND;
	must_write(&cmd, sizeof cmd);
	if (mm_send_fd(m_state.s, s)) {
		log_print("monitor_bind: read/write error");
		return -1;
	}

	must_write(&namelen, sizeof namelen);
	must_write(name, namelen);

	must_read(&err, sizeof err);
	must_read(&ret, sizeof ret);
	if (err != 0)
		errno = err;
	return ret;
}

int
monitor_req_readdir(const char *filename)
{
	int cmd, err;
	size_t len;

	cmd = MONITOR_REQ_READDIR;
	must_write(&cmd, sizeof cmd);

	len = strlen(filename);
	must_write(&len, sizeof len);
	must_write(filename, len);

	must_read(&err, sizeof err);
	if (err == -1)
		must_read(&errno, sizeof errno);

	return err;
}

int
monitor_readdir(char *file, size_t size)
{
	int fd;
	size_t len;

	must_read(&len, sizeof len);
	if (len == 0)
		return -1;
	if (len >= size)
		log_fatal("monitor_readdir: received bad length from monitor");
	must_read(file, len);
	file[len] = '\0';
	fd = mm_receive_fd(m_state.s);
	return fd;
}

void
monitor_init_done(void)
{
	int	cmd;

	cmd = MONITOR_INIT_DONE;
	must_write(&cmd, sizeof cmd);
}

/*
 * Start of code running with privileges (the monitor process).
 */

static void
set_monitor_signals(void)
{
	int n;

	for (n = 1; n < _NSIG; n++)
		signal(n, SIG_DFL);

	/* Forward some signals to the child. */
	signal(SIGTERM, sig_pass_to_chld);
	signal(SIGHUP, sig_pass_to_chld);
	signal(SIGUSR1, sig_pass_to_chld);
}

static void
sig_pass_to_chld(int sig)
{
	int	oerrno = errno;

	if (m_state.pid > 0)
		kill(m_state.pid, sig);
	errno = oerrno;
}

/* This function is where the privileged process waits(loops) indefinitely.  */
void
monitor_loop(int debug)
{
	int	 msgcode, init_done = 0;

	if (!debug)
		log_to(0);

	for (;;) {
		must_read(&msgcode, sizeof msgcode);

		switch (msgcode) {
		case MONITOR_GET_FD:
			m_priv_getfd();
			break;

		case MONITOR_PFKEY_OPEN:
			LOG_DBG((LOG_MISC, 80,
			    "monitor_loop: MONITOR_PFKEY_OPEN"));
			if (init_done)
				log_fatal("monitor_loop: not allowed");
			m_priv_pfkey_open();
			break;

		case MONITOR_SETSOCKOPT:
			LOG_DBG((LOG_MISC, 80,
			    "monitor_loop: MONITOR_SETSOCKOPT"));
			m_priv_setsockopt();
			break;

		case MONITOR_BIND:
			LOG_DBG((LOG_MISC, 80,
			    "monitor_loop: MONITOR_BIND"));
			m_priv_bind();
			break;

		case MONITOR_REQ_READDIR:
			LOG_DBG((LOG_MISC, 80,
			    "monitor_loop: MONITOR_REQ_READDIR"));
			m_priv_req_readdir();
			break;

		case MONITOR_INIT_DONE:
			LOG_DBG((LOG_MISC, 80,
			    "monitor_loop: MONITOR_INIT_DONE"));
			if (init_done)
				log_fatal("monitor_loop: not allowed");
			init_done = 1;
			break;

		default:
			log_error("monitor_loop: got unknown code %d",
			    msgcode);
		}
	}

	exit(0);
}


/* Privileged: called by monitor_loop.  */
static void
m_priv_pfkey_open(void)
{
	int	fd, err = 0;

	fd = pf_key_v2_open();
	if (fd < 0)
		err = -1;

	must_write(&err, sizeof err);

	if (fd > 0 && mm_send_fd(m_state.s, fd)) {
		log_error("m_priv_pfkey_open: read/write operation failed");
		close(fd);
		return;
	}
	close(fd);
}

/* Privileged: called by monitor_loop.  */
static void
m_priv_getfd(void)
{
	char	path[PATH_MAX];
	struct stat sb;
	size_t	len;
	int	fd, flags;
	int	err = 0;

	must_read(&len, sizeof len);
	if (len == 0 || len >= sizeof path)
		log_fatal("m_priv_getfd: invalid pathname length");

	must_read(path, len);
	path[len] = '\0';
	if (strlen(path) != len)
		log_fatal("m_priv_getfd: invalid pathname");

	must_read(&flags, sizeof flags);
	if (flags != O_RDONLY)
		log_fatal("m_priv_getfd: invalid open flags");

	flags |= O_NOFOLLOW;
	if ((fd = open(path, flags)) == -1)
		err = errno;

	if (fd != -1 && (fstat(fd, &sb) == -1 || S_ISREG(sb.st_mode) == 0)) {
		log_error("m_priv_getfd: not an actual file %s", path);
		close(fd);
		fd = -1;
		err = EPERM;
	}

	must_write(&err, sizeof err);

	if (fd != -1) {
		if (mm_send_fd(m_state.s, fd) == -1)
			log_error("m_priv_getfd: sending fd failed");
		close(fd);
	}
}

/* Privileged: called by monitor_loop.  */
static void
m_priv_setsockopt(void)
{
	int		 sock, level, optname, v;
	int		 err = 0;
	char		*optval = 0;
	socklen_t	 optlen;

	sock = mm_receive_fd(m_state.s);
	if (sock < 0) {
		log_print("m_priv_setsockopt: read/write error");
		return;
	}

	must_read(&level, sizeof level);
	must_read(&optname, sizeof optname);
	must_read(&optlen, sizeof optlen);

	optval = malloc(optlen);
	if (!optval) {
		log_print("m_priv_setsockopt: malloc failed");
		close(sock);
		return;
	}

	must_read(optval, optlen);

	if (m_priv_check_sockopt(level, optname) != 0) {
		err = EACCES;
		v = -1;
	} else {
		v = setsockopt(sock, level, optname, optval, optlen);
		if (v < 0)
			err = errno;
	}

	close(sock);
	sock = -1;

	must_write(&err, sizeof err);
	must_write(&v, sizeof v);

	free(optval);
	return;
}

/* Privileged: called by monitor_loop.  */
static void
m_priv_bind(void)
{
	int		 sock, v, err = 0;
	struct sockaddr_storage name;
	socklen_t        namelen;

	sock = mm_receive_fd(m_state.s);
	if (sock < 0) {
		log_print("m_priv_bind: read/write error");
		return;
	}

	must_read(&namelen, sizeof namelen);
	if (namelen < sizeof(struct sockaddr) || namelen > sizeof name)
		log_fatalx("m_priv_bind: invalid address length");
	bzero(&name, sizeof name);
	must_read(&name, namelen);

	if (m_priv_check_bind((struct sockaddr *)&name, namelen) != 0) {
		err = EACCES;
		v = -1;
	} else {
		v = bind(sock, (struct sockaddr *)&name, namelen);
		if (v == -1) {
			log_error("m_priv_bind: bind(%d)", sock);
			err = errno;
		}
	}

	close(sock);
	sock = -1;

	must_write(&err, sizeof err);
	must_write(&v, sizeof v);
}

/*
 * Help functions, used by both privileged and unprivileged code
 */

/*
 * Read data with the assertion that it all must come through, or else abort
 * the process.  Based on atomicio() from openssh.
 */
static void
must_read(void *buf, size_t n)
{
        char *s = buf;
	size_t pos = 0;
        ssize_t res;

        while (n > pos) {
                res = read(m_state.s, s + pos, n - pos);
                switch (res) {
                case -1:
                        if (errno == EINTR || errno == EAGAIN)
                                continue;
                case 0:
			monitor_exit(0);
                default:
                        pos += res;
                }
        }
}

/*
 * Write data with the assertion that it all has to be written, or else abort
 * the process.  Based on atomicio() from openssh.
 */
static void
must_write(const void *buf, size_t n)
{
        const char *s = buf;
	size_t pos = 0;
	ssize_t res;

        while (n > pos) {
                res = write(m_state.s, s + pos, n - pos);
                switch (res) {
                case -1:
                        if (errno == EINTR || errno == EAGAIN)
                                continue;
                case 0:
			monitor_exit(0);
                default:
                        pos += res;
                }
        }
}

/* Check setsockopt */
static int
m_priv_check_sockopt(int level, int name)
{
	switch (level) {
		/* These are allowed */
		case SOL_SOCKET:
		case IPPROTO_IP:
		case IPPROTO_IPV6:
		break;

	default:
		log_print("m_priv_check_sockopt: Illegal level %d", level);
		return 1;
	}

	switch (name) {
		/* These are allowed */
	case SO_REUSEPORT:
	case SO_REUSEADDR:
	case IP_AUTH_LEVEL:
	case IP_ESP_TRANS_LEVEL:
	case IP_ESP_NETWORK_LEVEL:
	case IP_IPCOMP_LEVEL:
	case IPV6_AUTH_LEVEL:
	case IPV6_ESP_TRANS_LEVEL:
	case IPV6_ESP_NETWORK_LEVEL:
	case IPV6_IPCOMP_LEVEL:
		break;

	default:
		log_print("m_priv_check_sockopt: Illegal option name %d",
		    name);
		return 1;
	}

	return 0;
}

/* Check bind */
static int
m_priv_check_bind(const struct sockaddr *sa, socklen_t salen)
{
	in_port_t       port;

	if (sa == NULL) {
		log_print("NULL address");
		return 1;
	}
	if (SA_LEN(sa) != salen) {
		log_print("Length mismatch: %lu %lu", (unsigned long)sa->sa_len,
		    (unsigned long)salen);
		return 1;
	}
	switch (sa->sa_family) {
	case AF_INET:
		if (salen != sizeof(struct sockaddr_in)) {
			log_print("Invalid inet address length");
			return 1;
		}
		port = ((const struct sockaddr_in *)sa)->sin_port;
		break;
	case AF_INET6:
		if (salen != sizeof(struct sockaddr_in6)) {
			log_print("Invalid inet6 address length");
			return 1;
		}
		port = ((const struct sockaddr_in6 *)sa)->sin6_port;
		break;
	default:
		log_print("Unknown address family");
		return 1;
	}

	port = ntohs(port);

	if (port != ISAKMP_PORT_DEFAULT && port < 1024) {
		log_print("Disallowed port %u", port);
		return 1;
	}
	return 0;
}

static void
m_priv_req_readdir(void)
{
	size_t len;
	char dirpath[PATH_MAX], filepath[PATH_MAX];
	DIR *dp;
	struct dirent *file;
	struct stat sb;
	int fd, ret, serrno;

	must_read(&len, sizeof len);
	if (len == 0 || len >= sizeof dirpath)
		log_fatal("m_priv_req_readdir: invalid pathname length");
	must_read(dirpath, len);
	dirpath[len] = '\0';
	if (strlen(dirpath) != len)
		log_fatal("m_priv_req_readdir: invalid pathname");

	if ((dp = opendir(dirpath)) == NULL) {
		serrno = errno;
		ret = -1;
		must_write(&ret, sizeof ret);
		must_write(&serrno, sizeof serrno);
		return;
	}

	/* report opendir() success */
	ret = 0;
	must_write(&ret, sizeof ret);

	while ((file = readdir(dp)) != NULL) {
		if (strcmp(file->d_name, ".") == 0 ||
		    strcmp(file->d_name, "..") == 0)
			continue;
		fd = openat(dirfd(dp), file->d_name, O_RDONLY|O_NOFOLLOW);
		if (fd == -1) {
			log_error("m_priv_req_readdir: open "
			    "(\"%s\", O_RDONLY) failed", dirpath);
			close(fd);
			continue;
		}
		if (fstat(fd, &sb) == -1 || S_ISREG(sb.st_mode) == 0) {
			close(fd);
			fd = -1;
			continue;
		}

		if ((len = snprintf(filepath, sizeof filepath,
		    "%s/%s", dirpath, file->d_name)) >= sizeof(filepath)) {
			/* Long filenames are silently skipped */
			close(fd);
			continue;
		}

		must_write(&len, sizeof len);
		must_write(filepath, len);

		mm_send_fd(m_state.s, fd);
		close(fd);
	}
	closedir(dp);

	len = 0;
	must_write(&len, sizeof len);
}
