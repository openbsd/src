/*	$Id: kadm_locl.h,v 1.1.1.1 1995/12/14 06:52:34 tholo Exp $	*/

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

#include <sys/cdefs.h>
#include "kerberosIV/site.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>

#include <sys/time.h>
#include <time.h>

#include <sys/stat.h>
#include <fcntl.h>

#include <errno.h>
#include <signal.h>
#include <unistd.h>

#include <sys/wait.h>
#include <pwd.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include <syslog.h>

#include "kerberosIV/com_err.h"
#include <ss/ss.h>

#include <kerberosIV/des.h>
#include <kerberosIV/krb.h>
#include "krb_err.h"
#include <kerberosIV/krb_db.h>
#include <kerberosIV/kadm.h>
#include "kadm_err.h"
#include "kerberosIV/acl.h"

#include "kadm_server.h"

/* GLOBALS */
extern char *acldir;
extern Kadm_Server server_parm;

/* Utils */
int kadm_change __P((char *, char *, char *, des_cblock));
int kadm_add_entry __P((char *, char *, char *, Kadm_vals *, Kadm_vals *));
int kadm_mod_entry __P((char *, char *, char *, Kadm_vals *, Kadm_vals *, Kadm_vals *));
int kadm_get_entry __P((char *, char *, char *, Kadm_vals *, u_char *, Kadm_vals *));
int kadm_ser_cpw __P((u_char *, int, AUTH_DAT *, u_char **, int *));
int kadm_ser_add __P((u_char *, int, AUTH_DAT *, u_char **, int *));
int kadm_ser_mod __P((u_char *, int, AUTH_DAT *, u_char **, int *));
int kadm_ser_get __P((u_char *, int, AUTH_DAT *, u_char **, int *));
int kadm_ser_init __P((int inter, char realm[]));
int kadm_ser_in __P((u_char **, int *));

long maketime __P((struct tm *, int));

void change_password __P((int argc, char *argv[]));
void change_admin_password __P((int argc, char *argv[]));
void add_new_key __P((int argc, char *argv[]));
void get_entry __P((int argc, char *argv[]));
void mod_entry __P((int argc, char *argv[]));
void help __P((int argc, char *argv[]));
void clean_up __P((void));
void quit __P((void));
