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


#ifndef _HW_DISK_C_
#define _HW_DISK_C_

#include "device_table.h"

#include "pk.h"

#include <stdio.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifndef	SEEK_SET
#define	SEEK_SET 0
#endif

/* DEVICE
   
   cdrom - readable block device

   disk - readable block device that should be writeable
   
   floppy - readable block device that should be writeable

   DESCRIPTION
   
   Block I/O devices that model the behavour of a fixed or removable
   disk device.

   Creating an instance of this device with no arguments will provide
   access to primative read/write operators.  If arguments are
   specified then the disk-label package is used to perform abstract
   disk I/O. The disk-label package will then use this devices
   primatives.

   For hardware I/O, this device would normally be attached to a
   parent `bus' and that bus would use the I/O methods to read and
   write raw data.  The bus might actually be a SCSI or IDE device.
   It is assumed that the parent bus can take care of DMA.

   PROPERTIES
   
   reg = <address> (required)

   <address> is parent bus dependant.

   device_type = "block"

   name = "disk" | "cdrom" | "fd"

   file = <file-name> (required)

   The name of the file that contains the disk image.

   */

typedef struct _hw_disk_device {
  const char *name;
  int read_only;
  unsigned_word size;
  FILE *image;
} hw_disk_device;

typedef struct _hw_disk_instance {
  long pos;
  hw_disk_device *disk;
} hw_disk_instance;


static void
hw_disk_init_address(device *me)
{
  hw_disk_device *disk = device_data(me);
  generic_device_init_address(me);
  if (disk->image != NULL)
    fclose(disk->image);
  disk->name = device_find_string_property(me, "file");
  if (strcmp(device_name(me), "disk") == 0) {
    disk->read_only = 0;
    disk->image = fopen(disk->name, "r+");
  }
  else {
    disk->read_only = 1;
    disk->image = fopen(disk->name, "r");
  }
  if (disk->image == NULL) {
    perror(device_name(me));
    device_error(me, "open %s failed\n", disk->name);
  }
}

static unsigned
hw_disk_io_read_buffer(device *me,
		       void *dest,
		       int space,
		       unsigned_word addr,
		       unsigned nr_bytes,
		       cpu *processor,
		       unsigned_word cia)
{
  hw_disk_device *disk = device_data(me);
  if (nr_bytes == 0)
    return 0;
  if (addr + nr_bytes > disk->size)
    return 0;
  if (fseek(disk->image, addr, SEEK_SET) < 0)
    return 0;
  if (fread(dest, nr_bytes, 1, disk->image) != 1)
    return 0;
  return nr_bytes;
}


static unsigned
hw_disk_io_write_buffer(device *me,
			const void *source,
			int space,
			unsigned_word addr,
			unsigned nr_bytes,
			cpu *processor,
			unsigned_word cia)
{
  hw_disk_device *disk = device_data(me);
  if (disk->read_only)
    return 0;
  if (nr_bytes == 0)
    return 0;
  if (addr + nr_bytes > disk->size)
    return 0;
  if (fseek(disk->image, addr, SEEK_SET) < 0)
    return 0;
  if (fwrite(source, nr_bytes, 1, disk->image) != 1)
    return 0;
  return nr_bytes;
}


/* instances of the hw_disk device */

static void
hw_disk_instance_delete(device_instance *instance)
{
  hw_disk_instance *data = device_instance_data(instance);
  zfree(data);
}

static int
hw_disk_instance_read(device_instance *instance,
		      void *buf,
		      unsigned_word len)
{
  hw_disk_instance *data = device_instance_data(instance);
  DTRACE(disk, ("read - len=%ld\n", (long)len));
  if (fseek(data->disk->image, data->pos, SEEK_SET) < 0)
    return -1;
  if (fread(buf, len, 1, data->disk->image) != 1)
    return -1;
  data->pos = ftell(data->disk->image);
  return len;
}

static int
hw_disk_instance_write(device_instance *instance,
		       const void *buf,
		       unsigned_word len)
{
  hw_disk_instance *data = device_instance_data(instance);
  DTRACE(disk, ("write - len=%ld\n", (long)len));
  if (data->disk->read_only)
    return -1;
  if (fseek(data->disk->image, data->pos, SEEK_SET) < 0)
    return -1;
  if (fwrite(buf, len, 1, data->disk->image) != 1)
    return -1;
  data->pos = ftell(data->disk->image);
  return len;
}

static int
hw_disk_instance_seek(device_instance *instance,
		      unsigned_word pos_hi,
		      unsigned_word pos_lo)
{
  hw_disk_instance *data = device_instance_data(instance);
  DTRACE(disk, ("seek - pos_hi=%ld pos_lo=%ld\n", (long)pos_hi, (long)pos_lo));
  data->pos = pos_lo;
  return 0;
}

static const device_instance_callbacks hw_disk_instance_callbacks = {
  hw_disk_instance_delete,
  hw_disk_instance_read,
  hw_disk_instance_write,
  hw_disk_instance_seek,
};

static device_instance *
hw_disk_create_instance(device *me,
			const char *path,
			const char *args)
{
  device_instance *disk_instance;
  hw_disk_device *disk = device_data(me);
  hw_disk_instance *data = ZALLOC(hw_disk_instance);
  data->disk = disk;
  data->pos = 0;
  disk_instance = device_create_instance_from(me, NULL,
					      data,
					      path, args,
					      &hw_disk_instance_callbacks);
  return pk_disklabel_create_instance(disk_instance, args);
}

static device_callbacks const hw_disk_callbacks = {
  { hw_disk_init_address, NULL },
  { NULL, }, /* address */
  { hw_disk_io_read_buffer,
      hw_disk_io_write_buffer, },
  { NULL, }, /* DMA */
  { NULL, }, /* interrupt */
  { NULL, }, /* unit */
  hw_disk_create_instance,
};


static void *
hw_disk_create(const char *name,
	       const device_unit *unit_address,
	       const char *args)
{
  /* create the descriptor */
  hw_disk_device *hw_disk = ZALLOC(hw_disk_device);
  return hw_disk;
}


const device_descriptor hw_disk_device_descriptor[] = {
  { "disk", hw_disk_create, &hw_disk_callbacks },
  { "cdrom", hw_disk_create, &hw_disk_callbacks },
  { NULL },
};

#endif /* _HW_DISK_C_ */
