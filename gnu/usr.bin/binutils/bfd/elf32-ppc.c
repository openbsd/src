/* PowerPC-specific support for 32-bit ELF
   Copyright 1994, 1995 Free Software Foundation, Inc.
   Written by Ian Lance Taylor, Cygnus Support.

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

/* This file is based on a preliminary PowerPC ELF ABI.  The
   information may not match the final PowerPC ELF ABI.  It includes
   suggestions from the in-progress Embedded PowerPC ABI, and that
   information may also not match.  */

#include "bfd.h"
#include "sysdep.h"
#include "bfdlink.h"
#include "libbfd.h"
#include "elf-bfd.h"
#include "elf/ppc.h"

#define USE_RELA		/* we want RELA relocations, not REL */

/* PowerPC relocations defined by the ABIs */
enum ppc_reloc_type
{
  R_PPC_NONE			=   0,
  R_PPC_ADDR32			=   1,
  R_PPC_ADDR24			=   2,
  R_PPC_ADDR16			=   3,
  R_PPC_ADDR16_LO		=   4,
  R_PPC_ADDR16_HI		=   5,
  R_PPC_ADDR16_HA		=   6,
  R_PPC_ADDR14			=   7,
  R_PPC_ADDR14_BRTAKEN		=   8,
  R_PPC_ADDR14_BRNTAKEN		=   9,
  R_PPC_REL24			=  10,
  R_PPC_REL14			=  11,
  R_PPC_REL14_BRTAKEN		=  12,
  R_PPC_REL14_BRNTAKEN		=  13,
  R_PPC_GOT16			=  14,
  R_PPC_GOT16_LO		=  15,
  R_PPC_GOT16_HI		=  16,
  R_PPC_GOT16_HA		=  17,
  R_PPC_PLTREL24		=  18,
  R_PPC_COPY			=  19,
  R_PPC_GLOB_DAT		=  20,
  R_PPC_JMP_SLOT		=  21,
  R_PPC_RELATIVE		=  22,
  R_PPC_LOCAL24PC		=  23,
  R_PPC_UADDR32			=  24,
  R_PPC_UADDR16			=  25,
  R_PPC_REL32			=  26,
  R_PPC_PLT32			=  27,
  R_PPC_PLTREL32		=  28,
  R_PPC_PLT16_LO		=  29,
  R_PPC_PLT16_HI		=  30,
  R_PPC_PLT16_HA		=  31,
  R_PPC_SDAREL16		=  32,
  R_PPC_SECTOFF			=  33,
  R_PPC_SECTOFF_LO		=  34,
  R_PPC_SECTOFF_HI		=  35,
  R_PPC_SECTOFF_HA		=  36,

  /* The remaining relocs are from the Embedded ELF ABI, and are not
     in the SVR4 ELF ABI.  */
  R_PPC_EMB_NADDR32		= 101,
  R_PPC_EMB_NADDR16		= 102,
  R_PPC_EMB_NADDR16_LO		= 103,
  R_PPC_EMB_NADDR16_HI		= 104,
  R_PPC_EMB_NADDR16_HA		= 105,
  R_PPC_EMB_SDAI16		= 106,
  R_PPC_EMB_SDA2I16		= 107,
  R_PPC_EMB_SDA2REL		= 108,
  R_PPC_EMB_SDA21		= 109,
  R_PPC_EMB_MRKREF		= 110,
  R_PPC_EMB_RELSEC16		= 111,
  R_PPC_EMB_RELST_LO		= 112,
  R_PPC_EMB_RELST_HI		= 113,
  R_PPC_EMB_RELST_HA		= 114,
  R_PPC_EMB_BIT_FLD		= 115,
  R_PPC_EMB_RELSDA		= 116,
  R_PPC_max
};

static bfd_reloc_status_type ppc_elf_unsupported_reloc
  PARAMS ((bfd *, arelent *, asymbol *, PTR, asection *, bfd *, char **));
static bfd_reloc_status_type ppc_elf_std_reloc
  PARAMS ((bfd *, arelent *, asymbol *, PTR, asection *, bfd *, char **));

static bfd_vma ppc_elf_addr16_ha_inner PARAMS ((bfd_vma));
static bfd_reloc_status_type ppc_elf_addr16_ha_reloc
  PARAMS ((bfd *, arelent *, asymbol *, PTR, asection *, bfd *, char **));
static bfd_vma ppc_elf_got16_inner PARAMS ((asection *sec));
static bfd_reloc_status_type ppc_elf_got16_reloc
  PARAMS ((bfd *, arelent *, asymbol *, PTR, asection *, bfd *, char **));
static bfd_vma ppc_elf_brtaken_inner PARAMS ((bfd_vma, enum ppc_reloc_type));
static bfd_reloc_status_type ppc_elf_brtaken_reloc
  PARAMS ((bfd *, arelent *, asymbol *, PTR, asection *, bfd *, char **));
static reloc_howto_type *ppc_elf_reloc_type_lookup
  PARAMS ((bfd *abfd, bfd_reloc_code_real_type code));
static void ppc_elf_info_to_howto
  PARAMS ((bfd *abfd, arelent *cache_ptr, Elf32_Internal_Rela *dst));
