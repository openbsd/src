/* Motorola MCore specific support for 32-bit ELF
   Copyright 1994, 1995, 1999, 2000, 2001 Free Software Foundation, Inc.

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

/* This file is based on a preliminary RCE ELF ABI.  The
   information may not match the final RCE ELF ABI.   */

#include "bfd.h"
#include "sysdep.h"
#include "bfdlink.h"
#include "libbfd.h"
#include "elf-bfd.h"
#include "elf/mcore.h"
#include <assert.h>

#define	USE_RELA	/* Only USE_REL is actually significant, but this is
			   here are a reminder...  */

static void mcore_elf_howto_init
  PARAMS ((void));
static reloc_howto_type * mcore_elf_reloc_type_lookup
  PARAMS ((bfd *, bfd_reloc_code_real_type));
static void mcore_elf_info_to_howto
  PARAMS ((bfd *, arelent *, Elf32_Internal_Rela *));
static boolean mcore_elf_set_private_flags
  PARAMS ((bfd *, flagword));
static boolean mcore_elf_copy_private_bfd_data
  PARAMS ((bfd *, bfd *));
static boolean mcore_elf_merge_private_bfd_data
  PARAMS ((bfd *, bfd *));
static bfd_reloc_status_type mcore_elf_unsupported_reloc
  PARAMS ((bfd *, arelent *, asymbol *, PTR, asection *, bfd *, char **));
static boolean mcore_elf_relocate_section
  PARAMS ((bfd *, struct bfd_link_info *, bfd *, asection *, bfd_byte *,
	   Elf_Internal_Rela *, Elf_Internal_Sym *, asection **));

static reloc_howto_type * mcore_elf_howto_table [(int) R_MCORE_max];

