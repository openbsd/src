/* Tcl/Tk interface routines.
   Copyright 1994, 1995, 1996 Free Software Foundation, Inc.

   Written by Stu Grossman <grossman@cygnus.com> of Cygnus Support.

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
#include "symtab.h"
#include "inferior.h"
#include "command.h"
#include "bfd.h"
#include "symfile.h"
#include "objfiles.h"
#include "target.h"
#include <tcl.h>
#include <tk.h>
#ifdef ANSI_PROTOTYPES
#include <stdarg.h>
#else
#include <varargs.h>
#endif
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <setjmp.h>
#include "top.h"
#include <sys/ioctl.h>
#include "gdb_string.h"
#include "dis-asm.h"
#include <stdio.h>
#include "gdbcmd.h"

#ifndef FIOASYNC
#include <sys/stropts.h>
#endif

/* Some versions (1.3.79, 1.3.81) of Linux don't support SIOCSPGRP the way
   gdbtk wants to use it... */
#ifdef __linux__
#undef SIOCSPGRP
#endif

static void null_routine PARAMS ((int));
static void gdbtk_flush PARAMS ((FILE *));
static void gdbtk_fputs PARAMS ((const char *, FILE *));
static int gdbtk_query PARAMS ((const char *, va_list));
static char *gdbtk_readline PARAMS ((char *));
static void gdbtk_init PARAMS ((void));
static void tk_command_loop PARAMS ((void));
static void gdbtk_call_command PARAMS ((struct cmd_list_element *, char *, int));
static int gdbtk_wait PARAMS ((int, struct target_waitstatus *));
static void x_event PARAMS ((int));
static void gdbtk_interactive PARAMS ((void));
static void cleanup_init PARAMS ((int));
static void tk_command PARAMS ((char *, int));
static int gdb_disassemble PARAMS ((ClientData, Tcl_Interp *, int, char *[]));
static int compare_lines PARAMS ((const PTR, const PTR));
static int gdbtk_dis_asm_read_memory PARAMS ((bfd_vma, bfd_byte *, int, disassemble_info *));
static int gdb_stop PARAMS ((ClientData, Tcl_Interp *, int, char *[]));
static int gdb_listfiles PARAMS ((ClientData, Tcl_Interp *, int, char *[]));
static int call_wrapper PARAMS ((ClientData, Tcl_Interp *, int, char *[]));
static int gdb_cmd PARAMS ((ClientData, Tcl_Interp *, int, char *argv[]));
static int gdb_fetch_registers PARAMS ((ClientData, Tcl_Interp *, int, char *[]));
static void gdbtk_readline_end PARAMS ((void));
static int gdb_changed_register_list PARAMS ((ClientData, Tcl_Interp *, int, char *[]));
static void register_changed_p PARAMS ((int, void *));
static int gdb_get_breakpoint_list PARAMS ((ClientData, Tcl_Interp *, int, char *[]));
static int gdb_get_breakpoint_info PARAMS ((ClientData, Tcl_Interp *, int, char *[]));
static void breakpoint_notify PARAMS ((struct breakpoint *, const char *));
static void gdbtk_create_breakpoint PARAMS ((struct breakpoint *));
static void gdbtk_delete_breakpoint PARAMS ((struct breakpoint *));
static void gdbtk_modify_breakpoint PARAMS ((struct breakpoint *));
static int gdb_loc PARAMS ((ClientData, Tcl_Interp *, int, char *[]));
static int gdb_eval PARAMS ((ClientData, Tcl_Interp *, int, char *[]));
static int gdb_sourcelines PARAMS ((ClientData, Tcl_Interp *, int, char *[]));
static int map_arg_registers PARAMS ((int, char *[], void (*) (int, void *), void *));
static void get_register_name PARAMS ((int, void *));
static int gdb_regnames PARAMS ((ClientData, Tcl_Interp *, int, char *[]));
static void get_register PARAMS ((int, void *));

/* Handle for TCL interpreter */

static Tcl_Interp *interp = NULL;

static int x_fd;		/* X network socket */

/* This variable is true when the inferior is running.  Although it's
   possible to disable most input from widgets and thus prevent
   attempts to do anything while the inferior is running, any commands
   that get through - even a simple memory read - are Very Bad, and
   may cause GDB to crash or behave strangely.  So, this variable
   provides an extra layer of defense.  */

static int running_now;

/* This variable determines where memory used for disassembly is read from.
   If > 0, then disassembly comes from the exec file rather than the
   target (which might be at the other end of a slow serial link).  If
   == 0 then disassembly comes from target.  If < 0 disassembly is
   automatically switched to the target if it's an inferior process,
   otherwise the exec file is used.  */

static int disassemble_from_exec = -1;

/* Supply malloc calls for tcl/tk.  */

char *
Tcl_Malloc (size)
     unsigned int size;
{
  return xmalloc (size);
}

char *
Tcl_Realloc (ptr, size)
     char *ptr;
     unsigned int size;
{
  return xrealloc (ptr, size);
}

void
Tcl_Free(ptr)
     char *ptr;
{
  free (ptr);
}

static void
null_routine(arg)
     int arg;
{
}

