/* M32R ELF support for BFD.
   Copyright (C) 1996, 1997 Free Software Foundation, Inc.

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
along with this program; if not, write to the Free Software Foundation, Inc.,
59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#ifndef _ELF_M32R_H
#define _ELF_M32R_H

enum reloc_type
{
  R_M32R_NONE = 0,
  R_M32R_16,
  R_M32R_32,
  R_M32R_24,
  R_M32R_10_PCREL,
  R_M32R_18_PCREL,
  R_M32R_26_PCREL,
  R_M32R_HI16_ULO,
  R_M32R_HI16_SLO,
  R_M32R_LO16,
  R_M32R_SDA16,
  R_M32R_max
};

/* Processor specific section indices.  These sections do not actually
   exist.  Symbols with a st_shndx field corresponding to one of these
   values have a special meaning.  */

/* Small common symbol.  */
#define SHN_M32R_SCOMMON	0xff00

/* Processor specific section flags.  */

/* This section contains sufficient relocs to be relaxed.
   When relaxing, even relocs of branch instructions the assembler could
   complete must be present because relaxing may cause the branch target to
   move.  */
#define SHF_M32R_CAN_RELAX	0x10000000

#endif
