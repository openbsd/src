/*
 * Copyright (c) 2020 Mengshi Li <mengshi.li.mars@gmail.com> 
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

#include "assym.h"
#include <machine/asm.h>
#include <machine/trap.h>
#include <machine/riscvreg.h>

.macro save_registers el
	addi	sp, sp, -(TRAPFRAME_SIZEOF)

	sd	ra, (TF_RA)(sp)

.if \el == 0	/* We came from userspace. */
	sd	gp, (TF_GP)(sp)
.option push
.option norelax
	/* Load the kernel's global pointer */
	la	gp, __global_pointer$
.option pop

	/* Load our pcpu */
	sd	tp, (TF_TP)(sp)
	ld	tp, (TRAPFRAME_SIZEOF)(sp)
.endif

	sd	t0, (TF_T + 0 * 8)(sp)
	sd	t1, (TF_T + 1 * 8)(sp)
	sd	t2, (TF_T + 2 * 8)(sp)
	sd	t3, (TF_T + 3 * 8)(sp)
	sd	t4, (TF_T + 4 * 8)(sp)
	sd	t5, (TF_T + 5 * 8)(sp)
	sd	t6, (TF_T + 6 * 8)(sp)

	sd	s0, (TF_S + 0 * 8)(sp)
	sd	s1, (TF_S + 1 * 8)(sp)
	sd	s2, (TF_S + 2 * 8)(sp)
	sd	s3, (TF_S + 3 * 8)(sp)
	sd	s4, (TF_S + 4 * 8)(sp)
	sd	s5, (TF_S + 5 * 8)(sp)
	sd	s6, (TF_S + 6 * 8)(sp)
	sd	s7, (TF_S + 7 * 8)(sp)
	sd	s8, (TF_S + 8 * 8)(sp)
	sd	s9, (TF_S + 9 * 8)(sp)
	sd	s10, (TF_S + 10 * 8)(sp)
	sd	s11, (TF_S + 11 * 8)(sp)

	sd	a0, (TF_A + 0 * 8)(sp)
	sd	a1, (TF_A + 1 * 8)(sp)
	sd	a2, (TF_A + 2 * 8)(sp)
	sd	a3, (TF_A + 3 * 8)(sp)
	sd	a4, (TF_A + 4 * 8)(sp)
	sd	a5, (TF_A + 5 * 8)(sp)
	sd	a6, (TF_A + 6 * 8)(sp)
	sd	a7, (TF_A + 7 * 8)(sp)

.if \el == 1
	/* Store kernel sp */
	li	t1, TRAPFRAME_SIZEOF
	add	t0, sp, t1
	sd	t0, (TF_SP)(sp)
.else
	/* Store user sp */
	csrr	t0, sscratch
	sd	t0, (TF_SP)(sp)
.endif
	li	t0, 0
	csrw	sscratch, t0
	csrr	t0, sepc
	sd	t0, (TF_SEPC)(sp)
	csrr	t0, sstatus
	sd	t0, (TF_SSTATUS)(sp)
	csrr	t0, stval
	sd	t0, (TF_STVAL)(sp)
	csrr	t0, scause
	sd	t0, (TF_SCAUSE)(sp)
.endm

.macro restore_registers el
	ld	t0, (TF_SSTATUS)(sp)
.if \el == 0
	/* Ensure user interrupts will be enabled on eret */
	li	t1, SSTATUS_SPIE
	or	t0, t0, t1
.else
	/*
	 * Disable interrupts for supervisor mode exceptions.
	 * For user mode exceptions we have already done this
	 * in do_ast.
	 */
	li	t1, ~SSTATUS_SIE
	and	t0, t0, t1
.endif
	csrw	sstatus, t0

	ld	t0, (TF_SEPC)(sp)
	csrw	sepc, t0

.if \el == 0
	/* We go to userspace. Load user sp */
	ld	t0, (TF_SP)(sp)
	csrw	sscratch, t0

	/* Store our pcpu */
	sd	tp, (TRAPFRAME_SIZEOF)(sp)
	ld	tp, (TF_TP)(sp)

	/* And restore the user's global pointer */
	ld	gp, (TF_GP)(sp)
.endif

	ld	ra, (TF_RA)(sp)

	ld	t0, (TF_T + 0 * 8)(sp)
	ld	t1, (TF_T + 1 * 8)(sp)
	ld	t2, (TF_T + 2 * 8)(sp)
	ld	t3, (TF_T + 3 * 8)(sp)
	ld	t4, (TF_T + 4 * 8)(sp)
	ld	t5, (TF_T + 5 * 8)(sp)
	ld	t6, (TF_T + 6 * 8)(sp)

	ld	s0, (TF_S + 0 * 8)(sp)
	ld	s1, (TF_S + 1 * 8)(sp)
	ld	s2, (TF_S + 2 * 8)(sp)
	ld	s3, (TF_S + 3 * 8)(sp)
	ld	s4, (TF_S + 4 * 8)(sp)
	ld	s5, (TF_S + 5 * 8)(sp)
	ld	s6, (TF_S + 6 * 8)(sp)
	ld	s7, (TF_S + 7 * 8)(sp)
	ld	s8, (TF_S + 8 * 8)(sp)
	ld	s9, (TF_S + 9 * 8)(sp)
	ld	s10, (TF_S + 10 * 8)(sp)
	ld	s11, (TF_S + 11 * 8)(sp)

	ld	a0, (TF_A + 0 * 8)(sp)
	ld	a1, (TF_A + 1 * 8)(sp)
	ld	a2, (TF_A + 2 * 8)(sp)
	ld	a3, (TF_A + 3 * 8)(sp)
	ld	a4, (TF_A + 4 * 8)(sp)
	ld	a5, (TF_A + 5 * 8)(sp)
	ld	a6, (TF_A + 6 * 8)(sp)
	ld	a7, (TF_A + 7 * 8)(sp)

	addi	sp, sp, (TRAPFRAME_SIZEOF)
.endm

.macro	do_ast
	/* Disable interrupts */
	csrr	a4, sstatus
1:
	csrci	sstatus, (SSTATUS_SIE)

	/* Check for astpending */
	ld	a1, CI_CURPROC(tp)
	beqz	a1, 2f
	lw	a2, P_ASTPENDING(a1)
	beqz	a2, 2f

	sw	x0, P_ASTPENDING(a1)

	/* Restore interrupts */
	csrw	sstatus, a4

	/* handle the ast */
	mv	a0, sp
	la	t0, _C_LABEL(ast)
	jalr	t0
	j	1b
2:
.endm

ENTRY(cpu_trap_handler)
	csrrw	sp, sscratch, sp
	beqz	sp, 1f
	/* User mode detected */
	j	cpu_trap_handler_user
1:
	/* Supervisor mode detected */
	csrrw	sp, sscratch, sp
	j	cpu_trap_handler_supervisor
END(cpu_trap_handler)

ENTRY(cpu_trap_handler_supervisor)
	save_registers 1
	mv	a0, sp
	call	_C_LABEL(do_trap_supervisor)
	restore_registers 1
	sret
END(cpu_trap_handler_supervisor)

ENTRY(cpu_trap_handler_user)
	save_registers 0
	mv	a0, sp
	call	_C_LABEL(do_trap_user)
	do_ast
	restore_registers 0
	csrrw	sp, sscratch, sp
	sret
END(cpu_trap_handler_user)

ENTRY(syscall_return)
	do_ast
	restore_registers 0
	csrrw	sp, sscratch, sp
	sret
END(syscall_return)
