/*	$OpenBSD: tcp.h,v 1.8 1999/07/06 20:17:52 cmetz Exp $	*/
/*	$NetBSD: tcp.h,v 1.8 1995/04/17 05:32:58 cgd Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)tcp.h	8.1 (Berkeley) 6/10/93
 */

#ifndef _NETINET_TCP_H_
#define	_NETINET_TCP_H_

typedef u_int32_t tcp_seq;

/*
 * TCP header.
 * Per RFC 793, September, 1981.
 */
struct tcphdr {
	u_int16_t th_sport;		/* source port */
	u_int16_t th_dport;		/* destination port */
	tcp_seq	  th_seq;		/* sequence number */
	tcp_seq	  th_ack;		/* acknowledgement number */
#if BYTE_ORDER == LITTLE_ENDIAN
	u_int8_t  th_x2:4,		/* (unused) */
		  th_off:4;		/* data offset */
#endif
#if BYTE_ORDER == BIG_ENDIAN
	u_int8_t  th_off:4,		/* data offset */
		  th_x2:4;		/* (unused) */
#endif
	u_int8_t  th_flags;
#define	TH_FIN	  0x01
#define	TH_SYN	  0x02
#define	TH_RST	  0x04
#define	TH_PUSH	  0x08
#define	TH_ACK	  0x10
#define	TH_URG	  0x20
	u_int16_t th_win;			/* window */
	u_int16_t th_sum;			/* checksum */
	u_int16_t th_urp;			/* urgent pointer */
};
#define th_reseqlen th_urp			/* TCP data length for 
						   resequencing/reassembly */

#define	TCPOPT_EOL		0
#define	TCPOPT_NOP		1
#define	TCPOPT_MAXSEG		2
#define	   TCPOLEN_MAXSEG		4
#define	TCPOPT_WINDOW		3
#define	   TCPOLEN_WINDOW		3
#define	TCPOPT_SACK_PERMITTED	4		/* Experimental */
#define	   TCPOLEN_SACK_PERMITTED	2
#define	TCPOPT_SACK		5		/* Experimental */
#define	TCPOLEN_SACK		8		/* 2*sizeof(tcp_seq) */
#define	TCPOPT_TIMESTAMP	8
#define	   TCPOLEN_TIMESTAMP		10
#define	   TCPOLEN_TSTAMP_APPA		(TCPOLEN_TIMESTAMP+2) /* appendix A */
#define	TCPOPT_SIGNATURE	19
#define	   TCPOLEN_SIGNATURE		18

#define	MAX_TCPOPTLEN		40	/* Absolute maximum TCP options len */

#define TCPOPT_TSTAMP_HDR	\
    (TCPOPT_NOP<<24|TCPOPT_NOP<<16|TCPOPT_TIMESTAMP<<8|TCPOLEN_TIMESTAMP)

/* Option definitions */
#define TCPOPT_SACK_PERMIT_HDR \
(TCPOPT_NOP<<24|TCPOPT_NOP<<16|TCPOPT_SACK_PERMITTED<<8|TCPOLEN_SACK_PERMITTED)
#define TCPOPT_SACK_HDR   (TCPOPT_NOP<<24|TCPOPT_NOP<<16|TCPOPT_SACK<<8)
/* Miscellaneous constants */
#define MAX_SACK_BLKS	6	/* Max # SACK blocks stored at sender side */
#define TCP_MAX_SACK	3	/* MAX # SACKs sent in any segment */

#define TCP_MAXBURST	4	/* Max # packets after leaving Fast Rxmit */

/*
 * Default maximum segment size for TCP.
 * With an IP MSS of 576, this is 536,
 * but 512 is probably more convenient.
 * This should be defined as min(512, IP_MSS - sizeof (struct tcpiphdr)).
 */
#define	TCP_MSS		512

#define	TCP_MAXWIN	65535	/* largest value for (unscaled) window */

#define	TCP_MAX_WINSHIFT	14	/* maximum window shift */

/*
 * User-settable options (used with setsockopt).
 */
#define	TCP_NODELAY		0x01   /* don't delay send to coalesce pkts */
#define	TCP_MAXSEG		0x02   /* set maximum segment size */
#define	TCP_SIGNATURE_ENABLE	0x04   /* enable TCP MD5 signature option */
#define	TCP_SACK_DISABLE	0x300  /* disable SACKs (if enabled by def.) */

#endif /* !_NETINET_TCP_H_ */
