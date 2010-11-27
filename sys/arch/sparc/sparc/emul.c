/*	$OpenBSD: emul.c,v 1.5 2010/11/27 19:41:48 miod Exp $	*/
/*	$NetBSD: emul.c,v 1.3 1997/07/29 09:42:01 fair Exp $	*/

/*
 * Copyright (c) 1997 Christos Zoulas.  All rights reserved.
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
 *	This product includes software developed by Christos Zoulas.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <machine/reg.h>
#include <machine/instr.h>
#include <machine/cpu.h>
#include <machine/psl.h>
#include <sparc/sparc/cpuvar.h>

#ifdef DEBUG_EMUL
# define DPRINTF(a) uprintf a
#else
# define DPRINTF(a)
#endif

#define GPR(tf, i)	((int32_t *) &tf->tf_global)[i]
#define IPR(tf, i)	((int32_t *) tf->tf_out[6])[i - 16]
#define FPR(p, i)	((int32_t) p->p_md.md_fpstate->fs_regs[i])
#define FPRSET(p, i, v)	p->p_md.md_fpstate->fs_regs[i] = (v)

static __inline int readgpreg(struct trapframe *, int, void *);
static __inline int readfpreg(struct proc *, int, void *);
static __inline int writegpreg(struct trapframe *, int, const void *);
static __inline int writefpreg(struct proc *, int, const void *);
static __inline int decodeaddr(struct trapframe *, union instr *, void *);
static int muldiv(struct trapframe *, union instr *, int32_t *, int32_t *,
    int32_t *);

#define	REGNAME(i)	"goli"[i >> 3], i & 7


static __inline int
readgpreg(tf, i, val)
	struct trapframe *tf;
	int i;
	void *val;
{
	int error = 0;
	if (i == 0)
		*(int32_t *) val = 0;
	else if (i < 16)
		*(int32_t *) val = GPR(tf, i);
	else
		error = copyin(&IPR(tf, i), val, sizeof(int32_t));

	return error;
}

		
static __inline int
writegpreg(tf, i, val)
	struct trapframe *tf;
	int i;
	const void *val;
{
	int error = 0;

	if (i == 0)
		return error;
	else if (i < 16)
		GPR(tf, i) = *(int32_t *) val;
	else
		/* XXX: Fix copyout prototype */
		error = copyout((caddr_t) val, &IPR(tf, i), sizeof(int32_t));

	return error;
}
	

static __inline int
readfpreg(p, i, val)
	struct proc *p;
	int i;
	void *val;
{
	*(int32_t *) val = FPR(p, i);
	return 0;
}

		
static __inline int
writefpreg(p, i, val)
	struct proc *p;
	int i;
	const void *val;
{
	FPRSET(p, i, *(const int32_t *) val);
	return 0;
}

static __inline int
decodeaddr(tf, code, val)
	struct trapframe *tf;
	union instr *code;
	void *val;
{
	if (code->i_simm13.i_i)
		*((int32_t *) val) = code->i_simm13.i_simm13;
	else {
		int error;

		if (code->i_asi.i_asi)
			return EINVAL;
		if ((error = readgpreg(tf, code->i_asi.i_rs2, val)) != 0)
			return error;
	}
	return 0;
}


