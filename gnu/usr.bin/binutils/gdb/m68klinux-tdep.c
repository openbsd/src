/* Motorola m68k target-dependent support for GNU/Linux.

   Copyright 1996, 1998, 2000, 2001, 2002, 2003 Free Software Foundation,
   Inc.

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
#include "gdbcore.h"
#include "doublest.h"
#include "floatformat.h"
#include "frame.h"
#include "target.h"
#include "gdb_string.h"
#include "gdbtypes.h"
#include "osabi.h"
#include "regcache.h"
#include "objfiles.h"
#include "symtab.h"
#include "m68k-tdep.h"

/* Offsets (in target ints) into jmp_buf.  */

#define M68K_LINUX_JB_ELEMENT_SIZE 4
#define M68K_LINUX_JB_PC 7

/* Check whether insn1 and insn2 are parts of a signal trampoline.  */

#define IS_SIGTRAMP(insn1, insn2)					\
  (/* addaw #20,sp; moveq #119,d0; trap #0 */				\
   (insn1 == 0xdefc0014 && insn2 == 0x70774e40)				\
   /* moveq #119,d0; trap #0 */						\
   || insn1 == 0x70774e40)

#define IS_RT_SIGTRAMP(insn1, insn2)					\
  (/* movel #173,d0; trap #0 */						\
   (insn1 == 0x203c0000 && insn2 == 0x00ad4e40)				\
   /* moveq #82,d0; notb d0; trap #0 */					\
   || (insn1 == 0x70524600 && (insn2 >> 16) == 0x4e40))

/* Return non-zero if PC points into the signal trampoline.  For the
   sake of m68k_linux_get_sigtramp_info we also distinguish between
   non-RT and RT signal trampolines.  */

static int
m68k_linux_pc_in_sigtramp (CORE_ADDR pc, char *name)
{
  CORE_ADDR sp;
  char buf[12];
  unsigned long insn0, insn1, insn2;

  if (read_memory_nobpt (pc - 4, buf, sizeof (buf)))
    return 0;
  insn1 = extract_unsigned_integer (buf + 4, 4);
  insn2 = extract_unsigned_integer (buf + 8, 4);
  if (IS_SIGTRAMP (insn1, insn2))
    return 1;
  if (IS_RT_SIGTRAMP (insn1, insn2))
    return 2;

  insn0 = extract_unsigned_integer (buf, 4);
  if (IS_SIGTRAMP (insn0, insn1))
    return 1;
  if (IS_RT_SIGTRAMP (insn0, insn1))
    return 2;

  insn0 = ((insn0 << 16) & 0xffffffff) | (insn1 >> 16);
  insn1 = ((insn1 << 16) & 0xffffffff) | (insn2 >> 16);
  if (IS_SIGTRAMP (insn0, insn1))
    return 1;
  if (IS_RT_SIGTRAMP (insn0, insn1))
    return 2;

  return 0;
}

/* From <asm/sigcontext.h>.  */
static int m68k_linux_sigcontext_reg_offset[M68K_NUM_REGS] =
{
  2 * 4,			/* %d0 */
  3 * 4,			/* %d1 */
  -1,				/* %d2 */
  -1,				/* %d3 */
  -1,				/* %d4 */
  -1,				/* %d5 */
  -1,				/* %d6 */
  -1,				/* %d7 */
  4 * 4,			/* %a0 */
  5 * 4,			/* %a1 */
  -1,				/* %a2 */
  -1,				/* %a3 */
  -1,				/* %a4 */
  -1,				/* %a5 */
  -1,				/* %fp */
  1 * 4,			/* %sp */
  5 * 4 + 2,			/* %sr */
  6 * 4 + 2,			/* %pc */
  8 * 4,			/* %fp0 */
  11 * 4,			/* %fp1 */
  -1,				/* %fp2 */
  -1,				/* %fp3 */
  -1,				/* %fp4 */
  -1,				/* %fp5 */
  -1,				/* %fp6 */
  -1,				/* %fp7 */
  14 * 4,			/* %fpcr */
  15 * 4,			/* %fpsr */
  16 * 4			/* %fpiaddr */
};

/* From <asm/ucontext.h>.  */
static int m68k_linux_ucontext_reg_offset[M68K_NUM_REGS] =
{
  6 * 4,			/* %d0 */
  7 * 4,			/* %d1 */
  8 * 4,			/* %d2 */
  9 * 4,			/* %d3 */
  10 * 4,			/* %d4 */
  11 * 4,			/* %d5 */
  12 * 4,			/* %d6 */
  13 * 4,			/* %d7 */
  14 * 4,			/* %a0 */
  15 * 4,			/* %a1 */
  16 * 4,			/* %a2 */
  17 * 4,			/* %a3 */
  18 * 4,			/* %a4 */
  19 * 4,			/* %a5 */
  20 * 4,			/* %fp */
  21 * 4,			/* %sp */
  23 * 4,			/* %sr */
  22 * 4,			/* %pc */
  27 * 4,			/* %fp0 */
  30 * 4,			/* %fp1 */
  33 * 4,			/* %fp2 */
  36 * 4,			/* %fp3 */
  39 * 4,			/* %fp4 */
  42 * 4,			/* %fp5 */
  45 * 4,			/* %fp6 */
  48 * 4,			/* %fp7 */
  24 * 4,			/* %fpcr */
  25 * 4,			/* %fpsr */
  26 * 4			/* %fpiaddr */
};


/* Get info about saved registers in sigtramp.  */

