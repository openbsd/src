/* *INDENT-OFF* */ /* THIS FILE IS GENERATED */

/* Dynamic architecture support for GDB, the GNU debugger.

   Copyright 1998, 1999, 2000, 2001, 2002, 2003, 2004 Free
   Software Foundation, Inc.

   This file is part of GDB.

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
   Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

/* This file was created with the aid of ``gdbarch.sh''.

   The Bourne shell script ``gdbarch.sh'' creates the files
   ``new-gdbarch.c'' and ``new-gdbarch.h and then compares them
   against the existing ``gdbarch.[hc]''.  Any differences found
   being reported.

   If editing this file, please also run gdbarch.sh and merge any
   changes into that script. Conversely, when making sweeping changes
   to this file, modifying gdbarch.sh and using its output may prove
   easier. */


#include "defs.h"
#include "arch-utils.h"

#include "gdbcmd.h"
#include "inferior.h" /* enum CALL_DUMMY_LOCATION et.al. */
#include "symcat.h"

#include "floatformat.h"

#include "gdb_assert.h"
#include "gdb_string.h"
#include "gdb-events.h"
#include "reggroups.h"
#include "osabi.h"
#include "gdb_obstack.h"

/* Static function declarations */

static void alloc_gdbarch_data (struct gdbarch *);

/* Non-zero if we want to trace architecture code.  */

#ifndef GDBARCH_DEBUG
#define GDBARCH_DEBUG 0
#endif
int gdbarch_debug = GDBARCH_DEBUG;

static const char *
pformat (const struct floatformat *format)
{
  if (format == NULL)
    return "(null)";
  else
    return  format->name;
}


/* Maintain the struct gdbarch object */

struct gdbarch
{
  /* Has this architecture been fully initialized?  */
  int initialized_p;

  /* An obstack bound to the lifetime of the architecture.  */
  struct obstack *obstack;

  /* basic architectural information */
  const struct bfd_arch_info * bfd_arch_info;
  int byte_order;
  enum gdb_osabi osabi;

  /* target specific vector. */
  struct gdbarch_tdep *tdep;
  gdbarch_dump_tdep_ftype *dump_tdep;

  /* per-architecture data-pointers */
  unsigned nr_data;
  void **data;

  /* per-architecture swap-regions */
  struct gdbarch_swap *swap;

  /* Multi-arch values.

     When extending this structure you must:

     Add the field below.

     Declare set/get functions and define the corresponding
     macro in gdbarch.h.

     gdbarch_alloc(): If zero/NULL is not a suitable default,
     initialize the new field.

     verify_gdbarch(): Confirm that the target updated the field
     correctly.

     gdbarch_dump(): Add a fprintf_unfiltered call so that the new
     field is dumped out

     ``startup_gdbarch()'': Append an initial value to the static
     variable (base values on the host's c-type system).

     get_gdbarch(): Implement the set/get functions (probably using
     the macro's as shortcuts).

     */

  int short_bit;
  int int_bit;
  int long_bit;
  int long_long_bit;
  int float_bit;
  const struct floatformat * float_format;
  int double_bit;
  const struct floatformat * double_format;
  int long_double_bit;
  const struct floatformat * long_double_format;
  int ptr_bit;
  int addr_bit;
  int bfd_vma_bit;
  int char_signed;
  gdbarch_read_pc_ftype *read_pc;
  gdbarch_write_pc_ftype *write_pc;
  gdbarch_read_sp_ftype *read_sp;
  gdbarch_virtual_frame_pointer_ftype *virtual_frame_pointer;
  gdbarch_pseudo_register_read_ftype *pseudo_register_read;
  gdbarch_pseudo_register_write_ftype *pseudo_register_write;
  int num_regs;
  int num_pseudo_regs;
  int sp_regnum;
  int pc_regnum;
  int ps_regnum;
  int fp0_regnum;
  gdbarch_stab_reg_to_regnum_ftype *stab_reg_to_regnum;
  gdbarch_ecoff_reg_to_regnum_ftype *ecoff_reg_to_regnum;
  gdbarch_dwarf_reg_to_regnum_ftype *dwarf_reg_to_regnum;
  gdbarch_sdb_reg_to_regnum_ftype *sdb_reg_to_regnum;
  gdbarch_dwarf2_reg_to_regnum_ftype *dwarf2_reg_to_regnum;
  gdbarch_register_name_ftype *register_name;
  gdbarch_register_type_ftype *register_type;
  gdbarch_deprecated_register_byte_ftype *deprecated_register_byte;
  gdbarch_unwind_dummy_id_ftype *unwind_dummy_id;
  int deprecated_fp_regnum;
  gdbarch_push_dummy_call_ftype *push_dummy_call;
  gdbarch_deprecated_push_arguments_ftype *deprecated_push_arguments;
  int deprecated_register_size;
  int call_dummy_location;
  gdbarch_push_dummy_code_ftype *push_dummy_code;
  gdbarch_print_registers_info_ftype *print_registers_info;
  gdbarch_print_float_info_ftype *print_float_info;
  gdbarch_print_vector_info_ftype *print_vector_info;
  gdbarch_register_sim_regno_ftype *register_sim_regno;
  gdbarch_register_bytes_ok_ftype *register_bytes_ok;
  gdbarch_cannot_fetch_register_ftype *cannot_fetch_register;
  gdbarch_cannot_store_register_ftype *cannot_store_register;
  gdbarch_get_longjmp_target_ftype *get_longjmp_target;
  int believe_pcc_promotion;
  gdbarch_convert_register_p_ftype *convert_register_p;
  gdbarch_register_to_value_ftype *register_to_value;
  gdbarch_value_to_register_ftype *value_to_register;
  gdbarch_pointer_to_address_ftype *pointer_to_address;
  gdbarch_address_to_pointer_ftype *address_to_pointer;
  gdbarch_integer_to_address_ftype *integer_to_address;
  gdbarch_deprecated_store_struct_return_ftype *deprecated_store_struct_return;
  gdbarch_return_value_ftype *return_value;
  gdbarch_extract_return_value_ftype *extract_return_value;
  gdbarch_store_return_value_ftype *store_return_value;
  gdbarch_deprecated_extract_return_value_ftype *deprecated_extract_return_value;
  gdbarch_deprecated_store_return_value_ftype *deprecated_store_return_value;
  gdbarch_deprecated_use_struct_convention_ftype *deprecated_use_struct_convention;
  gdbarch_deprecated_extract_struct_value_address_ftype *deprecated_extract_struct_value_address;
  gdbarch_skip_prologue_ftype *skip_prologue;
  gdbarch_inner_than_ftype *inner_than;
  gdbarch_breakpoint_from_pc_ftype *breakpoint_from_pc;
  gdbarch_adjust_breakpoint_address_ftype *adjust_breakpoint_address;
  gdbarch_memory_insert_breakpoint_ftype *memory_insert_breakpoint;
  gdbarch_memory_remove_breakpoint_ftype *memory_remove_breakpoint;
  CORE_ADDR decr_pc_after_break;
  CORE_ADDR deprecated_function_start_offset;
  gdbarch_remote_translate_xfer_address_ftype *remote_translate_xfer_address;
  CORE_ADDR frame_args_skip;
  gdbarch_unwind_pc_ftype *unwind_pc;
  gdbarch_unwind_sp_ftype *unwind_sp;
  gdbarch_deprecated_saved_pc_after_call_ftype *deprecated_saved_pc_after_call;
  gdbarch_frame_num_args_ftype *frame_num_args;
  gdbarch_deprecated_stack_align_ftype *deprecated_stack_align;
  gdbarch_frame_align_ftype *frame_align;
  gdbarch_deprecated_reg_struct_has_addr_ftype *deprecated_reg_struct_has_addr;
  gdbarch_stabs_argument_has_addr_ftype *stabs_argument_has_addr;
  int frame_red_zone_size;
  gdbarch_convert_from_func_ptr_addr_ftype *convert_from_func_ptr_addr;
  gdbarch_addr_bits_remove_ftype *addr_bits_remove;
  gdbarch_smash_text_address_ftype *smash_text_address;
  gdbarch_software_single_step_ftype *software_single_step;
  gdbarch_print_insn_ftype *print_insn;
  gdbarch_skip_trampoline_code_ftype *skip_trampoline_code;
  gdbarch_skip_solib_resolver_ftype *skip_solib_resolver;
  gdbarch_in_solib_call_trampoline_ftype *in_solib_call_trampoline;
  gdbarch_in_solib_return_trampoline_ftype *in_solib_return_trampoline;
  gdbarch_in_function_epilogue_p_ftype *in_function_epilogue_p;
  gdbarch_construct_inferior_arguments_ftype *construct_inferior_arguments;
  gdbarch_elf_make_msymbol_special_ftype *elf_make_msymbol_special;
  gdbarch_coff_make_msymbol_special_ftype *coff_make_msymbol_special;
  const char * name_of_malloc;
  int cannot_step_breakpoint;
  int have_nonsteppable_watchpoint;
  gdbarch_address_class_type_flags_ftype *address_class_type_flags;
  gdbarch_address_class_type_flags_to_name_ftype *address_class_type_flags_to_name;
  gdbarch_address_class_name_to_type_flags_ftype *address_class_name_to_type_flags;
  gdbarch_register_reggroup_p_ftype *register_reggroup_p;
  gdbarch_fetch_pointer_argument_ftype *fetch_pointer_argument;
  gdbarch_regset_from_core_section_ftype *regset_from_core_section;
};


/* The default architecture uses host values (for want of a better
   choice). */

extern const struct bfd_arch_info bfd_default_arch_struct;

struct gdbarch startup_gdbarch =
{
  1, /* Always initialized.  */
  NULL, /* The obstack.  */
  /* basic architecture information */
  &bfd_default_arch_struct,  /* bfd_arch_info */
  BFD_ENDIAN_BIG,  /* byte_order */
  GDB_OSABI_UNKNOWN,  /* osabi */
  /* target specific vector and its dump routine */
  NULL, NULL,
  /*per-architecture data-pointers and swap regions */
  0, NULL, NULL,
  /* Multi-arch values */
  8 * sizeof (short),  /* short_bit */
  8 * sizeof (int),  /* int_bit */
  8 * sizeof (long),  /* long_bit */
  8 * sizeof (LONGEST),  /* long_long_bit */
  8 * sizeof (float),  /* float_bit */
  0,  /* float_format */
  8 * sizeof (double),  /* double_bit */
  0,  /* double_format */
  8 * sizeof (long double),  /* long_double_bit */
  0,  /* long_double_format */
  8 * sizeof (void*),  /* ptr_bit */
  8 * sizeof (void*),  /* addr_bit */
  8 * sizeof (void*),  /* bfd_vma_bit */
  1,  /* char_signed */
  0,  /* read_pc */
  0,  /* write_pc */
  0,  /* read_sp */
  0,  /* virtual_frame_pointer */
  0,  /* pseudo_register_read */
  0,  /* pseudo_register_write */
  0,  /* num_regs */
  0,  /* num_pseudo_regs */
  -1,  /* sp_regnum */
  -1,  /* pc_regnum */
  -1,  /* ps_regnum */
  0,  /* fp0_regnum */
  0,  /* stab_reg_to_regnum */
  0,  /* ecoff_reg_to_regnum */
  0,  /* dwarf_reg_to_regnum */
  0,  /* sdb_reg_to_regnum */
  0,  /* dwarf2_reg_to_regnum */
  0,  /* register_name */
  0,  /* register_type */
  generic_register_byte,  /* deprecated_register_byte */
  0,  /* unwind_dummy_id */
  -1,  /* deprecated_fp_regnum */
  0,  /* push_dummy_call */
  0,  /* deprecated_push_arguments */
  0,  /* deprecated_register_size */
  0,  /* call_dummy_location */
  0,  /* push_dummy_code */
  default_print_registers_info,  /* print_registers_info */
  0,  /* print_float_info */
  0,  /* print_vector_info */
  0,  /* register_sim_regno */
  0,  /* register_bytes_ok */
  0,  /* cannot_fetch_register */
  0,  /* cannot_store_register */
  0,  /* get_longjmp_target */
  0,  /* believe_pcc_promotion */
  0,  /* convert_register_p */
  0,  /* register_to_value */
  0,  /* value_to_register */
  0,  /* pointer_to_address */
  0,  /* address_to_pointer */
  0,  /* integer_to_address */
  0,  /* deprecated_store_struct_return */
  0,  /* return_value */
  0,  /* extract_return_value */
  0,  /* store_return_value */
  0,  /* deprecated_extract_return_value */
  0,  /* deprecated_store_return_value */
  0,  /* deprecated_use_struct_convention */
  0,  /* deprecated_extract_struct_value_address */
  0,  /* skip_prologue */
  0,  /* inner_than */
  0,  /* breakpoint_from_pc */
  0,  /* adjust_breakpoint_address */
  0,  /* memory_insert_breakpoint */
  0,  /* memory_remove_breakpoint */
  0,  /* decr_pc_after_break */
  0,  /* deprecated_function_start_offset */
  generic_remote_translate_xfer_address,  /* remote_translate_xfer_address */
  0,  /* frame_args_skip */
  0,  /* unwind_pc */
  0,  /* unwind_sp */
  0,  /* deprecated_saved_pc_after_call */
  0,  /* frame_num_args */
  0,  /* deprecated_stack_align */
  0,  /* frame_align */
  0,  /* deprecated_reg_struct_has_addr */
  default_stabs_argument_has_addr,  /* stabs_argument_has_addr */
  0,  /* frame_red_zone_size */
  convert_from_func_ptr_addr_identity,  /* convert_from_func_ptr_addr */
  0,  /* addr_bits_remove */
  0,  /* smash_text_address */
  0,  /* software_single_step */
  0,  /* print_insn */
  0,  /* skip_trampoline_code */
  generic_skip_solib_resolver,  /* skip_solib_resolver */
  0,  /* in_solib_call_trampoline */
  0,  /* in_solib_return_trampoline */
  generic_in_function_epilogue_p,  /* in_function_epilogue_p */
  construct_inferior_arguments,  /* construct_inferior_arguments */
  0,  /* elf_make_msymbol_special */
  0,  /* coff_make_msymbol_special */
  "malloc",  /* name_of_malloc */
  0,  /* cannot_step_breakpoint */
  0,  /* have_nonsteppable_watchpoint */
  0,  /* address_class_type_flags */
  0,  /* address_class_type_flags_to_name */
  0,  /* address_class_name_to_type_flags */
  default_register_reggroup_p,  /* register_reggroup_p */
  0,  /* fetch_pointer_argument */
  0,  /* regset_from_core_section */
  /* startup_gdbarch() */
};

struct gdbarch *current_gdbarch = &startup_gdbarch;

/* Create a new ``struct gdbarch'' based on information provided by
   ``struct gdbarch_info''. */

struct gdbarch *
gdbarch_alloc (const struct gdbarch_info *info,
               struct gdbarch_tdep *tdep)
{
  /* NOTE: The new architecture variable is named ``current_gdbarch''
     so that macros such as TARGET_DOUBLE_BIT, when expanded, refer to
     the current local architecture and not the previous global
     architecture.  This ensures that the new architectures initial
     values are not influenced by the previous architecture.  Once
     everything is parameterised with gdbarch, this will go away.  */
  struct gdbarch *current_gdbarch;

  /* Create an obstack for allocating all the per-architecture memory,
     then use that to allocate the architecture vector.  */
  struct obstack *obstack = XMALLOC (struct obstack);
  obstack_init (obstack);
  current_gdbarch = obstack_alloc (obstack, sizeof (*current_gdbarch));
  memset (current_gdbarch, 0, sizeof (*current_gdbarch));
  current_gdbarch->obstack = obstack;

  alloc_gdbarch_data (current_gdbarch);

  current_gdbarch->tdep = tdep;

  current_gdbarch->bfd_arch_info = info->bfd_arch_info;
  current_gdbarch->byte_order = info->byte_order;
  current_gdbarch->osabi = info->osabi;

  /* Force the explicit initialization of these. */
  current_gdbarch->short_bit = 2*TARGET_CHAR_BIT;
  current_gdbarch->int_bit = 4*TARGET_CHAR_BIT;
  current_gdbarch->long_bit = 4*TARGET_CHAR_BIT;
  current_gdbarch->long_long_bit = 2*TARGET_LONG_BIT;
  current_gdbarch->float_bit = 4*TARGET_CHAR_BIT;
  current_gdbarch->double_bit = 8*TARGET_CHAR_BIT;
  current_gdbarch->long_double_bit = 8*TARGET_CHAR_BIT;
  current_gdbarch->ptr_bit = TARGET_INT_BIT;
  current_gdbarch->bfd_vma_bit = TARGET_ARCHITECTURE->bits_per_address;
  current_gdbarch->char_signed = -1;
  current_gdbarch->write_pc = generic_target_write_pc;
  current_gdbarch->virtual_frame_pointer = legacy_virtual_frame_pointer;
  current_gdbarch->num_regs = -1;
  current_gdbarch->sp_regnum = -1;
  current_gdbarch->pc_regnum = -1;
  current_gdbarch->ps_regnum = -1;
  current_gdbarch->fp0_regnum = -1;
  current_gdbarch->stab_reg_to_regnum = no_op_reg_to_regnum;
  current_gdbarch->ecoff_reg_to_regnum = no_op_reg_to_regnum;
  current_gdbarch->dwarf_reg_to_regnum = no_op_reg_to_regnum;
  current_gdbarch->sdb_reg_to_regnum = no_op_reg_to_regnum;
  current_gdbarch->dwarf2_reg_to_regnum = no_op_reg_to_regnum;
  current_gdbarch->deprecated_register_byte = generic_register_byte;
  current_gdbarch->deprecated_fp_regnum = -1;
  current_gdbarch->call_dummy_location = AT_ENTRY_POINT;
  current_gdbarch->print_registers_info = default_print_registers_info;
  current_gdbarch->register_sim_regno = legacy_register_sim_regno;
  current_gdbarch->cannot_fetch_register = cannot_register_not;
  current_gdbarch->cannot_store_register = cannot_register_not;
  current_gdbarch->convert_register_p = generic_convert_register_p;
  current_gdbarch->pointer_to_address = unsigned_pointer_to_address;
  current_gdbarch->address_to_pointer = unsigned_address_to_pointer;
  current_gdbarch->return_value = legacy_return_value;
  current_gdbarch->extract_return_value = legacy_extract_return_value;
  current_gdbarch->store_return_value = legacy_store_return_value;
  current_gdbarch->deprecated_use_struct_convention = generic_use_struct_convention;
  current_gdbarch->memory_insert_breakpoint = default_memory_insert_breakpoint;
  current_gdbarch->memory_remove_breakpoint = default_memory_remove_breakpoint;
  current_gdbarch->remote_translate_xfer_address = generic_remote_translate_xfer_address;
  current_gdbarch->stabs_argument_has_addr = default_stabs_argument_has_addr;
  current_gdbarch->convert_from_func_ptr_addr = convert_from_func_ptr_addr_identity;
  current_gdbarch->addr_bits_remove = core_addr_identity;
  current_gdbarch->smash_text_address = core_addr_identity;
  current_gdbarch->skip_trampoline_code = generic_skip_trampoline_code;
  current_gdbarch->skip_solib_resolver = generic_skip_solib_resolver;
  current_gdbarch->in_solib_call_trampoline = generic_in_solib_call_trampoline;
  current_gdbarch->in_solib_return_trampoline = generic_in_solib_return_trampoline;
  current_gdbarch->in_function_epilogue_p = generic_in_function_epilogue_p;
  current_gdbarch->construct_inferior_arguments = construct_inferior_arguments;
  current_gdbarch->elf_make_msymbol_special = default_elf_make_msymbol_special;
  current_gdbarch->coff_make_msymbol_special = default_coff_make_msymbol_special;
  current_gdbarch->name_of_malloc = "malloc";
  current_gdbarch->register_reggroup_p = default_register_reggroup_p;
  /* gdbarch_alloc() */

  return current_gdbarch;
}


/* Allocate extra space using the per-architecture obstack.  */

void *
gdbarch_obstack_zalloc (struct gdbarch *arch, long size)
{
  void *data = obstack_alloc (arch->obstack, size);
  memset (data, 0, size);
  return data;
}


/* Free a gdbarch struct.  This should never happen in normal
   operation --- once you've created a gdbarch, you keep it around.
   However, if an architecture's init function encounters an error
   building the structure, it may need to clean up a partially
   constructed gdbarch.  */

void
gdbarch_free (struct gdbarch *arch)
{
  struct obstack *obstack;
  gdb_assert (arch != NULL);
  gdb_assert (!arch->initialized_p);
  obstack = arch->obstack;
  obstack_free (obstack, 0); /* Includes the ARCH.  */
  xfree (obstack);
}


/* Ensure that all values in a GDBARCH are reasonable.  */

/* NOTE/WARNING: The parameter is called ``current_gdbarch'' so that it
   just happens to match the global variable ``current_gdbarch''.  That
   way macros refering to that variable get the local and not the global
   version - ulgh.  Once everything is parameterised with gdbarch, this
   will go away. */

