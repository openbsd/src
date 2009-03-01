/*	$OpenBSD: nvramreg.h,v 1.5 2009/03/01 21:40:49 miod Exp $ */

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
 */

/*
 * Mostek MK48T02 clock.
 */
struct clockreg {
	volatile u_char	cl_csr;		/* control register */
	volatile u_char	cl_sec;		/* seconds (0..59; BCD) */
	volatile u_char	cl_min;		/* minutes (0..59; BCD) */
	volatile u_char	cl_hour;	/* hour (0..23; BCD) */
	volatile u_char	cl_wday;	/* weekday (1..7) */
	volatile u_char	cl_mday;	/* day in month (1..31; BCD) */
	volatile u_char	cl_month;	/* month (1..12; BCD) */
	volatile u_char	cl_year;	/* year (0..99; BCD) */
};

/*
 * MK48T08 clock registers as found on MVME165 (sparse layout)
 */
struct clockreg_165 {
	volatile u_char __cl_pad0[3];
	volatile u_char	cl_csr;		/* control register */
	volatile u_char __cl_pad1[3];
	volatile u_char	cl_sec;		/* seconds (0..59; BCD) */
	volatile u_char __cl_pad2[3];
	volatile u_char	cl_min;		/* minutes (0..59; BCD) */
	volatile u_char __cl_pad3[3];
	volatile u_char	cl_hour;	/* hour (0..23; BCD) */
	volatile u_char __cl_pad4[3];
	volatile u_char	cl_wday;	/* weekday (1..7) */
	volatile u_char __cl_pad5[3];
	volatile u_char	cl_mday;	/* day in month (1..31; BCD) */
	volatile u_char __cl_pad6[3];
	volatile u_char	cl_month;	/* month (1..12; BCD) */
	volatile u_char __cl_pad7[3];
	volatile u_char	cl_year;	/* year (0..99; BCD) */
};

/* bits in cl_csr */
#define	CLK_WRITE	0x80		/* want to write */
#define	CLK_READ	0x40		/* want to read (freeze clock) */

/*
 * Motorola chose the year `1900' as their base count.
 */
#define	YEAR0	0

#define MK48T02_SIZE	2*1024
#define MK48T08_SIZE	8*1024
