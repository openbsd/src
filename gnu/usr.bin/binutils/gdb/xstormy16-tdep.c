/* Target-dependent code for the Sanyo Xstormy16a (LC590000) processor.

   Copyright 2001, 2002, 2003, 2004 Free Software Foundation, Inc.

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
#include "value.h"
#include "inferior.h"
#include "arch-utils.h"
#include "regcache.h"
#include "gdbcore.h"
#include "objfiles.h"
#include "dis-asm.h"

struct gdbarch_tdep
{
  /* gdbarch target dependent data here. Currently unused for Xstormy16. */
};

/* Extra info which is saved in each frame_info. */
struct frame_extra_info
{
  int framesize;
  int frameless_p;
};

enum gdb_regnum
{
  /* Xstormy16 has 16 general purpose registers (R0-R15) plus PC.
     Functions will return their values in register R2-R7 as they fit.
     Otherwise a hidden pointer to an big enough area is given as argument
     to the function in r2. Further arguments are beginning in r3 then.
     R13 is used as frame pointer when GCC compiles w/o optimization
     R14 is used as "PSW", displaying the CPU status.
     R15 is used implicitely as stack pointer. */
  E_R0_REGNUM,
  E_R1_REGNUM,
  E_R2_REGNUM, E_1ST_ARG_REGNUM = E_R2_REGNUM, E_PTR_RET_REGNUM = E_R2_REGNUM,
  E_R3_REGNUM,
  E_R4_REGNUM,
  E_R5_REGNUM,
  E_R6_REGNUM,
  E_R7_REGNUM, E_LST_ARG_REGNUM = E_R7_REGNUM,
  E_R8_REGNUM,
  E_R9_REGNUM,
  E_R10_REGNUM,
  E_R11_REGNUM,
  E_R12_REGNUM,
  E_R13_REGNUM, E_FP_REGNUM = E_R13_REGNUM,
  E_R14_REGNUM, E_PSW_REGNUM = E_R14_REGNUM,
  E_R15_REGNUM, E_SP_REGNUM = E_R15_REGNUM,
  E_PC_REGNUM,
  E_NUM_REGS
};

/* Size of instructions, registers, etc. */
enum
{
  xstormy16_inst_size = 2,
  xstormy16_reg_size = 2,
  xstormy16_pc_size = 4
};

/* Size of return datatype which fits into the remaining return registers. */
#define E_MAX_RETTYPE_SIZE(regnum)	((E_LST_ARG_REGNUM - (regnum) + 1) \
					* xstormy16_reg_size)

/* Size of return datatype which fits into all return registers. */
enum
{
  E_MAX_RETTYPE_SIZE_IN_REGS = E_MAX_RETTYPE_SIZE (E_R2_REGNUM)
};


/* Size of all registers as a whole. */
enum
{
  E_ALL_REGS_SIZE = (E_NUM_REGS - 1) * xstormy16_reg_size + xstormy16_pc_size
};

/* Function: xstormy16_register_name
   Returns the name of the standard Xstormy16 register N. */

static const char *
xstormy16_register_name (int regnum)
{
  static char *register_names[] = {
    "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7",
    "r8", "r9", "r10", "r11", "r12", "r13",
    "psw", "sp", "pc"
  };

  if (regnum < 0 ||
      regnum >= sizeof (register_names) / sizeof (register_names[0]))
    internal_error (__FILE__, __LINE__,
		    "xstormy16_register_name: illegal register number %d",
		    regnum);
  else
    return register_names[regnum];

}

/* Function: xstormy16_register_byte 
   Returns the byte position in the register cache for register N. */

static int
xstormy16_register_byte (int regnum)
{
  if (regnum < 0 || regnum >= E_NUM_REGS)
    internal_error (__FILE__, __LINE__,
		    "xstormy16_register_byte: illegal register number %d",
		    regnum);
  else
    /* All registers occupy 2 bytes in the regcache except for PC
       which is the last one. Therefore the byte position is still
       simply a multiple of 2. */
    return regnum * xstormy16_reg_size;
}

/* Function: xstormy16_register_raw_size
   Returns the number of bytes occupied by the register on the target. */

static int
xstormy16_register_raw_size (int regnum)
{
  if (regnum < 0 || regnum >= E_NUM_REGS)
    internal_error (__FILE__, __LINE__,
		    "xstormy16_register_raw_size: illegal register number %d",
		    regnum);
  /* Only the PC has 4 Byte, all other registers 2 Byte. */
  else if (regnum == E_PC_REGNUM)
    return xstormy16_pc_size;
  else
    return xstormy16_reg_size;
}

/* Function: xstormy16_reg_virtual_type 
   Returns the default type for register N. */

static struct type *
xstormy16_reg_virtual_type (int regnum)
{
  if (regnum < 0 || regnum >= E_NUM_REGS)
    internal_error (__FILE__, __LINE__,
		    "xstormy16_register_virtual_type: illegal register number %d",
		    regnum);
  else if (regnum == E_PC_REGNUM)
    return builtin_type_uint32;
  else
    return builtin_type_uint16;
}

/* Function: xstormy16_get_saved_register
   Find a register's saved value on the call stack. */

