/*	$OpenBSD: main.c,v 1.12 2000/02/04 05:05:45 deraadt Exp $	*/

#ifndef lint
static char *rcsid = "$OpenBSD: main.c,v 1.12 2000/02/04 05:05:45 deraadt Exp $";
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
#include <sys/param.h>
#include "lib.h"
#include "add.h"

static char Options[] = "hvIRfnp:SMt:";

char	*Prefix		= NULL;
Boolean	NoInstall	= FALSE;
Boolean	NoRecord	= FALSE;

char	*Mode		= NULL;
char	*Owner		= NULL;
char	*Group		= NULL;
char	*PkgName	= NULL;
char	*Directory	= NULL;
char	FirstPen[FILENAME_MAX];
add_mode_t AddMode	= NORMAL;

char **pkgs;
int pkg_count = 0;

static void usage __P((void));

int
main(int argc, char **argv)
{
    int ch, err;
    char **start;
    char *cp;

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
	    usage();
	    break;
	}
    }
    argc -= optind;
    argv += optind;

    pkg_count = argc + 1;
    pkgs = (char **)calloc(pkg_count,  sizeof(char **));
    if (pkgs == NULL) {
    	fprintf(stderr, "malloc failed - abandoning package add.\n");
    	exit(1);		
    }      
    
    if (AddMode != SLAVE) {

	/* Get all the remaining package names, if any */
	for (ch = 0; *argv; ch++, argv++) {
	    /* Don't mangle stdin ("-") or URL arguments */
	    if ( (strcmp(*argv, "-") == 0)  
		 || (isURL(*argv))) {
	         pkgs[ch] = strdup(*argv);
		 if (pkgs[ch] == NULL) {
		     fprintf(stderr, 
			     "malloc failed - abandoning package add.\n");
		     exit(1);		
		 }
	    }
	    else {			/* expand all pathnames to fullnames */
		char *s, *tmp;

		s=ensure_tgz(*argv);
		    
		if (fexists(s)) { /* refers to a file directly */ 
		    pkgs[ch] = (char *) malloc(MAXPATHLEN * sizeof(char));
		    if (pkgs[ch] == NULL) {
		        fprintf(stderr, 
				"malloc failed - abandoning package add.\n");
			exit(1);		
		    }
		    tmp = realpath(s, pkgs[ch]);
		    if (tmp == NULL) {
		        perror("realpath failed");
			fprintf(stderr, "failing path was %s\n", pkgs[ch]);
			exit(1);
		    }
		}
		else if (ispkgpattern(*argv)
			 && (s=findbestmatchingname(dirname_of(*argv),
						    basename_of(*argv))) > 0) {
		    if (Verbose)
			printf("Using %s for %s\n",s, *argv);
		    pkgs[ch] = (char *) malloc(MAXPATHLEN * sizeof(char));
		    if (pkgs[ch] == NULL) {
		        fprintf(stderr, 
				"malloc failed - abandoning package add.\n");
			exit(1);		
		    }
		    tmp = realpath(s, pkgs[ch]);
		    if (tmp == NULL) {
		        perror("realpath failed");
			fprintf(stderr, "failing path was %s\n", pkgs[ch]);
			exit(1);
		    }
		} else {
		    /* look for the file(pattern) in the expected places */
		    if (!(cp = fileFindByPath(NULL, *argv))) {
		        fprintf(stderr, "can't find package '%s'\n", *argv);
			exit(1);
		    }
		    else {
			pkgs[ch] = strdup(cp);
			if (pkgs[ch] == NULL) {
			    fprintf(stderr, 
				  "malloc failed - abandoning package add.\n");
			    exit(1);		
			}
		    }
		}
	    }
	}
	/* If no packages, yelp */
	if (!ch)
	    warnx("missing package name(s)"), usage();
	else if (ch > 1 && AddMode == MASTER)
	    warnx("only one package name may be specified with master mode"),
	    usage();
    }
    if ((err = pkg_perform(pkgs)) != 0) {
	if (Verbose)
	    warnx("%d package addition(s) failed", err);
	return err;
    }
    else
	return 0;
}

static void
usage()
{
    fprintf(stderr, "%s\n",
	"usage: pkg_add [-vInfRMS] [-t template] [-p prefix] pkg-name ...");
    exit(1);
}
