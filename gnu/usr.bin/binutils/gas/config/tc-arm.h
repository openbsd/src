/* This file is tc-arm.h
   Contributed by Richard Earnshaw (rwe@pegasus.esprit.ec.org)
	Modified by David Taylor (dtaylor@armltd.co.uk)

   Copyright (C) 1994, 1995 Free Software Foundation, Inc.

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

#define TC_ARM 1

#define COFF_MAGIC ARMMAGIC
#define TARGET_ARCH bfd_arch_arm

#define AOUT_MACHTYPE 0

#define DIFF_EXPR_OK

#define LITTLE_ENDIAN 1234
#define BIG_ENDIAN 4321

/* If neither TARGET_BYTES_BIG_ENDIAN nor TARGET_BYTES_LITTLE_ENDIAN
   is specified, default to little endian.  */
#ifndef TARGET_BYTES_BIG_ENDIAN
#ifndef TARGET_BYTES_LITTLE_ENDIAN
#define TARGET_BYTES_LITTLE_ENDIAN
#endif
#endif

#ifdef OBJ_AOUT
#ifdef TE_RISCIX
#define TARGET_FORMAT "a.out-riscix"
#else
#ifdef TARGET_BYTES_BIG_ENDIAN
#define TARGET_FORMAT "a.out-arm-big"
#else
#define TARGET_FORMAT "a.out-arm-little"
#endif
#endif
#endif

#ifdef OBJ_AIF
#define TARGET_FORMAT "aif"
#endif

#ifdef OBJ_COFF
#define ARM_BI_ENDIAN
#ifdef TE_PE
#define TC_FORCE_RELOCATION(x) ((x)->fx_r_type==BFD_RELOC_RVA)
#define TARGET_FORMAT (target_big_endian ? "pe-arm-big" : "pe-arm-little")
#else
#define TARGET_FORMAT (target_big_endian ? "coff-arm-big" : "coff-arm-little")
/* Tell tc-arm.c to support runtime endian selection.  */
#endif
#endif

#define md_convert_frag(b,s,f)		{as_fatal ("arm convert_frag\n");}

#define md_after_pass_hook() arm_after_pass_hook ()
#define md_start_line_hook() arm_start_line_hook ()
#define tc_frob_label(S) arm_frob_label (S) 

#define obj_fix_adjustable(fixP) 0

#if 0	/* It isn't as simple as this */
#define tc_frob_symbol(sym,punt)	\
{	if (S_IS_LOCAL (sym))		\
	  {				\
	    punt = 1;			\
	    sym->sy_used_in_reloc = 0;	\
	  }}
#endif 

#if 0
#define tc_crawl_symbol_chain(a)	{;}	/* not used */
#define tc_headers_hook(a)		{;}	/* not used */
#endif

#define tc_aout_pre_write_hook(x)	{;}	/* not used */

#define LISTING_HEADER "ARM GAS "

#define OPTIONAL_REGISTER_PREFIX '%'

#define md_operand(x)

#define LOCAL_LABELS_FB  1

/* Use defaults for OBJ_AOUT.  */
#ifndef OBJ_AOUT
#define LOCAL_LABEL(name)	((name)[0] == '.' && (name)[1] == 'L')
#define FAKE_LABEL_NAME		".L0\001"
#endif

/* end of tc-arm.h */
