/*	$OpenBSD: pmap_bootstrap.c,v 1.19 2004/12/30 21:22:19 miod Exp $	*/
/*	$NetBSD: pmap_bootstrap.c,v 1.13 1997/06/10 18:56:50 veego Exp $	*/

/* 
 * Copyright (c) 1991, 1993
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
 *	@(#)pmap_bootstrap.c	8.1 (Berkeley) 6/10/93
 */

#include <sys/param.h>
#include <sys/msgbuf.h>

#include <machine/cpu.h>
#include <machine/frame.h>
#include <machine/pte.h>
#include <machine/vmparam.h>

#include <machine/hp300spu.h>
#include <hp300/hp300/clockreg.h>

#include <uvm/uvm_extern.h>

caddr_t ledbase;	/* SPU LEDs mapping */

extern vaddr_t CLKbase, MMUbase;
extern char *extiobase;
extern int maxmem;

#define	RELOC(v, t)	*((t*)((u_int)&(v) + firstpa))
#define	PA2VA(v, t)	*((t*)((u_int)&(v)))

#define	MACHINE_IIOMAPSIZE	IIOMAPSIZE
#define	MACHINE_INTIOBASE	INTIOBASE
#define	MACHINE_EIOMAPSIZE	EIOMAPSIZE

#define	PMAP_MD_LOCALS		/* nothing */

#define	PMAP_MD_RELOC1()	/* nothing */

#define PMAP_MD_MAPIOSPACE()	/* nothing */

	/*
	 * intiobase, intiolimit: base and end of internal (DIO) IO space.
	 * MACHINE_IIOMAPSIZE pages prior to external IO space at end of
	 * static kernel page table.
	 * extiobase: base of external (DIO-II) IO space.
	 * MACHINE_EIOMAPSIZE pages at the end of the static kernel page table.
	 * CLKbase, MMUbase: important registers in internal IO space
	 * accessed from locore.
	 */
#define	PMAP_MD_RELOC2() \
do { \
	RELOC(intiobase, char *) = (char *)iiobase; \
	RELOC(intiolimit, char *) = (char *)eiobase; \
	RELOC(extiobase, char *) = (char *)eiobase; \
	RELOC(CLKbase, vaddr_t) = iiobase + CLKBASE; \
	RELOC(MMUbase, vaddr_t) = iiobase + MMUBASE; \
} while (0)

#define	PMAP_MD_MEMSIZE() \
do { \
	RELOC(avail_end, paddr_t) = m68k_ptob(RELOC(maxmem, int)) - \
	    (round_page(MSGBUFSIZE) + m68k_ptob(1)); \
} while (0)

	/*
	 * Allocate some fixed, special purpose kernel virtual addresses
	 */
#define	PMAP_MD_RELOC3() \
do { \
		RELOC(ledbase, caddr_t) = (caddr_t)va; \
		va += NBPG; \
} while (0)

#include <m68k/m68k/pmap_bootstrap.c>

void
pmap_init_md()
{
	vaddr_t		addr;

	/*
	 * mark as unavailable the regions which we have mapped in
	 * pmap_bootstrap().
	 */
	addr = (vaddr_t) intiobase;
	if (uvm_map(kernel_map, &addr,
		    m68k_ptob(IIOMAPSIZE+EIOMAPSIZE),
		    NULL, UVM_UNKNOWN_OFFSET, 0,
		    UVM_MAPFLAG(UVM_PROT_NONE, UVM_PROT_NONE,
				UVM_INH_NONE, UVM_ADV_RANDOM,
				UVM_FLAG_FIXED)))
		panic("pmap_init: bogons in the VM system!");
}
