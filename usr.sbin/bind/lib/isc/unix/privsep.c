/*	$OpenBSD: privsep.c,v 1.4 2004/09/28 17:14:07 jakob Exp $	*/

/*
 * Copyright (c) 2004 Henning Brauer <henning@openbsd.org>
 * Copyright (c) 2004 Can Erkin Acar <canacar@openbsd.org>
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

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>

#include <netinet/in.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>

#include <isc/buffer.h>
#include <isc/mem.h>
#include <isc/privsep.h>
#include <isc/string.h>
#include <isc/util.h>

enum priv_state {
	STATE_RUN,
	STATE_QUIT
};

/* allowed privileged port numbers */
#define		NAMED_PORT_DEFAULT	53
#define		RNDC_PORT_DEFAULT	953
#define		LWRES_PORT_DEFAULT	921

int		debug_level = LOG_DEBUG;
int		log_stderr = 1;
int		priv_fd = -1;

static volatile	pid_t child_pid = -1;
static volatile sig_atomic_t cur_state = STATE_RUN;

static int	check_bind(const struct sockaddr *, socklen_t);
static void	fatal(const char *);
static void	logmsg(int, const char *, ...);
static void	parent_bind(int);
static void	sig_pass_to_chld(int);
static void	sig_got_chld(int);
static void	write_command(int, int);

int
isc_priv_init(int lstderr)
{
	int i, socks[2], cmd;

	logmsg(LOG_NOTICE, "Starting privilege seperation");

	log_stderr = lstderr;

	/* Create sockets */
	if (socketpair(AF_LOCAL, SOCK_STREAM, PF_UNSPEC, socks) == -1)
		fatal("socketpair() failed");

	switch (child_pid = fork()) {
	case -1:
		fatal("failed to fork() for privsep");
	case 0:
		close(socks[0]);
		priv_fd = socks[1];
		return (0);
	default:
		break;
	}

	for (i = 1; i < _NSIG; i++)
		signal(i, SIG_DFL);

	signal(SIGALRM, sig_pass_to_chld);
	signal(SIGTERM, sig_pass_to_chld);
	signal(SIGHUP,  sig_pass_to_chld);
	signal(SIGINT,  sig_pass_to_chld);
	signal(SIGCHLD, sig_got_chld);

	/* Father - close unneeded sockets */
	for (i = STDERR_FILENO + 1; i < socks[0]; i++)
		close(i);
	closefrom(socks[0] + 1);

	setproctitle("[priv]");

	while (cur_state != STATE_QUIT) {
		if (may_read(socks[0], &cmd, sizeof(int)))
			break;
		switch (cmd) {
		case PRIV_BIND:
			parent_bind(socks[0]);
			break;
		default:
			logmsg(LOG_ERR, "[priv]: unknown command %d", cmd);
			_exit(1);
			/* NOTREACHED */
		}
	}

	_exit(0);
}

int
isc_drop_privs(const char *username)
{
	struct passwd *pw;
	
	if ((pw = getpwnam(username)) == NULL) {
		logmsg(LOG_ERR, "unknown user %s", username);
		exit(1);
	}

	if (chroot(pw->pw_dir) == -1)
		fatal("chroot failed");

	if (chdir("/"))
		fatal("chdir failed");

	if (setgroups(1, &pw->pw_gid) ||
	    setegid(pw->pw_gid) || setgid(pw->pw_gid) ||
	    seteuid(pw->pw_uid) || setuid(pw->pw_uid))
		fatal("can't drop privileges");

	endpwent();
	return (0);
}

static int
check_bind(const struct sockaddr *sa, socklen_t salen)
{
	const char *pname = child_pid ? "[priv]" : "[child]";
	in_port_t port;

	if (sa == NULL) {
		logmsg(LOG_ERR, "%s: NULL address", pname);
		return (1);
	}

	if (sa->sa_len != salen) {
		logmsg(LOG_ERR, "%s: length mismatch: %d %d", pname,
		    (int) sa->sa_len, (int) salen);
		return (1);
	}

	switch (sa->sa_family) {
	case AF_INET:
		if (salen != sizeof(struct sockaddr_in)) {
			logmsg(LOG_ERR, "%s: Invalid inet address length",
			    pname);
			return (1);
		}
		port = ((const struct sockaddr_in *)sa)->sin_port;
		break;
	case AF_INET6:
		if (salen != sizeof(struct sockaddr_in6)) {
			logmsg(LOG_ERR, "%s: Invalid inet6 address length",
			    pname);
			return (1);
		}
		port = ((const struct sockaddr_in6 *)sa)->sin6_port;
		break;
	default:
		logmsg(LOG_ERR, "%s: unknown address family", pname);
		return (1);
	}

	port = ntohs(port);

	if (port != NAMED_PORT_DEFAULT && port != RNDC_PORT_DEFAULT &&
	    port != LWRES_PORT_DEFAULT) {
		if (port || child_pid)
			logmsg(LOG_ERR, "%s: disallowed port %u", pname, port);
		return (1);
	}

	return (0);
}
	
