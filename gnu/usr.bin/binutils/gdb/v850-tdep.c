/* Target-dependent code for the NEC V850 for GDB, the GNU debugger.

   Copyright 1996, 1998, 1999, 2000, 2001, 2002, 2003, 2004 Free
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

#include "defs.h"
#include "frame.h"
#include "inferior.h"
#include "target.h"
#include "value.h"
#include "bfd.h"
#include "gdb_string.h"
#include "gdbcore.h"
#include "objfiles.h"
#include "arch-utils.h"
#include "regcache.h"
#include "symtab.h"
#include "dis-asm.h"

struct gdbarch_tdep
{
  /* gdbarch target dependent data here. Currently unused for v850. */
};

/* Extra info which is saved in each frame_info. */
struct frame_extra_info
{ 
};

enum {
 E_R0_REGNUM,
 E_R1_REGNUM,
 E_R2_REGNUM, E_SAVE1_START_REGNUM = E_R2_REGNUM, E_SAVE1_END_REGNUM = E_R2_REGNUM,
 E_R3_REGNUM, E_SP_REGNUM = E_R3_REGNUM,
 E_R4_REGNUM,
 E_R5_REGNUM,
 E_R6_REGNUM, E_ARG0_REGNUM = E_R6_REGNUM,
 E_R7_REGNUM,
 E_R8_REGNUM,
 E_R9_REGNUM, E_ARGLAST_REGNUM = E_R9_REGNUM,
 E_R10_REGNUM, E_V0_REGNUM = E_R10_REGNUM,
 E_R11_REGNUM, E_V1_REGNUM = E_R11_REGNUM,
 E_R12_REGNUM,
 E_R13_REGNUM,
 E_R14_REGNUM,
 E_R15_REGNUM,
 E_R16_REGNUM,
 E_R17_REGNUM,
 E_R18_REGNUM,
 E_R19_REGNUM,
 E_R20_REGNUM, E_SAVE2_START_REGNUM = E_R20_REGNUM,
 E_R21_REGNUM,
 E_R22_REGNUM,
 E_R23_REGNUM,
 E_R24_REGNUM,
 E_R25_REGNUM,
 E_R26_REGNUM,
 E_R27_REGNUM,
 E_R28_REGNUM,
 E_R29_REGNUM, E_SAVE2_END_REGNUM = E_R29_REGNUM, E_FP_RAW_REGNUM = E_R29_REGNUM,
 E_R30_REGNUM, E_EP_REGNUM = E_R30_REGNUM,
 E_R31_REGNUM, E_SAVE3_START_REGNUM = E_R31_REGNUM, E_SAVE3_END_REGNUM = E_R31_REGNUM, E_RP_REGNUM = E_R31_REGNUM,
 E_R32_REGNUM, E_SR0_REGNUM = E_R32_REGNUM,
 E_R33_REGNUM,
 E_R34_REGNUM,
 E_R35_REGNUM,
 E_R36_REGNUM,
 E_R37_REGNUM, E_PS_REGNUM = E_R37_REGNUM,
 E_R38_REGNUM,
 E_R39_REGNUM,
 E_R40_REGNUM,
 E_R41_REGNUM,
 E_R42_REGNUM,
 E_R43_REGNUM,
 E_R44_REGNUM,
 E_R45_REGNUM,
 E_R46_REGNUM,
 E_R47_REGNUM,
 E_R48_REGNUM,
 E_R49_REGNUM,
 E_R50_REGNUM,
 E_R51_REGNUM,
 E_R52_REGNUM, E_CTBP_REGNUM = E_R52_REGNUM,
 E_R53_REGNUM,
 E_R54_REGNUM,
 E_R55_REGNUM,
 E_R56_REGNUM,
 E_R57_REGNUM,
 E_R58_REGNUM,
 E_R59_REGNUM,
 E_R60_REGNUM,
 E_R61_REGNUM,
 E_R62_REGNUM,
 E_R63_REGNUM,
 E_R64_REGNUM, E_PC_REGNUM = E_R64_REGNUM,
 E_R65_REGNUM, E_FP_REGNUM = E_R65_REGNUM,
 E_NUM_REGS
};

enum
{
  v850_reg_size = 4
};

/* Size of all registers as a whole. */
enum
{
  E_ALL_REGS_SIZE = (E_NUM_REGS) * v850_reg_size
};

/* Size of return datatype which fits into all return registers. */
enum
{
  E_MAX_RETTYPE_SIZE_IN_REGS = 2 * v850_reg_size
};

static LONGEST call_dummy_nil[] = {0};

static char *v850_generic_reg_names[] =
{ "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7", 
  "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15", 
  "r16", "r17", "r18", "r19", "r20", "r21", "r22", "r23", 
  "r24", "r25", "r26", "r27", "r28", "r29", "r30", "r31",
  "eipc", "eipsw", "fepc", "fepsw", "ecr", "psw", "sr6", "sr7",
  "sr8", "sr9", "sr10", "sr11", "sr12", "sr13", "sr14", "sr15",
  "sr16", "sr17", "sr18", "sr19", "sr20", "sr21", "sr22", "sr23",
  "sr24", "sr25", "sr26", "sr27", "sr28", "sr29", "sr30", "sr31",
  "pc", "fp"
};

static char *v850e_reg_names[] =
{
  "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7",
  "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15",
  "r16", "r17", "r18", "r19", "r20", "r21", "r22", "r23",
  "r24", "r25", "r26", "r27", "r28", "r29", "r30", "r31",
  "eipc", "eipsw", "fepc", "fepsw", "ecr", "psw", "sr6", "sr7",
  "sr8", "sr9", "sr10", "sr11", "sr12", "sr13", "sr14", "sr15",
  "ctpc", "ctpsw", "dbpc", "dbpsw", "ctbp", "sr21", "sr22", "sr23",
  "sr24", "sr25", "sr26", "sr27", "sr28", "sr29", "sr30", "sr31",
  "pc", "fp"
};

