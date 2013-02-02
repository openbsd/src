/* Configuration file for an m68k OpenBSD target.
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

/* m68k is an old configuration that does not yet use the TARGET_CPU_DEFAULT
   framework. OpenBSD uses -m68020-60 by default.  */
#define TARGET_DEFAULT \
	(MASK_BITFIELD | MASK_68881 | MASK_68020 | MASK_68040 | MASK_68060)

#define MOTOROLA		/* Use Motorola syntax */
#define USE_GAS			/* But GAS wants jbsr instead of jsr */

/* Get generic m68k definitions. */

#include <m68k/m68k.h>

/* Get generic OpenBSD definitions.  */
#define OBSD_HAS_DECLARE_FUNCTION_NAME
#define OBSD_HAS_DECLARE_FUNCTION_SIZE
#define OBSD_HAS_DECLARE_OBJECT
#include <openbsd.h>

/* Define __HAVE_68881__ in preprocessor, unless -msoft-float is specified.
   This will control the use of inline 68881 insns in certain macros.  */
#undef CPP_SPEC
#define CPP_SPEC "%{!msoft-float:-D__HAVE_68881__ -D__HAVE_FPU__} \
		  %{posix:-D_POSIX_SOURCE} %{pthread:-D_REENTRANT} \
		  %{fPIC:-D__PIC__} %{fpic:-D__PIC__}"

/* Run-time target specifications */
#define CPP_PREDEFINES "-D__unix__ -D__m68k__ -D__mc68000__ -D__mc68020__ -D__OpenBSD__ -D__ELF__ -D__SVR4_ABI__ -Asystem(unix) -Asystem(OpenBSD) -Acpu(m68k) -Amachine(m68k)"

#undef ASM_SPEC
#define ASM_SPEC "%| %{m68030} %{m68040} %{m68060} %{fpic:-k} %{fPIC:-k -K}"

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

/* Storage layout.  */

/* optimize_reg_copy_3() is known to misbehave with some constructs */
#define	BROKEN_OPTIMIZE_REG_COPY_3_P

/* Assembler format: exception region output.  */

/* All configurations that don't use elf must be explicit about not using
   dwarf unwind information. egcs doesn't try too hard to check internal
   configuration files...  */
/* #define DWARF2_UNWIND_INFO 0 */

#define OBJECT_FORMAT_ELF

#define bsd4_4
#undef HAS_INIT_SECTION

/* Provide a set of pre-definitions and pre-assertions appropriate for
   the m68k running svr4.  */

/* This is BSD, so it wants DBX format. */

#define DBX_DEBUGGING_INFO

#undef ASM_APP_ON
#define ASM_APP_ON "#APP\n"

#undef ASM_APP_OFF
#define ASM_APP_OFF "#NO_APP\n"

/* Here are four prefixes that are used by asm_fprintf to
   facilitate customization for alternate assembler syntaxes.
   Machines with no likelihood of an alternate syntax need not
   define these and need not use asm_fprintf.  */

/* The prefix for register names.  Note that REGISTER_NAMES
   is supposed to include this prefix. Also note that this is NOT an
   fprintf format string, it is a literal string */

#undef REGISTER_PREFIX
#define REGISTER_PREFIX "%"

/* The prefix for local (compiler generated) labels.
   These labels will not appear in the symbol table. */

#undef LOCAL_LABEL_PREFIX
#define LOCAL_LABEL_PREFIX "."

/* The prefix to add to user-visible assembler symbols. */

#undef USER_LABEL_PREFIX
#define USER_LABEL_PREFIX ""

#define ASM_COMMENT_START "|"

/* How to refer to registers in assembler output.
   This sequence is indexed by compiler's hard-register-number.
   Motorola format uses different register names than defined in m68k.h.
   We also take this chance to convert 'a6' to 'fp' */

#undef REGISTER_NAMES

#ifndef SUPPORT_SUN_FPA