/* The following routines deal with stdout/stderr data, which is created by
   {f}printf_{un}filtered and friends.  gdbtk_fputs and gdbtk_flush are the
   lowest level of these routines and capture all output from the rest of GDB.
   Normally they present their data to tcl via callbacks to the following tcl
   routines:  gdbtk_tcl_fputs, gdbtk_tcl_fputs_error, and gdbtk_flush.  These
   in turn call tk routines to update the display.

   Under some circumstances, you may want to collect the output so that it can
   be returned as the value of a tcl procedure.  This can be done by
   surrounding the output routines with calls to start_saving_output and
   finish_saving_output.  The saved data can then be retrieved with
   get_saved_output (but this must be done before the call to
   finish_saving_output).  */

/* Dynamic string header for stdout. */

static Tcl_DString *result_ptr;

static void
gdbtk_flush (stream)
     FILE *stream;
{
#if 0
  /* Force immediate screen update */

  Tcl_VarEval (interp, "gdbtk_tcl_flush", NULL);
#endif
}

static void
gdbtk_fputs (ptr, stream)
     const char *ptr;
     FILE *stream;
{

  if (result_ptr)
    Tcl_DStringAppend (result_ptr, (char *)ptr, -1);
  else
    {
      Tcl_DString str;

      Tcl_DStringInit (&str);

      Tcl_DStringAppend (&str, "gdbtk_tcl_fputs", -1);
      Tcl_DStringAppendElement (&str, (char *)ptr);

      Tcl_Eval (interp, Tcl_DStringValue (&str));
      Tcl_DStringFree (&str);
    }
}

static int
gdbtk_query (query, args)
     const char *query;
     va_list args;
{
  char buf[200], *merge[2];
  char *command;
  long val;

  vsprintf (buf, query, args);
  merge[0] = "gdbtk_tcl_query";
  merge[1] = buf;
  command = Tcl_Merge (2, merge);
  Tcl_Eval (interp, command);
  free (command);

  val = atol (interp->result);
  return val;
}

/* VARARGS */
static void
#ifdef ANSI_PROTOTYPES
gdbtk_readline_begin (char *format, ...)
#else
gdbtk_readline_begin (va_alist)
     va_dcl
#endif
{
  va_list args;
  char buf[200], *merge[2];
  char *command;

#ifdef ANSI_PROTOTYPES
  va_start (args, format);
#else
  char *format;
  va_start (args);
  format = va_arg (args, char *);
#endif

  vsprintf (buf, format, args);
  merge[0] = "gdbtk_tcl_readline_begin";
  merge[1] = buf;
  command = Tcl_Merge (2, merge);
  Tcl_Eval (interp, command);
  free (command);
}

static char *
gdbtk_readline (prompt)
     char *prompt;
{
  char *merge[2];
  char *command;

  merge[0] = "gdbtk_tcl_readline";
  merge[1] = prompt;
  command = Tcl_Merge (2, merge);
  if (Tcl_Eval (interp, command) == TCL_OK)
    {
      return (strdup (interp -> result));
    }
  else
    {
      gdbtk_fputs (interp -> result, gdb_stdout);
      gdbtk_fputs ("\n", gdb_stdout);
      return (NULL);
    }
}

static void
gdbtk_readline_end ()
{
  Tcl_Eval (interp, "gdbtk_tcl_readline_end");
}


static void
#ifdef ANSI_PROTOTYPES
dsprintf_append_element (Tcl_DString *dsp, char *format, ...)
#else
dsprintf_append_element (va_alist)
     va_dcl
#endif
{
  va_list args;
  char buf[1024];

#ifdef ANSI_PROTOTYPES
  va_start (args, format);
#else
  Tcl_DString *dsp;
  char *format;

  va_start (args);
  dsp = va_arg (args, Tcl_DString *);
  format = va_arg (args, char *);
#endif

  vsprintf (buf, format, args);

  Tcl_DStringAppendElement (dsp, buf);
}

static int
gdb_get_breakpoint_list (clientData, interp, argc, argv)
     ClientData clientData;
     Tcl_Interp *interp;
     int argc;
     char *argv[];
{
  struct breakpoint *b;
  extern struct breakpoint *breakpoint_chain;

  if (argc != 1)
    error ("wrong # args");

  for (b = breakpoint_chain; b; b = b->next)
    if (b->type == bp_breakpoint)
      dsprintf_append_element (result_ptr, "%d", b->number);

  return TCL_OK;
}

