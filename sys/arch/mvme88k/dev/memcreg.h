/*	$OpenBSD: memcreg.h,v 1.3 2003/06/02 07:06:56 deraadt Exp $ */

/*
 * Copyright (c) 1995 Theo de Raadt
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
 * the MEMC's registers are a subset of the MCECC chip
 */
struct memcreg {
	volatile u_char		memc_chipid;
	volatile u_char		xx0[3];
	volatile u_char		memc_chiprev;
	volatile u_char		xx1[3];
	volatile u_char		memc_memconf;
#define MEMC_MEMCONF_MSIZ	0x07
#define MEMC_MEMCONF_RTOB(x) ((4*1024*1024) << ((x) & MEMC_MEMCONF_MSIZ))
	volatile u_char		xx2[3];
	volatile u_char		memc_x0;
	volatile u_char		xx3[3];
	volatile u_char		memc_x1;
	volatile u_char		xx4[3];
	volatile u_char		memc_baseaddr;
	volatile u_char		xx5[3];
	volatile u_char		memc_control;
	volatile u_char		xx6[3];
	volatile u_char		memc_bclk;
	volatile u_char		xx7[3];

	/* the following registers only exist on the MCECC */
	volatile u_char		memc_datactl;
	volatile u_char		xx8[3];
	volatile u_char		memc_scrubctl;
	volatile u_char		xx9[3];
	volatile u_char		memc_scrubperh;
	volatile u_char		xx10[3];
	volatile u_char		memc_scrubperl;
	volatile u_char		xx11[3];
	volatile u_char		memc_chipprescale;
	volatile u_char		xx12[3];
	volatile u_char		memc_scrubtime;
	volatile u_char		xx13[3];
	volatile u_char		memc_scrubprescaleh;
	volatile u_char		xx14[3];
	volatile u_char		memc_scrubprescalem;
	volatile u_char		xx15[3];
	volatile u_char		memc_scrubprescalel;
	volatile u_char		xx16[3];
	volatile u_char		memc_scrubtimeh;
	volatile u_char		xx17[3];
	volatile u_char		memc_scrubtimel;
	volatile u_char		xx18[3];
	volatile u_char		memc_scrubaddrhh;
	volatile u_char		xx19[3];
	volatile u_char		memc_scrubaddrhm;
	volatile u_char		xx20[3];
	volatile u_char		memc_scrubaddrlm;
	volatile u_char		xx21[3];
	volatile u_char		memc_scrubaddrll;
	volatile u_char		xx22[3];
	volatile u_char		memc_errlog;
	volatile u_char		xx23[3];
	volatile u_char		memc_errloghh;
	volatile u_char		xx24[3];
	volatile u_char		memc_errloghm;
	volatile u_char		xx25[3];
	volatile u_char		memc_errloglm;
	volatile u_char		xx26[3];
	volatile u_char		memc_errlogll;
	volatile u_char		xx27[3];
	volatile u_char		memc_errsyndrome;
	volatile u_char		xx28[3];
	volatile u_char		memc_defaults1;
	volatile u_char		xx29[3];
	volatile u_char		memc_defaults2;
	volatile u_char		xx30[3];
};

#define MEMC_CHIPID		0x80
#define MCECC_CHIPID		0x81
