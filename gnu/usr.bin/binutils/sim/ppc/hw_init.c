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


#ifndef _HW_INIT_C_
#define _HW_INIT_C_

#include "device_table.h"
#include "bfd.h"
#include "psim.h"


/* DMA a file into memory */
static int
dma_file(device *me,
	 const char *file_name,
	 unsigned_word addr)
{
  int count;
  int inc;
  FILE *image;
  char buf[1024];

  /* get it open */
  image = fopen(file_name, "r");
  if (image == NULL)
    return -1;

  /* read it in slowly */
  count = 0;
  while (1) {
    inc = fread(buf, 1, sizeof(buf), image);
    if (feof(image) || ferror(image))
      break;
    if (device_dma_write_buffer(device_parent(me),
				buf,
				0 /*address-space*/,
				addr+count,
				inc /*nr-bytes*/,
				1 /*violate ro*/) != inc) {
      fclose(image);
      return -1;
    }
    count += inc;
  }

  /* close down again */
  fclose(image);

  return count;
}


/* DEVICE

   file - load a file into memory

   DESCRIPTION

   Loads the entire contents of <file-name> into memory at starting at
   <real-address>.  Assumes that memory exists for the load.

   PROPERTIES

   file-name = <string>

   Name of the file to be loaded into memory

   real-address = <integer>

   Real address at which the file is to be loaded */

static void
hw_file_init_data_callback(device *me)
{
  int count;
  const char *file_name = device_find_string_property(me, "file-name");
  unsigned_word addr = device_find_integer_property(me, "real-address");
  /* load the file */
  count = dma_file(me, file_name, addr);
  if (count < 0)
    device_error(me, "Problem loading file %s\n", file_name);
}


static device_callbacks const hw_file_callbacks = {
  { NULL, hw_file_init_data_callback, },
  { NULL, }, /* address */
  { NULL, }, /* IO */
  { NULL, }, /* DMA */
  { NULL, }, /* interrupt */
  { NULL, }, /* unit */
};


/* DEVICE

   data - initialize a memory location

   DESCRIPTION

   A word sized quantity of data is written into memory, using the
   targets byte ordering, at the specified memory location.

   In the future this device will be extended so that it supports
   initialization using other data types (eg array, ...)

   PROPERTIES

   data = <int>

   Integer value to be loaded into memory

   real-address = <integer>

   Start address for the data. */


static void
hw_data_init_data_callback(device *me)
{
  unsigned_word addr = device_find_integer_property(me, "real-address");
  const device_property *data = device_find_property(me, "data");
  if (data == NULL)
    device_error(me, "missing property <data>\n");
  switch (data->type) {
  case integer_property:
    {
      unsigned32 buf = device_find_integer_property(me, "data");
      H2T(buf);
      if (device_dma_write_buffer(device_parent(me),
				  &buf,
				  0 /*address-space*/,
				  addr,
				  sizeof(buf), /*nr-bytes*/
				  1 /*violate ro*/) != sizeof(buf))
	device_error(me, "Problem storing integer 0x%x at 0x%lx\n",
		     (unsigned)buf, (unsigned long)addr);
    }
    break;
  default:
    device_error(me, "Write of this data is not yet implemented\n");
    break;
  }
}


static device_callbacks const hw_data_callbacks = {
  { NULL, hw_data_init_data_callback, },
  { NULL, }, /* address */
  { NULL, }, /* IO */
  { NULL, }, /* DMA */
  { NULL, }, /* interrupt */
  { NULL, }, /* unit */
};


/* DEVICE

   load-binary - load binary segments into memory

   DESCRIPTION

   Each loadable segment of the specified binary is loaded into memory
   at its required address.  It is assumed that the memory at those
   addresses already exists.

   This device is normally used to load an executable into memory as
   part of real mode simulation.

   PROPERTIES

   file-name = <string>

   Name of the binary to be loaded.

   */

/* DEVICE

   map-binary - map the binary into the users address space

   DESCRIPTION
   
   Similar to load-binary except that memory for each segment is
   created before the corresponding data for the segment is loaded.

   This device is normally used to load an executable into a user mode
   simulation.

   PROPERTIES

   file-name = <string>

   Name of the binary to be loaded.

   */

