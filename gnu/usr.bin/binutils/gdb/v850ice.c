/* ICE interface for the NEC V850 for GDB, the GNU debugger.
   Copyright 1996, 1997, 1998, 1999, 2000, 2001
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
#include "gdb_string.h"
#include "frame.h"
#include "symtab.h"
#include "inferior.h"
#include "breakpoint.h"
#include "symfile.h"
#include "target.h"
#include "objfiles.h"
#include "gdbcore.h"
#include "value.h"
#include "command.h"
#include "regcache.h"

#include <tcl.h>
#include <windows.h>
#include <winuser.h>		/* for WM_USER */

extern unsigned long int strtoul (const char *nptr, char **endptr,
				  int base);

/* Local data definitions */
struct MessageIO
  {
    int size;			/* length of input or output in bytes         */
    char *buf;			/* buffer having the input/output information */
  };

/* Prototypes for functions located in other files */
extern void break_command (char *, int);

/* Prototypes for local functions */
static int init_hidden_window (void);

static LRESULT CALLBACK v850ice_wndproc (HWND, UINT, WPARAM, LPARAM);

static void v850ice_files_info (struct target_ops *ignore);

static int v850ice_xfer_memory (CORE_ADDR memaddr, char *myaddr,
				int len, int should_write,
				struct target_ops *target);

static void v850ice_prepare_to_store (void);

static void v850ice_fetch_registers (int regno);

static void v850ice_resume (ptid_t ptid, int step,
                            enum target_signal siggnal);

static void v850ice_open (char *name, int from_tty);

static void v850ice_close (int quitting);

static void v850ice_stop (void);

static void v850ice_store_registers (int regno);

static void v850ice_mourn (void);

static ptid_t v850ice_wait (ptid_t ptid,
                                  struct target_waitstatus *status);

static void v850ice_kill (void);

static void v850ice_detach (char *args, int from_tty);

static int v850ice_insert_breakpoint (CORE_ADDR, char *);

static int v850ice_remove_breakpoint (CORE_ADDR, char *);

static void v850ice_command (char *, int);

static int ice_disassemble (unsigned long, int, char *);

static int ice_lookup_addr (unsigned long *, char *, char *);

static int ice_lookup_symbol (unsigned long, char *);

static void ice_SimulateDisassemble (char *, int);

static void ice_SimulateAddrLookup (char *, int);

static void ice_Simulate_SymLookup (char *, int);

static void ice_fputs (const char *, struct ui_file *);

static int ice_file (char *);

static int ice_cont (char *);

static int ice_stepi (char *);

static int ice_nexti (char *);

static void togdb_force_update (void);

static void view_source (CORE_ADDR);

static void do_gdb (char *, char *, void (*func) (char *, int), int);


/* Globals */
static HWND hidden_hwnd;	/* HWND for messages */

long (__stdcall * ExeAppReq) (char *, long, char *, struct MessageIO *);

long (__stdcall * RegisterClient) (HWND);

long (__stdcall * UnregisterClient) (void);

extern Tcl_Interp *gdbtk_interp;

/* Globals local to this file only */
static int ice_open = 0;	/* Is ICE open? */

static char *v850_CB_Result;	/* special char array for saving 'callback' results */

static int SimulateCallback;	/* simulate a callback event */

#define MAX_BLOCK_SIZE    64*1024	/* Cannot transfer memory in blocks bigger
					   than this */
/* MDI/ICE Message IDs */
#define GSINGLESTEP     0x200	/* single-step target          */
#define GRESUME         0x201	/* resume target               */
#define GREADREG        0x202	/* read a register             */
#define GWRITEREG       0x203	/* write a register            */
#define GWRITEBLOCK     0x204	/* write a block of memory     */
#define GREADBLOCK      0x205	/* read a block of memory      */
#define GSETBREAK       0x206	/* set a breakpoint            */
#define GREMOVEBREAK    0x207	/* remove a breakpoint         */
#define GHALT           0x208	/* ??? */
#define GCHECKSTATUS    0x209	/* check status of ICE         */
#define GMDIREPLY       0x210	/* Reply for previous query - NOT USED */
#define GDOWNLOAD       0x211	/* something for MDI           */
#define GCOMMAND        0x212	/* execute command in ice      */
#define GLOADFILENAME   0x213	/* retrieve load filename      */
#define GWRITEMEM       0x214	/* write word, half-word, or byte */

