/*	$OpenBSD: hp300spu.h,v 1.3 1998/05/10 11:31:56 downsj Exp $	*/
/*	$NetBSD: hp300spu.h,v 1.2 1997/05/01 05:26:48 thorpej Exp $	*/

/*-
 * Copyright (c) 1996, 1997 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _HP300_HP300SPU_H_
#define	_HP300_HP300SPU_H_

/*
 * This file describes various constants that describe and/or
 * are unique to the HP 9000/300 or 400 SPUs.
 */

/* values for machineid */
#define	HP_320		0	/* 16MHz 68020+HP MMU+16K external cache */
#define	HP_330		1	/* 16MHz 68020+68851 MMU */
#define	HP_350		2	/* 25MHz 68020+HP MMU+32K external cache */
#define	HP_360		3	/* 25MHz 68030 */
#define	HP_370		4	/* 33MHz 68030+64K external cache */
#define	HP_340		5	/* 16MHz 68030 */
#define	HP_345		6	/* 50MHz 68030+32K external cache */
#define	HP_375		7	/* 50MHz 68030+32K external cache */
#define	HP_400		8	/* 50MHz 68030+32K external cache */
#define	HP_380		9	/* 25MHz 68040 */
#define	HP_425		10	/* 25MHz 68040 */
#define HP_433		11	/* 33MHz 68040 */
#define	HP_385		12	/* 33MHz 68040 */

/* values for mmuid - used to differentiate similar CPU/cache combos */
#define	MMUID_345	1	/* 345 */
#define	MMUID_375	3	/* 375 */
#define	MMUID_385	2	/* 385 */
#define	MMUID_425_T	5	/* 425t - 25MHz Trailways */
#define	MMUID_425_S	7	/* 425s - 25MHz Strider */
#define	MMUID_433_T	4	/* 433t - 33MHz Trailways */
#define	MMUID_433_S	6	/* 433s - 33MHz Strider */
#define MMUID_425_E	9	/* 425e - 25Mhz Woody */

#define	MMUID_SHIFT	8	/* left shift by this... */
#define	MMUID_MASK	0xff	/* ...and mask with this to get mmuid */

#if defined (_KERNEL) && !defined(_LOCORE)
extern	int machineid;		/* CPU model */
extern	int cpuspeed;		/* CPU speed, in MHz */
extern	int mmuid;		/* MMU id */
#endif /* _KERNEL && ! _LOCORE */

#ifdef _KERNEL

/*
 * This section associates hp300 model configurations with certain
 * combindations of CPU, MMU, and cache.
 */

/*
 * Pull in user-defined SPU configuration options.
 */
#include "opt_hp320.h"
#include "opt_hp330.h"
#include "opt_hp340.h"
#include "opt_hp345.h"
#include "opt_hp350.h"
#include "opt_hp360.h"
#include "opt_hp370.h"
#include "opt_hp375.h"
#include "opt_hp380.h"
#include "opt_hp385.h"
#include "opt_hp400.h"
#include "opt_hp425.h"
#include "opt_hp433.h"

/*
 * CPU configuration.
 */
#if defined(HP320) || defined(HP330) || defined(HP350)
#define M68020
#endif

#if defined(HP340) || defined(HP345) || defined(HP360) || defined(HP370) || \
    defined(HP375) || defined(HP400)
#define M68030
#endif

#if defined(HP380) || defined(HP385) || defined(HP425) || defined(HP433)
#define M68040
#endif

/*
 * MMU configuration.
 */
#if defined(HP320) || defined(HP350)
#define M68K_MMU_HP
#endif

#if defined(HP330) || defined(M68030) || defined(M68040)
#define	M68K_MMU_MOTOROLA
#endif

/*
 * Cache configuration.
 */
#if defined(M68K_MMU_HP)
#define	CACHE_HAVE_VAC
#endif

#if defined(HP345) || defined(HP360) || defined(HP370) || \
    defined(HP375) || defined(HP400)
#define	CACHE_HAVE_PAC
#endif

#endif /* _KERNEL */

#endif /* _HP300_HP300SPU_H_ */
