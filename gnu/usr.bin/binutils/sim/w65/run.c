/* run front end support for W65
   Copyright (C) 1995 Free Software Foundation, Inc.

This file is part of W65 SIM

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

#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#include "getopt.h"
#include "bfd.h"

#ifdef NEED_DECLARATION_PRINTF
extern int printf ();
#endif

void usage();
extern int optind;

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

  while ((i = getopt (ac, av, "tv")) != EOF)
    switch (i)
      {
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
  abfd = bfd_openr (name, "coff-w65");
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
	      free (buffer);
	    }

	  start_address = bfd_get_start_address (abfd);
	  sim_set_pc (start_address);
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
	    sim_info (printf, 0);

	  /* Find out what was in r0 and return that */
	  {
	    unsigned char b[4];
	    sim_fetch_register(0, b);
	    return b[3];
	  }
	  
	}
    }

  return 1;
}

void
usage()
{
  fprintf (stderr, "usage: run [-tv] program\n");
  exit (1);
}