char **v850_register_names = v850_generic_reg_names;

struct
  {
    char **regnames;
    int mach;
  }
v850_processor_type_table[] =
{
  {
    v850_generic_reg_names, bfd_mach_v850
  }
  ,
  {
    v850e_reg_names, bfd_mach_v850e
  }
  ,
  {
    v850e_reg_names, bfd_mach_v850e1
  }
  ,
  {
    NULL, 0
  }
};

/* Info gleaned from scanning a function's prologue.  */

struct pifsr			/* Info about one saved reg */
  {
    int framereg;		/* Frame reg (SP or FP) */
    int offset;			/* Offset from framereg */
    int cur_frameoffset;	/* Current frameoffset */
    int reg;			/* Saved register number */
  };

struct prologue_info
  {
    int framereg;
    int frameoffset;
    int start_function;
    struct pifsr *pifsrs;
  };

static CORE_ADDR v850_scan_prologue (CORE_ADDR pc, struct prologue_info *fs);

/* Function: v850_register_name
   Returns the name of the v850/v850e register N. */

static const char *
v850_register_name (int regnum)
{
  if (regnum < 0 || regnum >= E_NUM_REGS)
    internal_error (__FILE__, __LINE__,
                    "v850_register_name: illegal register number %d",
                    regnum);
  else
    return v850_register_names[regnum];

}

/* Function: v850_register_byte 
   Returns the byte position in the register cache for register N. */

static int
v850_register_byte (int regnum)
{
  if (regnum < 0 || regnum >= E_NUM_REGS)
    internal_error (__FILE__, __LINE__,
                    "v850_register_byte: illegal register number %d",
                    regnum);
  else
    return regnum * v850_reg_size;
}

/* Function: v850_register_raw_size
   Returns the number of bytes occupied by the register on the target. */

static int
v850_register_raw_size (int regnum)
{
  if (regnum < 0 || regnum >= E_NUM_REGS)
    internal_error (__FILE__, __LINE__,
                    "v850_register_raw_size: illegal register number %d",
                    regnum);
  /* Only the PC has 4 Byte, all other registers 2 Byte. */
  else
    return v850_reg_size;
}

/* Function: v850_reg_virtual_type 
   Returns the default type for register N. */

static struct type *
v850_reg_virtual_type (int regnum)
{
  if (regnum < 0 || regnum >= E_NUM_REGS)
    internal_error (__FILE__, __LINE__,
                    "v850_register_virtual_type: illegal register number %d",
                    regnum);
  else if (regnum == E_PC_REGNUM)
    return builtin_type_uint32;
  else
    return builtin_type_int32;
}

static int
v850_type_is_scalar (struct type *t)
{
  return (TYPE_CODE (t) != TYPE_CODE_STRUCT
	  && TYPE_CODE (t) != TYPE_CODE_UNION
	  && TYPE_CODE (t) != TYPE_CODE_ARRAY);
}

/* Should call_function allocate stack space for a struct return?  */
static int
v850_use_struct_convention (int gcc_p, struct type *type)
{
  /* According to ABI:
   * return TYPE_LENGTH (type) > 8);
   */

  /* Current implementation in gcc: */

  int i;
  struct type *fld_type, *tgt_type;

  /* 1. The value is greater than 8 bytes -> returned by copying */
  if (TYPE_LENGTH (type) > 8)
    return 1;

  /* 2. The value is a single basic type -> returned in register */
  if (v850_type_is_scalar (type))
    return 0;

  /* The value is a structure or union with a single element
   * and that element is either a single basic type or an array of
   * a single basic type whoes size is greater than or equal to 4
   * -> returned in register */
  if ((TYPE_CODE (type) == TYPE_CODE_STRUCT
       || TYPE_CODE (type) == TYPE_CODE_UNION)
       && TYPE_NFIELDS (type) == 1)
    {
      fld_type = TYPE_FIELD_TYPE (type, 0);
      if (v850_type_is_scalar (fld_type) && TYPE_LENGTH (fld_type) >= 4)
	return 0;

      if (TYPE_CODE (fld_type) == TYPE_CODE_ARRAY)
        {
	  tgt_type = TYPE_TARGET_TYPE (fld_type);
	  if (v850_type_is_scalar (tgt_type) && TYPE_LENGTH (tgt_type) >= 4)
	    return 0;
	}
    }

  /* The value is a structure whose first element is an integer or
   * a float, and which contains no arrays of more than two elements
   * -> returned in register */
  if (TYPE_CODE (type) == TYPE_CODE_STRUCT
      && v850_type_is_scalar (TYPE_FIELD_TYPE (type, 0))
      && TYPE_LENGTH (TYPE_FIELD_TYPE (type, 0)) == 4)
    {
      for (i = 1; i < TYPE_NFIELDS (type); ++i)
        {
	  fld_type = TYPE_FIELD_TYPE (type, 0);
	  if (TYPE_CODE (fld_type) == TYPE_CODE_ARRAY)
	    {
	      tgt_type = TYPE_TARGET_TYPE (fld_type);
	      if (TYPE_LENGTH (fld_type) >= 0 && TYPE_LENGTH (tgt_type) >= 0
		  && TYPE_LENGTH (fld_type) / TYPE_LENGTH (tgt_type) > 2)
		return 1;
	    }
	}
      return 0;
    }
    
  /* The value is a union which contains at least one field which
   * would be returned in registers according to these rules
   * -> returned in register */
  if (TYPE_CODE (type) == TYPE_CODE_UNION)
    {
      for (i = 0; i < TYPE_NFIELDS (type); ++i)
        {
	  fld_type = TYPE_FIELD_TYPE (type, 0);
	  if (!v850_use_struct_convention (0, fld_type))
	    return 0;
	}
    }

  return 1;
}



/* Structure for mapping bits in register lists to register numbers. */
struct reg_list
{
  long mask;
  int regno;
};

/* Helper function for v850_scan_prologue to handle prepare instruction. */