/* GCHECKSTATUS return codes: */
#define ICE_Idle        0x00
#define ICE_Breakpoint  0x01	/* hit a breakpoint */
#define ICE_Stepped     0x02	/* have stepped     */
#define ICE_Exception   0x03	/* have exception   */
#define ICE_Halted      0x04	/* hit a user halt  */
#define ICE_Exited      0x05	/* called exit      */
#define ICE_Terminated  0x06	/* user terminated  */
#define ICE_Running     0x07
#define ICE_Unknown     0x99

/* Windows messages */
#define WM_STATE_CHANGE WM_USER+101
#define WM_SYM_TO_ADDR  WM_USER+102
#define WM_ADDR_TO_SYM  WM_USER+103
#define WM_DISASSEMBLY  WM_USER+104
#define WM_SOURCE       WM_USER+105

/* STATE_CHANGE codes */
#define STATE_CHANGE_REGS   1	/* Register(s) changed */
#define STATE_CHANGE_LOAD   2	/* HW reset            */
#define STATE_CHANGE_RESET  3	/* Load new file       */
#define STATE_CHANGE_CONT   4	/* Run target          */
#define STATE_CHANGE_STOP   5	/* Stop target         */
#define STATE_CHANGE_STEPI  6	/* Stepi target        */
#define STATE_CHANGE_NEXTI  7	/* Nexti target        */

static struct target_ops v850ice_ops;	/* Forward decl */

/* This function creates a hidden window */
static int
init_hidden_window (void)
{
  WNDCLASS class;

  if (hidden_hwnd != NULL)
    return 1;

  class.style = 0;
  class.cbClsExtra = 0;
  class.cbWndExtra = 0;
  class.hInstance = GetModuleHandle (0);
  class.hbrBackground = NULL;
  class.lpszMenuName = NULL;
  class.lpszClassName = "gdb_v850ice";
  class.lpfnWndProc = v850ice_wndproc;
  class.hIcon = NULL;
  class.hCursor = NULL;

  if (!RegisterClass (&class))
    return 0;

  hidden_hwnd = CreateWindow ("gdb_v850ice", "gdb_v850ice", WS_TILED,
			      0, 0, 0, 0, NULL, NULL, class.hInstance,
			      NULL);
  if (hidden_hwnd == NULL)
    {
      char buf[200];
      DWORD err;

      err = GetLastError ();
      FormatMessage (FORMAT_MESSAGE_FROM_SYSTEM, NULL, err,
		     0, buf, 200, NULL);
      printf_unfiltered ("Could not create window: %s", buf);
      return 0;
    }

  return 1;
}

/* 
   This function is installed as the message handler for the hidden window
   which QBox will use to communicate with gdb. It recognize and acts
   on the following messages:

   WM_SYM_TO_ADDR  \
   WM_ADDR_TO_SYM   | Not implemented at NEC's request
   WM_DISASSEMBLY  /
   WM_STATE_CHANGE - tells us that a state change has occured in the ICE
 */
static LRESULT CALLBACK
v850ice_wndproc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  LRESULT result = FALSE;

  switch (message)
    {
    case WM_SYM_TO_ADDR:
      MessageBox (0, "Symbol resolution\nNot implemented", "GDB", MB_OK);
      break;
    case WM_ADDR_TO_SYM:
      MessageBox (0, "Address resolution\nNot implemented", "GDB", MB_OK);
      break;
    case WM_SOURCE:
      view_source ((CORE_ADDR) lParam);
      break;
    case WM_STATE_CHANGE:
      switch (wParam)
	{
	case STATE_CHANGE_LOAD:
	  {
	    struct MessageIO iob;
	    char buf[128];

	    iob.buf = buf;
	    iob.size = 128;

	    /* Load in a new file... Need filename */
	    ExeAppReq ("GDB", GLOADFILENAME, NULL, &iob);
	    if (!catch_errors ((catch_errors_ftype *) ice_file, iob.buf, "", RETURN_MASK_ALL))
	      printf_unfiltered ("load errored\n");
	  }
	  break;
	case STATE_CHANGE_RESET:
	  registers_changed ();
	  flush_cached_frames ();
	  togdb_force_update ();
	  result = TRUE;
	  break;
	case STATE_CHANGE_REGS:
	  registers_changed ();
	  togdb_force_update ();
	  result = TRUE;
	  break;
	case STATE_CHANGE_CONT:
	  if (!catch_errors ((catch_errors_ftype *) ice_cont, NULL, "", RETURN_MASK_ALL))
	    printf_unfiltered ("continue errored\n");
	  result = TRUE;
	  break;
	case STATE_CHANGE_STEPI:
	  if (!catch_errors ((catch_errors_ftype *) ice_stepi, (int) lParam, "",
			     RETURN_MASK_ALL))
	    printf_unfiltered ("stepi errored\n");
	  result = TRUE;
	  break;
	case STATE_CHANGE_NEXTI:
	  if (!catch_errors ((catch_errors_ftype *) ice_nexti, (int) lParam, "",
			     RETURN_MASK_ALL))
	    printf_unfiltered ("nexti errored\n");
	  result = TRUE;
	  break;
	}
    }

  if (result == FALSE)
    return DefWindowProc (hwnd, message, wParam, lParam);

  return FALSE;
}

