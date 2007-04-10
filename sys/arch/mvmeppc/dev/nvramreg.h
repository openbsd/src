/*	$OpenBSD: nvramreg.h,v 1.5 2007/04/10 17:47:54 miod Exp $ */

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
 *
 *	@(#)clockreg.h	8.1 (Berkeley) 6/11/93
 */

/*
 * mvme2x00 Mostek TOD clock/NVRAM
 */

/*
 * Mostek MK48T59 clock.
 * 
 * This chip is 8k in size.
 * The first TOD clock starts at offset 0x1FF8. The following structure
 * describes last 2K of its 8K address space. The first 6K of the NVRAM
 * space is used for various things as follows:
 * 	0000-0fff	User Area
 *	1000-10ff	Networking Area 
 *	1100-16f7	Operating System Area
 *	16f8-1ef7	ROM Debugger Area
 *	1ef8-1ff7	Configuration Area (Ethernet address etc)
 *	1ff8-1fff	TOD clock
 */

#define	NVRAM_BASE		0x80000000	/* access thrugh ISA space! */
#define NVRAM_S0		0x00000074
#define NVRAM_S1		0x00000075
#define NVRAM_DATA		0x00000077

#define	NVRAM_SIZE		0x2000

#define RTC_SECONDS		0x1FF9
#define RTC_MINUTES		0x1FFA
#define RTC_HOURS		0x1FFB
#define RTC_DAY_OF_WEEK		0x1FFC
#define RTC_DAY_OF_MONTH	0x1FFD
#define RTC_MONTH		0x1FFE
#define RTC_YEAR		0x1FFF

#define RTC_CONTROLA		0x1FF8
#define RTC_CA_WRITE		0x80
#define RTC_CA_READ		0x40
#define RTC_CA_CALIB_SIGN	0x20
#define RTC_CA_CALIB_MASK	0x1f

#define RTC_CONTROLB		0x1FF9
#define RTC_CB_STOP		0x80

