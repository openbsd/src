/* $NetBSD: fpe.c,v 1.2 1996/03/18 19:58:09 mark Exp $ */

/*
 * Copyright (c) 1995 Mark Brinicombe.
 * Copyright (c) 1995 Neil Carson.
 * All rights reserved.
 *
 * This code is derived from software written for Brini by Mark Brinicombe
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
 *	This product includes software developed by the RiscBSD kernel team.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * RiscBSD kernel project
 *
 * fpe.c
 *
 * Floating Point Emulator
 *
 * Currently calculations are only performed to single precision
 *
 * Created      : 11/02/95
 */

/* FPE Compile options
 *
 * FPE_SPEEDUPS	- use assembly speedups in place of C routines
 * FPE_NODEBUG	- don't compile in the debugging code
 * FPE_PROF	- compile in FPE profiling code
 * FPE_INLINE	- define this as inline to inline some functions
 */

#define FPE_SPEEDUPS
#define FPE_NODEBUG
/*#define FPE_PROF*/
#define FPE_INLINE __inline

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/syslog.h>

#include <machine/cpu.h>
#include <machine/cpus.h>
#include <machine/katelib.h>
#include <machine/frame.h>
#include <machine/undefined.h>
#include <machine/fp.h>

typedef struct {
	int opcode;
	int precision;
	int rounding;
	int fpregm;
	int fpregn;
	int fpregd;
	int regd;
} fp_op_data_t;

/* Types and stuctures used to hold floating point numbers */

typedef struct {
	u_int32_t exponent;
	u_int32_t mantissa_hi;
	u_int32_t mantissa_lo;
} fpe_reg_t;

typedef struct {
	u_int	mantissa:23;
	u_int	exponent:8;
	u_int	sign:1;
} fpe_sprec_t;

typedef struct {
	u_int	mantissa_hi:20;
	u_int	exponent:11;
	u_int	sign:1;
	u_int32_t	mantissa_lo;
} fpe_dprec_t;

typedef struct {
	u_int32_t exponent;
	u_int32_t mantissa_hi;
	u_int32_t mantissa_lo;
} fpe_eprec_t;

typedef struct {
	u_int32_t word1;
	u_int32_t word2;
	u_int32_t word3;
} fpe_pprec_t;

typedef struct {
	u_int32_t exponent;
	u_int64_t mantissa;
} fpe_qreg_t;

/* FPE status and control registers */

u_int	fpe_status  = 0x02000000;
u_int	fpe_control = 0x00000000;

#define FPE_STATUS_MASK		0x001f1f1f
#define FPE_CONTROL_MASK	0x9cffffff

/* Array containing the emulated registers */

fpe_reg_t fpe_registers[16] = {
	{ 0, 0, 0 }, /* f0 */
	{ 0, 0, 0 }, /* f1 */
	{ 0, 0, 0 }, /* f2 */
	{ 0, 0, 0 }, /* f3 */
	{ 0, 0, 0 }, /* f4 */
	{ 0, 0, 0 }, /* f5 */
	{ 0, 0, 0 }, /* f6 */
	{ 0, 0, 0 }, /* f7 */
	{ 0x0000, 0x00000000, 0x00000000 }, /* 0.0 */
	{ 0x3fff, 0x80000000, 0x00000000 }, /* 1.0 */
	{ 0x4000, 0x80000000, 0x00000000 }, /* 2.0 */
	{ 0x4000, 0xc0000000, 0x00000000 }, /* 3.0 */
	{ 0x4001, 0x80000000, 0x00000000 }, /* 4.0 */
	{ 0x4001, 0xa0000000, 0x00000000 }, /* 5.0 */
	{ 0x3ffe, 0x80000000, 0x00000000 }, /* 0.5 */
	{ 0x4002, 0xa0000000, 0x00000000 }, /* 10.0 */
};

fpe_reg_t *fpregs;

/* Prototypes for instruction emulation functions */ 

typedef int (*fpe_handler_t) __P((unsigned int, trapframe_t *));
typedef int (*fpe_dop_handler_t) __P((fp_op_data_t *, trapframe_t *));

int fpe_mvf __P((fp_op_data_t *, trapframe_t *));
int fpe_mnf __P((fp_op_data_t *, trapframe_t *));
int fpe_adf __P((fp_op_data_t *, trapframe_t *));
int fpe_muf __P((fp_op_data_t *, trapframe_t *));
int fpe_suf __P((fp_op_data_t *, trapframe_t *));
int fpe_dvf __P((fp_op_data_t *, trapframe_t *));
int fpe_muf __P((fp_op_data_t *, trapframe_t *));
int fpe_rdf __P((fp_op_data_t *, trapframe_t *));
int fpe_rsf __P((fp_op_data_t *, trapframe_t *));
int fpe_abs __P((fp_op_data_t *, trapframe_t *));

/* Table of handler functions for register to registers operations */

