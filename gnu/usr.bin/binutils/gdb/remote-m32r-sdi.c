/* Remote debugging interface for M32R/SDI.

   Copyright 2003 Free Software Foundation, Inc.

   Contributed by Renesas Technology Co.
   Written by Kei Sakamoto <sakamoto.kei@renesas.com>.

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
#include "gdbcmd.h"
#include "gdbcore.h"
#include "inferior.h"
#include "target.h"
#include "regcache.h"
#include "gdb_string.h"
#include <ctype.h>
#include <signal.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/time.h>
#include <signal.h>
#include <time.h>


#include "serial.h"

/* Descriptor for I/O to remote machine.  */

static struct serial *sdi_desc = NULL;

#define SDI_TIMEOUT 30


#define SDIPORT 3232

static char chip_name[64];

static int step_mode;
static unsigned long last_pc_addr = 0xffffffff;
static unsigned char last_pc_addr_data[2];

static int mmu_on = 0;

static int use_ib_breakpoints = 1;

#define MAX_BREAKPOINTS 1024
static int max_ib_breakpoints;
static unsigned long bp_address[MAX_BREAKPOINTS];
static unsigned char bp_data[MAX_BREAKPOINTS][4];
static const unsigned char ib_bp_entry_enable[] = {
  0x00, 0x00, 0x00, 0x06
};
static const unsigned char ib_bp_entry_disable[] = {
  0x00, 0x00, 0x00, 0x00
};

/* dbt -> nop */
static const unsigned char dbt_bp_entry[] = {
  0x10, 0xe0, 0x70, 0x00
};

#define MAX_ACCESS_BREAKS 4
static int max_access_breaks;
static unsigned long ab_address[MAX_ACCESS_BREAKS];
static unsigned int ab_type[MAX_ACCESS_BREAKS];
static unsigned int ab_size[MAX_ACCESS_BREAKS];
static CORE_ADDR hit_watchpoint_addr = 0;

static int interrupted = 0;

/* Forward data declarations */
extern struct target_ops m32r_ops;


/* Commands */
#define SDI_OPEN                 1
#define SDI_CLOSE                2
#define SDI_RELEASE              3
#define SDI_READ_CPU_REG         4
#define SDI_WRITE_CPU_REG        5
#define SDI_READ_MEMORY          6
#define SDI_WRITE_MEMORY         7
#define SDI_EXEC_CPU             8
#define SDI_STOP_CPU             9
#define SDI_WAIT_FOR_READY      10
#define SDI_GET_ATTR            11
#define SDI_SET_ATTR            12
#define SDI_STATUS              13

/* Attributes */
#define SDI_ATTR_NAME            1
#define SDI_ATTR_BRK             2
#define SDI_ATTR_ABRK            3
#define SDI_ATTR_CACHE           4
#define SDI_CACHE_TYPE_M32102    0
#define SDI_CACHE_TYPE_CHAOS     1
#define SDI_ATTR_MEM_ACCESS      5
#define SDI_MEM_ACCESS_DEBUG_DMA 0
#define SDI_MEM_ACCESS_MON_CODE  1

/* Registers */
#define SDI_REG_R0               0
#define SDI_REG_R1               1
#define SDI_REG_R2               2
#define SDI_REG_R3               3
#define SDI_REG_R4               4
#define SDI_REG_R5               5
#define SDI_REG_R6               6
#define SDI_REG_R7               7
#define SDI_REG_R8               8
#define SDI_REG_R9               9
#define SDI_REG_R10             10
#define SDI_REG_R11             11
#define SDI_REG_R12             12
#define SDI_REG_FP              13
#define SDI_REG_LR              14
#define SDI_REG_SP              15
#define SDI_REG_PSW             16
#define SDI_REG_CBR             17
#define SDI_REG_SPI             18
#define SDI_REG_SPU             19
#define SDI_REG_CR4             20
#define SDI_REG_EVB             21
#define SDI_REG_BPC             22
#define SDI_REG_CR7             23
#define SDI_REG_BBPSW           24
#define SDI_REG_CR9             25
#define SDI_REG_CR10            26
#define SDI_REG_CR11            27
#define SDI_REG_CR12            28
#define SDI_REG_WR              29
#define SDI_REG_BBPC            30
#define SDI_REG_PBP             31
#define SDI_REG_ACCH            32
#define SDI_REG_ACCL            33
#define SDI_REG_ACC1H           34
#define SDI_REG_ACC1L           35


/* Low level communication functions */

/* Check an ack packet from the target */
static int
get_ack (void)
{
  int c;

  if (!sdi_desc) 
    return -1;

  c = serial_readchar (sdi_desc, SDI_TIMEOUT);

  if (c < 0)
    return -1;

  if (c != '+')		/* error */
    return -1;

  return 0;
}

/* Send data to the target and check an ack packet */
static int
send_data (void *buf, int len)
{
  int ret;

  if (!sdi_desc) 
    return -1;

  if (serial_write (sdi_desc, buf, len) != 0)
    return -1;

  if (get_ack () == -1)
    return -1;

  return len;
}

/* Receive data from the target */
static int
recv_data (void *buf, int len)
{
  int total = 0;
  int c;

  if (!sdi_desc) 
    return -1;

  while (total < len)
    {
      c = serial_readchar (sdi_desc, SDI_TIMEOUT);

      if (c < 0)
	return -1;

      ((unsigned char *) buf)[total++] = c;
    }

  return len;
}

/* Store unsigned long parameter on packet */
static void
store_long_parameter (void *buf, long val)
{
  val = htonl (val);
  memcpy (buf, &val, 4);
}

/* Check if MMU is on */
static void
check_mmu_status (void)
{
  unsigned long val;
  unsigned char buf[2];

  /* Read PC address */
  buf[0] = SDI_READ_CPU_REG;
  buf[1] = SDI_REG_BPC;
  if (send_data (buf, 2) == -1)
    return;
  recv_data (&val, 4);
  val = ntohl (val);
  if ((val & 0xc0000000) == 0x80000000)
    {
      mmu_on = 1;
      return;
    }

  /* Read EVB address */
  buf[0] = SDI_READ_CPU_REG;
  buf[1] = SDI_REG_EVB;
  if (send_data (buf, 2) == -1)
    return;
  recv_data (&val, 4);
  val = ntohl (val);
  if ((val & 0xc0000000) == 0x80000000)
    {
      mmu_on = 1;
      return;
    }

  mmu_on = 0;
}


