/* Definitions of target machine Motorola Delta 68k using GAS
   for GNU Compiler.
   Copyright (C) 1994 Free Software Foundation, Inc.

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


/* Just in case someone asks */
#define USE_GAS

#include "m68k/m68k.h"

/* See m68k.h.  7 means 68020 with 68881,
   7 & 01400 means optimize for 68040 but allow execution on 68020. */
#undef TARGET_DEFAULT
#define	TARGET_DEFAULT (7 & 01400)

/* NYI: FP= is equivalent to -msoft-float
   We use /lib/libp/lib* when profiling.
   
   NYI: if FP=M68881U library is -lc881u
   NYI: if FP= library is -lc.
   Default for us: FP=M68881 library is -lc881  */
#undef LIB_SPEC
#define LIB_SPEC "%{!shlib:%{p:-L/usr/lib/libp} %{pg:-L/usr/lib/libp} -lc881}"

#undef CPP_SPEC
#define CPP_SPEC "%{!msoft-float:-D__HAVE_68881__}"

/* Shared libraries need to use crt0s.o  */
#undef STARTFILE_SPEC
#define STARTFILE_SPEC \
  "%{!shlib:%{pg:mcrt0.o%s}%{!pg:%{p:mcrt0.o%s}%{!p:crt0.o%s}}}\
   %{shlib:crt0s.o%s shlib.ifile%s} "

  /* -m68000 requires special flags to the assembler.  */
#define ASM_SPEC \
 "%{m68000:-mc68000}%{mc68000:-mc68000}%{!mc68000:%{!m68000:-mc68020}}"

/* Generate calls to memcpy, memcmp and memset.  */
#define TARGET_MEM_FUNCTIONS

/* size_t is unsigned int.  */
#define SIZE_TYPE "unsigned int"

/* Every structure or union's size must be a multiple of 2 bytes.  */
#define STRUCTURE_SIZE_BOUNDARY 16

/* man cpp on the Delta says pcc predefines "m68k", "unix", and "sysV68",
   and experimentation validates this.   -jla */
#define CPP_PREDEFINES "-Dm68k -Dunix -DsysV68"

/* cpp has to support a #sccs directive for the /usr/include files */
#define SCCS_DIRECTIVE

/* Make sure to use MIT syntax, not Motorola */
#undef MOTOROLA

/* Use SDB style because gdb on the delta doesn't understand stabs. */
#define SDB_DEBUGGING_INFO

/* Use a register prefix to avoid clashes with external symbols (classic
   example: `extern char PC;' in termcap).  */
#undef REGISTER_PREFIX
#define REGISTER_PREFIX "%"

/* In the machine description we can't use %R, because it will not be seen
   by ASM_FPRINTF.  (Isn't that a design bug?).  */
#undef REGISTER_PREFIX_MD
#define REGISTER_PREFIX_MD "%%"

/* The file command should always begin the output.  */
#undef ASM_FILE_START
#define ASM_FILE_START(FILE) \
    { \
       fprintf (FILE, "%s", ASM_APP_OFF); \
       output_file_directive ((FILE), main_input_filename); \
    }

/* Undefining these will allow `output_file_directive' (in toplev.c)
   to default to the right thing. */
#undef ASM_OUTPUT_SOURCE_FILENAME
#undef ASM_OUTPUT_MAIN_SOURCE_FILENAME

#undef REGISTER_NAMES
#define REGISTER_NAMES \
{"%d0", "%d1", "%d2", "%d3", "%d4", "%d5", "%d6", "%d7", \
 "%a0", "%a1", "%a2", "%a3", "%a4", "%a5", "%a6", "%sp", \
 "%fp0", "%fp1", "%fp2", "%fp3", "%fp4", "%fp5", "%fp6", "%fp7" }

/* Define how to jump to variable address from dispatch table of
   relative addresses, for m68k.md insn.  Note the use of 2 '%'
   chars to output one. */
#define ASM_RETURN_CASE_JUMP return "jmp %%pc@(2,%0:w)"