static int
gdb_get_breakpoint_info (clientData, interp, argc, argv)
     ClientData clientData;
     Tcl_Interp *interp;
     int argc;
     char *argv[];
{
  struct symtab_and_line sal;
  static char *bptypes[] = {"breakpoint", "hardware breakpoint", "until",
			      "finish", "watchpoint", "hardware watchpoint",
			      "read watchpoint", "access watchpoint",
			      "longjmp", "longjmp resume", "step resume",
			      "through sigtramp", "watchpoint scope",
			      "call dummy" };
  static char *bpdisp[] = {"delete", "disable", "donttouch"};
  struct command_line *cmd;
  int bpnum;
  struct breakpoint *b;
  extern struct breakpoint *breakpoint_chain;

  if (argc != 2)
    error ("wrong # args");

  bpnum = atoi (argv[1]);

  for (b = breakpoint_chain; b; b = b->next)
    if (b->number == bpnum)
      break;

  if (!b || b->type != bp_breakpoint)
    error ("Breakpoint #%d does not exist", bpnum);

  sal = find_pc_line (b->address, 0);

  Tcl_DStringAppendElement (result_ptr, symtab_to_filename (sal.symtab));
  dsprintf_append_element (result_ptr, "%d", sal.line);
  dsprintf_append_element (result_ptr, "0x%lx", b->address);
  Tcl_DStringAppendElement (result_ptr, bptypes[b->type]);
  Tcl_DStringAppendElement (result_ptr, b->enable == enabled ? "1" : "0");
  Tcl_DStringAppendElement (result_ptr, bpdisp[b->disposition]);
  dsprintf_append_element (result_ptr, "%d", b->silent);
  dsprintf_append_element (result_ptr, "%d", b->ignore_count);

  Tcl_DStringStartSublist (result_ptr);
  for (cmd = b->commands; cmd; cmd = cmd->next)
    Tcl_DStringAppendElement (result_ptr, cmd->line);
  Tcl_DStringEndSublist (result_ptr);

  Tcl_DStringAppendElement (result_ptr, b->cond_string);

  dsprintf_append_element (result_ptr, "%d", b->thread);
  dsprintf_append_element (result_ptr, "%d", b->hit_count);

  return TCL_OK;
}

static void
breakpoint_notify(b, action)
     struct breakpoint *b;
     const char *action;
{
  char buf[100];
  int v;

  if (b->type != bp_breakpoint)
    return;

  /* We ensure that ACTION contains no special Tcl characters, so we
     can do this.  */
  sprintf (buf, "gdbtk_tcl_breakpoint %s %d", action, b->number);

  v = Tcl_Eval (interp, buf);

  if (v != TCL_OK)
    {
      gdbtk_fputs (interp->result, gdb_stdout);
      gdbtk_fputs ("\n", gdb_stdout);
    }
}

static void
gdbtk_create_breakpoint(b)
     struct breakpoint *b;
{
  breakpoint_notify (b, "create");
}

static void
gdbtk_delete_breakpoint(b)
     struct breakpoint *b;
{
  breakpoint_notify (b, "delete");
}

static void
gdbtk_modify_breakpoint(b)
     struct breakpoint *b;
{
  breakpoint_notify (b, "modify");
}

/* This implements the TCL command `gdb_loc', which returns a list consisting
   of the source and line number associated with the current pc. */

static int
gdb_loc (clientData, interp, argc, argv)
     ClientData clientData;
     Tcl_Interp *interp;
     int argc;
     char *argv[];
{
  char *filename;
  struct symtab_and_line sal;
  char *funcname;
  CORE_ADDR pc;

  if (argc == 1)
    {
      pc = selected_frame ? selected_frame->pc : stop_pc;
      sal = find_pc_line (pc, 0);
    }
  else if (argc == 2)
    {
      struct symtabs_and_lines sals;
      int nelts;

      sals = decode_line_spec (argv[1], 1);

      nelts = sals.nelts;
      sal = sals.sals[0];
      free (sals.sals);

      if (sals.nelts != 1)
	error ("Ambiguous line spec");

      pc = sal.pc;
    }
  else
    error ("wrong # args");

  if (sal.symtab)
    Tcl_DStringAppendElement (result_ptr, sal.symtab->filename);
  else
    Tcl_DStringAppendElement (result_ptr, "");

  find_pc_partial_function (pc, &funcname, NULL, NULL);
  Tcl_DStringAppendElement (result_ptr, funcname);

  filename = symtab_to_filename (sal.symtab);
  Tcl_DStringAppendElement (result_ptr, filename);

  dsprintf_append_element (result_ptr, "%d", sal.line); /* line number */

  dsprintf_append_element (result_ptr, "0x%lx", pc); /* PC */

  return TCL_OK;
}

/* This implements the TCL command `gdb_eval'. */

static int
gdb_eval (clientData, interp, argc, argv)
     ClientData clientData;
     Tcl_Interp *interp;
     int argc;
     char *argv[];
{
  struct expression *expr;
  struct cleanup *old_chain;
  value_ptr val;

  if (argc != 2)
    error ("wrong # args");

  expr = parse_expression (argv[1]);

  old_chain = make_cleanup (free_current_contents, &expr);

  val = evaluate_expression (expr);

  val_print (VALUE_TYPE (val), VALUE_CONTENTS (val), VALUE_ADDRESS (val),
	     gdb_stdout, 0, 0, 0, 0);

  do_cleanups (old_chain);

  return TCL_OK;
}

/* This implements the TCL command `gdb_sourcelines', which returns a list of
   all of the lines containing executable code for the specified source file
   (ie: lines where you can put breakpoints). */

