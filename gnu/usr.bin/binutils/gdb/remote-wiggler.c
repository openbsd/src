/* Remote target communications for the Macraigor Systems BDM Wiggler
   Copyright 1996 Free Software Foundation, Inc.

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
#include "gdb_string.h"
#include <fcntl.h>
#include "frame.h"
#include "inferior.h"
#include "bfd.h"
#include "symfile.h"
#include "target.h"
#include "wait.h"
#include "gdbcmd.h"
#include "objfiles.h"
#include "gdb-stabs.h"

#include "dcache.h"

#ifdef USG
#include <sys/types.h>
#endif

#include <signal.h>
#include "serial.h"

/* Wiggler serial protocol definitions */

#define DLE 020			/* Quote char */
#define SYN 026			/* Start of packet */
#define RAW_SYN ((026 << 8) | 026) /* get_quoted_char found a naked SYN */

/* Status flags */

#define WIGGLER_FLAG_RESET 0x01 /* Target is being reset */
#define WIGGLER_FLAG_STOPPED 0x02 /* Target is halted */
#define WIGGLER_FLAG_BDM 0x04	/* Target is in BDM */
#define WIGGLER_FLAG_PWF 0x08	/* Power failed */
#define WIGGLER_FLAG_CABLE_DISC 0x10 /* BDM cable disconnected */

#define WIGGLER_AYT 0x0		/* Are you there? */
#define WIGGLER_GET_VERSION 0x1	/* Get Version */
#define WIGGLER_SET_BAUD_RATE 0x2 /* Set Baud Rate */
#define WIGGLER_INIT 0x10	/* Initialize Wiggler */
#define WIGGLER_SET_SPEED 0x11	/* Set Speed */
#define WIGGLER_GET_STATUS_MASK 0x12 /* Get Status Mask */
#define WIGGLER_GET_CTRS 0x13	/* Get Error Counters */
#define WIGGLER_SET_FUNC_CODE 0x14 /* Set Function Code */
#define WIGGLER_SET_CTL_FLAGS 0x15 /* Set Control Flags */
#define WIGGLER_SET_BUF_ADDR 0x16 /* Set Register Buffer Address */
#define WIGGLER_RUN 0x20	/* Run Target from PC */
#define WIGGLER_RUN_ADDR 0x21	/* Run Target from Specified Address */
#define WIGGLER_STOP 0x22	/* Stop Target */
#define WIGGLER_RESET_RUN 0x23	/* Reset Target and Run */
#define WIGGLER_RESET 0x24	/* Reset Target and Halt */
#define WIGGLER_STEP 0x25	/* Single step */
#define WIGGLER_READ_REGS 0x30	/* Read Registers */
#define WIGGLER_WRITE_REGS 0x31	/* Write Registers */
#define WIGGLER_READ_MEM 0x32	/* Read Memory */
#define WIGGLER_WRITE_MEM 0x33	/* Write Memory */
#define WIGGLER_FILL_MEM 0x34	/* Fill Memory */
#define WIGGLER_MOVE_MEM 0x35	/* Move Memory */

#define WIGGLER_READ_INT_MEM 0x80 /* Read Internal Memory */
#define WIGGLER_WRITE_INT_MEM 0x81 /* Write Internal Memory */
#define WIGGLER_JUMP 0x82	/* Jump to Subroutine */

#define WIGGLER_ERASE_FLASH 0x90 /* Erase flash memory */
#define WIGGLER_PROGRAM_FLASH 0x91 /* Write flash memory */
#define WIGGLER_EXIT_MON 0x93	/* Exit the flash programming monitor  */
#define WIGGLER_ENTER_MON 0x94	/* Enter the flash programming monitor  */

#define WIGGLER_SET_STATUS 0x0a	/* Set status */
#define   WIGGLER_FLAG_STOP 0x0 /* Stop the target, enter BDM */
#define   WIGGLER_FLAG_START 0x01 /* Start the target at PC */
#define   WIGGLER_FLAG_RETURN_STATUS 0x04 /* Return async status */

/* Stuff that should be in tm-xxx files. */
#if 1
#define BDM_NUM_REGS 24
#define BDM_REGMAP   0,  1,  2,  3,  4,  5,  6,  7, /* d0 -> d7 */ \
		     8,  9, 10, 11, 12, 13, 14, 15, /* a0 -> a7 */ \
		    18, 16,			    /* ps, pc */ \
		    -1, -1, -1, -1, -1, -1, -1, -1, /* fp0 -> fp7 */ \
		    -1, -1, -1, -1, -1 /* fpcontrol, fpstatus, fpiaddr, fpcode, fpflags */
#define BDM_BREAKPOINT 0x4a, 0xfa /* BGND insn */
#else
#define BDM_NUM_REGS 24
#define BDM_REGMAP   8,  9, 10, 11, 12, 13, 14, 15, /* d0 -> d7 */ \
		    16, 17, 18, 19, 20, 21, 22, 23, /* a0 -> a7 */ \
		     4,  0,			    /* ps, pc */ \
		    -1, -1, -1, -1, -1, -1, -1, -1, /* fp0 -> fp7 */ \
		    -1, -1, -1, -1, -1 /* fpcontrol, fpstatus, fpiaddr, fpcode, fpflags */
#define WIGGLER_POLL
#endif

/* Prototypes for local functions */

static void wiggler_stop PARAMS ((void));

static void put_packet PARAMS ((unsigned char *packet, int pktlen));
static unsigned char * get_packet PARAMS ((int cmd, int *pktlen, int timeout));

static int wiggler_write_bytes PARAMS ((CORE_ADDR memaddr,
				       char *myaddr, int len));

static int wiggler_read_bytes PARAMS ((CORE_ADDR memaddr,
				      char *myaddr, int len));

static void wiggler_files_info PARAMS ((struct target_ops *ignore));

static int wiggler_xfer_memory PARAMS ((CORE_ADDR memaddr, char *myaddr,
				       int len, int should_write,
				       struct target_ops *target));

static void wiggler_prepare_to_store PARAMS ((void));

static void wiggler_fetch_registers PARAMS ((int regno));

