/* Print mips instructions for GDB, the GNU debugger, or for objdump.
   Copyright 1989, 1991, 1992, 1993, 1994, 1995, 1996, 1997, 1998, 1999,
   2000, 2001
   Free Software Foundation, Inc.
   Contributed by Nobuyuki Hikichi(hikichi@sra.co.jp).

This file is part of GDB, GAS, and the GNU binutils.

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
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#include "sysdep.h"
#include "dis-asm.h"
#include "opcode/mips.h"
#include "opintl.h"

/* FIXME: These are needed to figure out if the code is mips16 or
   not. The low bit of the address is often a good indicator.  No
   symbol table is available when this code runs out in an embedded
   system as when it is used for disassembler support in a monitor. */

#if !defined(EMBEDDED_ENV)
#define SYMTAB_AVAILABLE 1
#include "elf-bfd.h"
#include "elf/mips.h"
#endif

/* Mips instructions are at maximum this many bytes long.  */
#define INSNLEN 4

static int _print_insn_mips
  PARAMS ((bfd_vma, struct disassemble_info *, enum bfd_endian));
static int print_insn_mips
  PARAMS ((bfd_vma, unsigned long int, struct disassemble_info *));
static void print_insn_arg
  PARAMS ((const char *, unsigned long, bfd_vma, struct disassemble_info *));
static int print_insn_mips16
  PARAMS ((bfd_vma, struct disassemble_info *));
static void print_mips16_insn_arg
  PARAMS ((int, const struct mips_opcode *, int, boolean, int, bfd_vma,
	   struct disassemble_info *));

/* FIXME: These should be shared with gdb somehow.  */

/* The mips16 register names.  */
static const char * const mips16_reg_names[] =
{
  "s0", "s1", "v0", "v1", "a0", "a1", "a2", "a3"
};

static const char * const mips32_reg_names[] =
{
  "zero", "at",	  "v0",	 "v1",	 "a0",	  "a1",	   "a2",   "a3",
  "t0",	  "t1",	  "t2",	 "t3",	 "t4",	  "t5",	   "t6",   "t7",
  "s0",	  "s1",	  "s2",	 "s3",	 "s4",	  "s5",	   "s6",   "s7",
  "t8",	  "t9",	  "k0",	 "k1",	 "gp",	  "sp",	   "s8",   "ra",
  "sr",	  "lo",	  "hi",	 "bad",	 "cause", "pc",
  "fv0",  "$f1",  "fv1", "$f3",  "ft0",   "$f5",   "ft1",  "$f7",
  "ft2",  "$f9",  "ft3", "$f11", "fa0",   "$f13",  "fa1",  "$f15",
  "ft4",  "f17",  "ft5", "f19",  "fs0",   "f21",   "fs1",  "f23",
  "fs2",  "$f25", "fs3", "$f27", "fs4",   "$f29",  "fs5",  "$f31",
  "fsr",  "fir",  "fp",  "inx",  "rand",  "tlblo", "ctxt", "tlbhi",
  "epc",  "prid"
};

static const char * const mips64_reg_names[] =
{
  "zero", "at",	  "v0",	  "v1",	  "a0",	   "a1",    "a2",   "a3",
  "a4",	  "a5",	  "a6",   "a7",	  "t0",	   "t1",    "t2",   "t3",
  "s0",	  "s1",	  "s2",	  "s3",	  "s4",	   "s5",    "s6",   "s7",
  "t8",	  "t9",	  "k0",	  "k1",	  "gp",	   "sp",    "s8",   "ra",
  "sr",	  "lo",	  "hi",	  "bad",  "cause", "pc",
  "fv0",  "$f1",  "fv1",  "$f3",  "ft0",   "ft1",   "ft2",  "ft3",
  "ft4",  "ft5",  "ft6",  "ft7",  "fa0",   "fa1",   "fa2",  "fa3",
  "fa4",  "fa5",  "fa6",  "fa7",  "ft8",   "ft9",   "ft10", "ft11",
  "fs0",  "fs1",  "fs2",  "fs3",  "fs4",   "fs5",   "fs6",  "fs7",
  "fsr",  "fir",  "fp",   "inx",  "rand",  "tlblo", "ctxt", "tlbhi",
  "epc",  "prid"
};

/* Scalar register names. _print_insn_mips() decides which register name
   table to use.  */
static const char * const *reg_names = NULL;

/* Print insn arguments for 32/64-bit code */

static void
print_insn_arg (d, l, pc, info)
     const char *d;
     register unsigned long int l;
     bfd_vma pc;
     struct disassemble_info *info;
{
  int delta;

