/* This file is tc-h8300.h

   Copyright (C) 1987-1992 Free Software Foundation, Inc.

   This file is part of GAS, the GNU Assembler.

   GAS is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   GAS is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GAS; see the file COPYING.  If not, write to
   the Free Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */


#define TC_H8300

#define TARGET_BYTES_BIG_ENDIAN 1

/* This macro translates between an internal fix and an coff reloc type */
#define TC_COFF_FIX2RTYPE(fixP) abort();

#define BFD_ARCH bfd_arch_h8300
#define COFF_MAGIC Hmode ? 0x8301 : 0x8300
#define TC_COUNT_RELOC(x) (1)
#define IGNORE_NONSTANDARD_ESCAPES

#define tc_coff_symbol_emit_hook(a) ; /* not used */
#define TC_RELOC_MANGLE(s,a,b,c) tc_reloc_mangle(a,b,c)
extern void tc_reloc_mangle ();

#define TC_CONS_RELOC          (Hmode ? R_RELLONG: R_RELWORD)

#define DO_NOT_STRIP 0
#define DO_STRIP 0
#define LISTING_HEADER "Hitachi H8/300 GAS "
#define NEED_FX_R_TYPE 1
#define RELOC_32 1234

extern int Hmode;

#define md_operand(x)

/* end of tc-h8300.h */
