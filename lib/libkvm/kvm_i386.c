/*	$OpenBSD: kvm_i386.c,v 1.25 2015/01/09 03:43:52 mlarkin Exp $ */
/*	$NetBSD: kvm_i386.c,v 1.9 1996/03/18 22:33:38 thorpej Exp $	*/

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

/*
 * i386 machine dependent routines for kvm.  Hopefully, the forthcoming
 * vm code will one day obsolete this module.
 */

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>
#include <nlist.h>
#include <kvm.h>

#include <uvm/uvm_extern.h>
#include <machine/vmparam.h>
#include <machine/pmap.h>

#include <limits.h>
#include <db.h>

#include "kvm_private.h"

#include <machine/pte.h>

/*
 * These must match the values in pmap.c/pmapae.c
 */
#define PD_MASK		0xffc00000	/* page directory address bits */
#define PT_MASK		0x003ff000	/* page table address bits */
#define pdei(VA)	(((VA) & PD_MASK) >> PDSHIFT)
#define ptei(VA)	(((VA) & PT_MASK) >> PAGE_SHIFT)


struct vmstate {
	pd_entry_t *PTD;
};

void
_kvm_freevtop(kvm_t *kd)
{
	if (kd->vmst != NULL) {
		if (kd->vmst->PTD != NULL)
			free(kd->vmst->PTD);

		free(kd->vmst);
		kd->vmst = NULL;
	}
}

int
_kvm_initvtop(kvm_t *kd)
{
	struct nlist nl[2];
	struct vmstate *vm;
	u_long pa;

	vm = (struct vmstate *)_kvm_malloc(kd, sizeof(*vm));
	if (vm == NULL)
		return (-1);
	kd->vmst = vm;

	vm->PTD = NULL;

	nl[0].n_name = "_PTDpaddr";
	nl[1].n_name = NULL;

	if (kvm_nlist(kd, nl) != 0) {
		_kvm_err(kd, kd->program, "bad namelist");
		return (-1);
	}

	if (_kvm_pread(kd, kd->pmfd, &pa, sizeof pa,
	    (off_t)_kvm_pa2off(kd, nl[0].n_value - KERNBASE)) != sizeof pa)
		goto invalid;

	vm->PTD = (pd_entry_t *)_kvm_malloc(kd, kd->nbpg);

	if (_kvm_pread(kd, kd->pmfd, vm->PTD, kd->nbpg,
	    (off_t)_kvm_pa2off(kd, pa)) != kd->nbpg)
		goto invalid;

	return (0);

invalid:
	if (vm->PTD != NULL) {
		free(vm->PTD);
		vm->PTD = NULL;
	}
	return (-1);
}

/*
 * Translate a kernel virtual address to a physical address.
 */
int
_kvm_kvatop(kvm_t *kd, u_long va, paddr_t *pa)
{
	u_long offset, pte_pa;
	struct vmstate *vm;
	pt_entry_t pte;

	if (!kd->vmst) {
		_kvm_err(kd, 0, "vatop called before initvtop");
		return (0);
	}

	if (ISALIVE(kd)) {
		_kvm_err(kd, 0, "vatop called in live kernel!");
		return (0);
	}

	vm = kd->vmst;
	offset = va & (kd->nbpg - 1);

	/*
	 * If we are initializing (kernel page table descriptor pointer
	 * not yet set) * then return pa == va to avoid infinite recursion.
	 */
	if (vm->PTD == NULL) {
		*pa = va;
		return (kd->nbpg - (int)offset);
	}
	if ((vm->PTD[pdei(va)] & PG_V) == 0)
		goto invalid;

	pte_pa = (vm->PTD[pdei(va)] & PG_FRAME) +
	    (ptei(va) * sizeof(pt_entry_t));

	/* XXX READ PHYSICAL XXX */
	if (_kvm_pread(kd, kd->pmfd, &pte, sizeof pte,
	    (off_t)_kvm_pa2off(kd, pte_pa)) != sizeof pte)
		goto invalid;

	*pa = (pte & PG_FRAME) + offset;
	return (kd->nbpg - (int)offset);

invalid:
	_kvm_err(kd, 0, "invalid address (%lx)", va);
	return (0);
}

/*
 * Translate a physical address to a file-offset in the crash-dump.
 */
off_t
_kvm_pa2off(kvm_t *kd, paddr_t pa)
{
	return ((off_t)(kd->dump_off + pa));
}
