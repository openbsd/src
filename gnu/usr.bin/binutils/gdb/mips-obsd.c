/* Low level DECstation interface to ptrace, for GDB when running native.
   Copyright 1988, 1989, 1991, 1992, 1995 Free Software Foundation, Inc.
   Contributed by Alessandro Forin(af@cs.cmu.edu) at CMU
   and by Per Bothner(bothner@cs.wisc.edu) at U.Wisconsin.

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

#include "defs.h"
#include "inferior.h"
#include "gdbcore.h"
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/user.h>
#include <machine/reg.h>
#include <machine/regnum.h>
#undef JB_S0
#undef JB_S1
#undef JB_S2
#undef JB_S3
#undef JB_S4
#undef JB_S5
#undef JB_S6
#undef JB_S7
#undef JB_SP
#undef JB_S8
#undef JB_PC
#undef JB_SR
#undef NJBREGS
#include <setjmp.h>		/* For JB_XXX.  */

/* Size of elements in jmpbuf */

#define JB_ELEMENT_SIZE 4
#define	JB_PC 2

/* Map gdb internal register number to ptrace ``address''.
   These ``addresses'' are defined in DECstation <sys/ptrace.h> */
#define GPR_BASE 0

#define REGISTER_PTRACE_ADDR(regno) \
   (regno < 32 ? 		GPR_BASE + regno \
  : regno == PC_REGNUM ?	PC	\
  : regno == CAUSE_REGNUM ?	CAUSE	\
  : regno == HI_REGNUM ?	MULHI	\
  : regno == LO_REGNUM ?	MULLO	\
  : regno == FCRCS_REGNUM ?	FSR	\
  : regno == FCRIR_REGNUM ?	FSR	\
  : regno >= FP0_REGNUM ?	FPBASE + (regno - FP0_REGNUM) \
  : 0)

static char zerobuf[MAX_REGISTER_RAW_SIZE] = {0};

/* Get all registers from the inferior */

void
fetch_inferior_registers (regno)
     int regno;
{
  register unsigned int regaddr;
  struct reg regs;
  register int i;

  registers_fetched ();

  i = ptrace (PT_GETREGS, inferior_pid, (PTRACE_ARG3_TYPE) &regs, 0);
  for (regno = 1; regno < F31; regno++)
    {
      supply_register (regno, &regs.r_regs[regno]);
    }

  supply_register (ZERO_REGNUM, zerobuf);
  /* Frame ptr reg must appear to be 0; it is faked by stack handling code. */
  supply_register (FP_REGNUM, zerobuf);
}

/* Store our register values back into the inferior.
   If REGNO is -1, do this for all registers.
   Otherwise, REGNO specifies which register (so we can save time).  */

void
store_inferior_registers (regno)
     int regno;
{
  register unsigned int regaddr;
  struct reg regs;
  char buf[80];

  for(regaddr = 0; regaddr < F31; regaddr++) {
    regs.r_regs[regaddr] = read_register (regaddr);
  }
  errno = 0;
  ptrace (PT_SETREGS, inferior_pid, (PTRACE_ARG3_TYPE) &regs, 0);
  if (errno != 0)
  {
    sprintf (buf, "writing register number %d", regno);
    perror_with_name (buf);
  }
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

static void
fetch_core_registers (core_reg_sect, core_reg_size, which, reg_addr)
     char *core_reg_sect;
     unsigned core_reg_size;
     int which;
     unsigned reg_addr;
{
  register int regno;
  register unsigned int addr;
  int bad_reg = -1;
  register reg_ptr = -reg_addr;		/* Original u.u_ar0 is -reg_addr. */

  for (regno = 0; regno < NUM_REGS; regno++)
    {
      addr = (regno * sizeof(int) + reg_ptr);
      if (addr >= core_reg_size) {
	if (bad_reg < 0)
	  bad_reg = regno;
      } else {
	supply_register (regno, core_reg_sect + addr);
      }
    }
  if (bad_reg >= 0)
    {
      error ("Register %s not found in core file.", reg_names[bad_reg]);
    }
  supply_register (ZERO_REGNUM, zerobuf);
  /* Frame ptr reg must appear to be 0; it is faked by stack handling code. */
  supply_register (FP_REGNUM, zerobuf);
}


/* Register that we are able to handle mips core file formats.
   FIXME: is this really bfd_target_unknown_flavour? */

static struct core_fns mips_core_fns =
{
  bfd_target_unknown_flavour,
  fetch_core_registers,
  NULL
};

void
_initialize_core_mips ()
{
  add_core_fns (&mips_core_fns);
}
