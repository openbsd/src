/* ==== p_bench_pthread_create.c =============================================
 * Copyright (c) 1993 by Chris Provenzano, proven@athena.mit.edu
 *
 * Description : Benchmark thread creation time
 *
 *  1.00 93/11/08 proven
 *      -Started coding this file.
 */


#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <getopt.h>
#include <stdlib.h>
#include <unistd.h>
#include "test.h"

/* ==========================================================================
 * new_thread();
 */
void * new_thread(void * arg)
{
	pthread_yield();
}

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
	pthread_t thread_id;
	int count = 10000;
	int debug = 0;
	int i;
	pthread_attr_t attr;;


	char word[256];

    /* Getopt variables. */
    extern int optind, opterr;
    extern char *optarg;

	pthread_attr_init(&attr);
	pthread_attr_setstackaddr(&attr, &word);
	pthread_attr_setstacksize(&attr, sizeof word);

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
		if (pthread_create(&thread_id, &attr, new_thread, NULL)) {
			printf("Bad pthread create routine\n");
			exit(1);
		}
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