/* This is called not only when we first attach, but also when the
   user types "run" after having attached.  */
static void
m32r_create_inferior (char *execfile, char *args, char **env)
{
  CORE_ADDR entry_pt;

  if (args && *args)
    error ("Cannot pass arguments to remote STDEBUG process");

  if (execfile == 0 || exec_bfd == 0)
    error ("No executable file specified");

  if (remote_debug)
    fprintf_unfiltered (gdb_stdlog, "m32r_create_inferior(%s,%s)\n", execfile,
			args);

  entry_pt = bfd_get_start_address (exec_bfd);

  /* The "process" (board) is already stopped awaiting our commands, and
     the program is already downloaded.  We just set its PC and go.  */

  clear_proceed_status ();

  /* Tell wait_for_inferior that we've started a new process.  */
  init_wait_for_inferior ();

  /* Set up the "saved terminal modes" of the inferior
     based on what modes we are starting it with.  */
  target_terminal_init ();

  /* Install inferior's terminal modes.  */
  target_terminal_inferior ();

  proceed (entry_pt, TARGET_SIGNAL_DEFAULT, 0);
}

/* Open a connection to a remote debugger.
   NAME is the filename used for communication.  */

static void
m32r_open (char *args, int from_tty)
{
  struct hostent *host_ent;
  struct sockaddr_in server_addr;
  char *port_str, hostname[256];
  int port;
  unsigned char buf[2];
  int i, n;
  int yes = 1;

  if (remote_debug)
    fprintf_unfiltered (gdb_stdlog, "m32r_open(%d)\n", from_tty);

  target_preopen (from_tty);

  push_target (&m32r_ops);

  if (args == NULL)
    sprintf (hostname, "localhost:%d", SDIPORT);
  else
    {
      port_str = strchr (args, ':');
      if (port_str == NULL)
        sprintf (hostname, "%s:%d", args, SDIPORT);
      else
	strcpy (hostname, args);
    }

  sdi_desc = serial_open (hostname);
  if (!sdi_desc)
    error ("Connection refused\n");

  if (get_ack () == -1)
    error ("Cannot connect to SDI target\n");

  buf[0] = SDI_OPEN;
  if (send_data (buf, 1) == -1)
    error ("Cannot connect to SDI target\n");

  /* Get maximum number of ib breakpoints */
  buf[0] = SDI_GET_ATTR;
  buf[1] = SDI_ATTR_BRK;
  send_data (buf, 2);
  recv_data (buf, 1);
  max_ib_breakpoints = buf[0];
  if (remote_debug)
    printf_filtered ("Max IB Breakpoints = %d\n", max_ib_breakpoints);

  /* Initialize breakpoints. */
  for (i = 0; i < MAX_BREAKPOINTS; i++)
    bp_address[i] = 0xffffffff;

  /* Get maximum number of access breaks. */
  buf[0] = SDI_GET_ATTR;
  buf[1] = SDI_ATTR_ABRK;
  send_data (buf, 2);
  recv_data (buf, 1);
  max_access_breaks = buf[0];
  if (remote_debug)
    printf_filtered ("Max Access Breaks = %d\n", max_access_breaks);

  /* Initialize access breask. */
  for (i = 0; i < MAX_ACCESS_BREAKS; i++)
    ab_address[i] = 0x00000000;

  check_mmu_status ();

  /* Get the name of chip on target board. */
  buf[0] = SDI_GET_ATTR;
  buf[1] = SDI_ATTR_NAME;
  send_data (buf, 2);
  recv_data (chip_name, 64);

  if (from_tty)
    printf_filtered ("Remote %s connected to %s\n", target_shortname,
		     chip_name);
}

/* Close out all files and local state before this target loses control. */

static void
m32r_close (int quitting)
{
  unsigned char buf[1];

  if (remote_debug)
    fprintf_unfiltered (gdb_stdlog, "m32r_close(%d)\n", quitting);

  if (sdi_desc)
    {
      buf[0] = SDI_CLOSE;
      send_data (buf, 1);
      serial_close (sdi_desc);
      sdi_desc = NULL;
    }

  inferior_ptid = null_ptid;
  return;
}

/* Tell the remote machine to resume.  */