static void
verify_gdbarch (struct gdbarch *current_gdbarch)
{
  struct ui_file *log;
  struct cleanup *cleanups;
  long dummy;
  char *buf;
  log = mem_fileopen ();
  cleanups = make_cleanup_ui_file_delete (log);
  /* fundamental */
  if (current_gdbarch->byte_order == BFD_ENDIAN_UNKNOWN)
    fprintf_unfiltered (log, "\n\tbyte-order");
  if (current_gdbarch->bfd_arch_info == NULL)
    fprintf_unfiltered (log, "\n\tbfd_arch_info");
  /* Check those that need to be defined for the given multi-arch level. */
  /* Skip verify of short_bit, invalid_p == 0 */
  /* Skip verify of int_bit, invalid_p == 0 */
  /* Skip verify of long_bit, invalid_p == 0 */
  /* Skip verify of long_long_bit, invalid_p == 0 */
  /* Skip verify of float_bit, invalid_p == 0 */
  if (current_gdbarch->float_format == 0)
    current_gdbarch->float_format = default_float_format (current_gdbarch);
  /* Skip verify of double_bit, invalid_p == 0 */
  if (current_gdbarch->double_format == 0)
    current_gdbarch->double_format = default_double_format (current_gdbarch);
  /* Skip verify of long_double_bit, invalid_p == 0 */
  if (current_gdbarch->long_double_format == 0)
    current_gdbarch->long_double_format = default_double_format (current_gdbarch);
  /* Skip verify of ptr_bit, invalid_p == 0 */
  if (current_gdbarch->addr_bit == 0)
    current_gdbarch->addr_bit = TARGET_PTR_BIT;
  /* Skip verify of bfd_vma_bit, invalid_p == 0 */
  if (current_gdbarch->char_signed == -1)
    current_gdbarch->char_signed = 1;
  /* Skip verify of read_pc, has predicate */
  /* Skip verify of write_pc, invalid_p == 0 */
  /* Skip verify of read_sp, has predicate */
  /* Skip verify of virtual_frame_pointer, invalid_p == 0 */
  /* Skip verify of pseudo_register_read, has predicate */
  /* Skip verify of pseudo_register_write, has predicate */
  if (current_gdbarch->num_regs == -1)
    fprintf_unfiltered (log, "\n\tnum_regs");
  /* Skip verify of num_pseudo_regs, invalid_p == 0 */
  /* Skip verify of sp_regnum, invalid_p == 0 */
  /* Skip verify of pc_regnum, invalid_p == 0 */
  /* Skip verify of ps_regnum, invalid_p == 0 */
  /* Skip verify of fp0_regnum, invalid_p == 0 */
  /* Skip verify of stab_reg_to_regnum, invalid_p == 0 */
  /* Skip verify of ecoff_reg_to_regnum, invalid_p == 0 */
  /* Skip verify of dwarf_reg_to_regnum, invalid_p == 0 */
  /* Skip verify of sdb_reg_to_regnum, invalid_p == 0 */
  /* Skip verify of dwarf2_reg_to_regnum, invalid_p == 0 */
  /* Skip verify of register_type, has predicate */
  /* Skip verify of deprecated_register_byte, has predicate */
  /* Skip verify of unwind_dummy_id, has predicate */
  /* Skip verify of deprecated_fp_regnum, invalid_p == 0 */
  /* Skip verify of push_dummy_call, has predicate */
  /* Skip verify of deprecated_push_arguments, has predicate */
  /* Skip verify of call_dummy_location, invalid_p == 0 */
  /* Skip verify of push_dummy_code, has predicate */
  /* Skip verify of print_registers_info, invalid_p == 0 */
  /* Skip verify of print_float_info, has predicate */
  /* Skip verify of print_vector_info, has predicate */
  /* Skip verify of register_sim_regno, invalid_p == 0 */
  /* Skip verify of register_bytes_ok, has predicate */
  /* Skip verify of cannot_fetch_register, invalid_p == 0 */
  /* Skip verify of cannot_store_register, invalid_p == 0 */
  /* Skip verify of get_longjmp_target, has predicate */
  /* Skip verify of convert_register_p, invalid_p == 0 */
  /* Skip verify of pointer_to_address, invalid_p == 0 */
  /* Skip verify of address_to_pointer, invalid_p == 0 */
  /* Skip verify of integer_to_address, has predicate */
  /* Skip verify of deprecated_store_struct_return, has predicate */
  /* Skip verify of return_value, has predicate */
  /* Skip verify of extract_return_value, invalid_p == 0 */
  /* Skip verify of store_return_value, invalid_p == 0 */
  /* Skip verify of deprecated_use_struct_convention, invalid_p == 0 */
  /* Skip verify of deprecated_extract_struct_value_address, has predicate */
  if (current_gdbarch->skip_prologue == 0)
    fprintf_unfiltered (log, "\n\tskip_prologue");
  if (current_gdbarch->inner_than == 0)
    fprintf_unfiltered (log, "\n\tinner_than");
  if (current_gdbarch->breakpoint_from_pc == 0)
    fprintf_unfiltered (log, "\n\tbreakpoint_from_pc");
  /* Skip verify of adjust_breakpoint_address, has predicate */
  /* Skip verify of memory_insert_breakpoint, invalid_p == 0 */
  /* Skip verify of memory_remove_breakpoint, invalid_p == 0 */
  /* Skip verify of decr_pc_after_break, invalid_p == 0 */
  /* Skip verify of deprecated_function_start_offset, invalid_p == 0 */
  /* Skip verify of remote_translate_xfer_address, invalid_p == 0 */
  /* Skip verify of frame_args_skip, invalid_p == 0 */
  /* Skip verify of unwind_pc, has predicate */
  /* Skip verify of unwind_sp, has predicate */
  /* Skip verify of deprecated_saved_pc_after_call, has predicate */
  /* Skip verify of frame_num_args, has predicate */
  /* Skip verify of deprecated_stack_align, has predicate */
  /* Skip verify of frame_align, has predicate */
  /* Skip verify of deprecated_reg_struct_has_addr, has predicate */
  /* Skip verify of stabs_argument_has_addr, invalid_p == 0 */
  /* Skip verify of convert_from_func_ptr_addr, invalid_p == 0 */
  /* Skip verify of addr_bits_remove, invalid_p == 0 */
  /* Skip verify of smash_text_address, invalid_p == 0 */
  /* Skip verify of software_single_step, has predicate */
  if (current_gdbarch->print_insn == 0)
    fprintf_unfiltered (log, "\n\tprint_insn");
  /* Skip verify of skip_trampoline_code, invalid_p == 0 */
  /* Skip verify of skip_solib_resolver, invalid_p == 0 */
  /* Skip verify of in_solib_call_trampoline, invalid_p == 0 */
  /* Skip verify of in_solib_return_trampoline, invalid_p == 0 */
  /* Skip verify of in_function_epilogue_p, invalid_p == 0 */
  /* Skip verify of construct_inferior_arguments, invalid_p == 0 */
  /* Skip verify of elf_make_msymbol_special, invalid_p == 0 */
  /* Skip verify of coff_make_msymbol_special, invalid_p == 0 */
  /* Skip verify of name_of_malloc, invalid_p == 0 */
  /* Skip verify of cannot_step_breakpoint, invalid_p == 0 */
  /* Skip verify of have_nonsteppable_watchpoint, invalid_p == 0 */
  /* Skip verify of address_class_type_flags, has predicate */
  /* Skip verify of address_class_type_flags_to_name, has predicate */
  /* Skip verify of address_class_name_to_type_flags, has predicate */
  /* Skip verify of register_reggroup_p, invalid_p == 0 */
  /* Skip verify of fetch_pointer_argument, has predicate */
  /* Skip verify of regset_from_core_section, has predicate */
  buf = ui_file_xstrdup (log, &dummy);
  make_cleanup (xfree, buf);
  if (strlen (buf) > 0)
    internal_error (__FILE__, __LINE__,
                    "verify_gdbarch: the following are invalid ...%s",
                    buf);
  do_cleanups (cleanups);
}


/* Print out the details of the current architecture. */

/* NOTE/WARNING: The parameter is called ``current_gdbarch'' so that it
   just happens to match the global variable ``current_gdbarch''.  That
   way macros refering to that variable get the local and not the global
   version - ulgh.  Once everything is parameterised with gdbarch, this
   will go away. */

