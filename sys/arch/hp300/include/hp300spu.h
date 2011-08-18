/*	$OpenBSD: hp300spu.h,v 1.10 2011/08/18 19:54:18 miod Exp $	*/
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

#ifndef _MACHINE_HP300SPU_H_
#define	_MACHINE_HP300SPU_H_

/*
 * This file describes various constants that describe and/or
 * are unique to the HP 9000/300 or 400 SPUs.
 */

/* values for machineid */
#define	HP_320		0	/* 16MHz 68020+HP MMU+16K external cache */
#define	HP_330		1	/* 16MHz 68020+68851 MMU */
#define	HP_350		2	/* 25MHz 68020+HP MMU+32K external cache */
#define	HP_36X		3	/* 25MHz 68030 */
#define	HP_370		4	/* 33MHz 68030+64K external cache */
#define	HP_340		5	/* 16MHz 68030 */
#define	HP_345		6	/* 50MHz 68030+32K external cache */
#define	HP_375		7	/* 50MHz 68030+32K external cache */
#define	HP_400		8	/* 50MHz 68030+32K external cache */
#define	HP_380		9	/* 25MHz 68040 */
#define	HP_425		10	/* 25MHz 68040 */
#define HP_433		11	/* 33MHz 68040 */
#define	HP_385		12	/* 33MHz 68040 */
#define	HP_382		13	/* 25MHz 68040 */

/* values for mmuid - used to differentiate similar CPU/cache combos */
#define	MMUID_345	1	/* 345 */
#define	MMUID_375	3	/* 375 */
#define	MMUID_382	11	/* 382 */
#define	MMUID_385	2	/* 385 */
#define	MMUID_425_T	5	/* 425t - 25MHz Trailways */
#define	MMUID_425_S	7	/* 425s - 25MHz Strider */
#define	MMUID_433_T	4	/* 433t - 33MHz Trailways */
#define	MMUID_433_S	6	/* 433s - 33MHz Strider */
#define MMUID_425_E	9	/* 425e - 25MHz Woody */

#define	MMUID_SHIFT	8	/* right shift by this... */
#define	MMUID_MASK	0xff	/* ...and mask with this to get mmuid */

#if defined (_KERNEL) && !defined(_LOCORE)
extern	int machineid;		/* CPU model */
extern	int cpuspeed;		/* CPU speed, in MHz */
extern	int mmuid;		/* MMU id */
#endif /* _KERNEL && ! _LOCORE */

#endif /* _MACHINE_HP300SPU_H_ */
