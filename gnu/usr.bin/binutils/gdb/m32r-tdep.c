/* Target-dependent code for Renesas M32R, for GDB.

   Copyright 1996, 1998, 1999, 2000, 2001, 2002, 2003 Free Software
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

#include "defs.h"
#include "frame.h"
#include "frame-unwind.h"
#include "frame-base.h"
#include "symtab.h"
#include "gdbtypes.h"
#include "gdbcmd.h"
#include "gdbcore.h"
#include "gdb_string.h"
#include "value.h"
#include "inferior.h"
#include "symfile.h"
#include "objfiles.h"
#include "language.h"
#include "arch-utils.h"
#include "regcache.h"
#include "trad-frame.h"
#include "dis-asm.h"

#include "gdb_assert.h"

struct gdbarch_tdep
{
  /* gdbarch target dependent data here. Currently unused for M32R. */
};

/* m32r register names. */

enum
{
  R0_REGNUM = 0,
  R3_REGNUM = 3,
  M32R_FP_REGNUM = 13,
  LR_REGNUM = 14,
  M32R_SP_REGNUM = 15,
  PSW_REGNUM = 16,
  M32R_PC_REGNUM = 21,
  /* m32r calling convention. */
  ARG1_REGNUM = R0_REGNUM,
  ARGN_REGNUM = R3_REGNUM,
  RET1_REGNUM = R0_REGNUM,
};

/* Local functions */

extern void _initialize_m32r_tdep (void);

static CORE_ADDR
m32r_frame_align (struct gdbarch *gdbarch, CORE_ADDR sp)
{
  /* Align to the size of an instruction (so that they can safely be
     pushed onto the stack.  */
  return sp & ~3;
}

/* Should we use DEPRECATED_EXTRACT_STRUCT_VALUE_ADDRESS instead of
   EXTRACT_RETURN_VALUE?  GCC_P is true if compiled with gcc and TYPE
   is the type (which is known to be struct, union or array).

   The m32r returns anything less than 8 bytes in size in
   registers. */

static int
m32r_use_struct_convention (int gcc_p, struct type *type)
{
  return (TYPE_LENGTH (type) > 8);
}


/* BREAKPOINT */
#define M32R_BE_BREAKPOINT32 {0x10, 0xf1, 0x70, 0x00}
#define M32R_LE_BREAKPOINT32 {0xf1, 0x10, 0x00, 0x70}
#define M32R_BE_BREAKPOINT16 {0x10, 0xf1}
#define M32R_LE_BREAKPOINT16 {0xf1, 0x10}

static int
m32r_memory_insert_breakpoint (CORE_ADDR addr, char *contents_cache)
{
  int val;
  unsigned char *bp;
  int bplen;

  bplen = (addr & 3) ? 2 : 4;

  /* Save the memory contents.  */
  val = target_read_memory (addr, contents_cache, bplen);
  if (val != 0)
    return val;			/* return error */

  /* Determine appropriate breakpoint contents and size for this address.  */
  if (TARGET_BYTE_ORDER == BFD_ENDIAN_BIG)
    {
      if (((addr & 3) == 0)
	  && ((contents_cache[0] & 0x80) || (contents_cache[2] & 0x80)))
	{
	  static unsigned char insn[] = M32R_BE_BREAKPOINT32;
	  bp = insn;
	  bplen = sizeof (insn);
	}
      else
	{
	  static unsigned char insn[] = M32R_BE_BREAKPOINT16;
	  bp = insn;
	  bplen = sizeof (insn);
	}
    }
  else
    {				/* little-endian */
      if (((addr & 3) == 0)
	  && ((contents_cache[1] & 0x80) || (contents_cache[3] & 0x80)))
	{
	  static unsigned char insn[] = M32R_LE_BREAKPOINT32;
	  bp = insn;
	  bplen = sizeof (insn);
	}
      else
	{
	  static unsigned char insn[] = M32R_LE_BREAKPOINT16;
	  bp = insn;
	  bplen = sizeof (insn);
	}
    }

  /* Write the breakpoint.  */
  val = target_write_memory (addr, (char *) bp, bplen);
  return val;
}

