/* Target-dependent code for the Motorola 68000 series.

   Copyright 1990, 1991, 1992, 1993, 1994, 1995, 1996, 1999, 2000,
   2001, 2002, 2003, 2004 Free Software Foundation, Inc.

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
#include "dwarf2-frame.h"
#include "frame.h"
#include "frame-base.h"
#include "frame-unwind.h"
#include "floatformat.h"
#include "symtab.h"
#include "gdbcore.h"
#include "value.h"
#include "gdb_string.h"
#include "gdb_assert.h"
#include "inferior.h"
#include "regcache.h"
#include "arch-utils.h"
#include "osabi.h"
#include "dis-asm.h"

#include "m68k-tdep.h"


#define P_LINKL_FP	0x480e
#define P_LINKW_FP	0x4e56
#define P_PEA_FP	0x4856
#define P_MOVEAL_SP_FP	0x2c4f
#define P_ADDAW_SP	0xdefc
#define P_ADDAL_SP	0xdffc
#define P_SUBQW_SP	0x514f
#define P_SUBQL_SP	0x518f
#define P_LEA_SP_SP	0x4fef
#define P_LEA_PC_A5	0x4bfb0170
#define P_FMOVEMX_SP	0xf227
#define P_MOVEL_SP	0x2f00
#define P_MOVEML_SP	0x48e7


#define REGISTER_BYTES_FP (16*4 + 8 + 8*12 + 3*4)
#define REGISTER_BYTES_NOFP (16*4 + 8)

/* Offset from SP to first arg on stack at first instruction of a function */
#define SP_ARG0 (1 * 4)

#if !defined (BPT_VECTOR)
#define BPT_VECTOR 0xf
#endif

static const unsigned char *
m68k_local_breakpoint_from_pc (CORE_ADDR *pcptr, int *lenptr)
{
  static unsigned char break_insn[] = {0x4e, (0x40 | BPT_VECTOR)};
  *lenptr = sizeof (break_insn);
  return break_insn;
}


static int
m68k_register_bytes_ok (long numbytes)
{
  return ((numbytes == REGISTER_BYTES_FP)
	  || (numbytes == REGISTER_BYTES_NOFP));
}

/* Return the GDB type object for the "standard" data type of data in
   register N.  This should be int for D0-D7, SR, FPCONTROL and
   FPSTATUS, long double for FP0-FP7, and void pointer for all others
   (A0-A7, PC, FPIADDR).  Note, for registers which contain
   addresses return pointer to void, not pointer to char, because we
   don't want to attempt to print the string after printing the
   address.  */

static struct type *
m68k_register_type (struct gdbarch *gdbarch, int regnum)
{
  if (regnum >= FP0_REGNUM && regnum <= FP0_REGNUM + 7)
    return builtin_type_m68881_ext;

  if (regnum == M68K_FPI_REGNUM || regnum == PC_REGNUM)
    return builtin_type_void_func_ptr;

  if (regnum == M68K_FPC_REGNUM || regnum == M68K_FPS_REGNUM
      || regnum == PS_REGNUM)
    return builtin_type_int32;

  if (regnum >= M68K_A0_REGNUM && regnum <= M68K_A0_REGNUM + 7)
    return builtin_type_void_data_ptr;

  return builtin_type_int32;
}

/* Function: m68k_register_name
   Returns the name of the standard m68k register regnum. */

static const char *
m68k_register_name (int regnum)
{
  static char *register_names[] = {
    "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d7",
    "a0", "a1", "a2", "a3", "a4", "a5", "fp", "sp",
    "ps", "pc",
    "fp0", "fp1", "fp2", "fp3", "fp4", "fp5", "fp6", "fp7",
    "fpcontrol", "fpstatus", "fpiaddr", "fpcode", "fpflags"
  };

  if (regnum < 0 ||
      regnum >= sizeof (register_names) / sizeof (register_names[0]))
    internal_error (__FILE__, __LINE__,
		    "m68k_register_name: illegal register number %d", regnum);
  else
    return register_names[regnum];
}

/* Return nonzero if a value of type TYPE stored in register REGNUM
   needs any special handling.  */

static int
m68k_convert_register_p (int regnum, struct type *type)
{
  return (regnum >= M68K_FP0_REGNUM && regnum <= M68K_FP0_REGNUM + 7);
}

/* Read a value of type TYPE from register REGNUM in frame FRAME, and
   return its contents in TO.  */

static void
m68k_register_to_value (struct frame_info *frame, int regnum,
			struct type *type, void *to)
{
  char from[M68K_MAX_REGISTER_SIZE];

  /* We only support floating-point values.  */
  if (TYPE_CODE (type) != TYPE_CODE_FLT)
    {
      warning ("Cannot convert floating-point register value "
	       "to non-floating-point type.");
      return;
    }

  /* Convert to TYPE.  This should be a no-op if TYPE is equivalent to
     the extended floating-point format used by the FPU.  */
  get_frame_register (frame, regnum, from);
  convert_typed_floating (from, builtin_type_m68881_ext, to, type);
}

