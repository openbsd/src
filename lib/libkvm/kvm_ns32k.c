/*	$OpenBSD: kvm_ns32k.c,v 1.6 2001/11/05 23:04:35 art Exp $ */
/*	$NetBSD: kvm_ns32k.c,v 1.4 1996/03/18 22:33:50 thorpej Exp $	*/

/*-
 * Copyright (c) 1989, 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software developed by the Computer Systems
 * Engineering group at Lawrence Berkeley Laboratory under DARPA contract
 * BG 91-66 and contributed to Berkeley.
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
 */

#if defined(LIBC_SCCS) && !defined(lint)
#if 0
static char sccsid[] = "@(#)kvm_hp300.c	8.1 (Berkeley) 6/4/93";
#else
static char *rcsid = "$OpenBSD: kvm_ns32k.c,v 1.6 2001/11/05 23:04:35 art Exp $";
#endif
#endif /* LIBC_SCCS and not lint */

/*
 * ns32k machine dependent routines for kvm.  Hopefully, the forthcoming 
 * vm code will one day obsolete this module.
 */

#include <sys/param.h>
#include <sys/user.h>
#include <sys/proc.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>
#include <nlist.h>
#include <kvm.h>

#include <vm/vm.h>

#include <limits.h>
#include <db.h>

#include "kvm_private.h"

#include <machine/pte.h>

#ifndef btop
#define	btop(x)		(((unsigned)(x)) >> PGSHIFT)	/* XXX */
#define	ptob(x)		((caddr_t)((x) << PGSHIFT))	/* XXX */
#endif

struct vmstate {
	pd_entry_t **PTDpaddr;
	pd_entry_t *PTD;
};

#define KREAD(kd, addr, p)\
	(kvm_read(kd, addr, (char *)(p), sizeof(*(p))) != sizeof(*(p)))

void
_kvm_freevtop(kd)
	kvm_t *kd;
{
	if (kd->vmst != 0) {
		if (kd->vmst->PTD != 0)
			free(kd->vmst->PTD);

		free(kd->vmst);
	}
}

int
_kvm_initvtop(kd)
	kvm_t *kd;
{
	struct vmstate *vm;
	struct nlist nlist[2];
	pt_entry_t *tmpPTD;

	vm = (struct vmstate *)_kvm_malloc(kd, sizeof(*vm));
	if (vm == 0)
		return (-1);
	kd->vmst = vm;

	nlist[0].n_name = "_PTDpaddr";
	nlist[1].n_name = 0;

	if (kvm_nlist(kd, nlist) != 0) {
		_kvm_err(kd, kd->program, "bad namelist");
		return (-1);
	}

	vm->PTDpaddr = 0;
	vm->PTD = 0;
	if (KREAD(kd, (u_long)nlist[0].n_value - KERNBASE, &vm->PTDpaddr)) {
		_kvm_err(kd, kd->program, "cannot read PTDpaddr");
		return (-1);
	}

	tmpPTD = (pd_entry_t *)_kvm_malloc(kd, NBPG);
	if ((kvm_read(kd, (u_long)vm->PTDpaddr, tmpPTD, NBPG)) != NBPG) {
		free(tmpPTD);
		_kvm_err(kd, kd->program, "cannot read PTD");
		return (-1);
	}
	vm->PTD = tmpPTD;
	return (0);
}

/*
 * Translate a kernel virtual address to a physical address.
 */
int
_kvm_kvatop(kd, va, pa)
	kvm_t *kd;
	u_long va;
	u_long *pa;
{
	struct vmstate *vm;
	u_long offset;
	u_long pte_pa;
	pt_entry_t pte;

	if (ISALIVE(kd)) {
		_kvm_err(kd, 0, "vatop called in live kernel!");
		return(0);
	}

	vm = kd->vmst;
	offset = va & PGOFSET;

        /*
         * If we are initializing (kernel page table descriptor pointer
	 * not yet set) * then return pa == va to avoid infinite recursion.
         */
        if (vm->PTD == 0) {
                *pa = va;
                return (NBPG - offset);
        }
	if ((vm->PTD[pdei(va)] & PG_V) == 0)
		goto invalid;

	pte_pa = (vm->PTD[pdei(va)] & PG_FRAME) +
	    (ptei(va) * sizeof(pt_entry_t));
	/* XXX READ PHYSICAL XXX */
	{
		if (_kvm_pread(kd, kd->pmfd, &pte, sizeof pte, (off_t)pte_pa) != sizeof pte) {
			goto invalid;
		}
	}

	*pa = (pte & PG_FRAME) + offset;
	return (NBPG - offset);

invalid:
	_kvm_err(kd, 0, "invalid address (%lx)", va);
	return (0);
}
