/*	$OpenBSD: main.c,v 1.12 2003/08/21 20:24:56 espie Exp $	*/

#include <sys/cdefs.h>
#ifndef lint
static const char rcsid[] = "$OpenBSD: main.c,v 1.12 2003/08/21 20:24:56 espie Exp $";
#endif

/*
 *
 * FreeBSD install - a package for the installation and maintainance
 * of non-core utilities.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * Jordan K. Hubbard
 * 18 July 1993
 *
 * This is the delete module.
 *
 */

#include <err.h>
#include "lib.h"
#include "delete.h"

static char Options[] = "chvDdnfp:q";

char	*Prefix		= NULL;
Boolean	NoDeInstall	= FALSE;
Boolean	CleanDirs	= FALSE;
Boolean CleanConf	= FALSE;
Boolean CheckMD5	= TRUE;

static void usage(void);

int
main(int argc, char **argv)
{
    int ch, error;
    char **pkgs, **start;

    pkgs = start = argv;
    while ((ch = getopt(argc, argv, Options)) != -1)
	switch(ch) {
	case 'v':
	    Verbose = TRUE;
	    break;

	case 'f':
	    Force = TRUE;
	    break;

	case 'p':
	    Prefix = optarg;
	    break;

	case 'D':
	    NoDeInstall = TRUE;
	    break;

	case 'd':
	    CleanDirs = TRUE;
	    break;

	case 'n':
	    Fake = TRUE;
	    Verbose = TRUE;
	    break;

	case 'q':
	    CheckMD5 = FALSE;
	    break;

	case 'c':
	    CleanConf = TRUE;
	    break;
	case 'h':
	case '?':
	default:
	    usage();
	    break;
	}

    argc -= optind;
    argv += optind;

    /* Get all the remaining package names, if any */
    /* Get all the remaining package names, if any */
    while (*argv)
	*pkgs++ = *argv++;

    /* If no packages, yelp */
    if (pkgs == start)
	pwarnx("missing package name(s)"), usage();
    *pkgs = NULL;
    if (!Fake && getuid() != 0)
	errx(1, "you must be root to delete packages");
    if ((error = pkg_perform(start)) != 0) {
	if (Verbose)
	    pwarnx("%d package deletion(s) failed", error);
	return error;
    }
    else
	return 0;
}

static void
usage()
{
    fprintf(stderr, "usage: pkg_delete [-cvDdhnfq] [-p prefix] pkg-name ...\n");
    exit(1);
}