static int
m32r_memory_remove_breakpoint (CORE_ADDR addr, char *contents_cache)
{
  int val;
  int bplen;

  /* Determine appropriate breakpoint contents and size for this address.  */
  if (TARGET_BYTE_ORDER == BFD_ENDIAN_BIG)
    {
      if (((addr & 3) == 0)
	  && ((contents_cache[0] & 0x80) || (contents_cache[2] & 0x80)))
	{
	  static unsigned char insn[] = M32R_BE_BREAKPOINT32;
	  bplen = sizeof (insn);
	}
      else
	{
	  static unsigned char insn[] = M32R_BE_BREAKPOINT16;
	  bplen = sizeof (insn);
	}
    }
  else
    {
      /* little-endian */
      if (((addr & 3) == 0)
	  && ((contents_cache[1] & 0x80) || (contents_cache[3] & 0x80)))
	{
	  static unsigned char insn[] = M32R_BE_BREAKPOINT32;
	  bplen = sizeof (insn);
	}
      else
	{
	  static unsigned char insn[] = M32R_BE_BREAKPOINT16;
	  bplen = sizeof (insn);
	}
    }

  /* Write contents.  */
  val = target_write_memory (addr, contents_cache, bplen);
  return val;
}

static const unsigned char *
m32r_breakpoint_from_pc (CORE_ADDR *pcptr, int *lenptr)
{
  unsigned char *bp;

  /* Determine appropriate breakpoint.  */
  if (TARGET_BYTE_ORDER == BFD_ENDIAN_BIG)
    {
      if ((*pcptr & 3) == 0)
	{
	  static unsigned char insn[] = M32R_BE_BREAKPOINT32;
	  bp = insn;
	  *lenptr = sizeof (insn);
	}
      else
	{
	  static unsigned char insn[] = M32R_BE_BREAKPOINT16;
	  bp = insn;
	  *lenptr = sizeof (insn);
	}
    }
  else
    {
      if ((*pcptr & 3) == 0)
	{
	  static unsigned char insn[] = M32R_LE_BREAKPOINT32;
	  bp = insn;
	  *lenptr = sizeof (insn);
	}
      else
	{
	  static unsigned char insn[] = M32R_LE_BREAKPOINT16;
	  bp = insn;
	  *lenptr = sizeof (insn);
	}
    }

  return bp;
}


char *m32r_register_names[] = {
  "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7",
  "r8", "r9", "r10", "r11", "r12", "fp", "lr", "sp",
  "psw", "cbr", "spi", "spu", "bpc", "pc", "accl", "acch",
  "evb"
};

static int
m32r_num_regs (void)
{
  return (sizeof (m32r_register_names) / sizeof (m32r_register_names[0]));
}

static const char *
m32r_register_name (int reg_nr)
{
  if (reg_nr < 0)
    return NULL;
  if (reg_nr >= m32r_num_regs ())
    return NULL;
  return m32r_register_names[reg_nr];
}


/* Return the GDB type object for the "standard" data type
   of data in register N.  */

static struct type *
m32r_register_type (struct gdbarch *gdbarch, int reg_nr)
{
  if (reg_nr == M32R_PC_REGNUM)
    return builtin_type_void_func_ptr;
  else if (reg_nr == M32R_SP_REGNUM || reg_nr == M32R_FP_REGNUM)
    return builtin_type_void_data_ptr;
  else
    return builtin_type_int32;
}


/* Write into appropriate registers a function return value
   of type TYPE, given in virtual format.  

   Things always get returned in RET1_REGNUM, RET2_REGNUM. */

