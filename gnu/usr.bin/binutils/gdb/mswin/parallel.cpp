/* windows parallel port driver for GDB, the GNU debugger.
   Copyright 1996
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
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#include "stdafx.h"

/* This file was originally written to provides support for the
   parallel port input on the SH3 target system. 

   It seems very sensitive to what's going on at the host end of the
   parallel cable.  To make things simpler, remove all print drivers,
   redirection and whatnot on your lpt port, and make sure the cable
   is securly screwed onto your PC.

   There are two versions of the parallel write code here - one uses
   the WriteFile interface, which sadly, just doesn't work when
   connected to the SH3 Target System.  The SH3 monitor complains
   about parallel I/O errors when trying to move non-trivial sized
   files.  The other version uses the I/O instructions to talk to the
   port directly. Which won't work in Windows NT.

*/



unsigned int inp (int port)
{
  unsigned char y;
  _asm mov edx,port ;
  _asm in al,dx ;
  _asm mov y,al ;
  return y;
}

void outp (int port, unsigned char val)
{
  _asm mov edx,port;
  _asm mov al, val;
  _asm out dx,al;
}


#define BUSY 0x80
#define SELECT 0x08
#define INIT 0x04
#define STROBE 0x01
#define READY(x) (x & BUSY)

static int
parallel_open (serial_t scb, const char *name)
{
  if (strnicmp (name, "/dev/", 5) == 0)
    name += 5;
  else if (strnicmp (name, "\\dev\\", 5) == 0)
    name += 5;
  
  if (strlen (name) != 4 || strnicmp(name, "lpt", 3) != 0)
    {
      errno = ENOENT;
      return -1;
    }
  
  switch (name[3])
    {
    case  '0':
      scb->fd = 0x3bc;
      break;
    case  '1':
      scb->fd = 0x378;
      break;
    case '2':
      scb->fd = 0x278;
      break;
    default:
      errno = ENOENT;
      return -1;
    }
  return 0;
}

static void parallel_close(serial_t scb)
{
  /* Nothing to do. */
}


static void
delay()
{
  static int i;
  /* Provide a little delay for the data to settle around
     the strobes (this should be at least 500ns according to the spec).
     We can have quite a big delay here and not break performance
     because the target has quite a bit of work to do of it's own. */
  for (i = 0; i < 1000; i++)
    _asm nop;
}

static int
parallel_write(serial_t scb,
	       const char *str,
	       int len)
{
  /* Show a little status */
  char b[12];

  int ilen = sizeof(b)-1;
  if (len < ilen)
    ilen = len;
  memcpy (b, str, ilen);
  b[ilen] = 0;
  doing_something (b);

  while (len--)
    {
      int loops = 0;
      while (1)
	{
	  int status = inp (scb->fd + 1);
	  loops++;
	  if (loops > 100000)
	    return 0;
	  if (READY(status)) break;
	}

      /* Send data */
      outp (scb->fd, * str);

      delay();
      outp (scb->fd+2, SELECT | INIT | STROBE);
      delay();
      outp (scb->fd+2, SELECT | INIT);
      str++;
    }
  return 0;
}

#if 0
static int
parallel_write(serial_t scb,
	       const char *str,
	       int len)
{
  while (len > 0)
    {
      DWORD done;
      if (!WriteFile ((HANDLE)scb->fd, str, len, &done, 0))
	{
	  return 1;
	}
      /* Allow the target some time to catch up. */
      Sleep (len * 3);
      len -= done;
      str += done;
    }
  return 0;
}

static void
parallel_close (serial_t scb)
{
  if (scb->fd < 0)
    return ;

  CloseHandle ((HANDLE)scb->fd);
  scb->fd = -1;
}

static int
parallel_open (serial_t scb, const char *name)
{
  SECURITY_ATTRIBUTES sa;

  if (strnicmp (name, "/dev/", 5) == 0)
    name += 5;
  else if (strnicmp (name, "\\dev\\", 5) == 0)
    name += 5;
  
  if (strlen (name) != 4 || strnicmp(name, "lpt", 3) != 0)
    {
      errno = ENOENT;
      return -1;
    }
  
  if (name[3] < '1' || name[3] > '4')
    {
      errno = ENOENT;
      return -1;
    }
  
  sa.nLength = sizeof(sa);
  sa.lpSecurityDescriptor = 0;
  sa.bInheritHandle = 0;

  scb->fd = (int)(CreateFile (name,
			      GENERIC_WRITE,
			      0,
			      &sa,
			      CREATE_ALWAYS,
			      FILE_ATTRIBUTE_NORMAL,
			      0 ));

    if (scb->fd < 0)
      {
	errno = ENOENT;
	return -1;
      }  
  
  return 0;
}
#endif


static int
parallel_return_0 (serial_t scb)
{
  return 0;
}


static int
parallel_readchar(serial_t scb, int timeout)
{
  return 0;
}


/* parallel_{get set}_tty_state() are both dummys to fill out the function
   vector.  Someday, they may do something real... */

static serial_ttystate
parallel_get_tty_state (serial_t scb)
{
  return (serial_ttystate) 0;
}

static int
parallel_set_baud_rate (serial_t scb, int rate)
{
  return 0;
}

static int
parallel_set_tty_state (serial_t scb, serial_ttystate ttystate)
{
  return 0;
}

static int
parallel_noflush_set_tty_state (
     serial_t scb,
     serial_ttystate new_ttystate,
     serial_ttystate old_ttystate)
{
  return 0;
}

static void
parallel_print_tty_state (serial_t scb, serial_ttystate ttystate)
{
  /* Nothing to print.  */
  return;
}

static void
parallel_raw (serial_t scb)
{
  /* Always effectively in raw mode. */
}

static void
paralell_print_tty_state (serial_t scb, serial_ttystate ttystate)
{
  /* Nothing to print. */
  return;
}

static int
parallel_set_stop_bits (serial_t scb, int num)
{
  return 0;
}

static struct serial_ops parallel_ops =
{
  "parallel",
  0,
  parallel_open,
  parallel_close,
  parallel_readchar,
  parallel_write,
  parallel_return_0,
  parallel_return_0,
  parallel_return_0,
  parallel_raw,
  parallel_get_tty_state,
  parallel_set_tty_state,
  parallel_print_tty_state,
  parallel_noflush_set_tty_state,
  parallel_set_baud_rate,
  parallel_set_stop_bits,
};


extern "C" 
{
  void
    _initialize_parallel_win32 ()
    {
      serial_add_interface (&parallel_ops);
    }
};
