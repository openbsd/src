/*	$OpenBSD: getopt.c,v 1.7 2006/06/07 07:33:23 jmc Exp $	*/

/*
 * This material, written by Henry Spencer, was released by him
 * into the public domain and is thus not subject to any copyright.
 */

#ifndef lint
static char rcsid[] = "$OpenBSD: getopt.c,v 1.7 2006/06/07 07:33:23 jmc Exp $";
#endif /* not lint */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int
main(int argc, char *argv[])
{
	extern int optind;
	extern char *optarg;
	int c;
	int status = 0;

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
