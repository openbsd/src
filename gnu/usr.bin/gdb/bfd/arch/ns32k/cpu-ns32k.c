/* BFD support for the ns32k architecture.
   Copyright 1992 Free Software Foundation, Inc.

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
Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

	$Id: cpu-ns32k.c,v 1.1.1.1 1995/10/18 08:39:55 deraadt Exp $
*/

#include "bfd.h"
#include "sysdep.h"
#include "libbfd.h"

static bfd_arch_info_type arch_info_struct = 
  {
    32,	/* 32 bits in a word */
    32,	/* 32 bits in an address */
    8,	/* 8 bits in a byte */
    bfd_arch_ns32k,
    0,	/* only 1 machine */
    "ns32k",
    "ns32k",
    2,
    true, /* the one and only */
    bfd_default_compatible, 
    bfd_default_scan ,
    0,
  };

void DEFUN_VOID(bfd_ns32k_arch)
{
  bfd_arch_linkin(&arch_info_struct);
}