static void
m32r_resume (ptid_t ptid, int step, enum target_signal sig)
{
  unsigned long pc_addr, bp_addr, ab_addr;
  unsigned char buf[13];
  int i;

  if (remote_debug)
    {
      if (step)
	fprintf_unfiltered (gdb_stdlog, "\nm32r_resume(step)\n");
      else
	fprintf_unfiltered (gdb_stdlog, "\nm32r_resume(cont)\n");
    }

  check_mmu_status ();

  pc_addr = read_pc ();
  if (remote_debug)
    fprintf_unfiltered (gdb_stdlog, "pc <= 0x%lx\n", pc_addr);

  /* At pc address there is a parallel instruction with +2 offset,
     so we have to make it a serial instruction or avoid it. */
  if (pc_addr == last_pc_addr)
    {
      /* Avoid a parallel nop. */
      if (last_pc_addr_data[0] == 0xf0 && last_pc_addr_data[1] == 0x00)
	{
	  pc_addr += 2;
	  /* Now we can forget this instruction. */
	  last_pc_addr = 0xffffffff;
	}
      /* Clear a parallel bit. */
      else
	{
	  buf[0] = SDI_WRITE_MEMORY;
	  if (TARGET_BYTE_ORDER == BFD_ENDIAN_BIG)
	    store_long_parameter (buf + 1, pc_addr);
	  else
	    store_long_parameter (buf + 1, pc_addr - 1);
	  store_long_parameter (buf + 5, 1);
	  buf[9] = last_pc_addr_data[0] & 0x7f;
	  send_data (buf, 10);
	}
    }

  /* Set PC. */
  buf[0] = SDI_WRITE_CPU_REG;
  buf[1] = SDI_REG_BPC;
  store_long_parameter (buf + 2, pc_addr);
  send_data (buf, 6);

  /* step mode. */
  step_mode = step;
  if (step)
    {
      /* Set PBP. */
      buf[0] = SDI_WRITE_CPU_REG;
      buf[1] = SDI_REG_PBP;
      store_long_parameter (buf + 2, pc_addr | 1);
      send_data (buf, 6);
    }
  else
    {
      int ib_breakpoints;

      if (use_ib_breakpoints)
	ib_breakpoints = max_ib_breakpoints;
      else
	ib_breakpoints = 0;

      /* Set ib breakpoints. */
      for (i = 0; i < ib_breakpoints; i++)
	{
	  bp_addr = bp_address[i];
	  if (bp_addr != 0xffffffff && bp_addr != pc_addr)
	    {
	      /* Set PBP. */
	      buf[0] = SDI_WRITE_MEMORY;
	      store_long_parameter (buf + 1, 0xffff8000 + 4 * i);
	      store_long_parameter (buf + 5, 4);
	      if (TARGET_BYTE_ORDER == BFD_ENDIAN_BIG)
		{
		  buf[9] = ib_bp_entry_enable[0];
		  buf[10] = ib_bp_entry_enable[1];
		  buf[11] = ib_bp_entry_enable[2];
		  buf[12] = ib_bp_entry_enable[3];
		}
	      else
		{
		  buf[9] = ib_bp_entry_enable[3];
		  buf[10] = ib_bp_entry_enable[2];
		  buf[11] = ib_bp_entry_enable[1];
		  buf[12] = ib_bp_entry_enable[0];
		}
	      send_data (buf, 13);

	      buf[0] = SDI_WRITE_MEMORY;
	      store_long_parameter (buf + 1, 0xffff8080 + 4 * i);
	      store_long_parameter (buf + 5, 4);
	      store_unsigned_integer (buf + 9, 4, bp_addr);
	      send_data (buf, 13);
	    }
	}

      /* Set dbt breakpoints. */
      for (i = ib_breakpoints; i < MAX_BREAKPOINTS; i++)
	{
	  bp_addr = bp_address[i];
	  if (bp_addr != 0xffffffff && bp_addr != pc_addr)
	    {
	      if (!mmu_on)
		bp_addr &= 0x7fffffff;

	      /* Write DBT instruction. */
	      buf[0] = SDI_WRITE_MEMORY;
	      if ((bp_addr & 2) == 0 && bp_addr != (pc_addr & 0xfffffffc))
		{
		  store_long_parameter (buf + 1, bp_addr);
		  store_long_parameter (buf + 5, 4);
		  if (TARGET_BYTE_ORDER == BFD_ENDIAN_BIG)
		    {
		      buf[9] = dbt_bp_entry[0];
		      buf[10] = dbt_bp_entry[1];
		      buf[11] = dbt_bp_entry[2];
		      buf[12] = dbt_bp_entry[3];
		    }
		  else
		    {
		      buf[9] = dbt_bp_entry[3];
		      buf[10] = dbt_bp_entry[2];
		      buf[11] = dbt_bp_entry[1];
		      buf[12] = dbt_bp_entry[0];
		    }
		  send_data (buf, 13);
		}
	      else
		{
		  if (TARGET_BYTE_ORDER == BFD_ENDIAN_BIG)
		    store_long_parameter (buf + 1, bp_addr);
		  else if ((bp_addr & 2) == 0)
		    store_long_parameter (buf + 1, bp_addr + 2);
		  else
		    store_long_parameter (buf + 1, bp_addr - 2);
		  store_long_parameter (buf + 5, 2);
		  if (TARGET_BYTE_ORDER == BFD_ENDIAN_BIG)
		    {
		      buf[9] = dbt_bp_entry[0];
		      buf[10] = dbt_bp_entry[1];
		    }
		  else
		    {
		      buf[9] = dbt_bp_entry[1];
		      buf[10] = dbt_bp_entry[0];
		    }
		  send_data (buf, 11);
		}
	    }
	}

      /* Set access breaks. */
      for (i = 0; i < max_access_breaks; i++)
	{
	  ab_addr = ab_address[i];
	  if (ab_addr != 0x00000000)
	    {
	      /* DBC register */
	      buf[0] = SDI_WRITE_MEMORY;
	      store_long_parameter (buf + 1, 0xffff8100 + 4 * i);
	      store_long_parameter (buf + 5, 4);
	      if (TARGET_BYTE_ORDER == BFD_ENDIAN_BIG)
		{
		  buf[9] = 0x00;
		  buf[10] = 0x00;
		  buf[11] = 0x00;
		  switch (ab_type[i])
		    {
		    case 0:	/* write watch */
		      buf[12] = 0x86;
		      break;
		    case 1:	/* read watch */
		      buf[12] = 0x46;
		      break;
		    case 2:	/* access watch */
		      buf[12] = 0x06;
		      break;
		    }
		}
	      else
		{
		  switch (ab_type[i])
		    {
		    case 0:	/* write watch */
		      buf[9] = 0x86;
		      break;
		    case 1:	/* read watch */
		      buf[9] = 0x46;
		      break;
		    case 2:	/* access watch */
		      buf[9] = 0x06;
		      break;
		    }
		  buf[10] = 0x00;
		  buf[11] = 0x00;
		  buf[12] = 0x00;
		}
	      send_data (buf, 13);

	      /* DBAH register */
	      buf[0] = SDI_WRITE_MEMORY;
	      store_long_parameter (buf + 1, 0xffff8180 + 4 * i);
	      store_long_parameter (buf + 5, 4);
	      store_unsigned_integer (buf + 9, 4, ab_addr);
	      send_data (buf, 13);

	      /* DBAL register */
	      buf[0] = SDI_WRITE_MEMORY;
	      store_long_parameter (buf + 1, 0xffff8200 + 4 * i);
	      store_long_parameter (buf + 5, 4);
	      store_long_parameter (buf + 9, 0xffffffff);
	      send_data (buf, 13);

	      /* DBD register */
	      buf[0] = SDI_WRITE_MEMORY;
	      store_long_parameter (buf + 1, 0xffff8280 + 4 * i);
	      store_long_parameter (buf + 5, 4);
	      store_long_parameter (buf + 9, 0x00000000);
	      send_data (buf, 13);

	      /* DBDM register */
	      buf[0] = SDI_WRITE_MEMORY;
	      store_long_parameter (buf + 1, 0xffff8300 + 4 * i);
	      store_long_parameter (buf + 5, 4);
	      store_long_parameter (buf + 9, 0x00000000);
	      send_data (buf, 13);
	    }
	}

      /* Unset PBP. */
      buf[0] = SDI_WRITE_CPU_REG;
      buf[1] = SDI_REG_PBP;
      store_long_parameter (buf + 2, 0x00000000);
      send_data (buf, 6);
    }

  buf[0] = SDI_EXEC_CPU;
  send_data (buf, 1);

  /* Without this, some commands which require an active target (such as kill)
     won't work.  This variable serves (at least) double duty as both the pid
     of the target process (if it has such), and as a flag indicating that a
     target is active.  These functions should be split out into seperate
     variables, especially since GDB will someday have a notion of debugging
     several processes.  */
  inferior_ptid = pid_to_ptid (32);

  return;
}

