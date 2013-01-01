/* Motorola 88k series support for 32-bit ELF
   Copyright 2006 Free Software Foundation, Inc.
   Inspired from elf-m68k.c and elf-openrisc.c

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
#include "elf-bfd.h"
#include "elf/m88k.h"

static reloc_howto_type *elf_m88k_reloc_type_lookup
  PARAMS ((bfd *, bfd_reloc_code_real_type));
static void elf_m88k_info_to_howto
  PARAMS ((bfd *, arelent *, Elf_Internal_Rela *));
static bfd_boolean elf_m88k_check_relocs
  PARAMS ((bfd *, struct bfd_link_info *, asection *,
	   const Elf_Internal_Rela *));
static asection *elf_m88k_gc_mark_hook
  PARAMS ((asection *, struct bfd_link_info *, Elf_Internal_Rela *,
	   struct elf_link_hash_entry *, Elf_Internal_Sym *));
static bfd_boolean elf_m88k_gc_sweep_hook
  PARAMS ((bfd *, struct bfd_link_info *, asection *,
	   const Elf_Internal_Rela *));
static bfd_boolean elf_m88k_relocate_section
  PARAMS ((bfd *, struct bfd_link_info *, bfd *, asection *, bfd_byte *,
	   Elf_Internal_Rela *, Elf_Internal_Sym *, asection **));

static bfd_boolean elf32_m88k_set_private_flags
  PARAMS ((bfd *, flagword));
static bfd_boolean elf32_m88k_merge_private_bfd_data
  PARAMS ((bfd *, bfd *));
static bfd_boolean elf32_m88k_print_private_bfd_data
  PARAMS ((bfd *, PTR));

#define	UNHANDLED_HOWTO(C) \
  HOWTO ((C), 0, 0, 0, FALSE, 0, complain_overflow_dont, NULL, __STRING(C), \
	 FALSE, 0, 0, FALSE)
#define	UNIMPLEMENTED_HOWTO \
  HOWTO (R_88K_UNIMPLEMENTED, 0, 0, 0, FALSE, 0, complain_overflow_dont, NULL, \
	 "R_88K_UNIMPLEMENTED", FALSE, 0, 0, FALSE)

static reloc_howto_type _bfd_m88k_elf_howto_table[] = {
  HOWTO (R_88K_NONE,       0, 0, 0, FALSE,0, complain_overflow_dont,     bfd_elf_generic_reloc, "R_88K_NONE",      FALSE, 0, 0x00000000,FALSE),
  UNHANDLED_HOWTO (R_88K_COPY),
  UNHANDLED_HOWTO (R_88K_GOTP_ENT),
  UNIMPLEMENTED_HOWTO,
  HOWTO (R_88K_8,          0, 0, 8, FALSE,0, complain_overflow_bitfield, bfd_elf_generic_reloc, "R_88K_8",         FALSE, 0, 0x000000ff,FALSE),
  UNHANDLED_HOWTO (R_88K_8S),
  UNIMPLEMENTED_HOWTO,
  UNHANDLED_HOWTO (R_88K_16S),
  HOWTO (R_88K_DISP16,     2, 2,16, TRUE, 0, complain_overflow_signed,   bfd_elf_generic_reloc, "R_88K_DISP16",      FALSE, 0, 0x0000ffff,TRUE),
  UNIMPLEMENTED_HOWTO,
  HOWTO (R_88K_DISP26,     2, 2,26, TRUE, 0, complain_overflow_signed,   bfd_elf_generic_reloc, "R_88K_DISP26",      FALSE, 0, 0x03ffffff,TRUE),
  UNIMPLEMENTED_HOWTO,
  UNIMPLEMENTED_HOWTO,
  UNIMPLEMENTED_HOWTO,
  UNHANDLED_HOWTO (R_88K_PLT_DISP26),
  UNIMPLEMENTED_HOWTO,
  UNHANDLED_HOWTO (R_88K_BBASED_32),
  UNHANDLED_HOWTO (R_88K_BBASED_32UA),
  UNHANDLED_HOWTO (R_88K_BBASED_16H),
  UNHANDLED_HOWTO (R_88K_BBASED_16L),
  UNIMPLEMENTED_HOWTO,
  UNIMPLEMENTED_HOWTO,
  UNIMPLEMENTED_HOWTO,
  UNIMPLEMENTED_HOWTO,
  UNHANDLED_HOWTO (R_88K_ABDIFF_32),
  UNHANDLED_HOWTO (R_88K_ABDIFF_32UA),
  UNHANDLED_HOWTO (R_88K_ABDIFF_16H),
  UNHANDLED_HOWTO (R_88K_ABDIFF_16L),
  UNHANDLED_HOWTO (R_88K_ABDIFF_16),
  UNIMPLEMENTED_HOWTO,
  UNIMPLEMENTED_HOWTO,
  UNIMPLEMENTED_HOWTO,
  HOWTO (R_88K_32,         0, 2,32, FALSE,0, complain_overflow_bitfield, bfd_elf_generic_reloc, "R_88K_32",        FALSE, 0, 0xffffffff,FALSE),
  UNHANDLED_HOWTO (R_88K_32UA),
  HOWTO (R_88K_16H,       16, 1,16, FALSE,0, complain_overflow_dont,     bfd_elf_generic_reloc, "R_88K_16H",      FALSE, 0, 0x0000ffff,FALSE),
  HOWTO (R_88K_16L,        0, 1,16, FALSE,0, complain_overflow_dont,     bfd_elf_generic_reloc, "R_88K_16L",      FALSE, 0, 0x0000ffff,FALSE),
  HOWTO (R_88K_16,         0, 1,16, FALSE,0, complain_overflow_bitfield, bfd_elf_generic_reloc, "R_88K_16",        FALSE, 0, 0x0000ffff,FALSE),
  UNIMPLEMENTED_HOWTO,
  UNIMPLEMENTED_HOWTO,
  UNIMPLEMENTED_HOWTO,
  UNHANDLED_HOWTO (R_88K_GOT_32),
  UNHANDLED_HOWTO (R_88K_GOT_32UA),
  UNHANDLED_HOWTO (R_88K_GOT_16H),
  UNHANDLED_HOWTO (R_88K_GOT_16L),
  UNHANDLED_HOWTO (R_88K_GOT_16),
  UNIMPLEMENTED_HOWTO,
  UNIMPLEMENTED_HOWTO,
  UNIMPLEMENTED_HOWTO,
  UNHANDLED_HOWTO (R_88K_GOTP_32),
  UNHANDLED_HOWTO (R_88K_GOTP_32UA),
  UNHANDLED_HOWTO (R_88K_GOTP_16H),
  UNHANDLED_HOWTO (R_88K_GOTP_16L),
  UNHANDLED_HOWTO (R_88K_GOTP_16),
  UNIMPLEMENTED_HOWTO,
  UNIMPLEMENTED_HOWTO,
  UNIMPLEMENTED_HOWTO,
  UNHANDLED_HOWTO (R_88K_PLT_32),
  UNHANDLED_HOWTO (R_88K_PLT_32UA),
  UNHANDLED_HOWTO (R_88K_PLT_16H),
  UNHANDLED_HOWTO (R_88K_PLT_16L),
  UNHANDLED_HOWTO (R_88K_PLT_16),
  UNIMPLEMENTED_HOWTO,
  UNIMPLEMENTED_HOWTO,
  UNIMPLEMENTED_HOWTO,
  UNHANDLED_HOWTO (R_88K_ABREL_32),
  UNHANDLED_HOWTO (R_88K_ABREL_32UA),
  UNHANDLED_HOWTO (R_88K_ABREL_16H),
  UNHANDLED_HOWTO (R_88K_ABREL_16L),
  UNHANDLED_HOWTO (R_88K_ABREL_16),
  UNIMPLEMENTED_HOWTO,
  UNIMPLEMENTED_HOWTO,
  UNIMPLEMENTED_HOWTO,
  UNHANDLED_HOWTO (R_88K_GOT_ABREL_32),
  UNHANDLED_HOWTO (R_88K_GOT_ABREL_32UA),
  UNHANDLED_HOWTO (R_88K_GOT_ABREL_16H),
  UNHANDLED_HOWTO (R_88K_GOT_ABREL_16L),
  UNHANDLED_HOWTO (R_88K_GOT_ABREL_16),
  UNIMPLEMENTED_HOWTO,
  UNIMPLEMENTED_HOWTO,
  UNIMPLEMENTED_HOWTO,
  UNHANDLED_HOWTO (R_88K_GOTP_ABREL_32),
  UNHANDLED_HOWTO (R_88K_GOTP_ABREL_32UA),
  UNHANDLED_HOWTO (R_88K_GOTP_ABREL_16H),
  UNHANDLED_HOWTO (R_88K_GOTP_ABREL_16L),
  UNHANDLED_HOWTO (R_88K_GOTP_ABREL_16),
  UNIMPLEMENTED_HOWTO,
  UNIMPLEMENTED_HOWTO,
  UNIMPLEMENTED_HOWTO,
  UNHANDLED_HOWTO (R_88K_PLT_ABREL_32),
  UNHANDLED_HOWTO (R_88K_PLT_ABREL_32UA),
  UNHANDLED_HOWTO (R_88K_PLT_ABREL_16H),
  UNHANDLED_HOWTO (R_88K_PLT_ABREL_16L),
  UNHANDLED_HOWTO (R_88K_PLT_ABREL_16),
  UNIMPLEMENTED_HOWTO,
  UNIMPLEMENTED_HOWTO,
  UNIMPLEMENTED_HOWTO,
  UNHANDLED_HOWTO (R_88K_SREL_32),
  UNHANDLED_HOWTO (R_88K_SREL_32UA),
  UNHANDLED_HOWTO (R_88K_SREL_16H),
  UNHANDLED_HOWTO (R_88K_SREL_16L),
  /* GNU extension to record C++ vtable hierarchy */
  HOWTO (R_88K_GNU_VTINHERIT,0,2,0, FALSE,0, complain_overflow_dont,     NULL,                  "R_88K_GNU_VTINHERIT",FALSE,0,0,        FALSE),
  /* GNU extension to record C++ vtable member usage */
  HOWTO (R_88K_GNU_VTENTRY,0, 2, 0, FALSE,0, complain_overflow_dont,     _bfd_elf_rel_vtable_reloc_fn,"R_88K_GNU_VTENTRY",FALSE,0,0,    FALSE)
};

