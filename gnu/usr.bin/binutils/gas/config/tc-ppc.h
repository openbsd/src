/* tc-ppc.h -- Header file for tc-ppc.c.
   Copyright (C) 1994 Free Software Foundation, Inc.
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
   along with GAS; see the file COPYING.  If not, write to
   the Free Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA. */

#define TC_PPC

#ifndef BFD_ASSEMBLER
 #error PowerPC support requires BFD_ASSEMBLER
#endif

/* If OBJ_COFF is defined, and TE_PE is not defined, we are assembling
   XCOFF for AIX or PowerMac.  If TE_PE is defined, we are assembling
   COFF for Windows NT.  */

#ifdef OBJ_COFF
#ifndef TE_PE
#define OBJ_XCOFF
#endif
#endif

/* The target BFD architecture.  */
#define TARGET_ARCH (ppc_arch ())
extern enum bfd_architecture ppc_arch PARAMS ((void));

/* Whether or not the target is big endian */
extern int target_big_endian;

/* The target BFD format.  */
#ifdef OBJ_COFF
#ifdef TE_PE
#define TARGET_FORMAT (target_big_endian ? "pe-powerpc" : "pe-powerpcle")
#else
#define TARGET_FORMAT "aixcoff-rs6000"
#endif
#endif

/* PowerMac has a BFD slightly different from AIX's.  */
#ifdef TE_POWERMAC
#ifdef TARGET_FORMAT
#undef TARGET_FORMAT
#endif
#define TARGET_FORMAT "xcoff-powermac"
#endif

#ifdef OBJ_ELF
#define TARGET_FORMAT (target_big_endian ? "elf32-powerpc" : "elf32-powerpcle")
#endif

/* Permit temporary numeric labels.  */
#define LOCAL_LABELS_FB 1

/* $ is used to refer to the current location.  */
#define DOLLAR_DOT

/* Strings do not use backslash escapes under COFF.  */
#ifdef OBJ_COFF
#define NO_STRING_ESCAPES
#endif

/* When using COFF, we determine whether or not to output a symbol
   based on sy_tc.output, not on the name.  */
#ifdef OBJ_XCOFF
#define LOCAL_LABEL(name) 0
#endif
#ifdef OBJ_ELF
/* When using ELF, local labels start with '.'.  */
#define LOCAL_LABEL(name) (name[0] == '.' \
			   && (name[1] == 'L' || name[1] == '.'))
#define FAKE_LABEL_NAME ".L0\001"
#define DIFF_EXPR_OK		/* .-foo gets turned into PC relative relocs */
#endif

/* Set the endianness we are using.  Default to big endian.  */
#ifndef TARGET_BYTES_BIG_ENDIAN
#ifndef TARGET_BYTES_LITTLE_ENDIAN
#define TARGET_BYTES_BIG_ENDIAN 1
#endif
#endif

#ifdef TARGET_BYTES_BIG_ENDIAN
#define PPC_BIG_ENDIAN 1
#else
#define PPC_BIG_ENDIAN 0
#endif

/* We don't need to handle .word strangely.  */
#define WORKING_DOT_WORD

/* We set the fx_done field appropriately in md_apply_fix.  */
#define TC_HANDLES_FX_DONE

#ifdef TE_PE

/* Question marks are permitted in symbol names.  */
#define LEX_QM 1

/* Don't adjust TOC relocs.  */
#define tc_fix_adjustable(fixp) ppc_pe_fix_adjustable (fixp)
extern int ppc_pe_fix_adjustable PARAMS ((struct fix *));

#endif

#ifdef OBJ_XCOFF

/* Declarations needed when generating XCOFF code.  XCOFF is an
   extension of COFF, used only on the RS/6000.  Rather than create an
   obj-xcoff, we just use obj-coff, and handle the extensions here in
   tc-ppc.  */

