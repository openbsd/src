/*  This file is part of the program psim.

    Copyright (C) 1994-1996, Andrew Cagney <cagney@highland.com.au>

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
    Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 
    */


#include <signal.h> /* FIXME - should be machine dependant version */
#include <stdarg.h>
#include <ctype.h>

#include "psim.h"
#include "options.h"

#undef printf_filtered /* blow away the mapping */

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#else
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#endif

#include "../../gdb/defs.h"

#include "../../gdb/remote-sim.h"
#include "../../gdb/callback.h"


/* Structures used by the simulator, for gdb just have static structures */

static psim *simulator;
static device *root_device;
static const char *register_names[] = REGISTER_NAMES;

void
sim_open (char *args)
{
  /* Note: The simulation is not created by sim_open() because
     complete information is not yet available */
  /* trace the call */
  TRACE(trace_gdb, ("sim_open(args=%s) called\n", args ? args : "(null)"));

  if (root_device != NULL)
    sim_io_printf_filtered("Warning - re-open of simulator leaks memory\n");
  root_device = psim_tree();
  simulator = NULL;

  if (args) {
    char **argv = buildargv(args);
    psim_options(root_device, argv);
    freeargv(argv);
  }

  if (ppc_trace[trace_opts])
    print_options ();
}


void
sim_close (int quitting)
{
  TRACE(trace_gdb, ("sim_close(quitting=%d) called\n", quitting));
  if (ppc_trace[trace_print_info] && simulator != NULL)
    psim_print_info (simulator, ppc_trace[trace_print_info]);
}


int
sim_load (char *prog, int from_tty)
{
  char **argv;
  TRACE(trace_gdb, ("sim_load(prog=%s, from_tty=%d) called\n",
		    prog, from_tty));
  ASSERT(prog != NULL);

  /* parse the arguments, assume that the file is argument 0 */
  argv = buildargv(prog);
  ASSERT(argv != NULL && argv[0] != NULL);

  /* create the simulator */
  TRACE(trace_gdb, ("sim_load() - first time, create the simulator\n"));
  simulator = psim_create(argv[0], root_device);

  /* bring in all the data section */
  psim_init(simulator);

  /* release the arguments */
  freeargv(argv);

  /* `I did it my way' */
  return 0;
}


void
sim_kill (void)
{
  TRACE(trace_gdb, ("sim_kill(void) called\n"));
  /* do nothing, nothing to do */
}


int
sim_read (SIM_ADDR mem, unsigned char *buf, int length)
{
  int result = psim_read_memory(simulator, MAX_NR_PROCESSORS,
				buf, mem, length);
  TRACE(trace_gdb, ("sim_read(mem=0x%lx, buf=0x%lx, length=%d) = %d\n",
		    (long)mem, (long)buf, length, result));
  return result;
}


int
sim_write (SIM_ADDR mem, unsigned char *buf, int length)
{
  int result = psim_write_memory(simulator, MAX_NR_PROCESSORS,
				 buf, mem, length,
				 1/*violate_ro*/);
  TRACE(trace_gdb, ("sim_write(mem=0x%lx, buf=0x%lx, length=%d) = %d\n",
		    (long)mem, (long)buf, length, result));
  return result;
}


void
sim_fetch_register (int regno, unsigned char *buf)
{
  if (simulator == NULL) {
    return;
  }
  TRACE(trace_gdb, ("sim_fetch_register(regno=%d(%s), buf=0x%lx)\n",
		    regno, register_names[regno], (long)buf));
  psim_read_register(simulator, MAX_NR_PROCESSORS,
		     buf, register_names[regno],
		     raw_transfer);
}


void
sim_store_register (int regno, unsigned char *buf)
{
  if (simulator == NULL)
    return;
  TRACE(trace_gdb, ("sim_store_register(regno=%d(%s), buf=0x%lx)\n",
		    regno, register_names[regno], (long)buf));
  psim_write_register(simulator, MAX_NR_PROCESSORS,
		      buf, register_names[regno],
		      raw_transfer);
}


void
sim_info (int verbose)
{
  TRACE(trace_gdb, ("sim_info(verbose=%d) called\n", verbose));
  psim_print_info (simulator, verbose);
}


void
sim_create_inferior (SIM_ADDR start_address, char **argv, char **envp)
{
  unsigned_word entry_point = start_address;

  TRACE(trace_gdb, ("sim_create_inferior(start_address=0x%x, ...)\n",
		    start_address));

  psim_init(simulator);
  psim_stack(simulator, argv, envp);

  psim_write_register(simulator, -1 /* all start at same PC */,
		      &entry_point, "pc", cooked_transfer);
}


static volatile int sim_should_run;

