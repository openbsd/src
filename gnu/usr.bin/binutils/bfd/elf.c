/* ELF executable support for BFD.
   Copyright 1993 Free Software Foundation, Inc.

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

/*

SECTION
	ELF backends

	BFD support for ELF formats is being worked on.
	Currently, the best supported back ends are for sparc and i386
	(running svr4 or Solaris 2).

	Documentation of the internals of the support code still needs
	to be written.  The code is changing quickly enough that we
	haven't bothered yet.
 */

#include "bfd.h"
#include "sysdep.h"
#include "bfdlink.h"
#include "libbfd.h"
#define ARCH_SIZE 0
#include "elf-bfd.h"

static file_ptr map_program_segments PARAMS ((bfd *, file_ptr,
					      Elf_Internal_Shdr *,
					      Elf_Internal_Shdr **,
					      bfd_size_type));
static boolean assign_file_positions_except_relocs PARAMS ((bfd *));
static boolean prep_headers PARAMS ((bfd *));
static boolean swap_out_syms PARAMS ((bfd *, struct bfd_strtab_hash **));

/* Standard ELF hash function.  Do not change this function; you will
   cause invalid hash tables to be generated.  (Well, you would if this
   were being used yet.)  */
unsigned long
bfd_elf_hash (name)
     CONST unsigned char *name;
{
  unsigned long h = 0;
  unsigned long g;
  int ch;

  while ((ch = *name++) != '\0')
    {
      h = (h << 4) + ch;
      if ((g = (h & 0xf0000000)) != 0)
	{
	  h ^= g >> 24;
	  h &= ~g;
	}
    }
  return h;
}

/* Read a specified number of bytes at a specified offset in an ELF
   file, into a newly allocated buffer, and return a pointer to the
   buffer. */

static char *
elf_read (abfd, offset, size)
     bfd * abfd;
     long offset;
     unsigned int size;
{
  char *buf;

  if ((buf = bfd_alloc (abfd, size)) == NULL)
    {
      bfd_set_error (bfd_error_no_memory);
      return NULL;
    }
  if (bfd_seek (abfd, offset, SEEK_SET) == -1)
    return NULL;
  if (bfd_read ((PTR) buf, size, 1, abfd) != size)
    {
      if (bfd_get_error () != bfd_error_system_call)
	bfd_set_error (bfd_error_file_truncated);
      return NULL;
    }
  return buf;
}

boolean
elf_mkobject (abfd)
     bfd * abfd;
{
  /* this just does initialization */
  /* coff_mkobject zalloc's space for tdata.coff_obj_data ... */
  elf_tdata (abfd) = (struct elf_obj_tdata *)
    bfd_zalloc (abfd, sizeof (struct elf_obj_tdata));
  if (elf_tdata (abfd) == 0)
    {
      bfd_set_error (bfd_error_no_memory);
      return false;
    }
  /* since everything is done at close time, do we need any
     initialization? */

  return true;
}

char *
bfd_elf_get_str_section (abfd, shindex)
     bfd * abfd;
     unsigned int shindex;
{
  Elf_Internal_Shdr **i_shdrp;
  char *shstrtab = NULL;
  unsigned int offset;
  unsigned int shstrtabsize;

  i_shdrp = elf_elfsections (abfd);
  if (i_shdrp == 0 || i_shdrp[shindex] == 0)
    return 0;

  shstrtab = (char *) i_shdrp[shindex]->contents;
  if (shstrtab == NULL)
    {
      /* No cached one, attempt to read, and cache what we read. */
      offset = i_shdrp[shindex]->sh_offset;
      shstrtabsize = i_shdrp[shindex]->sh_size;
      shstrtab = elf_read (abfd, offset, shstrtabsize);
      i_shdrp[shindex]->contents = (PTR) shstrtab;
    }
  return shstrtab;
}

char *
bfd_elf_string_from_elf_section (abfd, shindex, strindex)
     bfd * abfd;
     unsigned int shindex;
     unsigned int strindex;
{
  Elf_Internal_Shdr *hdr;

  if (strindex == 0)
    return "";

  hdr = elf_elfsections (abfd)[shindex];

  if (hdr->contents == NULL
      && bfd_elf_get_str_section (abfd, shindex) == NULL)
    return NULL;

  return ((char *) hdr->contents) + strindex;
}

/* Make a BFD section from an ELF section.  We store a pointer to the
   BFD section in the bfd_section field of the header.  */

boolean
_bfd_elf_make_section_from_shdr (abfd, hdr, name)
     bfd *abfd;
     Elf_Internal_Shdr *hdr;
     const char *name;
{
  asection *newsect;
  flagword flags;

  if (hdr->bfd_section != NULL)
    {
      BFD_ASSERT (strcmp (name,
			  bfd_get_section_name (abfd, hdr->bfd_section)) == 0);
      return true;
    }

  newsect = bfd_make_section_anyway (abfd, name);
  if (newsect == NULL)
    return false;

  newsect->filepos = hdr->sh_offset;

  if (! bfd_set_section_vma (abfd, newsect, hdr->sh_addr)
      || ! bfd_set_section_size (abfd, newsect, hdr->sh_size)
      || ! bfd_set_section_alignment (abfd, newsect,
				      bfd_log2 (hdr->sh_addralign)))
    return false;

  flags = SEC_NO_FLAGS;
  if (hdr->sh_type != SHT_NOBITS)
    flags |= SEC_HAS_CONTENTS;
  if ((hdr->sh_flags & SHF_ALLOC) != 0)
    {
      flags |= SEC_ALLOC;
      if (hdr->sh_type != SHT_NOBITS)
	flags |= SEC_LOAD;
    }
  if ((hdr->sh_flags & SHF_WRITE) == 0)
    flags |= SEC_READONLY;
  if ((hdr->sh_flags & SHF_EXECINSTR) != 0)
    flags |= SEC_CODE;
  else if ((flags & SEC_LOAD) != 0)
    flags |= SEC_DATA;

  /* The debugging sections appear to be recognized only by name, not
     any sort of flag.  */
  if (strncmp (name, ".debug", sizeof ".debug" - 1) == 0
      || strncmp (name, ".line", sizeof ".line" - 1) == 0
      || strncmp (name, ".stab", sizeof ".stab" - 1) == 0)
    flags |= SEC_DEBUGGING;

  if (! bfd_set_section_flags (abfd, newsect, flags))
    return false;

  if ((flags & SEC_ALLOC) != 0)
    {
      Elf_Internal_Phdr *phdr;
      unsigned int i;

      /* Look through the phdrs to see if we need to adjust the lma.  */
      phdr = elf_tdata (abfd)->phdr;
      for (i = 0; i < elf_elfheader (abfd)->e_phnum; i++, phdr++)
	{
	  if (phdr->p_type == PT_LOAD
	      && phdr->p_vaddr != phdr->p_paddr
	      && phdr->p_vaddr <= hdr->sh_addr
	      && phdr->p_vaddr + phdr->p_memsz >= hdr->sh_addr + hdr->sh_size)
	    {
	      newsect->lma += phdr->p_paddr - phdr->p_vaddr;
	      break;
	    }
	}
    }

  hdr->bfd_section = newsect;
  elf_section_data (newsect)->this_hdr = *hdr;

  return true;
}

/*
INTERNAL_FUNCTION
	bfd_elf_find_section

SYNOPSIS
	struct elf_internal_shdr *bfd_elf_find_section (bfd *abfd, char *name);

DESCRIPTION
	Helper functions for GDB to locate the string tables.
	Since BFD hides string tables from callers, GDB needs to use an
	internal hook to find them.  Sun's .stabstr, in particular,
	isn't even pointed to by the .stab section, so ordinary
	mechanisms wouldn't work to find it, even if we had some.
*/

struct elf_internal_shdr *
bfd_elf_find_section (abfd, name)
     bfd * abfd;
     char *name;
{
  Elf_Internal_Shdr **i_shdrp;
  char *shstrtab;
  unsigned int max;
  unsigned int i;

  i_shdrp = elf_elfsections (abfd);
  if (i_shdrp != NULL)
    {
      shstrtab = bfd_elf_get_str_section (abfd, elf_elfheader (abfd)->e_shstrndx);
      if (shstrtab != NULL)
	{
	  max = elf_elfheader (abfd)->e_shnum;
	  for (i = 1; i < max; i++)
	    if (!strcmp (&shstrtab[i_shdrp[i]->sh_name], name))
	      return i_shdrp[i];
	}
    }
  return 0;
}

const char *const bfd_elf_section_type_names[] = {
  "SHT_NULL", "SHT_PROGBITS", "SHT_SYMTAB", "SHT_STRTAB",
  "SHT_RELA", "SHT_HASH", "SHT_DYNAMIC", "SHT_NOTE",
  "SHT_NOBITS", "SHT_REL", "SHT_SHLIB", "SHT_DYNSYM",
};

/* ELF relocs are against symbols.  If we are producing relocateable
   output, and the reloc is against an external symbol, and nothing
   has given us any additional addend, the resulting reloc will also
   be against the same symbol.  In such a case, we don't want to
   change anything about the way the reloc is handled, since it will
   all be done at final link time.  Rather than put special case code
   into bfd_perform_relocation, all the reloc types use this howto
   function.  It just short circuits the reloc if producing
   relocateable output against an external symbol.  */

/*ARGSUSED*/
bfd_reloc_status_type
bfd_elf_generic_reloc (abfd,
		       reloc_entry,
		       symbol,
		       data,
		       input_section,
		       output_bfd,
		       error_message)
     bfd *abfd;
     arelent *reloc_entry;
     asymbol *symbol;
     PTR data;
     asection *input_section;
     bfd *output_bfd;
     char **error_message;
{
  if (output_bfd != (bfd *) NULL
      && (symbol->flags & BSF_SECTION_SYM) == 0
      && (! reloc_entry->howto->partial_inplace
	  || reloc_entry->addend == 0))
    {
      reloc_entry->address += input_section->output_offset;
      return bfd_reloc_ok;
    }

  return bfd_reloc_continue;
}

/* Display ELF-specific fields of a symbol.  */
void
bfd_elf_print_symbol (ignore_abfd, filep, symbol, how)
     bfd *ignore_abfd;
     PTR filep;
     asymbol *symbol;
     bfd_print_symbol_type how;
{
  FILE *file = (FILE *) filep;
  switch (how)
    {
    case bfd_print_symbol_name:
      fprintf (file, "%s", symbol->name);
      break;
    case bfd_print_symbol_more:
      fprintf (file, "elf ");
      fprintf_vma (file, symbol->value);
      fprintf (file, " %lx", (long) symbol->flags);
      break;
    case bfd_print_symbol_all:
      {
	CONST char *section_name;
	section_name = symbol->section ? symbol->section->name : "(*none*)";
	bfd_print_symbol_vandf ((PTR) file, symbol);
	fprintf (file, " %s\t", section_name);
	/* Print the "other" value for a symbol.  For common symbols,
	   we've already printed the size; now print the alignment.
	   For other symbols, we have no specified alignment, and
	   we've printed the address; now print the size.  */
	fprintf_vma (file,
		     (bfd_is_com_section (symbol->section)
		      ? ((elf_symbol_type *) symbol)->internal_elf_sym.st_value
		      : ((elf_symbol_type *) symbol)->internal_elf_sym.st_size));
	fprintf (file, " %s", symbol->name);
      }
      break;
    }
}

/* Create an entry in an ELF linker hash table.  */

struct bfd_hash_entry *
_bfd_elf_link_hash_newfunc (entry, table, string)
     struct bfd_hash_entry *entry;
     struct bfd_hash_table *table;
     const char *string;
{
  struct elf_link_hash_entry *ret = (struct elf_link_hash_entry *) entry;

  /* Allocate the structure if it has not already been allocated by a
     subclass.  */
  if (ret == (struct elf_link_hash_entry *) NULL)
    ret = ((struct elf_link_hash_entry *)
	   bfd_hash_allocate (table, sizeof (struct elf_link_hash_entry)));
  if (ret == (struct elf_link_hash_entry *) NULL)
    {
      bfd_set_error (bfd_error_no_memory);
      return (struct bfd_hash_entry *) ret;
    }

  /* Call the allocation method of the superclass.  */
  ret = ((struct elf_link_hash_entry *)
	 _bfd_link_hash_newfunc ((struct bfd_hash_entry *) ret,
				 table, string));
  if (ret != (struct elf_link_hash_entry *) NULL)
    {
      /* Set local fields.  */
      ret->indx = -1;
      ret->size = 0;
      ret->dynindx = -1;
      ret->dynstr_index = 0;
      ret->weakdef = NULL;
      ret->got_offset = (bfd_vma) -1;
      ret->plt_offset = (bfd_vma) -1;
      ret->type = STT_NOTYPE;
      ret->elf_link_hash_flags = 0;
    }

  return (struct bfd_hash_entry *) ret;
}

