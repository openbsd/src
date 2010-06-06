/*	$OpenBSD: identd.c,v 1.49 2010/06/06 07:05:40 jmc Exp $	*/

/*
 * This program is in the public domain and may be used freely by anyone
 * who wants to.
 *
 * Please send bug fixes/bug reports to: Peter Eriksson <pen@lysator.liu.se>
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <sys/time.h>
#include <sys/wait.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <stdio.h>
#include <stdlib.h>
#include <poll.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <syslog.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>

#include "identd.h"

extern char *__progname;

int	af = PF_UNSPEC;

int	verbose_flag;
int	debug_flag;
int	syslog_flag;
int	multi_flag;
int	unknown_flag;
int	number_flag;
int	noident_flag;
int	userident_flag;
int	token_flag;
int	no_user_token_flag;

int	lport;
int	fport;

const  char *opsys_name = "UNIX";
const  char *charset_sep = "";
char   *charset_name = "";

static pid_t child_pid;

void		usage(void);
void		sigchld(int);
char *		gethost(struct sockaddr_storage *ss);

void
usage(void)
{
	syslog(LOG_ERR,
	    "usage: %s [-46deHhlmNnoUv] [-b | -i | -w] [-a address] [-c charset] "
	    "[-g gid] [-p port] [-t seconds] [-u uid]", __progname);
	exit(2);
}

/*
 * Return the name of the connecting host, or the IP number as a string.
 */
char *
gethost4_addr(struct in_addr *addr)
{
	struct hostent *hp;

	hp = gethostbyaddr(addr, sizeof(struct in_addr), AF_INET);
	if (hp)
		return hp->h_name;
	return inet_ntoa(*addr);
}

char *
gethost(struct sockaddr_storage *ss)
{
	if (ss->ss_family == AF_INET6)
		return (gethost6((struct sockaddr_in6 *)ss));
	return (gethost4((struct sockaddr_in *)ss));
}

char *
gethost4(struct sockaddr_in *sin)
{
	struct hostent *hp;

	hp = gethostbyaddr(&sin->sin_addr, sizeof(struct in_addr), AF_INET);
	if (hp)
		return hp->h_name;
	return inet_ntoa(sin->sin_addr);
}

/*
 * Return the name of the connecting host, or the IP number as a string.
 */
char *
gethost6(struct sockaddr_in6 *addr)
{
	static char hbuf[2][NI_MAXHOST];
	const int niflags = NI_NUMERICHOST;
	static int bb = 0;
	int err;

	bb = (bb+1)%2;
	err = getnameinfo((struct sockaddr *)addr, addr->sin6_len,
	    hbuf[bb], sizeof(hbuf[bb]), NULL, 0, niflags);
	if (err != 0) {
		syslog(LOG_ERR, "getnameinfo failed (%s)", gai_strerror(err));
		strlcpy(hbuf[bb], "UNKNOWN", sizeof(hbuf[bb]));
	}
	return(hbuf[bb]);
}

volatile sig_atomic_t alarm_fired;

/*
 * Exit cleanly after our time's up.
 */
/* ARGSUSED */
static void
alarm_handler(int notused)
{
	alarm_fired = 1;
}

/*
 * Main entry point into this daemon
 */