void
gdbarch_dump (struct gdbarch *current_gdbarch, struct ui_file *file)
{
  const char *gdb_xm_file = "<not-defined>";
  const char *gdb_nm_file = "<not-defined>";
  const char *gdb_tm_file = "<not-defined>";
#if defined (GDB_XM_FILE)
  gdb_xm_file = GDB_XM_FILE;
#endif
  fprintf_unfiltered (file,
                      "gdbarch_dump: GDB_XM_FILE = %s\n",
                      gdb_xm_file);
#if defined (GDB_NM_FILE)
  gdb_nm_file = GDB_NM_FILE;
#endif
  fprintf_unfiltered (file,
                      "gdbarch_dump: GDB_NM_FILE = %s\n",
                      gdb_nm_file);
#if defined (GDB_TM_FILE)
  gdb_tm_file = GDB_TM_FILE;
#endif
  fprintf_unfiltered (file,
                      "gdbarch_dump: GDB_TM_FILE = %s\n",
                      gdb_tm_file);
#ifdef TARGET_ADDR_BIT
  fprintf_unfiltered (file,
                      "gdbarch_dump: TARGET_ADDR_BIT # %s\n",
                      XSTRING (TARGET_ADDR_BIT));
#endif
  fprintf_unfiltered (file,
                      "gdbarch_dump: addr_bit = %s\n",
                      paddr_d (current_gdbarch->addr_bit));
#ifdef ADDR_BITS_REMOVE
  fprintf_unfiltered (file,
                      "gdbarch_dump: %s # %s\n",
                      "ADDR_BITS_REMOVE(addr)",
                      XSTRING (ADDR_BITS_REMOVE (addr)));
#endif
  fprintf_unfiltered (file,
                      "gdbarch_dump: addr_bits_remove = <0x%lx>\n",
                      (long) current_gdbarch->addr_bits_remove);
  fprintf_unfiltered (file,
                      "gdbarch_dump: gdbarch_address_class_name_to_type_flags_p() = %d\n",
                      gdbarch_address_class_name_to_type_flags_p (current_gdbarch));
  fprintf_unfiltered (file,
                      "gdbarch_dump: address_class_name_to_type_flags = <0x%lx>\n",
                      (long) current_gdbarch->address_class_name_to_type_flags);
#ifdef ADDRESS_CLASS_TYPE_FLAGS_P
  fprintf_unfiltered (file,
                      "gdbarch_dump: %s # %s\n",
                      "ADDRESS_CLASS_TYPE_FLAGS_P()",
                      XSTRING (ADDRESS_CLASS_TYPE_FLAGS_P ()));
#endif
  fprintf_unfiltered (file,
                      "gdbarch_dump: gdbarch_address_class_type_flags_p() = %d\n",
                      gdbarch_address_class_type_flags_p (current_gdbarch));
#ifdef ADDRESS_CLASS_TYPE_FLAGS
  fprintf_unfiltered (file,
                      "gdbarch_dump: %s # %s\n",
                      "ADDRESS_CLASS_TYPE_FLAGS(byte_size, dwarf2_addr_class)",
                      XSTRING (ADDRESS_CLASS_TYPE_FLAGS (byte_size, dwarf2_addr_class)));
#endif
  fprintf_unfiltered (file,
                      "gdbarch_dump: address_class_type_flags = <0x%lx>\n",
                      (long) current_gdbarch->address_class_type_flags);
  fprintf_unfiltered (file,
                      "gdbarch_dump: gdbarch_address_class_type_flags_to_name_p() = %d\n",
                      gdbarch_address_class_type_flags_to_name_p (current_gdbarch));
  fprintf_unfiltered (file,
                      "gdbarch_dump: address_class_type_flags_to_name = <0x%lx>\n",
                      (long) current_gdbarch->address_class_type_flags_to_name);
#ifdef ADDRESS_TO_POINTER
  fprintf_unfiltered (file,
                      "gdbarch_dump: %s # %s\n",
                      "ADDRESS_TO_POINTER(type, buf, addr)",
                      XSTRING (ADDRESS_TO_POINTER (type, buf, addr)));
#endif
  fprintf_unfiltered (file,
                      "gdbarch_dump: address_to_pointer = <0x%lx>\n",
                      (long) current_gdbarch->address_to_pointer);
  fprintf_unfiltered (file,
                      "gdbarch_dump: gdbarch_adjust_breakpoint_address_p() = %d\n",
                      gdbarch_adjust_breakpoint_address_p (current_gdbarch));
  fprintf_unfiltered (file,
                      "gdbarch_dump: adjust_breakpoint_address = <0x%lx>\n",
                      (long) current_gdbarch->adjust_breakpoint_address);
#ifdef BELIEVE_PCC_PROMOTION
  fprintf_unfiltered (file,
                      "gdbarch_dump: BELIEVE_PCC_PROMOTION # %s\n",
                      XSTRING (BELIEVE_PCC_PROMOTION));
#endif
  fprintf_unfiltered (file,
                      "gdbarch_dump: believe_pcc_promotion = %s\n",
                      paddr_d (current_gdbarch->believe_pcc_promotion));
#ifdef TARGET_ARCHITECTURE
  fprintf_unfiltered (file,
                      "gdbarch_dump: TARGET_ARCHITECTURE # %s\n",
                      XSTRING (TARGET_ARCHITECTURE));
#endif
  fprintf_unfiltered (file,
                      "gdbarch_dump: bfd_arch_info = %s\n",
                      TARGET_ARCHITECTURE->printable_name);
#ifdef TARGET_BFD_VMA_BIT
  fprintf_unfiltered (file,
                      "gdbarch_dump: TARGET_BFD_VMA_BIT # %s\n",
                      XSTRING (TARGET_BFD_VMA_BIT));
#endif
  fprintf_unfiltered (file,
                      "gdbarch_dump: bfd_vma_bit = %s\n",
                      paddr_d (current_gdbarch->bfd_vma_bit));
#ifdef BREAKPOINT_FROM_PC
  fprintf_unfiltered (file,
                      "gdbarch_dump: %s # %s\n",
                      "BREAKPOINT_FROM_PC(pcptr, lenptr)",
                      XSTRING (BREAKPOINT_FROM_PC (pcptr, lenptr)));
#endif
  fprintf_unfiltered (file,
                      "gdbarch_dump: breakpoint_from_pc = <0x%lx>\n",
                      (long) current_gdbarch->breakpoint_from_pc);
#ifdef TARGET_BYTE_ORDER
  fprintf_unfiltered (file,
                      "gdbarch_dump: TARGET_BYTE_ORDER # %s\n",
                      XSTRING (TARGET_BYTE_ORDER));
#endif
  fprintf_unfiltered (file,
                      "gdbarch_dump: byte_order = %s\n",
                      paddr_d (current_gdbarch->byte_order));
#ifdef CALL_DUMMY_LOCATION
  fprintf_unfiltered (file,
                      "gdbarch_dump: CALL_DUMMY_LOCATION # %s\n",
                      XSTRING (CALL_DUMMY_LOCATION));
#endif
  fprintf_unfiltered (file,
                      "gdbarch_dump: call_dummy_location = %s\n",
                      paddr_d (current_gdbarch->call_dummy_location));
#ifdef CANNOT_FETCH_REGISTER
  fprintf_unfiltered (file,
                      "gdbarch_dump: %s # %s\n",
                      "CANNOT_FETCH_REGISTER(regnum)",
                      XSTRING (CANNOT_FETCH_REGISTER (regnum)));
#endif
  fprintf_unfiltered (file,
                      "gdbarch_dump: cannot_fetch_register = <0x%lx>\n",
                      (long) current_gdbarch->cannot_fetch_register);
#ifdef CANNOT_STEP_BREAKPOINT
  fprintf_unfiltered (file,
                      "gdbarch_dump: CANNOT_STEP_BREAKPOINT # %s\n",
                      XSTRING (CANNOT_STEP_BREAKPOINT));
#endif
  fprintf_unfiltered (file,
                      "gdbarch_dump: cannot_step_breakpoint = %s\n",
                      paddr_d (current_gdbarch->cannot_step_breakpoint));
#ifdef CANNOT_STORE_REGISTER
  fprintf_unfiltered (file,
                      "gdbarch_dump: %s # %s\n",
                      "CANNOT_STORE_REGISTER(regnum)",
                      XSTRING (CANNOT_STORE_REGISTER (regnum)));
#endif
  fprintf_unfiltered (file,
                      "gdbarch_dump: cannot_store_register = <0x%lx>\n",
                      (long) current_gdbarch->cannot_store_register);
#ifdef TARGET_CHAR_SIGNED
  fprintf_unfiltered (file,
                      "gdbarch_dump: TARGET_CHAR_SIGNED # %s\n",
                      XSTRING (TARGET_CHAR_SIGNED));
#endif
  fprintf_unfiltered (file,
                      "gdbarch_dump: char_signed = %s\n",
                      paddr_d (current_gdbarch->char_signed));
#ifdef COFF_MAKE_MSYMBOL_SPECIAL
  fprintf_unfiltered (file,
                      "gdbarch_dump: %s # %s\n",
                      "COFF_MAKE_MSYMBOL_SPECIAL(val, msym)",
                      XSTRING (COFF_MAKE_MSYMBOL_SPECIAL (val, msym)));
#endif
  fprintf_unfiltered (file,
                      "gdbarch_dump: coff_make_msymbol_special = <0x%lx>\n",
                      (long) current_gdbarch->coff_make_msymbol_special);
  fprintf_unfiltered (file,
                      "gdbarch_dump: construct_inferior_arguments = <0x%lx>\n",
                      (long) current_gdbarch->construct_inferior_arguments);
  fprintf_unfiltered (file,
                      "gdbarch_dump: convert_from_func_ptr_addr = <0x%lx>\n",
                      (long) current_gdbarch->convert_from_func_ptr_addr);
#ifdef CONVERT_REGISTER_P
  fprintf_unfiltered (file,
                      "gdbarch_dump: %s # %s\n",
                      "CONVERT_REGISTER_P(regnum, type)",
                      XSTRING (CONVERT_REGISTER_P (regnum, type)));
#endif
  fprintf_unfiltered (file,
                      "gdbarch_dump: convert_register_p = <0x%lx>\n",
                      (long) current_gdbarch->convert_register_p);
#ifdef DECR_PC_AFTER_BREAK
  fprintf_unfiltered (file,
                      "gdbarch_dump: DECR_PC_AFTER_BREAK # %s\n",
                      XSTRING (DECR_PC_AFTER_BREAK));
#endif
  fprintf_unfiltered (file,
                      "gdbarch_dump: decr_pc_after_break = 0x%s\n",
                      paddr_nz (current_gdbarch->decr_pc_after_break));
#ifdef DEPRECATED_EXTRACT_RETURN_VALUE
  fprintf_unfiltered (file,
                      "gdbarch_dump: %s # %s\n",
                      "DEPRECATED_EXTRACT_RETURN_VALUE(type, regbuf, valbuf)",
                      XSTRING (DEPRECATED_EXTRACT_RETURN_VALUE (type, regbuf, valbuf)));
#endif
  fprintf_unfiltered (file,
                      "gdbarch_dump: deprecated_extract_return_value = <0x%lx>\n",
                      (long) current_gdbarch->deprecated_extract_return_value);
#ifdef DEPRECATED_EXTRACT_STRUCT_VALUE_ADDRESS_P
  fprintf_unfiltered (file,
                      "gdbarch_dump: %s # %s\n",
                      "DEPRECATED_EXTRACT_STRUCT_VALUE_ADDRESS_P()",
                      XSTRING (DEPRECATED_EXTRACT_STRUCT_VALUE_ADDRESS_P ()));
#endif
  fprintf_unfiltered (file,
                      "gdbarch_dump: gdbarch_deprecated_extract_struct_value_address_p() = %d\n",
                      gdbarch_deprecated_extract_struct_value_address_p (current_gdbarch));
#ifdef DEPRECATED_EXTRACT_STRUCT_VALUE_ADDRESS
  fprintf_unfiltered (file,
                      "gdbarch_dump: %s # %s\n",
                      "DEPRECATED_EXTRACT_STRUCT_VALUE_ADDRESS(regcache)",
                      XSTRING (DEPRECATED_EXTRACT_STRUCT_VALUE_ADDRESS (regcache)));
#endif
  fprintf_unfiltered (file,
                      "gdbarch_dump: deprecated_extract_struct_value_address = <0x%lx>\n",
                      (long) current_gdbarch->deprecated_extract_struct_value_address);
#ifdef DEPRECATED_FP_REGNUM
  fprintf_unfiltered (file,
                      "gdbarch_dump: DEPRECATED_FP_REGNUM # %s\n",
                      XSTRING (DEPRECATED_FP_REGNUM));
#endif
  fprintf_unfiltered (file,
                      "gdbarch_dump: deprecated_fp_regnum = %s\n",
                      paddr_d (current_gdbarch->deprecated_fp_regnum));
#ifdef DEPRECATED_FUNCTION_START_OFFSET
  fprintf_unfiltered (file,
                      "gdbarch_dump: DEPRECATED_FUNCTION_START_OFFSET # %s\n",
                      XSTRING (DEPRECATED_FUNCTION_START_OFFSET));
#endif
  fprintf_unfiltered (file,
                      "gdbarch_dump: deprecated_function_start_offset = 0x%s\n",
                      paddr_nz (current_gdbarch->deprecated_function_start_offset));
#ifdef DEPRECATED_PUSH_ARGUMENTS_P
  fprintf_unfiltered (file,
                      "gdbarch_dump: %s # %s\n",
                      "DEPRECATED_PUSH_ARGUMENTS_P()",
                      XSTRING (DEPRECATED_PUSH_ARGUMENTS_P ()));
#endif
  fprintf_unfiltered (file,
                      "gdbarch_dump: gdbarch_deprecated_push_arguments_p() = %d\n",
                      gdbarch_deprecated_push_arguments_p (current_gdbarch));
#ifdef DEPRECATED_PUSH_ARGUMENTS
  fprintf_unfiltered (file,
                      "gdbarch_dump: %s # %s\n",
                      "DEPRECATED_PUSH_ARGUMENTS(nargs, args, sp, struct_return, struct_addr)",
                      XSTRING (DEPRECATED_PUSH_ARGUMENTS (nargs, args, sp, struct_return, struct_addr)));
#endif
  fprintf_unfiltered (file,
                      "gdbarch_dump: deprecated_push_arguments = <0x%lx>\n",
                      (long) current_gdbarch->deprecated_push_arguments);
#ifdef DEPRECATED_REG_STRUCT_HAS_ADDR_P
  fprintf_unfiltered (file,
                      "gdbarch_dump: %s # %s\n",
                      "DEPRECATED_REG_STRUCT_HAS_ADDR_P()",
                      XSTRING (DEPRECATED_REG_STRUCT_HAS_ADDR_P ()));
#endif
  fprintf_unfiltered (file,
                      "gdbarch_dump: gdbarch_deprecated_reg_struct_has_addr_p() = %d\n",
                      gdbarch_deprecated_reg_struct_has_addr_p (current_gdbarch));
#ifdef DEPRECATED_REG_STRUCT_HAS_ADDR
  fprintf_unfiltered (file,
                      "gdbarch_dump: %s # %s\n",
                      "DEPRECATED_REG_STRUCT_HAS_ADDR(gcc_p, type)",
                      XSTRING (DEPRECATED_REG_STRUCT_HAS_ADDR (gcc_p, type)));
#endif
  fprintf_unfiltered (file,
                      "gdbarch_dump: deprecated_reg_struct_has_addr = <0x%lx>\n",
                      (long) current_gdbarch->deprecated_reg_struct_has_addr);
#ifdef DEPRECATED_REGISTER_BYTE_P
  fprintf_unfiltered (file,
                      "gdbarch_dump: %s # %s\n",
                      "DEPRECATED_REGISTER_BYTE_P()",
                      XSTRING (DEPRECATED_REGISTER_BYTE_P ()));
#endif
  fprintf_unfiltered (file,
                      "gdbarch_dump: gdbarch_deprecated_register_byte_p() = %d\n",
                      gdbarch_deprecated_register_byte_p (current_gdbarch));
#ifdef DEPRECATED_REGISTER_BYTE
  fprintf_unfiltered (file,
                      "gdbarch_dump: %s # %s\n",
                      "DEPRECATED_REGISTER_BYTE(reg_nr)",
                      XSTRING (DEPRECATED_REGISTER_BYTE (reg_nr)));
#endif
  fprintf_unfiltered (file,
                      "gdbarch_dump: deprecated_register_byte = <0x%lx>\n",
                      (long) current_gdbarch->deprecated_register_byte);
#ifdef DEPRECATED_REGISTER_SIZE
  fprintf_unfiltered (file,
                      "gdbarch_dump: DEPRECATED_REGISTER_SIZE # %s\n",
                      XSTRING (DEPRECATED_REGISTER_SIZE));
#endif
  fprintf_unfiltered (file,
                      "gdbarch_dump: deprecated_register_size = %s\n",
                      paddr_d (current_gdbarch->deprecated_register_size));
#ifdef DEPRECATED_SAVED_PC_AFTER_CALL_P
  fprintf_unfiltered (file,
                      "gdbarch_dump: %s # %s\n",
                      "DEPRECATED_SAVED_PC_AFTER_CALL_P()",
                      XSTRING (DEPRECATED_SAVED_PC_AFTER_CALL_P ()));
#endif
  fprintf_unfiltered (file,
                      "gdbarch_dump: gdbarch_deprecated_saved_pc_after_call_p() = %d\n",
                      gdbarch_deprecated_saved_pc_after_call_p (current_gdbarch));
#ifdef DEPRECATED_SAVED_PC_AFTER_CALL
  fprintf_unfiltered (file,
                      "gdbarch_dump: %s # %s\n",
                      "DEPRECATED_SAVED_PC_AFTER_CALL(frame)",
                      XSTRING (DEPRECATED_SAVED_PC_AFTER_CALL (frame)));
#endif
  fprintf_unfiltered (file,
                      "gdbarch_dump: deprecated_saved_pc_after_call = <0x%lx>\n",
                      (long) current_gdbarch->deprecated_saved_pc_after_call);
#ifdef DEPRECATED_STACK_ALIGN_P
  fprintf_unfiltered (file,
                      "gdbarch_dump: %s # %s\n",
                      "DEPRECATED_STACK_ALIGN_P()",
                      XSTRING (DEPRECATED_STACK_ALIGN_P ()));
#endif
  fprintf_unfiltered (file,
                      "gdbarch_dump: gdbarch_deprecated_stack_align_p() = %d\n",
                      gdbarch_deprecated_stack_align_p (current_gdbarch));
#ifdef DEPRECATED_STACK_ALIGN
  fprintf_unfiltered (file,
                      "gdbarch_dump: %s # %s\n",
                      "DEPRECATED_STACK_ALIGN(sp)",
                      XSTRING (DEPRECATED_STACK_ALIGN (sp)));
#endif
  fprintf_unfiltered (file,
                      "gdbarch_dump: deprecated_stack_align = <0x%lx>\n",
                      (long) current_gdbarch->deprecated_stack_align);
#ifdef DEPRECATED_STORE_RETURN_VALUE
  fprintf_unfiltered (file,
                      "gdbarch_dump: %s # %s\n",
                      "DEPRECATED_STORE_RETURN_VALUE(type, valbuf)",
                      XSTRING (DEPRECATED_STORE_RETURN_VALUE (type, valbuf)));
#endif
  fprintf_unfiltered (file,
                      "gdbarch_dump: deprecated_store_return_value = <0x%lx>\n",
                      (long) current_gdbarch->deprecated_store_return_value);
#ifdef DEPRECATED_STORE_STRUCT_RETURN_P
  fprintf_unfiltered (file,
                      "gdbarch_dump: %s # %s\n",
                      "DEPRECATED_STORE_STRUCT_RETURN_P()",
                      XSTRING (DEPRECATED_STORE_STRUCT_RETURN_P ()));
#endif
  fprintf_unfiltered (file,
                      "gdbarch_dump: gdbarch_deprecated_store_struct_return_p() = %d\n",
                      gdbarch_deprecated_store_struct_return_p (current_gdbarch));
#ifdef DEPRECATED_STORE_STRUCT_RETURN
  fprintf_unfiltered (file,
                      "gdbarch_dump: %s # %s\n",
                      "DEPRECATED_STORE_STRUCT_RETURN(addr, sp)",
                      XSTRING (DEPRECATED_STORE_STRUCT_RETURN (addr, sp)));
#endif
  fprintf_unfiltered (file,
                      "gdbarch_dump: deprecated_store_struct_return = <0x%lx>\n",
                      (long) current_gdbarch->deprecated_store_struct_return);
#ifdef DEPRECATED_USE_STRUCT_CONVENTION
  fprintf_unfiltered (file,
                      "gdbarch_dump: %s # %s\n",
                      "DEPRECATED_USE_STRUCT_CONVENTION(gcc_p, value_type)",
                      XSTRING (DEPRECATED_USE_STRUCT_CONVENTION (gcc_p, value_type)));
#endif
  fprintf_unfiltered (file,
                      "gdbarch_dump: deprecated_use_struct_convention = <0x%lx>\n",
                      (long) current_gdbarch->deprecated_use_struct_convention);
#ifdef TARGET_DOUBLE_BIT
  fprintf_unfiltered (file,
                      "gdbarch_dump: TARGET_DOUBLE_BIT # %s\n",
                      XSTRING (TARGET_DOUBLE_BIT));
#endif
  fprintf_unfiltered (file,
                      "gdbarch_dump: double_bit = %s\n",
                      paddr_d (current_gdbarch->double_bit));
#ifdef TARGET_DOUBLE_FORMAT
  fprintf_unfiltered (file,
                      "gdbarch_dump: TARGET_DOUBLE_FORMAT # %s\n",
                      XSTRING (TARGET_DOUBLE_FORMAT));
#endif
  fprintf_unfiltered (file,
                      "gdbarch_dump: double_format = %s\n",
                      pformat (current_gdbarch->double_format));
#ifdef DWARF2_REG_TO_REGNUM
  fprintf_unfiltered (file,
                      "gdbarch_dump: %s # %s\n",
                      "DWARF2_REG_TO_REGNUM(dwarf2_regnr)",
                      XSTRING (DWARF2_REG_TO_REGNUM (dwarf2_regnr)));
#endif
  fprintf_unfiltered (file,
                      "gdbarch_dump: dwarf2_reg_to_regnum = <0x%lx>\n",
                      (long) current_gdbarch->dwarf2_reg_to_regnum);
#ifdef DWARF_REG_TO_REGNUM
  fprintf_unfiltered (file,
                      "gdbarch_dump: %s # %s\n",
                      "DWARF_REG_TO_REGNUM(dwarf_regnr)",
                      XSTRING (DWARF_REG_TO_REGNUM (dwarf_regnr)));
#endif
  fprintf_unfiltered (file,
                      "gdbarch_dump: dwarf_reg_to_regnum = <0x%lx>\n",
                      (long) current_gdbarch->dwarf_reg_to_regnum);
#ifdef ECOFF_REG_TO_REGNUM
  fprintf_unfiltered (file,
                      "gdbarch_dump: %s # %s\n",
                      "ECOFF_REG_TO_REGNUM(ecoff_regnr)",
                      XSTRING (ECOFF_REG_TO_REGNUM (ecoff_regnr)));
#endif
  fprintf_unfiltered (file,
                      "gdbarch_dump: ecoff_reg_to_regnum = <0x%lx>\n",
                      (long) current_gdbarch->ecoff_reg_to_regnum);
#ifdef ELF_MAKE_MSYMBOL_SPECIAL
  fprintf_unfiltered (file,
                      "gdbarch_dump: %s # %s\n",
                      "ELF_MAKE_MSYMBOL_SPECIAL(sym, msym)",
                      XSTRING (ELF_MAKE_MSYMBOL_SPECIAL (sym, msym)));
#endif
  fprintf_unfiltered (file,
                      "gdbarch_dump: elf_make_msymbol_special = <0x%lx>\n",
                      (long) current_gdbarch->elf_make_msymbol_special);
#ifdef EXTRACT_RETURN_VALUE
  fprintf_unfiltered (file,
                      "gdbarch_dump: %s # %s\n",
                      "EXTRACT_RETURN_VALUE(type, regcache, valbuf)",
                      XSTRING (EXTRACT_RETURN_VALUE (type, regcache, valbuf)));
#endif
  fprintf_unfiltered (file,
                      "gdbarch_dump: extract_return_value = <0x%lx>\n",
                      (long) current_gdbarch->extract_return_value);
#ifdef FETCH_POINTER_ARGUMENT_P
  fprintf_unfiltered (file,
                      "gdbarch_dump: %s # %s\n",
                      "FETCH_POINTER_ARGUMENT_P()",
                      XSTRING (FETCH_POINTER_ARGUMENT_P ()));
#endif
  fprintf_unfiltered (file,
                      "gdbarch_dump: gdbarch_fetch_pointer_argument_p() = %d\n",
                      gdbarch_fetch_pointer_argument_p (current_gdbarch));
#ifdef FETCH_POINTER_ARGUMENT
  fprintf_unfiltered (file,
                      "gdbarch_dump: %s # %s\n",
                      "FETCH_POINTER_ARGUMENT(frame, argi, type)",
                      XSTRING (FETCH_POINTER_ARGUMENT (frame, argi, type)));
#endif
  fprintf_unfiltered (file,
                      "gdbarch_dump: fetch_pointer_argument = <0x%lx>\n",
                      (long) current_gdbarch->fetch_pointer_argument);
#ifdef TARGET_FLOAT_BIT
  fprintf_unfiltered (file,
                      "gdbarch_dump: TARGET_FLOAT_BIT # %s\n",
                      XSTRING (TARGET_FLOAT_BIT));
#endif
  fprintf_unfiltered (file,
                      "gdbarch_dump: float_bit = %s\n",
                      paddr_d (current_gdbarch->float_bit));
#ifdef TARGET_FLOAT_FORMAT
  fprintf_unfiltered (file,
                      "gdbarch_dump: TARGET_FLOAT_FORMAT # %s\n",
                      XSTRING (TARGET_FLOAT_FORMAT));
#endif
  fprintf_unfiltered (file,
                      "gdbarch_dump: float_format = %s\n",
                      pformat (current_gdbarch->float_format));
#ifdef FP0_REGNUM
  fprintf_unfiltered (file,
                      "gdbarch_dump: FP0_REGNUM # %s\n",
                      XSTRING (FP0_REGNUM));
#endif
  fprintf_unfiltered (file,
                      "gdbarch_dump: fp0_regnum = %s\n",
                      paddr_d (current_gdbarch->fp0_regnum));
  fprintf_unfiltered (file,
                      "gdbarch_dump: gdbarch_frame_align_p() = %d\n",
                      gdbarch_frame_align_p (current_gdbarch));
  fprintf_unfiltered (file,
                      "gdbarch_dump: frame_align = <0x%lx>\n",
                      (long) current_gdbarch->frame_align);
#ifdef FRAME_ARGS_SKIP
  fprintf_unfiltered (file,
                      "gdbarch_dump: FRAME_ARGS_SKIP # %s\n",
                      XSTRING (FRAME_ARGS_SKIP));
#endif
  fprintf_unfiltered (file,
                      "gdbarch_dump: frame_args_skip = 0x%s\n",
                      paddr_nz (current_gdbarch->frame_args_skip));
#ifdef FRAME_NUM_ARGS_P
  fprintf_unfiltered (file,
                      "gdbarch_dump: %s # %s\n",
                      "FRAME_NUM_ARGS_P()",
                      XSTRING (FRAME_NUM_ARGS_P ()));
#endif
  fprintf_unfiltered (file,
                      "gdbarch_dump: gdbarch_frame_num_args_p() = %d\n",
                      gdbarch_frame_num_args_p (current_gdbarch));
#ifdef FRAME_NUM_ARGS
  fprintf_unfiltered (file,
                      "gdbarch_dump: %s # %s\n",
                      "FRAME_NUM_ARGS(frame)",
                      XSTRING (FRAME_NUM_ARGS (frame)));
#endif
  fprintf_unfiltered (file,
                      "gdbarch_dump: frame_num_args = <0x%lx>\n",
                      (long) current_gdbarch->frame_num_args);
#ifdef FRAME_RED_ZONE_SIZE
  fprintf_unfiltered (file,
                      "gdbarch_dump: FRAME_RED_ZONE_SIZE # %s\n",
                      XSTRING (FRAME_RED_ZONE_SIZE));
#endif
  fprintf_unfiltered (file,
                      "gdbarch_dump: frame_red_zone_size = %s\n",
                      paddr_d (current_gdbarch->frame_red_zone_size));
#ifdef GET_LONGJMP_TARGET_P
  fprintf_unfiltered (file,
                      "gdbarch_dump: %s # %s\n",
                      "GET_LONGJMP_TARGET_P()",
                      XSTRING (GET_LONGJMP_TARGET_P ()));
#endif
  fprintf_unfiltered (file,
                      "gdbarch_dump: gdbarch_get_longjmp_target_p() = %d\n",
                      gdbarch_get_longjmp_target_p (current_gdbarch));
#ifdef GET_LONGJMP_TARGET
  fprintf_unfiltered (file,
                      "gdbarch_dump: %s # %s\n",
                      "GET_LONGJMP_TARGET(pc)",
                      XSTRING (GET_LONGJMP_TARGET (pc)));
#endif
  fprintf_unfiltered (file,
                      "gdbarch_dump: get_longjmp_target = <0x%lx>\n",
                      (long) current_gdbarch->get_longjmp_target);
#ifdef HAVE_NONSTEPPABLE_WATCHPOINT
  fprintf_unfiltered (file,
                      "gdbarch_dump: HAVE_NONSTEPPABLE_WATCHPOINT # %s\n",
                      XSTRING (HAVE_NONSTEPPABLE_WATCHPOINT));
#endif
  fprintf_unfiltered (file,
                      "gdbarch_dump: have_nonsteppable_watchpoint = %s\n",
                      paddr_d (current_gdbarch->have_nonsteppable_watchpoint));
  fprintf_unfiltered (file,
                      "gdbarch_dump: in_function_epilogue_p = <0x%lx>\n",
                      (long) current_gdbarch->in_function_epilogue_p);
#ifdef IN_SOLIB_CALL_TRAMPOLINE
  fprintf_unfiltered (file,
                      "gdbarch_dump: %s # %s\n",
                      "IN_SOLIB_CALL_TRAMPOLINE(pc, name)",
                      XSTRING (IN_SOLIB_CALL_TRAMPOLINE (pc, name)));
#endif
  fprintf_unfiltered (file,
                      "gdbarch_dump: in_solib_call_trampoline = <0x%lx>\n",
                      (long) current_gdbarch->in_solib_call_trampoline);
#ifdef IN_SOLIB_RETURN_TRAMPOLINE
  fprintf_unfiltered (file,
                      "gdbarch_dump: %s # %s\n",
                      "IN_SOLIB_RETURN_TRAMPOLINE(pc, name)",
                      XSTRING (IN_SOLIB_RETURN_TRAMPOLINE (pc, name)));
#endif
  fprintf_unfiltered (file,
                      "gdbarch_dump: in_solib_return_trampoline = <0x%lx>\n",
                      (long) current_gdbarch->in_solib_return_trampoline);
#ifdef INNER_THAN
  fprintf_unfiltered (file,
                      "gdbarch_dump: %s # %s\n",
                      "INNER_THAN(lhs, rhs)",
                      XSTRING (INNER_THAN (lhs, rhs)));
#endif
  fprintf_unfiltered (file,
                      "gdbarch_dump: inner_than = <0x%lx>\n",
                      (long) current_gdbarch->inner_than);
#ifdef TARGET_INT_BIT
  fprintf_unfiltered (file,
                      "gdbarch_dump: TARGET_INT_BIT # %s\n",
                      XSTRING (TARGET_INT_BIT));
#endif
  fprintf_unfiltered (file,
                      "gdbarch_dump: int_bit = %s\n",
                      paddr_d (current_gdbarch->int_bit));
#ifdef INTEGER_TO_ADDRESS_P
  fprintf_unfiltered (file,
                      "gdbarch_dump: %s # %s\n",
                      "INTEGER_TO_ADDRESS_P()",
                      XSTRING (INTEGER_TO_ADDRESS_P ()));
#endif
  fprintf_unfiltered (file,
                      "gdbarch_dump: gdbarch_integer_to_address_p() = %d\n",
                      gdbarch_integer_to_address_p (current_gdbarch));
#ifdef INTEGER_TO_ADDRESS
  fprintf_unfiltered (file,
                      "gdbarch_dump: %s # %s\n",
                      "INTEGER_TO_ADDRESS(type, buf)",
                      XSTRING (INTEGER_TO_ADDRESS (type, buf)));
#endif
  fprintf_unfiltered (file,
                      "gdbarch_dump: integer_to_address = <0x%lx>\n",
                      (long) current_gdbarch->integer_to_address);
#ifdef TARGET_LONG_BIT
  fprintf_unfiltered (file,
                      "gdbarch_dump: TARGET_LONG_BIT # %s\n",
                      XSTRING (TARGET_LONG_BIT));
#endif
  fprintf_unfiltered (file,
                      "gdbarch_dump: long_bit = %s\n",
                      paddr_d (current_gdbarch->long_bit));
#ifdef TARGET_LONG_DOUBLE_BIT
  fprintf_unfiltered (file,
                      "gdbarch_dump: TARGET_LONG_DOUBLE_BIT # %s\n",
                      XSTRING (TARGET_LONG_DOUBLE_BIT));
#endif
  fprintf_unfiltered (file,
                      "gdbarch_dump: long_double_bit = %s\n",
                      paddr_d (current_gdbarch->long_double_bit));
#ifdef TARGET_LONG_DOUBLE_FORMAT
  fprintf_unfiltered (file,
                      "gdbarch_dump: TARGET_LONG_DOUBLE_FORMAT # %s\n",
                      XSTRING (TARGET_LONG_DOUBLE_FORMAT));
#endif
  fprintf_unfiltered (file,
                      "gdbarch_dump: long_double_format = %s\n",
                      pformat (current_gdbarch->long_double_format));
#ifdef TARGET_LONG_LONG_BIT
  fprintf_unfiltered (file,
                      "gdbarch_dump: TARGET_LONG_LONG_BIT # %s\n",
                      XSTRING (TARGET_LONG_LONG_BIT));
#endif
  fprintf_unfiltered (file,
                      "gdbarch_dump: long_long_bit = %s\n",
                      paddr_d (current_gdbarch->long_long_bit));
#ifdef MEMORY_INSERT_BREAKPOINT
  fprintf_unfiltered (file,
                      "gdbarch_dump: %s # %s\n",
                      "MEMORY_INSERT_BREAKPOINT(addr, contents_cache)",
                      XSTRING (MEMORY_INSERT_BREAKPOINT (addr, contents_cache)));
#endif
  fprintf_unfiltered (file,
                      "gdbarch_dump: memory_insert_breakpoint = <0x%lx>\n",
                      (long) current_gdbarch->memory_insert_breakpoint);
#ifdef MEMORY_REMOVE_BREAKPOINT
  fprintf_unfiltered (file,
                      "gdbarch_dump: %s # %s\n",
                      "MEMORY_REMOVE_BREAKPOINT(addr, contents_cache)",
                      XSTRING (MEMORY_REMOVE_BREAKPOINT (addr, contents_cache)));
#endif
  fprintf_unfiltered (file,
                      "gdbarch_dump: memory_remove_breakpoint = <0x%lx>\n",
                      (long) current_gdbarch->memory_remove_breakpoint);
#ifdef NAME_OF_MALLOC
  fprintf_unfiltered (file,
                      "gdbarch_dump: NAME_OF_MALLOC # %s\n",
                      XSTRING (NAME_OF_MALLOC));
#endif
  fprintf_unfiltered (file,
                      "gdbarch_dump: name_of_malloc = %s\n",
                      NAME_OF_MALLOC);
#ifdef NUM_PSEUDO_REGS
  fprintf_unfiltered (file,
                      "gdbarch_dump: NUM_PSEUDO_REGS # %s\n",
                      XSTRING (NUM_PSEUDO_REGS));
#endif
  fprintf_unfiltered (file,
                      "gdbarch_dump: num_pseudo_regs = %s\n",
                      paddr_d (current_gdbarch->num_pseudo_regs));
#ifdef NUM_REGS
  fprintf_unfiltered (file,
                      "gdbarch_dump: NUM_REGS # %s\n",
                      XSTRING (NUM_REGS));
#endif
  fprintf_unfiltered (file,
                      "gdbarch_dump: num_regs = %s\n",
                      paddr_d (current_gdbarch->num_regs));
#ifdef TARGET_OSABI
  fprintf_unfiltered (file,
                      "gdbarch_dump: TARGET_OSABI # %s\n",
                      XSTRING (TARGET_OSABI));
#endif
  fprintf_unfiltered (file,
                      "gdbarch_dump: osabi = %s\n",
                      paddr_d (current_gdbarch->osabi));
#ifdef PC_REGNUM
  fprintf_unfiltered (file,
                      "gdbarch_dump: PC_REGNUM # %s\n",
                      XSTRING (PC_REGNUM));
#endif
  fprintf_unfiltered (file,
                      "gdbarch_dump: pc_regnum = %s\n",
                      paddr_d (current_gdbarch->pc_regnum));
#ifdef POINTER_TO_ADDRESS
  fprintf_unfiltered (file,
                      "gdbarch_dump: %s # %s\n",
                      "POINTER_TO_ADDRESS(type, buf)",
                      XSTRING (POINTER_TO_ADDRESS (type, buf)));
#endif
  fprintf_unfiltered (file,
                      "gdbarch_dump: pointer_to_address = <0x%lx>\n",
                      (long) current_gdbarch->pointer_to_address);
  fprintf_unfiltered (file,
                      "gdbarch_dump: gdbarch_print_float_info_p() = %d\n",
                      gdbarch_print_float_info_p (current_gdbarch));
  fprintf_unfiltered (file,
                      "gdbarch_dump: print_float_info = <0x%lx>\n",
                      (long) current_gdbarch->print_float_info);
#ifdef TARGET_PRINT_INSN
  fprintf_unfiltered (file,
                      "gdbarch_dump: %s # %s\n",
                      "TARGET_PRINT_INSN(vma, info)",
                      XSTRING (TARGET_PRINT_INSN (vma, info)));
#endif
  fprintf_unfiltered (file,
                      "gdbarch_dump: print_insn = <0x%lx>\n",
                      (long) current_gdbarch->print_insn);
  fprintf_unfiltered (file,
                      "gdbarch_dump: print_registers_info = <0x%lx>\n",
                      (long) current_gdbarch->print_registers_info);
  fprintf_unfiltered (file,
                      "gdbarch_dump: gdbarch_print_vector_info_p() = %d\n",
                      gdbarch_print_vector_info_p (current_gdbarch));
  fprintf_unfiltered (file,
                      "gdbarch_dump: print_vector_info = <0x%lx>\n",
                      (long) current_gdbarch->print_vector_info);
#ifdef PS_REGNUM
  fprintf_unfiltered (file,
                      "gdbarch_dump: PS_REGNUM # %s\n",
                      XSTRING (PS_REGNUM));
#endif
  fprintf_unfiltered (file,
                      "gdbarch_dump: ps_regnum = %s\n",
                      paddr_d (current_gdbarch->ps_regnum));
  fprintf_unfiltered (file,
                      "gdbarch_dump: gdbarch_pseudo_register_read_p() = %d\n",
                      gdbarch_pseudo_register_read_p (current_gdbarch));
  fprintf_unfiltered (file,
                      "gdbarch_dump: pseudo_register_read = <0x%lx>\n",
                      (long) current_gdbarch->pseudo_register_read);
  fprintf_unfiltered (file,
                      "gdbarch_dump: gdbarch_pseudo_register_write_p() = %d\n",
                      gdbarch_pseudo_register_write_p (current_gdbarch));
  fprintf_unfiltered (file,
                      "gdbarch_dump: pseudo_register_write = <0x%lx>\n",
                      (long) current_gdbarch->pseudo_register_write);
#ifdef TARGET_PTR_BIT
  fprintf_unfiltered (file,
                      "gdbarch_dump: TARGET_PTR_BIT # %s\n",
                      XSTRING (TARGET_PTR_BIT));
#endif
  fprintf_unfiltered (file,
                      "gdbarch_dump: ptr_bit = %s\n",
                      paddr_d (current_gdbarch->ptr_bit));
  fprintf_unfiltered (file,
                      "gdbarch_dump: gdbarch_push_dummy_call_p() = %d\n",
                      gdbarch_push_dummy_call_p (current_gdbarch));
  fprintf_unfiltered (file,
                      "gdbarch_dump: push_dummy_call = <0x%lx>\n",
                      (long) current_gdbarch->push_dummy_call);
  fprintf_unfiltered (file,
                      "gdbarch_dump: gdbarch_push_dummy_code_p() = %d\n",
                      gdbarch_push_dummy_code_p (current_gdbarch));
  fprintf_unfiltered (file,
                      "gdbarch_dump: push_dummy_code = <0x%lx>\n",
                      (long) current_gdbarch->push_dummy_code);
#ifdef TARGET_READ_PC_P
  fprintf_unfiltered (file,
                      "gdbarch_dump: %s # %s\n",
                      "TARGET_READ_PC_P()",
                      XSTRING (TARGET_READ_PC_P ()));
#endif
  fprintf_unfiltered (file,
                      "gdbarch_dump: gdbarch_read_pc_p() = %d\n",
                      gdbarch_read_pc_p (current_gdbarch));
#ifdef TARGET_READ_PC
  fprintf_unfiltered (file,
                      "gdbarch_dump: %s # %s\n",
                      "TARGET_READ_PC(ptid)",
                      XSTRING (TARGET_READ_PC (ptid)));
#endif
  fprintf_unfiltered (file,
                      "gdbarch_dump: read_pc = <0x%lx>\n",
                      (long) current_gdbarch->read_pc);
#ifdef TARGET_READ_SP_P
  fprintf_unfiltered (file,
                      "gdbarch_dump: %s # %s\n",
                      "TARGET_READ_SP_P()",
                      XSTRING (TARGET_READ_SP_P ()));
#endif
  fprintf_unfiltered (file,
                      "gdbarch_dump: gdbarch_read_sp_p() = %d\n",
                      gdbarch_read_sp_p (current_gdbarch));
#ifdef TARGET_READ_SP
  fprintf_unfiltered (file,
                      "gdbarch_dump: %s # %s\n",
                      "TARGET_READ_SP()",
                      XSTRING (TARGET_READ_SP ()));
#endif
  fprintf_unfiltered (file,
                      "gdbarch_dump: read_sp = <0x%lx>\n",
                      (long) current_gdbarch->read_sp);
#ifdef REGISTER_BYTES_OK_P
  fprintf_unfiltered (file,
                      "gdbarch_dump: %s # %s\n",
                      "REGISTER_BYTES_OK_P()",
                      XSTRING (REGISTER_BYTES_OK_P ()));
#endif
  fprintf_unfiltered (file,
                      "gdbarch_dump: gdbarch_register_bytes_ok_p() = %d\n",
                      gdbarch_register_bytes_ok_p (current_gdbarch));
#ifdef REGISTER_BYTES_OK
  fprintf_unfiltered (file,
                      "gdbarch_dump: %s # %s\n",
                      "REGISTER_BYTES_OK(nr_bytes)",
                      XSTRING (REGISTER_BYTES_OK (nr_bytes)));
#endif
  fprintf_unfiltered (file,
                      "gdbarch_dump: register_bytes_ok = <0x%lx>\n",
                      (long) current_gdbarch->register_bytes_ok);
#ifdef REGISTER_NAME
  fprintf_unfiltered (file,
                      "gdbarch_dump: %s # %s\n",
                      "REGISTER_NAME(regnr)",
                      XSTRING (REGISTER_NAME (regnr)));
#endif
  fprintf_unfiltered (file,
                      "gdbarch_dump: register_name = <0x%lx>\n",
                      (long) current_gdbarch->register_name);
  fprintf_unfiltered (file,
                      "gdbarch_dump: register_reggroup_p = <0x%lx>\n",
                      (long) current_gdbarch->register_reggroup_p);
#ifdef REGISTER_SIM_REGNO
  fprintf_unfiltered (file,
                      "gdbarch_dump: %s # %s\n",
                      "REGISTER_SIM_REGNO(reg_nr)",
                      XSTRING (REGISTER_SIM_REGNO (reg_nr)));
#endif
  fprintf_unfiltered (file,
                      "gdbarch_dump: register_sim_regno = <0x%lx>\n",
                      (long) current_gdbarch->register_sim_regno);
#ifdef REGISTER_TO_VALUE
  fprintf_unfiltered (file,
                      "gdbarch_dump: %s # %s\n",
                      "REGISTER_TO_VALUE(frame, regnum, type, buf)",
                      XSTRING (REGISTER_TO_VALUE (frame, regnum, type, buf)));
#endif
  fprintf_unfiltered (file,
                      "gdbarch_dump: register_to_value = <0x%lx>\n",
                      (long) current_gdbarch->register_to_value);
  fprintf_unfiltered (file,
                      "gdbarch_dump: gdbarch_register_type_p() = %d\n",
                      gdbarch_register_type_p (current_gdbarch));
  fprintf_unfiltered (file,
                      "gdbarch_dump: register_type = <0x%lx>\n",
                      (long) current_gdbarch->register_type);
  fprintf_unfiltered (file,
                      "gdbarch_dump: gdbarch_regset_from_core_section_p() = %d\n",
                      gdbarch_regset_from_core_section_p (current_gdbarch));
  fprintf_unfiltered (file,
                      "gdbarch_dump: regset_from_core_section = <0x%lx>\n",
                      (long) current_gdbarch->regset_from_core_section);
  fprintf_unfiltered (file,
                      "gdbarch_dump: remote_translate_xfer_address = <0x%lx>\n",
                      (long) current_gdbarch->remote_translate_xfer_address);
  fprintf_unfiltered (file,
                      "gdbarch_dump: gdbarch_return_value_p() = %d\n",
                      gdbarch_return_value_p (current_gdbarch));
  fprintf_unfiltered (file,
                      "gdbarch_dump: return_value = <0x%lx>\n",
                      (long) current_gdbarch->return_value);
#ifdef SDB_REG_TO_REGNUM
  fprintf_unfiltered (file,
                      "gdbarch_dump: %s # %s\n",
                      "SDB_REG_TO_REGNUM(sdb_regnr)",
                      XSTRING (SDB_REG_TO_REGNUM (sdb_regnr)));
#endif
  fprintf_unfiltered (file,
                      "gdbarch_dump: sdb_reg_to_regnum = <0x%lx>\n",
                      (long) current_gdbarch->sdb_reg_to_regnum);
#ifdef TARGET_SHORT_BIT
  fprintf_unfiltered (file,
                      "gdbarch_dump: TARGET_SHORT_BIT # %s\n",
                      XSTRING (TARGET_SHORT_BIT));
#endif
  fprintf_unfiltered (file,
                      "gdbarch_dump: short_bit = %s\n",
                      paddr_d (current_gdbarch->short_bit));
#ifdef SKIP_PROLOGUE
  fprintf_unfiltered (file,
                      "gdbarch_dump: %s # %s\n",
                      "SKIP_PROLOGUE(ip)",
                      XSTRING (SKIP_PROLOGUE (ip)));
#endif
  fprintf_unfiltered (file,
                      "gdbarch_dump: skip_prologue = <0x%lx>\n",
                      (long) current_gdbarch->skip_prologue);
  fprintf_unfiltered (file,
                      "gdbarch_dump: skip_solib_resolver = <0x%lx>\n",
                      (long) current_gdbarch->skip_solib_resolver);
#ifdef SKIP_TRAMPOLINE_CODE
  fprintf_unfiltered (file,
                      "gdbarch_dump: %s # %s\n",
                      "SKIP_TRAMPOLINE_CODE(pc)",
                      XSTRING (SKIP_TRAMPOLINE_CODE (pc)));
#endif
  fprintf_unfiltered (file,
                      "gdbarch_dump: skip_trampoline_code = <0x%lx>\n",
                      (long) current_gdbarch->skip_trampoline_code);
#ifdef SMASH_TEXT_ADDRESS
  fprintf_unfiltered (file,
                      "gdbarch_dump: %s # %s\n",
                      "SMASH_TEXT_ADDRESS(addr)",
                      XSTRING (SMASH_TEXT_ADDRESS (addr)));
#endif
  fprintf_unfiltered (file,
                      "gdbarch_dump: smash_text_address = <0x%lx>\n",
                      (long) current_gdbarch->smash_text_address);
#ifdef SOFTWARE_SINGLE_STEP_P
  fprintf_unfiltered (file,
                      "gdbarch_dump: %s # %s\n",
                      "SOFTWARE_SINGLE_STEP_P()",
                      XSTRING (SOFTWARE_SINGLE_STEP_P ()));
#endif
  fprintf_unfiltered (file,
                      "gdbarch_dump: gdbarch_software_single_step_p() = %d\n",
                      gdbarch_software_single_step_p (current_gdbarch));
#ifdef SOFTWARE_SINGLE_STEP
  fprintf_unfiltered (file,
                      "gdbarch_dump: %s # %s\n",
                      "SOFTWARE_SINGLE_STEP(sig, insert_breakpoints_p)",
                      XSTRING (SOFTWARE_SINGLE_STEP (sig, insert_breakpoints_p)));
#endif
  fprintf_unfiltered (file,
                      "gdbarch_dump: software_single_step = <0x%lx>\n",
                      (long) current_gdbarch->software_single_step);
#ifdef SP_REGNUM
  fprintf_unfiltered (file,
                      "gdbarch_dump: SP_REGNUM # %s\n",
                      XSTRING (SP_REGNUM));
#endif
  fprintf_unfiltered (file,
                      "gdbarch_dump: sp_regnum = %s\n",
                      paddr_d (current_gdbarch->sp_regnum));
#ifdef STAB_REG_TO_REGNUM
  fprintf_unfiltered (file,
                      "gdbarch_dump: %s # %s\n",
                      "STAB_REG_TO_REGNUM(stab_regnr)",
                      XSTRING (STAB_REG_TO_REGNUM (stab_regnr)));
#endif
  fprintf_unfiltered (file,
                      "gdbarch_dump: stab_reg_to_regnum = <0x%lx>\n",
                      (long) current_gdbarch->stab_reg_to_regnum);
  fprintf_unfiltered (file,
                      "gdbarch_dump: stabs_argument_has_addr = <0x%lx>\n",
                      (long) current_gdbarch->stabs_argument_has_addr);
#ifdef STORE_RETURN_VALUE
  fprintf_unfiltered (file,
                      "gdbarch_dump: %s # %s\n",
                      "STORE_RETURN_VALUE(type, regcache, valbuf)",
                      XSTRING (STORE_RETURN_VALUE (type, regcache, valbuf)));
#endif
  fprintf_unfiltered (file,
                      "gdbarch_dump: store_return_value = <0x%lx>\n",
                      (long) current_gdbarch->store_return_value);
  fprintf_unfiltered (file,
                      "gdbarch_dump: gdbarch_unwind_dummy_id_p() = %d\n",
                      gdbarch_unwind_dummy_id_p (current_gdbarch));
  fprintf_unfiltered (file,
                      "gdbarch_dump: unwind_dummy_id = <0x%lx>\n",
                      (long) current_gdbarch->unwind_dummy_id);
  fprintf_unfiltered (file,
                      "gdbarch_dump: gdbarch_unwind_pc_p() = %d\n",
                      gdbarch_unwind_pc_p (current_gdbarch));
  fprintf_unfiltered (file,
                      "gdbarch_dump: unwind_pc = <0x%lx>\n",
                      (long) current_gdbarch->unwind_pc);
  fprintf_unfiltered (file,
                      "gdbarch_dump: gdbarch_unwind_sp_p() = %d\n",
                      gdbarch_unwind_sp_p (current_gdbarch));
  fprintf_unfiltered (file,
                      "gdbarch_dump: unwind_sp = <0x%lx>\n",
                      (long) current_gdbarch->unwind_sp);
#ifdef VALUE_TO_REGISTER
  fprintf_unfiltered (file,
                      "gdbarch_dump: %s # %s\n",
                      "VALUE_TO_REGISTER(frame, regnum, type, buf)",
                      XSTRING (VALUE_TO_REGISTER (frame, regnum, type, buf)));
#endif
  fprintf_unfiltered (file,
                      "gdbarch_dump: value_to_register = <0x%lx>\n",
                      (long) current_gdbarch->value_to_register);
#ifdef TARGET_VIRTUAL_FRAME_POINTER
  fprintf_unfiltered (file,
                      "gdbarch_dump: %s # %s\n",
                      "TARGET_VIRTUAL_FRAME_POINTER(pc, frame_regnum, frame_offset)",
                      XSTRING (TARGET_VIRTUAL_FRAME_POINTER (pc, frame_regnum, frame_offset)));
#endif
  fprintf_unfiltered (file,
                      "gdbarch_dump: virtual_frame_pointer = <0x%lx>\n",
                      (long) current_gdbarch->virtual_frame_pointer);
#ifdef TARGET_WRITE_PC
  fprintf_unfiltered (file,
                      "gdbarch_dump: %s # %s\n",
                      "TARGET_WRITE_PC(val, ptid)",
                      XSTRING (TARGET_WRITE_PC (val, ptid)));
#endif
  fprintf_unfiltered (file,
                      "gdbarch_dump: write_pc = <0x%lx>\n",
                      (long) current_gdbarch->write_pc);
  if (current_gdbarch->dump_tdep != NULL)
    current_gdbarch->dump_tdep (current_gdbarch, file);
}

