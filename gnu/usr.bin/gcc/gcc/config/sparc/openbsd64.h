/* Configuration file for sparc64 OpenBSD target.
   Copyright (C) 1999 Free Software Foundation, Inc.

This file is part of GCC.

GCC is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GCC is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GCC; see the file COPYING.  If not, write to
the Free Software Foundation, 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.  */

#undef TARGET_VERSION
#define TARGET_VERSION fprintf (stderr, " (sparc64 OpenBSD ELF)")

#undef TARGET_DEFAULT
#define TARGET_DEFAULT \
(MASK_V9 + MASK_PTR64 + MASK_64BIT /* + MASK_HARD_QUAD */ \
 + MASK_APP_REGS + MASK_FPU + MASK_STACK_BIAS + MASK_LONG_DOUBLE_128)

#undef SPARC_DEFAULT_CMODEL
#define SPARC_DEFAULT_CMODEL CM_MEDMID

/* Run-time target specifications.  */
#define TARGET_OS_CPP_BUILTINS()			\
  do							\
    {							\
      OPENBSD_OS_CPP_BUILTINS_ELF();			\
      OPENBSD_OS_CPP_BUILTINS_LP64();			\
      builtin_define ("__sparc64__");			\
      builtin_define ("__sparc_v9__");			\
      builtin_define ("__sparcv9__");			\
      builtin_define ("__arch64__");			\
      builtin_define ("__sparc");			\
      builtin_define ("__sparc__");			\
    }							\
  while (0)

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

#undef	WINT_TYPE
#define	WINT_TYPE "int"

#undef	WINT_TYPE_SIZE
#define	WINT_TYPE_SIZE 32

#define HANDLE_PRAGMA_REDEFINE_EXTNAME 1

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

#undef ASM_CPU_DEFAULT_SPEC
#define ASM_CPU_DEFAULT_SPEC "-xarch=v8plusa"

#undef ASM_CPU_SPEC
#define ASM_CPU_SPEC "\
%{mcpu=v8plus:-xarch=v8plus} \
%{mcpu=ultrasparc:-xarch=v8plusa} \
%{!mcpu*:%(asm_cpu_default)} \
"

/* Same as sparc.h */
#undef DBX_REGISTER_NUMBER
#define DBX_REGISTER_NUMBER(REGNO) \
  (TARGET_FLAT && (REGNO) == HARD_FRAME_POINTER_REGNUM ? 31 : REGNO)

/* The Solaris 2 assembler uses .skip, not .zero, so put this back.  */
#undef ASM_OUTPUT_SKIP
#define ASM_OUTPUT_SKIP(FILE,SIZE)  \
  fprintf (FILE, "\t.skip %u\n", (SIZE))

#undef  LOCAL_LABEL_PREFIX
#define LOCAL_LABEL_PREFIX  "."

/* This is how to output a definition of an internal numbered label where
   PREFIX is the class of label and NUM is the number within the class.  */

#undef  ASM_OUTPUT_INTERNAL_LABEL
#define ASM_OUTPUT_INTERNAL_LABEL(FILE,PREFIX,NUM)	\
  fprintf (FILE, ".L%s%d:\n", PREFIX, NUM)

/* This is how to output a reference to an internal numbered label where
   PREFIX is the class of label and NUM is the number within the class.  */

#undef  ASM_OUTPUT_INTERNAL_LABELREF
#define ASM_OUTPUT_INTERNAL_LABELREF(FILE,PREFIX,NUM)	\
  fprintf (FILE, ".L%s%d", PREFIX, NUM)

/* This is how to store into the string LABEL
   the symbol_ref name of an internal numbered label where
   PREFIX is the class of label and NUM is the number within the class.
   This is suitable for output with `assemble_name'.  */

#undef  ASM_GENERATE_INTERNAL_LABEL
#define ASM_GENERATE_INTERNAL_LABEL(LABEL,PREFIX,NUM)	\
  sprintf ((LABEL), "*.L%s%ld", (PREFIX), (long)(NUM))

/* Select a format to encode pointers in exception handling data.  CODE
   is 0 for data, 1 for code labels, 2 for function pointers.  GLOBAL is
   true if the symbol may be affected by dynamic relocations.

   Some Solaris dynamic linkers don't handle unaligned section relative
   relocs properly, so force them to be aligned.  */
#ifndef HAVE_AS_SPARC_UA_PCREL
#define ASM_PREFERRED_EH_DATA_FORMAT(CODE,GLOBAL)		\
  ((flag_pic || GLOBAL) ? DW_EH_PE_aligned : DW_EH_PE_absptr)
#endif

/* ??? This does not work in SunOS 4.x, so it is not enabled in sparc.h.
   Instead, it is enabled here, because it does work under Solaris.  */
/* Define for support of TFmode long double and REAL_ARITHMETIC.
   Sparc ABI says that long double is 4 words.  */
#define LONG_DOUBLE_TYPE_SIZE 128

/* But indicate that it isn't supported by the hardware.  */
#define WIDEST_HARDWARE_FP_SIZE 64

#define STDC_0_IN_SYSTEM_HEADERS 1

#define MULDI3_LIBCALL "__mul64"
#define DIVDI3_LIBCALL "__div64"
#define UDIVDI3_LIBCALL "__udiv64"
#define MODDI3_LIBCALL "__rem64"
#define UMODDI3_LIBCALL "__urem64"

#undef INIT_SUBTARGET_OPTABS
#define INIT_SUBTARGET_OPTABS						\
  fixsfdi_libfunc							\
    = init_one_libfunc (TARGET_ARCH64 ? "__ftol" : "__ftoll");		\
  fixunssfdi_libfunc							\
    = init_one_libfunc (TARGET_ARCH64 ? "__ftoul" : "__ftoull");	\
  fixdfdi_libfunc							\
    = init_one_libfunc (TARGET_ARCH64 ? "__dtol" : "__dtoll");		\
  fixunsdfdi_libfunc							\
    = init_one_libfunc (TARGET_ARCH64 ? "__dtoul" : "__dtoull")


#undef PREFERRED_DEBUGGING_TYPE
#define PREFERRED_DEBUGGING_TYPE DWARF2_DEBUG

#if 0
/* The medium/anywhere code model practically requires us to put jump tables
   in the text section as gcc is unable to distinguish LABEL_REF's of jump
   tables from other label refs (when we need to).  */
/* But we now defer the tables to the end of the function, so we make
   this 0 to not confuse the branch shortening code.  */
#undef JUMP_TABLES_IN_TEXT_SECTION
#define JUMP_TABLES_IN_TEXT_SECTION 0

/* V9 chips can handle either endianness.  */
#undef SUBTARGET_SWITCHES
#define SUBTARGET_SWITCHES \
{"big-endian", -MASK_LITTLE_ENDIAN, N_("Generate code for big endian") }, \
{"little-endian", MASK_LITTLE_ENDIAN, N_("Generate code for little endian") },

#undef BYTES_BIG_ENDIAN
#define BYTES_BIG_ENDIAN (! TARGET_LITTLE_ENDIAN)

#undef WORDS_BIG_ENDIAN
#define WORDS_BIG_ENDIAN (! TARGET_LITTLE_ENDIAN)
#endif
