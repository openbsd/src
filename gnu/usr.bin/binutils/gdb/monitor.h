/* Definitions for remote debugging interface for ROM monitors.
    Copyright 1990, 1991, 1992, 1996 Free Software Foundation, Inc.
    Contributed by Cygnus Support. Written by Rob Savoye for Cygnus.
  
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
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

#include "serial.h"

/* This structure describes the strings necessary to give small command
   sequences to the monitor, and parse the response.

   CMD is the actual command typed at the monitor.  Usually this has
   embedded sequences ala printf, which are substituted with the
   arguments appropriate to that type of command.  Ie: to examine a
   register, we substitute the register name for the first arg.  To
   modify memory, we substitute the memory location and the new
   contents for the first and second args, etc...

   RESP_DELIM used to home in on the response string, and is used to
   disambiguate the answer within the pile of text returned by the
   monitor.  This should be a unique string that immediately precedes
   the answer.  Ie: if your monitor prints out `PC: 00000001= ' in
   response to asking for the PC, you should use `: ' as the
   RESP_DELIM.  RESP_DELIM may be NULL if the res- ponse is going to
   be ignored, or has no particular leading text.

   TERM is the string that the monitor outputs to indicate that it is
   idle, and waiting for input.  This is usually a prompt of some
   sort.  In the previous example, it would be `= '.  It is important
   that TERM really means that the monitor is idle, otherwise GDB may
   try to type at it when it isn't ready for input.  This is a problem
   because many monitors cannot deal with type-ahead.  TERM may be
   NULL if the normal prompt is output.

   TERM_CMD is used to quit out of the subcommand mode and get back to
   the main prompt.  TERM_CMD may be NULL if it isn't necessary.  It
   will also be ignored if TERM is NULL.  */

struct memrw_cmd
{
  char *cmdb;			/* Command to send for byte read/write */
  char *cmdw;			/* Command for word (16 bit) read/write */
  char *cmdl;			/* Command for long (32 bit) read/write */
  char *cmdll;			/* Command for long long (64 bit) read/write */
  char *resp_delim;		/* String just prior to the desired value */
  char *term;			/* Terminating string to search for */
  char *term_cmd;		/* String to get out of sub-mode (if necessary) */
};

struct regrw_cmd
{
  char *cmd;			/* Command to send for reg read/write */
  char *resp_delim;		/* String (actually a regexp if getmem) just
				   prior to the desired value */
  char *term;			/* Terminating string to search for */
  char *term_cmd;		/* String to get out of sub-mode (if necessary) */
};

struct monitor_ops
{
  int flags;			/* See below */
  char **init;			/* List of init commands.  NULL terminated. */
  char *cont;			/* continue command */
  char *step;			/* single step */
  char *stop;			/* Interrupt program string */
  char *set_break;		/* set a breakpoint */
  char *clr_break;		/* clear a breakpoint */
  char *clr_all_break;		/* Clear all breakpoints */
  char *fill;			/* Memory fill cmd (addr len val) */
  struct memrw_cmd setmem;	/* set memory to a value */
  struct memrw_cmd getmem;	/* display memory */
  struct regrw_cmd setreg;	/* set a register */
  struct regrw_cmd getreg;	/* get a register */
				/* Some commands can dump a bunch of registers
				   at once.  This comes as a set of REG=VAL
				   pairs.  This should be called for each pair
				   of registers that we can parse to supply
				   GDB with the value of a register.  */
  char *dump_registers;		/* Command to dump all regs at once */
  char *register_pattern;	/* Pattern that picks out register from reg dump */
  void (*supply_register) PARAMS ((char *name, int namelen, char *val, int vallen));
  void (*load_routine) PARAMS ((serial_t desc, char *file, int hashmark)); /* Download routine */
  char *load;			/* load command */
  char *loadresp;		/* Response to load command */
  char *prompt;			/* monitor command prompt */
  char *line_term;		/* end-of-command delimitor */
  char *cmd_end;		/* optional command terminator */
  struct target_ops *target;	/* target operations */
  int stopbits;			/* number of stop bits */
  char **regnames;		/* array of register names in ascii */
  int magic;			/* Check value */
};

/* The monitor ops magic number, used to detect if an ops structure doesn't
   have the right number of entries filled in. */

#define MONITOR_OPS_MAGIC 600925

/* Flag definitions. */

/* If set, then clear breakpoint command uses address, otherwise it
   uses an index returned by the monitor.  */

#define MO_CLR_BREAK_USES_ADDR 0x1

/* If set, then memory fill command uses STARTADDR, ENDADDR+1, VALUE
   as args, else it uses STARTADDR, LENGTH, VALUE as args. */

#define MO_FILL_USES_ADDR 0x2

/* If set, then monitor doesn't automatically supply register dump
   when coming back after a continue.  */

#define MO_NEED_REGDUMP_AFTER_CONT 0x4

/* getmem needs start addr and end addr */

#define MO_GETMEM_NEEDS_RANGE 0x8

/* getmem can only read one loc at a time */

#define MO_GETMEM_READ_SINGLE 0x10

/* handle \r\n combinations */

#define MO_HANDLE_NL 0x20

/* don't expect echos in monitor_open */

#define MO_NO_ECHO_ON_OPEN 0x40

/* If set, send break to stop monitor */

#define MO_SEND_BREAK_ON_STOP 0x80

/* If set, target sends an ACK after each S-record */

#define MO_SREC_ACK 0x100

/* Allow 0x prefix on addresses retured from monitor */

#define MO_HEX_PREFIX 0x200

/* Some monitors require a different command when starting a program */

#define MO_RUN_FIRST_TIME 0x400

/* Don't expect echos when getting memory */

#define MO_NO_ECHO_ON_SETMEM 0x800

/* If set, then register store command expects value BEFORE regname */

#define MO_REGISTER_VALUE_FIRST 0x1000

#define SREC_SIZE 160

extern void monitor_open PARAMS ((char *args, struct monitor_ops *ops,
				  int from_tty));
extern void monitor_close PARAMS ((int quitting));
extern char *monitor_supply_register PARAMS ((int regno, char *valstr));
extern int monitor_expect PARAMS ((char *prompt, char *buf, int buflen));
extern int monitor_expect_prompt PARAMS ((char *buf, int buflen));
extern void monitor_printf PARAMS ((char *, ...))
     ATTR_FORMAT(printf, 1, 2);
extern void monitor_printf_noecho PARAMS ((char *, ...))
     ATTR_FORMAT(printf, 1, 2);
extern void init_monitor_ops PARAMS ((struct target_ops *));
