/* Configuration file for an hppa risc OpenBSD target.
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


#include <pa/pa.h>
#include <pa/pa32-regs.h>
#define OBSD_HAS_DECLARE_FUNCTION_NAME
#include <openbsd.h>

/* Turn off various SOM crap we don't want.  */
#undef TARGET_ELF32
#define TARGET_ELF32 1

#undef MAX_OFILE_ALIGNMENT
#define	MAX_OFILE_ALIGNMENT 0x8000

#undef TARGET_SCHED_DEFAULT
#define TARGET_SCHED_DEFAULT "700"

/* libc's profiling functions don't need gcc to allocate counters.  */
#define NO_PROFILE_COUNTERS 1

/* Run-time target specifications. */
#undef TARGET_OS_CPP_BUILTINS
#define TARGET_OS_CPP_BUILTINS()		\
  do						\
    {						\
	OPENBSD_OS_CPP_BUILTINS_ELF();		\
    }						\
  while (0)


/* XXX Why doesn't PA support -R  like everyone ??? */
#undef LINK_SPEC
#define LINK_SPEC \
  "%{EB} %{EL} %{shared} %{non_shared} \
   %{call_shared} %{no_archive} %{exact_version} \
   %{!shared: %{!non_shared: %{!call_shared: -non_shared}}} \
   %{!dynamic-linker:-dynamic-linker /usr/libexec/ld.so} \
   %{!nostdlib:%{!r*:%{!e*:-e __start}}} -dc -dp \
   %{static:-Bstatic} %{!static:-Bdynamic} %{assert*}"

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

/* Output at beginning of assembler file.  */
/* This is slightly changed from main pa.h to only output dyncall
   when compiling PIC. */
#undef ASM_FILE_START
#define ASM_FILE_START(FILE) \
do { \
     if (write_symbols != NO_DEBUG) \
       output_file_directive ((FILE), main_input_filename); \
     if (TARGET_64BIT) \
       fputs("\t.LEVEL 2.0w\n", FILE); \
     else if (TARGET_PA_20) \
       fputs("\t.LEVEL 2.0\n", FILE); \
     else if (TARGET_PA_11) \
       fputs("\t.LEVEL 1.1\n", FILE); \
     else \
       fputs("\t.LEVEL 1.0\n", FILE); \
     if (flag_pic || !TARGET_FAST_INDIRECT_CALLS) \
       fputs ("\t.IMPORT $$dyncall, MILLICODE\n", FILE); \
     if (profile_flag) \
       fputs ("\t.IMPORT _mcount, CODE\n", FILE); \
   } while (0)

#undef ASM_OUTPUT_FUNCTION_PREFIX

/* We want local labels to start with period if made with asm_fprintf.  */
#undef LOCAL_LABEL_PREFIX
#define LOCAL_LABEL_PREFIX "."

/* Use the default.  */
#undef ASM_OUTPUT_LABEL

/* This is how to output an internal numbered label where
   PREFIX is the class of label and NUM is the number within the class.

   For most svr4 systems, the convention is that any symbol which begins
   with a period is not put into the linker symbol table by the assembler.  */

#undef  ASM_OUTPUT_INTERNAL_LABEL
#define ASM_OUTPUT_INTERNAL_LABEL(FILE, PREFIX, NUM)            \
  do                                                            \
    {                                                           \
      fprintf (FILE, ".%s%u:\n", PREFIX, (unsigned) (NUM));     \
    }                                                           \
  while (0)

/* This is how to store into the string LABEL
   the symbol_ref name of an internal numbered label where
   PREFIX is the class of label and NUM is the number within the class.
   This is suitable for output with `assemble_name'.

   For most svr4 systems, the convention is that any symbol which begins
   with a period is not put into the linker symbol table by the assembler.  */

#undef  ASM_GENERATE_INTERNAL_LABEL
#define ASM_GENERATE_INTERNAL_LABEL(LABEL, PREFIX, NUM)         \
  do                                                            \
    {                                                           \
      sprintf (LABEL, "*.%s%u", PREFIX, (unsigned) (NUM));      \
    }                                                           \
  while (0)

