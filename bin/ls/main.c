/*	$OpenBSD: main.c,v 1.3 2003/06/11 23:42:12 deraadt Exp $	*/

/*
 * Public domain - no warranty.
 */

#include <sys/cdefs.h>

int ls_main(int argc, char **argv);

int
main(int argc, char *argv[])
{
	return ls_main(argc, argv);
}