/* Code for opening a connection to the ICE.  */

static void
v850ice_open (char *name, int from_tty)
{
  HINSTANCE handle;

  if (name)
    error ("Too many arguments.");

  target_preopen (from_tty);

  unpush_target (&v850ice_ops);

  if (from_tty)
    puts_filtered ("V850ice debugging\n");

  push_target (&v850ice_ops);	/* Switch to using v850ice target now */

  target_terminal_init ();

  /* Initialize everything necessary to facilitate communication
     between QBox, gdb, and the DLLs which control the ICE */
  if (ExeAppReq == NULL)
    {
      handle = LoadLibrary ("necmsg.dll");
      if (handle == NULL)
	error ("Cannot load necmsg.dll");

      ExeAppReq = (long (*) (char *, long, char *, struct MessageIO *))
	GetProcAddress (handle, "ExeAppReq");
      RegisterClient = (long (*) (HWND))
	GetProcAddress (handle, "RegisterClient");
      UnregisterClient = (long (*) (void))
	GetProcAddress (handle, "UnregisterClient");

      if (ExeAppReq == NULL || RegisterClient == NULL || UnregisterClient == NULL)
	error ("Could not find requisite functions in necmsg.dll.");

      if (!init_hidden_window ())
	error ("could not initialize message handling");
    }

  /* Tell the DLL we are here */
  RegisterClient (hidden_hwnd);

  ice_open = 1;

  /* Without this, some commands which require an active target (such as kill)
     won't work.  This variable serves (at least) double duty as both the pid
     of the target process (if it has such), and as a flag indicating that a
     target is active.  These functions should be split out into seperate
     variables, especially since GDB will someday have a notion of debugging
     several processes.  */
  inferior_ptid = pid_to_ptid (42000);

  start_remote ();
  return;
}

/* Clean up connection to a remote debugger.  */

static void
v850ice_close (int quitting)
{
  if (ice_open)
    {
      UnregisterClient ();
      ice_open = 0;
      inferior_ptid = null_ptid;
    }
}

/* Stop the process on the ice. */
static void
v850ice_stop (void)
{
  /* This is silly, but it works... */
  v850ice_command ("stop", 0);
}

static void
v850ice_detach (char *args, int from_tty)
{
  if (args)
    error ("Argument given to \"detach\" when remotely debugging.");

  pop_target ();
  if (from_tty)
    puts_filtered ("Ending v850ice debugging.\n");
}

/* Tell the remote machine to resume.  */

static void
v850ice_resume (ptid_t ptid, int step, enum target_signal siggnal)
{
  long retval;
  char buf[256];
  struct MessageIO iob;

  iob.size = 0;
  iob.buf = buf;

  if (step)
    retval = ExeAppReq ("GDB", GSINGLESTEP, "step", &iob);
  else
    retval = ExeAppReq ("GDB", GRESUME, "run", &iob);

  if (retval)
    error ("ExeAppReq (step = %d) returned %d", step, retval);
}

/* Wait until the remote machine stops, then return,
   storing status in STATUS just as `wait' would.
   Returns "pid" (though it's not clear what, if anything, that
   means in the case of this target).  */