static void
xstormy16_get_saved_register (char *raw_buffer,
			      int *optimized,
			      CORE_ADDR *addrp,
			      struct frame_info *fi,
			      int regnum, enum lval_type *lval)
{
  deprecated_generic_get_saved_register (raw_buffer, optimized, addrp, fi, regnum, lval);
}

/* Function: xstormy16_type_is_scalar
   Makes the decision if a given type is a scalar types.  Scalar
   types are returned in the registers r2-r7 as they fit. */

static int
xstormy16_type_is_scalar (struct type *t)
{
  return (TYPE_CODE(t) != TYPE_CODE_STRUCT
	  && TYPE_CODE(t) != TYPE_CODE_UNION
	  && TYPE_CODE(t) != TYPE_CODE_ARRAY);
}

/* Function: xstormy16_extract_return_value
   Copy the function's return value into VALBUF. 
   This function is called only in the context of "target function calls",
   ie. when the debugger forces a function to be called in the child, and
   when the debugger forces a function to return prematurely via the
   "return" command. */

static void
xstormy16_extract_return_value (struct type *type, char *regbuf, char *valbuf)
{
  CORE_ADDR return_buffer;
  int offset = 0;

  if (xstormy16_type_is_scalar (type)
      && TYPE_LENGTH (type) <= E_MAX_RETTYPE_SIZE_IN_REGS)
    {
      /* Scalar return values of <= 12 bytes are returned in 
         E_1ST_ARG_REGNUM to E_LST_ARG_REGNUM. */
      memcpy (valbuf,
	      &regbuf[DEPRECATED_REGISTER_BYTE (E_1ST_ARG_REGNUM)] + offset,
	      TYPE_LENGTH (type));
    }
  else
    {
      /* Aggregates and return values > 12 bytes are returned in memory,
         pointed to by R2. */
      return_buffer =
	extract_unsigned_integer (regbuf + DEPRECATED_REGISTER_BYTE (E_PTR_RET_REGNUM),
				  DEPRECATED_REGISTER_RAW_SIZE (E_PTR_RET_REGNUM));

      read_memory (return_buffer, valbuf, TYPE_LENGTH (type));
    }
}

/* Function: xstormy16_push_arguments
   Setup the function arguments for GDB to call a function in the inferior.
   Called only in the context of a target function call from the debugger.
   Returns the value of the SP register after the args are pushed.
*/

static CORE_ADDR
xstormy16_push_arguments (int nargs, struct value **args, CORE_ADDR sp,
			  int struct_return, CORE_ADDR struct_addr)
{
  CORE_ADDR stack_dest = sp;
  int argreg = E_1ST_ARG_REGNUM;
  int i, j;
  int typelen, slacklen;
  char *val;

  /* If struct_return is true, then the struct return address will
     consume one argument-passing register.  */
  if (struct_return)
    argreg++;

  /* Arguments are passed in R2-R7 as they fit. If an argument doesn't
     fit in the remaining registers we're switching over to the stack.
     No argument is put on stack partially and as soon as we switched
     over to stack no further argument is put in a register even if it
     would fit in the remaining unused registers. */
  for (i = 0; i < nargs && argreg <= E_LST_ARG_REGNUM; i++)
    {
      typelen = TYPE_LENGTH (VALUE_ENCLOSING_TYPE (args[i]));
      if (typelen > E_MAX_RETTYPE_SIZE (argreg))
	break;

      /* Put argument into registers wordwise. */
      val = VALUE_CONTENTS (args[i]);
      for (j = 0; j < typelen; j += xstormy16_reg_size)
	write_register (argreg++,
			extract_unsigned_integer (val + j,
						  typelen - j ==
						  1 ? 1 :
						  xstormy16_reg_size));
    }

  /* Align SP */
  if (stack_dest & 1)
    ++stack_dest;

  /* Loop backwards through remaining arguments and push them on the stack,
     wordaligned. */
  for (j = nargs - 1; j >= i; j--)
    {
      typelen = TYPE_LENGTH (VALUE_ENCLOSING_TYPE (args[j]));
      slacklen = typelen & 1;
      val = alloca (typelen + slacklen);
      memcpy (val, VALUE_CONTENTS (args[j]), typelen);
      memset (val + typelen, 0, slacklen);

      /* Now write this data to the stack. The stack grows upwards. */
      write_memory (stack_dest, val, typelen + slacklen);
      stack_dest += typelen + slacklen;
    }

  /* And that should do it.  Return the new stack pointer. */
  return stack_dest;
}

/* Function: xstormy16_push_return_address (pc)
   Setup the return address for GDB to call a function in the inferior.
   Called only in the context of a target function call from the debugger.
   Returns the value of the SP register when the operation is finished
   (which may or may not be the same as before).
*/

static CORE_ADDR
xstormy16_push_return_address (CORE_ADDR pc, CORE_ADDR sp)
{
  unsigned char buf[xstormy16_pc_size];

  store_unsigned_integer (buf, xstormy16_pc_size, entry_point_address ());
  write_memory (sp, buf, xstormy16_pc_size);
  return sp + xstormy16_pc_size;
}

/* Function: xstormy16_pop_frame
   Destroy the innermost (Top-Of-Stack) stack frame, restoring the 
   machine state that was in effect before the frame was created. 
   Used in the contexts of the "return" command, and of 
   target function calls from the debugger.
*/

