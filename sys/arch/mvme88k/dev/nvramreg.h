/*	$OpenBSD: nvramreg.h,v 1.2 1998/12/15 05:52:30 smurph Exp $ */

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
 *	@(#)clockreg.h	8.1 (Berkeley) 6/11/93
 */

/*
 * mvme1x7 Mostek TOD clock/NVRAM
 */

/*
 * Mostek MK48T08 clock.
 * 
 * This chip is 8k in size.
 * The first TOD clock starts at offset 0x1FF8. The following structure
 * describes last 2K of it's 8K address space. The first 6K of the NVRAM
 * space is used for various things as follows:
 * 	0000-0fff	User Area
 *	1000-10ff	Networking Area 
 *	1100-16f7	Operating System Area
 *	16f8-1ef7	ROM Debugger Area
 *	1ef8-1ff7	Configuration Area (Ethernet address etc)
 *	1ff8-1fff	TOD clock
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

/* bits in cl_csr */
#define	CLK_WRITE	0x80		/* want to write */
#define	CLK_READ	0x40		/* want to read (freeze clock) */

struct clockreg *clockreg;

/*
 * Motorola chose the year `1900' as their base count.
 * XXX what happens when it wraps?
 */
#define	YEAR0	00

#define NVRAMSIZE	0x8000
#define NVRAM_TOD_OFF	0x1ff8 /* offset of tod in NVRAM space */
#define MK48T02_SIZE	2*1024
#define MK48T08_SIZE	8*1024

