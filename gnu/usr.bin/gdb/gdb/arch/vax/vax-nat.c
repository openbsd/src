/* Native-dependent code for BSD Unix running on vax's, for GDB.
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

        $Id: vax-nat.c,v 1.1.1.1 1995/10/18 08:40:09 deraadt Exp $
*/

#include <sys/types.h>
#include <sys/param.h>
#include <signal.h>
#include <sys/user.h>
#include <machine/reg.h>
#include <machine/trap.h>
#include <sys/ptrace.h>

#include "defs.h"
#include "inferior.h"

void
fetch_inferior_registers (regno)
     int regno;
{
  struct reg inferior_registers;

  ptrace (PT_GETREGS, inferior_pid,
          (PTRACE_ARG3_TYPE) &inferior_registers, 0);

  memcpy (&registers[REGISTER_BYTE (0)], &inferior_registers, 4*16);

  registers_fetched ();
}

void
store_inferior_registers (regno)
     int regno;
{
  struct reg inferior_registers;

  memcpy (&inferior_registers, &registers[REGISTER_BYTE (0)], 4*16);

  ptrace (PT_SETREGS, inferior_pid,
          (PTRACE_ARG3_TYPE) &inferior_registers, 0);
}

void
fetch_kcore_registers(pcb)
        struct pcb *pcb;
{
	printf("fetch_kcore_registers\n");
}

void
clear_regs()
{
	printf("clear_regs\n");
}