struct gdbarch_tdep *
gdbarch_tdep (struct gdbarch *gdbarch)
{
  if (gdbarch_debug >= 2)
    fprintf_unfiltered (gdb_stdlog, "gdbarch_tdep called\n");
  return gdbarch->tdep;
}


const struct bfd_arch_info *
gdbarch_bfd_arch_info (struct gdbarch *gdbarch)
{
  gdb_assert (gdbarch != NULL);
  if (gdbarch_debug >= 2)
    fprintf_unfiltered (gdb_stdlog, "gdbarch_bfd_arch_info called\n");
  return gdbarch->bfd_arch_info;
}

int
gdbarch_byte_order (struct gdbarch *gdbarch)
{
  gdb_assert (gdbarch != NULL);
  if (gdbarch_debug >= 2)
    fprintf_unfiltered (gdb_stdlog, "gdbarch_byte_order called\n");
  return gdbarch->byte_order;
}

enum gdb_osabi
gdbarch_osabi (struct gdbarch *gdbarch)
{
  gdb_assert (gdbarch != NULL);
  if (gdbarch_debug >= 2)
    fprintf_unfiltered (gdb_stdlog, "gdbarch_osabi called\n");
  return gdbarch->osabi;
}

int
gdbarch_short_bit (struct gdbarch *gdbarch)
{
  gdb_assert (gdbarch != NULL);
  /* Skip verify of short_bit, invalid_p == 0 */
  if (gdbarch_debug >= 2)
    fprintf_unfiltered (gdb_stdlog, "gdbarch_short_bit called\n");
  return gdbarch->short_bit;
}

void
set_gdbarch_short_bit (struct gdbarch *gdbarch,
                       int short_bit)
{
  gdbarch->short_bit = short_bit;
}

int
gdbarch_int_bit (struct gdbarch *gdbarch)
{
  gdb_assert (gdbarch != NULL);
  /* Skip verify of int_bit, invalid_p == 0 */
  if (gdbarch_debug >= 2)
    fprintf_unfiltered (gdb_stdlog, "gdbarch_int_bit called\n");
  return gdbarch->int_bit;
}

void
set_gdbarch_int_bit (struct gdbarch *gdbarch,
                     int int_bit)
{
  gdbarch->int_bit = int_bit;
}

int
gdbarch_long_bit (struct gdbarch *gdbarch)
{
  gdb_assert (gdbarch != NULL);
  /* Skip verify of long_bit, invalid_p == 0 */
  if (gdbarch_debug >= 2)
    fprintf_unfiltered (gdb_stdlog, "gdbarch_long_bit called\n");
  return gdbarch->long_bit;
}

void
set_gdbarch_long_bit (struct gdbarch *gdbarch,
                      int long_bit)
{
  gdbarch->long_bit = long_bit;
}

int
gdbarch_long_long_bit (struct gdbarch *gdbarch)
{
  gdb_assert (gdbarch != NULL);
  /* Skip verify of long_long_bit, invalid_p == 0 */
  if (gdbarch_debug >= 2)
    fprintf_unfiltered (gdb_stdlog, "gdbarch_long_long_bit called\n");
  return gdbarch->long_long_bit;
}

void
set_gdbarch_long_long_bit (struct gdbarch *gdbarch,
                           int long_long_bit)
{
  gdbarch->long_long_bit = long_long_bit;
}

int
gdbarch_float_bit (struct gdbarch *gdbarch)
{
  gdb_assert (gdbarch != NULL);
  /* Skip verify of float_bit, invalid_p == 0 */
  if (gdbarch_debug >= 2)
    fprintf_unfiltered (gdb_stdlog, "gdbarch_float_bit called\n");
  return gdbarch->float_bit;
}

void
set_gdbarch_float_bit (struct gdbarch *gdbarch,
                       int float_bit)
{
  gdbarch->float_bit = float_bit;
}

const struct floatformat *
gdbarch_float_format (struct gdbarch *gdbarch)
{
  gdb_assert (gdbarch != NULL);
  if (gdbarch_debug >= 2)
    fprintf_unfiltered (gdb_stdlog, "gdbarch_float_format called\n");
  return gdbarch->float_format;
}

void
set_gdbarch_float_format (struct gdbarch *gdbarch,
                          const struct floatformat * float_format)
{
  gdbarch->float_format = float_format;
}

int
gdbarch_double_bit (struct gdbarch *gdbarch)
{
  gdb_assert (gdbarch != NULL);
  /* Skip verify of double_bit, invalid_p == 0 */
  if (gdbarch_debug >= 2)
    fprintf_unfiltered (gdb_stdlog, "gdbarch_double_bit called\n");
  return gdbarch->double_bit;
}

void
set_gdbarch_double_bit (struct gdbarch *gdbarch,
                        int double_bit)
{
  gdbarch->double_bit = double_bit;
}

const struct floatformat *
gdbarch_double_format (struct gdbarch *gdbarch)
{
  gdb_assert (gdbarch != NULL);
  if (gdbarch_debug >= 2)
    fprintf_unfiltered (gdb_stdlog, "gdbarch_double_format called\n");
  return gdbarch->double_format;
}

void
set_gdbarch_double_format (struct gdbarch *gdbarch,
                           const struct floatformat * double_format)
{
  gdbarch->double_format = double_format;
}

