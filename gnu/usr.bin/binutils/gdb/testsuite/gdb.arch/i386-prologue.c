/* Unwinder test program.

   Copyright 2003, 2004 Free Software Foundation, Inc.

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

#ifdef SYMBOL_PREFIX
#define SYMBOL(str)	SYMBOL_PREFIX #str
#else
#define SYMBOL(str)	#str
#endif

void gdb1253 (void);
void gdb1718 (void);
void gdb1338 (void);
void jump_at_beginning (void);

int
main (void)
{
  standard ();
  gdb1253 ();
  gdb1718 ();
  gdb1338 ();
  jump_at_beginning ();
  return 0;
}

/* A normal prologue.  */

asm(".text\n"
    "    .align 8\n"
    SYMBOL (standard) ":\n"
    "    pushl %ebp\n"
    "    movl  %esp, %ebp\n"
    "    pushl %edi\n"
    "    int   $0x03\n"
    "    leave\n"
    "    ret\n");

/* Relevant part of the prologue from symtab/1253.  */

asm(".text\n"
    "    .align 8\n"
    SYMBOL (gdb1253) ":\n"
    "    pushl %ebp\n"
    "    xorl  %ecx, %ecx\n"
    "    movl  %esp, %ebp\n"
    "    pushl %edi\n"
    "    int   $0x03\n"
    "    leave\n"
    "    ret\n");

/* Relevant part of the prologue from backtrace/1718.  */

asm(".text\n"
    "    .align 8\n"
    SYMBOL (gdb1718) ":\n"
    "    pushl %ebp\n"
    "    movl  $0x11111111, %eax\n"
    "    movl  %esp, %ebp\n"
    "    pushl %esi\n"
    "    movl  $0x22222222, %esi\n"
    "    pushl %ebx\n"
    "    int   $0x03\n"
    "    leave\n"
    "    ret\n");

/* Relevant part of the prologue from backtrace/1338.  */

asm(".text\n"
    "    .align 8\n"
    SYMBOL (gdb1338) ":\n"
    "    pushl %edi\n"
    "    pushl %esi\n"
    "    pushl %ebx\n"
    "    int   $0x03\n"
    "    popl  %ebx\n"
    "    popl  %esi\n"
    "    popl  %edi\n"
    "    ret\n");

/* The purpose of this function is to verify that, during prologue
   skip, GDB does not follow a jump at the beginnning of the "real"
   code.  */

asm(".text\n"
    "    .align 8\n"
    SYMBOL (jump_at_beginning) ":\n"
    "    pushl %ebp\n"
    "    movl  %esp,%ebp\n"
    "    jmp   .gdbjump\n"
    "    nop\n"
    ".gdbjump:\n"
    "    movl  %ebp,%esp\n"
    "    popl  %ebp\n"
    "    ret\n");