/* Write the contents FROM of a value of type TYPE into register
   REGNUM in frame FRAME.  */

static void
m68k_value_to_register (struct frame_info *frame, int regnum,
			struct type *type, const void *from)
{
  char to[M68K_MAX_REGISTER_SIZE];

  /* We only support floating-point values.  */
  if (TYPE_CODE (type) != TYPE_CODE_FLT)
    {
      warning ("Cannot convert non-floating-point type "
	       "to floating-point register value.");
      return;
    }

  /* Convert from TYPE.  This should be a no-op if TYPE is equivalent
     to the extended floating-point format used by the FPU.  */
  convert_typed_floating (from, type, to, builtin_type_m68881_ext);
  put_frame_register (frame, regnum, to);
}


/* There is a fair number of calling conventions that are in somewhat
   wide use.  The 68000/08/10 don't support an FPU, not even as a
   coprocessor.  All function return values are stored in %d0/%d1.
   Structures are returned in a static buffer, a pointer to which is
   returned in %d0.  This means that functions returning a structure
   are not re-entrant.  To avoid this problem some systems use a
   convention where the caller passes a pointer to a buffer in %a1
   where the return values is to be stored.  This convention is the
   default, and is implemented in the function m68k_return_value.

   The 68020/030/040/060 do support an FPU, either as a coprocessor
   (68881/2) or built-in (68040/68060).  That's why System V release 4
   (SVR4) instroduces a new calling convention specified by the SVR4
   psABI.  Integer values are returned in %d0/%d1, pointer return
   values in %a0 and floating values in %fp0.  When calling functions
   returning a structure the caller should pass a pointer to a buffer
   for the return value in %a0.  This convention is implemented in the
   function m68k_svr4_return_value, and by appropriately setting the
   struct_value_regnum member of `struct gdbarch_tdep'.

   GNU/Linux returns values in the same way as SVR4 does, but uses %a1
   for passing the structure return value buffer.

   GCC can also generate code where small structures are returned in
   %d0/%d1 instead of in memory by using -freg-struct-return.  This is
   the default on NetBSD a.out, OpenBSD and GNU/Linux and several
   embedded systems.  This convention is implemented by setting the
   struct_return member of `struct gdbarch_tdep' to reg_struct_return.  */

/* Read a function return value of TYPE from REGCACHE, and copy that
   into VALBUF.  */

static void
m68k_extract_return_value (struct type *type, struct regcache *regcache,
			   void *valbuf)
{
  int len = TYPE_LENGTH (type);
  char buf[M68K_MAX_REGISTER_SIZE];

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

static void
m68k_svr4_extract_return_value (struct type *type, struct regcache *regcache,
				void *valbuf)
{
  int len = TYPE_LENGTH (type);
  char buf[M68K_MAX_REGISTER_SIZE];

  if (TYPE_CODE (type) == TYPE_CODE_FLT)
    {
      regcache_raw_read (regcache, M68K_FP0_REGNUM, buf);
      convert_typed_floating (buf, builtin_type_m68881_ext, valbuf, type);
    }
  else if (TYPE_CODE (type) == TYPE_CODE_PTR && len == 4)
    regcache_raw_read (regcache, M68K_A0_REGNUM, valbuf);
  else
    m68k_extract_return_value (type, regcache, valbuf);
}

/* Write a function return value of TYPE from VALBUF into REGCACHE.  */

static void
m68k_store_return_value (struct type *type, struct regcache *regcache,
			 const void *valbuf)
{
  int len = TYPE_LENGTH (type);

  if (len <= 4)
    regcache_raw_write_part (regcache, M68K_D0_REGNUM, 4 - len, len, valbuf);
  else if (len <= 8)
    {
      regcache_raw_write_part (regcache, M68K_D0_REGNUM, 8 - len,
			       len - 4, valbuf);
      regcache_raw_write (regcache, M68K_D1_REGNUM,
			  (char *) valbuf + (len - 4));
    }
  else
    internal_error (__FILE__, __LINE__,
		    "Cannot store return value of %d bytes long.", len);
}

static void
m68k_svr4_store_return_value (struct type *type, struct regcache *regcache,
			      const void *valbuf)
{
  int len = TYPE_LENGTH (type);

  if (TYPE_CODE (type) == TYPE_CODE_FLT)
    {
      char buf[M68K_MAX_REGISTER_SIZE];
      convert_typed_floating (valbuf, type, buf, builtin_type_m68881_ext);
      regcache_raw_write (regcache, M68K_FP0_REGNUM, buf);
    }
  else if (TYPE_CODE (type) == TYPE_CODE_PTR && len == 4)
    {
      regcache_raw_write (regcache, M68K_A0_REGNUM, valbuf);
      regcache_raw_write (regcache, M68K_D0_REGNUM, valbuf);
    }
  else
    m68k_store_return_value (type, regcache, valbuf);
}

/* Return non-zero if TYPE, which is assumed to be a structure or
   union type, should be returned in registers for architecture
   GDBARCH.  */

static int
m68k_reg_struct_return_p (struct gdbarch *gdbarch, struct type *type)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);
  enum type_code code = TYPE_CODE (type);
  int len = TYPE_LENGTH (type);

  gdb_assert (code == TYPE_CODE_STRUCT || code == TYPE_CODE_UNION);

  if (tdep->struct_return == pcc_struct_return)
    return 0;

  return (len == 1 || len == 2 || len == 4 || len == 8);
}