/* Initialize an ELF linker hash table.  */

boolean
_bfd_elf_link_hash_table_init (table, abfd, newfunc)
     struct elf_link_hash_table *table;
     bfd *abfd;
     struct bfd_hash_entry *(*newfunc) PARAMS ((struct bfd_hash_entry *,
						struct bfd_hash_table *,
						const char *));
{
  table->dynamic_sections_created = false;
  table->dynobj = NULL;
  /* The first dynamic symbol is a dummy.  */
  table->dynsymcount = 1;
  table->dynstr = NULL;
  table->bucketcount = 0;
  table->needed = NULL;
  return _bfd_link_hash_table_init (&table->root, abfd, newfunc);
}

/* Create an ELF linker hash table.  */

struct bfd_link_hash_table *
_bfd_elf_link_hash_table_create (abfd)
     bfd *abfd;
{
  struct elf_link_hash_table *ret;

  ret = ((struct elf_link_hash_table *)
	 bfd_alloc (abfd, sizeof (struct elf_link_hash_table)));
  if (ret == (struct elf_link_hash_table *) NULL)
    {
      bfd_set_error (bfd_error_no_memory);
      return NULL;
    }

  if (! _bfd_elf_link_hash_table_init (ret, abfd, _bfd_elf_link_hash_newfunc))
    {
      bfd_release (abfd, ret);
      return NULL;
    }

  return &ret->root;
}

/* This is a hook for the ELF emulation code in the generic linker to
   tell the backend linker what file name to use for the DT_NEEDED
   entry for a dynamic object.  The generic linker passes name as an
   empty string to indicate that no DT_NEEDED entry should be made.  */

void
bfd_elf_set_dt_needed_name (abfd, name)
     bfd *abfd;
     const char *name;
{
  if (bfd_get_flavour (abfd) == bfd_target_elf_flavour)
    elf_dt_needed_name (abfd) = name;
}

/* Get the list of DT_NEEDED entries for a link.  */

struct bfd_link_needed_list *
bfd_elf_get_needed_list (abfd, info)
     bfd *abfd;
     struct bfd_link_info *info;
{
  if (info->hash->creator->flavour != bfd_target_elf_flavour)
    return NULL;
  return elf_hash_table (info)->needed;
}

/* Allocate an ELF string table--force the first byte to be zero.  */

struct bfd_strtab_hash *
_bfd_elf_stringtab_init ()
{
  struct bfd_strtab_hash *ret;

  ret = _bfd_stringtab_init ();
  if (ret != NULL)
    {
      bfd_size_type loc;

      loc = _bfd_stringtab_add (ret, "", true, false);
      BFD_ASSERT (loc == 0 || loc == (bfd_size_type) -1);
      if (loc == (bfd_size_type) -1)
	{
	  _bfd_stringtab_free (ret);
	  ret = NULL;
	}
    }
  return ret;
}

/* ELF .o/exec file reading */

/* Create a new bfd section from an ELF section header. */

boolean
bfd_section_from_shdr (abfd, shindex)
     bfd *abfd;
     unsigned int shindex;
{
  Elf_Internal_Shdr *hdr = elf_elfsections (abfd)[shindex];
  Elf_Internal_Ehdr *ehdr = elf_elfheader (abfd);
  struct elf_backend_data *bed = get_elf_backend_data (abfd);
  char *name;

  name = elf_string_from_elf_strtab (abfd, hdr->sh_name);

  switch (hdr->sh_type)
    {
    case SHT_NULL:
      /* Inactive section. Throw it away.  */
      return true;

    case SHT_PROGBITS:	/* Normal section with contents.  */
    case SHT_DYNAMIC:	/* Dynamic linking information.  */
    case SHT_NOBITS:	/* .bss section.  */
    case SHT_HASH:	/* .hash section.  */
      return _bfd_elf_make_section_from_shdr (abfd, hdr, name);

    case SHT_SYMTAB:		/* A symbol table */
      if (elf_onesymtab (abfd) == shindex)
	return true;

      BFD_ASSERT (hdr->sh_entsize == bed->s->sizeof_sym);
      BFD_ASSERT (elf_onesymtab (abfd) == 0);
      elf_onesymtab (abfd) = shindex;
      elf_tdata (abfd)->symtab_hdr = *hdr;
      elf_elfsections (abfd)[shindex] = hdr = &elf_tdata (abfd)->symtab_hdr;
      abfd->flags |= HAS_SYMS;

      /* Sometimes a shared object will map in the symbol table.  If
         SHF_ALLOC is set, and this is a shared object, then we also
         treat this section as a BFD section.  We can not base the
         decision purely on SHF_ALLOC, because that flag is sometimes
         set in a relocateable object file, which would confuse the
         linker.  */
      if ((hdr->sh_flags & SHF_ALLOC) != 0
	  && (abfd->flags & DYNAMIC) != 0
	  && ! _bfd_elf_make_section_from_shdr (abfd, hdr, name))
	return false;

      return true;

    case SHT_DYNSYM:		/* A dynamic symbol table */
      if (elf_dynsymtab (abfd) == shindex)
	return true;

      BFD_ASSERT (hdr->sh_entsize == bed->s->sizeof_sym);
      BFD_ASSERT (elf_dynsymtab (abfd) == 0);
      elf_dynsymtab (abfd) = shindex;
      elf_tdata (abfd)->dynsymtab_hdr = *hdr;
      elf_elfsections (abfd)[shindex] = hdr = &elf_tdata (abfd)->dynsymtab_hdr;
      abfd->flags |= HAS_SYMS;

      /* Besides being a symbol table, we also treat this as a regular
	 section, so that objcopy can handle it.  */
      return _bfd_elf_make_section_from_shdr (abfd, hdr, name);

    case SHT_STRTAB:		/* A string table */
      if (hdr->bfd_section != NULL)
	return true;
      if (ehdr->e_shstrndx == shindex)
	{
	  elf_tdata (abfd)->shstrtab_hdr = *hdr;
	  elf_elfsections (abfd)[shindex] = &elf_tdata (abfd)->shstrtab_hdr;
	  return true;
	}
      {
	unsigned int i;

	for (i = 1; i < ehdr->e_shnum; i++)
	  {
	    Elf_Internal_Shdr *hdr2 = elf_elfsections (abfd)[i];
	    if (hdr2->sh_link == shindex)
	      {
		if (! bfd_section_from_shdr (abfd, i))
		  return false;
		if (elf_onesymtab (abfd) == i)
		  {
		    elf_tdata (abfd)->strtab_hdr = *hdr;
		    elf_elfsections (abfd)[shindex] =
		      &elf_tdata (abfd)->strtab_hdr;
		    return true;
		  }
		if (elf_dynsymtab (abfd) == i)
		  {
		    elf_tdata (abfd)->dynstrtab_hdr = *hdr;
		    elf_elfsections (abfd)[shindex] = hdr =
		      &elf_tdata (abfd)->dynstrtab_hdr;
		    /* We also treat this as a regular section, so
		       that objcopy can handle it.  */
		    break;
		  }
#if 0 /* Not handling other string tables specially right now.  */
		hdr2 = elf_elfsections (abfd)[i];	/* in case it moved */
		/* We have a strtab for some random other section.  */
		newsect = (asection *) hdr2->bfd_section;
		if (!newsect)
		  break;
		hdr->bfd_section = newsect;
		hdr2 = &elf_section_data (newsect)->str_hdr;
		*hdr2 = *hdr;
		elf_elfsections (abfd)[shindex] = hdr2;
#endif
	      }
	  }
      }

      return _bfd_elf_make_section_from_shdr (abfd, hdr, name);

    case SHT_REL:
    case SHT_RELA:
      /* *These* do a lot of work -- but build no sections!  */
      {
	asection *target_sect;
	Elf_Internal_Shdr *hdr2;
	int use_rela_p = get_elf_backend_data (abfd)->use_rela_p;

	/* For some incomprehensible reason Oracle distributes
	   libraries for Solaris in which some of the objects have
	   bogus sh_link fields.  It would be nice if we could just
	   reject them, but, unfortunately, some people need to use
	   them.  We scan through the section headers; if we find only
	   one suitable symbol table, we clobber the sh_link to point
	   to it.  I hope this doesn't break anything.  */
	if (elf_elfsections (abfd)[hdr->sh_link]->sh_type != SHT_SYMTAB
	    && elf_elfsections (abfd)[hdr->sh_link]->sh_type != SHT_DYNSYM)
	  {
	    int scan;
	    int found;

	    found = 0;
	    for (scan = 1; scan < ehdr->e_shnum; scan++)
	      {
		if (elf_elfsections (abfd)[scan]->sh_type == SHT_SYMTAB
		    || elf_elfsections (abfd)[scan]->sh_type == SHT_DYNSYM)
		  {
		    if (found != 0)
		      {
			found = 0;
			break;
		      }
		    found = scan;
		  }
	      }
	    if (found != 0)
	      hdr->sh_link = found;
	  }

	/* Get the symbol table.  */
	if (elf_elfsections (abfd)[hdr->sh_link]->sh_type == SHT_SYMTAB
	    && ! bfd_section_from_shdr (abfd, hdr->sh_link))
	  return false;

	/* If this reloc section does not use the main symbol table we
	   don't treat it as a reloc section.  BFD can't adequately
	   represent such a section, so at least for now, we don't
	   try.  We just present it as a normal section.  */
	if (hdr->sh_link != elf_onesymtab (abfd))
	  return _bfd_elf_make_section_from_shdr (abfd, hdr, name);

	/* Don't allow REL relocations on a machine that uses RELA and
	   vice versa.  */
	/* @@ Actually, the generic ABI does suggest that both might be
	   used in one file.  But the four ABI Processor Supplements I
	   have access to right now all specify that only one is used on
	   each of those architectures.  It's conceivable that, e.g., a
	   bunch of absolute 32-bit relocs might be more compact in REL
	   form even on a RELA machine...  */
	BFD_ASSERT (use_rela_p
		    ? (hdr->sh_type == SHT_RELA
		       && hdr->sh_entsize == bed->s->sizeof_rela)
		    : (hdr->sh_type == SHT_REL
		       && hdr->sh_entsize == bed->s->sizeof_rel));

	if (! bfd_section_from_shdr (abfd, hdr->sh_info))
	  return false;
	target_sect = bfd_section_from_elf_index (abfd, hdr->sh_info);
	if (target_sect == NULL)
	  return false;

	hdr2 = &elf_section_data (target_sect)->rel_hdr;
	*hdr2 = *hdr;
	elf_elfsections (abfd)[shindex] = hdr2;
	target_sect->reloc_count = hdr->sh_size / hdr->sh_entsize;
	target_sect->flags |= SEC_RELOC;
	target_sect->relocation = NULL;
	target_sect->rel_filepos = hdr->sh_offset;
	abfd->flags |= HAS_RELOC;
	return true;
      }
      break;

    case SHT_NOTE:
      break;

    case SHT_SHLIB:
      return true;

    default:
      /* Check for any processor-specific section types.  */
      {
	if (bed->elf_backend_section_from_shdr)
	  (*bed->elf_backend_section_from_shdr) (abfd, hdr, name);
      }
      break;
    }

  return true;
}

/* Given an ELF section number, retrieve the corresponding BFD
   section.  */

asection *
bfd_section_from_elf_index (abfd, index)
     bfd *abfd;
     unsigned int index;
{
  BFD_ASSERT (index > 0 && index < SHN_LORESERVE);
  if (index >= elf_elfheader (abfd)->e_shnum)
    return NULL;
  return elf_elfsections (abfd)[index]->bfd_section;
}

