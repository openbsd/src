/* Disassembler for the PA-RISC. Somewhat derived from sparc-pinsn.c.
   Copyright 1989, 1990, 1992, 1993 Free Software Foundation, Inc.

   Contributed by the Center for Software Science at the
   University of Utah (pa-gdb-bugs@cs.utah.edu).

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

#include <ansidecl.h>
#include "sysdep.h"
#include "dis-asm.h"
#include "libhppa.h"
#include "opcode/hppa.h"

/* Integer register names, indexed by the numbers which appear in the
   opcodes.  */
static const char *const reg_names[] = 
 {"flags", "r1", "rp", "r3", "r4", "r5", "r6", "r7", "r8", "r9",
  "r10", "r11", "r12", "r13", "r14", "r15", "r16", "r17", "r18", "r19",
  "r20", "r21", "r22", "r23", "r24", "r25", "r26", "dp", "ret0", "ret1",
  "sp", "r31"};

/* Floating point register names, indexed by the numbers which appear in the
   opcodes.  */
static const char *const fp_reg_names[] = 
 {"fpsr", "fpe2", "fpe4", "fpe6", 
  "fr4", "fr5", "fr6", "fr7", "fr8", 
  "fr9", "fr10", "fr11", "fr12", "fr13", "fr14", "fr15", 
  "fr16", "fr17", "fr18", "fr19", "fr20", "fr21", "fr22", "fr23",
  "fr24", "fr25", "fr26", "fr27", "fr28", "fr29", "fr30", "fr31"};

typedef unsigned int CORE_ADDR;

/* Get at various relevent fields of an instruction word. */

#define MASK_5 0x1f
#define MASK_11 0x7ff
#define MASK_14 0x3fff
#define MASK_21 0x1fffff

/* This macro gets bit fields using HP's numbering (MSB = 0) */

#define GET_FIELD(X, FROM, TO) \
  ((X) >> (31 - (TO)) & ((1 << ((TO) - (FROM) + 1)) - 1))

/* Some of these have been converted to 2-d arrays because they
   consume less storage this way.  If the maintenance becomes a
   problem, convert them back to const 1-d pointer arrays.  */
static const char control_reg[][6] = {
  "rctr", "cr1", "cr2", "cr3", "cr4", "cr5", "cr6", "cr7",
  "pidr1", "pidr2", "ccr", "sar", "pidr3", "pidr4",
  "iva", "eiem", "itmr", "pcsq", "pcoq", "iir", "isr",
  "ior", "ipsw", "eirr", "tr0", "tr1", "tr2", "tr3",
  "tr4", "tr5", "tr6", "tr7"
};

static const char compare_cond_names[][5] = {
  "", ",=", ",<", ",<=", ",<<", ",<<=", ",sv",
  ",od", ",tr", ",<>", ",>=", ",>", ",>>=",
  ",>>", ",nsv", ",ev"
};
static const char add_cond_names[][5] = {
  "", ",=", ",<", ",<=", ",nuv", ",znv", ",sv",
  ",od", ",tr", ",<>", ",>=", ",>", ",uv",
  ",vnz", ",nsv", ",ev"
};
static const char *const logical_cond_names[] = {
  "", ",=", ",<", ",<=", 0, 0, 0, ",od",
  ",tr", ",<>", ",>=", ",>", 0, 0, 0, ",ev"};
static const char *const unit_cond_names[] = {
  "", 0, ",sbz", ",shz", ",sdc", 0, ",sbc", ",shc",
  ",tr", 0, ",nbz", ",nhz", ",ndc", 0, ",nbc", ",nhc"
};
static const char shift_cond_names[][4] = {
  "", ",=", ",<", ",od", ",tr", ",<>", ",>=", ",ev"
};
static const char index_compl_names[][4] = {"", ",m", ",s", ",sm"};
static const char short_ldst_compl_names[][4] = {"", ",ma", "", ",mb"};
static const char *const short_bytes_compl_names[] = {
  "", ",b,m", ",e", ",e,m"
};
static const char *const float_format_names[] = {",sgl", ",dbl", "", ",quad"};
static const char float_comp_names[][8] =
{
  ",false?", ",false", ",?", ",!<=>", ",=", ",=t", ",?=", ",!<>",
  ",!?>=", ",<", ",?<", ",!>=", ",!?>", ",<=", ",?<=", ",!>",
  ",!?<=", ",>", ",?>", ",!<=", ",!?<", ",>=", ",?>=", ",!<",
  ",!?=", ",<>", ",!=", ",!=t", ",!?", ",<=>", ",true?", ",true"
};

