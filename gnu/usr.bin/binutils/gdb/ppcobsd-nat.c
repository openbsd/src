/* Functions specific to running gdb native on a Powerpc System.
   Copyright (C) 1993, Free Software Foundation, Inc.

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
#include <sys/param.h>
#include <sys/signal.h>	/* for MAXSIG in sys/user.h */
#include <sys/types.h>	/* for ushort in sys/dir.h */
#include <sys/dir.h>	/* for struct direct in sys/user.h */
#include <sys/user.h>
#include <machine/reg.h>
#include "defs.h"
#include "inferior.h"
#include "target.h"
#include "gdbcore.h"

#include <nlist.h>

#if !defined (offsetof)
#define offsetof(TYPE, MEMBER) ((unsigned long) &((TYPE *)0)->MEMBER)
#endif

void
fetch_inferior_registers (regno)
     int regno;
{
  struct reg inferior_registers;

/* 
 * this gets fp and gpr?
 */

  ptrace (PT_GETREGS, inferior_pid,
	  (PTRACE_ARG3_TYPE) &inferior_registers, 0);
  memcpy (&registers, &inferior_registers,
	  sizeof(inferior_registers));


  registers_fetched ();
}

void
store_inferior_registers (regno)
     int regno;
{
  struct reg inferior_registers;


  memcpy (&inferior_registers, &registers,
	  sizeof(inferior_registers));
  ptrace (PT_SETREGS, inferior_pid,
	  (PTRACE_ARG3_TYPE) &inferior_registers, 0);

}
/* Return the address in the core dump or inferior of register REGNO.
   BLOCKEND is the address of the end of the user structure.  */

CORE_ADDR
register_addr (regno, blockend)
     int	regno;
     CORE_ADDR	blockend;
{
	int ppcreg[] = 
	{ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
	16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* fp 0-15 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* fp 16-31 */
	   36,    37,   33,    32,    35,    34 ,   0 };
	/* "pc", "ps", "cnd", "lr", "cnt", "xer", "mq" */
	/*
	32 lr
	33 cr
	34 xer
	35 ctr
	36 srr0
	37 srr1
	*/
  if (regno < NUM_REGS) {
    return (blockend + REGISTER_BYTE(regno));
  } else
    {
      fprintf_unfiltered (gdb_stderr, "\
Internal error: invalid register number %d in REGISTER_U_ADDR\n",
	       regno);
      return blockend;
    }
}
