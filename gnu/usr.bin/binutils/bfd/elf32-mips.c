/* MIPS-specific support for 32-bit ELF
   Copyright 1993, 1994, 1995 Free Software Foundation, Inc.

   Most of the information added by Ian Lance Taylor, Cygnus Support,
   <ian@cygnus.com>.

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
#include "bfdlink.h"
#include "genlink.h"
#include "elf-bfd.h"
#include "elf/mips.h"

/* Get the ECOFF swapping routines.  */
#include "coff/sym.h"
#include "coff/symconst.h"
#include "coff/internal.h"
#include "coff/ecoff.h"
#include "coff/mips.h"
#define ECOFF_32
#include "ecoffswap.h"

static bfd_reloc_status_type mips_elf_hi16_reloc PARAMS ((bfd *abfd,
							  arelent *reloc,
							  asymbol *symbol,
							  PTR data,
							  asection *section,
							  bfd *output_bfd,
							  char **error));
static bfd_reloc_status_type mips_elf_got16_reloc PARAMS ((bfd *abfd,
							   arelent *reloc,
							   asymbol *symbol,
							   PTR data,
							   asection *section,
							   bfd *output_bfd,
							   char **error));
static bfd_reloc_status_type mips_elf_lo16_reloc PARAMS ((bfd *abfd,
							  arelent *reloc,
							  asymbol *symbol,
							  PTR data,
							  asection *section,
							  bfd *output_bfd,
							  char **error));
static bfd_reloc_status_type mips_elf_gprel16_reloc PARAMS ((bfd *abfd,
							     arelent *reloc,
							     asymbol *symbol,
							     PTR data,
							     asection *section,
							     bfd *output_bfd,
							     char **error));
static reloc_howto_type *bfd_elf32_bfd_reloc_type_lookup
  PARAMS ((bfd *, bfd_reloc_code_real_type));
static void mips_info_to_howto_rel
  PARAMS ((bfd *, arelent *, Elf32_Internal_Rel *));
static boolean mips_elf_sym_is_global PARAMS ((bfd *, asymbol *));
static boolean mips_elf_object_p PARAMS ((bfd *));
static void mips_elf_final_write_processing
  PARAMS ((bfd *, boolean));
static boolean mips_elf_section_from_shdr
  PARAMS ((bfd *, Elf32_Internal_Shdr *, char *));
static boolean mips_elf_fake_sections
  PARAMS ((bfd *, Elf32_Internal_Shdr *, asection *));
static boolean mips_elf_section_from_bfd_section
  PARAMS ((bfd *, Elf32_Internal_Shdr *, asection *, int *));
static boolean mips_elf_section_processing
  PARAMS ((bfd *, Elf32_Internal_Shdr *));
static void mips_elf_symbol_processing PARAMS ((bfd *, asymbol *));
static boolean mips_elf_read_ecoff_info
  PARAMS ((bfd *, asection *, struct ecoff_debug_info *));
static boolean mips_elf_is_local_label
  PARAMS ((bfd *, asymbol *));
static boolean mips_elf_find_nearest_line
  PARAMS ((bfd *, asection *, asymbol **, bfd_vma, const char **,
	   const char **, unsigned int *));
static struct bfd_hash_entry *mips_elf_link_hash_newfunc
  PARAMS ((struct bfd_hash_entry *, struct bfd_hash_table *, const char *));
static struct bfd_link_hash_table *mips_elf_link_hash_table_create
  PARAMS ((bfd *));
static int gptab_compare PARAMS ((const void *, const void *));
static boolean mips_elf_final_link
  PARAMS ((bfd *, struct bfd_link_info *));
static void mips_elf_relocate_hi16
  PARAMS ((bfd *, Elf_Internal_Rela *, Elf_Internal_Rela *, bfd_byte *,
	   bfd_vma));
static boolean mips_elf_relocate_section
  PARAMS ((bfd *, struct bfd_link_info *, bfd *, asection *, bfd_byte *,
	   Elf_Internal_Rela *, Elf_Internal_Sym *, asection **));
static boolean mips_elf_add_symbol_hook
  PARAMS ((bfd *, struct bfd_link_info *, const Elf_Internal_Sym *,
	   const char **, flagword *, asection **, bfd_vma *));

#define USE_REL	1		/* MIPS uses REL relocations instead of RELA */

enum reloc_type
{
  R_MIPS_NONE = 0,
  R_MIPS_16,		R_MIPS_32,
  R_MIPS_REL32,		R_MIPS_26,
  R_MIPS_HI16,		R_MIPS_LO16,
  R_MIPS_GPREL16,	R_MIPS_LITERAL,
  R_MIPS_GOT16,		R_MIPS_PC16,
  R_MIPS_CALL16,	R_MIPS_GPREL32,
  /* The remaining relocs are defined on Irix, although they are not
     in the MIPS ELF ABI.  */
  R_MIPS_UNUSED1,	R_MIPS_UNUSED2,
  R_MIPS_UNUSED3,
  R_MIPS_SHIFT5,	R_MIPS_SHIFT6,
  R_MIPS_64,		R_MIPS_GOT_DISP,
  R_MIPS_GOT_PAGE,	R_MIPS_GOT_OFST,
  R_MIPS_GOT_HI16,	R_MIPS_GOT_LO16,
  R_MIPS_SUB,		R_MIPS_INSERT_A,
  R_MIPS_INSERT_B,	R_MIPS_DELETE,
  R_MIPS_HIGHER,	R_MIPS_HIGHEST,
  R_MIPS_CALL_HI16,	R_MIPS_CALL_LO16,
  R_MIPS_max
};