/* Wait until the remote machine stops, then return,
   storing status in STATUS just as `wait' would.  */

static void
gdb_cntrl_c (int signo)
{
  if (remote_debug)
    fprintf_unfiltered (gdb_stdlog, "interrupt\n");
  interrupted = 1;
}

static ptid_t
m32r_wait (ptid_t ptid, struct target_waitstatus *status)
{
  static RETSIGTYPE (*prev_sigint) ();
  unsigned long bp_addr, pc_addr;
  long i;
  unsigned char buf[13];
  unsigned long val;
  int ret, c;

  if (remote_debug)
    fprintf_unfiltered (gdb_stdlog, "m32r_wait()\n");

  status->kind = TARGET_WAITKIND_EXITED;
  status->value.sig = 0;

  interrupted = 0;
  prev_sigint = signal (SIGINT, gdb_cntrl_c);

  /* Wait for ready */
  buf[0] = SDI_WAIT_FOR_READY;
  if (serial_write (sdi_desc, buf, 1) != 0)
    error ("Remote connection closed");

  while (1)
    {
      c = serial_readchar (sdi_desc, SDI_TIMEOUT);
      if (c < 0)
	error ("Remote connection closed");

      if (c == '-')	/* error */
	{
	  status->kind = TARGET_WAITKIND_STOPPED;
	  status->value.sig = TARGET_SIGNAL_HUP;
	  return inferior_ptid;
	}
      else if (c == '+')	/* stopped */
	break;

      if (interrupted)
	ret = serial_write (sdi_desc, "!", 1);	/* packet to interrupt */
      else
	ret = serial_write (sdi_desc, ".", 1);	/* packet to wait */
      if (ret != 0)
	error ("Remote connection closed");
    }

  status->kind = TARGET_WAITKIND_STOPPED;
  if (interrupted)
    status->value.sig = TARGET_SIGNAL_INT;
  else
    status->value.sig = TARGET_SIGNAL_TRAP;

  interrupted = 0;
  signal (SIGINT, prev_sigint);

  check_mmu_status ();

  /* Recover parallel bit. */
  if (last_pc_addr != 0xffffffff)
    {
      buf[0] = SDI_WRITE_MEMORY;
      if (TARGET_BYTE_ORDER == BFD_ENDIAN_BIG)
	store_long_parameter (buf + 1, last_pc_addr);
      else
	store_long_parameter (buf + 1, last_pc_addr - 1);
      store_long_parameter (buf + 5, 1);
      buf[9] = last_pc_addr_data[0];
      send_data (buf, 10);
      last_pc_addr = 0xffffffff;
    }

  /* Breakpoints are inserted only for "next" command */
  if (!step_mode)
    {
      int ib_breakpoints;

      if (use_ib_breakpoints)
	ib_breakpoints = max_ib_breakpoints;
      else
	ib_breakpoints = 0;

      /* Set back pc by 2 if m32r is stopped with dbt. */
      buf[0] = SDI_READ_CPU_REG;
      buf[1] = SDI_REG_BPC;
      send_data (buf, 2);
      recv_data (&val, 4);
      pc_addr = ntohl (val) - 2;
      for (i = ib_breakpoints; i < MAX_BREAKPOINTS; i++)
	{
	  if (pc_addr == bp_address[i])
	    {
	      buf[0] = SDI_WRITE_CPU_REG;
	      buf[1] = SDI_REG_BPC;
	      store_long_parameter (buf + 2, pc_addr);
	      send_data (buf, 6);

	      /* If there is a parallel instruction with +2 offset at pc
	         address, we have to take care of it later. */
	      if ((pc_addr & 0x2) != 0)
		{
		  if (TARGET_BYTE_ORDER == BFD_ENDIAN_BIG)
		    {
		      if ((bp_data[i][2] & 0x80) != 0)
			{
			  last_pc_addr = pc_addr;
			  last_pc_addr_data[0] = bp_data[i][2];
			  last_pc_addr_data[1] = bp_data[i][3];
			}
		    }
		  else
		    {
		      if ((bp_data[i][1] & 0x80) != 0)
			{
			  last_pc_addr = pc_addr;
			  last_pc_addr_data[0] = bp_data[i][1];
			  last_pc_addr_data[1] = bp_data[i][0];
			}
		    }
		}
	      break;
	    }
	}

      /* Remove ib breakpoints. */
      for (i = 0; i < ib_breakpoints; i++)
	{
	  if (bp_address[i] != 0xffffffff)
	    {
	      buf[0] = SDI_WRITE_MEMORY;
	      store_long_parameter (buf + 1, 0xffff8000 + 4 * i);
	      store_long_parameter (buf + 5, 4);
	      buf[9] = ib_bp_entry_disable[0];
	      buf[10] = ib_bp_entry_disable[1];
	      buf[11] = ib_bp_entry_disable[2];
	      buf[12] = ib_bp_entry_disable[3];
	      send_data (buf, 13);
	    }
	}
      /* Remove dbt breakpoints. */
      for (i = ib_breakpoints; i < MAX_BREAKPOINTS; i++)
	{
	  bp_addr = bp_address[i];
	  if (bp_addr != 0xffffffff)
	    {
	      if (!mmu_on)
		bp_addr &= 0x7fffffff;
	      buf[0] = SDI_WRITE_MEMORY;
	      store_long_parameter (buf + 1, bp_addr & 0xfffffffc);
	      store_long_parameter (buf + 5, 4);
	      buf[9] = bp_data[i][0];
	      buf[10] = bp_data[i][1];
	      buf[11] = bp_data[i][2];
	      buf[12] = bp_data[i][3];
	      send_data (buf, 13);
	    }
	}

      /* Remove access breaks. */
      hit_watchpoint_addr = 0;
      for (i = 0; i < max_access_breaks; i++)
	{
	  if (ab_address[i] != 0x00000000)
	    {
	      buf[0] = SDI_READ_MEMORY;
	      store_long_parameter (buf + 1, 0xffff8100 + 4 * i);
	      store_long_parameter (buf + 5, 4);
	      serial_write (sdi_desc, buf, 9);
	      c = serial_readchar (sdi_desc, SDI_TIMEOUT);
	      if (c != '-' && recv_data (buf, 4) != -1)
		{
		  if (TARGET_BYTE_ORDER == BFD_ENDIAN_BIG)
		    {
		      if ((buf[3] & 0x1) == 0x1)
			hit_watchpoint_addr = ab_address[i];
		    }
		  else
		    {
		      if ((buf[0] & 0x1) == 0x1)
			hit_watchpoint_addr = ab_address[i];
		    }
		}

	      buf[0] = SDI_WRITE_MEMORY;
	      store_long_parameter (buf + 1, 0xffff8100 + 4 * i);
	      store_long_parameter (buf + 5, 4);
	      store_long_parameter (buf + 9, 0x00000000);
	      send_data (buf, 13);
	    }
	}

      if (remote_debug)
	fprintf_unfiltered (gdb_stdlog, "pc => 0x%lx\n", pc_addr);
    }
  else
    last_pc_addr = 0xffffffff;

  return inferior_ptid;
}

