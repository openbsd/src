/*	$OpenBSD: if_token.h,v 1.7 2007/05/29 18:21:19 claudio Exp $	*/
/*	$NetBSD: if_token.h,v 1.6 1999/11/19 20:41:19 thorpej Exp $	*/

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
 * 3. Neither the name of the University nor the names of its contributors
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
 *	from: NetBSD: if_fddi.h,v 1.2 1995/08/19 04:35:28 cgd Exp
 */

#ifndef _NET_IF_TOKEN_H_
#define _NET_IF_TOKEN_H_

#define ISO88025_ADDR_LEN 6

/* Token Ring physical header */
struct token_header {
	u_int8_t  token_ac;			/* access control field */
	u_int8_t  token_fc;			/* frame control field */
	u_int8_t  token_dhost[ISO88025_ADDR_LEN];	/* dest. address */
	u_int8_t  token_shost[ISO88025_ADDR_LEN];	/* source address */
} __packed;

#define TOKEN_MAX_BRIDGE 8

/* Token Ring routing information field */
struct token_rif {
	u_int16_t tr_rcf;			/* route control field */
	u_int16_t tr_rdf[TOKEN_MAX_BRIDGE];	/* route-designator fields */
} __packed;

/* standard values for address control and frame control field */
#define TOKEN_AC		0x10
#define TOKEN_FC		0x40

#define TOKEN_RI_PRESENT		0x80	/* routing info present bit */
#define TOKEN_RCF_LEN_MASK		0x1f00
#define TOKEN_RCF_BROADCAST_MASK	0xe000
#define	TOKEN_RCF_BROADCAST_ALL		0x8000  /* all routes broadcast */
#define	TOKEN_RCF_BROADCAST_SINGLE	0xc000  /* single route broadcast */
				
/*
 * A Token-ring frame consists of
 * header +      rif      + llcinfo + fcs
 *  14    + 2 * (0 ... 9) +    x    +  4  octets
 * where llcinfo contains the llcsnap header (8 octets) and the IP frame
 */
					/*  LLC INFO (802.5PD-2) */
#define TOKEN_RCF_FRAME0	0x0000  /*    516    */
#define TOKEN_RCF_FRAME1	0x0010  /*   1500    */
#define TOKEN_RCF_FRAME2	0x0020  /*   2052    */
#define TOKEN_RCF_FRAME3	0x0030  /*   4472    */
#define TOKEN_RCF_FRAME4	0x0040  /*   8144    */
#define TOKEN_RCF_FRAME5	0x0050  /*  11407    */
#define TOKEN_RCF_FRAME6	0x0060  /*  17800    */
#define TOKEN_RCF_FRAME7	0x0070	/*  65535    */
#define TOKEN_RCF_FRAME_MASK	0x0070

#define TOKEN_RCF_DIRECTION	0x0080

/*
 * According to RFC 1042
 */
#define IPMTU_4MBIT_MAX		4464
#define IPMTU_16MBIT_MAX	8188

/*
 * RFC 1042:
 * It is recommended that all implementations support IP packets
 * of at least 2002 octets.
 */
#define ISO88025_MTU 2002

/*
 * This assumes that route information fields are appended to
 * existing structures like llinfo_arp and token_header
 */
#define TOKEN_RIF(x) ((struct token_rif *) ((x) + 1))

#endif /* _NET_IF_TOKEN_H_ */
