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


#ifndef _HW_COM_C_
#define _HW_COM_C_

#ifndef STATIC_INLINE_HW_COM
#define STATIC_INLINE_HW_COM STATIC_INLINE
#endif

#include "device_table.h"

#include "cpu.h"

#ifdef HAVE_STRING_H
#include <string.h>
#else
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

/* DEVICE
   

   com - '550 compatible serial device
   

   DESCRIPTION
   

   Models the basics of the 8 register '550 serial device.  The model
   includes an interrupt line, input and output fifos along with
   status information.

   Both input and output can be separatly configured to: take data
   from (send data to) either a file or the simulation console;
   control the time taken for a fifo to become ready again; have
   either buffered or unbuffered output (input).

   If reading from a file, carrier will be lost when the end of file
   is reached (however one extra null character may be read before
   this is reported).
   

   PROPERTIES
   

   reg = <address> <size> ... (required)

   List of <address> <size> pairs.  Each pair specifies an address for
   the devices 8 registers.  The address should be 8 byte aligned.


   alternate-reg = <address> <size> ... (optional)

   Alternative addreses for the registers.


   input-file = <file-name> (optional)

   File to take all serial port input from (instead of the simulation
   console).


   output-file = <file-name> (optional)

   File to send all output to (instead of the simulation console).


   input-buffering = "unbuffered" (optional)

   Effect unknown.


   output-buffering = "unbuffered" (optional)

   Select unbuffered output - all data written to the serial device is
   immediatly output.


   input-delay = <delay> (optional)

   Specify the number of ticks after the current character has been
   read from the serial port that the next character becomes
   available.


   output-delay = <delay> (optional)

   Specify the number of ticks after a character has been written to
   the empty output fifo that the fifo finishes draining.  Any
   characters written to the output fifo before it has drained will
   not be lost and will still be displayed.


   EXAMPLES


   |  /iobus@0xf0000000/com@0x3000/reg 0x3000 8

   Create a simple console device at address <<0x3000>> within
   <<iobus>>.  Since iobus starts at address <<0xf0000000>> the
   absolute address of the serial port will be <<0xf0003000>>.

   The device will always already be ready for I/O (no delay
   properties) and both input and output will be taken from the
   simulation console (no file properties).


   |  $ psim \
   |    -o '/cpus/cpu@0' \
   |    -o '/iobus@0xf0000000/com@0x4000/reg 0x4000 8' \
   |    -o '/iobus@0xf0000000/com@0x4000/input-file /etc/passwd' \
   |    -o '/iobus@0xf0000000/com@0x4000/input-delay 1000' \
   |    -o '/iobus@0xf0000000/com@0x4000 > 0 int /cpus/cpu@0x0' \
   |    psim-test/hw-com/cat.be 0xf0004000

   The serial port (at address <<0xf0004000>> is configured so that it
   takes its input from the file <</etc/passwd>> while its output is
   allowed to appear on the simulation console.  (The <</cpus/cpu@0>>
   node is explicitly specified to ensure that the cpu node is created
   before any interrupt is attached to it).

   Since the program <<psim-test/hw-com/cat>> copies all characters on
   the serial ports input (<</etc/passwd>>) to its output (the
   console) the net effect of the above is to display the file
   <</etc/passwd>> on the screen.


   BUGS


   IEEE 1275 requires that a device on a PCI bus have, as its first
   reg entry, the specification of its configuration space.  Currently
   this device does not expect this.  Instead it assumes that all reg
   entries should be treated as aliases for a single set of registers.

   Instead of attempting to model the '550's 16 byte fifo's this
   device limits the input to single characters and provides an
   infinate output fifo.  Consequently, unlike the '550 which would
   start discarding characters after the first 16, the user can write
   any number of characters to the output fifo (regardless of the
   ready status) without loss of data.

   The input and output can only be taken from a file (or the current
   terminal device).  As an option, the com device should allow a
   program (such as xterm) to be used as the data source and sink.

   The input blocks if no data is available.  Only buffered and
   unbuffered file I/O is currently supported.

   Interrupts have not been tested.

   */

enum {
  max_hw_com_registers = 8,
};

typedef struct _com_port {
  int ready;
  int delay;
  int interrupting;
  FILE *file;
} com_port;

typedef struct _com_modem {
  int carrier;
  int carrier_changed;
  int interrupting;
} com_modem;

