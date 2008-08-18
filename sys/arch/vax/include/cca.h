/*	$OpenBSD: cca.h,v 1.1 2008/08/18 23:25:53 miod Exp $	*/
/*	$NetBSD: cca.h,v 1.2 2008/03/11 05:34:02 matt Exp $	*/

/*
 * Copyright (c) 2008 Miodrag Vallat.
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
/*
 * Copyright (c) 2000 Ludd, University of Lule}, Sweden. All rights reserved.
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
 *      This product includes software developed at Ludd, University of
 *      Lule}, Sweden and its contributors.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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
 * Console Communication Area (CCA) layout description.
 *
 * Based on NetBSD/vax <machine/cca.h>, further reconstructed from the
 * description in VAX 6000 Model 600 System Technical User's Guide
 * (660eatm1.pdf)
 */

struct cca {
	struct cca *cca_base;	/* Physical base address of block */
	uint16_t cca_size;	/* Size of this struct */
	uint16_t cca_id;	/* 'CC' */
	uint8_t  cca_nproc;	/* max number of CPUs */
	uint8_t  cca_cksum;	/* Checksum of all earlier bytes */
	uint8_t  cca_flags;
#define	CCAF_BOOTIP		0x01	/* bootstrap in progress */
#define	CCAF_REBOOT		0x10	/* reboot requested */
#define	CCAF_REPROMPT		0x20	/* trigger the prom prompt */
#define	CCAF_DISABLE_SECONDARY	0x40	/* ignore messages from secondaries */
	uint8_t  cca_revision;

	uint64_t cca_ready;	/* Data ready? */
	uint64_t cca_console;	/* Processors in console mode */
	uint64_t cca_enabled;	/* enabled/disabled */
	uint32_t cca_bitmapsz;	/* Size of memory bitmap */
	uint32_t cca_bitmap;	/* Address of memory bitmap */
	uint32_t cca_bmcksum;	/* Bitmap checksum */
	uint8_t	 cca_tknode;	/* TK nodes of boot device */
#define	CCA_TKN_VAXBI_MASK	0x0f
#define	CCA_TKN_VAXBI_SHIFT	0
#define	CCA_TKN_XMI_MASK	0xf0
#define	CCA_TKN_XMI_SHIFT	4
	uint8_t	 cca_pad0[3];
	uint64_t cca_starting;	/* Processors currently starting */
	uint64_t cca_restart;	/* Processors currently restarting */
	uint8_t	 cca_primary;	/* XMI node of boot processor */
	uint8_t	 cca_power;
	uint16_t cca_sernum_ext;
	uint32_t cca_pad1[2];
	uint64_t cca_halted;	/* Processors currently halted by user */
	uint8_t  cca_sernum[8];	/* Serial number */
/* CPU revisions */
	struct {
		uint8_t	 cr_compat;	/* cpu compatibility group */
		uint8_t	 cr_pad0[3];
		uint32_t cr_modulerev;	/* ascii module revision */
	} cca_revs[16];
	uint64_t cca_vecenab;	/* Processors with enabled vector processors */
	uint64_t cca_vecwork;	/* Processors with working vector processors */
	uint32_t cca_vecrevs[16];/* Vector processor revisions */
	uint32_t cca_console_xpgr[16];
	uint32_t cca_entry_xpgr[16];
	uint8_t  cca_pad2[80];	/* pad to VAX_NBPG */
/* Inter-CPU communication structs */
	struct {
		uint8_t	 cc_flags;	/* Status flags */
#define CCACCF_RXRDY	0x01	/* has message in rxbuf */
#define	CCACCF_DEST	0x02	/* currently sending to zdst */
#define	CCACCF_SRC	0x04	/* currently receiving from zsrc */
#define	CCACCF_ALT	0x08	/* unable to communicate through cca */
		uint8_t	 cc_zdst;	/* Node sending to */
		uint8_t	 cc_zsrc;	/* Node sending from */
		uint8_t	 cc_znid;	/* Originating node if not zsrc */
		uint8_t	 cc_txlen;	/* Length of transmit message */
		uint8_t	 cc_rxlen;	/* Length of receive message */
		uint16_t cc_rxcd;
#define	CCACC_RX_DATA_MASK	0x00ff	/* character received */
#define	CCACC_RX_DATA_SHIFT	0
#define	CCACC_RX_NODE_MASK	0x0f00	/* originating node */
#define	CCACC_RX_NODE_SHIFT	8
#define	CCACC_RX_DATA_READY	0x8000	/* rxcd assembled */
		char	 cc_txbuf[80];	/* Transmit buffer */
		char	 cc_rxbuf[80];	/* Receive buffer */
	} cca_cc[16];
};

#ifdef _KERNEL
extern	struct cca *cca;
#endif
