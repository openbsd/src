/* Remote target glue for the SPARC Sparclet ROM monitor.
   Copyright 1995, 1996 Free Software Foundation, Inc.

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
#include "gdbcore.h"
#include "target.h"
#include "monitor.h"
#include "serial.h"
#include "srec.h"
#include "symtab.h"
#include "symfile.h" /* for generic_load */

#if !defined (HAVE_TERMIOS) && !defined (HAVE_TERMIO) && !defined (HAVE_SGTTY)
#define HAVE_SGTTY
#endif

#ifdef HAVE_SGTTY
#include <sys/ioctl.h>
#endif

#include <sys/types.h>	/* Needed by file.h on Sys V */
#include <sys/file.h>
#include <signal.h>
#include <sys/stat.h>

#define USE_GENERIC_LOAD
#define USE_SW_BREAKS

static struct target_ops sparclet_ops;

static void sparclet_open PARAMS ((char *args, int from_tty));

#ifdef USE_GENERIC_LOAD

static void
sparclet_load_gen (filename, from_tty) 
    char *filename;
    int from_tty;
{
  extern int inferior_pid;

  generic_load (filename, from_tty);
  /* Finally, make the PC point at the start address */
  if (exec_bfd)
    write_pc (bfd_get_start_address (exec_bfd));

  inferior_pid = 0;             /* No process now */
}

#else

static void
sparclet_xmodem_load (desc, file, hashmark)
     serial_t desc;
     char *file;
     int hashmark;
{
  bfd *abfd;
  asection *s;
  char *buffer;
  int i;

  buffer = alloca (XMODEM_PACKETSIZE);
  abfd = bfd_openr (file, 0);
  if (!abfd)
    {
      printf_filtered ("Unable to open file %s\n", file);
      return;
    }
  if (bfd_check_format (abfd, bfd_object) == 0)
    {
      printf_filtered ("File is not an object file\n");
      return;
    }
  for (s = abfd->sections; s; s = s->next)
    if (s->flags & SEC_LOAD)
      {
	bfd_size_type section_size;
	printf_filtered ("%s\t: 0x%4x .. 0x%4x  ", s->name, s->vma,
			 s->vma + s->_raw_size);
	gdb_flush (gdb_stdout);
	monitor_printf (current_monitor->load, s->vma);
	if (current_monitor->loadresp)
	  monitor_expect (current_monitor->loadresp, NULL, 0);
	xmodem_init_xfer (desc);
	section_size = bfd_section_size (abfd, s);
	for (i = 0; i < section_size; i += XMODEM_DATASIZE)
	  {
	    int numbytes;
	    numbytes = min (XMODEM_DATASIZE, section_size - i);
	    bfd_get_section_contents (abfd, s, buffer + XMODEM_DATAOFFSET, i,
				      numbytes);
	    xmodem_send_packet (desc, buffer, numbytes, hashmark);
	    if (hashmark)
	      {
		putchar_unfiltered ('#');
		gdb_flush (gdb_stdout);
	      }
	  }			/* Per-packet (or S-record) loop */
	xmodem_finish_xfer (desc);
	monitor_expect_prompt (NULL, 0);
	putchar_unfiltered ('\n');
      }				/* Loadable sections */
  if (hashmark) 
    putchar_unfiltered ('\n');
}

static void
sparclet_load (desc, file, hashmark)
     serial_t desc;
     char *file;
     int hashmark;
{
???
}
#endif /* USE_GENERIC_LOAD */

/* This array of registers need to match the indexes used by GDB.
   This exists because the various ROM monitors use different strings
   than does GDB, and don't necessarily support all the registers
   either. So, typing "info reg sp" becomes a "r30".  */

/*PSR 0x00000080  impl ver icc AW LE EE EC EF PIL S PS ET CWP  WIM
                0x0  0x0 0x0  0  0  0  0  0 0x0 1  0  0 0x00 0x2
                                                             0000010
       INS        LOCALS       OUTS      GLOBALS
 0  0x00000000  0x00000000  0x00000000  0x00000000
 1  0x00000000  0x00000000  0x00000000  0x00000000
 2  0x00000000  0x00000000  0x00000000  0x00000000
 3  0x00000000  0x00000000  0x00000000  0x00000000
 4  0x00000000  0x00000000  0x00000000  0x00000000
 5  0x00000000  0x00001000  0x00000000  0x00000000
 6  0x00000000  0x00000000  0x123f0000  0x00000000
 7  0x00000000  0x00000000  0x00000000  0x00000000
pc:  0x12010000 0x00000000    unimp
npc: 0x12010004 0x00001000    unimp     0x1000
tbr: 0x00000000
y:   0x00000000
*/
/* these correspond to the offsets from tm-* files from config directories */

/* is wim part of psr?? */
/* monitor wants lower case */
static char *sparclet_regnames[NUM_REGS] = REGISTER_NAMES;

