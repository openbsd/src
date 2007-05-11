/* $OpenBSD: standalone.c,v 1.11 2007/05/11 01:47:48 ray Exp $ */

/*
 * Standalone POP server: accepts connections, checks the anti-flood limits,
 * logs and starts the actual POP sessions.
 */

#include "params.h"

#if POP_STANDALONE

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <syslog.h>
#include <time.h>
#include <errno.h>
#include <netdb.h>
#include <poll.h>
#include <sys/times.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#if DAEMON_LIBWRAP
#include <tcpd.h>
int allow_severity = SYSLOG_PRI_LO;
int deny_severity = SYSLOG_PRI_HI;
#endif

/*
 * These are defined in pop_root.c.
 */
extern int log_error(char *s);
extern int do_pop_startup(void);
extern int do_pop_session(void);
extern int af;

typedef volatile sig_atomic_t va_int;

/*
 * Active POP sessions. Those that were started within the last MIN_DELAY
 * seconds are also considered active (regardless of their actual state),
 * to allow for limiting the logging rate without throwing away critical
 * information about sessions that we could have allowed to proceed.
 */
static struct {
	char addr[NI_MAXHOST];		/* Source IP address */
	va_int pid;			/* PID of the server, or 0 for none */
	clock_t start;			/* When the server was started */
	clock_t log;			/* When we've last logged a failure */
} sessions[MAX_SESSIONS];

static va_int child_blocked;		/* We use blocking to avoid races */
static va_int child_pending;		/* Are any dead children waiting? */

int handle(int);

/*
 * SIGCHLD handler.
 */
static void handle_child(int signum)
{
	int saved_errno;
	pid_t pid;
	int i;

	saved_errno = errno;

	if (child_blocked)
		child_pending = 1;
	else {
		child_pending = 0;

		while ((pid = waitpid(0, NULL, WNOHANG)) > 0)
			for (i = 0; i < MAX_SESSIONS; i++)
				if (sessions[i].pid == pid) {
					sessions[i].pid = 0;
					break;
				}
	}

	signal(SIGCHLD, handle_child);

	errno = saved_errno;
}

#if DAEMON_LIBWRAP
static void check_access(int sock)
{
	struct request_info request;

	request_init(&request,
		RQ_DAEMON, DAEMON_LIBWRAP_IDENT,
		RQ_FILE, sock,
		0);
	fromhost(&request);

	if (!hosts_access(&request)) {
/* refuse() shouldn't return... */
		refuse(&request);
/* ...but just in case */
		exit(1);
	}
}
#endif

#if POP_OPTIONS
int do_standalone(void)
#else
int main(void)
#endif
{
	int error, i, n, true = 1;
	struct pollfd *pfds;
	struct addrinfo hints, *res, *res0;
	char sbuf[NI_MAXSERV];

	if (do_pop_startup()) return 1;

	snprintf(sbuf, sizeof(sbuf), "%u", DAEMON_PORT);
	memset(&hints, 0, sizeof(hints));
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_family = af;
	hints.ai_flags = AI_PASSIVE;
	error = getaddrinfo(NULL, sbuf, &hints, &res0);
	if (error)
		return log_error("getaddrinfo");

	i = 0;
	for (res = res0; res; res = res->ai_next)
		i++;

	pfds = calloc(i, sizeof(pfds[0]));
	if (!pfds) {
		freeaddrinfo(res0);
		return log_error("malloc");
	}

	i = 0;
	for (res = res0; res; res = res->ai_next) {
		if ((pfds[i].fd = socket(res->ai_family, res->ai_socktype,
		    res->ai_protocol)) < 0)
			continue;
		pfds[i].events = POLLIN;

		if (setsockopt(pfds[i].fd, SOL_SOCKET, SO_REUSEADDR,
		    (void *)&true, sizeof(true))) {
			close(pfds[i].fd);
			continue;
		}

#ifdef IPV6_V6ONLY
		if (res->ai_family == AF_INET6)
			(void)setsockopt(pfds[i].fd, IPPROTO_IPV6, IPV6_V6ONLY,
			    (void *)&true, sizeof(true));
#endif

		if (bind(pfds[i].fd, res->ai_addr, res->ai_addrlen)) {
			close(pfds[i].fd);
			continue;
		}

		if (listen(pfds[i].fd, MAX_BACKLOG)) {
			close(pfds[i].fd);
			continue;
		}

		i++;
	}
	freeaddrinfo(res0);

	if (i == 0)
		return log_error("socket");

	n = i;

	chdir("/");
	setsid();

	switch (fork()) {
	case -1:
		return log_error("fork");

	case 0:
		break;

	default:
		return 0;
	}

	setsid();

	child_blocked = 1;
	child_pending = 0;
	signal(SIGCHLD, handle_child);

	memset((void *)sessions, 0, sizeof(sessions));

	while (1) {
		child_blocked = 0;
		if (child_pending) raise(SIGCHLD);

		i = poll(pfds, n, INFTIM);
		if (i < 0) {
			if (errno == EINTR || errno == EAGAIN)
				continue;
			return log_error("poll");
		}

		for (i = 0; i < n; i++)
			if (pfds[i].revents & POLLIN)
				handle(pfds[i].fd);
	}
}

