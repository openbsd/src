/* This file is part of GDB, the GNU debugger.

   Copyright 2004 Free Software Foundation, Inc.

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
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

   Please email any bugs, comments, and/or additions to this file to:
   bug-gdb@prep.ai.mit.edu  */

#include <stdio.h>

/* Include a .c file.  This is usually considered bad practice in C,
   but this emulate a practice which is common in other languages.
   One such language is Ada and its concept of "separates", for instance.  */
#include "sep-proc.c"

int
main (void)
{
  say_hello ();
  return 0;
}