static void
xstormy16_pop_frame (void)
{
  struct frame_info *fi = get_current_frame ();
  int i;

  if (fi == NULL)
    return;			/* paranoia */

  if (DEPRECATED_PC_IN_CALL_DUMMY (get_frame_pc (fi), get_frame_base (fi),
				   get_frame_base (fi)))
    {
      generic_pop_dummy_frame ();
    }
  else
    {
      /* Restore the saved regs. */
      for (i = 0; i < NUM_REGS; i++)
	if (deprecated_get_frame_saved_regs (fi)[i])
	  {
	    if (i == SP_REGNUM)
	      write_register (i, deprecated_get_frame_saved_regs (fi)[i]);
	    else if (i == E_PC_REGNUM)
	      write_register (i, read_memory_integer (deprecated_get_frame_saved_regs (fi)[i],
						      xstormy16_pc_size));
	    else
	      write_register (i, read_memory_integer (deprecated_get_frame_saved_regs (fi)[i],
						      xstormy16_reg_size));
	  }
      /* Restore the PC */
      write_register (PC_REGNUM, DEPRECATED_FRAME_SAVED_PC (fi));
      flush_cached_frames ();
    }
  return;
}

/* Function: xstormy16_store_struct_return
   Copy the (struct) function return value to its destined location. 
   Called only in the context of a target function call from the debugger.
*/

static void
xstormy16_store_struct_return (CORE_ADDR addr, CORE_ADDR sp)
{
  write_register (E_PTR_RET_REGNUM, addr);
}

/* Function: xstormy16_store_return_value
   Copy the function return value from VALBUF into the 
   proper location for a function return. 
   Called only in the context of the "return" command.
*/

static void
xstormy16_store_return_value (struct type *type, char *valbuf)
{
  CORE_ADDR return_buffer;
  char buf[xstormy16_reg_size];

  if (xstormy16_type_is_scalar (type) && TYPE_LENGTH (type) == 1)
    {
      /* Add leading zeros to the value. */
      memset (buf, 0, xstormy16_reg_size);
      memcpy (buf, valbuf, 1);
      deprecated_write_register_gen (E_1ST_ARG_REGNUM, buf);
    }
  else if (xstormy16_type_is_scalar (type) &&
	   TYPE_LENGTH (type) <= E_MAX_RETTYPE_SIZE_IN_REGS)
    deprecated_write_register_bytes (DEPRECATED_REGISTER_BYTE (E_1ST_ARG_REGNUM),
				     valbuf, TYPE_LENGTH (type));
  else
    {
      return_buffer = read_register (E_PTR_RET_REGNUM);
      write_memory (return_buffer, valbuf, TYPE_LENGTH (type));
    }
}

/* Function: xstormy16_extract_struct_value_address
   Returns the address in which a function should return a struct value. 
   Used in the contexts of the "return" command, and of 
   target function calls from the debugger.
*/

static CORE_ADDR
xstormy16_extract_struct_value_address (struct regcache *regcache)
{
  /* FIXME: cagney/2004-01-17: Does the ABI guarantee that the return
     address regster is preserved across function calls?  Probably
     not, making this function wrong.  */
  ULONGEST val;
  regcache_raw_read_unsigned (regcache, E_PTR_RET_REGNUM, &val);
  return val;
}

/* Function: xstormy16_use_struct_convention 
   Returns non-zero if the given struct type will be returned using
   a special convention, rather than the normal function return method. 
   7sed in the contexts of the "return" command, and of 
   target function calls from the debugger.
*/

static int
xstormy16_use_struct_convention (int gcc_p, struct type *type)
{
  return !xstormy16_type_is_scalar (type)
    || TYPE_LENGTH (type) > E_MAX_RETTYPE_SIZE_IN_REGS;
}

/* Function: frame_saved_register
   Returns the value that regnum had in frame fi
   (saved in fi or in one of its children).  
*/

static CORE_ADDR
xstormy16_frame_saved_register (struct frame_info *fi, int regnum)
{
  int size = xstormy16_register_raw_size (regnum);
  char *buf = (char *) alloca (size);

  deprecated_generic_get_saved_register (buf, NULL, NULL, fi, regnum, NULL);
  return (CORE_ADDR) extract_unsigned_integer (buf, size);
}

/* Function: xstormy16_scan_prologue
   Decode the instructions within the given address range.
   Decide when we must have reached the end of the function prologue.
   If a frame_info pointer is provided, fill in its saved_regs etc.

   Returns the address of the first instruction after the prologue. 
*/

static CORE_ADDR
xstormy16_scan_prologue (CORE_ADDR start_addr, CORE_ADDR end_addr,
			 struct frame_info *fi, int *frameless)
{
  CORE_ADDR sp = 0, fp = 0;
  CORE_ADDR next_addr;
  ULONGEST inst, inst2;
  LONGEST offset;
  int regnum;

