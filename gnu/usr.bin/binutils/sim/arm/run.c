/* run front end support for ARM
   Copyright (C) 1996 Free Software Foundation, Inc.

This file is part of ARM SIM

GNU CC is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GNU CC is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

/* Steve Chamberlain
   sac@cygnus.com */

#include <stdio.h>
#include <varargs.h>
#include "bfd.h"
#include "getopt.h"
#include "remote-sim.h"

static void usage();

int target_byte_order;

int
main (ac, av)
     int ac;
     char **av;
{
  bfd *abfd;
  bfd_vma start_address;
  asection *s;
  int i;
  int trace = 0;
  int verbose = 0;
  char *name;

  while ((i = getopt (ac, av, "m:p:s:tv")) != EOF) 
    switch (i)
      {
      case 'm':
	arm_sim_set_mem_size (atoi (optarg));
	break;
      case 'p': /* FIXME: unused */
	arm_sim_set_profile (atoi (optarg));
	break;
      case 's': /* FIXME: unused */
	arm_sim_set_profile_size (atoi (optarg));
	break;
      case 't':
	trace = 1;
	break;
      case 'v':
	verbose = 1;
	arm_sim_set_verbosity (1);
	break;
      default:
	usage();
      }
  ac -= optind;
  av += optind;

  if (ac != 1)
    usage();

  name = *av;

  if (verbose)
    {
      printf ("run %s\n", name);
    }

  abfd = bfd_openr (name, 0);
  if (abfd)
    {
      if (bfd_check_format (abfd, bfd_object))
	{
	  for (s = abfd->sections; s; s = s->next)
	    {
	      if (s->flags & SEC_LOAD)
		{
		  unsigned char *buffer = malloc (bfd_section_size (abfd, s));
		  bfd_get_section_contents (abfd,
					    s,
					    buffer,
					    0,
					    bfd_section_size (abfd, s));
		  sim_write (s->vma, buffer, bfd_section_size (abfd, s));
		  free (buffer);
		}
	    }

	  start_address = bfd_get_start_address (abfd);
	  sim_create_inferior (start_address, NULL, NULL);

	  target_byte_order = bfd_big_endian (abfd) ? 4321 : 1234;

	  if (trace)
	    {
	      int done = 0;
	      while (!done)
		{
		  done = sim_trace ();
		}
	    }
	  else
	    {
	      sim_resume (0, 0);
	    }
	  if (verbose)
	    sim_info (0);

	  /* Assume we left through the exit system call,
	     in which case r0 has the exit code */
	  /* FIXME: byte order dependent? */
	  {
	    unsigned char b[4];
	    sim_fetch_register (0, b);
	    return b[0];
	  }
	}
    }

  return 1;
}

static void
usage()
{
  fprintf (stderr, "usage: run [-tv] program\n");
  exit (1);
}


/* Callbacks used by the simulator proper.  */

void
printf_filtered (va_alist)
     va_dcl
{
  va_list args;
  char *format;

  va_start (args);
  format = va_arg (args, char *);

  vfprintf (stdout, format, args);
  va_end (args);
}
