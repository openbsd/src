/* This file is tc-m68k.h
   Copyright (C) 1987, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 2000
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
   along with GAS; see the file COPYING.  If not, write to the Free
   Software Foundation, 59 Temple Place - Suite 330, Boston, MA
   02111-1307, USA.  */

#define TC_M68K 1

#ifdef ANSI_PROTOTYPES
struct fix;
#endif

#define TARGET_BYTES_BIG_ENDIAN 1

#ifdef OBJ_AOUT
#ifdef TE_SUN3
#define TARGET_FORMAT "a.out-sunos-big"
#endif
#ifdef TE_NetBSD
#define TARGET_FORMAT "a.out-m68k-netbsd"
#endif
#ifdef TE_LINUX
#define TARGET_FORMAT "a.out-m68k-linux"
#endif
#ifndef TARGET_FORMAT
#define TARGET_FORMAT "a.out-zero-big"
#endif
#endif

#ifdef OBJ_ELF
#define TARGET_FORMAT "elf32-m68k"
#endif

#ifdef TE_APOLLO
#define COFF_MAGIC		APOLLOM68KMAGIC
#define COFF_AOUTHDR_MAGIC	APOLLO_COFF_VERSION_NUMBER
#undef OBJ_COFF_OMIT_OPTIONAL_HEADER
#endif

#ifdef TE_LYNX
#define TARGET_FORMAT		"coff-m68k-lynx"
#endif
#ifdef TE_AUX
#define TARGET_FORMAT		"coff-m68k-aux"
#endif
#ifdef TE_DELTA
#define TARGET_FORMAT		"coff-m68k-sysv"
#endif

#ifndef COFF_MAGIC
#define COFF_MAGIC MC68MAGIC
#endif
#define BFD_ARCH bfd_arch_m68k /* for non-BFD_ASSEMBLER */
#define TARGET_ARCH bfd_arch_m68k /* BFD_ASSEMBLER */
#define COFF_FLAGS F_AR32W
#define TC_COUNT_RELOC(x) ((x)->fx_addsy||(x)->fx_subsy)

#define TC_COFF_FIX2RTYPE(fixP) tc_coff_fix2rtype(fixP)
#define TC_COFF_SIZEMACHDEP(frag) tc_coff_sizemachdep(frag)
extern int tc_coff_sizemachdep PARAMS ((struct frag *));
#ifdef TE_SUN3
/* This variable contains the value to write out at the beginning of
   the a.out file.  The 2<<16 means that this is a 68020 file instead
   of an old-style 68000 file */

#define DEFAULT_MAGIC_NUMBER_FOR_OBJECT_FILE (2<<16|OMAGIC);	/* Magic byte for file header */
#endif /* TE_SUN3 */

#ifndef AOUT_MACHTYPE
#define AOUT_MACHTYPE m68k_aout_machtype
extern int m68k_aout_machtype;
#endif

#define tc_comment_chars m68k_comment_chars
extern const char *m68k_comment_chars;

#define tc_crawl_symbol_chain(a)	{;}	/* not used */
#define tc_headers_hook(a)		{;}	/* not used */
#define tc_aout_pre_write_hook(x)	{;}	/* not used */

#define LISTING_WORD_SIZE 2	/* A word is 2 bytes */
#define LISTING_LHS_WIDTH 2	/* One word on the first line */
#define LISTING_LHS_WIDTH_SECOND 2	/* One word on the second line */
#define LISTING_LHS_CONT_LINES 4/* And 4 lines max */
#define LISTING_HEADER "68K GAS "

#ifndef REGISTER_PREFIX
#define REGISTER_PREFIX '%'
#endif

#if !defined (REGISTER_PREFIX_OPTIONAL)
#if defined (M68KCOFF) || defined (OBJ_ELF)
#ifndef BFD_ASSEMBLER
#define LOCAL_LABEL(name) (name[0] == '.' \
			   && (name[1] == 'L' || name[1] == '.'))
#endif /* ! BFD_ASSEMBLER */
#define REGISTER_PREFIX_OPTIONAL 0
#else /* ! (COFF || ELF) */
#define REGISTER_PREFIX_OPTIONAL 1
#endif /* ! (COFF || ELF) */
#endif /* not def REGISTER_PREFIX and not def OPTIONAL_REGISTER_PREFIX */