fpe_dop_handler_t fpe_dops[32] = {
	fpe_adf, /* 00000 ADF Add */
	fpe_mvf, /* 00001 MVF Move */
	fpe_muf, /* 00010 MUL Multiply */
	fpe_mnf, /* 00011 MNF Move negated */
	fpe_suf, /* 00100 SUF Subtract */
	fpe_abs, /* 00101 ABS Absolute value */
	fpe_rsf, /* 00110 RSF Reverse subtract */
	NULL,    /* 00111 RND Round to integer */
	fpe_dvf, /* 01000 DVF Divide */
	NULL,    /* 01001 SQT Square root */
	fpe_rdf, /* 01010 RDF Reverse divide */
	NULL,    /* 01011 LOG Logarithm to base 10 */
	NULL,    /* 01100 POW Power */
	NULL,    /* 01101 LGN Logarithm to base e */
	NULL,    /* 01110 RPW Reverse power */
	NULL,    /* 01111 EXP Exponent */
	NULL,    /* 10000 RMF Remainder */
	NULL,    /* 10001 SIN Sine */
	fpe_muf, /* 10010 FML Fast multiply */
	NULL,    /* 10011 COS Cosine */
	fpe_dvf, /* 10100 FDV Fast divide */
	NULL,    /* 10101 TAN Tangent */
	fpe_rdf, /* 10110 FRD Fast reverse divide */
	NULL,    /* 10111 ASN Arc sine */
	NULL,    /* 11000 ROL Rolar angle */
	NULL,    /* 11001 ACS Arc cosine */
	NULL,    /* 11010 undefined */
	NULL,    /* 11011 ATN Arc tangent */
	NULL,    /* 11100 undefined */
	NULL,    /* 11101 undefined */
	NULL,    /* 11110 undefined */
	NULL,    /* 11111 undefined */
};

/* Prototypes for instruction emulation functions */ 

int fpe_flt __P((fp_op_data_t *, trapframe_t *));
int fpe_fix __P((fp_op_data_t *, trapframe_t *));
int fpe_wfs __P((fp_op_data_t *, trapframe_t *));
int fpe_rfs __P((fp_op_data_t *, trapframe_t *));
int fpe_wfc __P((fp_op_data_t *, trapframe_t *));
int fpe_rfc __P((fp_op_data_t *, trapframe_t *));
int fpe_cmf __P((fp_op_data_t *, trapframe_t *));
int fpe_cnf __P((fp_op_data_t *, trapframe_t *));

/* Table of handler functions for fp register transfer operations */ 

fpe_dop_handler_t fpe_regtrans[16] = {
	fpe_flt, /* 0000 FLT Integer to FP */
	fpe_fix, /* 0001 FIX FP to Integer */
	fpe_wfs, /* 0010 Write FP status */
	fpe_rfs, /* 0011 Read FP status */
	fpe_wfc, /* 0100 Write FP control */
	fpe_rfc, /* 0101 Read FP control */
	NULL,    /* 0110 undefined */
	NULL,    /* 0111 undefined */
	NULL,    /* 1000 undefined */
	fpe_cmf, /* 1001 Compare FP */
	NULL,    /* 1010 undefined */
	fpe_cnf, /* 1011 Compare negated FP */
	NULL,    /* 1100 undefined */
	fpe_cmf, /* 1101 Compare FP with exception */
	NULL,    /* 1110 undefined */
	fpe_cnf, /* 1111 Compare negated FP with exception */
};

/* Prototypes for instruction emulation functions */ 

int fpe_ldfstf __P((unsigned int address, trapframe_t *));

FPE_INLINE int stofpe __P((u_int32_t */*address*/, fpe_reg_t */*fpreg*/));
int dtofpe __P((u_int32_t *, fpe_reg_t *));
int etofpe __P((u_int32_t *, fpe_reg_t *));
int ptofpe __P((u_int32_t *, fpe_reg_t *));
FPE_INLINE int fpetos __P((u_int32_t *, fpe_reg_t *));
int fpetod __P((u_int32_t *, fpe_reg_t *));
int fpetoe __P((u_int32_t *, fpe_reg_t *));
int fpetop __P((u_int32_t *, fpe_reg_t *));

int fpe_dump(void);

int coproc1_handler __P((unsigned int, unsigned int, trapframe_t *));
int coproc2_handler __P((unsigned int, unsigned int, trapframe_t *));

/* Prototypes for assembly functions */

u_int32_t nc_fp_add __P((u_int32_t, u_int32_t));
u_int32_t nc_fp_sub __P((u_int32_t, u_int32_t));
u_int32_t nc_fp_mul __P((u_int32_t, u_int32_t));
u_int32_t nc_fp_div __P((u_int32_t, u_int32_t));
u_int32_t nc_fp_fix __P((u_int32_t));
u_int32_t nc_fp_cmp __P((u_int32_t, u_int32_t));

#define FPP_ADD		0
#define FPP_SUB		1
#define FPP_MUL		2
#define FPP_DIV		3
#define FPP_CMF		4
#define FPP_CNF		5
#define FPP_FIX		6
#define FPP_FLT		7
#define FPP_ABS		8
#define FPP_MOV		9
#define FPP_MVN		10
#define FPP_RDV		11
#define FPP_RSB		12
#define FPP_STRS	13
#define FPP_STRD	14
#define FPP_STRE	15
#define FPP_STRP	16
#define FPP_LDRS	17
#define FPP_LDRD	18
#define FPP_LDRE	19
#define FPP_LDRP	20
#define FPP_MAX 	21

#ifdef FPE_PROF
u_int fp_profile[FPP_MAX];

#define FPP_INC(x)	++fp_profile[x]
#else
#define FPP_INC(x)
#endif

/*
* If FPE is defined then we are being built into the kernel.
* If not then we are being built as a standalone loadable module.
*/

#ifdef FPE
#define PRINTF(x)	;
#define FPE_DUMP()	;
#else
#define PRINTF(x) (*printf_store)(x)
#define FPE_DUMP()	fpe_dump()