static void
m32r_store_return_value (struct type *type, struct regcache *regcache,
			 const void *valbuf)
{
  CORE_ADDR regval;
  int len = TYPE_LENGTH (type);

  regval = extract_unsigned_integer (valbuf, len > 4 ? 4 : len);
  regcache_cooked_write_unsigned (regcache, RET1_REGNUM, regval);

  if (len > 4)
    {
      regval = extract_unsigned_integer ((char *) valbuf + 4, len - 4);
      regcache_cooked_write_unsigned (regcache, RET1_REGNUM + 1, regval);
    }
}

/* Extract from an array REGBUF containing the (raw) register state
   the address in which a function should return its structure value,
   as a CORE_ADDR (or an expression that can be used as one).  */

static CORE_ADDR
m32r_extract_struct_value_address (struct regcache *regcache)
{
  ULONGEST addr;
  regcache_cooked_read_unsigned (regcache, ARG1_REGNUM, &addr);
  return addr;
}


/* This is required by skip_prologue. The results of decoding a prologue
   should be cached because this thrashing is getting nuts.  */

static void
decode_prologue (CORE_ADDR start_pc, CORE_ADDR scan_limit,
		 CORE_ADDR *pl_endptr)
{
  unsigned long framesize;
  int insn;
  int op1;
  int maybe_one_more = 0;
  CORE_ADDR after_prologue = 0;
  CORE_ADDR after_stack_adjust = 0;
  CORE_ADDR current_pc;

  framesize = 0;
  after_prologue = 0;

  for (current_pc = start_pc; current_pc < scan_limit; current_pc += 2)
    {
      insn = read_memory_unsigned_integer (current_pc, 2);

      /* If this is a 32 bit instruction, we dont want to examine its
         immediate data as though it were an instruction */
      if (current_pc & 0x02)
	{
	  /* Clear the parallel execution bit from 16 bit instruction */
	  if (maybe_one_more)
	    {
	      /* The last instruction was a branch, usually terminates
	         the series, but if this is a parallel instruction,
	         it may be a stack framing instruction */
	      if (!(insn & 0x8000))
		{
		  /* nope, we are really done */
		  break;
		}
	    }
	  /* decode this instruction further */
	  insn &= 0x7fff;
	}
      else
	{
	  if (maybe_one_more)
	    break;		/* This isnt the one more */
	  if (insn & 0x8000)
	    {
	      if (current_pc == scan_limit)
		scan_limit += 2;	/* extend the search */
	      current_pc += 2;	/* skip the immediate data */
	      if (insn == 0x8faf)	/* add3 sp, sp, xxxx */
		/* add 16 bit sign-extended offset */
		{
		  framesize +=
		    -((short) read_memory_unsigned_integer (current_pc, 2));
		}
	      else
		{
		  if (((insn >> 8) == 0xe4)	/* ld24 r4, xxxxxx; sub sp, r4 */
		      && read_memory_unsigned_integer (current_pc + 2,
						       2) == 0x0f24)
		    /* subtract 24 bit sign-extended negative-offset */
		    {
		      insn = read_memory_unsigned_integer (current_pc - 2, 4);
		      if (insn & 0x00800000)	/* sign extend */
			insn |= 0xff000000;	/* negative */
		      else
			insn &= 0x00ffffff;	/* positive */
		      framesize += insn;
		    }
		}
	      after_prologue = current_pc;
	      continue;
	    }
	}
      op1 = insn & 0xf000;	/* isolate just the first nibble */

      if ((insn & 0xf0ff) == 0x207f)
	{			/* st reg, @-sp */
	  int regno;
	  framesize += 4;
	  regno = ((insn >> 8) & 0xf);
	  after_prologue = 0;
	  continue;
	}
      if ((insn >> 8) == 0x4f)	/* addi sp, xx */
	/* add 8 bit sign-extended offset */
	{
	  int stack_adjust = (char) (insn & 0xff);

	  /* there are probably two of these stack adjustments:
	     1) A negative one in the prologue, and
	     2) A positive one in the epilogue.
	     We are only interested in the first one.  */

	  if (stack_adjust < 0)
	    {
	      framesize -= stack_adjust;
	      after_prologue = 0;
	      /* A frameless function may have no "mv fp, sp".
	         In that case, this is the end of the prologue.  */
	      after_stack_adjust = current_pc + 2;
	    }
	  continue;
	}
      if (insn == 0x1d8f)
	{			/* mv fp, sp */
	  after_prologue = current_pc + 2;
	  break;		/* end of stack adjustments */
	}
      /* Nop looks like a branch, continue explicitly */
      if (insn == 0x7000)
	{
	  after_prologue = current_pc + 2;
	  continue;		/* nop occurs between pushes */
	}
      /* End of prolog if any of these are branch instructions */
      if ((op1 == 0x7000) || (op1 == 0xb000) || (op1 == 0xf000))
	{
	  after_prologue = current_pc;
	  maybe_one_more = 1;
	  continue;
	}
      /* Some of the branch instructions are mixed with other types */
      if (op1 == 0x1000)
	{
	  int subop = insn & 0x0ff0;
	  if ((subop == 0x0ec0) || (subop == 0x0fc0))
	    {
	      after_prologue = current_pc;
	      maybe_one_more = 1;
	      continue;		/* jmp , jl */
	    }
	}
    }

  if (current_pc >= scan_limit)
    {
      if (pl_endptr)
	{
	  if (after_stack_adjust != 0)
	    /* We did not find a "mv fp,sp", but we DID find
	       a stack_adjust.  Is it safe to use that as the
	       end of the prologue?  I just don't know. */
	    {
	      *pl_endptr = after_stack_adjust;
	    }
	  else
	    /* We reached the end of the loop without finding the end
	       of the prologue.  No way to win -- we should report failure.  
	       The way we do that is to return the original start_pc.
	       GDB will set a breakpoint at the start of the function (etc.) */
	    *pl_endptr = start_pc;
	}
      return;
    }
  if (after_prologue == 0)
    after_prologue = current_pc;

  if (pl_endptr)
    *pl_endptr = after_prologue;
}				/*  decode_prologue */

