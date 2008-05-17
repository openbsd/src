/* $OpenBSD: startup.c,v 1.7 2008/05/17 23:31:52 sobrado Exp $ */

/*
 * Command line option parsing.
 */

#include "params.h"

#if POP_OPTIONS

#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

/* version.c */
extern char popa3d_version[];
extern char popa3d_date[];

/* standalone.c */
extern int do_standalone(void);

/* pop_root.c */
extern int do_pop_startup(void);
extern int do_pop_session(void);

#ifdef HAVE_PROGNAME
extern char *__progname;
#define progname __progname
#else
static char *progname;
#endif

int af = PF_UNSPEC;

static void usage(void)
{
	fprintf(stderr, "usage: %s [-46DV]\n", progname);
	exit(1);
}

static void version(void)
{
	printf("popa3d version %s (%.10s)\n", popa3d_version, popa3d_date + 7);
	exit(0);
}

int main(int argc, char **argv)
{
	int c;
	int standalone = 0;

#ifndef HAVE_PROGNAME
	if (!(progname = argv[0]))
		progname = POP_SERVER;
#endif

	while ((c = getopt(argc, argv, "46DV")) != -1) {
		switch (c) {
		case '4':
			af = AF_INET;
			break;

		case '6':
			af = AF_INET6;
			break;

		case 'D':
			standalone++;
			break;

		case 'V':
			version();

		default:
			usage();
		}
	}

	if (optind != argc)
		usage();

	if (standalone)
		return do_standalone();

	if (do_pop_startup()) return 1;
	return do_pop_session();
}

#endif
