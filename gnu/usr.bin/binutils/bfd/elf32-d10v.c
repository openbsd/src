/* D10V-specific support for 32-bit ELF
   Copyright (C) 1996 Free Software Foundation, Inc.
   Contributed by Martin Hunt (hunt@cygnus.com).

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
/* #include "elf/d10v.h" */

static reloc_howto_type *bfd_elf32_bfd_reloc_type_lookup
  PARAMS ((bfd *abfd, bfd_reloc_code_real_type code));
static void d10v_info_to_howto_rel
  PARAMS ((bfd *, arelent *, Elf32_Internal_Rel *));


/* Use REL instead of RELA to save space */
#define USE_REL

enum reloc_type
{
  R_D10V_NONE = 0,
  R_D10V_10_PCREL_R,
  R_D10V_10_PCREL_L,
  R_D10V_16,
  R_D10V_18,
  R_D10V_18_PCREL,
  R_D10V_32,
  R_D10V_max
};

static reloc_howto_type elf_d10v_howto_table[] =
{
  /* This reloc does nothing.  */
  HOWTO (R_D10V_NONE,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_D10V_NONE",		/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 false),		/* pcrel_offset */

  /* An PC Relative 10-bit relocation, shifted by 2  */
  /* right container */
  HOWTO (R_D10V_10_PCREL_R,	/* type */
	 2,	                /* rightshift */
	 2,	                /* size (0 = byte, 1 = short, 2 = long) */
	 10,	                /* bitsize */
	 true,	                /* pc_relative */
	 0,	                /* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc, /* special_function */
	 "R_D10V_10_PCREL_R",	/* name */
	 false,	                /* partial_inplace */
	 0xff,		        /* src_mask */
	 0xff,   		/* dst_mask */
	 true),			/* pcrel_offset */

  /* An PC Relative 10-bit relocation, shifted by 2  */
  /* left container */
  HOWTO (R_D10V_10_PCREL_L,	/* type */
	 2,	                /* rightshift */
	 2,	                /* size (0 = byte, 1 = short, 2 = long) */
	 10,	                /* bitsize */
	 true,	                /* pc_relative */
	 15,	                /* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc, /* special_function */
	 "R_D10V_10_PCREL_L",	/* name */
	 false,	                /* partial_inplace */
	 0x07f8000,		        /* src_mask */
	 0x07f8000,   		/* dst_mask */
	 true),			/* pcrel_offset */

  /* A 16 bit absolute relocation */
  HOWTO (R_D10V_16,		/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_D10V_16",		/* name */
	 false,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* An 18 bit absolute relocation, right shifted 2 */
  HOWTO (R_D10V_18,		/* type */
	 2,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 18,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_D10V_18",		/* name */
	 false,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* A relative 18 bit relocation, right shifted by 2  */
  HOWTO (R_D10V_18_PCREL,	/* type */
	 2,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 18,			/* bitsize */
	 true,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_D10V_18_PCREL",	/* name */
	 false,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 true),			/* pcrel_offset */

  /* A 32 bit absolute relocation */
  HOWTO (R_D10V_32,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_D10V_32",		/* name */
	 false,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 false),		/* pcrel_offset */

};

/* Map BFD reloc types to D10V ELF reloc types.  */

struct d10v_reloc_map
{
  unsigned char bfd_reloc_val;
  unsigned char elf_reloc_val;
};

 static const struct d10v_reloc_map d10v_reloc_map[] =
{
  { BFD_RELOC_NONE, R_D10V_NONE, },
  { BFD_RELOC_D10V_10_PCREL_R, R_D10V_10_PCREL_R },
  { BFD_RELOC_D10V_10_PCREL_L, R_D10V_10_PCREL_L },
  { BFD_RELOC_16, R_D10V_16 },
  { BFD_RELOC_D10V_18, R_D10V_18 },
  { BFD_RELOC_D10V_18_PCREL, R_D10V_18_PCREL },
  { BFD_RELOC_32, R_D10V_32 },
};

static reloc_howto_type *
bfd_elf32_bfd_reloc_type_lookup (abfd, code)
     bfd *abfd;
     bfd_reloc_code_real_type code;
{
  unsigned int i;

  for (i = 0;
       i < sizeof (d10v_reloc_map) / sizeof (struct d10v_reloc_map);
       i++)
    {
      if (d10v_reloc_map[i].bfd_reloc_val == code)
	return &elf_d10v_howto_table[d10v_reloc_map[i].elf_reloc_val];
    }

  return NULL;
}

/* Set the howto pointer for an D10V ELF reloc.  */

static void
d10v_info_to_howto_rel (abfd, cache_ptr, dst)
     bfd *abfd;
     arelent *cache_ptr;
     Elf32_Internal_Rel *dst;
{
  unsigned int r_type;

  r_type = ELF32_R_TYPE (dst->r_info);
  BFD_ASSERT (r_type < (unsigned int) R_D10V_max);
  cache_ptr->howto = &elf_d10v_howto_table[r_type];
}

#define ELF_ARCH		bfd_arch_d10v
#define ELF_MACHINE_CODE	EM_CYGNUS_D10V
#define ELF_MAXPAGESIZE		0x1000

#define TARGET_BIG_SYM          bfd_elf32_d10v_vec
#define TARGET_BIG_NAME		"elf32-d10v"

#define elf_info_to_howto	0
#define elf_info_to_howto_rel	d10v_info_to_howto_rel
#define elf_backend_object_p	0
#define elf_backend_final_write_processing	0

#include "elf32-target.h"
