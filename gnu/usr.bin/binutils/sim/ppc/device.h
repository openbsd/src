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


#ifndef _DEVICE_H_
#define _DEVICE_H_

#ifndef INLINE_DEVICE
#define INLINE_DEVICE
#endif

/* declared in basics.h, this object is used everywhere */
/* typedef struct _device device; */


/* Introduction:

   As explained in earlier sections, the device, device instance,
   property and interrupts lie at the heart of PSIM's device model.

   In the below a synopsis of the device object and the operations it
   supports are given.  Details of this object can be found in the
   files <<device.h>> and <<device.c>>.

   */


/* Constructing the device tree:

   The initial device tree populated with devices and basic properties
   is created using the function <<device_tree_add_parsed()>>.  This
   function parses a PSIM device specification and uses it to populate
   the tree accordingly.

   This function accepts a printf style formatted string as the
   argument that describes the entry.  Any properties or interrupt
   connections added to a device tree using this function are marked
   as having a permenant disposition.  When the tree is (re)
   initialized they will be restored to their initial value.

   */

EXTERN_DEVICE\
(device *) device_tree_add_parsed
(device *current,
 const char *fmt,
 ...) __attribute__ ((format (printf, 2, 3)));


/* Initializing the created tree:

   Once a device tree has been created the <<device_tree_init()>>
   function is used to initialize it.  The exact sequence of events
   that occure during initialization are described separatly.

   */

INLINE_DEVICE\
(void) device_tree_init
(device *root,
 psim *system);


/* Relationships:

   A device is able to determine its relationship to other devices
   within the tree.  Operations include querying for a devices parent,
   sibling, child, name, and path (from the root).

   */

INLINE_DEVICE\
(device *) device_parent
(device *me);

INLINE_DEVICE\
(device *) device_sibling
(device *me);

INLINE_DEVICE\
(device *) device_child
(device *me);

INLINE_DEVICE\
(const char *) device_name
(device *me);

INLINE_DEVICE\
(const char *) device_path
(device *me);

INLINE_DEVICE\
(void *) device_data
(device *me);

INLINE_DEVICE\
(psim *) device_system
(device *me);

typedef struct _device_unit {
  int nr_cells;
  unsigned_cell cells[4]; /* unused cells are zero */
} device_unit;

INLINE_DEVICE\
(const device_unit *) device_unit_address
(device *me);

INLINE_DEVICE\
(void) device_address_to_attach_address
(device *me,
 const device_unit *address,
 int *attach_space,
 unsigned_word *attach_address);

INLINE_DEVICE\
(void) device_size_to_attach_size
(device *me,
 const device_unit *size,
 unsigned *nr_bytes);


/* Properties:

   Attached to a device are a number of properties.  Each property has
   a size and type (both of which can be queried).  A device is able
   to iterate over or query and set a properties value.

   */

/* The following are valid property types.  The property `array' is a
   for generic untyped data. */

typedef enum {
  array_property,
  boolean_property,
  ihandle_property,
  integer_property,
  reg_property,
  string_property,
} device_property_type;

typedef struct _device_property device_property;
struct _device_property {
  device *owner;
  const char *name;
  device_property_type type;
  unsigned sizeof_array;
  const void *array;
  const device_property *original;
  object_disposition disposition;
};


/* iterate through the properties attached to a device */

INLINE_DEVICE\
(const device_property *) device_next_property
(const device_property *previous);

INLINE_DEVICE\
(const device_property *) device_find_property
(device *me,
 const char *property); /* NULL for first property */


/* Manipulate the properties belonging to a given device.

   SET on the other hand will force the properties value.  The
   simulation is aborted if the property was present but of a
   conflicting type.

   FIND returns the specified properties value, aborting the
   simulation if the property is missing.  Code locating a property
   should first check its type (using device_find_property above) and
   then obtain its value using the below. */


INLINE_DEVICE\
(void) device_set_array_property
(device *me,
 const char *property,
 const void *array,
 int sizeof_array);

INLINE_DEVICE\
(const device_property *) device_find_array_property
(device *me,
 const char *property);


#if 0
INLINE_DEVICE\
(void) device_set_boolean_property
(device *me,
 const char *property,
 int bool);
#endif

INLINE_DEVICE\
(int) device_find_boolean_property
(device *me,
 const char *property);


#if 0
INLINE_DEVICE\
(void) device_set_ihandle_property
(device *me,
 const char *property,
 device_instance *ihandle);
#endif

INLINE_DEVICE\
(device_instance *) device_find_ihandle_property
(device *me,
 const char *property);


#if 0
INLINE_DEVICE\
(void) device_set_integer_property
(device *me,
 const char *property,
 signed_word integer);
#endif