/* For a bunch of different instructions form an index into a 
   completer name table. */
#define GET_COMPL(insn) (GET_FIELD (insn, 26, 26) | \
			 GET_FIELD (insn, 18, 18) << 1)

#define GET_COND(insn) (GET_FIELD ((insn), 16, 18) + \
			(GET_FIELD ((insn), 19, 19) ? 8 : 0))

/* Utility function to print registers.  Put these first, so gcc's function
   inlining can do its stuff.  */

#define fputs_filtered(STR,F)	(*info->fprintf_func) (info->stream, "%s", STR)

static void
fput_reg (reg, info)
     unsigned reg;
     disassemble_info *info;
{
  (*info->fprintf_func) (info->stream, reg ? reg_names[reg] : "r0");
}

static void
fput_fp_reg (reg, info)
     unsigned reg;
     disassemble_info *info;
{
  (*info->fprintf_func) (info->stream, reg ? fp_reg_names[reg] : "fr0");
}

static void
fput_fp_reg_r (reg, info)
     unsigned reg;
     disassemble_info *info;
{
  /* Special case floating point exception registers.  */
  if (reg < 4)
    (*info->fprintf_func) (info->stream, "fpe%d", reg * 2 + 1);
  else
    (*info->fprintf_func) (info->stream, "%sR", reg ? fp_reg_names[reg] 
						    : "fr0");
}

static void
fput_creg (reg, info)
     unsigned reg;
     disassemble_info *info;
{
  (*info->fprintf_func) (info->stream, control_reg[reg]);
}

/* print constants with sign */

static void
fput_const (num, info)
     unsigned num;
     disassemble_info *info;
{
  if ((int)num < 0)
    (*info->fprintf_func) (info->stream, "-%x", -(int)num);
  else
    (*info->fprintf_func) (info->stream, "%x", num);
}

/* Routines to extract various sized constants out of hppa
   instructions. */

/* extract a 3-bit space register number from a be, ble, mtsp or mfsp */
static int
extract_3 (word)
     unsigned word;
{
  return GET_FIELD (word, 18, 18) << 2 | GET_FIELD (word, 16, 17);
}

static int
extract_5_load (word)
     unsigned word;
{
  return low_sign_extend (word >> 16 & MASK_5, 5);
}

/* extract the immediate field from a st{bhw}s instruction */
static int
extract_5_store (word)
     unsigned word;
{
  return low_sign_extend (word & MASK_5, 5);
}

/* extract the immediate field from a break instruction */
static unsigned
extract_5r_store (word)
     unsigned word;
{
  return (word & MASK_5);
}

/* extract the immediate field from a {sr}sm instruction */
static unsigned
extract_5R_store (word)
     unsigned word;
{
  return (word >> 16 & MASK_5);
}

/* extract the immediate field from a bb instruction */
static unsigned
extract_5Q_store (word)
     unsigned word;
{
  return (word >> 21 & MASK_5);
}

/* extract an 11 bit immediate field */
static int
extract_11 (word)
     unsigned word;
{
  return low_sign_extend (word & MASK_11, 11);
}

/* extract a 14 bit immediate field */
static int
extract_14 (word)
     unsigned word;
{
  return low_sign_extend (word & MASK_14, 14);
}

/* extract a 21 bit constant */

static int
extract_21 (word)
     unsigned word;
{
  int val;

  word &= MASK_21;
  word <<= 11;
  val = GET_FIELD (word, 20, 20);
  val <<= 11;
  val |= GET_FIELD (word, 9, 19);
  val <<= 2;
  val |= GET_FIELD (word, 5, 6);
  val <<= 5;
  val |= GET_FIELD (word, 0, 4);
  val <<= 2;
  val |= GET_FIELD (word, 7, 8);
  return sign_extend (val, 21) << 11;
}

