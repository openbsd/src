/* Target machine description for Motorola Delta 88 box, for GDB.
   Copyright 1986, 1987, 1988, 1989, 1990, 1991 Free Software Foundation, Inc.

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

#include "m88k/tm-m88k.h"

#define DELTA88

#define IN_SIGTRAMP(pc, name) ((name) && STREQ ("_sigcode", (name)))
#define SIGTRAMP_FRAME_FIXUP(frame) (frame) += 0x20
#define SIGTRAMP_SP_FIXUP(sp) (sp) = read_memory_integer((sp), 4)