  switch (*d)
    {
    case ',':
    case '(':
    case ')':
      (*info->fprintf_func) (info->stream, "%c", *d);
      break;

    case 's':
    case 'b':
    case 'r':
    case 'v':
      (*info->fprintf_func) (info->stream, "%s",
			     reg_names[(l >> OP_SH_RS) & OP_MASK_RS]);
      break;

    case 't':
    case 'w':
      (*info->fprintf_func) (info->stream, "%s",
			     reg_names[(l >> OP_SH_RT) & OP_MASK_RT]);
      break;

    case 'i':
    case 'u':
      (*info->fprintf_func) (info->stream, "0x%x",
			(l >> OP_SH_IMMEDIATE) & OP_MASK_IMMEDIATE);
      break;

    case 'j': /* same as i, but sign-extended */
    case 'o':
      delta = (l >> OP_SH_DELTA) & OP_MASK_DELTA;
      if (delta & 0x8000)
	delta |= ~0xffff;
      (*info->fprintf_func) (info->stream, "%d",
			     delta);
      break;

    case 'h':
      (*info->fprintf_func) (info->stream, "0x%x",
			     (unsigned int) ((l >> OP_SH_PREFX)
					     & OP_MASK_PREFX));
      break;

    case 'k':
      (*info->fprintf_func) (info->stream, "0x%x",
			     (unsigned int) ((l >> OP_SH_CACHE)
					     & OP_MASK_CACHE));
      break;

    case 'a':
      (*info->print_address_func)
	((((pc + 4) & ~ (bfd_vma) 0x0fffffff)
	  | (((l >> OP_SH_TARGET) & OP_MASK_TARGET) << 2)),
	 info);
      break;

    case 'p':
      /* sign extend the displacement */
      delta = (l >> OP_SH_DELTA) & OP_MASK_DELTA;
      if (delta & 0x8000)
	delta |= ~0xffff;
      (*info->print_address_func)
	((delta << 2) + pc + INSNLEN,
	 info);
      break;

    case 'd':
      (*info->fprintf_func) (info->stream, "%s",
			     reg_names[(l >> OP_SH_RD) & OP_MASK_RD]);
      break;

    case 'U':
      {
      /* First check for both rd and rt being equal. */
      unsigned int reg = (l >> OP_SH_RD) & OP_MASK_RD;
      if (reg == ((l >> OP_SH_RT) & OP_MASK_RT))
        (*info->fprintf_func) (info->stream, "%s",
                               reg_names[reg]);
      else
        {
          /* If one is zero use the other. */
          if (reg == 0)
            (*info->fprintf_func) (info->stream, "%s",
                                   reg_names[(l >> OP_SH_RT) & OP_MASK_RT]);
          else if (((l >> OP_SH_RT) & OP_MASK_RT) == 0)
            (*info->fprintf_func) (info->stream, "%s",
                                   reg_names[reg]);
          else /* Bogus, result depends on processor. */
            (*info->fprintf_func) (info->stream, "%s or %s",
                                   reg_names[reg],
                                   reg_names[(l >> OP_SH_RT) & OP_MASK_RT]);
          }
      }
      break;

    case 'z':
      (*info->fprintf_func) (info->stream, "%s", reg_names[0]);
      break;

    case '<':
      (*info->fprintf_func) (info->stream, "0x%x",
			     (l >> OP_SH_SHAMT) & OP_MASK_SHAMT);
      break;

    case 'c':
      (*info->fprintf_func) (info->stream, "0x%x",
			     (l >> OP_SH_CODE) & OP_MASK_CODE);
      break;

    case 'q':
      (*info->fprintf_func) (info->stream, "0x%x",
			     (l >> OP_SH_CODE2) & OP_MASK_CODE2);
      break;

    case 'C':
      (*info->fprintf_func) (info->stream, "0x%x",
			     (l >> OP_SH_COPZ) & OP_MASK_COPZ);
      break;

    case 'B':
      (*info->fprintf_func) (info->stream, "0x%x",
			     (l >> OP_SH_CODE20) & OP_MASK_CODE20);
      break;

    case 'J':
      (*info->fprintf_func) (info->stream, "0x%x",
			     (l >> OP_SH_CODE19) & OP_MASK_CODE19);
      break;

    case 'S':
    case 'V':
      (*info->fprintf_func) (info->stream, "$f%d",
			     (l >> OP_SH_FS) & OP_MASK_FS);
      break;

    case 'T':
    case 'W':
      (*info->fprintf_func) (info->stream, "$f%d",
			     (l >> OP_SH_FT) & OP_MASK_FT);
      break;

    case 'D':
      (*info->fprintf_func) (info->stream, "$f%d",
			     (l >> OP_SH_FD) & OP_MASK_FD);
      break;

    case 'R':
      (*info->fprintf_func) (info->stream, "$f%d",
			     (l >> OP_SH_FR) & OP_MASK_FR);
      break;

    case 'E':
      (*info->fprintf_func) (info->stream, "%s",
			     reg_names[(l >> OP_SH_RT) & OP_MASK_RT]);
      break;

    case 'G':
      (*info->fprintf_func) (info->stream, "%s",
			     reg_names[(l >> OP_SH_RD) & OP_MASK_RD]);
      break;

    case 'N':
      (*info->fprintf_func) (info->stream, "$fcc%d",
			     (l >> OP_SH_BCC) & OP_MASK_BCC);
      break;

    case 'M':
      (*info->fprintf_func) (info->stream, "$fcc%d",
			     (l >> OP_SH_CCC) & OP_MASK_CCC);
      break;

    case 'P':
      (*info->fprintf_func) (info->stream, "%d",
			     (l >> OP_SH_PERFREG) & OP_MASK_PERFREG);
      break;

    case 'H':
      (*info->fprintf_func) (info->stream, "%d",
			     (l >> OP_SH_SEL) & OP_MASK_SEL);
      break;

    default:
      /* xgettext:c-format */
      (*info->fprintf_func) (info->stream,
			     _("# internal error, undefined modifier(%c)"),
			     *d);
      break;
    }
}

