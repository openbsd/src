/*	$OpenBSD: main.c,v 1.7 1998/04/07 04:18:45 deraadt Exp $	*/

#ifndef lint
static char *rcsid = "$OpenBSD: main.c,v 1.7 1998/04/07 04:18:45 deraadt Exp $";
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

#include <sys/param.h>
#include "lib.h"
#include "add.h"

static char Options[] = "hvIRfnp:SMt:";

char	*Prefix		= NULL;
Boolean	NoInstall	= FALSE;
Boolean	NoRecord	= FALSE;
Boolean	Force		= FALSE;

char	*Mode		= NULL;
char	*Owner		= NULL;
char	*Group		= NULL;
char	*PkgName	= NULL;
char	*Directory	= NULL;
char	FirstPen[FILENAME_MAX];
add_mode_t AddMode	= NORMAL;

#define MAX_PKGS	20
char	pkgnames[MAX_PKGS][MAXPATHLEN];
char	*pkgs[MAX_PKGS];

int
main(int argc, char **argv)
{
    int ch, err;
    char **start;
    char *prog_name = argv[0], *cp;

    start = argv;
    while ((ch = getopt(argc, argv, Options)) != -1) {
	switch(ch) {
	case 'v':
	    Verbose = TRUE;
	    break;

	case 'p':
	    Prefix = optarg;
	    break;

	case 'I':
	    NoInstall = TRUE;
	    break;

	case 'R':
	    NoRecord = TRUE;
	    break;

	case 'f':
	    Force = TRUE;
	    break;

	case 'n':
	    Fake = TRUE;
	    Verbose = TRUE;
	    break;

	case 't':
	    strcpy(FirstPen, optarg);
	    break;

	case 'S':
	    AddMode = SLAVE;
	    break;

	case 'M':
	    AddMode = MASTER;
	    break;

	case 'h':
	case '?':
	default:
	    usage(prog_name, NULL);
	    break;
	}
    }
    argc -= optind;
    argv += optind;

    if (argc > MAX_PKGS) {
	whinge("Too many packages (max %d).", MAX_PKGS);
	return(1);
    }

    if (AddMode != SLAVE) {
	for (ch = 0; ch < MAX_PKGS; pkgs[ch++] = NULL) ;

	/* Get all the remaining package names, if any */
	for (ch = 0; *argv; ch++, argv++) {
	    if (!strcmp(*argv, "-"))	/* stdin? */
		pkgs[ch] = "-";
	    else if (isURL(*argv))	/* preserve URLs */
		pkgs[ch] = strcpy(pkgnames[ch], *argv);
	    else {			/* expand all pathnames to fullnames */
		if (fexists(*argv)) /* refers to a file directly */
		    pkgs[ch] = realpath(*argv, pkgnames[ch]);
		else {		/* look for the file in the expected places */
		    if (!(cp = fileFindByPath(NULL, *argv)))
			whinge("Can't find package '%s'.", *argv);
		    else
			pkgs[ch] = strcpy(pkgnames[ch], cp);
		}
	    }
	}
	/* If no packages, yelp */
	if (!ch)
	  usage(prog_name, NULL);
	else if (ch > 1 && AddMode == MASTER)
	  usage(prog_name,
		"Only one package name may be specified with master mode");
    }
    if ((err = pkg_perform(pkgs)) != NULL) {
	if (Verbose)
	    fprintf(stderr, "%d package addition(s) failed.\n", err);
	return err;
    }
    else
	return 0;
}

void
usage(const char *name, const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    if (fmt) {
	fprintf(stderr, "%s: ", name);
	vfprintf(stderr, fmt, args);
	fprintf(stderr, "\n");
    }
    va_end(args);
    fprintf(stderr, "usage: %s [-vInfRMS] [-t template] [-p prefix] pkg ...\n", name);
    exit(1);
}
