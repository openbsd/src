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


#ifndef _PSIM_C_
#define _PSIM_C_

#include "cpu.h" /* includes psim.h */
#include "idecode.h"
#include "options.h"


#include <stdio.h>
#include <ctype.h>

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include <setjmp.h>

#ifdef HAVE_STRING_H
#include <string.h>
#else
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#endif


#include "bfd.h"


/* system structure, actual size of processor array determined at
   runtime */

struct _psim {
  event_queue *events;
  device *devices;
  mon *monitor;
  os_emul *os_emulation;
  core *memory;

  /* escape routine for inner functions */
  void *path_to_halt;
  void *path_to_restart;

  /* status from last halt */
  psim_status halt_status;

  /* the processors proper */
  int nr_cpus;
  int last_cpu; /* CPU that last (tried to) execute an instruction */
  cpu *processors[MAX_NR_PROCESSORS];
};


int current_target_byte_order;
int current_host_byte_order;
int current_environment;
int current_alignment;
int current_floating_point;
int current_model_issue = MODEL_ISSUE_IGNORE;
int current_stdio = DO_USE_STDIO;
model_enum current_model = WITH_DEFAULT_MODEL;


/* create the device tree */

INLINE_PSIM\
(device *)
psim_tree(void)
{
  device *root = device_tree_add_parsed(NULL, "core");
  device_tree_add_parsed(root, "/aliases");
  device_tree_add_parsed(root, "/options");
  device_tree_add_parsed(root, "/chosen");
  device_tree_add_parsed(root, "/packages");
  device_tree_add_parsed(root, "/cpus");
  device_tree_add_parsed(root, "/openprom");
  device_tree_add_parsed(root, "/openprom/init");
  device_tree_add_parsed(root, "/openprom/trace");
  device_tree_add_parsed(root, "/openprom/options");
  return root;
}

STATIC_INLINE_PSIM\
(char *)
find_arg(char *err_msg,
	 int *ptr_to_argp,
	 char **argv)
{
  *ptr_to_argp += 1;
  if (argv[*ptr_to_argp] == NULL)
    error(err_msg);
  return argv[*ptr_to_argp];
}

INLINE_PSIM\
(void)
psim_usage(int verbose)
{
  printf_filtered("Usage:\n");
  printf_filtered("\n");
  printf_filtered("\tpsim [ <psim-option> ... ] <image> [ <image-arg> ... ]\n");
  printf_filtered("\n");
  printf_filtered("Where\n");
  printf_filtered("\n");
  printf_filtered("\t<image>       Name of the PowerPC program to run.\n");
  if (verbose) {
  printf_filtered("\t              This can either be a PowerPC binary or\n");
  printf_filtered("\t              a text file containing a device tree\n");
  printf_filtered("\t              specification.\n");
  printf_filtered("\t              PSIM will attempt to determine from the\n");
  printf_filtered("\t              specified <image> the intended emulation\n");
  printf_filtered("\t              environment.\n");
  printf_filtered("\t              If PSIM gets it wrong, the emulation\n");
  printf_filtered("\t              environment can be specified using the\n");
  printf_filtered("\t              `-e' option (described below).\n");
  printf_filtered("\n"); }
  printf_filtered("\t<image-arg>   Argument to be passed to <image>\n");
  if (verbose) {
  printf_filtered("\t              These arguments will be passed to\n");
  printf_filtered("\t              <image> (as standard C argv, argc)\n");
  printf_filtered("\t              when <image> is started.\n");
  printf_filtered("\n"); }
  printf_filtered("\t<psim-option> See below\n");
  printf_filtered("\n");
  printf_filtered("The following are valid <psim-option>s:\n");
  printf_filtered("\n");
  printf_filtered("\t-m <model>    Specify the processor to model (604)\n");
  if (verbose) {
  printf_filtered("\t              Selects the processor to use when\n");
  printf_filtered("\t              modeling execution units.  Includes:\n");
  printf_filtered("\t              604, 603 and 603e\n");
  printf_filtered("\n"); }
  printf_filtered("\t-e <os-emul>  specify an OS or platform to model\n");
  if (verbose) {
  printf_filtered("\t              Can be any of the following:\n");
  printf_filtered("\t              bug - OEA + MOTO BUG ROM calls\n");
  printf_filtered("\t              netbsd - UEA + NetBSD system calls\n");
  printf_filtered("\t              solaris - UEA + Solaris system calls\n");
  printf_filtered("\t              linux - UEA + Linux system calls\n");
  printf_filtered("\t              chirp - OEA + a few OpenBoot calls\n");
  printf_filtered("\n"); }
  printf_filtered("\t-i            Print instruction counting statistics\n");
  if (verbose) { printf_filtered("\n"); }
  printf_filtered("\t-I            Print execution unit statistics\n");
  if (verbose) { printf_filtered("\n"); }
  printf_filtered("\t-r <size>     Set RAM size in bytes (OEA environments)\n");
  if (verbose) { printf_filtered("\n"); }
  printf_filtered("\t-t [!]<trace> Enable (disable) <trace> option\n");
  if (verbose) { printf_filtered("\n"); }
  printf_filtered("\t-o <spec>     add device <spec> to the device tree\n");
  if (verbose) { printf_filtered("\n"); }
  printf_filtered("\t-h -? -H      give more detailed usage\n");
  if (verbose) { printf_filtered("\n"); }
  printf_filtered("\n");
  trace_usage(verbose);
  device_usage(verbose);
  if (verbose > 1) {
    printf_filtered("\n");
    print_options();
  }
  error("");
}