static void
update_for_binary_section(bfd *abfd,
			  asection *the_section,
			  PTR obj)
{
  unsigned_word section_vma;
  unsigned_word section_size;
  access_type access;
  device *me = (device*)obj;

  /* skip the section if no memory to allocate */
  if (! (bfd_get_section_flags(abfd, the_section) & SEC_ALLOC))
    return;

  /* check/ignore any sections of size zero */
  section_size = bfd_get_section_size_before_reloc(the_section);
  if (section_size == 0)
    return;

  /* find where it is to go */
  section_vma = bfd_get_section_vma(abfd, the_section);

  DTRACE(binary,
	 ("name=%-7s, vma=0x%.8lx, size=%6ld, flags=%3lx(%s%s%s%s%s )\n",
	  bfd_get_section_name(abfd, the_section),
	  (long)section_vma,
	  (long)section_size,
	  (long)bfd_get_section_flags(abfd, the_section),
	  bfd_get_section_flags(abfd, the_section) & SEC_LOAD ? " LOAD" : "",
	  bfd_get_section_flags(abfd, the_section) & SEC_CODE ? " CODE" : "",
	  bfd_get_section_flags(abfd, the_section) & SEC_DATA ? " DATA" : "",
	  bfd_get_section_flags(abfd, the_section) & SEC_ALLOC ? " ALLOC" : "",
	  bfd_get_section_flags(abfd, the_section) & SEC_READONLY ? " READONLY" : ""
	  ));

  /* If there is an .interp section, it means it needs a shared library interpreter.  */
  if (strcmp(".interp", bfd_get_section_name(abfd, the_section)) == 0)
    error("Shared libraries are not yet supported.\n");

  /* determine the devices access */
  access = access_read;
  if (bfd_get_section_flags(abfd, the_section) & SEC_CODE)
    access |= access_exec;
  if (!(bfd_get_section_flags(abfd, the_section) & SEC_READONLY))
    access |= access_write;

  /* if a map, pass up a request to create the memory in core */
  if (strncmp(device_name(me), "map-binary", strlen("map-binary")) == 0)
    device_attach_address(device_parent(me),
			  device_name(me),
			  attach_raw_memory,
			  0 /*address space*/,
			  section_vma,
			  section_size,
			  access,
			  me);

  /* if a load dma in the required data */
  if (bfd_get_section_flags(abfd, the_section) & SEC_LOAD) {
    void *section_init = zalloc(section_size);
    if (!bfd_get_section_contents(abfd,
				  the_section,
				  section_init, 0,
				  section_size)) {
      bfd_perror("binary");
      device_error(me, "load of data failed");
      return;
    }
    if (device_dma_write_buffer(device_parent(me),
				section_init,
				0 /*space*/,
				section_vma,
				section_size,
				1 /*violate_read_only*/)
	!= section_size)
      device_error(me, "broken transfer\n");
    zfree(section_init); /* only free if load */
  }
}

static void
hw_binary_init_data_callback(device *me)
{
  /* get the file name */
  const char *file_name = device_find_string_property(me, "file-name");
  bfd *image;

  /* open the file */
  image = bfd_openr(file_name, NULL);
  if (image == NULL) {
    bfd_perror("binary");
    device_error(me, "Failed to open file %s\n", file_name);
  }

  /* check it is valid */
  if (!bfd_check_format(image, bfd_object)) {
    bfd_close(image);
    device_error(me, "The file %s has an invalid binary format\n", file_name);
  }

  /* and the data sections */
  bfd_map_over_sections(image,
			update_for_binary_section,
			(PTR)me);

  bfd_close(image);
}


static device_callbacks const hw_binary_callbacks = {
  { NULL, hw_binary_init_data_callback, },
  { NULL, }, /* address */
  { NULL, }, /* IO */
  { NULL, }, /* DMA */
  { NULL, }, /* interrupt */
  { NULL, }, /* unit */
};


