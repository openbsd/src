/* Definitions to make GDB run on an encore under umax 4.2
   Copyright 1987, 1989, 1991, 1993, 1994, 1998, 1999, 2000, 2001, 2002
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

#ifndef TM_NS32K_H
#define TM_NS32K_H

/* Need to get function ends by adding this to epilogue address from .bf
   record, not using x_fsize field.  */
#define FUNCTION_EPILOGUE_SIZE 4

/* Address of end of stack space.  */

#ifndef STACK_END_ADDR
#define STACK_END_ADDR (0xfffff000)
#endif

#endif /* TM_NS32K_H */
