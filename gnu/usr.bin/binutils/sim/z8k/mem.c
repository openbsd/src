/* memory support for Z8KSIM
   Copyright (C) 1987, 1992 Free Software Foundation, Inc.

This file is part of Z8KSIM

Z8KSIM is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

Z8KSIM is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Z8KZIM; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#include "config.h"

#include <ansidecl.h>

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include "tm.h"
#include "mem.h"
#include "sim.h"

#define INLINE
static 
long
sitoptr (si)
long si;
{
  return ((si & 0xff000000) >> 8) | (si & 0xffff);
}

static
unsigned short *
get_page_and_offset (context, where, offset_ptr)
     sim_state_type *context;
     sim_phys_addr_type where;
     int *offset_ptr;
{
  /* Is the page allocated ? */

  if (context->memory == 0)
    {
      /* Must allocate 16MB in order to run Z8001 programs.  */
      context->memory  = (unsigned short *)calloc(64*1024*64,4);
    }

  *offset_ptr = sitoptr(where);
  return context->memory;
}

void
sim_write_byte (context, where, what)
     sim_state_type *context;
     sim_phys_addr_type where;
     int what;
{
  unsigned int offset;
  char *ptr = (char *)get_page_and_offset (context, where, &offset);

  ptr[offset] = what;
}

void
sim_write_short (context, where, what)
     sim_state_type *context;
     sim_phys_addr_type where;
     int what;
{
  int offset;
  char *ptr = (char *)get_page_and_offset (context, where, &offset);

  ptr[offset] = what >> 8;
  ptr[offset + 1] = what;
}

void
sim_write_long (context, where, what)
     sim_state_type *context;
     sim_phys_addr_type where;
     int what;
{
  int offset;
  char *ptr = (char *)get_page_and_offset (context, where, &offset);

  ptr[offset] = what >> 24;
  ptr[offset + 1] = what >> 16;
  ptr[offset + 3] = what >> 8;
  ptr[offset + 4] = what;
}

int
sim_read_byte (context, where)
     sim_state_type *context;
     sim_phys_addr_type where;
{
  int offset;
  char *ptr = (char *)get_page_and_offset (context, where, &offset);

 return ptr[offset];
}

unsigned
sim_read_short (context, where)
     sim_state_type *context;
     sim_phys_addr_type where;
{
  int what;
  int offset;

  char *ptr = (char *)get_page_and_offset (context, where, &offset);

  what = (ptr[offset] << 8) | ptr[offset + 1];
  return what;
}



