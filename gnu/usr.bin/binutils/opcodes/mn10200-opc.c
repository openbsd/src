/* Assemble Matsushita MN10200 instructions.
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

#include "ansidecl.h"
#include "opcode/mn10200.h"


const struct mn10200_operand mn10200_operands[] = {
#define UNUSED	0
  { 0, 0, 0 }, 
} ; 


/* The opcode table.

   The format of the opcode table is:

   NAME		OPCODE		MASK		{ OPERANDS }

   NAME is the name of the instruction.
   OPCODE is the instruction opcode.
   MASK is the opcode mask; this is used to tell the disassembler
     which bits in the actual opcode must match OPCODE.
   OPERANDS is the list of operands.

   The disassembler reads the table in order and prints the first
   instruction which matches, so this table is sorted to put more
   specific instructions before more general instructions.  It is also
   sorted by major opcode.  */

const struct mn10200_opcode mn10200_opcodes[] = {
{ 0, 0, 0, {0}, } } ;

const int mn10200_num_opcodes =
  sizeof (mn10200_opcodes) / sizeof (mn10200_opcodes[0]);