INLINE_PSIM\
(char **)
psim_options(device *root,
	     char **argv)
{
  device *current = root;
  int argp;
  if (argv == NULL)
    return NULL;
  argp = 0;
  while (argv[argp] != NULL && argv[argp][0] == '-') {
    char *p = argv[argp] + 1;
    char *param;
    while (*p != '\0') {
      switch (*p) {
      default:
	psim_usage(0);
	error ("");
	break;
      case 'e':
	param = find_arg("Missing <emul> option for -e\n", &argp, argv);
	device_tree_add_parsed(root, "/openprom/options/os-emul %s", param);
	break;
      case 'h':
      case '?':
	psim_usage(1);
	break;
      case 'H':
	psim_usage(2);
	break;
      case 'i':
	device_tree_add_parsed(root, "/openprom/trace/print-info 1");
	break;
      case 'I':
	device_tree_add_parsed(root, "/openprom/trace/print-info 2");
	device_tree_add_parsed(root, "/openprom/options/model-issue %d",
			       MODEL_ISSUE_PROCESS);
	break;
      case 'm':
	param = find_arg("Missing <model> option for -m\n", &argp, argv);
	device_tree_add_parsed(root, "/openprom/options/model \"%s", param);
	break;
      case 'o':
	param = find_arg("Missing <device> option for -o\n", &argp, argv);
	current = device_tree_add_parsed(current, "%s", param);
	break;
      case 'r':
	param = find_arg("Missing <ram-size> option for -r\n", &argp, argv);
	device_tree_add_parsed(root, "/openprom/options/oea-memory-size %s",
			       param);
	break;
      case 't':
	param = find_arg("Missing <trace> option for -t\n", &argp, argv);
	if (param[0] == '!')
	  device_tree_add_parsed(root, "/openprom/trace/%s 0", param+1);
	else
	  device_tree_add_parsed(root, "/openprom/trace/%s 1", param);
	break;
      }
      p += 1;
    }
    argp += 1;
  }
  /* force the trace node to (re)process its options */
  device_ioctl(device_tree_find_device(root, "/openprom/trace"), NULL, 0);

  /* return where the options end */
  return argv + argp;
}


/* create the simulator proper from the device tree and executable */

