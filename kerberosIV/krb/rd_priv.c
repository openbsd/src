/*
 * This software may now be redistributed outside the US.
 *
 * $Source: /home/cvs/src/kerberosIV/krb/Attic/rd_priv.c,v $
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
 * This routine dissects a a Kerberos 'private msg', decrypting it,
 * checking its integrity, and returning a pointer to the application
 * data contained and its length.
 *
 * Returns 0 (RD_AP_OK) for success or an error code (RD_AP_...).  If
 * the return value is RD_AP_TIME, then either the times are too far
 * out of synch, OR the packet was modified.
 *
 * Steve Miller    Project Athena  MIT/DEC
 */

#include "krb_locl.h"

/* system include files */
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/time.h>

/* application include files */
#include "lsb_addr_comp.h"

/* static storage */
static u_int32_t c_length;
static int swap_bytes;
static struct timeval local_time;
static long delta_t;

/* Global! */
int private_msg_ver = KRB_PROT_VERSION;

/*
#ifdef NOENCRPYTION
 * krb_rd_priv() checks the integrity of an
#else
 * krb_rd_priv() decrypts and checks the integrity of an
#endif
 * AUTH_MSG_PRIVATE message.  Given the message received, "in",
 * the length of that message, "in_length", the key "schedule"
 * and "key", and the network addresses of the
 * "sender" and "receiver" of the message, krb_rd_safe() returns
 * RD_AP_OK if the message is okay, otherwise some error code.
 *
 * The message data retrieved from "in" are returned in the structure
 * "m_data".  The pointer to the application data
 * (m_data->app_data) refers back to the appropriate place in "in".
 *
 * See the file "mk_priv.c" for the format of the AUTH_MSG_PRIVATE
 * message.  The structure containing the extracted message
 * information, MSG_DAT, is defined in "krb.h".
 */

int32_t
krb_rd_priv(in, in_length, schedule, key, sender, receiver, m_data)
	u_char *in;		/* pointer to the msg received */
	u_int32_t in_length;	/* length of "in" msg */
	struct des_ks_struct *schedule;	/* precomputed key schedule */
	des_cblock *key;	/* encryption key for seed and ivec */
	struct sockaddr_in *sender;
	struct sockaddr_in *receiver;
	MSG_DAT *m_data;	/*various input/output data from msg */
{
    register u_char *p,*q;
    static u_int32_t src_addr;	/* Can't send structs since no
				 * guarantees on size */

    if (gettimeofday(&local_time,(struct timezone *)0))
        return  -1;

    p = in;			/* beginning of message */
    swap_bytes = 0;

    if (*p++ != KRB_PROT_VERSION && *(p-1) != 3)
        return RD_AP_VERSION;
    private_msg_ver = *(p-1);
    if (((*p) & ~1) != AUTH_MSG_PRIVATE)
        return RD_AP_MSG_TYPE;
    if ((*p++ & 1) != HOST_BYTE_ORDER)
        swap_bytes++;

    /* get cipher length */
    bcopy((char *)p,(char *)&c_length,sizeof(c_length));
    if (swap_bytes)
        swap_u_long(c_length);
    p += sizeof(c_length);
    /* check for rational length so we don't go comatose */
    if (VERSION_SZ + MSG_TYPE_SZ + c_length > in_length)
        return RD_AP_MODIFIED;


    q = p;			/* mark start of encrypted stuff */

#ifndef NOENCRYPTION
    des_pcbc_encrypt((des_cblock *)q,(des_cblock *)q,(long)c_length,schedule,key,DES_DECRYPT);
#endif

    /* safely get application data length */
    bcopy((char *) p,(char *)&(m_data->app_length),
          sizeof(m_data->app_length));
    if (swap_bytes)
        swap_u_long(m_data->app_length);
    p += sizeof(m_data->app_length);    /* skip over */

    if (m_data->app_length + sizeof(c_length) + sizeof(in_length) +
        sizeof(m_data->time_sec) + sizeof(m_data->time_5ms) +
        sizeof(src_addr) + VERSION_SZ + MSG_TYPE_SZ
        > in_length)
        return RD_AP_MODIFIED;

#ifndef NOENCRYPTION
    /* we're now at the decrypted application data */
#endif
    m_data->app_data = p;

    p += m_data->app_length;

    /* safely get time_5ms */
    bcopy((char *) p, (char *)&(m_data->time_5ms),
	  sizeof(m_data->time_5ms));
    /*  don't need to swap-- one byte for now */
    p += sizeof(m_data->time_5ms);

    /* safely get src address */
    bcopy((char *) p,(char *)&src_addr,sizeof(src_addr));
    /* don't swap, net order always */
    p += sizeof(src_addr);

    if (!krb_equiv(src_addr, sender->sin_addr.s_addr))
	return RD_AP_MODIFIED;

    /* safely get time_sec */
    bcopy((char *) p, (char *)&(m_data->time_sec),
	  sizeof(m_data->time_sec));
    if (swap_bytes) swap_u_long(m_data->time_sec);

    p += sizeof(m_data->time_sec);

    /* check direction bit is the sign bit */
    /* For compatibility with broken old code, compares are done in VAX 
       byte order (LSBFIRST) */ 
    if (lsb_net_ulong_less(sender->sin_addr.s_addr,
			   receiver->sin_addr.s_addr)==-1) 
	/* src < recv */ 
	m_data->time_sec =  - m_data->time_sec; 
    else if (lsb_net_ulong_less(sender->sin_addr.s_addr, 
				receiver->sin_addr.s_addr)==0) 
	if (lsb_net_ushort_less(sender->sin_port,receiver->sin_port)==-1)
	    /* src < recv */
	    m_data->time_sec =  - m_data->time_sec; 
    /*
     * all that for one tiny bit!
     * Heaven help those that talk to themselves.
     */

    /* check the time integrity of the msg */
    delta_t = abs((int)((long) local_time.tv_sec
			- m_data->time_sec));
    if (delta_t > CLOCK_SKEW)
	return RD_AP_TIME;
    if (krb_debug)
	printf("\ndelta_t = %d", (int) delta_t);

    /*
     * caller must check timestamps for proper order and
     * replays, since server might have multiple clients
     * each with its own timestamps and we don't assume
     * tightly synchronized clocks.
     */

#ifdef notdef
    bcopy((char *) p,(char *)&cksum,sizeof(cksum));
    if (swap_bytes) swap_u_long(cksum)
    /*
     * calculate the checksum of the length, sequence,
     * and input data, on the sending byte order!!
     */
    calc_cksum = des_quad_cksum(q,NULL,p-q,0,key);

    if (krb_debug)
	printf("\ncalc_cksum = %u, received cksum = %u",
	       calc_cksum, cksum);
    if (cksum != calc_cksum)
	return RD_AP_MODIFIED;
#endif
    return RD_AP_OK;        /* OK == 0 */
}