INLINE_DEVICE\
(signed_word) device_find_integer_property
(device *me,
 const char *property);


#if 0
INLINE_DEVICE\
(void) device_set_string_property
(device *me,
 const char *property,
 const char *string);
#endif

INLINE_DEVICE\
(int) device_find_reg_property
(device *me,
 const char *property,
 int reg,
 device_unit *address,
 device_unit *size);

#if 0
INLINE_DEVICE\
(void) device_set_string_property
(device *me,
 const char *property,
 const char *string);
#endif

INLINE_DEVICE\
(const char *) device_find_string_property
(device *me,
 const char *property);


/* Instances:

   As with IEEE1275, a device can be opened, creating an instance.
   Instances provide more abstract interfaces to the underlying
   hardware.  For example, the instance methods for a disk may include
   code that is able to interpret file systems found on disks.  Such
   methods would there for allow the manipulation of files on the
   disks file system.  The operations would be implemented using the
   basic block I/O model provided by the disk.

   This model includes methods that faciliate the creation of device
   instance and (should a given device support it) standard operations
   on those instances.

   */

typedef struct _device_instance_callbacks device_instance_callbacks;

INLINE_DEVICE\
(device_instance *) device_create_instance_from
(device *me, /*OR*/ device_instance *parent,
 void *data,
 const char *path,
 const char *args,
 const device_instance_callbacks *callbacks);

INLINE_DEVICE\
(device_instance *) device_create_instance
(device *me,
 const char *device_specifier);

INLINE_DEVICE\
(void) device_instance_delete
(device_instance *instance);

INLINE_DEVICE\
(int) device_instance_read
(device_instance *instance,
 void *addr,
 unsigned_word len);

INLINE_DEVICE\
(int) device_instance_write
(device_instance *instance,
 const void *addr,
 unsigned_word len);

INLINE_DEVICE\
(int) device_instance_seek
(device_instance *instance,
 unsigned_word pos_hi,
 unsigned_word pos_lo);

INLINE_DEVICE\
(unsigned_cell) device_instance_call_method
(device_instance *instance,
 const char *method,
 int n_stack_args,
 unsigned_cell stack_args[/*n_stack_args*/],
 int n_stack_returns,
 unsigned_cell stack_returns[/*n_stack_returns*/]);

INLINE_DEVICE\
(device *) device_instance_device
(device_instance *instance);

INLINE_DEVICE\
(const char *) device_instance_path
(device_instance *instance);

INLINE_DEVICE\
(void *) device_instance_data
(device_instance *instance);


/* Interrupts:

   */

/* Interrupt Source

   A device drives its interrupt line using the call

   */

INLINE_DEVICE\
(void) device_interrupt_event
(device *me,
 int my_port,
 int value,
 cpu *processor,
 unsigned_word cia);

/* This interrupt event will then be propogated to any attached
   interrupt destinations.

   Any interpretation of PORT and VALUE is model dependant.  However
   as guidelines the following are recommended: PCI interrupts a-d
   correspond to lines 0-3; level sensative interrupts be requested
   with a value of one and withdrawn with a value of 0; edge sensative
   interrupts always have a value of 1, the event its self is treated
   as the interrupt.


   Interrupt Destinations

   Attached to each interrupt line of a device can be zero or more
   desitinations.  These destinations consist of a device/port pair.
   A destination is attached/detached to a device line using the
   attach and detach calls. */

INLINE_DEVICE\
(void) device_interrupt_attach
(device *me,
 int my_port,
 device *dest,
 int dest_port,
 object_disposition disposition);

INLINE_DEVICE\
(void) device_interrupt_detach
(device *me,
 int my_port,
 device *dest,
 int dest_port);

/* DESTINATION is attached (detached) to LINE of the device ME


   Interrupt conversion

   Users refer to interrupt port numbers symbolically.  For instance a
   device may refer to its `INT' signal which is internally
   represented by port 3.

   To convert to/from the symbolic and internal representation of a
   port name/number.  The following functions are available. */

INLINE_DEVICE\
(int) device_interrupt_decode
(device *me,
 const char *symbolic_name);

INLINE_DEVICE\
(int) device_interrupt_encode
(device *me,
 int port_number,
 char *buf,
 int sizeof_buf);
 

/* Hardware operations:

   */

INLINE_DEVICE\
(unsigned) device_io_read_buffer
(device *me,
 void *dest,
 int space,
 unsigned_word addr,
 unsigned nr_bytes,
 cpu *processor,
 unsigned_word cia);

INLINE_DEVICE\
(unsigned) device_io_write_buffer
(device *me,
 const void *source,
 int space,
 unsigned_word addr,
 unsigned nr_bytes,
 cpu *processor,
 unsigned_word cia);


