/* Code to go along with tests in rtti.exp.
   
   Copyright 2003, 2004 Free Software Foundation, Inc.

   Contributed by David Carlton <carlton@bactrian.org> and by Kealia,
   Inc.

   This file is part of GDB.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or (at
   your option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

#include "rtti.h"

namespace n2 {
  
  D2::D2(C2 *expr_1, C2 *expr_2)
    : expr_1_(expr_1), expr_2_(expr_2) { }

  C2 *create2() {
    return new D2(0, 0);
  }

  n3::C3 *create3() {
    return new n3::C3();
  }

}