int
gdbarch_long_double_bit (struct gdbarch *gdbarch)
{
  gdb_assert (gdbarch != NULL);
  /* Skip verify of long_double_bit, invalid_p == 0 */
  if (gdbarch_debug >= 2)
    fprintf_unfiltered (gdb_stdlog, "gdbarch_long_double_bit called\n");
  return gdbarch->long_double_bit;
}

void
set_gdbarch_long_double_bit (struct gdbarch *gdbarch,
                             int long_double_bit)
{
  gdbarch->long_double_bit = long_double_bit;
}

const struct floatformat *
gdbarch_long_double_format (struct gdbarch *gdbarch)
{
  gdb_assert (gdbarch != NULL);
  if (gdbarch_debug >= 2)
    fprintf_unfiltered (gdb_stdlog, "gdbarch_long_double_format called\n");
  return gdbarch->long_double_format;
}

void
set_gdbarch_long_double_format (struct gdbarch *gdbarch,
                                const struct floatformat * long_double_format)
{
  gdbarch->long_double_format = long_double_format;
}

int
gdbarch_ptr_bit (struct gdbarch *gdbarch)
{
  gdb_assert (gdbarch != NULL);
  /* Skip verify of ptr_bit, invalid_p == 0 */
  if (gdbarch_debug >= 2)
    fprintf_unfiltered (gdb_stdlog, "gdbarch_ptr_bit called\n");
  return gdbarch->ptr_bit;
}

void
set_gdbarch_ptr_bit (struct gdbarch *gdbarch,
                     int ptr_bit)
{
  gdbarch->ptr_bit = ptr_bit;
}

int
gdbarch_addr_bit (struct gdbarch *gdbarch)
{
  gdb_assert (gdbarch != NULL);
  /* Check variable changed from pre-default.  */
  gdb_assert (gdbarch->addr_bit != 0);
  if (gdbarch_debug >= 2)
    fprintf_unfiltered (gdb_stdlog, "gdbarch_addr_bit called\n");
  return gdbarch->addr_bit;
}

void
set_gdbarch_addr_bit (struct gdbarch *gdbarch,
                      int addr_bit)
{
  gdbarch->addr_bit = addr_bit;
}

int
gdbarch_bfd_vma_bit (struct gdbarch *gdbarch)
{
  gdb_assert (gdbarch != NULL);
  /* Skip verify of bfd_vma_bit, invalid_p == 0 */
  if (gdbarch_debug >= 2)
    fprintf_unfiltered (gdb_stdlog, "gdbarch_bfd_vma_bit called\n");
  return gdbarch->bfd_vma_bit;
}

void
set_gdbarch_bfd_vma_bit (struct gdbarch *gdbarch,
                         int bfd_vma_bit)
{
  gdbarch->bfd_vma_bit = bfd_vma_bit;
}

int
gdbarch_char_signed (struct gdbarch *gdbarch)
{
  gdb_assert (gdbarch != NULL);
  /* Check variable changed from pre-default.  */
  gdb_assert (gdbarch->char_signed != -1);
  if (gdbarch_debug >= 2)
    fprintf_unfiltered (gdb_stdlog, "gdbarch_char_signed called\n");
  return gdbarch->char_signed;
}

void
set_gdbarch_char_signed (struct gdbarch *gdbarch,
                         int char_signed)
{
  gdbarch->char_signed = char_signed;
}

int
gdbarch_read_pc_p (struct gdbarch *gdbarch)
{
  gdb_assert (gdbarch != NULL);
  return gdbarch->read_pc != NULL;
}

CORE_ADDR
gdbarch_read_pc (struct gdbarch *gdbarch, ptid_t ptid)
{
  gdb_assert (gdbarch != NULL);
  gdb_assert (gdbarch->read_pc != NULL);
  if (gdbarch_debug >= 2)
    fprintf_unfiltered (gdb_stdlog, "gdbarch_read_pc called\n");
  return gdbarch->read_pc (ptid);
}

void
set_gdbarch_read_pc (struct gdbarch *gdbarch,
                     gdbarch_read_pc_ftype read_pc)
{
  gdbarch->read_pc = read_pc;
}

void
gdbarch_write_pc (struct gdbarch *gdbarch, CORE_ADDR val, ptid_t ptid)
{
  gdb_assert (gdbarch != NULL);
  gdb_assert (gdbarch->write_pc != NULL);
  if (gdbarch_debug >= 2)
    fprintf_unfiltered (gdb_stdlog, "gdbarch_write_pc called\n");
  gdbarch->write_pc (val, ptid);
}

void
set_gdbarch_write_pc (struct gdbarch *gdbarch,
                      gdbarch_write_pc_ftype write_pc)
{
  gdbarch->write_pc = write_pc;
}

int
gdbarch_read_sp_p (struct gdbarch *gdbarch)
{
  gdb_assert (gdbarch != NULL);
  return gdbarch->read_sp != NULL;
}

CORE_ADDR
gdbarch_read_sp (struct gdbarch *gdbarch)
{
  gdb_assert (gdbarch != NULL);
  gdb_assert (gdbarch->read_sp != NULL);
  if (gdbarch_debug >= 2)
    fprintf_unfiltered (gdb_stdlog, "gdbarch_read_sp called\n");
  return gdbarch->read_sp ();
}

void
set_gdbarch_read_sp (struct gdbarch *gdbarch,
                     gdbarch_read_sp_ftype read_sp)
{
  gdbarch->read_sp = read_sp;
}

void
gdbarch_virtual_frame_pointer (struct gdbarch *gdbarch, CORE_ADDR pc, int *frame_regnum, LONGEST *frame_offset)
{
  gdb_assert (gdbarch != NULL);
  gdb_assert (gdbarch->virtual_frame_pointer != NULL);
  if (gdbarch_debug >= 2)
    fprintf_unfiltered (gdb_stdlog, "gdbarch_virtual_frame_pointer called\n");
  gdbarch->virtual_frame_pointer (pc, frame_regnum, frame_offset);
}

void
set_gdbarch_virtual_frame_pointer (struct gdbarch *gdbarch,
                                   gdbarch_virtual_frame_pointer_ftype virtual_frame_pointer)
{
  gdbarch->virtual_frame_pointer = virtual_frame_pointer;
}

int
gdbarch_pseudo_register_read_p (struct gdbarch *gdbarch)
{
  gdb_assert (gdbarch != NULL);
  return gdbarch->pseudo_register_read != NULL;
}

void
gdbarch_pseudo_register_read (struct gdbarch *gdbarch, struct regcache *regcache, int cookednum, void *buf)
{
  gdb_assert (gdbarch != NULL);
  gdb_assert (gdbarch->pseudo_register_read != NULL);
  if (gdbarch_debug >= 2)
    fprintf_unfiltered (gdb_stdlog, "gdbarch_pseudo_register_read called\n");
  gdbarch->pseudo_register_read (gdbarch, regcache, cookednum, buf);
}

void
set_gdbarch_pseudo_register_read (struct gdbarch *gdbarch,
                                  gdbarch_pseudo_register_read_ftype pseudo_register_read)
{
  gdbarch->pseudo_register_read = pseudo_register_read;
}

int
gdbarch_pseudo_register_write_p (struct gdbarch *gdbarch)
{
  gdb_assert (gdbarch != NULL);
  return gdbarch->pseudo_register_write != NULL;
}

void
gdbarch_pseudo_register_write (struct gdbarch *gdbarch, struct regcache *regcache, int cookednum, const void *buf)
{
  gdb_assert (gdbarch != NULL);
  gdb_assert (gdbarch->pseudo_register_write != NULL);
  if (gdbarch_debug >= 2)
    fprintf_unfiltered (gdb_stdlog, "gdbarch_pseudo_register_write called\n");
  gdbarch->pseudo_register_write (gdbarch, regcache, cookednum, buf);
}

void
set_gdbarch_pseudo_register_write (struct gdbarch *gdbarch,
                                   gdbarch_pseudo_register_write_ftype pseudo_register_write)
{
  gdbarch->pseudo_register_write = pseudo_register_write;
}

int
gdbarch_num_regs (struct gdbarch *gdbarch)
{
  gdb_assert (gdbarch != NULL);
  /* Check variable changed from pre-default.  */
  gdb_assert (gdbarch->num_regs != -1);
  if (gdbarch_debug >= 2)
    fprintf_unfiltered (gdb_stdlog, "gdbarch_num_regs called\n");
  return gdbarch->num_regs;
}

void
set_gdbarch_num_regs (struct gdbarch *gdbarch,
                      int num_regs)
{
  gdbarch->num_regs = num_regs;
}

int
gdbarch_num_pseudo_regs (struct gdbarch *gdbarch)
{
  gdb_assert (gdbarch != NULL);
  /* Skip verify of num_pseudo_regs, invalid_p == 0 */
  if (gdbarch_debug >= 2)
    fprintf_unfiltered (gdb_stdlog, "gdbarch_num_pseudo_regs called\n");
  return gdbarch->num_pseudo_regs;
}

void
set_gdbarch_num_pseudo_regs (struct gdbarch *gdbarch,
                             int num_pseudo_regs)
{
  gdbarch->num_pseudo_regs = num_pseudo_regs;
}

int
gdbarch_sp_regnum (struct gdbarch *gdbarch)
{
  gdb_assert (gdbarch != NULL);
  /* Skip verify of sp_regnum, invalid_p == 0 */
  if (gdbarch_debug >= 2)
    fprintf_unfiltered (gdb_stdlog, "gdbarch_sp_regnum called\n");
  return gdbarch->sp_regnum;
}

void
set_gdbarch_sp_regnum (struct gdbarch *gdbarch,
                       int sp_regnum)
{
  gdbarch->sp_regnum = sp_regnum;
}

int
gdbarch_pc_regnum (struct gdbarch *gdbarch)
{
  gdb_assert (gdbarch != NULL);
  /* Skip verify of pc_regnum, invalid_p == 0 */
  if (gdbarch_debug >= 2)
    fprintf_unfiltered (gdb_stdlog, "gdbarch_pc_regnum called\n");
  return gdbarch->pc_regnum;
}

void
set_gdbarch_pc_regnum (struct gdbarch *gdbarch,
                       int pc_regnum)
{
  gdbarch->pc_regnum = pc_regnum;
}

int
gdbarch_ps_regnum (struct gdbarch *gdbarch)
{
  gdb_assert (gdbarch != NULL);
  /* Skip verify of ps_regnum, invalid_p == 0 */
  if (gdbarch_debug >= 2)
    fprintf_unfiltered (gdb_stdlog, "gdbarch_ps_regnum called\n");
  return gdbarch->ps_regnum;
}

void
set_gdbarch_ps_regnum (struct gdbarch *gdbarch,
                       int ps_regnum)
{
  gdbarch->ps_regnum = ps_regnum;
}

int
gdbarch_fp0_regnum (struct gdbarch *gdbarch)
{
  gdb_assert (gdbarch != NULL);
  /* Skip verify of fp0_regnum, invalid_p == 0 */
  if (gdbarch_debug >= 2)
    fprintf_unfiltered (gdb_stdlog, "gdbarch_fp0_regnum called\n");
  return gdbarch->fp0_regnum;
}

void
set_gdbarch_fp0_regnum (struct gdbarch *gdbarch,
                        int fp0_regnum)
{
  gdbarch->fp0_regnum = fp0_regnum;
}

int
gdbarch_stab_reg_to_regnum (struct gdbarch *gdbarch, int stab_regnr)
{
  gdb_assert (gdbarch != NULL);
  gdb_assert (gdbarch->stab_reg_to_regnum != NULL);
  if (gdbarch_debug >= 2)
    fprintf_unfiltered (gdb_stdlog, "gdbarch_stab_reg_to_regnum called\n");
  return gdbarch->stab_reg_to_regnum (stab_regnr);
}

void
set_gdbarch_stab_reg_to_regnum (struct gdbarch *gdbarch,
                                gdbarch_stab_reg_to_regnum_ftype stab_reg_to_regnum)
{
  gdbarch->stab_reg_to_regnum = stab_reg_to_regnum;
}

int
gdbarch_ecoff_reg_to_regnum (struct gdbarch *gdbarch, int ecoff_regnr)
{
  gdb_assert (gdbarch != NULL);
  gdb_assert (gdbarch->ecoff_reg_to_regnum != NULL);
  if (gdbarch_debug >= 2)
    fprintf_unfiltered (gdb_stdlog, "gdbarch_ecoff_reg_to_regnum called\n");
  return gdbarch->ecoff_reg_to_regnum (ecoff_regnr);
}

void
set_gdbarch_ecoff_reg_to_regnum (struct gdbarch *gdbarch,
                                 gdbarch_ecoff_reg_to_regnum_ftype ecoff_reg_to_regnum)
{
  gdbarch->ecoff_reg_to_regnum = ecoff_reg_to_regnum;
}

int
gdbarch_dwarf_reg_to_regnum (struct gdbarch *gdbarch, int dwarf_regnr)
{
  gdb_assert (gdbarch != NULL);
  gdb_assert (gdbarch->dwarf_reg_to_regnum != NULL);
  if (gdbarch_debug >= 2)
    fprintf_unfiltered (gdb_stdlog, "gdbarch_dwarf_reg_to_regnum called\n");
  return gdbarch->dwarf_reg_to_regnum (dwarf_regnr);
}

void
set_gdbarch_dwarf_reg_to_regnum (struct gdbarch *gdbarch,
                                 gdbarch_dwarf_reg_to_regnum_ftype dwarf_reg_to_regnum)
{
  gdbarch->dwarf_reg_to_regnum = dwarf_reg_to_regnum;
}

int
gdbarch_sdb_reg_to_regnum (struct gdbarch *gdbarch, int sdb_regnr)
{
  gdb_assert (gdbarch != NULL);
  gdb_assert (gdbarch->sdb_reg_to_regnum != NULL);
  if (gdbarch_debug >= 2)
    fprintf_unfiltered (gdb_stdlog, "gdbarch_sdb_reg_to_regnum called\n");
  return gdbarch->sdb_reg_to_regnum (sdb_regnr);
}

void
set_gdbarch_sdb_reg_to_regnum (struct gdbarch *gdbarch,
                               gdbarch_sdb_reg_to_regnum_ftype sdb_reg_to_regnum)
{
  gdbarch->sdb_reg_to_regnum = sdb_reg_to_regnum;
}

int
gdbarch_dwarf2_reg_to_regnum (struct gdbarch *gdbarch, int dwarf2_regnr)
{
  gdb_assert (gdbarch != NULL);
  gdb_assert (gdbarch->dwarf2_reg_to_regnum != NULL);
  if (gdbarch_debug >= 2)
    fprintf_unfiltered (gdb_stdlog, "gdbarch_dwarf2_reg_to_regnum called\n");
  return gdbarch->dwarf2_reg_to_regnum (dwarf2_regnr);
}

void
set_gdbarch_dwarf2_reg_to_regnum (struct gdbarch *gdbarch,
                                  gdbarch_dwarf2_reg_to_regnum_ftype dwarf2_reg_to_regnum)
{
  gdbarch->dwarf2_reg_to_regnum = dwarf2_reg_to_regnum;
}

const char *
gdbarch_register_name (struct gdbarch *gdbarch, int regnr)
{
  gdb_assert (gdbarch != NULL);
  gdb_assert (gdbarch->register_name != NULL);
  if (gdbarch_debug >= 2)
    fprintf_unfiltered (gdb_stdlog, "gdbarch_register_name called\n");
  return gdbarch->register_name (regnr);
}

void
set_gdbarch_register_name (struct gdbarch *gdbarch,
                           gdbarch_register_name_ftype register_name)
{
  gdbarch->register_name = register_name;
}

int
gdbarch_register_type_p (struct gdbarch *gdbarch)
{
  gdb_assert (gdbarch != NULL);
  return gdbarch->register_type != NULL;
}

struct type *
gdbarch_register_type (struct gdbarch *gdbarch, int reg_nr)
{
  gdb_assert (gdbarch != NULL);
  gdb_assert (gdbarch->register_type != NULL);
  if (gdbarch_debug >= 2)
    fprintf_unfiltered (gdb_stdlog, "gdbarch_register_type called\n");
  return gdbarch->register_type (gdbarch, reg_nr);
}

void
set_gdbarch_register_type (struct gdbarch *gdbarch,
                           gdbarch_register_type_ftype register_type)
{
  gdbarch->register_type = register_type;
}

int
gdbarch_deprecated_register_byte_p (struct gdbarch *gdbarch)
{
  gdb_assert (gdbarch != NULL);
  return gdbarch->deprecated_register_byte != generic_register_byte;
}

int
gdbarch_deprecated_register_byte (struct gdbarch *gdbarch, int reg_nr)
{
  gdb_assert (gdbarch != NULL);
  gdb_assert (gdbarch->deprecated_register_byte != NULL);
  /* Do not check predicate: gdbarch->deprecated_register_byte != generic_register_byte, allow call.  */
  if (gdbarch_debug >= 2)
    fprintf_unfiltered (gdb_stdlog, "gdbarch_deprecated_register_byte called\n");
  return gdbarch->deprecated_register_byte (reg_nr);
}

void
set_gdbarch_deprecated_register_byte (struct gdbarch *gdbarch,
                                      gdbarch_deprecated_register_byte_ftype deprecated_register_byte)
{
  gdbarch->deprecated_register_byte = deprecated_register_byte;
}

int
gdbarch_unwind_dummy_id_p (struct gdbarch *gdbarch)
{
  gdb_assert (gdbarch != NULL);
  return gdbarch->unwind_dummy_id != NULL;
}

struct frame_id
gdbarch_unwind_dummy_id (struct gdbarch *gdbarch, struct frame_info *info)
{
  gdb_assert (gdbarch != NULL);
  gdb_assert (gdbarch->unwind_dummy_id != NULL);
  if (gdbarch_debug >= 2)
    fprintf_unfiltered (gdb_stdlog, "gdbarch_unwind_dummy_id called\n");
  return gdbarch->unwind_dummy_id (gdbarch, info);
}

void
set_gdbarch_unwind_dummy_id (struct gdbarch *gdbarch,
                             gdbarch_unwind_dummy_id_ftype unwind_dummy_id)
{
  gdbarch->unwind_dummy_id = unwind_dummy_id;
}

int
gdbarch_deprecated_fp_regnum (struct gdbarch *gdbarch)
{
  gdb_assert (gdbarch != NULL);
  /* Skip verify of deprecated_fp_regnum, invalid_p == 0 */
  if (gdbarch_debug >= 2)
    fprintf_unfiltered (gdb_stdlog, "gdbarch_deprecated_fp_regnum called\n");
  return gdbarch->deprecated_fp_regnum;
}

void
set_gdbarch_deprecated_fp_regnum (struct gdbarch *gdbarch,
                                  int deprecated_fp_regnum)
{
  gdbarch->deprecated_fp_regnum = deprecated_fp_regnum;
}

int
gdbarch_push_dummy_call_p (struct gdbarch *gdbarch)
{
  gdb_assert (gdbarch != NULL);
  return gdbarch->push_dummy_call != NULL;
}

CORE_ADDR
gdbarch_push_dummy_call (struct gdbarch *gdbarch, struct value *function, struct regcache *regcache, CORE_ADDR bp_addr, int nargs, struct value **args, CORE_ADDR sp, int struct_return, CORE_ADDR struct_addr)
{
  gdb_assert (gdbarch != NULL);
  gdb_assert (gdbarch->push_dummy_call != NULL);
  if (gdbarch_debug >= 2)
    fprintf_unfiltered (gdb_stdlog, "gdbarch_push_dummy_call called\n");
  return gdbarch->push_dummy_call (gdbarch, function, regcache, bp_addr, nargs, args, sp, struct_return, struct_addr);
}

void
set_gdbarch_push_dummy_call (struct gdbarch *gdbarch,
                             gdbarch_push_dummy_call_ftype push_dummy_call)
{
  gdbarch->push_dummy_call = push_dummy_call;
}

int
gdbarch_deprecated_push_arguments_p (struct gdbarch *gdbarch)
{
  gdb_assert (gdbarch != NULL);
  return gdbarch->deprecated_push_arguments != NULL;
}

CORE_ADDR
gdbarch_deprecated_push_arguments (struct gdbarch *gdbarch, int nargs, struct value **args, CORE_ADDR sp, int struct_return, CORE_ADDR struct_addr)
{
  gdb_assert (gdbarch != NULL);
  gdb_assert (gdbarch->deprecated_push_arguments != NULL);
  if (gdbarch_debug >= 2)
    fprintf_unfiltered (gdb_stdlog, "gdbarch_deprecated_push_arguments called\n");
  return gdbarch->deprecated_push_arguments (nargs, args, sp, struct_return, struct_addr);
}

void
set_gdbarch_deprecated_push_arguments (struct gdbarch *gdbarch,
                                       gdbarch_deprecated_push_arguments_ftype deprecated_push_arguments)
{
  gdbarch->deprecated_push_arguments = deprecated_push_arguments;
}

int
gdbarch_deprecated_register_size (struct gdbarch *gdbarch)
{
  gdb_assert (gdbarch != NULL);
  if (gdbarch_debug >= 2)
    fprintf_unfiltered (gdb_stdlog, "gdbarch_deprecated_register_size called\n");
  return gdbarch->deprecated_register_size;
}

void
set_gdbarch_deprecated_register_size (struct gdbarch *gdbarch,
                                      int deprecated_register_size)
{
  gdbarch->deprecated_register_size = deprecated_register_size;
}

int
gdbarch_call_dummy_location (struct gdbarch *gdbarch)
{
  gdb_assert (gdbarch != NULL);
  /* Skip verify of call_dummy_location, invalid_p == 0 */
  if (gdbarch_debug >= 2)
    fprintf_unfiltered (gdb_stdlog, "gdbarch_call_dummy_location called\n");
  return gdbarch->call_dummy_location;
}

void
set_gdbarch_call_dummy_location (struct gdbarch *gdbarch,
                                 int call_dummy_location)
{
  gdbarch->call_dummy_location = call_dummy_location;
}

int
gdbarch_push_dummy_code_p (struct gdbarch *gdbarch)
{
  gdb_assert (gdbarch != NULL);
  return gdbarch->push_dummy_code != NULL;
}

