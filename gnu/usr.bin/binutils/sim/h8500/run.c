/* run front end support for H8/500
   Copyright (C) 1987, 1992 Free Software Foundation, Inc.

This file is part of H8/500 SIM

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

#include "config.h"

#include <varargs.h>
#include <stdio.h>
#include <signal.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#include "getopt.h"
#include "bfd.h"
#include "remote-sim.h"

void usage();
extern int optind;
extern char *optarg;

int
main (ac, av)
     int ac;
     char **av;
{
  bfd *abfd;
  bfd_vma start_address;
  asection *s;
  int i;
  int verbose = 0;
  int trace = 0;
  char *name = "";

  while ((i = getopt (ac, av, "c:tv")) != EOF)
    switch (i) 
      {
      case 'c':
	sim_csize (atoi (optarg));
	break;
      case 't':
	trace = 1;
	break;
      case 'v':
	verbose = 1;
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
  abfd = bfd_openr (name, "coff-h8500");
  if (abfd)
    {

      if (bfd_check_format (abfd, bfd_object))
	{

	  for (s = abfd->sections; s; s = s->next)
	    {
	      unsigned char *buffer = malloc (bfd_section_size (abfd, s));
	      bfd_get_section_contents (abfd,
					s,
					buffer,
					0,
					bfd_section_size (abfd, s));
	      sim_write (s->vma, buffer, bfd_section_size (abfd, s));
	    }

	  start_address = bfd_get_start_address (abfd);
	  sim_create_inferior (start_address, NULL, NULL);
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

	  {
	    enum sim_stop reason;
	    int sigrc;

	    sim_stop_reason (&reason, &sigrc);
	    if (sigrc == SIGQUIT)
	      return 0;
	    return sigrc;
	  }
	}
    }

  return 1;
}

void
printf_filtered (va_alist)
     va_dcl
{
  char *msg;
  va_list args;

  va_start (args);
  msg = va_arg (args, char *);
  vfprintf (stdout, msg, args);
  va_end (args);
}

void
usage()
{
  fprintf (stderr, "usage: run [-tv] program\n");
  exit (1);
}