static void
handle_prepare (int insn, int insn2, CORE_ADDR * current_pc_ptr,
		struct prologue_info *pi, struct pifsr **pifsr_ptr)
{
  CORE_ADDR current_pc = *current_pc_ptr;
  struct pifsr *pifsr = *pifsr_ptr;
  long next = insn2 & 0xffff;
  long list12 = ((insn & 1) << 16) + (next & 0xffe0);
  long offset = (insn & 0x3e) << 1;
  static struct reg_list reg_table[] =
  {
    {0x00800, 20},		/* r20 */
    {0x00400, 21},		/* r21 */
    {0x00200, 22},		/* r22 */
    {0x00100, 23},		/* r23 */
    {0x08000, 24},		/* r24 */
    {0x04000, 25},		/* r25 */
    {0x02000, 26},		/* r26 */
    {0x01000, 27},		/* r27 */
    {0x00080, 28},		/* r28 */
    {0x00040, 29},		/* r29 */
    {0x10000, 30},		/* ep */
    {0x00020, 31},		/* lp */
    {0, 0}			/* end of table */
  };
  int i;

  if ((next & 0x1f) == 0x0b)	/* skip imm16 argument */
    current_pc += 2;
  else if ((next & 0x1f) == 0x13)	/* skip imm16 argument */
    current_pc += 2;
  else if ((next & 0x1f) == 0x1b)	/* skip imm32 argument */
    current_pc += 4;

  /* Calculate the total size of the saved registers, and add it
     it to the immediate value used to adjust SP. */
  for (i = 0; reg_table[i].mask != 0; i++)
    if (list12 & reg_table[i].mask)
      offset += v850_register_raw_size (reg_table[i].regno);
  pi->frameoffset -= offset;

  /* Calculate the offsets of the registers relative to the value
     the SP will have after the registers have been pushed and the
     imm5 value has been subtracted from it. */
  if (pifsr)
    {
      for (i = 0; reg_table[i].mask != 0; i++)
	{
	  if (list12 & reg_table[i].mask)
	    {
	      int reg = reg_table[i].regno;
	      offset -= v850_register_raw_size (reg);
	      pifsr->reg = reg;
	      pifsr->offset = offset;
	      pifsr->cur_frameoffset = pi->frameoffset;
#ifdef DEBUG
	      printf_filtered ("\tSaved register r%d, offset %d", reg, pifsr->offset);
#endif
	      pifsr++;
	    }
	}
    }
#ifdef DEBUG
  printf_filtered ("\tfound ctret after regsave func");
#endif

  /* Set result parameters. */
  *current_pc_ptr = current_pc;
  *pifsr_ptr = pifsr;
}


/* Helper function for v850_scan_prologue to handle pushm/pushl instructions.
   FIXME: the SR bit of the register list is not supported; must check
   that the compiler does not ever generate this bit. */

static void
handle_pushm (int insn, int insn2, struct prologue_info *pi,
	      struct pifsr **pifsr_ptr)
{
  struct pifsr *pifsr = *pifsr_ptr;
  long list12 = ((insn & 0x0f) << 16) + (insn2 & 0xfff0);
  long offset = 0;
  static struct reg_list pushml_reg_table[] =
  {
    {0x80000, E_PS_REGNUM},	/* PSW */
    {0x40000, 1},		/* r1 */
    {0x20000, 2},		/* r2 */
    {0x10000, 3},		/* r3 */
    {0x00800, 4},		/* r4 */
    {0x00400, 5},		/* r5 */
    {0x00200, 6},		/* r6 */
    {0x00100, 7},		/* r7 */
    {0x08000, 8},		/* r8 */
    {0x04000, 9},		/* r9 */
    {0x02000, 10},		/* r10 */
    {0x01000, 11},		/* r11 */
    {0x00080, 12},		/* r12 */
    {0x00040, 13},		/* r13 */
    {0x00020, 14},		/* r14 */
    {0x00010, 15},		/* r15 */
    {0, 0}			/* end of table */
  };
  static struct reg_list pushmh_reg_table[] =
  {
    {0x80000, 16},		/* r16 */
    {0x40000, 17},		/* r17 */
    {0x20000, 18},		/* r18 */
    {0x10000, 19},		/* r19 */
    {0x00800, 20},		/* r20 */
    {0x00400, 21},		/* r21 */
    {0x00200, 22},		/* r22 */
    {0x00100, 23},		/* r23 */
    {0x08000, 24},		/* r24 */
    {0x04000, 25},		/* r25 */
    {0x02000, 26},		/* r26 */
    {0x01000, 27},		/* r27 */
    {0x00080, 28},		/* r28 */
    {0x00040, 29},		/* r29 */
    {0x00010, 30},		/* r30 */
    {0x00020, 31},		/* r31 */
    {0, 0}			/* end of table */
  };
  struct reg_list *reg_table;
  int i;

  /* Is this a pushml or a pushmh? */
  if ((insn2 & 7) == 1)
    reg_table = pushml_reg_table;
  else
    reg_table = pushmh_reg_table;

  /* Calculate the total size of the saved registers, and add it
     it to the immediate value used to adjust SP. */
  for (i = 0; reg_table[i].mask != 0; i++)
    if (list12 & reg_table[i].mask)
      offset += v850_register_raw_size (reg_table[i].regno);
  pi->frameoffset -= offset;

  /* Calculate the offsets of the registers relative to the value
     the SP will have after the registers have been pushed and the
     imm5 value is subtracted from it. */
  if (pifsr)
    {
      for (i = 0; reg_table[i].mask != 0; i++)
	{
	  if (list12 & reg_table[i].mask)
	    {
	      int reg = reg_table[i].regno;
	      offset -= v850_register_raw_size (reg);
	      pifsr->reg = reg;
	      pifsr->offset = offset;
	      pifsr->cur_frameoffset = pi->frameoffset;
#ifdef DEBUG
	      printf_filtered ("\tSaved register r%d, offset %d", reg, pifsr->offset);
#endif
	      pifsr++;
	    }
	}
    }
#ifdef DEBUG
  printf_filtered ("\tfound ctret after regsave func");
#endif