  if (frameless)
    *frameless = 1;
  if (fi)
    {
      /* In a call dummy, don't touch the frame. */
      if (DEPRECATED_PC_IN_CALL_DUMMY (get_frame_pc (fi), get_frame_base (fi),
				       get_frame_base (fi)))
	return start_addr;

      /* Grab the frame-relative values of SP and FP, needed below. 
         The frame_saved_register function will find them on the
         stack or in the registers as appropriate. */
      sp = xstormy16_frame_saved_register (fi, E_SP_REGNUM);
      fp = xstormy16_frame_saved_register (fi, E_FP_REGNUM);

      /* Initialize framesize with size of PC put on stack by CALLF inst. */
      get_frame_extra_info (fi)->framesize = xstormy16_pc_size;
    }
  for (next_addr = start_addr;
       next_addr < end_addr; next_addr += xstormy16_inst_size)
    {
      inst = read_memory_unsigned_integer (next_addr, xstormy16_inst_size);
      inst2 = read_memory_unsigned_integer (next_addr + xstormy16_inst_size,
					    xstormy16_inst_size);

      if (inst >= 0x0082 && inst <= 0x008d)	/* push r2 .. push r13 */
	{
	  if (fi)
	    {
	      regnum = inst & 0x000f;
	      deprecated_get_frame_saved_regs (fi)[regnum] = get_frame_extra_info (fi)->framesize;
	      get_frame_extra_info (fi)->framesize += xstormy16_reg_size;
	    }
	}

      /* optional stack allocation for args and local vars <= 4 byte */
      else if (inst == 0x301f || inst == 0x303f)	/* inc r15, #0x1/#0x3 */
	{
	  if (fi)		/* Record the frame size. */
	    get_frame_extra_info (fi)->framesize += ((inst & 0x0030) >> 4) + 1;
	}

      /* optional stack allocation for args and local vars > 4 && < 16 byte */
      else if ((inst & 0xff0f) == 0x510f)	/* 51Hf   add r15, #0xH */
	{
	  if (fi)		/* Record the frame size. */
	    get_frame_extra_info (fi)->framesize += (inst & 0x00f0) >> 4;
	}

      /* optional stack allocation for args and local vars >= 16 byte */
      else if (inst == 0x314f && inst2 >= 0x0010)	/* 314f HHHH  add r15, #0xH */
	{
	  if (fi)		/* Record the frame size. */
	    get_frame_extra_info (fi)->framesize += inst2;
	  next_addr += xstormy16_inst_size;
	}

      else if (inst == 0x46fd)	/* mov r13, r15 */
	{
	  if (fi)		/* Record that the frame pointer is in use. */
	    get_frame_extra_info (fi)->frameless_p = 0;
	  if (frameless)
	    *frameless = 0;
	}

      /* optional copying of args in r2-r7 to r10-r13 */
      /* Probably only in optimized case but legal action for prologue */
      else if ((inst & 0xff00) == 0x4600	/* 46SD   mov rD, rS */
	       && (inst & 0x00f0) >= 0x0020 && (inst & 0x00f0) <= 0x0070
	       && (inst & 0x000f) >= 0x00a0 && (inst & 0x000f) <= 0x000d)
	;

      /* optional copying of args in r2-r7 to stack */
      /* 72DS HHHH   mov.b (rD, 0xHHHH), r(S-8) (bit3 always 1, bit2-0 = reg) */
      /* 73DS HHHH   mov.w (rD, 0xHHHH), r(S-8) */
      else if ((inst & 0xfed8) == 0x72d8 && (inst & 0x0007) >= 2)
	{
	  if (fi)
	    {
	      regnum = inst & 0x0007;
	      /* Only 12 of 16 bits of the argument are used for the
	         signed offset. */
	      offset = (LONGEST) (inst2 & 0x0fff);
	      if (offset & 0x0800)
		offset -= 0x1000;

	      deprecated_get_frame_saved_regs (fi)[regnum] = get_frame_extra_info (fi)->framesize + offset;
	    }
	  next_addr += xstormy16_inst_size;
	}

#if 0
      /* 2001-08-10: Not part of the prologue anymore due to change in
         ABI. r8 and r9 are not used for argument passing anymore. */

      /* optional copying of r8, r9 to stack */
      /* 46S7; 73Df HHHH   mov.w r7,rS; mov.w (rD, 0xHHHH), r7 D=8,9; S=13,15 */
      /* 46S7; 72df HHHH   mov.w r7,rS; mov.b (rD, 0xHHHH), r7 D=8,9; S=13,15 */
      else if ((inst & 0xffef) == 0x4687 && (inst2 & 0xfedf) == 0x72df)
	{
	  next_addr += xstormy16_inst_size;
	  if (fi)
	    {
	      regnum = (inst & 0x00f0) >> 4;
	      inst = inst2;
	      inst2 = read_memory_unsigned_integer (next_addr
						    + xstormy16_inst_size,
						    xstormy16_inst_size);
	      /* Only 12 of 16 bits of the argument are used for the
	         signed offset. */
	      offset = (LONGEST) (inst2 & 0x0fff);
	      if (offset & 0x0800)
		offset -= 0x1000;

	      fi->saved_regs[regnum] = fi->extra_info->framesize + offset;
	    }
	  next_addr += xstormy16_inst_size;
	}
#endif

      else			/* Not a prologue instruction. */
	break;
    }