/* DEVICE

   stack - create an initial stack frame in memory

   DESCRIPTION

   Creates a stack frame of the specified type in memory.

   Due to the startup sequence gdb uses when commencing a simulation,
   it is not possible for the data to be placed on the stack to be
   specified as part of the device tree.  Instead the arguments to be
   pushed onto the stack are specified using an IOCTL call.

   The IOCTL takes the additional arguments:

   | unsigned_word stack_end -- where the stack should come down from
   | char **argv -- ...
   | char **envp -- ...

   PROPERTIES

   stack-type = <string>

   The form of the stack frame that is to be created.

   */

static int
sizeof_argument_strings(char **arg)
{
  int sizeof_strings = 0;

  /* robust */
  if (arg == NULL)
    return 0;

  /* add up all the string sizes (padding as we go) */
  for (; *arg != NULL; arg++) {
    int len = strlen(*arg) + 1;
    sizeof_strings += ALIGN_8(len);
  }

  return sizeof_strings;
}

static int
number_of_arguments(char **arg)
{
  int nr;
  if (arg == NULL)
    return 0;
  for (nr = 0; *arg != NULL; arg++, nr++);
  return nr;
}

static int
sizeof_arguments(char **arg)
{
  return ALIGN_8((number_of_arguments(arg) + 1) * sizeof(unsigned_word));
}

static void
write_stack_arguments(device *me,
		      char **arg,
		      unsigned_word start_block,
		      unsigned_word end_block,
		      unsigned_word start_arg,
		      unsigned_word end_arg)
{
  DTRACE(stack,
	("write_stack_arguments(device=%s, arg=0x%lx, start_block=0x%lx, end_block=0x%lx, start_arg=0x%lx, end_arg=0x%lx)\n",
	 device_name(me), (long)arg, (long)start_block, (long)end_block, (long)start_arg, (long)end_arg));
  if (arg == NULL)
    device_error(me, "Attempt to write a null array onto the stack\n");
  /* only copy in arguments, memory is already zero */
  for (; *arg != NULL; arg++) {
    int len = strlen(*arg)+1;
    unsigned_word target_start_block;
    DTRACE(stack,
	  ("write_stack_arguments() write %s=%s at %s=0x%lx %s=0x%lx %s=0x%lx\n",
	   "**arg", *arg, "start_block", (long)start_block,
	   "len", (long)len, "start_arg", (long)start_arg));
    if (psim_write_memory(device_system(me), 0, *arg,
			  start_block, len,
			  0/*violate_readonly*/) != len)
      device_error(me, "Write of **arg (%s) at 0x%lx of stack failed\n",
		   *arg, (unsigned long)start_block);
    target_start_block = H2T_word(start_block);
    if (psim_write_memory(device_system(me), 0, &target_start_block,
			  start_arg, sizeof(target_start_block),
			  0) != sizeof(target_start_block))
      device_error(me, "Write of *arg onto stack failed\n");
    start_block += ALIGN_8(len);
    start_arg += sizeof(start_block);
  }
  start_arg += sizeof(start_block); /*the null at the end*/
  if (start_block != end_block
      || ALIGN_8(start_arg) != end_arg)
    device_error(me, "Probable corrpution of stack arguments\n");
  DTRACE(stack, ("write_stack_arguments() = void\n"));
}

