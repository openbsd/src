/*	$OpenBSD: alpha_trace.c,v 1.5 2002/08/09 20:26:44 jsyn Exp $	*/
/*
 * Copyright (c) 2002 Artur Grabowski <art@openbsd.org>
 * All rights reserved. 
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions 
 * are met: 
 *
 * 1. Redistributions of source code must retain the above copyright 
 *    notice, this list of conditions and the following disclaimer. 
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL  DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

/*
 * Copyright (c) 1997 Niklas Hallqvist.  All rights reserverd.
 * Copyright (c) 1997 Theo de Raadt.  All rights reserverd.
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
 *	This product includes software developed by Niklas Hallqvist and
 *	Theo de Raadt.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/ptrace.h>
#include <machine/reg.h>
#include <machine/frame.h>

#include <stdio.h>
#include <string.h>

#include "pmdb.h"
#include "alpha_instruction.h"
#include "symbol.h"

struct opcode {
        enum opc_fmt { OPC_PAL, OPC_RES, OPC_MEM, OPC_OP, OPC_BR } opc_fmt;
        char *opc_name;
        int opc_print;
};

#define inst_return(ins)        (((ins) & 0xfc000000) == 0x68000000)

const struct opcode opcode[] = {
	{ OPC_PAL, "call_pal", 0 },	/* 00 */
	{ OPC_RES, "opc01", 0 },	/* 01 */
	{ OPC_RES, "opc02", 0 },	/* 02 */
	{ OPC_RES, "opc03", 0 },	/* 03 */
	{ OPC_RES, "opc04", 0 },	/* 04 */
	{ OPC_RES, "opc05", 0 },	/* 05 */
	{ OPC_RES, "opc06", 0 },	/* 06 */
	{ OPC_RES, "opc07", 0 },	/* 07 */
	{ OPC_MEM, "lda", 1 },		/* 08 */
	{ OPC_MEM, "ldah", 1 },		/* 09 */
	{ OPC_RES, "opc0a", 0 },	/* 0A */
	{ OPC_MEM, "ldq_u", 1 },	/* 0B */
	{ OPC_RES, "opc0c", 0 },	/* 0C */
	{ OPC_RES, "opc0d", 0 },	/* 0D */
	{ OPC_RES, "opc0e", 0 },	/* 0E */
	{ OPC_MEM, "stq_u", 1 },	/* 0F */
	{ OPC_OP, "inta", 0 },		/* 10 */
	{ OPC_OP, "intl", 0 },		/* 11 */
	{ OPC_OP, "ints", 0 },		/* 12 */
	{ OPC_OP, "intm", 0 },		/* 13 */
	{ OPC_RES, "opc14", 0 },	/* 14 */
	{ OPC_OP, "fltv", 1 },		/* 15 */
	{ OPC_OP, "flti", 1 },		/* 16 */
	{ OPC_OP, "fltl", 1 },		/* 17 */
	{ OPC_MEM, "misc", 0 },		/* 18 */
	{ OPC_PAL, "pal19", 0 },	/* 19 */
	{ OPC_MEM, "jsr", 0 },		/* 1A */
	{ OPC_PAL, "pal1b", 0 },	/* 1B */
	{ OPC_RES, "opc1c", 0 },	/* 1C */
	{ OPC_PAL, "pal1d", 0 },	/* 1D */
	{ OPC_PAL, "pal1e", 0 },	/* 1E */
	{ OPC_PAL, "pal1f", 0 },	/* 1F */
	{ OPC_MEM, "ldf", 1 },		/* 20 */
	{ OPC_MEM, "ldg", 1 },		/* 21 */
	{ OPC_MEM, "lds", 1 },		/* 22 */
	{ OPC_MEM, "ldt", 1 },		/* 23 */
	{ OPC_MEM, "stf", 1 },		/* 24 */
	{ OPC_MEM, "stg", 1 },		/* 25 */
	{ OPC_MEM, "sts", 1 },		/* 26 */
	{ OPC_MEM, "stt", 1 },		/* 27 */
	{ OPC_MEM, "ldl", 1 },		/* 28 */
	{ OPC_MEM, "ldq", 1 },		/* 29 */
	{ OPC_MEM, "ldl_l", 1 },	/* 2A */
	{ OPC_MEM, "ldq_l", 1 },	/* 2B */
	{ OPC_MEM, "stl", 1 },		/* 2C */
	{ OPC_MEM, "stq", 1 },		/* 2D */
	{ OPC_MEM, "stl_c", 1 },	/* 2E */
	{ OPC_MEM, "stq_c", 1 },	/* 2F */
	{ OPC_BR, "br", 1 },		/* 30 */
	{ OPC_BR, "fbeq", 1 },		/* 31 */
	{ OPC_BR, "fblt", 1 },		/* 32 */
	{ OPC_BR, "fble", 1 },		/* 33 */
	{ OPC_BR, "bsr", 1 },		/* 34 */
	{ OPC_BR, "fbne", 1 },		/* 35 */
	{ OPC_BR, "fbge", 1 },		/* 36 */
	{ OPC_BR, "fbgt", 1 },		/* 37 */
	{ OPC_BR, "blbc", 1 },		/* 38 */
	{ OPC_BR, "beq", 1 },		/* 39 */
	{ OPC_BR, "blt", 1 },		/* 3A */
	{ OPC_BR, "ble", 1 },		/* 3B */
	{ OPC_BR, "blbs", 1 },		/* 3C */
	{ OPC_BR, "bne", 1 },		/* 3D */
	{ OPC_BR, "bge", 1 },		/* 3E */
	{ OPC_BR, "bgt", 1 },		/* 3F */
};

