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
	while(1) {
		CHECKe(write (fd, (char *) arg, 1));
		x[(char *)arg - buf] = 1;
	}
	PANIC("while");
}

int
main(argc, argv)
	int argc;
	char **argv;
{
	pthread_t thread;
	int count = 2;
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
	sleep (2);

	for (i = 0; i < count; i++)
		ASSERT(x[i]);	/* make sure each thread ran */

	SUCCEED;
}
