/*
 * This software may now be redistributed outside the US.
 *
 * $Source: /home/cvs/src/kerberosIV/krb/Attic/mk_priv.c,v $
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
 * This routine constructs a Kerberos 'private msg', i.e.
 * cryptographically sealed with a private session key.
 *
 * Note-- bcopy is used to avoid alignment problems on IBM RT.
 *
 * Note-- It's too bad that it did a long int compare on the RT before.
 *
 * Returns either < 0 ===> error, or resulting size of message
 *
 * Steve Miller    Project Athena  MIT/DEC
 */

#include "krb_locl.h"

#include <sys/types.h>
#include <netinet/in.h>
#include <sys/time.h>

/* application include files */
#include "lsb_addr_comp.h"

/* static storage */
static u_int32_t c_length;
static struct timeval msg_time;
static u_char msg_time_5ms;
static long msg_time_sec;

/*
 * krb_mk_priv() constructs an AUTH_MSG_PRIVATE message.  It takes
 * some user data "in" of "length" bytes and creates a packet in "out"
 * consisting of the user data, a timestamp, and the sender's network
 * address.
#ifndef NOENCRYTION
 * The packet is encrypted by pcbc_encrypt(), using the given
 * "key" and "schedule".
#endif
 * The length of the resulting packet "out" is
 * returned.
 *
 * It is similar to krb_mk_safe() except for the additional key
 * schedule argument "schedule" and the fact that the data is encrypted
 * rather than appended with a checksum.  Also, the protocol version
 * number is "private_msg_ver", defined in krb_rd_priv.c, rather than
 * KRB_PROT_VERSION, defined in "krb.h".
 *
 * The "out" packet consists of:
 *
 * Size			Variable		Field
 * ----			--------		-----
 *
 * 1 byte		private_msg_ver		protocol version number
 * 1 byte		AUTH_MSG_PRIVATE |	message type plus local
 *			HOST_BYTE_ORDER		byte order in low bit
 *
 * 4 bytes		c_length		length of data
#ifndef NOENCRYPT
 * we encrypt from here with pcbc_encrypt
#endif
 * 
 * 4 bytes		length			length of user data
 * length		in			user data
 * 1 byte		msg_time_5ms		timestamp milliseconds
 * 4 bytes		sender->sin.addr.s_addr	sender's IP address
 *
 * 4 bytes		msg_time_sec or		timestamp seconds with
 *			-msg_time_sec		direction in sign bit
 *
 * 0<=n<=7  bytes	pad to 8 byte multiple	zeroes
 */

int32_t
krb_mk_priv(in, out, length, schedule, key, sender, receiver)
	u_char *in;		/* application data */
	u_char *out;		/* put msg here, leave room for
                                 * header! breaks if in and out
                                 * (header stuff) overlap */
	u_int32_t length;	/* of in data */
	struct des_ks_struct *schedule;	/* precomputed key schedule */
	des_cblock *key;	/* encryption key for seed and ivec */
	struct sockaddr_in *sender; /* sender address */
	struct sockaddr_in *receiver; /* receiver address */
{
    register u_char     *p,*q;
    static       u_char *c_length_ptr;

    /*
     * get the current time to use instead of a sequence #, since
     * process lifetime may be shorter than the lifetime of a session
     * key.
     */
    if (gettimeofday(&msg_time,(struct timezone *)0)) {
        return -1;
    }
    msg_time_sec = (long) msg_time.tv_sec;
    msg_time_5ms = msg_time.tv_usec/5000; /* 5ms quanta */

    p = out;

    *p++ = private_msg_ver;
    *p++ = AUTH_MSG_PRIVATE | HOST_BYTE_ORDER;

    /* calculate cipher length */
    c_length_ptr = p;
    p += sizeof(c_length);

    q = p;

    /* stuff input length */
    bcopy((char *)&length,(char *)p,sizeof(length));
    p += sizeof(length);

#ifdef NOENCRYPTION
    /* make all the stuff contiguous for checksum */
#else
    /* make all the stuff contiguous for checksum and encryption */
#endif
    bcopy((char *)in,(char *)p,(int) length);
    p += length;

    /* stuff time 5ms */
    bcopy((char *)&msg_time_5ms,(char *)p,sizeof(msg_time_5ms));
    p += sizeof(msg_time_5ms);

    /* stuff source address */
    bcopy((char *)&sender->sin_addr.s_addr,(char *)p,
          sizeof(sender->sin_addr.s_addr));
    p += sizeof(sender->sin_addr.s_addr);

    /*
     * direction bit is the sign bit of the timestamp.  Ok
     * until 2038??
     */
    /* For compatibility with broken old code, compares are done in VAX 
       byte order (LSBFIRST) */ 
    if (lsb_net_ulong_less(sender->sin_addr.s_addr, /* src < recv */ 
			  receiver->sin_addr.s_addr)==-1) 
        msg_time_sec =  -msg_time_sec; 
    else if (lsb_net_ulong_less(sender->sin_addr.s_addr, 
				receiver->sin_addr.s_addr)==0) 
        if (lsb_net_ushort_less(sender->sin_port,receiver->sin_port) == -1) 
            msg_time_sec = -msg_time_sec; 
    /* stuff time sec */
    bcopy((char *)&msg_time_sec,(char *)p,sizeof(msg_time_sec));
    p += sizeof(msg_time_sec);

    /*
     * All that for one tiny bit!  Heaven help those that talk to
     * themselves.
     */

#ifdef notdef
    /*
     * calculate the checksum of the length, address, sequence, and
     * inp data
     */
    cksum =  des_quad_cksum(q,NULL,p-q,0,key);
    if (krb_debug)
        printf("\ncksum = %u",cksum);
    /* stuff checksum */
    bcopy((char *) &cksum,(char *) p,sizeof(cksum));
    p += sizeof(cksum);
#endif

    /*
     * All the data have been assembled, compute length
     */

    c_length = p - q;
    c_length = ((c_length + sizeof(des_cblock) -1)/sizeof(des_cblock)) *
        sizeof(des_cblock);
    /* stuff the length */
    bcopy((char *) &c_length,(char *)c_length_ptr,sizeof(c_length));

#ifndef NOENCRYPTION
    des_pcbc_encrypt((des_cblock *)q,(des_cblock *)q,(long)(p-q),schedule,key, DES_ENCRYPT);
#endif /* NOENCRYPTION */

    return (q - out + c_length);        /* resulting size */
}
