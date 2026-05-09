/*	$OpenBSD: vector.c,v 1.1 2026/05/09 17:38:50 jsing Exp $	*/

/*
 * Copyright (c) 2026 Joel Sing <jsing@openbsd.org>
 * Copyright (c) 2020 Dale Rahn <drahn@openbsd.org>
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/user.h>

#define CSR_VSTART	0x008
#define CSR_VXSAT	0x009
#define CSR_VXRM	0x00a
#define CSR_VCSR	0x00f
#define CSR_VL		0xc20
#define CSR_VTYPE	0xc21
#define CSR_VLENB	0xc22

int
vector_instruction(register_t stval)
{
	register_t opcode, funct3, csr;

	/*
	 * Indicate whether the instruction belongs to a vector extension,
	 * or is a CSR instruction that references a vector CSR register.
	 */
	opcode = stval & 0x7f;
	funct3 = (stval >> 12) & 0x7;

	/* LOAD-FP for V. */
	if (opcode == 0b0000111 && (funct3 == 0b000 || funct3 == 0b101 ||
	    funct3 == 0b110 || funct3 == 0b111))
		return 1;

	/* STORE-FP for V. */
	if (opcode == 0b0100111 && (funct3 == 0b000 || funct3 == 0b101 ||
	    funct3 == 0b110 || funct3 == 0b111))
		return 1;

	/* OP-V. */
	if (opcode == 0b1010111)
		return 1;

	/* CSR instruction with vector CSR register. */
	if (opcode == 0b1110011 && (funct3 == 0b001 || funct3 == 0b010 ||
	    funct3 == 0b011 || funct3 == 0b101 || funct3 == 0b110 ||
	    funct3 == 0b111)) {
		csr = (stval >> 20) & 0xfff;
		return csr == CSR_VSTART || csr == CSR_VXSAT ||
		    csr == CSR_VXRM || csr == CSR_VCSR || csr == CSR_VL ||
		    csr == CSR_VTYPE || csr == CSR_VLENB;
	}

	return 0;
}

void
vector_disable(void)
{
	__asm volatile ("csrc sstatus, %0" :: "r"(SSTATUS_VS_MASK));
}

void
vector_enable_clean(void)
{
	__asm volatile ("csrc sstatus, %0" :: "r"(SSTATUS_VS_MASK));
	__asm volatile ("csrs sstatus, %0" :: "r"(SSTATUS_VS_CLEAN));
}

void
vector_save(struct proc *p, struct trapframe *tf)
{
	struct pcb *pcb = &p->p_addr->u_pcb;
	struct vreg *v;

	if ((tf->tf_sstatus & SSTATUS_VS_MASK) == SSTATUS_VS_OFF ||
	    (tf->tf_sstatus & SSTATUS_VS_MASK) == SSTATUS_VS_CLEAN)
		return;

	v = pcb->pcb_vstate;

	vector_enable_clean();

	__asm volatile ("csrr %0, vtype" : "=r"(v->v_vtype));
	__asm volatile ("csrr %0, vl" : "=r"(v->v_vl));
	__asm volatile ("csrr %0, vstart" : "=r"(v->v_vstart));
	__asm volatile ("csrr %0, vcsr" : "=r"(v->v_vcsr));

	__asm volatile (
	    ".option push \n"
	    ".option arch, +zve32x \n"
	    "vs8r.v v0, (%0) \n"
	    "vs8r.v v8, (%1) \n"
	    "vs8r.v v16, (%2) \n"
	    "vs8r.v v24, (%3) \n"
	    ".option pop \n"
	    : : "r"(&v->v_vdata[0 * riscv_vlenb]),
		"r"(&v->v_vdata[8 * riscv_vlenb]),
		"r"(&v->v_vdata[16 * riscv_vlenb]),
		"r"(&v->v_vdata[24 * riscv_vlenb]) : "memory"
	);

	vector_disable();

	/* Mark vector as disabled. */
	p->p_addr->u_pcb.pcb_tf->tf_sstatus &= ~SSTATUS_VS_MASK;
	p->p_addr->u_pcb.pcb_tf->tf_sstatus |= SSTATUS_VS_OFF;
}

void
vector_load(struct proc *p)
{
	struct pcb *pcb = &p->p_addr->u_pcb;
	struct vreg *v;

	KASSERT((pcb->pcb_tf->tf_sstatus & SSTATUS_VS_MASK) == SSTATUS_VS_OFF);

	if ((pcb->pcb_flags & PCB_VECTOR) == 0) {
		pcb->pcb_vstate = malloc(sizeof(struct vreg) + 32 * riscv_vlenb,
		    M_SUBPROC, M_ZERO | M_WAITOK);
		pcb->pcb_flags |= PCB_VECTOR;
	}

	v = pcb->pcb_vstate;

	vector_enable_clean();

	__asm volatile (
	    ".option push \n"
	    ".option arch, +zve32x \n"
	    "vsetvl x0, %0, %1 \n"
	    "vl8r.v v0, (%2) \n"
	    "vl8r.v v8, (%3) \n"
	    "vl8r.v v16, (%4) \n"
	    "vl8r.v v24, (%5) \n"
	    ".option pop \n"
	    : : "r"(v->v_vl), "r"(v->v_vtype),
		"r"(&v->v_vdata[0 * riscv_vlenb]),
		"r"(&v->v_vdata[8 * riscv_vlenb]),
		"r"(&v->v_vdata[16 * riscv_vlenb]),
		"r"(&v->v_vdata[24 * riscv_vlenb]) : "memory"
	);

	__asm volatile ("csrw vstart, %0" : "=r"(v->v_vstart));
	__asm volatile ("csrw vcsr, %0" : "=r"(v->v_vcsr));

	vector_disable();

	/* Mark vector as clean. */
	p->p_addr->u_pcb.pcb_tf->tf_sstatus &= ~SSTATUS_VS_MASK;
	p->p_addr->u_pcb.pcb_tf->tf_sstatus |= SSTATUS_VS_CLEAN;
}