boolean
_bfd_elf_new_section_hook (abfd, sec)
     bfd *abfd;
     asection *sec;
{
  struct bfd_elf_section_data *sdata;

  sdata = (struct bfd_elf_section_data *) bfd_alloc (abfd, sizeof (*sdata));
  if (!sdata)
    {
      bfd_set_error (bfd_error_no_memory);
      return false;
    }
  sec->used_by_bfd = (PTR) sdata;
  memset (sdata, 0, sizeof (*sdata));
  return true;
}

/* Create a new bfd section from an ELF program header.

   Since program segments have no names, we generate a synthetic name
   of the form segment<NUM>, where NUM is generally the index in the
   program header table.  For segments that are split (see below) we
   generate the names segment<NUM>a and segment<NUM>b.

   Note that some program segments may have a file size that is different than
   (less than) the memory size.  All this means is that at execution the
   system must allocate the amount of memory specified by the memory size,
   but only initialize it with the first "file size" bytes read from the
   file.  This would occur for example, with program segments consisting
   of combined data+bss.

   To handle the above situation, this routine generates TWO bfd sections
   for the single program segment.  The first has the length specified by
   the file size of the segment, and the second has the length specified
   by the difference between the two sizes.  In effect, the segment is split
   into it's initialized and uninitialized parts.

 */

boolean
bfd_section_from_phdr (abfd, hdr, index)
     bfd *abfd;
     Elf_Internal_Phdr *hdr;
     int index;
{
  asection *newsect;
  char *name;
  char namebuf[64];
  int split;

  split = ((hdr->p_memsz > 0) &&
	   (hdr->p_filesz > 0) &&
	   (hdr->p_memsz > hdr->p_filesz));
  sprintf (namebuf, split ? "segment%da" : "segment%d", index);
  name = bfd_alloc (abfd, strlen (namebuf) + 1);
  if (!name)
    {
      bfd_set_error (bfd_error_no_memory);
      return false;
    }
  strcpy (name, namebuf);
  newsect = bfd_make_section (abfd, name);
  if (newsect == NULL)
    return false;
  newsect->vma = hdr->p_vaddr;
  newsect->lma = hdr->p_paddr;
  newsect->_raw_size = hdr->p_filesz;
  newsect->filepos = hdr->p_offset;
  newsect->flags |= SEC_HAS_CONTENTS;
  if (hdr->p_type == PT_LOAD)
    {
      newsect->flags |= SEC_ALLOC;
      newsect->flags |= SEC_LOAD;
      if (hdr->p_flags & PF_X)
	{
	  /* FIXME: all we known is that it has execute PERMISSION,
	     may be data. */
	  newsect->flags |= SEC_CODE;
	}
    }
  if (!(hdr->p_flags & PF_W))
    {
      newsect->flags |= SEC_READONLY;
    }

  if (split)
    {
      sprintf (namebuf, "segment%db", index);
      name = bfd_alloc (abfd, strlen (namebuf) + 1);
      if (!name)
	{
	  bfd_set_error (bfd_error_no_memory);
	  return false;
	}
      strcpy (name, namebuf);
      newsect = bfd_make_section (abfd, name);
      if (newsect == NULL)
	return false;
      newsect->vma = hdr->p_vaddr + hdr->p_filesz;
      newsect->lma = hdr->p_paddr + hdr->p_filesz;
      newsect->_raw_size = hdr->p_memsz - hdr->p_filesz;
      if (hdr->p_type == PT_LOAD)
	{
	  newsect->flags |= SEC_ALLOC;
	  if (hdr->p_flags & PF_X)
	    newsect->flags |= SEC_CODE;
	}
      if (!(hdr->p_flags & PF_W))
	newsect->flags |= SEC_READONLY;
    }

  return true;
}

/* Set up an ELF internal section header for a section.  */

/*ARGSUSED*/
static void
elf_fake_sections (abfd, asect, failedptrarg)
     bfd *abfd;
     asection *asect;
     PTR failedptrarg;
{
  struct elf_backend_data *bed = get_elf_backend_data (abfd);
  boolean *failedptr = (boolean *) failedptrarg;
  Elf_Internal_Shdr *this_hdr;

  if (*failedptr)
    {
      /* We already failed; just get out of the bfd_map_over_sections
         loop.  */
      return;
    }

  this_hdr = &elf_section_data (asect)->this_hdr;

  this_hdr->sh_name = (unsigned long) _bfd_stringtab_add (elf_shstrtab (abfd),
							  asect->name,
							  true, false);
  if (this_hdr->sh_name == (unsigned long) -1)
    {
      *failedptr = true;
      return;
    }

  this_hdr->sh_flags = 0;

  if ((asect->flags & SEC_ALLOC) != 0)
    this_hdr->sh_addr = asect->vma;
  else
    this_hdr->sh_addr = 0;

  this_hdr->sh_offset = 0;
  this_hdr->sh_size = asect->_raw_size;
  this_hdr->sh_link = 0;
  this_hdr->sh_addralign = 1 << asect->alignment_power;
  /* The sh_entsize and sh_info fields may have been set already by
     copy_private_section_data.  */

  this_hdr->bfd_section = asect;
  this_hdr->contents = NULL;

  /* FIXME: This should not be based on section names.  */
  if (strcmp (asect->name, ".dynstr") == 0)
    this_hdr->sh_type = SHT_STRTAB;
  else if (strcmp (asect->name, ".hash") == 0)
    {
      this_hdr->sh_type = SHT_HASH;
      this_hdr->sh_entsize = bed->s->arch_size / 8;
    }
  else if (strcmp (asect->name, ".dynsym") == 0)
    {
      this_hdr->sh_type = SHT_DYNSYM;
      this_hdr->sh_entsize = bed->s->sizeof_sym;
    }
  else if (strcmp (asect->name, ".dynamic") == 0)
    {
      this_hdr->sh_type = SHT_DYNAMIC;
      this_hdr->sh_entsize = bed->s->sizeof_dyn;
    }
  else if (strncmp (asect->name, ".rela", 5) == 0
	   && get_elf_backend_data (abfd)->use_rela_p)
    {
      this_hdr->sh_type = SHT_RELA;
      this_hdr->sh_entsize = bed->s->sizeof_rela;
    }
  else if (strncmp (asect->name, ".rel", 4) == 0
	   && ! get_elf_backend_data (abfd)->use_rela_p)
    {
      this_hdr->sh_type = SHT_REL;
      this_hdr->sh_entsize = bed->s->sizeof_rel;
    }
  else if (strcmp (asect->name, ".note") == 0)
    this_hdr->sh_type = SHT_NOTE;
  else if (strncmp (asect->name, ".stab", 5) == 0
	   && strcmp (asect->name + strlen (asect->name) - 3, "str") == 0)
    this_hdr->sh_type = SHT_STRTAB;
  else if ((asect->flags & SEC_ALLOC) != 0
	   && (asect->flags & SEC_LOAD) != 0)
    this_hdr->sh_type = SHT_PROGBITS;
  else if ((asect->flags & SEC_ALLOC) != 0
	   && ((asect->flags & SEC_LOAD) == 0))
    this_hdr->sh_type = SHT_NOBITS;
  else
    {
      /* Who knows?  */
      this_hdr->sh_type = SHT_PROGBITS;
    }

  if ((asect->flags & SEC_ALLOC) != 0)
    this_hdr->sh_flags |= SHF_ALLOC;
  if ((asect->flags & SEC_READONLY) == 0)
    this_hdr->sh_flags |= SHF_WRITE;
  if ((asect->flags & SEC_CODE) != 0)
    this_hdr->sh_flags |= SHF_EXECINSTR;

  /* Check for processor-specific section types.  */
  {
    struct elf_backend_data *bed = get_elf_backend_data (abfd);

    if (bed->elf_backend_fake_sections)
      (*bed->elf_backend_fake_sections) (abfd, this_hdr, asect);
  }

  /* If the section has relocs, set up a section header for the
     SHT_REL[A] section.  */
  if ((asect->flags & SEC_RELOC) != 0)
    {
      Elf_Internal_Shdr *rela_hdr;
      int use_rela_p = get_elf_backend_data (abfd)->use_rela_p;
      char *name;

      rela_hdr = &elf_section_data (asect)->rel_hdr;
      name = bfd_alloc (abfd, sizeof ".rela" + strlen (asect->name));
      if (name == NULL)
	{
	  bfd_set_error (bfd_error_no_memory);
	  *failedptr = true;
	  return;
	}
      sprintf (name, "%s%s", use_rela_p ? ".rela" : ".rel", asect->name);
      rela_hdr->sh_name =
	(unsigned int) _bfd_stringtab_add (elf_shstrtab (abfd), name,
					   true, false);
      if (rela_hdr->sh_name == (unsigned int) -1)
	{
	  *failedptr = true;
	  return;
	}
      rela_hdr->sh_type = use_rela_p ? SHT_RELA : SHT_REL;
      rela_hdr->sh_entsize = (use_rela_p
			      ? bed->s->sizeof_rela
			      : bed->s->sizeof_rel);
      rela_hdr->sh_addralign = bed->s->file_align;
      rela_hdr->sh_flags = 0;
      rela_hdr->sh_addr = 0;
      rela_hdr->sh_size = 0;
      rela_hdr->sh_offset = 0;
    }
}

/* Assign all ELF section numbers.  The dummy first section is handled here
   too.  The link/info pointers for the standard section types are filled
   in here too, while we're at it.  */

