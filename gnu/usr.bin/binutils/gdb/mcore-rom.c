/* Remote debugging interface to Motorola picobug monitor for gdb,
   the GNU debugger.
   Copyright 1999, 2000, 2001 Free Software Foundation, Inc.

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
   Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

#include "defs.h"
#include "gdbcore.h"
#include "target.h"
#include "monitor.h"
#include "gdb_string.h"
#include "regcache.h"
#include "serial.h"

/* Functions used only in this file. */

static void init_picobug_cmds (void);


/* Functions exported from this file. */

void _initialize_picobug_rom (void);

void picobug_open (char *args, int from_tty);

int picobug_dumpregs (void);


static char *picobug_inits[] =
{"\r", NULL};

static struct target_ops picobug_ops;
static struct monitor_ops picobug_cmds;

/* Picobug only supports a subset of registers from MCore. In reality,
   it doesn't support ss1, either. */
/* *INDENT-OFF* */
static char *picobug_regnames[] = {
  "r0",   "r1",   "r2",   "r3",   "r4",   "r5",   "r6",   "r7",
  "r8",   "r9",   "r10",  "r11",  "r12",  "r13",  "r14",  "r15",
  0,      0,      0,      0,      0,      0,      0,      0,
  0,      0,      0,      0,      0,      0,      0,      0,
  "psr",  "vbr",  "epsr", "fpsr", "epc",  "fpc",  0,      "ss1",
  "ss2",  "ss3",  "ss4",  0,      0,      0,      0,      0,
  0,      0,      0,      0,      0,      0,      0,      0,
  0,      0,      0,      0,      0,      0,      0,      0,
  "pc" };
/* *INDENT-ON* */



void
picobug_open (char *args, int from_tty)
{
  monitor_open (args, &picobug_cmds, from_tty);
}
/* *INDENT-OFF* */
/* We choose to write our own dumpregs routine, since the output of
   the register dumping is rather difficult to encapsulate in a
   regexp:

picobug> rd                                                                 
     pc 2f00031e      epc 2f00031e      fpc 00000000
    psr 80000101     epsr 80000101     fpsr 00000000
ss0-ss4 bad0beef 00000000 00000000 00000000 00000000      vbr 30005c00
  r0-r7 2f0fff4c 00000090 00000001 00000002 00000003 00000004 00000005 00000006
 r8-r15 2f0fff64 00000000 00000000 00000000 00000000 00000000 00000000 2f00031e */
/* *INDENT-ON* */



int
picobug_dumpregs (void)
{
  char buf[1024];
  int resp_len;
  char *p;

  /* Send the dump register command to the monitor and
     get the reply. */
  monitor_printf (picobug_cmds.dump_registers);
  resp_len = monitor_expect_prompt (buf, sizeof (buf));

  p = strtok (buf, " \t\r\n");
  while (p)
    {
      if (strchr (p, '-'))
	{
	  /* got a range. either r0-r7, r8-r15 or ss0-ss4 */
	  if (DEPRECATED_STREQN (p, "r0", 2) || DEPRECATED_STREQN (p, "r8", 2))
	    {
	      int rn = (p[1] == '0' ? 0 : 8);
	      int i = 0;

	      /* Get the next 8 values and record them. */
	      while (i < 8)
		{
		  p = strtok (NULL, " \t\r\n");
		  if (p)
		    monitor_supply_register (rn + i, p);
		  i++;
		}
	    }
	  else if (DEPRECATED_STREQN (p, "ss", 2))
	    {
	      /* get the next five values, ignoring the first */
	      int rn;
	      p = strtok (NULL, " \t\r\n");
	      for (rn = 39; rn < 43; rn++)
		{
		  p = strtok (NULL, " \t\r\n");
		  if (p)
		    monitor_supply_register (rn, p);
		}
	    }
	  else
	    {
	      break;
	    }
	}
      else
	{
	  /* Simple register type, paired */
	  char *name = p;
	  int i;

	  /* Get and record value */
	  p = strtok (NULL, " \t\r\n");
	  if (p)
	    {
	      for (i = 0; i < NUM_REGS; i++)
		{
		  if (picobug_regnames[i] && DEPRECATED_STREQ (picobug_regnames[i], name))
		    break;
		}

	      if (i <= NUM_REGS)
		monitor_supply_register (i, p);
	    }
	}
      p = strtok (NULL, " \t\r\n");
    }

  return 0;
}

static void
init_picobug_cmds (void)
{
  picobug_cmds.flags = MO_GETMEM_NEEDS_RANGE | MO_CLR_BREAK_USES_ADDR | MO_PRINT_PROGRAM_OUTPUT;

  picobug_cmds.init = picobug_inits;	/* Init strings                       */
  picobug_cmds.cont = "g\n";	/* continue command                   */
  picobug_cmds.step = "s\n";	/* single step                        */
  picobug_cmds.set_break = "br %x\n";	/* set a breakpoint                   */
  picobug_cmds.clr_break = "nobr %x\n";		/* clear a breakpoint                 */
  picobug_cmds.clr_all_break = "nobr\n";	/* clear all breakpoints              */
  picobug_cmds.setmem.cmdb = "mm %x %x ;b\n";	/* setmem.cmdb (addr, value)          */
  picobug_cmds.setmem.cmdw = "mm %x %x ;h\n";	/* setmem.cmdw (addr, value)          */
  picobug_cmds.setmem.cmdl = "mm %x %x ;w\n";	/* setmem.cmdl (addr, value)          */
  picobug_cmds.getmem.cmdb = "md %x %x\n";	/* getmem.cmdb (start addr, end addr) */
  picobug_cmds.getmem.resp_delim = ":";		/* getmem.resp_delim                  */
  picobug_cmds.setreg.cmd = "rm %s %x\n";	/* setreg.cmd (name, value)           */
  picobug_cmds.getreg.cmd = "rd %s\n";	/* getreg.cmd (name)                  */
  picobug_cmds.getreg.resp_delim = ":";		/* getreg.resp_delim                  */
  picobug_cmds.dump_registers = "rd\n";		/* dump_registers                     */
  picobug_cmds.dumpregs = picobug_dumpregs;	/* dump registers parser              */
  picobug_cmds.load = "lo\n";	/* download command                   */
  picobug_cmds.prompt = "picobug> ";	/* monitor command prompt             */
  picobug_cmds.line_term = "\n";	/* end-of-line terminator             */
  picobug_cmds.target = &picobug_ops;	/* target operations                  */
  picobug_cmds.stopbits = SERIAL_1_STOPBITS;	/* number of stop bits                */
  picobug_cmds.regnames = picobug_regnames;	/* registers names                    */
  picobug_cmds.num_breakpoints = 20;	/* number of breakpoints              */
  picobug_cmds.magic = MONITOR_OPS_MAGIC;	/* magic                              */
}

void
_initialize_picobug_rom (void)
{
  int i;

  /* Initialize m32r RevC monitor target */
  init_picobug_cmds ();
  init_monitor_ops (&picobug_ops);
  picobug_ops.to_shortname = "picobug";
  picobug_ops.to_longname = "picobug monitor";
  picobug_ops.to_doc = "Debug via the picobug monitor.\n\
Specify the serial device it is connected to (e.g. /dev/ttya).";
  picobug_ops.to_open = picobug_open;

  add_target (&picobug_ops);
}
