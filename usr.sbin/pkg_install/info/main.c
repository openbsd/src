/*	$OpenBSD: main.c,v 1.12 1999/02/28 18:55:25 deraadt Exp $	*/

#ifndef lint
static char *rcsid = "$OpenBSD: main.c,v 1.12 1999/02/28 18:55:25 deraadt Exp $";
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
 * This is the add module.
 *
 */

#include <err.h>
#include "lib.h"
#include "info.h"

static char Options[] = "acDde:fIikLl:mpqRrvh";

int	Flags		= 0;
Boolean AllInstalled	= FALSE;
Boolean Quiet		= FALSE;
char *InfoPrefix	= "";
char PlayPen[FILENAME_MAX];
size_t PlayPenSize	= sizeof(PlayPen);
char *CheckPkg		= NULL;

static void usage __P((void));

int
main(int argc, char **argv)
{
    int ch;
    char **pkgs, **start;

    pkgs = start = argv;
    while ((ch = getopt(argc, argv, Options)) != -1)
	switch(ch) {
	case 'a':
	    AllInstalled = TRUE;
	    break;

	case 'c':
	    Flags |= SHOW_COMMENT;
	    break;

	case 'D':
	    Flags |= SHOW_DISPLAY;
	    break;

	case 'd':
	    Flags |= SHOW_DESC;
	    break;

	case 'e':
	    CheckPkg = optarg;
	    break;

	case 'f':
	    Flags |= SHOW_PLIST;
	    break;

	case 'I':
	    Flags |= SHOW_INDEX;
	    break;

	case 'i':
	    Flags |= SHOW_INSTALL;
	    break;

	case 'k':
	    Flags |= SHOW_DEINSTALL;
	    break;

	case 'L':
	    Flags |= SHOW_FILES;
	    break;

	case 'l':
	    InfoPrefix = optarg;
	    break;

	case 'm':
	    Flags |= SHOW_MTREE;
	    break;

	case 'p':
	    Flags |= SHOW_PREFIX;
	    break;

	case 'q':
	    Quiet = TRUE;
	    break;

	case 'R':
	    Flags |= SHOW_REQBY;
	    break;

	case 'r':
	    Flags |= SHOW_REQUIRE;
	    break;

	case 'v':
	    Verbose = TRUE;
	    /* Reasonable definition of 'everything' */
	    Flags = SHOW_COMMENT | SHOW_DESC | SHOW_PLIST | SHOW_INSTALL |
		SHOW_DEINSTALL | SHOW_REQUIRE | SHOW_DISPLAY | SHOW_MTREE |
		SHOW_REQBY;
	    break;

	case 'h':
	case '?':
	default:
	    usage();
	    /* NOTREACHED */
	}

    argc -= optind;
    argv += optind;

    if (argc == 0 && !Flags) {
	/* No argument or flags specified - assume -Ia */
	Flags = SHOW_INDEX;
	AllInstalled = TRUE;
    }

    /* Set some reasonable defaults */
    if (!Flags)
	Flags = SHOW_COMMENT | SHOW_DESC | SHOW_REQBY;

    /* Get all the remaining package names, if any */
    while (*argv)
	*pkgs++ = *argv++;

    /* If no packages, yelp */
    if (pkgs == start && !AllInstalled && !CheckPkg)
	warnx("missing package name(s)"), usage();
    *pkgs = NULL;
    return pkg_perform(start);
}

static void
usage()
{
    fprintf(stderr, "usage: %s\nusage: %s\n",
	"pkg_info [-cDdfIikLmpqRrvh] [-e package] [-l prefix] pkg-name ...",
	"pkg_info -a [flags]");
    exit(1);
}
