/* Configuration file for sparc64 OpenBSD target.
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

#ifndef TARGET_CPU_DEFAULT
#define TARGET_CPU_DEFAULT	TARGET_CPU_ultrasparc
#endif

#include <sparc/sp64-elf.h>

#define OBSD_HAS_DECLARE_FUNCTION_NAME
#define OBSD_HAS_DECLARE_FUNCTION_SIZE
#define OBSD_HAS_DECLARE_OBJECT

#include <openbsd.h>

#undef TARGET_VERSION
#define TARGET_VERSION fprintf (stderr, " (sparc64 OpenBSD ELF)")

/* A 64 but v9 complier in a Medium/Mid code model.  */

#undef TARGET_DEFAULT
#define TARGET_DEFAULT \
(MASK_V9 + MASK_PTR64 + MASK_64BIT /* + MASK_HARD_QUAD */ \
 + MASK_APP_REGS + MASK_EPILOGUE + MASK_FPU + MASK_STACK_BIAS)

#undef SPARC_DEFAULT_CMODEL
#define SPARC_DEFAULT_CMODEL CM_MEDMID

/* Run-time target specifications.  */
#undef CPP_PREDEFINES
#define CPP_PREDEFINES "-D__unix__ -D__sparc__ -D__sparc64__ -D__sparcv9__ \
-D__sparc_v9__ -D__arch64__ -D__LP64__ -D_LP64 -D__ELF__ -D__OpenBSD__ \
-Asystem(unix) -Asystem(OpenBSD) -Acpu(sparc) -Amachine(sparc)"

#undef CPP_SUBTARGET_SPEC
#define CPP_SUBTARGET_SPEC ""

#undef MD_EXEC_PREFIX
#undef MD_STARTFILE_PREFIX

#undef ASM_SPEC
#define ASM_SPEC "\
%{v:-V} -s %{fpic:-K PIC} %{fPIC:-K PIC} \
%{mlittle-endian:-EL} \
%(asm_cpu) %(asm_arch) \
"

/* Layout of source language data types.  */
#undef WCHAR_TYPE
#define WCHAR_TYPE "int"

#undef WCHAR_TYPE_SIZE
#define WCHAR_TYPE_SIZE 32

#undef LONG_DOUBLE_TYPE_SIZE
#define LONG_DOUBLE_TYPE_SIZE 128

#undef LINK_SPEC
#define LINK_SPEC \
  "%{!shared:%{!nostdlib:%{!r*:%{!e*:-e __start}}}} \
   %{shared:-shared} %{R*} \
   %{static:-Bstatic} \
   %{!static:-Bdynamic} \
   %{assert*} \
   %{!dynamic-linker:-dynamic-linker /usr/libexec/ld.so}"

/* For profiled binaries we have to change the memory model.  */
#undef CC1_SPEC
#define CC1_SPEC "%{p*:-mcmodel=medlow} %{p:-mcmodel=medlow}"

/* As an elf system, we need crtbegin/crtend stuff.  */
#undef STARTFILE_SPEC
#define STARTFILE_SPEC "\
        %{!shared: %{pg:gcrt0%O%s} %{!pg:%{p:gcrt0%O%s} %{!p:crt0%O%s}} \
        crtbegin%O%s} %{shared:crtbeginS%O%s}"
#undef ENDFILE_SPEC
#define ENDFILE_SPEC "%{!shared:crtend%O%s} %{shared:crtendS%O%s}"

/* A C statement (sans semicolon) to output an element in the table of
   global constructors.  */
#undef ASM_OUTPUT_CONSTRUCTOR
#define ASM_OUTPUT_CONSTRUCTOR(FILE,NAME)				\
  do {									\
    ctors_section ();							\
    fprintf ((FILE), "\t%s\t ", TARGET_ARCH64 ? ASM_LONGLONG : INT_ASM_OP) ; \
    assemble_name ((FILE), (NAME));					\
    fprintf ((FILE), "\n");						\
  } while (0)

/* A C statement (sans semicolon) to output an element in the table of
   global destructors.  */
#undef ASM_OUTPUT_DESTRUCTOR
#define ASM_OUTPUT_DESTRUCTOR(FILE,NAME)				\
  do {									\
    dtors_section ();							\
    fprintf ((FILE), "\t%s\t ", TARGET_ARCH64 ? ASM_LONGLONG : INT_ASM_OP); \
    assemble_name ((FILE), (NAME));					\
    fprintf ((FILE), "\n");						\
  } while (0)

