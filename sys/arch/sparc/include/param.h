/*	$OpenBSD: param.h,v 1.51 2015/03/30 20:30:22 miod Exp $	*/

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

#ifndef	_MACHINE_PARAM_H_
#define	_MACHINE_PARAM_H_

#ifdef _KERNEL				/* XXX */
#ifndef _LOCORE				/* XXX */
#include <machine/cpu.h>		/* XXX */
#endif					/* XXX */
#endif					/* XXX */

/*
 * Sun4M support by Aaron Brown, Harvard University.
 * Changes Copyright (c) 1995 The President and Fellows of Harvard College.
 * All rights reserved.
 */
#define	_MACHINE	sparc
#define	MACHINE		"sparc"
#define	_MACHINE_ARCH	sparc
#define	MACHINE_ARCH	"sparc"
#define	MID_MACHINE	MID_SPARC

/*
 * The original SUN4 machines have 8K page size while the newer ones have a
 * 4K page size.  It is possible to build a kernel for either architecture,
 * or have the page size be dynamically handled (via uvmexp variables) at
 * run time.
 */
#define	SUN4_PGSHIFT	13			/* for a sun4 machine */
#define	SUN4CM_PGSHIFT	12			/* for a sun4c or sun4m machine */
#if (defined(SUN4) || defined(SUN4E)) && \
    !(defined(SUN4C) || defined(SUN4D) || defined(SUN4M))
# define PAGE_SHIFT	SUN4_PGSHIFT
# define PAGE_SIZE	(1 << PAGE_SHIFT)
# define PAGE_MASK	(PAGE_SIZE - 1)
#elif (defined(SUN4C) || defined(SUN4D) || defined(SUN4M)) && \
    !(defined(SUN4) || defined(SUN4E))
# define PAGE_SHIFT	SUN4CM_PGSHIFT
# define PAGE_SIZE	(1 << PAGE_SHIFT)
# define PAGE_MASK	(PAGE_SIZE - 1)
#elif defined(STANDALONE)	/* boot blocks */
# define PAGE_SHIFT	pgshift
# define PAGE_SIZE	nbpg
# define PAGE_MASK	pgofset
#else
# define PAGE_SHIFT	uvmexp.pageshift
# define PAGE_SIZE	uvmexp.pagesize
# define PAGE_MASK	uvmexp.pagemask
#endif

#define	KERNBASE	0xf8000000

#ifdef _KERNEL

#define	NBPG		PAGE_SIZE		/* bytes/page */
#define	PGSHIFT		PAGE_SHIFT		/* LOG2(PAGE_SIZE) */
#define	PGOFSET		PAGE_MASK		/* byte offset into page */

/*#define UPAGES	depends on machine model */
#define	USPACE		8192			/* total size of u-area */
#define	USPACE_ALIGN	0			/* u-area alignment 0-none */

#define	NMBCLUSTERS	2048			/* map size, max cluster allocation */

/* cannot be changed without great pain */
#ifndef	MSGBUFSIZE
#define	MSGBUFSIZE	4096			/* default message buffer size */
#endif

/*
 * Maximum size of the kernel malloc arena in PAGE_SIZE-sized
 * logical pages.
 */
#define	NKMEMPAGES_MAX_DEFAULT		((4 * 1024 * 1024) >> PAGE_SHIFT)

#ifndef _LOCORE
extern struct extent	*dvmamap_extent;

extern void	dvma_init(void);
extern caddr_t	kdvma_mapin(caddr_t, int, int);
extern caddr_t	dvma_malloc_space(size_t, void *, int, int);
extern void	dvma_free(caddr_t, size_t, void *);
#define		dvma_malloc(len,kaddr,flags)	dvma_malloc_space(len,kaddr,flags,0)

extern void	delay(unsigned int);
#define	DELAY(n)	delay(n)

extern int cputyp;

#endif /* _LOCORE */

#endif /* _KERNEL */

/*
 * Values for the cputyp variable. Order is important!
 * needed by locore, and libkvm
 */
#define	CPU_SUN4	0
#define	CPU_SUN4E	1
#define	CPU_SUN4C	2
#define	CPU_SUN4M	3
#define	CPU_SUN4D	4

/*
 * Shorthand CPU-type macros. Enumerate all eight cases.
 * Let compiler optimize away code conditional on constants.
 *
 * On sun4 and sun4e machines, the page size is 8192, while on sun4c, sun4d
 * and sun4m machines, it is 4096. Therefore, in the common case below, the
 * various pagesize-related defines are defined as variables which are
 * initialized early in locore.s after the machine type has been detected.
 *
 * Note that whenever the macros defined below evaluate to expressions
 * involving variables, the kernel will perform slightly worse due to the
 * extra memory references they'll generate.
 */
