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


#ifndef _HW_EEPROM_C_
#define _HW_EEPROM_C_

#ifndef STATIC_INLINE_HW_EEPROM
#define STATIC_INLINE_HW_EEPROM STATIC_INLINE
#endif

#include "device_table.h"

#ifdef HAVE_STRING_H
#include <string.h>
#else
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#endif

/* EEPROM - electricaly erasable programable memory

   Description:

   This device implements a small byte addressable EEPROM.
   Programming is performed using the same write sequences as used by
   modern EEPROM components.  Writes occure in real time, the device
   returning a progress value until the programing has been completed.

   Properties:

   reg = <address> <size>.  Determine where the device lives in the
   parents address space.

   nr-sectors = <integer>.  When erasing an entire sector is cleared
   at a time.  This specifies the number of sectors in the EEPROM
   component.

   byte-write-delay = <integer>.  Number of clock ticks before the
   programming of a single byte completes.

   sector-start-delay = <integer>.  When erasing sectors, the number
   of clock ticks after the sector has been specified and the actual
   erase process commences.

   erase-delay = <intger>.  Number of clock ticks before an erase
   program completes. */

typedef enum {
  read_reset,
  write_nr_2,
  write_nr_3,
  write_nr_4,
  write_nr_5,
  write_nr_6,
  byte_program,
  byte_programming,
  chip_erase, chip_erasing,
  sector_erase, sector_erasing,
  sector_erase_suspend,
  sector_erase_resume,
} eeprom_states;

typedef struct _eeprom_device {
  unsigned8 *memory;
  unsigned sizeof_memory;
  unsigned sector_size;
  unsigned nr_sectors;
  unsigned byte_write_delay;
  unsigned sector_start_delay;
  unsigned erase_delay;
  signed64 programme_start_time;
  unsigned program_byte_address;
  eeprom_states state;
} eeprom_device;

static void *
eeprom_create(const char *name,
	     const device_unit *unit_address,
	     const char *args,
	     device *parent)
{
  eeprom_device *eeprom = ZALLOC(eeprom_device);
  return eeprom;
}

typedef struct _eeprom_reg_spec {
  unsigned32 base;
  unsigned32 size;
} eeprom_reg_spec;

static void
eeprom_init_address(device *me,
		   psim *system)
{
  eeprom_device *eeprom = (eeprom_device*)device_data(me);
  const device_property *reg = device_find_array_property(me, "reg");
  const eeprom_reg_spec *spec = reg->array;
  int nr_entries = reg->sizeof_array / sizeof(*spec);

  if ((reg->sizeof_array % sizeof(*spec)) != 0)
    error("devices/%s reg property of incorrect size\n", device_name(me));
  if (nr_entries > 1)
    error("devices/%s reg property contains multiple specs\n",
	  device_name(me));

  /* initialize the eeprom */
  if (eeprom->memory == NULL) {
    eeprom->sizeof_memory = BE2H_4(spec->size);
    eeprom->memory = zalloc(eeprom->sizeof_memory);
  }
  else
    memset(eeprom->memory, eeprom->sizeof_memory, 0);

  /* figure out the sectors in the eeprom */
  eeprom->nr_sectors = device_find_integer_property(me, "nr-sectors");
  eeprom->sector_size = eeprom->sizeof_memory / eeprom->nr_sectors;
  if (eeprom->sector_size * eeprom->nr_sectors != eeprom->sizeof_memory)
    error("device/%s nr-sectors does not evenly divide eeprom\n",
	  device_name(me));

  /* timing */
  eeprom->byte_write_delay = device_find_integer_property(me, "byte-write-delay");
  eeprom->sector_start_delay = device_find_integer_property(me, "sector-start-delay");
  eeprom->erase_delay = device_find_integer_property(me, "erase-delay");

  device_attach_address(device_parent(me),
			device_name(me),
			attach_callback,
			0 /*address space*/,
			BE2H_4(spec->base),
			eeprom->sizeof_memory,
			access_read_write_exec,
			me);
}


static unsigned
eeprom_io_read_buffer(device *me,
		      void *dest,
		      int space,
		      unsigned_word addr,
		      unsigned nr_bytes,
		      cpu *processor,
		      unsigned_word cia)
{
  eeprom_device *eeprom = (eeprom_device*)device_data(me);
  int i;
  for (i = 0; i < nr_bytes; i++) {
    unsigned_word address = (addr + nr_bytes) % eeprom->sizeof_memory;
    eeprom->memory[address] = eeprom_io_read_byte(address);
  }
  return nr_bytes;
}

static void
eeprom_io_write_byte()
{
  switch (state) {
  case read_reset:
    if (address == 0x5555 && data = 0xaa)
      state = first_write;
    else
      state = read_reset;
    break;
  case first_write:
    if (address == 0x2aaa && data == 0x55)
      state = second_write;
    else
      state = read_reset; /* issue warning */
    break;
  case second_write:
    if (address == 0x5555 && data == 0xf0)
      state = read_reset;
    else if (address == 0x5555 && data == 0x90)
      state = auto_select;
    else if (address == 0x5555 && data == 0xa0)
      state = byte_program;
    else if (address == 0x5555 && data == 0x80)
      state = third_write;
    else
      state = read_reset;
    break;
  case fourth_write:
    if (address == 0x5555 && data == 0xaa)
      state = fith_write;
    else
      state = read_reset;
    break;
  case fith_write:
    if (address == 0x2aaa && data == 0x55)
      state = sixth_write;
    else
      state = read_reset;
    break;
  case sixth_write:
    if (address == 0x5555 && data == 0x10)
      state = chip_erase;
    else
      sector_erase();
    break;
  case auto_select:
    if (data == 0xf0)
      state = read_reset;
    else if (address == 0x5555 && data == 0xaa)
      state = second_write;
    else
      state = read_reset; /* issue warning */
    break;
  case sector_erase:
    if (data == 0xb0)
      state = sector_erase_suspend;
    else
      state = sector_erase; /* ignore */
    break;
  case sector_erase_suspend:
    if (data == 0x30)
      state = sector_erase;
    else
      state = sector_erase_suspend; /* ignore */
    break;
  case byte_program:
    /* perform the byte program */
    program_address = address;
    program_start = some_time();
    toggle = 0;
    /* but only make things `0' and never 1 */
    byte[address] = data;
    state = byte_programming;
    break;
  case byte_programming:
    if (finished)
      state = read_reset;
    else
      state = byte_programming;
    break;
  }
}

static unsigned
eeprom_io_write_buffer(device *me,
		       const void *source,
		       int space,
		       unsigned_word addr,
		       unsigned nr_bytes,
		       cpu *processor,
		       unsigned_word cia)
{
  eeprom_device *eeprom = (eeprom_device*)device_data(me);
  int i;
  for (i = 0; i < nr_bytes; i++) {
    unsigned_word address = (addr + nr_bytes) % eeprom->sizeof_memory;
    eeprom_io_read_byte(address, eeprom->memory[address]);
  }
  return nr_bytes;
}



static device_callbacks const eeprom_callbacks = {
  { eeprom_init_address, },
  { NULL, }, /* address */
  { eeprom_io_read_buffer, eeprom_io_write_buffer }, /* IO */
};

const device_descriptor eeprom_device_descriptor[] = {
  { "eeprom", eeprom_create, &eeprom_callbacks },
  { NULL },
};

#endif /* _HW_EEPROM_C_ */
