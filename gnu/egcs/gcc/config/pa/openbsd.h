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
#define OBSD_HAS_DECLARE_FUNCTION_NAME
#include <openbsd.h>

#define	INT_ASM_OP	".long"

/* Run-time target specifications. */
#define CPP_PREDEFINES "-D__unix__ -D__ANSI_COMPAT -Asystem(unix) -Asystem(OpenBSD) -Amachine(hppa) -D__ELF__ -D__OpenBSD__ -D__hppa__ -D__hppa"

#undef OVERRIDE_OPTIONS
#define OVERRIDE_OPTIONS						 \
{							                 \
    override_options ();				                 \
    if (! flag_pic)						         \
        target_flags |= MASK_PORTABLE_RUNTIME | MASK_FAST_INDIRECT_CALLS;\
}
	
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

/* This must agree with <machine/ansi.h> */
#undef SIZE_TYPE
#define SIZE_TYPE "unsigned int"

#undef PTRDIFF_TYPE
#define PTRDIFF_TYPE "int"

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
     if (flag_pic || !TARGET_FAST_INDIRECT_CALLS)\
       fputs ("\t.IMPORT $$dyncall, MILLICODE\n", FILE);\
     if (profile_flag)\
       fputs ("\t.IMPORT _mcount, CODE\n", FILE);\
     if (write_symbols != NO_DEBUG) \
       output_file_directive ((FILE), main_input_filename); \
   } while (0)

#undef ASM_OUTPUT_FUNCTION_PREFIX

#undef SELECT_RTX_SECTION
#define SELECT_RTX_SECTION(MODE,RTX)	readonly_data_section ();

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

/* A C statement (sans semicolon) to output an element in the table of
   global constructors.  */
#define ASM_OUTPUT_CONSTRUCTOR(FILE,NAME)				\
  do {									\
    ctors_section ();							\
    fprintf (FILE, "\t%s\t ", INT_ASM_OP);				\
    assemble_name (FILE, NAME);						\
    fprintf (FILE, "\n");						\
  } while (0)

/* A C statement (sans semicolon) to output an element in the table of
   global destructors.  */
#define ASM_OUTPUT_DESTRUCTOR(FILE,NAME)       				\
  do {									\
    dtors_section ();                   				\
    fprintf (FILE, "\t%s\t ", INT_ASM_OP);				\
    assemble_name (FILE, NAME);              				\
    fprintf (FILE, "\n");						\
  } while (0)

/* As an elf system, we need crtbegin/crtend stuff.  */
#undef STARTFILE_SPEC
#define STARTFILE_SPEC "%{!shared: %{pg:gcrt0%O%s} %{!pg:%{p:gcrt0%O%s} %{!p:crt0%O%s}} crtbegin%O%s} %{shared:crtbeginS%O%s}"
#undef ENDFILE_SPEC
#define ENDFILE_SPEC "%{!shared:crtend%O%s} %{shared:crtendS%O%s}"

#undef TEXT_SECTION_ASM_OP
#define TEXT_SECTION_ASM_OP "\t.text"
#undef CONST_SECTION_ASM_OP
#define CONST_SECTION_ASM_OP "\t.section\t.rodata"
#undef DATA_SECTION_ASM_OP
#define DATA_SECTION_ASM_OP "\t.data"
#undef BSS_SECTION_ASM_OP
#define BSS_SECTION_ASM_OP "\t.section\t.bss"
#define CTORS_SECTION_ASM_OP    "\t.section\t.ctors,\"aw\""
#define DTORS_SECTION_ASM_OP    "\t.section\t.dtors,\"aw\""

/* Remove hpux specific pa defines. */
#undef LDD_SUFFIX
#undef PARSE_LDD_OUTPUT

#undef DO_GLOBAL_DTORS_BODY
#define	HAS_INIT_SECTION

#undef CTORS_SECTION_FUNCTION
#define CTORS_SECTION_FUNCTION						\
void									\
ctors_section ()							\
{									\
  if (in_section != in_ctors)						\
    {									\
      fprintf (asm_out_file, "%s\n", CTORS_SECTION_ASM_OP);		\
      in_section = in_ctors;						\
    }									\
}

#undef DTORS_SECTION_FUNCTION
#define DTORS_SECTION_FUNCTION						\
void									\
dtors_section ()							\
{									\
  if (in_section != in_dtors)						\
    {									\
      fprintf (asm_out_file, "%s\n", DTORS_SECTION_ASM_OP);		\
      in_section = in_dtors;						\
    }									\
}

#undef READONLY_DATA_SECTION
#define READONLY_DATA_SECTION() const_section ()

#undef CONST_SECTION_FUNCTION
#define CONST_SECTION_FUNCTION						\
void									\
const_section ()                                                        \
{									\
  if (in_section != in_const)						\
    {									\
      fprintf (asm_out_file, "%s\n", CONST_SECTION_ASM_OP);		\
      in_section = in_const;						\
    }									\
}

#undef EXTRA_SECTIONS
#define EXTRA_SECTIONS in_const, in_ctors, in_dtors

#undef EXTRA_SECTION_FUNCTIONS
#define EXTRA_SECTION_FUNCTIONS						\
  CONST_SECTION_FUNCTION						\
  CTORS_SECTION_FUNCTION						\
  DTORS_SECTION_FUNCTION

/* A C statement or statements to switch to the appropriate
   section for output of DECL.  DECL is either a `VAR_DECL' node
   or a constant of some sort.  RELOC indicates whether forming
   the initial value of DECL requires link-time relocations.  */

#undef SELECT_SECTION
#define SELECT_SECTION(DECL,RELOC)					\
{									\
  if (TREE_CODE (DECL) == STRING_CST)					\
    {									\
      if (! flag_writable_strings)					\
	const_section ();						\
      else								\
	data_section ();						\
    }									\
  else if (TREE_CODE (DECL) == VAR_DECL					\
	   || TREE_CODE (DECL) == CONSTRUCTOR)				\
    {									\
      if ((flag_pic && RELOC)						\
	  || !TREE_READONLY (DECL) || TREE_SIDE_EFFECTS (DECL)		\
	  || !DECL_INITIAL (DECL)					\
	  || (DECL_INITIAL (DECL) != error_mark_node			\
	      && !TREE_CONSTANT (DECL_INITIAL (DECL))))			\
	data_section ();						\
      else								\
	const_section ();						\
    }									\
  else									\
    const_section ();							\
}

/* A C statement or statements to switch to the appropriate
   section for output of RTX in mode MODE.  RTX is some kind
   of constant in RTL.  The argument MODE is redundant except
   in the case of a `const_int' rtx.  Currently, these always
   go into the const section.  */

#undef SELECT_RTX_SECTION
#define SELECT_RTX_SECTION(MODE,RTX) const_section()
