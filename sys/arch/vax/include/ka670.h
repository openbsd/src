/*	$OpenBSD: ka670.h,v 1.1 2000/04/26 06:08:27 bjc Exp $	*/
/*	$NetBSD: ka670.h,v 1.1 1999/06/06 14:23:46 ragge Exp $	*/
/*
 * Copyright (c) 1999 Ludd, University of Lule}, Sweden.
 * All rights reserved.
 *
 * This code is derived from software contributed to Ludd by Bertram Barth.
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
 * Definitions for I/O addresses of
 *
 *	VAX 4000/300 (KA670)
 */

#define KA670_SIDEX	0x20040004	/* SID extension register */
#define KA670_IORESET	0x20020000	/* I/O Reset register */

#define KA670_ROM_BASE	0x20040000	/* System module ROM */
#define KA670_ROM_END	0x2007FFFF
#define KA670_ROM_SIZE	   0x40000	

/*
 * The following values refer to bits/bitfields within the 4 internal 
 * registers controlling primary cache: 
 * PR_PCTAG(124, tag-register)		PR_PCIDX(125, index-register)
 * PR_PCERR(126, error-register)	PR_PCSTS(127, status-register)
 */
#define KA670_PCTAG_TAG		0x1FFFF800	/* bits 11-29 */
#define KA670_PCTAG_PARITY	0x40000000
#define KA670_PCTAG_VALID	0x80000000

#define KA670_PCIDX_INDEX	0x000007F8	/* 0x100 Q-word entries */

#define KA670_PCERR_ADDR		0x3FFFFFFF

#define KA670_PCS_FORCEHIT	0x00000001	/* Force hit */
#define KA670_PCS_ENABLE		0x00000002	/* Enable primary cache */
#define KA670_PCS_FLUSH		0x00000004	/* Flush cache */
#define KA670_PCS_REFRESH	0x00000008	/* Enable refresh */
#define KA670_PCS_HIT		0x00000010	/* Cache hit */
#define KA670_PCS_INTERRUPT	0x00000020	/* Interrupt pending */
#define KA670_PCS_TRAP2		0x00000040	/* Trap while trap */
#define KA670_PCS_TRAP1		0x00000080	/* Micro trap/machine check */
#define KA670_PCS_TPERR		0x00000100	/* Tag parity error */
#define KA670_PCS_DPERR		0x00000200	/* Dal data parity error */
#define KA670_PCS_PPERR		0x00000400	/* P data parity error */
#define KA670_PCS_BUSERR		0x00000800	/* Bus error */
#define KA670_PCS_BCHIT		0x00001000	/* B cache hit */

#define KA670_PCSTS_BITS \
	"\020\015BCHIT\014BUSERR\013PPERR\012DPERR\011TPERR\010TRAP1" \
	"\007TRAP2\006INTR\005HIT\004REFRESH\003FLUSH\002ENABLE\001FORCEHIT"

#define KA670_BCSTS_BITS \
	"\020\015BCHIT\014BUSERR\013PPERR\012DPERR\011TPERR\010TRAP1" \
	"\007TRAP2\006INTR\005HIT\004REFRESH\003FLUSH\002ENABLE\001FORCEHIT"

/*
 * Bits in PR_ACCS (Floating Point Accelerator Register)
 */
#define KA670_ACCS_VECTOR	(1<<0)	/* Vector Unit Present */
#define KA670_ACCS_FCHIP		(1<<1)	/* FPU chip present */
#define KA670_ACCS_WEP		(1<<31)	/* Write Even Parity */
