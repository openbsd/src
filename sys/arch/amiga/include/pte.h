/*	$OpenBSD: pte.h,v 1.3 2001/11/30 23:20:09 miod Exp $	*/
/*	$NetBSD: pte.h,v 1.14 1995/09/29 13:52:09 chopps Exp $	*/

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
 * from: Utah $Hdr: pte.h 1.11 89/09/03$
 *
 *	@(#)pte.h	7.3 (Berkeley) 5/8/91
 */
#ifndef _MACHINE_PTE_H_
#define _MACHINE_PTE_H_

#include <m68k/pte_motorola.h>

#define AMIGA_040RTSIZE		512	/* root (level 1) table size */
#define AMIGA_040STSIZE		512	/* segment (level 2) table size */
#define	AMIGA_040PTSIZE		128	/* page (level 3) table size */

#if 0
#define AMIGA_STSIZE		1024	/* segment table size */
#else
#define	AMIGA_STSIZE		(MAXUL2SIZE*SG4_LEV2SIZE*sizeof(st_entry_t))
#endif
/*
 * AMIGA_MAX_COREUPT	maximum number of incore user page tables
 * AMIGA_USER_PTSIZE	the number of bytes for user pagetables
 * AMIGA_PTBASE		the VA start of the map from which upt's are allocated
 * AMIGA_PTSIZE		the size of the map from which upt's are allocated
 * AMIGA_KPTSIZE	size of kernel page table
 * AMIGA_MAX_KPTSIZE	the most number of bytes for kpt pages
 * AMIGA_MAX_PTSIZE	the number of bytes to map everything
 */
#define AMIGA_MAX_COREUPT	1024
#define AMIGA_UPTSIZE		roundup(VM_MAXUSER_ADDRESS / NPTEPG, NBPG)
#define AMIGA_UPTBASE		0x10000000
#define AMIGA_UPTMAXSIZE \
    roundup((AMIGA_MAX_COREUPT * AMIGA_UPTSIZE), NBPG)
#define AMIGA_MAX_KPTSIZE \
    (AMIGA_MAX_COREUPT * AMIGA_UPTSIZE / NPTEPG)
#define AMIGA_KPTSIZE \
    roundup((VM_MAX_KERNEL_ADDRESS - VM_MIN_KERNEL_ADDRESS) / NPTEPG, NBPG)
#define AMIGA_MAX_PTSIZE	roundup(0xffffffff / NPTEPG, NBPG)

#define	MACHINE_STSIZE		AMIGA_STSIZE
#define	MACHINE_MAX_PTSIZE	AMIGA_MAX_PTSIZE
#define	MACHINE_MAX_KPTSIZE	AMIGA_MAX_KPTSIZE
#define	MACHINE_PTBASE		AMIGA_UPTBASE
#define	MACHINE_PTMAXSIZE	AMIGA_UPTMAXSIZE

#endif /* !_MACHINE_PTE_H_ */
