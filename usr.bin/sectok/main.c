/*	$OpenBSD: main.c,v 1.10 2007/12/30 13:35:27 sobrado Exp $ */

/*
 * Smartcard commander.
 * Written by Jim Rees and others at University of Michigan.
 */

/*
 * copyright 2001
 * the regents of the university of michigan
 * all rights reserved
 *
 * permission is granted to use, copy, create derivative works
 * and redistribute this software and such derivative works
 * for any purpose, so long as the name of the university of
 * michigan is not used in any advertising or publicity
 * pertaining to the use or distribution of this software
 * without specific, written prior authorization.  if the
 * above copyright notice or any other identification of the
 * university of michigan is included in any copy of any
 * portion of this software, then the disclaimer below must
 * also be included.
 *
 * this software is provided as is, without representation
 * from the university of michigan as to its fitness for any
 * purpose, and without warranty by the university of
 * michigan of any kind, either express or implied, including
 * without limitation the implied warranties of
 * merchantability and fitness for a particular purpose. the
 * regents of the university of michigan shall not be liable
 * for any damages, including special, indirect, incidental, or
 * consequential damages, with respect to any claim arising
 * out of or in connection with the use of the software, even
 * if it has been or is hereafter advised of the possibility of
 * such damages.
 */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <limits.h>
#include <string.h>
#include <errno.h>
#include <sectok.h>

#include "sc.h"

#define MAXTOKENS 300
#define CARDIOSIZE 200

void    onintr(int sigraised);

int     port, fd = -1, cla, sleepytime;
volatile sig_atomic_t interrupted;
FILE   *cmdf;

static void
usage(void)
{
	fprintf(stderr,
	    "usage: sectok [-0123] [-f scriptfile] [-s sleeptime] [command [arg ...]]\n");
	exit(1);
}

int
main(int argc, char *argv[])
{
	char    buf[_POSIX2_LINE_MAX], *scriptfile = NULL, *tp, *tv[MAXTOKENS];
	int     i, tc;

	tp = getenv("SCPORT");
	if (tp)
		port = atoi(tp);

	while ((i = getopt(argc, argv, "0123f:s:h")) != -1) {
		switch (i) {
		case '0':
		case '1':
		case '2':
		case '3':
			port = i - '0';
			break;
		case 'f':
			scriptfile = optarg;
			break;
		case 's':
			sleepytime = atoi(optarg);
			break;
		case 'h':
		default:
			usage();
			break;
		}
	}

	if (optind != argc) {
		/* Dispatch from command line */
		dispatch(argc - optind, &argv[optind]);
		exit(0);
	}
	if (scriptfile != NULL) {
		cmdf = fopen(scriptfile, "r");
		if (cmdf == NULL) {
			perror(scriptfile);
			exit(2);
		}
	} else
		cmdf = stdin;

	/* Interactive mode, or script file */

	signal(SIGINT, onintr);
#ifdef __OpenBSD__
	siginterrupt(SIGINT, 1);
#endif

	/* The Main Loop */
	while (1) {
		fflush(stdout);
		interrupted = 0;
		if (sleepytime)
			usleep(sleepytime * 1000);
		if (cmdf == stdin) {
			fprintf(stderr, "sectok> ");
			fflush(stderr);
		}
		if (!fgets(buf, sizeof buf, cmdf)) {
			putchar('\n');
			if (interrupted)
				continue;
			else
				break;
		}
		if (cmdf != stdin)
			printf("sectok> %s", buf);

		for ((tp = strtok(buf, " \t\n\r")), tc = 0; tp;
		    (tp = strtok(NULL, " \t\n\r")), tc++) {
			if (tc < MAXTOKENS - 1)
				tv[tc] = tp;
		}
		tv[tc] = NULL;

		dispatch(tc, tv);
	}

	quit(0, NULL);
	return 0;
}

void
onintr(int sigraised)
{
	interrupted++;
}