typedef struct _hw_com_device {
  com_port input;
  com_port output;
  com_modem modem;
  char dlab[2];
  char reg[max_hw_com_registers];
  event_queue *events;
  int interrupting;
} hw_com_device;


static void
hw_com_device_init_data(device *me)
{
  hw_com_device *com = (hw_com_device*)device_data(me);
  /* clean up */
  if (com->output.file != NULL)
    fclose(com->output.file);
  if (com->input.file != NULL)
    fclose(com->input.file);
  memset(com, 0, sizeof(hw_com_device));

  /* the fifo speed */
  com->output.delay = (device_find_property(me, "output-delay") != NULL
		       ? device_find_integer_property(me, "output-delay")
		       : 0);
  com->input.delay = (device_find_property(me, "input-delay") != NULL
		      ? device_find_integer_property(me, "input-delay")
		      : 0);

  /* the data source/sink */
  if (device_find_property(me, "input-file") != NULL) {
    const char *input_file = device_find_string_property(me, "input-file");
    com->input.file = fopen(input_file, "r");
    if (com->input.file == NULL)
      device_error(me, "Problem opening input file %s\n", input_file);
    if (device_find_property(me, "input-buffering") != NULL) {
      const char *buffering = device_find_string_property(me, "input-buffering");
      if (strcmp(buffering, "unbuffered") == 0)
	setbuf(com->input.file, NULL);
    }
  }
  if (device_find_property(me, "output-file") != NULL) {
    const char *output_file = device_find_string_property(me, "output-file");
    com->output.file = fopen(output_file, "w");
    if (com->input.file == NULL)
      device_error(me, "Problem opening output file %s\n", output_file);
    if (device_find_property(me, "output-buffering") != NULL) {
      const char *buffering = device_find_string_property(me, "output-buffering");
      if (strcmp(buffering, "unbuffered") == 0)
	setbuf(com->output.file, NULL);
    }
  }

  /* the master event queue */
  com->events = psim_event_queue(device_system(me));

  /* ready from the start */
  com->input.ready = 1;
  com->modem.carrier = 1;
  com->output.ready = 1;
}


static void
update_com_interrupts(device *me,
		      hw_com_device *com)
{
  int interrupting;
  com->modem.interrupting = (com->modem.carrier_changed && (com->reg[1] & 0x80));
  com->input.interrupting = (com->input.ready && (com->reg[1] & 0x1));
  com->output.interrupting = (com->output.ready && (com->reg[1] & 0x2));
  interrupting = (com->input.interrupting
		  || com->output.interrupting
		  || com->modem.interrupting);

  if (interrupting) {
    if (!com->interrupting) {
      device_interrupt_event(me, 0 /*port*/, 1 /*value*/, NULL, 0);
    }
  }
  else /*!interrupting*/ {
    if (com->interrupting)
      device_interrupt_event(me, 0 /*port*/, 0 /*value*/, NULL, 0);
  }
  com->interrupting = interrupting;
}


static void
make_read_ready(void *data)
{
  device *me = (device*)data;
  hw_com_device *com = (hw_com_device*)device_data(me);
  com->input.ready = 1;
  update_com_interrupts(me, com);
}

static void
read_com(device *me,
	 unsigned_word a,
	 char val[1])
{
  unsigned_word addr = a % 8;
  hw_com_device *com = (hw_com_device*)device_data(me);

  /* the divisor latch is special */
  if (com->reg[3] & 0x8 && addr < 2) {
    *val = com->dlab[addr];
    return;
  }

  switch (addr) {
  
  case 0:
    /* fifo */
    if (!com->modem.carrier)
      *val = '\0';
    if (com->input.ready) {
      /* read the char in */
      if (com->input.file == NULL) {
	if (sim_io_read_stdin(val, 1) < 0)
	  com->modem.carrier_changed = 1;
      }
      else {
	if (fread(val, 1, 1, com->input.file) == 0)
	  com->modem.carrier_changed = 1;
      }
      /* setup for next read */
      if (com->modem.carrier_changed) {
	/* once lost carrier, never ready */
	com->modem.carrier = 0;
	com->input.ready = 0;
	*val = '\0';
      }
      else if (com->input.delay > 0) {
	com->input.ready = 0;
	event_queue_schedule(com->events, com->input.delay, make_read_ready, me);
      }
    }
    else {
      /* discard it? */
      /* overflow input fifo? */
      *val = '\0';
    }
    break;

  case 2:
    /* interrupt ident */
    if (com->interrupting) {
      if (com->input.interrupting)
	*val = 0x4;
      else if (com->output.interrupting)
	*val = 0x2;
      else if (com->modem.interrupting == 0)
	*val = 0;
      else
	device_error(me, "bad elif for interrupts\n");
    }
    else
      *val = 0x1;
    break;

  case 5:
    /* line status */
    *val = ((com->input.ready ? 0x1 : 0)
	    | (com->output.ready ? 0x60 : 0)
	    );
    break;

  case 6:
    /* modem status */
    *val = ((com->modem.carrier_changed ? 0x08 : 0)
	    | (com->modem.carrier ? 0x80 : 0)
	    );
    com->modem.carrier_changed = 0;
    break;

  default:
    *val = com->reg[addr];
    break;

  }
  update_com_interrupts(me, com);
}

