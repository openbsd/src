/*	$OpenBSD: openbsd.h,v 1.14 1999/01/17 17:41:13 espie Exp $	*/

/* OPENBSD_NATIVE is defined when gcc is integrated into the OpenBSD
   source tree so it can be configured appropriately when using the
   'wrapper' makefile with the GNU configure/build mechanism. The
   'wrapper' method and use of OPENBSD_NATIVE is NOT recommended
   while building cross-compilers. */

#ifdef OPENBSD_NATIVE

#undef GPLUSPLUS_INCLUDE_DIR
#define GPLUSPLUS_INCLUDE_DIR "/usr/include/g++"

#undef GCC_INCLUDE_DIR
#define GCC_INCLUDE_DIR "/usr/include"

/* Look for the include files in the system-defined places.  */

#undef INCLUDE_DEFAULTS
#define INCLUDE_DEFAULTS			\
  {						\
    { GPLUSPLUS_INCLUDE_DIR, "G++", 1, 1 },	\
    { GCC_INCLUDE_DIR, "GCC", 0, 0 },		\
    { 0, 0, 0, 0 }				\
  }

/* Under OpenBSD, the normal location of the various *crt*.o files is the
   /usr/lib directory.  */

#undef STANDARD_STARTFILE_PREFIX
#define STANDARD_STARTFILE_PREFIX	"/usr/lib/"

#endif


/* Controlling the compilation driver 
 * ---------------------------------- */

/* CPP_SPEC appropriate for OpenBSD. We deal with -posix and -pthread */
#undef CPP_SPEC
#define CPP_SPEC "%{posix:-D_POSIX_SOURCE} %{pthread:-D_POSIX_THREADS}"


#ifdef OBSD_OLD_GAS
/* ASM_SPEC appropriate for OpenBSD.  For some architectures, OpenBSD 
   still uses a special flavor of gas that needs to be told when generating 
   pic code. */
#undef ASM_SPEC
#define ASM_SPEC "%{fpic:-k} %{fPIC:-k -K} %|"
#else
/* Since we use gas, stdin -> - is a good idea, but we don't want to
   override native specs just for that. */
#ifndef ASM_SPEC
#define ASM_SPEC "%|"
#endif
#endif

/* LIB_SPEC appropriate for OpenBSD.  Select the appropriate libc, 
   depending on profiling and threads.
   Basically, -lc(_r)?(_p)?, select _r for threads, and _p for p or pg
 */
#undef LIB_SPEC
#define LIB_SPEC "-lc%{pthread:_r}%{p:_p}%{!p:%{pg:_p}}"

/* LINK_SPEC appropriate for OpenBSD.  Support for GCC options 
   -static, -assert, and -nostdlib.  */
#undef LINK_SPEC
#define LINK_SPEC \
  "%{!nostdlib:%{!r*:%{!e*:-e start}}} -dc -dp %{R*} %{static:-Bstatic} %{assert*}"

/* Add the -R arg switch, needed for dynamic library support. */
#undef SWITCH_TAKES_ARG
#define SWITCH_TAKES_ARG(CHAR) \
  (DEFAULT_SWITCH_TAKES_ARG(CHAR) \
   || (CHAR) == 'R')

/* Runtime target specification 
 * ---------------------------- */

/* You must redefine CPP_PREDEFINES in any arch specific file. */
#undef CPP_PREDEFINES

/* we want gcc.c to call mkstemps for each file it generates
   (fix taken from egcs-current). */
#define MKTEMP_EACH_FILE

/* Implicit calls to library routines
 * ---------------------------------- */
/* Use memcpy and memset instead of bcopy and bzero for implicit library
   calls. */
#define TARGET_MEM_FUNCTIONS

/* Miscellaneous parameters
 * ------------------------ */
/* tell libgcc2.c that OpenBSD targets support atexit */
#define HAVE_ATEXIT


/*
 * Some imports from svr4.h in support of shared libraries.
 * Currently, we need the DECLARE_OBJECT_SIZE stuff.
 */

/* Define the strings used for the .type, .size, and .set directives.
   These strings generally do not vary from one system running openbsd
   to another, but if a given system needs to use different pseudo-op
   names for these, they may be overridden in the file which includes
   this one.  */

#undef TYPE_ASM_OP
#undef SIZE_ASM_OP
#undef SET_ASM_OP
#define TYPE_ASM_OP	".type"
#define SIZE_ASM_OP	".size"
#define SET_ASM_OP	".set"

/* This is how we tell the assembler that a symbol is weak.  */

#undef ASM_WEAKEN_LABEL
#define ASM_WEAKEN_LABEL(FILE,NAME) \
  do { fputs ("\t.weak\t", FILE); assemble_name (FILE, NAME); \
       fputc ('\n', FILE); } while (0)

