/* tc-sparc.h - Macros and type defines for the sparc.
   Copyright (C) 1989, 90, 91, 92, 93, 94, 1995 Free Software Foundation, Inc.

   This file is part of GAS, the GNU Assembler.

   GAS is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2,
   or (at your option) any later version.

   GAS is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
   the GNU General Public License for more details.

   You should have received a copy of the GNU General Public
   License along with GAS; see the file COPYING.  If not, write
   to the Free Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA. */

#define TC_SPARC 1

#define LOCAL_LABELS_FB 1

#define TARGET_ARCH bfd_arch_sparc
#ifdef OBJ_AOUT
#ifdef TE_NetBSD
#define TARGET_FORMAT "a.out-sparc-netbsd"
#else
#define TARGET_FORMAT "a.out-sunos-big"
#endif
#endif
#ifdef OBJ_BOUT
#define TARGET_FORMAT "b.out.big"
#endif
#ifdef OBJ_ELF
#ifndef sparcv9
#define TARGET_FORMAT "elf32-sparc"
#else	/* sparcv9 */
#define TARGET_FORMAT "elf64-sparc" /* v9 */
#define ENV64			/* v9 */
#endif	/* sparcv9 */
#define LOCAL_LABEL(name)	(((name)[0] == '.' && (name)[1] == 'L') || !strncmp ((name), "_.L_", 4))
#endif
#define WORKING_DOT_WORD

#define md_convert_frag(b,s,f)		{as_fatal ("sparc convert_frag\n");}
#define md_create_long_jump(p,f,t,fr,s)	as_fatal("sparc_create_long_jump")
#define md_create_short_jump(p,f,t,fr,s) as_fatal("sparc_create_short_jump")
#define md_estimate_size_before_relax(f,s) \
			(as_fatal("estimate_size_before_relax called"),1)
void tc_aout_pre_write_hook ();

#define LISTING_HEADER "SPARC GAS "

extern int sparc_pic_code;

#if defined (OBJ_ELF) || defined (OBJ_AOUT)

/* This expression evaluates to false if the relocation is for a local
   object for which we still want to do the relocation at runtime.
   True if we are willing to perform this relocation while building
   the .o file.

   If we are generating PIC, and the reloc is against an externally
   visible symbol, we do not want gas to do the relocation.  */

#define TC_RELOC_RTSYM_LOC_FIXUP(FIX)  \
  (! sparc_pic_code \
   || (FIX)->fx_addsy == NULL \
   || (! S_IS_EXTERNAL ((FIX)->fx_addsy) \
       && S_IS_DEFINED ((FIX)->fx_addsy) \
       && ! S_IS_COMMON ((FIX)->fx_addsy)))
#endif

/* I know that "call 0" fails in sparc-coff if this doesn't return 1.  I
   don't know about other relocation types, or other formats, yet.  */
#ifdef OBJ_COFF
#define TC_FORCE_RELOCATION(FIXP)	\
	((FIXP)->fx_r_type == BFD_RELOC_32_PCREL_S2 \
	 && ((FIXP)->fx_addsy == 0 \
	     || S_GET_SEGMENT ((FIXP)->fx_addsy) == absolute_section))
#define RELOC_REQUIRES_SYMBOL
#endif

#define TC_HANDLES_FX_DONE

#ifdef OBJ_ELF
/* Keep relocations against global symbols.  Don't turn them into
   relocations against sections.  This is required for the dynamic
   linker to operate properly.  When generating PIC, we need to keep
   any non PC relative reloc.  */
#define tc_fix_adjustable(FIX) \
  (! S_IS_EXTERNAL ((FIX)->fx_addsy) \
   && (! sparc_pic_code || (FIX)->fx_pcrel))
#endif

#ifdef OBJ_AOUT
/* When generating PIC code, we must not adjust any reloc which will
   turn into a reloc against the global offset table.  */
#define tc_fix_adjustable(FIX) \
  (! sparc_pic_code \
   || (FIX)->fx_pcrel \
   || (FIX)->fx_r_type == BFD_RELOC_16 \
   || (FIX)->fx_r_type == BFD_RELOC_32)
#endif

#define md_operand(x)

/* end of tc-sparc.h */
