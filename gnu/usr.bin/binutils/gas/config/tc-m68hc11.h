/* tc-m68hc11.h -- Header file for tc-m68hc11.c.
   Copyright 1999, 2000, 2001 Free Software Foundation, Inc.

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
   along with GAS; see the file COPYING.  If not, write to the Free
   Software Foundation, 59 Temple Place - Suite 330, Boston, MA
   02111-1307, USA.  */

#define TC_M68HC11
#define TC_M68HC12

#ifdef ANSI_PROTOTYPES
struct fix;
#endif

/* Define TC_M68K so that we can use the MRI mode.  */
#define TC_M68K

#define TARGET_BYTES_BIG_ENDIAN 1

/* Motorola assembler specs does not require '.' before pseudo-ops.  */
#define NO_PSEUDO_DOT 1

#if 0
/* Treat the single quote as a string delimiter.
   ??? This does not work at all.  */
#define SINGLE_QUOTE_STRINGS 1
#endif

#ifndef BFD_ASSEMBLER
#error M68HC11 support requires BFD_ASSEMBLER
#endif

/* The target BFD architecture.  */
#define TARGET_ARCH (m68hc11_arch ())
extern enum bfd_architecture m68hc11_arch PARAMS ((void));

#define TARGET_MACH (m68hc11_mach ())
extern int m68hc11_mach PARAMS ((void));

#define TARGET_FORMAT (m68hc11_arch_format ())
extern const char *m68hc11_arch_format PARAMS ((void));

/* Specific sections:
   - The .page0 is a data section that is mapped in [0x0000..0x00FF].
     Page0 accesses are faster on the M68HC11. Soft registers used by GCC-m6811
     are located in .page0.
   - The .vectors is the data section that represents the interrupt
     vectors.  */
#define ELF_TC_SPECIAL_SECTIONS \
  { ".page0",	SHT_PROGBITS,	SHF_ALLOC + SHF_WRITE	}, \
  { ".vectors",	SHT_PROGBITS,	SHF_ALLOC + SHF_WRITE	},

#define LISTING_WORD_SIZE 1	/* A word is 1 bytes */
#define LISTING_LHS_WIDTH 4	/* One word on the first line */
#define LISTING_LHS_WIDTH_SECOND 4	/* One word on the second line */
#define LISTING_LHS_CONT_LINES 4	/* And 4 lines max */
#define LISTING_HEADER m68hc11_listing_header ()
extern const char *m68hc11_listing_header PARAMS (());

/* call md_pcrel_from_section, not md_pcrel_from */
#define MD_PCREL_FROM_SECTION(FIXP, SEC) md_pcrel_from_section(FIXP, SEC)
extern long md_pcrel_from_section PARAMS ((struct fix *fixp, segT sec));

/* Permit temporary numeric labels.  */
#define LOCAL_LABELS_FB 1

#define DIFF_EXPR_OK		/* .-foo gets turned into PC relative relocs */

#define tc_init_after_args m68hc11_init_after_args
extern void m68hc11_init_after_args PARAMS ((void));

#define md_parse_long_option m68hc11_parse_long_option
extern int m68hc11_parse_long_option PARAMS ((char *));

#define DWARF2_LINE_MIN_INSN_LENGTH 1

/* We don't need to handle .word strangely.  */
#define WORKING_DOT_WORD

#define md_number_to_chars           number_to_chars_bigendian

/* Relax table to translate short relative branches (-128..127) into
   absolute branches.  */
#define TC_GENERIC_RELAX_TABLE md_relax_table
extern struct relax_type md_relax_table[];

#define md_operand(x)
#define tc_frob_label(sym) do {\
  S_SET_VALUE (sym, (valueT) frag_now_fix ()); \
} while (0)

#define tc_print_statistics(FILE) m68hc11_print_statistics (FILE)
extern void m68hc11_print_statistics PARAMS ((FILE *));
