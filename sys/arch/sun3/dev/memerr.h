/*	$NetBSD: memerr.h,v 1.1 1996/03/26 14:57:44 gwr Exp $ */

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory.
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
 *	@(#)memreg.h	8.1 (Berkeley) 6/11/93
 */

/*
 * Sun3 memory error register.
 *
 * All Sun3 memory systems use either parity checking or
 * Error Correction Coding (ECC).  A memory error causes
 * the Memory Error Register (MER) to latch information
 * about the location and type of error, and if the MER
 * interrupt is enabled, generateds a level 7 interrupt.
 * The latched information persists (even if more errors
 * occur) until the MER is cleared by a write (at mer_er).
 */


struct memerr {
	volatile u_char	me_csr;		/* MER control/status reg. */
	volatile u_char	me__pad[3];
	volatile u_int	me_vaddr;
};

/*
 * Bits in me_csr common between ECC/parity memory systems:
 */
#define	ME_CSR_IPEND	0x80	/* (ro) error interrupt pending */
#define	ME_CSR_IENA 	0x40	/* (rw) error interrupt enable */

/*
 *  Bits in me_csr on parity-checked memory system:
 */
#define ME_PAR_TEST 	0x20	/* (rw) write inverse parity */
#define ME_PAR_CHECK	0x10	/* (rw) enable parity checking */
#define ME_PAR_ERR3 	0x08	/* (ro) parity error in <24..31> */
#define ME_PAR_ERR2 	0x04	/* (ro) parity error in <16..23> */
#define ME_PAR_ERR1 	0x02	/* (ro) parity error in <8..15> */
#define ME_PAR_ERR0 	0x01	/* (ro) parity error in <0..7> */
#define	ME_PAR_EMASK	0x0F	/* (ro) mask of above four */
#define ME_PAR_STR	"\20\10IPEND\7IENA\6TEST\5CHK\4ERR3\3ERR2\2ERR1\1ERR0"

/*
 *  Bits in me_csr on an ECC memory system:
 */
#define ME_ECC_BUSLK	0x20	/* (rw) hold memory bus mastership */
#define ME_ECC_CE_ENA	0x10	/* (rw) enable CE recording */
#define	ME_ECC_WBTMO	0x08	/* (ro) write-back timeout */
#define	ME_ECC_WBERR	0x04	/* (ro) write-back error */
#define ME_ECC_UE		0x02	/* (ro) UE, uncorrectable error  */
#define ME_ECC_CE		0x01	/* (ro) CE, correctable (single bit) error */
#define	ME_ECC_EMASK	0x0F	/* (ro) mask for some ECC error occuring */
#define ME_ECC_STR	"\20\10IPEND\7IENA\6BUSLK\5CE_ENA\4TMOUT\3WBERR\2UE\1CE"