/* Function: skip_prologue
   Find end of function prologue */

#define DEFAULT_SEARCH_LIMIT 44

CORE_ADDR
m32r_skip_prologue (CORE_ADDR pc)
{
  CORE_ADDR func_addr, func_end;
  struct symtab_and_line sal;

  /* See what the symbol table says */

  if (find_pc_partial_function (pc, NULL, &func_addr, &func_end))
    {
      sal = find_pc_line (func_addr, 0);

      if (sal.line != 0 && sal.end <= func_end)
	{
	  func_end = sal.end;
	}
      else
	/* Either there's no line info, or the line after the prologue is after
	   the end of the function.  In this case, there probably isn't a
	   prologue.  */
	{
	  func_end = min (func_end, func_addr + DEFAULT_SEARCH_LIMIT);
	}
    }
  else
    func_end = pc + DEFAULT_SEARCH_LIMIT;
  decode_prologue (pc, func_end, &sal.end);
  return sal.end;
}


struct m32r_unwind_cache
{
  /* The previous frame's inner most stack address.  Used as this
     frame ID's stack_addr.  */
  CORE_ADDR prev_sp;
  /* The frame's base, optionally used by the high-level debug info.  */
  CORE_ADDR base;
  int size;
  /* How far the SP and r13 (FP) have been offset from the start of
     the stack frame (as defined by the previous frame's stack
     pointer).  */
  LONGEST sp_offset;
  LONGEST r13_offset;
  int uses_frame;
  /* Table indicating the location of each and every register.  */
  struct trad_frame_saved_reg *saved_regs;
};

/* Put here the code to store, into fi->saved_regs, the addresses of
   the saved registers of frame described by FRAME_INFO.  This
   includes special registers such as pc and fp saved in special ways
   in the stack frame.  sp is even more special: the address we return
   for it IS the sp for the next frame. */

