/* IBM RS/6000 native-dependent macros for GDB, the GNU debugger.
   Copyright 1986, 1987, 1989, 1991, 1992, 1994 Free Software Foundation, Inc.

This file is part of GDB.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

/* Do implement the attach and detach commands.  */

#define ATTACH_DETACH

#define PTRACE_ATTACH PT_ATTACH
#define PTRACE_DETACH PT_DETACH

/* Override copies of {fetch,store}_inferior_registers in infptrace.c.  */

#define FETCH_INFERIOR_REGISTERS

/* When a child process is just starting, we sneak in and relocate
   the symbol table (and other stuff) after the dynamic linker has
   figured out where they go.  */

#define	SOLIB_CREATE_INFERIOR_HOOK(PID)	\
  do {					\
    xcoff_relocate_symtab (PID);	\
  } while (0)

/* When a target process or core-file has been attached, we sneak in
   and figure out where the shared libraries have got to.  */

#define	SOLIB_ADD(a, b, c)	\
  if (inferior_pid)	\
    /* Attach to process.  */  \
    xcoff_relocate_symtab (inferior_pid); \
  else		\
    /* Core file.  */ \
    xcoff_relocate_core (c);

extern void xcoff_relocate_symtab PARAMS ((unsigned int));
#ifdef __STDC__
struct target_ops;
#endif
extern void xcoff_relocate_core PARAMS ((struct target_ops *));

/* Return sizeof user struct to callers in less machine dependent routines */

#define KERNEL_U_SIZE kernel_u_size()
extern int kernel_u_size PARAMS ((void));