/* Define the monitor command strings. Since these are passed directly
   through to a printf style function, we may include formatting
   strings. We also need a CR or LF on the end.  */

/* need to pause the monitor for timing reasons, so slow it down */

static char *sparclet_inits[] = {"\n\r\r\n", NULL};

static struct monitor_ops sparclet_cmds =
{
  MO_CLR_BREAK_USES_ADDR
    | MO_HEX_PREFIX
    | MO_HANDLE_NL
    | MO_NO_ECHO_ON_OPEN
    | MO_NO_ECHO_ON_SETMEM
    | MO_RUN_FIRST_TIME
    | MO_GETMEM_READ_SINGLE,    /* flags */
  sparclet_inits,			/* Init strings */
  "cont\r",			/* continue command */
  "step\r",			/* single step */
  "\r",			/* break interrupts the program */
  "+bp %x\r",				/* set a breakpoint */
				/* can't use "br" because only 2 hw bps are supported */
  "-bp %x\r",				/* clear a breakpoint */
  "-bp\r",				/* clear all breakpoints */
  NULL,				/* fill (start end val) */
				/* can't use "fi" because it takes words, not bytes */
  {
    /* ex [addr] [-n count] [-b|-s|-l]          default: ex cur -n 1 -b */
    "ex %x -b\r%x\rq\r",                        /* setmem.cmdb (addr, value) */
    "ex %x -s\r%x\rq\r",                /* setmem.cmdw (addr, value) */
    "ex %x -l\r%x\rq\r",         
    NULL,			/* setmem.cmdll (addr, value) */
    NULL, /*": ",			/* setmem.resp_delim */
    NULL, /*"? ",			/* setmem.term */
    NULL, /*"q\r",			/* setmem.term_cmd */
  },
  {
    /* since the parsing of multiple bytes is difficult due to
       interspersed addresses, we'll only read 1 value at a time, 
       even tho these can handle a count */
    /* we can use -n to set count to read, but may have to parse? */
    "ex %x -n 1 -b\r",		/* getmem.cmdb (addr, #bytes) */
    "ex %x -n 1 -s\r",		/* getmem.cmdw (addr, #swords) */
    "ex %x -n 1 -l\r",		/* getmem.cmdl (addr, #words) */
    NULL,		/* getmem.cmdll (addr, #dwords) */
    ": ",			/* getmem.resp_delim */
    NULL,			/* getmem.term */
    NULL,			/* getmem.term_cmd */
  },
  {
    "reg %s 0x%x\r",		/* setreg.cmd (name, value) */
    NULL,			/* setreg.resp_delim */
    NULL,			/* setreg.term */
    NULL			/* setreg.term_cmd */
  },
  {
    "reg %s\r",		/* getreg.cmd (name) */
    ": ",			/* getreg.resp_delim */
    NULL,			/* getreg.term */
    NULL,			/* getreg.term_cmd */
  },
  "reg\r",			/* dump_registers */
  "\\(\\w+\\)=\\([0-9a-fA-F]+\\)",	/* register_pattern */
  NULL,				/* supply_register */
#ifdef USE_GENERIC_LOAD
  NULL,				/* load_routine (defaults to SRECs) */
  NULL,				/* download command */
  NULL,				/* load response */
#else
  sparclet_load,			/* load_routine (defaults to SRECs) */
  /* load [c|a] [s|f|r] [addr count] */
  "load a s %x\r",			/* download command */
  "load: ",		/* load response */
#endif
  "monitor>",				/* monitor command prompt */
  /* yikes!  gdb core dumps without this delimitor!! */
  "\r",			/* end-of-command delimitor */
  NULL,				/* optional command terminator */
  &sparclet_ops,			/* target operations */
  SERIAL_1_STOPBITS,		/* number of stop bits */
  sparclet_regnames,		/* registers names */
  MONITOR_OPS_MAGIC		/* magic */
};

static void
sparclet_open (args, from_tty)
     char *args;
     int from_tty;
{
  monitor_open (args, &sparclet_cmds, from_tty);
}

void
_initialize_sparclet ()
{
  init_monitor_ops (&sparclet_ops);

  sparclet_ops.to_shortname = "sparclet"; /* for the target command */
  sparclet_ops.to_longname = "SPARC Sparclet monitor";
#ifdef USE_GENERIC_LOAD
  sparclet_ops.to_load = sparclet_load_gen; /* FIXME - should go back and try "do" */
#endif
#ifdef USE_SW_BREAKS
  /* use SW breaks; target only supports 2 HW breakpoints */
  sparclet_ops.to_insert_breakpoint = memory_insert_breakpoint; 
  sparclet_ops.to_remove_breakpoint = memory_remove_breakpoint; 
#endif

  sparclet_ops.to_doc = 
    "Use a board running the Sparclet debug monitor.\n\
Specify the serial device it is connected to (e.g. /dev/ttya).";

  sparclet_ops.to_open = sparclet_open;
  add_target (&sparclet_ops);
}

