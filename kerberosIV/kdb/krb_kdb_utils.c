/*	$Id: krb_kdb_utils.c,v 1.1.1.1 1995/12/14 06:52:36 tholo Exp $	*/

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
 */

/*
 * Utility routines for Kerberos programs which directly access
 * the database.  This code was duplicated in too many places
 * before I gathered it here.
 *
 * Jon Rochlis, MIT Telecom, March 1988
 */

#include "kdb_locl.h"

#include <kdc.h>

long 
kdb_get_master_key(prompt, master_key, master_key_sched)
	int prompt;
	des_cblock *master_key;
	struct des_ks_struct *master_key_sched;
{
  int kfile;

  if (prompt)  {
#ifdef NOENCRYPTION
      placebo_read_password(master_key,
			    "\nEnter Kerberos master key: ", 0);
#else
      des_read_password(master_key,
			     "\nEnter Kerberos master key: ", 0);
#endif
      printf ("\n");
  }
  else {
    kfile = open(MKEYFILE, O_RDONLY, 0600);
    if (kfile < 0) {
      /* oh, for com_err_ */
      return (-1);
    }
    if (read(kfile, (char *) master_key, 8) != 8) {
      return (-1);
    }
    close(kfile);
  }

#ifndef NOENCRYPTION
  des_key_sched(master_key,master_key_sched);
#endif
  return (0);
}

/* The old algorithm used the key schedule as the initial vector which
   was byte order depedent ... */

void
kdb_encrypt_key (in, out, master_key, master_key_sched, e_d_flag)
	des_cblock *in;
	des_cblock *out;
	des_cblock *master_key;
	struct des_ks_struct *master_key_sched;
	int e_d_flag;
{

#ifdef NOENCRYPTION
  bcopy(in, out, sizeof(des_cblock));
#else
  des_pcbc_encrypt(in,out,(long)sizeof(des_cblock),master_key_sched,master_key,
 	e_d_flag);
#endif
}

/* The caller is reasponsible for cleaning up the master key and sched,
   even if we can't verify the master key */

/* Returns master key version if successful, otherwise -1 */

long 
kdb_verify_master_key (master_key, master_key_sched, out)
	des_cblock *master_key;
	struct des_ks_struct *master_key_sched;
	FILE *out;		/* setting this to non-null be do output */
{
  des_cblock key_from_db;
  Principal principal_data[1];
  int n, more = 0;
  long master_key_version;

  /* lookup the master key version */
  n = kerb_get_principal(KERB_M_NAME, KERB_M_INST, principal_data,
			 1 /* only one please */, &more);
  if ((n != 1) || more) {
    if (out != (FILE *) NULL) 
      fprintf(out,
	      "verify_master_key: %s, %d found.\n",
	      "Kerberos error on master key version lookup",
	      n);
    return (-1);
  }

  master_key_version = (long) principal_data[0].key_version;

  /* set up the master key */
  if (out != (FILE *) NULL)  /* should we punt this? */
    fprintf(out, "Current Kerberos master key version is %d.\n",
	    principal_data[0].kdc_key_ver);

  /*
   * now use the master key to decrypt the key in the db, had better
   * be the same! 
   */
  bcopy(&principal_data[0].key_low, key_from_db, 4);
  bcopy(&principal_data[0].key_high, ((long *) key_from_db) + 1, 4);
  kdb_encrypt_key (&key_from_db, &key_from_db, 
		   master_key, master_key_sched, DES_DECRYPT);

  /* the decrypted database key had better equal the master key */
  n = bcmp((char *) master_key, (char *) key_from_db,
	   sizeof(master_key));
  /* this used to zero the master key here! */
  bzero(key_from_db, sizeof(key_from_db));
  bzero(principal_data, sizeof (principal_data));

  if (n && (out != (FILE *) NULL)) {
    fprintf(out, "\n\07\07verify_master_key: Invalid master key; ");
    fprintf(out, "does not match database.\n");
    return (-1);
  }
  if (out != (FILE *) NULL) {
    fprintf(out, "\nMaster key entered.  BEWARE!\07\07\n");
    fflush(out);
  }

  return (master_key_version);
}
