/* Intel i860 specific support for 32-bit ELF.
   Copyright 1993, 1995, 1999, 2000, 2001, 2002
   Free Software Foundation, Inc.

   Full i860 support contributed by Jason Eckhardt <jle@cygnus.com>.

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
#include "elf/i860.h"

/* Prototypes.  */
static reloc_howto_type *lookup_howto
  PARAMS ((unsigned int));

static reloc_howto_type *elf32_i860_reloc_type_lookup
  PARAMS ((bfd *abfd, bfd_reloc_code_real_type code));

static void elf32_i860_info_to_howto_rela
  PARAMS ((bfd *, arelent *, Elf_Internal_Rela *));

static bfd_reloc_status_type elf32_i860_relocate_splitn
  PARAMS ((bfd *,  Elf_Internal_Rela *, bfd_byte *, bfd_vma));

static bfd_reloc_status_type elf32_i860_relocate_pc16
  PARAMS ((bfd *,  asection *, Elf_Internal_Rela *, bfd_byte *, bfd_vma));

static bfd_reloc_status_type elf32_i860_relocate_pc26
  PARAMS ((bfd *,  asection *, Elf_Internal_Rela *, bfd_byte *, bfd_vma));

static bfd_reloc_status_type elf32_i860_relocate_highadj
  PARAMS ((bfd *,  Elf_Internal_Rela *, bfd_byte *, bfd_vma));

static bfd_boolean elf32_i860_relocate_section
  PARAMS ((bfd *, struct bfd_link_info *, bfd *, asection *, bfd_byte *,
	   Elf_Internal_Rela *, Elf_Internal_Sym *, asection **));

static bfd_reloc_status_type i860_final_link_relocate
  PARAMS ((reloc_howto_type *, bfd *, asection *, bfd_byte *,
	   Elf_Internal_Rela *, bfd_vma));

static bfd_boolean elf32_i860_is_local_label_name
  PARAMS ((bfd *, const char *));

