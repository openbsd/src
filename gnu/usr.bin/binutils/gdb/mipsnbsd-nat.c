/* Functions specific to running gdb native on a mips running NetBSD
   Copyright  1997 Free Software Foundation, Inc.
   Contributed by Jonathan Stone(jonathan@dsg.stanford.edu) at Stanford

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
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#include <sys/types.h>
#include <sys/ptrace.h>
#include <machine/reg.h>
#include <machine/pcb.h>
#include <setjmp.h>

#include "defs.h"
#include "inferior.h"
#include "target.h"
#include "gdbcore.h"

#define JB_ELEMENT_SIZE 4

void
fetch_inferior_registers (regno)
     int regno;
{
  struct reg inferior_registers;
  struct fpreg inferior_fp_registers;

  bzero(&inferior_registers, sizeof(inferior_registers));
  ptrace (PT_GETREGS, inferior_pid,
	  (PTRACE_ARG3_TYPE) &inferior_registers, 0);

  memcpy (&registers[REGISTER_BYTE (0)], 
	  &inferior_registers, sizeof(inferior_registers));

  bzero(&inferior_fp_registers, sizeof(inferior_fp_registers));
  ptrace (PT_GETFPREGS, inferior_pid,
	  (PTRACE_ARG3_TYPE) &inferior_fp_registers, 0);

  memcpy (&registers[REGISTER_BYTE (FP0_REGNUM)],
	  &inferior_fp_registers, sizeof(struct fpreg));

  registers_fetched ();
}

void
store_inferior_registers (regno)
     int regno;
{
  struct reg inferior_registers;
  struct fpreg inferior_fp_registers;

  memcpy (&inferior_registers, &registers[REGISTER_BYTE (0)], 
	  sizeof(inferior_registers));

  ptrace (PT_SETREGS, inferior_pid,
	  (PTRACE_ARG3_TYPE) &inferior_registers, 0);

  memcpy (&inferior_fp_registers, &registers[REGISTER_BYTE (FP0_REGNUM)],
	  sizeof(inferior_fp_registers));

  ptrace (PT_SETFPREGS, inferior_pid,
	  (PTRACE_ARG3_TYPE) &inferior_fp_registers, 0);
}


/* Figure out where the longjmp will land.
   We expect the first arg to be a pointer to the jmp_buf structure from which
   we extract the pc (JB_PC) that we will land at.  The pc is copied into PC.
   This routine returns true on success. */

int
get_longjmp_target(pc)
     CORE_ADDR *pc;
{
  CORE_ADDR jb_addr;
  char buf[TARGET_PTR_BIT / TARGET_CHAR_BIT];

  jb_addr = read_register (A0_REGNUM);

  if (target_read_memory (jb_addr + JB_PC * JB_ELEMENT_SIZE, buf,
			  TARGET_PTR_BIT / TARGET_CHAR_BIT))
    return 0;

  *pc = extract_address (buf, TARGET_PTR_BIT / TARGET_CHAR_BIT);

  return 1;
}


/* XXX - Add this to machine/regs.h instead? */
struct md_core {
	struct reg intreg;
	struct fpreg freg;
};


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
void
fetch_core_registers (core_reg_sect, core_reg_size, which, ignore)
  char *core_reg_sect;
  unsigned core_reg_size;
  int which;
  unsigned int ignore;	/* reg addr, unused in this version */
{
  struct md_core *core_reg;

  core_reg = (struct md_core *)core_reg_sect;

  if (which == 0) {
    /* Integer registers */
    memcpy(&registers[REGISTER_BYTE (0)],
	   &core_reg->intreg, sizeof(struct reg));
  }

  else if (which == 2) {
    /* Floating point registers */
    memcpy(&registers[REGISTER_BYTE (FP0_REGNUM)],
	   &core_reg->freg, sizeof(struct fpreg));
  }
}

#ifdef	FETCH_KCORE_REGISTERS
/* Get registers from a kernel crash dump. 
   FIXME: NetBSD 1.3 does not produce kernel crashdumps.  */
void
fetch_kcore_registers(pcb)
struct pcb *pcb;
{
  int i, *ip, tmp=0;
  u_long sp;

#if 0
  supply_register(SP_REGNUM, (char *)&pcb->pcb_sp);
  supply_register(PC_REGNUM, (char *)&pcb->pcb_pc);
#endif

  /* The kernel does not use the FPU, so ignore it. */
  registers_fetched ();
}
#endif	/* FETCH_KCORE_REGISTERS */


/* Register that we are able to handle core file formats.
   FIXME: is this really bfd_target_unknown_flavour? */

static struct core_fns netbsd_core_fns =
{
  bfd_target_unknown_flavour,
  fetch_core_registers,
  NULL
};

void
_initialize_mipsbsd_nat ()
{
  add_core_fns (&netbsd_core_fns);
}
