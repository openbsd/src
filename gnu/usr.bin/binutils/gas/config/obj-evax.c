/* obj-evax.c - EVAX (openVMS/Alpha) object file format.
   Copyright (C) 1996 Free Software Foundation, Inc.
   Contributed by Klaus Kämpf (kkaempf@progis.de) of
     proGIS Software, Aachen, Germany.

   This file is part of GAS, the GNU Assembler

   GAS is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   GAS is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GAS; see the file COPYING.  If not, write to
   the Free Software Foundation, 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA.  */

#define OBJ_HEADER "obj-evax.h"

#include "as.h"

void obj_read_begin_hook () {}

const pseudo_typeS obj_pseudo_table[] =
{
  {0, 0, 0},
};				/* obj_pseudo_table */


/*
 * Local Variables:
 * comment-column: 0
 * fill-column: 131
 * End:
 */

/* end of obj-evax.c */