/* Terminate the open connection to the remote debugger.
   Use this when you want to detach and do something else
   with your gdb.  */
static void
m32r_detach (char *args, int from_tty)
{
  if (remote_debug)
    fprintf_unfiltered (gdb_stdlog, "m32r_detach(%d)\n", from_tty);

  m32r_resume (inferior_ptid, 0, 0);

  /* calls m32r_close to do the real work */
  pop_target ();
  if (from_tty)
    fprintf_unfiltered (gdb_stdlog, "Ending remote %s debugging\n",
			target_shortname);
}

/* Return the id of register number REGNO. */

static int
get_reg_id (int regno)
{
  switch (regno)
    {
    case 20:
      return SDI_REG_BBPC;
    case 21:
      return SDI_REG_BPC;
    case 22:
      return SDI_REG_ACCL;
    case 23:
      return SDI_REG_ACCH;
    case 24:
      return SDI_REG_EVB;
    }

  return regno;
}

/* Read the remote registers into the block REGS.  */

static void m32r_fetch_register (int);

static void
m32r_fetch_registers (void)
{
  int regno;

  for (regno = 0; regno < NUM_REGS; regno++)
    m32r_fetch_register (regno);
}

/* Fetch register REGNO, or all registers if REGNO is -1.
   Returns errno value.  */
static void
m32r_fetch_register (int regno)
{
  unsigned long val, val2, regid;
  unsigned char buf[2];

  if (regno == -1)
    m32r_fetch_registers ();
  else
    {
      char buffer[MAX_REGISTER_SIZE];

      regid = get_reg_id (regno);
      buf[0] = SDI_READ_CPU_REG;
      buf[1] = regid;
      send_data (buf, 2);
      recv_data (&val, 4);
      val = ntohl (val);

      if (regid == SDI_REG_PSW)
	{
	  buf[0] = SDI_READ_CPU_REG;
	  buf[1] = SDI_REG_BBPSW;
	  send_data (buf, 2);
	  recv_data (&val2, 4);
	  val2 = ntohl (val2);
	  val = ((0x00c1 & val2) << 8) | ((0xc100 & val) >> 8);
	}

      if (remote_debug)
	fprintf_unfiltered (gdb_stdlog, "m32r_fetch_register(%d,0x%08lx)\n",
			    regno, val);

      /* We got the number the register holds, but gdb expects to see a
         value in the target byte ordering.  */
      store_unsigned_integer (buffer, 4, val);
      supply_register (regno, buffer);
    }
  return;
}

/* Store the remote registers from the contents of the block REGS.  */

static void m32r_store_register (int);

static void
m32r_store_registers (void)
{
  int regno;

  for (regno = 0; regno < NUM_REGS; regno++)
    m32r_store_register (regno);

  registers_changed ();
}

/* Store register REGNO, or all if REGNO == 0.
   Return errno value.  */
static void
m32r_store_register (int regno)
{
  int regid;
  ULONGEST regval, tmp;
  unsigned char buf[6];

  if (regno == -1)
    m32r_store_registers ();
  else
    {
      regcache_cooked_read_unsigned (current_regcache, regno, &regval);
      regid = get_reg_id (regno);

      if (regid == SDI_REG_PSW)
	{
	  unsigned long psw, bbpsw;

	  buf[0] = SDI_READ_CPU_REG;
	  buf[1] = SDI_REG_PSW;
	  send_data (buf, 2);
	  recv_data (&psw, 4);
	  psw = ntohl (psw);

	  buf[0] = SDI_READ_CPU_REG;
	  buf[1] = SDI_REG_BBPSW;
	  send_data (buf, 2);
	  recv_data (&bbpsw, 4);
	  bbpsw = ntohl (bbpsw);

	  tmp = (0x00c1 & psw) | ((0x00c1 & regval) << 8);
	  buf[0] = SDI_WRITE_CPU_REG;
	  buf[1] = SDI_REG_PSW;
	  store_long_parameter (buf + 2, tmp);
	  send_data (buf, 6);

	  tmp = (0x0030 & bbpsw) | ((0xc100 & regval) >> 8);
	  buf[0] = SDI_WRITE_CPU_REG;
	  buf[1] = SDI_REG_BBPSW;
	  store_long_parameter (buf + 2, tmp);
	  send_data (buf, 6);
	}
      else
	{
	  buf[0] = SDI_WRITE_CPU_REG;
	  buf[1] = regid;
	  store_long_parameter (buf + 2, regval);
	  send_data (buf, 6);
	}

      if (remote_debug)
	fprintf_unfiltered (gdb_stdlog, "m32r_store_register(%d,0x%08lu)\n",
			    regno, (unsigned long) regval);
    }
}

