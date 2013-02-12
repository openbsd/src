/* Configuration file for an m88k OpenBSD target.
   Copyright (C) 2000 Free Software Foundation, Inc.

This file is part of GNU CC.

GNU CC is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GNU CC is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU CC; see the file COPYING.  If not, write to
the Free Software Foundation, 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.  */

/* <m88k/m88k.h> provided wrong GLOBAL_ASM_OP and SET_ASM_OP */
#undef SET_ASM_OP
#define SET_ASM_OP	"\tequ\t"
#undef GLOBAL_ASM_OP
#define GLOBAL_ASM_OP	"\t.globl\t"

/* <m88k/m88k.h> provided wrong SUPPORTS_WEAK and SUPPORTS_ONE_ONLY values */
#undef SUPPORTS_WEAK
#undef SUPPORTS_ONE_ONLY

/* Run-time target specifications.  */
#define TARGET_OS_CPP_BUILTINS()			\
  do							\
    {							\
      OPENBSD_OS_CPP_BUILTINS_COMMON();			\
      builtin_define ("__m88k");			\
      builtin_define ("__m88k__");			\
      builtin_assert ("cpu=m88k");			\
      builtin_assert ("machine=m88k");			\
      if (TARGET_88000)					\
	builtin_define ("__mc88000__");			\
      else						\
	{						\
	  if (TARGET_88100)				\
	    builtin_define ("__mc88100__");		\
	  if (TARGET_88110)				\
	    builtin_define ("__mc88110__");		\
	}						\
    }							\
  while (0)

/* Layout of source language data types. */

/* This must agree with <machine/_types.h> */
#undef SIZE_TYPE
#define SIZE_TYPE "long unsigned int"

#undef PTRDIFF_TYPE
#define PTRDIFF_TYPE "long int"

#undef INTMAX_TYPE
#define INTMAX_TYPE "long long int"

#undef UINTMAX_TYPE
#define UINTMAX_TYPE "long long unsigned int"

#undef WCHAR_TYPE
#define WCHAR_TYPE "int"

#undef WCHAR_TYPE_SIZE
#define WCHAR_TYPE_SIZE 32

/* Every structure or union's size must be a multiple of 2 bytes.  */
#undef STRUCTURE_SIZE_BOUNDARY
#define STRUCTURE_SIZE_BOUNDARY 16 

/* Due to the split instruction and data caches, trampolines must cause the
   data cache to be synced before attempting to execute the trampoline code.
   Under OpenBSD, this is done by invoking trap #451 with r2 and r3 set to
   the address of the trampoline area and its size, respectively.  */
#undef FINALIZE_TRAMPOLINE
#define FINALIZE_TRAMPOLINE(TRAMP)					\
  emit_library_call(gen_rtx_SYMBOL_REF (Pmode, "__dcache_sync"),	\
		    0, VOIDmode, 2, (TRAMP), Pmode,			\
		    GEN_INT (TRAMPOLINE_SIZE), Pmode)

#if defined(CROSS_COMPILE) && !defined(ATTRIBUTE_UNUSED)
#define ATTRIBUTE_UNUSED
#endif
#undef TRANSFER_FROM_TRAMPOLINE
#define TRANSFER_FROM_TRAMPOLINE					\
extern void __dcache_sync(int, int);					\
void									\
__dcache_sync (addr, len)						\
     int addr ATTRIBUTE_UNUSED, len ATTRIBUTE_UNUSED;			\
{									\
  /* r2 and r3 are set by the caller and need not be modified */	\
  __asm __volatile ("tb0 0, r0, 451");					\
}

/* All configurations that don't use elf must be explicit about not using
   dwarf unwind information. egcs doesn't try too hard to check internal
   configuration files...  */
#define DWARF2_UNWIND_INFO 0
