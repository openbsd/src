/*	$OpenBSD: ext_srvtab.c,v 1.5 1998/08/12 23:09:05 art Exp $	*/
/*      $KTH: ext_srvtab.c,v 1.13 1997/05/02 14:27:33 assar Exp $       */

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

#include <sys/param.h>

#include <adm_locl.h>
#include <err.h>

static des_cblock master_key;
static des_cblock session_key;
static des_key_schedule master_key_schedule;
char progname[] = "ext_srvtab";
static char realm[REALM_SZ];

static void
usage(void)
{
    fprintf(stderr, 
	    "Usage: %s [-n] [-r realm] instance [instance ...]\n", progname);
    exit(1);
}

static void
StampOutSecrets(void)
{
    memset(master_key, 0, sizeof master_key);
    memset(session_key, 0, sizeof session_key);
    memset(master_key_schedule, 0, sizeof master_key_schedule);
}

static void
FWrite(void *p, int size, int n, FILE *f)
{
    if (fwrite(p, size, n, f) != n) {
        StampOutSecrets();
	errx(1, "Error writing output file.  Terminating.\n");
    }
}

int
main(int argc, char **argv)
{
    FILE *fout;
    char fname[MAXPATHLEN];
    int fopen_errs = 0;
    int arg;
    Principal princs[40];
    int more; 
    int prompt = KDB_GET_PROMPT;
    int n, i;
    
    memset(realm, 0, sizeof(realm));

    if (atexit(StampOutSecrets))
	errx(1, "Out of resources");
    
    /* Parse commandline arguments */
    if (argc < 2)
	usage();
    else {
	for (i = 1; i < argc; i++) {
	    if (strcmp(argv[i], "-n") == 0)
		prompt = FALSE;
	    else if (strcmp(argv[i], "-r") == 0) {
		if (++i >= argc)
		    usage();
		else {
		    strcpy(realm, argv[i]);
		    /* 
		     * This is to humor the broken way commandline
		     * argument parsing is done.  Later, this
		     * program ignores everything that starts with -.
		     */
		    argv[i][0] = '-';
		}
	    }
	    else if (argv[i][0] == '-')
		usage();
	    else
		if (!k_isinst(argv[i])) {
		    warnx("bad instance name: %s", argv[i]);
		    usage();
		}
	}
    }

    if (kdb_get_master_key (prompt, &master_key, master_key_schedule) != 0)
	errx (1, "Couldn't read master key.");

    if (kdb_verify_master_key (&master_key, master_key_schedule, stderr) < 0)
	exit(1);

    /* For each arg, search for instances of arg, and produce */
    /* srvtab file */
    if (!realm[0])
      if (krb_get_lrealm(realm, 1) != KSUCCESS) {
	  StampOutSecrets();
	  errx (1, "couldn't get local realm");
      }
    umask(077);

    for (arg = 1; arg < argc; arg++) {
	if (argv[arg][0] == '-')
	    continue;
	snprintf(fname, sizeof(fname), "%s-new-srvtab", argv[arg]);
	if ((fout = fopen(fname, "w")) == NULL) {
	    warn("Couldn't create file '%s'.", fname);
	    fopen_errs++;
	    continue;
	}
	printf("Generating '%s'....\n", fname);
	n = kerb_get_principal("*", argv[arg], &princs[0], 40, &more);
	if (more)
	    fprintf(stderr, "More than 40 found...\n");
	for (i = 0; i < n; i++) {
	    FWrite(princs[i].name, strlen(princs[i].name) + 1, 1, fout);
	    FWrite(princs[i].instance, strlen(princs[i].instance) + 1,
		   1, fout);
	    FWrite(realm, strlen(realm) + 1, 1, fout);
	    FWrite(&princs[i].key_version,
		sizeof(princs[i].key_version), 1, fout);
	    copy_to_key(&princs[i].key_low, &princs[i].key_high, session_key);
	    kdb_encrypt_key (&session_key, &session_key, 
			     &master_key, master_key_schedule, DES_DECRYPT);
	    FWrite(session_key, sizeof session_key, 1, fout);
	}
	fclose(fout);
    }
    StampOutSecrets();
    return fopen_errs;		/* 0 errors if successful */
}
