/*	$OpenBSD: cvax.h,v 1.2 2011/09/19 20:58:32 miod Exp $	*/
/*	$NetBSD: ka650.h,v 1.6 1997/07/26 10:12:43 ragge Exp $	*/
/*
 * Copyright (c) 1988 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Mt. Xinu.
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
 *	@(#)ka650.h	7.5 (Berkeley) 6/28/90
 */

/*
 *
 * Definitions specific to CVAX processors.
 */

/*
 * CADR: Cache Disable Register (IPR 37)
 */
#define CADR_STMASK	0x000000f0	/* 1st level cache state mask */
#define CADR_SEN2	0x00000080	/* 1st level cache set 2 enabled */
#define CADR_SEN1	0x00000040	/* 1st level cache set 1 enabled */
#define CADR_CENI	0x00000020	/* 1st level I-stream caching enabled */
#define CADR_CEND	0x00000010	/* 1st level D-stream caching enabled */

/*
 * Internal State Info 2: (for mcheck recovery)
 */
#define IS2_VCR		0x00008000	/* VAX Can't Restart flag */

#ifndef _LOCORE

/*
 * System Support Chip (SSC) registers
 */
struct cvax_ssc {
	u_long	ssc_sscbr;	/* SSC Base Addr Register */
	u_long	pad1[3];
	u_long	ssc_ssccr;	/* SSC Configuration Register */
	u_long	pad2[3];
	u_long	ssc_cbtcr;	/* CDAL Bus Timeout Control Register */
	u_long	pad3[18];
	u_long	ssc_todr;	/* TOY Clock Register */
	u_long	pad4[36];
	u_long	ssc_tcr0;	/* timer control reg 0 */
	u_long	ssc_tir0;	/* timer interval reg 0 */
	u_long	ssc_tnir0;	/* timer next interval reg 0 */
	u_long	ssc_tivr0;	/* timer interrupt vector reg 0 */
	u_long	ssc_tcr1;	/* timer control reg 1 */
	u_long	ssc_tir1;	/* timer interval reg 1 */
	u_long	ssc_tnir1;	/* timer next interval reg 1 */
	u_long	ssc_tivr1;	/* timer interrupt vector reg 1 */
	u_long	pad5[184];
	u_char	ssc_cpmbx;	/* Console Program Mail Box: Lang & Hact */
	u_char	ssc_terminfo;	/* TTY info: Video Dev, MCS, CRT & ROM flags */
	u_char	ssc_keyboard;	/* Keyboard code */
};
#define CVAX_SSC	0x20140000

extern struct cvax_ssc *cvax_ssc_ptr;

/*
 * CBTCR: CDAL Bus Timeout Control Register (ssc_cbtcr)
 */
#define CBTCR_BTO	0x80000000	/* r/w unimp IPR or unack intr */
#define CBTCR_RWT	0x40000000	/* CDAL Bus Timeout on CPU or DMA */

/*
 * TCR0/TCR1: Programable Timer Control Registers (ssc_tcr[01])
 * (The rest of the bits are the same as in the standard VAX
 *	Interval timer and are defined in clock.h)
 */
#define TCR_STP		0x00000004	/* Stop after time-out */

/*
 * Flags for Console Program Mail Box
 */
#define CPMB_CVAX_HALTACT	0x03	/* Field for halt action */
#define CPMB_CVAX_RESTART	0x01	/* Restart */
#define CPMB_CVAX_REBOOT	0x02	/* Reboot */
#define CPMB_CVAX_HALT		0x03	/* Halt */
#define CPMB_CVAX_BIP		0x04	/* Bootstrap in progress */
#define CPMB_CVAX_RIP		0x08	/* Restart in progress */
#define	CPMB_CVAX_DOTHIS	0x30	/* Execute sommand */
#define CPMB_CVAX_LANG		0xf0	/* Language field */

/*
 * Machine Check frame
 */
struct cvax_mchk_frame {
	int	cvax_bcnt;		/* byte count == 0x10 */
	int	cvax_summary;		/* summary parameter */
	int	cvax_mrvaddr;		/* most recent vad */
	int	cvax_istate1;		/* internal state */
	int	cvax_istate2;		/* internal state */
	int	cvax_pc;		/* trapped pc */
	int	cvax_psl;		/* trapped psl */
};

const char *cvax_mchk_descr(int);
void	cvax_halt(void);
void	cvax_reboot(int);

#endif /* _LOCORE */
