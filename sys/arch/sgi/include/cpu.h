/*	$OpenBSD: cpu.h,v 1.6 2009/12/25 21:02:18 miod Exp $ */
/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Ralph Campbell and Rick Macklem.
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
 *	Copyright (C) 1989 Digital Equipment Corporation.
 *	Permission to use, copy, modify, and distribute this software and
 *	its documentation for any purpose and without fee is hereby granted,
 *	provided that the above copyright notice appears in all copies.
 *	Digital Equipment Corporation makes no representations about the
 *	suitability of this software for any purpose.  It is provided "as is"
 *	without express or implied warranty.
 *
 *	from: @(#)cpu.h	8.4 (Berkeley) 1/4/94
 */

#ifndef _SGI_CPU_H_
#define _SGI_CPU_H_

#ifdef _KERNEL

#ifdef MULTIPROCESSOR

#if defined(TGT_OCTANE)
#define HW_CPU_NUMBER_REG 0x900000000ff50000 /* HEART_PRID */
#else
#error MULTIPROCESSOR kernel not supported on this configuration
#endif

#if !defined(_LOCORE)
void hw_cpu_boot_secondary(struct cpu_info *);
void hw_cpu_hatch(struct cpu_info *);
void hw_cpu_spinup_trampoline(struct cpu_info *);
int  hw_ipi_intr_establish(int (*)(void *), u_long);
void hw_ipi_intr_set(u_long);
void hw_ipi_intr_clear(u_long);
#endif

#define hw_cpu_number() (*(uint64_t *)HW_CPU_NUMBER_REG)

#else	/* MULTIPROCESSOR */

#define hw_cpu_number() 0

#endif	/* MULTIPROCESSOR */

/*
 * Define soft selected cache functions.
 */
#define	Mips_SyncCache()			\
	(*(sys_config._SyncCache))()
#define	Mips_InvalidateICache(va, l)		\
	(*(sys_config._InvalidateICache))((va), (l))
#define	Mips_SyncDCachePage(va, pa)		\
	(*(sys_config._SyncDCachePage))((va))
#define	Mips_HitSyncDCache(va, pa, l)		\
	(*(sys_config._HitSyncDCache))((va), (l))
#define	Mips_IOSyncDCache(va, pa, l, h)		\
	(*(sys_config._IOSyncDCache))((va), (l), (h))
#define	Mips_HitInvalidateDCache(va, pa, l)	\
	(*(sys_config._HitInvalidateDCache))((va), (l))

#endif/* _KERNEL */

#include <mips64/cpu.h>

#endif /* !_SGI_CPU_H_ */
