/* Configuration file for an rs6000 OpenBSD target.
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

#include <rs6000/sysv4.h>

#define OBSD_HAS_CORRECT_SPECS
#define OBSD_HAS_DECLARE_FUNCTION_NAME
#define OBSD_HAS_DECLARE_FUNCTION_SIZE
#define OBSD_HAS_DECLARE_OBJECT

#include <openbsd.h>
/* XXX need to check ASM_WEAKEN_LABEL/ASM_GLOBALIZE_LABEL. */

/* Run-time target specifications. */
#define CPP_PREDEFINES \
  "-D__ELF__ -D__PPC -D__PPC__ -D__powerpc -D__powerpc__ -Acpu(powerpc) -Amachine(powerpc)"

#undef	CPP_OS_DEFAULT_SPEC
#define CPP_OS_DEFAULT_SPEC "%(cpp_os_openbsd)"

#undef LINKER_NAME 
#define LINKER_NAME "ld"

#undef LINK_SPEC
#define LINK_SPEC "%{shared:-shared} \
  %{!shared: \
    %{!static: \
      %{rdynamic:-export-dynamic} \
      %{!dynamic-linker:-dynamic-linker /usr/libexec/ld.so}} \
    %{static:-static}}"

#undef	LIB_DEFAULT_SPEC
#define LIB_DEFAULT_SPEC "%(lib_openbsd)"

#undef	STARTFILE_DEFAULT_SPEC
#define STARTFILE_DEFAULT_SPEC "%(startfile_openbsd)"

#undef	ENDFILE_DEFAULT_SPEC
#define ENDFILE_DEFAULT_SPEC "%(endfile_openbsd)"

#undef	LINK_START_DEFAULT_SPEC
#define LINK_START_DEFAULT_SPEC "%(link_start_openbsd)"

#undef	LINK_OS_DEFAULT_SPEC
#define LINK_OS_DEFAULT_SPEC "%(link_os_openbsd)"

#undef TARGET_VERSION
#define TARGET_VERSION fprintf (stderr, " (PowerPC OpenBSD)");

/* Default ABI to use */
#undef RS6000_ABI_NAME 
#define RS6000_ABI_NAME "openbsd"

/* Define this macro as a C expression for the initializer of an
   array of string to tell the driver program which options are
   defaults for this target and thus do not need to be handled
   specially when using `MULTILIB_OPTIONS'.

   Do not define this macro if `MULTILIB_OPTIONS' is not defined in
   the target makefile fragment or if none of the options listed in
   `MULTILIB_OPTIONS' are set by default.  *Note Target Fragment::.  */

#undef	MULTILIB_DEFAULTS
#define	MULTILIB_DEFAULTS { "mbig", "mcall-openbsd" }

/* collect2 support (Macros for initialization). */


/* Don't tell collect2 we use COFF as we don't have (yet ?) a dynamic ld
   library with the proper functions to handle this -> collect2 will
   default to using nm. */
#undef OBJECT_FORMAT_COFF