void (*printf_store)();
void (*install_coproc_store)();

#ifdef NOTEXT
void
fpe_printf(char *a, ...)
{
}
#endif

/* Standalone initialisation point */

int
init_fpe(printf_handler, coproc_handler)
	void (*printf_handler)();
	void (*coproc_handler)();
{
	int loop;

	printf_store = printf_handler;
	install_coproc_store = coproc_handler;
#ifdef NOTEXT
	printf_store = fpe_printf;
#endif
	(*install_coproc_store)(FP_COPROC, coproc1_handler);
	(*install_coproc_store)(FP_COPROC2, coproc2_handler);

#ifdef FPE_PROF
	for (loop = 0; loop < FPP_MAX; ++loop)
		fp_profile[loop] = 0;
#endif

	PRINTF(("FPE installed\n"));
	return(0);
}
#endif


/*
 * Main entry point after an undefined instruction.
 * The instruction has been identified as a co-proc 2 instruction.
 * We get the address of the instruction, the instruction itself and
 * and pointer to the trap frame.
 * Currently we do support this (LFM/SFM) so its gallows time
 */

int
coproc2_handler(address, instruction, frame)
	unsigned int address;
	unsigned int instruction;
	trapframe_t *frame;
{
	log(LOG_ERR, "Coprocessor 2 FP instruction 0x%08x not supported yet.", instruction);
	return(1);
}


void
init_fpe_state(p)
	struct proc *p;
{
	struct fp_state *fpstate;
	int loop;

	fpstate = &p->p_addr->u_pcb.pcb_fpstate;
	fpstate->fp_flags |= 1;

	for (loop = 8; loop < 16; ++loop)
		fpstate->fp_registers[loop] = *((fp_reg_t *)&fpe_registers[loop]);
}


int
initialise_fpe(cpu)
	cpu_t *cpu;
{
	cpu->fpu_class = FPU_CLASS_FPE;
	cpu->fpu_type = FPU_TYPE_SP_FPE;
	strcpy(cpu->fpu_model, "Single precision floating point emulator");
	install_coproc_handler(FP_COPROC, coproc1_handler);
	install_coproc_handler(FP_COPROC2, coproc2_handler);
	init_fpe_state(curproc);
	return(0);
}


/*
 * Main entry point after an undefined instruction.
 * The instruction has been identified as a co-proc 1 instruction.
 * We get the address of the instruction, the instruction itself and
 * and pointer to the trap frame.
 */

int
coproc1_handler(address, instruction, frame)
	unsigned int address;
	unsigned int instruction;
	trapframe_t *frame;
{
	struct fp_state *fpstate;
	PRINTF(("FPE instruction 0x%08x -> 0x%08x\n",
		address, instruction));

/* Get the fp registers pointer */

	fpstate = &curproc->p_addr->u_pcb.pcb_fpstate;

	fpstate->fp_flags |= 2;	/* so statclock can tell if we are spending time in the fpe */
	
	if (fpstate->fp_flags & 1)
		fpregs = (fpe_reg_t *) &fpstate->fp_registers;
	else
		fpregs = fpe_registers;

/* Test for a FP data transfer operation */

	if ((instruction & (1 << 25)) == 0) {
		int err;

		PRINTF(("FP data transfer\n"));
		err = fpe_ldfstf(instruction, frame);
		FPE_DUMP();
		fpstate->fp_flags &= ~2;
		return(err);
	}

/* Test for a FP data operation */

	else if ((instruction & (1 << 4)) == 0) {
		fp_op_data_t op_data;

		op_data.opcode = (instruction >> 19) & 0x1e;
		if (instruction & 0x00008000)
			op_data.opcode |= 0x01;

		op_data.precision = ((instruction & (1<<19)) ? 2 : 0)
					| ((instruction & (1<<7)) ? 1 : 0);
		op_data.rounding = (instruction >> 5) & 3;
		op_data.fpregm = (instruction) & 15;
		op_data.fpregn = (instruction >> 16) & 7;
		op_data.fpregd = (instruction >> 12) & 7;

		PRINTF(("FP data op = %d\n", op_data.opcode));
		if (fpe_dops[op_data.opcode]) {
			int err;
			err = (*fpe_dops[op_data.opcode])(&op_data, frame);
#ifndef FPE_NODEBUG
			if (err == 0)
				FPE_DUMP();
#endif
			fpstate->fp_flags &= ~2;
			return(err);
		}
		else {
			fpstate->fp_flags &= ~2;
			return(1);
		}
	}

/* Test for a FP register transfer operation */
	
	else {
		fp_op_data_t op_data;

		op_data.opcode = (instruction >> 20) & 0x0f;

		op_data.precision = ((instruction & (1<<19)) ? 2 : 0)
					| ((instruction & (1<<7)) ? 1 : 0);
		op_data.rounding = (instruction >> 5) & 3;
		op_data.fpregm = (instruction) & 15;
		op_data.fpregn = (instruction >> 16) & 7;
		op_data.regd = (instruction >> 12) & 15;

		PRINTF(("FP reg trans = %d\n", op_data.opcode));
		if (fpe_regtrans[op_data.opcode]) {
			int err;

			PRINTF(("FP reg trans op = %d\n", op_data.opcode));
			err = (*fpe_regtrans[op_data.opcode])(&op_data, frame);
#ifndef FPE_NODEBUG
			if (err == 0 && (op_data.opcode == 0 || op_data.opcode == 2
				|| op_data.opcode == 4))
				FPE_DUMP();
#endif
			fpstate->fp_flags &= ~2;
			return(err);
		}
		else {
			fpstate->fp_flags &= ~2;
			return(1);
		}
	}

/* Well if it gets here then it was not a decodable FP instruction */

	fpstate->fp_flags &= ~2;
	return(1);
}