/* Determine, for architecture GDBARCH, how a return value of TYPE
   should be returned.  If it is supposed to be returned in registers,
   and READBUF is non-zero, read the appropriate value from REGCACHE,
   and copy it into READBUF.  If WRITEBUF is non-zero, write the value
   from WRITEBUF into REGCACHE.  */

static enum return_value_convention
m68k_return_value (struct gdbarch *gdbarch, struct type *type,
		   struct regcache *regcache, void *readbuf,
		   const void *writebuf)
{
  enum type_code code = TYPE_CODE (type);

  if ((code == TYPE_CODE_STRUCT || code == TYPE_CODE_UNION)
      && !m68k_reg_struct_return_p (gdbarch, type))
    return RETURN_VALUE_STRUCT_CONVENTION;

  /* GCC returns a `long double' in memory.  */
  if (code == TYPE_CODE_FLT && TYPE_LENGTH (type) == 12)
    return RETURN_VALUE_STRUCT_CONVENTION;

  if (readbuf)
    m68k_extract_return_value (type, regcache, readbuf);
  if (writebuf)
    m68k_store_return_value (type, regcache, writebuf);

  return RETURN_VALUE_REGISTER_CONVENTION;
}

static enum return_value_convention
m68k_svr4_return_value (struct gdbarch *gdbarch, struct type *type,
			struct regcache *regcache, void *readbuf,
			const void *writebuf)
{
  enum type_code code = TYPE_CODE (type);

  if ((code == TYPE_CODE_STRUCT || code == TYPE_CODE_UNION)
      && !m68k_reg_struct_return_p (gdbarch, type))
    {
      /* The System V ABI says that:

	 "A function returning a structure or union also sets %a0 to
	 the value it finds in %a0.  Thus when the caller receives
	 control again, the address of the returned object resides in
	 register %a0."

	 So the ABI guarantees that we can always find the return
	 value just after the function has returned.  */

      if (readbuf)
	{
	  ULONGEST addr;

	  regcache_raw_read_unsigned (regcache, M68K_A0_REGNUM, &addr);
	  read_memory (addr, readbuf, TYPE_LENGTH (type));
	}

      return RETURN_VALUE_ABI_RETURNS_ADDRESS;
    }

  /* This special case is for structures consisting of a single
     `float' or `double' member.  These structures are returned in
     %fp0.  For these structures, we call ourselves recursively,
     changing TYPE into the type of the first member of the structure.
     Since that should work for all structures that have only one
     member, we don't bother to check the member's type here.  */
  if (code == TYPE_CODE_STRUCT && TYPE_NFIELDS (type) == 1)
    {
      type = check_typedef (TYPE_FIELD_TYPE (type, 0));
      return m68k_svr4_return_value (gdbarch, type, regcache,
				     readbuf, writebuf);
    }

  if (readbuf)
    m68k_svr4_extract_return_value (type, regcache, readbuf);
  if (writebuf)
    m68k_svr4_store_return_value (type, regcache, writebuf);

  return RETURN_VALUE_REGISTER_CONVENTION;
}


static CORE_ADDR
m68k_push_dummy_call (struct gdbarch *gdbarch, struct value *function,
		      struct regcache *regcache, CORE_ADDR bp_addr, int nargs,
		      struct value **args, CORE_ADDR sp, int struct_return,
		      CORE_ADDR struct_addr)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);
  char buf[4];
  int i;

  /* Push arguments in reverse order.  */
  for (i = nargs - 1; i >= 0; i--)
    {
      struct type *value_type = VALUE_ENCLOSING_TYPE (args[i]);
      int len = TYPE_LENGTH (value_type);
      int container_len = (len + 3) & ~3;
      int offset;

      /* Non-scalars bigger than 4 bytes are left aligned, others are
	 right aligned.  */
      if ((TYPE_CODE (value_type) == TYPE_CODE_STRUCT
	   || TYPE_CODE (value_type) == TYPE_CODE_UNION
	   || TYPE_CODE (value_type) == TYPE_CODE_ARRAY)
	  && len > 4)
	offset = 0;
      else
	offset = container_len - len;
      sp -= container_len;
      write_memory (sp + offset, VALUE_CONTENTS_ALL (args[i]), len);
    }

  /* Store struct value address.  */
  if (struct_return)
    {
      store_unsigned_integer (buf, 4, struct_addr);
      regcache_cooked_write (regcache, tdep->struct_value_regnum, buf);
    }

  /* Store return address.  */
  sp -= 4;
  store_unsigned_integer (buf, 4, bp_addr);
  write_memory (sp, buf, 4);

  /* Finally, update the stack pointer...  */
  store_unsigned_integer (buf, 4, sp);
  regcache_cooked_write (regcache, M68K_SP_REGNUM, buf);

  /* ...and fake a frame pointer.  */
  regcache_cooked_write (regcache, M68K_FP_REGNUM, buf);

  /* DWARF2/GCC uses the stack address *before* the function call as a
     frame's CFA.  */
  return sp + 8;
}