static struct m68k_sigtramp_info
m68k_linux_get_sigtramp_info (struct frame_info *next_frame)
{
  CORE_ADDR sp;
  char buf[4];
  struct m68k_sigtramp_info info;

  frame_unwind_register (next_frame, M68K_SP_REGNUM, buf);
  sp = extract_unsigned_integer (buf, 4);

  /* Get sigcontext address, it is the third parameter on the stack.  */
  info.sigcontext_addr = read_memory_unsigned_integer (sp + 8, 4);

  if (m68k_linux_pc_in_sigtramp (frame_pc_unwind (next_frame), 0) == 2)
    info.sc_reg_offset = m68k_linux_ucontext_reg_offset;
  else
    info.sc_reg_offset = m68k_linux_sigcontext_reg_offset;
  return info;
}

/* Extract from an array REGBUF containing the (raw) register state, a
   function return value of TYPE, and copy that, in virtual format,
   into VALBUF.  */

static void
m68k_linux_extract_return_value (struct type *type, struct regcache *regcache,
				 void *valbuf)
{
  int len = TYPE_LENGTH (type);
  char buf[M68K_MAX_REGISTER_SIZE];

  if (TYPE_CODE (type) == TYPE_CODE_STRUCT
      && TYPE_NFIELDS (type) == 1)
    {
      m68k_linux_extract_return_value (TYPE_FIELD_TYPE (type, 0), regcache,
				       valbuf);
      return;
    }

  if (TYPE_CODE (type) == TYPE_CODE_FLT)
    {
      regcache_raw_read (regcache, M68K_FP0_REGNUM, buf);
      convert_typed_floating (buf, builtin_type_m68881_ext, valbuf, type);
    }
  else if (TYPE_CODE (type) == TYPE_CODE_PTR)
    regcache_raw_read (regcache, M68K_A0_REGNUM, valbuf);
  else
    {
      if (len <= 4)
	{
	  regcache_raw_read (regcache, M68K_D0_REGNUM, buf);
	  memcpy (valbuf, buf + (4 - len), len);
	}
      else if (len <= 8)
	{
	  regcache_raw_read (regcache, M68K_D0_REGNUM, buf);
	  memcpy (valbuf, buf + (8 - len), len - 4);
	  regcache_raw_read (regcache, M68K_D1_REGNUM,
			     (char *) valbuf + (len - 4));
	}
      else
	internal_error (__FILE__, __LINE__,
			"Cannot extract return value of %d bytes long.", len);
    }
}

/* Write into the appropriate registers a function return value stored
   in VALBUF of type TYPE, given in virtual format.  */

static void
m68k_linux_store_return_value (struct type *type, struct regcache *regcache,
			       const void *valbuf)
{
  int len = TYPE_LENGTH (type);

  if (TYPE_CODE (type) == TYPE_CODE_STRUCT
      && TYPE_NFIELDS (type) == 1)
    {
      m68k_linux_store_return_value (TYPE_FIELD_TYPE (type, 0), regcache,
				     valbuf);
      return;
    }

  if (TYPE_CODE (type) == TYPE_CODE_FLT)
    {
      char buf[M68K_MAX_REGISTER_SIZE];
      convert_typed_floating (valbuf, type, buf, builtin_type_m68881_ext);
      regcache_raw_write (regcache, M68K_FP0_REGNUM, buf);
    }
  else if (TYPE_CODE (type) == TYPE_CODE_PTR)
    regcache_raw_write (regcache, M68K_A0_REGNUM, valbuf);
  else
    {
      if (len <= 4)
	regcache_raw_write_part (regcache, M68K_D0_REGNUM,
				 4 - len, len, valbuf);
      else if (len <= 8)
	{
	  regcache_raw_write_part (regcache, M68K_D1_REGNUM, 8 - len,
				   len - 4, valbuf);
	  regcache_raw_write (regcache, M68K_D0_REGNUM,
			      (char *) valbuf + (len - 4));
	}
      else
	internal_error (__FILE__, __LINE__,
			"Cannot store return value of %d bytes long.", len);
    }
}

/* Extract from an array REGBUF containing the (raw) register state
   the address in which a function should return its structure value,
   as a CORE_ADDR.  */

static CORE_ADDR
m68k_linux_extract_struct_value_address (struct regcache *regcache)
{
  char buf[4];

  regcache_cooked_read (regcache, M68K_A0_REGNUM, buf);
  return extract_unsigned_integer (buf, 4);
}

static void
m68k_linux_init_abi (struct gdbarch_info info, struct gdbarch *gdbarch)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);

  tdep->jb_pc = M68K_LINUX_JB_PC;
  tdep->jb_elt_size = M68K_LINUX_JB_ELEMENT_SIZE;
  tdep->get_sigtramp_info = m68k_linux_get_sigtramp_info;
  tdep->struct_return = reg_struct_return;

  set_gdbarch_extract_return_value (gdbarch, m68k_linux_extract_return_value);
  set_gdbarch_store_return_value (gdbarch, m68k_linux_store_return_value);
  set_gdbarch_deprecated_extract_struct_value_address (gdbarch, m68k_linux_extract_struct_value_address);

  set_gdbarch_pc_in_sigtramp (gdbarch, m68k_linux_pc_in_sigtramp);

  /* Shared library handling.  */
  set_gdbarch_in_solib_call_trampoline (gdbarch, in_plt_section);
  set_gdbarch_skip_trampoline_code (gdbarch, find_solib_trampoline_target);
}

void
_initialize_m68k_linux_tdep (void)
{
  gdbarch_register_osabi (bfd_arch_m68k, 0, GDB_OSABI_LINUX,
			  m68k_linux_init_abi);
}
