/* Low level Alpha interface, for GDB when running native.
   Copyright 1993, 1995 Free Software Foundation, Inc.

This file is part of GDB.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

#include "defs.h"
#include "inferior.h"
#include "gdbcore.h"
#include "target.h"
#include <sys/ptrace.h>
#include <machine/reg.h>
#include <machine/frame.h>
#include <machine/pcb.h>
#include <string.h>

/* Size of elements in jmpbuf */

#define JB_ELEMENT_SIZE 8

/* The definition for JB_PC in machine/reg.h is wrong.
   And we can't get at the correct definition in setjmp.h as it is
   not always available (eg. if _POSIX_SOURCE is defined which is the
   default). As the defintion is unlikely to change (see comment
   in <setjmp.h>, define the correct value here.  */

#undef JB_PC
#define JB_PC 2

/* Figure out where the longjmp will land.
   We expect the first arg to be a pointer to the jmp_buf structure from which
   we extract the pc (JB_PC) that we will land at.  The pc is copied into PC.
   This routine returns true on success. */

int
get_longjmp_target (pc)
     CORE_ADDR *pc;
{
  CORE_ADDR jb_addr;
  char raw_buffer[MAX_REGISTER_RAW_SIZE];

  jb_addr = read_register(A0_REGNUM);

  if (target_read_memory(jb_addr + JB_PC * JB_ELEMENT_SIZE, raw_buffer,
			 sizeof(CORE_ADDR)))
    return 0;

  *pc = extract_address (raw_buffer, sizeof(CORE_ADDR));
  return 1;
}

/* Extract the register values out of the core file and store
   them where `read_register' will find them.

   CORE_REG_SECT points to the register values themselves, read into memory.
   CORE_REG_SIZE is the size of that area.
   WHICH says which set of registers we are handling (0 = int, 2 = float
         on machines where they are discontiguous).
   REG_ADDR is the offset from u.u_ar0 to the register values relative to
            core_reg_sect.  This is used with old-fashioned core files to
	    locate the registers in a large upage-plus-stack ".reg" section.
	    Original upage address X is at location core_reg_sect+x+reg_addr.
 */

#define oi(name) \
	    offsetof(struct md_coredump, md_tf.tf_regs[__CONCAT(FRAME_,name)])
#define of(num) \
	    offsetof(struct md_coredump, md_fpstate.fpr_regs[num])

void
fetch_core_registers (core_reg_sect, core_reg_size, which, reg_addr)
     char *core_reg_sect;
     unsigned core_reg_size;
     int which;
     unsigned reg_addr;
{
  register int regno;
  register int addr;
  int bad_reg = -1;
  static char zerobuf[MAX_REGISTER_RAW_SIZE] = {0};
  int regoff[NUM_REGS] = {
      oi(V0),  oi(T0),  oi(T1),  oi(T2),  oi(T3),  oi(T4),  oi(T5),  oi(T6),
      oi(T7),  oi(S0),  oi(S1),  oi(S2),  oi(S3),  oi(S4),  oi(S5),  oi(S6),
      oi(A0),  oi(A1),  oi(A2),  oi(A3),  oi(A4),  oi(A5),  oi(T8),  oi(T9), 
      oi(T10), oi(T11), oi(RA),  oi(T12), oi(AT),  oi(GP),  oi(SP),  -1,
      of(0),   of(1),   of(2),   of(3),   of(4),   of(5),   of(6),   of(7),
      of(8),   of(9),   of(10),  of(11),  of(12),  of(13),  of(14),  of(15),
      of(16),  of(17),  of(18),  of(19),  of(20),  of(21),  of(22),  of(23),  
      of(24),  of(25),  of(26),  of(27),  of(28),  of(29),  of(30),  of(31),
      oi(PC),  -1,
  };

  for (regno = 0; regno < NUM_REGS; regno++)
    {
      if (CANNOT_FETCH_REGISTER (regno))
	{
	  supply_register (regno, zerobuf);
	  continue;
	}
      addr = regoff[regno];
      if (addr < 0 || addr >= core_reg_size)
	{
	  if (bad_reg < 0)
	    bad_reg = regno;
	}
      else
	{
	  supply_register (regno, core_reg_sect + addr);
	}
    }
  if (bad_reg >= 0)
    {
      error ("Register %s not found in core file.", reg_names[bad_reg]);
    }
}

register_t
rrf_to_register(regno, reg, fpreg)
	int regno;
	struct reg *reg;
	struct fpreg *fpreg;
{