static void wiggler_resume PARAMS ((int pid, int step,
				   enum target_signal siggnal));

static int wiggler_start_remote PARAMS ((char *dummy));

static void wiggler_open PARAMS ((char *name, int from_tty));

static void wiggler_close PARAMS ((int quitting));

static void wiggler_store_registers PARAMS ((int regno));

static void wiggler_mourn PARAMS ((void));

static int readchar PARAMS ((int timeout));

static void reset_packet PARAMS ((void));

static void output_packet PARAMS ((void));

static int get_quoted_char PARAMS ((int timeout));

static void put_quoted_char PARAMS ((int c));

static int wiggler_wait PARAMS ((int pid, struct target_waitstatus *status));

static void wiggler_kill PARAMS ((void));

static void wiggler_detach PARAMS ((char *args, int from_tty));

static void wiggler_interrupt PARAMS ((int signo));

static void wiggler_interrupt_twice PARAMS ((int signo));

static void interrupt_query PARAMS ((void));

static unsigned char * do_command PARAMS ((int cmd, int *statusp, int *lenp));

static unsigned char * read_bdm_registers PARAMS ((int first_bdm_regno,
						   int last_bdm_regno,
						   int *numregs));

extern struct target_ops wiggler_ops;	/* Forward decl */

static int last_run_status;

/* This was 5 seconds, which is a long time to sit and wait.
   Unless this is going though some terminal server or multiplexer or
   other form of hairy serial connection, I would think 2 seconds would
   be plenty.  */

/* Changed to allow option to set timeout value.
   was static int remote_timeout = 2; */
extern int remote_timeout;

/* Descriptor for I/O to remote machine.  Initialize it to NULL so that
   wiggler_open knows that we don't have a file open when the program
   starts.  */
serial_t wiggler_desc = NULL;

static void
wiggler_error (s, error_code)
     char *s;
     int error_code;
{
  char buf[100];

  fputs_filtered (s, gdb_stderr);
  fputs_filtered (" ", gdb_stderr);

  switch (error_code)
    {
    case 0x1: s = "Unknown fault"; break;
    case 0x2: s = "Power failed"; break;
    case 0x3: s = "Cable disconnected"; break;
    case 0x4: s = "Couldn't enter BDM"; break;
    case 0x5: s = "Target stuck in reset"; break;
    case 0x6: s = "Port not configured"; break;
    case 0x7: s = "Write verify failed"; break;
    case 0x11: s = "Bus error"; break;
    case 0x12: s = "Checksum error"; break;
    case 0x13: s = "Illegal command"; break;
    case 0x14: s = "Parameter error"; break;
    case 0x15: s = "Internal error"; break;
    case 0x16: s = "Register buffer error"; break;
    case 0x80: s = "Flash erase error"; break;
    default:
      sprintf (buf, "Unknown error code %d", error_code);
      s = buf;
    }

  error (s);
}

/*  Return nonzero if the thread TH is still alive on the remote system.  */

static int
wiggler_thread_alive (th)
     int th;
{
  return 1;
}

/* Clean up connection to a remote debugger.  */

/* ARGSUSED */
static void
wiggler_close (quitting)
     int quitting;
{
  if (wiggler_desc)
    SERIAL_CLOSE (wiggler_desc);
  wiggler_desc = NULL;
}

/* Stub for catch_errors.  */

static int
wiggler_start_remote (dummy)
     char *dummy;
{
  unsigned char buf[10], *p;
  int pktlen;
  int status;
  int error_code;
  int speed;

  immediate_quit = 1;		/* Allow user to interrupt it */

  SERIAL_SEND_BREAK (wiggler_desc); /* Wake up the wiggler */

  do_command (WIGGLER_AYT, &status, &pktlen);

  p = do_command (WIGGLER_GET_VERSION, &status, &pktlen);

  printf_unfiltered ("[Wiggler version %x.%x, capability 0x%x]\n",
		     p[0], p[1], (p[2] << 16) | p[3]);

#if 1
  speed = 80;			/* Divide clock by 4000 */

  buf[0] = WIGGLER_INIT;
  buf[1] = speed >> 8;
  buf[2] = speed & 0xff;
  buf[3] = 0;			/* CPU32 for now */
  put_packet (buf, 4);		/* Init Wiggler params */
  p = get_packet (buf[0], &pktlen, remote_timeout);

  if (pktlen < 2)
    error ("Truncated response packet from Wiggler");

  status = p[1];
  error_code = p[2];

  if (error_code != 0)
    wiggler_error ("WIGGLER_INIT:", error_code);
#endif

#if 0
  /* Reset the target */

  do_command (WIGGLER_RESET_RUN, &status, &pktlen);
/*  do_command (WIGGLER_RESET, &status, &pktlen);*/
#endif

  /* If processor is still running, stop it.  */

  if (!(status & WIGGLER_FLAG_BDM))
    wiggler_stop ();

#if 1
  buf[0] = WIGGLER_SET_CTL_FLAGS;
  buf[1] = 0;
  buf[2] = 1;		/* Asynchronously return status when target stops */
  put_packet (buf, 3);

  p = get_packet (buf[0], &pktlen, remote_timeout);

  if (pktlen < 2)
    error ("Truncated response packet from Wiggler");

  status = p[1];
  error_code = p[2];

  if (error_code != 0)
    wiggler_error ("WIGGLER_SET_CTL_FLAGS:", error_code);
#endif

  immediate_quit = 0;

/* This is really the job of start_remote however, that makes an assumption
   that the target is about to print out a status message of some sort.  That
   doesn't happen here (in fact, it may not be possible to get the monitor to
   send the appropriate packet).  */

  flush_cached_frames ();
  registers_changed ();
  stop_pc = read_pc ();
  set_current_frame (create_new_frame (read_fp (), stop_pc));
  select_frame (get_current_frame (), 0);
  print_stack_frame (selected_frame, -1, 1);

  return 1;
}

/* Open a connection to a remote debugger.
   NAME is the filename used for communication.  */

static DCACHE *wiggler_dcache;

