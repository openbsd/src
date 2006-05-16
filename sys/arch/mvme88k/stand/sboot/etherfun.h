/*	$OpenBSD: etherfun.h,v 1.3 2006/05/16 22:52:26 miod Exp $	*/

/*
 *
 * Copyright (c) 1995 Charles D. Cranor and Seth Widoff
 * All rights reserved.
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
 *      This product includes software developed by Charles D. Cranor
 *	and Seth Widoff.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/* etherfun.h */

/* constants */
/* ether header */
#define ETYPE_RARP 0x8035  /* ethertype is RARP */
#define ETYPE_IP   0x800   /* ethertype is IP   */

/* rev arp */
#define PTYPE_IP 0x800     /* Protocol type is IP      */
#define OPCODE_RARP 3      /* Optype is REVARP request */
#define OPCODE_REPLY 4     /* Optype is REVARP reply   */

/* ip header */
#define  IPP_UDP 17	/* IP Protocol is UDP       */
#define  IP_VERSION 4      /* IP version number	*/
#define  IP_HLEN 5	 /* IP header length is a fixed 50 bytes */
#define N 1536

/* tftp header */
#define FTPOP_ACKN 4      /* Opcode is acknowledge     */
#define FTPOP_ERR 5       /* Opcode is Error	       */
#define FTP_PORT 69       /* Standard TFTP port number */
#define MSG "\0\1xxxxxxxx.mvme68k\0octet\0" /* implicit NULL */

/* data structures */

struct  ether_header {
	u_char  ether_dhost[6];
	u_char  ether_shost[6];
	u_short ether_type;
};

struct  ether_arp {
	u_short ar_hrd;		/* format of hardware address */
	u_short ar_pro;		/* format of protocol address */
	u_char  ar_hln;		/* length of hardware address */
	u_char  ar_pln;		/* length of protocol address */
	u_short ar_op;
	u_char  arp_sha[6];	/* sender hardware address */
	u_char  arp_spa[4];	/* sender protocol address */
	u_char  arp_tha[6];	/* target hardware address */
	u_char  arp_tpa[4];	/* target protocol address */
};

struct ip {
	u_char  ip_v:4,		/* version */
		ip_hl:4;	/* header length */
	u_char  ip_tos;		/* type of service */
	short   ip_len;		/* total length */
	u_short ip_id;		/* identification */
	short   ip_off;		/* fragment offset field */
#define IP_DF 0x4000		/* dont fragment flag */
#define IP_MF 0x2000		/* more fragments flag */
#define IP_OFFMASK 0x1fff	/* mask for fragmenting bits */
	u_char  ip_ttl;		/* time to live */
	u_char  ip_p;		/* protocol */
	u_short ip_sum;		/* checksum */
	u_char  ip_src[4];
	u_char  ip_dst[4];	/* source and dest address */
};

struct udp {
	u_short uh_sport;
	u_short uh_dport;
	short uh_ulen;
	u_short uh_sum;
};

struct tftph {
	u_short	op_code;
	u_short	block;
};

struct tftphr {
	struct tftph info;
	char	data[1];
};

/* globals */
int last_ack;
char buf[N];
struct ether_header *eh = (struct ether_header *)buf;
struct ether_arp *rarp = (struct ether_arp *)
	(buf + sizeof(struct ether_header));
struct ip *iph = (struct ip *)(buf + sizeof(struct ether_header));
struct udp *udph = (struct udp *)
	(buf + sizeof(struct ether_header) + sizeof(struct ip));
char *tftp_r = buf + sizeof(struct ether_header) + sizeof(struct ip) +
	sizeof(struct udp);
struct tftph *tftp_a = (struct tftph *)(buf + sizeof(struct ether_header) +
	sizeof(struct ip) + sizeof(struct udp));
struct tftphr *tftp = (struct tftphr *)(buf + sizeof(struct ether_header) +
	sizeof(struct ip) + sizeof(struct udp));