INLINE_PSIM\
(psim *)
psim_create(const char *file_name,
	    device *root)
{
  int cpu_nr;
  const char *env;
  psim *system;
  os_emul *os_emulation;
  int nr_cpus;

  /* given this partially populated device tree, os_emul_create() uses
     it and file_name to determine the selected emulation and hence
     further populate the tree with any other required nodes. */

  os_emulation = os_emul_create(file_name, root);
  if (os_emulation == NULL)
    error("psim: either file %s was not reconized or unreconized or unknown os-emulation type\n", file_name);

  /* fill in the missing real number of CPU's */
  nr_cpus = device_find_integer_property(root, "/openprom/options/smp");
  if (MAX_NR_PROCESSORS < nr_cpus)
    error("target and configured number of cpus conflict\n");

  /* fill in the missing TARGET BYTE ORDER information */
  current_target_byte_order
    = (device_find_boolean_property(root, "/options/little-endian?")
       ? LITTLE_ENDIAN
       : BIG_ENDIAN);
  if (CURRENT_TARGET_BYTE_ORDER != current_target_byte_order)
    error("target and configured byte order conflict\n");

  /* fill in the missing HOST BYTE ORDER information */
  current_host_byte_order = (current_host_byte_order = 1,
			     (*(char*)(&current_host_byte_order)
			      ? LITTLE_ENDIAN
			      : BIG_ENDIAN));
  if (CURRENT_HOST_BYTE_ORDER != current_host_byte_order)
    error("host and configured byte order conflict\n");

  /* fill in the missing OEA/VEA information */
  env = device_find_string_property(root, "/openprom/options/env");
  current_environment = ((strcmp(env, "user") == 0
			  || strcmp(env, "uea") == 0)
			 ? USER_ENVIRONMENT
			 : (strcmp(env, "virtual") == 0
			    || strcmp(env, "vea") == 0)
			 ? VIRTUAL_ENVIRONMENT
			 : (strcmp(env, "operating") == 0
			    || strcmp(env, "oea") == 0)
			 ? OPERATING_ENVIRONMENT
			 : 0);
  if (current_environment == 0)
    error("unreconized /options env property\n");
  if (CURRENT_ENVIRONMENT != current_environment)
    error("target and configured environment conflict\n");

  /* fill in the missing ALLIGNMENT information */
  current_alignment
    = (device_find_boolean_property(root, "/openprom/options/strict-alignment?")
       ? STRICT_ALIGNMENT
       : NONSTRICT_ALIGNMENT);
  if (CURRENT_ALIGNMENT != current_alignment)
    error("target and configured alignment conflict\n");

  /* fill in the missing FLOATING POINT information */
  current_floating_point
    = (device_find_boolean_property(root, "/openprom/options/floating-point?")
       ? HARD_FLOATING_POINT
       : SOFT_FLOATING_POINT);
  if (CURRENT_FLOATING_POINT != current_floating_point)
    error("target and configured floating-point conflict\n");

  /* fill in the missing STDIO information */
  current_stdio
    = (device_find_boolean_property(root, "/openprom/options/use-stdio?")
       ? DO_USE_STDIO
       : DONT_USE_STDIO);
  if (CURRENT_STDIO != current_stdio)
    error("target and configured stdio interface conflict\n");

  /* sort out the level of detail for issue modeling */
  current_model_issue
    = device_find_integer_property(root, "/openprom/options/model-issue");
  if (CURRENT_MODEL_ISSUE != current_model_issue)
    error("target and configured model-issue conflict\n");

  /* sort out our model architecture - wrong.

     FIXME: this should be obtaining the required information from the
     device tree via the "/chosen" property "cpu" which is an instance
     (ihandle) for the only executing processor. By converting that
     ihandle into the corresponding cpu's phandle and then querying
     the "name" property, the cpu type can be determined. Ok? */

  model_set(device_find_string_property(root, "/openprom/options/model"));

  /* create things */
  system = ZALLOC(psim);
  system->events = event_queue_create();
  system->memory = core_from_device(root);
  system->monitor = mon_create();
  system->nr_cpus = nr_cpus;
  system->os_emulation = os_emulation;
  system->devices = root;

  /* now all the processors attaching to each their per-cpu information */
  for (cpu_nr = 0; cpu_nr < MAX_NR_PROCESSORS; cpu_nr++) {
    system->processors[cpu_nr] = cpu_create(system,
					    system->memory,
					    mon_cpu(system->monitor,
						    cpu_nr),
					    system->os_emulation,
					    cpu_nr);
  }

  /* dump out the contents of the device tree */
  if (ppc_trace[trace_print_device_tree] || ppc_trace[trace_dump_device_tree])
    device_tree_traverse(root, device_tree_print_device, NULL, NULL);
  if (ppc_trace[trace_dump_device_tree])
    error("");

  return system;
}


/* allow the simulation to stop/restart abnormaly */