  /* Set result parameters. */
  *pifsr_ptr = pifsr;
}




/* Function: scan_prologue
   Scan the prologue of the function that contains PC, and record what
   we find in PI.  Returns the pc after the prologue.  Note that the
   addresses saved in frame->saved_regs are just frame relative (negative
   offsets from the frame pointer).  This is because we don't know the
   actual value of the frame pointer yet.  In some circumstances, the
   frame pointer can't be determined till after we have scanned the
   prologue.  */

static CORE_ADDR
v850_scan_prologue (CORE_ADDR pc, struct prologue_info *pi)
{
  CORE_ADDR func_addr, prologue_end, current_pc;
  struct pifsr *pifsr, *pifsr_tmp;
  int fp_used;
  int ep_used;
  int reg;
  CORE_ADDR save_pc, save_end;
  int regsave_func_p;
  int r12_tmp;

  /* First, figure out the bounds of the prologue so that we can limit the
     search to something reasonable.  */

  if (find_pc_partial_function (pc, NULL, &func_addr, NULL))
    {
      struct symtab_and_line sal;

      sal = find_pc_line (func_addr, 0);

      if (func_addr == entry_point_address ())
	pi->start_function = 1;
      else
	pi->start_function = 0;

#if 0
      if (sal.line == 0)
	prologue_end = pc;
      else
	prologue_end = sal.end;
#else
      prologue_end = pc;
#endif
    }
  else
    {				/* We're in the boondocks */
      func_addr = pc - 100;
      prologue_end = pc;
    }

  prologue_end = min (prologue_end, pc);

  /* Now, search the prologue looking for instructions that setup fp, save
     rp, adjust sp and such.  We also record the frame offset of any saved
     registers. */

  pi->frameoffset = 0;
  pi->framereg = E_SP_REGNUM;
  fp_used = 0;
  ep_used = 0;
  pifsr = pi->pifsrs;
  regsave_func_p = 0;
  save_pc = 0;
  save_end = 0;
  r12_tmp = 0;

#ifdef DEBUG
  printf_filtered ("Current_pc = 0x%.8lx, prologue_end = 0x%.8lx\n",
		   (long) func_addr, (long) prologue_end);
#endif

  for (current_pc = func_addr; current_pc < prologue_end;)
    {
      int insn;
      int insn2 = -1; /* dummy value */

#ifdef DEBUG
      fprintf_filtered (gdb_stdlog, "0x%.8lx ", (long) current_pc);
      gdb_print_insn (current_pc, gdb_stdlog);
#endif

      insn = read_memory_unsigned_integer (current_pc, 2);
      current_pc += 2;
      if ((insn & 0x0780) >= 0x0600)	/* Four byte instruction? */
	{
	  insn2 = read_memory_unsigned_integer (current_pc, 2);
	  current_pc += 2;
	}

      if ((insn & 0xffc0) == ((10 << 11) | 0x0780) && !regsave_func_p)
	{			/* jarl <func>,10 */
	  long low_disp = insn2 & ~(long) 1;
	  long disp = (((((insn & 0x3f) << 16) + low_disp)
			& ~(long) 1) ^ 0x00200000) - 0x00200000;

	  save_pc = current_pc;
	  save_end = prologue_end;
	  regsave_func_p = 1;
	  current_pc += disp - 4;
	  prologue_end = (current_pc
			  + (2 * 3)	/* moves to/from ep */
			  + 4	/* addi <const>,sp,sp */
			  + 2	/* jmp [r10] */
			  + (2 * 12)	/* sst.w to save r2, r20-r29, r31 */
			  + 20);	/* slop area */

#ifdef DEBUG
	  printf_filtered ("\tfound jarl <func>,r10, disp = %ld, low_disp = %ld, new pc = 0x%.8lx\n",
			   disp, low_disp, (long) current_pc + 2);
#endif
	  continue;
	}
      else if ((insn & 0xffc0) == 0x0200 && !regsave_func_p)
	{			/* callt <imm6> */
	  long ctbp = read_register (E_CTBP_REGNUM);
	  long adr = ctbp + ((insn & 0x3f) << 1);

	  save_pc = current_pc;
	  save_end = prologue_end;
	  regsave_func_p = 1;
	  current_pc = ctbp + (read_memory_unsigned_integer (adr, 2) & 0xffff);
	  prologue_end = (current_pc
			  + (2 * 3)	/* prepare list2,imm5,sp/imm */
			  + 4	/* ctret */
			  + 20);	/* slop area */

#ifdef DEBUG
	  printf_filtered ("\tfound callt,  ctbp = 0x%.8lx, adr = %.8lx, new pc = 0x%.8lx\n",
			   ctbp, adr, (long) current_pc);
#endif
	  continue;
	}
      else if ((insn & 0xffc0) == 0x0780)	/* prepare list2,imm5 */
	{
	  handle_prepare (insn, insn2, &current_pc, pi, &pifsr);
	  continue;
	}
      else if (insn == 0x07e0 && regsave_func_p && insn2 == 0x0144)
	{			/* ctret after processing register save function */
	  current_pc = save_pc;
	  prologue_end = save_end;
	  regsave_func_p = 0;
#ifdef DEBUG
	  printf_filtered ("\tfound ctret after regsave func");
#endif
	  continue;
	}
      else if ((insn & 0xfff0) == 0x07e0 && (insn2 & 5) == 1)
	{			/* pushml, pushmh */
	  handle_pushm (insn, insn2, pi, &pifsr);
	  continue;
	}
      else if ((insn & 0xffe0) == 0x0060 && regsave_func_p)
	{			/* jmp after processing register save function */
	  current_pc = save_pc;
	  prologue_end = save_end;
	  regsave_func_p = 0;
#ifdef DEBUG
	  printf_filtered ("\tfound jmp after regsave func");
#endif
	  continue;
	}
      else if ((insn & 0x07c0) == 0x0780	/* jarl or jr */
	       || (insn & 0xffe0) == 0x0060	/* jmp */
	       || (insn & 0x0780) == 0x0580)	/* branch */
	{
#ifdef DEBUG
	  printf_filtered ("\n");
#endif
	  break;		/* Ran into end of prologue */
	}

      else if ((insn & 0xffe0) == ((E_SP_REGNUM << 11) | 0x0240))		/* add <imm>,sp */
	pi->frameoffset += ((insn & 0x1f) ^ 0x10) - 0x10;
      else if (insn == ((E_SP_REGNUM << 11) | 0x0600 | E_SP_REGNUM))	/* addi <imm>,sp,sp */
	pi->frameoffset += insn2;
      else if (insn == ((E_FP_RAW_REGNUM << 11) | 0x0000 | E_SP_REGNUM))	/* mov sp,fp */
	{
	  fp_used = 1;
	  pi->framereg = E_FP_RAW_REGNUM;
	}

      else if (insn == ((E_R12_REGNUM << 11) | 0x0640 | E_R0_REGNUM))	/* movhi hi(const),r0,r12 */
	r12_tmp = insn2 << 16;
      else if (insn == ((E_R12_REGNUM << 11) | 0x0620 | E_R12_REGNUM))	/* movea lo(const),r12,r12 */
	r12_tmp += insn2;
      else if (insn == ((E_SP_REGNUM << 11) | 0x01c0 | E_R12_REGNUM) && r12_tmp)	/* add r12,sp */
	pi->frameoffset = r12_tmp;
      else if (insn == ((E_EP_REGNUM << 11) | 0x0000 | E_SP_REGNUM))	/* mov sp,ep */
	ep_used = 1;
      else if (insn == ((E_EP_REGNUM << 11) | 0x0000 | E_R1_REGNUM))	/* mov r1,ep */
	ep_used = 0;
      else if (((insn & 0x07ff) == (0x0760 | E_SP_REGNUM)		/* st.w <reg>,<offset>[sp] */
		|| (fp_used
		    && (insn & 0x07ff) == (0x0760 | E_FP_RAW_REGNUM)))	/* st.w <reg>,<offset>[fp] */
	       && pifsr
	       && (((reg = (insn >> 11) & 0x1f) >= E_SAVE1_START_REGNUM && reg <= E_SAVE1_END_REGNUM)
		   || (reg >= E_SAVE2_START_REGNUM && reg <= E_SAVE2_END_REGNUM)
		 || (reg >= E_SAVE3_START_REGNUM && reg <= E_SAVE3_END_REGNUM)))
	{
	  pifsr->reg = reg;
	  pifsr->offset = insn2 & ~1;
	  pifsr->cur_frameoffset = pi->frameoffset;
#ifdef DEBUG
	  printf_filtered ("\tSaved register r%d, offset %d", reg, pifsr->offset);
#endif
	  pifsr++;
	}

      else if (ep_used		/* sst.w <reg>,<offset>[ep] */
	       && ((insn & 0x0781) == 0x0501)
	       && pifsr
	       && (((reg = (insn >> 11) & 0x1f) >= E_SAVE1_START_REGNUM && reg <= E_SAVE1_END_REGNUM)
		   || (reg >= E_SAVE2_START_REGNUM && reg <= E_SAVE2_END_REGNUM)
		 || (reg >= E_SAVE3_START_REGNUM && reg <= E_SAVE3_END_REGNUM)))
	{
	  pifsr->reg = reg;
	  pifsr->offset = (insn & 0x007e) << 1;
	  pifsr->cur_frameoffset = pi->frameoffset;
#ifdef DEBUG
	  printf_filtered ("\tSaved register r%d, offset %d", reg, pifsr->offset);
#endif
	  pifsr++;
	}

#ifdef DEBUG
      printf_filtered ("\n");
#endif
    }

  if (pifsr)
    pifsr->framereg = 0;	/* Tie off last entry */

  /* Fix up any offsets to the final offset.  If a frame pointer was created, use it
     instead of the stack pointer.  */
  for (pifsr_tmp = pi->pifsrs; pifsr_tmp && pifsr_tmp != pifsr; pifsr_tmp++)
    {
      pifsr_tmp->offset -= pi->frameoffset - pifsr_tmp->cur_frameoffset;
      pifsr_tmp->framereg = pi->framereg;

#ifdef DEBUG
      printf_filtered ("Saved register r%d, offset = %d, framereg = r%d\n",
		    pifsr_tmp->reg, pifsr_tmp->offset, pifsr_tmp->framereg);
#endif
    }

#ifdef DEBUG
  printf_filtered ("Framereg = r%d, frameoffset = %d\n", pi->framereg, pi->frameoffset);
#endif

  return current_pc;
}

