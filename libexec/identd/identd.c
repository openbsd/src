/*
 * This program is in the public domain and may be used freely by anyone
 * who wants to.
 *
 * Please send bug fixes/bug reports to: Peter Eriksson <pen@lysator.liu.se>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <sys/time.h>
#include <sys/wait.h>

#include <pwd.h>
#include <grp.h>

#include <netinet/in.h>

#include <arpa/inet.h>

extern int errno;

#include "identd.h"
#include "error.h"

extern char *version;

int     verbose_flag = 0;
int     debug_flag = 0;
int     syslog_flag = 0;
int     multi_flag = 0;
int     other_flag = 0;
int     unknown_flag = 0;
int     number_flag = 0;
int     noident_flag = 0;

int     lport = 0;
int     fport = 0;

char   *charset_name = NULL;
char   *indirect_host = NULL;
char   *indirect_password = NULL;

static int child_pid;

#ifdef LOG_DAEMON
static int syslog_facility = LOG_DAEMON;
#endif

/*
 * Return the name of the connecting host, or the IP number as a string.
 */
char   *
gethost(addr)
	struct in_addr *addr;
{
	struct hostent *hp;

	hp = gethostbyaddr((char *) addr, sizeof(struct in_addr), AF_INET);
	if (hp)
		return hp->h_name;
	else
		return inet_ntoa(*addr);
}

/*
 * Exit cleanly after our time's up.
 */
static void
alarm_handler()
{
	if (syslog_flag)
		syslog(LOG_DEBUG, "SIGALRM triggered, exiting");
	exit(0);
}

/*
 * Main entry point into this daemon
 */