static boolean
assign_section_numbers (abfd)
     bfd *abfd;
{
  struct elf_obj_tdata *t = elf_tdata (abfd);
  asection *sec;
  unsigned int section_number;
  Elf_Internal_Shdr **i_shdrp;
  struct elf_backend_data *bed = get_elf_backend_data (abfd);

  section_number = 1;

  for (sec = abfd->sections; sec; sec = sec->next)
    {
      struct bfd_elf_section_data *d = elf_section_data (sec);

      d->this_idx = section_number++;
      if ((sec->flags & SEC_RELOC) == 0)
	d->rel_idx = 0;
      else
	d->rel_idx = section_number++;
    }

  t->shstrtab_section = section_number++;
  elf_elfheader (abfd)->e_shstrndx = t->shstrtab_section;
  t->shstrtab_hdr.sh_size = _bfd_stringtab_size (elf_shstrtab (abfd));

  if (abfd->symcount > 0)
    {
      t->symtab_section = section_number++;
      t->strtab_section = section_number++;
    }

  elf_elfheader (abfd)->e_shnum = section_number;

  /* Set up the list of section header pointers, in agreement with the
     indices.  */
  i_shdrp = ((Elf_Internal_Shdr **)
	     bfd_alloc (abfd, section_number * sizeof (Elf_Internal_Shdr *)));
  if (i_shdrp == NULL)
    {
      bfd_set_error (bfd_error_no_memory);
      return false;
    }

  i_shdrp[0] = ((Elf_Internal_Shdr *)
		bfd_alloc (abfd, sizeof (Elf_Internal_Shdr)));
  if (i_shdrp[0] == NULL)
    {
      bfd_release (abfd, i_shdrp);
      bfd_set_error (bfd_error_no_memory);
      return false;
    }
  memset (i_shdrp[0], 0, sizeof (Elf_Internal_Shdr));

  elf_elfsections (abfd) = i_shdrp;

  i_shdrp[t->shstrtab_section] = &t->shstrtab_hdr;
  if (abfd->symcount > 0)
    {
      i_shdrp[t->symtab_section] = &t->symtab_hdr;
      i_shdrp[t->strtab_section] = &t->strtab_hdr;
      t->symtab_hdr.sh_link = t->strtab_section;
    }
  for (sec = abfd->sections; sec; sec = sec->next)
    {
      struct bfd_elf_section_data *d = elf_section_data (sec);
      asection *s;
      const char *name;

      i_shdrp[d->this_idx] = &d->this_hdr;
      if (d->rel_idx != 0)
	i_shdrp[d->rel_idx] = &d->rel_hdr;

      /* Fill in the sh_link and sh_info fields while we're at it.  */

      /* sh_link of a reloc section is the section index of the symbol
	 table.  sh_info is the section index of the section to which
	 the relocation entries apply.  */
      if (d->rel_idx != 0)
	{
	  d->rel_hdr.sh_link = t->symtab_section;
	  d->rel_hdr.sh_info = d->this_idx;
	}

      switch (d->this_hdr.sh_type)
	{
	case SHT_REL:
	case SHT_RELA:
	  /* A reloc section which we are treating as a normal BFD
	     section.  sh_link is the section index of the symbol
	     table.  sh_info is the section index of the section to
	     which the relocation entries apply.  We assume that an
	     allocated reloc section uses the dynamic symbol table.
	     FIXME: How can we be sure?  */
	  s = bfd_get_section_by_name (abfd, ".dynsym");
	  if (s != NULL)
	    d->this_hdr.sh_link = elf_section_data (s)->this_idx;

	  /* We look up the section the relocs apply to by name.  */
	  name = sec->name;
	  if (d->this_hdr.sh_type == SHT_REL)
	    name += 4;
	  else
	    name += 5;
	  s = bfd_get_section_by_name (abfd, name);
	  if (s != NULL)
	    d->this_hdr.sh_info = elf_section_data (s)->this_idx;
	  break;

	case SHT_STRTAB:
	  /* We assume that a section named .stab*str is a stabs
	     string section.  We look for a section with the same name
	     but without the trailing ``str'', and set its sh_link
	     field to point to this section.  */
	  if (strncmp (sec->name, ".stab", sizeof ".stab" - 1) == 0
	      && strcmp (sec->name + strlen (sec->name) - 3, "str") == 0)
	    {
	      size_t len;
	      char *alc;

	      len = strlen (sec->name);
	      alc = (char *) malloc (len - 2);
	      if (alc == NULL)
		{
		  bfd_set_error (bfd_error_no_memory);
		  return false;
		}
	      strncpy (alc, sec->name, len - 3);
	      alc[len - 3] = '\0';
	      s = bfd_get_section_by_name (abfd, alc);
	      free (alc);
	      if (s != NULL)
		{
		  elf_section_data (s)->this_hdr.sh_link = d->this_idx;

		  /* This is a .stab section.  */
		  elf_section_data (s)->this_hdr.sh_entsize =
		    4 + 2 * (bed->s->arch_size / 8);
		}
	    }
	  break;

	case SHT_DYNAMIC:
	case SHT_DYNSYM:
	  /* sh_link is the section header index of the string table
	     used for the dynamic entries or symbol table.  */
	  s = bfd_get_section_by_name (abfd, ".dynstr");
	  if (s != NULL)
	    d->this_hdr.sh_link = elf_section_data (s)->this_idx;
	  break;

	case SHT_HASH:
	  /* sh_link is the section header index of the symbol table
	     this hash table is for.  */
	  s = bfd_get_section_by_name (abfd, ".dynsym");
	  if (s != NULL)
	    d->this_hdr.sh_link = elf_section_data (s)->this_idx;
	  break;
	}
    }

  return true;
}

/* Map symbol from it's internal number to the external number, moving
   all local symbols to be at the head of the list.  */

static INLINE int
sym_is_global (abfd, sym)
     bfd *abfd;
     asymbol *sym;
{
  /* If the backend has a special mapping, use it.  */
  if (get_elf_backend_data (abfd)->elf_backend_sym_is_global)
    return ((*get_elf_backend_data (abfd)->elf_backend_sym_is_global)
	    (abfd, sym));

  return ((sym->flags & (BSF_GLOBAL | BSF_WEAK)) != 0
	  || bfd_is_und_section (bfd_get_section (sym))
	  || bfd_is_com_section (bfd_get_section (sym)));
}

static boolean
elf_map_symbols (abfd)
     bfd *abfd;
{
  int symcount = bfd_get_symcount (abfd);
  asymbol **syms = bfd_get_outsymbols (abfd);
  asymbol **sect_syms;
  int num_locals = 0;
  int num_globals = 0;
  int num_locals2 = 0;
  int num_globals2 = 0;
  int max_index = 0;
  int num_sections = 0;
  int idx;
  asection *asect;
  asymbol **new_syms;

#ifdef DEBUG
  fprintf (stderr, "elf_map_symbols\n");
  fflush (stderr);
#endif

  /* Add a section symbol for each BFD section.  FIXME: Is this really
     necessary?  */
  for (asect = abfd->sections; asect; asect = asect->next)
    {
      if (max_index < asect->index)
	max_index = asect->index;
    }

  max_index++;
  sect_syms = (asymbol **) bfd_zalloc (abfd, max_index * sizeof (asymbol *));
  if (sect_syms == NULL)
    {
      bfd_set_error (bfd_error_no_memory);
      return false;
    }
  elf_section_syms (abfd) = sect_syms;

  for (idx = 0; idx < symcount; idx++)
    {
      if ((syms[idx]->flags & BSF_SECTION_SYM) != 0
	  && (syms[idx]->value + syms[idx]->section->vma) == 0)
	{
	  asection *sec;

	  sec = syms[idx]->section;
	  if (sec->owner != NULL)
	    {
	      if (sec->owner != abfd)
		{
		  if (sec->output_offset != 0)
		    continue;
		  sec = sec->output_section;
		  BFD_ASSERT (sec->owner == abfd);
		}
	      sect_syms[sec->index] = syms[idx];
	    }
	}
    }

  for (asect = abfd->sections; asect; asect = asect->next)
    {
      asymbol *sym;

      if (sect_syms[asect->index] != NULL)
	continue;

      sym = bfd_make_empty_symbol (abfd);
      if (sym == NULL)
	return false;
      sym->the_bfd = abfd;
      sym->name = asect->name;
      sym->value = 0;
      /* Set the flags to 0 to indicate that this one was newly added.  */
      sym->flags = 0;
      sym->section = asect;
      sect_syms[asect->index] = sym;
      num_sections++;
#ifdef DEBUG
      fprintf (stderr,
	       "creating section symbol, name = %s, value = 0x%.8lx, index = %d, section = 0x%.8lx\n",
	       asect->name, (long) asect->vma, asect->index, (long) asect);
#endif
    }

  /* Classify all of the symbols.  */
  for (idx = 0; idx < symcount; idx++)
    {
      if (!sym_is_global (abfd, syms[idx]))
	num_locals++;
      else
	num_globals++;
    }
  for (asect = abfd->sections; asect; asect = asect->next)
    {
      if (sect_syms[asect->index] != NULL
	  && sect_syms[asect->index]->flags == 0)
	{
	  sect_syms[asect->index]->flags = BSF_SECTION_SYM;
	  if (!sym_is_global (abfd, sect_syms[asect->index]))
	    num_locals++;
	  else
	    num_globals++;
	  sect_syms[asect->index]->flags = 0;
	}
    }

  /* Now sort the symbols so the local symbols are first.  */
  new_syms = ((asymbol **)
	      bfd_alloc (abfd,
			 (num_locals + num_globals) * sizeof (asymbol *)));
  if (new_syms == NULL)
    {
      bfd_set_error (bfd_error_no_memory);
      return false;
    }

  for (idx = 0; idx < symcount; idx++)
    {
      asymbol *sym = syms[idx];
      int i;

      if (!sym_is_global (abfd, sym))
	i = num_locals2++;
      else
	i = num_locals + num_globals2++;
      new_syms[i] = sym;
      sym->udata.i = i + 1;
    }
  for (asect = abfd->sections; asect; asect = asect->next)
    {
      if (sect_syms[asect->index] != NULL
	  && sect_syms[asect->index]->flags == 0)
	{
	  asymbol *sym = sect_syms[asect->index];
	  int i;

	  sym->flags = BSF_SECTION_SYM;
	  if (!sym_is_global (abfd, sym))
	    i = num_locals2++;
	  else
	    i = num_locals + num_globals2++;
	  new_syms[i] = sym;
	  sym->udata.i = i + 1;
	}
    }

  bfd_set_symtab (abfd, new_syms, num_locals + num_globals);

  elf_num_locals (abfd) = num_locals;
  elf_num_globals (abfd) = num_globals;
  return true;
}

/* Align to the maximum file alignment that could be required for any
   ELF data structure.  */

static INLINE file_ptr align_file_position PARAMS ((file_ptr, int));
static INLINE file_ptr
align_file_position (off, align)
     file_ptr off;
     int align;
{
  return (off + align - 1) & ~(align - 1);
}

/* Assign a file position to a section, optionally aligning to the
   required section alignment.  */

INLINE file_ptr
_bfd_elf_assign_file_position_for_section (i_shdrp, offset, align)
     Elf_Internal_Shdr *i_shdrp;
     file_ptr offset;
     boolean align;
{
  if (align)
    {
      unsigned int al;

      al = i_shdrp->sh_addralign;
      if (al > 1)
	offset = BFD_ALIGN (offset, al);
    }
  i_shdrp->sh_offset = offset;
  if (i_shdrp->bfd_section != NULL)
    i_shdrp->bfd_section->filepos = offset;
  if (i_shdrp->sh_type != SHT_NOBITS)
    offset += i_shdrp->sh_size;
  return offset;
}

/* Compute the file positions we are going to put the sections at, and
   otherwise prepare to begin writing out the ELF file.  If LINK_INFO
   is not NULL, this is being called by the ELF backend linker.  */

boolean
_bfd_elf_compute_section_file_positions (abfd, link_info)
     bfd *abfd;
     struct bfd_link_info *link_info;
{
  struct elf_backend_data *bed = get_elf_backend_data (abfd);
  boolean failed;
  struct bfd_strtab_hash *strtab;
  Elf_Internal_Shdr *shstrtab_hdr;

  if (abfd->output_has_begun)
    return true;

  /* Do any elf backend specific processing first.  */
  if (bed->elf_backend_begin_write_processing)
    (*bed->elf_backend_begin_write_processing) (abfd, link_info);

  if (! prep_headers (abfd))
    return false;

  failed = false;
  bfd_map_over_sections (abfd, elf_fake_sections, &failed);
  if (failed)
    return false;

  if (!assign_section_numbers (abfd))
    return false;

  /* The backend linker builds symbol table information itself.  */
  if (link_info == NULL && abfd->symcount > 0)
    {
      if (! swap_out_syms (abfd, &strtab))
	return false;
    }

  shstrtab_hdr = &elf_tdata (abfd)->shstrtab_hdr;
  /* sh_name was set in prep_headers.  */
  shstrtab_hdr->sh_type = SHT_STRTAB;
  shstrtab_hdr->sh_flags = 0;
  shstrtab_hdr->sh_addr = 0;
  shstrtab_hdr->sh_size = _bfd_stringtab_size (elf_shstrtab (abfd));
  shstrtab_hdr->sh_entsize = 0;
  shstrtab_hdr->sh_link = 0;
  shstrtab_hdr->sh_info = 0;
  /* sh_offset is set in assign_file_positions_except_relocs.  */
  shstrtab_hdr->sh_addralign = 1;

  if (!assign_file_positions_except_relocs (abfd))
    return false;

  if (link_info == NULL && abfd->symcount > 0)
    {
      file_ptr off;
      Elf_Internal_Shdr *hdr;

      off = elf_tdata (abfd)->next_file_pos;

      hdr = &elf_tdata (abfd)->symtab_hdr;
      off = _bfd_elf_assign_file_position_for_section (hdr, off, true);

      hdr = &elf_tdata (abfd)->strtab_hdr;
      off = _bfd_elf_assign_file_position_for_section (hdr, off, true);

      elf_tdata (abfd)->next_file_pos = off;

      /* Now that we know where the .strtab section goes, write it
         out.  */
      if (bfd_seek (abfd, hdr->sh_offset, SEEK_SET) != 0
	  || ! _bfd_stringtab_emit (abfd, strtab))
	return false;
      _bfd_stringtab_free (strtab);
    }

  abfd->output_has_begun = true;

  return true;
}

