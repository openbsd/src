/* BFD library support routines for the Hitachi H8/300 architecture.
   Copyright (C) 1990, 91, 92, 93, 94, 1995 Free Software Foundation, Inc.
   Hacked by Steve Chamberlain of Cygnus Support.

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

int bfd_default_scan_num_mach ();

static boolean
h8300_scan (info, string)
     const struct bfd_arch_info *info;
     const char *string;
{
  if (*string != 'h' && *string != 'H')
    return false;

  string++;
  if (*string != '8')
    return false;

  string++;
  if (*string == '/')
    string++;

  if (*string != '3')
    return false;
  string++;
  if (*string != '0')
    return false;
  string++;
  if (*string != '0')
    return false;
  string++;
  if (*string == '-')
    string++;
  if (*string == 'h' || *string == 'H')
    {
      return (info->mach == bfd_mach_h8300h);
    }
  else if (*string == 's' || *string == 'S')
    {
      return (info->mach == bfd_mach_h8300s);
    }
  else
    {
      return info->mach == bfd_mach_h8300;
    }
}


/* This routine is provided two arch_infos and works out the 
   machine which would be compatible with both and returns a pointer
   to its info structure */

static const bfd_arch_info_type *
compatible (in, out)
     const bfd_arch_info_type * in;
     const bfd_arch_info_type * out;
{
  /* It's really not a good idea to mix and match modes.  */
  if (in->mach != out->mach)
    return 0;
  else
    return in;
}

static const bfd_arch_info_type h8300_info_struct =
{
  16,				/* 16 bits in a word */
  16,				/* 16 bits in an address */
  8,				/* 8 bits in a byte */
  bfd_arch_h8300,
  bfd_mach_h8300,
  "h8300",			/* arch_name  */
  "h8300",			/* printable name */
  1,
  true,				/* the default machine */
  compatible,
  h8300_scan,
/*    local_bfd_reloc_type_lookup, */
  0,
};

static const bfd_arch_info_type h8300h_info_struct =
{
  32,				/* 32 bits in a word */
  32,				/* 32 bits in an address */
  8,				/* 8 bits in a byte */
  bfd_arch_h8300,
  bfd_mach_h8300h,
  "h8300h",			/* arch_name  */
  "h8300h",			/* printable name */
  1,
  false,			/* the default machine */
  compatible,
  h8300_scan,
/*    local_bfd_reloc_type_lookup, */
  &h8300_info_struct,
};

const bfd_arch_info_type bfd_h8300_arch =
{
  32,				/* 32 bits in a word */
  32,				/* 32 bits in an address */
  8,				/* 8 bits in a byte */
  bfd_arch_h8300,
  bfd_mach_h8300s,
  "h8300s",			/* arch_name  */
  "h8300s",			/* printable name */
  1,
  false,			/* the default machine */
  compatible,
  h8300_scan,
/*    local_bfd_reloc_type_lookup, */
  &h8300h_info_struct,
};
