/* Print VAX instructions for GDB, the GNU debugger.

   Copyright 1986, 1989, 1991, 1992, 1995, 1996, 1998, 1999, 2000,
   2002, 2003, 2004 Free Software Foundation, Inc.

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

#include "defs.h"
#include "symtab.h"
#include "opcode/vax.h"
#include "gdbcore.h"
#include "inferior.h"
#include "regcache.h"
#include "frame.h"
#include "value.h"
#include "arch-utils.h"
#include "gdb_string.h"
#include "osabi.h"
#include "dis-asm.h"

#include "vax-tdep.h"

static gdbarch_register_name_ftype vax_register_name;

static gdbarch_skip_prologue_ftype vax_skip_prologue;
static gdbarch_frame_num_args_ftype vax_frame_num_args;
static gdbarch_deprecated_frame_chain_ftype vax_frame_chain;

static gdbarch_deprecated_extract_return_value_ftype vax_extract_return_value;

static gdbarch_deprecated_push_dummy_frame_ftype vax_push_dummy_frame;

static const char *
vax_register_name (int regno)
{
  static char *register_names[] =
  {
    "r0",  "r1",  "r2",  "r3", "r4", "r5", "r6", "r7",
    "r8",  "r9", "r10", "r11", "ap", "fp", "sp", "pc",
    "ps",
  };

  if (regno < 0)
    return (NULL);
  if (regno >= (sizeof(register_names) / sizeof(*register_names)))
    return (NULL);
  return (register_names[regno]);
}

static int
vax_register_byte (int regno)
{
  return (regno * 4);
}

static int
vax_register_raw_size (int regno)
{
  return (4);
}

static int
vax_register_virtual_size (int regno)
{
  return (4);
}

static struct type *
vax_register_virtual_type (int regno)
{
  return (builtin_type_int);
}

static void
vax_frame_init_saved_regs (struct frame_info *frame)
{
  int regnum, regmask;
  CORE_ADDR next_addr;

  if (deprecated_get_frame_saved_regs (frame))
    return;

  frame_saved_regs_zalloc (frame);

  regmask = read_memory_integer (get_frame_base (frame) + 4, 4) >> 16;

  next_addr = get_frame_base (frame) + 16;

  /* regmask's low bit is for register 0, which is the first one
     what would be pushed.  */
  for (regnum = 0; regnum < VAX_AP_REGNUM; regnum++)
    {
      if (regmask & (1 << regnum))
        deprecated_get_frame_saved_regs (frame)[regnum] = next_addr += 4;
    }

  deprecated_get_frame_saved_regs (frame)[SP_REGNUM] = next_addr + 4;
  if (regmask & (1 << DEPRECATED_FP_REGNUM))
    deprecated_get_frame_saved_regs (frame)[SP_REGNUM] +=
      4 + (4 * read_memory_integer (next_addr + 4, 4));

  deprecated_get_frame_saved_regs (frame)[PC_REGNUM] = get_frame_base (frame) + 16;
  deprecated_get_frame_saved_regs (frame)[DEPRECATED_FP_REGNUM] = get_frame_base (frame) + 12;
  deprecated_get_frame_saved_regs (frame)[VAX_AP_REGNUM] = get_frame_base (frame) + 8;
  deprecated_get_frame_saved_regs (frame)[PS_REGNUM] = get_frame_base (frame) + 4;
}

/* Get saved user PC for sigtramp from sigcontext for BSD style sigtramp.  */

static CORE_ADDR
vax_sigtramp_saved_pc (struct frame_info *frame)
{
  CORE_ADDR sigcontext_addr;
  char *buf;
  int ptrbytes = TYPE_LENGTH (builtin_type_void_func_ptr);
  int sigcontext_offs = (2 * TARGET_INT_BIT) / TARGET_CHAR_BIT;

  buf = alloca (ptrbytes);
  /* Get sigcontext address, it is the third parameter on the stack.  */
  if (get_next_frame (frame))
    sigcontext_addr = read_memory_typed_address
      (DEPRECATED_FRAME_ARGS_ADDRESS (get_next_frame (frame))
       + FRAME_ARGS_SKIP + sigcontext_offs,
       builtin_type_void_data_ptr);
  else
    sigcontext_addr = read_memory_typed_address
      (read_register (SP_REGNUM) + sigcontext_offs, builtin_type_void_data_ptr);

  /* Offset to saved PC in sigcontext, from <sys/signal.h>.  Don't
     cause a memory_error when accessing sigcontext in case the stack
     layout has changed or the stack is corrupt.  */
  target_read_memory (sigcontext_addr + 12, buf, ptrbytes);
  return extract_typed_address (buf, builtin_type_void_func_ptr);
}