/* Figure out the MIPS ISA and CPU based on the machine number. */

static void
mips_isa_type (mach, isa, cputype)
     int mach;
     int *isa;
     int *cputype;
{
  switch (mach)
    {
    case bfd_mach_mips3000:
      *cputype = CPU_R3000;
      *isa = ISA_MIPS1;
      break;
    case bfd_mach_mips3900:
      *cputype = CPU_R3900;
      *isa = ISA_MIPS1;
      break;
    case bfd_mach_mips4000:
      *cputype = CPU_R4000;
      *isa = ISA_MIPS3;
      break;
    case bfd_mach_mips4010:
      *cputype = CPU_R4010;
      *isa = ISA_MIPS2;
      break;
    case bfd_mach_mips4100:
      *cputype = CPU_VR4100;
      *isa = ISA_MIPS3;
      break;
    case bfd_mach_mips4111:
      *cputype = CPU_R4111;
      *isa = ISA_MIPS3;
      break;
    case bfd_mach_mips4300:
      *cputype = CPU_R4300;
      *isa = ISA_MIPS3;
      break;
    case bfd_mach_mips4400:
      *cputype = CPU_R4400;
      *isa = ISA_MIPS3;
      break;
    case bfd_mach_mips4600:
      *cputype = CPU_R4600;
      *isa = ISA_MIPS3;
      break;
    case bfd_mach_mips4650:
      *cputype = CPU_R4650;
      *isa = ISA_MIPS3;
      break;
    case bfd_mach_mips5000:
      *cputype = CPU_R5000;
      *isa = ISA_MIPS4;
      break;
    case bfd_mach_mips6000:
      *cputype = CPU_R6000;
      *isa = ISA_MIPS2;
      break;
    case bfd_mach_mips8000:
      *cputype = CPU_R8000;
      *isa = ISA_MIPS4;
      break;
    case bfd_mach_mips10000:
      *cputype = CPU_R10000;
      *isa = ISA_MIPS4;
      break;
    case bfd_mach_mips12000:
      *cputype = CPU_R12000;
      *isa = ISA_MIPS4;
      break;
    case bfd_mach_mips16:
      *cputype = CPU_MIPS16;
      *isa = ISA_MIPS3;
      break;
    case bfd_mach_mips32:
      *cputype = CPU_MIPS32;
      *isa = ISA_MIPS32;
      break;
    case bfd_mach_mips32_4k:
      *cputype = CPU_MIPS32_4K;
      *isa = ISA_MIPS32;
      break;
    case bfd_mach_mips5:
      *cputype = CPU_MIPS5;
      *isa = ISA_MIPS5;
      break;
    case bfd_mach_mips64:
      *cputype = CPU_MIPS64;
      *isa = ISA_MIPS64;
      break;
    case bfd_mach_mips_sb1:
      *cputype = CPU_SB1;
      *isa = ISA_MIPS64;
      break;
    default:
      *cputype = CPU_R3000;
      *isa = ISA_MIPS3;
      break;
    }
}