CORE_ADDR
gdbarch_push_dummy_code (struct gdbarch *gdbarch, CORE_ADDR sp, CORE_ADDR funaddr, int using_gcc, struct value **args, int nargs, struct type *value_type, CORE_ADDR *real_pc, CORE_ADDR *bp_addr)
{
  gdb_assert (gdbarch != NULL);
  gdb_assert (gdbarch->push_dummy_code != NULL);
  if (gdbarch_debug >= 2)
    fprintf_unfiltered (gdb_stdlog, "gdbarch_push_dummy_code called\n");
  return gdbarch->push_dummy_code (gdbarch, sp, funaddr, using_gcc, args, nargs, value_type, real_pc, bp_addr);
}

void
set_gdbarch_push_dummy_code (struct gdbarch *gdbarch,
                             gdbarch_push_dummy_code_ftype push_dummy_code)
{
  gdbarch->push_dummy_code = push_dummy_code;
}

void
gdbarch_print_registers_info (struct gdbarch *gdbarch, struct ui_file *file, struct frame_info *frame, int regnum, int all)
{
  gdb_assert (gdbarch != NULL);
  gdb_assert (gdbarch->print_registers_info != NULL);
  if (gdbarch_debug >= 2)
    fprintf_unfiltered (gdb_stdlog, "gdbarch_print_registers_info called\n");
  gdbarch->print_registers_info (gdbarch, file, frame, regnum, all);
}

void
set_gdbarch_print_registers_info (struct gdbarch *gdbarch,
                                  gdbarch_print_registers_info_ftype print_registers_info)
{
  gdbarch->print_registers_info = print_registers_info;
}

int
gdbarch_print_float_info_p (struct gdbarch *gdbarch)
{
  gdb_assert (gdbarch != NULL);
  return gdbarch->print_float_info != NULL;
}

void
gdbarch_print_float_info (struct gdbarch *gdbarch, struct ui_file *file, struct frame_info *frame, const char *args)
{
  gdb_assert (gdbarch != NULL);
  gdb_assert (gdbarch->print_float_info != NULL);
  if (gdbarch_debug >= 2)
    fprintf_unfiltered (gdb_stdlog, "gdbarch_print_float_info called\n");
  gdbarch->print_float_info (gdbarch, file, frame, args);
}

void
set_gdbarch_print_float_info (struct gdbarch *gdbarch,
                              gdbarch_print_float_info_ftype print_float_info)
{
  gdbarch->print_float_info = print_float_info;
}

int
gdbarch_print_vector_info_p (struct gdbarch *gdbarch)
{
  gdb_assert (gdbarch != NULL);
  return gdbarch->print_vector_info != NULL;
}

void
gdbarch_print_vector_info (struct gdbarch *gdbarch, struct ui_file *file, struct frame_info *frame, const char *args)
{
  gdb_assert (gdbarch != NULL);
  gdb_assert (gdbarch->print_vector_info != NULL);
  if (gdbarch_debug >= 2)
    fprintf_unfiltered (gdb_stdlog, "gdbarch_print_vector_info called\n");
  gdbarch->print_vector_info (gdbarch, file, frame, args);
}

void
set_gdbarch_print_vector_info (struct gdbarch *gdbarch,
                               gdbarch_print_vector_info_ftype print_vector_info)
{
  gdbarch->print_vector_info = print_vector_info;
}

int
gdbarch_register_sim_regno (struct gdbarch *gdbarch, int reg_nr)
{
  gdb_assert (gdbarch != NULL);
  gdb_assert (gdbarch->register_sim_regno != NULL);
  if (gdbarch_debug >= 2)
    fprintf_unfiltered (gdb_stdlog, "gdbarch_register_sim_regno called\n");
  return gdbarch->register_sim_regno (reg_nr);
}

void
set_gdbarch_register_sim_regno (struct gdbarch *gdbarch,
                                gdbarch_register_sim_regno_ftype register_sim_regno)
{
  gdbarch->register_sim_regno = register_sim_regno;
}

int
gdbarch_register_bytes_ok_p (struct gdbarch *gdbarch)
{
  gdb_assert (gdbarch != NULL);
  return gdbarch->register_bytes_ok != NULL;
}

int
gdbarch_register_bytes_ok (struct gdbarch *gdbarch, long nr_bytes)
{
  gdb_assert (gdbarch != NULL);
  gdb_assert (gdbarch->register_bytes_ok != NULL);
  if (gdbarch_debug >= 2)
    fprintf_unfiltered (gdb_stdlog, "gdbarch_register_bytes_ok called\n");
  return gdbarch->register_bytes_ok (nr_bytes);
}

void
set_gdbarch_register_bytes_ok (struct gdbarch *gdbarch,
                               gdbarch_register_bytes_ok_ftype register_bytes_ok)
{
  gdbarch->register_bytes_ok = register_bytes_ok;
}

int
gdbarch_cannot_fetch_register (struct gdbarch *gdbarch, int regnum)
{
  gdb_assert (gdbarch != NULL);
  gdb_assert (gdbarch->cannot_fetch_register != NULL);
  if (gdbarch_debug >= 2)
    fprintf_unfiltered (gdb_stdlog, "gdbarch_cannot_fetch_register called\n");
  return gdbarch->cannot_fetch_register (regnum);
}

void
set_gdbarch_cannot_fetch_register (struct gdbarch *gdbarch,
                                   gdbarch_cannot_fetch_register_ftype cannot_fetch_register)
{
  gdbarch->cannot_fetch_register = cannot_fetch_register;
}

int
gdbarch_cannot_store_register (struct gdbarch *gdbarch, int regnum)
{
  gdb_assert (gdbarch != NULL);
  gdb_assert (gdbarch->cannot_store_register != NULL);
  if (gdbarch_debug >= 2)
    fprintf_unfiltered (gdb_stdlog, "gdbarch_cannot_store_register called\n");
  return gdbarch->cannot_store_register (regnum);
}

void
set_gdbarch_cannot_store_register (struct gdbarch *gdbarch,
                                   gdbarch_cannot_store_register_ftype cannot_store_register)
{
  gdbarch->cannot_store_register = cannot_store_register;
}

int
gdbarch_get_longjmp_target_p (struct gdbarch *gdbarch)
{
  gdb_assert (gdbarch != NULL);
  return gdbarch->get_longjmp_target != NULL;
}

int
gdbarch_get_longjmp_target (struct gdbarch *gdbarch, CORE_ADDR *pc)
{
  gdb_assert (gdbarch != NULL);
  gdb_assert (gdbarch->get_longjmp_target != NULL);
  if (gdbarch_debug >= 2)
    fprintf_unfiltered (gdb_stdlog, "gdbarch_get_longjmp_target called\n");
  return gdbarch->get_longjmp_target (pc);
}

void
set_gdbarch_get_longjmp_target (struct gdbarch *gdbarch,
                                gdbarch_get_longjmp_target_ftype get_longjmp_target)
{
  gdbarch->get_longjmp_target = get_longjmp_target;
}

int
gdbarch_believe_pcc_promotion (struct gdbarch *gdbarch)
{
  gdb_assert (gdbarch != NULL);
  if (gdbarch_debug >= 2)
    fprintf_unfiltered (gdb_stdlog, "gdbarch_believe_pcc_promotion called\n");
  return gdbarch->believe_pcc_promotion;
}

void
set_gdbarch_believe_pcc_promotion (struct gdbarch *gdbarch,
                                   int believe_pcc_promotion)
{
  gdbarch->believe_pcc_promotion = believe_pcc_promotion;
}

int
gdbarch_convert_register_p (struct gdbarch *gdbarch, int regnum, struct type *type)
{
  gdb_assert (gdbarch != NULL);
  gdb_assert (gdbarch->convert_register_p != NULL);
  if (gdbarch_debug >= 2)
    fprintf_unfiltered (gdb_stdlog, "gdbarch_convert_register_p called\n");
  return gdbarch->convert_register_p (regnum, type);
}

void
set_gdbarch_convert_register_p (struct gdbarch *gdbarch,
                                gdbarch_convert_register_p_ftype convert_register_p)
{
  gdbarch->convert_register_p = convert_register_p;
}

void
gdbarch_register_to_value (struct gdbarch *gdbarch, struct frame_info *frame, int regnum, struct type *type, void *buf)
{
  gdb_assert (gdbarch != NULL);
  gdb_assert (gdbarch->register_to_value != NULL);
  if (gdbarch_debug >= 2)
    fprintf_unfiltered (gdb_stdlog, "gdbarch_register_to_value called\n");
  gdbarch->register_to_value (frame, regnum, type, buf);
}

void
set_gdbarch_register_to_value (struct gdbarch *gdbarch,
                               gdbarch_register_to_value_ftype register_to_value)
{
  gdbarch->register_to_value = register_to_value;
}

void
gdbarch_value_to_register (struct gdbarch *gdbarch, struct frame_info *frame, int regnum, struct type *type, const void *buf)
{
  gdb_assert (gdbarch != NULL);
  gdb_assert (gdbarch->value_to_register != NULL);
  if (gdbarch_debug >= 2)
    fprintf_unfiltered (gdb_stdlog, "gdbarch_value_to_register called\n");
  gdbarch->value_to_register (frame, regnum, type, buf);
}

void
set_gdbarch_value_to_register (struct gdbarch *gdbarch,
                               gdbarch_value_to_register_ftype value_to_register)
{
  gdbarch->value_to_register = value_to_register;
}

CORE_ADDR
gdbarch_pointer_to_address (struct gdbarch *gdbarch, struct type *type, const void *buf)
{
  gdb_assert (gdbarch != NULL);
  gdb_assert (gdbarch->pointer_to_address != NULL);
  if (gdbarch_debug >= 2)
    fprintf_unfiltered (gdb_stdlog, "gdbarch_pointer_to_address called\n");
  return gdbarch->pointer_to_address (type, buf);
}

void
set_gdbarch_pointer_to_address (struct gdbarch *gdbarch,
                                gdbarch_pointer_to_address_ftype pointer_to_address)
{
  gdbarch->pointer_to_address = pointer_to_address;
}

void
gdbarch_address_to_pointer (struct gdbarch *gdbarch, struct type *type, void *buf, CORE_ADDR addr)
{
  gdb_assert (gdbarch != NULL);
  gdb_assert (gdbarch->address_to_pointer != NULL);
  if (gdbarch_debug >= 2)
    fprintf_unfiltered (gdb_stdlog, "gdbarch_address_to_pointer called\n");
  gdbarch->address_to_pointer (type, buf, addr);
}

void
set_gdbarch_address_to_pointer (struct gdbarch *gdbarch,
                                gdbarch_address_to_pointer_ftype address_to_pointer)
{
  gdbarch->address_to_pointer = address_to_pointer;
}

int
gdbarch_integer_to_address_p (struct gdbarch *gdbarch)
{
  gdb_assert (gdbarch != NULL);
  return gdbarch->integer_to_address != NULL;
}

CORE_ADDR
gdbarch_integer_to_address (struct gdbarch *gdbarch, struct type *type, void *buf)
{
  gdb_assert (gdbarch != NULL);
  gdb_assert (gdbarch->integer_to_address != NULL);
  if (gdbarch_debug >= 2)
    fprintf_unfiltered (gdb_stdlog, "gdbarch_integer_to_address called\n");
  return gdbarch->integer_to_address (type, buf);
}

void
set_gdbarch_integer_to_address (struct gdbarch *gdbarch,
                                gdbarch_integer_to_address_ftype integer_to_address)
{
  gdbarch->integer_to_address = integer_to_address;
}

int
gdbarch_deprecated_store_struct_return_p (struct gdbarch *gdbarch)
{
  gdb_assert (gdbarch != NULL);
  return gdbarch->deprecated_store_struct_return != NULL;
}

void
gdbarch_deprecated_store_struct_return (struct gdbarch *gdbarch, CORE_ADDR addr, CORE_ADDR sp)
{
  gdb_assert (gdbarch != NULL);
  gdb_assert (gdbarch->deprecated_store_struct_return != NULL);
  if (gdbarch_debug >= 2)
    fprintf_unfiltered (gdb_stdlog, "gdbarch_deprecated_store_struct_return called\n");
  gdbarch->deprecated_store_struct_return (addr, sp);
}

void
set_gdbarch_deprecated_store_struct_return (struct gdbarch *gdbarch,
                                            gdbarch_deprecated_store_struct_return_ftype deprecated_store_struct_return)
{
  gdbarch->deprecated_store_struct_return = deprecated_store_struct_return;
}

int
gdbarch_return_value_p (struct gdbarch *gdbarch)
{
  gdb_assert (gdbarch != NULL);
  return gdbarch->return_value != legacy_return_value;
}

enum return_value_convention
gdbarch_return_value (struct gdbarch *gdbarch, struct type *valtype, struct regcache *regcache, void *readbuf, const void *writebuf)
{
  gdb_assert (gdbarch != NULL);
  gdb_assert (gdbarch->return_value != NULL);
  /* Do not check predicate: gdbarch->return_value != legacy_return_value, allow call.  */
  if (gdbarch_debug >= 2)
    fprintf_unfiltered (gdb_stdlog, "gdbarch_return_value called\n");
  return gdbarch->return_value (gdbarch, valtype, regcache, readbuf, writebuf);
}

void
set_gdbarch_return_value (struct gdbarch *gdbarch,
                          gdbarch_return_value_ftype return_value)
{
  gdbarch->return_value = return_value;
}

void
gdbarch_extract_return_value (struct gdbarch *gdbarch, struct type *type, struct regcache *regcache, void *valbuf)
{
  gdb_assert (gdbarch != NULL);
  gdb_assert (gdbarch->extract_return_value != NULL);
  if (gdbarch_debug >= 2)
    fprintf_unfiltered (gdb_stdlog, "gdbarch_extract_return_value called\n");
  gdbarch->extract_return_value (type, regcache, valbuf);
}

void
set_gdbarch_extract_return_value (struct gdbarch *gdbarch,
                                  gdbarch_extract_return_value_ftype extract_return_value)
{
  gdbarch->extract_return_value = extract_return_value;
}

void
gdbarch_store_return_value (struct gdbarch *gdbarch, struct type *type, struct regcache *regcache, const void *valbuf)
{
  gdb_assert (gdbarch != NULL);
  gdb_assert (gdbarch->store_return_value != NULL);
  if (gdbarch_debug >= 2)
    fprintf_unfiltered (gdb_stdlog, "gdbarch_store_return_value called\n");
  gdbarch->store_return_value (type, regcache, valbuf);
}

void
set_gdbarch_store_return_value (struct gdbarch *gdbarch,
                                gdbarch_store_return_value_ftype store_return_value)
{
  gdbarch->store_return_value = store_return_value;
}

void
gdbarch_deprecated_extract_return_value (struct gdbarch *gdbarch, struct type *type, char *regbuf, char *valbuf)
{
  gdb_assert (gdbarch != NULL);
  gdb_assert (gdbarch->deprecated_extract_return_value != NULL);
  if (gdbarch_debug >= 2)
    fprintf_unfiltered (gdb_stdlog, "gdbarch_deprecated_extract_return_value called\n");
  gdbarch->deprecated_extract_return_value (type, regbuf, valbuf);
}

void
set_gdbarch_deprecated_extract_return_value (struct gdbarch *gdbarch,
                                             gdbarch_deprecated_extract_return_value_ftype deprecated_extract_return_value)
{
  gdbarch->deprecated_extract_return_value = deprecated_extract_return_value;
}

void
gdbarch_deprecated_store_return_value (struct gdbarch *gdbarch, struct type *type, char *valbuf)
{
  gdb_assert (gdbarch != NULL);
  gdb_assert (gdbarch->deprecated_store_return_value != NULL);
  if (gdbarch_debug >= 2)
    fprintf_unfiltered (gdb_stdlog, "gdbarch_deprecated_store_return_value called\n");
  gdbarch->deprecated_store_return_value (type, valbuf);
}

void
set_gdbarch_deprecated_store_return_value (struct gdbarch *gdbarch,
                                           gdbarch_deprecated_store_return_value_ftype deprecated_store_return_value)
{
  gdbarch->deprecated_store_return_value = deprecated_store_return_value;
}

int
gdbarch_deprecated_use_struct_convention (struct gdbarch *gdbarch, int gcc_p, struct type *value_type)
{
  gdb_assert (gdbarch != NULL);
  gdb_assert (gdbarch->deprecated_use_struct_convention != NULL);
  if (gdbarch_debug >= 2)
    fprintf_unfiltered (gdb_stdlog, "gdbarch_deprecated_use_struct_convention called\n");
  return gdbarch->deprecated_use_struct_convention (gcc_p, value_type);
}

void
set_gdbarch_deprecated_use_struct_convention (struct gdbarch *gdbarch,
                                              gdbarch_deprecated_use_struct_convention_ftype deprecated_use_struct_convention)
{
  gdbarch->deprecated_use_struct_convention = deprecated_use_struct_convention;
}

int
gdbarch_deprecated_extract_struct_value_address_p (struct gdbarch *gdbarch)
{
  gdb_assert (gdbarch != NULL);
  return gdbarch->deprecated_extract_struct_value_address != NULL;
}

CORE_ADDR
gdbarch_deprecated_extract_struct_value_address (struct gdbarch *gdbarch, struct regcache *regcache)
{
  gdb_assert (gdbarch != NULL);
  gdb_assert (gdbarch->deprecated_extract_struct_value_address != NULL);
  if (gdbarch_debug >= 2)
    fprintf_unfiltered (gdb_stdlog, "gdbarch_deprecated_extract_struct_value_address called\n");
  return gdbarch->deprecated_extract_struct_value_address (regcache);
}

void
set_gdbarch_deprecated_extract_struct_value_address (struct gdbarch *gdbarch,
                                                     gdbarch_deprecated_extract_struct_value_address_ftype deprecated_extract_struct_value_address)
{
  gdbarch->deprecated_extract_struct_value_address = deprecated_extract_struct_value_address;
}

CORE_ADDR
gdbarch_skip_prologue (struct gdbarch *gdbarch, CORE_ADDR ip)
{
  gdb_assert (gdbarch != NULL);
  gdb_assert (gdbarch->skip_prologue != NULL);
  if (gdbarch_debug >= 2)
    fprintf_unfiltered (gdb_stdlog, "gdbarch_skip_prologue called\n");
  return gdbarch->skip_prologue (ip);
}

void
set_gdbarch_skip_prologue (struct gdbarch *gdbarch,
                           gdbarch_skip_prologue_ftype skip_prologue)
{
  gdbarch->skip_prologue = skip_prologue;
}

int
gdbarch_inner_than (struct gdbarch *gdbarch, CORE_ADDR lhs, CORE_ADDR rhs)
{
  gdb_assert (gdbarch != NULL);
  gdb_assert (gdbarch->inner_than != NULL);
  if (gdbarch_debug >= 2)
    fprintf_unfiltered (gdb_stdlog, "gdbarch_inner_than called\n");
  return gdbarch->inner_than (lhs, rhs);
}

void
set_gdbarch_inner_than (struct gdbarch *gdbarch,
                        gdbarch_inner_than_ftype inner_than)
{
  gdbarch->inner_than = inner_than;
}

const unsigned char *
gdbarch_breakpoint_from_pc (struct gdbarch *gdbarch, CORE_ADDR *pcptr, int *lenptr)
{
  gdb_assert (gdbarch != NULL);
  gdb_assert (gdbarch->breakpoint_from_pc != NULL);
  if (gdbarch_debug >= 2)
    fprintf_unfiltered (gdb_stdlog, "gdbarch_breakpoint_from_pc called\n");
  return gdbarch->breakpoint_from_pc (pcptr, lenptr);
}

void
set_gdbarch_breakpoint_from_pc (struct gdbarch *gdbarch,
                                gdbarch_breakpoint_from_pc_ftype breakpoint_from_pc)
{
  gdbarch->breakpoint_from_pc = breakpoint_from_pc;
}

int
gdbarch_adjust_breakpoint_address_p (struct gdbarch *gdbarch)
{
  gdb_assert (gdbarch != NULL);
  return gdbarch->adjust_breakpoint_address != NULL;
}

CORE_ADDR
gdbarch_adjust_breakpoint_address (struct gdbarch *gdbarch, CORE_ADDR bpaddr)
{
  gdb_assert (gdbarch != NULL);
  gdb_assert (gdbarch->adjust_breakpoint_address != NULL);
  if (gdbarch_debug >= 2)
    fprintf_unfiltered (gdb_stdlog, "gdbarch_adjust_breakpoint_address called\n");
  return gdbarch->adjust_breakpoint_address (gdbarch, bpaddr);
}

void
set_gdbarch_adjust_breakpoint_address (struct gdbarch *gdbarch,
                                       gdbarch_adjust_breakpoint_address_ftype adjust_breakpoint_address)
{
  gdbarch->adjust_breakpoint_address = adjust_breakpoint_address;
}

int
gdbarch_memory_insert_breakpoint (struct gdbarch *gdbarch, CORE_ADDR addr, char *contents_cache)
{
  gdb_assert (gdbarch != NULL);
  gdb_assert (gdbarch->memory_insert_breakpoint != NULL);
  if (gdbarch_debug >= 2)
    fprintf_unfiltered (gdb_stdlog, "gdbarch_memory_insert_breakpoint called\n");
  return gdbarch->memory_insert_breakpoint (addr, contents_cache);
}

void
set_gdbarch_memory_insert_breakpoint (struct gdbarch *gdbarch,
                                      gdbarch_memory_insert_breakpoint_ftype memory_insert_breakpoint)
{
  gdbarch->memory_insert_breakpoint = memory_insert_breakpoint;
}

int
gdbarch_memory_remove_breakpoint (struct gdbarch *gdbarch, CORE_ADDR addr, char *contents_cache)
{
  gdb_assert (gdbarch != NULL);
  gdb_assert (gdbarch->memory_remove_breakpoint != NULL);
  if (gdbarch_debug >= 2)
    fprintf_unfiltered (gdb_stdlog, "gdbarch_memory_remove_breakpoint called\n");
  return gdbarch->memory_remove_breakpoint (addr, contents_cache);
}

void
set_gdbarch_memory_remove_breakpoint (struct gdbarch *gdbarch,
                                      gdbarch_memory_remove_breakpoint_ftype memory_remove_breakpoint)
{
  gdbarch->memory_remove_breakpoint = memory_remove_breakpoint;
}

