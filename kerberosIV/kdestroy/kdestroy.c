/*	$OpenBSD: kdestroy.c,v 1.3 1998/02/18 11:53:53 art Exp $	*/
/* $KTH: kdestroy.c,v 1.8 1997/03/30 16:15:03 joda Exp $");

/*
 * This software may now be redistributed outside the US.
 */

/*-
 * Copyright (C) 1989 by the Massachusetts Institute of Technology
 *
 * Export of this software from the United States of America is assumed
 * to require a specific license from the United States Government.
 * It is the responsibility of any person or organization contemplating
 * export to obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 *
 */

/*
 * This program causes Kerberos tickets to be destroyed.
 * Options are: 
 *
 *   -q[uiet]	- no bell even if tickets not destroyed
 *   -f[orce]	- no message printed at all 
 *   -t		- do not destroy tokens
 */

#include "kuser_locl.h"
#include <kerberosIV/kafs.h>

char progname[] = "kdestroy";

static void
usage(void)
{
    fprintf(stderr, "Usage: %s [-f] [-q] [-t]\n", progname);
    exit(1);
}

int
main(int argc, char **argv)
{
    int fflag=0, tflag = 0, k_errno;
    int c;

    while((c = getopt(argc, argv, "fqt")) >= 0){
	switch(c){
	case 'f':
	case 'q':
	    fflag++;
	    break;
	case 't':
	    tflag++;
	    break;
	default:
	    usage();
	}
    }
    if(argc - optind > 0)
	usage();

    k_errno = dest_tkt();

    if(!tflag && k_hasafs())
	k_unlog();

    if (fflag) {
	if (k_errno != 0 && k_errno != RET_TKFIL)
	    exit(1);
	else
	    exit(0);
    } else {
	if (k_errno == 0)
	    printf("Tickets destroyed.\n");
	else if (k_errno == RET_TKFIL)
	    printf("No tickets to destroy.\n");
	else {
	    printf("Tickets NOT destroyed.\n");
	    exit(1);
	}
    }
    exit(0);
}
