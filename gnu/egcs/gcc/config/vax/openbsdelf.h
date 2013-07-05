/* Configuration file for a vax OpenBSD target.
   Copyright (C) 1999 Free Software Foundation, Inc.

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

#undef	TARGET_DEFAULT
#define	TARGET_DEFAULT	0
#include <vax/vax.h>

#undef PCC_BITFIELD_TYPE_MATTERS
#include <elfos.h>

#define OBSD_NO_DYNAMIC_LIBRARIES
#include <openbsd.h>

#undef CPP_PREDEFINES
#define CPP_PREDEFINES "-D__unix__ -D__vax__ -D__OpenBSD__ -D__ELF__ -Asystem(unix) -Asystem(OpenBSD) -Acpu(vax) -Amachine(vax)"

#undef LINK_SPEC
#define LINK_SPEC \
	"%{!nostdlib:%{!r*:%{!e*:-e _start}}} %{R*} %{assert*}"

/* As an elf system, we need crtbegin/crtend stuff.  */
#undef STARTFILE_SPEC
#define STARTFILE_SPEC \
        "%{pg:gcrt0%O%s} %{!pg:%{p:gcrt0%O%s} %{!p:crt0%O%s}} crtbegin%O%s"
#undef ENDFILE_SPEC
#define ENDFILE_SPEC "crtend%O%s"

/* Layout of source language data types. */

/* This must agree with <machine/_types.h> */
#undef SIZE_TYPE
#define SIZE_TYPE "long unsigned int"

#undef PTRDIFF_TYPE
#define PTRDIFF_TYPE "long int"

#undef WCHAR_TYPE
#define WCHAR_TYPE "int"

#undef WCHAR_TYPE_SIZE
#define WCHAR_TYPE_SIZE 32

#undef	DEFAULT_PCC_STRUCT_RETURN
#define	DEFAULT_PCC_STRUCT_RETURN 0
#undef	PCC_STATIC_STRUCT_RETURN

#undef	REGISTER_PREFIX
#define	REGISTER_PREFIX	"%"

#undef	USER_LABEL_PREFIX
#define USER_LABEL_PREFIX ""

/* Redefine this with register prefixes.  */
#undef	VAX_ISTREAM_SYNC
#define	VAX_ISTREAM_SYNC	"movpsl -(%sp)\n\tpushal 1(%pc)\n\trei"

#undef	FUNCTION_PROFILER
#define	FUNCTION_PROFILER(FILE, LABELNO)  \
  asm_fprintf (FILE, "\tmovab .LP%d,%Rr0\n\tjsb __mcount\n", (LABELNO))

/* Use sjlj exceptions. */
#undef DWARF2_UNWIND_INFO

/* Make sure .stabs for a function are always the same section.  */
#define DBX_OUTPUT_FUNCTION_END(file,decl) function_section(decl)

/* The VAX wants no space between the case instruction and the jump table.  */
#undef	ASM_OUTPUT_BEFORE_CASE_LABEL
#define	ASM_OUTPUT_BEFORE_CASE_LABEL(FILE, PREFIX, NUM, TABLE)

/* Get the udiv/urem calls out of the user's namespace */
#undef	UDIVSI3_LIBCALL
#define	UDIVSI3_LIBCALL	"*__udiv"
#undef	UMODSI3_LIBCALL
#define	UMODSI3_LIBCALL	"*__urem"