#define REGISTER_NAMES \
{"%d0",   "%d1",   "%d2",   "%d3",   "%d4",   "%d5",   "%d6",   "%d7",	     \
 "%a0",   "%a1",   "%a2",   "%a3",   "%a4",   "%a5",   "%fp",   "%sp",	     \
 "%fp0",  "%fp1",  "%fp2",  "%fp3",  "%fp4",  "%fp5",  "%fp6",  "%fp7" }

#else /* SUPPORTED_SUN_FPA */

#define REGISTER_NAMES \
{"%d0",   "%d1",   "%d2",   "%d3",   "%d4",   "%d5",   "%d6",   "%d7",	     \
 "%a0",   "%a1",   "%a2",   "%a3",   "%a4",   "%a5",   "%fp",   "%sp",	     \
 "%fp0",  "%fp1",  "%fp2",  "%fp3",  "%fp4",  "%fp5",  "%fp6",  "%fp7",	     \
 "%fpa0", "%fpa1", "%fpa2", "%fpa3", "%fpa4", "%fpa5", "%fpa6","%fpa7",	     \
 "%fpa8", "%fpa9", "%fpa10","%fpa11","%fpa12","%fpa13","%fpa14","%fpa15",    \
 "%fpa16","%fpa17","%fpa18","%fpa19","%fpa20","%fpa21","%fpa22","%fpa23",    \
 "%fpa24","%fpa25","%fpa26","%fpa27","%fpa28","%fpa29","%fpa30","%fpa31" }

#endif /* defined SUPPORT_SUN_FPA */

/* Currently, JUMP_TABLES_IN_TEXT_SECTION must be defined in order to
   keep switch tables in the text section.  */

#define JUMP_TABLES_IN_TEXT_SECTION 1

/* Use the default action for outputting the case label.  */
#undef ASM_OUTPUT_CASE_LABEL
#define ASM_RETURN_CASE_JUMP			\
  do {						\
    if (TARGET_5200)				\
      return "ext%.l %0\n\tjmp %%pc@(2,%0:l)";	\
    else					\
      return "jmp %%pc@(2,%0:w)";		\
  } while (0)

/* This is how to output an assembler line that says to advance the
   location counter to a multiple of 2**LOG bytes.  */

#undef ALIGN_ASM_OP
#define ALIGN_ASM_OP ".align"

#undef ASM_OUTPUT_ALIGN
#define ASM_OUTPUT_ALIGN(FILE,LOG)				\
do {								\
  if ((LOG) > 0)						\
    fprintf ((FILE), "\t%s %u\n", ALIGN_ASM_OP, 1 << (LOG));	\
} while (0)

/* If defined, a C expression whose value is a string containing the
   assembler operation to identify the following data as uninitialized global
   data.  */

#define BSS_SECTION_ASM_OP ".section\t.bss"

/* A C statement (sans semicolon) to output to the stdio stream
   FILE the assembler definition of uninitialized global DECL named
   NAME whose size is SIZE bytes and alignment is ALIGN bytes.
   Try to use asm_output_aligned_bss to implement this macro.  */

#define ASM_OUTPUT_ALIGNED_BSS(FILE, DECL, NAME, SIZE, ALIGN) \
  asm_output_aligned_bss (FILE, DECL, NAME, SIZE, ALIGN)

/* Section output setup. */

#define	USE_CONST_SECTION	1

#define	BSS_SECTION_ASM_OP	".section\t.bss"
#define CONST_SECTION_ASM_OP    ".section\t.rodata"
#define CTORS_SECTION_ASM_OP    ".section\t.ctors,\"aw\""
#define DTORS_SECTION_ASM_OP    ".section\t.dtors,\"aw\""
#define INIT_SECTION_ASM_OP     ".section\t.init"
#define FINI_SECTION_ASM_OP     ".section\t.fini"

#undef EXTRA_SECTIONS
#define EXTRA_SECTIONS in_const, in_ctors, in_dtors

#undef EXTRA_SECTION_FUNCTIONS
#define EXTRA_SECTION_FUNCTIONS						\
  CONST_SECTION_FUNCTION						\
  CTORS_SECTION_FUNCTION						\
  DTORS_SECTION_FUNCTION

#undef READONLY_DATA_SECTION
#define READONLY_DATA_SECTION() const_section ()

