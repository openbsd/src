/* GNU/Linux/m68k specific low level interface, for the remote server for GDB.
   Copyright 1995, 1996, 1998, 1999, 2000, 2001, 2002, 2003
   Free Software Foundation, Inc.

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
   Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

#include "server.h"
#include "linux-low.h"

#ifdef HAVE_SYS_REG_H
#include <sys/reg.h>
#endif

#define m68k_num_regs 29

/* This table must line up with REGISTER_NAMES in tm-m68k.h */
static int m68k_regmap[] =
{
#ifdef PT_D0
  PT_D0 * 4, PT_D1 * 4, PT_D2 * 4, PT_D3 * 4,
  PT_D4 * 4, PT_D5 * 4, PT_D6 * 4, PT_D7 * 4,
  PT_A0 * 4, PT_A1 * 4, PT_A2 * 4, PT_A3 * 4,
  PT_A4 * 4, PT_A5 * 4, PT_A6 * 4, PT_USP * 4,
  PT_SR * 4, PT_PC * 4,
#else
  14 * 4, 0 * 4, 1 * 4, 2 * 4, 3 * 4, 4 * 4, 5 * 4, 6 * 4,
  7 * 4, 8 * 4, 9 * 4, 10 * 4, 11 * 4, 12 * 4, 13 * 4, 15 * 4,
  17 * 4, 18 * 4,
#endif
#ifdef PT_FP0
  PT_FP0 * 4, PT_FP1 * 4, PT_FP2 * 4, PT_FP3 * 4,
  PT_FP4 * 4, PT_FP5 * 4, PT_FP6 * 4, PT_FP7 * 4,
  PT_FPCR * 4, PT_FPSR * 4, PT_FPIAR * 4
#else
  21 * 4, 24 * 4, 27 * 4, 30 * 4, 33 * 4, 36 * 4,
  39 * 4, 42 * 4, 45 * 4, 46 * 4, 47 * 4
#endif
};

static int
m68k_cannot_store_register (int regno)
{
  return (regno >= m68k_num_regs);
}

static int
m68k_cannot_fetch_register (int regno)
{
  return (regno >= m68k_num_regs);
}

struct linux_target_ops the_low_target = {
  m68k_num_regs,
  m68k_regmap,
  m68k_cannot_fetch_register,
  m68k_cannot_store_register,
};
