/*	$OpenBSD: rtrd.c,v 1.5 2026/09/19 17:23:52 schwarze Exp $ */
/*
 * Copyright (c) 2025-2026 Ralph Covelli <rcovelli@he.net>
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
#include <sys/tree.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <assert.h>
#include <err.h>
#include <errno.h>
#include <limits.h>
#include <netdb.h>
#include <pwd.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>

#include "rtr_config.h"
#include "rtrd.h"
#include "version.h"

__dead static void
usage(void)
{
	extern char *__progname;

	fprintf(stderr, "usage: %s [-fvV] "
	    "[-b bind_address] [-m max_version] [-p port] [-s socket]\n",
	    __progname);
	exit(1);
}

int
main(int argc, char **argv)
{
	int c;
	char *bind_str = NULL;
	uint16_t port = DEFAULT_PORT;
	int daemonize = 1;
	struct passwd *pw;
	int i;

	if (pledge("stdio rpath cpath unix inet proc id chown fattr",
	    NULL) == -1) {
		fprintf(stderr, "pledge error\n");
		exit(1);
	}

	setvbuf(stdout, NULL, _IOLBF, 0);

	controller_filename = CONTROLLER_FILENAME;

	while ((c = getopt(argc, argv, "b:fhm:p:s:Vv")) != -1) {
		switch (c) {
		case 'b':
			bind_str = optarg;
			break;
		case 'f':
			daemonize = 0;
			break;
		case 'm':
			rtr_max_version = strtoul(optarg, NULL, 10);
			if (rtr_max_version > RTR_MAX_VERSION)
				rtr_max_version = RTR_MAX_VERSION;
			break;
		case 'p':
			port = strtoul(optarg, NULL, 10);
			break;
		case 's':
			controller_filename = optarg;
			break;
		case 'V':
			fprintf(stdout, "OpenRTRd %s\n", RTRD_VERSION);
			return 0;
		case 'v':
			verbose++;
			break;
		default:
			usage();
		}
	}

	argv += optind;
	argc -= optind;

	init_masks();

	if (init_stats() != 0) {
		fprintf(stderr, "couldnt initialize stats\n");
		exit(1);
	}

	if (clock_gettime(CLOCK_REALTIME, (struct timespec *)&now) != 0) {
		fprintf(stderr, "couldnt get the clock\n");
		exit(1);
	}
	now.tv_usec /= 1000; /* nsec -> usec */
	global_stats.start_time = now.tv_sec;

	sched_init();

	if (init_cache_array(CACHE_FRAME_COUNT) != 0) {
		fprintf(stderr, "couldnt initialize cache\n");
		exit(1);
	}

	if (init_socket_table(stderr, bind_str, port) != 0)
		exit(1);

	if (init_signals() != 0) {
		fprintf(stderr, "couldnt initialize signals\n");
		exit(1);
	}

	if (getuid() == 0) {
		pw = getpwnam(RTRD_USER);
		if (pw == NULL) {
			fprintf(stderr,
			    "couldnt get user %s to drop root privileges\n",
			    RTRD_USER);
			exit(1);
		}

		if (chown(controller_filename, pw->pw_uid, pw->pw_gid) == -1) {
			fprintf(stderr, "couldnt chown control socket\n");
			exit(1);
		}

		if (chmod(controller_filename, S_IRUSR | S_IWUSR |
		    S_IRGRP | S_IWGRP) == -1) {
			fprintf(stderr, "couldnt chmod control socket\n");
			exit(1);
		}

		if (setgroups(1, &pw->pw_gid) == -1 ||
		    setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) == -1 ||
		    setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid) == -1) {
			fprintf(stderr, "couldnt drop root privileges\n");
			exit(1);
		}
	}

	if (daemonize) {
		close(STDIN_FILENO);
		close(STDOUT_FILENO);
		close(STDERR_FILENO);

		foreground = 0;

		if (fork())
			exit(0);
	}

	if (pledge("stdio unix inet", NULL) == -1) {
		logx(0, "RTR Pledge error\n");
		exit(1);
	}

	logx(0, "OpenRTRd %s server starting...\n", RTRD_VERSION);
	logx(0, "Max RTR version supported is %d\n", rtr_max_version);
	logx(0, "Max open sockets is %d\n", max_sockets);
	logx(0, "Cache frame count is %d\n", CACHE_FRAME_COUNT);
	for (i = 0; i <= rtr_max_version; i++) {
		logx(0, "RTR v%d cache session id is %u\n",
		    i, cache[i].session_id);
	}
	logx(0, "SendQ max is %d bytes per socket\n", max_sendq);
	logx(0, "SendQ block size is %d bytes\n", SENDQ_BLOCK);
	logx(0, "Server is ready\n");

	core_loop();  /* never returns */

	return 0;
}
