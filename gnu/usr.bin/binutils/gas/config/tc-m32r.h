/* tc-m32r.h -- Header file for tc-m32r.c.
   Copyright 1996, 1997, 1998, 1999, 2000, 2001
   Free Software Foundation, Inc.

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
   the Free Software Foundation, 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

#define TC_M32R

#ifndef BFD_ASSEMBLER
/* leading space so will compile with cc */
 #error M32R support requires BFD_ASSEMBLER
#endif

#define LISTING_HEADER "M32R GAS "

/* The target BFD architecture.  */
#define TARGET_ARCH bfd_arch_m32r

#define TARGET_FORMAT "elf32-m32r"

#define TARGET_BYTES_BIG_ENDIAN 1

/* call md_pcrel_from_section, not md_pcrel_from */
long md_pcrel_from_section PARAMS ((struct fix *, segT));
#define MD_PCREL_FROM_SECTION(FIXP, SEC) md_pcrel_from_section(FIXP, SEC)

/* Permit temporary numeric labels.  */
#define LOCAL_LABELS_FB 1

#define DIFF_EXPR_OK		/* .-foo gets turned into PC relative relocs */

/* We don't need to handle .word strangely.  */
#define WORKING_DOT_WORD

/* For 8 vs 16 vs 32 bit branch selection.  */
extern const struct relax_type md_relax_table[];
#define TC_GENERIC_RELAX_TABLE md_relax_table
#if 0
extern void m32r_prepare_relax_scan ();
#define md_prepare_relax_scan(fragP, address, aim, this_state, this_type) \
m32r_prepare_relax_scan (fragP, address, aim, this_state, this_type)
#else
extern long m32r_relax_frag PARAMS ((segT, fragS *, long));
#define md_relax_frag(segment, fragP, stretch) \
m32r_relax_frag (segment, fragP, stretch)
#endif
/* Account for nop if 32 bit insn falls on odd halfword boundary.  */
#define TC_CGEN_MAX_RELAX(insn, len) (6)

/* Fill in rs_align_code fragments.  */
extern void m32r_handle_align PARAMS ((fragS *));
#define HANDLE_ALIGN(f)  m32r_handle_align (f)

#define MAX_MEM_FOR_RS_ALIGN_CODE  (1 + 2 + 4)

#define MD_APPLY_FIX3
#define md_apply_fix3 gas_cgen_md_apply_fix3

#define obj_fix_adjustable(fixP) m32r_fix_adjustable(fixP)

/* After creating a fixup for an instruction operand, we need to check for
   HI16 relocs and queue them up for later sorting.  */
#define md_cgen_record_fixup_exp m32r_cgen_record_fixup_exp

#define TC_HANDLES_FX_DONE

#define tc_gen_reloc gas_cgen_tc_gen_reloc

#define tc_frob_file() m32r_frob_file ()
extern void m32r_frob_file PARAMS ((void));

/* When relaxing, we need to emit various relocs we otherwise wouldn't.  */
#define TC_FORCE_RELOCATION(fix) m32r_force_relocation (fix)
extern int m32r_force_relocation ();

/* Ensure insns at labels are aligned to 32 bit boundaries.  */
int m32r_fill_insn PARAMS ((int));
#define md_after_pass_hook()	m32r_fill_insn (1)
#define TC_START_LABEL(ch, ptr)	(ch == ':' && m32r_fill_insn (0))

/* Add extra M32R sections.  */
#define ELF_TC_SPECIAL_SECTIONS \
  { ".sdata",		SHT_PROGBITS,	SHF_ALLOC + SHF_WRITE }, \
  { ".sbss",		SHT_NOBITS,	SHF_ALLOC + SHF_WRITE },

#define md_cleanup                 m32r_elf_section_change_hook
#define md_elf_section_change_hook m32r_elf_section_change_hook
extern void m32r_elf_section_change_hook ();
