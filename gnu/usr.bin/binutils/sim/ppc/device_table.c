/*  This file is part of the program psim.

    Copyright (C) 1994-1996, Andrew Cagney <cagney@highland.com.au>

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
 
    */


#ifndef _DEVICE_TABLE_C_
#define _DEVICE_TABLE_C_

#include "device_table.h"

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif


/* Helper functions */

/* Generic device init: Attaches the device of size <nr_bytes> (taken
   from <name>@<int>,<nr_bytes>) to its parent at address zero and
   with read/write access. */

typedef struct _generic_reg_spec {
  unsigned32 base;
  unsigned32 size;
} generic_reg_spec;


void
generic_device_init_address(device *me)
{
  static const char *(reg_property_names[]) = { "reg", "alternate-reg" , NULL };
  const char **reg_property_name;
  int nr_reg_entries = 0;
  for (reg_property_name = reg_property_names;
       *reg_property_name != NULL;
       reg_property_name++) {
    if (device_find_property(me, *reg_property_name) != NULL) {
      device_unit address;
      device_unit size;
      int reg_entry;
      for (reg_entry = 0;
	   device_find_reg_property(me, *reg_property_name, reg_entry,
				    &address, &size);
	   reg_entry++, nr_reg_entries++) {
	unsigned_word attach_address;
	int attach_space;
	unsigned attach_size;
	device_address_to_attach_address(device_parent(me),
					 &address, &attach_space, &attach_address);
	device_size_to_attach_size(device_parent(me), &size, &attach_size);
	device_attach_address(device_parent(me),
			      device_name(me),
			      attach_callback,
			      attach_space, attach_address, attach_size,
			      access_read_write_exec,
			      me);
      }
    }
  }

  if (nr_reg_entries == 0) {
    device_error(me, "Missing or empty reg property\n");
  }
}

int
generic_device_unit_decode(device *me,
			   const char *unit,
			   device_unit *phys)
{
  memset(phys, 0, sizeof(device_unit));
  if (unit == NULL)
    return 0;
  else {
    char *pos = (char*)unit; /* force for strtoul() */
    while (1) {
      char *old_pos = pos;
      long int val = strtoul(pos, &pos, 0);
      if (old_pos == pos && *pos == '\0')
	return phys->nr_cells;
      if (old_pos == pos && *pos != '\0')
	return -1;
      if (phys->nr_cells == 4)
	return -1;
      phys->cells[phys->nr_cells] = val;
      phys->nr_cells++;
      if (*pos != ',')
	return phys->nr_cells;
      pos++;
    }
  }
}

int
generic_device_unit_encode(device *me,
			   const device_unit *phys,
			   char *buf,
			   int sizeof_buf)
{
  int i;
  int len;
  char *pos = buf; /* force for strtoul() */
  for (i = 0; i < phys->nr_cells; i++) {
    if (pos != buf) {
      strcat(pos, ",");
      pos = strchr(pos, '\0');
    }
    sprintf(pos, "0x%lx", (unsigned long)phys->cells[i]);
    pos = strchr(pos, '\0');
  }
  len = pos - buf;
  if (len >= sizeof_buf)
    error("generic_unit_encode - buffer overflow\n");
  return len;
}

void
generic_device_address_to_attach_address(device *me,
					 const device_unit *address,
					 int *attach_space,
					 unsigned_word *attach_address)
{
  if (address->nr_cells != 1)
    device_error(me, "number of cells in reg address should be one for this device\n");
  if (device_find_property(me, "#address-cells") != NULL
      && device_find_integer_property(me, "#address-cells") != 1)
    device_error(me, "Parents #address-cells property should be one for this device\n");
  *attach_space = 0;
  *attach_address = address->cells[0];
}

void
generic_device_size_to_attach_size(device *me,
				   const device_unit *size,
				   unsigned *nr_bytes)
{
  if (size->nr_cells != 1)
    device_error(me, "number of cells in reg size should be one\n");
  if (device_find_property(me, "#size-cells") != NULL
      && device_find_integer_property(me, "#size-cells") != 1)
    device_error(me, "Parents #address-cells property should be one for this device\n");
  *nr_bytes = size->cells[0];
}

/* ignore/passthrough versions of each function */

void
passthrough_device_address_attach(device *me,
				  const char *name,
				  attach_type attach,
				  int space,
				  unsigned_word addr,
				  unsigned nr_bytes,
				  access_type access,
				  device *who) /*callback/default*/
{
  device_attach_address(device_parent(me), name, attach,
			space, addr, nr_bytes,
			access,
			who);
}

void
passthrough_device_address_detach(device *me,
				  const char *name,
				  attach_type attach,
				  int space,
				  unsigned_word addr,
				  unsigned nr_bytes,
				  access_type access,
				  device *who) /*callback/default*/
{
  device_detach_address(device_parent(me), name, attach,
			space, addr, nr_bytes, access,
			who);
}

unsigned
passthrough_device_dma_read_buffer(device *me,
				   void *dest,
				   int space,
				   unsigned_word addr,
				   unsigned nr_bytes)
{
  return device_dma_read_buffer(device_parent(me), dest,
				space, addr, nr_bytes);
}

unsigned
passthrough_device_dma_write_buffer(device *me,
			     const void *source,
			     int space,
			     unsigned_word addr,
			     unsigned nr_bytes,
			     int violate_read_only_section)
{
  return device_dma_write_buffer(device_parent(me), source,
				 space, addr,
				 nr_bytes,
				 violate_read_only_section);
}

int
ignore_device_unit_decode(device *me,
			  const char *unit,
			  device_unit *phys)
{
  memset(phys, 0, sizeof(device_unit));
  return 0;
}


static const device_callbacks passthrough_callbacks = {
  { NULL, }, /* init */
  { passthrough_device_address_attach,
    passthrough_device_address_detach, },
  { NULL, }, /* IO */
  { passthrough_device_dma_read_buffer, passthrough_device_dma_write_buffer, },
  { NULL, }, /* interrupt */
  { generic_device_unit_decode,
    generic_device_unit_encode, },
};


static const device_descriptor ob_device_table[] = {
  /* standard OpenBoot devices */
  { "aliases", NULL, &passthrough_callbacks },
  { "options", NULL, &passthrough_callbacks },
  { "chosen", NULL, &passthrough_callbacks },
  { "packages", NULL, &passthrough_callbacks },
  { "cpus", NULL, &passthrough_callbacks },
  { "openprom", NULL, &passthrough_callbacks },
  { "init", NULL, &passthrough_callbacks },
  { NULL },
};

const device_descriptor *const device_table[] = {
  ob_device_table,
#include "hw.c"
  NULL,
};


#endif /* _DEVICE_TABLE_C_ */