int
main(int argc, char *argv[])
{
	struct sockaddr_storage sa, sa2;
	struct sockaddr_in *sin;
	struct sockaddr_in6 *sin6;
	struct in_addr laddr, faddr;
	struct in6_addr laddr6, faddr6;
	struct passwd *pwd;
	struct group *grp;
	struct pollfd *pfds = NULL;
	int	i, n = 0, background_flag = 0, timeout = 0, ch;
	char   *portno = "auth";
	char   *bind_address = NULL;
	uid_t   set_uid = 0;
	gid_t   set_gid = 0;
	extern char *optarg;
	socklen_t len;
	const char *errstr;

	openlog(__progname, LOG_PID, LOG_DAEMON);

	/* runs as _identd if possible, fallback to "nobody" */
	if (getuid() == 0) {
		if ((pwd = getpwnam(DEFAULT_UID)) == NULL)
			error("no such user %s", DEFAULT_UID);
		set_uid = pwd->pw_uid;
		set_gid = pwd->pw_gid;
	}

	/*
	 * Parse the command line arguments
	 */
	while ((ch = getopt(argc, argv, "46hHbwit:p:a:u:g:c:loenvdmNU")) != -1) {
		switch (ch) {
		case '4':
			af = AF_INET;
			break;
		case '6':
			af = AF_INET6;
			break;
		case 'h':
			token_flag = 1;
			break;
		case 'H':
			no_user_token_flag = token_flag = 1;
			break;
		case 'b':	/* Start as standalone daemon */
			background_flag = 1;
			break;
		case 'w':	/* Start from Inetd, wait mode */
			background_flag = 2;
			break;
		case 'i':	/* Start from Inetd, nowait mode */
			background_flag = 0;
			break;
		case 't':
			timeout = strtonum(optarg, 0, 100000000, &errstr);
			if (errstr)
				error("timeout is %s: %s", errstr, optarg);
			break;
		case 'p':
			portno = optarg;
			break;
		case 'a':
			bind_address = optarg;
			break;
		case 'u':
			pwd = getpwnam(optarg);
			if (pwd == NULL && isdigit(optarg[0])) {
				set_uid = atoi(optarg);
				if ((pwd = getpwuid(set_uid)) == NULL)
					break;
			}
			if (pwd == NULL)
				error("no such user (%s) for -u option",
				    optarg);
			else {
				set_uid = pwd->pw_uid;
				if (set_gid == 0)
					set_gid = pwd->pw_gid;
			}
			break;
		case 'g':
			grp = getgrnam(optarg);
			if (grp == NULL && isdigit(optarg[0])) {
				set_gid = atoi(optarg);
				break;
			}
			grp = getgrnam(optarg);
			if (!grp)
				error("no such group (%s) for -g option", optarg);
			else
				set_gid = grp->gr_gid;
			break;
		case 'c':
			charset_name = optarg;
			charset_sep = " , ";
			break;
		case 'l':	/* Use the Syslog daemon for logging */
			syslog_flag++;
			break;
		case 'o':
			opsys_name = "OTHER";
			break;
		case 'e':
			unknown_flag = 1;
			break;
		case 'n':
			number_flag = 1;
			break;
		case 'v':	/* Be verbose */
			verbose_flag++;
			break;
		case 'd':	/* Enable debugging */
			debug_flag++;
			break;
		case 'm':	/* Enable multiline queries */
			multi_flag++;
			break;
		case 'N':	/* Enable users ".noident" files */
			noident_flag++;
			break;
		case 'U':	/* Enable user ".ident" files */
			userident_flag++;
			break;
		default:
			usage();
		}
	}

	/*
	 * Do the special handling needed for the "-b" flag
	 */
	if (background_flag == 1) {
		struct addrinfo hints, *res, *res0;
		int	true = 1;

		if (daemon(0, 0) != 0)
			exit(0);

		memset(&hints, 0, sizeof(hints));
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_family = af;
		hints.ai_flags = AI_PASSIVE;
		if (getaddrinfo(bind_address, portno, &hints, &res0) != 0)
			error("main: getaddrinfo");

		i = 0;
		for (res = res0; res; res = res->ai_next)
			i++;

		pfds = calloc(i, sizeof(pfds[0]));
		if (!pfds) {
			freeaddrinfo(res0);
			error("main: calloc");
		}

		i = 0;
		for (res = res0; res; res = res->ai_next) {
			if ((pfds[i].fd = socket(res->ai_family, res->ai_socktype,
			    res->ai_protocol)) < 0)
				continue;

			if (setsockopt(pfds[i].fd, SOL_SOCKET, SO_REUSEADDR,
			    (void *)&true, sizeof(true))) {
				close(pfds[i].fd);
				continue;
			}

#ifdef IPV6_V6ONLY
			if (res->ai_family == AF_INET6)
				(void)setsockopt(pfds[i].fd, IPPROTO_IPV6,
				    IPV6_V6ONLY, (void *)&true, sizeof(true));
#endif

			if (bind(pfds[i].fd, res->ai_addr, res->ai_addrlen)) {
				close(pfds[i].fd);
				continue;
			}

			if (listen(pfds[i].fd, 3)) {
				close(pfds[i].fd);
				continue;
			}

			pfds[i].events = POLLIN;
			i++;
		}
		freeaddrinfo(res0);

		if (i == 0)
			error("main: socket");

		n = i;
	}

	/*
	 * Do the special handling needed for the "-w" flag
	 */
	if (background_flag == 2) {
		pfds = calloc(1, sizeof(pfds[0]));
		if (!pfds)
			error("main: calloc");

		pfds[0].fd = 0;
		pfds[0].events = POLLIN;
		n = 1;
	}

	if (set_gid) {
		if (setegid(set_gid) == -1)
			error("main: setegid");
		if (setgid(set_gid) == -1)
			error("main: setgid");
	}
	if (set_uid) {
		if (seteuid(set_uid) == -1)
			error("main: seteuid");
		if (setuid(set_uid) == -1)
			error("main: setuid");
	}

	/*
	 * Do some special handling if the "-b" or "-w" flags are used
	 */
	if (background_flag) {
		int	fd = 0;

		signal(SIGCHLD, sigchld);

		/*
		 * Loop and dispatch client handling processes
		 */
		do {
			/*
			 * Terminate if we've been idle for 'timeout' seconds
			 */
			if (background_flag == 2 && timeout) {
				signal(SIGALRM, alarm_handler);
				alarm(timeout);
			}

			/*
			 * Wait for a connection request to occur.
			 * Ignore EINTR (Interrupted System Call).
			 */
			do {
				if (alarm_fired) {
					if (syslog_flag)
						syslog(LOG_DEBUG,
						    "SIGALRM triggered, exiting");
					exit(0);
				}

				if (timeout)
					i = poll(pfds, n, timeout * 1000);
				else
					i = poll(pfds, n, INFTIM);
			} while (i < 0 && errno == EINTR);

			/*
			 * An error occurred in poll? Just die
			 */
			if (i < 0)
				error("main: poll");

			/*
			 * Timeout limit reached. Exit nicely
			 */
			if (i == 0)
				exit(0);

			/*
			 * Disable the alarm timeout
			 */
			alarm(0);

			for (i = 0; i < n; i++) {
				if ((pfds[i].revents & POLLIN) == 0)
					continue;

				/*
				 * Accept the new client
				 */
				fd = accept(pfds[i].fd, NULL, NULL);
				if (fd == -1)
					error("main: accept. errno = %d", errno);

				/*
				 * Fork a child, parent continues
				 */
				child_pid = fork();
				if (child_pid == 0)
					break;

				close(fd);
			}
		} while (child_pid != 0);

		/*
		 * We are now in child, the parent has returned to "do" above.
		 */
		if (dup2(fd, 0) == -1)
			error("main: dup2: failed fd 0");

		if (dup2(fd, 1) == -1)
			error("main: dup2: failed fd 1");

		if (dup2(fd, 2) == -1)
			error("main: dup2: failed fd 2");
	}

	/*
	 * Get foreign internet address
	 */
	len = sizeof(sa);
	if (getpeername(0, (struct sockaddr *) &sa, &len) == -1) {
		/*
		 * A user has tried to start us from the command line or
		 * the network link died, in which case this message won't
		 * reach to other end anyway, so lets give the poor user some
		 * errors.
		 */
		perror("identd: getpeername()");
		exit(1);
	}
	if (sa.ss_family == AF_INET6) {
		sin6 = (struct sockaddr_in6 *)&sa;
		faddr6 = sin6->sin6_addr;
	} else {
		sin = (struct sockaddr_in *)&sa;
		faddr = sin->sin_addr;
	}

	/*
	 * Open the connection to the Syslog daemon if requested
	 */
	if (syslog_flag)
		syslog(LOG_INFO, "Connection from %s", gethost(&sa));

	/*
	 * Get local internet address
	 */
	len = sizeof(sa2);
	if (getsockname(0, (struct sockaddr *) &sa2, &len) == -1) {
		/*
		 * We can just die here, because if this fails then the
		 * network has died and we haven't got anyone to return
		 * errors to.
		 */
		exit(1);
	}
	/* are we v4 or v6? */
	if (sa2.ss_family == AF_INET6) {
		sin6 = (struct sockaddr_in6 *)&sa2;
		laddr6 = sin6->sin6_addr;
		/*
		 * Get the local/foreign port pair from the luser
		 */
		parse6(STDIN_FILENO, (struct sockaddr_in6 *)&sa2,
		    (struct sockaddr_in6 *)&sa);
	} else {
		sin = (struct sockaddr_in *)&sa2;
		laddr = sin->sin_addr;
		/*
		 * Get the local/foreign port pair from the luser
		 */
		parse(STDIN_FILENO, &laddr, &faddr);
	}

	exit(0);
}

void
error(char *fmt, ...)
{
	va_list ap, ap2;

	va_start(ap, fmt);
	
	if (syslog_flag) {
		va_copy(ap2, ap);
		vsyslog(LOG_ERR, fmt, ap2);
		va_end(ap2);
	}
	if (debug_flag) {
		fprintf(stderr, "%d , %d : ERROR : X-DBG : ", lport, fport);
		vfprintf(stderr, fmt, ap);
		perror(": ");
	} else
		printf("%d , %d : ERROR : UNKNOWN-ERROR\r\n", lport, fport);
	va_end(ap);
	exit(1);
}

void
sigchld(int signo)
{
	pid_t pid;

	do {
	    pid = waitpid(-1, NULL, WNOHANG);
	} while (pid > 0 || (pid == -1 && errno == EINTR));
}