/* Precision convertion routines */

FPE_INLINE int
stofpe(address, fpreg)
	u_int32_t *address;
	fpe_reg_t *fpreg;
{
	fpe_sprec_t real;
	u_int32_t *words = (u_int32_t *)&real;

	if ((address[0] & ~(1<<31)) == 0) {
		PRINTF(("single=0\n"));

		fpreg->mantissa_hi = 0;
		fpreg->mantissa_lo = 0;
		fpreg->exponent = address[0] & (1<<31);
		return(0);
	}

	words[0] = address[0];
	PRINTF(("word=%08x\n", address[0]));

	PRINTF(("sign = %d exponent=%08x mantissa=%08x\n", real.sign,
	real.exponent, real.mantissa));
	fpreg->mantissa_hi = real.mantissa << 8;
	fpreg->mantissa_lo = 0;
	fpreg->exponent = ((int)real.exponent - 0x80) + 0x4000;
	fpreg->exponent |= (real.sign << 31);
	return(0);
}


int
dtofpe(address, fpreg)
	u_int32_t *address;
	fpe_reg_t *fpreg;
{
	fpe_dprec_t real;

	if ((address[0] & ~(1<<31)) == 0 && address[1] == 0) {
		PRINTF(("double=0\n"));

		fpreg->mantissa_hi = 0;
		fpreg->mantissa_lo = 0;
		fpreg->exponent = address[0] & (1<<31);
		return(0);
	}

	real = *(fpe_dprec_t *)address;
	PRINTF(("sign = %d exponent=%08x mantissa=%08x %08x\n", real.sign,
	real.exponent, real.mantissa_lo, real.mantissa_hi));
	fpreg->mantissa_hi = real.mantissa_hi << 11;
	fpreg->mantissa_hi |= real.mantissa_lo >> 21;
	fpreg->mantissa_lo = real.mantissa_lo << 11;
	fpreg->exponent = (real.exponent - 0x400) + 0x4000;
	fpreg->exponent |= (real.sign << 31);

	return(0);
}


#ifndef FPE_SPEEDUPS
int
etofpe(address, fpreg)
	u_int32_t *address;
	fpe_reg_t *fpreg;
{
	bcopy((char *)address, (char *)fpreg, 12);
	return(0);
}
#endif


int
ptofpe(address, fpreg)
	u_int32_t *address;
	fpe_reg_t *fpreg;
{
	fpe_pprec_t real;

	real = *(fpe_pprec_t *)address;
	return(1);
}


FPE_INLINE int
fpetos(address, fpreg)
	u_int32_t *address;
	fpe_reg_t *fpreg;
{
	fpe_sprec_t real;
	int exp;

	if (fpreg->mantissa_hi == 0 && fpreg->mantissa_lo == 0
	    && (fpreg->exponent & ~(1<<31)) == 0) {
		PRINTF(("single=0\n"));

		address[0] = fpreg->exponent & (1<<31);
		return(0);
	}

	real.sign = (fpreg->exponent >> 31) & 1;
	real.mantissa = (fpreg->mantissa_hi >> 8);
	exp = ((int)(fpreg->exponent & 0x7fff) - 0x4000) + 0x80;

	if (exp < 0 || exp > 0xff) {
		PRINTF(("ermm exponent out of range !\n"));
		exp = 0x80;
	}

	real.exponent = exp;

	PRINTF(("single=%08x\n", *((unsigned int *)&real)));

	*(fpe_sprec_t *)address = real;
	if (real.sign == 1 && real.mantissa == 0 && real.exponent == 0)
		printf("single result = %08x", *address);
	return(0);
}


int
fpetod(address, fpreg)
	u_int32_t *address;
	fpe_reg_t *fpreg;
{
	fpe_dprec_t real;
	u_int32_t *words = (u_int32_t *)&real;
	int exp;

	if (fpreg->mantissa_lo == 0 && (fpreg->exponent & ~(1<<31)) == 0) {
		PRINTF(("double=0\n"));

		address[0] = fpreg->exponent & (1<<31);
		address[1] = 0;
		return(0);
	}

	real.sign = (fpreg->exponent >> 31) & 1;
	real.mantissa_hi = (fpreg->mantissa_hi >> 11);
	real.mantissa_lo = (fpreg->mantissa_hi << 21) | (fpreg->mantissa_lo >> 11);
	exp = ((int)(fpreg->exponent & 0x7fff) - 0x4000) + 0x400;

	if (exp < 0 || exp > 0x7ff) {
		PRINTF(("ermm exponent out of range !\n"));
		exp = 0x400;
	}

	real.exponent = exp;

	PRINTF(("double=%08x%08x\n", words[0], words[1]));

	address[0] = words[0];
	address[1] = words[1];
	return(0);
}

#ifndef FPE_SPEEDUPS
int
fpetoe(address, fpreg)
	u_int32_t *address;
	fpe_reg_t *fpreg;
{
	bcopy((char *)fpreg, (char *)address, 12);
	return(0);
}
#endif


