/* RTL buffer overflow protection function for GNU C compiler
   Copyright (C) 1987, 88, 89, 92-7, 1998 Free Software Foundation, Inc.

This file is part of GCC.

GCC is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation; either version 2, or (at your option) any later
version.

GCC is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License
along with GCC; see the file COPYING.  If not, write to the Free
Software Foundation, 59 Temple Place - Suite 330, Boston, MA
02111-1307, USA.  */


/* declaration of GUARD variable */
#define GUARD_m		Pmode
#define UNITS_PER_GUARD MAX(BIGGEST_ALIGNMENT / BITS_PER_UNIT, GET_MODE_SIZE (GUARD_m))

#ifndef L_stack_smash_handler

/* insert a guard variable before a character buffer and change the order
 of pointer variables, character buffers and pointer arguments */

extern void prepare_stack_protection  PARAMS ((int inlinable));

#ifdef TREE_CODE
/* search a character array from the specified type tree */

extern int search_string_def PARAMS ((tree names));
#endif

/* examine whether the input contains frame pointer addressing */

extern int contains_fp PARAMS ((rtx op));

/* Return size that is not allocated for stack frame. It will be allocated
   to modify the home of pseudo registers called from global_alloc.  */

extern HOST_WIDE_INT get_frame_free_size PARAMS ((void));

/* allocate a local variable in the stack area before character buffers
   to avoid the corruption of it */

extern rtx assign_stack_local_for_pseudo_reg PARAMS ((enum machine_mode, HOST_WIDE_INT, int));

#endif
