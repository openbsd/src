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


#ifndef _HW_NVRAM_C_
#define _HW_NVRAM_C_

#ifndef STATIC_INLINE_HW_NVRAM
#define STATIC_INLINE_HW_NVRAM STATIC_INLINE
#endif

#include "device_table.h"

#ifdef HAVE_TIME_H
#include <time.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#else
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#endif

/* DEVICE

   nvram - non-volatile memory with clock

   DESCRIPTION

   This device implements a small byte addressable non-volatile
   memory.  The top 8 bytes of this memory include a real-time clock.

   PROPERTIES

   reg = <address> <size> (required)

   Specify the address/size of this device within its parents address
   space.

   timezone = <integer> (required)

   Adjustment to the hosts current GMT (in seconds) that should be
   applied when updating the NVRAM's clock.

   */

typedef struct _hw_nvram_device {
  unsigned8 *memory;
  unsigned sizeof_memory;
#ifdef HAVE_TIME_H
  time_t host_time;
#else
  long host_time;
#endif
  unsigned timezone;
  /* useful */
  unsigned addr_year;
  unsigned addr_month;
  unsigned addr_date;
  unsigned addr_day;
  unsigned addr_hour;
  unsigned addr_minutes;
  unsigned addr_seconds;
  unsigned addr_control;
} hw_nvram_device;

static void *
hw_nvram_create(const char *name,
		const device_unit *unit_address,
		const char *args)
{
  hw_nvram_device *hw_nvram = ZALLOC(hw_nvram_device);
  return hw_nvram;
}

typedef struct _hw_nvram_reg_spec {
  unsigned32 base;
  unsigned32 size;
} hw_nvram_reg_spec;

static void
hw_nvram_init_address(device *me)
{
  hw_nvram_device *hw_nvram = (hw_nvram_device*)device_data(me);
  device_unit addr;
  device_unit size;
  int attach_space;
  unsigned_word attach_addr;
  unsigned attach_size;
  
  /* find our address information */
  if (device_find_reg_property(me, "reg", 0, &addr, &size) != 1)
    device_error(me, "reg property should contain only one phys-addr:size tupple\n");
  device_address_to_attach_address(device_parent(me), &addr, &attach_space, &attach_addr);
  device_size_to_attach_size(device_parent(me), &size, &attach_size);

  /* initialize the hw_nvram */
  if (hw_nvram->memory == NULL) {
    hw_nvram->sizeof_memory = attach_size;
    hw_nvram->memory = zalloc(hw_nvram->sizeof_memory);
  }
  else
    memset(hw_nvram->memory, hw_nvram->sizeof_memory, 0);
  
  hw_nvram->timezone = device_find_integer_property(me, "timezone");
  
  hw_nvram->addr_year = hw_nvram->sizeof_memory - 1;
  hw_nvram->addr_month = hw_nvram->sizeof_memory - 2;
  hw_nvram->addr_date = hw_nvram->sizeof_memory - 3;
  hw_nvram->addr_day = hw_nvram->sizeof_memory - 4;
  hw_nvram->addr_hour = hw_nvram->sizeof_memory - 5;
  hw_nvram->addr_minutes = hw_nvram->sizeof_memory - 6;
  hw_nvram->addr_seconds = hw_nvram->sizeof_memory - 7;
  hw_nvram->addr_control = hw_nvram->sizeof_memory - 8;
  
  device_attach_address(device_parent(me),
			device_name(me),
			attach_callback,
			attach_space,
			attach_addr,
			hw_nvram->sizeof_memory,
			access_read_write_exec,
			me);
}

static int
hw_nvram_bcd(int val)
{
  return ((val / 10) << 4) + (val % 10);
}

/* If reached an update interval and allowed, update the clock within
   the hw_nvram.  While this function could be implemented using events
   it isn't on the assumption that the HW_NVRAM will hardly ever be
   referenced and hence there is little need in keeping the clock
   continually up-to-date */
static void
hw_nvram_update_clock(hw_nvram_device *hw_nvram, cpu *processor)
{
#ifdef HAVE_TIME_H
  if (!(hw_nvram->memory[hw_nvram->addr_control] & 0xc0)) {
    time_t host_time = time(NULL);
    if (hw_nvram->host_time != host_time) {
      time_t nvtime = host_time + hw_nvram->timezone;
      struct tm *clock = gmtime(&nvtime);
      hw_nvram->host_time = host_time;
      hw_nvram->memory[hw_nvram->addr_year] = hw_nvram_bcd(clock->tm_year);
      hw_nvram->memory[hw_nvram->addr_month] = hw_nvram_bcd(clock->tm_mon + 1);
      hw_nvram->memory[hw_nvram->addr_date] = hw_nvram_bcd(clock->tm_mday);
      hw_nvram->memory[hw_nvram->addr_day] = hw_nvram_bcd(clock->tm_wday + 1);
      hw_nvram->memory[hw_nvram->addr_hour] = hw_nvram_bcd(clock->tm_hour);
      hw_nvram->memory[hw_nvram->addr_minutes] = hw_nvram_bcd(clock->tm_min);
      hw_nvram->memory[hw_nvram->addr_seconds] = hw_nvram_bcd(clock->tm_sec);
    }
  }
#else
  error("fixme - where do I find out GMT\n");
#endif
}

static void
hw_nvram_set_clock(hw_nvram_device *hw_nvram, cpu *processor)
{
  error ("fixme - how do I set the localtime\n");
}

static unsigned
hw_nvram_io_read_buffer(device *me,
			void *dest,
			int space,
			unsigned_word addr,
			unsigned nr_bytes,
			cpu *processor,
			unsigned_word cia)
{
  int i;
  hw_nvram_device *hw_nvram = (hw_nvram_device*)device_data(me);
  for (i = 0; i < nr_bytes; i++) {
    unsigned address = (addr + i) % hw_nvram->sizeof_memory;
    unsigned8 data = hw_nvram->memory[address];
    hw_nvram_update_clock(hw_nvram, processor);
    ((unsigned8*)dest)[i] = data;
  }
  return nr_bytes;
}

static unsigned
hw_nvram_io_write_buffer(device *me,
			 const void *source,
			 int space,
			 unsigned_word addr,
			 unsigned nr_bytes,
			 cpu *processor,
			 unsigned_word cia)
{
  int i;
  hw_nvram_device *hw_nvram = (hw_nvram_device*)device_data(me);
  for (i = 0; i < nr_bytes; i++) {
    unsigned address = (addr + i) % hw_nvram->sizeof_memory;
    unsigned8 data = ((unsigned8*)source)[i];
    if (address == hw_nvram->addr_control
	&& (data & 0x80) == 0
	&& (hw_nvram->memory[address] & 0x80) == 0x80)
      hw_nvram_set_clock(hw_nvram, processor);
    else
      hw_nvram_update_clock(hw_nvram, processor);
    hw_nvram->memory[address] = data;
  }
  return nr_bytes;
}

static device_callbacks const hw_nvram_callbacks = {
  { hw_nvram_init_address, },
  { NULL, }, /* address */
  { hw_nvram_io_read_buffer, hw_nvram_io_write_buffer }, /* IO */
};

const device_descriptor hw_nvram_device_descriptor[] = {
  { "nvram", hw_nvram_create, &hw_nvram_callbacks },
  { NULL },
};

#endif /* _HW_NVRAM_C_ */
