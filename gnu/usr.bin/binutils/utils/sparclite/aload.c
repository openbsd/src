/* Program to load an image into the SPARClite monitor board
   Copyright 1993, 1994, 1995 Free Software Foundation, Inc.

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
Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

/* Call with:

   aload PROG TTY

ie: aload hello /dev/ttya

*/

#include <stdio.h>

#include "ansidecl.h"

#ifdef ANSI_PROTOTYPES
#include <stdarg.h>
#else
#include <varargs.h>
#endif

#include "libiberty.h"
#include "bfd.h"

#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

#ifndef HAVE_TERMIOS
you lose
#endif

#if defined(HAVE_TERMIOS)
#include <termios.h>
#elif defined(HAVE_TERMIO)
#include <termio.h>
#elif defined(HAVE_SGTTY)
#include <sgtty.h>
#endif

#define min(A, B) (((A) < (B)) ? (A) : (B))

/* Where the code goes by default. */

#ifndef LOAD_ADDRESS
#define LOAD_ADDRESS 0x40000000
#endif

int quiet = 0;

static void
usage ()
{
  fprintf (stderr, "usage: aload [-q] file device\n");
  exit (1);
}

static void
#ifdef ANSI_PROTOTYPES
sys_error (char *msg, ...)
#else
sys_error (va_alist)
     va_dcl
#endif
{
  int e = errno;
  va_list args;

#ifdef ANSI_PROTOTYPES
  va_start (args, msg);
#else
  va_start (args);
#endif

#ifdef ANSI_PROTOTYPES
  vfprintf (stderr, msg, args);
#else
  {
    char *msg1;

    msg1 = va_arg (args, char *);
    vfprintf (stderr, msg1, args);
  }
#endif
  va_end (args);

  fprintf (stderr, ": %s\n", strerror(e));
  exit (1);
}

static void
#ifdef ANSI_PROTOTYPES
error (char *msg, ...)
#else
error (va_alist)
     va_dcl
#endif
{
  va_list args;
  
#ifdef ANSI_PROTOTYPES
  va_start (args, msg);
#else
  va_start (args);
#endif

#ifdef ANSI_PROTOTYPES
  vfprintf (stderr, msg, args);
#else
  {
    char *msg1;

    msg1 = va_arg (args, char *);
    vfprintf (stderr, msg1, args);
  }
#endif
  va_end (args);

  fputc ('\n', stderr);
  exit (1);
}

static int ttyfd;

static void
sendex (outtxt, outlen, intxt, inlen, id)
     unsigned char *outtxt;
     int outlen;
     unsigned char *intxt;
     int inlen;
     char *id;
{
  char buf[100];
  int cc;

  if (outlen > 0)
    {
      cc = write (ttyfd, outtxt, outlen);
      if (cc != outlen)
	sys_error ("Write %s failed", id);
    }

  if (inlen > 0)
    {
      cc = read (ttyfd, buf, inlen);	/* Get reply */
      if (cc != inlen)
	sys_error ("Read %s reply failed", id);
      if (bcmp (buf, intxt, inlen) != 0)
	error ("Bad reply to %s", id);
    }
}

extern int optind;

int
main (argc, argv)
     int argc;
     char **argv;
{
  struct termios termios;
  asection *section;
  bfd *pbfd;
  unsigned long entry;
  int c;

  while ((c = getopt (argc, argv, "q")) != EOF) 
    {
      switch (c) 
	{
	case 'q':
	  quiet = 1;
	  break;
	default:
	  usage();
	}
    }
  argc -= optind;
  argv += optind;

  if (argc != 2)
    usage();

  pbfd = bfd_openr (argv[0], 0);

  if (pbfd == NULL)
    sys_error ("Open of PROG failed");

/* setup the tty.  Must be raw, no flow control, 9600 baud */

  ttyfd = open (argv[1], O_RDWR);
  if (ttyfd == -1)
    sys_error ("Open of TTY failed");

  if (tcgetattr(ttyfd, &termios))
    sys_error ("tcgetattr failed");

  termios.c_iflag = 0;
  termios.c_oflag = 0;
  termios.c_cflag = CS8 | CREAD | CLOCAL;
  termios.c_lflag = 0;
  termios.c_cc[VMIN] = 1;
  termios.c_cc[VTIME] = 0;

  if (cfsetospeed (&termios, B9600)
      || cfsetispeed (&termios, B9600))
    sys_error ("cfset{i|o}speed failed");

  if (tcsetattr (ttyfd, TCSANOW, &termios))
    sys_error ("tcsetattr failed");

  /* The char is documented as 0xaa, \252 is portable octal form.   */
  sendex("", 1, "\252", 1, "alive?");
  sendex ("U", 1, "U", 1, "alive");
  if (!quiet)
    printf ("[SPARClite appears to be alive]\n");

  if (!bfd_check_format (pbfd, bfd_object)) 
    error ("It doesn't seem to be an object file");

  for (section = pbfd->sections; section; section = section->next) 
    {
      if (bfd_get_section_flags (pbfd, section) & SEC_ALLOC)
	{
	  bfd_vma section_address;
	  unsigned long section_size;
	  const char *section_name;

	  section_name = bfd_get_section_name (pbfd, section);

	  section_address = bfd_get_section_vma (pbfd, section);
	  /* Adjust sections from a.out files, since they don't
	     carry their addresses with.  */
	  if (bfd_get_flavour (pbfd) == bfd_target_aout_flavour)
	    section_address += LOAD_ADDRESS;
	  section_size = bfd_section_size (pbfd, section);

	  if (!quiet)
	    printf ("[Loading section %s at %lx (%ld bytes)]\n",
		    section_name, section_address, section_size);

	  /* Text, data or lit */
	  if (bfd_get_section_flags (pbfd, section) & SEC_LOAD)
	    {
	      file_ptr fptr;

	      fptr = 0;

	      while (section_size > 0)
		{
		  char buffer[1024];
		  int count, i;
		  unsigned char checksum;
		  static char inds[] = "|/-\\";
		  static int k = 0;

		  count = min (section_size, 1024);

		  bfd_get_section_contents (pbfd, section, buffer, fptr,
					    count);

		  checksum = 0;
		  for (i = 0; i < count; i++)
		    checksum += buffer[i];

		  if (!quiet) 
		    {
		      printf ("\r%c", inds[k++ % 4]);
		      fflush (stdout);
		    }

		  sendex ("\001", 1, "Z", 1, "load command");
		  sendex (&section_address, 4, NULL, 0, "load address");
		  sendex (&count, 4, NULL, 0, "program size");
		  sendex (buffer, count, &checksum, 1, "program");

		  section_address += count;
		  fptr += count;
		  section_size -= count;
		}
	    }
	  else			/* BSS */
	    {
	      if (!quiet)
		printf ("Not loading BSS \n");
	    }
	}
    }

  entry = bfd_get_start_address (pbfd);

  if (!quiet) 
    printf ("[Starting %s at 0x%lx]\n", argv[0], entry);

  sendex ("\003", 1, NULL, 0, "exec command");
  sendex (&entry, 4, "U", 1, "program start");

  exit (0);
}
