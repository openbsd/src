/* Reading symbol files from memory.

   Copyright 1986, 1987, 1989, 1991, 1994, 1995, 1996, 1998, 2000,
   2001, 2002, 2003, 2004 Free Software Foundation, Inc.

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
   Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

/* This file defines functions (and commands to exercise those
   functions) for reading debugging information from object files
   whose images are mapped directly into the inferior's memory.  For
   example, the Linux kernel maps a "syscall DSO" into each process's
   address space; this DSO provides kernel-specific code for some
   system calls.

   At the moment, BFD only has functions for parsing object files from
   memory for the ELF format, even though the general idea isn't
   ELF-specific.  This means that BFD only provides the functions GDB
   needs when configured for ELF-based targets.  So these functions
   may only be compiled on ELF-based targets.

   GDB has no idea whether it has been configured for an ELF-based
   target or not: it just tries to handle whatever files it is given.
   But this means there are no preprocessor symbols on which we could
   make these functions' compilation conditional.

   So, for the time being, we put these functions alone in this file,
   and have .mt files reference them as appropriate.  In the future, I
   hope BFD will provide a format-independent bfd_from_remote_memory
   entry point.  */


#include "defs.h"
#include "symtab.h"
#include "gdbcore.h"
#include "objfiles.h"
#include "gdbcmd.h"
#include "target.h"
#include "value.h"
#include "symfile.h"


/* Read inferior memory at ADDR to find the header of a loaded object file
   and read its in-core symbols out of inferior memory.  TEMPL is a bfd
   representing the target's format.  */
static struct objfile *
symbol_file_add_from_memory (struct bfd *templ, CORE_ADDR addr, int from_tty)
{
  struct objfile *objf;
  struct bfd *nbfd;
  struct bfd_section *sec;
  bfd_vma loadbase;
  struct section_addr_info *sai;
  unsigned int i;

  if (bfd_get_flavour (templ) != bfd_target_elf_flavour)
    error ("add-symbol-file-from-memory not supported for this target");

  nbfd = bfd_elf_bfd_from_remote_memory (templ, addr, &loadbase,
					 target_read_memory);
  if (nbfd == NULL)
    error ("Failed to read a valid object file image from memory.");

  nbfd->filename = xstrdup ("shared object read from target memory");

  if (!bfd_check_format (nbfd, bfd_object))
    {
      /* FIXME: should be checking for errors from bfd_close (for one thing,
         on error it does not free all the storage associated with the
         bfd).  */
      bfd_close (nbfd);
      error ("Got object file from memory but can't read symbols: %s.",
	     bfd_errmsg (bfd_get_error ()));
    }

  sai = alloc_section_addr_info (bfd_count_sections (nbfd));
  make_cleanup (xfree, sai);
  i = 0;
  for (sec = nbfd->sections; sec != NULL; sec = sec->next)
    if ((bfd_get_section_flags (nbfd, sec) & (SEC_ALLOC|SEC_LOAD)) != 0)
      {
	sai->other[i].addr = bfd_get_section_vma (nbfd, sec) + loadbase;
	sai->other[i].name = (char *) bfd_get_section_name (nbfd, sec);
	sai->other[i].sectindex = sec->index;
	++i;
      }

  objf = symbol_file_add_from_bfd (nbfd, from_tty,
                                   sai, 2, OBJF_SHARED);

  /* This might change our ideas about frames already looked at.  */
  reinit_frame_cache ();

  return objf;
}


static void
add_symbol_file_from_memory_command (char *args, int from_tty)
{
  CORE_ADDR addr;
  struct bfd *templ;

  if (args == NULL)
    error ("add-symbol-file-from-memory requires an expression argument");

  addr = parse_and_eval_address (args);

  /* We need some representative bfd to know the target we are looking at.  */
  if (symfile_objfile != NULL)
    templ = symfile_objfile->obfd;
  else
    templ = exec_bfd;
  if (templ == NULL)
    error ("\
Must use symbol-file or exec-file before add-symbol-file-from-memory.");

  symbol_file_add_from_memory (templ, addr, from_tty);
}


void
_initialize_symfile_mem (void)
{
  add_cmd ("add-symbol-file-from-memory", class_files,
           add_symbol_file_from_memory_command,
           "\
Load the symbols out of memory from a dynamically loaded object file.\n\
Give an expression for the address of the file's shared object file header.",
           &cmdlist);

}