/* The following macro defines the format used to output the second
   operand of the .type assembler directive.  Different svr4 assemblers
   expect various different forms for this operand.  The one given here
   is just a default.  You may need to override it in your machine-
   specific tm.h file (depending upon the particulars of your assembler).  */

#undef TYPE_OPERAND_FMT
#define TYPE_OPERAND_FMT	"@%s"

/* Write the extra assembler code needed to declare a function's result.
   Most svr4 assemblers don't require any special declaration of the
   result value, but there are exceptions.  */

#ifndef ASM_DECLARE_RESULT
#define ASM_DECLARE_RESULT(FILE, RESULT)
#endif

/* These macros generate the special .type and .size directives which
   are used to set the corresponding fields of the linker symbol table
   entries in an ELF object file under SVR4.  These macros also output
   the starting labels for the relevant functions/objects.  */

/* Write the extra assembler code needed to declare a function properly.
   Some svr4 assemblers need to also have something extra said about the
   function's return value.  We allow for that here.  */

#undef ASM_DECLARE_FUNCTION_NAME
#define ASM_DECLARE_FUNCTION_NAME(FILE, NAME, DECL)			\
  do {									\
    fprintf (FILE, "\t%s\t ", TYPE_ASM_OP);				\
    assemble_name (FILE, NAME);						\
    putc (',', FILE);							\
    fprintf (FILE, TYPE_OPERAND_FMT, "function");			\
    putc ('\n', FILE);							\
    ASM_DECLARE_RESULT (FILE, DECL_RESULT (DECL));			\
    ASM_OUTPUT_LABEL(FILE, NAME);					\
  } while (0)

/* Write the extra assembler code needed to declare an object properly.  */

#undef ASM_DECLARE_OBJECT_NAME
#define ASM_DECLARE_OBJECT_NAME(FILE, NAME, DECL)			\
  do {									\
    fprintf (FILE, "\t%s\t ", TYPE_ASM_OP);				\
    assemble_name (FILE, NAME);						\
    putc (',', FILE);							\
    fprintf (FILE, TYPE_OPERAND_FMT, "object");				\
    putc ('\n', FILE);							\
    size_directive_output = 0;						\
    if (!flag_inhibit_size_directive && DECL_SIZE (DECL))		\
      {									\
	size_directive_output = 1;					\
	fprintf (FILE, "\t%s\t ", SIZE_ASM_OP);				\
	assemble_name (FILE, NAME);					\
	fprintf (FILE, ",%d\n",  int_size_in_bytes (TREE_TYPE (DECL)));	\
      }									\
    ASM_OUTPUT_LABEL(FILE, NAME);					\
  } while (0)

/* Output the size directive for a decl in rest_of_decl_compilation
   in the case where we did not do so before the initializer.
   Once we find the error_mark_node, we know that the value of
   size_directive_output was set
   by ASM_DECLARE_OBJECT_NAME when it was run for the same decl.  */

#undef ASM_FINISH_DECLARE_OBJECT
#define ASM_FINISH_DECLARE_OBJECT(FILE, DECL, TOP_LEVEL, AT_END)	 \
do {									 \
     char *name = XSTR (XEXP (DECL_RTL (DECL), 0), 0);			 \
     if (!flag_inhibit_size_directive && DECL_SIZE (DECL)		 \
         && ! AT_END && TOP_LEVEL					 \
	 && DECL_INITIAL (DECL) == error_mark_node			 \
	 && !size_directive_output)					 \
       {								 \
	 size_directive_output = 1;					 \
	 fprintf (FILE, "\t%s\t ", SIZE_ASM_OP);			 \
	 assemble_name (FILE, name);					 \
	 fprintf (FILE, ",%d\n",  int_size_in_bytes (TREE_TYPE (DECL))); \
       }								 \
   } while (0)

/* This is how to declare the size of a function.  */

#undef ASM_DECLARE_FUNCTION_SIZE
#define ASM_DECLARE_FUNCTION_SIZE(FILE, FNAME, DECL)			\
  do {									\
    if (!flag_inhibit_size_directive)					\
      {									\
        char label[256];						\
	static int labelno;						\
	labelno++;							\
	ASM_GENERATE_INTERNAL_LABEL (label, "Lfe", labelno);		\
	ASM_OUTPUT_INTERNAL_LABEL (FILE, "Lfe", labelno);		\
	fprintf (FILE, "\t%s\t ", SIZE_ASM_OP);				\
	assemble_name (FILE, (FNAME));					\
        fprintf (FILE, ",");						\
	assemble_name (FILE, label);					\
        fprintf (FILE, "-");						\
	assemble_name (FILE, (FNAME));					\
	putc ('\n', FILE);						\
      }									\
  } while (0)
