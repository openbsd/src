/*	$Id: kdb_locl.h,v 1.1.1.1 1995/12/14 06:52:37 tholo Exp $	*/

#ifndef __kdb_locl_h
#define __kdb_locl_h

#include "kerberosIV/site.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <errno.h>

#include <sys/types.h>

#include <sys/time.h>
#include <time.h>

#include <sys/stat.h>
#include <fcntl.h>
#include <utime.h>
#include <sys/file.h>

#include <kerberosIV/krb.h>
#include <kerberosIV/krb_db.h>

/* --- */

/* Globals! */

/* Utils */

int kerb_db_set_lockmode __P((int));
void kerb_db_fini __P((void));
int kerb_db_init __P((void));
int kerb_db_set_name __P((char *name));
time_t kerb_get_db_age __P((void));
int kerb_db_create __P((char *db_name));
int kerb_db_rename __P((char *from, char *to));

int kerb_db_get_principal __P((char *name, char *, Principal *, unsigned int, int *));
int kerb_db_put_principal __P((Principal *, unsigned int));
int kerb_db_get_dba __P((char *, char *, Dba *, unsigned int, int *));

void kerb_db_get_stat __P((DB_stat *));
void kerb_db_put_stat __P((DB_stat *));
void delta_stat __P((DB_stat *, DB_stat *, DB_stat *));

int kerb_db_iterate __P((int (*func) (/* ??? */), char *arg));

int kerb_cache_init __P((void));
int kerb_cache_get_principal __P((char *name, char *, Principal *, unsigned int));
int kerb_cache_put_principal __P((Principal *, unsigned int));
int kerb_cache_get_dba __P((char *, char *, Dba *, unsigned int));
int kerb_cache_put_dba __P((Dba *, unsigned int));

void krb_print_principal __P((Principal *));

#endif /*  __kdb_locl_h */
