/* run front end support for Z8KSIM
   Copyright (C) 1987, 1992 Free Software Foundation, Inc.

This file is part of Z8KSIM

Z8KSIM is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

Z8KSIM is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Z8KZIM; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#include <stdio.h>
#include "bfd.h"
#include "tm.h"

void usage();
extern int optind;

main(ac,av)
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
    printf("run %s\n", name);
  }
  abfd = bfd_openr(name,"coff-z8k");

  if (abfd) {
      
    if (bfd_check_format(abfd, bfd_object)) 
    {
      if (abfd->arch_info->mach == bfd_mach_z8001)
      {
	extern int sim_z8001_mode;
	sim_z8001_mode = 1;
      }
      for (s = abfd->sections; s; s=s->next) 
      {
	char *buffer = malloc(bfd_section_size(abfd,s));
	bfd_get_section_contents(abfd, s, buffer, 0, bfd_section_size(abfd,s));
	sim_write(s->vma, buffer, bfd_section_size(abfd,s));
      }

      start_address = bfd_get_start_address(abfd);
      sim_create_inferior (start_address, NULL, NULL);
      if (trace) 
      {
	int done = 0;
	while (!done) 
	{
	  done = sim_trace();
	}
      }
      else 
      { 
	sim_resume(0,0);
      }
      if (verbose) 
      {
	sim_info(0);
      }
      {
	char b[4];
     sim_fetch_register (2,b);
	return b[1];
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