int
fpetop(address, fpreg)
	u_int32_t *address;
	fpe_reg_t *fpreg;
{
	return(1);
}


/* Unary fp data operations */

/*
 * int fpe_mvf(fp_op_data_t *op, trapframe_t *frame)
 *
 * MVF - Move FP register to FP register
 */

int
fpe_mvf(op, frame)
	fp_op_data_t *op;
	trapframe_t *frame;
{
	FPP_INC(FPP_MOV);
	PRINTF(("MVF prec=%d rnd=%d fd=%d fn=%d fm=%d\n",
		op->precision, op->rounding, op->fpregd, op->fpregn, op->fpregm));
	fpregs[op->fpregd] = fpregs[op->fpregm];
	return(0);
}


/*
* int fpe_mnf(fp_op_data_t *op, trapframe_t *frame)
*
* MNF - Move FP register to FP register negated
*/

int
fpe_mnf(op, frame)
	fp_op_data_t *op;
	trapframe_t *frame;
{
	fpe_qreg_t real1 = *(fpe_qreg_t *)&fpregs[8];
	fpe_qreg_t real2 = *(fpe_qreg_t *)&fpregs[op->fpregm];
	fpe_qreg_t result;
	u_int32_t single_a, single_b, single_result;

	FPP_INC(FPP_MVN);
	PRINTF(("MNF prec=%d rnd=%d fd=%d fn=%d fm=%d\n",
		op->precision, op->rounding, op->fpregd, op->fpregn, op->fpregm));

	fpetos(&single_a, (fpe_reg_t *) &real1);
	fpetos(&single_b, (fpe_reg_t *) &real2);
	single_result = nc_fp_sub(single_a, single_b);
	stofpe(&single_result, (fpe_reg_t *) &result);

	fpregs[op->fpregd] = *(fpe_reg_t *) &result;
	return(0);
}


/*
* int fpe_abs(fp_op_data_t *op, trapframe_t *frame)
*
* ABS - Absolute value of a FP register
*/

int
fpe_abs(op, frame)
	fp_op_data_t *op;
	trapframe_t *frame;
{
	fpe_qreg_t real1 = *(fpe_qreg_t *)&fpregs[8];
	fpe_qreg_t real2 = *(fpe_qreg_t *)&fpregs[op->fpregm];
	fpe_qreg_t result;
	u_int32_t single_a, single_b, single_result;

	FPP_INC(FPP_ABS);
	PRINTF(("ABS prec=%d rnd=%d fd=%d fn=%d fm=%d\n",
		op->precision, op->rounding, op->fpregd, op->fpregn, op->fpregm));

	fpetos(&single_a, (fpe_reg_t *) &real2);
	fpetos(&single_b, (fpe_reg_t *) &real1);
	single_result = nc_fp_cmp(single_a, single_b);
	if (single_result & 0x80000000) {
		fpetos(&single_a, (fpe_reg_t *) &real1);
		fpetos(&single_b, (fpe_reg_t *) &real2);
		single_result = nc_fp_sub(single_a, single_b);
		stofpe(&single_result, (fpe_reg_t *) &result);

		fpregs[op->fpregd] = *(fpe_reg_t *) &result;
	} else {
		fpregs[op->fpregd] = fpregs[op->fpregm];
	}

	return(0);
}


/* Binary fp data operations */

/*
 * int fpe_adf(fp_op_data_t *op, trapframe_t *frame)
 *
 * ADF - Floating point add
 */

int
fpe_adf(op, frame)
	fp_op_data_t *op;
	trapframe_t *frame;
{
	fpe_qreg_t real1 = *(fpe_qreg_t *)&fpregs[op->fpregn];
	fpe_qreg_t real2 = *(fpe_qreg_t *)&fpregs[op->fpregm];
	fpe_qreg_t result;
	u_int32_t single_a, single_b, single_result;

	FPP_INC(FPP_ADD);
	PRINTF(("ADF prec=%d rnd=%d fd=%d fn=%d fm=%d\n",
		op->precision, op->rounding, op->fpregd, op->fpregn, op->fpregm));

	fpetos(&single_a, (fpe_reg_t *) &real1);
	fpetos(&single_b, (fpe_reg_t *) &real2);
	single_result = nc_fp_add(single_a, single_b);
	stofpe(&single_result, (fpe_reg_t *) &result);
	fpregs[op->fpregd] = *(fpe_reg_t *) &result;
	return(0);
}


/*
 * int fpe_suf(fp_op_data_t *op, trapframe_t *frame)
 *
 * SUF - Floating point subtract
 */

int
fpe_suf(op, frame)
	fp_op_data_t *op;
	trapframe_t *frame;
{
	fpe_qreg_t real1 = *(fpe_qreg_t *)&fpregs[op->fpregn];
	fpe_qreg_t real2 = *(fpe_qreg_t *)&fpregs[op->fpregm];
	fpe_qreg_t result;
	u_int32_t single_a, single_b, single_result;

	FPP_INC(FPP_SUB);
	PRINTF(("SUF prec=%d rnd=%d fd=%d fn=%d fm=%d\n",
		op->precision, op->rounding, op->fpregd, op->fpregn, op->fpregm));

	fpetos(&single_a, (fpe_reg_t *) &real1);
	fpetos(&single_b, (fpe_reg_t *) &real2);
	single_result = nc_fp_sub(single_a, single_b);
	stofpe(&single_result, (fpe_reg_t *) &result);
	fpregs[op->fpregd] = *(fpe_reg_t *) &result;
	return(0);
}


