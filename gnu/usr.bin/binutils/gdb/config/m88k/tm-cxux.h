/* Target definitions for m88k running Harris CX/UX.
   Copyright 1993, 1994 Free Software Foundation, Inc.

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

#define HARRIS_TARGET 1

#define CXUX_TARGET 1

/* Type of X registers, as supplied by the OS */

typedef struct {
   long w1, w2, w3, w4;
} X_REGISTER_RAW_TYPE;

#define X_REGISTER_VIRTUAL_TYPE double

#include "m88k/tm-m88k.h"

#define ADD_SHARED_SYMBOL_FILES(args,have_tty)  add_shared_symbol_files ()

#define CONVERT_REGISTER_ADDRESS

/* Always allocate space for both, but recognize that the m88100 has no
   FP_REGS.  */

#undef ARCH_NUM_REGS
#define ARCH_NUM_REGS (target_is_m88110 ? (GP_REGS + FP_REGS) : (GP_REGS))

/* Don't need this grotesquerie.  */

#undef SHIFT_INST_REGS

/* Extended registers are treated as 16 bytes by Harris' OS's. 
   We treat them as 16 bytes here for consistency's sake.  */

#undef REGISTER_RAW_SIZE
#define REGISTER_RAW_SIZE(N) ((N) < XFP_REGNUM ? 4 : 16)

#undef REGISTER_BYTE
#define REGISTER_BYTE(N) \
  ((N) >= XFP_REGNUM \
   ? (((GP_REGS) * REGISTER_RAW_SIZE(0)) + \
      (((N) - XFP_REGNUM) * REGISTER_RAW_SIZE(XFP_REGNUM))) \
   : ((N) * REGISTER_RAW_SIZE(0)))