  if (fi)
    {
      /* Special handling for the "saved" address of the SP:
         The SP is of course never saved on the stack at all, so
         by convention what we put here is simply the previous 
         _value_ of the SP (as opposed to an address where the
         previous value would have been pushed).  */
      if (get_frame_extra_info (fi)->frameless_p)
	{
	  deprecated_get_frame_saved_regs (fi)[E_SP_REGNUM] = sp - get_frame_extra_info (fi)->framesize;
	  deprecated_update_frame_base_hack (fi, sp);
	}
      else
	{
	  deprecated_get_frame_saved_regs (fi)[E_SP_REGNUM] = fp - get_frame_extra_info (fi)->framesize;
	  deprecated_update_frame_base_hack (fi, fp);
	}

      /* So far only offsets to the beginning of the frame are
         saved in the saved_regs. Now we now the relation between
         sp, fp and framesize. We know the beginning of the frame
         so we can translate the register offsets to real addresses. */
      for (regnum = 0; regnum < E_SP_REGNUM; ++regnum)
	if (deprecated_get_frame_saved_regs (fi)[regnum])
	  deprecated_get_frame_saved_regs (fi)[regnum] += deprecated_get_frame_saved_regs (fi)[E_SP_REGNUM];

      /* Save address of PC on stack. */
      deprecated_get_frame_saved_regs (fi)[E_PC_REGNUM] = deprecated_get_frame_saved_regs (fi)[E_SP_REGNUM];
    }

  return next_addr;
}

/* Function: xstormy16_skip_prologue
   If the input address is in a function prologue, 
   returns the address of the end of the prologue;
   else returns the input address.

   Note: the input address is likely to be the function start, 
   since this function is mainly used for advancing a breakpoint
   to the first line, or stepping to the first line when we have
   stepped into a function call.  */

static CORE_ADDR
xstormy16_skip_prologue (CORE_ADDR pc)
{
  CORE_ADDR func_addr = 0, func_end = 0;
  char *func_name;

  if (find_pc_partial_function (pc, &func_name, &func_addr, &func_end))
    {
      struct symtab_and_line sal;
      struct symbol *sym;

      /* Don't trust line number debug info in frameless functions. */
      int frameless = 1;
      CORE_ADDR plg_end = xstormy16_scan_prologue (func_addr, func_end,
						   NULL, &frameless);
      if (frameless)
        return plg_end;

      /* Found a function.  */
      sym = lookup_symbol (func_name, NULL, VAR_DOMAIN, NULL, NULL);
      /* Don't use line number debug info for assembly source files. */
      if (sym && SYMBOL_LANGUAGE (sym) != language_asm)
	{
	  sal = find_pc_line (func_addr, 0);
	  if (sal.end && sal.end < func_end)
	    {
	      /* Found a line number, use it as end of prologue.  */
	      return sal.end;
	    }
	}
      /* No useable line symbol.  Use result of prologue parsing method. */
      return plg_end;
    }

  /* No function symbol -- just return the PC. */

  return (CORE_ADDR) pc;
}

/* The epilogue is defined here as the area at the end of a function,
   either on the `ret' instruction itself or after an instruction which
   destroys the function's stack frame. */
static int
xstormy16_in_function_epilogue_p (struct gdbarch *gdbarch, CORE_ADDR pc)
{
  CORE_ADDR addr, func_addr = 0, func_end = 0;

  if (find_pc_partial_function (pc, NULL, &func_addr, &func_end))
    {
      ULONGEST inst, inst2;
      CORE_ADDR addr = func_end - xstormy16_inst_size;

      /* The Xstormy16 epilogue is max. 14 bytes long. */
      if (pc < func_end - 7 * xstormy16_inst_size)
	return 0;

      /* Check if we're on a `ret' instruction.  Otherwise it's
         too dangerous to proceed. */
      inst = read_memory_unsigned_integer (addr, xstormy16_inst_size);
      if (inst != 0x0003)
	return 0;

      while ((addr -= xstormy16_inst_size) >= func_addr)
	{
	  inst = read_memory_unsigned_integer (addr, xstormy16_inst_size);
	  if (inst >= 0x009a && inst <= 0x009d)	/* pop r10...r13 */
	    continue;
	  if (inst == 0x305f || inst == 0x307f)	/* dec r15, #0x1/#0x3 */
	    break;
	  inst2 = read_memory_unsigned_integer (addr - xstormy16_inst_size,
						xstormy16_inst_size);
	  if (inst2 == 0x314f && inst >= 0x8000)	/* add r15, neg. value */
	    {
	      addr -= xstormy16_inst_size;
	      break;
	    }
	  return 0;
	}
      if (pc > addr)
	return 1;
    }
  return 0;
}

/* Function: xstormy16_frame_init_saved_regs
   Set up the 'saved_regs' array.
   This is a data structure containing the addresses on the stack 
   where each register has been saved, for each stack frame.  
   Registers that have not been saved will have zero here.
   The stack register is special: rather than the address where the 
   stack register has been saved, saved_regs[SP_REGNUM] will have the
   actual value of the previous frame's stack register. 

   This function may be called in any context where the saved register
   values may be needed (backtrace, frame_info, frame_register).  On
   many targets, it is called directly by init_extra_frame_info, in
   part because the information may be needed immediately by
   frame_chain.  */