static void ppc_elf_howto_init PARAMS ((void));
static boolean ppc_elf_set_private_flags PARAMS ((bfd *, flagword));
static boolean ppc_elf_copy_private_bfd_data PARAMS ((bfd *, bfd *));
static boolean ppc_elf_merge_private_bfd_data PARAMS ((bfd *, bfd *));

static boolean ppc_elf_relocate_section PARAMS ((bfd *,
						 struct bfd_link_info *info,
						 bfd *,
						 asection *,
						 bfd_byte *,
						 Elf_Internal_Rela *relocs,
						 Elf_Internal_Sym *local_syms,
						 asection **));

#define BRANCH_PREDICT_BIT 0x200000

static reloc_howto_type *ppc_elf_howto_table[ (int)R_PPC_max ];

static reloc_howto_type ppc_elf_howto_raw[] =
{
  /* This reloc does nothing.  */
  HOWTO (R_PPC_NONE,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 ppc_elf_std_reloc,	/* special_function */
	 "R_PPC_NONE",		/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 false),		/* pcrel_offset */

  /* A standard 32 bit relocation.  */
  HOWTO (R_PPC_ADDR32,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 ppc_elf_std_reloc,	/* special_function */
	 "R_PPC_ADDR32",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffffffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* An absolute 26 bit branch; the lower two bits must be zero.
     FIXME: we don't check that, we just clear them.  */
  HOWTO (R_PPC_ADDR24,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 26,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 ppc_elf_std_reloc,	/* special_function */
	 "R_PPC_ADDR24",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0x3fffffc,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* A standard 16 bit relocation.  */
  HOWTO (R_PPC_ADDR16,		/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 ppc_elf_std_reloc,	/* special_function */
	 "R_PPC_ADDR16",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* A 16 bit relocation without overflow.  */
  HOWTO (R_PPC_ADDR16_LO,	/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont,/* complain_on_overflow */
	 ppc_elf_std_reloc,	/* special_function */
	 "R_PPC_ADDR16_LO",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* The high order 16 bits of an address.  */
  HOWTO (R_PPC_ADDR16_HI,	/* type */
	 16,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 ppc_elf_std_reloc,	/* special_function */
	 "R_PPC_ADDR16_HI",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* The high order 16 bits of an address, plus 1 if the contents of
     the low 16 bits, treated as a signed number, is negative.	*/
  HOWTO (R_PPC_ADDR16_HA,	/* type */
	 16,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 ppc_elf_addr16_ha_reloc, /* special_function */
	 "R_PPC_ADDR16_HA",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* An absolute 16 bit branch; the lower two bits must be zero.
     FIXME: we don't check that, we just clear them.  */
  HOWTO (R_PPC_ADDR14,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 ppc_elf_std_reloc,	/* special_function */
	 "R_PPC_ADDR14",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xfffc,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* An absolute 16 bit branch, for which bit 10 should be set to
     indicate that the branch is expected to be taken.	The lower two
     bits must be zero.	 */
  HOWTO (R_PPC_ADDR14_BRTAKEN,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 ppc_elf_brtaken_reloc,	/* special_function */
	 "R_PPC_ADDR14_BRTAKEN",/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xfffc,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* An absolute 16 bit branch, for which bit 10 should be set to
     indicate that the branch is not expected to be taken.  The lower
     two bits must be zero.  */
  HOWTO (R_PPC_ADDR14_BRNTAKEN, /* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 ppc_elf_brtaken_reloc,	/* special_function */
	 "R_PPC_ADDR14_BRNTAKEN",/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xfffc,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* A relative 26 bit branch; the lower two bits must be zero.	 */
  HOWTO (R_PPC_REL24,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 26,			/* bitsize */
	 true,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 ppc_elf_std_reloc,	/* special_function */
	 "R_PPC_REL24",		/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0x3fffffc,		/* dst_mask */
	 true),			/* pcrel_offset */

  /* A relative 16 bit branch; the lower two bits must be zero.	 */
  HOWTO (R_PPC_REL14,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 true,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 ppc_elf_std_reloc,	/* special_function */
	 "R_PPC_REL14",		/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xfffc,		/* dst_mask */
	 true),			/* pcrel_offset */

  /* A relative 16 bit branch.	Bit 10 should be set to indicate that
     the branch is expected to be taken.  The lower two bits must be
     zero.  */
  HOWTO (R_PPC_REL14_BRTAKEN,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 true,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 ppc_elf_brtaken_reloc,	/* special_function */
	 "R_PPC_REL14_BRTAKEN",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xfffc,		/* dst_mask */
	 true),			/* pcrel_offset */

  /* A relative 16 bit branch.	Bit 10 should be set to indicate that
     the branch is not expected to be taken.  The lower two bits must
     be zero.  */
  HOWTO (R_PPC_REL14_BRNTAKEN,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 true,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 ppc_elf_brtaken_reloc,	/* special_function */
	 "R_PPC_REL14_BRNTAKEN",/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xfffc,		/* dst_mask */
	 true),			/* pcrel_offset */

  /* Like R_PPC_ADDR16, but referring to the GOT table entry for the
     symbol.  */
  HOWTO (R_PPC_GOT16,		/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 ppc_elf_got16_reloc,	/* special_function */
	 "R_PPC_GOT16",		/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Like R_PPC_ADDR16_LO, but referring to the GOT table entry for
     the symbol.  */
  HOWTO (R_PPC_GOT16_LO,	/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 ppc_elf_got16_reloc,	/* special_function */
	 "R_PPC_GOT16_LO",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Like R_PPC_ADDR16_HI, but referring to the GOT table entry for
     the symbol.  */
  HOWTO (R_PPC_GOT16_HI,	/* type */
	 16,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 ppc_elf_got16_reloc,	/* special_function */
	 "R_PPC_GOT16_HI",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		 /* pcrel_offset */

  /* Like R_PPC_ADDR16_HA, but referring to the GOT table entry for
     the symbol.  FIXME: Not supported.	 */
  HOWTO (R_PPC_GOT16_HA,	/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 ppc_elf_unsupported_reloc, /* special_function */
	 "R_PPC_GOT16_HA",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Like R_PPC_REL24, but referring to the procedure linkage table
     entry for the symbol.  FIXME: Not supported.  */
  HOWTO (R_PPC_PLTREL24,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 26,			/* bitsize */
	 true,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed,  /* complain_on_overflow */
	 ppc_elf_unsupported_reloc, /* special_function */
	 "R_PPC_PLTREL24",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0x3fffffc,		/* dst_mask */
	 true),			/* pcrel_offset */

  /* This is used only by the dynamic linker.  The symbol should exist
     both in the object being run and in some shared library.  The
     dynamic linker copies the data addressed by the symbol from the
     shared library into the object.  I have no idea what the purpose
     of this is.  */
  HOWTO (R_PPC_COPY,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 ppc_elf_unsupported_reloc,  /* special_function */
	 "R_PPC_COPY",		/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 false),		/* pcrel_offset */

  /* Like R_PPC_ADDR32, but used when setting global offset table
     entries.  */
  HOWTO (R_PPC_GLOB_DAT,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 ppc_elf_unsupported_reloc,  /* special_function */
	 "R_PPC_GLOB_DAT",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffffffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Marks a procedure linkage table entry for a symbol.  */
  HOWTO (R_PPC_JMP_SLOT,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 ppc_elf_unsupported_reloc,  /* special_function */
	 "R_PPC_JMP_SLOT",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 false),		/* pcrel_offset */

  /* Used only by the dynamic linker.  When the object is run, this
     longword is set to the load address of the object, plus the
     addend.  */
  HOWTO (R_PPC_RELATIVE,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 ppc_elf_unsupported_reloc,  /* special_function */
	 "R_PPC_RELATIVE",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffffffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Like R_PPC_REL24, but uses the value of the symbol within the
     object rather than the final value.  Normally used for
     _GLOBAL_OFFSET_TABLE_.  FIXME: Not supported.  */
  HOWTO (R_PPC_LOCAL24PC,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 26,			/* bitsize */
	 true,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 ppc_elf_unsupported_reloc, /* special_function */
	 "R_PPC_LOCAL24PC",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0x3fffffc,		/* dst_mask */
	 true),			/* pcrel_offset */

  /* Like R_PPC_ADDR32, but may be unaligned.  */
  HOWTO (R_PPC_UADDR32,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 ppc_elf_std_reloc,	/* special_function */
	 "R_PPC_UADDR32",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffffffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Like R_PPC_ADDR16, but may be unaligned.  */
  HOWTO (R_PPC_UADDR16,		/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 ppc_elf_std_reloc,	/* special_function */
	 "R_PPC_UADDR16",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* 32-bit PC relative */
  HOWTO (R_PPC_REL32,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 true,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 ppc_elf_std_reloc,	/* special_function */
	 "R_PPC_REL32",		/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffffffff,		/* dst_mask */
	 true),			/* pcrel_offset */

  /* 32-bit relocation to the symbol's procedure linkage table.
     FIXEME: not supported. */
  HOWTO (R_PPC_PLT32,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 ppc_elf_unsupported_reloc, /* special_function */
	 "R_PPC_PLT32",		/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 false),		/* pcrel_offset */

  /* 32-bit PC relative relocation to the symbol's procedure linkage table.
     FIXEME: not supported. */
  HOWTO (R_PPC_PLTREL32,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 true,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 ppc_elf_unsupported_reloc, /* special_function */
	 "R_PPC_PLTREL32",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 true),			/* pcrel_offset */

  /* Like R_PPC_ADDR16_LO, but referring to the PLT table entry for
     the symbol.  */
  HOWTO (R_PPC_PLT16_LO,	/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 ppc_elf_unsupported_reloc, /* special_function */
	 "R_PPC_PLT16_LO",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Like R_PPC_ADDR16_HI, but referring to the PLT table entry for
     the symbol.  */
  HOWTO (R_PPC_PLT16_HI,	/* type */
	 16,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 ppc_elf_unsupported_reloc, /* special_function */
	 "R_PPC_PLT16_HI",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		 /* pcrel_offset */

  /* Like R_PPC_ADDR16_HA, but referring to the PLT table entry for
     the symbol.  FIXME: Not supported.	 */
  HOWTO (R_PPC_PLT16_HA,	/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 ppc_elf_unsupported_reloc, /* special_function */
	 "R_PPC_PLT16_HA",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* A sign-extended 16 bit value relative to _SDA_BASE, for use with
     small data items.	*/
  HOWTO (R_PPC_SDAREL16,	/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 ppc_elf_got16_reloc,	/* special_function */
	 "R_PPC_SDAREL16",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* These next 4 relocations were added by Sun. */
  /* 32-bit section relative relocation. FIXME: not supported. */
  HOWTO (R_PPC_SECTOFF,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 true,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 ppc_elf_unsupported_reloc, /* special_function */
	 "R_PPC_SECTOFF",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 true),			/* pcrel_offset */

  /* 16-bit lower half section relative relocation. FIXME: not supported. */
  HOWTO (R_PPC_SECTOFF_LO,	  /* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 ppc_elf_unsupported_reloc, /* special_function */
	 "R_PPC_SECTOFF_LO",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* 16-bit upper half section relative relocation. FIXME: not supported. */
  HOWTO (R_PPC_SECTOFF_HI,	/* type */
	 16,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 ppc_elf_unsupported_reloc, /* special_function */
	 "R_PPC_SECTOFF_HI",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		 /* pcrel_offset */

  /* 16-bit upper half adjusted section relative relocation. FIXME: not supported. */
  HOWTO (R_PPC_SECTOFF_HA,	/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 ppc_elf_unsupported_reloc, /* special_function */
	 "R_PPC_SECTOFF_HA",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* The remaining relocs are from the Embedded ELF ABI, and are not
     in the SVR4 ELF ABI.  */

  /* 32 bit value resulting from the addend minus the symbol */
  HOWTO (R_PPC_EMB_NADDR32,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 ppc_elf_unsupported_reloc, /* special_function */
	 "R_PPC_EMB_NADDR32",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffffffff,		/* dst_mask */
	 false),		/* pcrel_offset */

/* 16 bit value resulting from the addend minus the symbol */
  HOWTO (R_PPC_EMB_NADDR16,	/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 ppc_elf_unsupported_reloc, /* special_function */
	 "R_PPC_EMB_NADDR16",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* 16 bit value resulting from the addend minus the symbol */
  HOWTO (R_PPC_EMB_NADDR16_LO,	/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont,/* complain_on_overflow */
	 ppc_elf_unsupported_reloc, /* special_function */
	 "R_PPC_EMB_ADDR16_LO",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* The high order 16 bits of the addend minus the symbol */
  HOWTO (R_PPC_EMB_NADDR16_HI,	/* type */
	 16,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 ppc_elf_unsupported_reloc, /* special_function */
	 "R_PPC_EMB_NADDR16_HI", /* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* The high order 16 bits of the result of the addend minus the address,
     plus 1 if the contents of the low 16 bits, treated as a signed number,
     is negative.  */
  HOWTO (R_PPC_EMB_NADDR16_HA,	/* type */
	 16,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 ppc_elf_unsupported_reloc, /* special_function */
	 "R_PPC_EMB_NADDR16_HA", /* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */
};


/* Initialize the ppc_elf_howto_table, so that linear accesses can be done.  */

static void
ppc_elf_howto_init ()
{
  unsigned int i, type;

  for (i = 0; i < sizeof (ppc_elf_howto_raw) / sizeof (ppc_elf_howto_raw[0]); i++)
    {
      type = ppc_elf_howto_raw[i].type;
      BFD_ASSERT (type < sizeof(ppc_elf_howto_table) / sizeof(ppc_elf_howto_table[0]));
      ppc_elf_howto_table[type] = &ppc_elf_howto_raw[i];
    }
}


static reloc_howto_type *
ppc_elf_reloc_type_lookup (abfd, code)
     bfd *abfd;
     bfd_reloc_code_real_type code;
{
  enum ppc_reloc_type ppc_reloc = R_PPC_NONE;

  if (!ppc_elf_howto_table[ R_PPC_ADDR32 ])	/* Initialize howto table if needed */
    ppc_elf_howto_init ();

  switch ((int)code)
    {
    default:
      return (reloc_howto_type *)NULL;

    case BFD_RELOC_NONE:		ppc_reloc = R_PPC_NONE;			break;
    case BFD_RELOC_32:			ppc_reloc = R_PPC_ADDR32;		break;
    case BFD_RELOC_PPC_BA26:		ppc_reloc = R_PPC_ADDR24;		break;
    case BFD_RELOC_16:			ppc_reloc = R_PPC_ADDR16;		break;
    case BFD_RELOC_LO16:		ppc_reloc = R_PPC_ADDR16_LO;		break;
    case BFD_RELOC_HI16:		ppc_reloc = R_PPC_ADDR16_HI;		break;
    case BFD_RELOC_HI16_S:		ppc_reloc = R_PPC_ADDR16_HA;		break;
    case BFD_RELOC_PPC_BA16:		ppc_reloc = R_PPC_ADDR14;		break;
    case BFD_RELOC_PPC_BA16_BRTAKEN:	ppc_reloc = R_PPC_ADDR14_BRTAKEN;	break;
    case BFD_RELOC_PPC_BA16_BRNTAKEN:	ppc_reloc = R_PPC_ADDR14_BRNTAKEN;	break;
    case BFD_RELOC_PPC_B26:		ppc_reloc = R_PPC_REL24;		break;
    case BFD_RELOC_PPC_B16:		ppc_reloc = R_PPC_REL14;		break;
    case BFD_RELOC_PPC_B16_BRTAKEN:	ppc_reloc = R_PPC_REL14_BRTAKEN;	break;
    case BFD_RELOC_PPC_B16_BRNTAKEN:	ppc_reloc = R_PPC_REL14_BRNTAKEN;	break;
    case BFD_RELOC_PPC_TOC16:		ppc_reloc = R_PPC_GOT16;		break;
    case BFD_RELOC_LO16_GOTOFF:		ppc_reloc = R_PPC_GOT16_LO;		break;
    case BFD_RELOC_HI16_GOTOFF:		ppc_reloc = R_PPC_GOT16_HI;		break;
    case BFD_RELOC_HI16_S_GOTOFF:	ppc_reloc = R_PPC_GOT16_HA;		break;
    case BFD_RELOC_24_PLT_PCREL:	ppc_reloc = R_PPC_PLTREL24;		break;
    case BFD_RELOC_PPC_COPY:		ppc_reloc = R_PPC_COPY;			break;
    case BFD_RELOC_PPC_GLOB_DAT:	ppc_reloc = R_PPC_GLOB_DAT;		break;
    case BFD_RELOC_PPC_LOCAL24PC:	ppc_reloc = R_PPC_LOCAL24PC;		break;
    case BFD_RELOC_32_PCREL:		ppc_reloc = R_PPC_REL32;		break;
    case BFD_RELOC_32_PLTOFF:		ppc_reloc = R_PPC_PLT32;		break;
    case BFD_RELOC_32_PLT_PCREL:	ppc_reloc = R_PPC_PLTREL32;		break;
    case BFD_RELOC_LO16_PLTOFF:		ppc_reloc = R_PPC_PLT16_LO;		break;
    case BFD_RELOC_HI16_PLTOFF:		ppc_reloc = R_PPC_PLT16_HI;		break;
    case BFD_RELOC_HI16_S_PLTOFF:	ppc_reloc = R_PPC_PLT16_HA;		break;
    case BFD_RELOC_GPREL16:		ppc_reloc = R_PPC_SDAREL16;		break;
    case BFD_RELOC_32_BASEREL:		ppc_reloc = R_PPC_SECTOFF;		break;
    case BFD_RELOC_LO16_BASEREL:	ppc_reloc = R_PPC_SECTOFF_LO;		break;
    case BFD_RELOC_HI16_BASEREL:	ppc_reloc = R_PPC_SECTOFF_HI;		break;
    case BFD_RELOC_HI16_S_BASEREL:	ppc_reloc = R_PPC_SECTOFF_HA;		break;
    case BFD_RELOC_CTOR:		ppc_reloc = R_PPC_ADDR32;		break;
    }

  return ppc_elf_howto_table[ (int)ppc_reloc ];
};

/* Set the howto pointer for a PowerPC ELF reloc.  */

static void
ppc_elf_info_to_howto (abfd, cache_ptr, dst)
     bfd *abfd;
     arelent *cache_ptr;
     Elf32_Internal_Rela *dst;
{
  if (!ppc_elf_howto_table[ R_PPC_ADDR32 ])	/* Initialize howto table if needed */
    ppc_elf_howto_init ();

  BFD_ASSERT (ELF32_R_TYPE (dst->r_info) < (unsigned int) R_PPC_max);
  cache_ptr->howto = ppc_elf_howto_table[ELF32_R_TYPE (dst->r_info)];
}

/* Function to set whether a module needs the -mrelocatable bit set. */

static boolean
ppc_elf_set_private_flags (abfd, flags)
     bfd *abfd;
     flagword flags;
{
  BFD_ASSERT (!elf_ppc_flags_init (abfd)
	      || elf_elfheader (abfd)->e_flags == flags);

  elf_elfheader (abfd)->e_flags = flags;
  elf_ppc_flags_init (abfd) = true;
  return true;
}

/* Copy backend specific data from one object module to another */
static boolean
ppc_elf_copy_private_bfd_data (ibfd, obfd)
     bfd *ibfd;
     bfd *obfd;
{
  /* This function is selected based on the input vector.  We only
     want to copy information over if the output BFD also uses Elf
     format.  */
  if (bfd_get_flavour (obfd) != bfd_target_elf_flavour)
    return true;

  BFD_ASSERT (!elf_ppc_flags_init (obfd)
	      || elf_elfheader (obfd)->e_flags == elf_elfheader (ibfd)->e_flags);

  elf_elfheader (obfd)->e_flags = elf_elfheader (ibfd)->e_flags;
  elf_ppc_flags_init (obfd) = true;
  return true;
}

/* Merge backend specific data from an object file to the output
   object file when linking */
static boolean
ppc_elf_merge_private_bfd_data (ibfd, obfd)
     bfd *ibfd;
     bfd *obfd;
{
  flagword old_flags;
  flagword new_flags;
  boolean error;

  /* Check if we have the same endianess */
  if (ibfd->xvec->byteorder_big_p != obfd->xvec->byteorder_big_p)
    {
      (*_bfd_error_handler)
	("%s: compiled for a %s endian system and target is %s endian.\n",
	 bfd_get_filename (ibfd),
	 (ibfd->xvec->byteorder_big_p) ? "big" : "little",
	 (obfd->xvec->byteorder_big_p) ? "big" : "little");

      bfd_set_error (bfd_error_wrong_format);
      return false;
    }

  /* This function is selected based on the input vector.  We only
     want to copy information over if the output BFD also uses Elf
     format.  */
  if (bfd_get_flavour (obfd) != bfd_target_elf_flavour)
    return true;

  new_flags = elf_elfheader (ibfd)->e_flags;
  old_flags = elf_elfheader (obfd)->e_flags;
  if (!elf_ppc_flags_init (obfd))	/* First call, no flags set */
    {
      elf_ppc_flags_init (obfd) = true;
      elf_elfheader (obfd)->e_flags = new_flags;
    }

  else if (new_flags == old_flags)	/* Compatible flags are ok */
    ;

  else					/* Incompatible flags */
    {
      /* Warn about -mrelocatable mismatch.  Allow -mrelocatable-lib to be linked
         with either.  */
      error = false;
      if ((new_flags & EF_PPC_RELOCATABLE) != 0
	  && (old_flags & (EF_PPC_RELOCATABLE | EF_PPC_RELOCATABLE_LIB)) == 0)
	{
	  error = true;
	  (*_bfd_error_handler)
	    ("%s: compiled with -mrelocatable and linked with modules compiled normally\n",
	     bfd_get_filename (ibfd));
	}
      else if ((new_flags & (EF_PPC_RELOCATABLE | EF_PPC_RELOCATABLE_LIB)) == 0
	       && (old_flags & EF_PPC_RELOCATABLE) != 0)
	{
	  error = true;
	  (*_bfd_error_handler)
	    ("%s: compiled normally and linked with modules compiled with -mrelocatable\n",
	     bfd_get_filename (ibfd));
	}
      else if ((new_flags & EF_PPC_RELOCATABLE_LIB) != 0)
	elf_elfheader (obfd)->e_flags |= EF_PPC_RELOCATABLE_LIB;

      new_flags &= ~ (EF_PPC_RELOCATABLE | EF_PPC_RELOCATABLE_LIB);
      old_flags &= ~ (EF_PPC_RELOCATABLE | EF_PPC_RELOCATABLE_LIB);

      /* Warn about eabi vs. V.4 mismatch */
      if ((new_flags & EF_PPC_EMB) != 0 && (old_flags & EF_PPC_EMB) == 0)
	{
	  new_flags &= ~EF_PPC_EMB;
	  error = true;
	  (*_bfd_error_handler)
	    ("%s: compiled for the eabi and linked with modules compiled for System V\n",
	     bfd_get_filename (ibfd));
	}
      else if ((new_flags & EF_PPC_EMB) == 0 && (old_flags & EF_PPC_EMB) != 0)
	{
	  old_flags &= ~EF_PPC_EMB;
	  error = true;
	  (*_bfd_error_handler)
	    ("%s: compiled for System V and linked with modules compiled for eabi\n",
	     bfd_get_filename (ibfd));
	}

      /* Warn about any other mismatches */
      if (new_flags != old_flags)
	{
	  error = true;
	  (*_bfd_error_handler)
	    ("%s: uses different e_flags (0x%lx) fields than previous modules (0x%lx)\n",
	     bfd_get_filename (ibfd), (long)new_flags, (long)old_flags);
	}

      if (error)
	{
	  bfd_set_error (bfd_error_bad_value);
	  return false;
	}
    }

  return true;
}


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
static bfd_reloc_status_type
ppc_elf_std_reloc (abfd,
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
      && (! reloc_entry->howto->partial_inplace || reloc_entry->addend == 0))
    {
      reloc_entry->address += input_section->output_offset;
      return bfd_reloc_ok;
    }

  return bfd_reloc_continue;
}

/* Don't pretend we can deal with unsupported relocs.  */

/*ARGSUSED*/
static bfd_reloc_status_type
ppc_elf_unsupported_reloc (abfd, reloc_entry, symbol, data, input_section,
			   output_bfd, error_message)
     bfd *abfd;
     arelent *reloc_entry;
     asymbol *symbol;
     PTR data;
     asection *input_section;
     bfd *output_bfd;
     char **error_message;
{
  BFD_ASSERT (reloc_entry->howto != (reloc_howto_type *)0);
  (*_bfd_error_handler)
    ("%s: Relocation %s (%d) is not currently supported.\n",
     bfd_get_filename (abfd),
     reloc_entry->howto->name,
     reloc_entry->howto->type);

  return bfd_reloc_notsupported;
}

/* Internal function to return the adjustment to the addend for relocations
   that return the upper 16 bits after sign extending the lower 16 bits, ie
   for use with a ADDIS instruction followed by a memory reference using the
   bottom 16 bits.  */

INLINE
static bfd_vma
ppc_elf_addr16_ha_inner (relocation)
     bfd_vma relocation;
{
  return (relocation & 0x8000) << 1;
}

/* Handle the ADDR16_HA reloc by adjusting the reloc addend.  */

/*ARGSUSED*/
static bfd_reloc_status_type
ppc_elf_addr16_ha_reloc (abfd, reloc_entry, symbol, data, input_section,
		         output_bfd, error_message)
     bfd *abfd;
     arelent *reloc_entry;
     asymbol *symbol;
     PTR data;
     asection *input_section;
     bfd *output_bfd;
     char **error_message;
{
  bfd_vma relocation;
  asection *sec;

  if (output_bfd != (bfd *) NULL)
    return ppc_elf_std_reloc (abfd, reloc_entry, symbol, data,
			      input_section, output_bfd, error_message);

  sec = symbol->section;
  relocation = (((bfd_is_com_section (sec)) ? 0 : symbol->value)
		+ sec->output_section->vma
		+ sec->output_offset
		+ reloc_entry->addend);

  reloc_entry->addend += ppc_elf_addr16_ha_inner (relocation);
  return bfd_reloc_continue;
}

/* Internal function to return the addjustment to the addend for GOT16
   entries */

INLINE
static bfd_vma
ppc_elf_got16_inner (sec)
     asection *sec;
{
  BFD_ASSERT (bfd_is_und_section (sec)
	      || strcmp (bfd_get_section_name (abfd, sec), ".got") == 0
	      || strcmp (bfd_get_section_name (abfd, sec), ".cgot") == 0
	      || strcmp (bfd_get_section_name (abfd, sec), ".sdata") == 0
	      || strcmp (bfd_get_section_name (abfd, sec), ".sbss") == 0)

  return -(sec->output_section->vma + 0x8000);
}

/* Handle the GOT16 reloc.  We want to use the offset within the .got
   section, not the actual VMA.  This is appropriate when generating
   an embedded ELF object, for which the .got section acts like the
   AIX .toc section.  When and if we support PIC code, we will have to
   change this, perhaps by switching off on the e_type field.  */

/*ARGSUSED*/
static bfd_reloc_status_type
ppc_elf_got16_reloc (abfd, reloc_entry, symbol, data, input_section,
		     output_bfd, error_message)
     bfd *abfd;
     arelent *reloc_entry;
     asymbol *symbol;
     PTR data;
     asection *input_section;
     bfd *output_bfd;
     char **error_message;
{
  if (output_bfd != (bfd *) NULL)
    return ppc_elf_std_reloc (abfd, reloc_entry, symbol, data,
			      input_section, output_bfd, error_message);

  reloc_entry->addend += ppc_elf_got16_inner (bfd_get_section (*reloc_entry->sym_ptr_ptr));
  return bfd_reloc_continue;
}

/* Internal function to return the adjustment for relocations that set the
   branch taken bit or branch not taken in B0 for conditional branches.
   The dst_mask for these relocations allows this bit to be set as part
   of the addend.  */

INLINE
static bfd_vma
ppc_elf_brtaken_inner (relocation, ppc_reloc)
     bfd_vma relocation;
     enum ppc_reloc_type ppc_reloc;
{
  if (ppc_reloc == R_PPC_ADDR14_BRTAKEN || ppc_reloc == R_PPC_REL14_BRTAKEN)
    return (relocation & 0x8000) ? 0 : BRANCH_PREDICT_BIT;	 /* branch taken */
  else
    return (relocation & 0x8000) ? BRANCH_PREDICT_BIT : 0;	/* branch not taken */
}

/* Handle the R_PPC_{ADDR,REL}14_BR{,N}TAKEN relocs by setting bit 10 to indicate
   whether the branch is taken or not.  */

/*ARGSUSED*/
static bfd_reloc_status_type
ppc_elf_brtaken_reloc (abfd, reloc_entry, symbol, data, input_section,
		       output_bfd, error_message)
     bfd *abfd;
     arelent *reloc_entry;
     asymbol *symbol;
     PTR data;
     asection *input_section;
     bfd *output_bfd;
     char **error_message;
{
  bfd_vma relocation;
  asection *sec;
  long insn;

  if (output_bfd != (bfd *) NULL)
    return ppc_elf_std_reloc (abfd, reloc_entry, symbol, data,
			      input_section, output_bfd, error_message);

  sec = symbol->section;
  relocation = (((bfd_is_com_section (sec)) ? 0 : symbol->value)
		+ sec->output_section->vma
		+ sec->output_offset
		+ reloc_entry->addend);

  /* Set the branch prediction bit */
  insn = bfd_get_32 (abfd, (bfd_byte *) data + reloc_entry->address);
  insn &= ~BRANCH_PREDICT_BIT;
  insn |= ppc_elf_brtaken_inner (relocation - reloc_entry->address,
				 (enum ppc_reloc_type)reloc_entry->howto->type);
  bfd_put_32 (abfd, insn, (bfd_byte *) data + reloc_entry->address);

  return bfd_reloc_continue;
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
ppc_elf_relocate_section (output_bfd, info, input_bfd, input_section,
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
  Elf_Internal_Shdr *symtab_hdr = &elf_tdata (input_bfd)->symtab_hdr;
  struct elf_link_hash_entry **sym_hashes = elf_sym_hashes (input_bfd);
  Elf_Internal_Rela *rel = relocs;
  Elf_Internal_Rela *relend = relocs + input_section->reloc_count;
  boolean ret = true;
  long insn;

#ifdef DEBUG
  fprintf (stderr, "ppc_elf_relocate_section called for %s section %s, %ld relocations%s\n",
	   bfd_get_filename (input_bfd),
	   bfd_section_name(input_bfd, input_section),
	   (long)input_section->reloc_count,
	   (info->relocateable) ? " (relocatable)" : "");
#endif

  if (!ppc_elf_howto_table[ R_PPC_ADDR32 ])	/* Initialize howto table if needed */
    ppc_elf_howto_init ();

  for (; rel < relend; rel++)
    {
      enum ppc_reloc_type r_type = (enum ppc_reloc_type)ELF32_R_TYPE (rel->r_info);
      bfd_vma offset = rel->r_offset;
      bfd_vma addend = rel->r_addend;
      bfd_reloc_status_type r = bfd_reloc_other;
      Elf_Internal_Sym *sym = (Elf_Internal_Sym *)0;
      asection *sec = (asection *)0;
      struct elf_link_hash_entry *h = (struct elf_link_hash_entry *)0;
      reloc_howto_type *howto;
      unsigned long r_symndx;
      bfd_vma relocation;

      /* Unknown relocation handling */
      if ((unsigned)r_type >= (unsigned)R_PPC_max || !ppc_elf_howto_table[(int)r_type])
	{
	  (*_bfd_error_handler)
	    ("%s: Unknown relocation type %d\n",
	     bfd_get_filename (input_bfd),
	     (int)r_type);

	  bfd_set_error (bfd_error_bad_value);
	  ret = false;
	  continue;
	}

      howto = ppc_elf_howto_table[(int)r_type];
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
		   howto->name,
		   (int)r_type,
		   r_symndx,
		   (long)offset,
		   (long)addend);
#endif
	  continue;
	}

      /* This is a final link.  */

      /* Complain about known relocation that are not yet supported */
      if (howto->special_function == ppc_elf_unsupported_reloc)
	{
	  (*_bfd_error_handler)
	    ("%s: Relocation %s (%d) is not currently supported.\n",
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
	  sec = local_sections[r_symndx];
	  relocation = (sec->output_section->vma
			+ sec->output_offset
			+ sym->st_value);
	}
      else
	{
	  h = sym_hashes[r_symndx - symtab_hdr->sh_info];
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
	  else if (info->shared)
	    relocation = 0;
	  else
	    {
	      (*info->callbacks->undefined_symbol)(info,
						   h->root.root.string,
						   input_bfd,
						   input_section,
						   rel->r_offset);
	      ret = false;
	      continue;
	    }
	}

      switch ((int)r_type)
	{
	default:
	  break;

	case (int)R_PPC_ADDR14_BRTAKEN:		/* branch prediction relocations */
	case (int)R_PPC_ADDR14_BRNTAKEN:
	case (int)R_PPC_REL14_BRTAKEN:
	case (int)R_PPC_REL14_BRNTAKEN:
	  BFD_ASSERT (sec != (asection *)0);
	  insn = bfd_get_32 (output_bfd, contents + offset);
	  insn &= ~BRANCH_PREDICT_BIT;
	  insn |= ppc_elf_brtaken_inner (relocation - offset, r_type);
	  bfd_put_32 (output_bfd, insn, contents + offset);
	  break;

	case (int)R_PPC_GOT16:			/* GOT16 relocations */
	case (int)R_PPC_GOT16_LO:
	case (int)R_PPC_GOT16_HI:
	case (int)R_PPC_SDAREL16:
	  BFD_ASSERT (sec != (asection *)0);
	  addend += ppc_elf_got16_inner (sec);
	  break;

	case (int)R_PPC_ADDR16_HA:		/* arithmetic adjust relocations */
	  BFD_ASSERT (sec != (asection *)0);
	  addend += ppc_elf_addr16_ha_inner (relocation + addend);
	  break;
	}


#ifdef DEBUG
      fprintf (stderr, "\ttype = %s (%d), symbol index = %ld, offset = %ld, addend = %ld\n",
	       howto->name,
	       (int)r_type,
	       r_symndx,
	       (long)offset,
	       (long)addend);
#endif

      r = _bfd_final_link_relocate (howto,
				    input_bfd,
				    input_section,
				    contents,
				    offset,
				    relocation,
				    addend);

      if (r != bfd_reloc_ok)
	{
	  ret = false;
	  switch (r)
	    {
	    default:
	      break;

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
		      break;

		    if (*name == '\0')
		      name = bfd_section_name (input_bfd, sec);
		  }

		(*info->callbacks->reloc_overflow)(info,
						   name,
						   howto->name,
						   (bfd_vma) 0,
						   input_bfd,
						   input_section,
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

#define TARGET_LITTLE_SYM	bfd_elf32_powerpcle_vec
#define TARGET_LITTLE_NAME	"elf32-powerpcle"
#define TARGET_BIG_SYM		bfd_elf32_powerpc_vec
#define TARGET_BIG_NAME		"elf32-powerpc"
#define ELF_ARCH		bfd_arch_powerpc
#define ELF_MACHINE_CODE	EM_PPC
#define ELF_MAXPAGESIZE		0x10000
#define elf_info_to_howto	ppc_elf_info_to_howto

#ifdef  EM_CYGNUS_POWERPC
#define ELF_MACHINE_ALT1	EM_CYGNUS_POWERPC
#endif

#ifdef EM_PPC_OLD
#define ELF_MACHINE_ALT2	EM_PPC_OLD
#endif

#define bfd_elf32_bfd_copy_private_bfd_data	ppc_elf_copy_private_bfd_data
#define bfd_elf32_bfd_merge_private_bfd_data	ppc_elf_merge_private_bfd_data
#define bfd_elf32_bfd_set_private_flags		ppc_elf_set_private_flags
#define bfd_elf32_bfd_reloc_type_lookup		ppc_elf_reloc_type_lookup
#define elf_backend_relocate_section		ppc_elf_relocate_section

#include "elf32-target.h"