static int
muldiv(tf, code, rd, rs1, rs2)
	struct trapframe *tf;
	union instr *code;
	int32_t *rd, *rs1, *rs2;
{
	/*
	 * We check for {S,U}{MUL,DIV}{,cc}
	 *
	 * [c = condition code, s = sign]
	 * Mul = 0c101s
	 * Div = 0c111s
	 */
	union {
		struct {
			unsigned unused:26;	/* padding */
			unsigned zero:1;	/* zero by opcode */
			unsigned cc:1;		/* one to send condition code */
			unsigned one1:1;	/* one by opcode */
			unsigned div:1;		/* one if divide */
			unsigned one2:1;	/* one by opcode */
			unsigned sgn:1;		/* sign bit */
		} bits;
		int num;
	} op;

	op.num = code->i_op3.i_op3;

#ifdef DEBUG_EMUL
	uprintf("muldiv 0x%x: %c%s%s %c%d, %c%d, ", code->i_int,
	    "us"[op.bits.sgn], op.bits.div ? "div" : "mul",
	    op.bits.cc ? "cc" : "", REGNAME(code->i_op3.i_rd),
	    REGNAME(code->i_op3.i_rs1));
	if (code->i_loadstore.i_i)
		uprintf("0x%x\n", *rs2);
	else
		uprintf("%c%d\n", REGNAME(code->i_asi.i_rs2));
#endif

	if (op.bits.div) {
		if (*rs2 == 0) {
			/*
			 * XXX: to be 100% correct here, on sunos we need to
			 *	ignore the error and return *rd = *rs1.
			 *	It should be easy to fix by passing struct
			 *	proc in here.
			 */
			DPRINTF(("emulinstr: avoid zerodivide\n"));
			return EINVAL;
		}
		*rd = *rs1 / *rs2;
		DPRINTF(("muldiv: %d / %d = %d\n", *rs1, *rs2, *rd));
	}
	else {
		*rd = *rs1 * *rs2;
		DPRINTF(("muldiv: %d * %d = %d\n", *rs1, *rs2, *rd));
	}

	if (op.bits.cc) {
		/* Set condition codes */
		tf->tf_psr &= ~PSR_ICC;

		if (*rd == 0)
			tf->tf_psr |= PSR_Z << 20;
		else {
			if (op.bits.sgn && *rd < 0)
				tf->tf_psr |= PSR_N << 20;
			if (op.bits.div) {
				if (*rd * *rs2 != *rs1)
					tf->tf_psr |= PSR_O << 20;
			}
			else {
				if (*rd / *rs2 != *rs1)
					tf->tf_psr |= PSR_O << 20;
			}
		}
	}

	return 0;
}

/*
 * Emulate unimplemented instructions on earlier sparc chips.
 */
int
emulinstr(pc, tf)
	int pc;
	struct trapframe *tf;
{
	union instr code;
	int32_t rs1, rs2, rd;
	int error;

	/* fetch and check the instruction that caused the fault */
	error = copyin((caddr_t) pc, &code.i_int, sizeof(code.i_int));
	if (error != 0) {
		DPRINTF(("emulinstr: Bad instruction fetch\n"));
		return SIGILL;
	}

	/* Only support format 2 */
	if (code.i_any.i_op != 2) {
		DPRINTF(("emulinstr: Not a format 2 instruction\n"));
		return SIGILL;
	}

	write_user_windows();

	if ((error = readgpreg(tf, code.i_op3.i_rs1, &rs1)) != 0) {
		DPRINTF(("emulinstr: read rs1 %d\n", error));
		return SIGILL;
	}

	if ((error = decodeaddr(tf, &code, &rs2)) != 0) {
		DPRINTF(("emulinstr: decode addr %d\n", error));
		return SIGILL;
	}

	switch (code.i_op3.i_op3) {
	case IOP3_FLUSH:
		cpuinfo.cache_flush((caddr_t)(rs1 + rs2), 4); /*XXX*/
		return 0;

	default:
		if ((code.i_op3.i_op3 & 0x2a) != 0xa) {
			DPRINTF(("emulinstr: Unsupported op3 0x%x\n",
			    code.i_op3.i_op3));
			return SIGILL;
		}
		else if ((error = muldiv(tf, &code, &rd, &rs1, &rs2)) != 0)
			return SIGFPE;
	}

	if ((error = writegpreg(tf, code.i_op3.i_rd, &rd)) != 0) {
		DPRINTF(("muldiv: write rd %d\n", error));
		return SIGILL;
	}

	return 0;
}
