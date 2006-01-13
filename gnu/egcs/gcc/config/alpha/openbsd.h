/* Configuration file for an alpha OpenBSD target.
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

/* We settle for little endian for now.  */
#define TARGET_ENDIAN_DEFAULT 0

#include <alpha/alpha.h>
#include <alpha/elf.h>

#define OBSD_HAS_DECLARE_FUNCTION_NAME
#define OBSD_HAS_DECLARE_FUNCTION_SIZE
#define OBSD_HAS_DECLARE_OBJECT

#include <openbsd.h>

/* Controlling the compilation driver.  */

/* alpha needs __start.  */
#undef LINK_SPEC
#define LINK_SPEC \
  "%{!shared:%{!nostdlib:%{!r*:%{!e*:-e __start}}}} \
   %{shared:-shared} %{R*} \
   %{static:-Bstatic} \
   %{!static:-Bdynamic} \
   %{assert*} \
   %{!dynamic-linker:-dynamic-linker /usr/libexec/ld.so}"

/* As an elf system, we need crtbegin/crtend stuff.  */
#undef STARTFILE_SPEC
#define STARTFILE_SPEC "\
	%{!shared: %{pg:gcrt0%O%s} %{!pg:%{p:gcrt0%O%s} %{!p:crt0%O%s}} \
	crtbegin%O%s} %{shared:crtbeginS%O%s}"
#undef ENDFILE_SPEC
#define ENDFILE_SPEC "%{!shared:crtend%O%s} %{shared:crtendS%O%s}"

/* run-time target specifications */
#define CPP_PREDEFINES "-D__unix__ -D__ANSI_COMPAT -Asystem(unix) \
-D__OpenBSD__ -D__alpha__ -D__alpha -D__LP64__ -D_LP64 -D__ELF__"

/* Layout of source language data types.  */

/* This must agree with <machine/_types.h> */
#undef SIZE_TYPE
#define SIZE_TYPE "long unsigned int"

#undef PTRDIFF_TYPE
#define PTRDIFF_TYPE "long int"

#undef WCHAR_TYPE
#define WCHAR_TYPE "int"

#undef WCHAR_TYPE_SIZE
#define WCHAR_TYPE_SIZE 32


#undef PREFERRED_DEBUGGING_TYPE
#define PREFERRED_DEBUGGING_TYPE DBX_DEBUG

/* Output and generation of labels.  */
#define LOCAL_LABEL_PREFIX	"."

/* .set on alpha is not used to output labels.  */
#undef SET_ASM_OP

/* So, provide corresponding default, without the .set.  */
#undef ASM_OUTPUT_DEFINE_LABEL_DIFFERENCE_SYMBOL
#define ASM_OUTPUT_DEFINE_LABEL_DIFFERENCE_SYMBOL(FILE, SY, HI, LO)     \
 do {                                                                   \
  assemble_name (FILE, SY);                                             \
  fputc ('=', FILE);                                                    \
  assemble_name (FILE, HI);                                             \
  fputc ('-', FILE);                                                    \
  assemble_name (FILE, LO);                                             \
 } while (0)

/* problems occur if we're too liberal in preserve_subexpressions_p */
#define	BROKEN_PRESERVE_SUBEXPRESSIONS_P