/* Get the size of the program header.

   SORTED_HDRS, if non-NULL, is an array of COUNT pointers to headers sorted
   by VMA.  Non-allocated sections (!SHF_ALLOC) must appear last.  All
   section VMAs and sizes are known so we can compute the correct value.
   (??? This may not be perfectly true.  What cases do we miss?)

   If SORTED_HDRS is NULL we assume there are two segments: text and data
   (exclusive of .interp and .dynamic).

   If this is called by the linker before any of the section VMA's are set, it
   can't calculate the correct value for a strange memory layout.  This only
   happens when SIZEOF_HEADERS is used in a linker script.  In this case,
   SORTED_HDRS is NULL and we assume the normal scenario of one text and one
   data segment (exclusive of .interp and .dynamic).

   ??? User written scripts must either not use SIZEOF_HEADERS, or assume there
   will be two segments.  */

static bfd_size_type
get_program_header_size (abfd, sorted_hdrs, count, maxpagesize)
     bfd *abfd;
     Elf_Internal_Shdr **sorted_hdrs;
     unsigned int count;
     bfd_vma maxpagesize;
{
  size_t segs;
  asection *s;
  struct elf_backend_data *bed = get_elf_backend_data (abfd);

  /* We can't return a different result each time we're called.  */
  if (elf_tdata (abfd)->program_header_size != 0)
    return elf_tdata (abfd)->program_header_size;

  if (sorted_hdrs != NULL)
    {
      unsigned int i;
      unsigned int last_type;
      Elf_Internal_Shdr **hdrpp;
      /* What we think the current segment's offset is.  */
      bfd_vma p_offset;
      /* What we think the current segment's address is.  */
      bfd_vma p_vaddr;
      /* How big we think the current segment is.  */
      bfd_vma p_memsz;
      /* What we think the current file offset is.  */
      bfd_vma file_offset;
      bfd_vma next_offset;

      /* Scan the headers and compute the number of segments required.  This
	 code is intentionally similar to the code in map_program_segments.

	 The `sh_offset' field isn't valid at this point, so we keep our own
	 running total in `file_offset'.

	 This works because section VMAs are already known.  */

      segs = 1;
      /* Make sure the first section goes in the first segment.  */
      file_offset = p_offset = sorted_hdrs[0]->sh_addr % maxpagesize;
      p_vaddr = sorted_hdrs[0]->sh_addr;
      p_memsz = 0;
      last_type = SHT_PROGBITS;

      for (i = 0, hdrpp = sorted_hdrs; i < count; i++, hdrpp++)
	{
	  Elf_Internal_Shdr *hdr;

	  hdr = *hdrpp;

	  /* Ignore any section which will not be part of the process
	     image.  */
	  if ((hdr->sh_flags & SHF_ALLOC) == 0)
	    continue;

	  /* Keep track of where this and the next sections go.
	     The section VMA must equal the file position modulo
	     the page size.  */
	  file_offset += (hdr->sh_addr - file_offset) % maxpagesize;
	  next_offset = file_offset;
	  if (hdr->sh_type != SHT_NOBITS)
	    next_offset = file_offset + hdr->sh_size;

	  /* If this section fits in the segment we are constructing, add
	     it in.  */
	  if ((file_offset - (p_offset + p_memsz)
	       == hdr->sh_addr - (p_vaddr + p_memsz))
	      && (last_type != SHT_NOBITS || hdr->sh_type == SHT_NOBITS))
	    {
	      bfd_size_type adjust;

	      adjust = hdr->sh_addr - (p_vaddr + p_memsz);
	      p_memsz += hdr->sh_size + adjust;
	      file_offset = next_offset;
	      last_type = hdr->sh_type;
	      continue;
	    }

	  /* The section won't fit, start a new segment.  */
	  ++segs;

	  /* Initialize the segment.  */
	  p_vaddr = hdr->sh_addr;
	  p_memsz = hdr->sh_size;
	  p_offset = file_offset;
	  file_offset = next_offset;

	  last_type = hdr->sh_type;
	}
    }
  else
    {
      /* Assume we will need exactly two PT_LOAD segments: one for text
	 and one for data.  */
      segs = 2;
    }

  s = bfd_get_section_by_name (abfd, ".interp");
  if (s != NULL && (s->flags & SEC_LOAD) != 0)
    {
      /* If we have a loadable interpreter section, we need a
	 PT_INTERP segment.  In this case, assume we also need a
	 PT_PHDR segment, although that may not be true for all
	 targets.  */
      segs += 2;
    }

  if (bfd_get_section_by_name (abfd, ".dynamic") != NULL)
    {
      /* We need a PT_DYNAMIC segment.  */
      ++segs;
    }

  /* Let the backend count up any program headers it might need.  */
  if (bed->elf_backend_create_program_headers)
    segs = ((*bed->elf_backend_create_program_headers)
	    (abfd, (Elf_Internal_Phdr *) NULL, segs));

  elf_tdata (abfd)->program_header_size = segs * bed->s->sizeof_phdr;
  return elf_tdata (abfd)->program_header_size;
}

/* Create the program header.  OFF is the file offset where the
   program header should be written.  FIRST is the first loadable ELF
   section.  SORTED_HDRS is the ELF sections sorted by section
   address.  PHDR_SIZE is the size of the program header as returned
   by get_program_header_size.  */

static file_ptr
map_program_segments (abfd, off, first, sorted_hdrs, phdr_size)
     bfd *abfd;
     file_ptr off;
     Elf_Internal_Shdr *first;
     Elf_Internal_Shdr **sorted_hdrs;
     bfd_size_type phdr_size;
{
  Elf_Internal_Phdr phdrs[10];
  unsigned int phdr_count;
  Elf_Internal_Phdr *phdr;
  int phdr_size_adjust;
  unsigned int i;
  Elf_Internal_Shdr **hdrpp;
  asection *sinterp, *sdyn;
  unsigned int last_type;
  Elf_Internal_Ehdr *i_ehdrp;
  struct elf_backend_data *bed = get_elf_backend_data (abfd);

  BFD_ASSERT ((abfd->flags & (EXEC_P | DYNAMIC)) != 0);
  BFD_ASSERT (phdr_size / sizeof (Elf_Internal_Phdr)
	      <= sizeof phdrs / sizeof (phdrs[0]));

  phdr_count = 0;
  phdr = phdrs;

  if (bed->want_hdr_in_seg)
    phdr_size_adjust = first->sh_offset - phdr_size;
  else
    phdr_size_adjust = 0;

  /* If we have a loadable .interp section, we must create a PT_INTERP
     segment which must precede all PT_LOAD segments.  We assume that
     we must also create a PT_PHDR segment, although that may not be
     true for all targets.  */
  sinterp = bfd_get_section_by_name (abfd, ".interp");
  if (sinterp != NULL && (sinterp->flags & SEC_LOAD) != 0)
    {
      BFD_ASSERT (first != NULL);

      phdr->p_type = PT_PHDR;

      phdr->p_offset = off;

      /* Account for any adjustment made because of the alignment of
	 the first loadable section.  */
      phdr_size_adjust = (first->sh_offset - phdr_size) - off;
      BFD_ASSERT (phdr_size_adjust >= 0 && phdr_size_adjust < 128);

      /* The program header precedes all loadable sections.  This lets
	 us compute its loadable address.  This depends on the linker
	 script.  */
      phdr->p_vaddr = first->sh_addr - (phdr_size + phdr_size_adjust);

      phdr->p_paddr = first->bfd_section->lma - (phdr_size + phdr_size_adjust);
      phdr->p_filesz = phdr_size;
      phdr->p_memsz = phdr_size;

      /* FIXME: UnixWare and Solaris set PF_X, Irix 5 does not.  */
      phdr->p_flags = PF_R | PF_X;

      phdr->p_align = bed->s->file_align;
      BFD_ASSERT ((phdr->p_vaddr - phdr->p_offset) % bed->s->file_align == 0);

      /* Include the ELF header in the first loadable segment.  */
      phdr_size_adjust += off;

      ++phdr_count;
      ++phdr;

      phdr->p_type = PT_INTERP;
      phdr->p_offset = sinterp->filepos;
      phdr->p_vaddr = sinterp->vma;
      phdr->p_paddr = sinterp->lma;
      phdr->p_filesz = sinterp->_raw_size;
      phdr->p_memsz = sinterp->_raw_size;
      phdr->p_flags = PF_R;
      phdr->p_align = 1 << bfd_get_section_alignment (abfd, sinterp);

      ++phdr_count;
      ++phdr;
    }

  /* Look through the sections to see how they will be divided into
     program segments.  The sections must be arranged in order by
     sh_addr for this to work correctly.  */
  phdr->p_type = PT_NULL;
  last_type = SHT_PROGBITS;
  for (i = 1, hdrpp = sorted_hdrs;
       i < elf_elfheader (abfd)->e_shnum;
       i++, hdrpp++)
    {
      Elf_Internal_Shdr *hdr;

      hdr = *hdrpp;

      /* Ignore any section which will not be part of the process
	 image.  */
      if ((hdr->sh_flags & SHF_ALLOC) == 0)
	continue;

      /* If this section fits in the segment we are constructing, add
	 it in.  */
      if (phdr->p_type != PT_NULL
	  && (hdr->sh_offset - (phdr->p_offset + phdr->p_memsz)
	      == hdr->sh_addr - (phdr->p_vaddr + phdr->p_memsz))
	  && (hdr->sh_addr - hdr->bfd_section->lma
	      == phdr->p_vaddr - phdr->p_paddr)
	  && (last_type != SHT_NOBITS || hdr->sh_type == SHT_NOBITS))
	{
	  bfd_size_type adjust;

	  adjust = hdr->sh_addr - (phdr->p_vaddr + phdr->p_memsz);
	  phdr->p_memsz += hdr->sh_size + adjust;
	  if (hdr->sh_type != SHT_NOBITS)
	    phdr->p_filesz += hdr->sh_size + adjust;
	  if ((hdr->sh_flags & SHF_WRITE) != 0)
	    phdr->p_flags |= PF_W;
	  if ((hdr->sh_flags & SHF_EXECINSTR) != 0)
	    phdr->p_flags |= PF_X;
	  last_type = hdr->sh_type;
	  continue;
	}

      /* The section won't fit, start a new segment.  If we're already in one,
	 move to the next one.  */
      if (phdr->p_type != PT_NULL)
	{
	  ++phdr;
	  ++phdr_count;
	}

      /* Initialize the segment.  */
      phdr->p_type = PT_LOAD;
      phdr->p_offset = hdr->sh_offset;
      phdr->p_vaddr = hdr->sh_addr;
      phdr->p_paddr = hdr->bfd_section->lma;
      if (hdr->sh_type == SHT_NOBITS)
	phdr->p_filesz = 0;
      else
	phdr->p_filesz = hdr->sh_size;
      phdr->p_memsz = hdr->sh_size;
      phdr->p_flags = PF_R;
      if ((hdr->sh_flags & SHF_WRITE) != 0)
	phdr->p_flags |= PF_W;
      if ((hdr->sh_flags & SHF_EXECINSTR) != 0)
	phdr->p_flags |= PF_X;
      phdr->p_align = bed->maxpagesize;

      if (hdr == first
	  && (bed->want_hdr_in_seg
	      || (sinterp != NULL
		  && (sinterp->flags & SEC_LOAD) != 0)))
	{
	  phdr->p_offset -= phdr_size + phdr_size_adjust;
	  phdr->p_vaddr -= phdr_size + phdr_size_adjust;
	  phdr->p_paddr -= phdr_size + phdr_size_adjust;
	  phdr->p_filesz += phdr_size + phdr_size_adjust;
	  phdr->p_memsz += phdr_size + phdr_size_adjust;
	}

      last_type = hdr->sh_type;
    }

  if (phdr->p_type != PT_NULL)
    {
      ++phdr;
      ++phdr_count;
    }

  /* If we have a .dynamic section, create a PT_DYNAMIC segment.  */
  sdyn = bfd_get_section_by_name (abfd, ".dynamic");
  if (sdyn != NULL && (sdyn->flags & SEC_LOAD) != 0)
    {
      phdr->p_type = PT_DYNAMIC;
      phdr->p_offset = sdyn->filepos;
      phdr->p_vaddr = sdyn->vma;
      phdr->p_paddr = sdyn->lma;
      phdr->p_filesz = sdyn->_raw_size;
      phdr->p_memsz = sdyn->_raw_size;
      phdr->p_flags = PF_R;
      if ((sdyn->flags & SEC_READONLY) == 0)
	phdr->p_flags |= PF_W;
      if ((sdyn->flags & SEC_CODE) != 0)
	phdr->p_flags |= PF_X;
      phdr->p_align = 1 << bfd_get_section_alignment (abfd, sdyn);

      ++phdr;
      ++phdr_count;
    }

  /* Let the backend create additional program headers.  */
  if (bed->elf_backend_create_program_headers)
    phdr_count = (*bed->elf_backend_create_program_headers) (abfd,
							     phdrs,
							     phdr_count);

  /* Make sure the return value from get_program_header_size matches
     what we computed here.  Actually, it's OK if we allocated too
     much space in the program header.  */
  if (phdr_count > phdr_size / bed->s->sizeof_phdr)
    {
      ((*_bfd_error_handler)
       ("%s: Not enough room for program headers (allocated %lu, need %u)",
	bfd_get_filename (abfd),
	(unsigned long) (phdr_size / bed->s->sizeof_phdr),
	phdr_count));
      bfd_set_error (bfd_error_bad_value);
      return (file_ptr) -1;
    }

  /* Set up program header information.  */
  i_ehdrp = elf_elfheader (abfd);
  i_ehdrp->e_phentsize = bed->s->sizeof_phdr;
  i_ehdrp->e_phoff = off;
  i_ehdrp->e_phnum = phdr_count;

  /* Save the program headers away.  I don't think anybody uses this
     information right now.  */
  elf_tdata (abfd)->phdr = ((Elf_Internal_Phdr *)
			    bfd_alloc (abfd,
				       (phdr_count
					* sizeof (Elf_Internal_Phdr))));
  if (elf_tdata (abfd)->phdr == NULL && phdr_count != 0)
    {
      bfd_set_error (bfd_error_no_memory);
      return (file_ptr) -1;
    }
  memcpy (elf_tdata (abfd)->phdr, phdrs,
	  phdr_count * sizeof (Elf_Internal_Phdr));

  /* Write out the program headers.  */
  if (bfd_seek (abfd, off, SEEK_SET) != 0)
    return (file_ptr) -1;

  if (bed->s->write_out_phdrs (abfd, phdrs, phdr_count) != 0)
    return (file_ptr) -1;

  return off + phdr_count * bed->s->sizeof_phdr;
}