static int
gdb_sourcelines (clientData, interp, argc, argv)
     ClientData clientData;
     Tcl_Interp *interp;
     int argc;
     char *argv[];
{
  struct symtab *symtab;
  struct linetable_entry *le;
  int nlines;

  if (argc != 2)
    error ("wrong # args");

  symtab = lookup_symtab (argv[1]);

  if (!symtab)
    error ("No such file");

  /* If there's no linetable, or no entries, then we are done. */

  if (!symtab->linetable
      || symtab->linetable->nitems == 0)
    {
      Tcl_DStringAppendElement (result_ptr, "");
      return TCL_OK;
    }

  le = symtab->linetable->item;
  nlines = symtab->linetable->nitems;

  for (;nlines > 0; nlines--, le++)
    {
      /* If the pc of this line is the same as the pc of the next line, then
	 just skip it.  */
      if (nlines > 1
	  && le->pc == (le + 1)->pc)
	continue;

      dsprintf_append_element (result_ptr, "%d", le->line);
    }

  return TCL_OK;
}

static int
map_arg_registers (argc, argv, func, argp)
     int argc;
     char *argv[];
     void (*func) PARAMS ((int regnum, void *argp));
     void *argp;
{
  int regnum;

  /* Note that the test for a valid register must include checking the
     reg_names array because NUM_REGS may be allocated for the union of the
     register sets within a family of related processors.  In this case, the
     trailing entries of reg_names will change depending upon the particular
     processor being debugged.  */

  if (argc == 0)		/* No args, just do all the regs */
    {
      for (regnum = 0;
	   regnum < NUM_REGS
	   && reg_names[regnum] != NULL
	   && *reg_names[regnum] != '\000';
	   regnum++)
	func (regnum, argp);

      return TCL_OK;
    }

  /* Else, list of register #s, just do listed regs */
  for (; argc > 0; argc--, argv++)
    {
      regnum = atoi (*argv);

      if (regnum >= 0
	  && regnum < NUM_REGS
	  && reg_names[regnum] != NULL
	  && *reg_names[regnum] != '\000')
	func (regnum, argp);
      else
	error ("bad register number");
    }

  return TCL_OK;
}

static void
get_register_name (regnum, argp)
     int regnum;
     void *argp;		/* Ignored */
{
  Tcl_DStringAppendElement (result_ptr, reg_names[regnum]);
}

/* This implements the TCL command `gdb_regnames', which returns a list of
   all of the register names. */

static int
gdb_regnames (clientData, interp, argc, argv)
     ClientData clientData;
     Tcl_Interp *interp;
     int argc;
     char *argv[];
{
  argc--;
  argv++;

  return map_arg_registers (argc, argv, get_register_name, NULL);
}

#ifndef REGISTER_CONVERTIBLE
#define REGISTER_CONVERTIBLE(x) (0 != 0)
#endif

#ifndef REGISTER_CONVERT_TO_VIRTUAL
#define REGISTER_CONVERT_TO_VIRTUAL(x, y, z, a)
#endif

#ifndef INVALID_FLOAT
#define INVALID_FLOAT(x, y) (0 != 0)
#endif

static void
get_register (regnum, fp)
     int regnum;
     void *fp;
{
  char raw_buffer[MAX_REGISTER_RAW_SIZE];
  char virtual_buffer[MAX_REGISTER_VIRTUAL_SIZE];
  int format = (int)fp;

  if (read_relative_register_raw_bytes (regnum, raw_buffer))
    {
      Tcl_DStringAppendElement (result_ptr, "Optimized out");
      return;
    }

  /* Convert raw data to virtual format if necessary.  */

  if (REGISTER_CONVERTIBLE (regnum))
    {
      REGISTER_CONVERT_TO_VIRTUAL (regnum, REGISTER_VIRTUAL_TYPE (regnum),
				   raw_buffer, virtual_buffer);
    }
  else
    memcpy (virtual_buffer, raw_buffer, REGISTER_VIRTUAL_SIZE (regnum));

  if (format == 'r')
    {
      int j;
      printf_filtered ("0x");
      for (j = 0; j < REGISTER_RAW_SIZE (regnum); j++)
	{
	  register int idx = TARGET_BYTE_ORDER == BIG_ENDIAN ? j
	    : REGISTER_RAW_SIZE (regnum) - 1 - j;
	  printf_filtered ("%02x", (unsigned char)raw_buffer[idx]);
	}
    }
  else
    val_print (REGISTER_VIRTUAL_TYPE (regnum), virtual_buffer, 0,
	       gdb_stdout, format, 1, 0, Val_pretty_default);

  Tcl_DStringAppend (result_ptr, " ", -1);
}

static int
gdb_fetch_registers (clientData, interp, argc, argv)
     ClientData clientData;
     Tcl_Interp *interp;
     int argc;
     char *argv[];
{
  int format;

  if (argc < 2)
    error ("wrong # args");

  argc--;
  argv++;

  argc--;
  format = **argv++;

  return map_arg_registers (argc, argv, get_register, (void *) format);
}

/* This contains the previous values of the registers, since the last call to
   gdb_changed_register_list.  */

static char old_regs[REGISTER_BYTES];