/* Figure out ISA from disassemble_info data */

static int
get_mips_isa (info)
     struct disassemble_info *info;
{
  int isa;
  int cpu;

  mips_isa_type (info->mach, &isa, &cpu);
  return isa;
}

/* Print the mips instruction at address MEMADDR in debugged memory,
   on using INFO.  Returns length of the instruction, in bytes, which is
   always INSNLEN.  BIGENDIAN must be 1 if this is big-endian code, 0 if
   this is little-endian code.  */

static int
print_insn_mips (memaddr, word, info)
     bfd_vma memaddr;
     unsigned long int word;
     struct disassemble_info *info;
{
  register const struct mips_opcode *op;
  int target_processor, mips_isa;
  static boolean init = 0;
  static const struct mips_opcode *mips_hash[OP_MASK_OP + 1];

  /* Build a hash table to shorten the search time.  */
  if (! init)
    {
      unsigned int i;

      for (i = 0; i <= OP_MASK_OP; i++)
	{
	  for (op = mips_opcodes; op < &mips_opcodes[NUMOPCODES]; op++)
	    {
	      if (op->pinfo == INSN_MACRO)
		continue;
	      if (i == ((op->match >> OP_SH_OP) & OP_MASK_OP))
		{
		  mips_hash[i] = op;
		  break;
		}
	    }
        }

      init = 1;
    }

#if ! SYMTAB_AVAILABLE
  /* This is running out on a target machine, not in a host tool.
     FIXME: Where does mips_target_info come from?  */
  target_processor = mips_target_info.processor;
  mips_isa = mips_target_info.isa;
#else
  mips_isa_type (info->mach, &mips_isa, &target_processor);
#endif

  info->bytes_per_chunk = INSNLEN;
  info->display_endian = info->endian;

  op = mips_hash[(word >> OP_SH_OP) & OP_MASK_OP];
  if (op != NULL)
    {
      for (; op < &mips_opcodes[NUMOPCODES]; op++)
	{
	  if (op->pinfo != INSN_MACRO && (word & op->mask) == op->match)
	    {
	      register const char *d;

	      if (! OPCODE_IS_MEMBER (op, mips_isa, target_processor, 0))
		continue;

	      (*info->fprintf_func) (info->stream, "%s", op->name);

	      d = op->args;
	      if (d != NULL && *d != '\0')
		{
		    (*info->fprintf_func) (info->stream, "\t");
		  for (; *d != '\0'; d++)
		      print_insn_arg (d, word, memaddr, info);
		}

	      return INSNLEN;
	    }
	}
    }

  /* Handle undefined instructions.  */
  (*info->fprintf_func) (info->stream, "0x%x", word);
  return INSNLEN;
}

/* In an environment where we do not know the symbol type of the
   instruction we are forced to assume that the low order bit of the
   instructions' address may mark it as a mips16 instruction.  If we
   are single stepping, or the pc is within the disassembled function,
   this works.  Otherwise, we need a clue.  Sometimes.  */

static int
_print_insn_mips (memaddr, info, endianness)
     bfd_vma memaddr;
     struct disassemble_info *info;
     enum bfd_endian endianness;
{
  bfd_byte buffer[INSNLEN];
  int status;

#if 1
  /* FIXME: If odd address, this is CLEARLY a mips 16 instruction.  */
  /* Only a few tools will work this way.  */
  if (memaddr & 0x01)
    return print_insn_mips16 (memaddr, info);
#endif

#if SYMTAB_AVAILABLE
  if (info->mach == 16
      || (info->flavour == bfd_target_elf_flavour
	  && info->symbols != NULL
	  && ((*(elf_symbol_type **) info->symbols)->internal_elf_sym.st_other
	      == STO_MIPS16)))
    return print_insn_mips16 (memaddr, info);
#endif

  /* Use mips64_reg_names for new ABI.  */
  if (info->flavour == bfd_target_elf_flavour
      && info->symbols != NULL
      && (((get_mips_isa(info) | INSN_ISA_MASK) & ISA_MIPS2) != 0)
      && ((elf_elfheader (bfd_asymbol_bfd(*(info->symbols)))->e_flags
	   & EF_MIPS_ABI2) != 0))
    reg_names = mips64_reg_names;
  else
    reg_names = mips32_reg_names;

  status = (*info->read_memory_func) (memaddr, buffer, INSNLEN, info);
  if (status == 0)
    {
      unsigned long insn;

      if (endianness == BFD_ENDIAN_BIG)
        insn = (unsigned long) bfd_getb32 (buffer);
      else
	insn = (unsigned long) bfd_getl32 (buffer);

      return print_insn_mips (memaddr, insn, info);
    }
  else
    {
      (*info->memory_error_func) (status, memaddr, info);
      return -1;
    }
}

