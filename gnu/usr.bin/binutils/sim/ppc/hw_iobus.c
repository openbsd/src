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


#ifndef _HW_IOBUS_C_
#define _HW_IOBUS_C_

#ifndef STATIC_INLINE_HW_IOBUS
#define STATIC_INLINE_HW_IOBUS STATIC_INLINE
#endif

#include "device_table.h"


/* DEVICE

   iobus - simple bus for attaching devices

   DESCRIPTION

   IOBUS provides a simple `local' bus for attaching (hanging)
   programmed IO devices from.  All child devices are mapped into this
   devices parent address space (after adjusting for the iobus's base
   address).

   PROPERTIES

   None.

   */

static void
hw_iobus_attach_address_callback(device *me,
			      const char *name,
			      attach_type type,
			      int space,
			      unsigned_word addr,
			      unsigned nr_bytes,
			      access_type access,
			      device *who) /*callback/default*/
{
  unsigned_word hw_iobus_addr;
  /* sanity check */
  if (space != 0)
    error("hw_iobus_attach_address_callback() no space for %s/%s\n",
	  device_name(me), name);
  /* get the bus address */
  if (device_unit_address(me)->nr_cells != 1)
    error("hw_iobus_attach_address_callback() invalid address for %s\n",
	  device_name(me));
  hw_iobus_addr = device_unit_address(me)->cells[0];
  device_attach_address(device_parent(me),
			device_name(me),
			type,
			0 /*space*/,
			hw_iobus_addr + addr,
			nr_bytes,
			access,
			who);
}


static device_callbacks const hw_iobus_callbacks = {
  { NULL, },
  { hw_iobus_attach_address_callback, },
  { NULL, }, /* IO */
  { NULL, }, /* DMA */
  { NULL, }, /* interrupt */
  { generic_device_unit_decode,
    generic_device_unit_encode,
    generic_device_address_to_attach_address,
    generic_device_size_to_attach_size }
};


const device_descriptor hw_iobus_device_descriptor[] = {
  { "iobus", NULL, &hw_iobus_callbacks },
  { NULL, },
};

#endif /* _HW_IOBUS_ */