static void
register_changed_p (regnum, argp)
     int regnum;
     void *argp;		/* Ignored */
{
  char raw_buffer[MAX_REGISTER_RAW_SIZE];

  if (read_relative_register_raw_bytes (regnum, raw_buffer))
    return;

  if (memcmp (&old_regs[REGISTER_BYTE (regnum)], raw_buffer,
	      REGISTER_RAW_SIZE (regnum)) == 0)
    return;

  /* Found a changed register.  Save new value and return its number. */

  memcpy (&old_regs[REGISTER_BYTE (regnum)], raw_buffer,
	  REGISTER_RAW_SIZE (regnum));

  dsprintf_append_element (result_ptr, "%d", regnum);
}

static int
gdb_changed_register_list (clientData, interp, argc, argv)
     ClientData clientData;
     Tcl_Interp *interp;
     int argc;
     char *argv[];
{
  argc--;
  argv++;

  return map_arg_registers (argc, argv, register_changed_p, NULL);
}

/* This implements the TCL command `gdb_cmd', which sends its argument into
   the GDB command scanner.  */

static int
gdb_cmd (clientData, interp, argc, argv)
     ClientData clientData;
     Tcl_Interp *interp;
     int argc;
     char *argv[];
{
  if (argc != 2)
    error ("wrong # args");

  if (running_now)
    return TCL_OK;

  execute_command (argv[1], 1);

  bpstat_do_actions (&stop_bpstat);

  return TCL_OK;
}

/* This routine acts as a top-level for all GDB code called by tcl/Tk.  It
   handles cleanups, and calls to return_to_top_level (usually via error).
   This is necessary in order to prevent a longjmp out of the bowels of Tk,
   possibly leaving things in a bad state.  Since this routine can be called
   recursively, it needs to save and restore the contents of the jmp_buf as
   necessary.  */

static int
call_wrapper (clientData, interp, argc, argv)
     ClientData clientData;
     Tcl_Interp *interp;
     int argc;
     char *argv[];
{
  int val;
  struct cleanup *saved_cleanup_chain;
  Tcl_CmdProc *func;
  jmp_buf saved_error_return;
  Tcl_DString result, *old_result_ptr;

  Tcl_DStringInit (&result);
  old_result_ptr = result_ptr;
  result_ptr = &result;

  func = (Tcl_CmdProc *)clientData;
  memcpy (saved_error_return, error_return, sizeof (jmp_buf));

  saved_cleanup_chain = save_cleanups ();

  if (!setjmp (error_return))
    val = func (clientData, interp, argc, argv);
  else
    {
      val = TCL_ERROR;		/* Flag an error for TCL */

      gdb_flush (gdb_stderr);	/* Flush error output */

      gdb_flush (gdb_stdout);	/* Sometimes error output comes here as well */

      /* In case of an error, we may need to force the GUI into idle
	 mode because gdbtk_call_command may have bombed out while in
	 the command routine.  */

      Tcl_Eval (interp, "gdbtk_tcl_idle");
    }

  do_cleanups (ALL_CLEANUPS);

  restore_cleanups (saved_cleanup_chain);

  memcpy (error_return, saved_error_return, sizeof (jmp_buf));

  Tcl_DStringResult (interp, &result);
  result_ptr = old_result_ptr;

  return val;
}

static int
gdb_listfiles (clientData, interp, argc, argv)
     ClientData clientData;
     Tcl_Interp *interp;
     int argc;
     char *argv[];
{
  struct objfile *objfile;
  struct partial_symtab *psymtab;
  struct symtab *symtab;

  ALL_PSYMTABS (objfile, psymtab)
    Tcl_DStringAppendElement (result_ptr, psymtab->filename);

  ALL_SYMTABS (objfile, symtab)
    Tcl_DStringAppendElement (result_ptr, symtab->filename);

  return TCL_OK;
}

static int
gdb_stop (clientData, interp, argc, argv)
     ClientData clientData;
     Tcl_Interp *interp;
     int argc;
     char *argv[];
{
  target_stop ();

  return TCL_OK;
}

/* This implements the TCL command `gdb_disassemble'.  */

static int
gdbtk_dis_asm_read_memory (memaddr, myaddr, len, info)
     bfd_vma memaddr;
     bfd_byte *myaddr;
     int len;
     disassemble_info *info;
{
  extern struct target_ops exec_ops;
  int res;

  errno = 0;
  res = xfer_memory (memaddr, myaddr, len, 0, &exec_ops);

  if (res == len)
    return 0;
  else
    if (errno == 0)
      return EIO;
    else
      return errno;
}

/* We need a different sort of line table from the normal one cuz we can't
   depend upon implicit line-end pc's for lines.  This is because of the
   reordering we are about to do.  */

struct my_line_entry {
  int line;
  CORE_ADDR start_pc;
  CORE_ADDR end_pc;
};

static int
compare_lines (mle1p, mle2p)
     const PTR mle1p;
     const PTR mle2p;
{
  struct my_line_entry *mle1, *mle2;
  int val;

  mle1 = (struct my_line_entry *) mle1p;
  mle2 = (struct my_line_entry *) mle2p;

  val =  mle1->line - mle2->line;

  if (val != 0)
    return val;

  return mle1->start_pc - mle2->start_pc;
}