int
print_insn_big_mips (memaddr, info)
     bfd_vma memaddr;
     struct disassemble_info *info;
{
  return _print_insn_mips (memaddr, info, BFD_ENDIAN_BIG);
}

int
print_insn_little_mips (memaddr, info)
     bfd_vma memaddr;
     struct disassemble_info *info;
{
  return _print_insn_mips (memaddr, info, BFD_ENDIAN_LITTLE);
}

/* Disassemble mips16 instructions.  */

static int
print_insn_mips16 (memaddr, info)
     bfd_vma memaddr;
     struct disassemble_info *info;
{
  int status;
  bfd_byte buffer[2];
  int length;
  int insn;
  boolean use_extend;
  int extend = 0;
  const struct mips_opcode *op, *opend;

  info->bytes_per_chunk = 2;
  info->display_endian = info->endian;
  info->insn_info_valid = 1;
  info->branch_delay_insns = 0;
  info->data_size = 0;
  info->insn_type = dis_nonbranch;
  info->target = 0;
  info->target2 = 0;

  status = (*info->read_memory_func) (memaddr, buffer, 2, info);
  if (status != 0)
    {
      (*info->memory_error_func) (status, memaddr, info);
      return -1;
    }

  length = 2;

  if (info->endian == BFD_ENDIAN_BIG)
    insn = bfd_getb16 (buffer);
  else
    insn = bfd_getl16 (buffer);

  /* Handle the extend opcode specially.  */
  use_extend = false;
  if ((insn & 0xf800) == 0xf000)
    {
      use_extend = true;
      extend = insn & 0x7ff;

      memaddr += 2;

      status = (*info->read_memory_func) (memaddr, buffer, 2, info);
      if (status != 0)
	{
	  (*info->fprintf_func) (info->stream, "extend 0x%x",
				 (unsigned int) extend);
	  (*info->memory_error_func) (status, memaddr, info);
	  return -1;
	}

      if (info->endian == BFD_ENDIAN_BIG)
	insn = bfd_getb16 (buffer);
      else
	insn = bfd_getl16 (buffer);

      /* Check for an extend opcode followed by an extend opcode.  */
      if ((insn & 0xf800) == 0xf000)
	{
	  (*info->fprintf_func) (info->stream, "extend 0x%x",
				 (unsigned int) extend);
	  info->insn_type = dis_noninsn;
	  return length;
	}

      length += 2;
    }

  /* FIXME: Should probably use a hash table on the major opcode here.  */

  opend = mips16_opcodes + bfd_mips16_num_opcodes;
  for (op = mips16_opcodes; op < opend; op++)
    {
      if (op->pinfo != INSN_MACRO && (insn & op->mask) == op->match)
	{
	  const char *s;

	  if (strchr (op->args, 'a') != NULL)
	    {
	      if (use_extend)
		{
		  (*info->fprintf_func) (info->stream, "extend 0x%x",
					 (unsigned int) extend);
		  info->insn_type = dis_noninsn;
		  return length - 2;
		}

	      use_extend = false;

	      memaddr += 2;

	      status = (*info->read_memory_func) (memaddr, buffer, 2,
						  info);
	      if (status == 0)
		{
		  use_extend = true;
		  if (info->endian == BFD_ENDIAN_BIG)
		    extend = bfd_getb16 (buffer);
		  else
		    extend = bfd_getl16 (buffer);
		  length += 2;
		}
	    }

	  (*info->fprintf_func) (info->stream, "%s", op->name);
	  if (op->args[0] != '\0')
	    (*info->fprintf_func) (info->stream, "\t");

	  for (s = op->args; *s != '\0'; s++)
	    {
	      if (*s == ','
		  && s[1] == 'w'
		  && (((insn >> MIPS16OP_SH_RX) & MIPS16OP_MASK_RX)
		      == ((insn >> MIPS16OP_SH_RY) & MIPS16OP_MASK_RY)))
		{
		  /* Skip the register and the comma.  */
		  ++s;
		  continue;
		}
	      if (*s == ','
		  && s[1] == 'v'
		  && (((insn >> MIPS16OP_SH_RZ) & MIPS16OP_MASK_RZ)
		      == ((insn >> MIPS16OP_SH_RX) & MIPS16OP_MASK_RX)))
		{
		  /* Skip the register and the comma.  */
		  ++s;
		  continue;
		}
	      print_mips16_insn_arg (*s, op, insn, use_extend, extend, memaddr,
				     info);
	    }

	  if ((op->pinfo & INSN_UNCOND_BRANCH_DELAY) != 0)
	    {
	      info->branch_delay_insns = 1;
	      if (info->insn_type != dis_jsr)
		info->insn_type = dis_branch;
	    }

	  return length;
	}
    }