/* This howto table is preliminary.  */
static reloc_howto_type elf32_i860_howto_table [] =
{
  /* This relocation does nothing.  */
  HOWTO (R_860_NONE,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_860_NONE",		/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* A 32-bit absolute relocation.  */
  HOWTO (R_860_32,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_860_32",		/* name */
	 FALSE,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  HOWTO (R_860_COPY,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_860_COPY",		/* name */
	 TRUE,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  HOWTO (R_860_GLOB_DAT,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_860_GLOB_DAT",	/* name */
	 TRUE,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  HOWTO (R_860_JUMP_SLOT,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_860_JUMP_SLOT",	/* name */
	 TRUE,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  HOWTO (R_860_RELATIVE,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_860_RELATIVE",	/* name */
	 TRUE,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* A 26-bit PC-relative relocation.  */
  HOWTO (R_860_PC26,	        /* type */
	 2,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 26,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_860_PC26",		/* name */
	 FALSE,			/* partial_inplace */
	 0x3ffffff,		/* src_mask */
	 0x3ffffff,		/* dst_mask */
	 TRUE),		        /* pcrel_offset */

  HOWTO (R_860_PLT26,	        /* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 26,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_860_PLT26",		/* name */
	 TRUE,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 TRUE),		        /* pcrel_offset */

  /* A 16-bit PC-relative relocation.  */
  HOWTO (R_860_PC16,	        /* type */
	 2,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_860_PC16",		/* name */
	 FALSE,			/* partial_inplace */
	 0x1f07ff,		/* src_mask */
	 0x1f07ff,		/* dst_mask */
	 TRUE),		        /* pcrel_offset */

  HOWTO (R_860_LOW0,	        /* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_860_LOW0",		/* name */
	 FALSE,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),	        /* pcrel_offset */

  HOWTO (R_860_SPLIT0,	        /* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_860_SPLIT0",	/* name */
	 FALSE,			/* partial_inplace */
	 0x1f07ff,		/* src_mask */
	 0x1f07ff,		/* dst_mask */
	 FALSE),	        /* pcrel_offset */

  HOWTO (R_860_LOW1,	        /* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_860_LOW1",		/* name */
	 FALSE,			/* partial_inplace */
	 0xfffe,		/* src_mask */
	 0xfffe,		/* dst_mask */
	 FALSE),	        /* pcrel_offset */

  HOWTO (R_860_SPLIT1,	        /* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_860_SPLIT1",	/* name */
	 FALSE,			/* partial_inplace */
	 0x1f07fe,		/* src_mask */
	 0x1f07fe,		/* dst_mask */
	 FALSE),	        /* pcrel_offset */

  HOWTO (R_860_LOW2,	        /* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_860_LOW2",		/* name */
	 FALSE,			/* partial_inplace */
	 0xfffc,		/* src_mask */
	 0xfffc,		/* dst_mask */
	 FALSE),	        /* pcrel_offset */

  HOWTO (R_860_SPLIT2,	        /* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_860_SPLIT2",	/* name */
	 FALSE,			/* partial_inplace */
	 0x1f07fc,		/* src_mask */
	 0x1f07fc,		/* dst_mask */
	 FALSE),	        /* pcrel_offset */

  HOWTO (R_860_LOW3,	        /* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_860_LOW3",		/* name */
	 FALSE,			/* partial_inplace */
	 0xfff8,		/* src_mask */
	 0xfff8,		/* dst_mask */
	 FALSE),	        /* pcrel_offset */

  HOWTO (R_860_LOGOT0,	        /* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_860_LOGOT0",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 TRUE),		        /* pcrel_offset */

  HOWTO (R_860_SPGOT0,	        /* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_860_SPGOT0",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 TRUE),		        /* pcrel_offset */

  HOWTO (R_860_LOGOT1,	        /* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_860_LOGOT1",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 TRUE),		        /* pcrel_offset */

  HOWTO (R_860_SPGOT1,	        /* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_860_SPGOT1",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 TRUE),		        /* pcrel_offset */

  HOWTO (R_860_LOGOTOFF0,        /* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_860_LOGOTOFF0",	/* name */
	 TRUE,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 FALSE),	        /* pcrel_offset */

  HOWTO (R_860_SPGOTOFF0,        /* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_860_SPGOTOFF0",	/* name */
	 TRUE,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 FALSE),	        /* pcrel_offset */

  HOWTO (R_860_LOGOTOFF1,        /* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_860_LOGOTOFF1",	/* name */
	 TRUE,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 FALSE),	        /* pcrel_offset */

  HOWTO (R_860_SPGOTOFF1,       /* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_860_SPGOTOFF1",	/* name */
	 TRUE,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 FALSE),	        /* pcrel_offset */

  HOWTO (R_860_LOGOTOFF2,        /* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_860_LOGOTOFF2",	/* name */
	 TRUE,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 FALSE),	        /* pcrel_offset */

  HOWTO (R_860_LOGOTOFF3,        /* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_860_LOGOTOFF3",	/* name */
	 TRUE,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 FALSE),	        /* pcrel_offset */

  HOWTO (R_860_LOPC,	        /* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_860_LOPC",		/* name */
	 FALSE,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 TRUE),		        /* pcrel_offset */

  HOWTO (R_860_HIGHADJ,	        /* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_860_HIGHADJ",	/* name */
	 FALSE,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),	        /* pcrel_offset */

  HOWTO (R_860_HAGOT,	        /* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_860_HAGOT",		/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 TRUE),		        /* pcrel_offset */

  HOWTO (R_860_HAGOTOFF,        /* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_860_HAGOTOFF",	/* name */
	 TRUE,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 FALSE),	        /* pcrel_offset */

  HOWTO (R_860_HAPC,	        /* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_860_HAPC",		/* name */
	 FALSE,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 TRUE),		        /* pcrel_offset */

  HOWTO (R_860_HIGH,	        /* type */
	 16,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_860_HIGH",		/* name */
	 FALSE,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),	        /* pcrel_offset */

  HOWTO (R_860_HIGOT,	        /* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_860_HIGOT",		/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 TRUE),		        /* pcrel_offset */

  HOWTO (R_860_HIGOTOFF,        /* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_860_HIGOTOFF",	/* name */
	 TRUE,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 FALSE),	        /* pcrel_offset */
};

static unsigned char elf_code_to_howto_index[R_860_max + 1];

static reloc_howto_type *
lookup_howto (rtype)
     unsigned int rtype;
{
  static int initialized = 0;
  int i;
  int howto_tbl_size = (int) (sizeof (elf32_i860_howto_table)
			/ sizeof (elf32_i860_howto_table[0]));

  if (! initialized)
    {
      initialized = 1;
      memset (elf_code_to_howto_index, 0xff,
	      sizeof (elf_code_to_howto_index));
      for (i = 0; i < howto_tbl_size; i++)
        elf_code_to_howto_index[elf32_i860_howto_table[i].type] = i;
    }

  BFD_ASSERT (rtype <= R_860_max);
  i = elf_code_to_howto_index[rtype];
  if (i >= howto_tbl_size)
    return 0;
  return elf32_i860_howto_table + i;
}

/* Given a BFD reloc, return the matching HOWTO structure.  */
static reloc_howto_type *
elf32_i860_reloc_type_lookup (abfd, code)
     bfd * abfd ATTRIBUTE_UNUSED;
     bfd_reloc_code_real_type code;
{
  unsigned int rtype;

  switch (code)
    {
    case BFD_RELOC_NONE:
      rtype = R_860_NONE;
      break;
    case BFD_RELOC_32:
      rtype = R_860_32;
      break;
    case BFD_RELOC_860_COPY:
      rtype = R_860_COPY;
      break;
    case BFD_RELOC_860_GLOB_DAT:
      rtype = R_860_GLOB_DAT;
      break;
    case BFD_RELOC_860_JUMP_SLOT:
      rtype = R_860_JUMP_SLOT;
      break;
    case BFD_RELOC_860_RELATIVE:
      rtype = R_860_RELATIVE;
      break;
    case BFD_RELOC_860_PC26:
      rtype = R_860_PC26;
      break;
    case BFD_RELOC_860_PLT26:
      rtype = R_860_PLT26;
      break;
    case BFD_RELOC_860_PC16:
      rtype = R_860_PC16;
      break;
    case BFD_RELOC_860_LOW0:
      rtype = R_860_LOW0;
      break;
    case BFD_RELOC_860_SPLIT0:
      rtype = R_860_SPLIT0;
      break;
    case BFD_RELOC_860_LOW1:
      rtype = R_860_LOW1;
      break;
    case BFD_RELOC_860_SPLIT1:
      rtype = R_860_SPLIT1;
      break;
    case BFD_RELOC_860_LOW2:
      rtype = R_860_LOW2;
      break;
    case BFD_RELOC_860_SPLIT2:
      rtype = R_860_SPLIT2;
      break;
    case BFD_RELOC_860_LOW3:
      rtype = R_860_LOW3;
      break;
    case BFD_RELOC_860_LOGOT0:
      rtype = R_860_LOGOT0;
      break;
    case BFD_RELOC_860_SPGOT0:
      rtype = R_860_SPGOT0;
      break;
    case BFD_RELOC_860_LOGOT1:
      rtype = R_860_LOGOT1;
      break;
    case BFD_RELOC_860_SPGOT1:
      rtype = R_860_SPGOT1;
      break;
    case BFD_RELOC_860_LOGOTOFF0:
      rtype = R_860_LOGOTOFF0;
      break;
    case BFD_RELOC_860_SPGOTOFF0:
      rtype = R_860_SPGOTOFF0;
      break;
    case BFD_RELOC_860_LOGOTOFF1:
      rtype = R_860_LOGOTOFF1;
      break;
    case BFD_RELOC_860_SPGOTOFF1:
      rtype = R_860_SPGOTOFF1;
      break;
    case BFD_RELOC_860_LOGOTOFF2:
      rtype = R_860_LOGOTOFF2;
      break;
    case BFD_RELOC_860_LOGOTOFF3:
      rtype = R_860_LOGOTOFF3;
      break;
    case BFD_RELOC_860_LOPC:
      rtype = R_860_LOPC;
      break;
    case BFD_RELOC_860_HIGHADJ:
      rtype = R_860_HIGHADJ;
      break;
    case BFD_RELOC_860_HAGOT:
      rtype = R_860_HAGOT;
      break;
    case BFD_RELOC_860_HAGOTOFF:
      rtype = R_860_HAGOTOFF;
      break;
    case BFD_RELOC_860_HAPC:
      rtype = R_860_HAPC;
      break;
    case BFD_RELOC_860_HIGH:
      rtype = R_860_HIGH;
      break;
    case BFD_RELOC_860_HIGOT:
      rtype = R_860_HIGOT;
      break;
    case BFD_RELOC_860_HIGOTOFF:
      rtype = R_860_HIGOTOFF;
      break;
    default:
      rtype = 0;
      break;
    }
  return lookup_howto (rtype);
}

/* Given a ELF reloc, return the matching HOWTO structure.  */
static void
elf32_i860_info_to_howto_rela (abfd, bfd_reloc, elf_reloc)
     bfd *abfd ATTRIBUTE_UNUSED;
     arelent *bfd_reloc;
     Elf_Internal_Rela *elf_reloc;
{
  bfd_reloc->howto
    = lookup_howto ((unsigned) ELF32_R_TYPE (elf_reloc->r_info));
}

/* Specialized relocation handler for R_860_SPLITn.  These relocations
   involves a 16-bit field that is split into two contiguous parts.  */
static bfd_reloc_status_type
elf32_i860_relocate_splitn (input_bfd, rello, contents, value)
     bfd *input_bfd;
     Elf_Internal_Rela *rello;
     bfd_byte *contents;
     bfd_vma value;
{
  bfd_vma insn;
  reloc_howto_type *howto;
  howto = lookup_howto ((unsigned) ELF32_R_TYPE (rello->r_info));
  insn = bfd_get_32 (input_bfd, contents + rello->r_offset);

  /* Relocate.  */
  value += rello->r_addend;

  /* Separate the fields and insert.  */
  value = (((value & 0xf8) << 5) | (value & 0x7ff)) & howto->dst_mask;
  insn = (insn & ~howto->dst_mask) | value;

  bfd_put_32 (input_bfd, insn, contents + rello->r_offset);
  return bfd_reloc_ok;
}

/* Specialized relocation handler for R_860_PC16.  This relocation
   involves a 16-bit, PC-relative field that is split into two contiguous
   parts.  */
static bfd_reloc_status_type
elf32_i860_relocate_pc16 (input_bfd, input_section, rello, contents, value)
     bfd *input_bfd;
     asection *input_section;
     Elf_Internal_Rela *rello;
     bfd_byte *contents;
     bfd_vma value;
{
  bfd_vma insn;
  reloc_howto_type *howto;
  howto = lookup_howto ((unsigned) ELF32_R_TYPE (rello->r_info));
  insn = bfd_get_32 (input_bfd, contents + rello->r_offset);

  /* Adjust for PC-relative relocation.  */
  value -= (input_section->output_section->vma
	    + input_section->output_offset);
  value -= rello->r_offset;

  /* Relocate.  */
  value += rello->r_addend;

  /* Separate the fields and insert.  */
  value = (((value & 0xf8) << 5) | (value & 0x7ff)) & howto->dst_mask;
  insn = (insn & ~howto->dst_mask) | value;

  bfd_put_32 (input_bfd, insn, contents + rello->r_offset);
  return bfd_reloc_ok;

}

/* Specialized relocation handler for R_860_PC26.  This relocation
   involves a 26-bit, PC-relative field which must be adjusted by 4.  */
static bfd_reloc_status_type
elf32_i860_relocate_pc26 (input_bfd, input_section, rello, contents, value)
     bfd *input_bfd;
     asection *input_section;
     Elf_Internal_Rela *rello;
     bfd_byte *contents;
     bfd_vma value;
{
  bfd_vma insn;
  reloc_howto_type *howto;
  howto = lookup_howto ((unsigned) ELF32_R_TYPE (rello->r_info));
  insn = bfd_get_32 (input_bfd, contents + rello->r_offset);

  /* Adjust for PC-relative relocation.  */
  value -= (input_section->output_section->vma
	    + input_section->output_offset);
  value -= rello->r_offset;

  /* Relocate.  */
  value += rello->r_addend;

  /* Adjust value by 4 and insert the field.  */
  value = ((value - 4) >> howto->rightshift) & howto->dst_mask;
  insn = (insn & ~howto->dst_mask) | value;

  bfd_put_32 (input_bfd, insn, contents + rello->r_offset);
  return bfd_reloc_ok;

}

/* Specialized relocation handler for R_860_HIGHADJ.  */
static bfd_reloc_status_type
elf32_i860_relocate_highadj (input_bfd, rel, contents, value)
     bfd *input_bfd;
     Elf_Internal_Rela *rel;
     bfd_byte *contents;
     bfd_vma value;
{
  bfd_vma insn;

  insn = bfd_get_32 (input_bfd, contents + rel->r_offset);

  value += ((rel->r_addend & 0x8000) << 1);
  value += rel->r_addend;
  value = ((value >> 16) & 0xffff);

  insn = (insn & 0xffff0000) | value;

  bfd_put_32 (input_bfd, insn, contents + rel->r_offset);
  return bfd_reloc_ok;
}

/* Perform a single relocation.  By default we use the standard BFD
   routines. However, we handle some specially.  */
static bfd_reloc_status_type
i860_final_link_relocate (howto, input_bfd, input_section, contents, rel, relocation)
     reloc_howto_type *  howto;
     bfd *               input_bfd;
     asection *          input_section;
     bfd_byte *          contents;
     Elf_Internal_Rela * rel;
     bfd_vma             relocation;
{
  return _bfd_final_link_relocate (howto, input_bfd, input_section,
				   contents, rel->r_offset, relocation,
				   rel->r_addend);
}

/* Relocate an i860 ELF section.

   This is boiler-plate code copied from fr30.

   The RELOCATE_SECTION function is called by the new ELF backend linker
   to handle the relocations for a section.

   The relocs are always passed as Rela structures; if the section
   actually uses Rel structures, the r_addend field will always be
   zero.

   This function is responsible for adjusting the section contents as
   necessary, and (if using Rela relocs and generating a relocateable
   output file) adjusting the reloc addend as necessary.

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
static bfd_boolean
elf32_i860_relocate_section (output_bfd, info, input_bfd, input_section,
			     contents, relocs, local_syms, local_sections)
     bfd *output_bfd ATTRIBUTE_UNUSED;
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

  if (info->relocateable)
    return TRUE;

  symtab_hdr = & elf_tdata (input_bfd)->symtab_hdr;
  sym_hashes = elf_sym_hashes (input_bfd);
  relend     = relocs + input_section->reloc_count;

  for (rel = relocs; rel < relend; rel ++)
    {
      reloc_howto_type *           howto;
      unsigned long                r_symndx;
      Elf_Internal_Sym *           sym;
      asection *                   sec;
      struct elf_link_hash_entry * h;
      bfd_vma                      relocation;
      bfd_reloc_status_type        r;
      const char *                 name = NULL;
      int                          r_type;

      r_type = ELF32_R_TYPE (rel->r_info);

#if 0
      if (   r_type == R_860_GNU_VTINHERIT
	  || r_type == R_860_GNU_VTENTRY)
	continue;
#endif

      r_symndx = ELF32_R_SYM (rel->r_info);

      howto = lookup_howto ((unsigned) ELF32_R_TYPE (rel->r_info));
      h     = NULL;
      sym   = NULL;
      sec   = NULL;

      if (r_symndx < symtab_hdr->sh_info)
	{
	  sym = local_syms + r_symndx;
	  sec = local_sections [r_symndx];
	  relocation = _bfd_elf_rela_local_sym (output_bfd, sym, sec, rel);

	  name = bfd_elf_string_from_elf_section
	    (input_bfd, symtab_hdr->sh_link, sym->st_name);
	  name = (name == NULL) ? bfd_section_name (input_bfd, sec) : name;
	}
      else
	{
	  h = sym_hashes [r_symndx - symtab_hdr->sh_info];

	  while (h->root.type == bfd_link_hash_indirect
		 || h->root.type == bfd_link_hash_warning)
	    h = (struct elf_link_hash_entry *) h->root.u.i.link;

	  name = h->root.root.string;

	  if (h->root.type == bfd_link_hash_defined
	      || h->root.type == bfd_link_hash_defweak)
	    {
	      sec = h->root.u.def.section;
	      relocation = (h->root.u.def.value
			    + sec->output_section->vma
			    + sec->output_offset);
	    }
	  else if (h->root.type == bfd_link_hash_undefweak)
	    {
	      relocation = 0;
	    }
	  else
	    {
	      if (! ((*info->callbacks->undefined_symbol)
		     (info, h->root.root.string, input_bfd,
		      input_section, rel->r_offset, TRUE)))
		return FALSE;
	      relocation = 0;
	    }
	}

      switch (r_type)
	{
	default:
	  r = i860_final_link_relocate (howto, input_bfd, input_section,
					contents, rel, relocation);
	  break;

	case R_860_HIGHADJ:
	  r = elf32_i860_relocate_highadj (input_bfd, rel, contents,
					   relocation);
	  break;

	case R_860_PC16:
	  r = elf32_i860_relocate_pc16 (input_bfd, input_section, rel,
					contents, relocation);
	  break;

	case R_860_PC26:
	  r = elf32_i860_relocate_pc26 (input_bfd, input_section, rel,
					contents, relocation);
	  break;

	case R_860_SPLIT0:
	case R_860_SPLIT1:
	case R_860_SPLIT2:
	  r = elf32_i860_relocate_splitn (input_bfd, rel, contents,
					  relocation);
	  break;

	/* We do not yet handle GOT/PLT/Dynamic relocations.  */
	case R_860_COPY:
	case R_860_GLOB_DAT:
	case R_860_JUMP_SLOT:
	case R_860_RELATIVE:
	case R_860_PLT26:
	case R_860_LOGOT0:
	case R_860_SPGOT0:
	case R_860_LOGOT1:
	case R_860_SPGOT1:
	case R_860_LOGOTOFF0:
	case R_860_SPGOTOFF0:
	case R_860_LOGOTOFF1:
	case R_860_SPGOTOFF1:
	case R_860_LOGOTOFF2:
	case R_860_LOGOTOFF3:
	case R_860_LOPC:
	case R_860_HAGOT:
	case R_860_HAGOTOFF:
	case R_860_HAPC:
	case R_860_HIGOT:
	case R_860_HIGOTOFF:
	  r = bfd_reloc_notsupported;
	  break;
	}

      if (r != bfd_reloc_ok)
	{
	  const char * msg = (const char *) NULL;

	  switch (r)
	    {
	    case bfd_reloc_overflow:
	      r = info->callbacks->reloc_overflow
		(info, name, howto->name, (bfd_vma) 0,
		 input_bfd, input_section, rel->r_offset);
	      break;

	    case bfd_reloc_undefined:
	      r = info->callbacks->undefined_symbol
		(info, name, input_bfd, input_section, rel->r_offset, TRUE);
	      break;

	    case bfd_reloc_outofrange:
	      msg = _("internal error: out of range error");
	      break;

	    case bfd_reloc_notsupported:
	      msg = _("internal error: unsupported relocation error");
	      break;

	    case bfd_reloc_dangerous:
	      msg = _("internal error: dangerous relocation");
	      break;

	    default:
	      msg = _("internal error: unknown error");
	      break;
	    }

	  if (msg)
	    r = info->callbacks->warning
	      (info, msg, name, input_bfd, input_section, rel->r_offset);

	  if (! r)
	    return FALSE;
	}
    }

  return TRUE;
}

/* Return whether a symbol name implies a local label.  SVR4/860 compilers
   generate labels of the form ".ep.function_name" to denote the end of a
   function prolog. These should be local.
   ??? Do any other SVR4 compilers have this convention? If so, this should
   be added to the generic routine.  */
static bfd_boolean
elf32_i860_is_local_label_name (abfd, name)
     bfd *abfd;
     const char *name;
{
  if (name[0] == '.' && name[1] == 'e' && name[2] == 'p' && name[3] == '.')
    return TRUE;

  return _bfd_elf_is_local_label_name (abfd, name);
}

#define TARGET_BIG_SYM		bfd_elf32_i860_vec
#define TARGET_BIG_NAME		"elf32-i860"
#define TARGET_LITTLE_SYM	bfd_elf32_i860_little_vec
#define TARGET_LITTLE_NAME	"elf32-i860-little"
#define ELF_ARCH		bfd_arch_i860
#define ELF_MACHINE_CODE	EM_860
#define ELF_MAXPAGESIZE		4096

#define elf_backend_rela_normal			1
#define elf_info_to_howto_rel                   NULL
#define elf_info_to_howto			elf32_i860_info_to_howto_rela
#define elf_backend_relocate_section		elf32_i860_relocate_section
#define bfd_elf32_bfd_reloc_type_lookup		elf32_i860_reloc_type_lookup
#define bfd_elf32_bfd_is_local_label_name	elf32_i860_is_local_label_name

#include "elf32-target.h"