struct m68k_frame_cache
{
  /* Base address.  */
  CORE_ADDR base;
  CORE_ADDR sp_offset;
  CORE_ADDR pc;

  /* Saved registers.  */
  CORE_ADDR saved_regs[M68K_NUM_REGS];
  CORE_ADDR saved_sp;

  /* Stack space reserved for local variables.  */
  long locals;
};

/* Allocate and initialize a frame cache.  */

static struct m68k_frame_cache *
m68k_alloc_frame_cache (void)
{
  struct m68k_frame_cache *cache;
  int i;

  cache = FRAME_OBSTACK_ZALLOC (struct m68k_frame_cache);

  /* Base address.  */
  cache->base = 0;
  cache->sp_offset = -4;
  cache->pc = 0;

  /* Saved registers.  We initialize these to -1 since zero is a valid
     offset (that's where %fp is supposed to be stored).  */
  for (i = 0; i < M68K_NUM_REGS; i++)
    cache->saved_regs[i] = -1;

  /* Frameless until proven otherwise.  */
  cache->locals = -1;

  return cache;
}

/* Check whether PC points at a code that sets up a new stack frame.
   If so, it updates CACHE and returns the address of the first
   instruction after the sequence that sets removes the "hidden"
   argument from the stack or CURRENT_PC, whichever is smaller.
   Otherwise, return PC.  */

static CORE_ADDR
m68k_analyze_frame_setup (CORE_ADDR pc, CORE_ADDR current_pc,
			  struct m68k_frame_cache *cache)
{
  int op;

  if (pc >= current_pc)
    return current_pc;

  op = read_memory_unsigned_integer (pc, 2);

  if (op == P_LINKW_FP || op == P_LINKL_FP || op == P_PEA_FP)
    {
      cache->saved_regs[M68K_FP_REGNUM] = 0;
      cache->sp_offset += 4;
      if (op == P_LINKW_FP)
	{
	  /* link.w %fp, #-N */
	  /* link.w %fp, #0; adda.l #-N, %sp */
	  cache->locals = -read_memory_integer (pc + 2, 2);

	  if (pc + 4 < current_pc && cache->locals == 0)
	    {
	      op = read_memory_unsigned_integer (pc + 4, 2);
	      if (op == P_ADDAL_SP)
		{
		  cache->locals = read_memory_integer (pc + 6, 4);
		  return pc + 10;
		}
	    }

	  return pc + 4;
	}
      else if (op == P_LINKL_FP)
	{
	  /* link.l %fp, #-N */
	  cache->locals = -read_memory_integer (pc + 2, 4);
	  return pc + 6;
	}
      else
	{
	  /* pea (%fp); movea.l %sp, %fp */
	  cache->locals = 0;

	  if (pc + 2 < current_pc)
	    {
	      op = read_memory_unsigned_integer (pc + 2, 2);

	      if (op == P_MOVEAL_SP_FP)
		{
		  /* move.l %sp, %fp */
		  return pc + 4;
		}
	    }

	  return pc + 2;
	}
    }
  else if ((op & 0170777) == P_SUBQW_SP || (op & 0170777) == P_SUBQL_SP)
    {
      /* subq.[wl] #N,%sp */
      /* subq.[wl] #8,%sp; subq.[wl] #N,%sp */
      cache->locals = (op & 07000) == 0 ? 8 : (op & 07000) >> 9;
      if (pc + 2 < current_pc)
	{
	  op = read_memory_unsigned_integer (pc + 2, 2);
	  if ((op & 0170777) == P_SUBQW_SP || (op & 0170777) == P_SUBQL_SP)
	    {
	      cache->locals += (op & 07000) == 0 ? 8 : (op & 07000) >> 9;
	      return pc + 4;
	    }
	}
      return pc + 2;
    }
  else if (op == P_ADDAW_SP || op == P_LEA_SP_SP)
    {
      /* adda.w #-N,%sp */
      /* lea (-N,%sp),%sp */
      cache->locals = -read_memory_integer (pc + 2, 2);
      return pc + 4;
    }
  else if (op == P_ADDAL_SP)
    {
      /* adda.l #-N,%sp */
      cache->locals = -read_memory_integer (pc + 2, 4);
      return pc + 6;
    }

  return pc;
}

/* Check whether PC points at code that saves registers on the stack.
   If so, it updates CACHE and returns the address of the first
   instruction after the register saves or CURRENT_PC, whichever is
   smaller.  Otherwise, return PC.  */