static void
wiggler_open (name, from_tty)
     char *name;
     int from_tty;
{
  if (name == 0)
    error ("To open a Wiggler connection, you need to specify what serial\n\
device the Wiggler is attached to (e.g. /dev/ttya).");

  target_preopen (from_tty);

  unpush_target (&wiggler_ops);

  wiggler_dcache = dcache_init (wiggler_read_bytes, wiggler_write_bytes);

  wiggler_desc = SERIAL_OPEN (name);
  if (!wiggler_desc)
    perror_with_name (name);

  if (baud_rate != -1)
    {
      if (SERIAL_SETBAUDRATE (wiggler_desc, baud_rate))
	{
	  SERIAL_CLOSE (wiggler_desc);
	  perror_with_name (name);
	}
    }

  SERIAL_RAW (wiggler_desc);

  /* If there is something sitting in the buffer we might take it as a
     response to a command, which would be bad.  */
  SERIAL_FLUSH_INPUT (wiggler_desc);

  if (from_tty)
    {
      puts_filtered ("Remote target wiggler connected to ");
      puts_filtered (name);
      puts_filtered ("\n");
    }
  push_target (&wiggler_ops);	/* Switch to using remote target now */

  /* Without this, some commands which require an active target (such as kill)
     won't work.  This variable serves (at least) double duty as both the pid
     of the target process (if it has such), and as a flag indicating that a
     target is active.  These functions should be split out into seperate
     variables, especially since GDB will someday have a notion of debugging
     several processes.  */

  inferior_pid = 42000;
  /* Start the remote connection; if error (0), discard this target.
     In particular, if the user quits, be sure to discard it
     (we'd be in an inconsistent state otherwise).  */
  if (!catch_errors (wiggler_start_remote, (char *)0, 
		     "Couldn't establish connection to remote target\n", RETURN_MASK_ALL))
    pop_target();
}

/* This takes a program previously attached to and detaches it.  After
   this is done, GDB can be used to debug some other program.  We
   better not have left any breakpoints in the target program or it'll
   die when it hits one.  */

static void
wiggler_detach (args, from_tty)
     char *args;
     int from_tty;
{
  if (args)
    error ("Argument given to \"detach\" when remotely debugging.");

  pop_target ();
  if (from_tty)
    puts_filtered ("Ending remote debugging.\n");
}

/* Tell the remote machine to resume.  */

static void
wiggler_resume (pid, step, siggnal)
     int pid, step;
     enum target_signal siggnal;
{
  int pktlen;

  dcache_flush (wiggler_dcache);

  if (step)
    do_command (WIGGLER_STEP, &last_run_status, &pktlen);
  else
    do_command (WIGGLER_RUN, &last_run_status, &pktlen);
}

static void
wiggler_stop ()
{
  int status;
  int pktlen;

  do_command (WIGGLER_STOP, &status, &pktlen);

  if (!(status & WIGGLER_FLAG_BDM))
    error ("Can't stop target via BDM");
}

static volatile int wiggler_interrupt_flag;

/* Send ^C to target to halt it.  Target will respond, and send us a
   packet.  */

static void
wiggler_interrupt (signo)
     int signo;
{
  /* If this doesn't work, try more severe steps.  */
  signal (signo, wiggler_interrupt_twice);
  
  if (remote_debug)
    printf_unfiltered ("wiggler_interrupt called\n");

  {
    char buf[1];

    wiggler_stop ();
    buf[0] = WIGGLER_AYT;
    put_packet (buf, 1);
    wiggler_interrupt_flag = 1;
  }
}

static void (*ofunc)();

/* The user typed ^C twice.  */
static void
wiggler_interrupt_twice (signo)
     int signo;
{
  signal (signo, ofunc);
  
  interrupt_query ();

  signal (signo, wiggler_interrupt);
}

/* Ask the user what to do when an interrupt is received.  */

static void
interrupt_query ()
{
  target_terminal_ours ();

  if (query ("Interrupted while waiting for the program.\n\
Give up (and stop debugging it)? "))
    {
      target_mourn_inferior ();
      return_to_top_level (RETURN_QUIT);
    }

  target_terminal_inferior ();
}

/* If nonzero, ignore the next kill.  */
static int kill_kludge;

/* Wait until the remote machine stops, then return,
   storing status in STATUS just as `wait' would.
   Returns "pid" (though it's not clear what, if anything, that
   means in the case of this target).  */

static int
wiggler_wait (pid, target_status)
     int pid;
     struct target_waitstatus *target_status;
{
  unsigned char *p;
  int error_code, status;
  int pktlen;

  wiggler_interrupt_flag = 0;

  target_status->kind = TARGET_WAITKIND_STOPPED;
  target_status->value.sig = TARGET_SIGNAL_TRAP;

  /* Target may already be stopped by the time we get here. */

  if (!(last_run_status & WIGGLER_FLAG_BDM))
    {
      ofunc = (void (*)()) signal (SIGINT, wiggler_interrupt);

      p = get_packet (WIGGLER_AYT, &pktlen, -1);

      signal (SIGINT, ofunc);

      if (pktlen < 2)
	error ("Truncated response packet from Wiggler");

      status = p[1];
      error_code = p[2];

      if (error_code != 0)
	wiggler_error ("target_wait:", error_code);

      if (status & WIGGLER_FLAG_PWF)
	error ("Wiggler lost VCC at BDM interface.");
      else if (status & WIGGLER_FLAG_CABLE_DISC)
	error ("BDM cable appears to have been disconnected.");

      if (!(status & WIGGLER_FLAG_BDM))
	error ("Wiggler woke up, but wasn't stopped: 0x%x", status);

      if (wiggler_interrupt_flag)
	target_status->value.sig = TARGET_SIGNAL_INT;
    }

  /* This test figures out if we just executed a BGND insn, and if it's one of
     our breakpoints.  If so, then we back up PC.  N.B. When a BGND insn is
     executed, the PC points at the loc just after the insn (ie: it's always
     two bytes *after* the BGND).  So, it's not sufficient to just see if PC-2
     is a BGND insn because we could have gotten there via a jump.  We dis-
     ambiguate this case by examining the ATEMP register (which is only
     accessible from BDM).  This will tell us if we entered BDM because we
     executed a BGND insn.  */

