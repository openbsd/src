/*	$OpenBSD: ubsecreg.h,v 1.2 2000/06/03 13:14:39 jason Exp $	*/

/*
 * Copyright (c) 2000 Theo de Raadt
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
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

/*
 * Register definitions for 5601 BlueSteel Networks Ubiquitous Broadband
 * Security "uBSec" chip.  Definitions from revision 2.8 of the product
 * datasheet.
 */

#define BS_BAR		0x10	/* DMA and status base address register */

/*
 * DMA Control & Status Registers (offset from BS_BAR)
 */
#define	BS_MCR1		0x00	/* DMA Master Command Record 1 */
#define	BS_CTRL		0x04	/* DMA Control */
#define	BS_STAT		0x08	/* DMA Status */
#define	BS_ERR		0x0c	/* DMA Error Address */
#define	BS_MCR2		0x10	/* DMA Master Command Record 2 */

/* BS_CTRL - DMA Control */
#define	BS_CTRL_MCR2INT		0x40000000	/* enable intr MCR for MCR2 */
#define	BS_CTRL_MCR1INT		0x20000000	/* enable intr MCR for MCR1 */
#define	BS_CTRL_OFM		0x10000000	/* Output fragment mode */
#define	BS_CTRL_BE32		0x08000000	/* big-endian, 32bit bytes */
#define	BS_CTRL_BE64		0x04000000	/* big-endian, 64bit bytes */
#define	BS_CTRL_DMAERR		0x02000000	/* enable intr DMA error */
#define	BS_CTRL_RNG_M		0x01800000	/* RND mode */
#define	BS_CTRL_RNG_1		0x00000000	/* 1bit rn/one slow clock */
#define	BS_CTRL_RNG_4		0x00800000	/* 1bit rn/four slow clocks */
#define	BS_CTRL_RNG_8		0x01000000	/* 1bit rn/eight slow clocks */
#define	BS_CTRL_RNG_16		0x01800000	/* 1bit rn/16 slow clocks */
#define	BS_CTRL_FRAG_M		0x0000ffff	/* output fragment size mask */

/* BS_STAT - DMA Status */
#define	BS_STAT_MCR1_BUSY	0x80000000	/* MCR1 is busy */
#define	BS_STAT_MCR1_FULL	0x40000000	/* MCR1 is full */
#define	BS_STAT_MCR1_DONE	0x20000000	/* MCR1 is done */
#define	BS_STAT_DMAERR		0x10000000	/* DMA error */
#define	BS_STAT_MCR2_FULL	0x08000000	/* MCR2 is full */
#define	BS_STAT_MCR2_DONE	0x04000000	/* MCR2 is done */

/* BS_ERR - DMA Error Address */
#define	BS_ERR_READ		0x00000001	/* fault was on read */

#define	UBSEC_CARD(sid)		(((sid) & 0xf0000000) >> 28)
#define	UBSEC_SID(crd,ses)	(((crd) << 28) | ((ses) & 0x7ff))
#define	MAX_SCATTER		10

struct ubsec_pktctx {
	u_int8_t	pc_deskey[24];		/* 3DES key */
	u_int8_t	pc_hminner[20];		/* hmac inner state */
	u_int8_t	pc_hmouter[20];		/* hmac outer state */
	u_int8_t	pc_iv[8];		/* 3DES iv */
	u_int32_t	pc_flags;
};
#define	UBS_PKTCTX_COFFSET	0xffff0000	/* cryto to mac offset */
#define	UBS_PKTCTX_ENC_3DES	0x00008000	/* use 3des */
#define	UBS_PKTCTX_ENC_NONE	0x00000000	/* no encryption */
#define	UBS_PKTCTX_INBOUND	0x00004000	/* inbound packet */
#define	UBS_PKTCTX_AUTH		0x00003000	/* authentication mask */
#define	UBS_PKTCTX_AUTH_NONE	0x00000000	/* no authentication */
#define	UBS_PKTCTX_AUTH_MD5	0x00001000	/* use hmac-md5 */
#define	UBS_PKTCTX_AUTH_SHA1	0x00002000	/* use hmac-sha1 */

struct ubsec_pktbuf {
	u_int32_t	pb_addr;		/* address of buffer start */
	u_int32_t	pb_next;		/* pointer to next pktbuf */
	u_int32_t	pb_len;
};
#define	UBS_PKTBUF_LEN		0x0000ffff	/* length mask */

struct ubsec_mcr {
	u_int32_t		mcr_flags;	/* flags/packet count */

	u_int32_t		mcr_cmdctxp;	/* command ctx pointer */
	struct ubsec_pktbuf	mcr_ipktbuf;	/* input chain header */
	struct ubsec_pktbuf	mcr_opktbuf;	/* output chain header */
};
#define	UBS_MCR_PACKETS		0x0000ffff	/* packets in this mcr */
#define	UBS_MCR_DONE		0x00010000	/* mcr has been processed */
#define	UBS_MCR_ERROR		0x00020000	/* error in processing */
#define	UBS_MCR_ERRORCODE	0xff000000	/* error type */

struct ubsec_q {
	SIMPLEQ_ENTRY(ubsec_q)		q_next;
	struct ubsec_softc		*q_sc;
	struct cryptop			*q_crp;
	struct ubsec_mcr		q_mcr;
	struct ubsec_pktctx		q_ctx;

	struct mbuf *		      	q_src_m;
	long				q_src_packp[MAX_SCATTER];
	int				q_src_packl[MAX_SCATTER];
	int				q_src_npa, q_src_l;
	struct ubsec_pktbuf		q_srcpkt[MAX_SCATTER-1];

	struct mbuf *			q_dst_m;
	long				q_dst_packp[MAX_SCATTER];
	int				q_dst_packl[MAX_SCATTER];
	int				q_dst_npa, q_dst_l;
	struct ubsec_pktbuf		q_dstpkt[MAX_SCATTER-1];
};