/*
 * int fpe_muf(fp_op_data_t *op, trapframe_t *frame)
 *
 * MUF - Floating point multiply
 */

int
fpe_muf(op, frame)
	fp_op_data_t *op;
	trapframe_t *frame;
{
	fpe_qreg_t real1 = *(fpe_qreg_t *)&fpregs[op->fpregn];
	fpe_qreg_t real2 = *(fpe_qreg_t *)&fpregs[op->fpregm];
	fpe_qreg_t result;
	u_int32_t single_a, single_b, single_result;

	FPP_INC(FPP_MUL);
	PRINTF(("MUF prec=%d rnd=%d fd=%d fn=%d fm=%d\n",
		op->precision, op->rounding, op->fpregd, op->fpregn, op->fpregm));

	fpetos(&single_a, (fpe_reg_t *) &real1);
	fpetos(&single_b, (fpe_reg_t *) &real2);
	single_result = nc_fp_mul(single_a, single_b);
	stofpe(&single_result, (fpe_reg_t *) &result);
		fpregs[op->fpregd] = *(fpe_reg_t *) &result;
	return(0);
}


/*
 * int fpe_dvf(fp_op_data_t *op, trapframe_t *frame)
 *
 * DVF - Floating point divide
 */

int fpe_dvf(fp_op_data_t *op, trapframe_t *frame)
{
	fpe_qreg_t real1 = *(fpe_qreg_t *)&fpregs[op->fpregn];
	fpe_qreg_t real2 = *(fpe_qreg_t *)&fpregs[op->fpregm];
	fpe_qreg_t result;
	u_int32_t single_a, single_b, single_result;

	FPP_INC(FPP_DIV);
	PRINTF(("DVF prec=%d rnd=%d fd=%d fn=%d fm=%d\n", op->precision, op->rounding, op->fpregd, op->fpregn, op->fpregm));

	fpetos(&single_a, (fpe_reg_t *) &real1);
	fpetos(&single_b, (fpe_reg_t *) &real2);
	single_result = nc_fp_div(single_a, single_b);
	stofpe(&single_result, (fpe_reg_t *) &result);
	fpregs[op->fpregd] = *(fpe_reg_t *) &result;
	return(0);
}


/*
 * int fpe_rdf(fp_op_data_t *op, trapframe_t *frame)
 *
 * RDF - Floating point reverse divide
 */

int
fpe_rdf(op, frame)
	fp_op_data_t *op;
	trapframe_t *frame;
{
	fpe_qreg_t real1 = *(fpe_qreg_t *)&fpregs[op->fpregm];
	fpe_qreg_t real2 = *(fpe_qreg_t *)&fpregs[op->fpregn];
	fpe_qreg_t result;
	u_int32_t single_a, single_b, single_result;

	FPP_INC(FPP_RDV);
	PRINTF(("RDF prec=%d rnd=%d fd=%d fn=%d fm=%d\n",
		op->precision, op->rounding, op->fpregd, op->fpregn, op->fpregm));

	fpetos(&single_a, (fpe_reg_t *) &real1);
	fpetos(&single_b, (fpe_reg_t *) &real2);
	single_result = nc_fp_div(single_a, single_b);
	stofpe(&single_result, (fpe_reg_t *) &result);
	fpregs[op->fpregd] = *(fpe_reg_t *) &result;
	return(0);
}


/*
 * int fpe_rsf(fp_op_data_t *op, trapframe_t *frame)
 *
 * RSF - Floating point reverse subtract
 */

int
fpe_rsf(op, frame)
	fp_op_data_t *op;
	trapframe_t *frame;
{
	fpe_qreg_t real1 = *(fpe_qreg_t *)&fpregs[op->fpregm];
	fpe_qreg_t real2 = *(fpe_qreg_t *)&fpregs[op->fpregn];
	fpe_qreg_t result;
	u_int32_t single_a, single_b, single_result;

	FPP_INC(FPP_RSB);
	PRINTF(("RSF prec=%d rnd=%d fd=%d fn=%d fm=%d\n",
		op->precision, op->rounding, op->fpregd, op->fpregn, op->fpregm));

	fpetos(&single_a, (fpe_reg_t *) &real1);
	fpetos(&single_b, (fpe_reg_t *) &real2);
	single_result = nc_fp_sub(single_a, single_b);
	stofpe(&single_result, (fpe_reg_t *) &result);
	fpregs[op->fpregd] = *(fpe_reg_t *) &result;
	return(0);
}


/* FPE Register transfer instructions */

/*
* int fpe_flt(fp_op_data_t *op, trapframe_t *frame)
*
* FLT - Integer register to float register
*/

int
fpe_flt(op, frame)
	fp_op_data_t *op;
	trapframe_t *frame;
{
	int regd;
	fpe_reg_t real;
	int *regs = &frame->tf_r0;

	FPP_INC(FPP_FLT);
	PRINTF(("FLT prec=%d rnd=%d rd=%d fn=%d fm=%d\n",
		op->precision, op->rounding, op->regd, op->fpregn, op->fpregm));

	regd = regs[op->regd];

	if (regd > 0)
		real.mantissa_lo = regd;
	else
		real.mantissa_lo = -regd;
	real.mantissa_hi = 0;

	if (regd == 0)
		real.exponent = 0;
	else {
		real.exponent = 16383 + 63;

		while ((real.mantissa_hi & (1 << 31)) == 0) {
			--real.exponent;
			real.mantissa_hi = real.mantissa_hi << 1;
			if (real.mantissa_lo & (1 << 31))
				real.mantissa_hi |= 1;
			real.mantissa_lo = real.mantissa_lo << 1;
		}
	}

	if (regd < 0)
		real.exponent |= (1 <<31);

	fpregs[op->fpregn] = real;

	return(0);
}