static struct m32r_unwind_cache *
m32r_frame_unwind_cache (struct frame_info *next_frame,
			 void **this_prologue_cache)
{
  CORE_ADDR pc;
  ULONGEST prev_sp;
  ULONGEST this_base;
  unsigned long op;
  int i;
  struct m32r_unwind_cache *info;

  if ((*this_prologue_cache))
    return (*this_prologue_cache);

  info = FRAME_OBSTACK_ZALLOC (struct m32r_unwind_cache);
  (*this_prologue_cache) = info;
  info->saved_regs = trad_frame_alloc_saved_regs (next_frame);

  info->size = 0;
  info->sp_offset = 0;

  info->uses_frame = 0;
  for (pc = frame_func_unwind (next_frame);
       pc > 0 && pc < frame_pc_unwind (next_frame); pc += 2)
    {
      if ((pc & 2) == 0)
	{
	  op = get_frame_memory_unsigned (next_frame, pc, 4);
	  if ((op & 0x80000000) == 0x80000000)
	    {
	      /* 32-bit instruction */
	      if ((op & 0xffff0000) == 0x8faf0000)
		{
		  /* add3 sp,sp,xxxx */
		  short n = op & 0xffff;
		  info->sp_offset += n;
		}
	      else if (((op >> 8) == 0xe4)	/* ld24 r4, xxxxxx; sub sp, r4 */
		       && get_frame_memory_unsigned (next_frame, pc + 4,
						     2) == 0x0f24)
		{
		  unsigned long n = op & 0xffffff;
		  info->sp_offset += n;
		  pc += 2;
		}
	      else
		break;

	      pc += 2;
	      continue;
	    }
	}

      /* 16-bit instructions */
      op = get_frame_memory_unsigned (next_frame, pc, 2) & 0x7fff;
      if ((op & 0xf0ff) == 0x207f)
	{
	  /* st rn, @-sp */
	  int regno = ((op >> 8) & 0xf);
	  info->sp_offset -= 4;
	  info->saved_regs[regno].addr = info->sp_offset;
	}
      else if ((op & 0xff00) == 0x4f00)
	{
	  /* addi sp, xx */
	  int n = (char) (op & 0xff);
	  info->sp_offset += n;
	}
      else if (op == 0x1d8f)
	{
	  /* mv fp, sp */
	  info->uses_frame = 1;
	  info->r13_offset = info->sp_offset;
	}
      else if (op == 0x7000)
	/* nop */
	continue;
      else
	break;
    }

  info->size = -info->sp_offset;

  /* Compute the previous frame's stack pointer (which is also the
     frame's ID's stack address), and this frame's base pointer.  */
  if (info->uses_frame)
    {
      /* The SP was moved to the FP.  This indicates that a new frame
         was created.  Get THIS frame's FP value by unwinding it from
         the next frame.  */
      this_base = frame_unwind_register_unsigned (next_frame, M32R_FP_REGNUM);
      /* The FP points at the last saved register.  Adjust the FP back
         to before the first saved register giving the SP.  */
      prev_sp = this_base + info->size;
    }
  else
    {
      /* Assume that the FP is this frame's SP but with that pushed
         stack space added back.  */
      this_base = frame_unwind_register_unsigned (next_frame, M32R_SP_REGNUM);
      prev_sp = this_base + info->size;
    }

  /* Convert that SP/BASE into real addresses.  */
  info->prev_sp = prev_sp;
  info->base = this_base;

  /* Adjust all the saved registers so that they contain addresses and
     not offsets.  */
  for (i = 0; i < NUM_REGS - 1; i++)
    if (trad_frame_addr_p (info->saved_regs, i))
      info->saved_regs[i].addr = (info->prev_sp + info->saved_regs[i].addr);

  /* The call instruction moves the caller's PC in the callee's LR.
     Since this is an unwind, do the reverse.  Copy the location of LR
     into PC (the address / regnum) so that a request for PC will be
     converted into a request for the LR.  */
  info->saved_regs[M32R_PC_REGNUM] = info->saved_regs[LR_REGNUM];

  /* The previous frame's SP needed to be computed.  Save the computed
     value.  */
  trad_frame_set_value (info->saved_regs, M32R_SP_REGNUM, prev_sp);

  return info;
}

