/* tc-ns32k.h -- Opcode table for National Semi 32k processor
   Copyright (C) 1987, 1992 Free Software Foundation, Inc.

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

#define TC_NS32K

#define TC_PCREL_ADJUST(F) md_pcrel_adjust(F)

#ifdef BFD_ASSEMBLER
#define NO_RELOC BFD_RELOC_NONE

#define TARGET_ARCH		bfd_arch_ns32k
#define TARGET_BYTES_BIG_ENDIAN	0

#ifndef TARGET_FORMAT
#define TARGET_FORMAT		"a.out-pc532-mach"
#endif

/* Experimental code. See write.c */
#define BFD_FAST_SECTION_FILL

#else
#define NO_RELOC 0
#endif

#define LOCAL_LABELS_FB 1

#include "bit_fix.h"

#define tc_aout_pre_write_hook(x)	{;}	/* not used */
#define tc_crawl_symbol_chain(a)	{;}	/* not used */
#define tc_headers_hook(a)		{;}	/* not used */

#ifdef SEQUENT_COMPATABILITY
#define DEF_MODEC 20
#define DEF_MODEL 21
#endif

#ifndef DEF_MODEC
#define DEF_MODEC 20
#endif

#ifndef DEF_MODEL
#define DEF_MODEL 20
#endif

#define MAX_ARGS 4
#define ARG_LEN 50

#define TC_CONS_FIX_NEW cons_fix_new_ns32k
extern void fix_new_ns32k_exp PARAMS((fragS *frag,
				   int where,
				   int size,
				   expressionS *exp,
				   int pcrel,
				   int pcrel_adjust,
				   int im_disp,
				   bit_fixS *bit_fixP,	/* really bit_fixS */
				   int bsr));


extern void fix_new_ns32k PARAMS ((fragS *frag,
				   int where,
				   int size,
				   struct symbol *add_symbol,
				   long offset,
				   int pcrel,
				   int pcrel_adjust,
				   int im_disp,
				   bit_fixS *bit_fixP,	/* really bit_fixS */
				   int bsr));

extern void cons_fix_new_ns32k PARAMS ((fragS *frag,
					int where,
					int size,
					expressionS *exp));

/* the NS32x32 has a non 0 nop instruction which should be used in alligns */
#define NOP_OPCODE 0xa2

#define md_operand(x)

extern const struct relax_type md_relax_table[];
#define TC_GENERIC_RELAX_TABLE md_relax_table

#define TC_FIX_TYPE struct { unsigned bsr : 1; }
#define fx_bsr tc_fix_data.bsr
#define TC_INIT_FIX_DATA(F)	((F)->tc_fix_data.bsr = 0)