static CORE_ADDR
vax_frame_saved_pc (struct frame_info *frame)
{
  if ((get_frame_type (frame) == SIGTRAMP_FRAME))
    return (vax_sigtramp_saved_pc (frame)); /* XXXJRT */

  return (read_memory_integer (get_frame_base (frame) + 16, 4));
}

static CORE_ADDR
vax_frame_args_address (struct frame_info *frame)
{
  /* In most of GDB, getting the args address is too important to just
     say "I don't know".  This is sometimes wrong for functions that
     aren't on top of the stack, but c'est la vie.  */
  if (get_next_frame (frame))
    return (read_memory_integer (get_frame_base (get_next_frame (frame)) + 8, 4));
  /* Cannot find the AP register value directly from the FP value.
     Must find it saved in the frame called by this one, or in the AP
     register for the innermost frame.  However, there is no way to
     tell the difference between the innermost frame and a frame for
     which we just don't know the frame that it called (e.g. "info
     frame 0x7ffec789").  For the sake of argument, suppose that the
     stack is somewhat trashed (which is one reason that "info frame"
     exists).  So, return 0 (indicating we don't know the address of
     the arglist) if we don't know what frame this frame calls.  */
  return 0;
}

static int
vax_frame_num_args (struct frame_info *fi)
{
  return (0xff & read_memory_integer (DEPRECATED_FRAME_ARGS_ADDRESS (fi), 1));
}

static CORE_ADDR
vax_frame_chain (struct frame_info *frame)
{
  /* In the case of the VAX, the frame's nominal address is the FP value,
     and 12 bytes later comes the saved previous FP value as a 4-byte word.  */
  return (read_memory_integer (get_frame_base (frame) + 12, 4));
}

static void
vax_push_dummy_frame (void)
{
  CORE_ADDR sp = read_register (SP_REGNUM);
  int regnum;

  sp = push_word (sp, 0);	/* arglist */
  for (regnum = 11; regnum >= 0; regnum--)
    sp = push_word (sp, read_register (regnum));
  sp = push_word (sp, read_register (PC_REGNUM));
  sp = push_word (sp, read_register (DEPRECATED_FP_REGNUM));
  sp = push_word (sp, read_register (VAX_AP_REGNUM));
  sp = push_word (sp, (read_register (PS_REGNUM) & 0xffef) + 0x2fff0000);
  sp = push_word (sp, 0);
  write_register (SP_REGNUM, sp);
  write_register (DEPRECATED_FP_REGNUM, sp);
  write_register (VAX_AP_REGNUM, sp + (17 * 4));
}

static void
vax_pop_frame (void)
{
  CORE_ADDR fp = read_register (DEPRECATED_FP_REGNUM);
  int regnum;
  int regmask = read_memory_integer (fp + 4, 4);

  write_register (PS_REGNUM,
		  (regmask & 0xffff)
		  | (read_register (PS_REGNUM) & 0xffff0000));
  write_register (PC_REGNUM, read_memory_integer (fp + 16, 4));
  write_register (DEPRECATED_FP_REGNUM, read_memory_integer (fp + 12, 4));
  write_register (VAX_AP_REGNUM, read_memory_integer (fp + 8, 4));
  fp += 16;
  for (regnum = 0; regnum < 12; regnum++)
    if (regmask & (0x10000 << regnum))
      write_register (regnum, read_memory_integer (fp += 4, 4));
  fp = fp + 4 + ((regmask >> 30) & 3);
  if (regmask & 0x20000000)
    {
      regnum = read_memory_integer (fp, 4);
      fp += (regnum + 1) * 4;
    }
  write_register (SP_REGNUM, fp);
  flush_cached_frames ();
}