CORE_ADDR
gdbarch_decr_pc_after_break (struct gdbarch *gdbarch)
{
  gdb_assert (gdbarch != NULL);
  /* Skip verify of decr_pc_after_break, invalid_p == 0 */
  if (gdbarch_debug >= 2)
    fprintf_unfiltered (gdb_stdlog, "gdbarch_decr_pc_after_break called\n");
  return gdbarch->decr_pc_after_break;
}

void
set_gdbarch_decr_pc_after_break (struct gdbarch *gdbarch,
                                 CORE_ADDR decr_pc_after_break)
{
  gdbarch->decr_pc_after_break = decr_pc_after_break;
}

CORE_ADDR
gdbarch_deprecated_function_start_offset (struct gdbarch *gdbarch)
{
  gdb_assert (gdbarch != NULL);
  /* Skip verify of deprecated_function_start_offset, invalid_p == 0 */
  if (gdbarch_debug >= 2)
    fprintf_unfiltered (gdb_stdlog, "gdbarch_deprecated_function_start_offset called\n");
  return gdbarch->deprecated_function_start_offset;
}

void
set_gdbarch_deprecated_function_start_offset (struct gdbarch *gdbarch,
                                              CORE_ADDR deprecated_function_start_offset)
{
  gdbarch->deprecated_function_start_offset = deprecated_function_start_offset;
}

void
gdbarch_remote_translate_xfer_address (struct gdbarch *gdbarch, struct regcache *regcache, CORE_ADDR gdb_addr, int gdb_len, CORE_ADDR *rem_addr, int *rem_len)
{
  gdb_assert (gdbarch != NULL);
  gdb_assert (gdbarch->remote_translate_xfer_address != NULL);
  if (gdbarch_debug >= 2)
    fprintf_unfiltered (gdb_stdlog, "gdbarch_remote_translate_xfer_address called\n");
  gdbarch->remote_translate_xfer_address (gdbarch, regcache, gdb_addr, gdb_len, rem_addr, rem_len);
}

void
set_gdbarch_remote_translate_xfer_address (struct gdbarch *gdbarch,
                                           gdbarch_remote_translate_xfer_address_ftype remote_translate_xfer_address)
{
  gdbarch->remote_translate_xfer_address = remote_translate_xfer_address;
}

CORE_ADDR
gdbarch_frame_args_skip (struct gdbarch *gdbarch)
{
  gdb_assert (gdbarch != NULL);
  /* Skip verify of frame_args_skip, invalid_p == 0 */
  if (gdbarch_debug >= 2)
    fprintf_unfiltered (gdb_stdlog, "gdbarch_frame_args_skip called\n");
  return gdbarch->frame_args_skip;
}

void
set_gdbarch_frame_args_skip (struct gdbarch *gdbarch,
                             CORE_ADDR frame_args_skip)
{
  gdbarch->frame_args_skip = frame_args_skip;
}

int
gdbarch_unwind_pc_p (struct gdbarch *gdbarch)
{
  gdb_assert (gdbarch != NULL);
  return gdbarch->unwind_pc != NULL;
}

CORE_ADDR
gdbarch_unwind_pc (struct gdbarch *gdbarch, struct frame_info *next_frame)
{
  gdb_assert (gdbarch != NULL);
  gdb_assert (gdbarch->unwind_pc != NULL);
  if (gdbarch_debug >= 2)
    fprintf_unfiltered (gdb_stdlog, "gdbarch_unwind_pc called\n");
  return gdbarch->unwind_pc (gdbarch, next_frame);
}

void
set_gdbarch_unwind_pc (struct gdbarch *gdbarch,
                       gdbarch_unwind_pc_ftype unwind_pc)
{
  gdbarch->unwind_pc = unwind_pc;
}

int
gdbarch_unwind_sp_p (struct gdbarch *gdbarch)
{
  gdb_assert (gdbarch != NULL);
  return gdbarch->unwind_sp != NULL;
}

CORE_ADDR
gdbarch_unwind_sp (struct gdbarch *gdbarch, struct frame_info *next_frame)
{
  gdb_assert (gdbarch != NULL);
  gdb_assert (gdbarch->unwind_sp != NULL);
  if (gdbarch_debug >= 2)
    fprintf_unfiltered (gdb_stdlog, "gdbarch_unwind_sp called\n");
  return gdbarch->unwind_sp (gdbarch, next_frame);
}

void
set_gdbarch_unwind_sp (struct gdbarch *gdbarch,
                       gdbarch_unwind_sp_ftype unwind_sp)
{
  gdbarch->unwind_sp = unwind_sp;
}

int
gdbarch_deprecated_saved_pc_after_call_p (struct gdbarch *gdbarch)
{
  gdb_assert (gdbarch != NULL);
  return gdbarch->deprecated_saved_pc_after_call != NULL;
}

CORE_ADDR
gdbarch_deprecated_saved_pc_after_call (struct gdbarch *gdbarch, struct frame_info *frame)
{
  gdb_assert (gdbarch != NULL);
  gdb_assert (gdbarch->deprecated_saved_pc_after_call != NULL);
  if (gdbarch_debug >= 2)
    fprintf_unfiltered (gdb_stdlog, "gdbarch_deprecated_saved_pc_after_call called\n");
  return gdbarch->deprecated_saved_pc_after_call (frame);
}

void
set_gdbarch_deprecated_saved_pc_after_call (struct gdbarch *gdbarch,
                                            gdbarch_deprecated_saved_pc_after_call_ftype deprecated_saved_pc_after_call)
{
  gdbarch->deprecated_saved_pc_after_call = deprecated_saved_pc_after_call;
}

int
gdbarch_frame_num_args_p (struct gdbarch *gdbarch)
{
  gdb_assert (gdbarch != NULL);
  return gdbarch->frame_num_args != NULL;
}

int
gdbarch_frame_num_args (struct gdbarch *gdbarch, struct frame_info *frame)
{
  gdb_assert (gdbarch != NULL);
  gdb_assert (gdbarch->frame_num_args != NULL);
  if (gdbarch_debug >= 2)
    fprintf_unfiltered (gdb_stdlog, "gdbarch_frame_num_args called\n");
  return gdbarch->frame_num_args (frame);
}

void
set_gdbarch_frame_num_args (struct gdbarch *gdbarch,
                            gdbarch_frame_num_args_ftype frame_num_args)
{
  gdbarch->frame_num_args = frame_num_args;
}

int
gdbarch_deprecated_stack_align_p (struct gdbarch *gdbarch)
{
  gdb_assert (gdbarch != NULL);
  return gdbarch->deprecated_stack_align != NULL;
}

CORE_ADDR
gdbarch_deprecated_stack_align (struct gdbarch *gdbarch, CORE_ADDR sp)
{
  gdb_assert (gdbarch != NULL);
  gdb_assert (gdbarch->deprecated_stack_align != NULL);
  if (gdbarch_debug >= 2)
    fprintf_unfiltered (gdb_stdlog, "gdbarch_deprecated_stack_align called\n");
  return gdbarch->deprecated_stack_align (sp);
}

void
set_gdbarch_deprecated_stack_align (struct gdbarch *gdbarch,
                                    gdbarch_deprecated_stack_align_ftype deprecated_stack_align)
{
  gdbarch->deprecated_stack_align = deprecated_stack_align;
}

int
gdbarch_frame_align_p (struct gdbarch *gdbarch)
{
  gdb_assert (gdbarch != NULL);
  return gdbarch->frame_align != NULL;
}

CORE_ADDR
gdbarch_frame_align (struct gdbarch *gdbarch, CORE_ADDR address)
{
  gdb_assert (gdbarch != NULL);
  gdb_assert (gdbarch->frame_align != NULL);
  if (gdbarch_debug >= 2)
    fprintf_unfiltered (gdb_stdlog, "gdbarch_frame_align called\n");
  return gdbarch->frame_align (gdbarch, address);
}

void
set_gdbarch_frame_align (struct gdbarch *gdbarch,
                         gdbarch_frame_align_ftype frame_align)
{
  gdbarch->frame_align = frame_align;
}

int
gdbarch_deprecated_reg_struct_has_addr_p (struct gdbarch *gdbarch)
{
  gdb_assert (gdbarch != NULL);
  return gdbarch->deprecated_reg_struct_has_addr != NULL;
}

int
gdbarch_deprecated_reg_struct_has_addr (struct gdbarch *gdbarch, int gcc_p, struct type *type)
{
  gdb_assert (gdbarch != NULL);
  gdb_assert (gdbarch->deprecated_reg_struct_has_addr != NULL);
  if (gdbarch_debug >= 2)
    fprintf_unfiltered (gdb_stdlog, "gdbarch_deprecated_reg_struct_has_addr called\n");
  return gdbarch->deprecated_reg_struct_has_addr (gcc_p, type);
}

void
set_gdbarch_deprecated_reg_struct_has_addr (struct gdbarch *gdbarch,
                                            gdbarch_deprecated_reg_struct_has_addr_ftype deprecated_reg_struct_has_addr)
{
  gdbarch->deprecated_reg_struct_has_addr = deprecated_reg_struct_has_addr;
}

int
gdbarch_stabs_argument_has_addr (struct gdbarch *gdbarch, struct type *type)
{
  gdb_assert (gdbarch != NULL);
  gdb_assert (gdbarch->stabs_argument_has_addr != NULL);
  if (gdbarch_debug >= 2)
    fprintf_unfiltered (gdb_stdlog, "gdbarch_stabs_argument_has_addr called\n");
  return gdbarch->stabs_argument_has_addr (gdbarch, type);
}

void
set_gdbarch_stabs_argument_has_addr (struct gdbarch *gdbarch,
                                     gdbarch_stabs_argument_has_addr_ftype stabs_argument_has_addr)
{
  gdbarch->stabs_argument_has_addr = stabs_argument_has_addr;
}

int
gdbarch_frame_red_zone_size (struct gdbarch *gdbarch)
{
  gdb_assert (gdbarch != NULL);
  if (gdbarch_debug >= 2)
    fprintf_unfiltered (gdb_stdlog, "gdbarch_frame_red_zone_size called\n");
  return gdbarch->frame_red_zone_size;
}

void
set_gdbarch_frame_red_zone_size (struct gdbarch *gdbarch,
                                 int frame_red_zone_size)
{
  gdbarch->frame_red_zone_size = frame_red_zone_size;
}

CORE_ADDR
gdbarch_convert_from_func_ptr_addr (struct gdbarch *gdbarch, CORE_ADDR addr, struct target_ops *targ)
{
  gdb_assert (gdbarch != NULL);
  gdb_assert (gdbarch->convert_from_func_ptr_addr != NULL);
  if (gdbarch_debug >= 2)
    fprintf_unfiltered (gdb_stdlog, "gdbarch_convert_from_func_ptr_addr called\n");
  return gdbarch->convert_from_func_ptr_addr (gdbarch, addr, targ);
}

void
set_gdbarch_convert_from_func_ptr_addr (struct gdbarch *gdbarch,
                                        gdbarch_convert_from_func_ptr_addr_ftype convert_from_func_ptr_addr)
{
  gdbarch->convert_from_func_ptr_addr = convert_from_func_ptr_addr;
}

CORE_ADDR
gdbarch_addr_bits_remove (struct gdbarch *gdbarch, CORE_ADDR addr)
{
  gdb_assert (gdbarch != NULL);
  gdb_assert (gdbarch->addr_bits_remove != NULL);
  if (gdbarch_debug >= 2)
    fprintf_unfiltered (gdb_stdlog, "gdbarch_addr_bits_remove called\n");
  return gdbarch->addr_bits_remove (addr);
}

void
set_gdbarch_addr_bits_remove (struct gdbarch *gdbarch,
                              gdbarch_addr_bits_remove_ftype addr_bits_remove)
{
  gdbarch->addr_bits_remove = addr_bits_remove;
}

CORE_ADDR
gdbarch_smash_text_address (struct gdbarch *gdbarch, CORE_ADDR addr)
{
  gdb_assert (gdbarch != NULL);
  gdb_assert (gdbarch->smash_text_address != NULL);
  if (gdbarch_debug >= 2)
    fprintf_unfiltered (gdb_stdlog, "gdbarch_smash_text_address called\n");
  return gdbarch->smash_text_address (addr);
}

void
set_gdbarch_smash_text_address (struct gdbarch *gdbarch,
                                gdbarch_smash_text_address_ftype smash_text_address)
{
  gdbarch->smash_text_address = smash_text_address;
}

int
gdbarch_software_single_step_p (struct gdbarch *gdbarch)
{
  gdb_assert (gdbarch != NULL);
  return gdbarch->software_single_step != NULL;
}

void
gdbarch_software_single_step (struct gdbarch *gdbarch, enum target_signal sig, int insert_breakpoints_p)
{
  gdb_assert (gdbarch != NULL);
  gdb_assert (gdbarch->software_single_step != NULL);
  if (gdbarch_debug >= 2)
    fprintf_unfiltered (gdb_stdlog, "gdbarch_software_single_step called\n");
  gdbarch->software_single_step (sig, insert_breakpoints_p);
}

void
set_gdbarch_software_single_step (struct gdbarch *gdbarch,
                                  gdbarch_software_single_step_ftype software_single_step)
{
  gdbarch->software_single_step = software_single_step;
}

int
gdbarch_print_insn (struct gdbarch *gdbarch, bfd_vma vma, struct disassemble_info *info)
{
  gdb_assert (gdbarch != NULL);
  gdb_assert (gdbarch->print_insn != NULL);
  if (gdbarch_debug >= 2)
    fprintf_unfiltered (gdb_stdlog, "gdbarch_print_insn called\n");
  return gdbarch->print_insn (vma, info);
}

void
set_gdbarch_print_insn (struct gdbarch *gdbarch,
                        gdbarch_print_insn_ftype print_insn)
{
  gdbarch->print_insn = print_insn;
}

CORE_ADDR
gdbarch_skip_trampoline_code (struct gdbarch *gdbarch, CORE_ADDR pc)
{
  gdb_assert (gdbarch != NULL);
  gdb_assert (gdbarch->skip_trampoline_code != NULL);
  if (gdbarch_debug >= 2)
    fprintf_unfiltered (gdb_stdlog, "gdbarch_skip_trampoline_code called\n");
  return gdbarch->skip_trampoline_code (pc);
}

void
set_gdbarch_skip_trampoline_code (struct gdbarch *gdbarch,
                                  gdbarch_skip_trampoline_code_ftype skip_trampoline_code)
{
  gdbarch->skip_trampoline_code = skip_trampoline_code;
}

CORE_ADDR
gdbarch_skip_solib_resolver (struct gdbarch *gdbarch, CORE_ADDR pc)
{
  gdb_assert (gdbarch != NULL);
  gdb_assert (gdbarch->skip_solib_resolver != NULL);
  if (gdbarch_debug >= 2)
    fprintf_unfiltered (gdb_stdlog, "gdbarch_skip_solib_resolver called\n");
  return gdbarch->skip_solib_resolver (gdbarch, pc);
}

void
set_gdbarch_skip_solib_resolver (struct gdbarch *gdbarch,
                                 gdbarch_skip_solib_resolver_ftype skip_solib_resolver)
{
  gdbarch->skip_solib_resolver = skip_solib_resolver;
}

int
gdbarch_in_solib_call_trampoline (struct gdbarch *gdbarch, CORE_ADDR pc, char *name)
{
  gdb_assert (gdbarch != NULL);
  gdb_assert (gdbarch->in_solib_call_trampoline != NULL);
  if (gdbarch_debug >= 2)
    fprintf_unfiltered (gdb_stdlog, "gdbarch_in_solib_call_trampoline called\n");
  return gdbarch->in_solib_call_trampoline (pc, name);
}

void
set_gdbarch_in_solib_call_trampoline (struct gdbarch *gdbarch,
                                      gdbarch_in_solib_call_trampoline_ftype in_solib_call_trampoline)
{
  gdbarch->in_solib_call_trampoline = in_solib_call_trampoline;
}

int
gdbarch_in_solib_return_trampoline (struct gdbarch *gdbarch, CORE_ADDR pc, char *name)
{
  gdb_assert (gdbarch != NULL);
  gdb_assert (gdbarch->in_solib_return_trampoline != NULL);
  if (gdbarch_debug >= 2)
    fprintf_unfiltered (gdb_stdlog, "gdbarch_in_solib_return_trampoline called\n");
  return gdbarch->in_solib_return_trampoline (pc, name);
}

void
set_gdbarch_in_solib_return_trampoline (struct gdbarch *gdbarch,
                                        gdbarch_in_solib_return_trampoline_ftype in_solib_return_trampoline)
{
  gdbarch->in_solib_return_trampoline = in_solib_return_trampoline;
}

int
gdbarch_in_function_epilogue_p (struct gdbarch *gdbarch, CORE_ADDR addr)
{
  gdb_assert (gdbarch != NULL);
  gdb_assert (gdbarch->in_function_epilogue_p != NULL);
  if (gdbarch_debug >= 2)
    fprintf_unfiltered (gdb_stdlog, "gdbarch_in_function_epilogue_p called\n");
  return gdbarch->in_function_epilogue_p (gdbarch, addr);
}

void
set_gdbarch_in_function_epilogue_p (struct gdbarch *gdbarch,
                                    gdbarch_in_function_epilogue_p_ftype in_function_epilogue_p)
{
  gdbarch->in_function_epilogue_p = in_function_epilogue_p;
}

char *
gdbarch_construct_inferior_arguments (struct gdbarch *gdbarch, int argc, char **argv)
{
  gdb_assert (gdbarch != NULL);
  gdb_assert (gdbarch->construct_inferior_arguments != NULL);
  if (gdbarch_debug >= 2)
    fprintf_unfiltered (gdb_stdlog, "gdbarch_construct_inferior_arguments called\n");
  return gdbarch->construct_inferior_arguments (gdbarch, argc, argv);
}

void
set_gdbarch_construct_inferior_arguments (struct gdbarch *gdbarch,
                                          gdbarch_construct_inferior_arguments_ftype construct_inferior_arguments)
{
  gdbarch->construct_inferior_arguments = construct_inferior_arguments;
}

void
gdbarch_elf_make_msymbol_special (struct gdbarch *gdbarch, asymbol *sym, struct minimal_symbol *msym)
{
  gdb_assert (gdbarch != NULL);
  gdb_assert (gdbarch->elf_make_msymbol_special != NULL);
  if (gdbarch_debug >= 2)
    fprintf_unfiltered (gdb_stdlog, "gdbarch_elf_make_msymbol_special called\n");
  gdbarch->elf_make_msymbol_special (sym, msym);
}

void
set_gdbarch_elf_make_msymbol_special (struct gdbarch *gdbarch,
                                      gdbarch_elf_make_msymbol_special_ftype elf_make_msymbol_special)
{
  gdbarch->elf_make_msymbol_special = elf_make_msymbol_special;
}

void
gdbarch_coff_make_msymbol_special (struct gdbarch *gdbarch, int val, struct minimal_symbol *msym)
{
  gdb_assert (gdbarch != NULL);
  gdb_assert (gdbarch->coff_make_msymbol_special != NULL);
  if (gdbarch_debug >= 2)
    fprintf_unfiltered (gdb_stdlog, "gdbarch_coff_make_msymbol_special called\n");
  gdbarch->coff_make_msymbol_special (val, msym);
}

void
set_gdbarch_coff_make_msymbol_special (struct gdbarch *gdbarch,
                                       gdbarch_coff_make_msymbol_special_ftype coff_make_msymbol_special)
{
  gdbarch->coff_make_msymbol_special = coff_make_msymbol_special;
}

const char *
gdbarch_name_of_malloc (struct gdbarch *gdbarch)
{
  gdb_assert (gdbarch != NULL);
  /* Skip verify of name_of_malloc, invalid_p == 0 */
  if (gdbarch_debug >= 2)
    fprintf_unfiltered (gdb_stdlog, "gdbarch_name_of_malloc called\n");
  return gdbarch->name_of_malloc;
}

void
set_gdbarch_name_of_malloc (struct gdbarch *gdbarch,
                            const char * name_of_malloc)
{
  gdbarch->name_of_malloc = name_of_malloc;
}

int
gdbarch_cannot_step_breakpoint (struct gdbarch *gdbarch)
{
  gdb_assert (gdbarch != NULL);
  /* Skip verify of cannot_step_breakpoint, invalid_p == 0 */
  if (gdbarch_debug >= 2)
    fprintf_unfiltered (gdb_stdlog, "gdbarch_cannot_step_breakpoint called\n");
  return gdbarch->cannot_step_breakpoint;
}

void
set_gdbarch_cannot_step_breakpoint (struct gdbarch *gdbarch,
                                    int cannot_step_breakpoint)
{
  gdbarch->cannot_step_breakpoint = cannot_step_breakpoint;
}

int
gdbarch_have_nonsteppable_watchpoint (struct gdbarch *gdbarch)
{
  gdb_assert (gdbarch != NULL);
  /* Skip verify of have_nonsteppable_watchpoint, invalid_p == 0 */
  if (gdbarch_debug >= 2)
    fprintf_unfiltered (gdb_stdlog, "gdbarch_have_nonsteppable_watchpoint called\n");
  return gdbarch->have_nonsteppable_watchpoint;
}

void
set_gdbarch_have_nonsteppable_watchpoint (struct gdbarch *gdbarch,
                                          int have_nonsteppable_watchpoint)
{
  gdbarch->have_nonsteppable_watchpoint = have_nonsteppable_watchpoint;
}

int
gdbarch_address_class_type_flags_p (struct gdbarch *gdbarch)
{
  gdb_assert (gdbarch != NULL);
  return gdbarch->address_class_type_flags != NULL;
}

int
gdbarch_address_class_type_flags (struct gdbarch *gdbarch, int byte_size, int dwarf2_addr_class)
{
  gdb_assert (gdbarch != NULL);
  gdb_assert (gdbarch->address_class_type_flags != NULL);
  if (gdbarch_debug >= 2)
    fprintf_unfiltered (gdb_stdlog, "gdbarch_address_class_type_flags called\n");
  return gdbarch->address_class_type_flags (byte_size, dwarf2_addr_class);
}