static ptid_t
v850ice_wait (ptid_t ptid, struct target_waitstatus *status)
{
  long v850_status;
  char buf[256];
  struct MessageIO iob;
  int done = 0;
  int count = 0;

  iob.size = 0;
  iob.buf = buf;

  do
    {
      if (count++ % 100000)
	{
	  deprecated_ui_loop_hook (0);
	  count = 0;
	}

      v850_status = ExeAppReq ("GDB", GCHECKSTATUS, NULL, &iob);

      switch (v850_status)
	{
	case ICE_Idle:
	case ICE_Breakpoint:
	case ICE_Stepped:
	case ICE_Halted:
	  status->kind = TARGET_WAITKIND_STOPPED;
	  status->value.sig = TARGET_SIGNAL_TRAP;
	  done = 1;
	  break;
	case ICE_Exception:
	  status->kind = TARGET_WAITKIND_SIGNALLED;
	  status->value.sig = TARGET_SIGNAL_SEGV;
	  done = 1;
	  break;
	case ICE_Exited:
	  status->kind = TARGET_WAITKIND_EXITED;
	  status->value.integer = 0;
	  done = 1;
	  break;
	case ICE_Terminated:
	  status->kind = TARGET_WAITKIND_SIGNALLED;
	  status->value.sig = TARGET_SIGNAL_KILL;
	  done = 1;
	  break;
	default:
	  break;
	}
    }
  while (!done);

  return inferior_ptid;
}

static int
convert_register (int regno, char *buf)
{
  if (regno <= 31)
    sprintf (buf, "r%d", regno);
  else if (REGISTER_NAME (regno)[0] == 's'
	   && REGISTER_NAME (regno)[1] == 'r')
    return 0;
  else
    sprintf (buf, "%s", REGISTER_NAME (regno));

  return 1;
}

/* Read the remote registers into the block REGS.  */
/* Note that the ICE returns register contents as ascii hex strings.  We have
   to convert that to an unsigned long, and then call store_unsigned_integer to
   convert it to target byte-order if necessary.  */

static void
v850ice_fetch_registers (int regno)
{
  long retval;
  char cmd[100];
  char val[100];
  struct MessageIO iob;
  unsigned long regval;
  char *p;

  if (regno == -1)
    {
      for (regno = 0; regno < NUM_REGS; regno++)
	v850ice_fetch_registers (regno);
      return;
    }

  strcpy (cmd, "reg ");
  if (!convert_register (regno, &cmd[4]))
    return;

  iob.size = sizeof val;
  iob.buf = val;
  retval = ExeAppReq ("GDB", GREADREG, cmd, &iob);
  if (retval)
    error ("1: ExeAppReq returned %d: cmd = %s", retval, cmd);

  regval = strtoul (val, NULL, 16);
  if (regval == 0 && p == val)
    error ("v850ice_fetch_registers (%d):  bad value from ICE: %s.",
	   regno, val);

  store_unsigned_integer (val, register_size (current_gdbarch, regno), regval);
  regcache_raw_supply (current_regcache, regno, val);
}

/* Store register REGNO, or all registers if REGNO == -1, from the contents
   of REGISTERS.  */

static void
v850ice_store_registers (int regno)
{
  long retval;
  char cmd[100];
  unsigned long regval;
  char buf[256];
  struct MessageIO iob;
  iob.size = 0;
  iob.buf = buf;

  if (regno == -1)
    {
      for (regno = 0; regno < NUM_REGS; regno++)
	v850ice_store_registers (regno);
      return;
    }

  regval = extract_unsigned_integer (&deprecated_registers[DEPRECATED_REGISTER_BYTE (regno)],
				     register_size (current_gdbarch, regno));
  strcpy (cmd, "reg ");
  if (!convert_register (regno, &cmd[4]))
    return;
  sprintf (cmd + strlen (cmd), "=0x%x", regval);

  retval = ExeAppReq ("GDB", GWRITEREG, cmd, &iob);
  if (retval)
    error ("2: ExeAppReq returned %d: cmd = %s", retval, cmd);
}

/* Prepare to store registers.  Nothing to do here, since the ICE can write one
   register at a time.  */

static void
v850ice_prepare_to_store (void)
{
}

/* Read or write LEN bytes from inferior memory at MEMADDR, transferring
   to or from debugger address MYADDR.  Write to inferior if SHOULD_WRITE is
   nonzero.  TARGET is unused.  Returns length of data written or read;
   0 for error.

   We can only read/write MAX_BLOCK_SIZE bytes at a time, though, or the DLL
   dies.  */
