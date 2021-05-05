/* Generic support for 64-bit ELF
   Copyright 1993, 1995, 1998, 1999, 2001, 2002, 2004, 2005
   Free Software Foundation, Inc.

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
   Foundation, Inc., 51 Franklin Street - Fifth Floor, Boston, MA 02110-1301, USA.  */

#include "bfd.h"
#include "sysdep.h"
#include "libbfd.h"
#include "elf-bfd.h"

/* This does not include any relocation information, but should be
   good enough for GDB or objdump to read the file.  */

static reloc_howto_type dummy =
  HOWTO (0,			/* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 NULL,			/* special_function */
	 "UNKNOWN",		/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 FALSE);		/* pcrel_offset */

static void
elf_generic_info_to_howto (bfd *abfd ATTRIBUTE_UNUSED,
			   arelent *bfd_reloc,
			   Elf_Internal_Rela *elf_reloc ATTRIBUTE_UNUSED)
{
  bfd_reloc->howto = &dummy;
}

static void
elf_generic_info_to_howto_rel (bfd *abfd ATTRIBUTE_UNUSED,
			       arelent *bfd_reloc,
			       Elf_Internal_Rela *elf_reloc ATTRIBUTE_UNUSED)
{
  bfd_reloc->howto = &dummy;
}

#define TARGET_LITTLE_SYM		bfd_elf64_riscv_vec
#define TARGET_LITTLE_NAME		"elf64-riscv"
#define ELF_ARCH			bfd_arch_riscv64
#define ELF_MACHINE_CODE		EM_RISCV
#define ELF_MAXPAGESIZE			0x1000
#define bfd_elf64_bfd_reloc_type_lookup bfd_default_reloc_type_lookup
#define elf_info_to_howto		elf_generic_info_to_howto
#define elf_info_to_howto_rel		elf_generic_info_to_howto_rel

#include "elf64-target.h"