static void
xstormy16_frame_init_saved_regs (struct frame_info *fi)
{
  CORE_ADDR func_addr, func_end;

  if (!deprecated_get_frame_saved_regs (fi))
    {
      frame_saved_regs_zalloc (fi);

      /* Find the beginning of this function, so we can analyze its
         prologue. */
      if (find_pc_partial_function (get_frame_pc (fi), NULL, &func_addr, &func_end))
	xstormy16_scan_prologue (func_addr, get_frame_pc (fi), fi, NULL);
      /* Else we're out of luck (can't debug completely stripped code). 
         FIXME. */
    }
}

/* Function: xstormy16_frame_saved_pc
   Returns the return address for the selected frame. 
   Called by frame_info, legacy_frame_chain_valid, and sometimes by
   get_prev_frame.  */

static CORE_ADDR
xstormy16_frame_saved_pc (struct frame_info *fi)
{
  CORE_ADDR saved_pc;

  if (DEPRECATED_PC_IN_CALL_DUMMY (get_frame_pc (fi), get_frame_base (fi),
				   get_frame_base (fi)))
    {
      saved_pc = deprecated_read_register_dummy (get_frame_pc (fi),
						 get_frame_base (fi),
						 E_PC_REGNUM);
    }
  else
    {
      saved_pc = read_memory_unsigned_integer (deprecated_get_frame_saved_regs (fi)[E_PC_REGNUM],
					       xstormy16_pc_size);
    }

  return saved_pc;
}

/* Function: xstormy16_init_extra_frame_info
   This is the constructor function for the frame_info struct, 
   called whenever a new frame_info is created (from create_new_frame,
   and from get_prev_frame).
*/

static void
xstormy16_init_extra_frame_info (int fromleaf, struct frame_info *fi)
{
  if (!get_frame_extra_info (fi))
    {
      frame_extra_info_zalloc (fi, sizeof (struct frame_extra_info));
      get_frame_extra_info (fi)->framesize = 0;
      get_frame_extra_info (fi)->frameless_p = 1;	/* Default frameless, detect framed */

      /* By default, the fi->frame is set to the value of the FP reg by gdb.
         This may not always be right; we may be in a frameless function,
         or we may be in the prologue, before the FP has been set up.
         Unfortunately, we can't make this determination without first
         calling scan_prologue, and we can't do that unles we know the
         get_frame_pc (fi).  */

      if (!get_frame_pc (fi))
	{
	  /* Sometimes we are called from get_prev_frame without
	     the PC being set up first.  Long history, don't ask.
	     Fortunately this will never happen from the outermost
	     frame, so we should be able to get the saved pc from
	     the next frame. */
	  if (get_next_frame (fi))
	    deprecated_update_frame_pc_hack (fi, xstormy16_frame_saved_pc (get_next_frame (fi)));
	}

      /* Take care of the saved_regs right here (non-lazy). */
      xstormy16_frame_init_saved_regs (fi);
    }
}

/* Function: xstormy16_frame_chain
   Returns a pointer to the stack frame of the calling function.
   Called only from get_prev_frame.  Needed for backtrace, "up", etc.
*/

static CORE_ADDR
xstormy16_frame_chain (struct frame_info *fi)
{
  if (DEPRECATED_PC_IN_CALL_DUMMY (get_frame_pc (fi), get_frame_base (fi),
				   get_frame_base (fi)))
    {
      /* Call dummy's frame is the same as caller's.  */
      return get_frame_base (fi);
    }
  else
    {
      /* Return computed offset from this frame's fp. */
      return get_frame_base (fi) - get_frame_extra_info (fi)->framesize;
    }
}

static int
xstormy16_frame_chain_valid (CORE_ADDR chain, struct frame_info *thisframe)
{
  return chain < 0x8000 && DEPRECATED_FRAME_SAVED_PC (thisframe) >= 0x8000 &&
    (get_frame_extra_info (thisframe)->frameless_p ||
     get_frame_base (thisframe) - get_frame_extra_info (thisframe)->framesize == chain);
}

/* Function: xstormy16_saved_pc_after_call Returns the previous PC
   immediately after a function call.  This function is meant to
   bypass the regular frame_register() mechanism, ie. it is meant to
   work even if the frame isn't complete.  Called by
   step_over_function, and sometimes by get_prev_frame.  */

static CORE_ADDR
xstormy16_saved_pc_after_call (struct frame_info *ignore)
{
  CORE_ADDR sp, pc, tmp;

  sp = read_register (E_SP_REGNUM) - xstormy16_pc_size;
  pc = read_memory_integer (sp, xstormy16_pc_size);

  /* Skip over jump table entry if necessary.  */
  if ((tmp = SKIP_TRAMPOLINE_CODE (pc)))
    pc = tmp;

  return pc;
}

const static unsigned char *
xstormy16_breakpoint_from_pc (CORE_ADDR *pcptr, int *lenptr)
{
  static unsigned char breakpoint[] = { 0x06, 0x0 };
  *lenptr = sizeof (breakpoint);
  return breakpoint;
}

/* Given a pointer to a jump table entry, return the address
   of the function it jumps to.  Return 0 if not found. */
