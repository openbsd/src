/*	$Id: kstash.c,v 1.1.1.1 1995/12/14 06:52:41 tholo Exp $	*/

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

/* change this later, but krblib_dbm needs it for now */
char   *progname;

static des_cblock master_key;
static des_key_schedule master_key_schedule;
static int kfile;

static void 
clear_secrets(void)
{
    bzero(master_key_schedule, sizeof(master_key_schedule));
    bzero(master_key, sizeof(master_key));
}

int
main(int argc, char **argv)
{
    long    n;
    if ((n = kerb_init())) {
	fprintf(stderr, "Kerberos db and cache init failed = %ld\n", n);
	exit(1);
    }

    if (kdb_get_master_key (TRUE, &master_key, master_key_schedule) != 0) {
      fprintf (stderr, "%s: Couldn't read master key.\n", argv[0]);
      fflush (stderr);
      clear_secrets();
      exit (-1);
    }

    if (kdb_verify_master_key (&master_key, master_key_schedule, stderr) < 0) {
      clear_secrets();
      exit (-1);
    }

    kfile = open(MKEYFILE, O_TRUNC | O_RDWR | O_CREAT, 0600);
    if (kfile < 0) {
	clear_secrets();
	fprintf(stderr, "\n\07\07%s: Unable to open master key file\n",
		argv[0]);
	exit(1);
    }
    if (write(kfile, (char *) master_key, 8) < 0) {
	clear_secrets();
	fprintf(stderr, "\n%s: Write I/O error on master key file\n",
		argv[0]);
	exit(1);
    }
    (void) close(kfile);
    clear_secrets();
    exit(0);
}
