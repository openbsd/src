/* -------------------------------------------------- 
 |  NAME
 |    devices
 |  PURPOSE
 |    provide simple routines and access to an exec device.
 |  NOTES
 | 
 |  COPYRIGHT
 |    Copyright (C) 1993  Christian E. Hopps
 |
 |    This program is free software; you can redistribute it and/or modify
 |    it under the terms of the GNU General Public License as published by
 |    the Free Software Foundation; either version 2 of the License, or
 |    (at your option) any later version.
 |
 |    This program is distributed in the hope that it will be useful,
 |    but WITHOUT ANY WARRANTY; without even the implied warranty of
 |    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 |    GNU General Public License for more details.
 |
 |    You should have received a copy of the GNU General Public License
 |    along with this program; if not, write to the Free Software
 |    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 |    
 |  HISTORY
 |    chopps - Oct 9, 1993: Created.
 +--------------------------------------------------- */

#if ! defined (_DEVICES_H)
#define _DEVICES_H
#include "util.h"
#include <exec/ports.h>
#include <exec/io.h>
#include <exec/devices.h>

struct device_data {
    struct MsgPort *port;
    struct IORequest *io;
    char  *name;
    ulong  unit;
    ulong  flags;
    int    open;
    
};

struct device_data * init_device (char *name, ulong unit, ulong flags, ulong iosize);
int open_device (struct device_data *dd);
void close_device (struct device_data *dd);
void free_device (struct device_data *dd);

#endif /* _DEVICES_H */