static void
elf_m88k_info_to_howto (abfd, cache_ptr, dst)
     bfd *abfd ATTRIBUTE_UNUSED;
     arelent *cache_ptr;
     Elf_Internal_Rela *dst;
{
  BFD_ASSERT (ELF32_R_TYPE(dst->r_info) < (unsigned int) R_88K_UNIMPLEMENTED);
  cache_ptr->howto = &_bfd_m88k_elf_howto_table[ELF32_R_TYPE(dst->r_info)];
}

struct elf_reloc_map {
  bfd_reloc_code_real_type bfd_reloc_val;
  unsigned char elf_reloc_val;
};

static const struct elf_reloc_map m88k_reloc_map[] = {
  { BFD_RELOC_NONE,           R_88K_NONE },
  { BFD_RELOC_LO16,           R_88K_16L },
  { BFD_RELOC_HI16,           R_88K_16H },
  { BFD_RELOC_18_PCREL_S2,    R_88K_DISP16 },
  { BFD_RELOC_28_PCREL_S2,    R_88K_DISP26 },
  { BFD_RELOC_8,              R_88K_8 },
  { BFD_RELOC_16,             R_88K_16 },
  { BFD_RELOC_32,             R_88K_32 },
  { BFD_RELOC_VTABLE_INHERIT, R_88K_GNU_VTINHERIT },
  { BFD_RELOC_VTABLE_ENTRY,   R_88K_GNU_VTENTRY }
};