/* Function: find_callers_reg
   Find REGNUM on the stack.  Otherwise, it's in an active register.
   One thing we might want to do here is to check REGNUM against the
   clobber mask, and somehow flag it as invalid if it isn't saved on
   the stack somewhere.  This would provide a graceful failure mode
   when trying to get the value of caller-saves registers for an inner
   frame.  */

static CORE_ADDR
v850_find_callers_reg (struct frame_info *fi, int regnum)
{
  for (; fi; fi = get_next_frame (fi))
    if (DEPRECATED_PC_IN_CALL_DUMMY (get_frame_pc (fi), get_frame_base (fi),
				     get_frame_base (fi)))
      return deprecated_read_register_dummy (get_frame_pc (fi),
					     get_frame_base (fi), regnum);
    else if (deprecated_get_frame_saved_regs (fi)[regnum] != 0)
      return read_memory_unsigned_integer (deprecated_get_frame_saved_regs (fi)[regnum],
					   v850_register_raw_size (regnum));

  return read_register (regnum);
}

/* Function: frame_chain
   Figure out the frame prior to FI.  Unfortunately, this involves
   scanning the prologue of the caller, which will also be done
   shortly by v850_init_extra_frame_info.  For the dummy frame, we
   just return the stack pointer that was in use at the time the
   function call was made.  */

