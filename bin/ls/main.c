/*	$OpenBSD: main.c,v 1.1 1999/02/23 23:54:17 art Exp $	*/

/*
 * Public domain - no warranty.
 */

#include <sys/cdefs.h>

int ls_main __P((int argc, char **argv));

int
main(argc, argv)
	int argc;
	char **argv;
{
	return ls_main(argc, argv);
}