void
set_gdbarch_address_class_type_flags (struct gdbarch *gdbarch,
                                      gdbarch_address_class_type_flags_ftype address_class_type_flags)
{
  gdbarch->address_class_type_flags = address_class_type_flags;
}

int
gdbarch_address_class_type_flags_to_name_p (struct gdbarch *gdbarch)
{
  gdb_assert (gdbarch != NULL);
  return gdbarch->address_class_type_flags_to_name != NULL;
}

const char *
gdbarch_address_class_type_flags_to_name (struct gdbarch *gdbarch, int type_flags)
{
  gdb_assert (gdbarch != NULL);
  gdb_assert (gdbarch->address_class_type_flags_to_name != NULL);
  if (gdbarch_debug >= 2)
    fprintf_unfiltered (gdb_stdlog, "gdbarch_address_class_type_flags_to_name called\n");
  return gdbarch->address_class_type_flags_to_name (gdbarch, type_flags);
}

void
set_gdbarch_address_class_type_flags_to_name (struct gdbarch *gdbarch,
                                              gdbarch_address_class_type_flags_to_name_ftype address_class_type_flags_to_name)
{
  gdbarch->address_class_type_flags_to_name = address_class_type_flags_to_name;
}

int
gdbarch_address_class_name_to_type_flags_p (struct gdbarch *gdbarch)
{
  gdb_assert (gdbarch != NULL);
  return gdbarch->address_class_name_to_type_flags != NULL;
}

int
gdbarch_address_class_name_to_type_flags (struct gdbarch *gdbarch, const char *name, int *type_flags_ptr)
{
  gdb_assert (gdbarch != NULL);
  gdb_assert (gdbarch->address_class_name_to_type_flags != NULL);
  if (gdbarch_debug >= 2)
    fprintf_unfiltered (gdb_stdlog, "gdbarch_address_class_name_to_type_flags called\n");
  return gdbarch->address_class_name_to_type_flags (gdbarch, name, type_flags_ptr);
}

void
set_gdbarch_address_class_name_to_type_flags (struct gdbarch *gdbarch,
                                              gdbarch_address_class_name_to_type_flags_ftype address_class_name_to_type_flags)
{
  gdbarch->address_class_name_to_type_flags = address_class_name_to_type_flags;
}

int
gdbarch_register_reggroup_p (struct gdbarch *gdbarch, int regnum, struct reggroup *reggroup)
{
  gdb_assert (gdbarch != NULL);
  gdb_assert (gdbarch->register_reggroup_p != NULL);
  if (gdbarch_debug >= 2)
    fprintf_unfiltered (gdb_stdlog, "gdbarch_register_reggroup_p called\n");
  return gdbarch->register_reggroup_p (gdbarch, regnum, reggroup);
}

void
set_gdbarch_register_reggroup_p (struct gdbarch *gdbarch,
                                 gdbarch_register_reggroup_p_ftype register_reggroup_p)
{
  gdbarch->register_reggroup_p = register_reggroup_p;
}

int
gdbarch_fetch_pointer_argument_p (struct gdbarch *gdbarch)
{
  gdb_assert (gdbarch != NULL);
  return gdbarch->fetch_pointer_argument != NULL;
}

CORE_ADDR
gdbarch_fetch_pointer_argument (struct gdbarch *gdbarch, struct frame_info *frame, int argi, struct type *type)
{
  gdb_assert (gdbarch != NULL);
  gdb_assert (gdbarch->fetch_pointer_argument != NULL);
  if (gdbarch_debug >= 2)
    fprintf_unfiltered (gdb_stdlog, "gdbarch_fetch_pointer_argument called\n");
  return gdbarch->fetch_pointer_argument (frame, argi, type);
}

void
set_gdbarch_fetch_pointer_argument (struct gdbarch *gdbarch,
                                    gdbarch_fetch_pointer_argument_ftype fetch_pointer_argument)
{
  gdbarch->fetch_pointer_argument = fetch_pointer_argument;
}

int
gdbarch_regset_from_core_section_p (struct gdbarch *gdbarch)
{
  gdb_assert (gdbarch != NULL);
  return gdbarch->regset_from_core_section != NULL;
}

const struct regset *
gdbarch_regset_from_core_section (struct gdbarch *gdbarch, const char *sect_name, size_t sect_size)
{
  gdb_assert (gdbarch != NULL);
  gdb_assert (gdbarch->regset_from_core_section != NULL);
  if (gdbarch_debug >= 2)
    fprintf_unfiltered (gdb_stdlog, "gdbarch_regset_from_core_section called\n");
  return gdbarch->regset_from_core_section (gdbarch, sect_name, sect_size);
}

void
set_gdbarch_regset_from_core_section (struct gdbarch *gdbarch,
                                      gdbarch_regset_from_core_section_ftype regset_from_core_section)
{
  gdbarch->regset_from_core_section = regset_from_core_section;
}


/* Keep a registry of per-architecture data-pointers required by GDB
   modules. */

struct gdbarch_data
{
  unsigned index;
  int init_p;
  gdbarch_data_pre_init_ftype *pre_init;
  gdbarch_data_post_init_ftype *post_init;
};

struct gdbarch_data_registration
{
  struct gdbarch_data *data;
  struct gdbarch_data_registration *next;
};

struct gdbarch_data_registry
{
  unsigned nr;
  struct gdbarch_data_registration *registrations;
};

struct gdbarch_data_registry gdbarch_data_registry =
{
  0, NULL,
};

static struct gdbarch_data *
gdbarch_data_register (gdbarch_data_pre_init_ftype *pre_init,
		       gdbarch_data_post_init_ftype *post_init)
{
  struct gdbarch_data_registration **curr;
  /* Append the new registraration.  */
  for (curr = &gdbarch_data_registry.registrations;
       (*curr) != NULL;
       curr = &(*curr)->next);
  (*curr) = XMALLOC (struct gdbarch_data_registration);
  (*curr)->next = NULL;
  (*curr)->data = XMALLOC (struct gdbarch_data);
  (*curr)->data->index = gdbarch_data_registry.nr++;
  (*curr)->data->pre_init = pre_init;
  (*curr)->data->post_init = post_init;
  (*curr)->data->init_p = 1;
  return (*curr)->data;
}

struct gdbarch_data *
gdbarch_data_register_pre_init (gdbarch_data_pre_init_ftype *pre_init)
{
  return gdbarch_data_register (pre_init, NULL);
}

struct gdbarch_data *
gdbarch_data_register_post_init (gdbarch_data_post_init_ftype *post_init)
{
  return gdbarch_data_register (NULL, post_init);
}

/* Create/delete the gdbarch data vector. */

static void
alloc_gdbarch_data (struct gdbarch *gdbarch)
{
  gdb_assert (gdbarch->data == NULL);
  gdbarch->nr_data = gdbarch_data_registry.nr;
  gdbarch->data = GDBARCH_OBSTACK_CALLOC (gdbarch, gdbarch->nr_data, void *);
}

/* Initialize the current value of the specified per-architecture
   data-pointer. */

void
deprecated_set_gdbarch_data (struct gdbarch *gdbarch,
			     struct gdbarch_data *data,
			     void *pointer)
{
  gdb_assert (data->index < gdbarch->nr_data);
  gdb_assert (gdbarch->data[data->index] == NULL);
  gdb_assert (data->pre_init == NULL);
  gdbarch->data[data->index] = pointer;
}

/* Return the current value of the specified per-architecture
   data-pointer. */

void *
gdbarch_data (struct gdbarch *gdbarch, struct gdbarch_data *data)
{
  gdb_assert (data->index < gdbarch->nr_data);
  if (gdbarch->data[data->index] == NULL)
    {
      /* The data-pointer isn't initialized, call init() to get a
	 value.  */
      if (data->pre_init != NULL)
	/* Mid architecture creation: pass just the obstack, and not
	   the entire architecture, as that way it isn't possible for
	   pre-init code to refer to undefined architecture
	   fields.  */
	gdbarch->data[data->index] = data->pre_init (gdbarch->obstack);
      else if (gdbarch->initialized_p
	       && data->post_init != NULL)
	/* Post architecture creation: pass the entire architecture
	   (as all fields are valid), but be careful to also detect
	   recursive references.  */
	{
	  gdb_assert (data->init_p);
	  data->init_p = 0;
	  gdbarch->data[data->index] = data->post_init (gdbarch);
	  data->init_p = 1;
	}
      else
	/* The architecture initialization hasn't completed - punt -
	 hope that the caller knows what they are doing.  Once
	 deprecated_set_gdbarch_data has been initialized, this can be
	 changed to an internal error.  */
	return NULL;
      gdb_assert (gdbarch->data[data->index] != NULL);
    }
  return gdbarch->data[data->index];
}



/* Keep a registry of swapped data required by GDB modules. */

struct gdbarch_swap
{
  void *swap;
  struct gdbarch_swap_registration *source;
  struct gdbarch_swap *next;
};

struct gdbarch_swap_registration
{
  void *data;
  unsigned long sizeof_data;
  gdbarch_swap_ftype *init;
  struct gdbarch_swap_registration *next;
};

struct gdbarch_swap_registry
{
  int nr;
  struct gdbarch_swap_registration *registrations;
};

struct gdbarch_swap_registry gdbarch_swap_registry = 
{
  0, NULL,
};

void
deprecated_register_gdbarch_swap (void *data,
		                  unsigned long sizeof_data,
		                  gdbarch_swap_ftype *init)
{
  struct gdbarch_swap_registration **rego;
  for (rego = &gdbarch_swap_registry.registrations;
       (*rego) != NULL;
       rego = &(*rego)->next);
  (*rego) = XMALLOC (struct gdbarch_swap_registration);
  (*rego)->next = NULL;
  (*rego)->init = init;
  (*rego)->data = data;
  (*rego)->sizeof_data = sizeof_data;
}

static void
current_gdbarch_swap_init_hack (void)
{
  struct gdbarch_swap_registration *rego;
  struct gdbarch_swap **curr = &current_gdbarch->swap;
  for (rego = gdbarch_swap_registry.registrations;
       rego != NULL;
       rego = rego->next)
    {
      if (rego->data != NULL)
	{
	  (*curr) = GDBARCH_OBSTACK_ZALLOC (current_gdbarch,
					    struct gdbarch_swap);
	  (*curr)->source = rego;
	  (*curr)->swap = gdbarch_obstack_zalloc (current_gdbarch,
						  rego->sizeof_data);
	  (*curr)->next = NULL;
	  curr = &(*curr)->next;
	}
      if (rego->init != NULL)
	rego->init ();
    }
}

static struct gdbarch *
current_gdbarch_swap_out_hack (void)
{
  struct gdbarch *old_gdbarch = current_gdbarch;
  struct gdbarch_swap *curr;

  gdb_assert (old_gdbarch != NULL);
  for (curr = old_gdbarch->swap;
       curr != NULL;
       curr = curr->next)
    {
      memcpy (curr->swap, curr->source->data, curr->source->sizeof_data);
      memset (curr->source->data, 0, curr->source->sizeof_data);
    }
  current_gdbarch = NULL;
  return old_gdbarch;
}

static void
current_gdbarch_swap_in_hack (struct gdbarch *new_gdbarch)
{
  struct gdbarch_swap *curr;

  gdb_assert (current_gdbarch == NULL);
  for (curr = new_gdbarch->swap;
       curr != NULL;
       curr = curr->next)
    memcpy (curr->source->data, curr->swap, curr->source->sizeof_data);
  current_gdbarch = new_gdbarch;
}


/* Keep a registry of the architectures known by GDB. */

struct gdbarch_registration
{
  enum bfd_architecture bfd_architecture;
  gdbarch_init_ftype *init;
  gdbarch_dump_tdep_ftype *dump_tdep;
  struct gdbarch_list *arches;
  struct gdbarch_registration *next;
};

static struct gdbarch_registration *gdbarch_registry = NULL;

static void
append_name (const char ***buf, int *nr, const char *name)
{
  *buf = xrealloc (*buf, sizeof (char**) * (*nr + 1));
  (*buf)[*nr] = name;
  *nr += 1;
}

const char **
gdbarch_printable_names (void)
{
  /* Accumulate a list of names based on the registed list of
     architectures. */
  enum bfd_architecture a;
  int nr_arches = 0;
  const char **arches = NULL;
  struct gdbarch_registration *rego;
  for (rego = gdbarch_registry;
       rego != NULL;
       rego = rego->next)
    {
      const struct bfd_arch_info *ap;
      ap = bfd_lookup_arch (rego->bfd_architecture, 0);
      if (ap == NULL)
        internal_error (__FILE__, __LINE__,
                        "gdbarch_architecture_names: multi-arch unknown");
      do
        {
          append_name (&arches, &nr_arches, ap->printable_name);
          ap = ap->next;
        }
      while (ap != NULL);
    }
  append_name (&arches, &nr_arches, NULL);
  return arches;
}


void
gdbarch_register (enum bfd_architecture bfd_architecture,
                  gdbarch_init_ftype *init,
		  gdbarch_dump_tdep_ftype *dump_tdep)
{
  struct gdbarch_registration **curr;
  const struct bfd_arch_info *bfd_arch_info;
  /* Check that BFD recognizes this architecture */
  bfd_arch_info = bfd_lookup_arch (bfd_architecture, 0);
  if (bfd_arch_info == NULL)
    {
      internal_error (__FILE__, __LINE__,
                      "gdbarch: Attempt to register unknown architecture (%d)",
                      bfd_architecture);
    }
  /* Check that we haven't seen this architecture before */
  for (curr = &gdbarch_registry;
       (*curr) != NULL;
       curr = &(*curr)->next)
    {
      if (bfd_architecture == (*curr)->bfd_architecture)
	internal_error (__FILE__, __LINE__,
                        "gdbarch: Duplicate registraration of architecture (%s)",
	                bfd_arch_info->printable_name);
    }
  /* log it */
  if (gdbarch_debug)
    fprintf_unfiltered (gdb_stdlog, "register_gdbarch_init (%s, 0x%08lx)\n",
			bfd_arch_info->printable_name,
			(long) init);
  /* Append it */
  (*curr) = XMALLOC (struct gdbarch_registration);
  (*curr)->bfd_architecture = bfd_architecture;
  (*curr)->init = init;
  (*curr)->dump_tdep = dump_tdep;
  (*curr)->arches = NULL;
  (*curr)->next = NULL;
}

void
register_gdbarch_init (enum bfd_architecture bfd_architecture,
		       gdbarch_init_ftype *init)
{
  gdbarch_register (bfd_architecture, init, NULL);
}


/* Look for an architecture using gdbarch_info.  Base search on only
   BFD_ARCH_INFO and BYTE_ORDER. */

struct gdbarch_list *
gdbarch_list_lookup_by_info (struct gdbarch_list *arches,
                             const struct gdbarch_info *info)
{
  for (; arches != NULL; arches = arches->next)
    {
      if (info->bfd_arch_info != arches->gdbarch->bfd_arch_info)
	continue;
      if (info->byte_order != arches->gdbarch->byte_order)
	continue;
      if (info->osabi != arches->gdbarch->osabi)
	continue;
      return arches;
    }
  return NULL;
}


/* Find an architecture that matches the specified INFO.  Create a new
   architecture if needed.  Return that new architecture.  Assumes
   that there is no current architecture.  */

static struct gdbarch *
find_arch_by_info (struct gdbarch *old_gdbarch, struct gdbarch_info info)
{
  struct gdbarch *new_gdbarch;
  struct gdbarch_registration *rego;

  /* The existing architecture has been swapped out - all this code
     works from a clean slate.  */
  gdb_assert (current_gdbarch == NULL);

  /* Fill in missing parts of the INFO struct using a number of
     sources: "set ..."; INFOabfd supplied; and the existing
     architecture.  */
  gdbarch_info_fill (old_gdbarch, &info);

  /* Must have found some sort of architecture. */
  gdb_assert (info.bfd_arch_info != NULL);

  if (gdbarch_debug)
    {
      fprintf_unfiltered (gdb_stdlog,
			  "find_arch_by_info: info.bfd_arch_info %s\n",
			  (info.bfd_arch_info != NULL
			   ? info.bfd_arch_info->printable_name
			   : "(null)"));
      fprintf_unfiltered (gdb_stdlog,
			  "find_arch_by_info: info.byte_order %d (%s)\n",
			  info.byte_order,
			  (info.byte_order == BFD_ENDIAN_BIG ? "big"
			   : info.byte_order == BFD_ENDIAN_LITTLE ? "little"
			   : "default"));
      fprintf_unfiltered (gdb_stdlog,
			  "find_arch_by_info: info.osabi %d (%s)\n",
			  info.osabi, gdbarch_osabi_name (info.osabi));
      fprintf_unfiltered (gdb_stdlog,
			  "find_arch_by_info: info.abfd 0x%lx\n",
			  (long) info.abfd);
      fprintf_unfiltered (gdb_stdlog,
			  "find_arch_by_info: info.tdep_info 0x%lx\n",
			  (long) info.tdep_info);
    }

  /* Find the tdep code that knows about this architecture.  */
  for (rego = gdbarch_registry;
       rego != NULL;
       rego = rego->next)
    if (rego->bfd_architecture == info.bfd_arch_info->arch)
      break;
  if (rego == NULL)
    {
      if (gdbarch_debug)
	fprintf_unfiltered (gdb_stdlog, "find_arch_by_info: "
			    "No matching architecture\n");
      return 0;
    }

  /* Ask the tdep code for an architecture that matches "info".  */
  new_gdbarch = rego->init (info, rego->arches);

  /* Did the tdep code like it?  No.  Reject the change and revert to
     the old architecture.  */
  if (new_gdbarch == NULL)
    {
      if (gdbarch_debug)
	fprintf_unfiltered (gdb_stdlog, "find_arch_by_info: "
			    "Target rejected architecture\n");
      return NULL;
    }

  /* Is this a pre-existing architecture (as determined by already
     being initialized)?  Move it to the front of the architecture
     list (keeping the list sorted Most Recently Used).  */
  if (new_gdbarch->initialized_p)
    {
      struct gdbarch_list **list;
      struct gdbarch_list *this;
      if (gdbarch_debug)
	fprintf_unfiltered (gdb_stdlog, "find_arch_by_info: "
			    "Previous architecture 0x%08lx (%s) selected\n",
			    (long) new_gdbarch,
			    new_gdbarch->bfd_arch_info->printable_name);
      /* Find the existing arch in the list.  */
      for (list = &rego->arches;
	   (*list) != NULL && (*list)->gdbarch != new_gdbarch;
	   list = &(*list)->next);
      /* It had better be in the list of architectures.  */
      gdb_assert ((*list) != NULL && (*list)->gdbarch == new_gdbarch);
      /* Unlink THIS.  */
      this = (*list);
      (*list) = this->next;
      /* Insert THIS at the front.  */
      this->next = rego->arches;
      rego->arches = this;
      /* Return it.  */
      return new_gdbarch;
    }

  /* It's a new architecture.  */
  if (gdbarch_debug)
    fprintf_unfiltered (gdb_stdlog, "find_arch_by_info: "
			"New architecture 0x%08lx (%s) selected\n",
			(long) new_gdbarch,
			new_gdbarch->bfd_arch_info->printable_name);
  
  /* Insert the new architecture into the front of the architecture
     list (keep the list sorted Most Recently Used).  */
  {
    struct gdbarch_list *this = XMALLOC (struct gdbarch_list);
    this->next = rego->arches;
    this->gdbarch = new_gdbarch;
    rego->arches = this;
  }    

  /* Check that the newly installed architecture is valid.  Plug in
     any post init values.  */
  new_gdbarch->dump_tdep = rego->dump_tdep;
  verify_gdbarch (new_gdbarch);
  new_gdbarch->initialized_p = 1;

  /* Initialize any per-architecture swap areas.  This phase requires
     a valid global CURRENT_GDBARCH.  Set it momentarially, and then
     swap the entire architecture out.  */
  current_gdbarch = new_gdbarch;
  current_gdbarch_swap_init_hack ();
  current_gdbarch_swap_out_hack ();

  if (gdbarch_debug)
    gdbarch_dump (new_gdbarch, gdb_stdlog);

  return new_gdbarch;
}

struct gdbarch *
gdbarch_find_by_info (struct gdbarch_info info)
{
  /* Save the previously selected architecture, setting the global to
     NULL.  This stops things like gdbarch->init() trying to use the
     previous architecture's configuration.  The previous architecture
     may not even be of the same architecture family.  The most recent
     architecture of the same family is found at the head of the
     rego->arches list.  */
  struct gdbarch *old_gdbarch = current_gdbarch_swap_out_hack ();

  /* Find the specified architecture.  */
  struct gdbarch *new_gdbarch = find_arch_by_info (old_gdbarch, info);

  /* Restore the existing architecture.  */
  gdb_assert (current_gdbarch == NULL);
  current_gdbarch_swap_in_hack (old_gdbarch);

  return new_gdbarch;
}

/* Make the specified architecture current, swapping the existing one
   out.  */

void
deprecated_current_gdbarch_select_hack (struct gdbarch *new_gdbarch)
{
  gdb_assert (new_gdbarch != NULL);
  gdb_assert (current_gdbarch != NULL);
  gdb_assert (new_gdbarch->initialized_p);
  current_gdbarch_swap_out_hack ();
  current_gdbarch_swap_in_hack (new_gdbarch);
  architecture_changed_event ();
}

extern void _initialize_gdbarch (void);

void
_initialize_gdbarch (void)
{
  struct cmd_list_element *c;

  deprecated_add_show_from_set
    (add_set_cmd ("arch",
	          class_maintenance,
		  var_zinteger,
		  (char *)&gdbarch_debug,
		  "Set architecture debugging.\n\
When non-zero, architecture debugging is enabled.", &setdebuglist),
     &showdebuglist);
  c = add_set_cmd ("archdebug",
		   class_maintenance,
		   var_zinteger,
		   (char *)&gdbarch_debug,
		   "Set architecture debugging.\n\
When non-zero, architecture debugging is enabled.", &setlist);

  deprecate_cmd (c, "set debug arch");
  deprecate_cmd (deprecated_add_show_from_set (c, &showlist), "show debug arch");
}