  if (breakpoint_inserted_here_p (read_pc () - 2)) /* One of our breakpoints? */
    {				/* Yes, see if we actually executed it */
#if 0	/* Temporarily disabled until atemp reading is fixed. */
      int atemp;
      int numregs;

      p = read_bdm_registers (23, 23, &numregs);
      atemp = extract_unsigned_integer (p, 4);

      if (atemp == 1)		/* And, did we hit a breakpoint insn? */
#endif
	write_pc (read_pc () - 2); /* Yes, then back up PC */
    }

  return inferior_pid;
}

/* Read the remote registers into the block REGS.  */
/* Currently we just read all the registers, so we don't use regno.  */
/* ARGSUSED */

static unsigned char *
read_bdm_registers (first_bdm_regno, last_bdm_regno, numregs)
     int first_bdm_regno;
     int last_bdm_regno;
     int *numregs;
{
  unsigned char buf[10];
  int i;
  unsigned char *p;
  unsigned char *regs;
  int error_code, status;
  int pktlen;

  buf[0] = WIGGLER_READ_REGS;
  buf[1] = first_bdm_regno >> 8;
  buf[2] = first_bdm_regno & 0xff;
  buf[3] = last_bdm_regno >> 8;
  buf[4] = last_bdm_regno & 0xff;

  put_packet (buf, 5);
  p = get_packet (WIGGLER_READ_REGS, &pktlen, remote_timeout);

  if (pktlen < 5)
    error ("Truncated response packet from Wiggler");

  status = p[1];
  error_code = p[2];

  if (error_code != 0)
    wiggler_error ("read_bdm_registers:", error_code);

  i = p[3];
  if (i == 0)
    i = 256;

  if (i > pktlen - 4
      || ((i & 3) != 0))
    error ("Register block size bad:  %d", i);

  *numregs = i / 4;

  regs = p + 4;

  return regs;
}

static void
dump_all_bdm_regs ()
{
  unsigned char *regs;
  int numregs;
  int i;

  regs = read_bdm_registers (0, BDM_NUM_REGS - 1, &numregs);

  printf_unfiltered ("rpc = 0x%x ",
		     (int)extract_unsigned_integer (regs, 4));
  regs += 4;
  printf_unfiltered ("usp = 0x%x ",
		     (int)extract_unsigned_integer (regs, 4));
  regs += 4;
  printf_unfiltered ("ssp = 0x%x ",
		     (int)extract_unsigned_integer (regs, 4));
  regs += 4;
  printf_unfiltered ("vbr = 0x%x ",
		     (int)extract_unsigned_integer (regs, 4));
  regs += 4;
  printf_unfiltered ("sr = 0x%x ",
		     (int)extract_unsigned_integer (regs, 4));
  regs += 4;
  printf_unfiltered ("sfc = 0x%x ",
		     (int)extract_unsigned_integer (regs, 4));
  regs += 4;
  printf_unfiltered ("dfc = 0x%x ",
		     (int)extract_unsigned_integer (regs, 4));
  regs += 4;
  printf_unfiltered ("atemp = 0x%x ",
		     (int)extract_unsigned_integer (regs, 4));
  regs += 4;
  printf_unfiltered ("\n");

  for (i = 0; i <= 7; i++)
    printf_unfiltered ("d%i = 0x%x ", i,
		       (int)extract_unsigned_integer (regs + i * 4, 4));
  regs += 8 * 4;
  printf_unfiltered ("\n");

  for (i = 0; i <= 7; i++)
    printf_unfiltered ("a%i = 0x%x ", i,
		       (int)extract_unsigned_integer (regs + i * 4, 4));
  printf_unfiltered ("\n");
}

static int bdm_regmap[] = {BDM_REGMAP};

/* Read the remote registers into the block REGS.  */
/* Currently we just read all the registers, so we don't use regno.  */
/* ARGSUSED */
static void
wiggler_fetch_registers (regno)
     int regno;
{
  int i;
  unsigned char *regs;
  int first_regno, last_regno;
  int first_bdm_regno, last_bdm_regno;
  int numregs;

  if (regno == -1)
    {
      first_regno = 0;
      last_regno = NUM_REGS - 1;

      first_bdm_regno = 0;
      last_bdm_regno = BDM_NUM_REGS - 1;
    }
  else
    {
      first_regno = regno;
      last_regno = regno;

      first_bdm_regno = bdm_regmap [regno];
      last_bdm_regno = bdm_regmap [regno];
    }

  if (first_bdm_regno == -1)
    {
      supply_register (first_regno, NULL);
      return;			/* Unsupported register */
    }

  regs = read_bdm_registers (first_bdm_regno, last_bdm_regno, &numregs);

  for (i = first_regno; i <= last_regno; i++)
    {
      int bdm_regno, regoffset;

      bdm_regno = bdm_regmap [i];
      if (bdm_regno != -1)
	{
	  regoffset = bdm_regno - first_bdm_regno;

	  if (regoffset >= numregs)
	    continue;

	  supply_register (i, regs + 4 * regoffset);
	}
      else
	supply_register (i, NULL); /* Unsupported register */
    }
}

static void 
wiggler_prepare_to_store ()
{
}

/* Store register REGNO, or all registers if REGNO == -1, from the contents
   of REGISTERS.  FIXME: ignores errors.  */