/* The VAX call dummy sequence:

	calls #69, @#32323232
	bpt

   It is 8 bytes long.  The address and argc are patched by
   vax_fix_call_dummy().  */
static LONGEST vax_call_dummy_words[] = { 0x329f69fb, 0x03323232 };
static int sizeof_vax_call_dummy_words = sizeof(vax_call_dummy_words);

static void
vax_fix_call_dummy (char *dummy, CORE_ADDR pc, CORE_ADDR fun, int nargs,
                    struct value **args, struct type *type, int gcc_p)
{
  dummy[1] = nargs;
  store_unsigned_integer (dummy + 3, 4, fun);
}

static void
vax_store_struct_return (CORE_ADDR addr, CORE_ADDR sp)
{
  write_register (1, addr);
}

static void
vax_extract_return_value (struct type *valtype, char *regbuf, char *valbuf)
{
  memcpy (valbuf, regbuf + DEPRECATED_REGISTER_BYTE (0), TYPE_LENGTH (valtype));
}

static void
vax_store_return_value (struct type *valtype, char *valbuf)
{
  deprecated_write_register_bytes (0, valbuf, TYPE_LENGTH (valtype));
}

static const unsigned char *
vax_breakpoint_from_pc (CORE_ADDR *pcptr, int *lenptr)
{
  static const unsigned char vax_breakpoint[] = { 3 };

  *lenptr = sizeof(vax_breakpoint);
  return (vax_breakpoint);
}

/* Advance PC across any function entry prologue instructions
   to reach some "real" code.  */

static CORE_ADDR
vax_skip_prologue (CORE_ADDR pc)
{
  int op = (unsigned char) read_memory_integer (pc, 1);
  if (op == 0x11)
    pc += 2;			/* skip brb */
  if (op == 0x31)
    pc += 3;			/* skip brw */
  if (op == 0xC2
      && ((unsigned char) read_memory_integer (pc + 2, 1)) == 0x5E)
    pc += 3;			/* skip subl2 */
  if (op == 0x9E
      && ((unsigned char) read_memory_integer (pc + 1, 1)) == 0xAE
      && ((unsigned char) read_memory_integer (pc + 3, 1)) == 0x5E)
    pc += 4;			/* skip movab */
  if (op == 0x9E
      && ((unsigned char) read_memory_integer (pc + 1, 1)) == 0xCE
      && ((unsigned char) read_memory_integer (pc + 4, 1)) == 0x5E)
    pc += 5;			/* skip movab */
  if (op == 0x9E
      && ((unsigned char) read_memory_integer (pc + 1, 1)) == 0xEE
      && ((unsigned char) read_memory_integer (pc + 6, 1)) == 0x5E)
    pc += 7;			/* skip movab */
  return pc;
}

static CORE_ADDR
vax_saved_pc_after_call (struct frame_info *frame)
{
  return (DEPRECATED_FRAME_SAVED_PC(frame));
}

/* Initialize the current architecture based on INFO.  If possible, re-use an
   architecture from ARCHES, which is a list of architectures already created
   during this debugging session.

   Called e.g. at program startup, when reading a core file, and when reading
   a binary file.  */

