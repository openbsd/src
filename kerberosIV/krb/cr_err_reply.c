/*
 * This software may now be redistributed outside the US.
 *
 * $Source: /home/cvs/src/kerberosIV/krb/Attic/cr_err_reply.c,v $
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
 * req_act_vno used to be defined as an extern ("defined in server").
 * However, that does noone anything good, so we define our own so
 * that the shared libraries do not turn up with an undefined variable!
 */
static int my_req_act_vno = KRB_PROT_VERSION;

/*
 * This routine is used by the Kerberos authentication server to
 * create an error reply packet to send back to its client.
 *
 * It takes a pointer to the packet to be built, the name, instance,
 * and realm of the principal, the client's timestamp, an error code
 * and an error string as arguments.  Its return value is undefined.
 *
 * The packet is built in the following format:
 * 
 * type			variable	   data
 *			or constant
 * ----			-----------	   ----
 *
 * unsigned char	req_ack_vno	   protocol version number
 * 
 * unsigned char	AUTH_MSG_ERR_REPLY protocol message type
 * 
 * [least significant	HOST_BYTE_ORDER	   sender's (server's) byte
 * bit of above field]			   order
 * 
 * string		pname		   principal's name
 * 
 * string		pinst		   principal's instance
 * 
 * string		prealm		   principal's realm
 * 
 * unsigned long	time_ws		   client's timestamp
 * 
 * unsigned long	e		   error code
 * 
 * string		e_string	   error text
 */

void
cr_err_reply(pkt, pname, pinst, prealm, time_ws, e, e_string)
	KTEXT pkt;
	char *pname;		/* Principal's name */
	char *pinst;		/* Principal's instance */
	char *prealm;		/* Principal's authentication domain */
	u_int32_t time_ws;	/* Workstation time */
	u_int32_t e;		/* Error code */
	char *e_string;		/* Text of error */
{
    u_char *v = (u_char *) pkt->dat; /* Prot vers number */
    u_char *t = (u_char *)(pkt->dat+1); /* Prot message type */

    /* Create fixed part of packet */
    *v = (unsigned char) my_req_act_vno; /* KRB_PROT_VERSION; */
    *t = (unsigned char) AUTH_MSG_ERR_REPLY;
    *t |= HOST_BYTE_ORDER;

    if (pname == 0)
        pname = "";
    if (pinst == 0)
        pinst = "";
    if (prealm == 0)
        prealm = "";

    /* Add the basic info */
    (void) strcpy((char *) (pkt->dat+2),pname);
    pkt->length = 3 + strlen(pname);
    (void) strcpy((char *)(pkt->dat+pkt->length),pinst);
    pkt->length += 1 + strlen(pinst);
    (void) strcpy((char *)(pkt->dat+pkt->length),prealm);
    pkt->length += 1 + strlen(prealm);
    /* ws timestamp */
    bcopy((char *) &time_ws,(char *)(pkt->dat+pkt->length),4);
    pkt->length += 4;
    /* err code */
    bcopy((char *) &e,(char *)(pkt->dat+pkt->length),4);
    pkt->length += 4;
    /* err text */
    (void) strcpy((char *)(pkt->dat+pkt->length),e_string);
    pkt->length += 1 + strlen(e_string);

    /* And return */
    return;
}