static void
wiggler_store_registers (regno)
     int regno;
{
  unsigned char buf[10 + 256];
  int i;
  unsigned char *p;
  int error_code, status;
  int pktlen;
  int first_regno, last_regno;
  int first_bdm_regno, last_bdm_regno;

  if (regno == -1)
    {
      first_regno = 0;
      last_regno = NUM_REGS - 1;

      first_bdm_regno = 0;
      last_bdm_regno = BDM_NUM_REGS - 1;
    }
  else
    {
      first_regno = regno;
      last_regno = regno;

      first_bdm_regno = bdm_regmap [regno];
      last_bdm_regno = bdm_regmap [regno];
    }

  if (first_bdm_regno == -1)
    return;			/* Unsupported register */

  buf[0] = WIGGLER_WRITE_REGS;
  buf[3] = 4;

  for (i = first_regno; i <= last_regno; i++)
    {
      int bdm_regno;

      bdm_regno = bdm_regmap [i];

      buf[1] = bdm_regno >> 8;
      buf[2] = bdm_regno & 0xff;

      memcpy (&buf[4], &registers[REGISTER_BYTE (i)], 4);
      put_packet (buf, 4 + 4);
      p = get_packet (WIGGLER_WRITE_REGS, &pktlen, remote_timeout);

      if (pktlen < 3)
	error ("Truncated response packet from Wiggler");

      status = p[1];
      error_code = p[2];

      if (error_code != 0)
	wiggler_error ("wiggler_store_registers:", error_code);
    }
}

/* Write memory data directly to the remote machine.
   This does not inform the data cache; the data cache uses this.
   MEMADDR is the address in the remote memory space.
   MYADDR is the address of the buffer in our space.
   LEN is the number of bytes.

   Returns number of bytes transferred, or 0 for error.  */

static int
wiggler_write_bytes (memaddr, myaddr, len)
     CORE_ADDR memaddr;
     char *myaddr;
     int len;
{
  char buf[256 + 10];
  unsigned char *p;
  int origlen;

  origlen = len;

  buf[0] = WIGGLER_WRITE_MEM;
  buf[5] = 1;			/* Write as bytes */
  buf[6] = 0;			/* Don't verify */

  while (len > 0)
    {
      int numbytes;
      int pktlen;
      int status, error_code;

      numbytes = min (len, 256 - 8);

      buf[1] = memaddr >> 24;
      buf[2] = memaddr >> 16;
      buf[3] = memaddr >> 8;
      buf[4] = memaddr;

      buf[7] = numbytes;

      memcpy (&buf[8], myaddr, numbytes);
      put_packet (buf, 8 + numbytes);
      p = get_packet (WIGGLER_WRITE_MEM, &pktlen, remote_timeout);
      if (pktlen < 3)
	error ("Truncated response packet from Wiggler");

      status = p[1];
      error_code = p[2];

      if (error_code == 0x11)	/* Got a bus error? */
	{
	  CORE_ADDR error_address;

	  error_address = p[3] << 24;
	  error_address |= p[4] << 16;
	  error_address |= p[5] << 8;
	  error_address |= p[6];
	  numbytes = error_address - memaddr;

	  len -= numbytes;

	  errno = EIO;

	  break;
	}
      else if (error_code != 0)
	wiggler_error ("wiggler_write_bytes:", error_code);

      len -= numbytes;
      memaddr += numbytes;
      myaddr += numbytes;
    }

  return origlen - len;
}

/* Read memory data directly from the remote machine.
   This does not use the data cache; the data cache uses this.
   MEMADDR is the address in the remote memory space.
   MYADDR is the address of the buffer in our space.
   LEN is the number of bytes.

   Returns number of bytes transferred, or 0 for error.  */

static int
wiggler_read_bytes (memaddr, myaddr, len)
     CORE_ADDR memaddr;
     char *myaddr;
     int len;
{
  char buf[256 + 10];
  unsigned char *p;
  int origlen;

  origlen = len;

  buf[0] = WIGGLER_READ_MEM;
  buf[5] = 1;			/* Read as bytes */

  while (len > 0)
    {
      int numbytes;
      int pktlen;
      int status, error_code;

      numbytes = min (len, 256 - 7);

      buf[1] = memaddr >> 24;
      buf[2] = memaddr >> 16;
      buf[3] = memaddr >> 8;
      buf[4] = memaddr;

      buf[6] = numbytes;

      put_packet (buf, 7);
      p = get_packet (WIGGLER_READ_MEM, &pktlen, remote_timeout);
      if (pktlen < 4)
	error ("Truncated response packet from Wiggler");

      status = p[1];
      error_code = p[2];

      if (error_code == 0x11)	/* Got a bus error? */
	{
	  CORE_ADDR error_address;

	  error_address = p[3] << 24;
	  error_address |= p[4] << 16;
	  error_address |= p[5] << 8;
	  error_address |= p[6];
	  numbytes = error_address - memaddr;

	  len -= numbytes;

	  errno = EIO;

	  break;
	}
      else if (error_code != 0)
	wiggler_error ("wiggler_read_bytes:", error_code);

      memcpy (myaddr, &p[4], numbytes);

      len -= numbytes;
      memaddr += numbytes;
      myaddr += numbytes;
    }

  return origlen - len;
}

/* Read or write LEN bytes from inferior memory at MEMADDR, transferring
   to or from debugger address MYADDR.  Write to inferior if SHOULD_WRITE is
   nonzero.  Returns length of data written or read; 0 for error.  */

/* ARGSUSED */
static int
wiggler_xfer_memory (memaddr, myaddr, len, should_write, target)
     CORE_ADDR memaddr;
     char *myaddr;
     int len;
     int should_write;
     struct target_ops *target;			/* ignored */
{
  return dcache_xfer_memory (wiggler_dcache, memaddr, myaddr, len, should_write);
}

static void
wiggler_files_info (ignore)
     struct target_ops *ignore;
{
  puts_filtered ("Debugging a target over a serial line.\n");
}

/* Stuff for dealing with the packets which are part of this protocol.
   See comment at top of file for details.  */

/* Read a single character from the remote side, handling wierd errors. */

static int
readchar (timeout)
     int timeout;
{
  int ch;

  ch = SERIAL_READCHAR (wiggler_desc, timeout);

  switch (ch)
    {
    case SERIAL_EOF:
      error ("Remote connection closed");
    case SERIAL_ERROR:
      perror_with_name ("Remote communication error");
    case SERIAL_TIMEOUT:
    default:
      return ch;
    }
}

#if 0
/* Read a character from the data stream, dequoting as necessary.  SYN is
   treated special.  Any SYNs appearing in the data stream are returned as the
   distinct value RAW_SYN (which has a value > 8 bits and therefore cannot be
   mistaken for real data).  */

