/* front end to the simulator.

   Written by Steve Chamberlain of Cygnus Support.
   sac@cygnus.com

   This file is part of H8/300 sim


		THIS SOFTWARE IS NOT COPYRIGHTED

   Cygnus offers the following for use in the public domain.  Cygnus
   makes no warranty with regard to the software or it's performance
   and the user accepts the software "AS IS" with all faults.

   CYGNUS DISCLAIMS ANY WARRANTIES, EXPRESS OR IMPLIED, WITH REGARD TO
   THIS SOFTWARE INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
   MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.

*/

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
  int sigrc;
  enum sim_stop reason;

  while ((i = getopt (ac, av, "c:htv")) != EOF)
    switch (i) 
      {
      case 'c':
	sim_csize (atoi (optarg));
	break;
      case 'h':
	set_h8300h (1);
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

  if (ac - optind != 1)
    usage();

  name = av[ac - 1];

  if (verbose)
    printf ("run %s\n", name);

  abfd = bfd_openr (name, "coff-h8300");
  if (! abfd)
    {
      fprintf (stderr, "%s: unable to open %s\n", av[0], name);
      exit (1);
    }

  if (! bfd_check_format(abfd, bfd_object)) 
    {
      fprintf (stderr, "%s: %s is not a valid executable\n", av[0], name);
      exit (1);
    }

  if (abfd->arch_info->mach == bfd_mach_h8300h
      || abfd->arch_info->mach == bfd_mach_h8300s)
    set_h8300h (1);

  for (s = abfd->sections; s; s=s->next) 
    {
      char *buffer;

      if (s->flags & SEC_LOAD)
	{

	  buffer = malloc(bfd_section_size(abfd,s));
	  bfd_get_section_contents(abfd, s, buffer, 0,
				   bfd_section_size (abfd, s));
	  sim_write(s->vma, buffer, bfd_section_size (abfd, s));
	}
    }

  start_address = bfd_get_start_address(abfd);
  sim_create_inferior (start_address, NULL, NULL);
  sim_resume(0,0);
  if (verbose)
    sim_info (verbose - 1);
  sim_stop_reason (&reason, &sigrc);
  /* FIXME: this test is insufficient but we can't do much
     about it until sim_stop_reason is cleaned up.  */
  if (sigrc == SIGILL)
    abort ();
  return 0;
}

/* gdb callback used by simulator */

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
  fprintf (stderr, "usage: run [-h] [-t] [-v] [-c csize] program\n");
  exit (1);
}
