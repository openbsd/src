# This shell script emits a C file. -*- C -*-
# It does some substitutions.
cat >e${EMULATION_NAME}.c <<EOF
/* An emulation for HP PA-RISC ELF linkers.
   Copyright (C) 1991, 93, 94, 95, 97, 1999 Free Software Foundation, Inc.
   Written by Steve Chamberlain steve@cygnus.com

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

#include "bfd.h"
#include "sysdep.h"
#include "bfdlink.h"

#include "ld.h"
#include "ldemul.h"
#include "ldfile.h"
#include "ldexp.h"
#include "ldlang.h"
#include "ldmisc.h"
#include "ldmain.h"
#include "ldctor.h"

/* Section in which we build stubs.  */
static asection *stub_sec;
static lang_input_statement_type *stub_file;


/* FIXME.  This doesn't belong here.  */
extern lang_statement_list_type file_chain;

/* Perform some emulation specific initialization.  For PA ELF we set
   up the local label prefix and the output architecture.  */

static void
hppaelf_before_parse ()
{
  ldfile_output_architecture = bfd_arch_hppa;
}

/* Set the output architecture and machine.  */

static void
hppaelf_set_output_arch()
{
  unsigned long machine = 0;

  bfd_set_arch_mach (output_bfd, ldfile_output_architecture, machine);
}

/* This is called before the input files are opened.  We create a new
   fake input file to hold the stub section.  */

static void
hppaelf_create_output_section_statements ()
{
  stub_file = lang_add_input_file ("linker stubs",
				   lang_input_file_is_fake_enum,
				   NULL);
  stub_file->the_bfd = bfd_create ("linker stubs", output_bfd);
  if (stub_file->the_bfd == NULL
      || ! bfd_set_arch_mach (stub_file->the_bfd,
			      bfd_get_arch (output_bfd),
			      bfd_get_mach (output_bfd)))
    {
      einfo ("%X%P: can not create BFD %E\n");
      return;
    }

  stub_sec = bfd_make_section_old_way (stub_file->the_bfd, ".text");
  /* Don't set SEC_RELOC until we actually have relocations in this
     section.  */
  if (stub_sec == NULL
      || ! bfd_set_section_flags (stub_file->the_bfd, stub_sec,
				  (SEC_HAS_CONTENTS
				   | SEC_ALLOC
				   | SEC_LOAD
				   | SEC_CODE
				   | SEC_IN_MEMORY)))
    {
      einfo ("%X%P: can not create stub section: %E\n");
      return;
    }

  ldlang_add_file (stub_file);
}

/* Walk all the lang statements splicing out any padding statements from 
   the list.  */

static void
hppaelf_delete_padding_statements (s, prev)
     lang_statement_union_type *s;
     lang_statement_union_type **prev;
{
  lang_statement_union_type *sprev = NULL;
  for (; s != NULL; s = s->next)
    {
      switch (s->header.type)
	{

	/* We want recursively walk these sections.  */
	case lang_constructors_statement_enum:
	  hppaelf_delete_padding_statements (constructor_list.head,
					     &constructor_list.head);
	  break;

	case lang_output_section_statement_enum:
	  hppaelf_delete_padding_statements (s->output_section_statement.
					       children.head,
					     &s->output_section_statement.
					       children.head);
	  break;

	/* Huh?  What is a lang_wild_statement?  */
	case lang_wild_statement_enum:
	  hppaelf_delete_padding_statements (s->wild_statement.
					       children.head,
					     &s->wild_statement.
					       children.head);
	  break;

	/* Here's what we are really looking for.  Splice these out of
	   the list.  */
	case lang_padding_statement_enum:
	  if (sprev)
	    sprev->header.next = s->header.next;
	  else
	    **prev = *s;
	  break;

	/* We don't care about these cases.  */
	case lang_data_statement_enum:
	case lang_object_symbols_statement_enum:
	case lang_output_statement_enum:
	case lang_target_statement_enum:
	case lang_input_section_enum:
	case lang_input_statement_enum:
	case lang_assignment_statement_enum:
	case lang_address_statement_enum:
	  break;

	default:
	  abort ();
	  break;
	}
      sprev = s;
    }
}

/* Final emulation specific call.  For the PA we use this opportunity
   to build linker stubs.  */