  if (use_extend)
    (*info->fprintf_func) (info->stream, "0x%x", extend | 0xf000);
  (*info->fprintf_func) (info->stream, "0x%x", insn);
  info->insn_type = dis_noninsn;

  return length;
}

/* Disassemble an operand for a mips16 instruction.  */

static void
print_mips16_insn_arg (type, op, l, use_extend, extend, memaddr, info)
     char type;
     const struct mips_opcode *op;
     int l;
     boolean use_extend;
     int extend;
     bfd_vma memaddr;
     struct disassemble_info *info;
{
  switch (type)
    {
    case ',':
    case '(':
    case ')':
      (*info->fprintf_func) (info->stream, "%c", type);
      break;

    case 'y':
    case 'w':
      (*info->fprintf_func) (info->stream, "%s",
			     mips16_reg_names[((l >> MIPS16OP_SH_RY)
					       & MIPS16OP_MASK_RY)]);
      break;

    case 'x':
    case 'v':
      (*info->fprintf_func) (info->stream, "%s",
			     mips16_reg_names[((l >> MIPS16OP_SH_RX)
					       & MIPS16OP_MASK_RX)]);
      break;

    case 'z':
      (*info->fprintf_func) (info->stream, "%s",
			     mips16_reg_names[((l >> MIPS16OP_SH_RZ)
					       & MIPS16OP_MASK_RZ)]);
      break;

    case 'Z':
      (*info->fprintf_func) (info->stream, "%s",
			     mips16_reg_names[((l >> MIPS16OP_SH_MOVE32Z)
					       & MIPS16OP_MASK_MOVE32Z)]);
      break;

    case '0':
      (*info->fprintf_func) (info->stream, "%s", mips32_reg_names[0]);
      break;

    case 'S':
      (*info->fprintf_func) (info->stream, "%s", mips32_reg_names[29]);
      break;

    case 'P':
      (*info->fprintf_func) (info->stream, "$pc");
      break;

    case 'R':
      (*info->fprintf_func) (info->stream, "%s", mips32_reg_names[31]);
      break;

    case 'X':
      (*info->fprintf_func) (info->stream, "%s",
			     mips32_reg_names[((l >> MIPS16OP_SH_REGR32)
					& MIPS16OP_MASK_REGR32)]);
      break;

    case 'Y':
      (*info->fprintf_func) (info->stream, "%s",
			     mips32_reg_names[MIPS16OP_EXTRACT_REG32R (l)]);
      break;

    case '<':
    case '>':
    case '[':
    case ']':
    case '4':
    case '5':
    case 'H':
    case 'W':
    case 'D':
    case 'j':
    case '6':
    case '8':
    case 'V':
    case 'C':
    case 'U':
    case 'k':
    case 'K':
    case 'p':
    case 'q':
    case 'A':
    case 'B':
    case 'E':
      {
	int immed, nbits, shift, signedp, extbits, pcrel, extu, branch;

	shift = 0;
	signedp = 0;
	extbits = 16;
	pcrel = 0;
	extu = 0;
	branch = 0;
	switch (type)
	  {
	  case '<':
	    nbits = 3;
	    immed = (l >> MIPS16OP_SH_RZ) & MIPS16OP_MASK_RZ;
	    extbits = 5;
	    extu = 1;
	    break;
	  case '>':
	    nbits = 3;
	    immed = (l >> MIPS16OP_SH_RX) & MIPS16OP_MASK_RX;
	    extbits = 5;
	    extu = 1;
	    break;
	  case '[':
	    nbits = 3;
	    immed = (l >> MIPS16OP_SH_RZ) & MIPS16OP_MASK_RZ;
	    extbits = 6;
	    extu = 1;
	    break;
	  case ']':
	    nbits = 3;
	    immed = (l >> MIPS16OP_SH_RX) & MIPS16OP_MASK_RX;
	    extbits = 6;
	    extu = 1;
	    break;
	  case '4':
	    nbits = 4;
	    immed = (l >> MIPS16OP_SH_IMM4) & MIPS16OP_MASK_IMM4;
	    signedp = 1;
	    extbits = 15;
	    break;
	  case '5':
	    nbits = 5;
	    immed = (l >> MIPS16OP_SH_IMM5) & MIPS16OP_MASK_IMM5;
	    info->insn_type = dis_dref;
	    info->data_size = 1;
	    break;
	  case 'H':
	    nbits = 5;
	    shift = 1;
	    immed = (l >> MIPS16OP_SH_IMM5) & MIPS16OP_MASK_IMM5;
	    info->insn_type = dis_dref;
	    info->data_size = 2;
	    break;
	  case 'W':
	    nbits = 5;
	    shift = 2;
	    immed = (l >> MIPS16OP_SH_IMM5) & MIPS16OP_MASK_IMM5;
	    if ((op->pinfo & MIPS16_INSN_READ_PC) == 0
		&& (op->pinfo & MIPS16_INSN_READ_SP) == 0)
	      {
		info->insn_type = dis_dref;
		info->data_size = 4;
	      }
	    break;
	  case 'D':
	    nbits = 5;
	    shift = 3;
	    immed = (l >> MIPS16OP_SH_IMM5) & MIPS16OP_MASK_IMM5;
	    info->insn_type = dis_dref;
	    info->data_size = 8;
	    break;
	  case 'j':
	    nbits = 5;
	    immed = (l >> MIPS16OP_SH_IMM5) & MIPS16OP_MASK_IMM5;
	    signedp = 1;
	    break;
	  case '6':
	    nbits = 6;
	    immed = (l >> MIPS16OP_SH_IMM6) & MIPS16OP_MASK_IMM6;
	    break;
	  case '8':
	    nbits = 8;
	    immed = (l >> MIPS16OP_SH_IMM8) & MIPS16OP_MASK_IMM8;
	    break;
	  case 'V':
	    nbits = 8;
	    shift = 2;
	    immed = (l >> MIPS16OP_SH_IMM8) & MIPS16OP_MASK_IMM8;
	    /* FIXME: This might be lw, or it might be addiu to $sp or
               $pc.  We assume it's load.  */
	    info->insn_type = dis_dref;
	    info->data_size = 4;
	    break;
	  case 'C':
	    nbits = 8;
	    shift = 3;
	    immed = (l >> MIPS16OP_SH_IMM8) & MIPS16OP_MASK_IMM8;
	    info->insn_type = dis_dref;
	    info->data_size = 8;
	    break;
	  case 'U':
	    nbits = 8;
	    immed = (l >> MIPS16OP_SH_IMM8) & MIPS16OP_MASK_IMM8;
	    extu = 1;
	    break;
	  case 'k':
	    nbits = 8;
	    immed = (l >> MIPS16OP_SH_IMM8) & MIPS16OP_MASK_IMM8;
	    signedp = 1;
	    break;
	  case 'K':
	    nbits = 8;
	    shift = 3;
	    immed = (l >> MIPS16OP_SH_IMM8) & MIPS16OP_MASK_IMM8;
	    signedp = 1;
	    break;
	  case 'p':
	    nbits = 8;
	    immed = (l >> MIPS16OP_SH_IMM8) & MIPS16OP_MASK_IMM8;
	    signedp = 1;
	    pcrel = 1;
	    branch = 1;
	    info->insn_type = dis_condbranch;
	    break;
	  case 'q':
	    nbits = 11;
	    immed = (l >> MIPS16OP_SH_IMM11) & MIPS16OP_MASK_IMM11;
	    signedp = 1;
	    pcrel = 1;
	    branch = 1;
	    info->insn_type = dis_branch;
	    break;
	  case 'A':
	    nbits = 8;
	    shift = 2;
	    immed = (l >> MIPS16OP_SH_IMM8) & MIPS16OP_MASK_IMM8;
	    pcrel = 1;
	    /* FIXME: This can be lw or la.  We assume it is lw.  */
	    info->insn_type = dis_dref;
	    info->data_size = 4;
	    break;
	  case 'B':
	    nbits = 5;
	    shift = 3;
	    immed = (l >> MIPS16OP_SH_IMM5) & MIPS16OP_MASK_IMM5;
	    pcrel = 1;
	    info->insn_type = dis_dref;
	    info->data_size = 8;
	    break;
	  case 'E':
	    nbits = 5;
	    shift = 2;
	    immed = (l >> MIPS16OP_SH_IMM5) & MIPS16OP_MASK_IMM5;
	    pcrel = 1;
	    break;
	  default:
	    abort ();
	  }

	if (! use_extend)
	  {
	    if (signedp && immed >= (1 << (nbits - 1)))
	      immed -= 1 << nbits;
	    immed <<= shift;
	    if ((type == '<' || type == '>' || type == '[' || type == ']')
		&& immed == 0)
	      immed = 8;
	  }
	else
	  {
	    if (extbits == 16)
	      immed |= ((extend & 0x1f) << 11) | (extend & 0x7e0);
	    else if (extbits == 15)
	      immed |= ((extend & 0xf) << 11) | (extend & 0x7f0);
	    else
	      immed = ((extend >> 6) & 0x1f) | (extend & 0x20);
	    immed &= (1 << extbits) - 1;
	    if (! extu && immed >= (1 << (extbits - 1)))
	      immed -= 1 << extbits;
	  }

	if (! pcrel)
	  (*info->fprintf_func) (info->stream, "%d", immed);
	else
	  {
	    bfd_vma baseaddr;
	    bfd_vma val;

	    if (branch)
	      {
		immed *= 2;
		baseaddr = memaddr + 2;
	      }
	    else if (use_extend)
	      baseaddr = memaddr - 2;
	    else
	      {
		int status;
		bfd_byte buffer[2];

		baseaddr = memaddr;

		/* If this instruction is in the delay slot of a jr
                   instruction, the base address is the address of the
                   jr instruction.  If it is in the delay slot of jalr
                   instruction, the base address is the address of the
                   jalr instruction.  This test is unreliable: we have
                   no way of knowing whether the previous word is
                   instruction or data.  */
		status = (*info->read_memory_func) (memaddr - 4, buffer, 2,
						    info);
		if (status == 0
		    && (((info->endian == BFD_ENDIAN_BIG
			  ? bfd_getb16 (buffer)
			  : bfd_getl16 (buffer))
			 & 0xf800) == 0x1800))
		  baseaddr = memaddr - 4;
		else
		  {
		    status = (*info->read_memory_func) (memaddr - 2, buffer,
							2, info);
		    if (status == 0
			&& (((info->endian == BFD_ENDIAN_BIG
			      ? bfd_getb16 (buffer)
			      : bfd_getl16 (buffer))
			     & 0xf81f) == 0xe800))
		      baseaddr = memaddr - 2;
		  }
	      }
	    val = (baseaddr & ~ ((1 << shift) - 1)) + immed;
	    (*info->print_address_func) (val, info);
	    info->target = val;
	  }
      }
      break;

    case 'a':
      if (! use_extend)
	extend = 0;
      l = ((l & 0x1f) << 23) | ((l & 0x3e0) << 13) | (extend << 2);
      (*info->print_address_func) (((memaddr + 4) & 0xf0000000) | l, info);
      info->insn_type = dis_jsr;
      info->target = ((memaddr + 4) & 0xf0000000) | l;
      info->branch_delay_insns = 1;
      break;

    case 'l':
    case 'L':
      {
	int need_comma, amask, smask;

	need_comma = 0;

	l = (l >> MIPS16OP_SH_IMM6) & MIPS16OP_MASK_IMM6;

	amask = (l >> 3) & 7;

	if (amask > 0 && amask < 5)
	  {
	    (*info->fprintf_func) (info->stream, "%s", mips32_reg_names[4]);
	    if (amask > 1)
	      (*info->fprintf_func) (info->stream, "-%s",
				     mips32_reg_names[amask + 3]);
	    need_comma = 1;
	  }

	smask = (l >> 1) & 3;
	if (smask == 3)
	  {
	    (*info->fprintf_func) (info->stream, "%s??",
				   need_comma ? "," : "");
	    need_comma = 1;
	  }
	else if (smask > 0)
	  {
	    (*info->fprintf_func) (info->stream, "%s%s",
				   need_comma ? "," : "",
				   mips32_reg_names[16]);
	    if (smask > 1)
	      (*info->fprintf_func) (info->stream, "-%s",
				     mips32_reg_names[smask + 15]);
	    need_comma = 1;
	  }

	if (l & 1)
	  {
	    (*info->fprintf_func) (info->stream, "%s%s",
				   need_comma ? "," : "",
				   mips32_reg_names[31]);
	    need_comma = 1;
	  }

	if (amask == 5 || amask == 6)
	  {
	    (*info->fprintf_func) (info->stream, "%s$f0",
				   need_comma ? "," : "");
	    if (amask == 6)
	      (*info->fprintf_func) (info->stream, "-$f1");
	  }
      }
      break;

    default:
      /* xgettext:c-format */
      (*info->fprintf_func)
	(info->stream,
	 _("# internal disassembler error, unrecognised modifier (%c)"),
	 type);
      abort ();
    }
}