/* Conversly, the device pci1000,1@1 my need to perform a dma transfer
   into the cpu/memory core.  Just as I/O moves towards the leaves,
   dma transfers move towards the core via the initiating devices
   parent nodes.  The root device (special) converts the DMA transfer
   into reads/writes to memory */

INLINE_DEVICE\
(unsigned) device_dma_read_buffer
(device *me,
 void *dest,
 int space,
 unsigned_word addr,
 unsigned nr_bytes);

INLINE_DEVICE\
(unsigned) device_dma_write_buffer
(device *me,
 const void *source,
 int space,
 unsigned_word addr,
 unsigned nr_bytes,
 int violate_read_only_section);

/* To avoid the need for an intermediate (bridging) node to ask each
   of its child devices in turn if an IO access is intended for them,
   parent nodes maintain a table mapping addresses directly to
   specific devices.  When a device is `connected' to its bus it
   attaches its self to its parent. */

/* Address access attributes */
typedef enum _access_type {
  access_invalid = 0,
  access_read = 1,
  access_write = 2,
  access_read_write = 3,
  access_exec = 4,
  access_read_exec = 5,
  access_write_exec = 6,
  access_read_write_exec = 7,
} access_type;

/* Address attachement types */
typedef enum _attach_type {
  attach_invalid,
  attach_raw_memory,
  attach_callback,
  /* ... */
} attach_type;

INLINE_DEVICE\
(void) device_attach_address
(device *me,
 const char *name,
 attach_type attach,
 int space,
 unsigned_word addr,
 unsigned nr_bytes,
 access_type access,
 device *who); /*callback/default*/

INLINE_DEVICE\
(void) device_detach_address
(device *me,
 const char *name,
 attach_type attach,
 int space,
 unsigned_word addr,
 unsigned nr_bytes,
 access_type access,
 device *who); /*callback/default*/

/* Utilities:

   */

/* IOCTL::

   Often devices require `out of band' operations to be performed.
   For instance a pal device may need to notify a PCI bridge device
   that an interrupt ack cycle needs to be performed on the PCI bus.
   Within PSIM such operations are performed by using the generic
   ioctl call <<device_ioctl()>>.

   */

EXTERN_DEVICE\
(int) device_ioctl
(device *me,
 cpu *processor,
 unsigned_word cia,
 ...);


/* Error reporting::

   So that errors originating from devices appear in a consistent
   format, the <<device_error()>> function can be used.  Formats and
   outputs the error message before aborting the simulation

   Devices should use this function to abort the simulation except
   when the abort reason leaves the simulation in a hazardous
   condition (for instance a failed malloc).

   */

EXTERN_DEVICE\
(void volatile) device_error
(device *me,
 const char *fmt,
 ...) __attribute__ ((format (printf, 2, 3)));

/* Tree traversal::

   The entire device tree can be traversed using the
   <<device_tree_traverse()>> function.  The traversal can be in
   either pre- or postfix order.

   */

typedef void (device_tree_traverse_function)
     (device *device,
      void *data);

INLINE_DEVICE\
(void) device_tree_traverse
(device *root,
 device_tree_traverse_function *prefix,
 device_tree_traverse_function *postfix,
 void *data);

/* Device description::

   */

INLINE_DEVICE\
(void) device_tree_print_device
(device *device,
 void *ignore_data_argument);


/* Tree lookup::

   The function <<device_tree_find_device()>> will attempt to locate
   the specified device within the tree.  If the device is not found a
   NULL device is returned.

   */

INLINE_DEVICE\
(device *) device_tree_find_device
(device *root,
 const char *path);


/* Device list or usage::

   The <<device_usage()>> function outputs a list of all the devices
   compiled into PSIM.  The verbose option will result in additional
   information being printed (for instance, the interrupt ports).

   */

INLINE_DEVICE\
(void) device_usage
(int verbose);


/* External representation:

   Both device nodes and device instances, in OpenBoot firmware have
   an external representation (phandles and ihandles) and these values
   are both stored in the device tree in property nodes and passed
   between the client program and the simulator during emulation
   calls.

   To limit the potential risk associated with trusing `data' from the
   client program, the following mapping operators `safely' convert
   between the two representations

   */

INLINE_DEVICE\
(device *) external_to_device
(device *tree_member,
 unsigned_cell phandle);

INLINE_DEVICE\
(unsigned_cell) device_to_external
(device *me);

INLINE_DEVICE\
(device_instance *) external_to_device_instance
(device *tree_member,
 unsigned_cell ihandle);

INLINE_DEVICE\
(unsigned_cell) device_instance_to_external
(device_instance *me);

#endif /* _DEVICE_H_ */