static CORE_ADDR
v850_frame_chain (struct frame_info *fi)
{
  struct prologue_info pi;
  CORE_ADDR callers_pc, fp;

  /* First, find out who called us */
  callers_pc = DEPRECATED_FRAME_SAVED_PC (fi);
  /* If caller is a call-dummy, then our FP bears no relation to his FP! */
  fp = v850_find_callers_reg (fi, E_FP_RAW_REGNUM);
  if (DEPRECATED_PC_IN_CALL_DUMMY (callers_pc, fp, fp))
    return fp;			/* caller is call-dummy: return oldest value of FP */

  /* Caller is NOT a call-dummy, so everything else should just work.
     Even if THIS frame is a call-dummy! */
  pi.pifsrs = NULL;

  v850_scan_prologue (callers_pc, &pi);

  if (pi.start_function)
    return 0;			/* Don't chain beyond the start function */

  if (pi.framereg == E_FP_RAW_REGNUM)
    return v850_find_callers_reg (fi, pi.framereg);

  return get_frame_base (fi) - pi.frameoffset;
}

/* Function: skip_prologue
   Return the address of the first code past the prologue of the function.  */

static CORE_ADDR
v850_skip_prologue (CORE_ADDR pc)
{
  CORE_ADDR func_addr, func_end;

  /* See what the symbol table says */

  if (find_pc_partial_function (pc, NULL, &func_addr, &func_end))
    {
      struct symtab_and_line sal;

      sal = find_pc_line (func_addr, 0);

      if (sal.line != 0 && sal.end < func_end)
	return sal.end;
      else
	/* Either there's no line info, or the line after the prologue is after
	   the end of the function.  In this case, there probably isn't a
	   prologue.  */
	return pc;
    }

/* We can't find the start of this function, so there's nothing we can do. */
  return pc;
}

/* Function: pop_frame
   This routine gets called when either the user uses the `return'
   command, or the call dummy breakpoint gets hit.  */

static void
v850_pop_frame (void)
{
  struct frame_info *frame = get_current_frame ();
  int regnum;

  if (DEPRECATED_PC_IN_CALL_DUMMY (get_frame_pc (frame),
				   get_frame_base (frame),
				   get_frame_base (frame)))
    generic_pop_dummy_frame ();
  else
    {
      write_register (E_PC_REGNUM, DEPRECATED_FRAME_SAVED_PC (frame));

      for (regnum = 0; regnum < E_NUM_REGS; regnum++)
	if (deprecated_get_frame_saved_regs (frame)[regnum] != 0)
	  write_register (regnum,
		      read_memory_unsigned_integer (deprecated_get_frame_saved_regs (frame)[regnum],
					     v850_register_raw_size (regnum)));

      write_register (E_SP_REGNUM, get_frame_base (frame));
    }

  flush_cached_frames ();
}

/* Function: push_arguments
   Setup arguments and RP for a call to the target.  First four args
   go in R6->R9, subsequent args go into sp + 16 -> sp + ...  Structs
   are passed by reference.  64 bit quantities (doubles and long
   longs) may be split between the regs and the stack.  When calling a
   function that returns a struct, a pointer to the struct is passed
   in as a secret first argument (always in R6).

   Stack space for the args has NOT been allocated: that job is up to us.
 */

static CORE_ADDR
v850_push_arguments (int nargs, struct value **args, CORE_ADDR sp,
		     int struct_return, CORE_ADDR struct_addr)
{
  int argreg;
  int argnum;
  int len = 0;
  int stack_offset;

  /* First, just for safety, make sure stack is aligned */
  sp &= ~3;

  /* The offset onto the stack at which we will start copying parameters
     (after the registers are used up) begins at 16 rather than at zero.
     I don't really know why, that's just the way it seems to work.  */
  stack_offset = 16;

  /* Now make space on the stack for the args. */
  for (argnum = 0; argnum < nargs; argnum++)
    len += ((TYPE_LENGTH (VALUE_TYPE (args[argnum])) + 3) & ~3);
  sp -= len + stack_offset;	/* possibly over-allocating, but it works... */
  /* (you might think we could allocate 16 bytes */
  /* less, but the ABI seems to use it all! )  */

  argreg = E_ARG0_REGNUM;
  /* the struct_return pointer occupies the first parameter-passing reg */
  if (struct_return)
    argreg++;

  /* Now load as many as possible of the first arguments into
     registers, and push the rest onto the stack.  There are 16 bytes
     in four registers available.  Loop thru args from first to last.  */
  for (argnum = 0; argnum < nargs; argnum++)
    {
      int len;
      char *val;
      char valbuf[v850_register_raw_size (E_ARG0_REGNUM)];

      if (!v850_type_is_scalar (VALUE_TYPE (*args))
	  && TYPE_LENGTH (VALUE_TYPE (*args)) > E_MAX_RETTYPE_SIZE_IN_REGS)
	{
	  store_unsigned_integer (valbuf, 4, VALUE_ADDRESS (*args));
	  len = 4;
	  val = valbuf;
	}
      else
	{
	  len = TYPE_LENGTH (VALUE_TYPE (*args));
	  val = (char *) VALUE_CONTENTS (*args);
	}

      while (len > 0)
	if (argreg <= E_ARGLAST_REGNUM)
	  {
	    CORE_ADDR regval;

	    regval = extract_unsigned_integer (val, v850_register_raw_size (argreg));
	    write_register (argreg, regval);

	    len -= v850_register_raw_size (argreg);
	    val += v850_register_raw_size (argreg);
	    argreg++;
	  }
	else
	  {
	    write_memory (sp + stack_offset, val, 4);

	    len -= 4;
	    val += 4;
	    stack_offset += 4;
	  }
      args++;
    }
  return sp;
}

/* Function: push_return_address (pc)
   Set up the return address for the inferior function call.
   Needed for targets where we don't actually execute a JSR/BSR instruction */