static reloc_howto_type elf_mips_howto_table[] =
{
  /* No relocation.  */
  HOWTO (R_MIPS_NONE,		/* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_NONE",		/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 false),		/* pcrel_offset */

  /* 16 bit relocation.  */
  HOWTO (R_MIPS_16,		/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_16",		/* name */
	 true,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* 32 bit relocation.  */
  HOWTO (R_MIPS_32,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_32",		/* name */
	 true,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* 32 bit symbol relative relocation.  */
  HOWTO (R_MIPS_REL32,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_REL32",	/* name */
	 true,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* 26 bit branch address.  */
  HOWTO (R_MIPS_26,		/* type */
	 2,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 26,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 			/* This needs complex overflow
				   detection, because the upper four
				   bits must match the PC.  */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_26",		/* name */
	 true,			/* partial_inplace */
	 0x3ffffff,		/* src_mask */
	 0x3ffffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* High 16 bits of symbol value.  */
  HOWTO (R_MIPS_HI16,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 mips_elf_hi16_reloc,	/* special_function */
	 "R_MIPS_HI16",		/* name */
	 true,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Low 16 bits of symbol value.  */
  HOWTO (R_MIPS_LO16,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 mips_elf_lo16_reloc,	/* special_function */
	 "R_MIPS_LO16",		/* name */
	 true,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* GP relative reference.  */
  HOWTO (R_MIPS_GPREL16,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 mips_elf_gprel16_reloc, /* special_function */
	 "R_MIPS_GPREL16",	/* name */
	 true,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Reference to literal section.  */
  HOWTO (R_MIPS_LITERAL,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 mips_elf_gprel16_reloc, /* special_function */
	 "R_MIPS_LITERAL",	/* name */
	 true,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Reference to global offset table.  */
  /* FIXME: This is not handled correctly.  */
  HOWTO (R_MIPS_GOT16,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 mips_elf_got16_reloc,	/* special_function */
	 "R_MIPS_GOT16",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* 16 bit PC relative reference.  */
  HOWTO (R_MIPS_PC16,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 true,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_PC16",		/* name */
	 true,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* 16 bit call through global offset table.  */
  /* FIXME: This is not handled correctly.  */
  HOWTO (R_MIPS_CALL16,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_CALL16",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* 32 bit GP relative reference.  */
  /* FIXME: This is not handled correctly.  */
  HOWTO (R_MIPS_GPREL32,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_GPREL32",	/* name */
	 true,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 false),		/* pcrel_offset */

    /* The remaining relocs are defined on Irix 5, although they are
       not defined by the ABI.  */
    { 13 },
    { 14 },
    { 15 },

  /* A 5 bit shift field.  */
  HOWTO (R_MIPS_SHIFT5,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 5,			/* bitsize */
	 false,			/* pc_relative */
	 6,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_SHIFT5",	/* name */
	 true,			/* partial_inplace */
	 0x000007c0,		/* src_mask */
	 0x000007c0,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* A 6 bit shift field.  */
  /* FIXME: This is not handled correctly; a special function is
     needed to put the most significant bit in the right place.  */
  HOWTO (R_MIPS_SHIFT6,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 6,			/* bitsize */
	 false,			/* pc_relative */
	 6,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_SHIFT6",	/* name */
	 true,			/* partial_inplace */
	 0x000007c4,		/* src_mask */
	 0x000007c4,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* A 64 bit relocation.  Presumably not used in 32 bit ELF.  */
  { R_MIPS_64 },

  /* Displacement in the global offset table.  */
  /* FIXME: Not handled correctly.  */
  HOWTO (R_MIPS_GOT_DISP,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_GOT_DISP",	/* name */
	 true,			/* partial_inplace */
	 0x0000ffff,		/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Displacement to page pointer in the global offset table.  */
  /* FIXME: Not handled correctly.  */
  HOWTO (R_MIPS_GOT_PAGE,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_GOT_PAGE",	/* name */
	 true,			/* partial_inplace */
	 0x0000ffff,		/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Offset from page pointer in the global offset table.  */
  /* FIXME: Not handled correctly.  */
  HOWTO (R_MIPS_GOT_OFST,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_GOT_OFST",	/* name */
	 true,			/* partial_inplace */
	 0x0000ffff,		/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* High 16 bits of displacement in global offset table.  */
  /* FIXME: Not handled correctly.  */
  HOWTO (R_MIPS_GOT_HI16,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_GOT_HI16",	/* name */
	 true,			/* partial_inplace */
	 0x0000ffff,		/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Low 16 bits of displacement in global offset table.  */
  /* FIXME: Not handled correctly.  */
  HOWTO (R_MIPS_GOT_LO16,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_GOT_LO16",	/* name */
	 true,			/* partial_inplace */
	 0x0000ffff,		/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* 64 bit subtraction.  Presumably not used in 32 bit ELF.  */
  { R_MIPS_SUB },

  /* Used to cause the linker to insert and delete instructions?  */
  { R_MIPS_INSERT_A },
  { R_MIPS_INSERT_B },
  { R_MIPS_DELETE },

  /* Get the higher values of a 64 bit addend.  Presumably not used in
     32 bit ELF.  */
  { R_MIPS_HIGHER },
  { R_MIPS_HIGHEST },

  /* High 16 bits of displacement in global offset table.  */
  /* FIXME: Not handled correctly.  */
  HOWTO (R_MIPS_CALL_HI16,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_CALL_HI16",	/* name */
	 true,			/* partial_inplace */
	 0x0000ffff,		/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Low 16 bits of displacement in global offset table.  */
  /* FIXME: Not handled correctly.  */
  HOWTO (R_MIPS_CALL_LO16,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_CALL_LO16",	/* name */
	 true,			/* partial_inplace */
	 0x0000ffff,		/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 false)			/* pcrel_offset */
};

/* Do a R_MIPS_HI16 relocation.  This has to be done in combination
   with a R_MIPS_LO16 reloc, because there is a carry from the LO16 to
   the HI16.  Here we just save the information we need; we do the
   actual relocation when we see the LO16.  MIPS ELF requires that the
   LO16 immediately follow the HI16, so this ought to work.  */

static bfd_byte *mips_hi16_addr;
static bfd_vma mips_hi16_addend;

static bfd_reloc_status_type
mips_elf_hi16_reloc (abfd,
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
  bfd_reloc_status_type ret;
  bfd_vma relocation;

  /* If we're relocating, and this an external symbol, we don't want
     to change anything.  */
  if (output_bfd != (bfd *) NULL
      && (symbol->flags & BSF_SECTION_SYM) == 0
      && reloc_entry->addend == 0)
    {
      reloc_entry->address += input_section->output_offset;
      return bfd_reloc_ok;
    }

  /* FIXME: The symbol _gp_disp requires special handling, which we do
     not do.  */
  if (strcmp (bfd_asymbol_name (symbol), "_gp_disp") == 0)
    abort ();

  ret = bfd_reloc_ok;
  if (bfd_is_und_section (symbol->section)
      && output_bfd == (bfd *) NULL)
    ret = bfd_reloc_undefined;

  if (bfd_is_com_section (symbol->section))
    relocation = 0;
  else
    relocation = symbol->value;

  relocation += symbol->section->output_section->vma;
  relocation += symbol->section->output_offset;
  relocation += reloc_entry->addend;

  if (reloc_entry->address > input_section->_cooked_size)
    return bfd_reloc_outofrange;

  /* Save the information, and let LO16 do the actual relocation.  */
  mips_hi16_addr = (bfd_byte *) data + reloc_entry->address;
  mips_hi16_addend = relocation;

  if (output_bfd != (bfd *) NULL)
    reloc_entry->address += input_section->output_offset;

  return ret;
}

/* Do a R_MIPS_LO16 relocation.  This is a straightforward 16 bit
   inplace relocation; this function exists in order to do the
   R_MIPS_HI16 relocation described above.  */

static bfd_reloc_status_type
mips_elf_lo16_reloc (abfd,
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
  /* FIXME: The symbol _gp_disp requires special handling, which we do
     not do.  */
  if (output_bfd == (bfd *) NULL
      && strcmp (bfd_asymbol_name (symbol), "_gp_disp") == 0)
    abort ();

  if (mips_hi16_addr != (bfd_byte *) NULL)
    {
      unsigned long insn;
      unsigned long val;
      unsigned long vallo;

      /* Do the HI16 relocation.  Note that we actually don't need to
	 know anything about the LO16 itself, except where to find the
	 low 16 bits of the addend needed by the LO16.  */
      insn = bfd_get_32 (abfd, mips_hi16_addr);
      vallo = (bfd_get_32 (abfd, (bfd_byte *) data + reloc_entry->address)
	       & 0xffff);
      val = ((insn & 0xffff) << 16) + vallo;
      val += mips_hi16_addend;

      /* The low order 16 bits are always treated as a signed value.
	 Therefore, a negative value in the low order bits requires an
	 adjustment in the high order bits.  We need to make this
	 adjustment in two ways: once for the bits we took from the
	 data, and once for the bits we are putting back in to the
	 data.  */
      if ((vallo & 0x8000) != 0)
	val -= 0x10000;
      if ((val & 0x8000) != 0)
	val += 0x10000;

      insn = (insn &~ 0xffff) | ((val >> 16) & 0xffff);
      bfd_put_32 (abfd, insn, mips_hi16_addr);

      mips_hi16_addr = (bfd_byte *) NULL;
    }

  /* Now do the LO16 reloc in the usual way.  */
  return bfd_elf_generic_reloc (abfd, reloc_entry, symbol, data,
				input_section, output_bfd, error_message);
}

/* Do a R_MIPS_GOT16 reloc.  This is a reloc against the global offset
   table used for PIC code.  If the symbol is an external symbol, the
   instruction is modified to contain the offset of the appropriate
   entry in the global offset table.  If the symbol is a section
   symbol, the next reloc is a R_MIPS_LO16 reloc.  The two 16 bit
   addends are combined to form the real addend against the section
   symbol; the GOT16 is modified to contain the offset of an entry in
   the global offset table, and the LO16 is modified to offset it
   appropriately.  Thus an offset larger than 16 bits requires a
   modified value in the global offset table.

   This implementation suffices for the assembler, but the linker does
   not yet know how to create global offset tables.  */

static bfd_reloc_status_type
mips_elf_got16_reloc (abfd,
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
  /* If we're relocating, and this an external symbol, we don't want
     to change anything.  */
  if (output_bfd != (bfd *) NULL
      && (symbol->flags & BSF_SECTION_SYM) == 0
      && reloc_entry->addend == 0)
    {
      reloc_entry->address += input_section->output_offset;
      return bfd_reloc_ok;
    }

  /* If we're relocating, and this is a local symbol, we can handle it
     just like HI16.  */
  if (output_bfd != (bfd *) NULL
      && (symbol->flags & BSF_SECTION_SYM) != 0)
    return mips_elf_hi16_reloc (abfd, reloc_entry, symbol, data,
				input_section, output_bfd, error_message);

  abort ();
}

/* Do a R_MIPS_GPREL16 relocation.  This is a 16 bit value which must
   become the offset from the gp register.  This function also handles
   R_MIPS_LITERAL relocations, although those can be handled more
   cleverly because the entries in the .lit8 and .lit4 sections can be
   merged.  */

static bfd_reloc_status_type gprel16_with_gp PARAMS ((bfd *, asymbol *,
						      arelent *, asection *,
						      boolean, PTR, bfd_vma));

static bfd_reloc_status_type
mips_elf_gprel16_reloc (abfd,
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
  boolean relocateable;

  /* If we're relocating, and this is an external symbol with no
     addend, we don't want to change anything.  We will only have an
     addend if this is a newly created reloc, not read from an ELF
     file.  */
  if (output_bfd != (bfd *) NULL
      && (symbol->flags & BSF_SECTION_SYM) == 0
      && reloc_entry->addend == 0)
    {
      reloc_entry->address += input_section->output_offset;
      return bfd_reloc_ok;
    }

  if (output_bfd != (bfd *) NULL)
    relocateable = true;
  else
    {
      relocateable = false;
      output_bfd = symbol->section->output_section->owner;
    }

  if (bfd_is_und_section (symbol->section)
      && relocateable == false)
    return bfd_reloc_undefined;

  /* Some of the code below assumes the output bfd is ELF too.  */
  if (output_bfd->xvec->flavour != bfd_target_elf_flavour)
    abort ();

  /* We have to figure out the gp value, so that we can adjust the
     symbol value correctly.  We look up the symbol _gp in the output
     BFD.  If we can't find it, we're stuck.  We cache it in the ELF
     target data.  We don't need to adjust the symbol value for an
     external symbol if we are producing relocateable output.  */
  if (elf_gp (output_bfd) == 0
      && (relocateable == false
	  || (symbol->flags & BSF_SECTION_SYM) != 0))
    {
      if (relocateable != false)
	{
	  /* Make up a value.  */
	  elf_gp (output_bfd) =
	    symbol->section->output_section->vma + 0x4000;
	}
      else
	{
	  unsigned int count;
	  asymbol **sym;
	  unsigned int i;

	  count = bfd_get_symcount (output_bfd);
	  sym = bfd_get_outsymbols (output_bfd);

	  if (sym == (asymbol **) NULL)
	    i = count;
	  else
	    {
	      for (i = 0; i < count; i++, sym++)
		{
		  register CONST char *name;

		  name = bfd_asymbol_name (*sym);
		  if (*name == '_' && strcmp (name, "_gp") == 0)
		    {
		      elf_gp (output_bfd) = bfd_asymbol_value (*sym);
		      break;
		    }
		}
	    }

	  if (i >= count)
	    {
	      /* Only get the error once.  */
	      elf_gp (output_bfd) = 4;
	      *error_message =
		(char *) "GP relative relocation when _gp not defined";
	      return bfd_reloc_dangerous;
	    }
	}
    }

  return gprel16_with_gp (abfd, symbol, reloc_entry, input_section,
			  relocateable, data, elf_gp (output_bfd));
}

static bfd_reloc_status_type
gprel16_with_gp (abfd, symbol, reloc_entry, input_section, relocateable, data,
		 gp)
     bfd *abfd;
     asymbol *symbol;
     arelent *reloc_entry;
     asection *input_section;
     boolean relocateable;
     PTR data;
     bfd_vma gp;
{
  bfd_vma relocation;
  unsigned long insn;
  unsigned long val;

  if (bfd_is_com_section (symbol->section))
    relocation = 0;
  else
    relocation = symbol->value;

  relocation += symbol->section->output_section->vma;
  relocation += symbol->section->output_offset;

  if (reloc_entry->address > input_section->_cooked_size)
    return bfd_reloc_outofrange;

  insn = bfd_get_32 (abfd, (bfd_byte *) data + reloc_entry->address);

  /* Set val to the offset into the section or symbol.  */
  val = ((insn & 0xffff) + reloc_entry->addend) & 0xffff;
  if (val & 0x8000)
    val -= 0x10000;

  /* Adjust val for the final section location and GP value.  If we
     are producing relocateable output, we don't want to do this for
     an external symbol.  */
  if (relocateable == false
      || (symbol->flags & BSF_SECTION_SYM) != 0)
    val += relocation - gp;

  insn = (insn &~ 0xffff) | (val & 0xffff);
  bfd_put_32 (abfd, insn, (bfd_byte *) data + reloc_entry->address);

  if (relocateable != false)
    reloc_entry->address += input_section->output_offset;

  /* Make sure it fit in 16 bits.  */
  if (val >= 0x8000 && val < 0xffff8000)
    return bfd_reloc_overflow;

  return bfd_reloc_ok;
}

/* A mapping from BFD reloc types to MIPS ELF reloc types.  */

struct elf_reloc_map {
  bfd_reloc_code_real_type bfd_reloc_val;
  enum reloc_type elf_reloc_val;
};

static CONST struct elf_reloc_map mips_reloc_map[] =
{
  { BFD_RELOC_NONE, R_MIPS_NONE, },
  { BFD_RELOC_16, R_MIPS_16 },
  { BFD_RELOC_32, R_MIPS_32 },
  { BFD_RELOC_CTOR, R_MIPS_32 },
  { BFD_RELOC_32_PCREL, R_MIPS_REL32 },
  { BFD_RELOC_MIPS_JMP, R_MIPS_26 },
  { BFD_RELOC_HI16_S, R_MIPS_HI16 },
  { BFD_RELOC_LO16, R_MIPS_LO16 },
  { BFD_RELOC_MIPS_GPREL, R_MIPS_GPREL16 },
  { BFD_RELOC_MIPS_LITERAL, R_MIPS_LITERAL },
  { BFD_RELOC_MIPS_GOT16, R_MIPS_GOT16 },
  { BFD_RELOC_16_PCREL, R_MIPS_PC16 },
  { BFD_RELOC_MIPS_CALL16, R_MIPS_CALL16 },
  { BFD_RELOC_MIPS_GPREL32, R_MIPS_GPREL32 }
};

/* Given a BFD reloc type, return a howto structure.  */

static reloc_howto_type *
bfd_elf32_bfd_reloc_type_lookup (abfd, code)
     bfd *abfd;
     bfd_reloc_code_real_type code;
{
  unsigned int i;

  for (i = 0; i < sizeof (mips_reloc_map) / sizeof (struct elf_reloc_map); i++)
    {
      if (mips_reloc_map[i].bfd_reloc_val == code)
	return &elf_mips_howto_table[(int) mips_reloc_map[i].elf_reloc_val];
    }
  return NULL;
}

/* Given a MIPS reloc type, fill in an arelent structure.  */

static void
mips_info_to_howto_rel (abfd, cache_ptr, dst)
     bfd *abfd;
     arelent *cache_ptr;
     Elf32_Internal_Rel *dst;
{
  unsigned int r_type;

  r_type = ELF32_R_TYPE (dst->r_info);
  BFD_ASSERT (r_type < (unsigned int) R_MIPS_max);
  cache_ptr->howto = &elf_mips_howto_table[r_type];

  /* The addend for a GPREL16 or LITERAL relocation comes from the GP
     value for the object file.  We get the addend now, rather than
     when we do the relocation, because the symbol manipulations done
     by the linker may cause us to lose track of the input BFD.  */
  if (((*cache_ptr->sym_ptr_ptr)->flags & BSF_SECTION_SYM) != 0
      && (r_type == (unsigned int) R_MIPS_GPREL16
	  || r_type == (unsigned int) R_MIPS_LITERAL))
    cache_ptr->addend = elf_gp (abfd);
}

/* A .reginfo section holds a single Elf32_RegInfo structure.  These
   routines swap this structure in and out.  They are used outside of
   BFD, so they are globally visible.  */

void
bfd_mips_elf32_swap_reginfo_in (abfd, ex, in)
     bfd *abfd;
     const Elf32_External_RegInfo *ex;
     Elf32_RegInfo *in;
{
  in->ri_gprmask = bfd_h_get_32 (abfd, (bfd_byte *) ex->ri_gprmask);
  in->ri_cprmask[0] = bfd_h_get_32 (abfd, (bfd_byte *) ex->ri_cprmask[0]);
  in->ri_cprmask[1] = bfd_h_get_32 (abfd, (bfd_byte *) ex->ri_cprmask[1]);
  in->ri_cprmask[2] = bfd_h_get_32 (abfd, (bfd_byte *) ex->ri_cprmask[2]);
  in->ri_cprmask[3] = bfd_h_get_32 (abfd, (bfd_byte *) ex->ri_cprmask[3]);
  in->ri_gp_value = bfd_h_get_32 (abfd, (bfd_byte *) ex->ri_gp_value);
}

void
bfd_mips_elf32_swap_reginfo_out (abfd, in, ex)
     bfd *abfd;
     const Elf32_RegInfo *in;
     Elf32_External_RegInfo *ex;
{
  bfd_h_put_32 (abfd, (bfd_vma) in->ri_gprmask,
		(bfd_byte *) ex->ri_gprmask);
  bfd_h_put_32 (abfd, (bfd_vma) in->ri_cprmask[0],
		(bfd_byte *) ex->ri_cprmask[0]);
  bfd_h_put_32 (abfd, (bfd_vma) in->ri_cprmask[1],
		(bfd_byte *) ex->ri_cprmask[1]);
  bfd_h_put_32 (abfd, (bfd_vma) in->ri_cprmask[2],
		(bfd_byte *) ex->ri_cprmask[2]);
  bfd_h_put_32 (abfd, (bfd_vma) in->ri_cprmask[3],
		(bfd_byte *) ex->ri_cprmask[3]);
  bfd_h_put_32 (abfd, (bfd_vma) in->ri_gp_value,
		(bfd_byte *) ex->ri_gp_value);
}

/* Swap an entry in a .gptab section.  Note that these routines rely
   on the equivalence of the two elements of the union.  */

static void
bfd_mips_elf32_swap_gptab_in (abfd, ex, in)
     bfd *abfd;
     const Elf32_External_gptab *ex;
     Elf32_gptab *in;
{
  in->gt_entry.gt_g_value = bfd_h_get_32 (abfd, ex->gt_entry.gt_g_value);
  in->gt_entry.gt_bytes = bfd_h_get_32 (abfd, ex->gt_entry.gt_bytes);
}

static void
bfd_mips_elf32_swap_gptab_out (abfd, in, ex)
     bfd *abfd;
     const Elf32_gptab *in;
     Elf32_External_gptab *ex;
{
  bfd_h_put_32 (abfd, (bfd_vma) in->gt_entry.gt_g_value,
		ex->gt_entry.gt_g_value);
  bfd_h_put_32 (abfd, (bfd_vma) in->gt_entry.gt_bytes,
		ex->gt_entry.gt_bytes);
}

/* Determine whether a symbol is global for the purposes of splitting
   the symbol table into global symbols and local symbols.  At least
   on Irix 5, this split must be between section symbols and all other
   symbols.  On most ELF targets the split is between static symbols
   and externally visible symbols.  */

/*ARGSUSED*/
static boolean
mips_elf_sym_is_global (abfd, sym)
     bfd *abfd;
     asymbol *sym;
{
  return (sym->flags & BSF_SECTION_SYM) == 0 ? true : false;
}

/* Set the right machine number for a MIPS ELF file.  */

static boolean
mips_elf_object_p (abfd)
     bfd *abfd;
{
  switch (elf_elfheader (abfd)->e_flags & EF_MIPS_ARCH)
    {
    default:
    case E_MIPS_ARCH_1:
      /* Just use the default, which was set in elfcode.h.  */
      break;

    case E_MIPS_ARCH_2:
      (void) bfd_default_set_arch_mach (abfd, bfd_arch_mips, 6000);
      break;

    case E_MIPS_ARCH_3:
      (void) bfd_default_set_arch_mach (abfd, bfd_arch_mips, 4000);
      break;
    }

  /* Irix 5 is broken.  Object file symbol tables are not always
     sorted correctly such that local symbols precede global symbols,
     and the sh_info field in the symbol table is not always right.  */
  elf_bad_symtab (abfd) = true;

  return true;
}

/* The final processing done just before writing out a MIPS ELF object
   file.  This gets the MIPS architecture right based on the machine
   number.  */

/*ARGSUSED*/
static void
mips_elf_final_write_processing (abfd, linker)
     bfd *abfd;
     boolean linker;
{
  unsigned long val;
  unsigned int i;
  Elf_Internal_Shdr **hdrpp;

  switch (bfd_get_mach (abfd))
    {
    case 3000:
      val = E_MIPS_ARCH_1;
      break;

    case 6000:
      val = E_MIPS_ARCH_2;
      break;

    case 4000:
      val = E_MIPS_ARCH_3;
      break;

    default:
      return;
    }

  elf_elfheader (abfd)->e_flags &=~ EF_MIPS_ARCH;
  elf_elfheader (abfd)->e_flags |= val;

  /* Set the sh_info field for .gptab sections.  */
  for (i = 1, hdrpp = elf_elfsections (abfd) + 1;
       i < elf_elfheader (abfd)->e_shnum;
       i++, hdrpp++)
    {
      if ((*hdrpp)->sh_type == SHT_MIPS_GPTAB)
	{
	  const char *name;
	  asection *sec;

	  BFD_ASSERT ((*hdrpp)->bfd_section != NULL);
	  name = bfd_get_section_name (abfd, (*hdrpp)->bfd_section);
	  BFD_ASSERT (name != NULL
		      && strncmp (name, ".gptab.", sizeof ".gptab." - 1) == 0);
	  sec = bfd_get_section_by_name (abfd, name + sizeof ".gptab" - 1);
	  BFD_ASSERT (sec != NULL);
	  (*hdrpp)->sh_info = elf_section_data (sec)->this_idx;
	}
    }
}

/* Handle a MIPS specific section when reading an object file.  This
   is called when elfcode.h finds a section with an unknown type.
   FIXME: We need to handle the SHF_MIPS_GPREL flag, but I'm not sure
   how to.  */

static boolean
mips_elf_section_from_shdr (abfd, hdr, name)
     bfd *abfd;
     Elf32_Internal_Shdr *hdr;
     char *name;
{
  asection *newsect;

  /* There ought to be a place to keep ELF backend specific flags, but
     at the moment there isn't one.  We just keep track of the
     sections by their name, instead.  Fortunately, the ABI gives
     suggested names for all the MIPS specific sections, so we will
     probably get away with this.  */
  switch (hdr->sh_type)
    {
    case SHT_MIPS_LIBLIST:
      if (strcmp (name, ".liblist") != 0)
	return false;
      break;
    case SHT_MIPS_MSYM:
      if (strcmp (name, ".msym") != 0)
	return false;
      break;
    case SHT_MIPS_CONFLICT:
      if (strcmp (name, ".conflict") != 0)
	return false;
      break;
    case SHT_MIPS_GPTAB:
      if (strncmp (name, ".gptab.", sizeof ".gptab." - 1) != 0)
	return false;
      break;
    case SHT_MIPS_UCODE:
      if (strcmp (name, ".ucode") != 0)
	return false;
      break;
    case SHT_MIPS_DEBUG:
      if (strcmp (name, ".mdebug") != 0)
	return false;
      break;
    case SHT_MIPS_REGINFO:
      if (strcmp (name, ".reginfo") != 0
	  || hdr->sh_size != sizeof (Elf32_External_RegInfo))
	return false;
      break;
    case SHT_MIPS_OPTIONS:
      if (strcmp (name, ".options") != 0)
	return false;
      break;
    case SHT_MIPS_DWARF:
      if (strncmp (name, ".debug_", sizeof ".debug_" - 1) != 0)
	return false;
      break;
    case SHT_MIPS_EVENTS:
      if (strncmp (name, ".MIPS.events.", sizeof ".MIPS.events." - 1) != 0)
	return false;
      break;
    default:
      return false;
    }

  if (! _bfd_elf_make_section_from_shdr (abfd, hdr, name))
    return false;
  newsect = hdr->bfd_section;

  if (hdr->sh_type == SHT_MIPS_DEBUG)
    {
      if (! bfd_set_section_flags (abfd, newsect,
				   (bfd_get_section_flags (abfd, newsect)
				    | SEC_DEBUGGING)))
	return false;
    }

  /* FIXME: We should record sh_info for a .gptab section.  */

  /* For a .reginfo section, set the gp value in the tdata information
     from the contents of this section.  We need the gp value while
     processing relocs, so we just get it now.  */
  if (hdr->sh_type == SHT_MIPS_REGINFO)
    {
      Elf32_External_RegInfo ext;
      Elf32_RegInfo s;

      if (! bfd_get_section_contents (abfd, newsect, (PTR) &ext,
				      (file_ptr) 0, sizeof ext))
	return false;
      bfd_mips_elf32_swap_reginfo_in (abfd, &ext, &s);
      elf_gp (abfd) = s.ri_gp_value;
    }

  return true;
}

/* Set the correct type for a MIPS ELF section.  We do this by the
   section name, which is a hack, but ought to work.  */

static boolean
mips_elf_fake_sections (abfd, hdr, sec)
     bfd *abfd;
     Elf32_Internal_Shdr *hdr;
     asection *sec;
{
  register const char *name;

  name = bfd_get_section_name (abfd, sec);

  if (strcmp (name, ".liblist") == 0)
    {
      hdr->sh_type = SHT_MIPS_LIBLIST;
      hdr->sh_info = sec->_raw_size / sizeof (Elf32_Lib);
      /* FIXME: Set the sh_link field.  */
    }
  else if (strcmp (name, ".msym") == 0)
    {
      hdr->sh_type = SHT_MIPS_MSYM;
      hdr->sh_entsize = 8;
      /* FIXME: Set the sh_info field.  */
    }
  else if (strcmp (name, ".conflict") == 0)
    hdr->sh_type = SHT_MIPS_CONFLICT;
  else if (strncmp (name, ".gptab.", sizeof ".gptab." - 1) == 0)
    {
      hdr->sh_type = SHT_MIPS_GPTAB;
      hdr->sh_entsize = sizeof (Elf32_External_gptab);
      /* The sh_info field is set in mips_elf_final_write_processing.  */
    }
  else if (strcmp (name, ".ucode") == 0)
    hdr->sh_type = SHT_MIPS_UCODE;
  else if (strcmp (name, ".mdebug") == 0)
    {
      hdr->sh_type = SHT_MIPS_DEBUG;
      hdr->sh_entsize = 1;
    }
  else if (strcmp (name, ".reginfo") == 0)
    {
      hdr->sh_type = SHT_MIPS_REGINFO;
      hdr->sh_entsize = 1;

      /* Force the section size to the correct value, even if the
	 linker thinks it is larger.  The link routine below will only
	 write out this much data for .reginfo.  */
      hdr->sh_size = sec->_raw_size = sizeof (Elf32_External_RegInfo);
    }
  else if (strcmp (name, ".options") == 0)
    {
      hdr->sh_type = SHT_MIPS_OPTIONS;
      hdr->sh_entsize = 1;
    }
  else if (strncmp (name, ".debug_", sizeof ".debug_" - 1) == 0)
    hdr->sh_type = SHT_MIPS_DWARF;
  else if (strncmp (name, ".MIPS.events.", sizeof ".MIPS.events." - 1) == 0)
    hdr->sh_type = SHT_MIPS_EVENTS;

  return true;
}

/* Given a BFD section, try to locate the corresponding ELF section
   index.  */

static boolean
mips_elf_section_from_bfd_section (abfd, hdr, sec, retval)
     bfd *abfd;
     Elf32_Internal_Shdr *hdr;
     asection *sec;
     int *retval;
{
  if (strcmp (bfd_get_section_name (abfd, sec), ".scommon") == 0)
    {
      *retval = SHN_MIPS_SCOMMON;
      return true;
    }
  if (strcmp (bfd_get_section_name (abfd, sec), ".acommon") == 0)
    {
      *retval = SHN_MIPS_ACOMMON;
      return true;
    }
  return false;
}

/* Work over a section just before writing it out.  We update the GP
   value in the .reginfo section based on the value we are using.
   FIXME: We recognize sections that need the SHF_MIPS_GPREL flag by
   name; there has to be a better way.  */

static boolean
mips_elf_section_processing (abfd, hdr)
     bfd *abfd;
     Elf32_Internal_Shdr *hdr;
{
  if (hdr->sh_type == SHT_MIPS_REGINFO)
    {
      bfd_byte buf[4];

      BFD_ASSERT (hdr->sh_size == sizeof (Elf32_External_RegInfo));
      BFD_ASSERT (hdr->contents == NULL);

      if (bfd_seek (abfd,
		    hdr->sh_offset + sizeof (Elf32_External_RegInfo) - 4,
		    SEEK_SET) == -1)
	return false;
      bfd_h_put_32 (abfd, (bfd_vma) elf_gp (abfd), buf);
      if (bfd_write (buf, (bfd_size_type) 1, (bfd_size_type) 4, abfd) != 4)
	return false;
    }

  if (hdr->bfd_section != NULL)
    {
      const char *name = bfd_get_section_name (abfd, hdr->bfd_section);

      if (strcmp (name, ".sdata") == 0)
	{
	  hdr->sh_flags |= SHF_ALLOC | SHF_WRITE | SHF_MIPS_GPREL;
	  hdr->sh_type = SHT_PROGBITS;
	}
      else if (strcmp (name, ".sbss") == 0)
	{
	  hdr->sh_flags |= SHF_ALLOC | SHF_WRITE | SHF_MIPS_GPREL;
	  hdr->sh_type = SHT_NOBITS;
	}
      else if (strcmp (name, ".lit8") == 0
	       || strcmp (name, ".lit4") == 0)
	{
	  hdr->sh_flags |= SHF_ALLOC | SHF_WRITE | SHF_MIPS_GPREL;
	  hdr->sh_type = SHT_PROGBITS;
	}
    }

  return true;
}

/* MIPS ELF uses two common sections.  One is the usual one, and the
   other is for small objects.  All the small objects are kept
   together, and then referenced via the gp pointer, which yields
   faster assembler code.  This is what we use for the small common
   section.  This approach is copied from ecoff.c.  */
static asection mips_elf_scom_section;
static asymbol mips_elf_scom_symbol;
static asymbol *mips_elf_scom_symbol_ptr;

/* MIPS ELF also uses an acommon section, which represents an
   allocated common symbol which may be overridden by a 	
   definition in a shared library.  */
static asection mips_elf_acom_section;
static asymbol mips_elf_acom_symbol;
static asymbol *mips_elf_acom_symbol_ptr;

/* Handle the special MIPS section numbers that a symbol may use.  */

static void
mips_elf_symbol_processing (abfd, asym)
     bfd *abfd;
     asymbol *asym;
{
  elf_symbol_type *elfsym;

  elfsym = (elf_symbol_type *) asym;
  switch (elfsym->internal_elf_sym.st_shndx)
    {
    case SHN_MIPS_ACOMMON:
      /* This section is used in a dynamically linked executable file.
	 It is an allocated common section.  The dynamic linker can
	 either resolve these symbols to something in a shared
	 library, or it can just leave them here.  For our purposes,
	 we can consider these symbols to be in a new section.  */
      if (mips_elf_acom_section.name == NULL)
	{
	  /* Initialize the acommon section.  */
	  mips_elf_acom_section.name = ".acommon";
	  mips_elf_acom_section.flags = SEC_ALLOC;
	  mips_elf_acom_section.output_section = &mips_elf_acom_section;
	  mips_elf_acom_section.symbol = &mips_elf_acom_symbol;
	  mips_elf_acom_section.symbol_ptr_ptr = &mips_elf_acom_symbol_ptr;
	  mips_elf_acom_symbol.name = ".acommon";
	  mips_elf_acom_symbol.flags = BSF_SECTION_SYM;
	  mips_elf_acom_symbol.section = &mips_elf_acom_section;
	  mips_elf_acom_symbol_ptr = &mips_elf_acom_symbol;
	}
      asym->section = &mips_elf_acom_section;
      break;

    case SHN_COMMON:
      /* Common symbols less than the GP size are automatically
	 treated as SHN_MIPS_SCOMMON symbols.  */
      if (asym->value > elf_gp_size (abfd))
	break;
      /* Fall through.  */
    case SHN_MIPS_SCOMMON:
      if (mips_elf_scom_section.name == NULL)
	{
	  /* Initialize the small common section.  */
	  mips_elf_scom_section.name = ".scommon";
	  mips_elf_scom_section.flags = SEC_IS_COMMON;
	  mips_elf_scom_section.output_section = &mips_elf_scom_section;
	  mips_elf_scom_section.symbol = &mips_elf_scom_symbol;
	  mips_elf_scom_section.symbol_ptr_ptr = &mips_elf_scom_symbol_ptr;
	  mips_elf_scom_symbol.name = ".scommon";
	  mips_elf_scom_symbol.flags = BSF_SECTION_SYM;
	  mips_elf_scom_symbol.section = &mips_elf_scom_section;
	  mips_elf_scom_symbol_ptr = &mips_elf_scom_symbol;
	}
      asym->section = &mips_elf_scom_section;
      asym->value = elfsym->internal_elf_sym.st_size;
      break;

    case SHN_MIPS_SUNDEFINED:
      asym->section = bfd_und_section_ptr;
      break;
    }
}

/* Read ECOFF debugging information from a .mdebug section into a
   ecoff_debug_info structure.  */

static boolean
mips_elf_read_ecoff_info (abfd, section, debug)
     bfd *abfd;
     asection *section;
     struct ecoff_debug_info *debug;
{
  HDRR *symhdr;
  const struct ecoff_debug_swap *swap;
  char *ext_hdr = NULL;

  swap = get_elf_backend_data (abfd)->elf_backend_ecoff_debug_swap;

  ext_hdr = (char *) malloc ((size_t) swap->external_hdr_size);
  if (ext_hdr == NULL && swap->external_hdr_size != 0)
    {
      bfd_set_error (bfd_error_no_memory);
      goto error_return;
    }

  if (bfd_get_section_contents (abfd, section, ext_hdr, (file_ptr) 0,
				swap->external_hdr_size)
      == false)
    goto error_return;

  symhdr = &debug->symbolic_header;
  (*swap->swap_hdr_in) (abfd, ext_hdr, symhdr);

  /* The symbolic header contains absolute file offsets and sizes to
     read.  */
#define READ(ptr, offset, count, size, type)				\
  if (symhdr->count == 0)						\
    debug->ptr = NULL;							\
  else									\
    {									\
      debug->ptr = (type) malloc ((size_t) (size * symhdr->count));	\
      if (debug->ptr == NULL)						\
	{								\
	  bfd_set_error (bfd_error_no_memory);				\
	  goto error_return;						\
	}								\
      if (bfd_seek (abfd, (file_ptr) symhdr->offset, SEEK_SET) != 0	\
	  || (bfd_read (debug->ptr, size, symhdr->count,		\
			abfd) != size * symhdr->count))			\
	goto error_return;						\
    }

  READ (line, cbLineOffset, cbLine, sizeof (unsigned char), unsigned char *);
  READ (external_dnr, cbDnOffset, idnMax, swap->external_dnr_size, PTR);
  READ (external_pdr, cbPdOffset, ipdMax, swap->external_pdr_size, PTR);
  READ (external_sym, cbSymOffset, isymMax, swap->external_sym_size, PTR);
  READ (external_opt, cbOptOffset, ioptMax, swap->external_opt_size, PTR);
  READ (external_aux, cbAuxOffset, iauxMax, sizeof (union aux_ext),
	union aux_ext *);
  READ (ss, cbSsOffset, issMax, sizeof (char), char *);
  READ (ssext, cbSsExtOffset, issExtMax, sizeof (char), char *);
  READ (external_fdr, cbFdOffset, ifdMax, swap->external_fdr_size, PTR);
  READ (external_rfd, cbRfdOffset, crfd, swap->external_rfd_size, PTR);
  READ (external_ext, cbExtOffset, iextMax, swap->external_ext_size, PTR);
#undef READ

  debug->fdr = NULL;
  debug->adjust = NULL;

  return true;

 error_return:
  if (ext_hdr != NULL)
    free (ext_hdr);
  if (debug->line != NULL)
    free (debug->line);
  if (debug->external_dnr != NULL)
    free (debug->external_dnr);
  if (debug->external_pdr != NULL)
    free (debug->external_pdr);
  if (debug->external_sym != NULL)
    free (debug->external_sym);
  if (debug->external_opt != NULL)
    free (debug->external_opt);
  if (debug->external_aux != NULL)
    free (debug->external_aux);
  if (debug->ss != NULL)
    free (debug->ss);
  if (debug->ssext != NULL)
    free (debug->ssext);
  if (debug->external_fdr != NULL)
    free (debug->external_fdr);
  if (debug->external_rfd != NULL)
    free (debug->external_rfd);
  if (debug->external_ext != NULL)
    free (debug->external_ext);
  return false;
}

/* MIPS ELF local labels start with '$', not 'L'.  */

/*ARGSUSED*/
static boolean
mips_elf_is_local_label (abfd, symbol)
     bfd *abfd;
     asymbol *symbol;
{
  return symbol->name[0] == '$';
}

/* MIPS ELF uses a special find_nearest_line routine in order the
   handle the ECOFF debugging information.  */

struct mips_elf_find_line
{
  struct ecoff_debug_info d;
  struct ecoff_find_line i;
};

static boolean
mips_elf_find_nearest_line (abfd, section, symbols, offset, filename_ptr,
			    functionname_ptr, line_ptr)
     bfd *abfd;
     asection *section;
     asymbol **symbols;
     bfd_vma offset;
     const char **filename_ptr;
     const char **functionname_ptr;
     unsigned int *line_ptr;
{
  asection *msec;

  msec = bfd_get_section_by_name (abfd, ".mdebug");
  if (msec != NULL)
    {
      flagword origflags;
      struct mips_elf_find_line *fi;
      const struct ecoff_debug_swap * const swap =
	get_elf_backend_data (abfd)->elf_backend_ecoff_debug_swap;

      /* If we are called during a link, mips_elf_final_link may have
	 cleared the SEC_HAS_CONTENTS field.  We force it back on here
	 if appropriate (which it normally will be).  */
      origflags = msec->flags;
      if (elf_section_data (msec)->this_hdr.sh_type != SHT_NOBITS)
	msec->flags |= SEC_HAS_CONTENTS;

      fi = elf_tdata (abfd)->find_line_info;
      if (fi == NULL)
	{
	  bfd_size_type external_fdr_size;
	  char *fraw_src;
	  char *fraw_end;
	  struct fdr *fdr_ptr;

	  fi = ((struct mips_elf_find_line *)
		bfd_alloc (abfd, sizeof (struct mips_elf_find_line)));
	  if (fi == NULL)
	    {
	      bfd_set_error (bfd_error_no_memory);
	      msec->flags = origflags;
	      return false;
	    }

	  memset (fi, 0, sizeof (struct mips_elf_find_line));

	  if (! mips_elf_read_ecoff_info (abfd, msec, &fi->d))
	    {
	      msec->flags = origflags;
	      return false;
	    }

	  /* Swap in the FDR information.  */
	  fi->d.fdr = ((struct fdr *)
		       bfd_alloc (abfd,
				  (fi->d.symbolic_header.ifdMax *
				   sizeof (struct fdr))));
	  if (fi->d.fdr == NULL)
	    {
	      bfd_set_error (bfd_error_no_memory);
	      msec->flags = origflags;
	      return false;
	    }
	  external_fdr_size = swap->external_fdr_size;
	  fdr_ptr = fi->d.fdr;
	  fraw_src = (char *) fi->d.external_fdr;
	  fraw_end = (fraw_src
		      + fi->d.symbolic_header.ifdMax * external_fdr_size);
	  for (; fraw_src < fraw_end; fraw_src += external_fdr_size, fdr_ptr++)
	    (*swap->swap_fdr_in) (abfd, (PTR) fraw_src, fdr_ptr);

	  elf_tdata (abfd)->find_line_info = fi;

	  /* Note that we don't bother to ever free this information.
             find_nearest_line is either called all the time, as in
             objdump -l, so the information should be saved, or it is
             rarely called, as in ld error messages, so the memory
             wasted is unimportant.  Still, it would probably be a
             good idea for free_cached_info to throw it away.  */
	}

      if (_bfd_ecoff_locate_line (abfd, section, offset, &fi->d, swap,
				  &fi->i, filename_ptr, functionname_ptr,
				  line_ptr))
	{
	  msec->flags = origflags;
	  return true;
	}

      msec->flags = origflags;
    }

  /* Fall back on the generic ELF find_nearest_line routine.  */

  return _bfd_elf_find_nearest_line (abfd, section, symbols, offset,
				     filename_ptr, functionname_ptr,
				     line_ptr);
}

/* The MIPS ELF linker needs additional information for each symbol in
   the global hash table.  */

struct mips_elf_link_hash_entry
{
  struct elf_link_hash_entry root;

  /* External symbol information.  */
  EXTR esym;
};

/* MIPS ELF linker hash table.  */

struct mips_elf_link_hash_table
{
  struct elf_link_hash_table root;
};

/* Look up an entry in a MIPS ELF linker hash table.  */

#define mips_elf_link_hash_lookup(table, string, create, copy, follow)	\
  ((struct mips_elf_link_hash_entry *)					\
   elf_link_hash_lookup (&(table)->root, (string), (create),		\
			 (copy), (follow)))

/* Traverse a MIPS ELF linker hash table.  */

#define mips_elf_link_hash_traverse(table, func, info)			\
  (elf_link_hash_traverse						\
   (&(table)->root,							\
    (boolean (*) PARAMS ((struct elf_link_hash_entry *, PTR))) (func),	\
    (info)))

/* Get the MIPS ELF linker hash table from a link_info structure.  */

#define mips_elf_hash_table(p) \
  ((struct mips_elf_link_hash_table *) ((p)->hash))

static boolean mips_elf_output_extsym
  PARAMS ((struct mips_elf_link_hash_entry *, PTR));

/* Create an entry in a MIPS ELF linker hash table.  */

static struct bfd_hash_entry *
mips_elf_link_hash_newfunc (entry, table, string)
     struct bfd_hash_entry *entry;
     struct bfd_hash_table *table;
     const char *string;
{
  struct mips_elf_link_hash_entry *ret =
    (struct mips_elf_link_hash_entry *) entry;

  /* Allocate the structure if it has not already been allocated by a
     subclass.  */
  if (ret == (struct mips_elf_link_hash_entry *) NULL)
    ret = ((struct mips_elf_link_hash_entry *)
	   bfd_hash_allocate (table,
			      sizeof (struct mips_elf_link_hash_entry)));
  if (ret == (struct mips_elf_link_hash_entry *) NULL)
    {
      bfd_set_error (bfd_error_no_memory);
      return (struct bfd_hash_entry *) ret;
    }

  /* Call the allocation method of the superclass.  */
  ret = ((struct mips_elf_link_hash_entry *)
	 _bfd_elf_link_hash_newfunc ((struct bfd_hash_entry *) ret,
				     table, string));
  if (ret != (struct mips_elf_link_hash_entry *) NULL)
    {
      /* Set local fields.  */
      memset (&ret->esym, 0, sizeof (EXTR));
      /* We use -2 as a marker to indicate that the information has
	 not been set.  -1 means there is no associated ifd.  */
      ret->esym.ifd = -2;
    }

  return (struct bfd_hash_entry *) ret;
}

/* Create a MIPS ELF linker hash table.  */

static struct bfd_link_hash_table *
mips_elf_link_hash_table_create (abfd)
     bfd *abfd;
{
  struct mips_elf_link_hash_table *ret;

  ret = ((struct mips_elf_link_hash_table *)
	 bfd_alloc (abfd, sizeof (struct mips_elf_link_hash_table)));
  if (ret == (struct mips_elf_link_hash_table *) NULL)
    {
      bfd_set_error (bfd_error_no_memory);
      return NULL;
    }

  if (! _bfd_elf_link_hash_table_init (&ret->root, abfd,
				       mips_elf_link_hash_newfunc))
    {
      bfd_release (abfd, ret);
      return NULL;
    }

  return &ret->root.root;
}

/* Hook called by the linker routine which adds symbols from an object
   file.  We must handle the special MIPS section numbers here.  */

/*ARGSUSED*/
static boolean
mips_elf_add_symbol_hook (abfd, info, sym, namep, flagsp, secp, valp)
     bfd *abfd;
     struct bfd_link_info *info;
     const Elf_Internal_Sym *sym;
     const char **namep;
     flagword *flagsp;
     asection **secp;
     bfd_vma *valp;
{
  switch (sym->st_shndx)
    {
    case SHN_COMMON:
      /* Common symbols less than the GP size are automatically
	 treated as SHN_MIPS_SCOMMON symbols.  */
      if (sym->st_size > elf_gp_size (abfd))
	break;
      /* Fall through.  */
    case SHN_MIPS_SCOMMON:
      *secp = bfd_make_section_old_way (abfd, ".scommon");
      (*secp)->flags |= SEC_IS_COMMON;
      *valp = sym->st_size;
      break;

    case SHN_MIPS_SUNDEFINED:
      *secp = bfd_und_section_ptr;
      break;
    }

  return true;
}

/* Structure used to pass information to mips_elf_output_extsym.  */

struct extsym_info
{
  bfd *abfd;
  struct bfd_link_info *info;
  struct ecoff_debug_info *debug;
  const struct ecoff_debug_swap *swap;
  boolean failed;
};

/* This routine is used to write out ECOFF debugging external symbol
   information.  It is called via mips_elf_link_hash_traverse.  The
   ECOFF external symbol information must match the ELF external
   symbol information.  Unfortunately, at this point we don't know
   whether a symbol is required by reloc information, so the two
   tables may wind up being different.  We must sort out the external
   symbol information before we can set the final size of the .mdebug
   section, and we must set the size of the .mdebug section before we
   can relocate any sections, and we can't know which symbols are
   required by relocation until we relocate the sections.
   Fortunately, it is relatively unlikely that any symbol will be
   stripped but required by a reloc.  In particular, it can not happen
   when generating a final executable.  */

static boolean
mips_elf_output_extsym (h, data)
     struct mips_elf_link_hash_entry *h;
     PTR data;
{
  struct extsym_info *einfo = (struct extsym_info *) data;
  boolean strip;

  if (h->root.indx == -2)
    strip = false;
  else if (((h->root.elf_link_hash_flags & ELF_LINK_HASH_DEF_DYNAMIC) != 0
	    || (h->root.elf_link_hash_flags & ELF_LINK_HASH_REF_DYNAMIC) != 0)
	   && (h->root.elf_link_hash_flags & ELF_LINK_HASH_DEF_REGULAR) == 0
	   && (h->root.elf_link_hash_flags & ELF_LINK_HASH_REF_REGULAR) == 0)
    strip = true;
  else if (einfo->info->strip == strip_all
	   || (einfo->info->strip == strip_some
	       && bfd_hash_lookup (einfo->info->keep_hash,
				   h->root.root.root.string,
				   false, false) == NULL))
    strip = true;
  else
    strip = false;

  if (strip)
    return true;

  if (h->esym.ifd == -2)
    {
      h->esym.jmptbl = 0;
      h->esym.cobol_main = 0;
      h->esym.weakext = 0;
      h->esym.reserved = 0;
      h->esym.ifd = ifdNil;
      h->esym.asym.value = 0;
      h->esym.asym.st = stGlobal;

      if (h->root.root.type != bfd_link_hash_defined
	  && h->root.root.type != bfd_link_hash_defweak)
	h->esym.asym.sc = scAbs;
      else
	{
	  asection *output_section;
	  const char *name;

	  output_section = h->root.root.u.def.section->output_section;
	  name = bfd_section_name (output_section->owner, output_section);
	
	  if (strcmp (name, ".text") == 0)
	    h->esym.asym.sc = scText;
	  else if (strcmp (name, ".data") == 0)
	    h->esym.asym.sc = scData;
	  else if (strcmp (name, ".sdata") == 0)
	    h->esym.asym.sc = scSData;
	  else if (strcmp (name, ".rodata") == 0
		   || strcmp (name, ".rdata") == 0)
	    h->esym.asym.sc = scRData;
	  else if (strcmp (name, ".bss") == 0)
	    h->esym.asym.sc = scBss;
	  else if (strcmp (name, ".sbss") == 0)
	    h->esym.asym.sc = scSBss;
	  else if (strcmp (name, ".init") == 0)
	    h->esym.asym.sc = scInit;
	  else if (strcmp (name, ".fini") == 0)
	    h->esym.asym.sc = scFini;
	  else
	    h->esym.asym.sc = scAbs;
	}

      h->esym.asym.reserved = 0;
      h->esym.asym.index = indexNil;
    }

  if (h->root.root.type == bfd_link_hash_common)
    h->esym.asym.value = h->root.root.u.c.size;
  else if (h->root.root.type == bfd_link_hash_defined
	   || h->root.root.type == bfd_link_hash_defweak)
    {
      asection *sec;

      if (h->esym.asym.sc == scCommon)
	h->esym.asym.sc = scBss;
      else if (h->esym.asym.sc == scSCommon)
	h->esym.asym.sc = scSBss;

      sec = h->root.root.u.def.section;
      h->esym.asym.value = (h->root.root.u.def.value
			    + sec->output_offset
			    + sec->output_section->vma);
    }

  if (! bfd_ecoff_debug_one_external (einfo->abfd, einfo->debug, einfo->swap,
				      h->root.root.root.string,
				      &h->esym))
    {
      einfo->failed = true;
      return false;
    }

  return true;
}

/* A comparison routine used to sort .gptab entries.  */

static int
gptab_compare (p1, p2)
     const PTR p1;
     const PTR p2;
{
  const Elf32_gptab *a1 = (const Elf32_gptab *) p1;
  const Elf32_gptab *a2 = (const Elf32_gptab *) p2;

  return a1->gt_entry.gt_g_value - a2->gt_entry.gt_g_value;
}

/* We need to use a special link routine to handle the .reginfo and
   the .mdebug sections.  We need to merge all instances of these
   sections together, not write them all out sequentially.  */

static boolean
mips_elf_final_link (abfd, info)
     bfd *abfd;
     struct bfd_link_info *info;
{
  asection **secpp;
  asection *o;
  struct bfd_link_order *p;
  asection *reginfo_sec, *mdebug_sec, *gptab_data_sec, *gptab_bss_sec;
  Elf32_RegInfo reginfo;
  struct ecoff_debug_info debug;
  const struct ecoff_debug_swap *swap
    = get_elf_backend_data (abfd)->elf_backend_ecoff_debug_swap;
  HDRR *symhdr = &debug.symbolic_header;
  PTR mdebug_handle = NULL;

  /* Drop the .options section, since it has special semantics which I
     haven't bothered to figure out.  */
  for (secpp = &abfd->sections; *secpp != NULL; secpp = &(*secpp)->next)
    {
      if (strcmp ((*secpp)->name, ".options") == 0)
	{
	  for (p = (*secpp)->link_order_head; p != NULL; p = p->next)
	    if (p->type == bfd_indirect_link_order)
	      p->u.indirect.section->flags &=~ SEC_HAS_CONTENTS;
	  (*secpp)->link_order_head = NULL;
	  *secpp = (*secpp)->next;
	  --abfd->section_count;
	  break;
	}
    }

  /* Go through the sections and collect the .reginfo and .mdebug
     information.  */
  reginfo_sec = NULL;
  mdebug_sec = NULL;
  gptab_data_sec = NULL;
  gptab_bss_sec = NULL;
  for (o = abfd->sections; o != (asection *) NULL; o = o->next)
    {
      if (strcmp (o->name, ".reginfo") == 0)
	{
	  memset (&reginfo, 0, sizeof reginfo);

	  /* We have found the .reginfo section in the output file.
	     Look through all the link_orders comprising it and merge
	     the information together.  */
	  for (p = o->link_order_head;
	       p != (struct bfd_link_order *) NULL;
	       p = p->next)
	    {
	      asection *input_section;
	      bfd *input_bfd;
	      Elf32_External_RegInfo ext;
	      Elf32_RegInfo sub;

	      if (p->type != bfd_indirect_link_order)
		{
		  if (p->type == bfd_fill_link_order)
		    continue;
		  abort ();
		}

	      input_section = p->u.indirect.section;
	      input_bfd = input_section->owner;

	      /* The linker emulation code has probably clobbered the
                 size to be zero bytes.  */
	      if (input_section->_raw_size == 0)
		input_section->_raw_size = sizeof (Elf32_External_RegInfo);

	      if (! bfd_get_section_contents (input_bfd, input_section,
					      (PTR) &ext,
					      (file_ptr) 0,
					      sizeof ext))
		return false;

	      bfd_mips_elf32_swap_reginfo_in (input_bfd, &ext, &sub);

	      reginfo.ri_gprmask |= sub.ri_gprmask;
	      reginfo.ri_cprmask[0] |= sub.ri_cprmask[0];
	      reginfo.ri_cprmask[1] |= sub.ri_cprmask[1];
	      reginfo.ri_cprmask[2] |= sub.ri_cprmask[2];
	      reginfo.ri_cprmask[3] |= sub.ri_cprmask[3];

	      /* ri_gp_value is set by the function
		 mips_elf_section_processing when the section is
		 finally written out.  */

	      /* Hack: reset the SEC_HAS_CONTENTS flag so that
		 elf_link_input_bfd ignores this section.  */
	      input_section->flags &=~ SEC_HAS_CONTENTS;
	    }

	  /* Force the section size to the value we want.  */
	  o->_raw_size = sizeof (Elf32_External_RegInfo);

	  /* Skip this section later on (I don't think this currently
	     matters, but someday it might).  */
	  o->link_order_head = (struct bfd_link_order *) NULL;

	  reginfo_sec = o;
	}

      if (strcmp (o->name, ".mdebug") == 0)
	{
	  struct extsym_info einfo;

	  /* We have found the .mdebug section in the output file.
	     Look through all the link_orders comprising it and merge
	     the information together.  */
	  symhdr->magic = swap->sym_magic;
	  /* FIXME: What should the version stamp be?  */
	  symhdr->vstamp = 0;
	  symhdr->ilineMax = 0;
	  symhdr->cbLine = 0;
	  symhdr->idnMax = 0;
	  symhdr->ipdMax = 0;
	  symhdr->isymMax = 0;
	  symhdr->ioptMax = 0;
	  symhdr->iauxMax = 0;
	  symhdr->issMax = 0;
	  symhdr->issExtMax = 0;
	  symhdr->ifdMax = 0;
	  symhdr->crfd = 0;
	  symhdr->iextMax = 0;

	  /* We accumulate the debugging information itself in the
	     debug_info structure.  */
	  debug.line = NULL;
	  debug.external_dnr = NULL;
	  debug.external_pdr = NULL;
	  debug.external_sym = NULL;
	  debug.external_opt = NULL;
	  debug.external_aux = NULL;
	  debug.ss = NULL;
	  debug.ssext = debug.ssext_end = NULL;
	  debug.external_fdr = NULL;
	  debug.external_rfd = NULL;
	  debug.external_ext = debug.external_ext_end = NULL;

	  mdebug_handle = bfd_ecoff_debug_init (abfd, &debug, swap, info);
	  if (mdebug_handle == (PTR) NULL)
	    return false;

	  for (p = o->link_order_head;
	       p != (struct bfd_link_order *) NULL;
	       p = p->next)
	    {
	      asection *input_section;
	      bfd *input_bfd;
	      const struct ecoff_debug_swap *input_swap;
	      struct ecoff_debug_info input_debug;
	      char *eraw_src;
	      char *eraw_end;

	      if (p->type != bfd_indirect_link_order)
		{
		  if (p->type == bfd_fill_link_order)
		    continue;
		  abort ();
		}

	      input_section = p->u.indirect.section;
	      input_bfd = input_section->owner;

	      if (bfd_get_flavour (input_bfd) != bfd_target_elf_flavour
		  || (get_elf_backend_data (input_bfd)
		      ->elf_backend_ecoff_debug_swap) == NULL)
		{
		  /* I don't know what a non MIPS ELF bfd would be
		     doing with a .mdebug section, but I don't really
		     want to deal with it.  */
		  continue;
		}

	      input_swap = (get_elf_backend_data (input_bfd)
			    ->elf_backend_ecoff_debug_swap);

	      BFD_ASSERT (p->size == input_section->_raw_size);

	      /* The ECOFF linking code expects that we have already
		 read in the debugging information and set up an
		 ecoff_debug_info structure, so we do that now.  */
	      if (! mips_elf_read_ecoff_info (input_bfd, input_section,
					      &input_debug))
		return false;

	      if (! (bfd_ecoff_debug_accumulate
		     (mdebug_handle, abfd, &debug, swap, input_bfd,
		      &input_debug, input_swap, info)))
		return false;

	      /* Loop through the external symbols.  For each one with
		 interesting information, try to find the symbol in
		 the linker global hash table and save the information
		 for the output external symbols.  */
	      eraw_src = input_debug.external_ext;
	      eraw_end = (eraw_src
			  + (input_debug.symbolic_header.iextMax
			     * input_swap->external_ext_size));
	      for (;
		   eraw_src < eraw_end;
		   eraw_src += input_swap->external_ext_size)
		{
		  EXTR ext;
		  const char *name;
		  struct mips_elf_link_hash_entry *h;

		  (*input_swap->swap_ext_in) (input_bfd, (PTR) eraw_src, &ext);
		  if (ext.asym.sc == scNil
		      || ext.asym.sc == scUndefined
		      || ext.asym.sc == scSUndefined)
		    continue;

		  name = input_debug.ssext + ext.asym.iss;
		  h = mips_elf_link_hash_lookup (mips_elf_hash_table (info),
						 name, false, false, true);
		  if (h == NULL || h->esym.ifd != -2)
		    continue;

		  if (ext.ifd != -1)
		    {
		      BFD_ASSERT (ext.ifd
				  < input_debug.symbolic_header.ifdMax);
		      ext.ifd = input_debug.ifdmap[ext.ifd];
		    }

		  h->esym = ext;
		}

	      /* Free up the information we just read.  */
	      free (input_debug.line);
	      free (input_debug.external_dnr);
	      free (input_debug.external_pdr);
	      free (input_debug.external_sym);
	      free (input_debug.external_opt);
	      free (input_debug.external_aux);
	      free (input_debug.ss);
	      free (input_debug.ssext);
	      free (input_debug.external_fdr);
	      free (input_debug.external_rfd);
	      free (input_debug.external_ext);

	      /* Hack: reset the SEC_HAS_CONTENTS flag so that
		 elf_link_input_bfd ignores this section.  */
	      input_section->flags &=~ SEC_HAS_CONTENTS;
	    }

	  /* Build the external symbol information.  */
	  einfo.abfd = abfd;
	  einfo.info = info;
	  einfo.debug = &debug;
	  einfo.swap = swap;
	  einfo.failed = false;
	  mips_elf_link_hash_traverse (mips_elf_hash_table (info),
				       mips_elf_output_extsym,
				       (PTR) &einfo);
	  if (einfo.failed)
	    return false;

	  /* Set the size of the .mdebug section.  */
	  o->_raw_size = bfd_ecoff_debug_size (abfd, &debug, swap);

	  /* Skip this section later on (I don't think this currently
	     matters, but someday it might).  */
	  o->link_order_head = (struct bfd_link_order *) NULL;

	  mdebug_sec = o;
	}

      if (strncmp (o->name, ".gptab.", sizeof ".gptab." - 1) == 0)
	{
	  const char *subname;
	  unsigned int c;
	  Elf32_gptab *tab;
	  Elf32_External_gptab *ext_tab;
	  unsigned int i;

	  /* The .gptab.sdata and .gptab.sbss sections hold
	     information describing how the small data area would
	     change depending upon the -G switch.  These sections
	     not used in executables files.  */
	  if (! info->relocateable)
	    {
	      asection **secpp;

	      for (p = o->link_order_head;
		   p != (struct bfd_link_order *) NULL;
		   p = p->next)
		{
		  asection *input_section;

		  if (p->type != bfd_indirect_link_order)
		    {
		      if (p->type == bfd_fill_link_order)
			continue;
		      abort ();
		    }

		  input_section = p->u.indirect.section;

		  /* Hack: reset the SEC_HAS_CONTENTS flag so that
		     elf_link_input_bfd ignores this section.  */
		  input_section->flags &=~ SEC_HAS_CONTENTS;
		}

	      /* Skip this section later on (I don't think this
		 currently matters, but someday it might).  */
	      o->link_order_head = (struct bfd_link_order *) NULL;

	      /* Really remove the section.  */
	      for (secpp = &abfd->sections;
		   *secpp != o;
		   secpp = &(*secpp)->next)
		;
	      *secpp = (*secpp)->next;
	      --abfd->section_count;

	      continue;
	    }

	  /* There is one gptab for initialized data, and one for
	     uninitialized data.  */
	  if (strcmp (o->name, ".gptab.sdata") == 0)
	    gptab_data_sec = o;
	  else if (strcmp (o->name, ".gptab.sbss") == 0)
	    gptab_bss_sec = o;
	  else
	    {
	      (*_bfd_error_handler)
		("%s: illegal section name `%s'",
		 bfd_get_filename (abfd), o->name);
	      bfd_set_error (bfd_error_nonrepresentable_section);
	      return false;
	    }

	  /* The linker script always combines .gptab.data and
	     .gptab.sdata into .gptab.sdata, and likewise for
	     .gptab.bss and .gptab.sbss.  It is possible that there is
	     no .sdata or .sbss section in the output file, in which
	     case we must change the name of the output section.  */
	  subname = o->name + sizeof ".gptab" - 1;
	  if (bfd_get_section_by_name (abfd, subname) == NULL)
	    {
	      if (o == gptab_data_sec)
		o->name = ".gptab.data";
	      else
		o->name = ".gptab.bss";
	      subname = o->name + sizeof ".gptab" - 1;
	      BFD_ASSERT (bfd_get_section_by_name (abfd, subname) != NULL);
	    }

	  /* Set up the first entry.  */
	  c = 1;
	  tab = (Elf32_gptab *) malloc (c * sizeof (Elf32_gptab));
	  if (tab == NULL)
	    {
	      bfd_set_error (bfd_error_no_memory);
	      return false;
	    }
	  tab[0].gt_header.gt_current_g_value = elf_gp_size (abfd);
	  tab[0].gt_header.gt_unused = 0;

	  /* Combine the input sections.  */
	  for (p = o->link_order_head;
	       p != (struct bfd_link_order *) NULL;
	       p = p->next)
	    {
	      asection *input_section;
	      bfd *input_bfd;
	      bfd_size_type size;
	      unsigned long last;
	      bfd_size_type gpentry;

	      if (p->type != bfd_indirect_link_order)
		{
		  if (p->type == bfd_fill_link_order)
		    continue;
		  abort ();
		}

	      input_section = p->u.indirect.section;
	      input_bfd = input_section->owner;

	      /* Combine the gptab entries for this input section one
		 by one.  We know that the input gptab entries are
		 sorted by ascending -G value.  */
	      size = bfd_section_size (input_bfd, input_section);
	      last = 0;
	      for (gpentry = sizeof (Elf32_External_gptab);
		   gpentry < size;
		   gpentry += sizeof (Elf32_External_gptab))
		{
		  Elf32_External_gptab ext_gptab;
		  Elf32_gptab int_gptab;
		  unsigned long val;
		  unsigned long add;
		  boolean exact;
		  unsigned int look;

		  if (! (bfd_get_section_contents
			 (input_bfd, input_section, (PTR) &ext_gptab,
			  gpentry, sizeof (Elf32_External_gptab))))
		    {
		      free (tab);
		      return false;
		    }

		  bfd_mips_elf32_swap_gptab_in (input_bfd, &ext_gptab,
						&int_gptab);
		  val = int_gptab.gt_entry.gt_g_value;
		  add = int_gptab.gt_entry.gt_bytes - last;

		  exact = false;
		  for (look = 1; look < c; look++)
		    {
		      if (tab[look].gt_entry.gt_g_value >= val)
			tab[look].gt_entry.gt_bytes += add;

		      if (tab[look].gt_entry.gt_g_value == val)
			exact = true;
		    }

		  if (! exact)
		    {
		      Elf32_gptab *new_tab;
		      unsigned int max;

		      /* We need a new table entry.  */
		      new_tab = ((Elf32_gptab *)
				  realloc ((PTR) tab,
					   (c + 1) * sizeof (Elf32_gptab)));
		      if (new_tab == NULL)
			{
			  bfd_set_error (bfd_error_no_memory);
			  free (tab);
			  return false;
			}
		      tab = new_tab;
		      tab[c].gt_entry.gt_g_value = val;
		      tab[c].gt_entry.gt_bytes = add;

		      /* Merge in the size for the next smallest -G
			 value, since that will be implied by this new
			 value.  */
		      max = 0;
		      for (look = 1; look < c; look++)
			{
			  if (tab[look].gt_entry.gt_g_value < val
			      && (max == 0
				  || (tab[look].gt_entry.gt_g_value
				      > tab[max].gt_entry.gt_g_value)))
			    max = look;
			}
		      if (max != 0)
			tab[c].gt_entry.gt_bytes +=
			  tab[max].gt_entry.gt_bytes;

		      ++c;
		    }

		  last = int_gptab.gt_entry.gt_bytes;
		}

	      /* Hack: reset the SEC_HAS_CONTENTS flag so that
		 elf_link_input_bfd ignores this section.  */
	      input_section->flags &=~ SEC_HAS_CONTENTS;
	    }

	  /* The table must be sorted by -G value.  */
	  if (c > 2)
	    qsort (tab + 1, c - 1, sizeof (tab[0]), gptab_compare);

	  /* Swap out the table.  */
	  ext_tab = ((Elf32_External_gptab *)
		     bfd_alloc (abfd, c * sizeof (Elf32_External_gptab)));
	  if (ext_tab == NULL)
	    {
	      bfd_set_error (bfd_error_no_memory);
	      free (tab);
	      return false;
	    }

	  for (i = 0; i < c; i++)
	    bfd_mips_elf32_swap_gptab_out (abfd, tab + i, ext_tab + i);
	  free (tab);

	  o->_raw_size = c * sizeof (Elf32_External_gptab);
	  o->contents = (bfd_byte *) ext_tab;

	  /* Skip this section later on (I don't think this currently
	     matters, but someday it might).  */
	  o->link_order_head = (struct bfd_link_order *) NULL;
	}
    }

  /* Get a value for the GP register.  */
  if (elf_gp (abfd) == 0)
    {
      struct bfd_link_hash_entry *h;

      h = bfd_link_hash_lookup (info->hash, "_gp", false, false, true);
      if (h != (struct bfd_link_hash_entry *) NULL
	  && h->type == bfd_link_hash_defined)
	elf_gp (abfd) = (h->u.def.value
			 + h->u.def.section->output_section->vma
			 + h->u.def.section->output_offset);
      else if (info->relocateable)
	{
	  bfd_vma lo;

	  /* Make up a value.  */
	  lo = (bfd_vma) -1;
	  for (o = abfd->sections; o != (asection *) NULL; o = o->next)
	    {
	      if (o->vma < lo
		  && (strcmp (o->name, ".sbss") == 0
		      || strcmp (o->name, ".sdata") == 0
		      || strcmp (o->name, ".lit4") == 0
		      || strcmp (o->name, ".lit8") == 0))
		lo = o->vma;
	    }
	  elf_gp (abfd) = lo + 0x8000;
	}
      else
	{
	  /* If the relocate_section function needs to do a reloc
	     involving the GP value, it should make a reloc_dangerous
	     callback to warn that GP is not defined.  */
	}
    }

  /* Invoke the regular ELF backend linker to do all the work.  */
  if (! bfd_elf32_bfd_final_link (abfd, info))
    return false;

  /* Now write out the computed sections.  */

  if (reginfo_sec != (asection *) NULL)
    {
      Elf32_External_RegInfo ext;

      bfd_mips_elf32_swap_reginfo_out (abfd, &reginfo, &ext);
      if (! bfd_set_section_contents (abfd, reginfo_sec, (PTR) &ext,
				      (file_ptr) 0, sizeof ext))
	return false;
    }

  if (mdebug_sec != (asection *) NULL)
    {
      BFD_ASSERT (abfd->output_has_begun);
      if (! bfd_ecoff_write_accumulated_debug (mdebug_handle, abfd, &debug,
					       swap, info,
					       mdebug_sec->filepos))
	return false;

      bfd_ecoff_debug_free (mdebug_handle, abfd, &debug, swap, info);
    }

  if (gptab_data_sec != (asection *) NULL)
    {
      if (! bfd_set_section_contents (abfd, gptab_data_sec,
				      gptab_data_sec->contents,
				      (file_ptr) 0,
				      gptab_data_sec->_raw_size))
	return false;
    }

  if (gptab_bss_sec != (asection *) NULL)
    {
      if (! bfd_set_section_contents (abfd, gptab_bss_sec,
				      gptab_bss_sec->contents,
				      (file_ptr) 0,
				      gptab_bss_sec->_raw_size))
	return false;
    }

  return true;
}

/* Handle a MIPS ELF HI16 reloc.  */

static void
mips_elf_relocate_hi16 (input_bfd, relhi, rello, contents, addend)
     bfd *input_bfd;
     Elf_Internal_Rela *relhi;
     Elf_Internal_Rela *rello;
     bfd_byte *contents;
     bfd_vma addend;
{
  bfd_vma insn;
  bfd_vma addlo;

  insn = bfd_get_32 (input_bfd, contents + relhi->r_offset);

  addlo = bfd_get_32 (input_bfd, contents + rello->r_offset);
  addlo &= 0xffff;

  addend += ((insn & 0xffff) << 16) + addlo;

  if ((addlo & 0x8000) != 0)
    addend -= 0x10000;
  if ((addend & 0x8000) != 0)
    addend += 0x10000;

  bfd_put_32 (input_bfd,
	      (insn & 0xffff0000) | ((addend >> 16) & 0xffff),
	      contents + relhi->r_offset);
}

/* Relocate a MIPS ELF section.  */

static boolean
mips_elf_relocate_section (output_bfd, info, input_bfd, input_section,
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
  size_t locsymcount;
  size_t extsymoff;
  Elf_Internal_Rela *rel;
  Elf_Internal_Rela *relend;

  symtab_hdr = &elf_tdata (input_bfd)->symtab_hdr;

  if (elf_bad_symtab (input_bfd))
    {
      locsymcount = symtab_hdr->sh_size / sizeof (Elf32_External_Sym);
      extsymoff = 0;
    }
  else
    {
      locsymcount = symtab_hdr->sh_info;
      extsymoff = symtab_hdr->sh_info;
    }

  rel = relocs;
  relend = relocs + input_section->reloc_count;
  for (; rel < relend; rel++)
    {
      int r_type;
      reloc_howto_type *howto;
      unsigned long r_symndx;
      bfd_vma addend;
      struct elf_link_hash_entry *h;
      asection *sec;
      Elf_Internal_Sym *sym;
      bfd_reloc_status_type r;

      r_type = ELF32_R_TYPE (rel->r_info);
      if (r_type < 0 || r_type >= (int) R_MIPS_max)
	{
	  bfd_set_error (bfd_error_bad_value);
	  return false;
	}
      howto = elf_mips_howto_table + r_type;

      r_symndx = ELF32_R_SYM (rel->r_info);

      /* Mix in the change in GP address for a GP relative reloc.  */
      if (r_type != R_MIPS_GPREL16
	  && r_type != R_MIPS_LITERAL
	  && r_type != R_MIPS_GPREL32)
	addend = 0;
      else
	{
	  if (elf_gp (output_bfd) == 0)
	    {
	      if (! ((*info->callbacks->reloc_dangerous)
		     (info,
		      "GP relative relocation when GP not defined",
		      input_bfd, input_section,
		      rel->r_offset)))
		return false;
	      /* Only give the error once per link.  */
	      elf_gp (output_bfd) = 4;
	    }

	  if (r_symndx < extsymoff
	      || (elf_bad_symtab (input_bfd)
		  && local_sections[r_symndx] != NULL))
	    {
	      /* This is a relocation against a section.  The current
		 addend in the instruction is the difference between
		 INPUT_SECTION->vma and the GP value of INPUT_BFD.  We
		 must change this to be the difference between the
		 final definition (which will end up in RELOCATION)
		 and the GP value of OUTPUT_BFD (which is in GP).  */
	      addend = elf_gp (input_bfd) - elf_gp (output_bfd);
	    }
	  else if (! info->relocateable)
	    {
	      /* We are doing a final link.  The current addend in the
		 instruction is simply the desired offset into the
		 symbol (normally zero).  We want the instruction to
		 hold the difference between the final definition of
		 the symbol (which will end up in RELOCATION) and the
		 GP value of OUTPUT_BFD (which is in GP).  */
	      addend = - elf_gp (output_bfd);
	    }
	  else
	    {
	      /* We are generating relocateable output, and we aren't
		 going to define this symbol, so we just leave the
		 instruction alone.  */
	      addend = 0;
	    }
	}

      h = NULL;
      sym = NULL;
      sec = NULL;
      if (info->relocateable)
	{
	  /* This is a relocateable link.  We don't have to change
	     anything, unless the reloc is against a section symbol,
	     in which case we have to adjust according to where the
	     section symbol winds up in the output section.  */
	  if (r_symndx >= locsymcount
	      || (elf_bad_symtab (input_bfd)
		  && local_sections[r_symndx] == NULL))
	    r = bfd_reloc_ok;
	  else
	    {
	      sym = local_syms + r_symndx;
	      if (ELF_ST_TYPE (sym->st_info) != STT_SECTION)
		r = bfd_reloc_ok;
	      else
		{
		  sec = local_sections[r_symndx];

		  /* It would be logical to add sym->st_value here,
		     but Irix 5 sometimes generates a garbage symbol
		     value.  */
		  addend += sec->output_offset;

		  /* If this is HI16 with an associated LO16, adjust
		     the addend accordingly.  Otherwise, just
		     relocate.  */
		  if (r_type != R_MIPS_HI16
		      || (rel + 1) >= relend
		      || ELF32_R_TYPE ((rel + 1)->r_info) != R_MIPS_LO16)
		    r = _bfd_relocate_contents (howto, input_bfd,
						addend,
						contents + rel->r_offset);
		  else
		    {
		      mips_elf_relocate_hi16 (input_bfd, rel, rel + 1,
					      contents, addend);
		      r = bfd_reloc_ok;
		    }
		}
	    }
	}
      else
	{
	  bfd_vma relocation;

	  /* This is a final link.  */
	  sym = NULL;
	  if (r_symndx < extsymoff
	      || (elf_bad_symtab (input_bfd)
		  && local_sections[r_symndx] != NULL))
	    {
	      sym = local_syms + r_symndx;
	      sec = local_sections[r_symndx];
	      relocation = (sec->output_section->vma
			    + sec->output_offset);

	      /* It would be logical to always add sym->st_value here,
		 but Irix 5 sometimes generates a garbage symbol
		 value.  */
	      if (ELF_ST_TYPE (sym->st_info) != STT_SECTION)
		relocation += sym->st_value;
	    }
	  else
	    {
	      long indx;

	      indx = r_symndx - extsymoff;
	      h = elf_sym_hashes (input_bfd)[indx];
	      if (h->root.type == bfd_link_hash_defined
		  || h->root.type == bfd_link_hash_defweak)
		{
		  sec = h->root.u.def.section;
		  relocation = (h->root.u.def.value
				+ sec->output_section->vma
				+ sec->output_offset);
		}
	      else if (h->root.type == bfd_link_hash_undefweak)
		relocation = 0;
	      else
		{
		  if (! ((*info->callbacks->undefined_symbol)
			 (info, h->root.root.string, input_bfd,
			  input_section, rel->r_offset)))
		    return false;
		  relocation = 0;
		}
	    }

	  if (r_type != R_MIPS_HI16
	      || (rel + 1) >= relend
	      || ELF32_R_TYPE ((rel + 1)->r_info) != R_MIPS_LO16)
	    r = _bfd_final_link_relocate (howto, input_bfd, input_section,
					  contents, rel->r_offset,
					  relocation, addend);
	  else
	    {
	      mips_elf_relocate_hi16 (input_bfd, rel, rel + 1,
				      contents, relocation + addend);
	      r = bfd_reloc_ok;
	    }
	}

      if (r != bfd_reloc_ok)
	{
	  switch (r)
	    {
	    default:
	    case bfd_reloc_outofrange:
	      abort ();
	    case bfd_reloc_overflow:
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
		      return false;
		    if (*name == '\0')
		      name = bfd_section_name (input_bfd, sec);
		  }
		if (! ((*info->callbacks->reloc_overflow)
		       (info, name, howto->name, (bfd_vma) 0,
			input_bfd, input_section, rel->r_offset)))
		  return false;
	      }
	      break;
	    }
	}
    }

  return true;
}

/* This is almost identical to bfd_generic_get_... except that some
   MIPS relocations need to be handled specially.  Sigh.  */
static bfd_byte *
elf32_mips_get_relocated_section_contents (abfd, link_info, link_order, data,
					   relocateable, symbols)
     bfd *abfd;
     struct bfd_link_info *link_info;
     struct bfd_link_order *link_order;
     bfd_byte *data;
     boolean relocateable;
     asymbol **symbols;
{
  /* Get enough memory to hold the stuff */
  bfd *input_bfd = link_order->u.indirect.section->owner;
  asection *input_section = link_order->u.indirect.section;

  long reloc_size = bfd_get_reloc_upper_bound (input_bfd, input_section);
  arelent **reloc_vector = NULL;
  long reloc_count;

  if (reloc_size < 0)
    goto error_return;

  reloc_vector = (arelent **) malloc (reloc_size);
  if (reloc_vector == NULL && reloc_size != 0)
    {
      bfd_set_error (bfd_error_no_memory);
      goto error_return;
    }

  /* read in the section */
  if (!bfd_get_section_contents (input_bfd,
				 input_section,
				 (PTR) data,
				 0,
				 input_section->_raw_size))
    goto error_return;

  /* We're not relaxing the section, so just copy the size info */
  input_section->_cooked_size = input_section->_raw_size;
  input_section->reloc_done = true;

  reloc_count = bfd_canonicalize_reloc (input_bfd,
					input_section,
					reloc_vector,
					symbols);
  if (reloc_count < 0)
    goto error_return;

  if (reloc_count > 0)
    {
      arelent **parent;
      /* for mips */
      int gp_found;
      bfd_vma gp = 0x12345678;	/* initialize just to shut gcc up */

      {
	struct bfd_hash_entry *h;
	struct bfd_link_hash_entry *lh;
	/* Skip all this stuff if we aren't mixing formats.  */
	if (abfd && input_bfd
	    && abfd->xvec == input_bfd->xvec)
	  lh = 0;
	else
	  {
	    h = bfd_hash_lookup (&link_info->hash->table, "_gp", false, false);
	    lh = (struct bfd_link_hash_entry *) h;
	  }
      lookup:
	if (lh)
	  {
	    switch (lh->type)
	      {
	      case bfd_link_hash_undefined:
	      case bfd_link_hash_undefweak:
	      case bfd_link_hash_common:
		gp_found = 0;
		break;
	      case bfd_link_hash_defined:
	      case bfd_link_hash_defweak:
		gp_found = 1;
		gp = lh->u.def.value;
		break;
	      case bfd_link_hash_indirect:
	      case bfd_link_hash_warning:
		lh = lh->u.i.link;
		/* @@FIXME  ignoring warning for now */
		goto lookup;
	      case bfd_link_hash_new:
	      default:
		abort ();
	      }
	  }
	else
	  gp_found = 0;
      }
      /* end mips */
      for (parent = reloc_vector; *parent != (arelent *) NULL;
	   parent++)
	{
	  char *error_message = (char *) NULL;
	  bfd_reloc_status_type r;

	  /* Specific to MIPS: Deal with relocation types that require
	     knowing the gp of the output bfd.  */
	  asymbol *sym = *(*parent)->sym_ptr_ptr;
	  if (bfd_is_abs_section (sym->section) && abfd)
	    {
	      /* The special_function wouldn't get called anyways.  */
	    }
	  else if (!gp_found)
	    {
	      /* The gp isn't there; let the special function code
		 fall over on its own.  */
	    }
	  else if ((*parent)->howto->special_function == mips_elf_gprel16_reloc)
	    {
	      /* bypass special_function call */
	      r = gprel16_with_gp (input_bfd, sym, *parent, input_section,
				   relocateable, (PTR) data, gp);
	      goto skip_bfd_perform_relocation;
	    }
	  /* end mips specific stuff */

	  r = bfd_perform_relocation (input_bfd,
				      *parent,
				      (PTR) data,
				      input_section,
				      relocateable ? abfd : (bfd *) NULL,
				      &error_message);
	skip_bfd_perform_relocation:

	  if (relocateable)
	    {
	      asection *os = input_section->output_section;

	      /* A partial link, so keep the relocs */
	      os->orelocation[os->reloc_count] = *parent;
	      os->reloc_count++;
	    }

	  if (r != bfd_reloc_ok)
	    {
	      switch (r)
		{
		case bfd_reloc_undefined:
		  if (!((*link_info->callbacks->undefined_symbol)
			(link_info, bfd_asymbol_name (*(*parent)->sym_ptr_ptr),
			 input_bfd, input_section, (*parent)->address)))
		    goto error_return;
		  break;
		case bfd_reloc_dangerous:
		  BFD_ASSERT (error_message != (char *) NULL);
		  if (!((*link_info->callbacks->reloc_dangerous)
			(link_info, error_message, input_bfd, input_section,
			 (*parent)->address)))
		    goto error_return;
		  break;
		case bfd_reloc_overflow:
		  if (!((*link_info->callbacks->reloc_overflow)
			(link_info, bfd_asymbol_name (*(*parent)->sym_ptr_ptr),
			 (*parent)->howto->name, (*parent)->addend,
			 input_bfd, input_section, (*parent)->address)))
		    goto error_return;
		  break;
		case bfd_reloc_outofrange:
		default:
		  abort ();
		  break;
		}

	    }
	}
    }
  if (reloc_vector != NULL)
    free (reloc_vector);
  return data;

error_return:
  if (reloc_vector != NULL)
    free (reloc_vector);
  return NULL;
}
#define bfd_elf32_bfd_get_relocated_section_contents elf32_mips_get_relocated_section_contents

/* ECOFF swapping routines.  These are used when dealing with the
   .mdebug section, which is in the ECOFF debugging format.  */
static const struct ecoff_debug_swap mips_elf_ecoff_debug_swap =
{
  /* Symbol table magic number.  */
  magicSym,
  /* Alignment of debugging information.  E.g., 4.  */
  4,
  /* Sizes of external symbolic information.  */
  sizeof (struct hdr_ext),
  sizeof (struct dnr_ext),
  sizeof (struct pdr_ext),
  sizeof (struct sym_ext),
  sizeof (struct opt_ext),
  sizeof (struct fdr_ext),
  sizeof (struct rfd_ext),
  sizeof (struct ext_ext),
  /* Functions to swap in external symbolic data.  */
  ecoff_swap_hdr_in,
  ecoff_swap_dnr_in,
  ecoff_swap_pdr_in,
  ecoff_swap_sym_in,
  ecoff_swap_opt_in,
  ecoff_swap_fdr_in,
  ecoff_swap_rfd_in,
  ecoff_swap_ext_in,
  _bfd_ecoff_swap_tir_in,
  _bfd_ecoff_swap_rndx_in,
  /* Functions to swap out external symbolic data.  */
  ecoff_swap_hdr_out,
  ecoff_swap_dnr_out,
  ecoff_swap_pdr_out,
  ecoff_swap_sym_out,
  ecoff_swap_opt_out,
  ecoff_swap_fdr_out,
  ecoff_swap_rfd_out,
  ecoff_swap_ext_out,
  _bfd_ecoff_swap_tir_out,
  _bfd_ecoff_swap_rndx_out,
  /* Function to read in symbolic data.  */
  mips_elf_read_ecoff_info
};

#define TARGET_LITTLE_SYM		bfd_elf32_littlemips_vec
#define TARGET_LITTLE_NAME		"elf32-littlemips"
#define TARGET_BIG_SYM			bfd_elf32_bigmips_vec
#define TARGET_BIG_NAME			"elf32-bigmips"
#define ELF_ARCH			bfd_arch_mips
#define ELF_MACHINE_CODE		EM_MIPS
#define ELF_MAXPAGESIZE			0x10000
#define elf_backend_collect		true
#define elf_info_to_howto		0
#define elf_info_to_howto_rel		mips_info_to_howto_rel
#define elf_backend_sym_is_global	mips_elf_sym_is_global
#define elf_backend_object_p		mips_elf_object_p
#define elf_backend_section_from_shdr	mips_elf_section_from_shdr
#define elf_backend_fake_sections	mips_elf_fake_sections
#define elf_backend_section_from_bfd_section \
					mips_elf_section_from_bfd_section
#define elf_backend_section_processing	mips_elf_section_processing
#define elf_backend_symbol_processing	mips_elf_symbol_processing
#define elf_backend_final_write_processing \
					mips_elf_final_write_processing
#define elf_backend_ecoff_debug_swap	&mips_elf_ecoff_debug_swap

#define bfd_elf32_bfd_is_local_label	mips_elf_is_local_label
#define bfd_elf32_find_nearest_line	mips_elf_find_nearest_line

#define bfd_elf32_bfd_link_hash_table_create \
					mips_elf_link_hash_table_create
#define bfd_elf32_bfd_final_link	mips_elf_final_link
#define elf_backend_relocate_section	mips_elf_relocate_section
#define elf_backend_add_symbol_hook	mips_elf_add_symbol_hook

#include "elf32-target.h"