extern void text_section ();

#define CONST_SECTION_FUNCTION						\
void									\
const_section ()							\
{									\
  if (!USE_CONST_SECTION)						\
    text_section();							\
  else if (in_section != in_const)					\
    {									\
      fprintf (asm_out_file, "%s\n", CONST_SECTION_ASM_OP);		\
      in_section = in_const;						\
    }									\
}

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

/* Switch into a generic section.
   This is currently only used to support section attributes.

    We make the section read-only and executable for a function decl,
    read-only for a const data decl, and writable for a non-const data decl.  */
#define ASM_OUTPUT_SECTION_NAME(FILE, DECL, NAME, RELOC) \
  fprintf (FILE, ".section\t%s,\"%s\",@progbits\n", NAME, \
	   (DECL) && TREE_CODE (DECL) == FUNCTION_DECL ? "ax" : \
	   (DECL) && DECL_READONLY_SECTION (DECL, RELOC) ? "a" : "aw")

/* A C statement (sans semicolon) to output an element in the table of
   global constructors.  */
#define ASM_OUTPUT_CONSTRUCTOR(FILE,NAME)				\
  do {									\
    ctors_section ();							\
    fprintf (FILE, "\t.long\t ");					\
    assemble_name (FILE, NAME);						\
    fprintf (FILE, "\n");						\
  } while (0)

/* A C statement (sans semicolon) to output an element in the table of
   global destructors.  */
#define ASM_OUTPUT_DESTRUCTOR(FILE,NAME)       				\
  do {									\
    dtors_section ();                   				\
    fprintf (FILE, "\t.long\t ");					\
    assemble_name (FILE, NAME);              				\
    fprintf (FILE, "\n");						\
  } while (0)

/* These macros generate the special .type and .size directives which
   are used to set the corresponding fields of the linker symbol table
   entries in an ELF object file under SVR4.  These macros also output
   the starting labels for the relevant functions/objects.  */

/* Write the extra assembler code needed to declare a function properly.
   Some svr4 assemblers need to also have something extra said about the
   function's return value.  We allow for that here.  */

#define ASM_DECLARE_FUNCTION_NAME(FILE, NAME, DECL)			\
  do {									\
    fprintf (FILE, "\t%s\t", TYPE_ASM_OP);				\
    assemble_name (FILE, NAME);						\
    putc (',', FILE);							\
    fprintf (FILE, TYPE_OPERAND_FMT, "function");			\
    putc ('\n', FILE);							\
    ASM_DECLARE_RESULT (FILE, DECL_RESULT (DECL));			\
    ASM_OUTPUT_LABEL(FILE, NAME);					\
  } while (0)

/* Write the extra assembler code needed to declare an object properly.  */

#define ASM_DECLARE_OBJECT_NAME(FILE, NAME, DECL)			\
  do {									\
    fprintf (FILE, "\t%s\t", TYPE_ASM_OP);				\
    assemble_name (FILE, NAME);						\
    putc (',', FILE);							\
    fprintf (FILE, TYPE_OPERAND_FMT, "object");				\
    putc ('\n', FILE);							\
    size_directive_output = 0;						\
    if (!flag_inhibit_size_directive && DECL_SIZE (DECL))		\
      {									\
	size_directive_output = 1;					\
	fprintf (FILE, "\t%s\t", SIZE_ASM_OP);				\
	assemble_name (FILE, NAME);					\
	putc (',', FILE);						\
	fprintf (FILE, HOST_WIDE_INT_PRINT_DEC,				\
		 int_size_in_bytes (TREE_TYPE (DECL)));			\
	fputc ('\n', FILE);						\
      }									\
    ASM_OUTPUT_LABEL(FILE, NAME);					\
  } while (0)

/* Output the size directive for a decl in rest_of_decl_compilation
   in the case where we did not do so before the initializer.
   Once we find the error_mark_node, we know that the value of
   size_directive_output was set
   by ASM_DECLARE_OBJECT_NAME when it was run for the same decl.  */