static CORE_ADDR
xstormy16_resolve_jmp_table_entry (CORE_ADDR faddr)
{
  struct obj_section *faddr_sect = find_pc_section (faddr);

  if (faddr_sect)
    {
      LONGEST inst, inst2, addr;
      char buf[2 * xstormy16_inst_size];

      /* Return faddr if it's not pointing into the jump table. */
      if (strcmp (faddr_sect->the_bfd_section->name, ".plt"))
	return faddr;

      if (!target_read_memory (faddr, buf, sizeof buf))
	{
	  inst = extract_unsigned_integer (buf, xstormy16_inst_size);
	  inst2 = extract_unsigned_integer (buf + xstormy16_inst_size,
					    xstormy16_inst_size);
	  addr = inst2 << 8 | (inst & 0xff);
	  return addr;
	}
    }
  return 0;
}

/* Given a function's address, attempt to find (and return) the
   address of the corresponding jump table entry.  Return 0 if
   not found. */
static CORE_ADDR
xstormy16_find_jmp_table_entry (CORE_ADDR faddr)
{
  struct obj_section *faddr_sect = find_pc_section (faddr);

  if (faddr_sect)
    {
      struct obj_section *osect;

      /* Return faddr if it's already a pointer to a jump table entry. */
      if (!strcmp (faddr_sect->the_bfd_section->name, ".plt"))
	return faddr;

      ALL_OBJFILE_OSECTIONS (faddr_sect->objfile, osect)
      {
	if (!strcmp (osect->the_bfd_section->name, ".plt"))
	  break;
      }

      if (osect < faddr_sect->objfile->sections_end)
	{
	  CORE_ADDR addr;
	  for (addr = osect->addr;
	       addr < osect->endaddr; addr += 2 * xstormy16_inst_size)
	    {
	      int status;
	      LONGEST inst, inst2, faddr2;
	      char buf[2 * xstormy16_inst_size];

	      if (target_read_memory (addr, buf, sizeof buf))
		return 0;
	      inst = extract_unsigned_integer (buf, xstormy16_inst_size);
	      inst2 = extract_unsigned_integer (buf + xstormy16_inst_size,
						xstormy16_inst_size);
	      faddr2 = inst2 << 8 | (inst & 0xff);
	      if (faddr == faddr2)
		return addr;
	    }
	}
    }
  return 0;
}

static CORE_ADDR
xstormy16_skip_trampoline_code (CORE_ADDR pc)
{
  int tmp = xstormy16_resolve_jmp_table_entry (pc);

  if (tmp && tmp != pc)
    return tmp;
  return 0;
}

static int
xstormy16_in_solib_call_trampoline (CORE_ADDR pc, char *name)
{
  return xstormy16_skip_trampoline_code (pc) != 0;
}

static CORE_ADDR
xstormy16_pointer_to_address (struct type *type, const void *buf)
{
  enum type_code target = TYPE_CODE (TYPE_TARGET_TYPE (type));
  CORE_ADDR addr = extract_unsigned_integer (buf, TYPE_LENGTH (type));

  if (target == TYPE_CODE_FUNC || target == TYPE_CODE_METHOD)
    {
      CORE_ADDR addr2 = xstormy16_resolve_jmp_table_entry (addr);
      if (addr2)
	addr = addr2;
    }

  return addr;
}

static void
xstormy16_address_to_pointer (struct type *type, void *buf, CORE_ADDR addr)
{
  enum type_code target = TYPE_CODE (TYPE_TARGET_TYPE (type));

  if (target == TYPE_CODE_FUNC || target == TYPE_CODE_METHOD)
    {
      CORE_ADDR addr2 = xstormy16_find_jmp_table_entry (addr);
      if (addr2)
	addr = addr2;
    }
  store_unsigned_integer (buf, TYPE_LENGTH (type), addr);
}

static CORE_ADDR
xstormy16_stack_align (CORE_ADDR addr)
{
  if (addr & 1)
    ++addr;
  return addr;
}

static void
xstormy16_save_dummy_frame_tos (CORE_ADDR sp)
{
  generic_save_dummy_frame_tos (sp - xstormy16_pc_size);
}

/* Function: xstormy16_gdbarch_init
   Initializer function for the xstormy16 gdbarch vector.
   Called by gdbarch.  Sets up the gdbarch vector(s) for this target. */