int 
main(argc, argv)
	int     argc;
	char   *argv[];
{
	int     i, len;
	struct sockaddr_in sin;
	struct in_addr laddr, faddr;
	struct timeval tv;
	struct passwd *pwd;
	struct group *grp;
	int     background_flag = 0;
	int     timeout = 0;
	char   *portno = "auth";
	char   *bind_address = NULL;
	int     set_uid = 0;
	int     set_gid = 0;
	int     opt_count = 0;	/* Count of option flags */

	/*
	 * Parse the command line arguments
	 */
	for (i = 1; i < argc && argv[i][0] == '-'; i++) {
		opt_count++;
		switch (argv[i][1]) {
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
			timeout = atoi(argv[i] + 2);
			break;
		case 'p':
			portno = argv[i] + 2;
			break;
		case 'a':
			bind_address = argv[i] + 2;
			break;
		case 'u':
			if (isdigit(argv[i][2])) {
				set_uid = atoi(argv[i] + 2);
				break;
			}
			pwd = getpwnam(argv[i] + 2);
			if (!pwd)
				ERROR1("no such user (%s) for -u option", argv[i] + 2);
			else {
				set_uid = pwd->pw_uid;
				if (setgid == 0)
				  set_gid = pwd->pw_gid;
			}
			break;
		case 'g':
			if (isdigit(argv[i][2])) {
				set_gid = atoi(argv[i] + 2);
				break;
			}
			grp = getgrnam(argv[i] + 2);
			if (!grp)
				ERROR1("no such group (%s) for -g option", argv[i] + 2);
			else
				set_gid = grp->gr_gid;
			break;
		case 'c':
			charset_name = argv[i] + 2;
			break;
		case 'r':
			indirect_host = argv[i] + 2;
			break;
		case 'l':	/* Use the Syslog daemon for logging */
			syslog_flag++;
			break;
		case 'o':
			other_flag = 1;
			break;
		case 'e':
			unknown_flag = 1;
			break;
		case 'n':
			number_flag = 1;
			break;
		case 'V':	/* Give version of this daemon */
			printf("[in.identd, version %s]\r\n", version);
			exit(0);
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
		}
	}

	/*
	 * Do the special handling needed for the "-b" flag
	 */
	if (background_flag == 1) {
		struct sockaddr_in addr;
		struct servent *sp;
		int     fd;

		if (fork())
			exit(0);

		close(0);
		close(1);
		close(2);

		if (fork())
			exit(0);

		fd = socket(AF_INET, SOCK_STREAM, 0);
		if (fd == -1)
			ERROR("main: socket");

		if (fd != 0)
			dup2(fd, 0);

		memset(&addr, 0, sizeof(addr));

		addr.sin_len = sizeof(struct sockaddr_in);
		addr.sin_family = AF_INET;
		if (bind_address == NULL)
			addr.sin_addr.s_addr = htonl(INADDR_ANY);
		else {
			if (inet_aton(bind_address, &addr.sin_addr) == 0) {
				struct hostent *hp;

				hp = gethostbyname(bind_address);
				if (!hp)
					ERROR1("no such address (%s) for -a switch", bind_address);

				memcpy(&addr.sin_addr, hp->h_addr, sizeof(addr.sin_addr));
			}
		}

		if (isdigit(portno[0]))
			addr.sin_port = htons(atoi(portno));
		else {
			sp = getservbyname(portno, "tcp");
			if (sp == NULL)
				ERROR1("main: getservbyname: %s", portno);
			addr.sin_port = sp->s_port;
		}

		if (bind(0, (struct sockaddr *) &addr, sizeof(addr)) < 0)
			ERROR("main: bind");

		if (listen(0, 3) < 0)
			ERROR("main: listen");
	}
	if (set_gid) {
		if (setegid(set_gid) == -1)
			ERROR("main: setgid");
		if (setgid(set_gid) == -1)
			ERROR("main: setgid");
	}
	if (set_uid) {
		if (seteuid(set_uid) == -1)
			ERROR("main: setuid");
		if (setuid(set_uid) == -1)
			ERROR("main: setuid");
	}
	/*
	 * Do some special handling if the "-b" or "-w" flags are used
	 */
	if (background_flag) {
		int     nfds, fd;
		fd_set  read_set;

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
				FD_ZERO(&read_set);
				FD_SET(0, &read_set);

				if (timeout) {
					tv.tv_sec = timeout;
					tv.tv_usec = 0;
					nfds = select(1, &read_set, NULL, NULL, &tv);
				} else
					nfds = select(1, &read_set, NULL, NULL, NULL);
			} while (nfds < 0 && errno == EINTR);

			/*
			 * An error occured in select? Just die
			 */
			if (nfds < 0)
				ERROR("main: select");

			/*
			 * Timeout limit reached. Exit nicely
			 */
			if (nfds == 0)
				exit(0);

			/*
			 * Disable the alarm timeout
			 */
			alarm(0);

			/*
			 * Accept the new client
			 */
			fd = accept(0, NULL, NULL);
			if (fd == -1)
				ERROR1("main: accept. errno = %d", errno);

			/*
			 * And fork, then close the fd if we are the parent.
			 */
			child_pid = fork();
			while (waitpid(-1, NULL, WNOHANG) > 0)
				;
		} while (child_pid && (close(fd), 1));

		/*
		 * We are now in child, the parent has returned to "do" above.
		 */
		if (dup2(fd, 0) == -1)
			ERROR("main: dup2: failed fd 0");

		if (dup2(fd, 1) == -1)
			ERROR("main: dup2: failed fd 1");

		if (dup2(fd, 2) == -1)
			ERROR("main: dup2: failed fd 2");
	}
	/*
	 * Get foreign internet address
	 */
	len = sizeof(sin);
	if (getpeername(0, (struct sockaddr *) &sin, &len) == -1) {
		/*
		 * A user has tried to start us from the command line or
		 * the network link died, in which case this message won't
		 * reach to other end anyway, so lets give the poor user some
		 * errors.
		 */
		perror("in.identd: getpeername()");
		exit(1);
	}
	faddr = sin.sin_addr;

	/*
	 * Open the connection to the Syslog daemon if requested
	 */
	if (syslog_flag) {
#ifdef LOG_DAEMON
		openlog("identd", LOG_PID, syslog_facility);
#else
		openlog("identd", LOG_PID);
#endif
		syslog(LOG_INFO, "Connection from %s", gethost(&faddr));
	}
	/*
	 * Get local internet address
	 */
	len = sizeof(sin);
	if (getsockname(0, (struct sockaddr *) &sin, &len) == -1) {
		/*
		 * We can just die here, because if this fails then the
		 * network has died and we haven't got anyone to return
		 * errors to.
		 */
		exit(1);
	}
	laddr = sin.sin_addr;

	/*
	 * Get the local/foreign port pair from the luser
	 */
	parse(STDIN_FILENO, &laddr, &faddr);
	exit(0);
}
