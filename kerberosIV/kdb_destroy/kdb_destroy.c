/*	$Id: kdb_destroy.c,v 1.2 1996/08/22 01:18:51 deraadt Exp $	*/

/*-
 * Copyright 1987, 1988 by the Student Information Processing Board
 *	of the Massachusetts Institute of Technology
 *
 * Permission to use, copy, modify, and distribute this software
 * and its documentation for any purpose and without fee is
 * hereby granted, provided that the above copyright notice
 * appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation,
 * and that the names of M.I.T. and the M.I.T. S.I.P.B. not be
 * used in advertising or publicity pertaining to distribution
 * of the software without specific, written prior permission.
 * M.I.T. and the M.I.T. S.I.P.B. make no representations about
 * the suitability of this software for any purpose.  It is
 * provided "as is" without express or implied warranty.
 */

#include <adm_locl.h>

int
main(void)
{
    char    answer[10];		/* user input */
    char    dbm[256];		/* database path and name */
    char    dbm1[256];		/* database path and name */
#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
    char   *file;               /* database file names */
#else
    char   *file1, *file2;	/* database file names */
#endif

    strcpy(dbm, DBM_FILE);
#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
    file = strcat(dbm, ".db");
#else
    strcpy(dbm1, DBM_FILE);
    file1 = strcat(dbm, ".dir");
    file2 = strcat(dbm1, ".pag");
#endif

    printf("You are about to destroy the Kerberos database ");
    printf("on this machine.\n");
    printf("Are you sure you want to do this (y/n)? ");
    fgets(answer, sizeof(answer), stdin);

    if (answer[0] == 'y' || answer[0] == 'Y') {
#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
	if (unlink(file) == 0)
#else
	if (unlink(file1) == 0 && unlink(file2) == 0)
#endif
	    fprintf(stderr, "Database deleted at %s\n", DBM_FILE);
	else
	    fprintf(stderr, "Database cannot be deleted at %s\n",
		    DBM_FILE);
    } else
	fprintf(stderr, "Database not deleted.\n");
    exit(1);
}
