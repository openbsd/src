/* Matsushita 10300 specific support for 32-bit ELF
   Copyright (C) 1996 Free Software Foundation, Inc.

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

static reloc_howto_type *bfd_elf32_bfd_reloc_type_lookup
  PARAMS ((bfd *abfd, bfd_reloc_code_real_type code));
static void mn10300_info_to_howto_rel
  PARAMS ((bfd *, arelent *, Elf32_Internal_Rel *));

/* Try to minimize the amount of space occupied by relocation tables
   on the ROM (not that the ROM won't be swamped by other ELF overhead).  */
#define USE_REL

enum reloc_type
{
  R_MN10300_NONE = 0,
  R_MN10300_MAX
};

static reloc_howto_type elf_mn10300_howto_table[] =
{
  /* */
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
};

struct mn10300_reloc_map
{
  unsigned char bfd_reloc_val;
  unsigned char elf_reloc_val;
};

static const struct mn10300_reloc_map mn10300_reloc_map[] =
{
  { BFD_RELOC_NONE, R_MN10300_NONE, },
};

static reloc_howto_type *
bfd_elf32_bfd_reloc_type_lookup (abfd, code)
     bfd *abfd;
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

/* Set the howto pointer for an V850 ELF reloc.  */

static void
mn10300_info_to_howto_rel (abfd, cache_ptr, dst)
     bfd *abfd;
     arelent *cache_ptr;
     Elf32_Internal_Rel *dst;
{
  unsigned int r_type;

  r_type = ELF32_R_TYPE (dst->r_info);
  BFD_ASSERT (r_type < (unsigned int) R_MN10300_MAX);
  cache_ptr->howto = &elf_mn10300_howto_table[r_type];
}

#define TARGET_LITTLE_SYM	bfd_elf32_mn10300_vec
#define TARGET_LITTLE_NAME	"elf32-mn10300"
#define ELF_ARCH		bfd_arch_mn10300
#define ELF_MACHINE_CODE	EM_CYGNUS_MN10300
#define ELF_MAXPAGESIZE		0x1000

#define elf_info_to_howto	0
#define elf_info_to_howto_rel	mn10300_info_to_howto_rel

#include "elf32-target.h"
