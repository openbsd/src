#include <pa/pa.h>
#define OBSD_HAS_DECLARE_FUNCTION_NAME
#include <openbsd.h>

/* run-time target specifications */
#define CPP_PREDEFINES "-D__unix__ -D__ANSI_COMPAT -Asystem(unix) -Asystem(OpenBSD) -Amachine(hppa) -D__OpenBSD__ -D__hppa__ -D__hppa"

/* XXX why doesn't PA support -R  like everyone ??? */
#undef LINK_SPEC
#define LINK_SPEC \
  "%{EB} %{EL} %{shared} %{non_shared} \
   %{call_shared} %{no_archive} %{exact_version} \
   %{!shared: %{!non_shared: %{!call_shared: -non_shared}}} \
   %{!dynamic-linker:-dynamic-linker /usr/libexec/ld.so} \
   %{!nostdlib:%{!r*:%{!e*:-e __start}}} -dc -dp \
   %{static:-Bstatic} %{!static:-Bdynamic} %{assert*}"

/* layout of source language data types
 * ------------------------------------ */
/* this must agree with <machine/ansi.h> */
#undef SIZE_TYPE
#define SIZE_TYPE "unsigned int"

#undef PTRDIFF_TYPE
#define PTRDIFF_TYPE "int"

#undef WCHAR_TYPE
#define WCHAR_TYPE "int"

#undef WCHAR_TYPE_SIZE
#define WCHAR_TYPE_SIZE 32

/* Output at beginning of assembler file.  */
/* this is slightly changed from main pa.h to only output dyncall
 * when compiling PIC
 */
#undef ASM_FILE_START
#define ASM_FILE_START(FILE) \
do { fputs ("\t.SPACE $PRIVATE$\n\
\t.SUBSPA $DATA$,QUAD=1,ALIGN=8,ACCESS=0x1f,SORT=24\n\
\t.SUBSPA $BSS$,QUAD=1,ALIGN=8,ACCESS=0x1f,ZERO,SORT=80\n\
\t.SPACE $TEXT$\n\
\t.SUBSPA $LIT$,QUAD=0,ALIGN=8,ACCESS=0x2c\n\
\t.SUBSPA $CODE$,QUAD=0,ALIGN=8,ACCESS=0x2c,CODE_ONLY\n\
\t.IMPORT $global$,DATA\n", FILE);\
     if (flag_pic || !TARGET_FAST_INDIRECT_CALLS)\
       fputs ("\t.IMPORT $$dyncall, MILLICODE\n", FILE);\
     if (profile_flag)\
       fputs ("\t.IMPORT _mcount, CODE\n", FILE);\
     if (write_symbols != NO_DEBUG) \
       output_file_directive ((FILE), main_input_filename); \
   } while (0)

/* remove hpux specific pa defines */
#undef LDD_SUFFIX
#undef PARSE_LDD_OUTPUT
