/* gdb->simulator interface.
   Copyright (C) 1992, 1993, 1994 Free Software Foundation, Inc.

This file is part of Z8KSIM

Z8KSIM is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

Z8KSIM is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Z8KZIM; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#include <ansidecl.h>
#include "sim.h"
#include "tm.h"
#include "signal.h"
#include "callback.h"
#include "../../gdb/remote-sim.h"

void
sim_store_register (regno, value)
     int regno;
     unsigned char *value;
{
  /* FIXME: Review the computation of regval.  */
  int regval = (value[0] << 24) | (value[1] << 16) | (value[2] << 8) | value[3];

  tm_store_register (regno, regval);
}

void
sim_fetch_register (regno, buf)
     int regno;
     unsigned char *buf;
{
  tm_fetch_register (regno, buf);
}

int
sim_write (where, what, howmuch)
     SIM_ADDR where;
     unsigned char *what;
     int howmuch;
{
  int i;

  for (i = 0; i < howmuch; i++)
    tm_write_byte (where + i, what[i]);
  return howmuch;
}

int
sim_read (where, what, howmuch)
     SIM_ADDR where;
     unsigned char *what;
     int howmuch;
{
  int i;

  for (i = 0; i < howmuch; i++)
    what[i] = tm_read_byte (where + i);
  return howmuch;
}

static void 
control_c (sig, code, scp, addr)
     int sig;
     int code;
     char *scp;
     char *addr;
{
  tm_exception (SIM_INTERRUPT);
}

void
sim_resume (step, sig)
     int step;
     int sig;
{
  void (*prev) ();

  prev = signal (SIGINT, control_c);
  tm_resume (step);
  signal (SIGINT, prev);
}

void
sim_stop_reason (reason, sigrc)
     enum sim_stop *reason;
     int *sigrc;
{
  switch (tm_signal ())
    {
    case SIM_DIV_ZERO:
      *sigrc = SIGFPE;
      break;
    case SIM_INTERRUPT:
      *sigrc = SIGINT;
      break;
    case SIM_BAD_INST:
      *sigrc = SIGILL;
      break;
    case SIM_BREAKPOINT:
      *sigrc = SIGTRAP;
      break;
    case SIM_SINGLE_STEP:
      *sigrc = SIGTRAP;
      break;
    case SIM_BAD_SYSCALL:
      *sigrc = SIGILL;
      break;
    case SIM_BAD_ALIGN:
      *sigrc = SIGSEGV;
      break;
    case SIM_DONE:
      *sigrc = 1;
      *reason = sim_exited;
      return;
    default:
      abort ();
    }
  *reason = sim_stopped;
}

void
sim_info (verbose)
     int verbose;
{
  sim_state_type x;

  tm_state (&x);
  tm_info_print (&x);
}

void
sim_open (args)
     char *args;
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
  /* Return non-zero so gdb will handle it.  */
  return 1;
}

void
sim_create_inferior (start_address, argv, env)
     SIM_ADDR start_address;
     char **argv;
     char **env;
{
  tm_store_register (REG_PC, start_address);
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

}


void
sim_set_callbacks (ptr)
struct host_callback_struct *ptr;
{

}
