/* tc-i370.h -- Header file for tc-i370.c.
   Copyright 1994, 1995, 1996, 1997, 1998, 2000
   Free Software Foundation, Inc.
   Written by Ian Lance Taylor, Cygnus Support.

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

#define TC_I370

#ifdef ANSI_PROTOTYPES
struct fix;
#endif

/* Set the endianness we are using.  Default to big endian.  */
#ifndef TARGET_BYTES_BIG_ENDIAN
#define TARGET_BYTES_BIG_ENDIAN 1
#endif

#ifndef BFD_ASSEMBLER
 #error I370 support requires BFD_ASSEMBLER
#endif

/* The target BFD architecture.  */
#define TARGET_ARCH (i370_arch ())
extern enum bfd_architecture i370_arch PARAMS ((void));

/* Whether or not the target is big endian */
extern int target_big_endian;

/* The target BFD format.  */
#ifdef OBJ_ELF
#define TARGET_FORMAT ("elf32-i370")
#endif

/* Permit temporary numeric labels.  */
#define LOCAL_LABELS_FB 1

/* $ is used to refer to the current location.  */
/* #define DOLLAR_DOT */

#ifdef OBJ_ELF
#define DIFF_EXPR_OK		/* foo-. gets turned into PC relative relocs */
#endif

/* We don't need to handle .word strangely.  */
#define WORKING_DOT_WORD

/* We set the fx_done field appropriately in md_apply_fix.  */
#define TC_HANDLES_FX_DONE


#ifdef OBJ_ELF

/* Branch prediction relocations must force relocation.  */
#define TC_FORCE_RELOCATION_SECTION(FIXP,SEC) 1

/* Support for SHF_EXCLUDE and SHT_ORDERED */
extern int i370_section_letter PARAMS ((int, char **));
extern int i370_section_type PARAMS ((char *, size_t));
extern int i370_section_word PARAMS ((char *, size_t));
extern int i370_section_flags PARAMS ((int, int, int));

#define md_elf_section_letter(LETTER, PTR_MSG)	i370_section_letter (LETTER, PTR_MSG)
#define md_elf_section_type(STR, LEN)		i370_section_type (STR, LEN)
#define md_elf_section_word(STR, LEN)		i370_section_word (STR, LEN)
#define md_elf_section_flags(FLAGS, ATTR, TYPE)	i370_section_flags (FLAGS, ATTR, TYPE)

#define tc_comment_chars i370_comment_chars
extern const char *i370_comment_chars;

/* We must never ever try to resolve references to externally visible
   symbols in the assembler, because the .o file might go into a shared
   library, and some other shared library might override that symbol.  */
#define TC_RELOC_RTSYM_LOC_FIXUP(FIX)  \
  ((FIX)->fx_addsy == NULL \
   || (! S_IS_EXTERNAL ((FIX)->fx_addsy) \
       && ! S_IS_WEAK ((FIX)->fx_addsy) \
       && S_IS_DEFINED ((FIX)->fx_addsy) \
       && ! S_IS_COMMON ((FIX)->fx_addsy)))

#endif /* OBJ_ELF */

/* call md_apply_fix3 with segment instead of md_apply_fix */
#define MD_APPLY_FIX3

/* call md_pcrel_from_section, not md_pcrel_from */
#define MD_PCREL_FROM_SECTION(FIXP, SEC) md_pcrel_from_section(FIXP, SEC)
extern long md_pcrel_from_section PARAMS ((struct fix *, segT));

#define md_operand(x)
