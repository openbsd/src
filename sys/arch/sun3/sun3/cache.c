/*	$NetBSD: cache.c,v 1.6 1996/12/17 21:11:16 gwr Exp $	*/

/*-
 * Copyright (c) 1996 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Gordon W. Ross.
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

/*
 * cache flush operations specific to the Sun3 custom MMU
 * all are done using writes to control space
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/user.h>
#include <sys/queue.h>

#include <vm/vm.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>

#include <machine/cpu.h>
#include <machine/pte.h>
#include <machine/control.h>
#include <machine/vmparam.h>

#include "machdep.h"

#define	CACHE_LINE	16	/* bytes */
#define	VAC_FLUSH_INCR	512	/* bytes */
#define VADDR_MASK	0xfFFffFF	/* 28 bits */

static void cache_clear_tags __P((void));

void
cache_flush_page(pgva)
	vm_offset_t pgva;
{
	register char *va, *endva;
	register int old_dfc, ctl_dfc;
	register int data;

	pgva &= (VADDR_MASK & ~PGOFSET);
	pgva |= VAC_FLUSH_BASE;

	/* Set up for writes to control space. */
	__asm __volatile ("movc dfc, %0" : "=d" (old_dfc));
	ctl_dfc = FC_CONTROL;
	__asm __volatile ("movc %0, dfc" : : "d" (ctl_dfc));

	/* Write to control space for each cache line. */
	va = (char *) pgva;
	endva = (char *) (pgva + NBPG);
	data = VAC_FLUSH_PAGE;

	do {
		__asm __volatile ("movsl %0, %1@" : : "d" (data), "a" (va));
		va += VAC_FLUSH_INCR;
	} while (va < endva);

	/* Restore destination function code. */
	__asm __volatile ("movc %0, dfc" : : "d" (old_dfc));
}

void
cache_flush_segment(sgva)
	vm_offset_t sgva;
{
	register char *va, *endva;
	register int old_dfc, ctl_dfc;
	register int data;

	sgva &= (VADDR_MASK & ~SEGOFSET);
	sgva |= VAC_FLUSH_BASE;

	/* Set up for writes to control space. */
	__asm __volatile ("movc dfc, %0" : "=d" (old_dfc));
	ctl_dfc = FC_CONTROL;
	__asm __volatile ("movc %0, dfc" : : "d" (ctl_dfc));

	/* Write to control space for each cache line. */
	va = (char *) sgva;
	endva = (char *) (sgva + cache_size);
	data = VAC_FLUSH_SEGMENT;

	do {
		__asm __volatile ("movsl %0, %1@" : : "d" (data), "a" (va));
		va += VAC_FLUSH_INCR;
	} while (va < endva);

	/* Restore destination function code. */
	__asm __volatile ("movc %0, dfc" : : "d" (old_dfc));
}

void
cache_flush_context()
{
	register char *va, *endva;
	register int old_dfc, ctl_dfc;
	register int data;

	/* Set up for writes to control space. */
	__asm __volatile ("movc dfc, %0" : "=d" (old_dfc));
	ctl_dfc = FC_CONTROL;
	__asm __volatile ("movc %0, dfc" : : "d" (ctl_dfc));

	/* Write to control space for each cache line. */
	va = (char *) VAC_FLUSH_BASE;
	endva = (char *) (VAC_FLUSH_BASE + cache_size);
	data = VAC_FLUSH_CONTEXT;

	do {
		__asm __volatile ("movsl %0, %1@" : : "d" (data), "a" (va));
		va += VAC_FLUSH_INCR;
	} while (va < endva);

	/* Restore destination function code. */
	__asm __volatile ("movc %0, dfc" : : "d" (old_dfc));
}

static void
cache_clear_tags()
{
	register char *va, *endva;
	register int old_dfc, ctl_dfc;
	register int data;

	/* Set up for writes to control space. */
	__asm __volatile ("movc dfc, %0" : "=d" (old_dfc));
	ctl_dfc = FC_CONTROL;
	__asm __volatile ("movc %0, dfc" : : "d" (ctl_dfc));

	/* Write to control space for each cache line. */
	va = (char *) VAC_CACHE_TAGS;
	endva = (char *) (VAC_CACHE_TAGS + cache_size);
	data = 0;	/* invalid tags */

	do {
		__asm __volatile ("movsl %0, %1@" : : "d" (data), "a" (va));
		va += CACHE_LINE;
	} while (va < endva);

	/* Restore destination function code. */
	__asm __volatile ("movc %0, dfc" : : "d" (old_dfc));
}

void
cache_enable()
{
	int enab_reg;

	if (cache_size == 0)
		return;

	/* Have to invalidate all tags before enabling the cache. */
	cache_clear_tags();

	/* OK, just set the "enable" bit. */
	enab_reg = get_control_byte((char *) SYSTEM_ENAB);
	enab_reg |= SYSTEM_ENAB_CACHE;
	set_control_byte((char *) SYSTEM_ENAB, enab_reg);

	/* Brag... */
	printf("cache enabled\n");
}