#undef ASM_OUTPUT_ADDR_VEC_ELT
#define ASM_OUTPUT_ADDR_VEC_ELT(FILE, VALUE) \
  if (TARGET_BIG_SWITCH)					\
    fprintf (FILE, "\tstw %%r1,-16(%%r30)\n\tldil LR'.L%d,%%r1\n\tbe RR'.L%d(%%sr4,%%r1)\n\tldw -16(%%r30),%%r1\n", VALUE, VALUE);		\
  else								\
    fprintf (FILE, "\tb .L%d\n\tnop\n", VALUE)
#undef ASM_OUTPUT_ADDR_DIFF_ELT
#define ASM_OUTPUT_ADDR_DIFF_ELT(FILE, BODY, VALUE, REL) \
  if (TARGET_BIG_SWITCH)					\
    fprintf (FILE, "\tstw %%r1,-16(%%r30)\n\tldw T'.L%d(%%r19),%%r1\n\tbv %%r0(%%r1)\n\tldw -16(%%r30),%%r1\n", VALUE);				\
  else								\
    fprintf (FILE, "\tb .L%d\n\tnop\n", VALUE)

#undef STRING_ASM_OP
#define STRING_ASM_OP   ".stringz"

#undef DBX_OUTPUT_MAIN_SOURCE_FILE_END

#undef ASM_OUTPUT_SECTION_NAME
/* Switch into a generic section.
   This is currently only used to support section attributes.

   We make the section read-only and executable for a function decl,
   read-only for a const data decl, and writable for a non-const data decl.  */
#define ASM_OUTPUT_SECTION_NAME(FILE, DECL, NAME, RELOC) \
	fprintf (FILE, "\t.section\t%s,\"%s\",@progbits\n", NAME, \
	  (DECL) && TREE_CODE (DECL) == FUNCTION_DECL ? "ax" : \
	  (DECL) && DECL_READONLY_SECTION (DECL, RELOC) ? "a" : "aw")

/* Use the default.  */
#undef TARGET_ASM_GLOBALIZE_LABEL

/* FIXME: Hacked from the <elfos.h> one so that we avoid multiple
   labels in a function declaration (since pa.c seems determined to do
   it differently)  */

#undef ASM_DECLARE_FUNCTION_NAME
#define ASM_DECLARE_FUNCTION_NAME(FILE, NAME, DECL)		\
  do								\
    {								\
      ASM_OUTPUT_TYPE_DIRECTIVE (FILE, NAME, "function");	\
      ASM_DECLARE_RESULT (FILE, DECL_RESULT (DECL));		\
    }								\
  while (0)

/* As well as globalizing the label, we need to encode the label
   to ensure a plabel is generated in an indirect call.  */
#undef ASM_OUTPUT_EXTERNAL_LIBCALL
#define ASM_OUTPUT_EXTERNAL_LIBCALL(FILE, FUN)			\
  do								\
    {								\
      if (!FUNCTION_NAME_P (XSTR (FUN, 0)))			\
        hppa_encode_label (FUN);				\
      (*targetm.asm_out.globalize_label) (FILE, XSTR (FUN, 0));	\
    }								\
  while (0)

/* As an elf system, we need crtbegin/crtend stuff.  */
#undef STARTFILE_SPEC
#define STARTFILE_SPEC "%{!shared: %{pg:gcrt0%O%s} %{!pg:%{p:gcrt0%O%s} %{!p:crt0%O%s}} crtbegin%O%s} %{shared:crtbeginS%O%s}"
#undef ENDFILE_SPEC
#define ENDFILE_SPEC "%{!shared:crtend%O%s} %{shared:crtendS%O%s}"

#undef TEXT_SECTION_ASM_OP
#define TEXT_SECTION_ASM_OP "\t.text"
#undef READONLY_DATA_SECTION_ASM_OP
#define READONLY_DATA_SECTION_ASM_OP "\t.section\t.rodata"
#undef DATA_SECTION_ASM_OP
#define DATA_SECTION_ASM_OP "\t.data"
#undef BSS_SECTION_ASM_OP
#define BSS_SECTION_ASM_OP "\t.section\t.bss"
#define CTORS_SECTION_ASM_OP    "\t.section\t.ctors,\"aw\""
#define DTORS_SECTION_ASM_OP    "\t.section\t.dtors,\"aw\""
#define TARGET_ASM_NAMED_SECTION  default_elf_asm_named_section

/* Remove hpux specific pa defines. */
#undef LDD_SUFFIX
#undef PARSE_LDD_OUTPUT

#undef DO_GLOBAL_DTORS_BODY
#define	HAS_INIT_SECTION
