/* Configuration file for an m88k OpenBSD target.
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

/* a.out with DBX. */
#define DBX_DEBUGGING_INFO
#define DEFAULT_GDB_EXTENSIONS 0

#include <aoutos.h>
#include <m88k/m88k.h>

/* <m88k/m88k.h> provided a wrong SET_ASM_OP */
#undef	SET_ASM_OP
#define	SET_ASM_OP	"equ"

#define	OBSD_HAS_CORRECT_ASM_OPS
#define	OBSD_OLD_GAS
#define	OBSD_NO_DYNAMIC_LIBRARIES

#include <openbsd.h>

/* Macros to be automatically defined.  */
#define CPP_PREDEFINES \
    "-D__m88k__ -D__unix__ -D__OpenBSD__ -Asystem(unix) -Asystem(OpenBSD) -Acpu(m88k) -Amachine(m88k)"

/* If -m88000 is in effect, add -Dmc88000; similarly for -m88100 and -m88110.
   However, reproduce the effect of -Dmc88100 previously in CPP_PREDEFINES.
   Here, the CPU_DEFAULT is assumed to be -m88100.  */
#undef	CPP_SPEC
#define	CPP_SPEC "%{m88000:-D__mc88000__} \
		  %{!m88000:%{m88100:%{m88110:-D__mc88000__}}} \
		  %{!m88000:%{!m88100:%{m88110:-D__mc88110__}}} \
		  %{!m88000:%{!m88110:%{!ansi:%{traditional:-Dmc88100}} \
		  -D__mc88100__ -D__mc88100}} %{posix:-D_POSIX_SOURCE} \
		  %{pthread:-D_POSIX_THREADS}"

/* For the 88k, a float function returns a double in traditional
   mode (and a float in ansi mode).  */
#undef TRADITIONAL_RETURN_FLOAT

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

/* Every structure or union's size must be a multiple of 2 bytes.  */
#undef STRUCTURE_SIZE_BOUNDARY
#define STRUCTURE_SIZE_BOUNDARY 16 

/* problems occur if we're too liberal in preserve_subexpressions_p */
#define	BROKEN_PRESERVE_SUBEXPRESSIONS_P
