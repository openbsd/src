/* MIPS PPC support for BFD.
   Copyright (C) 1995 Free Software Foundation, Inc.

   By Michael Meissner, Cygnus Support, <meissner@cygnus.com>, from information
   in the System V Application Binary Interface, PowerPC Processor Supplement
   and the PowerPC Embedded Application Binary Interface (eabi).

This file is part of BFD, the Binary File Descriptor library.

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

/* This file holds definitions specific to the PPC ELF ABI.  Note
   that most of this is not actually implemented by BFD.  */

#ifndef _ELF_PPC_H
#define _ELF_PPC_H

/* Processor specific flags for the ELF header e_flags field.  */

#define	EF_PPC_EMB		0x80000000	/* PowerPC embedded flag  */

						/* CYGNUS local bits below */
#define	EF_PPC_RELOCATABLE	0x00010000	/* PowerPC -mrelocatable flag */
#define	EF_PPC_RELOCATABLE_LIB	0x00008000	/* PowerPC -mrelocatable-lib flag */

#endif /* _ELF_PPC_H */
