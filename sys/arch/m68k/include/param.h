/*	$OpenBSD: param.h,v 1.29 2013/03/25 17:46:24 deraadt Exp $	*/

/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1982, 1986, 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
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

#ifndef	_M68K_PARAM_H_
#define	_M68K_PARAM_H_

#define	_MACHINE_ARCH	m68k
#define	MACHINE_ARCH	"m68k"
#define	MID_MACHINE	MID_M68K

#define	PAGE_SIZE	(1 << PAGE_SHIFT)
#define	PAGE_MASK	(PAGE_SIZE - 1)
#define	PGSHIFT		PAGE_SHIFT
#define	PGOFSET		PAGE_MASK

#ifdef _KERNEL

#define	NPTEPG		(PAGE_SIZE / (sizeof(pt_entry_t)))

#define	BTOPKERNBASE	((u_long)KERNBASE >> PAGE_SHIFT)

#define	NBPG		PAGE_SIZE

#define	SEGSHIFT020	(34 - PAGE_SHIFT)
#define	SEGSHIFT040	(18)
#ifndef	SEGSHIFT
#if defined(M68040) || defined(M68060)
#if defined(M68020) || defined(M68030)
#define	SEGSHIFT	((mmutype <= MMU_68040) ? SEGSHIFT040 : SEGSHIFT020)
#else
#define	SEGSHIFT	SEGSHIFT040
#endif
#else
#define	SEGSHIFT	SEGSHIFT020
#endif
#define	NBSEG		(1 << SEGSHIFT)
#define	SEGOFSET	(NBSEG - 1)
#endif

#define	UPAGES		2			/* pages of u-area */
#define	USPACE		(UPAGES * PAGE_SIZE)	/* total size of u-area */
#define	USPACE_ALIGN	0			/* u-area alignment 0-none */

#define	NMBCLUSTERS	4096		/* map size, max cluster allocation */

/*
 * Maximum size of the kernel malloc arena in PAGE_SIZE-sized
 * logical pages.
 */
#define	NKMEMPAGES_MAX_DEFAULT	((64 * 1024 * 1024) >> PAGE_SHIFT)

/*
 * Mach derived conversion macros
 */
#define	m68k_round_seg(x)	((((unsigned)(x)) + SEGOFSET) & ~SEGOFSET)
#define	m68k_trunc_seg(x)	((unsigned)(x) & ~SEGOFSET)
#define	m68k_page_offset(x)	((unsigned)(x) & PGOFSET)

#include <machine/cpu.h>

#endif /* _KERNEL */

#endif /* _M68K_PARAM_H_ */