/* Work out the file positions of all the sections.  This is called by
   _bfd_elf_compute_section_file_positions.  All the section sizes and
   VMAs must be known before this is called.

   We do not consider reloc sections at this point, unless they form
   part of the loadable image.  Reloc sections are assigned file
   positions in assign_file_positions_for_relocs, which is called by
   write_object_contents and final_link.

   We also don't set the positions of the .symtab and .strtab here.  */

static int elf_sort_hdrs PARAMS ((const PTR, const PTR));

static boolean
assign_file_positions_except_relocs (abfd)
     bfd *abfd;
{
  struct elf_obj_tdata * const tdata = elf_tdata (abfd);
  Elf_Internal_Ehdr * const i_ehdrp = elf_elfheader (abfd);
  Elf_Internal_Shdr ** const i_shdrpp = elf_elfsections (abfd);
  file_ptr off;
  struct elf_backend_data *bed = get_elf_backend_data (abfd);

  /* Start after the ELF header.  */
  off = i_ehdrp->e_ehsize;

  if ((abfd->flags & (EXEC_P | DYNAMIC)) == 0)
    {
      Elf_Internal_Shdr **hdrpp;
      unsigned int i;

      /* We are not creating an executable, which means that we are
	 not creating a program header, and that the actual order of
	 the sections in the file is unimportant.  */
      for (i = 1, hdrpp = i_shdrpp + 1; i < i_ehdrp->e_shnum; i++, hdrpp++)
	{
	  Elf_Internal_Shdr *hdr;

	  hdr = *hdrpp;
	  if (hdr->sh_type == SHT_REL || hdr->sh_type == SHT_RELA)
	    {
	      hdr->sh_offset = -1;
	      continue;
	    }
	  if (i == tdata->symtab_section
	      || i == tdata->strtab_section)
	    {
	      hdr->sh_offset = -1;
	      continue;
	    }
	  
	  off = _bfd_elf_assign_file_position_for_section (hdr, off, true);
	}
    }
  else
    {
      file_ptr phdr_off;
      bfd_size_type phdr_size;
      bfd_vma maxpagesize;
      size_t hdrppsize;
      Elf_Internal_Shdr **sorted_hdrs;
      Elf_Internal_Shdr **hdrpp;
      unsigned int i;
      Elf_Internal_Shdr *first;
      file_ptr phdr_map;

      /* We are creating an executable.  */

      maxpagesize = get_elf_backend_data (abfd)->maxpagesize;
      if (maxpagesize == 0)
	maxpagesize = 1;

      /* We must sort the sections.  The GNU linker will always create
	 the sections in an appropriate order, but the Irix 5 linker
	 will not.  We don't include the dummy first section in the
	 sort.  We sort sections which are not SHF_ALLOC to the end.  */
      hdrppsize = (i_ehdrp->e_shnum - 1) * sizeof (Elf_Internal_Shdr *);
      sorted_hdrs = (Elf_Internal_Shdr **) malloc (hdrppsize);
      if (sorted_hdrs == NULL)
	{
	  bfd_set_error (bfd_error_no_memory);
	  return false;
	}

      memcpy (sorted_hdrs, i_shdrpp + 1, hdrppsize);
      qsort (sorted_hdrs, (size_t) i_ehdrp->e_shnum - 1,
	     sizeof (Elf_Internal_Shdr *), elf_sort_hdrs);

      /* We can't actually create the program header until we have set the
	 file positions for the sections, and we can't do that until we know
	 how big the header is going to be.  */
      off = align_file_position (off, bed->s->file_align);
      phdr_size = get_program_header_size (abfd,
					   sorted_hdrs, i_ehdrp->e_shnum - 1,
					   maxpagesize);
      if (phdr_size == (bfd_size_type) -1)
	return false;

      /* Compute the file offsets of each section.  */
      phdr_off = off;
      off += phdr_size;
      first = NULL;
      for (i = 1, hdrpp = sorted_hdrs; i < i_ehdrp->e_shnum; i++, hdrpp++)
	{
	  Elf_Internal_Shdr *hdr;

	  hdr = *hdrpp;
	  if ((hdr->sh_flags & SHF_ALLOC) == 0)
	    {
	      if (hdr->sh_type == SHT_REL || hdr->sh_type == SHT_RELA)
		{
		  hdr->sh_offset = -1;
		  continue;
		}
	      if (hdr == i_shdrpp[tdata->symtab_section]
		  || hdr == i_shdrpp[tdata->strtab_section])
		{
		  hdr->sh_offset = -1;
		  continue;
		}
	      off = _bfd_elf_assign_file_position_for_section (hdr, off,
							       true);
	    }
	  else
	    {
	      if (first == NULL)
		first = hdr;

	      /* The section VMA must equal the file position modulo
		 the page size.  This is required by the program
		 header.  */
	      off += (hdr->sh_addr - off) % maxpagesize;
	      off = _bfd_elf_assign_file_position_for_section (hdr, off,
							       false);
	    }
	}

      /* Create the program header.  */
      phdr_map = map_program_segments (abfd, phdr_off, first, sorted_hdrs,
				       phdr_size);
      if (phdr_map == (file_ptr) -1)
	return false;
      BFD_ASSERT ((bfd_size_type) phdr_map
		  <= (bfd_size_type) phdr_off + phdr_size);

      free (sorted_hdrs);
    }

  /* Place the section headers.  */
  off = align_file_position (off, bed->s->file_align);
  i_ehdrp->e_shoff = off;
  off += i_ehdrp->e_shnum * i_ehdrp->e_shentsize;

  elf_tdata (abfd)->next_file_pos = off;

  return true;
}

/* Sort the ELF headers by VMA.  We sort headers which are not
   SHF_ALLOC to the end.  */
static int
elf_sort_hdrs (arg1, arg2)
     const PTR arg1;
     const PTR arg2;
{
  int ret;
  const Elf_Internal_Shdr *hdr1 = *(const Elf_Internal_Shdr **) arg1;
  const Elf_Internal_Shdr *hdr2 = *(const Elf_Internal_Shdr **) arg2;

#define TOEND(x) (((x)->sh_flags & SHF_ALLOC)==0)

  if (TOEND (hdr1))
    if (TOEND (hdr2))
      return 0;
    else 
      return 1;

  if (TOEND (hdr2))
    return -1;

  if (hdr1->sh_addr < hdr2->sh_addr)
    return -1;
  else if (hdr1->sh_addr > hdr2->sh_addr)
    return 1;

  /* Put !SHT_NOBITS sections before SHT_NOBITS ones.
     The main loop in map_program_segments requires this. */

  ret = (hdr1->sh_type == SHT_NOBITS) - (hdr2->sh_type == SHT_NOBITS);

  if (ret != 0)
    return ret;
  if (hdr1->sh_size < hdr2->sh_size)
    return -1;
  if (hdr1->sh_size > hdr2->sh_size)
    return 1;
  return 0;
}

static boolean
prep_headers (abfd)
     bfd *abfd;
{
  Elf_Internal_Ehdr *i_ehdrp;	/* Elf file header, internal form */
  Elf_Internal_Phdr *i_phdrp = 0;	/* Program header table, internal form */
  Elf_Internal_Shdr **i_shdrp;	/* Section header table, internal form */
  int count;
  struct bfd_strtab_hash *shstrtab;
  struct elf_backend_data *bed = get_elf_backend_data (abfd);

  i_ehdrp = elf_elfheader (abfd);
  i_shdrp = elf_elfsections (abfd);

  shstrtab = _bfd_elf_stringtab_init ();
  if (shstrtab == NULL)
    return false;

  elf_shstrtab (abfd) = shstrtab;

  i_ehdrp->e_ident[EI_MAG0] = ELFMAG0;
  i_ehdrp->e_ident[EI_MAG1] = ELFMAG1;
  i_ehdrp->e_ident[EI_MAG2] = ELFMAG2;
  i_ehdrp->e_ident[EI_MAG3] = ELFMAG3;

  i_ehdrp->e_ident[EI_CLASS] = bed->s->elfclass;
  i_ehdrp->e_ident[EI_DATA] =
    abfd->xvec->byteorder_big_p ? ELFDATA2MSB : ELFDATA2LSB;
  i_ehdrp->e_ident[EI_VERSION] = bed->s->ev_current;

  for (count = EI_PAD; count < EI_NIDENT; count++)
    i_ehdrp->e_ident[count] = 0;

  if ((abfd->flags & DYNAMIC) != 0)
    i_ehdrp->e_type = ET_DYN;
  else if ((abfd->flags & EXEC_P) != 0)
    i_ehdrp->e_type = ET_EXEC;
  else
    i_ehdrp->e_type = ET_REL;

  switch (bfd_get_arch (abfd))
    {
    case bfd_arch_unknown:
      i_ehdrp->e_machine = EM_NONE;
      break;
    case bfd_arch_sparc:
      if (bed->s->arch_size == 64)
	i_ehdrp->e_machine = EM_SPARC64;
      else
	i_ehdrp->e_machine = EM_SPARC;
      break;
    case bfd_arch_i386:
      i_ehdrp->e_machine = EM_386;
      break;
    case bfd_arch_m68k:
      i_ehdrp->e_machine = EM_68K;
      break;
    case bfd_arch_m88k:
      i_ehdrp->e_machine = EM_88K;
      break;
    case bfd_arch_i860:
      i_ehdrp->e_machine = EM_860;
      break;
    case bfd_arch_mips:	/* MIPS Rxxxx */
      i_ehdrp->e_machine = EM_MIPS;	/* only MIPS R3000 */
      break;
    case bfd_arch_hppa:
      i_ehdrp->e_machine = EM_PARISC;
      break;
    case bfd_arch_powerpc:
      i_ehdrp->e_machine = EM_PPC;
      break;
      /* also note that EM_M32, AT&T WE32100 is unknown to bfd */
    default:
      i_ehdrp->e_machine = EM_NONE;
    }
  i_ehdrp->e_version = bed->s->ev_current;
  i_ehdrp->e_ehsize = bed->s->sizeof_ehdr;

  /* no program header, for now. */
  i_ehdrp->e_phoff = 0;
  i_ehdrp->e_phentsize = 0;
  i_ehdrp->e_phnum = 0;

  /* each bfd section is section header entry */
  i_ehdrp->e_entry = bfd_get_start_address (abfd);
  i_ehdrp->e_shentsize = bed->s->sizeof_shdr;

  /* if we're building an executable, we'll need a program header table */
  if (abfd->flags & EXEC_P)
    {
      /* it all happens later */
#if 0
      i_ehdrp->e_phentsize = sizeof (Elf_External_Phdr);

      /* elf_build_phdrs() returns a (NULL-terminated) array of
	 Elf_Internal_Phdrs */
      i_phdrp = elf_build_phdrs (abfd, i_ehdrp, i_shdrp, &i_ehdrp->e_phnum);
      i_ehdrp->e_phoff = outbase;
      outbase += i_ehdrp->e_phentsize * i_ehdrp->e_phnum;
#endif
    }
  else
    {
      i_ehdrp->e_phentsize = 0;
      i_phdrp = 0;
      i_ehdrp->e_phoff = 0;
    }

  elf_tdata (abfd)->symtab_hdr.sh_name =
    (unsigned int) _bfd_stringtab_add (shstrtab, ".symtab", true, false);
  elf_tdata (abfd)->strtab_hdr.sh_name =
    (unsigned int) _bfd_stringtab_add (shstrtab, ".strtab", true, false);
  elf_tdata (abfd)->shstrtab_hdr.sh_name =
    (unsigned int) _bfd_stringtab_add (shstrtab, ".shstrtab", true, false);
  if (elf_tdata (abfd)->symtab_hdr.sh_name == (unsigned int) -1
      || elf_tdata (abfd)->symtab_hdr.sh_name == (unsigned int) -1
      || elf_tdata (abfd)->shstrtab_hdr.sh_name == (unsigned int) -1)
    return false;

  return true;
}

