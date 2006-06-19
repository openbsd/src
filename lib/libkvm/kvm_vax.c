/*	$OpenBSD: kvm_vax.c,v 1.13 2006/06/19 20:25:49 miod Exp $ */
/*	$NetBSD: kvm_vax.c,v 1.3 1996/03/18 22:34:06 thorpej Exp $ */

/*-
 * Copyright (c) 1992, 1993
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
 * VAX machine dependent routines for kvm.  Hopefully, the forthcoming
 * vm code will one day obsolete this module.
 */

#include <sys/param.h>
#include <sys/user.h>
#include <sys/proc.h>
#include <sys/stat.h>
#include <sys/kcore.h>
#include <unistd.h>
#include <stdlib.h>
#include <nlist.h>
#include <kvm.h>

#include <uvm/uvm_extern.h>
#include <machine/vmparam.h>
#include <machine/kcore.h>
#include <machine/pmap.h>
#include <machine/pte.h>

#include <limits.h>
#include <db.h>

#include "kvm_private.h"

void
_kvm_freevtop(kvm_t *kd)
{
}

int
_kvm_initvtop(kvm_t *kd)
{
	return (0);
}

/*
 * Translate a kernel virtual address to a physical address using the
 * mapping information in kd->vm.  Returns the result in pa, and returns
 * the number of bytes that are contiguously available from this
 * physical address.  This routine is used only for crashdumps.
 */
int
_kvm_kvatop(kvm_t *kd, u_long va, paddr_t *pa)
{
	paddr_t ofs;
	u_long pteaddr;
	pt_entry_t pte;
	cpu_kcore_hdr_t *cpu_kh;

	if (ISALIVE(kd)) {
		_kvm_err(kd, 0, "vatop called in live kernel!");
		return (0);
	}

	if (va < (u_long)KERNBASE) {
		_kvm_err(kd, 0, "invalid address (%lx<%lx)", va, KERNBASE);
		return (0);
	}

	/* read pte from Sysmap */
	cpu_kh = kd->cpu_data;
	pteaddr = (cpu_kh->sysmap - KERNBASE) +
	    PG_PFNUM(va) * sizeof(pt_entry_t);
	if (_kvm_pread(kd, kd->pmfd, (char *)&pte, sizeof(pte),
	    (off_t)_kvm_pa2off(kd, pteaddr)) < 0) {
		_kvm_err(kd, 0, "invalid address (%lx)", va);
		return (0);
	}
	if ((pte & PG_V) == 0) {
		_kvm_err(kd, 0, "invalid pte %lx (address %lx)", pte, va);
		return (0);
	}

	ofs = va & PAGE_MASK;
	*pa = ((pte & PG_FRAME) << VAX_PGSHIFT) | ofs;
	return (int)(PAGE_SIZE - ofs);
}

/*
 * Translate a physical address to an offset in the crash dump.
 */
off_t
_kvm_pa2off(kvm_t *kd, paddr_t pa)
{
	return (kd->dump_off + pa);
}
