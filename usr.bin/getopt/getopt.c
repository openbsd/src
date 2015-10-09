/*	$OpenBSD: getopt.c,v 1.10 2015/10/09 01:37:07 deraadt Exp $	*/

/*
 * This material, written by Henry Spencer, was released by him
 * into the public domain and is thus not subject to any copyright.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <err.h>

int
main(int argc, char *argv[])
{
	extern int optind;
	extern char *optarg;
	int c;
	int status = 0;

	if (pledge("stdio", NULL) == -1)
		err(1, "pledge");

	optind = 2;	/* Past the program name and the option letters. */
	while ((c = getopt(argc, argv, argv[1])) != -1)
		switch (c) {
		case '?':
			status = 1;	/* getopt routine gave message */
			break;
		default:
			if (optarg != NULL)
				printf(" -%c %s", c, optarg);
			else
				printf(" -%c", c);
			break;
		}
	printf(" --");
	for (; optind < argc; optind++)
		printf(" %s", argv[optind]);
	printf("\n");
	exit(status);
}