static CORE_ADDR
v850_push_return_address (CORE_ADDR pc, CORE_ADDR sp)
{
  write_register (E_RP_REGNUM, entry_point_address ());
  return sp;
}

/* Function: frame_saved_pc 
   Find the caller of this frame.  We do this by seeing if E_RP_REGNUM
   is saved in the stack anywhere, otherwise we get it from the
   registers.  If the inner frame is a dummy frame, return its PC
   instead of RP, because that's where "caller" of the dummy-frame
   will be found.  */

static CORE_ADDR
v850_frame_saved_pc (struct frame_info *fi)
{
  if (DEPRECATED_PC_IN_CALL_DUMMY (get_frame_pc (fi), get_frame_base (fi),
				   get_frame_base (fi)))
    return deprecated_read_register_dummy (get_frame_pc (fi),
					   get_frame_base (fi), E_PC_REGNUM);
  else
    return v850_find_callers_reg (fi, E_RP_REGNUM);
}


/* Function: fix_call_dummy
   Pokes the callee function's address into the CALL_DUMMY assembly stub.
   Assumes that the CALL_DUMMY looks like this:
   jarl <offset24>, r31
   trap
 */

static void
v850_fix_call_dummy (char *dummy, CORE_ADDR sp, CORE_ADDR fun, int nargs,
		     struct value **args, struct type *type, int gcc_p)
{
  long offset24;

  offset24 = (long) fun - (long) entry_point_address ();
  offset24 &= 0x3fffff;
  offset24 |= 0xff800000;	/* jarl <offset24>, r31 */

  store_unsigned_integer ((unsigned int *) &dummy[2], 2, offset24 & 0xffff);
  store_unsigned_integer ((unsigned int *) &dummy[0], 2, offset24 >> 16);
}

static CORE_ADDR
v850_saved_pc_after_call (struct frame_info *ignore)
{
  return read_register (E_RP_REGNUM);
}

static void
v850_extract_return_value (struct type *type, char *regbuf, char *valbuf)
{
  CORE_ADDR return_buffer;

  if (!v850_use_struct_convention (0, type))
    {
      /* Scalar return values of <= 8 bytes are returned in 
         E_V0_REGNUM to E_V1_REGNUM. */
      memcpy (valbuf,
	      &regbuf[DEPRECATED_REGISTER_BYTE (E_V0_REGNUM)],
	      TYPE_LENGTH (type));
    }
  else
    {
      /* Aggregates and return values > 8 bytes are returned in memory,
         pointed to by R6. */
      return_buffer =
	extract_unsigned_integer (regbuf + DEPRECATED_REGISTER_BYTE (E_V0_REGNUM),
				  DEPRECATED_REGISTER_RAW_SIZE (E_V0_REGNUM));

      read_memory (return_buffer, valbuf, TYPE_LENGTH (type));
    }
}

const static unsigned char *
v850_breakpoint_from_pc (CORE_ADDR *pcptr, int *lenptr)
{
  static unsigned char breakpoint[] = { 0x85, 0x05 };
  *lenptr = sizeof (breakpoint);
  return breakpoint;
}

static void
v850_store_return_value (struct type *type, char *valbuf)
{
  CORE_ADDR return_buffer;

  if (!v850_use_struct_convention (0, type))
    deprecated_write_register_bytes (DEPRECATED_REGISTER_BYTE (E_V0_REGNUM), valbuf,
				     TYPE_LENGTH (type));
  else
    {
      return_buffer = read_register (E_V0_REGNUM);
      write_memory (return_buffer, valbuf, TYPE_LENGTH (type));
    }
}

static void
v850_frame_init_saved_regs (struct frame_info *fi)
{
  struct prologue_info pi;
  struct pifsr pifsrs[E_NUM_REGS + 1], *pifsr;
  CORE_ADDR func_addr, func_end;

  if (!deprecated_get_frame_saved_regs (fi))
    {
      frame_saved_regs_zalloc (fi);

      /* The call dummy doesn't save any registers on the stack, so we
         can return now.  */
      if (DEPRECATED_PC_IN_CALL_DUMMY (get_frame_pc (fi), get_frame_base (fi),
				       get_frame_base (fi)))
	return;

      /* Find the beginning of this function, so we can analyze its
         prologue. */
      if (find_pc_partial_function (get_frame_pc (fi), NULL, &func_addr, &func_end))
	{
	  pi.pifsrs = pifsrs;

	  v850_scan_prologue (get_frame_pc (fi), &pi);

	  if (!get_next_frame (fi) && pi.framereg == E_SP_REGNUM)
	    deprecated_update_frame_base_hack (fi, read_register (pi.framereg) - pi.frameoffset);

	  for (pifsr = pifsrs; pifsr->framereg; pifsr++)
	    {
	      deprecated_get_frame_saved_regs (fi)[pifsr->reg] = pifsr->offset + get_frame_base (fi);

	      if (pifsr->framereg == E_SP_REGNUM)
		deprecated_get_frame_saved_regs (fi)[pifsr->reg] += pi.frameoffset;
	    }
	}
      /* Else we're out of luck (can't debug completely stripped code). 
         FIXME. */
    }
}

/* Function: init_extra_frame_info
   Setup the frame's frame pointer, pc, and frame addresses for saved
   registers.  Most of the work is done in scan_prologue().

   Note that when we are called for the last frame (currently active frame),
   that get_frame_pc (fi) and fi->frame will already be setup.  However, fi->frame will
   be valid only if this routine uses FP.  For previous frames, fi-frame will
   always be correct (since that is derived from v850_frame_chain ()).

   We can be called with the PC in the call dummy under two
   circumstances.  First, during normal backtracing, second, while
   figuring out the frame pointer just prior to calling the target
   function (see call_function_by_hand).  */

static void
v850_init_extra_frame_info (int fromleaf, struct frame_info *fi)
{
  struct prologue_info pi;

  if (get_next_frame (fi))
    deprecated_update_frame_pc_hack (fi, DEPRECATED_FRAME_SAVED_PC (get_next_frame (fi)));

  v850_frame_init_saved_regs (fi);
}

