/*	$OpenBSD: cpustate.h,v 1.3 2004/08/10 21:10:56 pefo Exp $ */

/*
 * Copyright (c) 2002-2003 Opsycon AB  (www.opsycon.se / www.opsycon.com)
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
 */

#define	KERN_REG_SIZE		(NUMSAVEREGS * REGSZ)
#define	KERN_EXC_FRAME_SIZE	(CF_SZ + KERN_REG_SIZE + 16)

#define	SAVE_REG(reg, offs, base, bo) \
	REG_S	reg, bo + (REGSZ * offs) (base)

#define	RESTORE_REG(reg, offs, base, bo) \
	REG_L	reg, bo + (REGSZ * offs) (base)

/*
 *  This macro saves the 'scratch' cpu state on stack.
 *  Macros are generic so no 'special' instructions!
 *  a0 will have a pointer to the 'frame' on return.
 *  a1 will have saved STATUS_REG on return.
 *  a3 will have the exception pc on 'return'.
 *  No traps, no interrupts if frame = k1 or k0!
 */
#define	SAVE_CPU(frame, bo)			 \
	SAVE_REG(AT, AST, frame, bo)		;\
	SAVE_REG(v0, V0, frame, bo)		;\
	SAVE_REG(v1, V1, frame, bo)		;\
	SAVE_REG(a0, A0, frame, bo)		;\
	SAVE_REG(a1, A1, frame, bo)		;\
	SAVE_REG(a2, A2, frame, bo)		;\
	SAVE_REG(a3, A3, frame, bo)		;\
	SAVE_REG(t0, T0, frame, bo)		;\
	SAVE_REG(t1, T1, frame, bo)		;\
	SAVE_REG(t2, T2, frame, bo)		;\
	SAVE_REG(t3, T3, frame, bo)		;\
	SAVE_REG(t4, T4, frame, bo)		;\
	SAVE_REG(t5, T5, frame, bo)		;\
	SAVE_REG(t6, T6, frame, bo)		;\
	SAVE_REG(t7, T7, frame, bo)		;\
	SAVE_REG(t8, T8, frame, bo)		;\
	SAVE_REG(t9, T9, frame, bo)		;\
	SAVE_REG(gp, GP, frame, bo)		;\
	SAVE_REG(ra, RA, frame, bo)		;\
	mflo	v0				;\
	mfhi	v1				;\
	mfc0	a0, COP_0_CAUSE_REG		;\
	mfc0	a1, COP_0_STATUS_REG		;\
	mfc0	a2, COP_0_BAD_VADDR		;\
	mfc0	a3, COP_0_EXC_PC		;\
	SAVE_REG(v0, MULLO, frame, bo)		;\
	SAVE_REG(v1, MULHI, frame, bo)		;\
	SAVE_REG(a0, CAUSE, frame, bo)		;\
	SAVE_REG(a1, SR, frame, bo)		;\
	SAVE_REG(a2, BADVADDR, frame, bo)	;\
	SAVE_REG(a3, PC, frame, bo)		;\
	SAVE_REG(sp, SP, frame, bo)		;\
	addu	a0, frame, bo			;\
	lw	a2, cpl				;\
	SAVE_REG(a2, CPL, frame, bo)

/*
 *  Save 'callee save' registers in frame to aid DDB.
 */
#define	SAVE_CPU_SREG(frame, bo)		 \
	SAVE_REG(s0, S0, frame, bo)		;\
	SAVE_REG(s1, S1, frame, bo)		;\
	SAVE_REG(s2, S2, frame, bo)		;\
	SAVE_REG(s3, S3, frame, bo)		;\
	SAVE_REG(s4, S4, frame, bo)		;\
	SAVE_REG(s5, S5, frame, bo)		;\
	SAVE_REG(s6, S6, frame, bo)		;\
	SAVE_REG(s7, S7, frame, bo)		;\
	SAVE_REG(s8, S8, frame, bo)

/*
 *  Restore cpu state. When called a0 = EXC_PC.
 */
#define	RESTORE_CPU(frame, bo)			 \
	RESTORE_REG(t1, SR, frame, bo)		;\
	RESTORE_REG(t2, MULLO, frame, bo)	;\
	RESTORE_REG(t3, MULHI, frame, bo)	;\
	mtc0	t1, COP_0_STATUS_REG		;\
	mtlo	t2				;\
	mthi	t3				;\
	dmtc0	a0, COP_0_EXC_PC		;\
	RESTORE_REG(AT, AST, frame, bo)		;\
	RESTORE_REG(v0, V0, frame, bo)		;\
	RESTORE_REG(v1, V1, frame, bo)		;\
	RESTORE_REG(a0, A0, frame, bo)		;\
	RESTORE_REG(a1, A1, frame, bo)		;\
	RESTORE_REG(a2, A2, frame, bo)		;\
	RESTORE_REG(a3, A3, frame, bo)		;\
	RESTORE_REG(t0, T0, frame, bo)		;\
	RESTORE_REG(t1, T1, frame, bo)		;\
	RESTORE_REG(t2, T2, frame, bo)		;\
	RESTORE_REG(t3, T3, frame, bo)		;\
	RESTORE_REG(t4, T4, frame, bo)		;\
	RESTORE_REG(t5, T5, frame, bo)		;\
	RESTORE_REG(t6, T6, frame, bo)		;\
	RESTORE_REG(t7, T7, frame, bo)		;\
	RESTORE_REG(t8, T8, frame, bo)		;\
	RESTORE_REG(t9, T9, frame, bo)		;\
	RESTORE_REG(gp, GP, frame, bo)		;\
	RESTORE_REG(ra, RA, frame, bo)

/*
 *  Restore 'callee save' registers
 */
#define	RESTORE_CPU_SREG(frame, bo)		 \
	RESTORE_REG(s0, S0, frame, bo)		;\
	RESTORE_REG(s1, S1, frame, bo)		;\
	RESTORE_REG(s2, S2, frame, bo)		;\
	RESTORE_REG(s3, S3, frame, bo)		;\
	RESTORE_REG(s4, S4, frame, bo)		;\
	RESTORE_REG(s5, S5, frame, bo)		;\
	RESTORE_REG(s6, S6, frame, bo)		;\
	RESTORE_REG(s7, S7, frame, bo)		;\
	RESTORE_REG(s8, S8, frame, bo)

