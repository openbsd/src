/*	$OpenBSD: kvm_i386.c,v 1.19 2006/06/09 09:46:04 mickey Exp $ */
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

#if defined(LIBC_SCCS) && !defined(lint)
#if 0
static char sccsid[] = "@(#)kvm_hp300.c	8.1 (Berkeley) 6/4/93";
#else
static char *rcsid = "$OpenBSD: kvm_i386.c,v 1.19 2006/06/09 09:46:04 mickey Exp $";
#endif
#endif /* LIBC_SCCS and not lint */

/*
 * i386 machine dependent routines for kvm.  Hopefully, the forthcoming
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

#include <uvm/uvm_extern.h>
#include <machine/vmparam.h>
#include <machine/pmap.h>

#include <limits.h>
#include <db.h>

#include "kvm_private.h"

#include <machine/pte.h>

struct vmstate {
	void *PTD;
	paddr_t pg_frame;
	paddr_t pt_mask;
	int size;
	int pte_size;
};

#define	pdei(vm,v)	((v) >> ((vm)->size == NBPG ? 22 : 21))
#define	ptei(vm,v)	(((v) & (vm)->pt_mask) >> PGSHIFT)
#define	PDE(vm,v)	\
    ((vm)->size == NBPG ? ((u_int32_t *)(vm)->PTD)[pdei(vm,v)] : \
    ((u_int64_t *)(vm)->PTD)[pdei(vm,v)])
#define	PG_FRAME(vm)	((vm)->pg_frame)
#define	pte_size(vm)	((vm)->pte_size)

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
	struct nlist nl[3];
	struct vmstate *vm;
	paddr_t pa;
	int ps;

	vm = (struct vmstate *)_kvm_malloc(kd, sizeof(*vm));
	if (vm == NULL)
		return (-1);
	kd->vmst = vm;

	vm->PTD = NULL;

	nl[0].n_name = "_PTDpaddr";
	nl[1].n_name = "_PTDsize";
	nl[2].n_name = NULL;

	if (kvm_nlist(kd, nl) != 0) {
		_kvm_err(kd, kd->program, "bad namelist");
		return (-1);
	}

	if (_kvm_pread(kd, kd->pmfd, &ps, sizeof ps,
	    _kvm_pa2off(kd, nl[1].n_value - KERNBASE)) != sizeof ps)
		return (-1);

	pa = 0;
	if (ps == NBPG) {
		vm->pg_frame = 0xfffff000;
		vm->pt_mask = 0x003ff000;
		vm->pte_size = 4;
	} else if (ps == NBPG * 4) {
		vm->pg_frame = 0xffffff000ULL;
		vm->pt_mask = 0x001ff000;
		vm->pte_size = 8;
	} else {
		_kvm_err(kd, 0, "PTDsize is invalid");
		return (-1);
	}

	if (_kvm_pread(kd, kd->pmfd, &pa, vm->pte_size,
	    _kvm_pa2off(kd, nl[0].n_value - KERNBASE)) != vm->pte_size)
		return (-1);

	vm->PTD = _kvm_malloc(kd, ps);
	if (vm->PTD == NULL)
		return (-1);

	if (_kvm_pread(kd, kd->pmfd, vm->PTD, ps, _kvm_pa2off(kd, pa)) != ps) {
		free(vm->PTD);
		vm->PTD = NULL;
		return (-1);
	}

	vm->size = ps;
	return (0);
}

/*
 * Translate a kernel virtual address to a physical address.
 */
int
_kvm_kvatop(kvm_t *kd, u_long va, paddr_t *pa)
{
	u_long offset;
	struct vmstate *vm;
	paddr_t pte, pte_pa;

	if (!kd->vmst) {
		_kvm_err(kd, 0, "vatop called before initvtop");
		return (0);
	}

	if (ISALIVE(kd)) {
		_kvm_err(kd, 0, "vatop called in live kernel!");
		return (0);
	}

	vm = kd->vmst;
	offset = va & PGOFSET;

	/*
	 * If we are initializing (kernel page table descriptor pointer
	 * not yet set) * then return pa == va to avoid infinite recursion.
	 */
	if (vm->PTD == NULL) {
		*pa = va;
		return (NBPG - (int)offset);
	}
	if ((PDE(vm, va) & PG_V) == 0)
		goto invalid;

	pte_pa = (PDE(vm, va) & PG_FRAME(vm)) +
	    (ptei(vm, va) * pte_size(vm));

	/* XXX READ PHYSICAL XXX */
	pte = 0;
	if (_kvm_pread(kd, kd->pmfd, &pte, pte_size(vm),
	    _kvm_pa2off(kd, pte_pa)) != pte_size(vm))
		goto invalid;

	*pa = (pte & PG_FRAME(vm)) + offset;
	return (NBPG - (int)offset);

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