static void
parent_bind(int fd)
{
	int sock, status;
	struct sockaddr_storage ss;
	socklen_t sslen;
	int er;

	logmsg(LOG_DEBUG, "[priv]: msg PRIV_BIND received");

	sock = receive_fd(fd);
	must_read(fd, &sslen, sizeof(sslen));
	if (sslen == 0 || sslen > sizeof(ss))
		_exit(1);

	must_read(fd, &ss, sslen);

	if (check_bind((struct sockaddr *) &ss, sslen))
		_exit(1);

	status = bind(sock, (struct sockaddr *)&ss, sslen);
	er = errno;
	must_write(fd, &er, sizeof(er));
	must_write(fd, &status, sizeof(status));

	if (sock >= 0)
		close(sock);
}

/* Bind to allowed privileged ports using privsep, or try to bind locally */
int
isc_priv_bind(int fd, struct sockaddr *sa, socklen_t salen)
{
	int status, er;

	if (priv_fd < 0)
		errx(1, "%s called from privileged portion", __func__);

	if (check_bind(sa, salen)) {
		logmsg(LOG_DEBUG, "Binding locally");
		status = bind(fd, sa, salen);
	} else {
		logmsg(LOG_DEBUG, "Binding privsep");
		write_command(priv_fd, PRIV_BIND);
		send_fd(priv_fd, fd);
		must_write(priv_fd, &salen, sizeof(salen));
		must_write(priv_fd, sa, salen);
		must_read(priv_fd, &er, sizeof(er));
		must_read(priv_fd, &status, sizeof(status));
		errno = er;
	}

	return (status);
}

/* If priv parent gets a TERM or HUP, pass it through to child instead */
static void
sig_pass_to_chld(int sig)
{
	int save_err = errno;

	if (child_pid != -1)
		kill(child_pid, sig);
	errno = save_err;
}


/* When child dies, move into the shutdown state */
static void
sig_got_chld(int sig)
{
	pid_t pid;
	int status;
	int save_err = errno;

	do {
		pid = waitpid(child_pid, &status, WNOHANG);
	} while (pid == -1 && errno == EINTR);

	if (pid == child_pid && (WIFEXITED(status) || WIFSIGNALED(status)) &&
	    cur_state < STATE_QUIT)
		cur_state = STATE_QUIT;

	errno = save_err;
}

/* Read all data or return 1 for error.  */
int
may_read(int fd, void *buf, size_t n)
{
	char *s = buf;
	ssize_t res;
	size_t pos = 0;

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
void
must_read(int fd, void *buf, size_t n)
{
	char *s = buf;
	ssize_t res;
	size_t pos = 0;

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
void
must_write(int fd, const void *buf, size_t n)
{
	const char *s = buf;
	ssize_t res;
	size_t pos = 0;

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


/* write a command to the peer */
static void
write_command(int fd, int cmd)
{
	must_write(fd, &cmd, sizeof(cmd));
}

static void
logmsg(int pri, const char *message, ...)
{
	va_list ap;
	if (pri > debug_level)
		return;

	va_start(ap, message);
	if (log_stderr) {
		vfprintf(stderr, message, ap);
		fprintf(stderr, "\n");
	} else
		vsyslog(pri, message, ap);

	va_end(ap);
}

/* from bgpd */
static void
fatal(const char *emsg)
{
	const char *pname;

	if (child_pid == -1)
		pname = "bind";
	else if (child_pid)
		pname = "bind [priv]";
	else
		pname = "bind [child]";

	if (emsg == NULL)
		logmsg(LOG_CRIT, "fatal in %s: %s", pname, strerror(errno));
	else
		if (errno)
			logmsg(LOG_CRIT, "fatal in %s: %s: %s",
			    pname, emsg, strerror(errno));
		else
			logmsg(LOG_CRIT, "fatal in %s: %s", pname, emsg);

	if (child_pid)
		_exit(1);
	else				/* parent copes via SIGCHLD */
		exit(1);
}
