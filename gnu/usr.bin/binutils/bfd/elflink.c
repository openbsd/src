/* ELF linking support for BFD.
   Copyright 1995 Free Software Foundation, Inc.

This file is part of BFD, the Binary File Descriptor library.

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
#include "libbfd.h"
#define ARCH_SIZE 0
#include "elf-bfd.h"

boolean
_bfd_elf_create_got_section (abfd, info)
     bfd *abfd;
     struct bfd_link_info *info;
{
  flagword flags;
  register asection *s;
  struct elf_link_hash_entry *h;
  struct elf_backend_data *bed = get_elf_backend_data (abfd);

  /* This function may be called more than once.  */
  if (bfd_get_section_by_name (abfd, ".got") != NULL)
    return true;

  flags = SEC_ALLOC | SEC_LOAD | SEC_HAS_CONTENTS | SEC_IN_MEMORY;

  s = bfd_make_section (abfd, ".got");
  if (s == NULL
      || !bfd_set_section_flags (abfd, s, flags)
      || !bfd_set_section_alignment (abfd, s, 2))
    return false;

  if (bed->want_got_plt)
    {
      s = bfd_make_section (abfd, ".got.plt");
      if (s == NULL
	  || !bfd_set_section_flags (abfd, s, flags)
	  || !bfd_set_section_alignment (abfd, s, 2))
	return false;
    }

  /* Define the symbol _GLOBAL_OFFSET_TABLE_ at the start of the .got
     (or .got.plt) section.  We don't do this in the linker script
     because we don't want to define the symbol if we are not creating
     a global offset table.  */
  h = NULL;
  if (!(_bfd_generic_link_add_one_symbol
	(info, abfd, "_GLOBAL_OFFSET_TABLE_", BSF_GLOBAL, s, (bfd_vma) 0,
	 (const char *) NULL, false, get_elf_backend_data (abfd)->collect,
	 (struct bfd_link_hash_entry **) &h)))
    return false;
  h->elf_link_hash_flags |= ELF_LINK_HASH_DEF_REGULAR;
  h->type = STT_OBJECT;

  if (info->shared
      && ! _bfd_elf_link_record_dynamic_symbol (info, h))
    return false;

  /* The first three global offset table entries are reserved.  */
  s->_raw_size += 3 * 4;

  return true;
}

/* Create dynamic sections when linking against a dynamic object.  */

boolean
_bfd_elf_create_dynamic_sections (abfd, info)
     bfd *abfd;
     struct bfd_link_info *info;
{
  flagword flags;
  register asection *s;
  struct elf_backend_data *bed = get_elf_backend_data (abfd);

  /* We need to create .plt, .rel[a].plt, .got, .got.plt, .dynbss, and
     .rel[a].bss sections.  */

  flags = SEC_ALLOC | SEC_LOAD | SEC_HAS_CONTENTS | SEC_IN_MEMORY;

  s = bfd_make_section (abfd, ".plt");
  if (s == NULL
      || ! bfd_set_section_flags (abfd, s,
				  (flags | SEC_CODE
				   | (bed->plt_readonly ? SEC_READONLY : 0)))
      || ! bfd_set_section_alignment (abfd, s, 2))
    return false;

  if (bed->want_plt_sym)
    {
      /* Define the symbol _PROCEDURE_LINKAGE_TABLE_ at the start of the
	 .plt section.  */
      struct elf_link_hash_entry *h = NULL;
      if (! (_bfd_generic_link_add_one_symbol
	     (info, abfd, "_PROCEDURE_LINKAGE_TABLE_", BSF_GLOBAL, s,
	      (bfd_vma) 0, (const char *) NULL, false,
	      get_elf_backend_data (abfd)->collect,
	      (struct bfd_link_hash_entry **) &h)))
	return false;
      h->elf_link_hash_flags |= ELF_LINK_HASH_DEF_REGULAR;
      h->type = STT_OBJECT;

      if (info->shared
	  && ! _bfd_elf_link_record_dynamic_symbol (info, h))
	return false;
    }

  s = bfd_make_section (abfd, bed->use_rela_p ? ".rela.plt" : ".rel.plt");
  if (s == NULL
      || ! bfd_set_section_flags (abfd, s, flags | SEC_READONLY)
      || ! bfd_set_section_alignment (abfd, s, 2))
    return false;

  if (! _bfd_elf_create_got_section (abfd, info))
    return false;

  /* The .dynbss section is a place to put symbols which are defined
     by dynamic objects, are referenced by regular objects, and are
     not functions.  We must allocate space for them in the process
     image and use a R_*_COPY reloc to tell the dynamic linker to
     initialize them at run time.  The linker script puts the .dynbss
     section into the .bss section of the final image.  */
  s = bfd_make_section (abfd, ".dynbss");
  if (s == NULL
      || ! bfd_set_section_flags (abfd, s, SEC_ALLOC))
    return false;

  /* The .rel[a].bss section holds copy relocs.  This section is not
     normally needed.  We need to create it here, though, so that the
     linker will map it to an output section.  We can't just create it
     only if we need it, because we will not know whether we need it
     until we have seen all the input files, and the first time the
     main linker code calls BFD after examining all the input files
     (size_dynamic_sections) the input sections have already been
     mapped to the output sections.  If the section turns out not to
     be needed, we can discard it later.  We will never need this
     section when generating a shared object, since they do not use
     copy relocs.  */
  if (! info->shared)
    {
      s = bfd_make_section (abfd, bed->use_rela_p ? ".rela.bss" : ".rel.bss");
      if (s == NULL
	  || ! bfd_set_section_flags (abfd, s, flags | SEC_READONLY)
	  || ! bfd_set_section_alignment (abfd, s, 2))
	return false;
    }

  return true;
}

/* Record a new dynamic symbol.  We record the dynamic symbols as we
   read the input files, since we need to have a list of all of them
   before we can determine the final sizes of the output sections.
   Note that we may actually call this function even though we are not
   going to output any dynamic symbols; in some cases we know that a
   symbol should be in the dynamic symbol table, but only if there is
   one.  */

boolean
_bfd_elf_link_record_dynamic_symbol (info, h)
     struct bfd_link_info *info;
     struct elf_link_hash_entry *h;
{
  if (h->dynindx == -1)
    {
      struct bfd_strtab_hash *dynstr;

      h->dynindx = elf_hash_table (info)->dynsymcount;
      ++elf_hash_table (info)->dynsymcount;

      dynstr = elf_hash_table (info)->dynstr;
      if (dynstr == NULL)
	{
	  /* Create a strtab to hold the dynamic symbol names.  */
	  elf_hash_table (info)->dynstr = dynstr = _bfd_elf_stringtab_init ();
	  if (dynstr == NULL)
	    return false;
	}

      h->dynstr_index = ((unsigned long)
			 _bfd_stringtab_add (dynstr, h->root.root.string,
					     true, false));
      if (h->dynstr_index == (unsigned long) -1)
	return false;
    }

  return true;
}