static CORE_ADDR
m68k_analyze_register_saves (CORE_ADDR pc, CORE_ADDR current_pc,
			     struct m68k_frame_cache *cache)
{
  if (cache->locals >= 0)
    {
      CORE_ADDR offset;
      int op;
      int i, mask, regno;

      offset = -4 - cache->locals;
      while (pc < current_pc)
	{
	  op = read_memory_unsigned_integer (pc, 2);
	  if (op == P_FMOVEMX_SP)
	    {
	      /* fmovem.x REGS,-(%sp) */
	      op = read_memory_unsigned_integer (pc + 2, 2);
	      if ((op & 0xff00) == 0xe000)
		{
		  mask = op & 0xff;
		  for (i = 0; i < 16; i++, mask >>= 1)
		    {
		      if (mask & 1)
			{
			  cache->saved_regs[i + M68K_FP0_REGNUM] = offset;
			  offset -= 12;
			}
		    }
		  pc += 4;
		}
	      else
		break;
	    }
	  else if ((op & 0170677) == P_MOVEL_SP)
	    {
	      /* move.l %R,-(%sp) */
	      regno = ((op & 07000) >> 9) | ((op & 0100) >> 3);
	      cache->saved_regs[regno] = offset;
	      offset -= 4;
	      pc += 2;
	    }
	  else if (op == P_MOVEML_SP)
	    {
	      /* movem.l REGS,-(%sp) */
	      mask = read_memory_unsigned_integer (pc + 2, 2);
	      for (i = 0; i < 16; i++, mask >>= 1)
		{
		  if (mask & 1)
		    {
		      cache->saved_regs[15 - i] = offset;
		      offset -= 4;
		    }
		}
	      pc += 4;
	    }
	  else
	    break;
	}
    }

  return pc;
}


/* Do a full analysis of the prologue at PC and update CACHE
   accordingly.  Bail out early if CURRENT_PC is reached.  Return the
   address where the analysis stopped.

   We handle all cases that can be generated by gcc.

   For allocating a stack frame:

   link.w %a6,#-N
   link.l %a6,#-N
   pea (%fp); move.l %sp,%fp
   link.w %a6,#0; add.l #-N,%sp
   subq.l #N,%sp
   subq.w #N,%sp
   subq.w #8,%sp; subq.w #N-8,%sp
   add.w #-N,%sp
   lea (-N,%sp),%sp
   add.l #-N,%sp

   For saving registers:

   fmovem.x REGS,-(%sp)
   move.l R1,-(%sp)
   move.l R1,-(%sp); move.l R2,-(%sp)
   movem.l REGS,-(%sp)

   For setting up the PIC register:

   lea (%pc,N),%a5

   */

static CORE_ADDR
m68k_analyze_prologue (CORE_ADDR pc, CORE_ADDR current_pc,
		       struct m68k_frame_cache *cache)
{
  unsigned int op;

  pc = m68k_analyze_frame_setup (pc, current_pc, cache);
  pc = m68k_analyze_register_saves (pc, current_pc, cache);
  if (pc >= current_pc)
    return current_pc;

  /* Check for GOT setup.  */
  op = read_memory_unsigned_integer (pc, 4);
  if (op == P_LEA_PC_A5)
    {
      /* lea (%pc,N),%a5 */
      return pc + 6;
    }

  return pc;
}

/* Return PC of first real instruction.  */

static CORE_ADDR
m68k_skip_prologue (CORE_ADDR start_pc)
{
  struct m68k_frame_cache cache;
  CORE_ADDR pc;
  int op;

  cache.locals = -1;
  pc = m68k_analyze_prologue (start_pc, (CORE_ADDR) -1, &cache);
  if (cache.locals < 0)
    return start_pc;
  return pc;
}

static CORE_ADDR
m68k_unwind_pc (struct gdbarch *gdbarch, struct frame_info *next_frame)
{
  char buf[8];

  frame_unwind_register (next_frame, PC_REGNUM, buf);
  return extract_typed_address (buf, builtin_type_void_func_ptr);
}

/* Normal frames.  */