static void
hppaelf_finish ()
{
  /* Call into the BFD backend to do the real work.  */
  if (elf32_hppa_size_stubs (stub_file->the_bfd, output_bfd, &link_info)
      == false)
    {
      einfo ("%X%P: can not size stub section: %E\n");
      return;
    }
  
  /* If the size of the stub section is nonzero, then we need
     to resize the sections, recompute the assignments, and finally
     build the stubs.  */
  if (bfd_section_size (stub_file->the_bfd, stub_file->the_bfd->sections) != 0)
    {
      /* Delete all the padding statements, they're no longer valid.  */
      hppaelf_delete_padding_statements (stat_ptr->head, &stat_ptr->head);
      
      /* Resize the sections.  */
      lang_size_sections (stat_ptr->head, abs_output_section,
			  &stat_ptr->head, 0, (bfd_vma) 0, false);
      
      /* Redo special stuff.  */
      ldemul_after_allocation ();
      
      /* Do the assignments again.  */
      lang_do_assignments (stat_ptr->head,
			   abs_output_section,
			   (fill_type) 0, (bfd_vma) 0);
      
      /* Now build the linker stubs.  */
      if (elf32_hppa_build_stubs (stub_file->the_bfd, &link_info) == false)
	{
	  einfo ("%X%P: can not build stubs: %E\n");
	  return;
	}
    }
}

/* The script itself gets inserted here.  */

static char *
hppaelf_get_script(isfile)
     int *isfile;
EOF

if test -n "$COMPILE_IN"
then
# Scripts compiled in.

# sed commands to quote an ld script as a C string.
sc="-f stringify.sed"

cat >>e${EMULATION_NAME}.c <<EOF
{			     
  *isfile = 0;

  if (link_info.relocateable == true && config.build_constructors == true)
    return
EOF
sed $sc ldscripts/${EMULATION_NAME}.xu                     >> e${EMULATION_NAME}.c
echo '  ; else if (link_info.relocateable == true) return' >> e${EMULATION_NAME}.c
sed $sc ldscripts/${EMULATION_NAME}.xr                     >> e${EMULATION_NAME}.c
echo '  ; else if (!config.text_read_only) return'         >> e${EMULATION_NAME}.c
sed $sc ldscripts/${EMULATION_NAME}.xbn                    >> e${EMULATION_NAME}.c
echo '  ; else if (!config.magic_demand_paged) return'     >> e${EMULATION_NAME}.c
sed $sc ldscripts/${EMULATION_NAME}.xn                     >> e${EMULATION_NAME}.c
echo '  ; else return'                                     >> e${EMULATION_NAME}.c
sed $sc ldscripts/${EMULATION_NAME}.x                      >> e${EMULATION_NAME}.c
echo '; }'                                                 >> e${EMULATION_NAME}.c

else
# Scripts read from the filesystem.

cat >>e${EMULATION_NAME}.c <<EOF
{			     
  *isfile = 1;

  if (link_info.relocateable == true && config.build_constructors == true)
    return "ldscripts/${EMULATION_NAME}.xu";
  else if (link_info.relocateable == true)
    return "ldscripts/${EMULATION_NAME}.xr";
  else if (!config.text_read_only)
    return "ldscripts/${EMULATION_NAME}.xbn";
  else if (!config.magic_demand_paged)
    return "ldscripts/${EMULATION_NAME}.xn";
  else
    return "ldscripts/${EMULATION_NAME}.x";
}
EOF

fi

cat >>e${EMULATION_NAME}.c <<EOF

struct ld_emulation_xfer_struct ld_hppaelf_emulation = 
{
  hppaelf_before_parse,
  syslib_default,
  hll_default,
  after_parse_default,
  after_open_default,
  after_allocation_default,
  hppaelf_set_output_arch,
  ldemul_default_target,
  before_allocation_default,
  hppaelf_get_script,
  "hppaelf",
  "elf32-hppa",
  hppaelf_finish,
  hppaelf_create_output_section_statements,
  NULL,	/* open dynamic archive */
  NULL,	/* place orphan */
  NULL,	/* set symbols */
  NULL,	/* parse args */
  NULL,	/* unrecognized file */
  NULL,	/* list options */
  NULL,	/* recognized file */
  NULL 	/* find_potential_libraries */
};
EOF
