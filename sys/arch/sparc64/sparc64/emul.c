/*	$OpenBSD: emul.c,v 1.17 2010/01/01 23:19:20 kettenis Exp $	*/
/*	$NetBSD: emul.c,v 1.8 2001/06/29 23:58:40 eeh Exp $	*/

/*-
 * Copyright (c) 1997, 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Christos Zoulas.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/signalvar.h>
#include <sys/malloc.h>
#include <machine/reg.h>
#include <machine/instr.h>
#include <machine/cpu.h>
#include <machine/psl.h>
#include <uvm/uvm_extern.h>

#ifdef DEBUG_EMUL
# define DPRINTF(a) printf a
#else
# define DPRINTF(a)
#endif

#define GPR(tf, i)	((int32_t *)(u_long)&tf->tf_global)[i]
#define IPR(tf, i)	((int32_t *)(u_long)tf->tf_out[6])[i - 16]
#define FPR(p, i)	((int32_t) p->p_md.md_fpstate->fs_regs[i])

static __inline int readgpreg(struct trapframe64 *, int, void *);
static __inline int readfpreg(struct proc *, int, void *);
static __inline int writegpreg(struct trapframe64 *, int, const void *);
static __inline int writefpreg(struct proc *, int, const void *);
static __inline int decodeaddr(struct trapframe64 *, union instr *, void *);
static int muldiv(struct trapframe64 *, union instr *, int32_t *, int32_t *,
    int32_t *);
void swap_quad(int64_t *);

#define	REGNAME(i)	"goli"[i >> 3], i & 7


static __inline int
readgpreg(tf, i, val)
	struct trapframe64 *tf;
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
	struct trapframe64 *tf;
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
	FPR(p, i) = *(const int32_t *) val;
	return 0;
}

static __inline int
decodeaddr(tf, code, val)
	struct trapframe64 *tf;
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
	struct trapframe64 *tf;
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
	printf("muldiv 0x%x: %c%s%s %c%d, %c%d, ", code->i_int,
	    "us"[op.bits.sgn], op.bits.div ? "div" : "mul",
	    op.bits.cc ? "cc" : "", REGNAME(code->i_op3.i_rd),
	    REGNAME(code->i_op3.i_rs1));
	if (code->i_loadstore.i_i)
		printf("0x%x\n", *rs2);
	else
		printf("%c%d\n", REGNAME(code->i_asi.i_rs2));
#endif

	if (op.bits.div) {
		if (*rs2 == 0) {
			/*
			 * XXX: to be 100% correct here, on sunos we need to
			 *	ignore the error and return *rd = *rs1.
			 *	It should be easy to fix by passing struct
			 *	proc in here.
			 */
			DPRINTF(("muldiv: avoid zerodivide\n"));
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
		tf->tf_tstate &= ~(TSTATE_CCR);

		if (*rd == 0)
			tf->tf_tstate |= (u_int64_t)(ICC_Z|XCC_Z) << TSTATE_CCR_SHIFT;
		else {
			if (op.bits.sgn && *rd < 0)
				tf->tf_tstate |= (u_int64_t)(ICC_N|XCC_N) << TSTATE_CCR_SHIFT;
			if (op.bits.div) {
				if (*rd * *rs2 != *rs1)
					tf->tf_tstate |= (u_int64_t)(ICC_V|XCC_V) << TSTATE_CCR_SHIFT;
			}
			else {
				if (*rd / *rs2 != *rs1)
					tf->tf_tstate |= (u_int64_t)(ICC_V|XCC_V) << TSTATE_CCR_SHIFT;
			}
		}
	}

	return 0;
}

/*
 * Code to handle alignment faults on the sparc. This is enabled by sending
 * a fixalign trap. Such code is generated by compiling with cc -misalign
 * on SunOS, but we don't have such a feature yet on our gcc.
 */

int
fixalign(p, tf)
	struct proc *p;
	struct trapframe64 *tf;
{
	static u_char sizedef[] = { 0x4, 0xff, 0x2, 0x8 };

	/*
	 * This is particular to load and store instructions
	 */
	union {
		struct {
			unsigned unused:26;	/* 26 padding */
			unsigned fl:1;		/* 1 bit float flag */
			unsigned op:1;		/* 1 bit opcode */
			unsigned sgn:1;		/* 1 bit sign */
			unsigned st:1;		/* 1 bit load/store */
			unsigned sz:2;		/* 2 bit size register */
		} bits;
		int num;
	} op;

	union {
		double	d;
		int32_t i[2];
		int16_t s[4];
		int8_t  c[8];
	} data;

	union instr code;
	size_t size;
	int64_t rs1, rs2;
	int error;

	/* fetch and check the instruction that caused the fault */
	error = copyin((caddr_t)(u_long)tf->tf_pc, &code.i_int, sizeof(code.i_int));
	if (error != 0) {
		DPRINTF(("fixalign: Bad instruction fetch\n"));
		return EINVAL;
	}

	/* Only support format 3 */
	if (code.i_any.i_op != 3) {
		DPRINTF(("fixalign: Not a load or store\n"));
		return EINVAL;
	}

