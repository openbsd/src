/*	$OpenBSD: nofnreg.h,v 1.1 2002/01/07 23:16:38 jason Exp $	*/

/*
 * Copyright (c) 2002 Jason L. Wright (jason@thought.net)
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
 *	This product includes software developed by Jason L. Wright
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#define	NOFN_BAR0	0x10		/* Group 0 space */
#define	NOFN_BAR1	0x14		/* Group 1 space */
#define	NOFN_BAR2	0x18		/* GPRAM */

#define	NOFN_G0_SEISR	0x08		/* security engine intr status */
#define	NOFN_G0_SEIER	0x10		/* security engine intr enable */
#define	NOFN_G0_SESR	0x14		/* security engine status */

#define	NOFN_G1_RNGER	0x60		/* RNG enable */
#define	NOFN_G1_RNGCR	0x64		/* RNG config */
#define	NOFN_G1_RNGDAT	0x68		/* RNG data */
#define	NOFN_G1_RNGSTS	0x6c		/* RNG status */

/* g0, seisr: security engine intr status */
#define	SEISR_INVAL	0x00008000	/* invalid command */
#define	SEISR_DATERR	0x00004000	/* data error */
#define	SEISR_SRCFIFO	0x00002000	/* source fifo ready */
#define	SEISR_DSTFIFO	0x00001000	/* destination fifo ready */
#define	SEISR_DSTOVF	0x00000200	/* destination overrun */
#define	SEISR_SRCCMD	0x00000080	/* source command phase */
#define	SEISR_SRCCTX	0x00000040	/* source context phase */
#define	SEISR_SRCDAT	0x00000020	/* source data phase */
#define	SEISR_DSTDAT	0x00000010	/* destination data phase */
#define	SEISR_DSTRES	0x00000004	/* destination result phase */

/* g0, seier: security engine intr enable */
#define	SEIER_INVAL	0x00008000	/* invalid command */
#define	SEIER_DATERR	0x00004000	/* data error */
#define	SEIER_SRCFIFO	0x00002000	/* source fifo ready */
#define	SEIER_DSTFIFO	0x00001000	/* destination fifo ready */
#define	SEIER_DSTOVF	0x00000200	/* destination overrun */
#define	SEIER_SRCCMD	0x00000080	/* source command phase */
#define	SEIER_SRCCTX	0x00000040	/* source context phase */
#define	SEIER_SRCDAT	0x00000020	/* source data phase */
#define	SEIER_DSTDAT	0x00000010	/* destination data phase */
#define	SEIER_DSTRES	0x00000004	/* destination result phase */

/* g0, sesr: security engine status */
#define	SESR_INVAL	0x00008000	/* invalid command */
#define	SESR_DATERR	0x00004000	/* data error */
#define	SESR_SRCFIFO	0x00002000	/* source fifo ready */
#define	SESR_DSTFIFO	0x00001000	/* destination fifo ready */
#define	SESR_DSTOVF	0x00000200	/* destination overrun */
#define	SESR_SRCCMD	0x00000080	/* source command phase */
#define	SESR_SRCCTX	0x00000040	/* source context phase */
#define	SESR_SRCDAT	0x00000020	/* source data phase */
#define	SESR_DSTDAT	0x00000010	/* destination data phase */
#define	SESR_DSTRES	0x00000004	/* destination result phase */

/* g1, rnger: RNG enable */
#define	RNGER_ENABLE	0x00000001	/* enable rng */

/* g1, rngcr: RNG config */
#define	RNGCR_PRE1	0x00000f00	/* prescaler 1 */
#define	RNGCR_PRE2	0x00000080	/* prescaler 2, 0 = 1024, 1 = 512 */
#define	RNGCR_DEFAULT	0x00000900	/* default value for us */

/* g1, rngsts: RNG status */
#define	RNGSTS_RDY	0x00004000	/* RNG ready (2 words in FIFO) */
#define	RNGSTS_UFL	0x00001000	/* RNG underflow */
