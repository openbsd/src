/* $KTH: kstash.c,v 1.10 1997/03/30 17:35:37 assar Exp $ */

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

/* change this later, but krblib_dbm needs it for now */

static des_cblock master_key;
static des_key_schedule master_key_schedule;

static void 
clear_secrets(void)
{
    memset(master_key_schedule, 0, sizeof(master_key_schedule));
    memset(master_key, 0, sizeof(master_key));
}

static char progname[] = "kstash";

int
main(int argc, char **argv)
{
    long    n;
    int ret = 0;

    if (atexit(clear_secrets))
	errx(1, "Out of resources\n");

    if ((n = kerb_init()))
        errx(1, "Kerberos db and cache init failed = %ld\n", n);

    if (kdb_get_master_key (KDB_GET_PROMPT, &master_key,
			    master_key_schedule) != 0) {
	errx(1, "Couldn't read master key.");
    }

    if (kdb_verify_master_key (&master_key, master_key_schedule, stderr) < 0) {
	return 1;
    }

    ret = kdb_kstash(&master_key, MKEYFILE);
    if(ret < 0)
        warn("writing master key");
    else
	fprintf(stderr, "Wrote master key to %s\n", MKEYFILE);
    
    return ret;
}