/* Get ready to modify the registers array.  On machines which store
   individual registers, this doesn't need to do anything.  On machines
   which store all the registers in one fell swoop, this makes sure
   that registers contains all the registers from the program being
   debugged.  */

static void
m32r_prepare_to_store (void)
{
  /* Do nothing, since we can store individual regs */
  if (remote_debug)
    fprintf_unfiltered (gdb_stdlog, "m32r_prepare_to_store()\n");
}

static void
m32r_files_info (struct target_ops *target)
{
  char *file = "nothing";

  if (exec_bfd)
    {
      file = bfd_get_filename (exec_bfd);
      printf_filtered ("\tAttached to %s running program %s\n",
		       chip_name, file);
    }
}

/* Read/Write memory.  */
static int
m32r_xfer_memory (CORE_ADDR memaddr, char *myaddr, int len,
		  int write,
		  struct mem_attrib *attrib, struct target_ops *target)
{
  unsigned long taddr;
  unsigned char buf[0x2000];
  int ret, c;

  taddr = memaddr;

  if (!mmu_on)
    {
      if ((taddr & 0xa0000000) == 0x80000000)
	taddr &= 0x7fffffff;
    }

  if (remote_debug)
    {
      if (write)
	fprintf_unfiltered (gdb_stdlog, "m32r_xfer_memory(%08lx,%d,write)\n",
			    memaddr, len);
      else
	fprintf_unfiltered (gdb_stdlog, "m32r_xfer_memory(%08lx,%d,read)\n",
			    memaddr, len);
    }

  if (write)
    {
      buf[0] = SDI_WRITE_MEMORY;
      store_long_parameter (buf + 1, taddr);
      store_long_parameter (buf + 5, len);
      if (len < 0x1000)
	{
	  memcpy (buf + 9, myaddr, len);
	  ret = send_data (buf, len + 9) - 9;
	}
      else
	{
	  if (serial_write (sdi_desc, buf, 9) != 0)
	    {
	      if (remote_debug)
		fprintf_unfiltered (gdb_stdlog,
				    "m32r_xfer_memory() failed\n");
	      return 0;
	    }
	  ret = send_data (myaddr, len);
	}
    }
  else
    {
      buf[0] = SDI_READ_MEMORY;
      store_long_parameter (buf + 1, taddr);
      store_long_parameter (buf + 5, len);
      if (serial_write (sdi_desc, buf, 9) != 0)
	{
	  if (remote_debug)
	    fprintf_unfiltered (gdb_stdlog, "m32r_xfer_memory() failed\n");
	  return 0;
	}

      c = serial_readchar (sdi_desc, SDI_TIMEOUT);
      if (c < 0 || c == '-')
	{
	  if (remote_debug)
	    fprintf_unfiltered (gdb_stdlog, "m32r_xfer_memory() failed\n");
	  return 0;
	}

      ret = recv_data (myaddr, len);
    }

  if (ret <= 0)
    {
      if (remote_debug)
	fprintf_unfiltered (gdb_stdlog, "m32r_xfer_memory() fails\n");
      return 0;
    }

  return ret;
}

static void
m32r_kill (void)
{
  if (remote_debug)
    fprintf_unfiltered (gdb_stdlog, "m32r_kill()\n");

  inferior_ptid = null_ptid;

  return;
}

/* Clean up when a program exits.

   The program actually lives on in the remote processor's RAM, and may be
   run again without a download.  Don't leave it full of breakpoint
   instructions.  */

static void
m32r_mourn_inferior (void)
{
  if (remote_debug)
    fprintf_unfiltered (gdb_stdlog, "m32r_mourn_inferior()\n");

  remove_breakpoints ();
  generic_mourn_inferior ();
}

static int
m32r_insert_breakpoint (CORE_ADDR addr, char *shadow)
{
  int ib_breakpoints;
  unsigned char buf[13];
  int i, c;

  if (remote_debug)
    fprintf_unfiltered (gdb_stdlog, "m32r_insert_breakpoint(%08lx,\"%s\")\n",
			addr, shadow);

  if (use_ib_breakpoints)
    ib_breakpoints = max_ib_breakpoints;
  else
    ib_breakpoints = 0;

  for (i = 0; i < MAX_BREAKPOINTS; i++)
    {
      if (bp_address[i] == 0xffffffff)
	{
	  bp_address[i] = addr;
	  if (i >= ib_breakpoints)
	    {
	      buf[0] = SDI_READ_MEMORY;
	      if (mmu_on)
		store_long_parameter (buf + 1, addr & 0xfffffffc);
	      else
		store_long_parameter (buf + 1, addr & 0x7ffffffc);
	      store_long_parameter (buf + 5, 4);
	      serial_write (sdi_desc, buf, 9);
	      c = serial_readchar (sdi_desc, SDI_TIMEOUT);
	      if (c != '-')
		recv_data (bp_data[i], 4);
	    }
	  return 0;
	}
    }

  error ("Too many breakpoints");
  return 1;
}

static int
m32r_remove_breakpoint (CORE_ADDR addr, char *shadow)
{
  int i;

  if (remote_debug)
    fprintf_unfiltered (gdb_stdlog, "m32r_remove_breakpoint(%08lx,\"%s\")\n",
			addr, shadow);

  for (i = 0; i < MAX_BREAKPOINTS; i++)
    {
      if (bp_address[i] == addr)
	{
	  bp_address[i] = 0xffffffff;
	  break;
	}
    }

  return 0;
}

