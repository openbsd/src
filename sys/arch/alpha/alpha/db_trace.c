/*	$OpenBSD: db_trace.c,v 1.3 1997/07/20 06:58:57 niklas Exp $	*/

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

/*
 * XXX There are a couple of problems with this code:
 *
 *	The argument lists are *not* printed, rather we print the contents
 *	of the stack frame slots.
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
	int		i, framesize, ra_index;
	db_addr_t	pc, ra;
	u_int		inst;
	char		*name;
	db_expr_t	offset;
	db_regs_t	*regs;

	if (count == -1)
		count = 65535;

	regs = have_addr ? (db_regs_t *)addr : DDB_REGS;
trapframe:
	frame = (u_long *)regs->tf_regs[FRAME_SP];
	pc = (db_addr_t)regs->tf_regs[FRAME_PC];
	ra = (db_addr_t)regs->tf_regs[FRAME_RA];
db_printf("%p %p %lx %lx\n", regs, frame, pc, ra);

	while (count-- && pc >= (db_addr_t)KERNBASE && pc < (db_addr_t)&etext) {
		db_find_sym_and_offset(pc, &name, &offset);
		if (!name) {
			name = "?";
			/* Limit the search for procedure start */
			offset = 65536;
		}
		db_printf("%s(", name);

		framesize = 0;
		ra_index = -1;
		for (i = sizeof(int); i <= offset; i += sizeof(int)) {
			inst = *(u_int *)(pc - i);
	
			/*
			 * If by chance we don't have any symbols we have to
			 * get out somehow anyway.  Check for the preceding
			 * procedure return in that case.
			 */
			if (name[0] == '?' && inst_return(inst))
				break;

			/*
			 * Disassemble to get the needed info for the frame 
			 * size calculation.
			 */
			if ((inst & 0xffff0000) == 0x23de0000)
				/* lda sp,-n(sp) */
				if (inst & 0x8000)
					framesize += (-inst) & 0xffff;
				else
					framesize -= inst & 0x7fff;
			else if ((inst & 0xffff8000) == 0xb75e0000)
				/* stq ra,+n(sp) */
				ra_index = (inst & 0x7fff) >> 3;
			else if (inst == 0x47de040f) {
				/* XXX bis sp,sp,fp : has an fp register */
			}
			/*
			 * XXX recognize argument saving instructions too.
			 */
		}

		/*
		 * Print the stack THINGS.  It's practically impossible
		 * to get at the arguments without *lots* of work, and even
		 * then it might not be possible at all.
		 */
		if ((framesize >> 3) > 1) {
			for (i = 0; i < (framesize >> 3) - 1; i++)
				db_printf("%lx, ", frame[i]);
			db_printf("%lx", frame[i]);
		}
		db_printf(") at ");
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
		if (ra_index >= 0)
			ra = (db_addr_t)frame[ra_index];
		frame = (u_long *)((vm_offset_t)frame + framesize);
		pc = ra;
	}
}