/* We need to keep some information for symbols.  */
struct ppc_tc_sy
{
  /* We keep a few linked lists of symbols.  */
  struct symbol *next;
  /* Non-zero if the symbol should be output.  The RS/6000 assembler
     only outputs symbols that are external or are mentioned in a
     .globl or .lglobl statement.  */
  int output;
  /* The symbol class.  */
  int class;
  /* The real name, if the symbol was renamed.  */
  char *real_name;
  /* For a csect symbol, the subsegment we are using.  This is zero
     for symbols that are not csects.  */
  subsegT subseg;
  /* For a csect or common symbol, the alignment to use.  */
  int align;
  /* For a function symbol, a symbol whose value is the size.  The
     field is NULL if there is no size.  */
  struct symbol *size;
  /* For a csect symbol, the last symbol which has been defined in
     this csect, or NULL if none have been defined so far.  For a .bs
     symbol, the referenced csect symbol.  */
  struct symbol *within;
};

#define TC_SYMFIELD_TYPE struct ppc_tc_sy

/* We need an additional auxent for function symbols.  */
#define OBJ_COFF_MAX_AUXENTRIES 2

/* Square and curly brackets are permitted in symbol names.  */
#define LEX_BR 3

/* Canonicalize the symbol name.  */
#define tc_canonicalize_symbol_name(name) ppc_canonicalize_symbol_name (name)
extern char *ppc_canonicalize_symbol_name PARAMS ((char *));

/* Get the symbol class from the name.  */
#define tc_symbol_new_hook(sym) ppc_symbol_new_hook (sym)
extern void ppc_symbol_new_hook PARAMS ((struct symbol *));

/* Set the symbol class of a label based on the csect.  */
#define tc_frob_label(sym) ppc_frob_label (sym)
extern void ppc_frob_label PARAMS ((struct symbol *));

/* TOC relocs requires special handling.  */
#define tc_fix_adjustable(fixp) ppc_fix_adjustable (fixp)
extern int ppc_fix_adjustable PARAMS ((struct fix *));

/* We need to set the section VMA.  */
#define tc_frob_section(sec) ppc_frob_section (sec)
extern void ppc_frob_section PARAMS ((asection *));

/* Finish up the symbol.  */
#define tc_frob_symbol(sym, punt) punt = ppc_frob_symbol (sym)
extern int ppc_frob_symbol PARAMS ((struct symbol *));

/* Finish up the entire symtab.  */
#define tc_adjust_symtab() ppc_adjust_symtab ()
extern void ppc_adjust_symtab PARAMS ((void));

/* Niclas Andersson <nican@ida.liu.se> says this is needed.  */
#define SUB_SEGMENT_ALIGN(SEG) 2

/* Finish up the file.  */
#define tc_frob_file() ppc_frob_file ()
extern void ppc_frob_file PARAMS ((void));

#endif /* OBJ_XCOFF */

#ifdef OBJ_ELF
/* The name of the global offset table generated by the compiler. Allow
   this to be overridden if need be. */
#ifndef GLOBAL_OFFSET_TABLE_NAME
#define GLOBAL_OFFSET_TABLE_NAME "_GLOBAL_OFFSET_TABLE_"
#endif

/* Branch prediction relocations must force relocation */
#define TC_FORCE_RELOCATION_SECTION(FIXP,SEC)				\
((FIXP)->fx_r_type == BFD_RELOC_PPC_B16_BRTAKEN				\
 || (FIXP)->fx_r_type == BFD_RELOC_PPC_B16_BRNTAKEN			\
 || (FIXP)->fx_r_type == BFD_RELOC_PPC_BA16_BRTAKEN			\
 || (FIXP)->fx_r_type == BFD_RELOC_PPC_BA16_BRNTAKEN			\
 || ((FIXP)->fx_addsy && !(FIXP)->fx_subsy && (FIXP)->fx_addsy->bsym	\
     && (FIXP)->fx_addsy->bsym->section != SEC))

#endif /* OBJ_ELF */

/* call md_apply_fix3 with segment instead of md_apply_fix */
#define MD_APPLY_FIX3

/* call md_pcrel_from_section, not md_pcrel_from */
#define MD_PCREL_FROM_SECTION(FIXP, SEC) md_pcrel_from_section(FIXP, SEC)

#define md_operand(x)
