/*	$OpenBSD: ka630.h,v 1.6 2003/06/02 23:27:57 millert Exp $ */
/*	$NetBSD: ka630.h,v 1.5 2000/07/19 01:01:58 matt Exp $ */
/*-
 * Copyright (c) 1986, 1988 The Regents of the University of California.
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
 *	@(#)uvaxII.h	7.4 (Berkeley) 5/9/91
 */

#ifndef _VAX_INCLUDE_KA630_H_
#define	_VAX_INCLUDE_KA630_H_

#define UVAXIISID	((u_long *)0x20040004)
#define UVAXIICPU	((struct uvaxIIcpu *)0x20080000)

#ifndef _LOCORE
struct uvaxIIcpu {
	u_short uvaxII_bdr;
	u_short uvaxII_xxx;
	u_long  uvaxII_mser;
	u_long  uvaxII_cear;
	u_long  uvaxII_dear;
};
#endif

/* Memory system err reg. */
#define	UVAXIIMSER_CD	0x00000300
#define	UVAXIIMSER_NXM	0x00000080
#define	UVAXIIMSER_LPE	0x00000040
#define	UVAXIIMSER_QPE	0x00000020
#define	UVAXIIMSER_MERR	0x000000f0
#define	UVAXIIMSER_CPUE	0x00000060
#define	UVAXIIMSER_DQPE	0x00000010
#define	UVAXIIMSER_LEB	0x00000008
#define	UVAXIIMSER_WRWP	0x00000002
#define	UVAXIIMSER_PEN	0x00000001

/* Mem. error address regs. */
#define	UVAXIICEAR_PG	0x00007fff
#define	UVAXIIDEAR_PG	0x00007fff

/*
 * Definitions specific to the ka630 MicroVAXII Q22 bus cpu card. Includes the
 * tod clock chip and the cpu registers.
 */
#define KA630CLK	((struct ka630clock *)0x200b8000)

/* Bdr register bits */
#define	KA630BDR_PWROK	0x8000
#define	KA630BDR_HLTENB	0x4000
#define	KA630BDR_CPU	0x0c00
#define	KA630BDR_BDG	0x0300
#define	KA630BDR_DSPL	0x000f

/* Clock registers and constants */
#define	MINSEC	60
#define	HRSEC	3600
#define DAYSEC	(HRSEC * 24)
#define YEARSEC	(DAYSEC * 365)

#define	KA630CLK_VRT	0200
#define	KA630CLK_UIP	0200
#define	KA630CLK_RATE	040
#define	KA630CLK_ENABLE	06
#define	KA630CLK_SET	0206
/* cpmbx bits */
#define	KA630CLK_HLTACT	03
/* halt action values */
#define	KA630CLK_RESTRT	01
#define	KA630CLK_REBOOT	02
#define	KA630CLK_HALT	03
#define	KA630CLK_DOTHIS	040
/* in progress flags */
#define	KA630CLK_BOOT	04
#define	KA630CLK_RSTRT	010
#define	KA630CLK_LANG	0360

#ifndef _LOCORE
struct ka630clock {
	u_short	sec;
	u_short	secalrm;
	u_short	min;
	u_short	minalrm;
	u_short	hr;
	u_short	hralrm;
	u_short	dayofwk;
	u_short	day;
	u_short	mon;
	u_short	yr;
	u_short	csr0;
	u_short	csr1;
	u_short	csr2;
	u_short	csr3;
	u_short	cpmbx;	/* CPMBX is used by the boot rom. see ka630-ug-3.3.3 */
};
#endif

#define KA630_NVR_ADRS	0x200B8024
/* Definitions for various locations in the KA630 console page */
#define KA630_PUTC_POLL 0x20
#define KA630_PUTC	0x24
#define KA630_GETC	0x1C
#define KA630_ROW	0x4C
#define KA630_MINROW	0x4D
#define KA630_MAXROW	0x4E
#define KA630_COL	0x50
#define KA630_MINCOL	0x51
#define KA630_MAXCOL	0x52

#endif /* _VAX_INCLUDE_KA630_H_ */

