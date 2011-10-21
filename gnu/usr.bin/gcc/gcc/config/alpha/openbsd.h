/* Configuration file for an alpha OpenBSD target.
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

/* Controlling the compilation driver.  */

#undef TARGET_DEFAULT
#define TARGET_DEFAULT \
	(MASK_FP | MASK_FPREGS | MASK_IEEE | MASK_IEEE_CONFORMANT | MASK_GAS)

/* alpha needs __start.  */
#undef LINK_SPEC
#define LINK_SPEC \
  "%{!shared:%{!nostdlib:%{!r*:%{!e*:-e __start}}}} \
   %{shared:-shared} %{R*} \
   %{static:-Bstatic} \
   %{!static:-Bdynamic} \
   %{rdynamic:-export-dynamic} \
   %{assert*} \
   %{!dynamic-linker:-dynamic-linker /usr/libexec/ld.so}"

/* As an elf system, we need crtbegin/crtend stuff.  */
#undef STARTFILE_SPEC
#define STARTFILE_SPEC "\
	%{!shared: %{pg:gcrt0%O%s} %{!pg:%{p:gcrt0%O%s} %{!p:crt0%O%s}} \
	crtbegin%O%s} %{shared:crtbeginS%O%s}"
#undef ENDFILE_SPEC
#define ENDFILE_SPEC "%{!shared:crtend%O%s} %{shared:crtendS%O%s}"

/* run-time target specifications */
#define TARGET_OS_CPP_BUILTINS()		\
    do {					\
	OPENBSD_OS_CPP_BUILTINS_ELF();		\
	OPENBSD_OS_CPP_BUILTINS_LP64();		\
    } while (0)

/* Layout of source language data types.  */

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

/* Output and generation of labels.  */
#define LOCAL_LABEL_PREFIX	"."

/* .set on alpha is not used to output labels.  */
#undef SET_ASM_OP

/* don't want no friggin' stack checks.  */
#undef STACK_CHECK_BUILTIN
#define STACK_CHECK_BUILTIN 0

/* OpenBSD doesn't currently support thread-local storage. */
/* alpha.c undefs TARGET_HAVE_TLS and redefines it to HAVE_AS_TLS !?!?! */
#undef HAVE_AS_TLS
#define HAVE_AS_TLS false
