/* Disassemble MN10200 instructions.
   Copyright (C) 1996 Free Software Foundation, Inc.

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


#include <stdio.h>

#include "ansidecl.h"
#include "opcode/mn10200.h" 
#include "dis-asm.h"

static void disassemble PARAMS ((bfd_vma memaddr,
				 struct disassemble_info *info,
				 unsigned long insn));

int 
print_insn_mn10200 (memaddr, info)
     bfd_vma memaddr;
     struct disassemble_info *info;
{
}