static int
gdb_disassemble (clientData, interp, argc, argv)
     ClientData clientData;
     Tcl_Interp *interp;
     int argc;
     char *argv[];
{
  CORE_ADDR pc, low, high;
  int mixed_source_and_assembly;
  static disassemble_info di;
  static int di_initialized;

  if (! di_initialized)
    {
      INIT_DISASSEMBLE_INFO_NO_ARCH (di, gdb_stdout,
				     (fprintf_ftype) fprintf_unfiltered);
      di.flavour = bfd_target_unknown_flavour;
      di.memory_error_func = dis_asm_memory_error;
      di.print_address_func = dis_asm_print_address;
      di_initialized = 1;
    }

  di.mach = tm_print_insn_info.mach;
  if (TARGET_BYTE_ORDER == BIG_ENDIAN)
    tm_print_insn_info.endian = BFD_ENDIAN_BIG;
  else
    tm_print_insn_info.endian = BFD_ENDIAN_LITTLE;

  if (argc != 3 && argc != 4)
    error ("wrong # args");

  if (strcmp (argv[1], "source") == 0)
    mixed_source_and_assembly = 1;
  else if (strcmp (argv[1], "nosource") == 0)
    mixed_source_and_assembly = 0;
  else
    error ("First arg must be 'source' or 'nosource'");

  low = parse_and_eval_address (argv[2]);

  if (argc == 3)
    {
      if (find_pc_partial_function (low, NULL, &low, &high) == 0)
	error ("No function contains specified address");
    }
  else
    high = parse_and_eval_address (argv[3]);

  /* If disassemble_from_exec == -1, then we use the following heuristic to
     determine whether or not to do disassembly from target memory or from the
     exec file:

     If we're debugging a local process, read target memory, instead of the
     exec file.  This makes disassembly of functions in shared libs work
     correctly.

     Else, we're debugging a remote process, and should disassemble from the
     exec file for speed.  However, this is no good if the target modifies its
     code (for relocation, or whatever).
   */

  if (disassemble_from_exec == -1)
    if (strcmp (target_shortname, "child") == 0
	|| strcmp (target_shortname, "procfs") == 0
	|| strcmp (target_shortname, "vxprocess") == 0)
      disassemble_from_exec = 0; /* It's a child process, read inferior mem */
    else
      disassemble_from_exec = 1; /* It's remote, read the exec file */

  if (disassemble_from_exec)
    di.read_memory_func = gdbtk_dis_asm_read_memory;
  else
    di.read_memory_func = dis_asm_read_memory;

  /* If just doing straight assembly, all we need to do is disassemble
     everything between low and high.  If doing mixed source/assembly, we've
     got a totally different path to follow.  */

  if (mixed_source_and_assembly)
    {				/* Come here for mixed source/assembly */
      /* The idea here is to present a source-O-centric view of a function to
	 the user.  This means that things are presented in source order, with
	 (possibly) out of order assembly immediately following.  */
      struct symtab *symtab;
      struct linetable_entry *le;
      int nlines;
      int newlines;
      struct my_line_entry *mle;
      struct symtab_and_line sal;
      int i;
      int out_of_order;
      int next_line;

      symtab = find_pc_symtab (low); /* Assume symtab is valid for whole PC range */

      if (!symtab)
	goto assembly_only;

/* First, convert the linetable to a bunch of my_line_entry's.  */

      le = symtab->linetable->item;
      nlines = symtab->linetable->nitems;

      if (nlines <= 0)
	goto assembly_only;

      mle = (struct my_line_entry *) alloca (nlines * sizeof (struct my_line_entry));

      out_of_order = 0;

/* Copy linetable entries for this function into our data structure, creating
   end_pc's and setting out_of_order as appropriate.  */

/* First, skip all the preceding functions.  */

      for (i = 0; i < nlines - 1 && le[i].pc < low; i++) ;

/* Now, copy all entries before the end of this function.  */

      newlines = 0;
      for (; i < nlines - 1 && le[i].pc < high; i++)
	{
	  if (le[i].line == le[i + 1].line
	      && le[i].pc == le[i + 1].pc)
	    continue;		/* Ignore duplicates */

	  mle[newlines].line = le[i].line;
	  if (le[i].line > le[i + 1].line)
	    out_of_order = 1;
	  mle[newlines].start_pc = le[i].pc;
	  mle[newlines].end_pc = le[i + 1].pc;
	  newlines++;
	}

/* If we're on the last line, and it's part of the function, then we need to
   get the end pc in a special way.  */

      if (i == nlines - 1
	  && le[i].pc < high)
	{
	  mle[newlines].line = le[i].line;
	  mle[newlines].start_pc = le[i].pc;
	  sal = find_pc_line (le[i].pc, 0);
	  mle[newlines].end_pc = sal.end;
	  newlines++;
	}

/* Now, sort mle by line #s (and, then by addresses within lines). */

      if (out_of_order)
	qsort (mle, newlines, sizeof (struct my_line_entry), compare_lines);

/* Now, for each line entry, emit the specified lines (unless they have been
   emitted before), followed by the assembly code for that line.  */

      next_line = 0;		/* Force out first line */
      for (i = 0; i < newlines; i++)
	{
/* Print out everything from next_line to the current line.  */

	  if (mle[i].line >= next_line)
	    {
	      if (next_line != 0)
		print_source_lines (symtab, next_line, mle[i].line + 1, 0);
	      else
		print_source_lines (symtab, mle[i].line, mle[i].line + 1, 0);

	      next_line = mle[i].line + 1;
	    }

	  for (pc = mle[i].start_pc; pc < mle[i].end_pc; )
	    {
	      QUIT;
	      fputs_unfiltered ("    ", gdb_stdout);
	      print_address (pc, gdb_stdout);
	      fputs_unfiltered (":\t    ", gdb_stdout);
	      pc += (*tm_print_insn) (pc, &di);
	      fputs_unfiltered ("\n", gdb_stdout);
	    }
	}
    }
  else
    {
assembly_only:
      for (pc = low; pc < high; )
	{
	  QUIT;
	  fputs_unfiltered ("    ", gdb_stdout);
	  print_address (pc, gdb_stdout);
	  fputs_unfiltered (":\t    ", gdb_stdout);
	  pc += (*tm_print_insn) (pc, &di);
	  fputs_unfiltered ("\n", gdb_stdout);
	}
    }

  gdb_flush (gdb_stdout);

  return TCL_OK;
}