static struct m68k_frame_cache *
m68k_frame_cache (struct frame_info *next_frame, void **this_cache)
{
  struct m68k_frame_cache *cache;
  char buf[4];
  int i;

  if (*this_cache)
    return *this_cache;

  cache = m68k_alloc_frame_cache ();
  *this_cache = cache;

  /* In principle, for normal frames, %fp holds the frame pointer,
     which holds the base address for the current stack frame.
     However, for functions that don't need it, the frame pointer is
     optional.  For these "frameless" functions the frame pointer is
     actually the frame pointer of the calling frame.  Signal
     trampolines are just a special case of a "frameless" function.
     They (usually) share their frame pointer with the frame that was
     in progress when the signal occurred.  */

  frame_unwind_register (next_frame, M68K_FP_REGNUM, buf);
  cache->base = extract_unsigned_integer (buf, 4);
  if (cache->base == 0)
    return cache;

  /* For normal frames, %pc is stored at 4(%fp).  */
  cache->saved_regs[M68K_PC_REGNUM] = 4;

  cache->pc = frame_func_unwind (next_frame);
  if (cache->pc != 0)
    m68k_analyze_prologue (cache->pc, frame_pc_unwind (next_frame), cache);

  if (cache->locals < 0)
    {
      /* We didn't find a valid frame, which means that CACHE->base
	 currently holds the frame pointer for our calling frame.  If
	 we're at the start of a function, or somewhere half-way its
	 prologue, the function's frame probably hasn't been fully
	 setup yet.  Try to reconstruct the base address for the stack
	 frame by looking at the stack pointer.  For truly "frameless"
	 functions this might work too.  */

      frame_unwind_register (next_frame, M68K_SP_REGNUM, buf);
      cache->base = extract_unsigned_integer (buf, 4) + cache->sp_offset;
    }

  /* Now that we have the base address for the stack frame we can
     calculate the value of %sp in the calling frame.  */
  cache->saved_sp = cache->base + 8;

  /* Adjust all the saved registers such that they contain addresses
     instead of offsets.  */
  for (i = 0; i < M68K_NUM_REGS; i++)
    if (cache->saved_regs[i] != -1)
      cache->saved_regs[i] += cache->base;

  return cache;
}

static void
m68k_frame_this_id (struct frame_info *next_frame, void **this_cache,
		    struct frame_id *this_id)
{
  struct m68k_frame_cache *cache = m68k_frame_cache (next_frame, this_cache);

  /* This marks the outermost frame.  */
  if (cache->base == 0)
    return;

  /* See the end of m68k_push_dummy_call.  */
  *this_id = frame_id_build (cache->base + 8, cache->pc);
}

static void
m68k_frame_prev_register (struct frame_info *next_frame, void **this_cache,
			  int regnum, int *optimizedp,
			  enum lval_type *lvalp, CORE_ADDR *addrp,
			  int *realnump, void *valuep)
{
  struct m68k_frame_cache *cache = m68k_frame_cache (next_frame, this_cache);

  gdb_assert (regnum >= 0);

  if (regnum == M68K_SP_REGNUM && cache->saved_sp)
    {
      *optimizedp = 0;
      *lvalp = not_lval;
      *addrp = 0;
      *realnump = -1;
      if (valuep)
	{
	  /* Store the value.  */
	  store_unsigned_integer (valuep, 4, cache->saved_sp);
	}
      return;
    }

  if (regnum < M68K_NUM_REGS && cache->saved_regs[regnum] != -1)
    {
      *optimizedp = 0;
      *lvalp = lval_memory;
      *addrp = cache->saved_regs[regnum];
      *realnump = -1;
      if (valuep)
	{
	  /* Read the value in from memory.  */
	  read_memory (*addrp, valuep,
		       register_size (current_gdbarch, regnum));
	}
      return;
    }

  frame_register_unwind (next_frame, regnum,
			 optimizedp, lvalp, addrp, realnump, valuep);
}

static const struct frame_unwind m68k_frame_unwind =
{
  NORMAL_FRAME,
  m68k_frame_this_id,
  m68k_frame_prev_register
};

static const struct frame_unwind *
m68k_frame_sniffer (struct frame_info *next_frame)
{
  return &m68k_frame_unwind;
}

static CORE_ADDR
m68k_frame_base_address (struct frame_info *next_frame, void **this_cache)
{
  struct m68k_frame_cache *cache = m68k_frame_cache (next_frame, this_cache);

  return cache->base;
}

static const struct frame_base m68k_frame_base =
{
  &m68k_frame_unwind,
  m68k_frame_base_address,
  m68k_frame_base_address,
  m68k_frame_base_address
};

static struct frame_id
m68k_unwind_dummy_id (struct gdbarch *gdbarch, struct frame_info *next_frame)
{
  char buf[4];
  CORE_ADDR fp;

  frame_unwind_register (next_frame, M68K_FP_REGNUM, buf);
  fp = extract_unsigned_integer (buf, 4);

  /* See the end of m68k_push_dummy_call.  */
  return frame_id_build (fp + 8, frame_pc_unwind (next_frame));
}

#ifdef USE_PROC_FS		/* Target dependent support for /proc */

#include <sys/procfs.h>

/* Prototypes for supply_gregset etc. */
#include "gregset.h"

/*  The /proc interface divides the target machine's register set up into
   two different sets, the general register set (gregset) and the floating
   point register set (fpregset).  For each set, there is an ioctl to get
   the current register set and another ioctl to set the current values.

   The actual structure passed through the ioctl interface is, of course,
   naturally machine dependent, and is different for each set of registers.
   For the m68k for example, the general register set is typically defined
   by:

   typedef int gregset_t[18];

   #define      R_D0    0
   ...
   #define      R_PS    17

   and the floating point set by:

   typedef      struct fpregset {
   int  f_pcr;
   int  f_psr;
   int  f_fpiaddr;
   int  f_fpregs[8][3];         (8 regs, 96 bits each)
   } fpregset_t;

   These routines provide the packing and unpacking of gregset_t and
   fpregset_t formatted data.

 */