static int
get_quoted_char (timeout)
     int timeout;
{
  int ch;

  ch = readchar (timeout);

  switch (ch)
    {
    case SERIAL_TIMEOUT:
      error ("Timeout in mid-packet, aborting");
    case SYN:
      return RAW_SYN;
    case DLE:
      ch = readchar (timeout);
      if (ch == SYN)
	return RAW_SYN;
      return ch & ~0100;
    default:
      return ch;
    }
}

static unsigned char pkt[256 * 2 + 10], *pktp; /* Worst case */

static void
reset_packet ()
{
  pktp = pkt;
}

static void
output_packet ()
{
  if (SERIAL_WRITE (wiggler_desc, pkt, pktp - pkt))
    perror_with_name ("output_packet: write failed");

  reset_packet ();
}

/* Output a quoted character.  SYNs and DLEs are quoted.  Everything else goes
   through untouched.  */

static void
put_quoted_char (c)
     int c;
{
  switch (c)
    {
    case SYN:
    case DLE:
      *pktp++ = DLE;
      c |= 0100;
    }

  *pktp++ = c;
}

/* Send a packet to the Wiggler.  The packet framed by a SYN character, a byte
   count and a checksum.  The byte count only counts the number of bytes
   between the count and the checksum.  A count of zero actually means 256.
   Any SYNs within the packet (including the checksum and count) must be
   quoted.  The quote character must be quoted as well.  Quoting is done by
   replacing the character with the two-character sequence DLE, {char} | 0100.
   Note that the quoting mechanism has no effect on the byte count.
 */

static void
stu_put_packet (buf, len)
     unsigned char *buf;
     int len;
{
  unsigned char checksum;
  unsigned char c;

  if (len == 0 || len > 256)
    abort ();			/* Can't represent 0 length packet */

  reset_packet ();

  checksum = 0;

  put_quoted_char (RAW_SYN);

  c = len;

  do
    {
      checksum += c;

      put_quoted_char (c);

      c = *buf++;
    }
  while (len-- > 0);

  put_quoted_char (-checksum & 0xff);

  output_packet ();
}

#else

/* Send a packet to the Wiggler.  The packet framed by a SYN character, a byte
   count and a checksum.  The byte count only counts the number of bytes
   between the count and the checksum.  A count of zero actually means 256.
   Any SYNs within the packet (including the checksum and count) must be
   quoted.  The quote character must be quoted as well.  Quoting is done by
   replacing the character with the two-character sequence DLE, {char} | 0100.
   Note that the quoting mechanism has no effect on the byte count.
 */

static void
put_packet (buf, len)
     unsigned char *buf;
     int len;
{
  unsigned char checksum;
  unsigned char c;
  unsigned char *packet, *packet_ptr;

  packet = alloca (len + 1 + 1); /* packet + SYN + checksum */
  packet_ptr = packet;

  checksum = 0;

  *packet_ptr++ = 0x55;

  while (len-- > 0)
    {
      c = *buf++;

      checksum += c;
      *packet_ptr++ = c;
    }

  *packet_ptr++ = -checksum;
  if (SERIAL_WRITE (wiggler_desc, packet, packet_ptr - packet))
    perror_with_name ("output_packet: write failed");
}
#endif

#if 0
/* Get a packet from the Wiggler.  Timeout is only enforced for the first byte
   of the packet.  Subsequent bytes are expected to arrive in time <= 
   remote_timeout.  Returns a pointer to a static buffer containing the payload
   of the packet.  *LENP contains the length of the packet.
*/

static unsigned char *
stu_get_packet (cmd, lenp, timeout)
     unsigned char cmd;
     int *lenp;
{
  int ch;
  int len;
  static unsigned char buf[256 + 10], *p;
  unsigned char checksum;

 find_packet:

  ch = get_quoted_char (timeout);

  if (ch < 0)
    error ("get_packet (readchar): %d", ch);

  if (ch != RAW_SYN)
    goto find_packet;

 found_syn:			/* Found the start of a packet */

  p = buf;
  checksum = 0;

  len = get_quoted_char (remote_timeout);

  if (len == RAW_SYN)
    goto found_syn;

  checksum += len;

  if (len == 0)
    len = 256;

  len++;			/* Include checksum */

  while (len-- > 0)
    {
      ch = get_quoted_char (remote_timeout);
      if (ch == RAW_SYN)
	goto found_syn;

      *p++ = ch;
      checksum += ch;
    }

  if (checksum != 0)
    goto find_packet;

  if (cmd != buf[0])
    error ("Response phase error.  Got 0x%x, expected 0x%x", buf[0], cmd);

  *lenp = p - buf - 1;
  return buf;
}

#else

/* Get a packet from the Wiggler.  Timeout is only enforced for the first byte
   of the packet.  Subsequent bytes are expected to arrive in time <= 
   remote_timeout.  Returns a pointer to a static buffer containing the payload
   of the packet.  *LENP contains the length of the packet.
*/