	op.num = code.i_loadstore.i_op3;

	/* Check operand size */
	if ((size = sizedef[op.bits.sz]) == 0xff) {
		DPRINTF(("fixalign: Bad operand size\n"));
		return EINVAL;
	}

	write_user_windows();

	if ((error = readgpreg(tf, code.i_op3.i_rs1, &rs1)) != 0) {
		DPRINTF(("fixalign: read rs1 %d\n", error));
		return error;
	}

	if ((error = decodeaddr(tf, &code, &rs2)) != 0) {
		DPRINTF(("fixalign: decode addr %d\n", error));
		return error;
	}


	rs1 += rs2;

#ifdef DEBUG_EMUL
	printf("memalign 0x%x: %s%c%c %c%d, %c%d, ", code.i_int,
	    op.bits.st ? "st" : "ld", "us"[op.bits.sgn],
	    "w*hd"[op.bits.sz], op.bits.fl ? 'f' : REGNAME(code.i_op3.i_rd),
	    REGNAME(code.i_op3.i_rs1));
	if (code.i_loadstore.i_i)
		printf("0x%llx\n", (unsigned long long)rs2);
	else
		printf("%c%d\n", REGNAME(code.i_asi.i_rs2));
#endif
#ifdef DIAGNOSTIC
	if (op.bits.fl && p != fpproc)
		panic("fp align without being the FP owning process");
#endif

	if (op.bits.st) {
		if (op.bits.fl) {
			fpusave_proc(p, 1);

			error = readfpreg(p, code.i_op3.i_rd, &data.i[0]);
			if (error)
				return error;
			if (size == 8) {
				error = readfpreg(p, code.i_op3.i_rd + 1,
				    &data.i[1]);
				if (error)
					return error;
			}
		}
		else {
			error = readgpreg(tf, code.i_op3.i_rd, &data.i[0]);
			if (error)
				return error;
			if (size == 8) {
				error = readgpreg(tf, code.i_op3.i_rd + 1,
				    &data.i[1]);
				if (error)
					return error;
			}
		}

		if (size == 2)
			return copyout(&data.s[1], (caddr_t)(u_long)rs1, size);
		else
			return copyout(&data.d, (caddr_t)(u_long)rs1, size);
	}
	else { /* load */
		if (size == 2) {
			error = copyin((caddr_t)(u_long)rs1, &data.s[1], size);
			if (error)
				return error;

			/* Sign extend if necessary */
			if (op.bits.sgn && (data.s[1] & 0x8000) != 0)
				data.s[0] = ~0;
			else
				data.s[0] = 0;
		}
		else
			error = copyin((caddr_t)(u_long)rs1, &data.d, size);

		if (error)
			return error;

		if (op.bits.fl) {
			error = writefpreg(p, code.i_op3.i_rd, &data.i[0]);
			if (error)
				return error;
			if (size == 8) {
				error = writefpreg(p, code.i_op3.i_rd + 1,
				    &data.i[1]);
				if (error)
					return error;
			}
			loadfpstate(p->p_md.md_fpstate);
			fpproc = p;
		}
		else {
			error = writegpreg(tf, code.i_op3.i_rd, &data.i[0]);
			if (error)
				return error;
			if (size == 8)
				error = writegpreg(tf, code.i_op3.i_rd + 1,
				    &data.i[1]);
		}
	}
	return error;
}

/*
 * Emulate unimplemented instructions on earlier sparc chips.
 */
int
emulinstr(pc, tf)
	vaddr_t pc;
	struct trapframe64 *tf;
{
	union instr code;
	int32_t rs1, rs2, rd;
	int error;

	/* fetch and check the instruction that caused the fault */
	error = copyin((caddr_t) pc, &code.i_int, sizeof(code.i_int));
	if (error != 0) {
		DPRINTF(("emulinstr: Bad instruction fetch\n"));
		return (SIGILL);
	}

	/* Only support format 2 */
	if (code.i_any.i_op != 2) {
		DPRINTF(("emulinstr: Not a format 2 instruction\n"));
		return (SIGILL);
	}

	write_user_windows();

	if ((error = readgpreg(tf, code.i_op3.i_rs1, &rs1)) != 0) {
		DPRINTF(("emulinstr: read rs1 %d\n", error));
		return (SIGILL);
	}

	if ((error = decodeaddr(tf, &code, &rs2)) != 0) {
		DPRINTF(("emulinstr: decode addr %d\n", error));
		return (SIGILL);
	}

	switch (code.i_op3.i_op3) {
	case IOP3_FLUSH:
/*		cpuinfo.cache_flush((caddr_t)(rs1 + rs2), 4); XXX */
		return (0);

	default:
		if ((code.i_op3.i_op3 & 0x2a) != 0xa) {
			DPRINTF(("emulinstr: Unsupported op3 0x%x\n",
			    code.i_op3.i_op3));
			return (SIGILL);
		}
		else if ((error = muldiv(tf, &code, &rd, &rs1, &rs2)) != 0)
			return (SIGFPE);
	}

