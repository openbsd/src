/*	$OpenBSD: param.h,v 1.29 2001/12/05 01:57:15 provos Exp $	*/
/*	$NetBSD: param.h,v 1.34 1996/03/04 05:04:40 cgd Exp $	*/

/*
 * Copyright (c) 1994, 1995 Gordon W. Ross
 * Copyright (c) 1993 Adam Glass
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1982, 1986, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	from: Utah Hdr: machparam.h 1.16 92/12/20
 *	from: @(#)param.h	8.1 (Berkeley) 6/10/93
 */

#ifndef	_MACHINE_PARAM_H_

/*
 * Machine dependent constants for the Sun3 series.
 */
#define	_MACHINE	sun3
#define	MACHINE		"sun3"

/*
 * Round p (pointer or byte index) up to a correctly-aligned value
 * for all data types (int, long, ...).   The result is u_int and
 * must be cast to any desired pointer type.
 */

#define	PGSHIFT		13		/* LOG2(NBPG) */

#define	PAGE_SHIFT	13
#define	PAGE_SIZE	(1 << PAGE_SHIFT)
#define	PAGE_MASK	(PAGE_SIZE - 1)

#define SEGSHIFT	17	        /* LOG2(NBSG) */
#define NBSG		(1 << SEGSHIFT)	/* bytes/segment */
#define	SEGOFSET	(NBSG-1)	/* byte offset into segment */

#define	KERNBASE	0x0E000000	/* start of kernel virtual */
#define	KERNTEXTOFF	0x0E004000	/* start of kernel text */

#include <m68k/param.h>

#define MAXBSIZE 0x8000		/* XXX temp until sun3 dma chaining */

#define	MSGBUFOFF	0x200
#define MSGBUFSIZE	(NBPG - MSGBUFOFF)

/*
 * Minimum and maximum sizes of the kernel malloc arena in PAGE_SIZE-sized
 * logical pages.
 */
#define	NKMEMPAGES_MIN_DEFAULT	((2 * 1024 * 1024) >> PAGE_SHIFT)
#define	NKMEMPAGES_MAX_DEFAULT	((2 * 1024 * 1024) >> PAGE_SHIFT)

/*
 * spl functions; all are done in-line
 */

#include <machine/psl.h>
#if defined(_KERNEL) && !defined(_LOCORE)
#include <machine/cpu.h>

extern void _delay __P((unsigned));
#define delay(us)	_delay((us)<<8)
#define	DELAY(n)	delay(n)
#endif	/* _KERNEL && !_LOCORE */

#endif	/* _MACHINE_PARAM_H_ */
