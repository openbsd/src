/* run front end support for arm
   Copyright (C) 1995 Free Software Foundation, Inc.

This file is part of ARM SIM

GNU CC is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GNU CC is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

/* This file provides the interface between the simulator and run.c and gdb
   (when the simulator is linked with gdb).
   All simulator interaction should go through this file.

   Functions that begin with sim_ belong to the standard simulator interface.
   Functions that begin with ARMul_ belong to the ARM simulator.
   Functions that begin with arm_sim_ are additional functions necessary to
   implement the interface.
*/

#include <stdio.h>
#include <stdarg.h>
#include <bfd.h>
#include <signal.h>
#include "callback.h"
#include "remote-sim.h"
#include "armdefs.h"
#include "armemu.h"
#include "dbg_rdi.h"

static struct ARMul_State *state;

/* Memory size (log2 (n)).  */
static int mem_size = 21;

/* Non-zero to display start up banner, and maybe other things.  */
static int verbosity;

static void 
init ()
{
  static int done;

  if (!done)
    {
      ARMul_EmulateInit();
      state = ARMul_NewState ();
      ARMul_MemoryInit(state, 1 << mem_size);
      ARMul_OSInit(state);
      ARMul_CoProInit(state); 
      state->verbose = verbosity;
      done = 1;
    }
}

/* Must be called before initializing simulator.  */

void
arm_sim_set_verbosity (v)
     int v;
{
  verbosity = v;
}

/* Must be called before initializing simulator.  */

void 
arm_sim_set_mem_size (size)
     int size;
{
  mem_size = size;
}

void 
arm_sim_set_profile ()
{
}

void 
arm_sim_set_profile_size ()
{
}

void 
ARMul_ConsolePrint (ARMul_State * state, const char *format,...)
{
  va_list ap;

  if (state->verbose)
    {
      va_start (ap, format);
      vprintf (format, ap);
      va_end (ap);
    }
}

ARMword 
ARMul_Debug (ARMul_State * state, ARMword pc, ARMword instr)
{

}

int
sim_write (addr, buffer, size)
     SIM_ADDR addr;
     unsigned char *buffer;
     int size;
{
  int i;
  init ();
  for (i = 0; i < size; i++)
    {
      ARMul_WriteByte (state, addr+i, buffer[i]);
    }
  return size;
}

int
sim_read (addr, buffer, size)
     SIM_ADDR addr;
     unsigned char *buffer;
     int size;
{
  int i;
  init ();
  for (i = 0; i < size; i++)
    {
      buffer[i] = ARMul_ReadByte (state, addr + i);
    }
  return size;
}

void 
sim_trace ()
{
}

void
sim_resume (step, siggnal)
     int step, siggnal;
{
  state->EndCondition = 0;

  if (step)
    {
      state->Reg[15] = ARMul_DoInstr (state);
      if (state->EndCondition == 0)
	state->EndCondition = RDIError_BreakpointReached;
    }
  else
    {
      state->Reg[15] = ARMul_DoProg (state);
    }

  FLUSHPIPE;
}

void
sim_create_inferior (start_address, argv, env)
     SIM_ADDR start_address;
     char **argv;
     char **env;
{
  ARMul_SetPC(state, start_address);
}

void
sim_info (verbose)
     int verbose;
{
}


static int 
frommem (state, memory)
     struct ARMul_State *state;
     unsigned char *memory;
{
  if (state->bigendSig == HIGH)
    {
      return (memory[0] << 24)
	| (memory[1] << 16)
	| (memory[2] << 8)
	| (memory[3] << 0);
    }
  else
    {
      return (memory[3] << 24)
	| (memory[2] << 16)
	| (memory[1] << 8)
	| (memory[0] << 0);
    }
}


static void
tomem (state, memory,  val)
     struct ARMul_State *state;
     unsigned char *memory;
     int val;
{
  if (state->bigendSig == HIGH)
    {
      memory[0] = val >> 24;
      memory[1] = val >> 16;
      memory[2] = val >> 8;
      memory[3] = val >> 0;
    }
  else
    {
      memory[3] = val >> 24;
      memory[2] = val >> 16;
      memory[1] = val >> 8;
      memory[0] = val >> 0;
    }
}

void
sim_store_register (rn, memory)
     int rn;
     unsigned char *memory;
{
  init ();
  ARMul_SetReg(state, state->Mode, rn, frommem (state, memory));
}

void
sim_fetch_register (rn, memory)
     int rn;
     unsigned char *memory;
{
  init ();
  tomem (state, memory, ARMul_GetReg(state, state->Mode, rn));
}




void
sim_open (name)
     char *name;
{
  /* nothing to do */
}

void
sim_close (quitting)
     int quitting;
{
  /* nothing to do */
}

int
sim_load (prog, from_tty)
     char *prog;
     int from_tty;
{
  /* Return nonzero so GDB will handle it.  */
  return 1;
}

void
sim_stop_reason (reason, sigrc)
     enum sim_stop *reason;
     int *sigrc;
{
  if (state->EndCondition == 0)
    {
      *reason = sim_exited;
      *sigrc = state->Reg[0] & 255;
    }
  else
    {
      *reason = sim_stopped;
      if (state->EndCondition == RDIError_BreakpointReached)
	*sigrc = SIGTRAP;
      else
	*sigrc = 0;
    }
}

void
sim_kill ()
{
  /* nothing to do */
}

void
sim_do_command (cmd)
     char *cmd;
{
  printf_filtered ("This simulator does not accept any commands.\n");
}


void
sim_set_callbacks (ptr)
struct host_callback_struct *ptr;
{

}