static void
m32r_load (char *args, int from_tty)
{
  struct cleanup *old_chain;
  asection *section;
  bfd *pbfd;
  bfd_vma entry;
  char *filename;
  int quiet;
  int nostart;
  time_t start_time, end_time;	/* Start and end times of download */
  unsigned long data_count;	/* Number of bytes transferred to memory */
  int ret;
  static RETSIGTYPE (*prev_sigint) ();

  /* for direct tcp connections, we can do a fast binary download */
  quiet = 0;
  nostart = 0;
  filename = NULL;

  while (*args != '\000')
    {
      char *arg;

      while (isspace (*args))
	args++;

      arg = args;

      while ((*args != '\000') && !isspace (*args))
	args++;

      if (*args != '\000')
	*args++ = '\000';

      if (*arg != '-')
	filename = arg;
      else if (strncmp (arg, "-quiet", strlen (arg)) == 0)
	quiet = 1;
      else if (strncmp (arg, "-nostart", strlen (arg)) == 0)
	nostart = 1;
      else
	error ("Unknown option `%s'", arg);
    }

  if (!filename)
    filename = get_exec_file (1);

  pbfd = bfd_openr (filename, gnutarget);
  if (pbfd == NULL)
    {
      perror_with_name (filename);
      return;
    }
  old_chain = make_cleanup_bfd_close (pbfd);

  if (!bfd_check_format (pbfd, bfd_object))
    error ("\"%s\" is not an object file: %s", filename,
	   bfd_errmsg (bfd_get_error ()));

  start_time = time (NULL);
  data_count = 0;

  interrupted = 0;
  prev_sigint = signal (SIGINT, gdb_cntrl_c);

  for (section = pbfd->sections; section; section = section->next)
    {
      if (bfd_get_section_flags (pbfd, section) & SEC_LOAD)
	{
	  bfd_vma section_address;
	  bfd_size_type section_size;
	  file_ptr fptr;
	  int n;

	  section_address = bfd_section_lma (pbfd, section);
	  section_size = bfd_get_section_size_before_reloc (section);

	  if (!mmu_on)
	    {
	      if ((section_address & 0xa0000000) == 0x80000000)
		section_address &= 0x7fffffff;
	    }

	  if (!quiet)
	    printf_filtered ("[Loading section %s at 0x%lx (%d bytes)]\n",
			     bfd_get_section_name (pbfd, section),
			     section_address, (int) section_size);

	  fptr = 0;

	  data_count += section_size;

	  n = 0;
	  while (section_size > 0)
	    {
	      char unsigned buf[0x1000 + 9];
	      int count;

	      count = min (section_size, 0x1000);

	      buf[0] = SDI_WRITE_MEMORY;
	      store_long_parameter (buf + 1, section_address);
	      store_long_parameter (buf + 5, count);

	      bfd_get_section_contents (pbfd, section, buf + 9, fptr, count);
	      if (send_data (buf, count + 9) <= 0)
		error ("Error while downloading %s section.",
		       bfd_get_section_name (pbfd, section));

	      if (!quiet)
		{
		  printf_unfiltered (".");
		  if (n++ > 60)
		    {
		      printf_unfiltered ("\n");
		      n = 0;
		    }
		  gdb_flush (gdb_stdout);
		}

	      section_address += count;
	      fptr += count;
	      section_size -= count;

	      if (interrupted)
		break;
	    }

	  if (!quiet && !interrupted)
	    {
	      printf_unfiltered ("done.\n");
	      gdb_flush (gdb_stdout);
	    }
	}

      if (interrupted)
	{
	  printf_unfiltered ("Interrupted.\n");
	  break;
	}
    }

  interrupted = 0;
  signal (SIGINT, prev_sigint);

  end_time = time (NULL);

  /* Make the PC point at the start address */
  if (exec_bfd)
    write_pc (bfd_get_start_address (exec_bfd));

  inferior_ptid = null_ptid;	/* No process now */

  /* This is necessary because many things were based on the PC at the time
     that we attached to the monitor, which is no longer valid now that we
     have loaded new code (and just changed the PC).  Another way to do this
     might be to call normal_stop, except that the stack may not be valid,
     and things would get horribly confused... */

  clear_symtab_users ();

  if (!nostart)
    {
      entry = bfd_get_start_address (pbfd);

      if (!quiet)
	printf_unfiltered ("[Starting %s at 0x%lx]\n", filename, entry);
    }

  print_transfer_performance (gdb_stdout, data_count, 0,
			      end_time - start_time);

  do_cleanups (old_chain);
}

static void
m32r_stop (void)
{
  unsigned char buf[1];

  if (remote_debug)
    fprintf_unfiltered (gdb_stdlog, "m32r_stop()\n");

  buf[0] = SDI_STOP_CPU;
  send_data (buf, 1);

  return;
}


/* Tell whether this target can support a hardware breakpoint.  CNT
   is the number of hardware breakpoints already installed.  This
   implements the TARGET_CAN_USE_HARDWARE_WATCHPOINT macro.  */

int
m32r_can_use_hw_watchpoint (int type, int cnt, int othertype)
{
  return sdi_desc != NULL && cnt < max_access_breaks;
}

/* Set a data watchpoint.  ADDR and LEN should be obvious.  TYPE is 0
   for a write watchpoint, 1 for a read watchpoint, or 2 for a read/write
   watchpoint. */

int
m32r_insert_watchpoint (CORE_ADDR addr, int len, int type)
{
  int i;

  if (remote_debug)
    fprintf_unfiltered (gdb_stdlog, "m32r_insert_watchpoint(%08lx,%d,%d)\n",
			addr, len, type);

  for (i = 0; i < MAX_ACCESS_BREAKS; i++)
    {
      if (ab_address[i] == 0x00000000)
	{
	  ab_address[i] = addr;
	  ab_size[i] = len;
	  ab_type[i] = type;
	  return 0;
	}
    }

  error ("Too many watchpoints");
  return 1;
}

