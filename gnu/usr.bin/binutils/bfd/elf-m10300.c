/* Matsushita 10300 specific support for 32-bit ELF
   Copyright (C) 1996, 1997, 1998, 1999 Free Software Foundation, Inc.

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
#include "libbfd.h"
#include "elf-bfd.h"
#include "elf/mn10300.h"

struct elf32_mn10300_link_hash_entry
{
  /* The basic elf link hash table entry.  */
  struct elf_link_hash_entry root;

  /* For function symbols, the number of times this function is
     called directly (ie by name).  */
  unsigned int direct_calls;

  /* For function symbols, the size of this function's stack
     (if <= 255 bytes).  We stuff this into "call" instructions
     to this target when it's valid and profitable to do so.

     This does not include stack allocated by movm!  */
  unsigned char stack_size;

  /* For function symbols, arguments (if any) for movm instruction
     in the prologue.  We stuff this value into "call" instructions
     to the target when it's valid and profitable to do so.  */
  unsigned char movm_args;

  /* For funtion symbols, the amount of stack space that would be allocated
     by the movm instruction.  This is redundant with movm_args, but we
     add it to the hash table to avoid computing it over and over.  */
  unsigned char movm_stack_size;

/* When set, convert all "call" instructions to this target into "calls"
   instructions.  */
#define MN10300_CONVERT_CALL_TO_CALLS 0x1

/* Used to mark functions which have had redundant parts of their
   prologue deleted.  */
#define MN10300_DELETED_PROLOGUE_BYTES 0x2
  unsigned char flags;
};

/* We derive a hash table from the main elf linker hash table so
   we can store state variables and a secondary hash table without
   resorting to global variables.  */
struct elf32_mn10300_link_hash_table
{
  /* The main hash table.  */
  struct elf_link_hash_table root;

  /* A hash table for static functions.  We could derive a new hash table
     instead of using the full elf32_mn10300_link_hash_table if we wanted
     to save some memory.  */
  struct elf32_mn10300_link_hash_table *static_hash_table;

  /* Random linker state flags.  */
#define MN10300_HASH_ENTRIES_INITIALIZED 0x1
  char flags;
};

/* For MN10300 linker hash table.  */

/* Get the MN10300 ELF linker hash table from a link_info structure.  */

#define elf32_mn10300_hash_table(p) \
  ((struct elf32_mn10300_link_hash_table *) ((p)->hash))

#define elf32_mn10300_link_hash_traverse(table, func, info)		\
  (elf_link_hash_traverse						\
   (&(table)->root,							\
    (boolean (*) PARAMS ((struct elf_link_hash_entry *, PTR))) (func),	\
    (info)))

static struct bfd_hash_entry *elf32_mn10300_link_hash_newfunc
  PARAMS ((struct bfd_hash_entry *, struct bfd_hash_table *, const char *));
static struct bfd_link_hash_table *elf32_mn10300_link_hash_table_create
  PARAMS ((bfd *));

static reloc_howto_type *bfd_elf32_bfd_reloc_type_lookup
  PARAMS ((bfd *abfd, bfd_reloc_code_real_type code));
static void mn10300_info_to_howto
  PARAMS ((bfd *, arelent *, Elf32_Internal_Rela *));
static boolean mn10300_elf_check_relocs
  PARAMS ((bfd *, struct bfd_link_info *, asection *,
	   const Elf_Internal_Rela *));
static asection *mn10300_elf_gc_mark_hook
  PARAMS ((bfd *, struct bfd_link_info *info, Elf_Internal_Rela *,
	   struct elf_link_hash_entry *, Elf_Internal_Sym *));
static boolean mn10300_elf_relax_delete_bytes
  PARAMS ((bfd *, asection *, bfd_vma, int));
static boolean mn10300_elf_symbol_address_p
  PARAMS ((bfd *, asection *, Elf32_External_Sym *, bfd_vma));
static boolean elf32_mn10300_finish_hash_table_entry
  PARAMS ((struct bfd_hash_entry *, PTR));
static void compute_function_info
  PARAMS ((bfd *, struct elf32_mn10300_link_hash_entry *,
	   bfd_vma, unsigned char *));

/* We have to use RELA instructions since md_apply_fix3 in the assembler
   does absolutely nothing.  */
#define USE_RELA


static reloc_howto_type elf_mn10300_howto_table[] =
{
  /* Dummy relocation.  Does nothing.  */
  HOWTO (R_MN10300_NONE,
	 0,
	 2,
	 16,
	 false,
	 0,
	 complain_overflow_bitfield,
	 bfd_elf_generic_reloc,
	 "R_MN10300_NONE",
	 false,
	 0,
	 0,
	 false),
  /* Standard 32 bit reloc.  */
  HOWTO (R_MN10300_32,
	 0,
	 2,
	 32,
	 false,
	 0,
	 complain_overflow_bitfield,
	 bfd_elf_generic_reloc,
	 "R_MN10300_32",
	 false,
	 0xffffffff,
	 0xffffffff,
	 false),
  /* Standard 16 bit reloc.  */
  HOWTO (R_MN10300_16,
	 0,
	 1,
	 16,
	 false,
	 0,
	 complain_overflow_bitfield,
	 bfd_elf_generic_reloc,
	 "R_MN10300_16",
	 false,
	 0xffff,
	 0xffff,
	 false),
  /* Standard 8 bit reloc.  */
  HOWTO (R_MN10300_8,
	 0,
	 0,
	 8,
	 false,
	 0,
	 complain_overflow_bitfield,
	 bfd_elf_generic_reloc,
	 "R_MN10300_8",
	 false,
	 0xff,
	 0xff,
	 false),
  /* Standard 32bit pc-relative reloc.  */
  HOWTO (R_MN10300_PCREL32,
	 0,
	 2,
	 32,
	 true,
	 0,
	 complain_overflow_bitfield,
	 bfd_elf_generic_reloc,
	 "R_MN10300_PCREL32",
	 false,
	 0xffffffff,
	 0xffffffff,
	 true),
  /* Standard 16bit pc-relative reloc.  */
  HOWTO (R_MN10300_PCREL16,
	 0,
	 1,
	 16,
	 true,
	 0,
	 complain_overflow_bitfield,
	 bfd_elf_generic_reloc,
	 "R_MN10300_PCREL16",
	 false,
	 0xffff,
	 0xffff,
	 true),
  /* Standard 8 pc-relative reloc.  */
  HOWTO (R_MN10300_PCREL8,
	 0,
	 0,
	 8,
	 true,
	 0,
	 complain_overflow_bitfield,
	 bfd_elf_generic_reloc,
	 "R_MN10300_PCREL8",
	 false,
	 0xff,
	 0xff,
	 true),

  /* GNU extension to record C++ vtable hierarchy */
  HOWTO (R_MN10300_GNU_VTINHERIT, /* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 NULL,			/* special_function */
	 "R_MN10300_GNU_VTINHERIT", /* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 false),		/* pcrel_offset */

  /* GNU extension to record C++ vtable member usage */
  HOWTO (R_MN10300_GNU_VTENTRY,	/* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 NULL,			/* special_function */
	 "R_MN10300_GNU_VTENTRY", /* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 false),		/* pcrel_offset */

  /* Standard 24 bit reloc.  */
  HOWTO (R_MN10300_24,
	 0,
	 2,
	 24,
	 false,
	 0,
	 complain_overflow_bitfield,
	 bfd_elf_generic_reloc,
	 "R_MN10300_24",
	 false,
	 0xffffff,
	 0xffffff,
	 false),

};

struct mn10300_reloc_map
{
  bfd_reloc_code_real_type bfd_reloc_val;
  unsigned char elf_reloc_val;
};

static const struct mn10300_reloc_map mn10300_reloc_map[] =
{
  { BFD_RELOC_NONE, R_MN10300_NONE, },
  { BFD_RELOC_32, R_MN10300_32, },
  { BFD_RELOC_16, R_MN10300_16, },
  { BFD_RELOC_8, R_MN10300_8, },
  { BFD_RELOC_32_PCREL, R_MN10300_PCREL32, },
  { BFD_RELOC_16_PCREL, R_MN10300_PCREL16, },
  { BFD_RELOC_8_PCREL, R_MN10300_PCREL8, },
  { BFD_RELOC_24, R_MN10300_24, },
  { BFD_RELOC_VTABLE_INHERIT, R_MN10300_GNU_VTINHERIT },
  { BFD_RELOC_VTABLE_ENTRY, R_MN10300_GNU_VTENTRY },
};

static reloc_howto_type *
bfd_elf32_bfd_reloc_type_lookup (abfd, code)
     bfd *abfd ATTRIBUTE_UNUSED;
     bfd_reloc_code_real_type code;
{
  unsigned int i;

  for (i = 0;
       i < sizeof (mn10300_reloc_map) / sizeof (struct mn10300_reloc_map);
       i++)
    {
      if (mn10300_reloc_map[i].bfd_reloc_val == code)
	return &elf_mn10300_howto_table[mn10300_reloc_map[i].elf_reloc_val];
    }

  return NULL;
}

/* Set the howto pointer for an MN10300 ELF reloc.  */

static void
mn10300_info_to_howto (abfd, cache_ptr, dst)
     bfd *abfd ATTRIBUTE_UNUSED;
     arelent *cache_ptr;
     Elf32_Internal_Rela *dst;
{
  unsigned int r_type;

  r_type = ELF32_R_TYPE (dst->r_info);
  BFD_ASSERT (r_type < (unsigned int) R_MN10300_MAX);
  cache_ptr->howto = &elf_mn10300_howto_table[r_type];
}

/* Look through the relocs for a section during the first phase.
   Since we don't do .gots or .plts, we just need to consider the
   virtual table relocs for gc.  */

static boolean
mn10300_elf_check_relocs (abfd, info, sec, relocs)
     bfd *abfd;
     struct bfd_link_info *info;
     asection *sec;
     const Elf_Internal_Rela *relocs;
{
  Elf_Internal_Shdr *symtab_hdr;
  struct elf_link_hash_entry **sym_hashes, **sym_hashes_end;
  const Elf_Internal_Rela *rel;
  const Elf_Internal_Rela *rel_end;

  if (info->relocateable)
    return true;

  symtab_hdr = &elf_tdata (abfd)->symtab_hdr;
  sym_hashes = elf_sym_hashes (abfd);
  sym_hashes_end = sym_hashes + symtab_hdr->sh_size/sizeof(Elf32_External_Sym);
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
	case R_MN10300_GNU_VTINHERIT:
	  if (!_bfd_elf32_gc_record_vtinherit (abfd, sec, h, rel->r_offset))
	    return false;
	  break;

	/* This relocation describes which C++ vtable entries are actually
	   used.  Record for later use during GC.  */
	case R_MN10300_GNU_VTENTRY:
	  if (!_bfd_elf32_gc_record_vtentry (abfd, sec, h, rel->r_addend))
	    return false;
	  break;
	}
    }

  return true;
}

/* Return the section that should be marked against GC for a given
   relocation.  */