static unsigned
hw_com_io_read_buffer_callback(device *me,
			       void *dest,
			       int space,
			       unsigned_word addr,
			       unsigned nr_bytes,
			       cpu *processor,
			       unsigned_word cia)
{
  int i;
  for (i = 0; i < nr_bytes; i++) {
    read_com(me, addr + i, &((char*)dest)[i]);
  }
  return nr_bytes;
}


static void
make_write_ready(void *data)
{
  device *me = (device*)data;
  hw_com_device *com = (hw_com_device*)device_data(me);
  com->output.ready = 1;
  update_com_interrupts(me, com);
}

static void
write_com(device *me,
	  unsigned_word a,
	  char val)
{
  unsigned_word addr = a % 8;
  hw_com_device *com = (hw_com_device*)device_data(me);

  /* the divisor latch is special */
  if (com->reg[3] & 0x8 && addr < 2) {
    com->dlab[addr] = val;
    return;
  }

  switch (addr) {
  
  case 0:
    /* fifo */
    if (com->output.file == NULL) {
      sim_io_write_stdout(&val, 1);
    }
    else {
      fwrite(&val, 1, 1, com->output.file);
    }
    /* setup for next write */
    if (com->output.ready && com->output.delay > 0) {
      com->output.ready = 0;
      event_queue_schedule(com->events, com->output.delay, make_write_ready, me);
    }
    break;

  default:
    com->reg[addr] = val;
    break;

  }
  update_com_interrupts(me, com);
}

static unsigned
hw_com_io_write_buffer_callback(device *me,
				const void *source,
				int space,
				unsigned_word addr,
				unsigned nr_bytes,
				cpu *processor,
				unsigned_word cia)
{
  int i;
  for (i = 0; i < nr_bytes; i++) {
    write_com(me, addr + i, ((char*)source)[i]);
  }
  return nr_bytes;
}


/* instances of the hw_com device */

static void
hw_com_instance_delete_callback(device_instance *instance)
{
  /* nothing to delete, the hw_com is attached to the device */
  return;
}

static int
hw_com_instance_read_callback(device_instance *instance,
			      void *buf,
			      unsigned_word len)
{
  return -1;
}

static int
hw_com_instance_write_callback(device_instance *instance,
			       const void *buf,
			       unsigned_word len)
{
  return -1;
}

static const device_instance_callbacks hw_com_instance_callbacks = {
  hw_com_instance_delete_callback,
  hw_com_instance_read_callback,
  hw_com_instance_write_callback,
};

static device_instance *
hw_com_create_instance(device *me,
		       const char *path,
		       const char *args)
{
  return device_create_instance_from(me, NULL,
				     device_data(me),
				     path, args,
				     &hw_com_instance_callbacks);
}


static device_callbacks const hw_com_callbacks = {
  { generic_device_init_address,
    hw_com_device_init_data },
  { NULL, }, /* address */
  { hw_com_io_read_buffer_callback,
      hw_com_io_write_buffer_callback, },
  { NULL, }, /* DMA */
  { NULL, }, /* interrupt */
  { NULL, }, /* unit */
  hw_com_create_instance,
};


static void *
hw_com_create(const char *name,
	      const device_unit *unit_address,
	      const char *args)
{
  /* create the descriptor */
  hw_com_device *hw_com = ZALLOC(hw_com_device);
  return hw_com;
}


const device_descriptor hw_com_device_descriptor[] = {
  { "com", hw_com_create, &hw_com_callbacks },
  { NULL },
};

#endif /* _HW_COM_C_ */
