/* BFD i370 CPU definition
   Copyright (C) 1994, 95, 96, 98, 99, 2000 Free Software Foundation, Inc.
   Contributed by Ian Lance Taylor, Cygnus Support.
   Hacked by Linas Vepstas <linas@linas.org> in 1998, 1999

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

#include "bfd.h"
#include "sysdep.h"
#include "libbfd.h"

/* The common i360/370 architecture comes in many forms  */

static const bfd_arch_info_type *i370_compatible
  PARAMS ((const bfd_arch_info_type *, const bfd_arch_info_type *));

static const bfd_arch_info_type *
i370_compatible (a, b)
     const bfd_arch_info_type *a;
     const bfd_arch_info_type *b;
{
  BFD_ASSERT (a->arch == bfd_arch_i370);
  switch (b->arch)
    {
    default:
      return NULL;
    case bfd_arch_i370:
      return bfd_default_compatible (a, b);
    }
  /*NOTREACHED*/
}

static const bfd_arch_info_type arch_info_struct[] =
{
  /* hack alert: old old machines are really 16 and 24 bit arch ... */
  {
    32, /* 32 bits in a word */
    32, /* 32 bits in an address */
    8,  /* 8 bits in a byte */
    bfd_arch_i370,
    360, /* for the 360 */
    "i370",
    "i370:360",
    3,
    false, /* not the default */
    i370_compatible,
    bfd_default_scan,
    &arch_info_struct[1]
  },
  {
    32, /* 32 bits in a word */
    32, /* 32 bits in an address */
    8,  /* 8 bits in a byte */
    bfd_arch_i370,
    370, /* for the 370 */
    "i370",
    "i370:370",
    3,
    false, /* not the default */
    i370_compatible,
    bfd_default_scan,
    0
  },
};

const bfd_arch_info_type bfd_i370_arch =
  {
    32, /* 32 bits in a word */
    32, /* 32 bits in an address */
    8,  /* 8 bits in a byte */
    bfd_arch_i370,
    0,  /* for the 360/370 common architecture */
    "i370",
    "i370:common",
    3,
    true, /* the default */
    i370_compatible,
    bfd_default_scan,
    &arch_info_struct[0]
  };