static reloc_howto_type *
elf_m88k_reloc_type_lookup (abfd, code)
     bfd *abfd ATTRIBUTE_UNUSED;
     bfd_reloc_code_real_type code;
{
  unsigned int i;

  for (i = 0; i < sizeof (m88k_reloc_map) / sizeof (struct elf_reloc_map); i++)
    {
      if (m88k_reloc_map[i].bfd_reloc_val == code)
	return (_bfd_m88k_elf_howto_table +
		(int) m88k_reloc_map[i].elf_reloc_val);
    }

  bfd_set_error (bfd_error_bad_value);
  return NULL;
}


/* Functions for the m88k ELF linker.  */

/* Keep m88k-specific flags in the ELF header.  */
static bfd_boolean
elf32_m88k_set_private_flags (abfd, flags)
     bfd *abfd;
     flagword flags;
{
  elf_elfheader (abfd)->e_flags = flags;
  elf_flags_init (abfd) = TRUE;
  return TRUE;
}

/* Merge backend specific data from an object file to the output
   object file when linking.  */
static bfd_boolean
elf32_m88k_merge_private_bfd_data (ibfd, obfd)
     bfd *ibfd;
     bfd *obfd;
{
  flagword out_flags;
  flagword in_flags;

  if (   bfd_get_flavour (ibfd) != bfd_target_elf_flavour
      || bfd_get_flavour (obfd) != bfd_target_elf_flavour)
    return TRUE;

  in_flags  = elf_elfheader (ibfd)->e_flags;
  out_flags = elf_elfheader (obfd)->e_flags;

  if (!elf_flags_init (obfd))
    {
      elf_flags_init (obfd) = TRUE;
      elf_elfheader (obfd)->e_flags = in_flags;
    }

  return TRUE;
}