#define ASM_FINISH_DECLARE_OBJECT(FILE, DECL, TOP_LEVEL, AT_END)	 \
do {									 \
     char *name = XSTR (XEXP (DECL_RTL (DECL), 0), 0);			 \
     if (!flag_inhibit_size_directive && DECL_SIZE (DECL)		 \
         && ! AT_END && TOP_LEVEL					 \
	 && DECL_INITIAL (DECL) == error_mark_node			 \
	 && !size_directive_output)					 \
       {								 \
	 size_directive_output = 1;					 \
	 fprintf (FILE, "\t%s\t", SIZE_ASM_OP);			 \
	 assemble_name (FILE, name);					 \
	 putc (',', FILE);						 \
	 fprintf (FILE, HOST_WIDE_INT_PRINT_DEC,			 \
		  int_size_in_bytes (TREE_TYPE (DECL))); 		 \
	fputc ('\n', FILE);						 \
       }								 \
   } while (0)

/* This is how to declare the size of a function.  */

#define ASM_DECLARE_FUNCTION_SIZE(FILE, FNAME, DECL)			\
  do {									\
    if (!flag_inhibit_size_directive)					\
      {									\
        char label[256];						\
	static int labelno;						\
	labelno++;							\
	ASM_GENERATE_INTERNAL_LABEL (label, "Lfe", labelno);		\
	ASM_OUTPUT_INTERNAL_LABEL (FILE, "Lfe", labelno);		\
	fprintf (FILE, "\t%s\t", SIZE_ASM_OP);				\
	assemble_name (FILE, (FNAME));					\
        fprintf (FILE, ",");						\
	assemble_name (FILE, label);					\
        fprintf (FILE, "-");						\
	assemble_name (FILE, (FNAME));					\
	putc ('\n', FILE);						\
      }									\
  } while (0)

/* This is how we tell the assembler that two symbols have the same value.  */

#define ASM_OUTPUT_DEF(FILE,NAME1,NAME2) \
  do { assemble_name(FILE, NAME1); 	 \
       fputs(" = ", FILE);		 \
       assemble_name(FILE, NAME2);	 \
       fputc('\n', FILE); } while (0)

#undef ASM_OUTPUT_COMMON
#undef ASM_OUTPUT_LOCAL
#define ASM_OUTPUT_COMMON(FILE, NAME, SIZE, ROUNDED)  \
( fputs (".comm ", (FILE)),			\
  assemble_name ((FILE), (NAME)),		\
  fprintf ((FILE), ",%u\n", (SIZE)))

#define ASM_OUTPUT_LOCAL(FILE, NAME, SIZE, ROUNDED)  \
( fputs (".lcomm ", (FILE)),			\
  assemble_name ((FILE), (NAME)),		\
  fprintf ((FILE), ",%u\n", (SIZE)))

/* Currently, JUMP_TABLES_IN_TEXT_SECTION must be defined in order to
   keep switch tables in the text section.  */

#define JUMP_TABLES_IN_TEXT_SECTION 1

/* Output assembler code to FILE to increment profiler label # LABELNO
   for profiling a function entry. */

#undef FUNCTION_PROFILER
#define FUNCTION_PROFILER(FILE, LABELNO) \
do {									\
  asm_fprintf (FILE, "\tlea (%LLP%d,%Rpc),%Ra1\n", (LABELNO));		\
  if (flag_pic)								\
    fprintf (FILE, "\tbsr.l __mcount@PLTPC\n");				\
  else									\
    fprintf (FILE, "\tjbsr __mcount\n");				\
} while (0)

/* Register in which address to store a structure value is passed to a
   function.  The default in m68k.h is a1.  For m68k/SVR4 it is a0. */

#undef STRUCT_VALUE_REGNUM
#define STRUCT_VALUE_REGNUM 8

/* Register in which static-chain is passed to a function.  The
   default in m68k.h is a0, but that is already the struct value
   regnum.  Make it a1 instead.  */

#undef STATIC_CHAIN_REGNUM
#define STATIC_CHAIN_REGNUM 9

/* How to renumber registers for dbx and gdb.
   On the Sun-3, the floating point registers have numbers
   18 to 25, not 16 to 23 as they do in the compiler.  */

