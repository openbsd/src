/*	$OpenBSD: cgfourteenreg.h,v 1.4 2005/03/15 18:50:43 miod Exp $	*/
/*	$NetBSD: cgfourteenreg.h,v 1.1 1996/09/30 22:41:02 abrown Exp $ */

/*
 * Copyright (c) 1996
 *	The President and Fellows of Harvard College. All rights reserved.
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
 *	This product includes software developed by Harvard University and
 *	its contributors.
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
 */

/*
 * Register/dac/clut/cursor definitions for cgfourteen frame buffer
 */

/* Control registers */
#define	CG14_REG_CONTROL	0
#define	CG14_REG_VRAM		1

#define	CG14_NREG		2

/* Locations of control registers in cg14 register set */
#define	CG14_OFFSET_CURS	0x1000
#define CG14_OFFSET_DAC		0x2000
#define CG14_OFFSET_XLUT	0x3000
#define CG14_OFFSET_CLUT1	0x4000
#define CG14_OFFSET_CLUT2	0x5000
#define CG14_OFFSET_CLUT3	0x6000
#define CG14_OFFSET_AUTOINCR	0xf000

/* Main control register set */
struct cg14ctl {
	volatile u_int8_t	ctl_mctl;	/* main control register */
#define CG14_MCTL_ENABLEINTR	0x80		/* interrupts */
#define CG14_MCTL_R0_ENABLEHW	0x40		/* hardware enable */
#define	CG14_MCTL_R1_ENABLEHW	0x01
#define	CG14_MCTL_R1_ENABLEVID	0x40		/* display enable */
#define CG14_MCTL_PIXMODE_MASK	0x30
#define		CG14_MCTL_PIXMODE_8	0x00	/* data is 16 8-bit pixels */
#define		CG14_MCTL_PIXMODE_16	0x20	/* data is 8 16-bit pixels */
#define		CG14_MCTL_PIXMODE_32	0x30	/* data is 4 32-bit pixels */
#define CG14_MCTL_PIXMODE_SHIFT	4
#define	CG14_MCTL_TMR		0x0c
#define CG14_MCTL_ENABLETMR	0x02
#define CG14_MCTL_R0_RESET	0x01
	volatile u_int8_t	ctl_ppr;	/* packed pixel register */
	volatile u_int8_t	ctl_tmsr0; 	/* test status reg. 0 */
	volatile u_int8_t	ctl_tmsr1;	/* test status reg. 1 */
	volatile u_int8_t	ctl_msr;	/* master status register */
#define	CG14_MSR_PENDING	0x20		/* interrupt pending */
#define	CG14_MSR_VRETRACE	0x10		/* vertical retrace interrupt */
#define	CG14_MSR_FAULT		0x01		/* fault interrupt */
	volatile u_int8_t	ctl_fsr;	/* fault status register */
	volatile u_int8_t	ctl_rsr;	/* revision status register */
#define CG14_RSR_REVMASK	0xf0 		/*  mask to get revision */
#define CG14_RSR_REVSHIFT	4
#define CG14_RSR_IMPLMASK	0x0f		/*  mask to get impl. code */
	volatile u_int8_t	ctl_ccr;	/* clock control register */
	/* XXX etc. */
};

/* Hardware cursor map */
#define CG14_CURS_SIZE		32
#define	CG14_CURS_MASK		0x1f
struct cg14curs {
	volatile u_int32_t	curs_plane0[CG14_CURS_SIZE];	/* plane 0 */
	volatile u_int32_t	curs_plane1[CG14_CURS_SIZE];
	volatile u_int8_t	curs_ctl;	/* control register */
#define CG14_CURS_ENABLE	0x4
#define CG14_CURS_DOUBLEBUFFER	0x2 		/* use X-channel for curs */
	volatile u_int8_t	pad0[3];
	volatile u_int16_t	curs_x;		/* x position */
	volatile u_int16_t	curs_y;		/* y position */
	volatile u_int32_t	curs_color1;	/* color register 1 */
	volatile u_int32_t	curs_color2;	/* color register 2 */
	volatile u_int32_t	pad[444];	/* pad to 2KB boundary */
	volatile u_int32_t	curs_plane0incr[CG14_CURS_SIZE]; /* autoincr */
	volatile u_int32_t	curs_plane1incr[CG14_CURS_SIZE]; /* autoincr */
};

/* DAC */
struct cg14dac {
	volatile u_int8_t	dac_addr;	/* address register */
	volatile u_int8_t	pad0[255];
	volatile u_int8_t	dac_gammalut;	/* gamma LUT */
	volatile u_int8_t	pad1[255];
	volatile u_int8_t	dac_regsel;	/* register select */
	volatile u_int8_t	pad2[255];
	volatile u_int8_t	dac_mode;	/* mode register */
};

#define CG14_CLUT_SIZE	256

/* XLUT registers */
struct cg14xlut {
	volatile u_int8_t	xlut_lut[CG14_CLUT_SIZE];	/* the LUT */
	volatile u_int8_t	xlut_lutd[CG14_CLUT_SIZE];	/* ??? */
	volatile u_int8_t	pad0[0x600];
	volatile u_int8_t	xlut_lutinc[CG14_CLUT_SIZE];	/* autoincrLUT*/
	volatile u_int8_t	xlut_lutincd[CG14_CLUT_SIZE];
};

/* Color Look-Up Table (CLUT) */
struct cg14clut {
	volatile u_int32_t	clut_lut[CG14_CLUT_SIZE];	/* the LUT */
	volatile u_int32_t	clut_lutd[CG14_CLUT_SIZE];	/* ??? */
	volatile u_int32_t	clut_lutinc[CG14_CLUT_SIZE];	/* autoincr */
	volatile u_int32_t	clut_lutincd[CG14_CLUT_SIZE];
};
