/* $OpenBSD: cpuswitch.S,v 1.4 2018/09/07 01:32:01 mortimer Exp $ */
/*
 * Copyright (c) 2015 Dale Rahn <drahn@dalerahn.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "machine/asm.h"
#include "assym.h"

/*
 * cpu_switchto(struct proc *oldproc, struct proc *newproc)
 * 	x0	'struct proc *' of the old context
 * 	x1	'struct proc *' of the new context
 *      x2	- may contain curcpu
 *      x3	- may contain switchframe pointer (new or old)
 *      x4	- may contain user thread pointer (TCB)
 *	x5	- may contain PCB pointer (new or old)
 */
ENTRY(cpu_switchto)
        // check if old context needs to be saved
	cmp x0, #0
	beq	1f

	// create switchframe
	sub	sp, sp, #(SWITCHFRAME_SZ)
	mov	x3, sp
	stp	x19, x20,  [x3, #(SF_X19)]
	stp	x21, x22,  [x3, #(SF_X21)]
	stp	x23, x24,  [x3, #(SF_X23)]
	stp	x25, x26,  [x3, #(SF_X25)]
	stp	x27, x28,  [x3, #(SF_X27)]
	stp	x29, x30,  [x3, #(SF_X29)]

	mrs	x2, tpidr_el1			// load curcpu
	ldr	x5, [x2, #(CI_CURPCB)]
	str 	x3, [x5, #(PCB_SP) ]		// save to old pcb


	mov	x19, x1				//save new ctx across vfp
	// old process has been saved
	// save old fpu?
	ldr	w7, [x5, #(PCB_FLAGS)]
	mov	w3, #(PCB_FPU)
	and	w7, w7, w3
	cbz	w7, 1f

	bl	vfp_save

	mov	x1, x19
1:
	RETGUARD_SYMBOL(cpu_switchto)
	RETGUARD_LOAD_RANDOM(cpu_switchto, x20)

	mrs	x2, tpidr_el1			// load curcpu
	mov	w5, #SONPROC
	strb	w5, [x1, #(P_STAT) ] 		// Mark new on cpu
	str	x2, [x1, #(P_CPU)]
	ldr 	x5, [x1, #(P_ADDR) ]		// load new pcb
	str	x5, [x2, #(CI_CURPCB)]
	str	x1, [x2, #(CI_CURPROC)]

	ldr	x4, [x5, #(PCB_TCB)]
	msr	tpidr_el0, x4			// load user tls

	ldr 	x19, [x5, #(PCB_SP) ]		// load new stack pointer
	mov	x0, x1
	bl	pmap_setttb
	mov	x3, x19
	mov	x15, x20

	mov	sp, x3

	ldp	x19, x20,  [x3, #(SF_X19)]
	ldp	x21, x22,  [x3, #(SF_X21)]
	ldp	x23, x24,  [x3, #(SF_X23)]
	ldp	x25, x26,  [x3, #(SF_X25)]
	ldp	x27, x28,  [x3, #(SF_X27)]
	ldp	x29, x30,  [x3, #(SF_X29)]
	RETGUARD_CALC_COOKIE(x15)
	add	sp, sp, #(SWITCHFRAME_SZ)
	RETGUARD_CHECK(cpu_switchto, x15)
	ret

ENTRY(proc_trampoline)
#ifdef MULTIPROCESSOR
	bl _C_LABEL(proc_trampoline_mp)
#endif
	// call it or just set the variable?
	mov x0, IPL_NONE
	bl spllower
	mov x0, x20
	blr x19
	b syscall_return

	
