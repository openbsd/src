/* ==== p_bench_getpid.c =================================================
 * Copyright (c) 1993 by Chris Provenzano, proven@athena.mit.edu
 *
 * Description : Benchmark getpid() times
 *
 *  1.00 93/11/08 proven
 *      -Started coding this file.
 */

#include <errno.h>
#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include "test.h"

/* ==========================================================================
 * usage();
 */
void usage(void)
{
	printf("p_bench_getpid [-d?] [-c count]\n");
    errno = 0;
}

int
main(int argc, char **argv)
{
	struct timeval starttime, endtime;
	pid_t process_id;
	int count = 1000000;
	int debug = 0;
	int i;

	char word[256];

    /* Getopt variables. */
    extern int optind, opterr;
    extern char *optarg;

	while ((word[0] = getopt(argc, argv, "c:d?")) != (char)EOF) {
		switch (word[0]) {
		case 'd':
			debug++;
			break;
		case 'c':
			count = atoi(optarg);
			break;
		case '?':
			usage();
			return(OK);
		default:
			usage();
			return(NOTOK);
		}
	}

	if (gettimeofday(&starttime, NULL)) {
	  perror ("gettimeofday");
	  return 1;
	}
	for (i = 0; i < count; i++) {
		process_id = getpid();
	}
	if (gettimeofday(&endtime, NULL)) {
	  perror ("gettimeofday");
	  return 1;
	}

	printf("%d getpid calls took %ld usecs.\n", count, 
		(endtime.tv_sec - starttime.tv_sec) * 1000000 +
		(endtime.tv_usec - starttime.tv_usec));

	return 0;
}
