/* Remote target glue for the Renesas SH-3 ROM monitor.
   Copyright 1995, 1996, 1997, 1998, 1999, 2000, 2001
   Free Software Foundation, Inc.

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
#include "serial.h"
#include "srec.h"
#include "arch-utils.h"
#include "regcache.h"
#include "gdb_string.h"

#include "sh-tdep.h"

static struct serial *parallel;
static int parallel_in_use;

static void sh3_open (char *args, int from_tty);

static void
sh3_supply_register (char *regname, int regnamelen, char *val, int vallen)
{
  int numregs;
  int regno;

  numregs = 1;
  regno = -1;

  if (regnamelen == 2)
    {
      switch (regname[0])
	{
	case 'S':
	  if (regname[1] == 'R')
	    regno = SR_REGNUM;
	  break;
	case 'P':
	  if (regname[1] == 'C')
	    regno = PC_REGNUM;
	  else if (regname[1] == 'R')
	    regno = PR_REGNUM;
	  break;
	}
    }
  else if (regnamelen == 3)
    {
      switch (regname[0])
	{
	case 'G':
	case 'V':
	  if (regname[1] == 'B' && regname[2] == 'R')
	    {
	      if (regname[0] == 'G')
		regno = VBR_REGNUM;
	      else
		regno = GBR_REGNUM;
	    }
	  break;
	case 'S':
	  if (regname[1] == 'S' && regname[2] == 'R')
	    regno = SSR_REGNUM;
	  else if (regname[1] == 'P' && regname[2] == 'C')
	    regno = SPC_REGNUM;
	  break;
	}
    }
  else if (regnamelen == 4)
    {
      switch (regname[0])
	{
	case 'M':
	  if (regname[1] == 'A' && regname[2] == 'C')
	    {
	      if (regname[3] == 'H')
		regno = MACH_REGNUM;
	      else if (regname[3] == 'L')
		regno = MACL_REGNUM;
	    }
	  break;
	case 'R':
	  if (regname[1] == '0' && regname[2] == '-' && regname[3] == '7')
	    {
	      regno = R0_REGNUM;
	      numregs = 8;
	    }
	}
    }
  else if (regnamelen == 5)
    {
      if (regname[1] == '8' && regname[2] == '-' && regname[3] == '1'
	  && regname[4] == '5')
	{
	  regno = R0_REGNUM + 8;
	  numregs = 8;
	}
    }
  else if (regnamelen == 17)
    {
    }

  if (regno >= 0)
    while (numregs-- > 0)
      val = monitor_supply_register (regno++, val);
}

static void
sh3_load (struct serial *desc, char *file, int hashmark)
{
  if (parallel_in_use)
    {
      monitor_printf ("pl;s\r");
      load_srec (parallel, file, 0, 80, SREC_ALL, hashmark, NULL);
      monitor_expect_prompt (NULL, 0);
    }
  else
    {
      monitor_printf ("il;s:x\r");
      monitor_expect ("\005", NULL, 0);		/* Look for ENQ */
      serial_write (desc, "\006", 1);	/* Send ACK */
      monitor_expect ("LO x\r", NULL, 0);	/* Look for filename */

      load_srec (desc, file, 0, 80, SREC_ALL, hashmark, NULL);

      monitor_expect ("\005", NULL, 0);		/* Look for ENQ */
      serial_write (desc, "\006", 1);	/* Send ACK */
      monitor_expect_prompt (NULL, 0);
    }
}

/* This array of registers need to match the indexes used by GDB.
   This exists because the various ROM monitors use different strings
   than does GDB, and don't necessarily support all the registers
   either. So, typing "info reg sp" becomes a "r30".  */

static char *sh3_regnames[] =
{
  "R0", "R1", "R2", "R3", "R4", "R5", "R6", "R7",
  "R8", "R9", "R10", "R11", "R12", "R13", "R14", "R15",
  "PC", "PR", "GBR", "VBR", "MACH", "MACL", "SR",
  NULL, NULL,
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  "SSR", "SPC",
  "R0_BANK0", "R1_BANK0", "R2_BANK0", "R3_BANK0",
  "R4_BANK0", "R5_BANK0", "R6_BANK0", "R7_BANK0",
  "R0_BANK1", "R1_BANK1", "R2_BANK1", "R3_BANK1",
  "R4_BANK1", "R5_BANK1", "R6_BANK1", "R7_BANK1"
};