int
handle(int sock)
{
	clock_t now, log;
	int new;
	char hbuf[NI_MAXHOST];
	struct sockaddr_storage addr;
	socklen_t addrlen;
	pid_t pid;
	struct tms buf;
	int error;
	int j, n, i;

	log = 0;
	new = 0;

	addrlen = sizeof(addr);
	new = accept(sock, (struct sockaddr *)&addr, &addrlen);
/*
 * I wish there was a portable way to classify errno's... In this case,
 * it appears to be better to risk eating up the CPU on a fatal error
 * rather than risk terminating the entire service because of a minor
 * temporary error having to do with one particular connection attempt.
 */
	if (new < 0)
		return -1;

	error = getnameinfo((struct sockaddr *)&addr, addrlen,
	    hbuf, sizeof(hbuf), NULL, 0, NI_NUMERICHOST);
	if (error) {
		syslog(SYSLOG_PRI_HI,
		    "could not get host address");
		close(new);
		return -1;
	}

	now = times(&buf);
	if (!now)
		now = 1;

	child_blocked = 1;

	j = -1;
	n = 0;
	for (i = 0; i < MAX_SESSIONS; i++) {
		if (sessions[i].start > now)
			sessions[i].start = 0;
		if (sessions[i].pid ||
		    (sessions[i].start &&
		    now - sessions[i].start < MIN_DELAY * CLK_TCK)) {
			if (strcmp(sessions[i].addr, hbuf) == 0)
				if (++n >= MAX_SESSIONS_PER_SOURCE)
					break;
		} else if (j < 0)
			j = i;
	}

	if (n >= MAX_SESSIONS_PER_SOURCE) {
		if (!sessions[i].log ||
		    now < sessions[i].log ||
		    now - sessions[i].log >= MIN_DELAY * CLK_TCK) {
			syslog(SYSLOG_PRI_HI,
				"%s: per source limit reached",
				hbuf);
			sessions[i].log = now;
		}
		close(new);
		return -1;
	}

	if (j < 0) {
		if (!log ||
		    now < log || now - log >= MIN_DELAY * CLK_TCK) {
			syslog(SYSLOG_PRI_HI,
			    "%s: sessions limit reached", hbuf);
			log = now;
		}
		close(new);
		return -1;
	}

	switch ((pid = fork())) {
	case -1:
		syslog(SYSLOG_PRI_ERROR, "%s: fork: %m", hbuf);
		close(new);
		return -1;

	case 0:
#if DAEMON_LIBWRAP
		check_access(new);
#endif
		syslog(SYSLOG_PRI_LO, "Session from %s",
			hbuf);
		if (dup2(new, 0) < 0 || dup2(new, 1) < 0 || dup2(new, 2) < 0) {
			log_error("dup2");
			_exit(1);
		}
		closefrom(3);
		_exit(do_pop_session());

	default:
		close(new);
		strlcpy(sessions[j].addr, hbuf,
			sizeof(sessions[j].addr));
		sessions[j].pid = (va_int)pid;
		sessions[j].start = now;
		sessions[j].log = 0;
		return 0;
	}
}

#endif