/* extract a 12 bit constant from branch instructions */

static int
extract_12 (word)
     unsigned word;
{
  return sign_extend (GET_FIELD (word, 19, 28) |
                      GET_FIELD (word, 29, 29) << 10 |
                      (word & 0x1) << 11, 12) << 2;
}

/* extract a 17 bit constant from branch instructions, returning the
   19 bit signed value. */

static int
extract_17 (word)
     unsigned word;
{
  return sign_extend (GET_FIELD (word, 19, 28) |
                      GET_FIELD (word, 29, 29) << 10 |
                      GET_FIELD (word, 11, 15) << 11 |
                      (word & 0x1) << 16, 17) << 2;
}

/* Print one instruction.  */
int
print_insn_hppa (memaddr, info)
     bfd_vma memaddr;
     disassemble_info *info;
{
  bfd_byte buffer[4];
  unsigned int insn, i;

  {
    int status =
      (*info->read_memory_func) (memaddr, buffer, sizeof (buffer), info);
    if (status != 0)
      {
	(*info->memory_error_func) (status, memaddr, info);
	return -1;
      }
  }

  insn = bfd_getb32 (buffer);

  for (i = 0; i < NUMOPCODES; ++i)
    {
      const struct pa_opcode *opcode = &pa_opcodes[i];
      if ((insn & opcode->mask) == opcode->match)
	{
	  register const char *s;
	  
	  (*info->fprintf_func) (info->stream, "%s", opcode->name);

	  if (!strchr ("cfCY<?!@-+&U>~nHNZFIMadu|", opcode->args[0]))
	    (*info->fprintf_func) (info->stream, " ");
	  for (s = opcode->args; *s != '\0'; ++s)
	    {
	      switch (*s)
		{
		case 'x':
		  fput_reg (GET_FIELD (insn, 11, 15), info);
		  break;
		case 'X':
                  if (GET_FIELD (insn, 25, 25))
		      fput_fp_reg_r (GET_FIELD (insn, 11, 15), info);
		  else
		      fput_fp_reg (GET_FIELD (insn, 11, 15), info);
		  break;
		case 'b':
		  fput_reg (GET_FIELD (insn, 6, 10), info);
		  break;
		case '^':
		  fput_creg (GET_FIELD (insn, 6, 10), info);
		  break;
		case 'E':
                  if (GET_FIELD (insn, 25, 25))
		      fput_fp_reg_r (GET_FIELD (insn, 6, 10), info);
		  else
		      fput_fp_reg (GET_FIELD (insn, 6, 10), info);
		  break;
		case 't':
		  fput_reg (GET_FIELD (insn, 27, 31), info);
		  break;
		case 'v':
                  if (GET_FIELD (insn, 25, 25))
		      fput_fp_reg_r (GET_FIELD (insn, 27, 31), info);
		  else
		      fput_fp_reg (GET_FIELD (insn, 27, 31), info);
		  break;
		case 'y':
		  fput_fp_reg (GET_FIELD (insn, 27, 31), info);
		  break;
		case '4':
		  {
		    int reg = GET_FIELD (insn, 6, 10);

		    reg |= (GET_FIELD (insn, 26, 26) << 4);
		    fput_fp_reg (reg, info);
		    break;
		  }
		case '6':
		  {
		    int reg = GET_FIELD (insn, 11, 15);

		    reg |= (GET_FIELD (insn, 26, 26) << 4);
		    fput_fp_reg (reg, info);
		    break;
		  }
		case '7':
		  {
		    int reg = GET_FIELD (insn, 27, 31);

		    reg |= (GET_FIELD (insn, 26, 26) << 4);
		    fput_fp_reg (reg, info);
		    break;
		  }
		case '8':
		  {
		    int reg = GET_FIELD (insn, 16, 20);

		    reg |= (GET_FIELD (insn, 26, 26) << 4);
		    fput_fp_reg (reg, info);
		    break;
		  }
		case '9':
		  {
		    int reg = GET_FIELD (insn, 21, 25);

		    reg |= (GET_FIELD (insn, 26, 26) << 4);
		    fput_fp_reg (reg, info);
		    break;
		  }
		case '5':
		  fput_const (extract_5_load (insn), info);
		  break;
		case 's':
		  (*info->fprintf_func) (info->stream,
					 "sr%d", GET_FIELD (insn, 16, 17));
		  break;
		case 'S':
		  (*info->fprintf_func) (info->stream, "sr%d", extract_3 (insn));
		  break;
		case 'c':
		  (*info->fprintf_func) (info->stream, "%s ",
				    index_compl_names[GET_COMPL (insn)]);
		  break;
		case 'C':
		  (*info->fprintf_func) (info->stream, "%s ",
				    short_ldst_compl_names[GET_COMPL (insn)]);
		  break;
		case 'Y':
		  (*info->fprintf_func) (info->stream, "%s ",
				    short_bytes_compl_names[GET_COMPL (insn)]);
		  break;
		/* these four conditions are for the set of instructions
		   which distinguish true/false conditions by opcode rather
		   than by the 'f' bit (sigh): comb, comib, addb, addib */
		case '<':
		  fputs_filtered (compare_cond_names[GET_FIELD (insn, 16, 18)],
				  info);
		  break;
		case '?':
		  fputs_filtered (compare_cond_names[GET_FIELD (insn, 16, 18)
				  + GET_FIELD (insn, 4, 4) * 8], info);
		  break;
		case '@':
		  fputs_filtered (add_cond_names[GET_FIELD (insn, 16, 18)
				  + GET_FIELD (insn, 4, 4) * 8], info);
		  break;
		case 'a':
		  (*info->fprintf_func) (info->stream, "%s ",
					 compare_cond_names[GET_COND (insn)]);
		  break;
		case 'd':
		  (*info->fprintf_func) (info->stream, "%s ",
					 add_cond_names[GET_COND (insn)]);
		  break;
		case '!':
		  (*info->fprintf_func) (info->stream, "%s",
				    add_cond_names[GET_FIELD (insn, 16, 18)]);
		  break;

		case '&':
		  (*info->fprintf_func) (info->stream, "%s ",
				    logical_cond_names[GET_COND (insn)]);
		  break;
		case 'U':
		  (*info->fprintf_func) (info->stream, "%s ",
				    unit_cond_names[GET_COND (insn)]);
		  break;
		case '|':
		case '>':
		case '~':
		  (*info->fprintf_func)
		    (info->stream, "%s",
		     shift_cond_names[GET_FIELD (insn, 16, 18)]);

		  /* If the next character in args is 'n', it will handle
		     putting out the space.  */
		  if (s[1] != 'n')
		    (*info->fprintf_func) (info->stream, " ");
		  break;
		case 'V':
		  fput_const (extract_5_store (insn), info);
		  break;
		case 'r':
		  fput_const (extract_5r_store (insn), info);
		  break;
		case 'R':
		  fput_const (extract_5R_store (insn), info);
		  break;
		case 'Q':
		  fput_const (extract_5Q_store (insn), info);
		  break;
		case 'i':
		  fput_const (extract_11 (insn), info);
		  break;
		case 'j':
		  fput_const (extract_14 (insn), info);
		  break;
		case 'k':
		  fput_const (extract_21 (insn), info);
		  break;
		case 'n':
		  if (insn & 0x2)
		    (*info->fprintf_func) (info->stream, ",n ");
		  else
		    (*info->fprintf_func) (info->stream, " ");
		  break;
		case 'N':
		  if ((insn & 0x20) && s[1])
		    (*info->fprintf_func) (info->stream, ",n ");
		  else if (insn & 0x20)
		    (*info->fprintf_func) (info->stream, ",n");
		  else if (s[1])
		    (*info->fprintf_func) (info->stream, " ");
		  break;
		case 'w':
		  (*info->print_address_func) (memaddr + 8 + extract_12 (insn),
					       info);
		  break;
		case 'W':
		  /* 17 bit PC-relative branch.  */
		  (*info->print_address_func) ((memaddr + 8 
						+ extract_17 (insn)),
					       info);
		  break;
		case 'z':
		  /* 17 bit displacement.  This is an offset from a register
		     so it gets disasssembled as just a number, not any sort
		     of address.  */
		  fput_const (extract_17 (insn), info);
		  break;
		case 'p':
		  (*info->fprintf_func) (info->stream, "%d",
				    31 - GET_FIELD (insn, 22, 26));
		  break;
		case 'P':
		  (*info->fprintf_func) (info->stream, "%d",
				    GET_FIELD (insn, 22, 26));
		  break;
		case 'T':
		  (*info->fprintf_func) (info->stream, "%d",
				    32 - GET_FIELD (insn, 27, 31));
		  break;
		case 'A':
		  fput_const (GET_FIELD (insn, 6, 18), info);
		  break;
		case 'Z':
		  if (GET_FIELD (insn, 26, 26))
		    (*info->fprintf_func) (info->stream, ",m ");
		  else
		    (*info->fprintf_func) (info->stream, " ");
		  break;
		case 'D':
		  fput_const (GET_FIELD (insn, 6, 31), info);
		  break;
		case 'f':
		  (*info->fprintf_func) (info->stream, ",%d", GET_FIELD (insn, 23, 25));
		  break;
		case 'O':
		  fput_const ((GET_FIELD (insn, 6,20) << 5 |
			       GET_FIELD (insn, 27, 31)), info);
		  break;
		case 'o':
		  fput_const (GET_FIELD (insn, 6, 20), info);
		  break;
		case '2':
		  fput_const ((GET_FIELD (insn, 6, 22) << 5 |
			       GET_FIELD (insn, 27, 31)), info);
		  break;
		case '1':
		  fput_const ((GET_FIELD (insn, 11, 20) << 5 |
			       GET_FIELD (insn, 27, 31)), info);
		  break;
		case '0':
		  fput_const ((GET_FIELD (insn, 16, 20) << 5 |
			       GET_FIELD (insn, 27, 31)), info);
		  break;
		case 'u':
		  (*info->fprintf_func) (info->stream, ",%d", GET_FIELD (insn, 23, 25));
		  break;
		case 'F':
		  /* if no destination completer and not before a completer
		     for fcmp, need a space here */
		  if (GET_FIELD (insn, 21, 22) == 1 || s[1] == 'M')
		    fputs_filtered (float_format_names[GET_FIELD (insn, 19, 20)],
				    info);
		  else
		    (*info->fprintf_func) (info->stream, "%s ",
					   float_format_names[GET_FIELD
							      (insn, 19, 20)]);
		  break;
		case 'G':
		  (*info->fprintf_func) (info->stream, "%s ",
				    float_format_names[GET_FIELD (insn,
								  17, 18)]);
		  break;
		case 'H':
		  if (GET_FIELD (insn, 26, 26) == 1)
		    (*info->fprintf_func) (info->stream, "%s ",
				    float_format_names[0]);
		  else
		    (*info->fprintf_func) (info->stream, "%s ",
				    float_format_names[1]);
		  break;
		case 'I':
		  /* if no destination completer and not before a completer
		     for fcmp, need a space here */
		  if (GET_FIELD (insn, 21, 22) == 1 || s[1] == 'M')
		    fputs_filtered (float_format_names[GET_FIELD (insn, 20, 20)],
				    info);
		  else
		    (*info->fprintf_func) (info->stream, "%s ",
					   float_format_names[GET_FIELD
							      (insn, 20, 20)]);
		  break;
		case 'J':
                  if (GET_FIELD (insn, 24, 24))
		      fput_fp_reg_r (GET_FIELD (insn, 6, 10), info);
		  else
		      fput_fp_reg (GET_FIELD (insn, 6, 10), info);
		      
		  break;
		case 'K':
                  if (GET_FIELD (insn, 19, 19))
		      fput_fp_reg_r (GET_FIELD (insn, 11, 15), info);
		  else
		      fput_fp_reg (GET_FIELD (insn, 11, 15), info);
		  break;
		case 'M':
		    (*info->fprintf_func) (info->stream, "%s ",
					   float_comp_names[GET_FIELD
							      (insn, 27, 31)]);
		  break;
		default:
		  (*info->fprintf_func) (info->stream, "%c", *s);
		  break;
		}
	    }
	  return sizeof(insn);
	}
    }
  (*info->fprintf_func) (info->stream, "#%8x", insn);
  return sizeof(insn);
}
