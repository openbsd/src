/*	$OpenBSD: kvm_vax.c,v 1.11 2004/09/15 19:31:31 miod Exp $ */
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
 * vm code will one day obsolete this module.  Furthermore, I hope it
 * gets here soon, because this basically is an error stub! (sorry)
 * This code may not work anyway.
 */

#include <sys/param.h>
#include <sys/user.h>
#include <sys/proc.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <nlist.h>
#include <kvm.h>

#include <uvm/uvm_extern.h>
#include <machine/vmparam.h>
#include <machine/pmap.h>

#include <limits.h>
#include <db.h>

#include "kvm_private.h"

struct vmstate {
	u_long end;
};

void
_kvm_freevtop(kvm_t *kd)
{
	if (kd->vmst != NULL) {
		free(kd->vmst);
		kd->vmst = NULL;
	}
}

int
_kvm_initvtop(kvm_t *kd)
{
	struct nlist nlist[2];
	struct vmstate *vm;
	struct stat st;
	int i, off;

	vm = (struct vmstate *)_kvm_malloc(kd, sizeof(*vm));
	if (vm == 0)
		return (-1);

	kd->vmst = vm;

	if (fstat(kd->pmfd, &st) < 0)
		return (-1);

	/* Get end of kernel address */
	nlist[0].n_name = "_end";
	nlist[1].n_name = 0;
	if (kvm_nlist(kd, nlist) != 0) {
		_kvm_err(kd, kd->program, "pmap_stod: no such symbol");
		return (-1);
	}
	vm->end = (u_long)nlist[0].n_value;

	return (0);
}

#define VA_OFF(va) (va & (NBPG - 1))

/*
 * Translate a kernel virtual address to a physical address using the
 * mapping information in kd->vm.  Returns the result in pa, and returns
 * the number of bytes that are contiguously available from this
 * physical address.  This routine is used only for crashdumps.
 */
int
_kvm_kvatop(kvm_t *kd, u_long va, u_long *pa)
{
	u_long end;

	if (va < KERNBASE) {
		_kvm_err(kd, 0, "invalid address (%lx<%lx)", va, KERNBASE);
		return (0);
	}

	if (!kd->vmst) {
		_kvm_err(kd, 0, "vatop called before initvtop");
		return (0);
	}

	end = kd->vmst->end;
	if (va >= end) {
		_kvm_err(kd, 0, "invalid address (%lx>=%lx)", va, end);
		return (0);
	}

	*pa = (va - KERNBASE);
	return (end - va);
}

/*
 * Translate a physical address to an offset in the crash dump
 * XXX crashdumps not working yet anyway
 */
off_t
_kvm_pa2off(kvm_t *kd, u_long pa)
{
	return (kd->dump_off+pa);
}
