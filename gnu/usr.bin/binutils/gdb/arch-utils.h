/* Dynamic architecture support for GDB, the GNU debugger.

   Copyright 1998, 1999, 2000, 2002, 2003, 2004 Free Software
   Foundation, Inc.

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

#ifndef GDBARCH_UTILS_H
#define GDBARCH_UTILS_H

struct gdbarch;
struct frame_info;
struct minimal_symbol;
struct type;
struct gdbarch_info;

/* gdbarch trace variable */
extern int gdbarch_debug;

/* An implementation of return_value that props up architectures still
   using USE_STRUCT_RETURN, EXTRACT_RETURN_VALUE and
   STORE_RETURN_VALUE.  See also the hacks in "stack.c".  */
enum return_value_convention legacy_return_value (struct gdbarch *gdbarch,
						  struct type *valtype,
						  struct regcache *regcache,
						  void *readbuf,
						  const void *writebuf);

/* Implementation of extract return value that grubs around in the
   register cache.  */
extern gdbarch_extract_return_value_ftype legacy_extract_return_value;

/* Implementation of store return value that grubs the register cache.  */
extern gdbarch_store_return_value_ftype legacy_store_return_value;

/* To return any structure or union type by value, store it at the
   address passed as an invisible first argument to the function.  */
extern gdbarch_deprecated_use_struct_convention_ftype always_use_struct_convention;

/* Typical remote_translate_xfer_address */
extern gdbarch_remote_translate_xfer_address_ftype generic_remote_translate_xfer_address;

/* The only possible cases for inner_than. */
extern int core_addr_lessthan (CORE_ADDR lhs, CORE_ADDR rhs);
extern int core_addr_greaterthan (CORE_ADDR lhs, CORE_ADDR rhs);

/* Floating point values. */
extern const struct floatformat *default_float_format (struct gdbarch *gdbarch);
extern const struct floatformat *default_double_format (struct gdbarch *gdbarch);

/* Identity functions on a CORE_ADDR.  Just return the "addr".  */

extern CORE_ADDR core_addr_identity (CORE_ADDR addr);
extern gdbarch_convert_from_func_ptr_addr_ftype convert_from_func_ptr_addr_identity;

/* No-op conversion of reg to regnum. */

extern int no_op_reg_to_regnum (int reg);

/* Do nothing version of elf_make_msymbol_special. */

void default_elf_make_msymbol_special (asymbol *sym, struct minimal_symbol *msym);

/* Do nothing version of coff_make_msymbol_special. */

void default_coff_make_msymbol_special (int val, struct minimal_symbol *msym);

/* Version of cannot_fetch_register() / cannot_store_register() that
   always fails. */

int cannot_register_not (int regnum);

/* Legacy version of target_virtual_frame_pointer().  Assumes that
   there is an DEPRECATED_FP_REGNUM and that it is the same, cooked or
   raw.  */

extern gdbarch_virtual_frame_pointer_ftype legacy_virtual_frame_pointer;

extern CORE_ADDR generic_skip_trampoline_code (CORE_ADDR pc);

extern CORE_ADDR generic_skip_solib_resolver (struct gdbarch *gdbarch,
					      CORE_ADDR pc);

extern int generic_in_solib_call_trampoline (CORE_ADDR pc, char *name);

extern int generic_in_solib_return_trampoline (CORE_ADDR pc, char *name);

extern int generic_in_function_epilogue_p (struct gdbarch *gdbarch, CORE_ADDR pc);

/* Assume that the world is sane, a registers raw and virtual size
   both match its type.  */

extern int generic_register_size (int regnum);

/* Assume that the world is sane, the registers are all adjacent.  */
extern int generic_register_byte (int regnum);

/* Prop up old targets that use various sigtramp macros.  */
extern int legacy_pc_in_sigtramp (CORE_ADDR pc, char *name);

/* By default, registers are not convertible.  */
extern int generic_convert_register_p (int regnum, struct type *type);

extern int default_stabs_argument_has_addr (struct gdbarch *gdbarch,
					    struct type *type);

/* For compatibility with older architectures, returns
   (LEGACY_SIM_REGNO_IGNORE) when the register doesn't have a valid
   name.  */

extern int legacy_register_sim_regno (int regnum);

/* Return the selected byte order, or BFD_ENDIAN_UNKNOWN if no byte
   order was explicitly selected.  */
extern enum bfd_endian selected_byte_order (void);

/* Return the selected architecture's name, or NULL if no architecture
   was explicitly selected.  */
extern const char *selected_architecture_name (void);

/* Initialize a ``struct info''.  Can't use memset(0) since some
   default values are not zero.  "fill" takes all available
   information and fills in any unspecified fields.  */

extern void gdbarch_info_init (struct gdbarch_info *info);
extern void gdbarch_info_fill (struct gdbarch *gdbarch,
			       struct gdbarch_info *info);

/* Similar to init, but this time fill in the blanks.  Information is
   obtained from the specified architecture, global "set ..." options,
   and explicitly initialized INFO fields.  */
extern void gdbarch_info_fill (struct gdbarch *gdbarch,
			       struct gdbarch_info *info);

/* Return the architecture for ABFD.  If no suitable architecture
   could be find, return NULL.  */

extern struct gdbarch *gdbarch_from_bfd (bfd *abfd);

#endif
