/* tc-cris.h -- Header file for tc-cris.c, the CRIS GAS port.
   Copyright 2000, 2001 Free Software Foundation, Inc.

   Contributed by Axis Communications AB, Lund, Sweden.
   Originally written for GAS 1.38.1 by Mikael Asker.
   Updated, BFDized and GNUified by Hans-Peter Nilsson.

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
   along with GAS; see the file COPYING.  If not, write to the
   Free Software Foundation, 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA.  */

/* See the GAS "internal" document for general documentation on this.
   It is called internals.texi (internals.info when makeinfo:d), but is
   not installed or makeinfo:d by "make info".  */

/* Functions and variables that aren't declared in tc.h are declared here,
   with the type/prototype that is used in the local extern-declaration of
   their usage.  */

#ifndef TC_CRIS
#define TC_CRIS

/* Multi-target support is always on.  */
extern const char *cris_target_format PARAMS ((void));
#define TARGET_FORMAT cris_target_format ()

#define TARGET_ARCH bfd_arch_cris

#define TARGET_BYTES_BIG_ENDIAN 0

extern const char *md_shortopts;
extern struct option md_longopts[];
extern size_t md_longopts_size;

extern const pseudo_typeS md_pseudo_table[];

#define tc_comment_chars cris_comment_chars
extern const char cris_comment_chars[];
extern const char line_comment_chars[];
extern const char line_separator_chars[];
extern const char EXP_CHARS[];
extern const char FLT_CHARS[];

/* This should be optional, since it is ignored as an escape (assumed to
   be itself) if it is not recognized.  */
#define ONLY_STANDARD_ESCAPES

/* Note that we do not define TC_EQUAL_IN_INSN, since its current use is
   in the instruction rather than the operand, and thus does not come to
   use for side-effect assignments such as "and.d [r0 = r1 + 42], r3".  */
#define md_operand(x)

#define md_number_to_chars number_to_chars_littleendian

extern const int md_short_jump_size;
extern const int md_long_jump_size;

/* There's no use having different functions for this; the sizes are the
   same.  Note that we can't #define md_short_jump_size here.  */
#define md_create_short_jump md_create_long_jump

extern const struct relax_type md_cris_relax_table[];
#define TC_GENERIC_RELAX_TABLE md_cris_relax_table

#define TC_HANDLES_FX_DONE

#define TC_FORCE_RELOCATION(fixp) md_cris_force_relocation (fixp)
extern int md_cris_force_relocation PARAMS ((struct fix *));

/* This is really a workaround for a bug in write.c that resolves relocs
   for weak symbols - it should be postponed to the link stage or later.
   */
#define tc_fix_adjustable(X)				\
 ((! (X)->fx_addsy || ! S_IS_WEAK((X)->fx_addsy))	\
  && (X)->fx_r_type != BFD_RELOC_VTABLE_INHERIT		\
  && (X)->fx_r_type != BFD_RELOC_VTABLE_ENTRY)

/* When we have fixups against constant expressions, we get a GAS-specific
   section symbol at no extra charge for obscure reasons in
   adjust_reloc_syms.  Since ELF outputs section symbols, it gladly
   outputs this "*ABS*" symbol in every object.  Avoid that.  */
#define tc_frob_symbol(symp, punt)			\
 do {							\
  if (OUTPUT_FLAVOR == bfd_target_elf_flavour		\
      && (symp) == section_symbol (absolute_section))	\
    (punt) = 1;						\
 } while (0)

#define LISTING_HEADER "GAS for CRIS"

#if 0
/* The testsuite does not let me define these, although they IMHO should
   be preferred over the default.  */
#define LISTING_WORD_SIZE 2
#define LISTING_LHS_WIDTH 4
#define LISTING_LHS_WIDTH_SECOND 4
#endif

/* END of declaration and definitions described in the "internals"
   document.  */

/* Do this, or we will never know what hit us when the
   broken-word-fixes break.  Do _not_ use WARN_SIGNED_OVERFLOW_WORD,
   it is only for use with WORKING_DOT_WORD and warns about most stuff.
   (still in 2.9.1).  */
struct broken_word;
extern void tc_cris_check_adjusted_broken_word PARAMS ((offsetT,
							struct
							broken_word *));
#define TC_CHECK_ADJUSTED_BROKEN_DOT_WORD(new_offset, brokw) \
 tc_cris_check_adjusted_broken_word ((offsetT) (new_offset), brokw)

/* We don't want any implicit alignment, so we do nothing.  */
#define TC_IMPLICIT_LCOMM_ALIGNMENT(SIZE, P2VAR)

/* CRIS instructions, with operands and prefixes included, are a multiple
   of two bytes long.  */
#define DWARF2_LINE_MIN_INSN_LENGTH 2

#endif /* TC_CRIS */
/*
 * Local variables:
 * eval: (c-set-style "gnu")
 * indent-tabs-mode: t
 * End:
 */