/* Display the flags field.  */
static bfd_boolean
elf32_m88k_print_private_bfd_data (abfd, ptr)
     bfd *abfd;
     PTR ptr;
{
  FILE *file = (FILE *) ptr;

  BFD_ASSERT (abfd != NULL && ptr != NULL);

  /* Print normal ELF private data.  */
  _bfd_elf_print_private_bfd_data (abfd, ptr);

  /* xgettext:c-format */
  fprintf (file, _("private flags = %lx:"), elf_elfheader (abfd)->e_flags);

  if (elf_elfheader (abfd)->e_flags & EF_NABI)
    fprintf (file, _(" [not ABI]"));

  if (elf_elfheader (abfd)->e_flags & EF_M88110)
    fprintf (file, _(" [m88110]"));

  fputc ('\n', file);

  return TRUE;
}
/* Look through the relocs for a section during the first phase, and
   allocate space in the global offset table or procedure linkage
   table.  */

static bfd_boolean
elf_m88k_check_relocs (abfd, info, sec, relocs)
     bfd *abfd;
     struct bfd_link_info *info;
     asection *sec;
     const Elf_Internal_Rela *relocs;
{
  Elf_Internal_Shdr *symtab_hdr;
  struct elf_link_hash_entry **sym_hashes, **sym_hashes_end;
  const Elf_Internal_Rela *rel;
  const Elf_Internal_Rela *rel_end;

  if (info->relocatable)
    return TRUE;

  symtab_hdr = &elf_tdata (abfd)->symtab_hdr;
  sym_hashes = elf_sym_hashes (abfd);
  sym_hashes_end =
    sym_hashes + symtab_hdr->sh_size / sizeof (Elf32_External_Sym);
  if (!elf_bad_symtab (abfd))
    sym_hashes_end -= symtab_hdr->sh_info;

  rel_end = relocs + sec->reloc_count;
  for (rel = relocs; rel < rel_end; rel++)
    {
      struct elf_link_hash_entry *h;
      unsigned long r_symndx;

      r_symndx = ELF32_R_SYM (rel->r_info);
      if (r_symndx < symtab_hdr->sh_info)
	h = NULL;
      else
	h = sym_hashes[r_symndx - symtab_hdr->sh_info];

      switch (ELF32_R_TYPE (rel->r_info))
	{
	  /* This relocation describes the C++ object vtable hierarchy.
	     Reconstruct it for later use during GC.  */
	case R_88K_GNU_VTINHERIT:
	  if (!bfd_elf_gc_record_vtinherit (abfd, sec, h, rel->r_offset))
	    return FALSE;
	  break;

	  /* This relocation describes which C++ vtable entries are actually
	     used.  Record for later use during GC.  */
	case R_88K_GNU_VTENTRY:
	  if (!bfd_elf_gc_record_vtentry (abfd, sec, h, rel->r_addend))
	    return FALSE;
	  break;

	default:
	  break;
	}
    }

  return TRUE;
}

/* Return the section that should be marked against GC for a given
   relocation.  */

static asection *
elf_m88k_gc_mark_hook (sec, info, rel, h, sym)
     asection *sec;
     struct bfd_link_info *info ATTRIBUTE_UNUSED;
     Elf_Internal_Rela *rel;
     struct elf_link_hash_entry *h;
     Elf_Internal_Sym *sym;
{
  if (h != NULL)
    {
      switch (ELF32_R_TYPE (rel->r_info))
	{
	case R_88K_GNU_VTINHERIT:
	case R_88K_GNU_VTENTRY:
	  break;

	default:
	  switch (h->root.type)
	    {
	    case bfd_link_hash_defined:
	    case bfd_link_hash_defweak:
	      return h->root.u.def.section;

	    case bfd_link_hash_common:
	      return h->root.u.c.p->section;

	    default:
	      break;
	    }
	}
    }
  else
    return bfd_section_from_elf_index (sec->owner, sym->st_shndx);

  return NULL;
}

/* Update the got entry reference counts for the section being removed.  */

static bfd_boolean
elf_m88k_gc_sweep_hook (abfd, info, sec, relocs)
     bfd *abfd ATTRIBUTE_UNUSED;
     struct bfd_link_info *info ATTRIBUTE_UNUSED;
     asection *sec ATTRIBUTE_UNUSED;
     const Elf_Internal_Rela *relocs ATTRIBUTE_UNUSED;
{
  return TRUE;
}

/* Relocate an M88K ELF section.  */

