/*
 * This software may now be redistributed outside the US.
 *
 * $Source: /home/cvs/src/kerberosIV/krb/Attic/rd_err.c,v $
 *
 * $Locker:  $
 */

/* 
  Copyright (C) 1989 by the Massachusetts Institute of Technology

   Export of this software from the United States of America is assumed
   to require a specific license from the United States Government.
   It is the responsibility of any person or organization contemplating
   export to obtain such a license before exporting.

WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
distribute this software and its documentation for any purpose and
without fee is hereby granted, provided that the above copyright
notice appear in all copies and that both that copyright notice and
this permission notice appear in supporting documentation, and that
the name of M.I.T. not be used in advertising or publicity pertaining
to distribution of the software without specific, written prior
permission.  M.I.T. makes no representations about the suitability of
this software for any purpose.  It is provided "as is" without express
or implied warranty.

  */

/*
 * This routine dissects a a Kerberos 'safe msg',
 * checking its integrity, and returning a pointer to the application
 * data contained and its length.
 *
 * Returns 0 (RD_AP_OK) for success or an error code (RD_AP_...)
 *
 * Steve Miller    Project Athena  MIT/DEC
 */

#include "krb_locl.h"

/* system include files */
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/time.h>

/*
 * Given an AUTH_MSG_APPL_ERR message, "in" and its length "in_length",
 * return the error code from the message in "code" and the text in
 * "m_data" as follows:
 *
 *	m_data->app_data	points to the error text
 *	m_data->app_length	points to the length of the error text
 *
 * If all goes well, return RD_AP_OK.  If the version number
 * is wrong, return RD_AP_VERSION, and if it's not an AUTH_MSG_APPL_ERR
 * type message, return RD_AP_MSG_TYPE.
 *
 * The AUTH_MSG_APPL_ERR message format can be found in mk_err.c
 */

int
krb_rd_err(in, in_length, code, m_data)
	u_char *in;		/* pointer to the msg received */
	u_int32_t in_length;	/* of in msg */
	int32_t *code;		/* received error code */
	MSG_DAT *m_data;
{
    register u_char *p;
    int swap_bytes = 0;
    p = in;                     /* beginning of message */

    if (*p++ != KRB_PROT_VERSION)
        return(RD_AP_VERSION);
    if (((*p) & ~1) != AUTH_MSG_APPL_ERR)
        return(RD_AP_MSG_TYPE);
    if ((*p++ & 1) != HOST_BYTE_ORDER)
        swap_bytes++;

    /* safely get code */
    bcopy((char *)p,(char *)code,sizeof(*code));
    if (swap_bytes)
        swap_u_long(*code);
    p += sizeof(*code);         /* skip over */

    m_data->app_data = p;       /* we're now at the error text
                                 * message */
    m_data->app_length = in_length;

    return(RD_AP_OK);           /* OK == 0 */
}
