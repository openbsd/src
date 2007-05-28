/*	$OpenBSD: param.h,v 1.39 2007/05/28 21:02:49 thib Exp $	*/
/*	$NetBSD: param.h,v 1.29 1997/03/10 22:50:37 pk Exp $ */

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
 *	@(#)param.h	8.1 (Berkeley) 6/11/93
 */

#ifndef _SPARC_PARAM_H_
#define _SPARC_PARAM_H_

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

#ifdef _KERNEL				/* XXX */
#ifndef _LOCORE				/* XXX */
#include <machine/cpu.h>		/* XXX */
#endif					/* XXX */
#endif					/* XXX */

/*
 * Round p (pointer or byte index) up to a correctly-aligned value for
 * the machine's strictest data type.  The result is u_int and must be
 * cast to any desired pointer type.
 *
 * ALIGNED_POINTER is a boolean macro that checks whether an address
 * is valid to fetch data elements of type t from on this architecture.
 * This does not reflect the optimal alignment, just the possibility
 * (within reasonable limits). 
 *
 */
#define	ALIGNBYTES		7
#define	ALIGN(p)		(((u_int)(p) + ALIGNBYTES) & ~ALIGNBYTES)
#define ALIGNED_POINTER(p,t)	((((u_long)(p)) & (sizeof(t)-1)) == 0)

#define SUN4_PGSHIFT	13	/* for a sun4 machine */
#define SUN4CM_PGSHIFT	12	/* for a sun4c or sun4m machine */

#define	KERNBASE	0xf8000000	/* start of kernel virtual space */
#define	KERNTEXTOFF	0xf8004000	/* start of kernel text */

#define	DEV_BSHIFT	9		/* log2(DEV_BSIZE) */
#define	DEV_BSIZE	(1 << DEV_BSHIFT)
#define	BLKDEV_IOSIZE	2048
#define	MAXPHYS		(64 * 1024)

#define	USPACE		8192
#define	USPACE_ALIGN	(0)		/* u-area alignment 0-none */

/*
 * Constants related to network buffer management.
 */
#define	NMBCLUSTERS	2048		/* map size, max cluster allocation */

#define MSGBUFSIZE	4096		/* cannot be changed without great pain */

/*
 * Minimum and maximum sizes of the kernel malloc arena in PAGE_SIZE-sized
 * logical pages.
 */
#define	NKMEMPAGES_MIN_DEFAULT		((4 * 1024 * 1024) >> PAGE_SHIFT)
#define	NKMEMPAGES_MAX_DEFAULT		((4 * 1024 * 1024) >> PAGE_SHIFT)

/* pages ("clicks") to disk blocks */
#define	ctod(x)		((x) << (PGSHIFT - DEV_BSHIFT))
#define	dtoc(x)		((x) >> (PGSHIFT - DEV_BSHIFT))

/* pages to bytes */
#define	ctob(x)		((x) << PGSHIFT)
#define	btoc(x)		(((x) + PGOFSET) >> PGSHIFT)

/* bytes to disk blocks */
#define	btodb(x)	((x) >> DEV_BSHIFT)
#define	dbtob(x)	((x) << DEV_BSHIFT)

/*
 * dvmamap manages a range of DVMA addresses intended to create double
 * mappings of physical memory. In a way, `dvmamap' is a submap of the
 * VM map `phys_map'. The difference is the use of the `resource map'
 * routines to manage page allocation, allowing DVMA addresses to be
 * allocated and freed from within interrupt routines.
 *
 * Note that `phys_map' can still be used to allocate memory-backed pages
 * in DVMA space.
 */
#ifdef _KERNEL
#ifndef _LOCORE
extern vaddr_t		dvma_base;
extern vaddr_t		dvma_end;
extern struct extent	*dvmamap_extent;

extern caddr_t	kdvma_mapin(caddr_t, int, int);
extern caddr_t	dvma_malloc_space(size_t, void *, int, int);
extern void	dvma_free(caddr_t, size_t, void *);
#define		dvma_malloc(len,kaddr,flags)	dvma_malloc_space(len,kaddr,flags,0)

extern void	delay(unsigned int);
#define	DELAY(n)	delay(n)

extern int cputyp;
#if 0
extern int cpumod;
extern int mmumod;
#endif

#endif /* _LOCORE */
#endif /* _KERNEL */

/*
 * Values for the cputyp variable.
 */
#define CPU_SUN4	0
#define CPU_SUN4C	1
#define CPU_SUN4M	2

/*
 * Shorthand CPU-type macros. Enumerate all eight cases.
 * Let compiler optimize away code conditional on constants.
 *
 * On a sun4 machine, the page size is 8192, while on a sun4c and sun4m
 * it is 4096. Therefore, in the (SUN4 && (SUN4C || SUN4M)) cases below,
 * NBPG, PGOFSET and PGSHIFT are defined as variables which are initialized
 * early in locore.s after the machine type has been detected.
 *
 * Note that whenever the macros defined below evaluate to expressions
 * involving variables, the kernel will perform slightly worse due to the
 * extra memory references they'll generate.
 */
#if   defined(SUN4M) && defined(SUN4C) && defined(SUN4)
#	define CPU_ISSUN4M	(cputyp == CPU_SUN4M)
#	define CPU_ISSUN4C	(cputyp == CPU_SUN4C)
#	define CPU_ISSUN4	(cputyp == CPU_SUN4)
#	define CPU_ISSUN4OR4C	(cputyp == CPU_SUN4 || cputyp == CPU_SUN4C)
#	define CPU_ISSUN4COR4M	(cputyp == CPU_SUN4C || cputyp == CPU_SUN4M)
#elif defined(SUN4M) && defined(SUN4C) && !defined(SUN4)
#	define CPU_ISSUN4M	(cputyp == CPU_SUN4M)
#	define CPU_ISSUN4C	(cputyp == CPU_SUN4C)
#	define CPU_ISSUN4	(0)
#	define CPU_ISSUN4OR4C	(cputyp == CPU_SUN4C)
#	define CPU_ISSUN4COR4M	(cputyp == CPU_SUN4C || cputyp == CPU_SUN4M)
#	define NBPG		4096
#	define PGOFSET		(NBPG-1)
#	define PGSHIFT		SUN4CM_PGSHIFT
#	define PAGE_SIZE	4096
#	define PAGE_MASK	(PAGE_SIZE - 1)
#	define PAGE_SHIFT	SUN4CM_PGSHIFT
#elif defined(SUN4M) && !defined(SUN4C) && defined(SUN4)
#	define CPU_ISSUN4M	(cputyp == CPU_SUN4M)
#	define CPU_ISSUN4C	(0)
#	define CPU_ISSUN4	(cputyp == CPU_SUN4)
#	define CPU_ISSUN4OR4C	(cputyp == CPU_SUN4)
#	define CPU_ISSUN4COR4M	(cputyp == CPU_SUN4M)
#elif defined(SUN4M) && !defined(SUN4C) && !defined(SUN4)
#	define CPU_ISSUN4M	(1)
#	define CPU_ISSUN4C	(0)
#	define CPU_ISSUN4	(0)
#	define CPU_ISSUN4OR4C	(0)
#	define CPU_ISSUN4COR4M	(1)
#	define NBPG		4096
#	define PGOFSET		(NBPG-1)
#	define PGSHIFT		SUN4CM_PGSHIFT
#	define PAGE_SIZE	4096
#	define PAGE_MASK	(PAGE_SIZE - 1)
#	define PAGE_SHIFT	SUN4CM_PGSHIFT
#elif !defined(SUN4M) && defined(SUN4C) && defined(SUN4)
#	define CPU_ISSUN4M	(0)
#	define CPU_ISSUN4C	(cputyp == CPU_SUN4C)
#	define CPU_ISSUN4	(cputyp == CPU_SUN4)
#	define CPU_ISSUN4OR4C	(1)
#	define CPU_ISSUN4COR4M	(cputyp == CPU_SUN4C)
#elif !defined(SUN4M) && defined(SUN4C) && !defined(SUN4)
#	define CPU_ISSUN4M	(0)
#	define CPU_ISSUN4C	(1)
#	define CPU_ISSUN4	(0)
#	define CPU_ISSUN4OR4C	(1)
#	define CPU_ISSUN4COR4M	(1)
#	define NBPG		4096
#	define PGOFSET		(NBPG-1)
#	define PGSHIFT		SUN4CM_PGSHIFT
#	define PAGE_SIZE	4096
#	define PAGE_MASK	(PAGE_SIZE - 1)
#	define PAGE_SHIFT	SUN4CM_PGSHIFT
#elif !defined(SUN4M) && !defined(SUN4C) && defined(SUN4)
#	define CPU_ISSUN4M	(0)
#	define CPU_ISSUN4C	(0)
#	define CPU_ISSUN4	(1)
#	define CPU_ISSUN4OR4C	(1)
#	define CPU_ISSUN4COR4M	(0)
#	define NBPG		8192
#	define PGOFSET		(NBPG-1)
#	define PGSHIFT		SUN4_PGSHIFT
#	define PAGE_SIZE	8192
#	define PAGE_MASK	(PAGE_SIZE - 1)
#	define PAGE_SHIFT	SUN4_PGSHIFT
#elif !defined(SUN4M) && !defined(SUN4C) && !defined(SUN4)
#	define CPU_ISSUN4M	(cputyp == CPU_SUN4M)
#	define CPU_ISSUN4C	(cputyp == CPU_SUN4C)
#	define CPU_ISSUN4	(cputyp == CPU_SUN4)
#	define CPU_ISSUN4OR4C	(cputyp == CPU_SUN4 || cputyp == CPU_SUN4C)
#	define CPU_ISSUN4COR4M	(cputyp == CPU_SUN4C || cputyp == CPU_SUN4M)
#endif

#ifndef	NBPG
#ifdef	STANDALONE	/* boot blocks */
#	define NBPG		nbpg
#	define PGOFSET		pgofset
#	define PGSHIFT		pgshift
#	define PAGE_SIZE	nbpg
#	define PAGE_MASK	pgofset
#	define PAGE_SHIFT	pgshift
#else
#	define NBPG		uvmexp.pagesize
#	define PGOFSET		uvmexp.pagemask
#	define PGSHIFT		uvmexp.pageshift
#	define PAGE_SIZE	uvmexp.pagesize
#	define PAGE_MASK	uvmexp.pagemask
#	define PAGE_SHIFT	uvmexp.pageshift
#endif
#endif

#endif /* _SPARC_PARAM_H_ */
