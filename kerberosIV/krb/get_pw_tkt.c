/*
 * This software may now be redistributed outside the US.
 *
 * $Source: /home/cvs/src/kerberosIV/krb/Attic/get_pw_tkt.c,v $
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

#include "krb_locl.h"

/*
 * Get a ticket for the password-changing server ("changepw.KRB_MASTER").
 *
 * Given the name, instance, realm, and current password of the
 * principal for which the user wants a password-changing-ticket,
 * return either:
 *
 *	GT_PW_BADPW if current password was wrong,
 *	GT_PW_NULL  if principal had a NULL password,
 *	or the result of the krb_get_pw_in_tkt() call.
 *
 * First, try to get a ticket for "user.instance@realm" to use the
 * "changepw.KRB_MASTER" server (KRB_MASTER is defined in "krb.h").
 * The requested lifetime for the ticket is "1", and the current
 * password is the "cpw" argument given.
 *
 * If the password was bad, give up.
 *
 * If the principal had a NULL password in the Kerberos database
 * (indicating that the principal is known to Kerberos, but hasn't
 * got a password yet), try instead to get a ticket for the principal
 * "default.changepw@realm" to use the "changepw.KRB_MASTER" server.
 * Use the password "changepwkrb" instead of "cpw".  Return GT_PW_NULL
 * if all goes well, otherwise the error.
 *
 * If this routine succeeds, a ticket and session key for either the
 * principal "user.instance@realm" or "default.changepw@realm" to use
 * the password-changing server will be in the user's ticket file.
 */

int
get_pw_tkt(user, instance, realm, cpw)
	char *user;
	char *instance;
	char *realm;
	char *cpw;
{
    int kerror;

    kerror = krb_get_pw_in_tkt(user, instance, realm, "changepw",
			       KRB_MASTER, 1, cpw);

    if (kerror == INTK_BADPW)
	return(GT_PW_BADPW);

    if (kerror == KDC_NULL_KEY) {
	kerror = krb_get_pw_in_tkt("default","changepw",realm,"changepw",
				   KRB_MASTER,1,"changepwkrb");
	if (kerror)
	    return(kerror);
	return(GT_PW_NULL);
    }

    return(kerror);
}