static void
v850_store_struct_return (CORE_ADDR addr, CORE_ADDR sp)
{
  write_register (E_ARG0_REGNUM, addr);
}

static CORE_ADDR
v850_target_read_fp (void)
{
  return read_register (E_FP_RAW_REGNUM);
}

static struct gdbarch *
v850_gdbarch_init (struct gdbarch_info info, struct gdbarch_list *arches)
{
  static LONGEST call_dummy_words[1] = { 0 };
  struct gdbarch_tdep *tdep = NULL;
  struct gdbarch *gdbarch;
  int i;

  /* find a candidate among the list of pre-declared architectures. */
  arches = gdbarch_list_lookup_by_info (arches, &info);
  if (arches != NULL)
    return (arches->gdbarch);

#if 0
  tdep = (struct gdbarch_tdep *) xmalloc (sizeof (struct gdbarch_tdep));
#endif

  /* Change the register names based on the current machine type. */
  if (info.bfd_arch_info->arch != bfd_arch_v850)
    return 0;

  gdbarch = gdbarch_alloc (&info, 0);

  /* NOTE: cagney/2002-12-06: This can be deleted when this arch is
     ready to unwind the PC first (see frame.c:get_prev_frame()).  */
  set_gdbarch_deprecated_init_frame_pc (gdbarch, deprecated_init_frame_pc_default);

  for (i = 0; v850_processor_type_table[i].regnames != NULL; i++)
    {
      if (v850_processor_type_table[i].mach == info.bfd_arch_info->mach)
	{
	  v850_register_names = v850_processor_type_table[i].regnames;
	  break;
	}
    }

  /*
   * Basic register fields and methods.
   */
  set_gdbarch_num_regs (gdbarch, E_NUM_REGS);
  set_gdbarch_num_pseudo_regs (gdbarch, 0);
  set_gdbarch_sp_regnum (gdbarch, E_SP_REGNUM);
  set_gdbarch_deprecated_fp_regnum (gdbarch, E_FP_REGNUM);
  set_gdbarch_pc_regnum (gdbarch, E_PC_REGNUM);
  set_gdbarch_register_name (gdbarch, v850_register_name);
  set_gdbarch_deprecated_register_size (gdbarch, v850_reg_size);
  set_gdbarch_deprecated_register_bytes (gdbarch, E_ALL_REGS_SIZE);
  set_gdbarch_deprecated_register_byte (gdbarch, v850_register_byte);
  set_gdbarch_deprecated_register_raw_size (gdbarch, v850_register_raw_size);
  set_gdbarch_deprecated_max_register_raw_size (gdbarch, v850_reg_size);
  set_gdbarch_deprecated_register_virtual_size (gdbarch, v850_register_raw_size);
  set_gdbarch_deprecated_max_register_virtual_size (gdbarch, v850_reg_size);
  set_gdbarch_deprecated_register_virtual_type (gdbarch, v850_reg_virtual_type);

  set_gdbarch_deprecated_target_read_fp (gdbarch, v850_target_read_fp);

  /*
   * Frame Info
   */
  set_gdbarch_deprecated_frame_init_saved_regs (gdbarch, v850_frame_init_saved_regs);
  set_gdbarch_deprecated_init_extra_frame_info (gdbarch, v850_init_extra_frame_info);
  set_gdbarch_deprecated_frame_chain (gdbarch, v850_frame_chain);
  set_gdbarch_deprecated_saved_pc_after_call (gdbarch, v850_saved_pc_after_call);
  set_gdbarch_deprecated_frame_saved_pc (gdbarch, v850_frame_saved_pc);
  set_gdbarch_skip_prologue (gdbarch, v850_skip_prologue);

  /* 
   * Miscelany
   */
  /* Stack grows up. */
  set_gdbarch_inner_than (gdbarch, core_addr_lessthan);

  /*
   * Call Dummies
   * 
   * These values and methods are used when gdb calls a target function.  */
  set_gdbarch_deprecated_push_return_address (gdbarch, v850_push_return_address);
  set_gdbarch_deprecated_extract_return_value (gdbarch, v850_extract_return_value);
  set_gdbarch_deprecated_push_arguments (gdbarch, v850_push_arguments);
  set_gdbarch_deprecated_pop_frame (gdbarch, v850_pop_frame);
  set_gdbarch_deprecated_store_struct_return (gdbarch, v850_store_struct_return);
  set_gdbarch_deprecated_store_return_value (gdbarch, v850_store_return_value);
  set_gdbarch_use_struct_convention (gdbarch, v850_use_struct_convention);
  set_gdbarch_deprecated_call_dummy_words (gdbarch, call_dummy_nil);
  set_gdbarch_deprecated_sizeof_call_dummy_words (gdbarch, 0);
  set_gdbarch_deprecated_fix_call_dummy (gdbarch, v850_fix_call_dummy);
  set_gdbarch_breakpoint_from_pc (gdbarch, v850_breakpoint_from_pc);

  set_gdbarch_int_bit (gdbarch, 4 * TARGET_CHAR_BIT);
  set_gdbarch_ptr_bit (gdbarch, 4 * TARGET_CHAR_BIT);
  set_gdbarch_addr_bit (gdbarch, 4 * TARGET_CHAR_BIT);
  set_gdbarch_long_double_bit (gdbarch, 8 * TARGET_CHAR_BIT);

  /* Should be using push_dummy_call.  */
  set_gdbarch_deprecated_dummy_write_sp (gdbarch, deprecated_write_sp);

  set_gdbarch_print_insn (gdbarch, print_insn_v850);

  return gdbarch;
}

extern initialize_file_ftype _initialize_v850_tdep; /* -Wmissing-prototypes */

void
_initialize_v850_tdep (void)
{
  register_gdbarch_init (bfd_arch_v850, v850_gdbarch_init);
}