static int
v850ice_xfer_memory (CORE_ADDR memaddr, char *myaddr, int len,
		     int should_write, struct target_ops *target)
{
  long retval;
  char cmd[100];
  struct MessageIO iob;
  int sent;

  if (should_write)
    {
      if (len == 4 || len == 2 || len == 1)
	{
	  long value = 0;
	  char buf[256];
	  char c;

	  iob.size = 0;
	  iob.buf = buf;

	  sent = 0;
	  switch (len)
	    {
	    case 4:
	      c = 'w';
	      value |= (long) ((myaddr[3] << 24) & 0xff000000);
	      value |= (long) ((myaddr[2] << 16) & 0x00ff0000);
	      value |= (long) ((myaddr[1] << 8) & 0x0000ff00);
	      value |= (long) (myaddr[0] & 0x000000ff);
	      break;
	    case 2:
	      c = 'h';
	      value |= (long) ((myaddr[1] << 8) & 0xff00);
	      value |= (long) (myaddr[0] & 0x00ff);
	      break;
	    case 1:
	      c = 'b';
	      value |= (long) (myaddr[0] & 0xff);
	      break;
	    }

	  sprintf (cmd, "memory %c c 0x%x=0x%x", c, (int) memaddr, value);
	  retval = ExeAppReq ("GDB", GWRITEMEM, cmd, &iob);
	  if (retval == 0)
	    sent = len;
	}
      else
	{
	  sent = 0;
	  do
	    {
	      iob.size = len > MAX_BLOCK_SIZE ? MAX_BLOCK_SIZE : len;
	      iob.buf = myaddr;
	      sprintf (cmd, "memory b c 0x%x=0x00 l=%d", (int) memaddr, iob.size);
	      retval = ExeAppReq ("GDB", GWRITEBLOCK, cmd, &iob);
	      if (retval != 0)
		break;
	      len -= iob.size;
	      memaddr += iob.size;
	      myaddr += iob.size;
	      sent += iob.size;
	    }
	  while (len > 0);
	}
    }
  else
    {
      unsigned char *tmp;
      unsigned char *t;
      int i;

      tmp = alloca (len + 100);
      t = tmp;
      memset (tmp + len, 0xff, 100);

      sent = 0;
      do
	{
	  iob.size = len > MAX_BLOCK_SIZE ? MAX_BLOCK_SIZE : len;
	  iob.buf = tmp;
	  sprintf (cmd, "memory b 0x%x l=%d", (int) memaddr, iob.size);
	  retval = ExeAppReq ("GDB", GREADBLOCK, cmd, &iob);
	  if (retval != 0)
	    break;
	  len -= iob.size;
	  memaddr += iob.size;
	  sent += iob.size;
	  tmp += iob.size;
	}
      while (len > 0);

      if (retval == 0)
	{
	  for (i = 0; i < 100; i++)
	    {
	      if (t[sent + i] != 0xff)
		{
		  warning ("GREADBLOCK trashed bytes after transfer area.");
		  break;
		}
	    }
	  memcpy (myaddr, t, sent);
	}
    }

  if (retval != 0)
    error ("3: ExeAppReq returned %d: cmd = %s", retval, cmd);

  return sent;
}

static void
v850ice_files_info (struct target_ops *ignore)
{
  puts_filtered ("Debugging a target via the NEC V850 ICE.\n");
}

static int
v850ice_insert_breakpoint (CORE_ADDR addr, char *contents_cache)
{
  long retval;
  char cmd[100];
  char buf[256];
  struct MessageIO iob;

  iob.size = 0;
  iob.buf = buf;
  sprintf (cmd, "%d, ", addr);

  retval = ExeAppReq ("GDB", GSETBREAK, cmd, &iob);
  if (retval)
    error ("ExeAppReq (GSETBREAK) returned %d: cmd = %s", retval, cmd);

  return 0;
}

static int
v850ice_remove_breakpoint (CORE_ADDR addr, char *contents_cache)
{
  long retval;
  char cmd[100];
  char buf[256];
  struct MessageIO iob;

  iob.size = 0;
  iob.buf = buf;

  sprintf (cmd, "%d, ", addr);

  retval = ExeAppReq ("GDB", GREMOVEBREAK, cmd, &iob);
  if (retval)
    error ("ExeAppReq (GREMOVEBREAK) returned %d: cmd = %s", retval, cmd);

  return 0;
}

static void
v850ice_kill (void)
{
  target_mourn_inferior ();
  inferior_ptid = null_ptid;
}

static void
v850ice_mourn (void)
{
}

static void
v850ice_load (char *filename, int from_tty)
{
  struct MessageIO iob;
  char buf[256];

  iob.size = 0;
  iob.buf = buf;
  generic_load (filename, from_tty);
  ExeAppReq ("GDB", GDOWNLOAD, filename, &iob);
}