/* Atari SVR4 has R_SR but not R_PS */

#if !defined (R_PS) && defined (R_SR)
#define R_PS R_SR
#endif

/*  Given a pointer to a general register set in /proc format (gregset_t *),
   unpack the register contents and supply them as gdb's idea of the current
   register values. */

void
supply_gregset (gregset_t *gregsetp)
{
  int regi;
  greg_t *regp = (greg_t *) gregsetp;

  for (regi = 0; regi < R_PC; regi++)
    {
      regcache_raw_supply (current_regcache, regi, (char *) (regp + regi));
    }
  regcache_raw_supply (current_regcache, PS_REGNUM, (char *) (regp + R_PS));
  regcache_raw_supply (current_regcache, PC_REGNUM, (char *) (regp + R_PC));
}

void
fill_gregset (gregset_t *gregsetp, int regno)
{
  int regi;
  greg_t *regp = (greg_t *) gregsetp;

  for (regi = 0; regi < R_PC; regi++)
    {
      if (regno == -1 || regno == regi)
	regcache_raw_collect (current_regcache, regi, regp + regi);
    }
  if (regno == -1 || regno == PS_REGNUM)
    regcache_raw_collect (current_regcache, PS_REGNUM, regp + R_PS);
  if (regno == -1 || regno == PC_REGNUM)
    regcache_raw_collect (current_regcache, PC_REGNUM, regp + R_PC);
}

#if defined (FP0_REGNUM)

/*  Given a pointer to a floating point register set in /proc format
   (fpregset_t *), unpack the register contents and supply them as gdb's
   idea of the current floating point register values. */

void
supply_fpregset (fpregset_t *fpregsetp)
{
  int regi;
  char *from;

  for (regi = FP0_REGNUM; regi < M68K_FPC_REGNUM; regi++)
    {
      from = (char *) &(fpregsetp->f_fpregs[regi - FP0_REGNUM][0]);
      regcache_raw_supply (current_regcache, regi, from);
    }
  regcache_raw_supply (current_regcache, M68K_FPC_REGNUM,
		       (char *) &(fpregsetp->f_pcr));
  regcache_raw_supply (current_regcache, M68K_FPS_REGNUM,
		       (char *) &(fpregsetp->f_psr));
  regcache_raw_supply (current_regcache, M68K_FPI_REGNUM,
		       (char *) &(fpregsetp->f_fpiaddr));
}

/*  Given a pointer to a floating point register set in /proc format
   (fpregset_t *), update the register specified by REGNO from gdb's idea
   of the current floating point register set.  If REGNO is -1, update
   them all. */

void
fill_fpregset (fpregset_t *fpregsetp, int regno)
{
  int regi;

  for (regi = FP0_REGNUM; regi < M68K_FPC_REGNUM; regi++)
    {
      if (regno == -1 || regno == regi)
	regcache_raw_collect (current_regcache, regi,
			      &fpregsetp->f_fpregs[regi - FP0_REGNUM][0]);
    }
  if (regno == -1 || regno == M68K_FPC_REGNUM)
    regcache_raw_collect (current_regcache, M68K_FPC_REGNUM,
			  &fpregsetp->f_pcr);
  if (regno == -1 || regno == M68K_FPS_REGNUM)
    regcache_raw_collect (current_regcache, M68K_FPS_REGNUM,
			  &fpregsetp->f_psr);
  if (regno == -1 || regno == M68K_FPI_REGNUM)
    regcache_raw_collect (current_regcache, M68K_FPI_REGNUM,
			  &fpregsetp->f_fpiaddr);
}

#endif /* defined (FP0_REGNUM) */

#endif /* USE_PROC_FS */

/* Figure out where the longjmp will land.  Slurp the args out of the stack.
   We expect the first arg to be a pointer to the jmp_buf structure from which
   we extract the pc (JB_PC) that we will land at.  The pc is copied into PC.
   This routine returns true on success. */

static int
m68k_get_longjmp_target (CORE_ADDR *pc)
{
  char *buf;
  CORE_ADDR sp, jb_addr;
  struct gdbarch_tdep *tdep = gdbarch_tdep (current_gdbarch);

  if (tdep->jb_pc < 0)
    {
      internal_error (__FILE__, __LINE__,
		      "m68k_get_longjmp_target: not implemented");
      return 0;
    }

  buf = alloca (TARGET_PTR_BIT / TARGET_CHAR_BIT);
  sp = read_register (SP_REGNUM);

  if (target_read_memory (sp + SP_ARG0,	/* Offset of first arg on stack */
			  buf, TARGET_PTR_BIT / TARGET_CHAR_BIT))
    return 0;

  jb_addr = extract_unsigned_integer (buf, TARGET_PTR_BIT / TARGET_CHAR_BIT);

  if (target_read_memory (jb_addr + tdep->jb_pc * tdep->jb_elt_size, buf,
			  TARGET_PTR_BIT / TARGET_CHAR_BIT))
    return 0;

  *pc = extract_unsigned_integer (buf, TARGET_PTR_BIT / TARGET_CHAR_BIT);
  return 1;
}


