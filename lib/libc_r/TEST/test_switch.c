/*	$OpenBSD: test_switch.c,v 1.7 2000/10/04 05:50:58 d Exp $	*/
/* ==== test_switch.c ========================================================
 * Copyright (c) 1993 by Chris Provenzano, proven@athena.mit.edu
 *
 * Description : Test context switch functionality.
 *
 *  1.00 93/08/04 proven
 *      -Started coding this file.
 */

#include <pthread.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include "test.h"

const char buf[] = "abcdefghijklmnopqrstuvwxyz";
char x[sizeof(buf)];
int fd = 1;

volatile int ending = 0;

/* ==========================================================================
 * usage();
 */
void usage(void)
{
    printf("test_switch [-d?] [-c count]\n");
	printf("count must be between 2 and 26\n");
    errno = 0;
}

void *
new_thread(arg)
	void *arg;
{
	SET_NAME("writer");
	while (!ending) {
		CHECKe(write (fd, (char *) arg, 1));
		x[(char *)arg - buf] = 1;
	}
	return NULL;
}

int
main(argc, argv)
	int argc;
	char **argv;
{
	pthread_t thread;
	int count = 4;
	int debug = 0;
	int eof = 0;
	long i;

	/* Getopt variables. */
	extern int optind, opterr;
	extern char *optarg;

	while (!eof)
	  switch (getopt (argc, argv, "c:d?"))
	    {
	    case EOF:
	      eof = 1;
	      break;
	    case 'd':
	      debug++;
	      break;
	    case 'c':
	      count = atoi(optarg);
	      if ((count > 26) || (count < 2)) {
			  count = 2;
	      }
	      break;
	    case '?':
	      usage();
	      return(OK);
	    default:
	      usage();
	      return(NOTOK);
	    }

	/* create the threads */
	for (i = 0; i < count; i++)
		CHECKr(pthread_create(&thread, NULL, new_thread, 
		    (void*)(buf+i)));

	/* give all threads a chance to run */
	sleep (4);

	ending = 1;
	for (i = 0; i < count; i++)
		ASSERT(x[i]);	/* make sure each thread ran */

	SUCCEED;
}
