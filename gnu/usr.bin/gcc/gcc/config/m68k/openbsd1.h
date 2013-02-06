/* Configuration file for an m68k OpenBSD target.
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

/* m68k is an old configuration that does not yet use the TARGET_CPU_DEFAULT
   framework. OpenBSD uses -m68020-60 by default.  */
#define TARGET_DEFAULT \
	(MASK_BITFIELD | MASK_68881 | MASK_68020 | MASK_68040 | MASK_68060)

#define MOTOROLA		/* Use Motorola syntax */
#define USE_GAS			/* But GAS wants jbsr instead of jsr */

#define OBSD_HAS_DECLARE_FUNCTION_NAME
#define OBSD_HAS_DECLARE_FUNCTION_SIZE
#define OBSD_HAS_DECLARE_OBJECT