static asection *
mn10300_elf_gc_mark_hook (abfd, info, rel, h, sym)
     bfd *abfd;
     struct bfd_link_info *info ATTRIBUTE_UNUSED;
     Elf_Internal_Rela *rel;
     struct elf_link_hash_entry *h;
     Elf_Internal_Sym *sym;
{
  if (h != NULL)
    {
      switch (ELF32_R_TYPE (rel->r_info))
	{
	case R_MN10300_GNU_VTINHERIT:
	case R_MN10300_GNU_VTENTRY:
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
    {
      if (!(elf_bad_symtab (abfd)
	    && ELF_ST_BIND (sym->st_info) != STB_LOCAL)
	  && ! ((sym->st_shndx <= 0 || sym->st_shndx >= SHN_LORESERVE)
		&& sym->st_shndx != SHN_COMMON))
	{
	  return bfd_section_from_elf_index (abfd, sym->st_shndx);
	}
    }

  return NULL;
}

/* Perform a relocation as part of a final link.  */
static bfd_reloc_status_type
mn10300_elf_final_link_relocate (howto, input_bfd, output_bfd,
				 input_section, contents, offset, value,
				 addend, info, sym_sec, is_local)
     reloc_howto_type *howto;
     bfd *input_bfd;
     bfd *output_bfd ATTRIBUTE_UNUSED;
     asection *input_section;
     bfd_byte *contents;
     bfd_vma offset;
     bfd_vma value;
     bfd_vma addend;
     struct bfd_link_info *info ATTRIBUTE_UNUSED;
     asection *sym_sec ATTRIBUTE_UNUSED;
     int is_local ATTRIBUTE_UNUSED;
{
  unsigned long r_type = howto->type;
  bfd_byte *hit_data = contents + offset;

  switch (r_type)
    {
    case R_MN10300_NONE:
      return bfd_reloc_ok;

    case R_MN10300_32:
      value += addend;
      bfd_put_32 (input_bfd, value, hit_data);
      return bfd_reloc_ok;

    case R_MN10300_24:
      value += addend;

      if ((long)value > 0x7fffff || (long)value < -0x800000)
	return bfd_reloc_overflow;

      bfd_put_8 (input_bfd, value & 0xff, hit_data);
      bfd_put_8 (input_bfd, (value >> 8) & 0xff, hit_data + 1);
      bfd_put_8 (input_bfd, (value >> 16) & 0xff, hit_data + 2);
      return bfd_reloc_ok;

    case R_MN10300_16:
      value += addend;

      if ((long)value > 0x7fff || (long)value < -0x8000)
	return bfd_reloc_overflow;

      bfd_put_16 (input_bfd, value, hit_data);
      return bfd_reloc_ok;

    case R_MN10300_8:
      value += addend;

      if ((long)value > 0x7f || (long)value < -0x80)
	return bfd_reloc_overflow;

      bfd_put_8 (input_bfd, value, hit_data);
      return bfd_reloc_ok;

    case R_MN10300_PCREL8:
      value -= (input_section->output_section->vma
		+ input_section->output_offset);
      value -= offset;
      value += addend;

      if ((long)value > 0xff || (long)value < -0x100)
	return bfd_reloc_overflow;

      bfd_put_8 (input_bfd, value, hit_data);
      return bfd_reloc_ok;

    case R_MN10300_PCREL16:
      value -= (input_section->output_section->vma
		+ input_section->output_offset);
      value -= offset;
      value += addend;

      if ((long)value > 0xffff || (long)value < -0x10000)
	return bfd_reloc_overflow;

      bfd_put_16 (input_bfd, value, hit_data);
      return bfd_reloc_ok;

    case R_MN10300_PCREL32:
      value -= (input_section->output_section->vma
		+ input_section->output_offset);
      value -= offset;
      value += addend;

      bfd_put_32 (input_bfd, value, hit_data);
      return bfd_reloc_ok;

    case R_MN10300_GNU_VTINHERIT:
    case R_MN10300_GNU_VTENTRY:
      return bfd_reloc_ok;

    default:
      return bfd_reloc_notsupported;
    }
}


/* Relocate an MN10300 ELF section.  */
static boolean
mn10300_elf_relocate_section (output_bfd, info, input_bfd, input_section,
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
  struct elf32_mn10300_link_hash_entry **sym_hashes;
  Elf_Internal_Rela *rel, *relend;

  symtab_hdr = &elf_tdata (input_bfd)->symtab_hdr;
  sym_hashes = (struct elf32_mn10300_link_hash_entry **)
		 (elf_sym_hashes (input_bfd));

  rel = relocs;
  relend = relocs + input_section->reloc_count;
  for (; rel < relend; rel++)
    {
      int r_type;
      reloc_howto_type *howto;
      unsigned long r_symndx;
      Elf_Internal_Sym *sym;
      asection *sec;
      struct elf32_mn10300_link_hash_entry *h;
      bfd_vma relocation;
      bfd_reloc_status_type r;

      r_symndx = ELF32_R_SYM (rel->r_info);
      r_type = ELF32_R_TYPE (rel->r_info);
      howto = elf_mn10300_howto_table + r_type;

      /* Just skip the vtable gc relocs.  */
      if (r_type == R_MN10300_GNU_VTINHERIT
	  || r_type == R_MN10300_GNU_VTENTRY)
	continue;

      if (info->relocateable)
	{
	  /* This is a relocateable link.  We don't have to change
	     anything, unless the reloc is against a section symbol,
	     in which case we have to adjust according to where the
	     section symbol winds up in the output section.  */
	  if (r_symndx < symtab_hdr->sh_info)
	    {
	      sym = local_syms + r_symndx;
	      if (ELF_ST_TYPE (sym->st_info) == STT_SECTION)
		{
		  sec = local_sections[r_symndx];
		  rel->r_addend += sec->output_offset + sym->st_value;
		}
	    }

	  continue;
	}

      /* This is a final link.  */
      h = NULL;
      sym = NULL;
      sec = NULL;
      if (r_symndx < symtab_hdr->sh_info)
	{
	  sym = local_syms + r_symndx;
	  sec = local_sections[r_symndx];
	  relocation = (sec->output_section->vma
			+ sec->output_offset
			+ sym->st_value);
	}
      else
	{
	  h = sym_hashes[r_symndx - symtab_hdr->sh_info];
	  while (h->root.type == bfd_link_hash_indirect
		 || h->root.type == bfd_link_hash_warning)
	    h = (struct elf32_mn10300_link_hash_entry *) h->root.root.u.i.link;
	  if (h->root.root.type == bfd_link_hash_defined
	      || h->root.root.type == bfd_link_hash_defweak)
	    {
	      sec = h->root.root.u.def.section;
	      relocation = (h->root.root.u.def.value
			    + sec->output_section->vma
			    + sec->output_offset);
	    }
	  else if (h->root.root.type == bfd_link_hash_undefweak)
	    relocation = 0;
	  else
	    {
	      if (! ((*info->callbacks->undefined_symbol)
		     (info, h->root.root.root.string, input_bfd,
		      input_section, rel->r_offset, true)))
		return false;
	      relocation = 0;
	    }
	}

      r = mn10300_elf_final_link_relocate (howto, input_bfd, output_bfd,
					   input_section,
					   contents, rel->r_offset,
					   relocation, rel->r_addend,
					   info, sec, h == NULL);

      if (r != bfd_reloc_ok)
	{
	  const char *name;
	  const char *msg = (const char *)0;

	  if (h != NULL)
	    name = h->root.root.root.string;
	  else
	    {
	      name = (bfd_elf_string_from_elf_section
		      (input_bfd, symtab_hdr->sh_link, sym->st_name));
	      if (name == NULL || *name == '\0')
		name = bfd_section_name (input_bfd, sec);
	    }

	  switch (r)
	    {
	    case bfd_reloc_overflow:
	      if (! ((*info->callbacks->reloc_overflow)
		     (info, name, howto->name, (bfd_vma) 0,
		      input_bfd, input_section, rel->r_offset)))
		return false;
	      break;

	    case bfd_reloc_undefined:
	      if (! ((*info->callbacks->undefined_symbol)
		     (info, name, input_bfd, input_section,
		      rel->r_offset, true)))
		return false;
	      break;

	    case bfd_reloc_outofrange:
	      msg = _("internal error: out of range error");
	      goto common_error;

	    case bfd_reloc_notsupported:
	      msg = _("internal error: unsupported relocation error");
	      goto common_error;

	    case bfd_reloc_dangerous:
	      msg = _("internal error: dangerous error");
	      goto common_error;

	    default:
	      msg = _("internal error: unknown error");
	      /* fall through */

	    common_error:
	      if (!((*info->callbacks->warning)
		    (info, msg, name, input_bfd, input_section,
		     rel->r_offset)))
		return false;
	      break;
	    }
	}
    }

  return true;
}

/* Finish initializing one hash table entry.  */
static boolean
elf32_mn10300_finish_hash_table_entry (gen_entry, in_args)
     struct bfd_hash_entry *gen_entry;
     PTR in_args ATTRIBUTE_UNUSED;
{
  struct elf32_mn10300_link_hash_entry *entry;
  unsigned int byte_count = 0;

  entry = (struct elf32_mn10300_link_hash_entry *)gen_entry;

  /* If we already know we want to convert "call" to "calls" for calls
     to this symbol, then return now.  */
  if (entry->flags == MN10300_CONVERT_CALL_TO_CALLS)
    return true;

  /* If there are no named calls to this symbol, or there's nothing we
     can move from the function itself into the "call" instruction, then
     note that all "call" instructions should be converted into "calls"
     instructions and return.  */
  if (entry->direct_calls == 0
      || (entry->stack_size == 0 && entry->movm_args == 0))
    {
      /* Make a note that we should convert "call" instructions to "calls"
	 instructions for calls to this symbol.  */
      entry->flags |= MN10300_CONVERT_CALL_TO_CALLS;
      return true;
    }

  /* We may be able to move some instructions from the function itself into
     the "call" instruction.  Count how many bytes we might be able to
     eliminate in the function itself.  */

  /* A movm instruction is two bytes.  */
  if (entry->movm_args)
    byte_count += 2;

  /* Count the insn to allocate stack space too.  */
  if (entry->stack_size > 0 && entry->stack_size <= 128)
    byte_count += 3;
  else if (entry->stack_size > 0 && entry->stack_size < 256)
    byte_count += 4;

  /* If using "call" will result in larger code, then turn all
     the associated "call" instructions into "calls" instrutions.  */
  if (byte_count < entry->direct_calls)
    entry->flags |= MN10300_CONVERT_CALL_TO_CALLS;

  /* This routine never fails.  */
  return true;
}

/* This function handles relaxing for the mn10300.

   There's quite a few relaxing opportunites available on the mn10300:

	* calls:32 -> calls:16 					   2 bytes
	* call:32  -> call:16					   2 bytes

	* call:32 -> calls:32					   1 byte
	* call:16 -> calls:16					   1 byte
		* These are done anytime using "calls" would result
		in smaller code, or when necessary to preserve the
		meaning of the program.

	* call:32						   varies
	* call:16
		* In some circumstances we can move instructions
		from a function prologue into a "call" instruction.
		This is only done if the resulting code is no larger
		than the original code.


	* jmp:32 -> jmp:16					   2 bytes
	* jmp:16 -> bra:8					   1 byte

		* If the previous instruction is a conditional branch
		around the jump/bra, we may be able to reverse its condition
		and change its target to the jump's target.  The jump/bra
		can then be deleted.				   2 bytes

	* mov abs32 -> mov abs16				   1 or 2 bytes

	* Most instructions which accept imm32 can relax to imm16  1 or 2 bytes
	- Most instructions which accept imm16 can relax to imm8   1 or 2 bytes

	* Most instructions which accept d32 can relax to d16	   1 or 2 bytes
	- Most instructions which accept d16 can relax to d8	   1 or 2 bytes

	We don't handle imm16->imm8 or d16->d8 as they're very rare
	and somewhat more difficult to support.  */

static boolean
mn10300_elf_relax_section (abfd, sec, link_info, again)
     bfd *abfd;
     asection *sec;
     struct bfd_link_info *link_info;
     boolean *again;
{
  Elf_Internal_Shdr *symtab_hdr;
  Elf_Internal_Rela *internal_relocs = NULL;
  Elf_Internal_Rela *free_relocs = NULL;
  Elf_Internal_Rela *irel, *irelend;
  bfd_byte *contents = NULL;
  bfd_byte *free_contents = NULL;
  Elf32_External_Sym *extsyms = NULL;
  Elf32_External_Sym *free_extsyms = NULL;
  struct elf32_mn10300_link_hash_table *hash_table;

  /* Assume nothing changes.  */
  *again = false;

  /* We need a pointer to the mn10300 specific hash table.  */
  hash_table = elf32_mn10300_hash_table (link_info);

  /* Initialize fields in each hash table entry the first time through.  */
  if ((hash_table->flags & MN10300_HASH_ENTRIES_INITIALIZED) == 0)
    {
      bfd *input_bfd;

      /* Iterate over all the input bfds.  */
      for (input_bfd = link_info->input_bfds;
	   input_bfd != NULL;
	   input_bfd = input_bfd->link_next)
	{
	  asection *section;

	  /* We're going to need all the symbols for each bfd.  */
	  symtab_hdr = &elf_tdata (input_bfd)->symtab_hdr;

          /* Get cached copy if it exists.  */
          if (symtab_hdr->contents != NULL)
            extsyms = (Elf32_External_Sym *) symtab_hdr->contents;
          else
            {
              /* Go get them off disk.  */
              extsyms = ((Elf32_External_Sym *)
                         bfd_malloc (symtab_hdr->sh_size));
              if (extsyms == NULL)
                goto error_return;
              free_extsyms = extsyms;
              if (bfd_seek (input_bfd, symtab_hdr->sh_offset, SEEK_SET) != 0
                  || (bfd_read (extsyms, 1, symtab_hdr->sh_size, input_bfd)
                      != symtab_hdr->sh_size))
                goto error_return;
            }

	  /* Iterate over each section in this bfd.  */
	  for (section = input_bfd->sections;
	       section != NULL;
	       section = section->next)
	    {
	      struct elf32_mn10300_link_hash_entry *hash;
	      Elf_Internal_Sym *sym;
	      asection *sym_sec = NULL;
	      const char *sym_name;
	      char *new_name;

	      /* Get cached copy of section contents if it exists.  */
	      if (elf_section_data (section)->this_hdr.contents != NULL)
		contents = elf_section_data (section)->this_hdr.contents;
	      else if (section->_raw_size != 0)
		{
		  /* Go get them off disk.  */
		  contents = (bfd_byte *)bfd_malloc (section->_raw_size);
		  if (contents == NULL)
		    goto error_return;
		  free_contents = contents;

		  if (!bfd_get_section_contents (input_bfd, section,
						 contents, (file_ptr) 0,
						 section->_raw_size))
		    goto error_return;
		}
	      else
		{
		  contents = NULL;
		  free_contents = NULL;
		}

	      /* If there aren't any relocs, then there's nothing to do.  */
	      if ((section->flags & SEC_RELOC) != 0
		  && section->reloc_count != 0)
		{

		  /* Get a copy of the native relocations.  */
		  internal_relocs = (_bfd_elf32_link_read_relocs
				     (input_bfd, section, (PTR) NULL,
				      (Elf_Internal_Rela *) NULL,
				      link_info->keep_memory));
		  if (internal_relocs == NULL)
		    goto error_return;
		  if (! link_info->keep_memory)
		    free_relocs = internal_relocs;

		  /* Now examine each relocation.  */
		  irel = internal_relocs;
		  irelend = irel + section->reloc_count;
		  for (; irel < irelend; irel++)
		    {
		      long r_type;
		      unsigned long r_index;
		      unsigned char code;

		      r_type = ELF32_R_TYPE (irel->r_info);
		      r_index = ELF32_R_SYM (irel->r_info);

		      if (r_type < 0 || r_type >= (int)R_MN10300_MAX)
			goto error_return;

		      /* We need the name and hash table entry of the target
			 symbol!  */
		      hash = NULL;
		      sym = NULL;
		      sym_sec = NULL;

		      if (r_index < symtab_hdr->sh_info)
			{
			  /* A local symbol.  */
			  Elf_Internal_Sym isym;

			  bfd_elf32_swap_symbol_in (input_bfd,
						    extsyms + r_index, &isym);

			  if (isym.st_shndx == SHN_UNDEF)
			    sym_sec = bfd_und_section_ptr;
			  else if (isym.st_shndx > 0
				   && isym.st_shndx < SHN_LORESERVE)
			    sym_sec
			      = bfd_section_from_elf_index (input_bfd,
							    isym.st_shndx);
			  else if (isym.st_shndx == SHN_ABS)
			    sym_sec = bfd_abs_section_ptr;
			  else if (isym.st_shndx == SHN_COMMON)
			    sym_sec = bfd_com_section_ptr;
			  
			  sym_name = bfd_elf_string_from_elf_section (input_bfd,
							   symtab_hdr->sh_link,
							   isym.st_name);

			  /* If it isn't a function, then we don't care
			     about it.  */
			  if (r_index < symtab_hdr->sh_info
			      && ELF_ST_TYPE (isym.st_info) != STT_FUNC)
			    continue;

			  /* Tack on an ID so we can uniquely identify this
			     local symbol in the global hash table.  */
			  new_name = bfd_malloc (strlen (sym_name) + 10);
			  if (new_name == 0)
			    goto error_return;

			  sprintf (new_name, "%s_%08x", sym_name, (int)sym_sec);
			  sym_name = new_name;

			  hash = (struct elf32_mn10300_link_hash_entry *)
				   elf_link_hash_lookup (&hash_table->static_hash_table->root,
						         sym_name, true,
						         true, false);
			  free (new_name);
			}
		      else
			{
			  r_index -= symtab_hdr->sh_info;
			  hash = (struct elf32_mn10300_link_hash_entry *)
				   elf_sym_hashes (input_bfd)[r_index];
			}

		      /* If this is not a "call" instruction, then we
			 should convert "call" instructions to "calls"
			 instructions.  */
		      code = bfd_get_8 (input_bfd,
					contents + irel->r_offset - 1);
		      if (code != 0xdd && code != 0xcd)
			hash->flags |= MN10300_CONVERT_CALL_TO_CALLS;

		      /* If this is a jump/call, then bump the direct_calls
			 counter.  Else force "call" to "calls" conversions.  */
		      if (r_type == R_MN10300_PCREL32
			  || r_type == R_MN10300_PCREL16)
			hash->direct_calls++;
		      else
			hash->flags |= MN10300_CONVERT_CALL_TO_CALLS;
		    }
		}

	      /* Now look at the actual contents to get the stack size,
		 and a list of what registers were saved in the prologue
		 (ie movm_args).  */
	      if ((section->flags & SEC_CODE) != 0)
		{

		  Elf32_External_Sym *esym, *esymend;
		  int idx, shndx;

		  shndx = _bfd_elf_section_from_bfd_section (input_bfd,
							     section);


		  /* Look at each function defined in this section and
		     update info for that function.  */
		  esym = extsyms;
		  esymend = esym + symtab_hdr->sh_info;
		  for (; esym < esymend; esym++)
		    {
		      Elf_Internal_Sym isym;

		      bfd_elf32_swap_symbol_in (input_bfd, esym, &isym);
		      if (isym.st_shndx == shndx
			  && ELF_ST_TYPE (isym.st_info) == STT_FUNC)
			{
			  if (isym.st_shndx == SHN_UNDEF)
			    sym_sec = bfd_und_section_ptr;
			  else if (isym.st_shndx > 0
				   && isym.st_shndx < SHN_LORESERVE)
			    sym_sec
			      = bfd_section_from_elf_index (input_bfd,
							    isym.st_shndx);
			  else if (isym.st_shndx == SHN_ABS)
			    sym_sec = bfd_abs_section_ptr;
			  else if (isym.st_shndx == SHN_COMMON)
			    sym_sec = bfd_com_section_ptr;

			  sym_name = bfd_elf_string_from_elf_section (input_bfd,
							symtab_hdr->sh_link,
							isym.st_name);

			  /* Tack on an ID so we can uniquely identify this
			     local symbol in the global hash table.  */
			  new_name = bfd_malloc (strlen (sym_name) + 10);
			  if (new_name == 0)
			    goto error_return;

			  sprintf (new_name, "%s_%08x", sym_name, (int)sym_sec);
			  sym_name = new_name;

			  hash = (struct elf32_mn10300_link_hash_entry *)
				    elf_link_hash_lookup (&hash_table->static_hash_table->root,
						          sym_name, true,
						          true, false);
			  free (new_name);
			  compute_function_info (input_bfd, hash,
					         isym.st_value, contents);
			}
		    }

		  esym = extsyms + symtab_hdr->sh_info;
		  esymend = extsyms + (symtab_hdr->sh_size
				       / sizeof (Elf32_External_Sym));
		  for (idx = 0; esym < esymend; esym++, idx++)
		    {
		      Elf_Internal_Sym isym;

		      bfd_elf32_swap_symbol_in (input_bfd, esym, &isym);
		      hash = (struct elf32_mn10300_link_hash_entry *)
			       elf_sym_hashes (input_bfd)[idx];
		      if (isym.st_shndx == shndx
			  && ELF_ST_TYPE (isym.st_info) == STT_FUNC
			  && (hash)->root.root.u.def.section == section
			  && ((hash)->root.root.type == bfd_link_hash_defined
			      || (hash)->root.root.type == bfd_link_hash_defweak))
			compute_function_info (input_bfd, hash,
					       (hash)->root.root.u.def.value,
					       contents);
		    }
		}

	      /* Cache or free any memory we allocated for the relocs.  */
	      if (free_relocs != NULL)
		{
		  free (free_relocs);
		  free_relocs = NULL;
		}

	      /* Cache or free any memory we allocated for the contents.  */
	      if (free_contents != NULL)
		{
		  if (! link_info->keep_memory)
		    free (free_contents);
		  else
		    {
		      /* Cache the section contents for elf_link_input_bfd.  */
		      elf_section_data (section)->this_hdr.contents = contents;
		    }
		  free_contents = NULL;
		}
	    }

	  /* Cache or free any memory we allocated for the symbols.  */
	  if (free_extsyms != NULL)
	    {
	      if (! link_info->keep_memory)
		free (free_extsyms);
	      else
		{
		  /* Cache the symbols for elf_link_input_bfd.  */
		  symtab_hdr->contents = extsyms;
		}
	      free_extsyms = NULL;
	    }
	}

      /* Now iterate on each symbol in the hash table and perform
	 the final initialization steps on each.  */
      elf32_mn10300_link_hash_traverse (hash_table,
					elf32_mn10300_finish_hash_table_entry,
					NULL);
      elf32_mn10300_link_hash_traverse (hash_table->static_hash_table,
					elf32_mn10300_finish_hash_table_entry,
					NULL);

      /* All entries in the hash table are fully initialized.  */
      hash_table->flags |= MN10300_HASH_ENTRIES_INITIALIZED;

      /* Now that everything has been initialized, go through each
	 code section and delete any prologue insns which will be
	 redundant because their operations will be performed by
	 a "call" instruction.  */
      for (input_bfd = link_info->input_bfds;
	   input_bfd != NULL;
	   input_bfd = input_bfd->link_next)
	{
	  asection *section;

	  /* We're going to need all the symbols for each bfd.  */
	  symtab_hdr = &elf_tdata (input_bfd)->symtab_hdr;

          /* Get cached copy if it exists.  */
          if (symtab_hdr->contents != NULL)
            extsyms = (Elf32_External_Sym *) symtab_hdr->contents;
          else
            {
              /* Go get them off disk.  */
              extsyms = ((Elf32_External_Sym *)
                         bfd_malloc (symtab_hdr->sh_size));
              if (extsyms == NULL)
                goto error_return;
              free_extsyms = extsyms;
              if (bfd_seek (input_bfd, symtab_hdr->sh_offset, SEEK_SET) != 0
                  || (bfd_read (extsyms, 1, symtab_hdr->sh_size, input_bfd)
                      != symtab_hdr->sh_size))
                goto error_return;
            }

	  /* Walk over each section in this bfd.  */
	  for (section = input_bfd->sections;
	       section != NULL;
	       section = section->next)
	    {
	      int shndx;
	      Elf32_External_Sym *esym, *esymend;
	      int idx;

	      /* Skip non-code sections and empty sections.  */
	      if ((section->flags & SEC_CODE) == 0 || section->_raw_size == 0)
		continue;

	      if (section->reloc_count != 0)
		{
                  /* Get a copy of the native relocations.  */
                  internal_relocs = (_bfd_elf32_link_read_relocs
                                     (input_bfd, section, (PTR) NULL,
                                      (Elf_Internal_Rela *) NULL,
                                      link_info->keep_memory));
                  if (internal_relocs == NULL)
                    goto error_return;
                  if (! link_info->keep_memory)
                    free_relocs = internal_relocs;
		}

	      /* Get cached copy of section contents if it exists.  */
	      if (elf_section_data (section)->this_hdr.contents != NULL)
		contents = elf_section_data (section)->this_hdr.contents;
	      else
		{
		  /* Go get them off disk.  */
		  contents = (bfd_byte *)bfd_malloc (section->_raw_size);
		  if (contents == NULL)
		    goto error_return;
		  free_contents = contents;

		  if (!bfd_get_section_contents (input_bfd, section,
						 contents, (file_ptr) 0,
						 section->_raw_size))
		    goto error_return;
		}


	      shndx = _bfd_elf_section_from_bfd_section (input_bfd, section);

	      /* Now look for any function in this section which needs
		 insns deleted from its prologue.  */
	      esym = extsyms;
	      esymend = esym + symtab_hdr->sh_info;
	      for (; esym < esymend; esym++)
		{
		  Elf_Internal_Sym isym;
		  struct elf32_mn10300_link_hash_entry *sym_hash;
		  asection *sym_sec = NULL;
		  const char *sym_name;
		  char *new_name;

		  bfd_elf32_swap_symbol_in (input_bfd, esym, &isym);

		  if (isym.st_shndx != shndx)
		    continue;

		  if (isym.st_shndx == SHN_UNDEF)
		    sym_sec = bfd_und_section_ptr;
		  else if (isym.st_shndx > 0 && isym.st_shndx < SHN_LORESERVE)
		    sym_sec
		      = bfd_section_from_elf_index (input_bfd, isym.st_shndx);
		  else if (isym.st_shndx == SHN_ABS)
		    sym_sec = bfd_abs_section_ptr;
		  else if (isym.st_shndx == SHN_COMMON)
		    sym_sec = bfd_com_section_ptr;
		  else
		    abort ();
		  
		  sym_name = bfd_elf_string_from_elf_section (input_bfd,
							symtab_hdr->sh_link,
							isym.st_name);

		  /* Tack on an ID so we can uniquely identify this
		     local symbol in the global hash table.  */
		  new_name = bfd_malloc (strlen (sym_name) + 10);
		  if (new_name == 0)
		    goto error_return;
		  sprintf (new_name, "%s_%08x", sym_name, (int)sym_sec);
		  sym_name = new_name;

		  sym_hash = (struct elf32_mn10300_link_hash_entry *)
			    elf_link_hash_lookup (&hash_table->static_hash_table->root,
					          sym_name, false,
					          false, false);

		  free (new_name);
		  if (sym_hash == NULL)
		    continue;

		  if (! ((sym_hash)->flags & MN10300_CONVERT_CALL_TO_CALLS)
		      && ! ((sym_hash)->flags & MN10300_DELETED_PROLOGUE_BYTES))
		    {
		      int bytes = 0;

		      /* Note that we've changed things.  */
		      elf_section_data (section)->relocs = internal_relocs;
		      free_relocs = NULL;

		      elf_section_data (section)->this_hdr.contents = contents;
		      free_contents = NULL;

		      symtab_hdr->contents = (bfd_byte *)extsyms;
		      free_extsyms = NULL;

		      /* Count how many bytes we're going to delete.  */
		      if (sym_hash->movm_args)
			bytes += 2;

		      if (sym_hash->stack_size && sym_hash->stack_size <= 128)
			bytes += 3;
		      else if (sym_hash->stack_size
			       && sym_hash->stack_size < 256)
			bytes += 4;

		      /* Note that we've deleted prologue bytes for this
			 function.  */
		      sym_hash->flags |= MN10300_DELETED_PROLOGUE_BYTES;

		      /* Actually delete the bytes.  */
		      if (!mn10300_elf_relax_delete_bytes (input_bfd,
							   section,
							   isym.st_value,
							   bytes))
			goto error_return;

		      /* Something changed.  Not strictly necessary, but
			 may lead to more relaxing opportunities.  */
		      *again = true;
		    }
		}

	      /* Look for any global functions in this section which
		 need insns deleted from their prologues.  */
	      esym = extsyms + symtab_hdr->sh_info;
	      esymend = extsyms + (symtab_hdr->sh_size
				   / sizeof (Elf32_External_Sym));
	      for (idx = 0; esym < esymend; esym++, idx++)
		{
		  Elf_Internal_Sym isym;
		  struct elf32_mn10300_link_hash_entry *sym_hash;

		  bfd_elf32_swap_symbol_in (input_bfd, esym, &isym);
		  sym_hash = (struct elf32_mn10300_link_hash_entry *)
			       (elf_sym_hashes (input_bfd)[idx]);
		  if (isym.st_shndx == shndx
		      && (sym_hash)->root.root.u.def.section == section
		      && ! ((sym_hash)->flags & MN10300_CONVERT_CALL_TO_CALLS)
		      && ! ((sym_hash)->flags & MN10300_DELETED_PROLOGUE_BYTES))
		    {
		      int bytes = 0;

		      /* Note that we've changed things.  */
		      elf_section_data (section)->relocs = internal_relocs;
		      free_relocs = NULL;

		      elf_section_data (section)->this_hdr.contents = contents;
		      free_contents = NULL;

		      symtab_hdr->contents = (bfd_byte *)extsyms;
		      free_extsyms = NULL;

		      /* Count how many bytes we're going to delete.  */
		      if (sym_hash->movm_args)
			bytes += 2;

		      if (sym_hash->stack_size && sym_hash->stack_size <= 128)
			bytes += 3;
		      else if (sym_hash->stack_size
			       && sym_hash->stack_size < 256)
			bytes += 4;

		      /* Note that we've deleted prologue bytes for this
			 function.  */
		      sym_hash->flags |= MN10300_DELETED_PROLOGUE_BYTES;

		      /* Actually delete the bytes.  */
		      if (!mn10300_elf_relax_delete_bytes (input_bfd,
							   section,
							   (sym_hash)->root.root.u.def.value,
							   bytes))
			goto error_return;

		      /* Something changed.  Not strictly necessary, but
			 may lead to more relaxing opportunities.  */
		      *again = true;
		    }
		}

	      /* Cache or free any memory we allocated for the relocs.  */
	      if (free_relocs != NULL)
		{
		  free (free_relocs);
		  free_relocs = NULL;
		}

	      /* Cache or free any memory we allocated for the contents.  */
	      if (free_contents != NULL)
		{
		  if (! link_info->keep_memory)
		    free (free_contents);
		  else
		    {
		      /* Cache the section contents for elf_link_input_bfd.  */
		      elf_section_data (section)->this_hdr.contents = contents;
		    }
		  free_contents = NULL;
		}
	    }

	  /* Cache or free any memory we allocated for the symbols.  */
	  if (free_extsyms != NULL)
	    {
	      if (! link_info->keep_memory)
		free (free_extsyms);
	      else
		{
		  /* Cache the symbols for elf_link_input_bfd.  */
		  symtab_hdr->contents = extsyms;
		}
	      free_extsyms = NULL;
	    }
	}
    }


  /* (Re)initialize for the basic instruction shortening/relaxing pass.  */
  contents = NULL;
  extsyms = NULL;
  internal_relocs = NULL;
  free_relocs = NULL;
  free_contents = NULL;
  free_extsyms = NULL;

  /* We don't have to do anything for a relocateable link, if
     this section does not have relocs, or if this is not a
     code section.  */
  if (link_info->relocateable
      || (sec->flags & SEC_RELOC) == 0
      || sec->reloc_count == 0
      || (sec->flags & SEC_CODE) == 0)
    return true;

  /* If this is the first time we have been called for this section,
     initialize the cooked size.  */
  if (sec->_cooked_size == 0)
    sec->_cooked_size = sec->_raw_size;

  symtab_hdr = &elf_tdata (abfd)->symtab_hdr;

  /* Get a copy of the native relocations.  */
  internal_relocs = (_bfd_elf32_link_read_relocs
		     (abfd, sec, (PTR) NULL, (Elf_Internal_Rela *) NULL,
		      link_info->keep_memory));
  if (internal_relocs == NULL)
    goto error_return;
  if (! link_info->keep_memory)
    free_relocs = internal_relocs;

  /* Walk through them looking for relaxing opportunities.  */
  irelend = internal_relocs + sec->reloc_count;
  for (irel = internal_relocs; irel < irelend; irel++)
    {
      bfd_vma symval;
      struct elf32_mn10300_link_hash_entry *h = NULL;

      /* If this isn't something that can be relaxed, then ignore
	 this reloc.  */
      if (ELF32_R_TYPE (irel->r_info) == (int) R_MN10300_NONE
	  || ELF32_R_TYPE (irel->r_info) == (int) R_MN10300_8
	  || ELF32_R_TYPE (irel->r_info) == (int) R_MN10300_MAX)
	continue;

      /* Get the section contents if we haven't done so already.  */
      if (contents == NULL)
	{
	  /* Get cached copy if it exists.  */
	  if (elf_section_data (sec)->this_hdr.contents != NULL)
	    contents = elf_section_data (sec)->this_hdr.contents;
	  else
	    {
	      /* Go get them off disk.  */
	      contents = (bfd_byte *) bfd_malloc (sec->_raw_size);
	      if (contents == NULL)
		goto error_return;
	      free_contents = contents;

	      if (! bfd_get_section_contents (abfd, sec, contents,
					      (file_ptr) 0, sec->_raw_size))
		goto error_return;
	    }
	}

      /* Read this BFD's symbols if we haven't done so already.  */
      if (extsyms == NULL)
	{
	  /* Get cached copy if it exists.  */
	  if (symtab_hdr->contents != NULL)
	    extsyms = (Elf32_External_Sym *) symtab_hdr->contents;
	  else
	    {
	      /* Go get them off disk.  */
	      extsyms = ((Elf32_External_Sym *)
			 bfd_malloc (symtab_hdr->sh_size));
	      if (extsyms == NULL)
		goto error_return;
	      free_extsyms = extsyms;
	      if (bfd_seek (abfd, symtab_hdr->sh_offset, SEEK_SET) != 0
		  || (bfd_read (extsyms, 1, symtab_hdr->sh_size, abfd)
		      != symtab_hdr->sh_size))
		goto error_return;
	    }
	}

      /* Get the value of the symbol referred to by the reloc.  */
      if (ELF32_R_SYM (irel->r_info) < symtab_hdr->sh_info)
	{
	  Elf_Internal_Sym isym;
	  asection *sym_sec = NULL;
	  const char *sym_name;
	  char *new_name;

	  /* A local symbol.  */
	  bfd_elf32_swap_symbol_in (abfd,
				    extsyms + ELF32_R_SYM (irel->r_info),
				    &isym);

	  if (isym.st_shndx == SHN_UNDEF)
	    sym_sec = bfd_und_section_ptr;
	  else if (isym.st_shndx > 0 && isym.st_shndx < SHN_LORESERVE)
	    sym_sec = bfd_section_from_elf_index (abfd, isym.st_shndx);
	  else if (isym.st_shndx == SHN_ABS)
	    sym_sec = bfd_abs_section_ptr;
	  else if (isym.st_shndx == SHN_COMMON)
	    sym_sec = bfd_com_section_ptr;
	  else
	    abort ();
	  
	  symval = (isym.st_value
		    + sym_sec->output_section->vma
		    + sym_sec->output_offset);
	  sym_name = bfd_elf_string_from_elf_section (abfd,
						      symtab_hdr->sh_link,
						      isym.st_name);

	  /* Tack on an ID so we can uniquely identify this
	     local symbol in the global hash table.  */
	  new_name = bfd_malloc (strlen (sym_name) + 10);
	  if (new_name == 0)
	    goto error_return;
	  sprintf (new_name, "%s_%08x", sym_name, (int)sym_sec);
	  sym_name = new_name;

	  h = (struct elf32_mn10300_link_hash_entry *)
		elf_link_hash_lookup (&hash_table->static_hash_table->root,
				      sym_name, false, false, false);
	  free (new_name);
	}
      else
	{
	  unsigned long indx;

	  /* An external symbol.  */
	  indx = ELF32_R_SYM (irel->r_info) - symtab_hdr->sh_info;
	  h = (struct elf32_mn10300_link_hash_entry *)
		(elf_sym_hashes (abfd)[indx]);
	  BFD_ASSERT (h != NULL);
	  if (h->root.root.type != bfd_link_hash_defined
	      && h->root.root.type != bfd_link_hash_defweak)
	    {
	      /* This appears to be a reference to an undefined
 		symbol.  Just ignore it--it will be caught by the
 		regular reloc processing.  */
	      continue;
	    }

	  symval = (h->root.root.u.def.value
		    + h->root.root.u.def.section->output_section->vma
		    + h->root.root.u.def.section->output_offset);
	}

      /* For simplicity of coding, we are going to modify the section
	 contents, the section relocs, and the BFD symbol table.  We
	 must tell the rest of the code not to free up this
	 information.  It would be possible to instead create a table
	 of changes which have to be made, as is done in coff-mips.c;
	 that would be more work, but would require less memory when
	 the linker is run.  */

      /* Try to turn a 32bit pc-relative branch/call into a 16bit pc-relative
	 branch/call, also deal with "call" -> "calls" conversions and
	 insertion of prologue data into "call" instructions.  */
      if (ELF32_R_TYPE (irel->r_info) == (int) R_MN10300_PCREL32)
	{
	  bfd_vma value = symval;

	  /* If we've got a "call" instruction that needs to be turned
	     into a "calls" instruction, do so now.  It saves a byte.  */
	  if (h && (h->flags & MN10300_CONVERT_CALL_TO_CALLS))
	    {
	      unsigned char code;

	      /* Get the opcode.  */
	      code = bfd_get_8 (abfd, contents + irel->r_offset - 1);

	      /* Make sure we're working with a "call" instruction!  */
	      if (code == 0xdd)
		{
		  /* Note that we've changed the relocs, section contents,
		     etc.  */
		  elf_section_data (sec)->relocs = internal_relocs;
		  free_relocs = NULL;

		  elf_section_data (sec)->this_hdr.contents = contents;
		  free_contents = NULL;

		  symtab_hdr->contents = (bfd_byte *) extsyms;
		  free_extsyms = NULL;

		  /* Fix the opcode.  */
		  bfd_put_8 (abfd, 0xfc, contents + irel->r_offset - 1);
		  bfd_put_8 (abfd, 0xff, contents + irel->r_offset);

		  /* Fix irel->r_offset and irel->r_addend.  */
		  irel->r_offset += 1;
		  irel->r_addend += 1;

		  /* Delete one byte of data.  */
		  if (!mn10300_elf_relax_delete_bytes (abfd, sec,
						       irel->r_offset + 3, 1))
		    goto error_return;

		  /* That will change things, so, we should relax again.
		     Note that this is not required, and it may be slow.  */
		  *again = true;
		}
	    }
	  else if (h)
	    {
	      /* We've got a "call" instruction which needs some data
		 from target function filled in.  */
	      unsigned char code;

	      /* Get the opcode.  */
	      code = bfd_get_8 (abfd, contents + irel->r_offset - 1);

	      /* Insert data from the target function into the "call"
		 instruction if needed.  */
	      if (code == 0xdd)
		{
		  bfd_put_8 (abfd, h->movm_args, contents + irel->r_offset + 4);
		  bfd_put_8 (abfd, h->stack_size + h->movm_stack_size,
			     contents + irel->r_offset + 5);
		}
	    }

	  /* Deal with pc-relative gunk.  */
	  value -= (sec->output_section->vma + sec->output_offset);
	  value -= irel->r_offset;
	  value += irel->r_addend;

	  /* See if the value will fit in 16 bits, note the high value is
	     0x7fff + 2 as the target will be two bytes closer if we are
	     able to relax.  */
	  if ((long)value < 0x8001 && (long)value > -0x8000)
	    {
	      unsigned char code;

	      /* Get the opcode.  */
	      code = bfd_get_8 (abfd, contents + irel->r_offset - 1);

	      if (code != 0xdc && code != 0xdd && code != 0xff)
		continue;

	      /* Note that we've changed the relocs, section contents, etc.  */
	      elf_section_data (sec)->relocs = internal_relocs;
	      free_relocs = NULL;

	      elf_section_data (sec)->this_hdr.contents = contents;
	      free_contents = NULL;

	      symtab_hdr->contents = (bfd_byte *) extsyms;
	      free_extsyms = NULL;

	      /* Fix the opcode.  */
	      if (code == 0xdc)
		bfd_put_8 (abfd, 0xcc, contents + irel->r_offset - 1);
	      else if (code == 0xdd)
		bfd_put_8 (abfd, 0xcd, contents + irel->r_offset - 1);
	      else if (code == 0xff)
		bfd_put_8 (abfd, 0xfa, contents + irel->r_offset - 2);

	      /* Fix the relocation's type.  */
	      irel->r_info = ELF32_R_INFO (ELF32_R_SYM (irel->r_info),
					   R_MN10300_PCREL16);

	      /* Delete two bytes of data.  */
	      if (!mn10300_elf_relax_delete_bytes (abfd, sec,
						   irel->r_offset + 1, 2))
		goto error_return;

	      /* That will change things, so, we should relax again.
		 Note that this is not required, and it may be slow.  */
	      *again = true;
	    }
	}

      /* Try to turn a 16bit pc-relative branch into a 8bit pc-relative
	 branch.  */
      if (ELF32_R_TYPE (irel->r_info) == (int) R_MN10300_PCREL16)
	{
	  bfd_vma value = symval;

	  /* If we've got a "call" instruction that needs to be turned
	     into a "calls" instruction, do so now.  It saves a byte.  */
	  if (h && (h->flags & MN10300_CONVERT_CALL_TO_CALLS))
	    {
	      unsigned char code;

	      /* Get the opcode.  */
	      code = bfd_get_8 (abfd, contents + irel->r_offset - 1);

	      /* Make sure we're working with a "call" instruction!  */
	      if (code == 0xcd)
		{
		  /* Note that we've changed the relocs, section contents,
		     etc.  */
		  elf_section_data (sec)->relocs = internal_relocs;
		  free_relocs = NULL;

		  elf_section_data (sec)->this_hdr.contents = contents;
		  free_contents = NULL;

		  symtab_hdr->contents = (bfd_byte *) extsyms;
		  free_extsyms = NULL;

		  /* Fix the opcode.  */
		  bfd_put_8 (abfd, 0xfa, contents + irel->r_offset - 1);
		  bfd_put_8 (abfd, 0xff, contents + irel->r_offset);

		  /* Fix irel->r_offset and irel->r_addend.  */
		  irel->r_offset += 1;
		  irel->r_addend += 1;

		  /* Delete one byte of data.  */
		  if (!mn10300_elf_relax_delete_bytes (abfd, sec,
						       irel->r_offset + 1, 1))
		    goto error_return;

		  /* That will change things, so, we should relax again.
		     Note that this is not required, and it may be slow.  */
		  *again = true;
		}
	    }
	  else if (h)
	    {
	      unsigned char code;

	      /* Get the opcode.  */
	      code = bfd_get_8 (abfd, contents + irel->r_offset - 1);

	      /* Insert data from the target function into the "call"
		 instruction if needed.  */
	      if (code == 0xcd)
		{
		  bfd_put_8 (abfd, h->movm_args, contents + irel->r_offset + 2);
		  bfd_put_8 (abfd, h->stack_size + h->movm_stack_size,
			     contents + irel->r_offset + 3);
		}
	    }

	  /* Deal with pc-relative gunk.  */
	  value -= (sec->output_section->vma + sec->output_offset);
	  value -= irel->r_offset;
	  value += irel->r_addend;

	  /* See if the value will fit in 8 bits, note the high value is
	     0x7f + 1 as the target will be one bytes closer if we are
	     able to relax.  */
	  if ((long)value < 0x80 && (long)value > -0x80)
	    {
	      unsigned char code;

	      /* Get the opcode.  */
	      code = bfd_get_8 (abfd, contents + irel->r_offset - 1);

	      if (code != 0xcc)
		continue;

	      /* Note that we've changed the relocs, section contents, etc.  */
	      elf_section_data (sec)->relocs = internal_relocs;
	      free_relocs = NULL;

	      elf_section_data (sec)->this_hdr.contents = contents;
	      free_contents = NULL;

	      symtab_hdr->contents = (bfd_byte *) extsyms;
	      free_extsyms = NULL;

	      /* Fix the opcode.  */
	      bfd_put_8 (abfd, 0xca, contents + irel->r_offset - 1);

	      /* Fix the relocation's type.  */
	      irel->r_info = ELF32_R_INFO (ELF32_R_SYM (irel->r_info),
					   R_MN10300_PCREL8);

	      /* Delete one byte of data.  */
	      if (!mn10300_elf_relax_delete_bytes (abfd, sec,
						   irel->r_offset + 1, 1))
		goto error_return;

	      /* That will change things, so, we should relax again.
		 Note that this is not required, and it may be slow.  */
	      *again = true;
	    }
	}

      /* Try to eliminate an unconditional 8 bit pc-relative branch
	 which immediately follows a conditional 8 bit pc-relative
	 branch around the unconditional branch.

	    original:		new:
	    bCC lab1		bCC' lab2
	    bra lab2
	   lab1:	       lab1:


	 This happens when the bCC can't reach lab2 at assembly time,
	 but due to other relaxations it can reach at link time.  */
      if (ELF32_R_TYPE (irel->r_info) == (int) R_MN10300_PCREL8)
	{
	  Elf_Internal_Rela *nrel;
	  bfd_vma value = symval;
	  unsigned char code;

	  /* Deal with pc-relative gunk.  */
	  value -= (sec->output_section->vma + sec->output_offset);
	  value -= irel->r_offset;
	  value += irel->r_addend;

	  /* Do nothing if this reloc is the last byte in the section.  */
	  if (irel->r_offset == sec->_cooked_size)
	    continue;

	  /* See if the next instruction is an unconditional pc-relative
	     branch, more often than not this test will fail, so we
	     test it first to speed things up.  */
	  code = bfd_get_8 (abfd, contents + irel->r_offset + 1);
	  if (code != 0xca)
	    continue;

	  /* Also make sure the next relocation applies to the next
	     instruction and that it's a pc-relative 8 bit branch.  */
	  nrel = irel + 1;
	  if (nrel == irelend
	      || irel->r_offset + 2 != nrel->r_offset
	      || ELF32_R_TYPE (nrel->r_info) != (int) R_MN10300_PCREL8)
	    continue;

	  /* Make sure our destination immediately follows the
	     unconditional branch.  */
	  if (symval != (sec->output_section->vma + sec->output_offset
			 + irel->r_offset + 3))
	    continue;

	  /* Now make sure we are a conditional branch.  This may not
	     be necessary, but why take the chance.

	     Note these checks assume that R_MN10300_PCREL8 relocs
	     only occur on bCC and bCCx insns.  If they occured
	     elsewhere, we'd need to know the start of this insn
	     for this check to be accurate.  */
	  code = bfd_get_8 (abfd, contents + irel->r_offset - 1);
	  if (code != 0xc0 && code != 0xc1 && code != 0xc2
	      && code != 0xc3 && code != 0xc4 && code != 0xc5
	      && code != 0xc6 && code != 0xc7 && code != 0xc8
	      && code != 0xc9 && code != 0xe8 && code != 0xe9
	      && code != 0xea && code != 0xeb)
	    continue;

	  /* We also have to be sure there is no symbol/label
	     at the unconditional branch.  */
	  if (mn10300_elf_symbol_address_p (abfd, sec, extsyms,
					    irel->r_offset + 1))
	    continue;

	  /* Note that we've changed the relocs, section contents, etc.  */
	  elf_section_data (sec)->relocs = internal_relocs;
	  free_relocs = NULL;

	  elf_section_data (sec)->this_hdr.contents = contents;
	  free_contents = NULL;

	  symtab_hdr->contents = (bfd_byte *) extsyms;
	  free_extsyms = NULL;

	  /* Reverse the condition of the first branch.  */
	  switch (code)
	    {
	      case 0xc8:
		code = 0xc9;
		break;
	      case 0xc9:
		code = 0xc8;
		break;
	      case 0xc0:
		code = 0xc2;
		break;
	      case 0xc2:
		code = 0xc0;
		break;
	      case 0xc3:
		code = 0xc1;
		break;
	      case 0xc1:
		code = 0xc3;
		break;
	      case 0xc4:
		code = 0xc6;
		break;
	      case 0xc6:
		code = 0xc4;
		break;
	      case 0xc7:
		code = 0xc5;
		break;
	      case 0xc5:
		code = 0xc7;
		break;
	      case 0xe8:
		code = 0xe9;
		break;
	      case 0x9d:
		code = 0xe8;
		break;
	      case 0xea:
		code = 0xeb;
		break;
	      case 0xeb:
		code = 0xea;
		break;
	    }
	  bfd_put_8 (abfd, code, contents + irel->r_offset - 1);

	  /* Set the reloc type and symbol for the first branch
	     from the second branch.  */
	  irel->r_info = nrel->r_info;

	  /* Make the reloc for the second branch a null reloc.  */
	  nrel->r_info = ELF32_R_INFO (ELF32_R_SYM (nrel->r_info),
				       R_MN10300_NONE);

	  /* Delete two bytes of data.  */
	  if (!mn10300_elf_relax_delete_bytes (abfd, sec,
					       irel->r_offset + 1, 2))
	    goto error_return;

	  /* That will change things, so, we should relax again.
	     Note that this is not required, and it may be slow.  */
	  *again = true;
	}

      /* Try to turn a 24 immediate, displacement or absolute address
	 into a 8 immediate, displacement or absolute address.  */
      if (ELF32_R_TYPE (irel->r_info) == (int) R_MN10300_24)
	{
	  bfd_vma value = symval;
	  value += irel->r_addend;

	  /* See if the value will fit in 8 bits.  */
	  if ((long)value < 0x7f && (long)value > -0x80)
	    {
	      unsigned char code;

	      /* AM33 insns which have 24 operands are 6 bytes long and
		 will have 0xfd as the first byte.  */

	      /* Get the first opcode.  */
	      code = bfd_get_8 (abfd, contents + irel->r_offset - 3);

	      if (code == 0xfd)
		{
	          /* Get the second opcode.  */
	          code = bfd_get_8 (abfd, contents + irel->r_offset - 2);

		  /* We can not relax 0x6b, 0x7b, 0x8b, 0x9b as no 24bit
		     equivalent instructions exists.  */
	          if (code != 0x6b && code != 0x7b
		      && code != 0x8b && code != 0x9b
		      && ((code & 0x0f) == 0x09 || (code & 0x0f) == 0x08
			  || (code & 0x0f) == 0x0a || (code & 0x0f) == 0x0b
			  || (code & 0x0f) == 0x0e))
		    {
		      /* Not safe if the high bit is on as relaxing may
		         move the value out of high mem and thus not fit
		         in a signed 8bit value.  This is currently over
		         conservative.  */
		      if ((value & 0x80) == 0)
			{
			  /* Note that we've changed the relocation contents,
			     etc.  */
			  elf_section_data (sec)->relocs = internal_relocs;
			  free_relocs = NULL;

			  elf_section_data (sec)->this_hdr.contents = contents;
			  free_contents = NULL;

			  symtab_hdr->contents = (bfd_byte *) extsyms;
			  free_extsyms = NULL;

			  /* Fix the opcode.  */
			  bfd_put_8 (abfd, 0xfb, contents + irel->r_offset - 3);
			  bfd_put_8 (abfd, code, contents + irel->r_offset - 2);

			  /* Fix the relocation's type.  */
			  irel->r_info
			    = ELF32_R_INFO (ELF32_R_SYM (irel->r_info),
						         R_MN10300_8);

			  /* Delete two bytes of data.  */
			  if (!mn10300_elf_relax_delete_bytes (abfd, sec,
							       irel->r_offset + 1, 2))
			    goto error_return;

			  /* That will change things, so, we should relax
			     again.  Note that this is not required, and it
			      may be slow.  */
			  *again = true;
			  break;
			}
		    }

		}
	    }
	}

      /* Try to turn a 32bit immediate, displacement or absolute address
	 into a 16bit immediate, displacement or absolute address.  */
      if (ELF32_R_TYPE (irel->r_info) == (int) R_MN10300_32)
	{
	  bfd_vma value = symval;
	  value += irel->r_addend;

	  /* See if the value will fit in 24 bits.
	     We allow any 16bit match here.  We prune those we can't
	     handle below.  */
	  if ((long)value < 0x7fffff && (long)value > -0x800000)
	    {
	      unsigned char code;

	      /* AM33 insns which have 32bit operands are 7 bytes long and
		 will have 0xfe as the first byte.  */

	      /* Get the first opcode.  */
	      code = bfd_get_8 (abfd, contents + irel->r_offset - 3);

	      if (code == 0xfe)
		{
	          /* Get the second opcode.  */
	          code = bfd_get_8 (abfd, contents + irel->r_offset - 2);

		  /* All the am33 32 -> 24 relaxing possibilities.  */
		  /* We can not relax 0x6b, 0x7b, 0x8b, 0x9b as no 24bit
		     equivalent instructions exists.  */
	          if (code != 0x6b && code != 0x7b
		      && code != 0x8b && code != 0x9b
		      && ((code & 0x0f) == 0x09 || (code & 0x0f) == 0x08
			  || (code & 0x0f) == 0x0a || (code & 0x0f) == 0x0b
			  || (code & 0x0f) == 0x0e))
		    {
		      /* Not safe if the high bit is on as relaxing may
		         move the value out of high mem and thus not fit
		         in a signed 16bit value.  This is currently over
		         conservative.  */
		      if ((value & 0x8000) == 0)
			{
			  /* Note that we've changed the relocation contents,
			     etc.  */
			  elf_section_data (sec)->relocs = internal_relocs;
			  free_relocs = NULL;

			  elf_section_data (sec)->this_hdr.contents = contents;
			  free_contents = NULL;

			  symtab_hdr->contents = (bfd_byte *) extsyms;
			  free_extsyms = NULL;

			  /* Fix the opcode.  */
			  bfd_put_8 (abfd, 0xfd, contents + irel->r_offset - 3);
			  bfd_put_8 (abfd, code, contents + irel->r_offset - 2);

			  /* Fix the relocation's type.  */
			  irel->r_info
			    = ELF32_R_INFO (ELF32_R_SYM (irel->r_info),
						         R_MN10300_24);

			  /* Delete one byte of data.  */
			  if (!mn10300_elf_relax_delete_bytes (abfd, sec,
							       irel->r_offset + 3, 1))
			    goto error_return;

			  /* That will change things, so, we should relax
			     again.  Note that this is not required, and it
			      may be slow.  */
			  *again = true;
			  break;
			}
		    }

		}
	    }

	  /* See if the value will fit in 16 bits.
	     We allow any 16bit match here.  We prune those we can't
	     handle below.  */
	  if ((long)value < 0x7fff && (long)value > -0x8000)
	    {
	      unsigned char code;

	      /* Most insns which have 32bit operands are 6 bytes long;
		 exceptions are pcrel insns and bit insns.

		 We handle pcrel insns above.  We don't bother trying
		 to handle the bit insns here.

		 The first byte of the remaining insns will be 0xfc.  */

	      /* Get the first opcode.  */
	      code = bfd_get_8 (abfd, contents + irel->r_offset - 2);

	      if (code != 0xfc)
		continue;

	      /* Get the second opcode.  */
	      code = bfd_get_8 (abfd, contents + irel->r_offset - 1);

	      if ((code & 0xf0) < 0x80)
		switch (code & 0xf0)
		  {
		  /* mov (d32,am),dn   -> mov (d32,am),dn
		     mov dm,(d32,am)   -> mov dn,(d32,am)
		     mov (d32,am),an   -> mov (d32,am),an
		     mov dm,(d32,am)   -> mov dn,(d32,am)
		     movbu (d32,am),dn -> movbu (d32,am),dn
		     movbu dm,(d32,am) -> movbu dn,(d32,am)
		     movhu (d32,am),dn -> movhu (d32,am),dn
		     movhu dm,(d32,am) -> movhu dn,(d32,am) */
		  case 0x00:
		  case 0x10:
		  case 0x20:
		  case 0x30:
		  case 0x40:
		  case 0x50:
		  case 0x60:
		  case 0x70:
		    /* Not safe if the high bit is on as relaxing may
		       move the value out of high mem and thus not fit
		       in a signed 16bit value.  */
		    if (code == 0xcc
			&& (value & 0x8000))
		      continue;

		    /* Note that we've changed the relocation contents, etc.  */
		    elf_section_data (sec)->relocs = internal_relocs;
		    free_relocs = NULL;

		    elf_section_data (sec)->this_hdr.contents = contents;
		    free_contents = NULL;

		    symtab_hdr->contents = (bfd_byte *) extsyms;
		    free_extsyms = NULL;

		    /* Fix the opcode.  */
		    bfd_put_8 (abfd, 0xfa, contents + irel->r_offset - 2);
		    bfd_put_8 (abfd, code, contents + irel->r_offset - 1);

		    /* Fix the relocation's type.  */
		    irel->r_info = ELF32_R_INFO (ELF32_R_SYM (irel->r_info),
						 R_MN10300_16);

		    /* Delete two bytes of data.  */
		    if (!mn10300_elf_relax_delete_bytes (abfd, sec,
							 irel->r_offset + 2, 2))
		      goto error_return;

		    /* That will change things, so, we should relax again.
		       Note that this is not required, and it may be slow.  */
		    *again = true;
		    break;
		  }
	      else if ((code & 0xf0) == 0x80
		       || (code & 0xf0) == 0x90)
		switch (code & 0xf3)
		  {
		  /* mov dn,(abs32)   -> mov dn,(abs16)
		     movbu dn,(abs32) -> movbu dn,(abs16)
		     movhu dn,(abs32) -> movhu dn,(abs16)  */
		  case 0x81:
		  case 0x82:
		  case 0x83:
		    /* Note that we've changed the relocation contents, etc.  */
		    elf_section_data (sec)->relocs = internal_relocs;
		    free_relocs = NULL;

		    elf_section_data (sec)->this_hdr.contents = contents;
		    free_contents = NULL;

		    symtab_hdr->contents = (bfd_byte *) extsyms;
		    free_extsyms = NULL;

		    if ((code & 0xf3) == 0x81)
		      code = 0x01 + (code & 0x0c);
		    else if ((code & 0xf3) == 0x82)
		      code = 0x02 + (code & 0x0c);
		    else if ((code & 0xf3) == 0x83)
		      code = 0x03 + (code & 0x0c);
		    else
		      abort ();

		    /* Fix the opcode.  */
		    bfd_put_8 (abfd, code, contents + irel->r_offset - 2);

		    /* Fix the relocation's type.  */
		    irel->r_info = ELF32_R_INFO (ELF32_R_SYM (irel->r_info),
						 R_MN10300_16);

		    /* The opcode got shorter too, so we have to fix the
		       addend and offset too!  */
		    irel->r_offset -= 1;

		    /* Delete three bytes of data.  */
		    if (!mn10300_elf_relax_delete_bytes (abfd, sec,
							 irel->r_offset + 1, 3))
		      goto error_return;

		    /* That will change things, so, we should relax again.
		       Note that this is not required, and it may be slow.  */
		    *again = true;
		    break;

		  /* mov am,(abs32)    -> mov am,(abs16)
		     mov am,(d32,sp)   -> mov am,(d16,sp)
		     mov dm,(d32,sp)   -> mov dm,(d32,sp)
		     movbu dm,(d32,sp) -> movbu dm,(d32,sp)
		     movhu dm,(d32,sp) -> movhu dm,(d32,sp) */
		  case 0x80:
		  case 0x90:
		  case 0x91:
		  case 0x92:
		  case 0x93:
		    /* Note that we've changed the relocation contents, etc.  */
		    elf_section_data (sec)->relocs = internal_relocs;
		    free_relocs = NULL;

		    elf_section_data (sec)->this_hdr.contents = contents;
		    free_contents = NULL;

		    symtab_hdr->contents = (bfd_byte *) extsyms;
		    free_extsyms = NULL;

		    /* Fix the opcode.  */
		    bfd_put_8 (abfd, 0xfa, contents + irel->r_offset - 2);
		    bfd_put_8 (abfd, code, contents + irel->r_offset - 1);

		    /* Fix the relocation's type.  */
		    irel->r_info = ELF32_R_INFO (ELF32_R_SYM (irel->r_info),
						 R_MN10300_16);

		    /* Delete two bytes of data.  */
		    if (!mn10300_elf_relax_delete_bytes (abfd, sec,
							 irel->r_offset + 2, 2))
		      goto error_return;

		    /* That will change things, so, we should relax again.
		       Note that this is not required, and it may be slow.  */
		    *again = true;
		    break;
		  }
	      else if ((code & 0xf0) < 0xf0)
		switch (code & 0xfc)
		  {
		  /* mov imm32,dn     -> mov imm16,dn
		     mov imm32,an     -> mov imm16,an
		     mov (abs32),dn   -> mov (abs16),dn
		     movbu (abs32),dn -> movbu (abs16),dn
		     movhu (abs32),dn -> movhu (abs16),dn  */
		  case 0xcc:
		  case 0xdc:
		  case 0xa4:
		  case 0xa8:
		  case 0xac:
		    /* Not safe if the high bit is on as relaxing may
		       move the value out of high mem and thus not fit
		       in a signed 16bit value.  */
		    if (code == 0xcc
			&& (value & 0x8000))
		      continue;

		    /* Note that we've changed the relocation contents, etc.  */
		    elf_section_data (sec)->relocs = internal_relocs;
		    free_relocs = NULL;

		    elf_section_data (sec)->this_hdr.contents = contents;
		    free_contents = NULL;

		    symtab_hdr->contents = (bfd_byte *) extsyms;
		    free_extsyms = NULL;

		    if ((code & 0xfc) == 0xcc)
		      code = 0x2c + (code & 0x03);
		    else if ((code & 0xfc) == 0xdc)
		      code = 0x24 + (code & 0x03);
		    else if ((code & 0xfc) == 0xa4)
		      code = 0x30 + (code & 0x03);
		    else if ((code & 0xfc) == 0xa8)
		      code = 0x34 + (code & 0x03);
		    else if ((code & 0xfc) == 0xac)
		      code = 0x38 + (code & 0x03);
		    else
		      abort ();

		    /* Fix the opcode.  */
		    bfd_put_8 (abfd, code, contents + irel->r_offset - 2);

		    /* Fix the relocation's type.  */
		    irel->r_info = ELF32_R_INFO (ELF32_R_SYM (irel->r_info),
						 R_MN10300_16);

		    /* The opcode got shorter too, so we have to fix the
		       addend and offset too!  */
		    irel->r_offset -= 1;

		    /* Delete three bytes of data.  */
		    if (!mn10300_elf_relax_delete_bytes (abfd, sec,
							 irel->r_offset + 1, 3))
		      goto error_return;

		    /* That will change things, so, we should relax again.
		       Note that this is not required, and it may be slow.  */
		    *again = true;
		    break;

		  /* mov (abs32),an    -> mov (abs16),an
		     mov (d32,sp),an   -> mov (d32,sp),an
		     mov (d32,sp),dn   -> mov (d32,sp),dn
		     movbu (d32,sp),dn -> movbu (d32,sp),dn
		     movhu (d32,sp),dn -> movhu (d32,sp),dn
		     add imm32,dn      -> add imm16,dn
		     cmp imm32,dn      -> cmp imm16,dn
		     add imm32,an      -> add imm16,an
		     cmp imm32,an      -> cmp imm16,an
		     and imm32,dn      -> and imm32,dn
		     or imm32,dn       -> or imm32,dn
		     xor imm32,dn      -> xor imm32,dn
		     btst imm32,dn     -> btst imm32,dn */

		  case 0xa0:
		  case 0xb0:
		  case 0xb1:
		  case 0xb2:
		  case 0xb3:
		  case 0xc0:
		  case 0xc8:

		  case 0xd0:
		  case 0xd8:
		  case 0xe0:
		  case 0xe1:
		  case 0xe2:
		  case 0xe3:
		    /* Note that we've changed the relocation contents, etc.  */
		    elf_section_data (sec)->relocs = internal_relocs;
		    free_relocs = NULL;

		    elf_section_data (sec)->this_hdr.contents = contents;
		    free_contents = NULL;

		    symtab_hdr->contents = (bfd_byte *) extsyms;
		    free_extsyms = NULL;

		    /* Fix the opcode.  */
		    bfd_put_8 (abfd, 0xfa, contents + irel->r_offset - 2);
		    bfd_put_8 (abfd, code, contents + irel->r_offset - 1);

		    /* Fix the relocation's type.  */
		    irel->r_info = ELF32_R_INFO (ELF32_R_SYM (irel->r_info),
						 R_MN10300_16);

		    /* Delete two bytes of data.  */
		    if (!mn10300_elf_relax_delete_bytes (abfd, sec,
							 irel->r_offset + 2, 2))
		      goto error_return;

		    /* That will change things, so, we should relax again.
		       Note that this is not required, and it may be slow.  */
		    *again = true;
		    break;
		  }
	      else if (code == 0xfe)
		{
		  /* add imm32,sp -> add imm16,sp  */

		  /* Note that we've changed the relocation contents, etc.  */
		  elf_section_data (sec)->relocs = internal_relocs;
		  free_relocs = NULL;

		  elf_section_data (sec)->this_hdr.contents = contents;
		  free_contents = NULL;

		  symtab_hdr->contents = (bfd_byte *) extsyms;
		  free_extsyms = NULL;

		  /* Fix the opcode.  */
		  bfd_put_8 (abfd, 0xfa, contents + irel->r_offset - 2);
		  bfd_put_8 (abfd, 0xfe, contents + irel->r_offset - 1);

		  /* Fix the relocation's type.  */
		  irel->r_info = ELF32_R_INFO (ELF32_R_SYM (irel->r_info),
						 R_MN10300_16);

		  /* Delete two bytes of data.  */
		  if (!mn10300_elf_relax_delete_bytes (abfd, sec,
						       irel->r_offset + 2, 2))
		    goto error_return;

		  /* That will change things, so, we should relax again.
		     Note that this is not required, and it may be slow.  */
		  *again = true;
		  break;
		}
	    }
	}
    }

  if (free_relocs != NULL)
    {
      free (free_relocs);
      free_relocs = NULL;
    }

  if (free_contents != NULL)
    {
      if (! link_info->keep_memory)
	free (free_contents);
      else
	{
	  /* Cache the section contents for elf_link_input_bfd.  */
	  elf_section_data (sec)->this_hdr.contents = contents;
	}
      free_contents = NULL;
    }

  if (free_extsyms != NULL)
    {
      if (! link_info->keep_memory)
	free (free_extsyms);
      else
	{
	  /* Cache the symbols for elf_link_input_bfd.  */
	  symtab_hdr->contents = extsyms;
	}
      free_extsyms = NULL;
    }

  return true;

 error_return:
  if (free_relocs != NULL)
    free (free_relocs);
  if (free_contents != NULL)
    free (free_contents);
  if (free_extsyms != NULL)
    free (free_extsyms);
  return false;
}

/* Compute the stack size and movm arguments for the function
   referred to by HASH at address ADDR in section with
   contents CONTENTS, store the information in the hash table.  */
static void
compute_function_info (abfd, hash, addr, contents)
     bfd *abfd;
     struct elf32_mn10300_link_hash_entry *hash;
     bfd_vma addr;
     unsigned char *contents;
{
  unsigned char byte1, byte2;
  /* We only care about a very small subset of the possible prologue
     sequences here.  Basically we look for:

     movm [d2,d3,a2,a3],sp (optional)
     add <size>,sp (optional, and only for sizes which fit in an unsigned
		    8 bit number)

     If we find anything else, we quit.  */

  /* Look for movm [regs],sp */
  byte1 = bfd_get_8 (abfd, contents + addr);
  byte2 = bfd_get_8 (abfd, contents + addr + 1);

  if (byte1 == 0xcf)
    {
      hash->movm_args = byte2;
      addr += 2;
      byte1 = bfd_get_8 (abfd, contents + addr);
      byte2 = bfd_get_8 (abfd, contents + addr + 1);
    }

  /* Now figure out how much stack space will be allocated by the movm
     instruction.  We need this kept separate from the funtion's normal
     stack space.  */
  if (hash->movm_args)
    {
      /* Space for d2.  */
      if (hash->movm_args & 0x80)
	hash->movm_stack_size += 4;

      /* Space for d3.  */
      if (hash->movm_args & 0x40)
	hash->movm_stack_size += 4;

      /* Space for a2.  */
      if (hash->movm_args & 0x20)
	hash->movm_stack_size += 4;

      /* Space for a3.  */
      if (hash->movm_args & 0x10)
	hash->movm_stack_size += 4;

      /* "other" space.  d0, d1, a0, a1, mdr, lir, lar, 4 byte pad.  */
      if (hash->movm_args & 0x08)
	hash->movm_stack_size += 8 * 4;

      if (bfd_get_mach (abfd) == bfd_mach_am33)
	{
	  /* "exother" space.  e0, e1, mdrq, mcrh, mcrl, mcvf */
	  if (hash->movm_args & 0x1)
	    hash->movm_stack_size += 6 * 4;

	  /* exreg1 space.  e4, e5, e6, e7 */
	  if (hash->movm_args & 0x2)
	    hash->movm_stack_size += 4 * 4;

	  /* exreg0 space.  e2, e3  */
	  if (hash->movm_args & 0x4)
	    hash->movm_stack_size += 2 * 4;
	}
    }

  /* Now look for the two stack adjustment variants.  */
  if (byte1 == 0xf8 && byte2 == 0xfe)
    {
      int temp = bfd_get_8 (abfd, contents + addr + 2);
      temp = ((temp & 0xff) ^ (~0x7f)) + 0x80;

      hash->stack_size = -temp;
    }
  else if (byte1 == 0xfa && byte2 == 0xfe)
    {
      int temp = bfd_get_16 (abfd, contents + addr + 2);
      temp = ((temp & 0xffff) ^ (~0x7fff)) + 0x8000;
      temp = -temp;

      if (temp < 255)
	hash->stack_size = temp;
    }

  /* If the total stack to be allocated by the call instruction is more
     than 255 bytes, then we can't remove the stack adjustment by using
     "call" (we might still be able to remove the "movm" instruction.  */
  if (hash->stack_size + hash->movm_stack_size > 255)
    hash->stack_size = 0;

  return;
}

/* Delete some bytes from a section while relaxing.  */

static boolean
mn10300_elf_relax_delete_bytes (abfd, sec, addr, count)
     bfd *abfd;
     asection *sec;
     bfd_vma addr;
     int count;
{
  Elf_Internal_Shdr *symtab_hdr;
  Elf32_External_Sym *extsyms;
  int shndx, index;
  bfd_byte *contents;
  Elf_Internal_Rela *irel, *irelend;
  Elf_Internal_Rela *irelalign;
  bfd_vma toaddr;
  Elf32_External_Sym *esym, *esymend;
  struct elf32_mn10300_link_hash_entry *sym_hash;

  symtab_hdr = &elf_tdata (abfd)->symtab_hdr;
  extsyms = (Elf32_External_Sym *) symtab_hdr->contents;

  shndx = _bfd_elf_section_from_bfd_section (abfd, sec);

  contents = elf_section_data (sec)->this_hdr.contents;

  /* The deletion must stop at the next ALIGN reloc for an aligment
     power larger than the number of bytes we are deleting.  */

  irelalign = NULL;
  toaddr = sec->_cooked_size;

  irel = elf_section_data (sec)->relocs;
  irelend = irel + sec->reloc_count;

  /* Actually delete the bytes.  */
  memmove (contents + addr, contents + addr + count, toaddr - addr - count);
  sec->_cooked_size -= count;

  /* Adjust all the relocs.  */
  for (irel = elf_section_data (sec)->relocs; irel < irelend; irel++)
    {
      /* Get the new reloc address.  */
      if ((irel->r_offset > addr
	   && irel->r_offset < toaddr))
	irel->r_offset -= count;
    }

  /* Adjust the local symbols defined in this section.  */
  esym = extsyms;
  esymend = esym + symtab_hdr->sh_info;
  for (; esym < esymend; esym++)
    {
      Elf_Internal_Sym isym;

      bfd_elf32_swap_symbol_in (abfd, esym, &isym);

      if (isym.st_shndx == shndx
	  && isym.st_value > addr
	  && isym.st_value < toaddr)
	{
	  isym.st_value -= count;
	  bfd_elf32_swap_symbol_out (abfd, &isym, esym);
	}
    }

  /* Now adjust the global symbols defined in this section.  */
  esym = extsyms + symtab_hdr->sh_info;
  esymend = extsyms + (symtab_hdr->sh_size / sizeof (Elf32_External_Sym));
  for (index = 0; esym < esymend; esym++, index++)
    {
      Elf_Internal_Sym isym;

      bfd_elf32_swap_symbol_in (abfd, esym, &isym);
      sym_hash = (struct elf32_mn10300_link_hash_entry *)
		   (elf_sym_hashes (abfd)[index]);
      if (isym.st_shndx == shndx
	  && ((sym_hash)->root.root.type == bfd_link_hash_defined
	      || (sym_hash)->root.root.type == bfd_link_hash_defweak)
	  && (sym_hash)->root.root.u.def.section == sec
	  && (sym_hash)->root.root.u.def.value > addr
	  && (sym_hash)->root.root.u.def.value < toaddr)
	{
	  (sym_hash)->root.root.u.def.value -= count;
	}
    }

  return true;
}

/* Return true if a symbol exists at the given address, else return
   false.  */
static boolean
mn10300_elf_symbol_address_p (abfd, sec, extsyms, addr)
     bfd *abfd;
     asection *sec;
     Elf32_External_Sym *extsyms;
     bfd_vma addr;
{
  Elf_Internal_Shdr *symtab_hdr;
  int shndx;
  Elf32_External_Sym *esym, *esymend;
  struct elf32_mn10300_link_hash_entry **sym_hash, **sym_hash_end;

  symtab_hdr = &elf_tdata (abfd)->symtab_hdr;
  shndx = _bfd_elf_section_from_bfd_section (abfd, sec);

  /* Examine all the symbols.  */
  esym = extsyms;
  esymend = esym + symtab_hdr->sh_info;
  for (; esym < esymend; esym++)
    {
      Elf_Internal_Sym isym;

      bfd_elf32_swap_symbol_in (abfd, esym, &isym);

      if (isym.st_shndx == shndx
	  && isym.st_value == addr)
	return true;
    }

  sym_hash = (struct elf32_mn10300_link_hash_entry **)(elf_sym_hashes (abfd));
  sym_hash_end = (sym_hash
		  + (symtab_hdr->sh_size / sizeof (Elf32_External_Sym)
		     - symtab_hdr->sh_info));
  for (; sym_hash < sym_hash_end; sym_hash++)
    {
      if (((*sym_hash)->root.root.type == bfd_link_hash_defined
	   || (*sym_hash)->root.root.type == bfd_link_hash_defweak)
	  && (*sym_hash)->root.root.u.def.section == sec
	  && (*sym_hash)->root.root.u.def.value == addr)
	return true;
    }
  return false;
}

/* This is a version of bfd_generic_get_relocated_section_contents
   which uses mn10300_elf_relocate_section.  */

static bfd_byte *
mn10300_elf_get_relocated_section_contents (output_bfd, link_info, link_order,
					    data, relocateable, symbols)
     bfd *output_bfd;
     struct bfd_link_info *link_info;
     struct bfd_link_order *link_order;
     bfd_byte *data;
     boolean relocateable;
     asymbol **symbols;
{
  Elf_Internal_Shdr *symtab_hdr;
  asection *input_section = link_order->u.indirect.section;
  bfd *input_bfd = input_section->owner;
  asection **sections = NULL;
  Elf_Internal_Rela *internal_relocs = NULL;
  Elf32_External_Sym *external_syms = NULL;
  Elf_Internal_Sym *internal_syms = NULL;

  /* We only need to handle the case of relaxing, or of having a
     particular set of section contents, specially.  */
  if (relocateable
      || elf_section_data (input_section)->this_hdr.contents == NULL)
    return bfd_generic_get_relocated_section_contents (output_bfd, link_info,
						       link_order, data,
						       relocateable,
						       symbols);

  symtab_hdr = &elf_tdata (input_bfd)->symtab_hdr;

  memcpy (data, elf_section_data (input_section)->this_hdr.contents,
	  input_section->_raw_size);

  if ((input_section->flags & SEC_RELOC) != 0
      && input_section->reloc_count > 0)
    {
      Elf_Internal_Sym *isymp;
      asection **secpp;
      Elf32_External_Sym *esym, *esymend;

      if (symtab_hdr->contents != NULL)
	external_syms = (Elf32_External_Sym *) symtab_hdr->contents;
      else
	{
	  external_syms = ((Elf32_External_Sym *)
			   bfd_malloc (symtab_hdr->sh_info
				       * sizeof (Elf32_External_Sym)));
	  if (external_syms == NULL && symtab_hdr->sh_info > 0)
	    goto error_return;
	  if (bfd_seek (input_bfd, symtab_hdr->sh_offset, SEEK_SET) != 0
	      || (bfd_read (external_syms, sizeof (Elf32_External_Sym),
			    symtab_hdr->sh_info, input_bfd)
		  != (symtab_hdr->sh_info * sizeof (Elf32_External_Sym))))
	    goto error_return;
	}

      internal_relocs = (_bfd_elf32_link_read_relocs
			 (input_bfd, input_section, (PTR) NULL,
			  (Elf_Internal_Rela *) NULL, false));
      if (internal_relocs == NULL)
	goto error_return;

      internal_syms = ((Elf_Internal_Sym *)
		       bfd_malloc (symtab_hdr->sh_info
				   * sizeof (Elf_Internal_Sym)));
      if (internal_syms == NULL && symtab_hdr->sh_info > 0)
	goto error_return;

      sections = (asection **) bfd_malloc (symtab_hdr->sh_info
					   * sizeof (asection *));
      if (sections == NULL && symtab_hdr->sh_info > 0)
	goto error_return;

      isymp = internal_syms;
      secpp = sections;
      esym = external_syms;
      esymend = esym + symtab_hdr->sh_info;
      for (; esym < esymend; ++esym, ++isymp, ++secpp)
	{
	  asection *isec;

	  bfd_elf32_swap_symbol_in (input_bfd, esym, isymp);

	  if (isymp->st_shndx == SHN_UNDEF)
	    isec = bfd_und_section_ptr;
	  else if (isymp->st_shndx > 0 && isymp->st_shndx < SHN_LORESERVE)
	    isec = bfd_section_from_elf_index (input_bfd, isymp->st_shndx);
	  else if (isymp->st_shndx == SHN_ABS)
	    isec = bfd_abs_section_ptr;
	  else if (isymp->st_shndx == SHN_COMMON)
	    isec = bfd_com_section_ptr;
	  else
	    {
	      /* Who knows?  */
	      isec = NULL;
	    }

	  *secpp = isec;
	}

      if (! mn10300_elf_relocate_section (output_bfd, link_info, input_bfd,
				     input_section, data, internal_relocs,
				     internal_syms, sections))
	goto error_return;

      if (sections != NULL)
	free (sections);
      sections = NULL;
      if (internal_syms != NULL)
	free (internal_syms);
      internal_syms = NULL;
      if (external_syms != NULL && symtab_hdr->contents == NULL)
	free (external_syms);
      external_syms = NULL;
      if (internal_relocs != elf_section_data (input_section)->relocs)
	free (internal_relocs);
      internal_relocs = NULL;
    }

  return data;

 error_return:
  if (internal_relocs != NULL
      && internal_relocs != elf_section_data (input_section)->relocs)
    free (internal_relocs);
  if (external_syms != NULL && symtab_hdr->contents == NULL)
    free (external_syms);
  if (internal_syms != NULL)
    free (internal_syms);
  if (sections != NULL)
    free (sections);
  return NULL;
}

/* Assorted hash table functions.  */

/* Initialize an entry in the link hash table.  */

/* Create an entry in an MN10300 ELF linker hash table.  */

static struct bfd_hash_entry *
elf32_mn10300_link_hash_newfunc (entry, table, string)
     struct bfd_hash_entry *entry;
     struct bfd_hash_table *table;
     const char *string;
{
  struct elf32_mn10300_link_hash_entry *ret =
    (struct elf32_mn10300_link_hash_entry *) entry;

  /* Allocate the structure if it has not already been allocated by a
     subclass.  */
  if (ret == (struct elf32_mn10300_link_hash_entry *) NULL)
    ret = ((struct elf32_mn10300_link_hash_entry *)
	   bfd_hash_allocate (table,
			      sizeof (struct elf32_mn10300_link_hash_entry)));
  if (ret == (struct elf32_mn10300_link_hash_entry *) NULL)
    return (struct bfd_hash_entry *) ret;

  /* Call the allocation method of the superclass.  */
  ret = ((struct elf32_mn10300_link_hash_entry *)
	 _bfd_elf_link_hash_newfunc ((struct bfd_hash_entry *) ret,
				     table, string));
  if (ret != (struct elf32_mn10300_link_hash_entry *) NULL)
    {
      ret->direct_calls = 0;
      ret->stack_size = 0;
      ret->movm_stack_size = 0;
      ret->flags = 0;
      ret->movm_args = 0;
    }

  return (struct bfd_hash_entry *) ret;
}

/* Create an mn10300 ELF linker hash table.  */

static struct bfd_link_hash_table *
elf32_mn10300_link_hash_table_create (abfd)
     bfd *abfd;
{
  struct elf32_mn10300_link_hash_table *ret;

  ret = ((struct elf32_mn10300_link_hash_table *)
	 bfd_alloc (abfd, sizeof (struct elf32_mn10300_link_hash_table)));
  if (ret == (struct elf32_mn10300_link_hash_table *) NULL)
    return NULL;

  if (! _bfd_elf_link_hash_table_init (&ret->root, abfd,
				       elf32_mn10300_link_hash_newfunc))
    {
      bfd_release (abfd, ret);
      return NULL;
    }

  ret->flags = 0;
  ret->static_hash_table
    = ((struct elf32_mn10300_link_hash_table *)
       bfd_alloc (abfd, sizeof (struct elf_link_hash_table)));
  if (ret->static_hash_table == NULL)
    {
      bfd_release (abfd, ret);
      return NULL;
    }

  if (! _bfd_elf_link_hash_table_init (&ret->static_hash_table->root, abfd,
				       elf32_mn10300_link_hash_newfunc))
    {
      bfd_release (abfd, ret->static_hash_table);
      bfd_release (abfd, ret);
      return NULL;
    }
  return &ret->root.root;
}

static int
elf_mn10300_mach (flags)
     flagword flags;
{
  switch (flags & EF_MN10300_MACH)
    {
      case E_MN10300_MACH_MN10300:
      default:
        return bfd_mach_mn10300;

      case E_MN10300_MACH_AM33:
        return bfd_mach_am33;
    }
}

/* The final processing done just before writing out a MN10300 ELF object
   file.  This gets the MN10300 architecture right based on the machine
   number.  */

/*ARGSUSED*/
void
_bfd_mn10300_elf_final_write_processing (abfd, linker)
     bfd *abfd;
     boolean linker ATTRIBUTE_UNUSED;
{
  unsigned long val;

  switch (bfd_get_mach (abfd))
    {
      default:
      case bfd_mach_mn10300:
	val = E_MN10300_MACH_MN10300;
	break;

      case bfd_mach_am33:
	val = E_MN10300_MACH_AM33;
	break;
    }

  elf_elfheader (abfd)->e_flags &= ~ (EF_MN10300_MACH);
  elf_elfheader (abfd)->e_flags |= val;
}

boolean
_bfd_mn10300_elf_object_p (abfd)
     bfd *abfd;
{
  bfd_default_set_arch_mach (abfd, bfd_arch_mn10300,
                             elf_mn10300_mach (elf_elfheader (abfd)->e_flags));
  return true;
}

/* Merge backend specific data from an object file to the output
   object file when linking.  */

boolean
_bfd_mn10300_elf_merge_private_bfd_data (ibfd, obfd)
     bfd *ibfd;
     bfd *obfd;
{
  if (bfd_get_flavour (ibfd) != bfd_target_elf_flavour
      || bfd_get_flavour (obfd) != bfd_target_elf_flavour)
    return true;

  if (bfd_get_arch (obfd) == bfd_get_arch (ibfd)
      && bfd_get_mach (obfd) < bfd_get_mach (ibfd))
    {
      if (! bfd_set_arch_mach (obfd, bfd_get_arch (ibfd),
                               bfd_get_mach (ibfd)))
        return false;
    }

  return true;
}


#define TARGET_LITTLE_SYM	bfd_elf32_mn10300_vec
#define TARGET_LITTLE_NAME	"elf32-mn10300"
#define ELF_ARCH		bfd_arch_mn10300
#define ELF_MACHINE_CODE	EM_CYGNUS_MN10300
#define ELF_MAXPAGESIZE		0x1000

#define elf_info_to_howto		mn10300_info_to_howto
#define elf_info_to_howto_rel		0
#define elf_backend_can_gc_sections	1
#define elf_backend_check_relocs	mn10300_elf_check_relocs
#define elf_backend_gc_mark_hook	mn10300_elf_gc_mark_hook
#define elf_backend_relocate_section	mn10300_elf_relocate_section
#define bfd_elf32_bfd_relax_section	mn10300_elf_relax_section
#define bfd_elf32_bfd_get_relocated_section_contents \
				mn10300_elf_get_relocated_section_contents
#define bfd_elf32_bfd_link_hash_table_create \
				elf32_mn10300_link_hash_table_create

#define elf_symbol_leading_char '_'

/* So we can set bits in e_flags.  */
#define elf_backend_final_write_processing \
                                        _bfd_mn10300_elf_final_write_processing
#define elf_backend_object_p            _bfd_mn10300_elf_object_p

#define bfd_elf32_bfd_merge_private_bfd_data \
                                        _bfd_mn10300_elf_merge_private_bfd_data


#include "elf32-target.h"