/* Assign file positions for all the reloc sections which are not part
   of the loadable file image.  */

void
_bfd_elf_assign_file_positions_for_relocs (abfd)
     bfd *abfd;
{
  file_ptr off;
  unsigned int i;
  Elf_Internal_Shdr **shdrpp;

  off = elf_tdata (abfd)->next_file_pos;

  for (i = 1, shdrpp = elf_elfsections (abfd) + 1;
       i < elf_elfheader (abfd)->e_shnum;
       i++, shdrpp++)
    {
      Elf_Internal_Shdr *shdrp;

      shdrp = *shdrpp;
      if ((shdrp->sh_type == SHT_REL || shdrp->sh_type == SHT_RELA)
	  && shdrp->sh_offset == -1)
	off = _bfd_elf_assign_file_position_for_section (shdrp, off, true);
    }

  elf_tdata (abfd)->next_file_pos = off;
}

boolean
_bfd_elf_write_object_contents (abfd)
     bfd *abfd;
{
  struct elf_backend_data *bed = get_elf_backend_data (abfd);
  Elf_Internal_Ehdr *i_ehdrp;
  Elf_Internal_Shdr **i_shdrp;
  boolean failed;
  unsigned int count;

  if (! abfd->output_has_begun
      && ! _bfd_elf_compute_section_file_positions (abfd,
						    (struct bfd_link_info *) NULL))
    return false;

  i_shdrp = elf_elfsections (abfd);
  i_ehdrp = elf_elfheader (abfd);

  failed = false;
  bfd_map_over_sections (abfd, bed->s->write_relocs, &failed);
  if (failed)
    return false;
  _bfd_elf_assign_file_positions_for_relocs (abfd);

  /* After writing the headers, we need to write the sections too... */
  for (count = 1; count < i_ehdrp->e_shnum; count++)
    {
      if (bed->elf_backend_section_processing)
	(*bed->elf_backend_section_processing) (abfd, i_shdrp[count]);
      if (i_shdrp[count]->contents)
	{
	  if (bfd_seek (abfd, i_shdrp[count]->sh_offset, SEEK_SET) != 0
	      || (bfd_write (i_shdrp[count]->contents, i_shdrp[count]->sh_size,
			     1, abfd)
		  != i_shdrp[count]->sh_size))
	    return false;
	}
    }

  /* Write out the section header names.  */
  if (bfd_seek (abfd, elf_tdata (abfd)->shstrtab_hdr.sh_offset, SEEK_SET) != 0
      || ! _bfd_stringtab_emit (abfd, elf_shstrtab (abfd)))
    return false;

  if (bed->elf_backend_final_write_processing)
    (*bed->elf_backend_final_write_processing) (abfd,
						elf_tdata (abfd)->linker);

  return bed->s->write_shdrs_and_ehdr (abfd);
}

/* given a section, search the header to find them... */
int
_bfd_elf_section_from_bfd_section (abfd, asect)
     bfd *abfd;
     struct sec *asect;
{
  struct elf_backend_data *bed = get_elf_backend_data (abfd);
  Elf_Internal_Shdr **i_shdrp = elf_elfsections (abfd);
  int index;
  Elf_Internal_Shdr *hdr;
  int maxindex = elf_elfheader (abfd)->e_shnum;

  for (index = 0; index < maxindex; index++)
    {
      hdr = i_shdrp[index];
      if (hdr->bfd_section == asect)
	return index;
    }

  if (bed->elf_backend_section_from_bfd_section)
    {
      for (index = 0; index < maxindex; index++)
	{
	  int retval;

	  hdr = i_shdrp[index];
	  retval = index;
	  if ((*bed->elf_backend_section_from_bfd_section)
	      (abfd, hdr, asect, &retval))
	    return retval;
	}
    }

  if (bfd_is_abs_section (asect))
    return SHN_ABS;
  if (bfd_is_com_section (asect))
    return SHN_COMMON;
  if (bfd_is_und_section (asect))
    return SHN_UNDEF;

  return -1;
}

/* given a symbol, return the bfd index for that symbol.  */
 int
_bfd_elf_symbol_from_bfd_symbol (abfd, asym_ptr_ptr)
     bfd *abfd;
     struct symbol_cache_entry **asym_ptr_ptr;
{
  struct symbol_cache_entry *asym_ptr = *asym_ptr_ptr;
  int idx;
  flagword flags = asym_ptr->flags;

  /* When gas creates relocations against local labels, it creates its
     own symbol for the section, but does put the symbol into the
     symbol chain, so udata is 0.  When the linker is generating
     relocatable output, this section symbol may be for one of the
     input sections rather than the output section.  */
  if (asym_ptr->udata.i == 0
      && (flags & BSF_SECTION_SYM)
      && asym_ptr->section)
    {
      int indx;

      if (asym_ptr->section->output_section != NULL)
	indx = asym_ptr->section->output_section->index;
      else
	indx = asym_ptr->section->index;
      if (elf_section_syms (abfd)[indx])
	asym_ptr->udata.i = elf_section_syms (abfd)[indx]->udata.i;
    }

  idx = asym_ptr->udata.i;
  BFD_ASSERT (idx != 0);

#if DEBUG & 4
  {
    fprintf (stderr,
	     "elf_symbol_from_bfd_symbol 0x%.8lx, name = %s, sym num = %d, flags = 0x%.8lx%s\n",
     (long) asym_ptr, asym_ptr->name, idx, flags, elf_symbol_flags (flags));
    fflush (stderr);
  }
#endif

  return idx;
}

/* Copy private section information.  This copies over the entsize
   field, and sometimes the info field.  */

boolean
_bfd_elf_copy_private_section_data (ibfd, isec, obfd, osec)
     bfd *ibfd;
     asection *isec;
     bfd *obfd;
     asection *osec;
{
  Elf_Internal_Shdr *ihdr, *ohdr;

  if (ibfd->xvec->flavour != bfd_target_elf_flavour
      || obfd->xvec->flavour != bfd_target_elf_flavour)
    return true;

  ihdr = &elf_section_data (isec)->this_hdr;
  ohdr = &elf_section_data (osec)->this_hdr;

  ohdr->sh_entsize = ihdr->sh_entsize;

  if (ihdr->sh_type == SHT_SYMTAB
      || ihdr->sh_type == SHT_DYNSYM)
    ohdr->sh_info = ihdr->sh_info;

  return true;
}

/* Copy private symbol information.  If this symbol is in a section
   which we did not map into a BFD section, try to map the section
   index correctly.  We use special macro definitions for the mapped
   section indices; these definitions are interpreted by the
   swap_out_syms function.  */

#define MAP_ONESYMTAB (SHN_LORESERVE - 1)
#define MAP_DYNSYMTAB (SHN_LORESERVE - 2)
#define MAP_STRTAB (SHN_LORESERVE - 3)
#define MAP_SHSTRTAB (SHN_LORESERVE - 4)

boolean
_bfd_elf_copy_private_symbol_data (ibfd, isymarg, obfd, osymarg)
     bfd *ibfd;
     asymbol *isymarg;
     bfd *obfd;
     asymbol *osymarg;
{
  elf_symbol_type *isym, *osym;

  isym = elf_symbol_from (ibfd, isymarg);
  osym = elf_symbol_from (obfd, osymarg);

  if (isym != NULL
      && osym != NULL
      && bfd_is_abs_section (isym->symbol.section))
    {
      unsigned int shndx;

      shndx = isym->internal_elf_sym.st_shndx;
      if (shndx == elf_onesymtab (ibfd))
	shndx = MAP_ONESYMTAB;
      else if (shndx == elf_dynsymtab (ibfd))
	shndx = MAP_DYNSYMTAB;
      else if (shndx == elf_tdata (ibfd)->strtab_section)
	shndx = MAP_STRTAB;
      else if (shndx == elf_tdata (ibfd)->shstrtab_section)
	shndx = MAP_SHSTRTAB;
      osym->internal_elf_sym.st_shndx = shndx;
    }

  return true;
}

/* Swap out the symbols.  */

static boolean
swap_out_syms (abfd, sttp)
     bfd *abfd;
     struct bfd_strtab_hash **sttp;
{
  struct elf_backend_data *bed = get_elf_backend_data (abfd);

  if (!elf_map_symbols (abfd))
    return false;