#ifdef TE_DELTA
/* On the Delta, `%' can occur within a label name, but not as the
   initial character.  */
#define LEX_PCT LEX_NAME
/* On the Delta, `~' can start a label name, but is converted to '.'. */
#define LEX_TILDE LEX_BEGIN_NAME
#define tc_canonicalize_symbol_name(s) ((*(s) == '~' ? *(s) = '.' : '.'), s)
/* On the Delta, dots are not required before pseudo-ops.  */
#define NO_PSEUDO_DOT 1
#ifndef BFD_ASSEMBLER
#undef LOCAL_LABEL
#define LOCAL_LABEL(name) \
  (name[0] == '.' || (name[0] == 'L' && name[1] == '%'))
#endif
#endif

extern void m68k_mri_mode_change PARAMS ((int));
#define MRI_MODE_CHANGE(i) m68k_mri_mode_change (i)

extern int m68k_conditional_pseudoop PARAMS ((pseudo_typeS *));
#define tc_conditional_pseudoop(pop) m68k_conditional_pseudoop (pop)

extern void m68k_frob_label PARAMS ((symbolS *));
#define tc_frob_label(sym) m68k_frob_label (sym)

extern void m68k_flush_pending_output PARAMS ((void));
#define md_flush_pending_output() m68k_flush_pending_output ()

extern void m68k_frob_symbol PARAMS ((symbolS *));

#ifdef BFD_ASSEMBLER

#define tc_frob_symbol(sym,punt)				\
do								\
  {								\
    if (S_GET_SEGMENT (sym) == reg_section)			\
      punt = 1;							\
    m68k_frob_symbol (sym);					\
  }								\
while (0)

#define NO_RELOC BFD_RELOC_NONE

#ifdef OBJ_ELF

/* This expression evaluates to false if the relocation is for a local object
   for which we still want to do the relocation at runtime.  True if we
   are willing to perform this relocation while building the .o file.  If
   the reloc is against an externally visible symbol, then the assembler
   should never do the relocation.  */

#define TC_RELOC_RTSYM_LOC_FIXUP(FIX)			\
	((FIX)->fx_addsy == NULL			\
	 || (! S_IS_EXTERNAL ((FIX)->fx_addsy)		\
	     && ! S_IS_WEAK ((FIX)->fx_addsy)		\
	     && S_IS_DEFINED ((FIX)->fx_addsy)		\
	     && ! S_IS_COMMON ((FIX)->fx_addsy)))

#define tc_fix_adjustable(X) tc_m68k_fix_adjustable(X)
extern int tc_m68k_fix_adjustable PARAMS ((struct fix *));
#define elf_tc_final_processing m68k_elf_final_processing
extern void m68k_elf_final_processing PARAMS ((void));
#endif

#define TC_FORCE_RELOCATION(FIX)			\
	((FIX)->fx_r_type == BFD_RELOC_VTABLE_INHERIT	\
	 || (FIX)->fx_r_type == BFD_RELOC_VTABLE_ENTRY)

#else /* ! BFD_ASSEMBLER */

#define tc_frob_coff_symbol(sym) m68k_frob_symbol (sym)

#define NO_RELOC 0

#endif /* ! BFD_ASSEMBLER */

#define DIFF_EXPR_OK

extern void m68k_init_after_args PARAMS ((void));
#define tc_init_after_args m68k_init_after_args

extern int m68k_parse_long_option PARAMS ((char *));
#define md_parse_long_option m68k_parse_long_option

#define md_operand(x)

#define TARGET_WORD_SIZE 32
#define TARGET_ARCH bfd_arch_m68k

extern struct relax_type md_relax_table[];
#define TC_GENERIC_RELAX_TABLE md_relax_table

/* Copied from write.c */
/* This was formerly called M68K_AIM_KLUDGE.  */
#define md_prepare_relax_scan(fragP, address, aim, this_state, this_type) \
  if (aim==0 && this_state== 4) { /* hard encoded from tc-m68k.c */ \
    aim=this_type->rlx_forward+1; /* Force relaxation into word mode */ \
  }

/* end of tc-m68k.h */