static CORE_ADDR
m32r_read_pc (ptid_t ptid)
{
  ptid_t save_ptid;
  ULONGEST pc;

  save_ptid = inferior_ptid;
  inferior_ptid = ptid;
  regcache_cooked_read_unsigned (current_regcache, M32R_PC_REGNUM, &pc);
  inferior_ptid = save_ptid;
  return pc;
}

static void
m32r_write_pc (CORE_ADDR val, ptid_t ptid)
{
  ptid_t save_ptid;

  save_ptid = inferior_ptid;
  inferior_ptid = ptid;
  write_register (M32R_PC_REGNUM, val);
  inferior_ptid = save_ptid;
}

static CORE_ADDR
m32r_unwind_sp (struct gdbarch *gdbarch, struct frame_info *next_frame)
{
  return frame_unwind_register_unsigned (next_frame, M32R_SP_REGNUM);
}


static CORE_ADDR
m32r_push_dummy_call (struct gdbarch *gdbarch, CORE_ADDR func_addr,
		      struct regcache *regcache, CORE_ADDR bp_addr, int nargs,
		      struct value **args, CORE_ADDR sp, int struct_return,
		      CORE_ADDR struct_addr)
{
  int stack_offset, stack_alloc;
  int argreg = ARG1_REGNUM;
  int argnum;
  struct type *type;
  enum type_code typecode;
  CORE_ADDR regval;
  char *val;
  char valbuf[MAX_REGISTER_SIZE];
  int len;
  int odd_sized_struct;

  /* first force sp to a 4-byte alignment */
  sp = sp & ~3;

  /* Set the return address.  For the m32r, the return breakpoint is
     always at BP_ADDR.  */
  regcache_cooked_write_unsigned (regcache, LR_REGNUM, bp_addr);

  /* If STRUCT_RETURN is true, then the struct return address (in
     STRUCT_ADDR) will consume the first argument-passing register.
     Both adjust the register count and store that value.  */
  if (struct_return)
    {
      regcache_cooked_write_unsigned (regcache, argreg, struct_addr);
      argreg++;
    }

  /* Now make sure there's space on the stack */
  for (argnum = 0, stack_alloc = 0; argnum < nargs; argnum++)
    stack_alloc += ((TYPE_LENGTH (VALUE_TYPE (args[argnum])) + 3) & ~3);
  sp -= stack_alloc;		/* make room on stack for args */

  for (argnum = 0, stack_offset = 0; argnum < nargs; argnum++)
    {
      type = VALUE_TYPE (args[argnum]);
      typecode = TYPE_CODE (type);
      len = TYPE_LENGTH (type);

      memset (valbuf, 0, sizeof (valbuf));

      /* Passes structures that do not fit in 2 registers by reference.  */
      if (len > 8
	  && (typecode == TYPE_CODE_STRUCT || typecode == TYPE_CODE_UNION))
	{
	  store_unsigned_integer (valbuf, 4, VALUE_ADDRESS (args[argnum]));
	  typecode = TYPE_CODE_PTR;
	  len = 4;
	  val = valbuf;
	}
      else if (len < 4)
	{
	  /* value gets right-justified in the register or stack word */
	  memcpy (valbuf + (register_size (gdbarch, argreg) - len),
		  (char *) VALUE_CONTENTS (args[argnum]), len);
	  val = valbuf;
	}
      else
	val = (char *) VALUE_CONTENTS (args[argnum]);

      while (len > 0)
	{
	  if (argreg > ARGN_REGNUM)
	    {
	      /* must go on the stack */
	      write_memory (sp + stack_offset, val, 4);
	      stack_offset += 4;
	    }
	  else if (argreg <= ARGN_REGNUM)
	    {
	      /* there's room in a register */
	      regval =
		extract_unsigned_integer (val,
					  register_size (gdbarch, argreg));
	      regcache_cooked_write_unsigned (regcache, argreg++, regval);
	    }

	  /* Store the value 4 bytes at a time.  This means that things
	     larger than 4 bytes may go partly in registers and partly
	     on the stack.  */
	  len -= register_size (gdbarch, argreg);
	  val += register_size (gdbarch, argreg);
	}
    }

  /* Finally, update the SP register.  */
  regcache_cooked_write_unsigned (regcache, M32R_SP_REGNUM, sp);

  return sp;
}