static unsigned char *
get_packet (cmd, lenp, timeout)
     int cmd;
     int *lenp;
{
  int ch;
  int len;
  int i;
  static unsigned char packet[512];
  unsigned char *packet_ptr;
  unsigned char checksum;

 find_packet:

  ch = readchar (timeout);

  if (ch < 0)
    error ("get_packet (readchar): %d", ch);

  if (ch != 0x55)
    goto find_packet;

/* Found the start of a packet */

  packet_ptr = packet;
  checksum = 0;

/* Read command char.  That sort of tells us how long the packet is. */

  ch = readchar (timeout);

  if (ch < 0)
    error ("get_packet (readchar): %d", ch);

  *packet_ptr++ = ch;
  checksum += ch;

/* Get status. */

  ch = readchar (timeout);

  if (ch < 0)
    error ("get_packet (readchar): %d", ch);
  *packet_ptr++ = ch;
  checksum += ch;

/* Get error code. */

  ch = readchar (timeout);

  if (ch < 0)
    error ("get_packet (readchar): %d", ch);
  *packet_ptr++ = ch;
  checksum += ch;

  switch (ch)			/* Figure out length of packet */
    {
    case 0x7:			/* Write verify error? */
      len = 8;			/* write address, value read back */
      break;
    case 0x11:			/* Bus error? */
				/* write address, read flag */
    case 0x15:			/* Internal error */
      len = 5;			/* error code, vector */
      break;
    default:			/* Error w/no params */
      len = 0;
    case 0x0:			/* Normal result */
      switch (packet[0])
	{
	case WIGGLER_AYT:	/* Are You There? */
	case WIGGLER_SET_BAUD_RATE: /* Set Baud Rate */
	case WIGGLER_INIT:	/* Initialize wiggler */
	case WIGGLER_SET_SPEED:	/* Set Speed */
	case WIGGLER_SET_FUNC_CODE: /* Set Function Code */
	case WIGGLER_SET_CTL_FLAGS: /* Set Control Flags */
	case WIGGLER_SET_BUF_ADDR: /* Set Register Buffer Address */
	case WIGGLER_RUN:	/* Run Target from PC  */
	case WIGGLER_RUN_ADDR:	/* Run Target from Specified Address  */
	case WIGGLER_STOP:	/* Stop Target */
	case WIGGLER_RESET_RUN:	/* Reset Target and Run */
	case WIGGLER_RESET:	/* Reset Target and Halt */
	case WIGGLER_STEP:	/* Single Step */
	case WIGGLER_WRITE_REGS: /* Write Register */
	case WIGGLER_WRITE_MEM:	/* Write Memory */
	case WIGGLER_FILL_MEM:	/* Fill Memory */
	case WIGGLER_MOVE_MEM:	/* Move Memory */
	case WIGGLER_WRITE_INT_MEM: /* Write Internal Memory */
	case WIGGLER_JUMP:	/* Jump to Subroutine */
	case WIGGLER_ERASE_FLASH: /* Erase flash memory */
	case WIGGLER_PROGRAM_FLASH: /* Write flash memory */
	case WIGGLER_EXIT_MON:	/* Exit the flash programming monitor  */
	case WIGGLER_ENTER_MON:	/* Enter the flash programming monitor  */
	  len = 0;
	  break;
	case WIGGLER_GET_VERSION: /* Get Version */
	  len = 4;
	  break;
	case WIGGLER_GET_STATUS_MASK: /* Get Status Mask */
	  len = 1;
	  break;
	case WIGGLER_GET_CTRS:	/* Get Error Counters */
	case WIGGLER_READ_REGS:	/* Read Register */
	case WIGGLER_READ_MEM:	/* Read Memory */
	case WIGGLER_READ_INT_MEM: /* Read Internal Memory */
	  len = 257;
	  break;
	default:
	  fprintf_filtered (gdb_stderr, "Unknown packet type 0x%x\n", ch);
	  goto find_packet;
	}
    }

  if (len == 257)		/* Byte stream? */
    {				/* Yes, byte streams contain the length */
      ch = readchar (timeout);

      if (ch < 0)
	error ("get_packet (readchar): %d", ch);
      *packet_ptr++ = ch;
      checksum += ch;
      len = ch;
      if (len == 0)
	len = 256;
    }

  while (len-- >= 0)		/* Do rest of packet and checksum */
    {
      ch = readchar (timeout);

      if (ch < 0)
	error ("get_packet (readchar): %d", ch);
      *packet_ptr++ = ch;
      checksum += ch;
    }

  if (checksum != 0)
    goto find_packet;

  if (cmd != -1 && cmd != packet[0])
    error ("Response phase error.  Got 0x%x, expected 0x%x", packet[0], cmd);

  *lenp = packet_ptr - packet - 1; /* Subtract checksum byte */
  return packet;
}
#endif

/* Execute a simple (one-byte) command.  Returns a pointer to the data
   following the error code.  */

static unsigned char *
do_command (cmd, statusp, lenp)
     int cmd;
     int *statusp;
     int *lenp;
{
  unsigned char buf[100], *p;
  int status, error_code;
  char errbuf[100];

  buf[0] = cmd;
  put_packet (buf, 1);		/* Send command */
  p = get_packet (*buf, lenp, remote_timeout);

  if (*lenp < 3)
    error ("Truncated response packet from Wiggler");

  status = p[1];
  error_code = p[2];

  if (error_code != 0)
    {
      sprintf (errbuf, "do_command (0x%x):", cmd);
      wiggler_error (errbuf, error_code);
    }

  if (status & WIGGLER_FLAG_PWF)
    error ("Wiggler can't detect VCC at BDM interface.");
  else if (status & WIGGLER_FLAG_CABLE_DISC)
    error ("BDM cable appears to be disconnected.");

  *statusp = status;

  return p + 3;
}

static void
wiggler_kill ()
{
  /* For some mysterious reason, wait_for_inferior calls kill instead of
     mourn after it gets TARGET_WAITKIND_SIGNALLED.  Work around it.  */
  if (kill_kludge)
    {
      kill_kludge = 0;
      target_mourn_inferior ();
      return;
    }

  /* Don't wait for it to die.  I'm not really sure it matters whether
     we do or not.  */
  target_mourn_inferior ();
}

static void
wiggler_mourn ()
{
  unpush_target (&wiggler_ops);
  generic_mourn_inferior ();
}

/* All we actually do is set the PC to the start address of exec_bfd, and start
   the program at that point.  */

static void
wiggler_create_inferior (exec_file, args, env)
     char *exec_file;
     char *args;
     char **env;
{
  if (args && (*args != '\000'))
    error ("Args are not supported by BDM.");

  clear_proceed_status ();
  proceed (bfd_get_start_address (exec_bfd), TARGET_SIGNAL_0, 0);
}

static void
wiggler_load (args, from_tty)
     char *args;
     int from_tty;
{
  generic_load (args, from_tty);

  inferior_pid = 0;

/* This is necessary because many things were based on the PC at the time that
   we attached to the monitor, which is no longer valid now that we have loaded
   new code (and just changed the PC).  Another way to do this might be to call
   normal_stop, except that the stack may not be valid, and things would get
   horribly confused... */

  clear_symtab_users ();
}

/* BDM (at least on CPU32) uses a different breakpoint */

