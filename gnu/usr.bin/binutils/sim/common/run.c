/* run front end support for all the simulators.
   Copyright (C) 1992, 1993 1994, 1995 Free Software Foundation, Inc.

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

#include <signal.h>
#include <stdio.h>
#include <varargs.h>
#include "bfd.h"
#include "remote-sim.h"
#include "callback.h"
#ifndef SIGQUIT
#define SIGQUIT SIGTERM
#endif

void usage();
extern int optind;
extern char *optarg;

bfd *exec_bfd;

int target_byte_order;

extern host_callback default_callback;
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
  enum sim_stop reason;
  int sigrc;

  while ((i = getopt (ac, av, "m:p:s:tv")) != EOF) 
    switch (i)
      {
      case 'm':
	sim_size (atoi (optarg));
	break;
      case 'p':
	sim_set_profile (atoi (optarg));
	break;
      case 's':
	sim_set_profile_size (atoi (optarg));
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

  exec_bfd = abfd = bfd_openr (name, 0);
  if (!abfd) 
    {
      fprintf (stderr, "run: can't open %s: %s\n", 
	      name, bfd_errmsg(bfd_get_error()));
      exit (1);
    }

  if (!bfd_check_format (abfd, bfd_object))
    {
      fprintf (stderr, "run: can't load %s: %s\n",
	       name, bfd_errmsg(bfd_get_error()));
      exit (1);
    }

  sim_set_callbacks (&default_callback);
  default_callback.init (&default_callback);

  /* Ensure that any run-time initialisation that needs to be
     performed by the simulator can occur. */
  sim_open(NULL);

  for (s = abfd->sections; s; s = s->next)
  if (abfd && (s->flags & SEC_LOAD))
    {
      unsigned char *buffer = (unsigned char *)malloc ((size_t)(bfd_section_size (abfd, s)));
      if (buffer != NULL)
        {
          bfd_get_section_contents (abfd,
                                    s,
                                    buffer,
                                    0,
                                    bfd_section_size (abfd, s));
          sim_write (s->vma, buffer, bfd_section_size (abfd, s));
        }
      else
        {
          fprintf (stderr, "run: failed to allocate section buffer: %s\n", 
                   bfd_errmsg(bfd_get_error()));
          exit (1);
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

  sim_stop_reason (&reason, &sigrc);

  sim_close(0);

  /* Why did we stop? */
  switch (reason)
    {
    case sim_signalled:
    case sim_stopped:
      if (sigrc != 0)
	fprintf (stderr, "program stopped with signal %d.\n", sigrc);
      break;

    case sim_exited:
      break;
    }

  /* If reason is sim_exited, then sigrc holds the exit code which we want
     to return.  If reason is sim_stopped or sim_signalled, then sigrc holds
     the signal that the simulator received; we want to return that to
     indicate failure.  */
  return sigrc;
}

void
usage()
{
  fprintf (stderr, "usage: run [-tv][-m size] program\n");
  exit (1);
}