/* Given a return value in `regbuf' with a type `valtype', 
   extract and copy its value into `valbuf'.  */

static void
m32r_extract_return_value (struct type *type, struct regcache *regcache,
			   void *dst)
{
  bfd_byte *valbuf = dst;
  int len = TYPE_LENGTH (type);
  ULONGEST tmp;

  /* By using store_unsigned_integer we avoid having to do
     anything special for small big-endian values.  */
  regcache_cooked_read_unsigned (regcache, RET1_REGNUM, &tmp);
  store_unsigned_integer (valbuf, (len > 4 ? len - 4 : len), tmp);

  /* Ignore return values more than 8 bytes in size because the m32r
     returns anything more than 8 bytes in the stack. */
  if (len > 4)
    {
      regcache_cooked_read_unsigned (regcache, RET1_REGNUM + 1, &tmp);
      store_unsigned_integer (valbuf + len - 4, 4, tmp);
    }
}


static CORE_ADDR
m32r_unwind_pc (struct gdbarch *gdbarch, struct frame_info *next_frame)
{
  return frame_unwind_register_unsigned (next_frame, M32R_PC_REGNUM);
}

/* Given a GDB frame, determine the address of the calling function's
   frame.  This will be used to create a new GDB frame struct.  */

static void
m32r_frame_this_id (struct frame_info *next_frame,
		    void **this_prologue_cache, struct frame_id *this_id)
{
  struct m32r_unwind_cache *info
    = m32r_frame_unwind_cache (next_frame, this_prologue_cache);
  CORE_ADDR base;
  CORE_ADDR func;
  struct minimal_symbol *msym_stack;
  struct frame_id id;

  /* The FUNC is easy.  */
  func = frame_func_unwind (next_frame);

  /* Check if the stack is empty.  */
  msym_stack = lookup_minimal_symbol ("_stack", NULL, NULL);
  if (msym_stack && info->base == SYMBOL_VALUE_ADDRESS (msym_stack))
    return;

  /* Hopefully the prologue analysis either correctly determined the
     frame's base (which is the SP from the previous frame), or set
     that base to "NULL".  */
  base = info->prev_sp;
  if (base == 0)
    return;

  id = frame_id_build (base, func);

  /* Check that we're not going round in circles with the same frame
     ID (but avoid applying the test to sentinel frames which do go
     round in circles).  Can't use frame_id_eq() as that doesn't yet
     compare the frame's PC value.  */
  if (frame_relative_level (next_frame) >= 0
      && get_frame_type (next_frame) != DUMMY_FRAME
      && frame_id_eq (get_frame_id (next_frame), id))
    return;

  (*this_id) = id;
}

static void
m32r_frame_prev_register (struct frame_info *next_frame,
			  void **this_prologue_cache,
			  int regnum, int *optimizedp,
			  enum lval_type *lvalp, CORE_ADDR *addrp,
			  int *realnump, void *bufferp)
{
  struct m32r_unwind_cache *info
    = m32r_frame_unwind_cache (next_frame, this_prologue_cache);
  trad_frame_prev_register (next_frame, info->saved_regs, regnum,
			    optimizedp, lvalp, addrp, realnump, bufferp);
}

static const struct frame_unwind m32r_frame_unwind = {
  NORMAL_FRAME,
  m32r_frame_this_id,
  m32r_frame_prev_register
};

static const struct frame_unwind *
m32r_frame_sniffer (struct frame_info *next_frame)
{
  return &m32r_frame_unwind;
}

