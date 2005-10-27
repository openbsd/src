/*	$OpenBSD: pmap_bootstrap.c,v 1.20 2005/10/27 16:04:08 martin Exp $ */

/* 
 * Copyright (c) 1995 Theo de Raadt
 * Copyright (c) 1999 Steve Murphree, Jr. (68060 support)
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
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

#include <uvm/uvm_extern.h>

char *iiomapbase;
int iiomapsize;
#define	ETHERPAGES	16
void *etherbuf;
int etherlen;

extern char *extiobase;
extern int maxmem;

#define	RELOC(v, t)	*((t*)((u_int)&(v) + firstpa))
#define	PA2VA(v, t)	*((t*)((u_int)&(v)))

#define	MACHINE_IIOMAPSIZE	RELOC(iiomapsize, int)
#define	MACHINE_INTIOBASE	RELOC(iiomapbase, int)
#define	MACHINE_EIOMAPSIZE	EIOMAPSIZE

#define	PMAP_MD_LOCALS		/* nothing */

#define	PMAP_MD_RELOC1() \
do { \
	RELOC(etherbuf, void *) = (void *)nextpa; \
	nextpa += ETHERPAGES * NBPG; \
} while (0)

#define	PMAP_MD_MAPIOSPACE() \
do { \
	pte = &((u_int *)kptpa)[atop(etherbuf)]; \
	epte = pte + ETHERPAGES; \
	while (pte < epte) { \
		*pte = (*pte & ~PG_CMASK) | PG_CIS | PG_U; \
		pte++; \
	} \
	RELOC(etherlen, int) = ETHERPAGES * NBPG; \
} while (0)

	/*
	 * intiobase, intiolimit: base and end of internal IO space.
	 * MACHINE_IIOMAPSIZE pages prior to external IO space at end of
	 * static kernel page table.
	 * extiobase: base of external IO space.
	 * MACHINE_EIOMAPSIZE pages at the end of the static kernel page table.
	 */
#define	PMAP_MD_RELOC2() \
do { \
	RELOC(intiobase, char *) = (char *)iiobase; \
	RELOC(intiolimit, char *) = (char *)eiobase; \
	RELOC(extiobase, char *) = (char *)eiobase; \
} while (0)

#define	PMAP_MD_MEMSIZE() \
do { \
	RELOC(avail_end, paddr_t) = ptoa(RELOC(maxmem, int)) - \
	    (round_page(MSGBUFSIZE) + ptoa(1)); \
} while (0)

#define	PMAP_MD_RELOC3()	/* nothing */

#include <m68k/m68k/pmap_bootstrap.c>

void
pmap_init_md()
{
	vaddr_t         addr;

	/*
	 * mark as unavailable the regions which we have mapped in
	 * pmap_bootstrap().
	 */
	addr = (vaddr_t) intiobase;
	if (uvm_map(kernel_map, &addr, ptoa(iiomapsize+EIOMAPSIZE),
	    NULL, UVM_UNKNOWN_OFFSET, 0,
	    UVM_MAPFLAG(UVM_PROT_NONE, UVM_PROT_NONE,
	      UVM_INH_NONE, UVM_ADV_RANDOM, UVM_FLAG_FIXED)))
		panic("pmap_init: bogons in the VM system!");
}