INLINE_PSIM\
(void)
psim_set_halt_and_restart(psim *system,
			  void *halt_jmp_buf,
			  void *restart_jmp_buf)
{
  system->path_to_halt = halt_jmp_buf;
  system->path_to_restart = restart_jmp_buf;
}

INLINE_PSIM\
(void)
psim_clear_halt_and_restart(psim *system)
{
  system->path_to_halt = NULL;
  system->path_to_restart = NULL;
}

INLINE_PSIM\
(void)
psim_restart(psim *system,
	     int current_cpu)
{
  system->last_cpu = current_cpu;
  longjmp(*(jmp_buf*)(system->path_to_restart), current_cpu + 1);
}


INLINE_PSIM\
(void)
psim_halt(psim *system,
	  int current_cpu,
	  stop_reason reason,
	  int signal)
{
  ASSERT(current_cpu >= 0 && current_cpu < system->nr_cpus);
  system->last_cpu = current_cpu;
  system->halt_status.cpu_nr = current_cpu;
  system->halt_status.reason = reason;
  system->halt_status.signal = signal;
  system->halt_status.program_counter =
    cpu_get_program_counter(system->processors[current_cpu]);
  longjmp(*(jmp_buf*)(system->path_to_halt), current_cpu + 1);
}

INLINE_PSIM\
(int)
psim_last_cpu(psim *system)
{
  return system->last_cpu;
}

INLINE_PSIM\
(int)
psim_nr_cpus(psim *system)
{
  return system->nr_cpus;
}

INLINE_PSIM\
(psim_status)
psim_get_status(psim *system)
{
  return system->halt_status;
}


INLINE_PSIM\
(cpu *)
psim_cpu(psim *system,
	 int cpu_nr)
{
  if (cpu_nr < 0 || cpu_nr >= system->nr_cpus)
    return NULL;
  else
    return system->processors[cpu_nr];
}


INLINE_PSIM\
(device *)
psim_device(psim *system,
	    const char *path)
{
  return device_tree_find_device(system->devices, path);
}

INLINE_PSIM\
(event_queue *)
psim_event_queue(psim *system)
{
  return system->events;
}



INLINE_PSIM\
(void)
psim_init(psim *system)
{
  int cpu_nr;

  /* scrub the monitor */
  mon_init(system->monitor, system->nr_cpus);

  /* trash any pending events */
  event_queue_init(system->events);

  /* scrub all the cpus */
  for (cpu_nr = 0; cpu_nr < system->nr_cpus; cpu_nr++)
    cpu_init(system->processors[cpu_nr]);

  /* init all the devices (which updates the cpus) */
  device_tree_init(system->devices, system);

  /* and the emulation (which needs an initialized device tree) */
  os_emul_init(system->os_emulation, system->nr_cpus);

  /* now sync each cpu against the initialized state of its registers */
  for (cpu_nr = 0; cpu_nr < system->nr_cpus; cpu_nr++) {
    cpu_synchronize_context(system->processors[cpu_nr]);
    cpu_page_tlb_invalidate_all(system->processors[cpu_nr]);
  }

  /* force loop to start with first cpu (after processing events) */
  system->last_cpu = system->nr_cpus - 1;
}

INLINE_PSIM\
(void)
psim_stack(psim *system,
	   char **argv,
	   char **envp)
{
  /* pass the stack device the argv/envp and let it work out what to
     do with it */
  device *stack_device = device_tree_find_device(system->devices,
						 "/openprom/init/stack");
  if (stack_device != (device*)0) {
    unsigned_word stack_pointer;
    psim_read_register(system, 0, &stack_pointer, "sp", cooked_transfer);
    device_ioctl(stack_device,
		 NULL, /*cpu*/
		 0, /*cia*/
		 stack_pointer,
		 argv,
		 envp);
  }
}



/* SIMULATE INSTRUCTIONS, various different ways of achieving the same
   thing */

INLINE_PSIM\
(void)
psim_step(psim *system)
{
  volatile int keep_running = 0;
  idecode_run_until_stop(system, &keep_running,
			 system->events, system->processors, system->nr_cpus);
}

INLINE_PSIM\
(void)
psim_run(psim *system)
{
  idecode_run(system,
	      system->events, system->processors, system->nr_cpus);
}