static char *sh3e_regnames[] =
{
  "R0", "R1", "R2", "R3", "R4", "R5", "R6", "R7",
  "R8", "R9", "R10", "R11", "R12", "R13", "R14", "R15",
  "PC", "PR", "GBR", "VBR", "MACH", "MACL", "SR",
  "FPUL", "FPSCR",
  "FR0", "FR1", "FR2", "FR3", "FR4", "FR5", "FR6", "FR7",
  "FR8", "FR9", "FR10", "FR11", "FR12", "FR13", "FR14", "FR15",
  "SSR", "SPC",
  "R0_BANK0", "R1_BANK0", "R2_BANK0", "R3_BANK0",
  "R4_BANK0", "R5_BANK0", "R6_BANK0", "R7_BANK0",
  "R0_BANK1", "R1_BANK1", "R2_BANK1", "R3_BANK1",
  "R4_BANK1", "R5_BANK1", "R6_BANK1", "R7_BANK1"
};

/* Define the monitor command strings. Since these are passed directly
   through to a printf style function, we may include formatting
   strings. We also need a CR or LF on the end.  */

static struct target_ops sh3_ops, sh3e_ops;

static char *sh3_inits[] =
{"\003", NULL};			/* Exits sub-command mode & download cmds */

static struct monitor_ops sh3_cmds;

static void
init_sh3_cmds (void)
{
  sh3_cmds.flags = MO_CLR_BREAK_USES_ADDR | MO_GETMEM_READ_SINGLE;	/* flags */
  sh3_cmds.init = sh3_inits;	/* monitor init string */
  sh3_cmds.cont = "g\r";	/* continue command */
  sh3_cmds.step = "s\r";	/* single step */
  sh3_cmds.stop = "\003";	/* Interrupt program */
  sh3_cmds.set_break = "b %x\r";	/* set a breakpoint */
  sh3_cmds.clr_break = "b -%x\r";	/* clear a breakpoint */
  sh3_cmds.clr_all_break = "b -\r";	/* clear all breakpoints */
  sh3_cmds.fill = "f %x @%x %x\r";	/* fill (start len val) */
  sh3_cmds.setmem.cmdb = "m %x %x\r";	/* setmem.cmdb (addr, value) */
  sh3_cmds.setmem.cmdw = "m %x %x;w\r";		/* setmem.cmdw (addr, value) */
  sh3_cmds.setmem.cmdl = "m %x %x;l\r";		/* setmem.cmdl (addr, value) */
  sh3_cmds.setmem.cmdll = NULL;	/* setmem.cmdll (addr, value) */
  sh3_cmds.setmem.resp_delim = NULL;	/* setreg.resp_delim */
  sh3_cmds.setmem.term = NULL;	/* setreg.term */
  sh3_cmds.setmem.term_cmd = NULL;	/* setreg.term_cmd */
  sh3_cmds.getmem.cmdb = "m %x\r";	/* getmem.cmdb (addr, len) */
  sh3_cmds.getmem.cmdw = "m %x;w\r";	/* getmem.cmdw (addr, len) */
  sh3_cmds.getmem.cmdl = "m %x;l\r";	/* getmem.cmdl (addr, len) */
  sh3_cmds.getmem.cmdll = NULL;	/* getmem.cmdll (addr, len) */
  sh3_cmds.getmem.resp_delim = "^ [0-9A-F]+ ";	/* getmem.resp_delim */
  sh3_cmds.getmem.term = "? ";	/* getmem.term */
  sh3_cmds.getmem.term_cmd = ".\r";	/* getmem.term_cmd */
  sh3_cmds.setreg.cmd = ".%s %x\r";	/* setreg.cmd (name, value) */
  sh3_cmds.setreg.resp_delim = NULL;	/* setreg.resp_delim */
  sh3_cmds.setreg.term = NULL;	/* setreg.term */
  sh3_cmds.setreg.term_cmd = NULL;	/* setreg.term_cmd */
  sh3_cmds.getreg.cmd = ".%s\r";	/* getreg.cmd (name) */
  sh3_cmds.getreg.resp_delim = "=";	/* getreg.resp_delim */
  sh3_cmds.getreg.term = "? ";	/* getreg.term */
  sh3_cmds.getreg.term_cmd = ".\r";	/* getreg.term_cmd */
  sh3_cmds.dump_registers = "r\r";	/* dump_registers */
  sh3_cmds.register_pattern = "\\(\\w+\\)=\\([0-9a-fA-F]+\\( +[0-9a-fA-F]+\\b\\)*\\)";
  sh3_cmds.supply_register = sh3_supply_register;	/* supply_register */
  sh3_cmds.load_routine = sh3_load;	/* load_routine */
  sh3_cmds.load = NULL;		/* download command */
  sh3_cmds.loadresp = NULL;	/* Load response */
  sh3_cmds.prompt = "\n:";	/* monitor command prompt */
  sh3_cmds.line_term = "\r";	/* end-of-line terminator */
  sh3_cmds.cmd_end = ".\r";	/* optional command terminator */
  sh3_cmds.target = &sh3_ops;	/* target operations */
  sh3_cmds.stopbits = SERIAL_1_STOPBITS;	/* number of stop bits */
  sh3_cmds.regnames = sh3_regnames;	/* registers names */
  sh3_cmds.magic = MONITOR_OPS_MAGIC;	/* magic */
}				/* init_sh3_cmds */

