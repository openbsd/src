/* $Id: krb_locl.h,v 1.1.1.1 1995/12/14 06:52:38 tholo Exp $ */

#ifndef __krb_locl_h
#define __krb_locl_h

#include <sys/cdefs.h>
#include "kerberosIV/site.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <unistd.h>
#include <sys/types.h>
#include <time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/file.h>
#include <errno.h>

#include <kerberosIV/krb.h>
#include <prot.h>

/* --- */

/* Globals! */
extern int krb_debug;
extern int krb_ap_req_debug;

/* Utils */
char *pkt_cipher __P((KTEXT));

int new_log __P((time_t, char *));
char *klog ();

char *month_sname __P((int));
int fgetst __P((FILE *, char *, int));

#endif /*  __krb_locl_h */
