/* Simulator for the WDC 65816 architecture.

   Written by Steve Chamberlain of Cygnus Support.
   sac@cygnus.com

   This file is part of W65 sim


		THIS SOFTWARE IS NOT COPYRIGHTED

   Cygnus offers the following for use in the public domain.  Cygnus
   makes no warranty with regard to the software or it's performance
   and the user accepts the software "AS IS" with all faults.

   CYGNUS DISCLAIMS ANY WARRANTIES, EXPRESS OR IMPLIED, WITH REGARD TO
   THIS SOFTWARE INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
   MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.

*/

#include "config.h"

#include <stdio.h>
#include <signal.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_TIME_H
#include <time.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <sys/param.h>
#include "bfd.h"
#include "callback.h"
#include "remote-sim.h"
#include "../../newlib/libc/sys/w65/sys/syscall.h"

#include "interp.h"

saved_state_type saved_state;

int
get_now ()
{
  return time ((long *) 0);
}
void
control_c (sig, code, scp, addr)
     int sig;
     int code;
     char *scp;
     char *addr;
{
  saved_state.exception = SIGINT;
}

wai ()
{
  saved_state.exception = SIGTRAP;
}



wdm (acc, x)
     int acc;
     int x;

{
int cycles;
  /* The x points to where the registers live, acc has code */

#define R(arg)  (x +  arg * 2)
unsigned  R0 = R(0);
unsigned  R4 = R(4);
unsigned  R5 = R(5);
unsigned  R6 = R(6);
unsigned  R7 = R(7);
unsigned  R8 = R(8);
unsigned char *memory = saved_state.memory;
  int a1 = fetch16 (R (4));
  switch (a1)
    {
    case SYS_write:
      {
	int file = fetch16 (R5);
	unsigned char *buf = fetch24 (R6) + memory;
	int len = fetch16 (R8);
	int res = write (file, buf, len);
	store16 (R0, res);
	break;
      }
    case 0:
      printf ("%c", acc);
      fflush (stdout);
      break;
    case 1:
      saved_state.exception = SIGTRAP;
      break;
    default:
      saved_state.exception = SIGILL;
      break;
    }
}


void
sim_resume (step, insignal)
     int step;
     int insignal;
{
  void (*prev) ();
  register unsigned char *memory;
  if (step)
    {
      saved_state.exception = SIGTRAP;
    }
  else
    {
      saved_state.exception = 0;
    }


  prev = signal (SIGINT, control_c);
  do
    {
      int x = (saved_state.p >> 4) & 1;
      int m = (saved_state.p >> 5) & 1;
      if (x == 0 && m == 0)
	{
	  ifunc_X0_M0 ();
	}
      else if (x == 0 && m == 1)
	{
	  ifunc_X0_M1 ();
	}
      else if (x == 1 && m == 0)
	{
	  ifunc_X1_M0 ();
	}
      else if (x == 1 && m == 1)
	{
	  ifunc_X1_M1 ();
	}
    }
  while (saved_state.exception == 0);

  signal (SIGINT, prev);
}




init_pointers ()
{
  if (!saved_state.memory)
    {
      saved_state.memory = calloc (64 * 1024, NUMSEGS);
    }
}

int
sim_write (addr, buffer, size)
     SIM_ADDR addr;
     unsigned char *buffer;
     int size;
{
  int i;
  init_pointers ();

  for (i = 0; i < size; i++)
    {
      saved_state.memory[(addr + i) & MMASK] = buffer[i];
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

  init_pointers ();

  for (i = 0; i < size; i++)
    {
      buffer[i] = saved_state.memory[(addr + i) & MMASK];
    }
  return size;
}



struct
{
  unsigned int *ptr;
  int size;
}
rinfo[] =

{
  &saved_state.r[0], 2,
  &saved_state.r[1], 2,
  &saved_state.r[2], 2,
  &saved_state.r[3], 2,
  &saved_state.r[4], 2,
  &saved_state.r[5], 2,
  &saved_state.r[6], 2,
  &saved_state.r[7], 2,
  &saved_state.r[8], 2,
  &saved_state.r[9], 2,
  &saved_state.r[10], 2,
  &saved_state.r[11], 2,
  &saved_state.r[12], 2,
  &saved_state.r[13], 2,
  &saved_state.r[14], 2,
  &saved_state.r[15], 4,
  &saved_state.pc, 4,
  &saved_state.a, 4,
  &saved_state.x, 4,
  &saved_state.y, 4,
  &saved_state.dbr, 4,
  &saved_state.d, 4,
  &saved_state.s, 4,
  &saved_state.p, 4,
  &saved_state.ticks, 4,
  &saved_state.cycles, 4,
  &saved_state.insts, 4,
  0
};

void
sim_store_register (rn, value)
     int rn;
     unsigned char *value;
{
  unsigned int val;
  int i;
  val = 0;
  for (i = 0; i < rinfo[rn].size; i++)
    {
      val |= (*value++) << (i * 8);
    }

  *(rinfo[rn].ptr) = val;
}

void
sim_fetch_register (rn, buf)
     int rn;
     unsigned char *buf;
{
  unsigned int val = *(rinfo[rn].ptr);
  int i;

  for (i = 0; i < rinfo[rn].size; i++)
    {
      *buf++ = val;
      val = val >> 8;
    }
}


sim_reg_size (n)
{
  return rinfo[n].size;
}
int
sim_trace ()
{
  return 0;
}

void
sim_stop_reason (reason, sigrc)
     enum sim_stop *reason;
     int *sigrc;
{
  *reason = sim_stopped;
  *sigrc = saved_state.exception;
}

int
sim_set_pc (x)
     SIM_ADDR x;
{
  saved_state.pc = x;
  return 0;
}


void
sim_info (verbose)
     int verbose;
{
  double timetaken = (double) saved_state.ticks;
  double virttime = saved_state.cycles / 2.0e6;

  printf ("\n\n# instructions executed  %10d\n", saved_state.insts);
  printf ("# cycles                 %10d\n", saved_state.cycles);
  printf ("# real time taken        %10.4f\n", timetaken);
  printf ("# virtual time taken     %10.4f\n", virttime);

  if (timetaken != 0)
    {
      printf ("# cycles/second          %10d\n", (int) (saved_state.cycles / timetaken));
      printf ("# simulation ratio       %10.4f\n", virttime / timetaken);
    }

}



void
sim_kill ()
{

}

void
sim_open (name)
     char *name;
{
}



#undef fetch8
fetch8func (x)
{
  if (x & ~MMASK)
    {
      saved_state.exception = SIGBUS;
      return 0;
    }
  return saved_state.memory[x];
}

fetch8 (x)
{
return fetch8func(x);
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
  /* Return nonzero so gdb will handle it.  */
  return 1;
}


void
sim_create_inferior (start_address, argv, env)
     SIM_ADDR start_address;
     char **argv;
     char **env;
{
  /* ??? We assume this is a 4 byte quantity.  */
  int pc;

  pc = start_address;
  sim_store_register (16, (unsigned char *) &pc);
}

void
sim_set_callbacks (ptr)
struct host_callback_struct *ptr;
{

}
