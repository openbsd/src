/*	$OpenBSD: param.h,v 1.10 2000/02/22 19:27:46 deraadt Exp $	*/
/*	$NetBSD: param.h,v 1.35 1997/07/10 08:22:38 veego Exp $	*/

/*
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
 * from: Utah $Hdr: machparam.h 1.16 92/12/20$
 *
 *	@(#)param.h	8.1 (Berkeley) 6/10/93
 */

#ifndef	_MACHINE_PARAM_H_
#define	_MACHINE_PARAM_H_

/*
 * Machine dependent constants for HP9000 series 300.
 */
#define	_MACHINE	hp300
#define	MACHINE		"hp300"

/*
 * Interrupt glue.
 */
#include <machine/intr.h>

#define	PGSHIFT		12		/* LOG2(NBPG) */
#define	KERNBASE	0x00000000	/* start of kernel virtual */

#define	SEGSHIFT	22		/* LOG2(NBSEG) */
#define NBSEG		(1 << SEGSHIFT)	/* bytes/segment */
#define	SEGOFSET	(NBSEG-1)	/* byte offset into segment */

#define	UPAGES		2		/* pages of u-area */

#include <m68k/param.h>

#define	NPTEPG		(NBPG/(sizeof (pt_entry_t)))

/*
 * Size of kernel malloc arena in CLBYTES-sized logical pages
 */ 
#ifndef NKMEMCLUSTERS
# define	NKMEMCLUSTERS	(2048 * 1024 / CLBYTES)
#endif

#define MSGBUFSIZE 4096

#if defined(_KERNEL) && !defined(_LOCORE)
#define	delay(us)	_delay((us) << 8)
#define DELAY(us)	delay(us)

void	_delay __P((u_int));
#endif /* _KERNEL && !_LOCORE */

#ifdef COMPAT_HPUX
/*
 * Constants/macros for HPUX multiple mapping of user address space.
 * Pages in the first 256Mb are mapped in at every 256Mb segment.
 */
#define HPMMMASK	0xF0000000
#define ISHPMMADDR(v) \
	((curproc->p_md.md_flags & MDP_HPUXMMAP) && \
	 ((unsigned)(v) & HPMMMASK) && \
	 ((unsigned)(v) & HPMMMASK) != HPMMMASK)
#define HPMMBASEADDR(v) \
	((unsigned)(v) & ~HPMMMASK)
#endif

#endif	/* !_MACHINE_PARAM_H_ */
