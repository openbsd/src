/* Functions specific to running gdb native on an i386 running NetBSD
   Copyright 1989, 1992, 1993, 1994, 1996 Free Software Foundation, Inc.

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
#include <machine/frame.h>
#include <machine/pcb.h>

#include "defs.h"
#include "inferior.h"
#include "target.h"
#include "gdbcore.h"

void
fetch_inferior_registers (regno)
     int regno;
{
  struct reg inferior_registers;

  ptrace (PT_GETREGS, inferior_pid,
	  (PTRACE_ARG3_TYPE) &inferior_registers, 0);
  memcpy (&registers[REGISTER_BYTE (0)], &inferior_registers,
	  sizeof(inferior_registers));

  /* FIXME: FP regs? */
  registers_fetched ();
}

void
store_inferior_registers (regno)
     int regno;
{
  struct reg inferior_registers;

  memcpy (&inferior_registers, &registers[REGISTER_BYTE (0)],
	  sizeof(inferior_registers));
  ptrace (PT_SETREGS, inferior_pid,
	  (PTRACE_ARG3_TYPE) &inferior_registers, 0);

  /* FIXME: FP regs? */
}


/* XXX - Add this to machine/regs.h instead? */
struct md_core {
  struct reg intreg;
  struct fpreg freg;
};

static struct fpreg i386_fp_registers;
static int i386_fp_read = 0;

static void
fetch_core_registers (core_reg_sect, core_reg_size, which, reg_addr)
     char *core_reg_sect;
     unsigned core_reg_size;
     int which;
     unsigned int reg_addr;	/* Unused in this version */
{
  struct md_core *core_reg;

  core_reg = (struct md_core *)core_reg_sect;

  /* We get everything from the .reg section. */
  if (which != 0)
    return;

  if (core_reg_size < sizeof(struct reg)) {
    fprintf_unfiltered (gdb_stderr, "Couldn't read regs from core file\n");
    return;
  }

  /* Integer registers */
  memcpy(&registers[REGISTER_BYTE (0)],
	 &core_reg->intreg, sizeof(struct reg));

  /* Floating point registers */
  i386_fp_registers = core_reg->freg;
  i386_fp_read = 1;

  registers_fetched ();
}

/* Register that we are able to handle i386nbsd core file formats.
   FIXME: is this really bfd_target_unknown_flavour? */

static struct core_fns nat_core_fns =
{
  bfd_target_unknown_flavour,
  fetch_core_registers,
  NULL
};

void
_initialize_i386nbsd_nat ()
{
  add_core_fns (&nat_core_fns);
}


/*
 * kernel_u_size() is not helpful on NetBSD because
 * the "u" struct is NOT in the core dump file.
 */

#ifdef	FETCH_KCORE_REGISTERS
/*
 * Get registers from a kernel crash dump or live kernel.
 * Called by kcore-nbsd.c:get_kcore_registers().
 */
void
fetch_kcore_registers (pcb)
     struct pcb *pcb;
{
  int i, regno, regs[4];

  /*
   * get the register values out of the sys pcb and
   * store them where `read_register' will find them.
   */
  if (target_read_memory(pcb->pcb_tss.tss_esp+4,
			 (char *)regs, sizeof(regs)))
    error("Cannot read ebx, esi, and edi.");
  for (i = 0, regno = 0; regno < 3; regno++)
    supply_register(regno, (char *)&i);
  supply_register(3, (char *)&regs[2]);
  supply_register(4, (char *)&pcb->pcb_tss.tss_esp);
  supply_register(5, (char *)&pcb->pcb_tss.tss_ebp);
  supply_register(6, (char *)&regs[1]);
  supply_register(7, (char *)&regs[0]);
  supply_register(8, (char *)&regs[3]);
  for (i = 0, regno = 9; regno < 10; regno++)
    supply_register(regno, (char *)&i);
#if 0
  i = 0x08;
  supply_register(10, (char *)&i);
  i = 0x10;
  supply_register(11, (char *)&i);
#endif

  /* The kernel does not use the FPU, so ignore it. */
  registers_fetched ();
}
#endif	/* FETCH_KCORE_REGISTERS */

#ifdef FLOAT_INFO
#include "language.h"			/* for local_hex_string */
#include "floatformat.h"

#include <sys/param.h>
#include <sys/dir.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <fcntl.h>

#include <a.out.h>

#include <sys/time.h>
#include <sys/resource.h>
#include <sys/uio.h>
#define curpcb Xcurpcb	/* XXX avoid leaking declaration from pcb.h */
#include <sys/user.h>
#undef curpcb
#include <sys/file.h>
#include "gdb_stat.h"
#include <sys/ptrace.h>

extern void print_387_control_word ();		/* i387-tdep.h */
extern void print_387_status_word ();

struct env387 
{
  unsigned short control;
  unsigned short r0;
  unsigned short status;
  unsigned short r1;
  unsigned short tag;
  unsigned short r2;
  unsigned long eip;
  unsigned short code_seg;
  unsigned short opcode;
  unsigned long operand;
  unsigned short operand_seg;
  unsigned short r3;
  unsigned char regs[8][10];
};

static void
print_387_status (status, ep)
     unsigned short status;
     struct env387 *ep;
{
  int i;
  int bothstatus;
  int top;
  int fpreg;
  
  bothstatus = ((status != 0) && (ep->status != 0));
  if (status != 0) 
    {
      if (bothstatus)
	printf_unfiltered ("u: ");
      print_387_status_word ((unsigned int)status);
    }
  
  if (ep->status != 0) 
    {
      if (bothstatus)
	printf_unfiltered ("e: ");
      print_387_status_word ((unsigned int)ep->status);
    }
  
  print_387_control_word ((unsigned int)ep->control);
  printf_unfiltered ("last exception: ");
  printf_unfiltered ("opcode %s; ", local_hex_string(ep->opcode));
  printf_unfiltered ("pc %s:", local_hex_string(ep->code_seg));
  printf_unfiltered ("%s; ", local_hex_string(ep->eip));
  printf_unfiltered ("operand %s", local_hex_string(ep->operand_seg));
  printf_unfiltered (":%s\n", local_hex_string(ep->operand));

  top = (ep->status >> 11) & 7;
  
  printf_unfiltered ("regno     tag  msb              lsb  value\n");
  for (fpreg = 7; fpreg >= 0; fpreg--) 
    {
      double val;
      
      printf_unfiltered ("%s %d: ", fpreg == top ? "=>" : "  ", fpreg); 

      switch ((ep->tag >> (fpreg * 2)) & 3) 
	{
	case 0: printf_unfiltered ("valid "); break;
	case 1: printf_unfiltered ("zero  "); break;
	case 2: printf_unfiltered ("trap  "); break;
	case 3: printf_unfiltered ("empty "); break;
	}
      for (i = 9; i >= 0; i--)
	printf_unfiltered ("%02x", ep->regs[fpreg][i]);
      
      floatformat_to_double(&floatformat_i387_ext, (char *) ep->regs[fpreg], 
			      &val);
      printf_unfiltered ("  %g\n", val);
    }
}

i386_float_info ()
{
  extern int inferior_pid;
  
  if (inferior_pid) 
    {
      ptrace (PT_GETFPREGS, inferior_pid, (PTRACE_ARG3_TYPE) &i386_fp_registers,
	      0);
    } 
  else if (!i386_fp_read)
    {
      error ("The program has no floating point registers now.");
    }
  
  print_387_status (0, (struct env387 *) &i386_fp_registers);
}
#endif
