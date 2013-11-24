/*	$OpenBSD: param.h,v 1.42 2013/11/24 22:15:24 miod Exp $ */

/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
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

#define	_MACHINE	vax
#define	MACHINE		"vax"
#define	_MACHINE_ARCH	vax
#define	MACHINE_ARCH	"vax"
#define	MID_MACHINE	MID_VAX

#define	PAGE_SHIFT	12
#define	PAGE_SIZE	(1 << PAGE_SHIFT)
#define	PAGE_MASK	(PAGE_SIZE - 1)

#define	VAX_PGSHIFT	9
#define	VAX_NBPG	(1 << VAX_PGSHIFT)
#define	VAX_PGOFSET	(VAX_NBPG - 1)
#define	VAX_NPTEPG	(VAX_NBPG / 4)

#define	KERNBASE	0x80000000		/* start of kernel virtual */

#ifdef _KERNEL

#define	NBPG		PAGE_SIZE		/* bytes/page */
#define	PGSHIFT		PAGE_SHIFT		/* LOG2(PAGE_SIZE) */
#define	PGOFSET		PAGE_MASK		/* byte offset into page */

#define	UPAGES		2			/* pages of u-area */
#define	USPACE		(UPAGES * PAGE_SIZE)
#define	USPACE_ALIGN	(0)			/* u-area alignment 0-none */
#define	REDZONEADDR	(VAX_NBPG*3)		/* Must be > sizeof(struct user) */

#define	NMBCLUSTERS	1024			/* map size, max cluster allocation */

#ifndef	MSGBUFSIZE
#define	MSGBUFSIZE	(2 * PAGE_SIZE)		/* default message buffer size */
#endif

/*
 * Maximum size of the kernel malloc arena in PAGE_SIZE-sized
 * logical pages.
 */
#define	NKMEMPAGES_MAX_DEFAULT	((4 * 1024 * 1024) >> PAGE_SHIFT)

/* MD conversion macros */
#define	vax_atop(x)	(((unsigned)(x) + VAX_PGOFSET) >> VAX_PGSHIFT)
#define	vax_btop(x)	(((unsigned)(x)) >> VAX_PGSHIFT)

#include <machine/intr.h>

/* Prototype needed for delay() */
#ifndef	_LOCORE
#include <machine/cpu.h>

void	delay(int);
/* inline macros used inside kernel */
#include <machine/macros.h>
#endif

#define	DELAY(x) delay(x)
#endif /* _KERNEL */

#endif /* _MACHINE_PARAM_H_ */