static int
wiggler_insert_breakpoint (addr, contents_cache)
     CORE_ADDR addr;
     char *contents_cache;
{
  static char break_insn[] = {BDM_BREAKPOINT};
  int val;

  val = target_read_memory (addr, contents_cache, sizeof break_insn);

  if (val == 0)
    val = target_write_memory (addr, break_insn, sizeof break_insn);

  return val;
}

static void
bdm_command (args, from_tty)
     char *args;
     int from_tty;
{
  error ("bdm command must be followed by `reset'");
}

static void
bdm_reset_command (args, from_tty)
     char *args;
     int from_tty;
{
  int status, pktlen;

  if (!wiggler_desc)
    error ("Not connected to wiggler.");

  do_command (WIGGLER_RESET, &status, &pktlen);
  dcache_flush (wiggler_dcache);
  registers_changed ();
}

static void
bdm_restart_command (args, from_tty)
     char *args;
     int from_tty;
{
  int status, pktlen;

  if (!wiggler_desc)
    error ("Not connected to wiggler.");

  do_command (WIGGLER_RESET_RUN, &status, &pktlen);
  last_run_status = status;
  clear_proceed_status ();
  wait_for_inferior ();
  normal_stop ();
}

static int
flash_xfer_memory (memaddr, myaddr, len, should_write, target)
     CORE_ADDR memaddr;
     char *myaddr;
     int len;
     int should_write;
     struct target_ops *target;			/* ignored */
{
  char buf[256 + 10];
  unsigned char *p;
  int origlen;

  if (!should_write)
    abort ();

  origlen = len;

  buf[0] = WIGGLER_PROGRAM_FLASH;

  while (len > 0)
    {
      int numbytes;
      int pktlen;
      int status, error_code;

      numbytes = min (len, 256 - 6);

      buf[1] = memaddr >> 24;
      buf[2] = memaddr >> 16;
      buf[3] = memaddr >> 8;
      buf[4] = memaddr;

      buf[5] = numbytes;

      memcpy (&buf[6], myaddr, numbytes);
      put_packet (buf, 6 + numbytes);
      p = get_packet (WIGGLER_PROGRAM_FLASH, &pktlen, remote_timeout);
      if (pktlen < 3)
	error ("Truncated response packet from Wiggler");

      status = p[1];
      error_code = p[2];

      if (error_code != 0)	
	wiggler_error ("flash_xfer_memory:", error_code);

      len -= numbytes;
      memaddr += numbytes;
      myaddr += numbytes;
    }

  return origlen - len;
}

static void
bdm_update_flash_command (args, from_tty)
     char *args;
     int from_tty;
{
  int status, pktlen;
  struct cleanup *old_chain;

  if (!wiggler_desc)
    error ("Not connected to wiggler.");

  if (!args)
    error ("Must specify file containing new Wiggler code.");

/*  old_chain = make_cleanup (flash_cleanup, 0);*/

  do_command (WIGGLER_ENTER_MON, &status, &pktlen);

  do_command (WIGGLER_ERASE_FLASH, &status, &pktlen);

  wiggler_ops.to_xfer_memory = flash_xfer_memory;

  generic_load (args, from_tty);

  wiggler_ops.to_xfer_memory = wiggler_xfer_memory;

  do_command (WIGGLER_EXIT_MON, &status, &pktlen);

/*  discard_cleanups (old_chain);*/
}

/* Define the target subroutine names */

struct target_ops wiggler_ops = {
  "wiggler",			/* to_shortname */
  "",				/* to_longname */
  "",				/* to_doc */
  wiggler_open,			/* to_open */
  wiggler_close,		/* to_close */
  NULL,				/* to_attach */
  wiggler_detach,		/* to_detach */
  wiggler_resume,		/* to_resume */
  wiggler_wait,			/* to_wait */
  wiggler_fetch_registers,	/* to_fetch_registers */
  wiggler_store_registers,	/* to_store_registers */
  wiggler_prepare_to_store,	/* to_prepare_to_store */
  wiggler_xfer_memory,		/* to_xfer_memory */
  wiggler_files_info,		/* to_files_info */
  wiggler_insert_breakpoint,	/* to_insert_breakpoint */
  memory_remove_breakpoint,	/* to_remove_breakpoint */
  NULL,				/* to_terminal_init */
  NULL,				/* to_terminal_inferior */
  NULL,				/* to_terminal_ours_for_output */
  NULL,				/* to_terminal_ours */
  NULL,				/* to_terminal_info */
  wiggler_kill,			/* to_kill */
  wiggler_load,			/* to_load */
  NULL,				/* to_lookup_symbol */
  wiggler_create_inferior,	/* to_create_inferior */
  wiggler_mourn,		/* to_mourn_inferior */
  0,				/* to_can_run */
  0,				/* to_notice_signals */
  wiggler_thread_alive,		/* to_thread_alive */
  0,				/* to_stop */
  process_stratum,		/* to_stratum */
  NULL,				/* to_next */
  1,				/* to_has_all_memory */
  1,				/* to_has_memory */
  1,				/* to_has_stack */
  1,				/* to_has_registers */
  1,				/* to_has_execution */
  NULL,				/* sections */
  NULL,				/* sections_end */
  OPS_MAGIC			/* to_magic */
};

void
_initialize_remote_wiggler ()
{
  extern struct cmd_list_element *cmdlist;
  static struct cmd_list_element *bdm_cmd_list = NULL;

  add_target (&wiggler_ops);

  add_show_from_set (add_set_cmd ("remotetimeout", no_class,
				  var_integer, (char *)&remote_timeout,
				  "Set timeout value for remote read.\n", &setlist),
		     &showlist);

  add_prefix_cmd ("bdm", class_obscure, bdm_command, "", &bdm_cmd_list, "bdm ",
		  0, &cmdlist);

  add_cmd ("reset", class_obscure, bdm_reset_command, "", &bdm_cmd_list);
  add_cmd ("restart", class_obscure, bdm_restart_command, "", &bdm_cmd_list);
  add_cmd ("update-flash", class_obscure, bdm_update_flash_command, "", &bdm_cmd_list);
}