#define DBX_REGISTER_NUMBER(REGNO) ((REGNO) < 16 ? (REGNO) : (REGNO) + 2)

/* Do not break .stabs pseudos into continuations.  */

#define DBX_CONTIN_LENGTH 0

/* 1 if N is a possible register number for a function value.  For
   m68k/SVR4 allow d0, a0, or fp0 as return registers, for integral,
   pointer, or floating types, respectively.  Reject fp0 if not using
   a 68881 coprocessor.  */

#undef FUNCTION_VALUE_REGNO_P
#define FUNCTION_VALUE_REGNO_P(N) \
  ((N) == 0 || (N) == 8 || (TARGET_68881 && (N) == 16))

/* Define this to be true when FUNCTION_VALUE_REGNO_P is true for
   more than one register.  */

#undef NEEDS_UNTYPED_CALL
#define NEEDS_UNTYPED_CALL 1

/* Define how to generate (in the callee) the output value of a
   function and how to find (in the caller) the value returned by a
   function.  VALTYPE is the data type of the value (as a tree).  If
   the precise function being called is known, FUNC is its
   FUNCTION_DECL; otherwise, FUNC is 0.  For m68k/SVR4 generate the
   result in d0, a0, or fp0 as appropriate. */
   
#undef FUNCTION_VALUE
#define FUNCTION_VALUE(VALTYPE, FUNC)					\
  (TREE_CODE (VALTYPE) == REAL_TYPE && TARGET_68881			\
   ? gen_rtx_REG (TYPE_MODE (VALTYPE), 16)				\
   : (POINTER_TYPE_P (VALTYPE)						\
      ? gen_rtx_REG (TYPE_MODE (VALTYPE), 8)				\
      : gen_rtx_REG (TYPE_MODE (VALTYPE), 0)))

/* For compatibility with the large body of existing code which does
   not always properly declare external functions returning pointer
   types, the m68k/SVR4 convention is to copy the value returned for
   pointer functions from a0 to d0 in the function epilogue, so that
   callers that have neglected to properly declare the callee can
   still find the correct return value.  */

extern int current_function_returns_pointer;
#define FUNCTION_EXTRA_EPILOGUE(FILE, SIZE)				\
do {									\
  if ((current_function_returns_pointer) && 				\
      ! find_equiv_reg (0, get_last_insn (), 0, 0, 0, 8, Pmode))	\
    asm_fprintf (FILE, "\tmove.l %Ra0,%Rd0\n");				\
} while (0);

/* Define how to find the value returned by a library function
   assuming the value has mode MODE.
   For m68k/SVR4 look for integer values in d0, pointer values in d0
   (returned in both d0 and a0), and floating values in fp0.  */

#undef LIBCALL_VALUE
#define LIBCALL_VALUE(MODE)						\
  ((((MODE) == SFmode || (MODE) == DFmode || (MODE) == XFmode)		\
    && TARGET_68881)							\
   ? gen_rtx_REG (MODE, 16)						\
   : gen_rtx_REG (MODE, 0))

/* Boundary (in *bits*) on which stack pointer should be aligned.
   The m68k/SVR4 convention is to keep the stack pointer longword aligned. */

#undef STACK_BOUNDARY
#define STACK_BOUNDARY 32

/* Alignment of field after `int : 0' in a structure.
   For m68k/SVR4, this is the next longword boundary. */

#undef EMPTY_FIELD_BOUNDARY
#define EMPTY_FIELD_BOUNDARY 32

/* No data type wants to be aligned rounder than this.
   For m68k/SVR4, some types (doubles for example) are aligned on 8 byte
   boundaries */

#undef BIGGEST_ALIGNMENT
#define BIGGEST_ALIGNMENT 64

/* In m68k svr4, a symbol_ref rtx can be a valid PIC operand if it is
   an operand of a function call. */
