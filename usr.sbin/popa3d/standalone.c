/* $OpenBSD: standalone.c,v 1.4 2002/09/06 19:17:40 deraadt Exp $ */

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

typedef sig_atomic_t a_int;
typedef volatile a_int va_int;

/*
 * Active POP sessions. Those that were started within the last MIN_DELAY
 * seconds are also considered active (regardless of their actual state),
 * to allow for limiting the logging rate without throwing away critical
 * information about sessions that we could have allowed to proceed.
 */
static struct {
	struct in_addr addr;		/* Source IP address */
	a_int pid;			/* PID of the server, or 0 for none */
	clock_t start;			/* When the server was started */
	clock_t log;			/* When we've last logged a failure */
} sessions[MAX_SESSIONS];

static va_int child_blocked;		/* We use blocking to avoid races */
static va_int child_pending;		/* Are any dead children waiting? */

/*
 * SIGCHLD handler; can also be called directly with a zero signum.
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

	if (signum) signal(SIGCHLD, handle_child);

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
	int true = 1;
	int sock, new;
	struct sockaddr_in addr;
	socklen_t addrlen;
	pid_t pid;
	struct tms buf;
	clock_t now;
	int i, j, n;

	if (do_pop_startup()) return 1;

	if ((sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
		return log_error("socket");

	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,
	    (void *)&true, sizeof(true)))
		return log_error("setsockopt");

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr(DAEMON_ADDR);
	addr.sin_port = htons(DAEMON_PORT);
	if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)))
		return log_error("bind");

	if (listen(sock, MAX_BACKLOG))
		return log_error("listen");

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
	new = 0;

	while (1) {
		child_blocked = 0;
		if (child_pending) handle_child(0);

		if (new > 0)
		if (close(new)) return log_error("close");

		addrlen = sizeof(addr);
		new = accept(sock, (struct sockaddr *)&addr, &addrlen);

/*
 * I wish there was a portable way to classify errno's... In this case,
 * it appears to be better to risk eating up the CPU on a fatal error
 * rather than risk terminating the entire service because of a minor
 * temporary error having to do with one particular connection attempt.
 */
		if (new < 0) continue;

		now = times(&buf);

		child_blocked = 1;

		j = -1; n = 0;
		for (i = 0; i < MAX_SESSIONS; i++) {
			if (sessions[i].start > now)
				sessions[i].start = 0;
			if (sessions[i].pid ||
			    (sessions[i].start &&
			    now - sessions[i].start < MIN_DELAY * CLK_TCK)) {
				if (sessions[i].addr.s_addr ==
				    addr.sin_addr.s_addr)
				if (++n >= MAX_SESSIONS_PER_SOURCE) break;
			} else
			if (j < 0) j = i;
		}

		if (n >= MAX_SESSIONS_PER_SOURCE) {
			if (!sessions[i].log ||
			    now < sessions[i].log ||
			    now - sessions[i].log >= MIN_DELAY * CLK_TCK) {
				syslog(SYSLOG_PRI_HI,
					"%s: per source limit reached",
					inet_ntoa(addr.sin_addr));
				sessions[i].log = now;
			}
			continue;
		}

		if (j < 0) {
			syslog(SYSLOG_PRI_HI, "%s: sessions limit reached",
				inet_ntoa(addr.sin_addr));
			continue;
		}

		switch ((pid = fork())) {
		case -1:
			syslog(SYSLOG_PRI_ERROR, "%s: fork: %m",
				inet_ntoa(addr.sin_addr));
			break;

		case 0:
			if (close(sock)) return log_error("close");
#if DAEMON_LIBWRAP
			check_access(new);
#endif
			syslog(SYSLOG_PRI_LO, "Session from %s",
				inet_ntoa(addr.sin_addr));
			if (dup2(new, 0) < 0) return log_error("dup2");
			if (dup2(new, 1) < 0) return log_error("dup2");
			if (dup2(new, 2) < 0) return log_error("dup2");
			if (close(new)) return log_error("close");
			return do_pop_session();

		default:
			sessions[j].addr = addr.sin_addr;
			sessions[j].pid = pid;
			sessions[j].start = now;
			sessions[j].log = 0;
		}
	}
}

#endif