static bfd_boolean
elf_m88k_relocate_section (output_bfd, info, input_bfd, input_section,
			   contents, relocs, local_syms, local_sections)
     bfd *output_bfd;
     struct bfd_link_info *info;
     bfd *input_bfd;
     asection *input_section;
     bfd_byte *contents;
     Elf_Internal_Rela *relocs;
     Elf_Internal_Sym *local_syms;
     asection **local_sections;
{
  Elf_Internal_Shdr *symtab_hdr;
  struct elf_link_hash_entry **sym_hashes;
  Elf_Internal_Rela *rel;
  Elf_Internal_Rela *relend;

  if (info->relocatable)
    return TRUE;

  symtab_hdr = &elf_tdata (input_bfd)->symtab_hdr;
  sym_hashes = elf_sym_hashes (input_bfd);

  rel = relocs;
  relend = relocs + input_section->reloc_count;
  for (; rel < relend; rel++)
    {
      int r_type;
      reloc_howto_type *howto;
      unsigned long r_symndx;
      struct elf_link_hash_entry *h;
      Elf_Internal_Sym *sym;
      asection *sec;
      bfd_vma relocation;
      bfd_reloc_status_type r;

      r_type = ELF32_R_TYPE (rel->r_info);
      r_symndx = ELF32_R_SYM (rel->r_info);

      if (r_type == R_88K_GNU_VTINHERIT || r_type == R_88K_GNU_VTENTRY)
	continue;

      if ((unsigned int) r_type >= R_88K_UNIMPLEMENTED)
	abort ();

      howto = _bfd_m88k_elf_howto_table + r_type;
      h = NULL;
      sym = NULL;
      sec = NULL;

      if (r_symndx < symtab_hdr->sh_info)
	{
	  sym = local_syms + r_symndx;
	  sec = local_sections[r_symndx];
	  relocation = _bfd_elf_rela_local_sym (output_bfd, sym, &sec, rel);
	}
      else
	{
	  bfd_boolean unresolved_reloc, warned;

	  RELOC_FOR_GLOBAL_SYMBOL (info, input_bfd, input_section, rel,
				   r_symndx, symtab_hdr, sym_hashes,
				   h, sec, relocation,
				   unresolved_reloc, warned);
	}

      r = _bfd_final_link_relocate (howto, input_bfd, input_section,
				    contents, rel->r_offset,
				    relocation, rel->r_addend);

      if (r != bfd_reloc_ok)
	{
	  const char *name;

	  if (h != NULL)
	    name = h->root.root.string;
	  else
	    {
	      name = bfd_elf_string_from_elf_section (input_bfd,
						      symtab_hdr->sh_link,
						      sym->st_name);
	      if (name == NULL)
		return FALSE;
	      if (*name == '\0')
		name = bfd_section_name (input_bfd, sec);
	    }

	  if (r == bfd_reloc_overflow)
	    {
	      if (!(info->callbacks->reloc_overflow
		    (info, name, howto->name, (bfd_vma) 0,
		     input_bfd, input_section, rel->r_offset)))
		return FALSE;
	    }
	  else
	    {
	      (*_bfd_error_handler)
		(_("%s(%s+0x%lx): reloc against `%s': error %d"),
		 bfd_archive_filename (input_bfd),
		 bfd_get_section_name (input_bfd, input_section),
		 (long) rel->r_offset, name, (int) r);
	      return FALSE;
	    }
	}
    }

  return TRUE;
}

#define	ELF_ARCH			bfd_arch_m88k
#define ELF_MACHINE_CODE		EM_88K
#define ELF_MAXPAGESIZE			0x1000

#define TARGET_BIG_SYM			bfd_elf32_m88k_vec
#define TARGET_BIG_NAME			"elf32-m88k"

#define elf_info_to_howto		elf_m88k_info_to_howto

#define bfd_elf32_bfd_reloc_type_lookup elf_m88k_reloc_type_lookup
#define bfd_elf32_bfd_final_link	bfd_elf_gc_common_final_link

#define elf_backend_check_relocs	elf_m88k_check_relocs
#define elf_backend_relocate_section	elf_m88k_relocate_section
#define elf_backend_gc_mark_hook	elf_m88k_gc_mark_hook
#define elf_backend_gc_sweep_hook	elf_m88k_gc_sweep_hook

#define bfd_elf32_bfd_merge_private_bfd_data \
                                        elf32_m88k_merge_private_bfd_data
#define bfd_elf32_bfd_set_private_flags \
                                        elf32_m88k_set_private_flags
#define bfd_elf32_bfd_print_private_bfd_data \
                                        elf32_m88k_print_private_bfd_data

#define elf_backend_can_gc_sections	1
#define elf_backend_rela_normal		1

#include "elf32-target.h"