static void
tk_command (cmd, from_tty)
     char *cmd;
     int from_tty;
{
  int retval;
  char *result;
  struct cleanup *old_chain;

  /* Catch case of no argument, since this will make the tcl interpreter dump core. */
  if (cmd == NULL)
    error_no_arg ("tcl command to interpret");

  retval = Tcl_Eval (interp, cmd);

  result = strdup (interp->result);

  old_chain = make_cleanup (free, result);

  if (retval != TCL_OK)
    error (result);

  printf_unfiltered ("%s\n", result);

  do_cleanups (old_chain);
}

static void
cleanup_init (ignored)
     int ignored;
{
  if (interp != NULL)
    Tcl_DeleteInterp (interp);
  interp = NULL;
}

/* Come here during long calculations to check for GUI events.  Usually invoked
   via the QUIT macro.  */

static void
gdbtk_interactive ()
{
  /* Tk_DoOneEvent (TK_DONT_WAIT|TK_IDLE_EVENTS); */
}

/* Come here when there is activity on the X file descriptor. */

static void
x_event (signo)
     int signo;
{
  /* Process pending events */

  while (Tk_DoOneEvent (TK_DONT_WAIT|TK_ALL_EVENTS) != 0);
}

static int
gdbtk_wait (pid, ourstatus)
     int pid;
     struct target_waitstatus *ourstatus;
{
  struct sigaction action;
  static sigset_t nullsigmask = {0};

#ifndef SA_RESTART
  /* Needed for SunOS 4.1.x */
#define SA_RESTART 0
#endif

  action.sa_handler = x_event;
  action.sa_mask = nullsigmask;
  action.sa_flags = SA_RESTART;
  sigaction(SIGIO, &action, NULL);

  pid = target_wait (pid, ourstatus);

  action.sa_handler = SIG_IGN;
  sigaction(SIGIO, &action, NULL);

  return pid;
}

/* This is called from execute_command, and provides a wrapper around
   various command routines in a place where both protocol messages and
   user input both flow through.  Mostly this is used for indicating whether
   the target process is running or not.
*/

static void
gdbtk_call_command (cmdblk, arg, from_tty)
     struct cmd_list_element *cmdblk;
     char *arg;
     int from_tty;
{
  running_now = 0;
  if (cmdblk->class == class_run)
    {
      running_now = 1;
      Tcl_Eval (interp, "gdbtk_tcl_busy");
      (*cmdblk->function.cfunc)(arg, from_tty);
      Tcl_Eval (interp, "gdbtk_tcl_idle");
      running_now = 0;
    }
  else
    (*cmdblk->function.cfunc)(arg, from_tty);
}

/* This function is called instead of gdb's internal command loop.  This is the
   last chance to do anything before entering the main Tk event loop. */

static void
tk_command_loop ()
{
  extern GDB_FILE *instream;

  /* We no longer want to use stdin as the command input stream */
  instream = NULL;
  Tcl_Eval (interp, "gdbtk_tcl_preloop");
  Tk_MainLoop ();
}

