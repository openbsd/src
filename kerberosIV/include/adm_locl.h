/*	$Id: adm_locl.h,v 1.1.1.1 1995/12/14 06:52:34 tholo Exp $	*/

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

#ifndef __adm_locl_h
#define __adm_locl_h

#define TRUE 1
#define FALSE 0

#include <sys/cdefs.h>
#include <kerberosIV/site.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <sys/types.h>

#include <sys/time.h>
#include <time.h>

#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>

#include <netinet/in.h>

#include <kerberosIV/des.h>
#include <kerberosIV/krb.h>
#include <kerberosIV/krb_db.h>
#include "kdc.h"

/* Utils */
long maketime __P((struct tm *, int));

#endif /*  __adm_locl_h */
