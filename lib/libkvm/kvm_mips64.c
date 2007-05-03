/*	$OpenBSD: kvm_mips64.c,v 1.6 2007/05/03 19:33:58 miod Exp $ */
/*	$NetBSD: kvm_mips.c,v 1.3 1996/03/18 22:33:44 thorpej Exp $	*/

/*-
 * Copyright (c) 1989, 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software developed by the Computer Systems
 * Engineering group at Lawrence Berkeley Laboratory under DARPA contract
 * BG 91-66 and contributed to Berkeley. Modified for MIPS by Ralph Campbell.
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
static char sccsid[] = "@(#)kvm_mips.c	8.1 (Berkeley) 6/4/93";
#else
static char *rcsid = "$OpenBSD: kvm_mips64.c,v 1.6 2007/05/03 19:33:58 miod Exp $";
#endif
#endif /* LIBC_SCCS and not lint */

/*
 * MIPS machine dependent routines for kvm.  Hopefully, the forthcoming
 * vm code will one day obsolete this module.
 */

#include <sys/param.h>
#include <sys/user.h>
#include <sys/proc.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <nlist.h>
#include <kvm.h>

#include <limits.h>
#include <db.h>

#include "kvm_private.h"

#include <machine/cpu.h>
#include <machine/pte.h>
#include <machine/pmap.h>

struct vmstate {
	pt_entry_t	*Sysmap;
	u_int		Sysmapsize;
};

#define KREAD(kd, addr, p)\
	(kvm_read(kd, addr, (char *)(p), sizeof(*(p))) != sizeof(*(p)))

void
_kvm_freevtop(kvm_t *kd)
{
	if (kd->vmst != 0)
		free(kd->vmst);
}

int
_kvm_initvtop(kvm_t *kd)
{
	struct vmstate *vm;
	struct nlist nlist[3];

	vm = (struct vmstate *)_kvm_malloc(kd, sizeof(*vm));
	if (vm == 0)
		return (-1);
	kd->vmst = vm;

	nlist[0].n_name = "Sysmap";
	nlist[1].n_name = "Sysmapsize";
	nlist[2].n_name = 0;

	if (kvm_nlist(kd, nlist) != 0) {
		_kvm_err(kd, kd->program, "bad namelist");
		return (-1);
	}
	if (KREAD(kd, (u_long)nlist[0].n_value, &vm->Sysmap)) {
		_kvm_err(kd, kd->program, "cannot read Sysmap");
		return (-1);
	}
	if (KREAD(kd, (u_long)nlist[1].n_value, &vm->Sysmapsize)) {
		_kvm_err(kd, kd->program, "cannot read Sysmapsize");
		return (-1);
	}
	return (0);
}

/*
 * Translate a kernel virtual address to a physical address.
 */
int
_kvm_kvatop(kvm_t *kd, u_long va, paddr_t *pa)
{
	struct vmstate *vm;
	u_long pte, idx, addr, offset;

	if (ISALIVE(kd)) {
		_kvm_err(kd, 0, "vatop called in live kernel!");
		return((off_t)0);
	}
	vm = kd->vmst;
	offset = va & PAGE_MASK;
	/*
	 * If we are initializing (kernel segment table pointer not yet set)
	 * then return pa == va to avoid infinite recursion.
	 */
	if (vm->Sysmap == 0) {
		*pa = va;
		return (int)(PAGE_SIZE - offset);
	}
	/*
	 * Check for direct-mapped segments
	 */
	if (IS_XKPHYS(va)) {
		*pa = XKPHYS_TO_PHYS(va);
		return (int)(PAGE_SIZE - offset);
	}
	if (va >= (vaddr_t)KSEG0_BASE && va < (vaddr_t)KSSEG_BASE) {
		*pa = KSEG0_TO_PHYS(va);
		return (int)(PAGE_SIZE - offset);
	}
	if (va < VM_MIN_KERNEL_ADDRESS)
		goto invalid;
	idx = (va - VM_MIN_KERNEL_ADDRESS) >> PGSHIFT;
	if (idx >= vm->Sysmapsize)
		goto invalid;
	addr = (u_long)(vm->Sysmap + idx);
	/*
	 * Can't use KREAD to read kernel segment table entries.
	 * Fortunately it is 1-to-1 mapped so we don't have to.
	 */
	if (_kvm_pread(kd, kd->pmfd, (char *)&pte, sizeof(pte),
	    (off_t)addr) < 0)
		goto invalid;
	if (!(pte & PG_V))
		goto invalid;
	*pa = (pte & PG_FRAME) | offset;
	return (int)(PAGE_SIZE - offset);

invalid:
	_kvm_err(kd, 0, "invalid address (%lx)", va);
	return (0);
}

off_t
_kvm_pa2off(kvm_t *kd, paddr_t pa)
{
	_kvm_err(kd, 0, "pa2off going to be implemented!");
	return 0;
}