static struct gdbarch *
vax_gdbarch_init (struct gdbarch_info info, struct gdbarch_list *arches)
{
  struct gdbarch *gdbarch;

  /* If there is already a candidate, use it.  */
  arches = gdbarch_list_lookup_by_info (arches, &info);
  if (arches != NULL)
    return arches->gdbarch;

  gdbarch = gdbarch_alloc (&info, NULL);

  /* NOTE: cagney/2002-12-06: This can be deleted when this arch is
     ready to unwind the PC first (see frame.c:get_prev_frame()).  */
  set_gdbarch_deprecated_init_frame_pc (gdbarch, deprecated_init_frame_pc_default);

  /* Register info */
  set_gdbarch_num_regs (gdbarch, VAX_NUM_REGS);
  set_gdbarch_sp_regnum (gdbarch, VAX_SP_REGNUM);
  set_gdbarch_deprecated_fp_regnum (gdbarch, VAX_FP_REGNUM);
  set_gdbarch_pc_regnum (gdbarch, VAX_PC_REGNUM);
  set_gdbarch_ps_regnum (gdbarch, VAX_PS_REGNUM);

  set_gdbarch_register_name (gdbarch, vax_register_name);
  set_gdbarch_deprecated_register_size (gdbarch, VAX_REGISTER_SIZE);
  set_gdbarch_deprecated_register_bytes (gdbarch, VAX_REGISTER_BYTES);
  set_gdbarch_deprecated_register_byte (gdbarch, vax_register_byte);
  set_gdbarch_deprecated_register_raw_size (gdbarch, vax_register_raw_size);
  set_gdbarch_deprecated_max_register_raw_size (gdbarch, VAX_MAX_REGISTER_RAW_SIZE);
  set_gdbarch_deprecated_register_virtual_size (gdbarch, vax_register_virtual_size);
  set_gdbarch_deprecated_max_register_virtual_size (gdbarch,
                                         VAX_MAX_REGISTER_VIRTUAL_SIZE);
  set_gdbarch_deprecated_register_virtual_type (gdbarch, vax_register_virtual_type);

  /* Frame and stack info */
  set_gdbarch_skip_prologue (gdbarch, vax_skip_prologue);
  set_gdbarch_deprecated_saved_pc_after_call (gdbarch, vax_saved_pc_after_call);

  set_gdbarch_frame_num_args (gdbarch, vax_frame_num_args);

  set_gdbarch_deprecated_frame_chain (gdbarch, vax_frame_chain);
  set_gdbarch_deprecated_frame_saved_pc (gdbarch, vax_frame_saved_pc);

  set_gdbarch_deprecated_frame_args_address (gdbarch, vax_frame_args_address);

  set_gdbarch_deprecated_frame_init_saved_regs (gdbarch, vax_frame_init_saved_regs);

  set_gdbarch_frame_args_skip (gdbarch, 4);

  set_gdbarch_inner_than (gdbarch, core_addr_lessthan);

  /* Return value info */
  set_gdbarch_deprecated_store_struct_return (gdbarch, vax_store_struct_return);
  set_gdbarch_deprecated_extract_return_value (gdbarch, vax_extract_return_value);
  set_gdbarch_deprecated_store_return_value (gdbarch, vax_store_return_value);

  /* Call dummy info */
  set_gdbarch_deprecated_push_dummy_frame (gdbarch, vax_push_dummy_frame);
  set_gdbarch_deprecated_pop_frame (gdbarch, vax_pop_frame);
  set_gdbarch_call_dummy_location (gdbarch, ON_STACK);
  set_gdbarch_deprecated_call_dummy_words (gdbarch, vax_call_dummy_words);
  set_gdbarch_deprecated_sizeof_call_dummy_words (gdbarch, sizeof_vax_call_dummy_words);
  set_gdbarch_deprecated_fix_call_dummy (gdbarch, vax_fix_call_dummy);
  set_gdbarch_deprecated_call_dummy_breakpoint_offset (gdbarch, 7);
  set_gdbarch_deprecated_use_generic_dummy_frames (gdbarch, 0);
  set_gdbarch_deprecated_pc_in_call_dummy (gdbarch, deprecated_pc_in_call_dummy_on_stack);

  /* Breakpoint info */
  set_gdbarch_breakpoint_from_pc (gdbarch, vax_breakpoint_from_pc);

  /* Misc info */
  set_gdbarch_function_start_offset (gdbarch, 2);
  set_gdbarch_believe_pcc_promotion (gdbarch, 1);

  /* Should be using push_dummy_call.  */
  set_gdbarch_deprecated_dummy_write_sp (gdbarch, deprecated_write_sp);

  /* Hook in ABI-specific overrides, if they have been registered.  */
  gdbarch_init_osabi (info, gdbarch);

  set_gdbarch_print_insn (gdbarch, print_insn_vax);

  return (gdbarch);
}

extern initialize_file_ftype _initialize_vax_tdep; /* -Wmissing-prototypes */

void
_initialize_vax_tdep (void)
{
  gdbarch_register (bfd_arch_vax, vax_gdbarch_init, NULL);
}
