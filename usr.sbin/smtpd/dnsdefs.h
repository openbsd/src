/*
 * Copyright (c) 2009	Eric Faurot	<eric@faurot.net>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#define PACKET_MAXLEN	512

#define HEADER_LEN	12
#define DOMAIN_MAXLEN	255
#define LABEL_MAXLEN	63

#define OPCODE_SHIFT	11
#define Z_SHIFT		 4

#define QR_MASK		(0x1 << 15)
#define OPCODE_MASK	(0xf << 11)
#define AA_MASK		(0x1 << 10)
#define TC_MASK		(0x1 <<  9)
#define RD_MASK		(0x1 <<  8)
#define RA_MASK		(0x1 <<  7)
#define Z_MASK		(0x7 <<  4)
#define RCODE_MASK	(0xf)

#define OPCODE(v)	((v) & OPCODE_MASK)
#define RCODE(v)	((v) & RCODE_MASK)

#define OP_QUERY	(0)
#define OP_IQUERY	(0x1 << 11)	/* obsolete rfc3425*/
#define OP_STATUS	(0x2 << 11)
#define OP_NOTIFY	(0x4 << 11)	/* rfc1996 */
#define OP_UPDATE	(0x5 << 11)	/* rfc2136 */


#define NOERR	 	 0
#define ERR_FORMAT	 1
#define ERR_SERVER	 2
#define ERR_NAME	 3
#define ERR_NOFUNC	 4
#define ERR_REFUSED	 5

#define ERR_YXDOMAIN	 6	/* rfc2136 */
#define ERR_YXRRSET	 7	/* rfc2136 */
#define ERR_NXRRSET	 8	/* rfc2136 */
#define ERR_NOEAUTH	 9	/* rfc2136 */
#define ERR_NOTZONE	10	/* rfc2136 */

#define ERR_BADVERS	16	/* rfc2671 */
#define ERR_BADSIG	16	/* rfc2845 */
#define ERR_BADKEY	17	/* rfc2845 */
#define ERR_BADTIME	18	/* rfc2845 */
#define ERR_BADMODE	19	/* rfc2930 */
#define ERR_BADNAME	20	/* rfc2930 */
#define ERR_BADALG	21	/* rfc2930 */
#define ERR_BADTRUNC	22	/* rfc4635 */



/* TYPE */

			/* rfc1035 */
#define T_A 	1	/* host address					*/
#define T_NS 	2	/* authoritative name server			*/
#define T_MD 	3	/* mail destination (Obsolete - use MX)		*/
#define T_MF 	4	/* mail forwarder (Obsolete - use MX)		*/
#define T_CNAME 5	/* canonical name for an alias			*/
#define T_SOA 	6	/* marks the start of a zone of authority	*/
#define T_MB 	7	/* mailbox domain name (EXPERIMENTAL)		*/
#define T_MG 	8	/* mail group member (EXPERIMENTAL)		*/
#define T_MR 	9	/* mail rename domain name (EXPERIMENTAL)	*/
#define T_NULL 	10	/* null RR (EXPERIMENTAL)			*/
#define T_WKS 	11	/* well known service description		*/
#define T_PTR 	12	/* domain name pointer				*/
#define T_HINFO	13	/* host information				*/
#define T_MINFO	14	/* mailbox or mail list information		*/
#define T_MX 	15	/* mail exchange				*/
#define T_TXT 	16	/* text strings					*/

			/* rfc1183 */
#define T_RP	17	/* responsible person				*/
#define	T_AFSDB	18	/* AFS Database location			*/
#define T_X25	19	/* X25 PSDN address				*/
#define T_ISDN	20	/* ISDN address					*/
#define T_RT	21	/* route through				*/

#define T_NSAP	22	/* NSAP address		rfc1706			*/
#define T_NSAPPTR  23	/* 			rfc1348			*/
#define T_SIG	24	/* security signature	rfc2931, rfc4034	*/
#define T_KEY	25	/* security key		rfc3445, rfc4034	*/
#define T_PX	26	/* X.400 mail mapping info	rfc2163		*/
#define T_GPOS	27	/* geographical position	rfc1712		*/
#define T_AAAA	28	/* IPv6 address			rfc3596		*/
#define	T_LOC	29	/* location information		rfc1876		*/
#define T_NXT	30	/* next domain (obsolete)	rfc2535		*/
#define T_EID	31	/* endpoint identifier				*/
#define T_NIMLOC 32	/* nimrod locator				*/
#define T_NB	32	/* NetBIOS general name service	rfc1002		*/
#define T_SRV	33	/* server selection	rfc2052, rfc2782,	*/
#define T_NBSTAT 33	/* NetBIOS node status		rfc1002		*/
#define T_ATMA	34	/* atm address					*/
#define T_NAPTR	35	/* naming authority pointer	rfc3403		*/
#define T_KX	36	/* key exchange			rfc2230		*/
#define T_CERT	37	/* 		rfc2538, rfc4398		*/
#define T_A6	38	/*		rfc2874, rfc3226		*/
#define T_DNAME	39	/*		rfc2672				*/
#define T_SINK	40	/*						*/
#define T_OPT	41	/*		rfc2671				*/
#define T_APL	42	/*		rfc3123				*/
#define T_DS	43	/* delegation signer	rfc3658			*/
#define T_SSHFP 44	/* ssh key fingerprint	rfc4255			*/
#define	T_IPSECKEY 45	/*			rfc4025			*/
#define T_RRSIG	46	/*			rfc3755			*/
#define T_NSEC	47	/* NextSECure		rfc3755, rfc3845	*/
#define T_DNSKEY 48	/* 			rfc3755			*/
#define T_DHCID	49	/* DHCP identifier	rfc4701			*/
#define T_NSEC3	50	/*			rfc5155			*/
#define T_NSEC3PARAM 51	/*			rfc5155			*/

#define T_HIP	55	/* Host Identity protocol	rfc5205		*/
#define T_NINFO	56
#define T_RKEY	57

#define T_SPF	99	/* sender policy framework	rfc4408		*/
#define T_UINFO	100
#define T_UID	101
#define T_GID	102
#define T_UNSPEC	103

#define T_TKEY	249	/*	rfc2930					*/
#define T_TSIG	250	/* transaction signature rfc2845, rfc3645	*/
#define T_IXFR	251	/* incremental transfer	rfc1995			*/

			/* request only */
#define T_AXFR	252 /* transfer of an entire zone		rfc1035	*/
#define T_MAILB	253 /* mailbox-related records (MB, MG or MR)	rfc1035	*/
#define T_MAILA	254 /* mail agent RRs (Obsolete - see MX)	rfc1035	*/
#define T_ALL	255 /* all records				rfc1035	*/

#define T_DNSSECTA 32768 /* DNSSEC trust authorities			*/
#define T_DNSSECLV 32769 /* lookaside validation      rfc4431, rfc5074	*/


/* CLASS */
			/* 0 reserved		rfc5395			*/
#define C_IN	1	/* Internet					*/
#define C_CS	2	/* CSNET (obsolete)				*/
#define C_CH	3	/* Chaos					*/
#define C_HS	4	/* Hesiod					*/

#define C_NONE	254	/* 			rfc2136			*/
#define C_ANY	255	/*						*/

#define C_PRIV0	65280	/*			rfc5395			*/
#define C_PRIV1	65534	/*			rfc5395			*/
			/* 65535 reserved	rfc5395			*/