int
m32r_remove_watchpoint (CORE_ADDR addr, int len, int type)
{
  int i;

  if (remote_debug)
    fprintf_unfiltered (gdb_stdlog, "m32r_remove_watchpoint(%08lx,%d,%d)\n",
			addr, len, type);

  for (i = 0; i < MAX_ACCESS_BREAKS; i++)
    {
      if (ab_address[i] == addr)
	{
	  ab_address[i] = 0x00000000;
	  break;
	}
    }

  return 0;
}

CORE_ADDR
m32r_stopped_data_address (void)
{
  return hit_watchpoint_addr;
}

int
m32r_stopped_by_watchpoint (void)
{
  return (hit_watchpoint_addr != 0x00000000);
}


static void
sdireset_command (char *args, int from_tty)
{
  unsigned char buf[1];

  if (remote_debug)
    fprintf_unfiltered (gdb_stdlog, "m32r_sdireset()\n");

  buf[0] = SDI_OPEN;
  send_data (buf, 1);

  inferior_ptid = null_ptid;
}


static void
sdistatus_command (char *args, int from_tty)
{
  unsigned char buf[4096];
  int i, c;

  if (remote_debug)
    fprintf_unfiltered (gdb_stdlog, "m32r_sdireset()\n");

  if (!sdi_desc)
    return;

  buf[0] = SDI_STATUS;
  send_data (buf, 1);
  for (i = 0; i < 4096; i++)
    {
      c = serial_readchar (sdi_desc, SDI_TIMEOUT);
      if (c < 0)
        return;
      buf[i] = c;
      if (c == 0)
        break;
    }    

  printf_filtered ("%s", buf);
}


static void
debug_chaos_command (char *args, int from_tty)
{
  unsigned char buf[3];

  buf[0] = SDI_SET_ATTR;
  buf[1] = SDI_ATTR_CACHE;
  buf[2] = SDI_CACHE_TYPE_CHAOS;
  send_data (buf, 3);
}


static void
use_debug_dma_command (char *args, int from_tty)
{
  unsigned char buf[3];

  buf[0] = SDI_SET_ATTR;
  buf[1] = SDI_ATTR_MEM_ACCESS;
  buf[2] = SDI_MEM_ACCESS_DEBUG_DMA;
  send_data (buf, 3);
}

static void
use_mon_code_command (char *args, int from_tty)
{
  unsigned char buf[3];

  buf[0] = SDI_SET_ATTR;
  buf[1] = SDI_ATTR_MEM_ACCESS;
  buf[2] = SDI_MEM_ACCESS_MON_CODE;
  send_data (buf, 3);
}


static void
use_ib_breakpoints_command (char *args, int from_tty)
{
  use_ib_breakpoints = 1;
}

static void
use_dbt_breakpoints_command (char *args, int from_tty)
{
  use_ib_breakpoints = 0;
}


/* Define the target subroutine names */

struct target_ops m32r_ops;

static void
init_m32r_ops (void)
{
  m32r_ops.to_shortname = "m32rsdi";
  m32r_ops.to_longname = "Remote M32R debugging over SDI interface";
  m32r_ops.to_doc = "Use an M32R board using SDI debugging protocol.";
  m32r_ops.to_open = m32r_open;
  m32r_ops.to_close = m32r_close;
  m32r_ops.to_detach = m32r_detach;
  m32r_ops.to_resume = m32r_resume;
  m32r_ops.to_wait = m32r_wait;
  m32r_ops.to_fetch_registers = m32r_fetch_register;
  m32r_ops.to_store_registers = m32r_store_register;
  m32r_ops.to_prepare_to_store = m32r_prepare_to_store;
  m32r_ops.to_xfer_memory = m32r_xfer_memory;
  m32r_ops.to_files_info = m32r_files_info;
  m32r_ops.to_insert_breakpoint = m32r_insert_breakpoint;
  m32r_ops.to_remove_breakpoint = m32r_remove_breakpoint;
  m32r_ops.to_can_use_hw_breakpoint = m32r_can_use_hw_watchpoint;
  m32r_ops.to_insert_watchpoint = m32r_insert_watchpoint;
  m32r_ops.to_remove_watchpoint = m32r_remove_watchpoint;
  m32r_ops.to_stopped_by_watchpoint = m32r_stopped_by_watchpoint;
  m32r_ops.to_stopped_data_address = m32r_stopped_data_address;
  m32r_ops.to_kill = m32r_kill;
  m32r_ops.to_load = m32r_load;
  m32r_ops.to_create_inferior = m32r_create_inferior;
  m32r_ops.to_mourn_inferior = m32r_mourn_inferior;
  m32r_ops.to_stop = m32r_stop;
  m32r_ops.to_stratum = process_stratum;
  m32r_ops.to_has_all_memory = 1;
  m32r_ops.to_has_memory = 1;
  m32r_ops.to_has_stack = 1;
  m32r_ops.to_has_registers = 1;
  m32r_ops.to_has_execution = 1;
  m32r_ops.to_magic = OPS_MAGIC;
};


extern initialize_file_ftype _initialize_remote_m32r;

void
_initialize_remote_m32r (void)
{
  int i;

  init_m32r_ops ();

  /* Initialize breakpoints. */
  for (i = 0; i < MAX_BREAKPOINTS; i++)
    bp_address[i] = 0xffffffff;

  /* Initialize access breaks. */
  for (i = 0; i < MAX_ACCESS_BREAKS; i++)
    ab_address[i] = 0x00000000;

  add_target (&m32r_ops);

  add_com ("sdireset", class_obscure, sdireset_command,
	   "Reset SDI connection.");

  add_com ("sdistatus", class_obscure, sdistatus_command,
	   "Show status of SDI connection.");

  add_com ("debug_chaos", class_obscure, debug_chaos_command,
	   "Debug M32R/Chaos.");

  add_com ("use_debug_dma", class_obscure, use_debug_dma_command,
	   "Use debug DMA mem access.");
  add_com ("use_mon_code", class_obscure, use_mon_code_command,
	   "Use mon code mem access.");

  add_com ("use_ib_break", class_obscure, use_ib_breakpoints_command,
	   "Set breakpoints by IB break.");
  add_com ("use_dbt_break", class_obscure, use_dbt_breakpoints_command,
	   "Set breakpoints by dbt.");
}
