/* Definitions for Alpha systems running BSD as target machine for GNU compiler.
   Copyright (C) 1993, 1995 Free Software Foundation, Inc.

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

/* We settle for little endian for now */

#define TARGET_ENDIAN_DEFAULT 0

#ifndef CROSS_COMPILE
/* Look for the G++ include files in the system-defined place.  */

#undef GPLUSPLUS_INCLUDE_DIR
#define GPLUSPLUS_INCLUDE_DIR "/usr/include/g++"

/* Under OpenBSD, the normal location of the various *crt*.o files is the
   /usr/lib directory.  */

#undef STANDARD_STARTFILE_PREFIX
#define STANDARD_STARTFILE_PREFIX "/usr/lib/"
#endif

/* Provide a LINK_SPEC appropriate for OpenBSD.  Here we provide support
   for the special GCC options -static, -assert, and -nostdlib.  */

#undef LINK_SPEC
#define LINK_SPEC \
  "%{!nostdlib:%{!r*:%{!e*:-e __start}}} -dc -dp \
   %{static:-Bstatic} %{assert*}"

/* We have atexit(3).  */

#define HAVE_ATEXIT

/* Implicit library calls should use memcpy, not bcopy, etc.  */

#define TARGET_MEM_FUNCTIONS

/* Define alpha-specific OpenBSD predefines... */
#ifndef CPP_PREDEFINES
#define CPP_PREDEFINES "-Dunix -D__ANSI_COMPAT -Asystem(unix) \
-D__OpenBSD__ -D__alpha__ -D__alpha"
#endif

/* Always uses gas.  */
#ifndef ASM_SPEC
#define ASM_SPEC "\
%|"
#endif

#ifndef CPP_SPEC
#define CPP_SPEC "\
%{posix:-D_POSIX_SOURCE}"
#endif

#define LIB_SPEC "%{!p:%{!pg:-lc}}%{p:-lc_p}%{pg:-lc_p}"
#define STARTFILE_SPEC \
   "%{!shared:%{pg:gcrt0.o%s}%{!pg:%{p:mcrt0.o%s}%{!p:crt0.o%s}}}"

#ifndef MACHINE_TYPE
#define MACHINE_TYPE "OpenBSD/alpha"
#endif

#define TARGET_DEFAULT (MASK_GAS | 3)
#define PREFERRED_DEBUGGING_TYPE DBX_DEBUG

#define LOCAL_LABEL_PREFIX	"."

#include "alpha/alpha.h"

/* Don't tell collect2 we use COFF as we don't have (yet ?) a dynamic ld
   library with the proper functions to handle this -> collect2 will
   default to using nm. */
#undef OBJECT_FORMAT_COFF
#undef EXTENDED_COFF

/* This is how we tell the assembler that a symbol is weak.  */
#define ASM_OUTPUT_WEAK_ALIAS(FILE,NAME,VALUE)	\
 do {						\
  fputs ("\t.weakext\t", FILE);			\
  assemble_name (FILE, NAME);			\
  if (VALUE)					\
    {						\
      fputs (", ", FILE);			\
      assemble_name (FILE, VALUE);		\
    }						\
  fputc ('\n', FILE);				\
 } while (0)

#define ASM_WEAKEN_LABEL(FILE,NAME) ASM_OUTPUT_WEAK_ALIAS(FILE,NAME,0)

/* Since gas and gld are standard on OpenBSD, we don't need this */
#undef ASM_FINAL_SPEC

/* We're not ELF yet */
#undef HAS_INIT_SECTION

/* Special handling, refer to defaults.h for the logic behind it:
   if INCOMING_RETURN_ADDR_RTX is defined and DWARF2_UNWIND_INFO is
   not, DWARF2_UNWIND_INFO becomes the default. Logically, this should
   depend on the output format being/not being ELF. But we can't check
   that directly. And INCOMING_RETURN_ADDR_RTX was added to alpha.h post
   2.8.1. The following construct should work for all gcc versions.
 */

/* We dont't default to dwarf2-style exception handling as we're not 
   elf... */
#ifdef INCOMING_RETURN_ADDR_RTX
#undef DWARF2_UNWIND_INFO
#define DWARF2_UNWIND_INFO 0
#endif

