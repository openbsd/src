/*	$OpenBSD: main.c,v 1.2 2002/02/16 21:27:07 millert Exp $	*/

/*
 * Public domain - no warranty.
 */

#include <sys/cdefs.h>

int ls_main(int argc, char **argv);

int
main(argc, argv)
	int argc;
	char **argv;
{
	return ls_main(argc, argv);
}