	if ((error = writegpreg(tf, code.i_op3.i_rd, &rd)) != 0) {
		DPRINTF(("muldiv: write rd %d\n", error));
		return (SIGILL);
	}

	return (0);
}

#define	SIGN_EXT13(v)	(((int64_t)(v) << 51) >> 51)

void
swap_quad(int64_t *p)
{
	int64_t t;

	t = htole64(p[0]);
	p[0] = htole64(p[1]);
	p[1] = t;
}

/*
 * emulate STQF, STQFA, LDQF, and LDQFA
 */
int
emul_qf(int32_t insv, struct proc *p, union sigval sv, struct trapframe *tf)
{
	extern struct fpstate64 initfpstate;
	struct fpstate64 *fs = p->p_md.md_fpstate;
	int64_t addr, buf[2];
	union instr ins;
	int freg, isload, err;
	u_int8_t asi;

	ins.i_int = insv;
	freg = ins.i_op3.i_rd & ~1;
	freg |= (ins.i_op3.i_rd & 1) << 5;

	if (ins.i_op3.i_op3 == IOP3_LDQF || ins.i_op3.i_op3 == IOP3_LDQFA)
		isload = 1;
	else
		isload = 0;

	if (ins.i_op3.i_op3 == IOP3_STQF || ins.i_op3.i_op3 == IOP3_LDQF)
		asi = ASI_PRIMARY;
	else if (ins.i_loadstore.i_i)
		asi = (tf->tf_tstate & TSTATE_ASI) >> TSTATE_ASI_SHIFT;
	else
		asi = ins.i_asi.i_asi;

	addr = tf->tf_global[ins.i_asi.i_rs1];
	if (ins.i_loadstore.i_i)
		addr += SIGN_EXT13(ins.i_simm13.i_simm13);
	else
		addr += tf->tf_global[ins.i_asi.i_rs2];

	if (asi < ASI_PRIMARY) {
		/* privileged asi */
		KERNEL_PROC_LOCK(p);
		trapsignal(p, SIGILL, 0, ILL_PRVOPC, sv);
		KERNEL_PROC_UNLOCK(p);
		return (0);
	}
	if (asi > ASI_SECONDARY_NOFAULT_LITTLE ||
	    (asi > ASI_SECONDARY_NOFAULT && asi < ASI_PRIMARY_LITTLE)) {
		/* architecturally undefined user ASI's */
		goto segv;
	}

	if ((freg & 3) != 0) {
		/* only valid for %fN where N % 4 = 0 */
		KERNEL_PROC_LOCK(p);
		trapsignal(p, SIGILL, 0, ILL_ILLOPN, sv);
		KERNEL_PROC_UNLOCK(p);
		return (0);
	}

	if ((p->p_md.md_flags & MDP_FIXALIGN) == 0 && (addr & 3) != 0) {
		/*
		 * If process doesn't want us to fix alignment and the
		 * request isn't aligned, kill it.
		 */
		KERNEL_PROC_LOCK(p);
		trapsignal(p, SIGBUS, 0, BUS_ADRALN, sv);
		KERNEL_PROC_UNLOCK(p);
		return (0);
	}

	fs = p->p_md.md_fpstate;
	if (fs == NULL) {
		KERNEL_PROC_LOCK(p);
		/* don't currently have an fpu context, get one */
		fs = malloc(sizeof(*fs), M_SUBPROC, M_WAITOK);
		*fs = initfpstate;
		fs->fs_qsize = 0;
		p->p_md.md_fpstate = fs;
		KERNEL_PROC_UNLOCK(p);
	} else
		fpusave_proc(p, 1);

	/* Ok, try to do the actual operation (finally) */
	if (isload) {
		err = copyin((caddr_t)addr, buf, sizeof(buf));
		if (err != 0 && (asi & 2) == 0)
			goto segv;
		if (err == 0) {
			if (asi & 8)
				swap_quad(buf);
			bcopy(buf, &fs->fs_regs[freg], sizeof(buf));
		}
	} else {
		bcopy(&fs->fs_regs[freg], buf, sizeof(buf));
		if (asi & 8)
			swap_quad(buf);
		if (copyout(buf, (caddr_t)addr, sizeof(buf)) && (asi & 2) == 0)
			goto segv;
	}

	return (1);

segv:
	KERNEL_PROC_LOCK(p);
	trapsignal(p, SIGSEGV, isload ? VM_PROT_READ : VM_PROT_WRITE,
	    SEGV_MAPERR, sv);
	KERNEL_PROC_UNLOCK(p);
	return (0);
}

int
emul_popc(int32_t insv, struct proc *p, union sigval sv, struct trapframe *tf)
{
	u_int64_t val, ret = 0;
	union instr ins;

	ins.i_int = insv;
	if (ins.i_simm13.i_i == 0)
		val = tf->tf_global[ins.i_asi.i_rs2];
	else
		val = SIGN_EXT13(ins.i_simm13.i_simm13);

	for (; val != 0; val >>= 1)
		ret += val & 1;

	tf->tf_global[ins.i_asi.i_rd] = ret;
	return (1);
}
