/* Configuration for  a Mips ABI32 OpenBSD target.
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

/* Definitions needed for OpenBSD, to avoid picking mips 'defaults'.  */

/* GAS must know this.  */
#define SUBTARGET_ASM_SPEC "%{fPIC:-KPIC} %|"

/* CPP specific OpenBSD specs.  */
#define SUBTARGET_CPP_SPEC OBSD_CPP_SPEC

/* Needed for ELF (inspired by netbsd-elf).  */
#undef PREFERRED_DEBUGGING_TYPE
#define PREFERRED_DEBUGGING_TYPE DBX_DEBUG
#define LOCAL_LABEL_PREFIX	"."

#define ASM_FINAL_SPEC ""
#include <mips/mips.h>

#undef MDEBUG_ASM_SPEC
#define MDEBUG_ASM_SPEC ""

#include <mips/elf.h>
#undef STARTFILE_SPEC
#define STARTFILE_SPEC "\
	%{!shared: %{pg:gcrt0%O%s} %{!pg:%{p:gcrt0%O%s} %{!p:crt0%O%s}} \
	crtbegin%O%s} %{shared:crtbeginS%O%s}"
#undef ENDFILE_SPEC
#define ENDFILE_SPEC "%{!shared:crtend%O%s} %{shared:crtendS%O%s}"
#undef LIB_SPEC
#define LIB_SPEC OBSD_LIB_SPEC

/* Get generic OpenBSD definitions.  */
#define OBSD_HAS_DECLARE_FUNCTION_NAME
#define OBSD_HAS_DECLARE_OBJECT
#define OBSD_HAS_CORRECT_SPECS
#include <openbsd.h>

/* mips assembler uses .set for arcane purposes.  __attribute__((alias))
   and friends won't work until we get recent binutils with .weakext
	support.  */
#undef SET_ASM_OP

#undef TARGET_OS_CPP_BUILTINS
#define TARGET_OS_CPP_BUILTINS()			\
    do {						\
	OPENBSD_OS_CPP_BUILTINS_ELF();			\
	builtin_define ("__NO_LEADING_UNDERSCORES__");	\
	builtin_define ("__GP_SUPPORT__");		\
	builtin_assert ("machine=mips");		\
	if (TARGET_LONG64)				\
		builtin_define ("__LONG64");		\
	if (TARGET_64BIT)				\
		OPENBSD_OS_CPP_BUILTINS_LP64();		\
	if (TARGET_ABICALLS)				\
		builtin_define ("__ABICALLS__");	\
	if (mips_abi == ABI_EABI)			\
		builtin_define ("__mips_eabi");		\
	else if (mips_abi == ABI_N32)			\
		builtin_define ("__mips_n32");		\
	else if (mips_abi == ABI_64)			\
		builtin_define ("__mips_n64");		\
	else if (mips_abi == ABI_O64)			\
		builtin_define ("__mips_o64");		\
	/* Needed to make libgcc to build properly */	\
	if (mips_abi == ABI_N32)			\
	{						\
		builtin_define ("_ABIN32=2");		\
		builtin_define ("_MIPS_SIM=_ABIN32");	\
		builtin_define ("_MIPS_SZLONG=32");	\
		builtin_define ("_MIPS_SZPTR=32");	\
	}						\
	else if (mips_abi == ABI_64)			\
	{						\
		builtin_define ("_ABI64=3");		\
		builtin_define ("_MIPS_SIM=_ABI64");	\
		builtin_define ("_MIPS_SZLONG=64");	\
		builtin_define ("_MIPS_SZPTR=64");	\
	}						\
	else						\
	{						\
		builtin_define ("_MIPS_SIM=_MIPS_SIM_ABI32");	\
		builtin_define ("_MIPS_SZLONG=32");	\
		builtin_define ("_MIPS_SZPTR=32");	\
	}						\
} while (0)


/* Layout of source language data types.  */

#undef WCHAR_TYPE
#define WCHAR_TYPE "int"

#undef WCHAR_TYPE_SIZE
#define WCHAR_TYPE_SIZE 32

/* Controlling the compilation driver.  */

/* LINK_SPEC appropriate for OpenBSD:  support for GCC options
   -static, -assert, and -nostdlib. Dynamic loader control.  */
#undef LINK_SPEC
#define LINK_SPEC \
  "%(endian_spec) \
   %{G*} %{mips1} %{mips2} %{mips3} %{mips4} %{mips32} %{mips64} \
   %{bestGnum} %{shared} %{non_shared} \
   %{call_shared} %{no_archive} %{exact_version} \
   %{!shared: %{!non_shared: %{!call_shared: -non_shared}}} \
   %{!dynamic-linker:-dynamic-linker /usr/libexec/ld.so} \
   %{!nostdlib:%{!r*:%{!e*:-e __start}}} -dc -dp \
   %{static:-Bstatic} %{!static:-Bdynamic} %{assert*}"

/* -G is incompatible with -KPIC which is the default, so only allow objects
   in the small data section if the user explicitly asks for it.  */
#undef MIPS_DEFAULT_GVALUE
#define MIPS_DEFAULT_GVALUE 0


