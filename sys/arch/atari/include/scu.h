/*	$NetBSD: scu.h,v 1.1.1.1 1995/03/26 07:12:08 leo Exp $	*/

/*
 * Copyright (c) 1995 Leo Weppelman.
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
 *      This product includes software developed by Leo Weppelman.
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

#ifndef _MACHINE_SCU_H
#define _MACHINE_SCU_H
/*
 * Atari TT hardware:
 * SCU registers
 */

#define	SCU	((struct scu *)AD_SCU)


/*
 * System control unit
 */
struct scu {
	volatile u_char	fil1;
	volatile u_char	sys_mask;	/* System interrupt mask	*/
	volatile u_char	fil2;
	volatile u_char	sys_stat;	/* System interrupt status	*/
	volatile u_char	fil3;
	volatile u_char	sys_int;	/* System interrupter		*/
	volatile u_char	fil4;
	volatile u_char	vme_int;	/* VME interrupter		*/
	volatile u_char	fil5;
	volatile u_char	gen_reg1;	/* General purpose reg. 1	*/
	volatile u_char	fil6;
	volatile u_char	gen_reg2;	/* General purpose reg. 2	*/
	volatile u_char	fil7;
	volatile u_char	vme_mask;	/* VME interrupt mask		*/
	volatile u_char	fil8;
	volatile u_char	vme_stat;	/* VME interrupt status		*/
};

/*
 * Bits for system mask & stat.
 * Read 'sys_stat' first, reading 'sys_mask' clears pending bits in 'sys_stat'.
 */
#define	SCU_SYSFAIL	0x80	/* _Sysfail in VME bus (Auto vectored)	*/
#define	SCU_MFP		0x40	/* MFP interrupt (Programmable)		*/
#define	SCU_SCC		0x20	/* SCC interrupt (Programmable)		*/
#define	SCU_VSYNC	0x10	/* Vertical Sync (Auto vectored)	*/
		     /* 0x08 Not Used */
#define	SCU_HSYNC	0x04	/* Horizontal Sync (Auto vectored)	*/
#define	SCU_SYS_SOFT	0x02	/* System Software INT (Auto vectored)	*/
		     /* 0x00 Not Used */

/*
 * Bits for VME mask & stat.
 * Read 'vme_stat' first, reading 'vme_mask' clears pending bits in 'vme_stat'.
 * Not that MFP/SCC interrupts are hard-wired to the mentioned VME IRQ's.
 * (or'ed).
 */
#define	SCU_IRQ7	0x80
#define	SCU_IQ6_MFP	0x40	/* Also MFP interrupt			*/
#define	SCU_IRQ5_SCC	0x20	/* Also SCC interrupt			*/
#define	SCU_IRQ4	0x10
#define	SCU_IRQ3_SOFT	0x08	/* Also VME Software interrupt		*/
#define	SCU_IRQ2	0x04
#define	SCU_IRQ1	0x02
		     /* 0x00 Not Used */

/*
 * Generate/remove Software system interrupt.
 * Note: Will not be cleared automatically!!
 */
#define	SET_SOFT_INT	(SCU->sys_int = 1)
#define	CLR_SOFT_INT	(SCU->sys_int = 0)

/*
 * Generate/remove Software VME interrupt.
 * Note: Will not be cleared automatically!!
 */
#define	SET_VME_INT	(SCU->vme_int = 1)
#define	CLR_VME_INT	(SCU->vme_int = 0)
#endif /* _MACHINE_SCU_H */