static void
create_ppc_elf_stack_frame(device *me,
			   unsigned_word bottom_of_stack,
			   char **argv,
			   char **envp)
{
  /* fixme - this is over aligned */

  /* information block */
  const unsigned sizeof_envp_block = sizeof_argument_strings(envp);
  const unsigned_word start_envp_block = bottom_of_stack - sizeof_envp_block;
  const unsigned sizeof_argv_block = sizeof_argument_strings(argv);
  const unsigned_word start_argv_block = start_envp_block - sizeof_argv_block;

  /* auxiliary vector - contains only one entry */
  const unsigned sizeof_aux_entry = 2*sizeof(unsigned_word); /* magic */
  const unsigned_word start_aux = start_argv_block - ALIGN_8(sizeof_aux_entry);

  /* environment points (including null sentinal) */
  const unsigned sizeof_envp = sizeof_arguments(envp);
  const unsigned_word start_envp = start_aux - sizeof_envp;

  /* argument pointers (including null sentinal) */
  const int argc = number_of_arguments(argv);
  const unsigned sizeof_argv = sizeof_arguments(argv);
  const unsigned_word start_argv = start_envp - sizeof_argv;

  /* link register save address - alligned to a 16byte boundary */
  const unsigned_word top_of_stack = ((start_argv
				       - 2 * sizeof(unsigned_word))
				      & ~0xf);

  /* install arguments on stack */
  write_stack_arguments(me, envp,
			start_envp_block, bottom_of_stack,
			start_envp, start_aux);
  write_stack_arguments(me, argv,
			start_argv_block, start_envp_block,
			start_argv, start_envp);

  /* set up the registers */
  psim_write_register(device_system(me), -1,
		      &top_of_stack, "sp", cooked_transfer);
  psim_write_register(device_system(me), -1,
		      &argc, "r3", cooked_transfer);
  psim_write_register(device_system(me), -1,
		      &start_argv, "r4", cooked_transfer);
  psim_write_register(device_system(me), -1,
		      &start_envp, "r5", cooked_transfer);
  psim_write_register(device_system(me), -1,
		      &start_aux, "r6", cooked_transfer);
}

static void
create_ppc_aix_stack_frame(device *me,
			   unsigned_word bottom_of_stack,
			   char **argv,
			   char **envp)
{
  unsigned_word core_envp;
  unsigned_word core_argv;
  unsigned_word core_argc;
  unsigned_word core_aux;
  unsigned_word top_of_stack;

  /* cheat - create an elf stack frame */
  create_ppc_elf_stack_frame(me, bottom_of_stack, argv, envp);
  
  /* extract argument addresses from registers */
  psim_read_register(device_system(me), 0,
		     &top_of_stack, "r1", cooked_transfer);
  psim_read_register(device_system(me), 0,
		     &core_argc, "r3", cooked_transfer);
  psim_read_register(device_system(me), 0,
		     &core_argv, "r4", cooked_transfer);
  psim_read_register(device_system(me), 0,
		     &core_envp, "r5", cooked_transfer);
  psim_read_register(device_system(me), 0,
		     &core_aux, "r6", cooked_transfer);

  /* extract arguments from registers */
  device_error(me, "Unfinished procedure create_ppc_aix_stack_frame\n");
}



static int
hw_stack_ioctl_callback(device *me,
			cpu *processor,
			unsigned_word cia,
			va_list ap)
{
  unsigned_word stack_pointer;
  const char *stack_type;
  char **argv;
  char **envp;
  stack_pointer = va_arg(ap, unsigned_word);
  argv = va_arg(ap, char **);
  envp = va_arg(ap, char **);
  DTRACE(stack,
	 ("stack_ioctl_callback(me=0x%lx:%s processor=0x%lx cia=0x%lx argv=0x%lx envp=0x%lx)\n",
	  (long)me, device_name(me), (long)processor, (long)cia, (long)argv, (long)envp));
  stack_type = device_find_string_property(me, "stack-type");
  if (strcmp(stack_type, "ppc-elf") == 0)
    create_ppc_elf_stack_frame(me, stack_pointer, argv, envp);
  else if (strcmp(stack_type, "ppc-xcoff") == 0)
    create_ppc_aix_stack_frame(me, stack_pointer, argv, envp);
  else if (strcmp(stack_type, "none") != 0)
    device_error(me, "Unknown initial stack frame type %s\n", stack_type);
  DTRACE(stack, 
	 ("stack_ioctl_callback() = void\n"));
  return 0;
}

static device_callbacks const hw_stack_callbacks = {
  { NULL, },
  { NULL, }, /* address */
  { NULL, }, /* IO */
  { NULL, }, /* DMA */
  { NULL, }, /* interrupt */
  { NULL, }, /* unit */
  NULL, /* instance */
  hw_stack_ioctl_callback,
};

const device_descriptor hw_init_device_descriptor[] = {
  { "file", NULL, &hw_file_callbacks },
  { "data", NULL, &hw_data_callbacks },
  { "load-binary", NULL, &hw_binary_callbacks },
  { "map-binary", NULL, &hw_binary_callbacks },
  { "stack", NULL, &hw_stack_callbacks },
  { NULL },
};

#endif _HW_INIT_C_
