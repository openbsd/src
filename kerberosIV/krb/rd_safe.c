/*
 * This software may now be redistributed outside the US.
 *
 * $Source: /home/cvs/src/kerberosIV/krb/Attic/rd_safe.c,v $
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
 * This routine dissects a a Kerberos 'safe msg', checking its
 * integrity, and returning a pointer to the application data
 * contained and its length.
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

/* application include files */
#include "lsb_addr_comp.h"

/* static storage */
static des_cblock calc_cksum[2];
static des_cblock big_cksum[2];
static int swap_bytes;
static struct timeval local_time;
static u_int32_t delta_t;

/*
 * krb_rd_safe() checks the integrity of an AUTH_MSG_SAFE message.
 * Given the message received, "in", the length of that message,
 * "in_length", the "key" to compute the checksum with, and the
 * network addresses of the "sender" and "receiver" of the message,
 * krb_rd_safe() returns RD_AP_OK if message is okay, otherwise
 * some error code.
 *
 * The message data retrieved from "in" is returned in the structure
 * "m_data".  The pointer to the application data (m_data->app_data)
 * refers back to the appropriate place in "in".
 *
 * See the file "mk_safe.c" for the format of the AUTH_MSG_SAFE
 * message.  The structure containing the extracted message
 * information, MSG_DAT, is defined in "krb.h".
 */

int32_t
krb_rd_safe(in, in_length, key, sender, receiver, m_data)
	u_char *in;		/* pointer to the msg received */
	u_int32_t in_length;	/* length of "in" msg */
	des_cblock *key;	/* encryption key for seed and ivec */
	struct sockaddr_in *sender; /* sender's address */
	struct sockaddr_in *receiver; /* receiver's address -- me */
	MSG_DAT *m_data;	/* where to put message information */
{
    register u_char     *p,*q;
    static      u_int32_t  src_addr; /* Can't send structs since no
                                   * guarantees on size */
    /* Be very conservative */
    if (sizeof(src_addr) != sizeof(struct in_addr)) {
        fprintf(stderr,"\n\
krb_rd_safe protocol err sizeof(src_addr) != sizeof(struct in_addr)");
        exit(-1);
    }

    if (gettimeofday(&local_time,(struct timezone *)0))
        return  -1;

    p = in;                     /* beginning of message */
    swap_bytes = 0;

    if (*p++ != KRB_PROT_VERSION)       return RD_AP_VERSION;
    if (((*p) & ~1) != AUTH_MSG_SAFE) return RD_AP_MSG_TYPE;
    if ((*p++ & 1) != HOST_BYTE_ORDER) swap_bytes++;

    q = p;                      /* mark start of cksum stuff */

    /* safely get length */
    bcopy((char *)p,(char *)&(m_data->app_length),
          sizeof(m_data->app_length));
    if (swap_bytes) swap_u_long(m_data->app_length);
    p += sizeof(m_data->app_length); /* skip over */

    if (m_data->app_length + sizeof(in_length)
        + sizeof(m_data->time_sec) + sizeof(m_data->time_5ms)
        + sizeof(big_cksum) + sizeof(src_addr)
        + VERSION_SZ + MSG_TYPE_SZ > in_length)
        return(RD_AP_MODIFIED);

    m_data->app_data = p;       /* we're now at the application data */

    /* skip app data */
    p += m_data->app_length;

    /* safely get time_5ms */
    bcopy((char *)p, (char *)&(m_data->time_5ms),
          sizeof(m_data->time_5ms));

    /* don't need to swap-- one byte for now */
    p += sizeof(m_data->time_5ms);

    /* safely get src address */
    bcopy((char *)p,(char *)&src_addr,sizeof(src_addr));

    /* don't swap, net order always */
    p += sizeof(src_addr);

    if (!krb_equiv(src_addr, sender->sin_addr.s_addr))
        return RD_AP_MODIFIED;

    /* safely get time_sec */
    bcopy((char *)p, (char *)&(m_data->time_sec),
          sizeof(m_data->time_sec));
    if (swap_bytes)
        swap_u_long(m_data->time_sec);
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
     * All that for one tiny bit!  Heaven help those that talk to
     * themselves.
     */

    /* check the time integrity of the msg */
    delta_t = abs((int)((long) local_time.tv_sec - m_data->time_sec));
    if (delta_t > CLOCK_SKEW) return RD_AP_TIME;

    /*
     * caller must check timestamps for proper order and replays, since
     * server might have multiple clients each with its own timestamps
     * and we don't assume tightly synchronized clocks.
     */

    bcopy((char *)p,(char *)big_cksum,sizeof(big_cksum));
    if (swap_bytes) swap_u_16(big_cksum);

#ifdef NOENCRYPTION
    bzero(calc_cksum, sizeof(calc_cksum));
#else
    des_quad_cksum((des_cblock *)q,calc_cksum,p-q,2,key);
#endif

    if (krb_debug)
        printf("\ncalc_cksum = %u, received cksum = %u",
               (u_int) calc_cksum[0], (u_int) big_cksum[0]);
    if (bcmp((char *)big_cksum,(char *)calc_cksum,sizeof(big_cksum)))
        return(RD_AP_MODIFIED);

    return(RD_AP_OK);           /* OK == 0 */
}