  /* Dump out the symtabs. */
  {
    int symcount = bfd_get_symcount (abfd);
    asymbol **syms = bfd_get_outsymbols (abfd);
    struct bfd_strtab_hash *stt;
    Elf_Internal_Shdr *symtab_hdr;
    Elf_Internal_Shdr *symstrtab_hdr;
    char *outbound_syms;
    int idx;

    stt = _bfd_elf_stringtab_init ();
    if (stt == NULL)
      return false;

    symtab_hdr = &elf_tdata (abfd)->symtab_hdr;
    symtab_hdr->sh_type = SHT_SYMTAB;
    symtab_hdr->sh_entsize = bed->s->sizeof_sym;
    symtab_hdr->sh_size = symtab_hdr->sh_entsize * (symcount + 1);
    symtab_hdr->sh_info = elf_num_locals (abfd) + 1;
    symtab_hdr->sh_addralign = bed->s->file_align;

    symstrtab_hdr = &elf_tdata (abfd)->strtab_hdr;
    symstrtab_hdr->sh_type = SHT_STRTAB;

    outbound_syms = bfd_alloc (abfd,
			       (1 + symcount) * bed->s->sizeof_sym);
    if (outbound_syms == NULL)
      {
	bfd_set_error (bfd_error_no_memory);
	return false;
      }
    symtab_hdr->contents = (PTR) outbound_syms;

    /* now generate the data (for "contents") */
    {
      /* Fill in zeroth symbol and swap it out.  */
      Elf_Internal_Sym sym;
      sym.st_name = 0;
      sym.st_value = 0;
      sym.st_size = 0;
      sym.st_info = 0;
      sym.st_other = 0;
      sym.st_shndx = SHN_UNDEF;
      bed->s->swap_symbol_out (abfd, &sym, (PTR) outbound_syms);
      outbound_syms += bed->s->sizeof_sym;
    }
    for (idx = 0; idx < symcount; idx++)
      {
	Elf_Internal_Sym sym;
	bfd_vma value = syms[idx]->value;
	elf_symbol_type *type_ptr;
	flagword flags = syms[idx]->flags;

	if (flags & BSF_SECTION_SYM)
	  /* Section symbols have no names.  */
	  sym.st_name = 0;
	else
	  {
	    sym.st_name = (unsigned long) _bfd_stringtab_add (stt,
							      syms[idx]->name,
							      true, false);
	    if (sym.st_name == (unsigned long) -1)
	      return false;
	  }

	type_ptr = elf_symbol_from (abfd, syms[idx]);

	if (bfd_is_com_section (syms[idx]->section))
	  {
	    /* ELF common symbols put the alignment into the `value' field,
	       and the size into the `size' field.  This is backwards from
	       how BFD handles it, so reverse it here.  */
	    sym.st_size = value;
	    if (type_ptr == NULL
		|| type_ptr->internal_elf_sym.st_value == 0)
	      sym.st_value = value >= 16 ? 16 : (1 << bfd_log2 (value));
	    else
	      sym.st_value = type_ptr->internal_elf_sym.st_value;
	    sym.st_shndx = _bfd_elf_section_from_bfd_section (abfd,
							      syms[idx]->section);
	  }
	else
	  {
	    asection *sec = syms[idx]->section;
	    int shndx;

	    if (sec->output_section)
	      {
		value += sec->output_offset;
		sec = sec->output_section;
	      }
	    value += sec->vma;
	    sym.st_value = value;
	    sym.st_size = type_ptr ? type_ptr->internal_elf_sym.st_size : 0;

	    if (bfd_is_abs_section (sec)
		&& type_ptr != NULL
		&& type_ptr->internal_elf_sym.st_shndx != 0)
	      {
		/* This symbol is in a real ELF section which we did
                   not create as a BFD section.  Undo the mapping done
                   by copy_private_symbol_data.  */
		shndx = type_ptr->internal_elf_sym.st_shndx;
		switch (shndx)
		  {
		  case MAP_ONESYMTAB:
		    shndx = elf_onesymtab (abfd);
		    break;
		  case MAP_DYNSYMTAB:
		    shndx = elf_dynsymtab (abfd);
		    break;
		  case MAP_STRTAB:
		    shndx = elf_tdata (abfd)->strtab_section;
		    break;
		  case MAP_SHSTRTAB:
		    shndx = elf_tdata (abfd)->shstrtab_section;
		    break;
		  default:
		    break;
		  }
	      }
	    else
	      {
		shndx = _bfd_elf_section_from_bfd_section (abfd, sec);

		if (shndx == -1)
		  {
		    asection *sec2;

		    /* Writing this would be a hell of a lot easier if
		       we had some decent documentation on bfd, and
		       knew what to expect of the library, and what to
		       demand of applications.  For example, it
		       appears that `objcopy' might not set the
		       section of a symbol to be a section that is
		       actually in the output file.  */
		    sec2 = bfd_get_section_by_name (abfd, sec->name);
		    BFD_ASSERT (sec2 != 0);
		    shndx = _bfd_elf_section_from_bfd_section (abfd, sec2);
		    BFD_ASSERT (shndx != -1);
		  }
	      }

	    sym.st_shndx = shndx;
	  }

	if (bfd_is_com_section (syms[idx]->section))
	  sym.st_info = ELF_ST_INFO (STB_GLOBAL, STT_OBJECT);
	else if (bfd_is_und_section (syms[idx]->section))
	  sym.st_info = ELF_ST_INFO (((flags & BSF_WEAK)
				      ? STB_WEAK
				      : STB_GLOBAL),
				     ((flags & BSF_FUNCTION)
				      ? STT_FUNC
				      : STT_NOTYPE));
	else if (flags & BSF_SECTION_SYM)
	  sym.st_info = ELF_ST_INFO (STB_LOCAL, STT_SECTION);
	else if (flags & BSF_FILE)
	  sym.st_info = ELF_ST_INFO (STB_LOCAL, STT_FILE);
	else
	  {
	    int bind = STB_LOCAL;
	    int type = STT_OBJECT;

	    if (flags & BSF_LOCAL)
	      bind = STB_LOCAL;
	    else if (flags & BSF_WEAK)
	      bind = STB_WEAK;
	    else if (flags & BSF_GLOBAL)
	      bind = STB_GLOBAL;

	    if (flags & BSF_FUNCTION)
	      type = STT_FUNC;

	    sym.st_info = ELF_ST_INFO (bind, type);
	  }

	sym.st_other = 0;
	bed->s->swap_symbol_out (abfd, &sym, (PTR) outbound_syms);
	outbound_syms += bed->s->sizeof_sym;
      }

    *sttp = stt;
    symstrtab_hdr->sh_size = _bfd_stringtab_size (stt);
    symstrtab_hdr->sh_type = SHT_STRTAB;

    symstrtab_hdr->sh_flags = 0;
    symstrtab_hdr->sh_addr = 0;
    symstrtab_hdr->sh_entsize = 0;
    symstrtab_hdr->sh_link = 0;
    symstrtab_hdr->sh_info = 0;
    symstrtab_hdr->sh_addralign = 1;
  }

  return true;
}

/* Return the number of bytes required to hold the symtab vector.

   Note that we base it on the count plus 1, since we will null terminate
   the vector allocated based on this size.  However, the ELF symbol table
   always has a dummy entry as symbol #0, so it ends up even.  */

long
_bfd_elf_get_symtab_upper_bound (abfd)
     bfd *abfd;
{
  long symcount;
  long symtab_size;
  Elf_Internal_Shdr *hdr = &elf_tdata (abfd)->symtab_hdr;

  symcount = hdr->sh_size / get_elf_backend_data (abfd)->s->sizeof_sym;
  symtab_size = (symcount - 1 + 1) * (sizeof (asymbol *));

  return symtab_size;
}

long
_bfd_elf_get_dynamic_symtab_upper_bound (abfd)
     bfd *abfd;
{
  long symcount;
  long symtab_size;
  Elf_Internal_Shdr *hdr = &elf_tdata (abfd)->dynsymtab_hdr;

  if (elf_dynsymtab (abfd) == 0)
    {
      bfd_set_error (bfd_error_invalid_operation);
      return -1;
    }

  symcount = hdr->sh_size / get_elf_backend_data (abfd)->s->sizeof_sym;
  symtab_size = (symcount - 1 + 1) * (sizeof (asymbol *));

  return symtab_size;
}

long
_bfd_elf_get_reloc_upper_bound (abfd, asect)
     bfd *abfd;
     sec_ptr asect;
{
  return (asect->reloc_count + 1) * sizeof (arelent *);
}

/* Canonicalize the relocs.  */

long
_bfd_elf_canonicalize_reloc (abfd, section, relptr, symbols)
     bfd *abfd;
     sec_ptr section;
     arelent **relptr;
     asymbol **symbols;
{
  arelent *tblptr;
  unsigned int i;

  if (! get_elf_backend_data (abfd)->s->slurp_reloc_table (abfd, section, symbols))
    return -1;

  tblptr = section->relocation;
  for (i = 0; i < section->reloc_count; i++)
    *relptr++ = tblptr++;

  *relptr = NULL;

  return section->reloc_count;
}

long
_bfd_elf_get_symtab (abfd, alocation)
     bfd *abfd;
     asymbol **alocation;
{
  long symcount = get_elf_backend_data (abfd)->s->slurp_symbol_table (abfd, alocation, false);

  if (symcount >= 0)
    bfd_get_symcount (abfd) = symcount;
  return symcount;
}

long
_bfd_elf_canonicalize_dynamic_symtab (abfd, alocation)
     bfd *abfd;
     asymbol **alocation;
{
  return get_elf_backend_data (abfd)->s->slurp_symbol_table (abfd, alocation, true);
}

asymbol *
_bfd_elf_make_empty_symbol (abfd)
     bfd *abfd;
{
  elf_symbol_type *newsym;

  newsym = (elf_symbol_type *) bfd_zalloc (abfd, sizeof (elf_symbol_type));
  if (!newsym)
    {
      bfd_set_error (bfd_error_no_memory);
      return NULL;
    }
  else
    {
      newsym->symbol.the_bfd = abfd;
      return &newsym->symbol;
    }
}

void
_bfd_elf_get_symbol_info (ignore_abfd, symbol, ret)
     bfd *ignore_abfd;
     asymbol *symbol;
     symbol_info *ret;
{
  bfd_symbol_info (symbol, ret);
}

alent *
_bfd_elf_get_lineno (ignore_abfd, symbol)
     bfd *ignore_abfd;
     asymbol *symbol;
{
  abort ();
  return NULL;
}

boolean
_bfd_elf_set_arch_mach (abfd, arch, machine)
     bfd *abfd;
     enum bfd_architecture arch;
     unsigned long machine;
{
  /* If this isn't the right architecture for this backend, and this
     isn't the generic backend, fail.  */
  if (arch != get_elf_backend_data (abfd)->arch
      && arch != bfd_arch_unknown
      && get_elf_backend_data (abfd)->arch != bfd_arch_unknown)
    return false;

  return bfd_default_set_arch_mach (abfd, arch, machine);
}

/* Find the nearest line to a particular section and offset, for error
   reporting.  */

boolean
_bfd_elf_find_nearest_line (abfd,
			    section,
			    symbols,
			    offset,
			    filename_ptr,
			    functionname_ptr,
			    line_ptr)
     bfd *abfd;
     asection *section;
     asymbol **symbols;
     bfd_vma offset;
     CONST char **filename_ptr;
     CONST char **functionname_ptr;
     unsigned int *line_ptr;
{
  const char *filename;
  asymbol *func;
  asymbol **p;

  if (symbols == NULL)
    return false;

  filename = NULL;
  func = NULL;

  for (p = symbols; *p != NULL; p++)
    {
      elf_symbol_type *q;

      q = (elf_symbol_type *) *p;

      if (bfd_get_section (&q->symbol) != section)
	continue;

      switch (ELF_ST_TYPE (q->internal_elf_sym.st_info))
	{
	default:
	  break;
	case STT_FILE:
	  filename = bfd_asymbol_name (&q->symbol);
	  break;
	case STT_FUNC:
	  if (func == NULL
	      || q->symbol.value <= offset)
	    func = (asymbol *) q;
	  break;
	}
    }

  if (func == NULL)
    return false;

  *filename_ptr = filename;
  *functionname_ptr = bfd_asymbol_name (func);
  *line_ptr = 0;
  return true;
}

int
_bfd_elf_sizeof_headers (abfd, reloc)
     bfd *abfd;
     boolean reloc;
{
  int ret;

  ret = get_elf_backend_data (abfd)->s->sizeof_ehdr;
  if (! reloc)
    ret += get_program_header_size (abfd, (Elf_Internal_Shdr **) NULL, 0,
				    (bfd_vma) 0);
  return ret;
}

boolean
_bfd_elf_set_section_contents (abfd, section, location, offset, count)
     bfd *abfd;
     sec_ptr section;
     PTR location;
     file_ptr offset;
     bfd_size_type count;
{
  Elf_Internal_Shdr *hdr;

  if (! abfd->output_has_begun
      && ! _bfd_elf_compute_section_file_positions (abfd,
						    (struct bfd_link_info *) NULL))
    return false;

  hdr = &elf_section_data (section)->this_hdr;

  if (bfd_seek (abfd, hdr->sh_offset + offset, SEEK_SET) == -1)
    return false;
  if (bfd_write (location, 1, count, abfd) != count)
    return false;

  return true;
}

void
_bfd_elf_no_info_to_howto (abfd, cache_ptr, dst)
     bfd *abfd;
     arelent *cache_ptr;
     Elf_Internal_Rela *dst;
{
  abort ();
}

#if 0
void
_bfd_elf_no_info_to_howto_rel (abfd, cache_ptr, dst)
     bfd *abfd;
     arelent *cache_ptr;
     Elf_Internal_Rel *dst;
{
  abort ();
}
#endif
