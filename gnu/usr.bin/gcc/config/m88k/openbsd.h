/* BSD a.out, not COFF or ELF.  */

#define DBX_DEBUGGING_INFO
#define DEFAULT_GDB_EXTENSIONS 0

#include "aoutos.h"
#include "m88k/m88k.h"
#include <openbsd.h>

/* Identify the compiler.  */
#undef  VERSION_INFO1
#define VERSION_INFO1 "Motorola m88k, "

/* Macros to be automatically defined.  */
#undef	CPP_PREDEFINES
#define CPP_PREDEFINES \
    "-Dunix -D__OpenBSD__ -D__CLASSIFY_TYPE__=2 -Asystem(unix) -Asystem(OpenBSD) -Acpu(m88k) -Amachine(m88k)"

/* If -m88000 is in effect, add -Dmc88000; similarly for -m88100 and -m88110.
   However, reproduce the effect of -Dmc88100 previously in CPP_PREDEFINES.
   Here, the CPU_DEFAULT is assumed to be -m88100.  */
#undef	CPP_SPEC
#define	CPP_SPEC "%{m88000:-D__mc88000__} \
		  %{!m88000:%{m88100:%{m88110:-D__mc88000__}}} \
		  %{!m88000:%{!m88100:%{m88110:-D__mc88110__}}} \
		  %{!m88000:%{!m88110:%{!ansi:%{traditional:-Dmc88100}} \
		  -D__mc88100__ -D__mc88100}} %{posix:-D_POSIX_SOURCE}"

/* For the 88k, a float function returns a double in traditional
   mode (and a float in ansi mode).  */
#undef TRADITIONAL_RETURN_FLOAT

/* Make gcc agree with <machine/ansi.h> */
#undef SIZE_TYPE
#define SIZE_TYPE "unsigned int"

#undef PTRDIFF_TYPE
#define PTRDIFF_TYPE "int"

#undef WCHAR_TYPE
#define WCHAR_TYPE "int"

#undef WCHAR_UNSIGNED
#define WCHAR_UNSIGNED 0

#undef WCHAR_TYPE_SIZE
#define WCHAR_TYPE_SIZE 32

/* Every structure or union's size must be a multiple of 2 bytes.  */

#undef STRUCTURE_SIZE_BOUNDARY
#define STRUCTURE_SIZE_BOUNDARY 16 

/* This is BSD, so it wants DBX format.  */

/* This is the char to use for continuation (in case we need to turn
   continuation back on).  */

#define DBX_CONTIN_CHAR '?'

/* Don't use the `xsfoo;' construct in DBX output; this system
   doesn't support it.  */

#define DBX_NO_XREFS

/* Don't default to pcc-struct-return, because gcc is the only compiler, and
   we want to retain compatibility with older gcc versions.  */
#define DEFAULT_PCC_STRUCT_RETURN 0

#undef SET_ASM_OP
#define SET_ASM_OP	".def"   

