/* Copyright (C) 1991 Free Software Foundation, Inc.

This file is part of GLD, the Gnu Linker.

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

#include <stdio.h>
#include "bfd.h"
#include "sysdep.h"

#include "ld.h"
#include "ldver.h"
#include "ldemul.h"
#include "ldmain.h"

void
ldversion (noisy)
     int noisy;
{
  fprintf(stdout,"ld version 2.6 (with BFD %s)\n", BFD_VERSION);

  if (noisy) 
  {
    ld_emulation_xfer_type **ptr = ld_emulations;
    
    printf("  Supported emulations:\n");
    while (*ptr) 
    {
      printf("   %s \n", (*ptr)->emulation_name);
      ptr++;
    }
  }
}

void
help ()
{
  extern bfd_target *bfd_target_vector[];
  int t;

  printf ("\
Usage: %s [-o output] objfile...\n\
Options:\n\
       [-A architecture] [-b input-format]\n\
       [-Bstatic] [-Bdynamic] [-Bsymbolic] [-base-file file]\n\
       [-c MRI-commandfile] [-d | -dc | -dp]\n\
       [-defsym symbol=expression] [-dynamic-linker filename]\n",
	  program_name);
  puts ("\
       [-EB | -EL] [-e entry] [-embeddded-relocs] [-export-dynamic]\n\
       [-F] [-Fformat] [-format input-format] [-g] [-G size]\n\
       [-heap reserve[,commit]] [-help] [-i]\n\
       [-l archive] [-L searchdir] [-M] [-Map mapfile]\n\
       [-m emulation] [-N | -n] [-no-keep-memory] [-noinhibit-exec]\n\
       [-oformat output-format] [-R filename] [-relax]");
  puts ("\
       [-retain-symbols-file file] [-rpath path] [-shared] [-soname name]\n\
       [-r | -Ur] [-S] [-s] [-sort-common] [-stack reserve[,commit]]\n\
       [-split-by-reloc count] [-split-by-file]\n\
       [-stats] [-subsystem type] [-T commandfile]\n\
       [-Ttext textorg] [-Tdata dataorg] [-Tbss bssorg] [-t]");
  puts ("\
       [-traditional-format] [-u symbol] [-V] [-v] [-verbose]\n\
       [-version] [-warn-common] [-warn-constructors] [-warn-once]\n\
       [-whole-archive] [-X] [-x] [-y symbol]\n\
       [-( archives -)] [--start-group archives --end-group]");

  printf ("%s: supported targets:", program_name);
  for (t = 0; bfd_target_vector[t] != NULL; t++)
    printf (" %s", bfd_target_vector[t]->name);
  printf ("\n");

  printf ("%s: supported emulations: ", program_name);
  ldemul_list_emulations (stdout);
  printf ("\n");
}