#if defined(SUN4) && !(defined(SUN4C) || defined(SUN4D) || defined(SUN4E) || defined(SUN4M))
#	define	CPU_ISSUN4	(1)
#elif defined(SUN4C) && !(defined(SUN4) || defined(SUN4D) || defined(SUN4E) || defined(SUN4M))
#	define	CPU_ISSUN4C	(1)
#elif defined(SUN4D) && !(defined(SUN4) || defined(SUN4C) || defined(SUN4E) || defined(SUN4M))
#	define	CPU_ISSUN4D	(1)
#elif defined(SUN4E) && !(defined(SUN4) || defined(SUN4C) || defined(SUN4D) || defined(SUN4M))
#	define	CPU_ISSUN4E	(1)
#elif defined(SUN4M) && !(defined(SUN4) || defined(SUN4C) || defined(SUN4D) || defined(SUN4E))
#	define	CPU_ISSUN4M	(1)
#elif (defined(SUN4) || defined(SUN4C)) && !(defined(SUN4D) || defined(SUN4E) || defined(SUN4M))
#	define	CPU_ISSUN4OR4C	(1)
#elif (defined(SUN4) || defined(SUN4C) || defined(SUN4E)) && !(defined(SUN4D) || defined(SUN4M))
#	define	CPU_ISSUN4OR4COR4E	(1)
#elif (defined(SUN4) || defined(SUN4E)) && !(defined(SUN4C) || defined(SUN4D) || defined(SUN4M))
#	define	CPU_ISSUN4OR4E	(1)
#elif (defined(SUN4C) || defined(SUN4M)) && !(defined(SUN4) || defined(SUN4D) || defined(SUN4E))
#	define	CPU_ISSUN4COR4M	(1)
#elif (defined(SUN4D) || defined(SUN4M)) && !(defined(SUN4) || defined(SUN4C) || defined(SUN4E))
#	define	CPU_ISSUN4DOR4M	(1)
#endif

#if !defined(CPU_ISSUN4)
#if defined(SUN4)
# define CPU_ISSUN4	(cputyp == CPU_SUN4)
#else
# define CPU_ISSUN4	(0)
#endif
#endif
#if !defined(CPU_ISSUN4C)
#if defined(SUN4C)
# define CPU_ISSUN4C	(cputyp == CPU_SUN4C)
#else
# define CPU_ISSUN4C	(0)
#endif
#endif
#if !defined(CPU_ISSUN4D)
#if defined(SUN4D)
# define CPU_ISSUN4D	(cputyp == CPU_SUN4D)
#else
# define CPU_ISSUN4D	(0)
#endif
#endif
#if !defined(CPU_ISSUN4E)
#if defined(SUN4E)
# define CPU_ISSUN4E	(cputyp == CPU_SUN4E)
#else
# define CPU_ISSUN4E	(0)
#endif
#endif
#if !defined(CPU_ISSUN4M)
#if defined(SUN4M)
# define CPU_ISSUN4M	(cputyp == CPU_SUN4M)
#else
# define CPU_ISSUN4M	(0)
#endif
#endif

#if !defined(CPU_ISSUN4OR4C)
# define CPU_ISSUN4OR4C	(CPU_ISSUN4 || CPU_ISSUN4C)
#endif
#if !defined(CPU_ISSUN4OR4E)
# define CPU_ISSUN4OR4E	(cputyp <= CPU_SUN4E)
				/* (CPU_ISSUN4 || CPU_ISSUN4E) */
#endif
#if !defined(CPU_ISSUN4OR4COR4E)
# define CPU_ISSUN4OR4COR4E	(cputyp <= CPU_SUN4C)
				/* (CPU_ISSUN4 || CPU_ISSUN4C || CPU_ISSUN4E) */
#endif
#if !defined(CPU_ISSUN4COR4M)
# define CPU_ISSUN4COR4M	(CPU_ISSUN4C || CPU_ISSUN4M)
#endif
#if !defined(CPU_ISSUN4DOR4M)
# define CPU_ISSUN4DOR4M	(cputyp >= CPU_SUN4M)
				/* (CPU_ISSUN4D || CPU_ISSUN4M) */
#endif

#endif /* _MACHINE_PARAM_H_ */