	if (regno < 0)
		abort();
	else if (regno < FP0_REGNUM)
		return (reg->r_regs[regno]);
	else if (regno == PC_REGNUM)
		return (reg->r_regs[R_ZERO]);
	else if (regno >= FP0_REGNUM)
		return (fpreg->fpr_regs[regno - FP0_REGNUM]);
	else
		abort();
}

void
fetch_inferior_registers (regno)
	int regno;
{
	struct reg reg;
	struct fpreg fpreg;
	register_t regval;
	static char zerobuf[MAX_REGISTER_RAW_SIZE] = {0};
	char *rp;

	ptrace(PT_GETREGS, inferior_pid, (PTRACE_ARG3_TYPE)&reg, 0);
	ptrace(PT_GETFPREGS, inferior_pid, (PTRACE_ARG3_TYPE)&fpreg, 0);

	if (regno < 0) {
		for (regno = 0; regno < NUM_REGS; regno++) {
			if (CANNOT_FETCH_REGISTER (regno))
				rp = zerobuf;
			else {
				regval = rrf_to_register(regno, &reg, &fpreg);
				rp = (char *)&regval;
			}
			supply_register(regno, rp);
		}
	} else {
		if (CANNOT_FETCH_REGISTER (regno))
			rp = zerobuf;
		else {
			regval = rrf_to_register(regno, &reg, &fpreg);
			rp = (char *)&regval;
		}

		supply_register(regno, rp);
	}
}

void
register_into_rrf(val, regno, reg, fpreg)
	register_t val;
	int regno;
	struct reg *reg;
	struct fpreg *fpreg;
{

	if (regno < 0)
		abort();
	else if (regno < FP0_REGNUM)
		reg->r_regs[regno] = val;
	else if (regno == PC_REGNUM)
		reg->r_regs[R_ZERO] = val;
	else if (regno >= FP0_REGNUM)
		fpreg->fpr_regs[regno - FP0_REGNUM] = val;
	else
		abort();
}

void
store_inferior_registers (regno)
	int regno;
{
	struct reg reg;
	struct fpreg fpreg;
	register_t regval;

	if (regno < 0) {
		for (regno = 0; regno < NUM_REGS; regno++) {
			if (CANNOT_STORE_REGISTER (regno))
				continue;
	
			if (REGISTER_RAW_SIZE (regno) != sizeof regval)
				abort();
			memcpy(&regval, &registers[REGISTER_BYTE (regno)],
			    REGISTER_RAW_SIZE (regno));
			register_into_rrf(regval, regno, &reg, &fpreg);
		}
	} else {
		ptrace(PT_GETREGS, inferior_pid, (PTRACE_ARG3_TYPE)&reg, 0);
		ptrace(PT_GETFPREGS, inferior_pid, (PTRACE_ARG3_TYPE)&fpreg, 0);
		
		memcpy(&regval, &registers[REGISTER_BYTE (regno)],
		    REGISTER_RAW_SIZE (regno));
		register_into_rrf(regval, regno, &reg, &fpreg);
	}
	
	ptrace(PT_SETREGS, inferior_pid, (PTRACE_ARG3_TYPE)&reg, 0);
	ptrace(PT_SETFPREGS, inferior_pid, (PTRACE_ARG3_TYPE)&fpreg, 0);
}

void 
child_resume (pid, step, signal)
	int pid;
	int step;
	enum target_signal signal;
{    

  errno = 0;

  if (pid == -1)
    /* Resume all threads.  */
    /* I think this only gets used in the non-threaded case, where "resume
       all threads" and "resume inferior_pid" are the same.  */
    pid = inferior_pid;

  /* An address of (PTRACE_ARG3_TYPE)1 tells ptrace to continue from where
     it was.  (If GDB wanted it to start some other way, we have already
     written a new PC value to the child.)

     If this system does not support PT_STEP, a higher level function will
     have called single_step() to transmute the step request into a
     continue request (by setting breakpoints on all possible successor
     instructions), so we don't have to worry about that here.  */

  if (step)
	abort();
  else
    ptrace (PT_CONTINUE, pid, (PTRACE_ARG3_TYPE) 1,
	    target_signal_to_host (signal));

  if (errno)
    perror_with_name ("ptrace");
}

/* Register that we are able to handle alpha core file formats. */

static struct core_fns alpha_core_fns =
{
  bfd_target_coff_flavour,
  fetch_core_registers,
  NULL
};

void
_initialize_core_alpha ()
{
  add_core_fns (&alpha_core_fns);
}