int
inst_load(int ins)
{
	alpha_instruction insn;

	insn.bits = ins;

	/* Loads. */
	if (insn.mem_format.opcode == op_ldbu ||
	    insn.mem_format.opcode == op_ldq_u ||
	    insn.mem_format.opcode == op_ldwu)
	        return (1);
	if ((insn.mem_format.opcode >= op_ldf) &&
	    (insn.mem_format.opcode <= op_ldt))
		return (1);
	if ((insn.mem_format.opcode >= op_ldl) &&
	    (insn.mem_format.opcode <= op_ldq_l))
		return (1);

	/* Prefetches. */
	if (insn.mem_format.opcode == op_special) {
		/* Note: MB is treated as a store. */
		if ((insn.mem_format.displacement == (short)op_fetch) ||
		    (insn.mem_format.displacement == (short)op_fetch_m))
			return (1);
	}

	return (0);
}

static __inline int sext(u_int);
static __inline int rega(u_int);
static __inline int regb(u_int);
static __inline int regc(u_int);
static __inline int disp(u_int);

static __inline int
sext(x)
	u_int x;
{
	return ((x & 0x8000) ? -(-x & 0xffff) : (x & 0xffff));
}

static __inline int
rega(x)
	u_int x;
{
	return ((x >> 21) & 0x1f);
}

static __inline int
regb(x)
	u_int x;
{
	return ((x >> 16) & 0x1f);
}

static __inline int
regc(x)
	u_int x;
{
	return (x & 0x1f);
}

static __inline int
disp(x)
	u_int x;
{
	return (sext(x & 0xffff));
}

/*
 * XXX There are a couple of problems with this code:
 *
 *	The argument list printout code is likely to get confused.
 *
 *	It relies on the conventions of gcc code generation.
 *
 *	It uses heuristics to calculate the framesize, and might get it wrong.
 *
 *	It doesn't yet use the framepointer if available.
 *
 *	The address argument can only be used for pointing at trapframes
 *	since a frame pointer of its own serves no good on the alpha,
 *	you need a pc value too.
 *
 *	The heuristics used for tracing through a trap relies on having
 *	symbols available.
 */
int
md_getframe(struct pstate *ps, int framec, struct md_frame *fram)
{
	reg frame;
	int i, framesize;
	reg pc, ra;
	u_int inst;
	char *name;
	char namebuf[1024];
	reg offset;
	reg slot[32];
	struct reg regs;
	int count;

	bzero(slot, sizeof(slot));

	if (process_getregs(ps, &regs))
		return (-1);

	for (i = 0; i < 32; i++)
		slot[i] = -1;

	frame = regs.r_regs[R_SP];
	pc = regs.r_regs[R_ZERO];	/* Ieeek. on drugs. */
	ra = regs.r_regs[R_RA];

	for (count = 0; count < framec + 1; count++) {
		/* XXX - better out of bounds check needed. */
		if (pc < 0x1000 || pc == 0xffffffffffffffff) {
			return -1;
		}

		name = sym_name_and_offset(ps, pc, namebuf,
		    sizeof(namebuf), &offset);
		if (!name) {
			/* Limit the search for procedure start */
			offset = 65536;
		}

		framesize = 0;
		for (i = sizeof (int); i <= offset; i += sizeof (int)) {
			if (process_read(ps, pc - i, &inst, sizeof(inst)) < 0)
				return -1;
	
			/*
			 * If by chance we don't have any symbols we have to
			 * get out somehow anyway.  Check for the preceding
			 * procedure return in that case.
			 */
			if (name == NULL && inst_return(inst))
				break;

			/*
			 * Disassemble to get the needed info for the frame.
			 */
			if ((inst & 0xffff0000) == 0x23de0000) {
				/* lda sp,n(sp) */
				framesize -= disp(inst) / sizeof (u_long);
			} else if ((inst & 0xfc1f0000) == 0xb41e0000) {
				/* stq X,n(sp) */
				slot[rega(inst)] = frame + disp(inst);
			} else if ((inst & 0xfc000fe0) == 0x44000400 &&
			    rega(inst) == regb(inst)) {
				/* bis X,X,Y (aka mov X,Y) */
				/* zero is hardwired */
				if (rega(inst) != 31)
					slot[rega(inst)] = slot[regc(inst)];
				slot[regc(inst)] = 0;
				/*
				 * XXX In here we might special case a frame
				 * pointer setup, i.e. mov sp, fp.
				 */
			} else if (inst_load(inst)) {
				/* clobbers a register */
				slot[rega(inst)] = 0;
			} else if (opcode[inst >> 26].opc_fmt == OPC_OP) {
				/* clobbers a register */
				slot[regc(inst)] = 0;
			}
			/*
			 * XXX Recognize more reg clobbering instructions and
			 * set slot[reg] = 0 then too.
			 */
		}

		fram->pc = pc;
		fram->fp = frame;

		/* Look for the return address if recorded.  */
		if (slot[R_RA]) {
			if (slot[R_RA] == -1)
				ra = regs.r_regs[R_RA];
			else
				if (process_read(ps, (off_t)slot[R_RA],
				    &ra, sizeof(ra)) < 0)
					return -1;
		} else {
			break;
		}

		/* Advance to the next frame.  */
		frame += framesize * sizeof(u_long);
		if (pc == ra) {
			break;
		}
		pc = ra;
	}

	return count == framec + 1 ? 0 : -1;
}