#undef LEGITIMATE_PIC_OPERAND_P
#define LEGITIMATE_PIC_OPERAND_P(X) \
  ((! symbolic_operand (X, VOIDmode) \
    && ! (GET_CODE (X) == CONST_DOUBLE && CONST_DOUBLE_MEM (X)	\
	  && GET_CODE (CONST_DOUBLE_MEM (X)) == MEM		\
	  && symbolic_operand (XEXP (CONST_DOUBLE_MEM (X), 0), VOIDmode))) \
   || (GET_CODE (X) == SYMBOL_REF && SYMBOL_REF_FLAG (X)))

/* Turn off function cse if we are doing PIC. We always want function
   call to be done as `bsr foo@PLTPC', so it will force the assembler
   to create the PLT entry for `foo'.  Doing function cse will cause
   the address of `foo' to be loaded into a register, which is exactly
   what we want to avoid when we are doing PIC on svr4 m68k.  */
#undef SUBTARGET_OVERRIDE_OPTIONS
#define SUBTARGET_OVERRIDE_OPTIONS \
  if (flag_pic) flag_no_function_cse = 1;

/* For m68k SVR4, structures are returned using the reentrant
   technique. */

#undef PCC_STATIC_STRUCT_RETURN

/* The svr4 ABI for the m68k says that records and unions are returned
   in memory.  */

#define DEFAULT_PCC_STRUCT_RETURN 1

/* Output code to add DELTA to the first argument, and then jump to FUNCTION.
   Used for C++ multiple inheritance.  */
#define ASM_OUTPUT_MI_THUNK(FILE, THUNK_FNDECL, DELTA, FUNCTION)	\
do {									\
  if (DELTA > 0 && DELTA <= 8)						\
    asm_fprintf (FILE, "\taddq.l %I%d,4(%Rsp)\n", DELTA);		\
  else if (DELTA < 0 && DELTA >= -8)					\
    asm_fprintf (FILE, "\tsubq.l %I%d,4(%Rsp)\n", -DELTA);		\
  else									\
    asm_fprintf (FILE, "\tadd.l %I%d,4(%Rsp)\n", DELTA);		\
									\
  if (flag_pic)								\
    {									\
      fprintf (FILE, "\tbra.l ");					\
      assemble_name (FILE, XSTR (XEXP (DECL_RTL (FUNCTION), 0), 0));	\
      fprintf (FILE, "@PLTPC\n");					\
    }									\
  else									\
    {									\
      fprintf (FILE, "\tjmp ");						\
      assemble_name (FILE, XSTR (XEXP (DECL_RTL (FUNCTION), 0), 0));	\
      fprintf (FILE, "\n");						\
    }									\
} while (0)

/* Output assembler code for a block containing the constant parts
   of a trampoline, leaving space for the variable parts.  */

/* On m68k svr4, the trampoline is different from the generic version
   in that we use a1 as the static call chain.  */

#undef TRAMPOLINE_TEMPLATE
#define TRAMPOLINE_TEMPLATE(FILE)					\
{									\
  ASM_OUTPUT_SHORT (FILE, GEN_INT (0x227a));				\
  ASM_OUTPUT_SHORT (FILE, GEN_INT (8));					\
  ASM_OUTPUT_SHORT (FILE, GEN_INT (0x2f3a));				\
  ASM_OUTPUT_SHORT (FILE, GEN_INT (8));					\
  ASM_OUTPUT_SHORT (FILE, GEN_INT (0x4e75));				\
  ASM_OUTPUT_INT (FILE, const0_rtx);					\
  ASM_OUTPUT_INT (FILE, const0_rtx);					\
}

/* Redefine since we are using a different trampoline */
#undef TRAMPOLINE_SIZE
#define TRAMPOLINE_SIZE 18

/* Emit RTL insns to initialize the variable parts of a trampoline.
   FNADDR is an RTX for the address of the function's pure code.
   CXT is an RTX for the static chain value for the function.  */

#undef INITIALIZE_TRAMPOLINE
#define INITIALIZE_TRAMPOLINE(TRAMP, FNADDR, CXT)                       \
{                                                                       \
  emit_move_insn (gen_rtx (MEM, SImode, plus_constant (TRAMP, 10)), CXT); \
  emit_move_insn (gen_rtx (MEM, SImode, plus_constant (TRAMP, 14)), FNADDR); \
}