static struct gdbarch *
xstormy16_gdbarch_init (struct gdbarch_info info, struct gdbarch_list *arches)
{
  static LONGEST call_dummy_words[1] = { 0 };
  struct gdbarch_tdep *tdep = NULL;
  struct gdbarch *gdbarch;

  /* find a candidate among the list of pre-declared architectures. */
  arches = gdbarch_list_lookup_by_info (arches, &info);
  if (arches != NULL)
    return (arches->gdbarch);

#if 0
  tdep = (struct gdbarch_tdep *) xmalloc (sizeof (struct gdbarch_tdep));
#endif

  gdbarch = gdbarch_alloc (&info, 0);

  /* NOTE: cagney/2002-12-06: This can be deleted when this arch is
     ready to unwind the PC first (see frame.c:get_prev_frame()).  */
  set_gdbarch_deprecated_init_frame_pc (gdbarch, deprecated_init_frame_pc_default);

  /*
   * Basic register fields and methods.
   */

  set_gdbarch_num_regs (gdbarch, E_NUM_REGS);
  set_gdbarch_num_pseudo_regs (gdbarch, 0);
  set_gdbarch_sp_regnum (gdbarch, E_SP_REGNUM);
  set_gdbarch_deprecated_fp_regnum (gdbarch, E_FP_REGNUM);
  set_gdbarch_pc_regnum (gdbarch, E_PC_REGNUM);
  set_gdbarch_register_name (gdbarch, xstormy16_register_name);
  set_gdbarch_deprecated_register_size (gdbarch, xstormy16_reg_size);
  set_gdbarch_deprecated_register_bytes (gdbarch, E_ALL_REGS_SIZE);
  set_gdbarch_deprecated_register_byte (gdbarch, xstormy16_register_byte);
  set_gdbarch_deprecated_register_raw_size (gdbarch, xstormy16_register_raw_size);
  set_gdbarch_deprecated_max_register_raw_size (gdbarch, xstormy16_pc_size);
  set_gdbarch_deprecated_register_virtual_size (gdbarch, xstormy16_register_raw_size);
  set_gdbarch_deprecated_max_register_virtual_size (gdbarch, 4);
  set_gdbarch_deprecated_register_virtual_type (gdbarch, xstormy16_reg_virtual_type);

  /*
   * Frame Info
   */
  set_gdbarch_deprecated_init_extra_frame_info (gdbarch,
				     xstormy16_init_extra_frame_info);
  set_gdbarch_deprecated_frame_init_saved_regs (gdbarch,
				     xstormy16_frame_init_saved_regs);
  set_gdbarch_deprecated_frame_chain (gdbarch, xstormy16_frame_chain);
  set_gdbarch_deprecated_get_saved_register (gdbarch, xstormy16_get_saved_register);
  set_gdbarch_deprecated_saved_pc_after_call (gdbarch, xstormy16_saved_pc_after_call);
  set_gdbarch_deprecated_frame_saved_pc (gdbarch, xstormy16_frame_saved_pc);
  set_gdbarch_skip_prologue (gdbarch, xstormy16_skip_prologue);
  set_gdbarch_deprecated_frame_chain_valid (gdbarch, xstormy16_frame_chain_valid);

  set_gdbarch_in_function_epilogue_p (gdbarch,
				      xstormy16_in_function_epilogue_p);

  /* 
   * Miscelany
   */
  /* Stack grows up. */
  set_gdbarch_inner_than (gdbarch, core_addr_greaterthan);

  /*
   * Call Dummies
   * 
   * These values and methods are used when gdb calls a target function.  */
  set_gdbarch_deprecated_push_return_address (gdbarch, xstormy16_push_return_address);
  set_gdbarch_deprecated_extract_return_value (gdbarch, xstormy16_extract_return_value);
  set_gdbarch_deprecated_push_arguments (gdbarch, xstormy16_push_arguments);
  set_gdbarch_deprecated_pop_frame (gdbarch, xstormy16_pop_frame);
  set_gdbarch_deprecated_store_struct_return (gdbarch, xstormy16_store_struct_return);
  set_gdbarch_deprecated_store_return_value (gdbarch, xstormy16_store_return_value);
  set_gdbarch_deprecated_extract_struct_value_address (gdbarch, xstormy16_extract_struct_value_address);
  set_gdbarch_use_struct_convention (gdbarch,
				     xstormy16_use_struct_convention);
  set_gdbarch_deprecated_call_dummy_words (gdbarch, call_dummy_words);
  set_gdbarch_deprecated_sizeof_call_dummy_words (gdbarch, 0);
  set_gdbarch_breakpoint_from_pc (gdbarch, xstormy16_breakpoint_from_pc);

  set_gdbarch_char_signed (gdbarch, 0);
  set_gdbarch_int_bit (gdbarch, 2 * TARGET_CHAR_BIT);
  set_gdbarch_ptr_bit (gdbarch, 2 * TARGET_CHAR_BIT);
  set_gdbarch_addr_bit (gdbarch, 4 * TARGET_CHAR_BIT);
  set_gdbarch_long_double_bit (gdbarch, 8 * TARGET_CHAR_BIT);

  set_gdbarch_address_to_pointer (gdbarch, xstormy16_address_to_pointer);
  set_gdbarch_pointer_to_address (gdbarch, xstormy16_pointer_to_address);

  set_gdbarch_deprecated_stack_align (gdbarch, xstormy16_stack_align);

  set_gdbarch_deprecated_save_dummy_frame_tos (gdbarch, xstormy16_save_dummy_frame_tos);

  set_gdbarch_skip_trampoline_code (gdbarch, xstormy16_skip_trampoline_code);

  set_gdbarch_in_solib_call_trampoline (gdbarch,
					xstormy16_in_solib_call_trampoline);

  /* Should be using push_dummy_call.  */
  set_gdbarch_deprecated_dummy_write_sp (gdbarch, deprecated_write_sp);

  set_gdbarch_print_insn (gdbarch, print_insn_xstormy16);

  return gdbarch;
}

/* Function: _initialize_xstormy16_tdep
   Initializer function for the Sanyo Xstormy16a module.
   Called by gdb at start-up. */

extern initialize_file_ftype _initialize_xstormy16_tdep; /* -Wmissing-prototypes */

void
_initialize_xstormy16_tdep (void)
{
  register_gdbarch_init (bfd_arch_xstormy16, xstormy16_gdbarch_init);
}