static reloc_howto_type mcore_elf_howto_raw[] =
{
  /* This reloc does nothing.  */
  HOWTO (R_MCORE_NONE,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield,  /* complain_on_overflow */
	 NULL,                  /* special_function */
	 "R_MCORE_NONE",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 false),		/* pcrel_offset */

  /* A standard 32 bit relocation.  */
  HOWTO (R_MCORE_ADDR32,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "ADDR32",		/* name *//* For compatability with coff/pe port.  */
	 false,			/* partial_inplace */
	 0x0,			/* src_mask */
	 0xffffffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* 8 bits + 2 zero bits; jmpi/jsri/lrw instructions.
     Should not appear in object files.  */
  HOWTO (R_MCORE_PCRELIMM8BY4,	/* type */
	 2,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 8,			/* bitsize */
	 true,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 mcore_elf_unsupported_reloc,	/* special_function */
	 "R_MCORE_PCRELIMM8BY4",/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 true),			/* pcrel_offset */

  /* bsr/bt/bf/br instructions; 11 bits + 1 zero bit
     Span 2k instructions == 4k bytes.
     Only useful pieces at the relocated address are the opcode (5 bits) */
  HOWTO (R_MCORE_PCRELIMM11BY2,/* type */
	 1,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 11,			/* bitsize */
	 true,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MCORE_PCRELIMM11BY2",/* name */
	 false,			/* partial_inplace */
	 0x0,			/* src_mask */
	 0x7ff,			/* dst_mask */
	 true),			/* pcrel_offset */

  /* 4 bits + 1 zero bit; 'loopt' instruction only; unsupported.  */
  HOWTO (R_MCORE_PCRELIMM4BY2,	/* type */
	 1,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 4,			/* bitsize */
	 true,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 mcore_elf_unsupported_reloc,/* special_function */
	 "R_MCORE_PCRELIMM4BY2",/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 true),			/* pcrel_offset */

  /* 32-bit pc-relative. Eventually this will help support PIC code.  */
  HOWTO (R_MCORE_PCREL32,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 true,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MCORE_PCREL32",	/* name */
	 false,			/* partial_inplace */
	 0x0,			/* src_mask */
	 0xffffffff,		/* dst_mask */
	 true),			/* pcrel_offset */

  /* Like PCRELIMM11BY2, this relocation indicates that there is a
     'jsri' at the specified address. There is a separate relocation
     entry for the literal pool entry that it references, but we
     might be able to change the jsri to a bsr if the target turns out
     to be close enough [even though we won't reclaim the literal pool
     entry, we'll get some runtime efficiency back]. Note that this
     is a relocation that we are allowed to safely ignore.  */
  HOWTO (R_MCORE_PCRELJSR_IMM11BY2,/* type */
	 1,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 11,			/* bitsize */
	 true,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MCORE_PCRELJSR_IMM11BY2", /* name */
	 false,			/* partial_inplace */
	 0x0,			/* src_mask */
	 0x7ff,			/* dst_mask */
	 true),			/* pcrel_offset */

  /* GNU extension to record C++ vtable hierarchy */
  HOWTO (R_MCORE_GNU_VTINHERIT, /* type */
         0,                     /* rightshift */
         2,                     /* size (0 = byte, 1 = short, 2 = long) */
         0,                     /* bitsize */
         false,                 /* pc_relative */
         0,                     /* bitpos */
         complain_overflow_dont, /* complain_on_overflow */
         NULL,                  /* special_function */
         "R_MCORE_GNU_VTINHERIT", /* name */
         false,                 /* partial_inplace */
         0,                     /* src_mask */
         0,                     /* dst_mask */
         false),                /* pcrel_offset */

  /* GNU extension to record C++ vtable member usage */
  HOWTO (R_MCORE_GNU_VTENTRY,   /* type */
         0,                     /* rightshift */
         2,                     /* size (0 = byte, 1 = short, 2 = long) */
         0,                     /* bitsize */
         false,                 /* pc_relative */
         0,                     /* bitpos */
         complain_overflow_dont,/* complain_on_overflow */
         _bfd_elf_rel_vtable_reloc_fn,  /* special_function */
         "R_MCORE_GNU_VTENTRY", /* name */
         false,                 /* partial_inplace */
         0,                     /* src_mask */
         0,                     /* dst_mask */
         false),                /* pcrel_offset */

  HOWTO (R_MCORE_RELATIVE,      /* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 NULL,                  /* special_function */
	 "R_MCORE_RELATIVE",    /* name */
	 true,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 false)			/* pcrel_offset */
};

#ifndef NUM_ELEM
#define NUM_ELEM(a) (sizeof (a) / sizeof (a)[0])
#endif

/* Initialize the mcore_elf_howto_table, so that linear accesses can be done.  */
static void
mcore_elf_howto_init ()
{
  unsigned int i;

  for (i = NUM_ELEM (mcore_elf_howto_raw); i--;)
    {
      unsigned int type;

      type = mcore_elf_howto_raw[i].type;

      BFD_ASSERT (type < NUM_ELEM (mcore_elf_howto_table));

      mcore_elf_howto_table [type] = & mcore_elf_howto_raw [i];
    }
}

static reloc_howto_type *
mcore_elf_reloc_type_lookup (abfd, code)
     bfd * abfd ATTRIBUTE_UNUSED;
     bfd_reloc_code_real_type code;
{
  enum elf_mcore_reloc_type mcore_reloc = R_MCORE_NONE;

  switch (code)
    {
    case BFD_RELOC_NONE:		     mcore_reloc = R_MCORE_NONE; break;
    case BFD_RELOC_32:			     mcore_reloc = R_MCORE_ADDR32; break;
    case BFD_RELOC_MCORE_PCREL_IMM8BY4:	     mcore_reloc = R_MCORE_PCRELIMM8BY4; break;
    case BFD_RELOC_MCORE_PCREL_IMM11BY2:     mcore_reloc = R_MCORE_PCRELIMM11BY2; break;
    case BFD_RELOC_MCORE_PCREL_IMM4BY2:	     mcore_reloc = R_MCORE_PCRELIMM4BY2; break;
    case BFD_RELOC_32_PCREL:		     mcore_reloc = R_MCORE_PCREL32; break;
    case BFD_RELOC_MCORE_PCREL_JSR_IMM11BY2: mcore_reloc = R_MCORE_PCRELJSR_IMM11BY2; break;
    case BFD_RELOC_VTABLE_INHERIT:           mcore_reloc = R_MCORE_GNU_VTINHERIT; break;
    case BFD_RELOC_VTABLE_ENTRY:             mcore_reloc = R_MCORE_GNU_VTENTRY; break;
    case BFD_RELOC_RVA:                      mcore_reloc = R_MCORE_RELATIVE; break;
    default:
      return (reloc_howto_type *)NULL;
    }

  if (! mcore_elf_howto_table [R_MCORE_PCRELIMM8BY4])	/* Initialize howto table if needed */
    mcore_elf_howto_init ();

  return mcore_elf_howto_table [(int) mcore_reloc];
};

/* Set the howto pointer for a RCE ELF reloc.  */
static void
mcore_elf_info_to_howto (abfd, cache_ptr, dst)
     bfd * abfd ATTRIBUTE_UNUSED;
     arelent * cache_ptr;
     Elf32_Internal_Rela * dst;
{
  if (! mcore_elf_howto_table [R_MCORE_PCRELIMM8BY4])	/* Initialize howto table if needed */
    mcore_elf_howto_init ();

  BFD_ASSERT (ELF32_R_TYPE (dst->r_info) < (unsigned int) R_MCORE_max);

  cache_ptr->howto = mcore_elf_howto_table [ELF32_R_TYPE (dst->r_info)];
}

/* Function to set whether a module needs the -mrelocatable bit set.  */
static boolean
mcore_elf_set_private_flags (abfd, flags)
     bfd * abfd;
     flagword flags;
{
  BFD_ASSERT (! elf_flags_init (abfd)
	      || elf_elfheader (abfd)->e_flags == flags);

  elf_elfheader (abfd)->e_flags = flags;
  elf_flags_init (abfd) = true;
  return true;
}

/* Copy backend specific data from one object module to another.  */
static boolean
mcore_elf_copy_private_bfd_data (ibfd, obfd)
     bfd * ibfd;
     bfd * obfd;
{
  if (   bfd_get_flavour (ibfd) != bfd_target_elf_flavour
      || bfd_get_flavour (obfd) != bfd_target_elf_flavour)
    return true;

  BFD_ASSERT (! elf_flags_init (obfd)
	      || elf_elfheader (obfd)->e_flags == elf_elfheader (ibfd)->e_flags);

  elf_elfheader (obfd)->e_flags = elf_elfheader (ibfd)->e_flags;
  elf_flags_init (obfd) = true;
  return true;
}

/* Merge backend specific data from an object file to the output
   object file when linking.  */
static boolean
mcore_elf_merge_private_bfd_data (ibfd, obfd)
     bfd * ibfd;
     bfd * obfd;
{
  flagword old_flags;
  flagword new_flags;

  /* Check if we have the same endianess */
  if (_bfd_generic_verify_endian_match (ibfd, obfd) == false)
    return false;

  if (   bfd_get_flavour (ibfd) != bfd_target_elf_flavour
      || bfd_get_flavour (obfd) != bfd_target_elf_flavour)
    return true;

  new_flags = elf_elfheader (ibfd)->e_flags;
  old_flags = elf_elfheader (obfd)->e_flags;

  if (! elf_flags_init (obfd))	/* First call, no flags set */
    {
      elf_flags_init (obfd) = true;
      elf_elfheader (obfd)->e_flags = new_flags;
    }
  else if (new_flags == old_flags)	/* Compatible flags are ok */
    ;
  else
    {
      /* FIXME */
    }

  return true;
}

/* Don't pretend we can deal with unsupported relocs.  */

static bfd_reloc_status_type
mcore_elf_unsupported_reloc (abfd, reloc_entry, symbol, data, input_section,
			   output_bfd, error_message)
     bfd * abfd;
     arelent * reloc_entry;
     asymbol * symbol ATTRIBUTE_UNUSED;
     PTR data ATTRIBUTE_UNUSED;
     asection * input_section ATTRIBUTE_UNUSED;
     bfd * output_bfd ATTRIBUTE_UNUSED;
     char ** error_message ATTRIBUTE_UNUSED;
{
  BFD_ASSERT (reloc_entry->howto != (reloc_howto_type *)0);

  _bfd_error_handler (_("%s: Relocation %s (%d) is not currently supported.\n"),
		      bfd_get_filename (abfd),
		      reloc_entry->howto->name,
		      reloc_entry->howto->type);

  return bfd_reloc_notsupported;
}

/* The RELOCATE_SECTION function is called by the ELF backend linker
   to handle the relocations for a section.

   The relocs are always passed as Rela structures; if the section
   actually uses Rel structures, the r_addend field will always be
   zero.

   This function is responsible for adjust the section contents as
   necessary, and (if using Rela relocs and generating a
   relocateable output file) adjusting the reloc addend as
   necessary.

   This function does not have to worry about setting the reloc
   address or the reloc symbol index.

   LOCAL_SYMS is a pointer to the swapped in local symbols.

   LOCAL_SECTIONS is an array giving the section in the input file
   corresponding to the st_shndx field of each local symbol.

   The global hash table entry for the global symbols can be found
   via elf_sym_hashes (input_bfd).

   When generating relocateable output, this function must handle
   STB_LOCAL/STT_SECTION symbols specially.  The output symbol is
   going to be the section symbol corresponding to the output
   section, which means that the addend must be adjusted
   accordingly.  */

static boolean
mcore_elf_relocate_section (output_bfd, info, input_bfd, input_section,
			  contents, relocs, local_syms, local_sections)
     bfd * output_bfd ATTRIBUTE_UNUSED;
     struct bfd_link_info * info;
     bfd * input_bfd;
     asection * input_section;
     bfd_byte * contents;
     Elf_Internal_Rela * relocs;
     Elf_Internal_Sym * local_syms;
     asection ** local_sections;
{
  Elf_Internal_Shdr *           symtab_hdr = & elf_tdata (input_bfd)->symtab_hdr;
  struct elf_link_hash_entry ** sym_hashes = elf_sym_hashes (input_bfd);
  Elf_Internal_Rela *           rel = relocs;
  Elf_Internal_Rela *           relend = relocs + input_section->reloc_count;
  boolean ret = true;

#ifdef DEBUG
  fprintf (stderr,
	   "mcore_elf_relocate_section called for %s section %s, %ld relocations%s\n",
	   bfd_get_filename (input_bfd),
	   bfd_section_name(input_bfd, input_section),
	   (long) input_section->reloc_count,
	   (info->relocateable) ? " (relocatable)" : "");
#endif

  if (! mcore_elf_howto_table [R_MCORE_PCRELIMM8BY4])	/* Initialize howto table if needed */
    mcore_elf_howto_init ();

  for (; rel < relend; rel++)
    {
      enum elf_mcore_reloc_type    r_type = (enum elf_mcore_reloc_type) ELF32_R_TYPE (rel->r_info);
      bfd_vma                      offset = rel->r_offset;
      bfd_vma                      addend = rel->r_addend;
      bfd_reloc_status_type        r = bfd_reloc_other;
      asection *                   sec = (asection *) 0;
      reloc_howto_type *           howto;
      bfd_vma                      relocation;
      Elf_Internal_Sym *           sym = (Elf_Internal_Sym *) 0;
      unsigned long                r_symndx;
      struct elf_link_hash_entry * h = (struct elf_link_hash_entry *) 0;
      unsigned short               oldinst = 0;

      /* Unknown relocation handling */
      if ((unsigned) r_type >= (unsigned) R_MCORE_max
	  || ! mcore_elf_howto_table [(int)r_type])
	{
	  _bfd_error_handler (_("%s: Unknown relocation type %d\n"),
			      bfd_get_filename (input_bfd),
			      (int) r_type);

	  bfd_set_error (bfd_error_bad_value);
	  ret = false;
	  continue;
	}

      howto = mcore_elf_howto_table [(int) r_type];
      r_symndx = ELF32_R_SYM (rel->r_info);

      if (info->relocateable)
	{
	  /* This is a relocateable link.  We don't have to change
	     anything, unless the reloc is against a section symbol,
	     in which case we have to adjust according to where the
	     section symbol winds up in the output section.  */
	  if (r_symndx < symtab_hdr->sh_info)
	    {
	      sym = local_syms + r_symndx;

	      if ((unsigned)ELF_ST_TYPE (sym->st_info) == STT_SECTION)
		{
		  sec = local_sections[r_symndx];
		  addend = rel->r_addend += sec->output_offset + sym->st_value;
		}
	    }

#ifdef DEBUG
	  fprintf (stderr, "\ttype = %s (%d), symbol index = %ld, offset = %ld, addend = %ld\n",
		   howto->name, (int) r_type, r_symndx, (long) offset, (long) addend);
#endif
	  continue;
	}

      /* This is a final link.  */

      /* Complain about known relocation that are not yet supported.  */
      if (howto->special_function == mcore_elf_unsupported_reloc)
	{
	  _bfd_error_handler (_("%s: Relocation %s (%d) is not currently supported.\n"),
			      bfd_get_filename (input_bfd),
			      howto->name,
			      (int)r_type);

	  bfd_set_error (bfd_error_bad_value);
	  ret = false;
	  continue;
	}

      if (r_symndx < symtab_hdr->sh_info)
	{
	  sym = local_syms + r_symndx;
	  sec = local_sections [r_symndx];
	  relocation = (sec->output_section->vma
			+ sec->output_offset
			+ sym->st_value);
	}
      else
	{
	  h = sym_hashes [r_symndx - symtab_hdr->sh_info];
	  if (   h->root.type == bfd_link_hash_defined
	      || h->root.type == bfd_link_hash_defweak)
	    {
	      sec = h->root.u.def.section;
	      relocation = (h->root.u.def.value
			    + sec->output_section->vma
			    + sec->output_offset);
	    }
	  else if (h->root.type == bfd_link_hash_undefweak)
	    relocation = 0;
	  else if (info->shared
		   && ELF_ST_VISIBILITY (h->other) == STV_DEFAULT)
	    relocation = 0;
	  else
	    {
	      if (! ((*info->callbacks->undefined_symbol)
			(info, h->root.root.string, input_bfd,
		 	 input_section, rel->r_offset, true)))
		return false;

	      ret = false;
	      continue;
	    }
	}

      switch (r_type)
	{
	default:
	  break;

	case R_MCORE_PCRELJSR_IMM11BY2:
	  oldinst = bfd_get_16 (input_bfd, contents + offset);
#define	MCORE_INST_BSR	0xF800
	  bfd_put_16 (input_bfd, MCORE_INST_BSR, contents + offset);
	  break;
	}

#ifdef DEBUG
      fprintf (stderr, "\ttype = %s (%d), symbol index = %ld, offset = %ld, addend = %ld\n",
	       howto->name, r_type, r_symndx, (long) offset, (long) addend);
#endif

      r = _bfd_final_link_relocate
	(howto, input_bfd, input_section, contents, offset, relocation, addend);

      if (r != bfd_reloc_ok && r_type == R_MCORE_PCRELJSR_IMM11BY2)
	{
	  /* Wasn't ok, back it out and give up.  */
	  bfd_put_16 (input_bfd, oldinst, contents + offset);
	  r = bfd_reloc_ok;
	}

      if (r != bfd_reloc_ok)
	{
	  ret = false;

	  switch (r)
	    {
	    default:
	      break;

	    case bfd_reloc_overflow:
	      {
		const char * name;

		if (h != NULL)
		  name = h->root.root.string;
		else
		  {
		    name = bfd_elf_string_from_elf_section
		      (input_bfd, symtab_hdr->sh_link, sym->st_name);

		    if (name == NULL)
		      break;

		    if (* name == '\0')
		      name = bfd_section_name (input_bfd, sec);
		  }

		(*info->callbacks->reloc_overflow)
		  (info, name, howto->name, (bfd_vma) 0, input_bfd, input_section,
		   offset);
	      }
	      break;
	    }
	}
    }

#ifdef DEBUG
  fprintf (stderr, "\n");
#endif

  return ret;
}

/* Return the section that should be marked against GC for a given
   relocation.  */

static asection *
mcore_elf_gc_mark_hook (abfd, info, rel, h, sym)
     bfd *                        abfd;
     struct bfd_link_info *       info ATTRIBUTE_UNUSED;
     Elf_Internal_Rela *          rel;
     struct elf_link_hash_entry * h;
     Elf_Internal_Sym *           sym;
{
  if (h != NULL)
    {
      switch (ELF32_R_TYPE (rel->r_info))
	{
	case R_MCORE_GNU_VTINHERIT:
	case R_MCORE_GNU_VTENTRY:
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

/* Update the got entry reference counts for the section being removed.  */

static boolean
mcore_elf_gc_sweep_hook (abfd, info, sec, relocs)
     bfd *                     abfd ATTRIBUTE_UNUSED;
     struct bfd_link_info *    info ATTRIBUTE_UNUSED;
     asection *                sec ATTRIBUTE_UNUSED;
     const Elf_Internal_Rela * relocs ATTRIBUTE_UNUSED;
{
  return true;
}

/* Look through the relocs for a section during the first phase.
   Since we don't do .gots or .plts, we just need to consider the
   virtual table relocs for gc.  */

static boolean
mcore_elf_check_relocs (abfd, info, sec, relocs)
     bfd * abfd;
     struct bfd_link_info * info;
     asection * sec;
     const Elf_Internal_Rela * relocs;
{
  Elf_Internal_Shdr *           symtab_hdr;
  struct elf_link_hash_entry ** sym_hashes;
  struct elf_link_hash_entry ** sym_hashes_end;
  const Elf_Internal_Rela *     rel;
  const Elf_Internal_Rela *     rel_end;

  if (info->relocateable)
    return true;

  symtab_hdr = & elf_tdata (abfd)->symtab_hdr;
  sym_hashes = elf_sym_hashes (abfd);
  sym_hashes_end = sym_hashes + symtab_hdr->sh_size / sizeof (Elf32_External_Sym);
  if (!elf_bad_symtab (abfd))
    sym_hashes_end -= symtab_hdr->sh_info;

  rel_end = relocs + sec->reloc_count;

  for (rel = relocs; rel < rel_end; rel++)
    {
      struct elf_link_hash_entry * h;
      unsigned long r_symndx;

      r_symndx = ELF32_R_SYM (rel->r_info);

      if (r_symndx < symtab_hdr->sh_info)
        h = NULL;
      else
        h = sym_hashes [r_symndx - symtab_hdr->sh_info];

      switch (ELF32_R_TYPE (rel->r_info))
        {
        /* This relocation describes the C++ object vtable hierarchy.
           Reconstruct it for later use during GC.  */
        case R_MCORE_GNU_VTINHERIT:
          if (!_bfd_elf32_gc_record_vtinherit (abfd, sec, h, rel->r_offset))
            return false;
          break;

        /* This relocation describes which C++ vtable entries are actually
           used.  Record for later use during GC.  */
        case R_MCORE_GNU_VTENTRY:
          if (!_bfd_elf32_gc_record_vtentry (abfd, sec, h, rel->r_addend))
            return false;
          break;
        }
    }

  return true;
}

#define TARGET_BIG_SYM		bfd_elf32_mcore_big_vec
#define TARGET_BIG_NAME		"elf32-mcore-big"
#define TARGET_LITTLE_SYM       bfd_elf32_mcore_little_vec
#define TARGET_LITTLE_NAME      "elf32-mcore-little"

#define ELF_ARCH		bfd_arch_mcore
#define ELF_MACHINE_CODE	EM_MCORE
#define ELF_MAXPAGESIZE		0x1000		/* 4k, if we ever have 'em */
#define elf_info_to_howto	mcore_elf_info_to_howto
#define elf_info_to_howto_rel	NULL

#define bfd_elf32_bfd_copy_private_bfd_data	mcore_elf_copy_private_bfd_data
#define bfd_elf32_bfd_merge_private_bfd_data	mcore_elf_merge_private_bfd_data
#define bfd_elf32_bfd_set_private_flags		mcore_elf_set_private_flags
#define bfd_elf32_bfd_reloc_type_lookup		mcore_elf_reloc_type_lookup
#define elf_backend_relocate_section		mcore_elf_relocate_section
#define elf_backend_gc_mark_hook		mcore_elf_gc_mark_hook
#define elf_backend_gc_sweep_hook		mcore_elf_gc_sweep_hook
#define elf_backend_check_relocs                mcore_elf_check_relocs

#define elf_backend_can_gc_sections		1

#include "elf32-target.h"
