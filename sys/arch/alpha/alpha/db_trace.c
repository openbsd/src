/*	$OpenBSD: db_trace.c,v 1.4 1997/07/23 23:29:45 niklas Exp $	*/

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

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>

#include <vm/vm.h>

#include <machine/db_machdep.h>
#include <machine/frame.h>

#include <ddb/db_access.h>
#include <ddb/db_command.h>
#include <ddb/db_output.h>
#include <ddb/db_sym.h>
#include <ddb/db_variables.h>
#include <ddb/db_extern.h>
#include <ddb/db_interface.h>

extern int	etext;

static __inline int sext __P((u_int));
static __inline int rega __P((u_int));
static __inline int regb __P((u_int));
static __inline int regc __P((u_int));
static __inline int disp __P((u_int));

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
void
db_stack_trace_cmd(addr, have_addr, count, modif)
	db_expr_t       addr;
	int             have_addr;
	db_expr_t       count;
	char            *modif;
{
	u_long		*frame;
	int		i, framesize;
	db_addr_t	pc, ra;
	u_int		inst;
	char		*name;
	db_expr_t	offset;
	db_regs_t	*regs;
	u_long		*slot[32];

	bzero(slot, sizeof(slot));
	if (count == -1)
		count = 65535;

	regs = have_addr ? (db_regs_t *)addr : DDB_REGS;
trapframe:
	/* remember where various registers are stored */
	for (i = 0; i < 31; i++)
		slot[i] = &regs->tf_regs[0] +
		    ((u_long *)db_regs[i].valuep - &ddb_regs.tf_regs[0]);
	frame = (u_long *)regs->tf_regs[FRAME_SP];
	pc = (db_addr_t)regs->tf_regs[FRAME_PC];
	ra = (db_addr_t)regs->tf_regs[FRAME_RA];

	while (count-- && pc >= (db_addr_t)KERNBASE && pc < (db_addr_t)&etext) {
		db_find_sym_and_offset(pc, &name, &offset);
		if (!name) {
			name = "?";
			/* Limit the search for procedure start */
			offset = 65536;
		}
		db_printf("%s(", name);

		framesize = 0;
		for (i = sizeof (int); i <= offset; i += sizeof (int)) {
			inst = *(u_int *)(pc - i);
	
			/*
			 * If by chance we don't have any symbols we have to
			 * get out somehow anyway.  Check for the preceding
			 * procedure return in that case.
			 */
			if (name[0] == '?' && inst_return(inst))
				break;

			/*
			 * Disassemble to get the needed info for the frame.
			 */
			if ((inst & 0xffff0000) == 0x23de0000)
				/* lda sp,n(sp) */
				framesize -= disp(inst) / sizeof (u_long);
			else if ((inst & 0xfc1f0000) == 0xb41e0000)
				/* stq X,n(sp) */
				slot[rega(inst)] =
				    frame + disp(inst) / sizeof (u_long);
			else if ((inst & 0xfc000fe0) == 0x44000400 &&
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
			} else if (inst_load(inst))
				/* clobbers a register */
				slot[rega(inst)] = 0;
			else if (opcode[inst >> 26].opc_fmt == OPC_OP)
				/* clobbers a register */
				slot[regc(inst)] = 0;
			/*
			 * XXX Recognize more reg clobbering instructions and
			 * set slot[reg] = 0 then too.
			 */
		}

		/*
		 * Try to print the 6 quads that might hold the args.
		 * We print 6 of them even if there are fewer, cause we don't
		 * know the number.  Maybe we could skip the last ones
		 * that never got used.  If we cannot know the value, print
		 * a question mark.
		 */
		for (i = 0; i < 6; i++) {
			if (i > 0)
				db_printf(", ");
			if (slot[16 + i])
				db_printf("%lx", *slot[16 + i]);
			else
				db_printf("?");
		}

#if 0
		/*
		 * XXX This will go eventually when I trust the argument
		 * printout heuristics.
		 *
		 * Print the stack frame contents.
		 */
		db_printf(") [%p: ", frame);
		if (framesize > 1) {
			for (i = 0; i < framesize - 1; i++)
				db_printf("%lx, ", frame[i]);
			db_printf("%lx", frame[i]);
		}
		db_printf("] at ");
#else
		db_printf(") at ");
#endif
		db_printsym(pc, DB_STGY_PROC);
		db_printf("\n");

		/*
		 * If we are looking at a Xent* routine we are in a trap
		 * context.
		 */
		if (strncmp(name, "Xent", sizeof("Xent") - 1) == 0) {
			regs = (db_regs_t *)frame;
			goto trapframe;
		}

		/* Look for the return address if recorded.  */
		if (slot[26])
			ra = *(db_addr_t *)slot[26];

		/* Advance to the next frame.  */
		frame += framesize;
		pc = ra;
	}
}
