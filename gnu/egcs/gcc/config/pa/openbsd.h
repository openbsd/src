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

/* Run-time target specifications. */
#define CPP_PREDEFINES "-D__unix__ -D__ANSI_COMPAT -Asystem(unix) -Asystem(OpenBSD) -Amachine(hppa) -D__OpenBSD__ -D__hppa__ -D__hppa"

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

#undef STRING_ASM_OP
#define STRING_ASM_OP   ".stringz"

#undef DBX_OUTPUT_MAIN_SOURCE_FILE_END
#undef ASM_OUTPUT_SECTION_NAME

#undef TEXT_SECTION_ASM_OP
#define TEXT_SECTION_ASM_OP "\t.text"
#undef READONLY_DATA_ASM_OP
#define READONLY_DATA_ASM_OP "\t.text"
#undef DATA_SECTION_ASM_OP
#define DATA_SECTION_ASM_OP "\t.data"
#undef BSS_SECTION_ASM_OP
#define BSS_SECTION_ASM_OP "\t.section\t.bss"

/* Remove hpux specific pa defines. */
#undef LDD_SUFFIX
#undef PARSE_LDD_OUTPUT