INLINE_PSIM\
(void)
psim_run_until_stop(psim *system,
		    volatile int *keep_running)
{
  idecode_run_until_stop(system, keep_running,
			 system->events, system->processors, system->nr_cpus);
}



/* storage manipulation functions */

INLINE_PSIM\
(void)
psim_read_register(psim *system,
		   int which_cpu,
		   void *buf,
		   const char reg[],
		   transfer_mode mode)
{
  register_descriptions description;
  char cooked_buf[sizeof(unsigned_8)];
  cpu *processor;

  /* find our processor */
  if (which_cpu == MAX_NR_PROCESSORS)
    which_cpu = system->last_cpu;
  if (which_cpu < 0 || which_cpu >= system->nr_cpus)
    error("psim_read_register() - invalid processor %d\n", which_cpu);
  processor = system->processors[which_cpu];

  /* find the register description */
  description = register_description(reg);
  if (description.type == reg_invalid)
    error("psim_read_register() invalid register name `%s'\n", reg);

  /* get the cooked value */
  switch (description.type) {

  case reg_gpr:
    *(gpreg*)cooked_buf = cpu_registers(processor)->gpr[description.index];
    break;

  case reg_spr:
    *(spreg*)cooked_buf = cpu_registers(processor)->spr[description.index];
    break;
    
  case reg_sr:
    *(sreg*)cooked_buf = cpu_registers(processor)->sr[description.index];
    break;

  case reg_fpr:
    *(fpreg*)cooked_buf = cpu_registers(processor)->fpr[description.index];
    break;

  case reg_pc:
    *(unsigned_word*)cooked_buf = cpu_get_program_counter(processor);
    break;

  case reg_cr:
    *(creg*)cooked_buf = cpu_registers(processor)->cr;
    break;

  case reg_msr:
    *(msreg*)cooked_buf = cpu_registers(processor)->msr;
    break;

  case reg_insns:
    *(unsigned_word*)cooked_buf = mon_get_number_of_insns(system->monitor,
							  which_cpu);
    break;

  case reg_stalls:
    if (cpu_model(processor) == NULL)
      error("$stalls only valid if processor unit model enabled (-I)\n");
    *(unsigned_word*)cooked_buf = model_get_number_of_stalls(cpu_model(processor));
    break;

  case reg_cycles:
    if (cpu_model(processor) == NULL)
      error("$cycles only valid if processor unit model enabled (-I)\n");
    *(unsigned_word*)cooked_buf = model_get_number_of_cycles(cpu_model(processor));
    break;

  default:
    printf_filtered("psim_read_register(processor=0x%lx,buf=0x%lx,reg=%s) %s\n",
		    (unsigned long)processor, (unsigned long)buf, reg,
		    "read of this register unimplemented");
    break;

  }

  /* the PSIM internal values are in host order.  To fetch raw data,
     they need to be converted into target order and then returned */
  if (mode == raw_transfer) {
    /* FIXME - assumes that all registers are simple integers */
    switch (description.size) {
    case 1: 
      *(unsigned_1*)buf = H2T_1(*(unsigned_1*)cooked_buf);
      break;
    case 2:
      *(unsigned_2*)buf = H2T_2(*(unsigned_2*)cooked_buf);
      break;
    case 4:
      *(unsigned_4*)buf = H2T_4(*(unsigned_4*)cooked_buf);
      break;
    case 8:
      *(unsigned_8*)buf = H2T_8(*(unsigned_8*)cooked_buf);
      break;
    }
  }
  else {
    memcpy(buf/*dest*/, cooked_buf/*src*/, description.size);
  }

}