/* System V Release 4 (SVR4).  */

void
m68k_svr4_init_abi (struct gdbarch_info info, struct gdbarch *gdbarch)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);

  /* SVR4 uses a different calling convention.  */
  set_gdbarch_return_value (gdbarch, m68k_svr4_return_value);

  /* SVR4 uses %a0 instead of %a1.  */
  tdep->struct_value_regnum = M68K_A0_REGNUM;
}


/* Function: m68k_gdbarch_init
   Initializer function for the m68k gdbarch vector.
   Called by gdbarch.  Sets up the gdbarch vector(s) for this target. */

static struct gdbarch *
m68k_gdbarch_init (struct gdbarch_info info, struct gdbarch_list *arches)
{
  struct gdbarch_tdep *tdep = NULL;
  struct gdbarch *gdbarch;

  /* find a candidate among the list of pre-declared architectures. */
  arches = gdbarch_list_lookup_by_info (arches, &info);
  if (arches != NULL)
    return (arches->gdbarch);

  tdep = xmalloc (sizeof (struct gdbarch_tdep));
  gdbarch = gdbarch_alloc (&info, tdep);

  set_gdbarch_long_double_format (gdbarch, &floatformat_m68881_ext);
  set_gdbarch_long_double_bit (gdbarch, 96);

  set_gdbarch_skip_prologue (gdbarch, m68k_skip_prologue);
  set_gdbarch_breakpoint_from_pc (gdbarch, m68k_local_breakpoint_from_pc);

  /* Stack grows down. */
  set_gdbarch_inner_than (gdbarch, core_addr_lessthan);

  set_gdbarch_believe_pcc_promotion (gdbarch, 1);
  set_gdbarch_decr_pc_after_break (gdbarch, 2);

  set_gdbarch_frame_args_skip (gdbarch, 8);

  set_gdbarch_register_type (gdbarch, m68k_register_type);
  set_gdbarch_register_name (gdbarch, m68k_register_name);
  set_gdbarch_num_regs (gdbarch, 29);
  set_gdbarch_register_bytes_ok (gdbarch, m68k_register_bytes_ok);
  set_gdbarch_sp_regnum (gdbarch, M68K_SP_REGNUM);
  set_gdbarch_pc_regnum (gdbarch, M68K_PC_REGNUM);
  set_gdbarch_ps_regnum (gdbarch, M68K_PS_REGNUM);
  set_gdbarch_fp0_regnum (gdbarch, M68K_FP0_REGNUM);
  set_gdbarch_convert_register_p (gdbarch, m68k_convert_register_p);
  set_gdbarch_register_to_value (gdbarch,  m68k_register_to_value);
  set_gdbarch_value_to_register (gdbarch, m68k_value_to_register);

  set_gdbarch_push_dummy_call (gdbarch, m68k_push_dummy_call);
  set_gdbarch_return_value (gdbarch, m68k_return_value);

  /* Disassembler.  */
  set_gdbarch_print_insn (gdbarch, print_insn_m68k);

#if defined JB_PC && defined JB_ELEMENT_SIZE
  tdep->jb_pc = JB_PC;
  tdep->jb_elt_size = JB_ELEMENT_SIZE;
#else
  tdep->jb_pc = -1;
#endif
  tdep->struct_value_regnum = M68K_A1_REGNUM;
  tdep->struct_return = reg_struct_return;

  /* Frame unwinder.  */
  set_gdbarch_unwind_dummy_id (gdbarch, m68k_unwind_dummy_id);
  set_gdbarch_unwind_pc (gdbarch, m68k_unwind_pc);

  /* Hook in the DWARF CFI frame unwinder.  */
  frame_unwind_append_sniffer (gdbarch, dwarf2_frame_sniffer);

  frame_base_set_default (gdbarch, &m68k_frame_base);

  /* Hook in ABI-specific overrides, if they have been registered.  */
  gdbarch_init_osabi (info, gdbarch);

  /* Now we have tuned the configuration, set a few final things,
     based on what the OS ABI has told us.  */

  if (tdep->jb_pc >= 0)
    set_gdbarch_get_longjmp_target (gdbarch, m68k_get_longjmp_target);

  frame_unwind_append_sniffer (gdbarch, m68k_frame_sniffer);

  return gdbarch;
}


static void
m68k_dump_tdep (struct gdbarch *current_gdbarch, struct ui_file *file)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (current_gdbarch);

  if (tdep == NULL)
    return;
}

extern initialize_file_ftype _initialize_m68k_tdep; /* -Wmissing-prototypes */

void
_initialize_m68k_tdep (void)
{
  gdbarch_register (bfd_arch_m68k, m68k_gdbarch_init, m68k_dump_tdep);
}