void
sim_stop_reason (enum sim_stop *reason, int *sigrc)
{
  psim_status status = psim_get_status(simulator);

  switch (CURRENT_ENVIRONMENT) {

  case USER_ENVIRONMENT:
  case VIRTUAL_ENVIRONMENT:
    switch (status.reason) {
    case was_continuing:
      *reason = sim_stopped;
      *sigrc = SIGTRAP;
      if (sim_should_run) {
	error("sim_stop_reason() unknown reason for halt\n");
      }
      break;
    case was_trap:
      *reason = sim_stopped;
      *sigrc = SIGTRAP;
      break;
    case was_exited:
      *reason = sim_exited;
      *sigrc = 0;
      break;
    case was_signalled:
      *reason = sim_signalled;
      *sigrc = status.signal;
      break;
    }
    break;

  case OPERATING_ENVIRONMENT:
    *reason = sim_stopped;
    *sigrc = SIGTRAP;
    break;

  default:
    error("sim_stop_reason() - unknown environment\n");
  
  }

  TRACE(trace_gdb, ("sim_stop_reason(reason=0x%lx(%ld), sigrc=0x%lx(%ld))\n",
		    (long)reason, (long)*reason, (long)sigrc, (long)*sigrc));
}



/* Run (or resume) the program.  */
static void
sim_ctrl_c()
{
  sim_should_run = 0;
}

void
sim_resume (int step, int siggnal)
{
  void (*prev) ();

  TRACE(trace_gdb, ("sim_resume(step=%d, siggnal=%d)\n",
		    step, siggnal));

  prev = signal(SIGINT, sim_ctrl_c);
  sim_should_run = 1;

  if (step)
    psim_step(simulator);
  else
    psim_run_until_stop(simulator, &sim_should_run);

  signal(SIGINT, prev);
}

void
sim_do_command (char *cmd)
{
  TRACE(trace_gdb, ("sim_do_commands(cmd=%s) called\n",
		    cmd ? cmd : "(null)"));
  if (cmd) {
    char **argv = buildargv(cmd);
    psim_options(root_device, argv);
    freeargv(argv);
  }
}


/* Map simulator IO operations onto the corresponding GDB I/O
   functions.
   
   NB: Only a limited subset of operations are mapped across.  More
   advanced operations (such as dup or write) must either be mapped to
   one of the below calls or handled internally */

static host_callback *callbacks;

int
sim_io_read_stdin(char *buf,
		  int sizeof_buf)
{
  switch (CURRENT_STDIO) {
  case DO_USE_STDIO:
    return callbacks->read_stdin(callbacks, buf, sizeof_buf);
    break;
  case DONT_USE_STDIO:
    return callbacks->read(callbacks, 0, buf, sizeof_buf);
    break;
  default:
    error("sim_io_read_stdin: unaccounted switch\n");
    break;
  }
}

int
sim_io_write_stdout(const char *buf,
		    int sizeof_buf)
{
  switch (CURRENT_STDIO) {
  case DO_USE_STDIO:
    return callbacks->write_stdout(callbacks, buf, sizeof_buf);
    break;
  case DONT_USE_STDIO:
    return callbacks->write(callbacks, 1, buf, sizeof_buf);
    break;
  default:
    error("sim_io_write_stdout: unaccounted switch\n");
    break;
  }
}

int
sim_io_write_stderr(const char *buf,
		    int sizeof_buf)
{
  switch (CURRENT_STDIO) {
  case DO_USE_STDIO:
    /* NB: I think there should be an explicit write_stderr callback */
    return callbacks->write(callbacks, 3, buf, sizeof_buf);
    break;
  case DONT_USE_STDIO:
    return callbacks->write(callbacks, 3, buf, sizeof_buf);
    break;
  default:
    error("sim_io_write_stderr: unaccounted switch\n");
    break;
  }
}


void
sim_io_printf_filtered(const char *fmt,
		       ...)
{
  char message[1024];
  va_list ap;
  /* format the message */
  va_start(ap, fmt);
  vsprintf(message, fmt, ap);
  va_end(ap);
  /* sanity check */
  if (strlen(message) >= sizeof(message))
    error("sim_io_printf_filtered: buffer overflow\n");
  callbacks->printf_filtered(callbacks, "%s", message);
}

void
sim_io_flush_stdoutput(void)
{
  switch (CURRENT_STDIO) {
  case DO_USE_STDIO:
    gdb_flush (gdb_stdout);
    break;
  case DONT_USE_STDIO:
    break;
  default:
    error("sim_io_read_stdin: unaccounted switch\n");
    break;
  }
}

void
sim_set_callbacks (host_callback *callback)
{
  callbacks = callback;
  TRACE(trace_gdb, ("sim_set_callbacks called\n"));
}

/****/

void *
zalloc(long size)
{
  void *memory = (void*)xmalloc(size);
  if (memory == NULL)
    error("xmalloc failed\n");
  memset(memory, 0, size);
  return memory;
}

void zfree(void *data)
{
  mfree(NULL, data);
}