/*
 * int fpe_fix(fp_op_data_t *op, trapframe_t *frame)
 *
 * FIX - Float register to integer register
 */

int
fpe_fix(op, frame)
	fp_op_data_t *op;
	trapframe_t *frame;
{
	fpe_qreg_t src = *(fpe_qreg_t *) &fpregs[op->fpregm];
	u_int32_t single_argument;
	unsigned int *regs = &frame->tf_r0;

	FPP_INC(FPP_FIX);
	PRINTF(("FIX prec=%d rnd=%d rd=%d fn=%d fm=%d\n",
		op->precision, op->rounding, op->regd, op->fpregn, op->fpregm));

	fpetos(&single_argument, (fpe_reg_t *) &src);

/*
 * Call the assembler (32-bit) code for now
 */
	regs[op->regd] = nc_fp_fix(single_argument);
	return(0);
}


/*
 * int fpe_wfs(fp_op_data_t *op, trapframe_t *frame)
 *
 * WFS - Write FP status register
 */

int fpe_wfs(op, frame)
	fp_op_data_t *op;
	trapframe_t *frame;
{
	unsigned int *regs = &frame->tf_r0;

	PRINTF(("WFS rd=%d\n", op->regd));

	fpe_status = (fpe_status & ~FPE_STATUS_MASK)
			| (regs[op->regd] & FPE_STATUS_MASK);

	return(0);
}


/*
* int fpe_rfs(fp_op_data_t *op, trapframe_t *frame)
*
* RFS - Read FP status register
*/

int
fpe_rfs(op, frame)
	fp_op_data_t *op;
	trapframe_t *frame;
{
	unsigned int *regs = &frame->tf_r0;

	PRINTF(("RFS rd=%d\n", op->regd));

	regs[op->regd] = fpe_status;

	return(0);
}


/*
 * int fpe_wfc(fp_op_data_t *op, trapframe_t *frame)
 *
 * WFC - Write FP control register
 */

int
fpe_wfc(op, frame)
	fp_op_data_t *op;
	trapframe_t *frame;
{
	unsigned int *regs = &frame->tf_r0;

	PRINTF(("WFC rd=%d\n", op->regd));

	fpe_control = (fpe_control & ~FPE_CONTROL_MASK)
			| (regs[op->regd] & FPE_CONTROL_MASK);

	return(0);
}


/*
 * int fpe_rfc(fp_op_data_t *op, trapframe_t *frame)
 *
 * RFC - Read FP control register
 */

int
fpe_rfc(op, frame)
	fp_op_data_t *op;
	trapframe_t *frame;
{
	unsigned int *regs = &frame->tf_r0;

	PRINTF(("RFC rd=%d\n", op->regd));

	regs[op->regd] = fpe_control;

	return(0);
}


/*
 * int fpe_cmf(fp_op_data_t *op, trapframe_t *frame)
 *
 * CMF - FP compare
 */

int
fpe_cmf(op, frame)
	fp_op_data_t *op;
	trapframe_t *frame;
{
	fpe_qreg_t real1 = *(fpe_qreg_t *)&fpregs[op->fpregn];
	fpe_qreg_t real2 = *(fpe_qreg_t *)&fpregs[op->fpregm];
	u_int32_t single_a, single_b, single_result;

	FPP_INC(FPP_CMF);
	PRINTF(("CMF prec=%d rnd=%d rd=%d fn=%d fm=%d\n",
		op->precision, op->rounding, op->regd, op->fpregn, op->fpregm));

	fpetos(&single_a, (fpe_reg_t *) &real1);
	fpetos(&single_b, (fpe_reg_t *) &real2);
	single_result = nc_fp_cmp(single_a, single_b);
	frame->tf_spsr = (frame->tf_spsr & ~0xf0000000) | (single_result & 0xf0000000);
	return(0);
}


/*
 * int fpe_cnf(fp_op_data_t *op, trapframe_t *frame)
 *
 * CNF - FP compare negated
 */

int
fpe_cnf(op, frame)
	fp_op_data_t *op;
	trapframe_t *frame;
{
	fpe_qreg_t real0 = *(fpe_qreg_t *)&fpregs[8];
	fpe_qreg_t real1 = *(fpe_qreg_t *)&fpregs[op->fpregn];
	fpe_qreg_t real2 = *(fpe_qreg_t *)&fpregs[op->fpregm];
	u_int32_t single_a, single_b, single_result;

	FPP_INC(FPP_CNF);
	PRINTF(("CNF prec=%d rnd=%d rd=%d fn=%d fm=%d\n",
		op->precision, op->rounding, op->regd, op->fpregn, op->fpregm));

	fpetos(&single_a, (fpe_reg_t *) &real0);
	fpetos(&single_b, (fpe_reg_t *) &real2);
	single_b = nc_fp_sub(single_a, single_b);

	fpetos(&single_a, (fpe_reg_t *) &real1);
	single_result = nc_fp_cmp(single_a, single_b);
	frame->tf_spsr = (frame->tf_spsr & ~0xf0000000) | (single_result & 0xf0000000);
	return(0);
}


