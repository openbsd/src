/* Enable stack execute around trampoline address.  
   Copyright (C) 2002 Free Software Foundation, Inc.

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

#undef FINALIZE_TRAMPOLINE
#define FINALIZE_TRAMPOLINE(TRAMP) \
  emit_library_call(gen_rtx_SYMBOL_REF (Pmode, "__enable_execute_stack"), \
		    0, VOIDmode, 1, memory_address (SImode, (TRAMP)), Pmode)

#undef TRANSFER_FROM_TRAMPOLINE
#define TRANSFER_FROM_TRAMPOLINE					\
extern void __enable_execute_stack (void *);				\
void									\
__enable_execute_stack (addr)						\
     void *addr;							\
{									\
  long size = getpagesize ();						\
  long mask = ~(size-1);						\
  char *page = (char *) (((long) addr) & mask); 			\
  char *end  = (char *) ((((long) (addr + TRAMPOLINE_SIZE)) & mask) + size); \
								      \
  if (mprotect (page, end - page, PROT_READ | PROT_WRITE | PROT_EXEC) < 0) \
    perror ("mprotect of trampoline code");				\
}
