/* GCC definition OpenBSD Mips ABI32 */

#include <openbsd.h>

/* Mips targets uses it's own for this */
#undef ASM_SPEC
#undef ASM_DECLARE_OBJECT_NAME
#undef ASM_DECLARE_FUNCTION_NAME

/* Undef SET_ASM_OP because it means something else in mips gas */
#undef SET_ASM_OP

/* We settle for little endian for now */
#define TARGET_ENDIAN_DEFAULT 0

/* Target uses ELF object format */
#define OBJECT_FORMAT_ELF

/* Provide a LINK_SPEC appropriate for OpenBSD.  Here we provide support
   for the special GCC options -static, -assert, and -nostdlib.  */
/* We also need to control dynamic stuff like dynamic loader etc */

#undef LINK_SPEC
#define LINK_SPEC \
  "%{G*} %{EB} %{EL} %{mips1} %{mips2} %{mips3} \
   %{bestGnum} %{shared} %{non_shared} \
   %{call_shared} %{no_archive} %{exact_version} \
   %{!shared: %{!non_shared: %{!call_shared: -non_shared}}} \
   %{!dynamic-linker:-dynamic-linker /usr/libexec/ld.so} \
   %{!nostdlib:%{!r*:%{!e*:-e __start}}} -dc -dp \
   %{static:-Bstatic} %{!static:-Bdynamic} %{assert*}"


/* Define mips-specific OpenBSD predefines... */
#ifndef CPP_PREDEFINES
#define CPP_PREDEFINES "-DMIPSEL -D_MIPSEL -DSYSTYPE_BSD \
-D__NO_LEADING_UNDERSCORES__ -D__GP_SUPPORT__ \
-Dunix  -D__OpenBSD__ -Dmips \
-Asystem(unix) -Asystem(OpenBSD) -Amachine(mips)"
#endif

/* GAS needs to know this */
#define SUBTARGET_ASM_SPEC "%{fPIC:-KPIC}"

/* ABI style and other controls. Our target uses GAS */
#define TARGET_DEFAULT MASK_GAS|MASK_ABICALLS

/* Some comment say that this is needed for ELF */
#define PREFERRED_DEBUGGING_TYPE DBX_DEBUG

/* Some comment say that we need to redefine this for ELF */
#define LOCAL_LABEL_PREFIX	"."

/* -G is incompatible with -KPIC which is the default, so only allow objects
   in the small data section if the user explicitly asks for it.  */
#undef MIPS_DEFAULT_GVALUE
#define MIPS_DEFAULT_GVALUE 0

#include "mips/mips.h"

/* Since gas and gld are standard on OpenBSD, we don't need these */

#undef ASM_FINAL_SPEC
#undef STARTFILE_SPEC

/*
 A C statement to output something to the assembler file to switch to section
 NAME for object DECL which is either a FUNCTION_DECL, a VAR_DECL or
 NULL_TREE.  Some target formats do not support arbitrary sections.  Do not
 define this macro in such cases. mips.h doesn't define this, do it here.
*/
#define ASM_OUTPUT_SECTION_NAME(F, DECL, NAME, RELOC)                        \
do {                                                                         \
  extern FILE *asm_out_text_file;                                            \
  if ((DECL) && TREE_CODE (DECL) == FUNCTION_DECL)                           \
    fprintf (asm_out_text_file, "\t.section %s,\"ax\",@progbits\n", (NAME)); \
  else if ((DECL) && DECL_READONLY_SECTION (DECL, RELOC))                    \
    fprintf (F, "\t.section %s,\"a\",@progbits\n", (NAME));                  \
  else                                                                       \
    fprintf (F, "\t.section %s,\"aw\",@progbits\n", (NAME));                 \
} while (0)