/*
 * int fpe_ldfstf(unsigned in instruction, trapframe_t *frame)
 *
 * LDF/STF - Load / store FP registers to / from memory
 */


int
fpe_ldfstf(instruction, frame)
	unsigned int instruction;
	trapframe_t *frame;
{
	unsigned int *regs = &frame->tf_r0;
	int offset;
	int fpreg;
	int reg;
	u_int32_t address;
	int precision;
	int error = 0;

	offset = (instruction & 0xff) << 2;

	fpreg = (instruction >> 12) & 0x07;
	reg = (instruction >> 16) & 0x0f;
	precision = ((instruction & (1<<22)) ? 2 : 0)
			| ((instruction & (1<<15)) ? 1 : 0);

	if (!(instruction & (1<<23)))
		offset = -offset;

	if (reg == 15)
		printf("WARNING: LDF/STF using r15\n");

	address = regs[reg];

	if ((instruction & (1<<24)))
		address += offset;

	if (!(instruction & (1 << 20))) {
		PRINTF(("STF rd=%d fd=%d prec=%d offset=%d address = %08x\n",
			reg, fpreg, precision, offset, address));

		switch(precision) {
		case 0 :
			FPP_INC(FPP_STRS);
			error = fpetos((u_int32_t *)address, &fpregs[fpreg]);
			break;
		case 1 :
			FPP_INC(FPP_STRD);
			error = fpetod((u_int32_t *)address, &fpregs[fpreg]);
			break;
		case 2 :
			FPP_INC(FPP_STRE);
			error = fpetoe((u_int32_t *)address, &fpregs[fpreg]);
			break;
		case 3 :
			FPP_INC(FPP_STRP);
			error = fpetop((u_int32_t *)address, &fpregs[fpreg]);
			break;
		}
	} else {
		PRINTF(("LDF rd=%d fd=%d prec=%d offset=%d address = %08x\n",
			reg, fpreg, precision, offset, address));

		switch(precision) {
		case 0 :
			FPP_INC(FPP_LDRS);
			error = stofpe((u_int32_t *)address, &fpregs[fpreg]);
			break;
		case 1 :
			FPP_INC(FPP_LDRD);
			error = dtofpe((u_int32_t *)address, &fpregs[fpreg]);
			break;
		case 2 :
			FPP_INC(FPP_LDRE);
			error = etofpe((u_int32_t *)address, &fpregs[fpreg]);
			break;
		case 3 :
			FPP_INC(FPP_LDRP);
			error = ptofpe((u_int32_t *)address, &fpregs[fpreg]);
			break;
		}
	}

	if (!(instruction & (1<<24)))
		address += offset;

	if ((instruction & (1<<21)))
		regs[reg] = address;

	return(error);
}


/* Debugging functions */

int
fpe_dump()
{
	int loop;

	for (loop = 0; loop < 8; loop+=2) {
		PRINTF(("f%2d : %08x %08x %08x   f%2d : %08x %08x %08x\n",
			loop, fpregs[loop].mantissa_hi,
		fpregs[loop].mantissa_lo, fpregs[loop].exponent,
			loop+1, fpregs[loop+1].mantissa_hi,
		fpregs[loop+1].mantissa_lo, fpregs[loop+1].exponent));
	}
	PRINTF(("\x1b[0mFPSR=%08x FPCR=%08x\n", fpe_status, fpe_control));
	return(0);
}


#ifdef FPE_PROF
void
fpe_dump_prof()
{
	printf("adf : %6d\n", fp_profile[FPP_ADD]);
	printf("suf : %6d\n", fp_profile[FPP_SUB]);
	printf("muf : %6d\n", fp_profile[FPP_MUL]);
	printf("dvf : %6d\n", fp_profile[FPP_DIV]);
	printf("cmf : %6d\n", fp_profile[FPP_CMF]);
	printf("cnf : %6d\n", fp_profile[FPP_CNF]);
	printf("fix : %6d\n", fp_profile[FPP_FIX]);
	printf("flt : %6d\n", fp_profile[FPP_FLT]);
	printf("abs : %6d\n", fp_profile[FPP_ABS]);
	printf("mvf : %6d\n", fp_profile[FPP_MOV]);
	printf("mnf : %6d\n", fp_profile[FPP_MVN]);
	printf("rdf : %6d\n", fp_profile[FPP_RDV]);
	printf("rsf : %6d\n", fp_profile[FPP_RSB]);
	printf("strs: %6d\n", fp_profile[FPP_STRS]);
	printf("strd: %6d\n", fp_profile[FPP_STRD]);
	printf("stre: %6d\n", fp_profile[FPP_STRE]);
	printf("strp: %6d\n", fp_profile[FPP_STRP]);
	printf("ldrs: %6d\n", fp_profile[FPP_LDRS]);
	printf("ldrd: %6d\n", fp_profile[FPP_LDRD]);
	printf("ldre: %6d\n", fp_profile[FPP_LDRE]);
	printf("ldrp: %6d\n", fp_profile[FPP_LDRP]);
}
#else
void
fpe_dump_prof()
{
	printf("kernel not compiled with FPE profiling\n");
}
#endif

/* End of fpe.c */