static int
ice_file (char *arg)
{
  char *s;

  target_detach (NULL, 0);
  pop_target ();

  printf_unfiltered ("\n");

  s = arg;
  while (*s != '\0')
    {
      if (*s == '\\')
	*s = '/';
      s++;
    }

  /* Safegaurd against confusing the breakpoint routines... */
  delete_command (NULL, 0);

  /* Must supress from_tty, otherwise we could start asking if the
     user really wants to load a new symbol table, etc... */
  printf_unfiltered ("Reading symbols from %s...", arg);
  exec_open (arg, 0);
  symbol_file_add_main (arg, 0);
  printf_unfiltered ("done\n");

  /* exec_open will kill our target, so reinstall the ICE as
     the target. */
  v850ice_open (NULL, 0);

  togdb_force_update ();
  return 1;
}

static int
ice_cont (char *c)
{
  printf_filtered ("continue (ice)\n");
  ReplyMessage ((LRESULT) 1);

  if (gdbtk_interp == NULL)
    {
      continue_command (NULL, 1);
    }
  else
    Tcl_Eval (gdbtk_interp, "gdb_immediate continue");

  return 1;
}

static void
do_gdb (char *cmd, char *str, void (*func) (char *, int), int count)
{
  ReplyMessage ((LRESULT) 1);

  while (count--)
    {
      printf_unfiltered (str);

      if (gdbtk_interp == NULL)
	{
	  func (NULL, 0);
	}
      else
	Tcl_Eval (gdbtk_interp, cmd);
    }
}


static int
ice_stepi (char *c)
{
  int count = (int) c;

  do_gdb ("gdb_immediate stepi", "stepi (ice)\n", stepi_command, count);
  return 1;
}

static int
ice_nexti (char *c)
{
  int count = (int) c;

  do_gdb ("gdb_immediate nexti", "nexti (ice)\n", nexti_command, count);
  return 1;
}

static void
v850ice_command (char *arg, int from_tty)
{
  struct MessageIO iob;
  char buf[256];

  iob.buf = buf;
  iob.size = 0;
  ExeAppReq ("GDB", GCOMMAND, arg, &iob);
}

static void
togdb_force_update (void)
{
  if (gdbtk_interp != NULL)
    Tcl_Eval (gdbtk_interp, "gdbtk_update");
}

static void
view_source (CORE_ADDR addr)
{
  char c[256];

  if (gdbtk_interp != NULL)
    {
      sprintf (c, "catch {set src [lindex [ManagedWin::find SrcWin] 0]\n$src location BROWSE [gdb_loc *0x%x]}", addr);
      Tcl_Eval (gdbtk_interp, c);
    }
}

/* Define the target subroutine names */

static void
init_850ice_ops (void)
{
  v850ice_ops.to_shortname = "ice";
  v850ice_ops.to_longname = "NEC V850 ICE interface";
  v850ice_ops.to_doc = "Debug a system controlled by a NEC 850 ICE.";
  v850ice_ops.to_open = v850ice_open;
  v850ice_ops.to_close = v850ice_close;
  v850ice_ops.to_detach = v850ice_detach;
  v850ice_ops.to_resume = v850ice_resume;
  v850ice_ops.to_wait = v850ice_wait;
  v850ice_ops.to_fetch_registers = v850ice_fetch_registers;
  v850ice_ops.to_store_registers = v850ice_store_registers;
  v850ice_ops.to_prepare_to_store = v850ice_prepare_to_store;
  v850ice_ops.deprecated_xfer_memory = v850ice_xfer_memory;
  v850ice_ops.to_files_info = v850ice_files_info;
  v850ice_ops.to_insert_breakpoint = v850ice_insert_breakpoint;
  v850ice_ops.to_remove_breakpoint = v850ice_remove_breakpoint;
  v850ice_ops.to_kill = v850ice_kill;
  v850ice_ops.to_load = v850ice_load;
  v850ice_ops.to_mourn_inferior = v850ice_mourn;
  v850ice_ops.to_stop = v850ice_stop;
  v850ice_ops.to_stratum = process_stratum;
  v850ice_ops.to_has_all_memory = 1;
  v850ice_ops.to_has_memory = 1;
  v850ice_ops.to_has_stack = 1;
  v850ice_ops.to_has_registers = 1;
  v850ice_ops.to_has_execution = 1;
  v850ice_ops.to_magic = OPS_MAGIC;
}

void
_initialize_v850ice (void)
{
  init_850ice_ops ();
  add_target (&v850ice_ops);

  add_com ("ice", class_obscure, v850ice_command,
	   "Send command to ICE");
}
