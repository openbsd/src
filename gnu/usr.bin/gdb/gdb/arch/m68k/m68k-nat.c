/*	$OpenBSD: m68k-nat.c,v 1.2 1996/03/30 15:29:56 niklas Exp $	*/

/* Native-dependent code for BSD Unix running on i386's, for GDB.
   Copyright 1988, 1989, 1991, 1992 Free Software Foundation, Inc.

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
Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include <sys/types.h>
#include <sys/param.h>
#include <signal.h>
#include <sys/user.h>
#include <machine/reg.h>
#include <sys/ptrace.h>

#include "defs.h"
#include "inferior.h"

void
fetch_inferior_registers (regno)
     int regno;
{
  struct reg inferior_registers;
  struct fpreg inferior_fp_registers;

  ptrace (PT_GETREGS, inferior_pid,
	  (PTRACE_ARG3_TYPE) &inferior_registers, 0);

  memcpy (&registers[REGISTER_BYTE (0)], &inferior_registers, 4*18);

  ptrace (PT_GETFPREGS, inferior_pid,
	  (PTRACE_ARG3_TYPE) &inferior_fp_registers, 0);

  memcpy (&registers[REGISTER_BYTE (18)], &inferior_fp_registers, 8*12+4*3);

  registers_fetched ();
}

void
store_inferior_registers (regno)
     int regno;
{
  struct reg inferior_registers;
  struct fpreg inferior_fp_registers;

  memcpy (&inferior_registers, &registers[REGISTER_BYTE (0)], 4*18);

  ptrace (PT_SETREGS, inferior_pid,
	  (PTRACE_ARG3_TYPE) &inferior_registers, 0);

  memcpy (&inferior_fp_registers, &registers[REGISTER_BYTE (18)], 8*12+4*3);

  ptrace (PT_SETFPREGS, inferior_pid,
	  (PTRACE_ARG3_TYPE) &inferior_fp_registers, 0);
}

/* XXX - Add this to machine/regs.h instead? */
struct md_core {
	struct reg intreg;
	struct fpreg freg;
};
/* Get registers from a core file. */
void
fetch_core_registers (core_reg_sect, core_reg_size, which, ignore)
  char *core_reg_sect;
  unsigned core_reg_size;
  int which;
  unsigned int ignore;	/* reg addr, unused in this version */
{
  struct md_core *core_reg;

  core_reg = (struct md_core *)core_reg_sect;

  /* Integer registers */
  memcpy(&registers[REGISTER_BYTE (0)],
	&core_reg->intreg, sizeof(struct reg));
  /* Floating point registers */
  memcpy(&registers[REGISTER_BYTE (FP0_REGNUM)],
	&core_reg->freg, sizeof(struct fpreg));
}

/* Get registers from a kernel crash dump. */
void
fetch_kcore_registers(pcb)
struct pcb *pcb;
{
	int i, *ip, tmp=0;

	/* D0,D1 */
	ip = &tmp;
	supply_register(0, (char *)ip);
	supply_register(1, (char *)ip);
	/* D2-D7 */
	ip = &pcb->pcb_regs[0];
	for (i = 2; i < 8; i++, ip++)
		supply_register(i, (char *)ip);

	/* A0,A1 */
	ip = &tmp;
	supply_register(8, (char *)ip);
	supply_register(9, (char *)ip);
	/* A2-A7 */
	ip = &pcb->pcb_regs[6];
	for (i = 10; i < 16; i++, (char *)ip++)
		supply_register(i, (char *)ip);

	/* PC (use return address) */
	tmp = pcb->pcb_regs[10] + 4;
	if (!target_read_memory(tmp, &tmp, sizeof(tmp), 0))
		supply_register(PC_REGNUM, (char *)&tmp);

	supply_register(PS_REGNUM, (char *)&pcb->pcb_ps);

	/* FP0-FP7 */
	ip = &pcb->pcb_fpregs.fpf_regs[0];
	for (i = 0; i < 8; ++i, ip+=3)
		supply_register(FP0_REGNUM+i, (char *)ip);

	/* FPCR, FPSR, FPIAR */
	supply_register(FPC_REGNUM, (char *)ip++);
	supply_register(FPS_REGNUM, (char *)ip++);
	supply_register(FPI_REGNUM, (char *)ip++);

	return;
}

void
clear_regs()
{
	u_long reg = 0;
	float freg = 0.0;
	int i = 0;

	for (; i < FP0_REGNUM; i++)
		supply_register(i, (char *)&reg);
	for (; i < FPC_REGNUM; i++)
		supply_register(i, (char *)&freg);
	for (; i < NUM_REGS; i++)
		supply_register(i, (char *)&reg);
}