/* This monitor structure is identical except for a couple slots, so
   we will fill it in from the base structure when needed.  */

static struct monitor_ops sh3e_cmds;

static void
sh3_open (char *args, int from_tty)
{
  char *serial_port_name = args;
  char *parallel_port_name = 0;

  if (args)
    {
      char *cursor = serial_port_name = xstrdup (args);

      while (*cursor && *cursor != ' ')
	cursor++;

      if (*cursor)
	*cursor++ = 0;

      while (*cursor == ' ')
	cursor++;

      if (*cursor)
	parallel_port_name = cursor;
    }

  monitor_open (serial_port_name, &sh3_cmds, from_tty);

  if (parallel_port_name)
    {
      parallel = serial_open (parallel_port_name);

      if (!parallel)
	perror_with_name ("Unable to open parallel port.");

      parallel_in_use = 1;
    }


  /* If we connected successfully, we know the processor is an SH3.  */
  {
    struct gdbarch_info info;
    gdbarch_info_init (&info);
    info.bfd_arch_info = bfd_lookup_arch (bfd_arch_sh, bfd_mach_sh3);
    if (!gdbarch_update_p (info))
      error ("Target is not an SH3");
  }
}


static void
sh3e_open (char *args, int from_tty)
{
  char *serial_port_name = args;
  char *parallel_port_name = 0;

  if (args)
    {
      char *cursor = serial_port_name = xstrdup (args);

      while (*cursor && *cursor != ' ')
	cursor++;

      if (*cursor)
	*cursor++ = 0;

      while (*cursor == ' ')
	cursor++;

      if (*cursor)
	parallel_port_name = cursor;
    }

  /* Set up the SH-3E monitor commands structure.  */

  memcpy (&sh3e_cmds, &sh3_cmds, sizeof (struct monitor_ops));

  sh3e_cmds.target = &sh3e_ops;
  sh3e_cmds.regnames = sh3e_regnames;

  monitor_open (serial_port_name, &sh3e_cmds, from_tty);

  if (parallel_port_name)
    {
      parallel = serial_open (parallel_port_name);

      if (!parallel)
	perror_with_name ("Unable to open parallel port.");

      parallel_in_use = 1;
    }

  /* If we connected successfully, we know the processor is an SH3E.  */
  {
    struct gdbarch_info info;
    gdbarch_info_init (&info);
    info.bfd_arch_info = bfd_lookup_arch (bfd_arch_sh, bfd_mach_sh3);
    if (!gdbarch_update_p (info))
      error ("Target is not an SH3");
  }
}

static void
sh3_close (int quitting)
{
  monitor_close (quitting);
  if (parallel_in_use)
    {
      serial_close (parallel);
      parallel_in_use = 0;
    }
}

extern initialize_file_ftype _initialize_sh3_rom; /* -Wmissing-prototypes */

void
_initialize_sh3_rom (void)
{
  init_sh3_cmds ();
  init_monitor_ops (&sh3_ops);

  sh3_ops.to_shortname = "sh3";
  sh3_ops.to_longname = "Renesas SH-3 rom monitor";

  sh3_ops.to_doc =
  /* We can download through the parallel port too. */
    "Debug on a Renesas eval board running the SH-3E rom monitor.\n"
    "Specify the serial device it is connected to.\n"
    "If you want to use the parallel port to download to it, specify that\n"
    "as an additional second argument.";

  sh3_ops.to_open = sh3_open;
  sh3_ops.to_close = sh3_close;

  add_target (&sh3_ops);

  /* Setup the SH3e, which has float registers.  */

  init_monitor_ops (&sh3e_ops);

  sh3e_ops.to_shortname = "sh3e";
  sh3e_ops.to_longname = "Renesas SH-3E rom monitor";

  sh3e_ops.to_doc =
  /* We can download through the parallel port too. */
    "Debug on a Renesas eval board running the SH-3E rom monitor.\n"
    "Specify the serial device it is connected to.\n"
    "If you want to use the parallel port to download to it, specify that\n"
    "as an additional second argument.";

  sh3e_ops.to_open = sh3e_open;
  sh3e_ops.to_close = sh3_close;

  add_target (&sh3e_ops);
}
