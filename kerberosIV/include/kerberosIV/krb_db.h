/*	$Id: krb_db.h,v 1.1.1.1 1995/12/14 06:52:35 tholo Exp $	*/

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

/* spm		Project Athena  8/85 
 *
 * This file defines data structures for the kerberos
 * authentication/authorization database. 
 *
 * They MUST correspond to those defined in *.rel 
 */

#ifndef KRB_DB_DEFS
#define KRB_DB_DEFS

#define KERB_M_NAME		"K"	/* Kerberos */
#define KERB_M_INST		"M"	/* Master */
#define KERB_DEFAULT_NAME	"default"
#define KERB_DEFAULT_INST	""

/* this also defines the number of queue headers */
#define KERB_DB_HASH_MODULO 64


/* Arguments to kerb_dbl_lock() */

#define KERB_DBL_EXCLUSIVE 1
#define KERB_DBL_SHARED 0

/* arguments to kerb_db_set_lockmode() */

#define KERB_DBL_BLOCKING 0
#define KERB_DBL_NONBLOCKING 1

/* Principal defines the structure of a principal's name */

typedef struct {
    char    name[ANAME_SZ];
    char    instance[INST_SZ];

    u_int32_t key_low;
    u_int32_t key_high;
    u_int32_t exp_date;
    char    exp_date_txt[DATE_SZ];
    u_int32_t mod_date;
    char    mod_date_txt[DATE_SZ];
    u_int16_t attributes;
    u_int8_t max_life;
    u_int8_t kdc_key_ver;
    u_int8_t key_version;

    char    mod_name[ANAME_SZ];
    char    mod_instance[INST_SZ];
    char   *old;		/* cast to (Principal *); not in db,
				 * ptr to old vals */
}
        Principal;

typedef struct {
    int32_t    cpu;
    int32_t    elapsed;
    int32_t    dio;
    int32_t    pfault;
    int32_t    t_stamp;
    int32_t    n_retrieve;
    int32_t    n_replace;
    int32_t    n_append;
    int32_t    n_get_stat;
    int32_t    n_put_stat;
}
        DB_stat;

/* Dba defines the structure of a database administrator */

typedef struct {
    char    name[ANAME_SZ];
    char    instance[INST_SZ];
    u_int16_t attributes;
    u_int32_t exp_date;
    char    exp_date_txt[DATE_SZ];
    char   *old;	/*
			 * cast to (Dba *); not in db, ptr to
			 * old vals
			 */
}
        Dba;

int kerb_get_principal __P((char *, char *, Principal *, unsigned int, int *));
int kerb_put_principal __P((Principal *, unsigned int));
void kerb_db_get_stat __P((DB_stat *));
void kerb_db_put_stat __P((DB_stat *));
int kerb_get_dba __P((char *, char *, Dba *, unsigned int, int *));
int kerb_db_get_dba __P(());
int kerb_init __P((void));
void kerb_fini __P((void));
time_t kerb_get_db_age __P((void));

void kdb_encrypt_key __P((des_cblock *, des_cblock *, des_cblock *, des_key_schedule, int));
int kerb_db_set_name __P((char *));

long kdb_get_master_key __P((int, des_cblock *, des_key_schedule));

#include <stdio.h>
long kdb_verify_master_key __P((des_cblock *, des_key_schedule, FILE *));

int kerb_db_create __P((char *db_name));
int kerb_db_put_principal __P((Principal *, unsigned int));
int kerb_db_iterate __P((int (*)(char *, Principal *), char *));
int kerb_db_rename __P((char *, char *));
int kerb_db_set_lockmode __P((int));

#endif /* KRB_DB_DEFS */
