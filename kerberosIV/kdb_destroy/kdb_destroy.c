/* $KTH: kdb_destroy.c,v 1.7 1997/03/31 02:25:21 assar Exp $ */

/*-
 * Copyright 1987, 1988 by the Student Information Processing Board
 *      of the Massachusetts Institute of Technology
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

#include "adm_locl.h"

int
main(int argc, char **argv)
{
    char    answer[10];		/* user input */
    char    dbm[256];		/* database path and name */
    char    dbm1[256];		/* database path and name */
#ifdef HAVE_NEW_DB
    char   *file;               /* database file names */
#else
    char   *file1, *file2;	/* database file names */
#endif

    strncpy(dbm, DBM_FILE, sizeof(dbm) - 5);
    dbm[sizeof(dbm) - 5] = '\0';
#ifdef HAVE_NEW_DB
    file = strcat(dbm, ".db");
    file[sizeof(dbm) - 1] = '\0';
#else
    strncpy(dbm1, DBM_FILE, sizeof(dbm) - 5);
    file1 = strcat(dbm, ".dir");
    file1[sizeof(dbm) - 1] = '\0';
    file2 = strcat(dbm1, ".pag");
    file2[sizeof(dbm) - 1] = '\0';
#endif

    printf("You are about to destroy the Kerberos database ");
    printf("on this machine.\n");
    printf("Are you sure you want to do this (y/n)? ");
    fgets(answer, sizeof(answer), stdin);

    if (answer[0] == 'y' || answer[0] == 'Y') {
#ifdef HAVE_NEW_DB
      if (unlink(file) == 0)
#else
	if (unlink(file1) == 0 && unlink(file2) == 0)
#endif
	  {
	    warnx ("Database deleted at %s", DBM_FILE);
	    return 0;
	  }
	else
	    warn ("Database cannot be deleted at %s", DBM_FILE);
    } else
        warnx ("Database not deleted at %s", DBM_FILE);
    return 1;
}
