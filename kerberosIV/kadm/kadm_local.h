/*	$Id: kadm_local.h,v 1.1.1.1 1995/12/14 06:52:45 tholo Exp $	*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <errno.h>

#include <sys/socket.h>
#include <sys/param.h>
#include <netinet/in.h>
#include <netdb.h>

#include <kerberosIV/des.h>
#include <kerberosIV/krb.h>
#include "krb_err.h"
#include <kerberosIV/krb_db.h>
#include <kerberosIV/kadm.h>
#include "kadm_err.h"

int vts_long __P((u_int32_t, u_char **, int));
int vals_to_stream __P((Kadm_vals *, u_char **));
int stream_to_vals __P((u_char *, Kadm_vals *, int));

int kadm_init_link __P((char n[], char i[], char r[]));
int kadm_change_pw __P((des_cblock));
int kadm_add __P((Kadm_vals *));
int kadm_mod __P((Kadm_vals *, Kadm_vals *));
int kadm_get __P((Kadm_vals *, u_char fl[4]));
