/* $OpenBSD: startup.c,v 1.1 2001/09/21 20:22:06 camield Exp $ */

/*
 * Command line option parsing.
 */

#include "params.h"

#if POP_OPTIONS

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

/* pop_root.c */
extern int do_pop_startup(void);
extern int do_pop_session(void);

/* standalone.c */
extern int do_standalone(void);

#ifdef HAVE_PROGNAME
extern char *__progname;
#define progname __progname
#else
static char *progname;
#endif

static void usage(void)
{
	fprintf(stderr, "Usage: %s [-D]\n", progname);
	exit(1);
}

int main(int argc, char **argv)
{
	int c;
	int standalone = 0;

#ifndef HAVE_PROGNAME
	if (!(progname = argv[0]))
		progname = POP_SERVER;
#endif

	while ((c = getopt(argc, argv, "D")) != -1) {
		switch (c) {
		case 'D':
			standalone++;
			break;

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