static void
gdbtk_init ()
{
  struct cleanup *old_chain;
  char *gdbtk_filename;
  int i;
  struct sigaction action;
  static sigset_t nullsigmask = {0};

  /* If there is no DISPLAY environment variable, Tk_Init below will fail,
     causing gdb to abort.  If instead we simply return here, gdb will
     gracefully degrade to using the command line interface. */

  if (getenv ("DISPLAY") == NULL)
    return;

  old_chain = make_cleanup (cleanup_init, 0);

  /* First init tcl and tk. */

  interp = Tcl_CreateInterp ();

  if (!interp)
    error ("Tcl_CreateInterp failed");

  if (Tcl_Init(interp) != TCL_OK)
    error ("Tcl_Init failed: %s", interp->result);

  if (Tk_Init(interp) != TCL_OK)
    error ("Tk_Init failed: %s", interp->result);

  Tcl_CreateCommand (interp, "gdb_cmd", call_wrapper, gdb_cmd, NULL);
  Tcl_CreateCommand (interp, "gdb_loc", call_wrapper, gdb_loc, NULL);
  Tcl_CreateCommand (interp, "gdb_sourcelines", call_wrapper, gdb_sourcelines,
		     NULL);
  Tcl_CreateCommand (interp, "gdb_listfiles", call_wrapper, gdb_listfiles,
		     NULL);
  Tcl_CreateCommand (interp, "gdb_stop", call_wrapper, gdb_stop, NULL);
  Tcl_CreateCommand (interp, "gdb_regnames", call_wrapper, gdb_regnames, NULL);
  Tcl_CreateCommand (interp, "gdb_fetch_registers", call_wrapper,
		     gdb_fetch_registers, NULL);
  Tcl_CreateCommand (interp, "gdb_changed_register_list", call_wrapper,
		     gdb_changed_register_list, NULL);
  Tcl_CreateCommand (interp, "gdb_disassemble", call_wrapper,
		     gdb_disassemble, NULL);
  Tcl_CreateCommand (interp, "gdb_eval", call_wrapper, gdb_eval, NULL);
  Tcl_CreateCommand (interp, "gdb_get_breakpoint_list", call_wrapper,
		     gdb_get_breakpoint_list, NULL);
  Tcl_CreateCommand (interp, "gdb_get_breakpoint_info", call_wrapper,
		     gdb_get_breakpoint_info, NULL);

  command_loop_hook = tk_command_loop;
  print_frame_info_listing_hook =
    (void (*) PARAMS ((struct symtab *, int, int, int))) null_routine;
  query_hook = gdbtk_query;
  flush_hook = gdbtk_flush;
  create_breakpoint_hook = gdbtk_create_breakpoint;
  delete_breakpoint_hook = gdbtk_delete_breakpoint;
  modify_breakpoint_hook = gdbtk_modify_breakpoint;
  interactive_hook = gdbtk_interactive;
  target_wait_hook = gdbtk_wait;
  call_command_hook = gdbtk_call_command;
  readline_begin_hook = gdbtk_readline_begin;
  readline_hook = gdbtk_readline;
  readline_end_hook = gdbtk_readline_end;

  /* Get the file descriptor for the X server */

  x_fd = ConnectionNumber (Tk_Display (Tk_MainWindow (interp)));

  /* Setup for I/O interrupts */

  action.sa_mask = nullsigmask;
  action.sa_flags = 0;
  action.sa_handler = SIG_IGN;
  sigaction(SIGIO, &action, NULL);

#ifdef FIOASYNC
  i = 1;
  if (ioctl (x_fd, FIOASYNC, &i))
    perror_with_name ("gdbtk_init: ioctl FIOASYNC failed");

#ifdef SIOCSPGRP
  i = getpid();
  if (ioctl (x_fd, SIOCSPGRP, &i))
    perror_with_name ("gdbtk_init: ioctl SIOCSPGRP failed");

#else
#ifdef F_SETOWN
  i = getpid();
  if (fcntl (x_fd, F_SETOWN, i))
    perror_with_name ("gdbtk_init: fcntl F_SETOWN failed");
#endif	/* F_SETOWN */
#endif	/* !SIOCSPGRP */
#else
  if (ioctl (x_fd,  I_SETSIG, S_INPUT|S_RDNORM) < 0)
    perror_with_name ("gdbtk_init: ioctl I_SETSIG failed");
#endif /* ifndef FIOASYNC */

  add_com ("tk", class_obscure, tk_command,
	   "Send a command directly into tk.");

  Tcl_LinkVar (interp, "disassemble-from-exec", (char *)&disassemble_from_exec,
	       TCL_LINK_INT);

  /* Load up gdbtk.tcl after all the environment stuff has been setup.  */

  gdbtk_filename = getenv ("GDBTK_FILENAME");
  if (!gdbtk_filename)
    if (access ("gdbtk.tcl", R_OK) == 0)
      gdbtk_filename = "gdbtk.tcl";
    else
      gdbtk_filename = GDBTK_FILENAME;

/* Defer setup of fputs_unfiltered_hook to near the end so that error messages
   prior to this point go to stdout/stderr.  */

  fputs_unfiltered_hook = gdbtk_fputs;

  if (Tcl_EvalFile (interp, gdbtk_filename) != TCL_OK)
    {
      fputs_unfiltered_hook = NULL; /* Force errors to stdout/stderr */

      fprintf_unfiltered (stderr, "%s:%d: %s\n", gdbtk_filename,
			  interp->errorLine, interp->result);

      fputs_unfiltered ("Stack trace:\n", gdb_stderr);
      fputs_unfiltered (Tcl_GetVar (interp, "errorInfo", 0), gdb_stderr);
      error ("");
    }

  discard_cleanups (old_chain);
}

/* Come here during initialize_all_files () */

void
_initialize_gdbtk ()
{
  if (use_windows)
    {
      /* Tell the rest of the world that Gdbtk is now set up. */

      init_ui_hook = gdbtk_init;
    }
}