static CORE_ADDR
m32r_frame_base_address (struct frame_info *next_frame, void **this_cache)
{
  struct m32r_unwind_cache *info
    = m32r_frame_unwind_cache (next_frame, this_cache);
  return info->base;
}

static const struct frame_base m32r_frame_base = {
  &m32r_frame_unwind,
  m32r_frame_base_address,
  m32r_frame_base_address,
  m32r_frame_base_address
};

/* Assuming NEXT_FRAME->prev is a dummy, return the frame ID of that
   dummy frame.  The frame ID's base needs to match the TOS value
   saved by save_dummy_frame_tos(), and the PC match the dummy frame's
   breakpoint.  */

static struct frame_id
m32r_unwind_dummy_id (struct gdbarch *gdbarch, struct frame_info *next_frame)
{
  return frame_id_build (m32r_unwind_sp (gdbarch, next_frame),
			 frame_pc_unwind (next_frame));
}


static gdbarch_init_ftype m32r_gdbarch_init;

static struct gdbarch *
m32r_gdbarch_init (struct gdbarch_info info, struct gdbarch_list *arches)
{
  struct gdbarch *gdbarch;
  struct gdbarch_tdep *tdep;

  /* If there is already a candidate, use it.  */
  arches = gdbarch_list_lookup_by_info (arches, &info);
  if (arches != NULL)
    return arches->gdbarch;

  /* Allocate space for the new architecture.  */
  tdep = XMALLOC (struct gdbarch_tdep);
  gdbarch = gdbarch_alloc (&info, tdep);

  set_gdbarch_read_pc (gdbarch, m32r_read_pc);
  set_gdbarch_write_pc (gdbarch, m32r_write_pc);
  set_gdbarch_unwind_sp (gdbarch, m32r_unwind_sp);

  set_gdbarch_num_regs (gdbarch, m32r_num_regs ());
  set_gdbarch_sp_regnum (gdbarch, M32R_SP_REGNUM);
  set_gdbarch_register_name (gdbarch, m32r_register_name);
  set_gdbarch_register_type (gdbarch, m32r_register_type);

  set_gdbarch_extract_return_value (gdbarch, m32r_extract_return_value);
  set_gdbarch_push_dummy_call (gdbarch, m32r_push_dummy_call);
  set_gdbarch_store_return_value (gdbarch, m32r_store_return_value);
  set_gdbarch_deprecated_extract_struct_value_address (gdbarch, m32r_extract_struct_value_address);
  set_gdbarch_use_struct_convention (gdbarch, m32r_use_struct_convention);

  set_gdbarch_skip_prologue (gdbarch, m32r_skip_prologue);
  set_gdbarch_inner_than (gdbarch, core_addr_lessthan);
  set_gdbarch_breakpoint_from_pc (gdbarch, m32r_breakpoint_from_pc);
  set_gdbarch_memory_insert_breakpoint (gdbarch,
					m32r_memory_insert_breakpoint);
  set_gdbarch_memory_remove_breakpoint (gdbarch,
					m32r_memory_remove_breakpoint);

  set_gdbarch_deprecated_frameless_function_invocation (gdbarch, legacy_frameless_look_for_prologue);

  set_gdbarch_frame_align (gdbarch, m32r_frame_align);

  frame_unwind_append_sniffer (gdbarch, m32r_frame_sniffer);
  frame_base_set_default (gdbarch, &m32r_frame_base);

  /* Methods for saving / extracting a dummy frame's ID.  The ID's
     stack address must match the SP value returned by
     PUSH_DUMMY_CALL, and saved by generic_save_dummy_frame_tos.  */
  set_gdbarch_unwind_dummy_id (gdbarch, m32r_unwind_dummy_id);

  /* Return the unwound PC value.  */
  set_gdbarch_unwind_pc (gdbarch, m32r_unwind_pc);

  set_gdbarch_print_insn (gdbarch, print_insn_m32r);

  return gdbarch;
}

void
_initialize_m32r_tdep (void)
{
  register_gdbarch_init (bfd_arch_m32r, m32r_gdbarch_init);
}
