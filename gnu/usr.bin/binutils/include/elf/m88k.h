/* MC88k ELF support for BFD.
   Copyright 1998, 1999, 2000 Free Software Foundation, Inc.

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
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#ifndef _ELF_M88K_H
#define _ELF_M88K_H

#include "elf/reloc-macros.h"

/* Relocation types.  */
START_RELOC_NUMBERS (elf_m88k_reloc_type)
  RELOC_NUMBER (R_88K_NONE, 0)
  RELOC_NUMBER (R_88K_COPY, 1)
  RELOC_NUMBER (R_88K_GOTP_ENT, 2)
  RELOC_NUMBER (R_88K_8, 4)
  RELOC_NUMBER (R_88K_8S, 5)
  RELOC_NUMBER (R_88K_16S, 7)
  RELOC_NUMBER (R_88K_DISP16, 8)
  RELOC_NUMBER (R_88K_DISP26, 10)
  RELOC_NUMBER (R_88K_PLT_DISP26, 14)
  RELOC_NUMBER (R_88K_BBASED_32, 16)
  RELOC_NUMBER (R_88K_BBASED_32UA, 17)
  RELOC_NUMBER (R_88K_BBASED_16H, 18)
  RELOC_NUMBER (R_88K_BBASED_16L, 19)
  RELOC_NUMBER (R_88K_ABDIFF_32, 24)
  RELOC_NUMBER (R_88K_ABDIFF_32UA, 25)
  RELOC_NUMBER (R_88K_ABDIFF_16H, 26)
  RELOC_NUMBER (R_88K_ABDIFF_16L, 27)
  RELOC_NUMBER (R_88K_ABDIFF_16, 28)
  RELOC_NUMBER (R_88K_32, 32)
  RELOC_NUMBER (R_88K_32UA, 33)
  RELOC_NUMBER (R_88K_16H, 34)
  RELOC_NUMBER (R_88K_16L, 35)
  RELOC_NUMBER (R_88K_16, 36)
  RELOC_NUMBER (R_88K_GOT_32, 40)
  RELOC_NUMBER (R_88K_GOT_32UA, 41)
  RELOC_NUMBER (R_88K_GOT_16H, 42)
  RELOC_NUMBER (R_88K_GOT_16L, 43)
  RELOC_NUMBER (R_88K_GOT_16, 44)
  RELOC_NUMBER (R_88K_GOTP_32, 48)
  RELOC_NUMBER (R_88K_GOTP_32UA, 49)
  RELOC_NUMBER (R_88K_GOTP_16H, 50)
  RELOC_NUMBER (R_88K_GOTP_16L, 51)
  RELOC_NUMBER (R_88K_GOTP_16, 52)
  RELOC_NUMBER (R_88K_PLT_32, 56)
  RELOC_NUMBER (R_88K_PLT_32UA, 57)
  RELOC_NUMBER (R_88K_PLT_16H, 58)
  RELOC_NUMBER (R_88K_PLT_16L, 59)
  RELOC_NUMBER (R_88K_PLT_16, 60)
  RELOC_NUMBER (R_88K_ABREL_32, 64)
  RELOC_NUMBER (R_88K_ABREL_32UA, 65)
  RELOC_NUMBER (R_88K_ABREL_16H, 66)
  RELOC_NUMBER (R_88K_ABREL_16L, 67)
  RELOC_NUMBER (R_88K_ABREL_16, 68)
  RELOC_NUMBER (R_88K_GOT_ABREL_32, 72)
  RELOC_NUMBER (R_88K_GOT_ABREL_32UA, 73)
  RELOC_NUMBER (R_88K_GOT_ABREL_16H, 74)
  RELOC_NUMBER (R_88K_GOT_ABREL_16L, 75)
  RELOC_NUMBER (R_88K_GOT_ABREL_16, 76)
  RELOC_NUMBER (R_88K_GOTP_ABREL_32, 80)
  RELOC_NUMBER (R_88K_GOTP_ABREL_32UA, 81)
  RELOC_NUMBER (R_88K_GOTP_ABREL_16H, 82)
  RELOC_NUMBER (R_88K_GOTP_ABREL_16L, 83)
  RELOC_NUMBER (R_88K_GOTP_ABREL_16, 84)
  RELOC_NUMBER (R_88K_PLT_ABREL_32, 88)
  RELOC_NUMBER (R_88K_PLT_ABREL_32UA, 89)
  RELOC_NUMBER (R_88K_PLT_ABREL_16H, 90)
  RELOC_NUMBER (R_88K_PLT_ABREL_16L, 91)
  RELOC_NUMBER (R_88K_PLT_ABREL_16, 92)
  RELOC_NUMBER (R_88K_SREL_32, 96)
  RELOC_NUMBER (R_88K_SREL_32UA, 97)
  RELOC_NUMBER (R_88K_SREL_16H, 98)
  RELOC_NUMBER (R_88K_SREL_16L, 99)
  /* These are GNU extensions to enable C++ vtable garbage collection. */
  RELOC_NUMBER (R_88K_GNU_VTINHERIT, 100)
  RELOC_NUMBER (R_88K_GNU_VTENTRY, 101)
END_RELOC_NUMBERS (R_88K_UNIMPLEMENTED)

/* Processor specific flags for the ELF header e_flags field.  */

#define	EF_NABI     0x80000000	/* not ABI compliant */
#define EF_M88110   0x00000004	/* used 88110-specific features */

/* Processor specific dynamic tag values.  */

#define	DT_88K_ADDRBASE	0x70000001
#define	DT_88K_PLTSTART	0x70000002
#define	DT_88K_PLTEND	0x70000003
#define	DT_88K_TDESC	0x70000004

#endif