INLINE_PSIM\
(void)
psim_write_register(psim *system,
		    int which_cpu,
		    const void *buf,
		    const char reg[],
		    transfer_mode mode)
{
  cpu *processor;
  register_descriptions description;
  char cooked_buf[sizeof(unsigned_8)];

  /* find our processor */
  if (which_cpu == MAX_NR_PROCESSORS)
    which_cpu = system->last_cpu;
  if (which_cpu == -1) {
    int i;
    for (i = 0; i < system->nr_cpus; i++)
      psim_write_register(system, i, buf, reg, mode);
    return;
  }
  else if (which_cpu < 0 || which_cpu >= system->nr_cpus) {
    error("psim_read_register() - invalid processor %d\n", which_cpu);
  }

  processor = system->processors[which_cpu];

  /* find the description of the register */
  description = register_description(reg);
  if (description.type == reg_invalid)
    error("psim_write_register() invalid register name %s\n", reg);

  /* If the data is comming in raw (target order), need to cook it
     into host order before putting it into PSIM's internal structures */
  if (mode == raw_transfer) {
    switch (description.size) {
    case 1: 
      *(unsigned_1*)cooked_buf = T2H_1(*(unsigned_1*)buf);
      break;
    case 2:
      *(unsigned_2*)cooked_buf = T2H_2(*(unsigned_2*)buf);
      break;
    case 4:
      *(unsigned_4*)cooked_buf = T2H_4(*(unsigned_4*)buf);
      break;
    case 8:
      *(unsigned_8*)cooked_buf = T2H_8(*(unsigned_8*)buf);
      break;
    }
  }
  else {
    memcpy(cooked_buf/*dest*/, buf/*src*/, description.size);
  }

  /* put the cooked value into the register */
  switch (description.type) {

  case reg_gpr:
    cpu_registers(processor)->gpr[description.index] = *(gpreg*)cooked_buf;
    break;

  case reg_fpr:
    cpu_registers(processor)->fpr[description.index] = *(fpreg*)cooked_buf;
    break;

  case reg_pc:
    cpu_set_program_counter(processor, *(unsigned_word*)cooked_buf);
    break;

  case reg_spr:
    cpu_registers(processor)->spr[description.index] = *(spreg*)cooked_buf;
    break;

  case reg_sr:
    cpu_registers(processor)->sr[description.index] = *(sreg*)cooked_buf;
    break;

  case reg_cr:
    cpu_registers(processor)->cr = *(creg*)cooked_buf;
    break;

  case reg_msr:
    cpu_registers(processor)->msr = *(msreg*)cooked_buf;
    break;

  default:
    printf_filtered("psim_write_register(processor=0x%lx,cooked_buf=0x%lx,reg=%s) %s\n",
		    (unsigned long)processor, (unsigned long)cooked_buf, reg,
		    "read of this register unimplemented");
    break;

  }

}



INLINE_PSIM\
(unsigned)
psim_read_memory(psim *system,
		 int which_cpu,
		 void *buffer,
		 unsigned_word vaddr,
		 unsigned nr_bytes)
{
  cpu *processor;
  if (which_cpu == MAX_NR_PROCESSORS)
    which_cpu = system->last_cpu;
  if (which_cpu < 0 || which_cpu >= system->nr_cpus)
    error("psim_read_memory() invalid cpu\n");
  processor = system->processors[which_cpu];
  return vm_data_map_read_buffer(cpu_data_map(processor),
				 buffer, vaddr, nr_bytes);
}


INLINE_PSIM\
(unsigned)
psim_write_memory(psim *system,
		  int which_cpu,
		  const void *buffer,
		  unsigned_word vaddr,
		  unsigned nr_bytes,
		  int violate_read_only_section)
{
  cpu *processor;
  if (which_cpu == MAX_NR_PROCESSORS)
    which_cpu = system->last_cpu;
  if (which_cpu < 0 || which_cpu >= system->nr_cpus)
    error("psim_read_memory() invalid cpu\n");
  processor = system->processors[which_cpu];
  return vm_data_map_write_buffer(cpu_data_map(processor),
				  buffer, vaddr, nr_bytes, 1);
}


INLINE_PSIM\
(void)
psim_print_info(psim *system,
		int verbose)
{
  mon_print_info(system, system->monitor, verbose);
}


/* Merge a device tree and a device file. */

INLINE_PSIM\
(void)
psim_merge_device_file(device *root,
		       const char *file_name)
{
  FILE *description = fopen(file_name, "r");
  int line_nr = 0;
  char device_path[1000];
  device *current = root;
  while (fgets(device_path, sizeof(device_path), description)) {
    /* check all of line was read */
    if (strchr(device_path, '\n') == NULL) {
      fclose(description);
      error("create_filed_device_tree() line %d to long: %s\n",
	    line_nr, device_path);
    }
    line_nr++;
    /* parse this line */
    current = device_tree_add_parsed(current, "%s", device_path);
  }
  fclose(description);
}


#endif /* _PSIM_C_ */
