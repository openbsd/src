/* tc-h8300.c -- Assemble code for the Renesas H8/300
   Copyright 1991, 1992, 1993, 1994, 1995, 1996, 1997, 1998, 2000,
   2001, 2002, 2003 Free Software Foundation, Inc.

   This file is part of GAS, the GNU Assembler.

   GAS is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   GAS is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GAS; see the file COPYING.  If not, write to the Free
   Software Foundation, 59 Temple Place - Suite 330, Boston, MA
   02111-1307, USA.  */

/* Written By Steve Chamberlain <sac@cygnus.com>.  */

#include <stdio.h>
#include "as.h"
#include "subsegs.h"
#include "bfd.h"

#ifdef BFD_ASSEMBLER
#include "dwarf2dbg.h"
#endif

#define DEFINE_TABLE
#define h8_opcodes ops
#include "opcode/h8300.h"
#include "safe-ctype.h"

#ifdef OBJ_ELF
#include "elf/h8.h"
#endif

const char comment_chars[] = ";";
const char line_comment_chars[] = "#";
const char line_separator_chars[] = "";

void cons        PARAMS ((int));
void sbranch     PARAMS ((int));
void h8300hmode  PARAMS ((int));
void h8300smode  PARAMS ((int));
void h8300hnmode PARAMS ((int));
void h8300snmode PARAMS ((int));
static void pint PARAMS ((int));

int Hmode;
int Smode;
int Nmode;

#define PSIZE (Hmode ? L_32 : L_16)
#define DMODE (L_16)
#define DSYMMODE (Hmode ? L_24 : L_16)

int bsize = L_8;		/* Default branch displacement.  */

struct h8_instruction
{
  int length;
  int noperands;
  int idx;
  int size;
  const struct h8_opcode *opcode;
};

struct h8_instruction *h8_instructions;

void
h8300hmode (arg)
     int arg ATTRIBUTE_UNUSED;
{
  Hmode = 1;
  Smode = 0;
#ifdef BFD_ASSEMBLER
  if (!bfd_set_arch_mach (stdoutput, bfd_arch_h8300, bfd_mach_h8300h))
    as_warn (_("could not set architecture and machine"));
#endif
}

void
h8300smode (arg)
     int arg ATTRIBUTE_UNUSED;
{
  Smode = 1;
  Hmode = 1;
#ifdef BFD_ASSEMBLER
  if (!bfd_set_arch_mach (stdoutput, bfd_arch_h8300, bfd_mach_h8300s))
    as_warn (_("could not set architecture and machine"));
#endif
}

void
h8300hnmode (arg)
     int arg ATTRIBUTE_UNUSED;
{
  Hmode = 1;
  Smode = 0;
  Nmode = 1;
#ifdef BFD_ASSEMBLER
  if (!bfd_set_arch_mach (stdoutput, bfd_arch_h8300, bfd_mach_h8300hn))
    as_warn (_("could not set architecture and machine"));
#endif
}

void
h8300snmode (arg)
     int arg ATTRIBUTE_UNUSED;
{
  Smode = 1;
  Hmode = 1;
  Nmode = 1;
#ifdef BFD_ASSEMBLER
  if (!bfd_set_arch_mach (stdoutput, bfd_arch_h8300, bfd_mach_h8300sn))
    as_warn (_("could not set architecture and machine"));
#endif
}

void
sbranch (size)
     int size;
{
  bsize = size;
}

static void
pint (arg)
     int arg ATTRIBUTE_UNUSED;
{
  cons (Hmode ? 4 : 2);
}

/* This table describes all the machine specific pseudo-ops the assembler
   has to support.  The fields are:
   pseudo-op name without dot
   function to call to execute this pseudo-op
   Integer arg to pass to the function.  */

const pseudo_typeS md_pseudo_table[] =
{
  {"h8300h", h8300hmode, 0},
  {"h8300hn", h8300hnmode, 0},
  {"h8300s", h8300smode, 0},
  {"h8300sn", h8300snmode, 0},
  {"sbranch", sbranch, L_8},
  {"lbranch", sbranch, L_16},

#ifdef BFD_ASSEMBLER
  {"file", (void (*) PARAMS ((int))) dwarf2_directive_file, 0 },
  {"loc", dwarf2_directive_loc, 0 },
#endif

  {"int", pint, 0},
  {"data.b", cons, 1},
  {"data.w", cons, 2},
  {"data.l", cons, 4},
  {"form", listing_psize, 0},
  {"heading", listing_title, 0},
  {"import", s_ignore, 0},
  {"page", listing_eject, 0},
  {"program", s_ignore, 0},
  {0, 0, 0}
};

const int md_reloc_size;

const char EXP_CHARS[] = "eE";

/* Chars that mean this number is a floating point constant
   As in 0f12.456
   or    0d1.2345e12.  */
const char FLT_CHARS[] = "rRsSfFdDxXpP";

static struct hash_control *opcode_hash_control;	/* Opcode mnemonics.  */

/* This function is called once, at assembler startup time.  This
   should set up all the tables, etc. that the MD part of the assembler
   needs.  */

void
md_begin ()
{
  unsigned int nopcodes;
  const struct h8_opcode *p;
  struct h8_instruction *pi;
  char prev_buffer[100];
  int idx = 0;

#ifdef BFD_ASSEMBLER
  if (!bfd_set_arch_mach (stdoutput, bfd_arch_h8300, bfd_mach_h8300))
    as_warn (_("could not set architecture and machine"));
#endif

  opcode_hash_control = hash_new ();
  prev_buffer[0] = 0;

  nopcodes = sizeof (h8_opcodes) / sizeof (struct h8_opcode);
  
  h8_instructions = (struct h8_instruction *)
    xmalloc (nopcodes * sizeof (struct h8_instruction));

  for (p = h8_opcodes, pi = h8_instructions; p->name; p++, pi++)
    {
      /* Strip off any . part when inserting the opcode and only enter
         unique codes into the hash table.  */
      char *src = p->name;
      unsigned int len = strlen (src);
      char *dst = malloc (len + 1);
      char *buffer = dst;

      pi->size = 0;
      while (*src)
	{
	  if (*src == '.')
	    {
	      src++;
	      pi->size = *src;
	      break;
	    }
	  *dst++ = *src++;
	}
      *dst++ = 0;
      if (strcmp (buffer, prev_buffer))
	{
	  hash_insert (opcode_hash_control, buffer, (char *) pi);
	  strcpy (prev_buffer, buffer);
	  idx++;
	}
      pi->idx = idx;

      /* Find the number of operands.  */
      pi->noperands = 0;
      while (p->args.nib[pi->noperands] != E)
	pi->noperands++;

      /* Find the length of the opcode in bytes.  */
      pi->length = 0;
      while (p->data.nib[pi->length * 2] != E)
	pi->length++;

      pi->opcode = p;
    }

  /* Add entry for the NULL vector terminator.  */
  pi->length = 0;
  pi->noperands = 0;
  pi->idx = 0;
  pi->size = 0;
  pi->opcode = p;

  linkrelax = 1;
}

struct h8_exp
{
  char *e_beg;
  char *e_end;
  expressionS e_exp;
};

int dispreg;
int opsize;			/* Set when a register size is seen.  */

struct h8_op
{
  op_type mode;
  unsigned reg;
  expressionS exp;
};

static void clever_message PARAMS ((const struct h8_instruction *, struct h8_op *));
static void build_bytes    PARAMS ((const struct h8_instruction *, struct h8_op *));
static void do_a_fix_imm   PARAMS ((int, struct h8_op *, int));
static void check_operand  PARAMS ((struct h8_op *, unsigned int, char *));
static const struct h8_instruction * get_specific PARAMS ((const struct h8_instruction *, struct h8_op *, int));
static char * get_operands PARAMS ((unsigned, char *, struct h8_op *));
static void   get_operand  PARAMS ((char **, struct h8_op *, unsigned, int));
static char * skip_colonthing PARAMS ((char *, expressionS *, int *));
static char * parse_exp PARAMS ((char *, expressionS *));
static int    parse_reg PARAMS ((char *, op_type *, unsigned *, int));
char * colonmod24 PARAMS ((struct h8_op *, char *));

/*
  parse operands
  WREG r0,r1,r2,r3,r4,r5,r6,r7,fp,sp
  r0l,r0h,..r7l,r7h
  @WREG
  @WREG+
  @-WREG
  #const
  ccr
*/

/* Try to parse a reg name.  Return the number of chars consumed.  */

static int
parse_reg (src, mode, reg, direction)
     char *src;
     op_type *mode;
     unsigned int *reg;
     int direction;
{
  char *end;
  int len;

  /* Cribbed from get_symbol_end.  */
  if (!is_name_beginner (*src) || *src == '\001')
    return 0;
  end = src + 1;
  while (is_part_of_name (*end) || *end == '\001')
    end++;
  len = end - src;

  if (len == 2 && src[0] == 's' && src[1] == 'p')
    {
      *mode = PSIZE | REG | direction;
      *reg = 7;
      return len;
    }
  if (len == 3 && src[0] == 'c' && src[1] == 'c' && src[2] == 'r')
    {
      *mode = CCR;
      *reg = 0;
      return len;
    }
  if (len == 3 && src[0] == 'e' && src[1] == 'x' && src[2] == 'r')
    {
      *mode = EXR;
      *reg = 0;
      return len;
    }
  if (len == 2 && src[0] == 'f' && src[1] == 'p')
    {
      *mode = PSIZE | REG | direction;
      *reg = 6;
      return len;
    }
  if (len == 3 && src[0] == 'e' && src[1] == 'r'
      && src[2] >= '0' && src[2] <= '7')
    {
      *mode = L_32 | REG | direction;
      *reg = src[2] - '0';
      if (!Hmode)
	as_warn (_("Reg not valid for H8/300"));
      return len;
    }
  if (len == 2 && src[0] == 'e' && src[1] >= '0' && src[1] <= '7')
    {
      *mode = L_16 | REG | direction;
      *reg = src[1] - '0' + 8;
      if (!Hmode)
	as_warn (_("Reg not valid for H8/300"));
      return len;
    }

  if (src[0] == 'r')
    {
      if (src[1] >= '0' && src[1] <= '7')
	{
	  if (len == 3 && src[2] == 'l')
	    {
	      *mode = L_8 | REG | direction;
	      *reg = (src[1] - '0') + 8;
	      return len;
	    }
	  if (len == 3 && src[2] == 'h')
	    {
	      *mode = L_8 | REG | direction;
	      *reg = (src[1] - '0');
	      return len;
	    }
	  if (len == 2)
	    {
	      *mode = L_16 | REG | direction;
	      *reg = (src[1] - '0');
	      return len;
	    }
	}
    }

  return 0;
}

static char *
parse_exp (s, op)
     char *s;
     expressionS *op;
{
  char *save = input_line_pointer;
  char *new;

  input_line_pointer = s;
  expression (op);
  if (op->X_op == O_absent)
    as_bad (_("missing operand"));
  new = input_line_pointer;
  input_line_pointer = save;
  return new;
}

static char *
skip_colonthing (ptr, exp, mode)
     char *ptr;
     expressionS *exp ATTRIBUTE_UNUSED;
     int *mode;
{
  if (*ptr == ':')
    {
      ptr++;
      *mode &= ~SIZE;
      if (*ptr == '8')
	{
	  ptr++;
	  /* ff fill any 8 bit quantity.  */
	  /* exp->X_add_number -= 0x100; */
	  *mode |= L_8;
	}
      else
	{
	  if (*ptr == '2')
	    {
	      *mode |= L_24;
	    }
	  else if (*ptr == '3')
	    {
	      *mode |= L_32;
	    }
	  else if (*ptr == '1')
	    {
	      *mode |= L_16;
	    }
	  while (ISDIGIT (*ptr))
	    ptr++;
	}
    }
  return ptr;
}

/* The many forms of operand:

   Rn			Register direct
   @Rn			Register indirect
   @(exp[:16], Rn)	Register indirect with displacement
   @Rn+
   @-Rn
   @aa:8		absolute 8 bit
   @aa:16		absolute 16 bit
   @aa			absolute 16 bit

   #xx[:size]		immediate data
   @(exp:[8], pc)	pc rel
   @@aa[:8]		memory indirect.  */

char *
colonmod24 (op, src)
     struct h8_op *op;
     char *src;
{
  int mode = 0;
  src = skip_colonthing (src, &op->exp, &mode);

  if (!mode)
    {
      /* Choose a default mode.  */
      if (op->exp.X_add_number < -32768
	  || op->exp.X_add_number > 32767)
	{
	  if (Hmode)
	    mode = L_24;
	  else
	    mode = L_16;
	}
      else if (op->exp.X_add_symbol
	       || op->exp.X_op_symbol)
	mode = DSYMMODE;
      else
	mode = DMODE;
    }

  op->mode |= mode;
  return src;
}

static void
get_operand (ptr, op, dst, direction)
     char **ptr;
     struct h8_op *op;
     unsigned int dst ATTRIBUTE_UNUSED;
     int direction;
{
  char *src = *ptr;
  op_type mode;
  unsigned int num;
  unsigned int len;

  op->mode = E;

  /* Check for '(' and ')' for instructions ldm and stm.  */
  if (src[0] == '(' && src[8] == ')')
    ++ src;

  /* Gross.  Gross.  ldm and stm have a format not easily handled
     by get_operand.  We deal with it explicitly here.  */
  if (src[0] == 'e' && src[1] == 'r' && ISDIGIT (src[2])
      && src[3] == '-' && src[4] == 'e' && src[5] == 'r' && ISDIGIT (src[6]))
    {
      int low, high;

      low = src[2] - '0';
      high = src[6] - '0';

      if (high < low)
	as_bad (_("Invalid register list for ldm/stm\n"));

      if (low % 2)
	as_bad (_("Invalid register list for ldm/stm\n"));

      if (high - low > 3)
	as_bad (_("Invalid register list for ldm/stm\n"));

      if (high - low != 1
	  && low % 4)
	as_bad (_("Invalid register list for ldm/stm\n"));

      /* Even sicker.  We encode two registers into op->reg.  One
	 for the low register to save, the other for the high
	 register to save;  we also set the high bit in op->reg
	 so we know this is "very special".  */
      op->reg = 0x80000000 | (high << 8) | low;
      op->mode = REG;
      if (src[7] == ')')
	*ptr = src + 8;
      else
	*ptr = src + 7;
      return;
    }

  len = parse_reg (src, &op->mode, &op->reg, direction);
  if (len)
    {
      *ptr = src + len;
      return;
    }

  if (*src == '@')
    {
      src++;
      if (*src == '@')
	{
	  src++;
	  src = parse_exp (src, &op->exp);

	  src = skip_colonthing (src, &op->exp, &op->mode);

	  *ptr = src;

	  op->mode = MEMIND;
	  return;
	}

      if (*src == '-')
	{
	  src++;
	  len = parse_reg (src, &mode, &num, direction);
	  if (len == 0)
	    {
	      /* Oops, not a reg after all, must be ordinary exp.  */
	      src--;
	      /* Must be a symbol.  */
	      op->mode = ABS | PSIZE | direction;
	      *ptr = skip_colonthing (parse_exp (src, &op->exp),
				      &op->exp, &op->mode);

	      return;
	    }

	  if ((mode & SIZE) != PSIZE)
	    as_bad (_("Wrong size pointer register for architecture."));
	  op->mode = RDDEC;
	  op->reg = num;
	  *ptr = src + len;
	  return;
	}
      if (*src == '(')
	{
	  /* Disp.  */
	  src++;

	  /* Start off assuming a 16 bit offset.  */

	  src = parse_exp (src, &op->exp);

	  src = colonmod24 (op, src);

	  if (*src == ')')
	    {
	      src++;
	      op->mode |= ABS | direction;
	      *ptr = src;
	      return;
	    }

	  if (*src != ',')
	    {
	      as_bad (_("expected @(exp, reg16)"));
	      return;

	    }
	  src++;

	  len = parse_reg (src, &mode, &op->reg, direction);
	  if (len == 0 || !(mode & REG))
	    {
	      as_bad (_("expected @(exp, reg16)"));
	      return;
	    }
	  op->mode |= DISP | direction;
	  dispreg = op->reg;
	  src += len;
	  src = skip_colonthing (src, &op->exp, &op->mode);

	  if (*src != ')' && '(')
	    {
	      as_bad (_("expected @(exp, reg16)"));
	      return;
	    }
	  *ptr = src + 1;

	  return;
	}
      len = parse_reg (src, &mode, &num, direction);

      if (len)
	{
	  src += len;
	  if (*src == '+')
	    {
	      src++;
	      if ((mode & SIZE) != PSIZE)
		as_bad (_("Wrong size pointer register for architecture."));
	      op->mode = RSINC;
	      op->reg = num;
	      *ptr = src;
	      return;
	    }
	  if ((mode & SIZE) != PSIZE)
	    as_bad (_("Wrong size pointer register for architecture."));

	  op->mode = direction | IND | PSIZE;
	  op->reg = num;
	  *ptr = src;

	  return;
	}
      else
	{
	  /* must be a symbol */

	  op->mode = ABS | direction;
	  src = parse_exp (src, &op->exp);

	  *ptr = colonmod24 (op, src);

	  return;
	}
    }

  if (*src == '#')
    {
      src++;
      op->mode = IMM;
      src = parse_exp (src, &op->exp);
      *ptr = skip_colonthing (src, &op->exp, &op->mode);

      return;
    }
  else if (strncmp (src, "mach", 4) == 0
	   || strncmp (src, "macl", 4) == 0)
    {
      op->reg = src[3] == 'l';
      op->mode = MACREG;
      *ptr = src + 4;
      return;
    }
  else
    {
      src = parse_exp (src, &op->exp);
      /* Trailing ':' size ? */
      if (*src == ':')
	{
	  if (src[1] == '1' && src[2] == '6')
	    {
	      op->mode = PCREL | L_16;
	      src += 3;
	    }
	  else if (src[1] == '8')
	    {
	      op->mode = PCREL | L_8;
	      src += 2;
	    }
	  else
	    as_bad (_("expect :8 or :16 here"));
	}
      else
	op->mode = PCREL | bsize;

      *ptr = src;
    }
}

static char *
get_operands (noperands, op_end, operand)
     unsigned int noperands;
     char *op_end;
     struct h8_op *operand;
{
  char *ptr = op_end;

  switch (noperands)
    {
    case 0:
      operand[0].mode = 0;
      operand[1].mode = 0;
      break;

    case 1:
      ptr++;
      get_operand (&ptr, operand + 0, 0, SRC);
      if (*ptr == ',')
	{
	  ptr++;
	  get_operand (&ptr, operand + 1, 1, DST);
	}
      else
	{
	  operand[1].mode = 0;
	}
      break;

    case 2:
      ptr++;
      get_operand (&ptr, operand + 0, 0, SRC);
      if (*ptr == ',')
	ptr++;
      get_operand (&ptr, operand + 1, 1, DST);
      break;

    default:
      abort ();
    }

  return ptr;
}

/* Passed a pointer to a list of opcodes which use different
   addressing modes, return the opcode which matches the opcodes
   provided.  */

static const struct h8_instruction *
get_specific (instruction, operands, size)
     const struct h8_instruction *instruction;
     struct h8_op *operands;
     int size;
{
  const struct h8_instruction *this_try = instruction;
  int found = 0;
  int this_index = instruction->idx;

  /* There's only one ldm/stm and it's easier to just
     get out quick for them.  */
  if (strcmp (instruction->opcode->name, "stm.l") == 0
      || strcmp (instruction->opcode->name, "ldm.l") == 0)
    return this_try;

  while (this_index == instruction->idx && !found)
    {
      found = 1;

      this_try = instruction++;
      if (this_try->noperands == 0)
	{
	  int this_size;

	  this_size = this_try->opcode->how & SN;
	  if (this_size != size && (this_size != SB || size != SN))
	    found = 0;
	}
      else
	{
	  int i;

	  for (i = 0; i < this_try->noperands && found; i++)
	    {
	      op_type op = this_try->opcode->args.nib[i];
	      int x = operands[i].mode;

	      if ((op & (DISP | REG)) == (DISP | REG)
		  && ((x & (DISP | REG)) == (DISP | REG)))
		{
		  dispreg = operands[i].reg;
		}
	      else if (op & REG)
		{
		  if (!(x & REG))
		    found = 0;

		  if (x & L_P)
		    x = (x & ~L_P) | (Hmode ? L_32 : L_16);
		  if (op & L_P)
		    op = (op & ~L_P) | (Hmode ? L_32 : L_16);

		  opsize = op & SIZE;

		  /* The size of the reg is v important.  */
		  if ((op & SIZE) != (x & SIZE))
		    found = 0;
		}
	      else if ((op & ABSJMP) && (x & ABS))
		{
		  operands[i].mode &= ~ABS;
		  operands[i].mode |= ABSJMP;
		  /* But it may not be 24 bits long.  */
		  if (!Hmode)
		    {
		      operands[i].mode &= ~SIZE;
		      operands[i].mode |= L_16;
		    }
		}
	      else if ((op & (KBIT | DBIT)) && (x & IMM))
		{
		  /* This is ok if the immediate value is sensible.  */
		}
	      else if (op & PCREL)
		{
		  /* The size of the displacement is important.  */
		  if ((op & SIZE) != (x & SIZE))
		    found = 0;
		}
	      else if ((op & (DISP | IMM | ABS))
		       && (op & (DISP | IMM | ABS)) == (x & (DISP | IMM | ABS)))
		{
		  /* Promote a L_24 to L_32 if it makes us match.  */
		  if ((x & L_24) && (op & L_32))
		    {
		      x &= ~L_24;
		      x |= L_32;
		    }
		  /* Promote an L8 to L_16 if it makes us match.  */
		  if (op & ABS && op & L_8 && op & DISP)
		    {
		      if (x & L_16)
			found = 1;
		    }
		  else if ((x & SIZE) != 0
			   && ((op & SIZE) != (x & SIZE)))
		    found = 0;
		}
	      else if ((op & MACREG) != (x & MACREG))
		{
		  found = 0;
		}
	      else if ((op & MODE) != (x & MODE))
		{
		  found = 0;
		}
	    }
	}
    }
  if (found)
    return this_try;
  else
    return 0;
}

static void
check_operand (operand, width, string)
     struct h8_op *operand;
     unsigned int width;
     char *string;
{
  if (operand->exp.X_add_symbol == 0
      && operand->exp.X_op_symbol == 0)
    {
      /* No symbol involved, let's look at offset, it's dangerous if
	 any of the high bits are not 0 or ff's, find out by oring or
	 anding with the width and seeing if the answer is 0 or all
	 fs.  */

      if ((operand->exp.X_add_number & ~width) != 0 &&
	  (operand->exp.X_add_number | width) != (unsigned)(~0))
	{
	  if (width == 255
	      && (operand->exp.X_add_number & 0xff00) == 0xff00)
	    {
	      /* Just ignore this one - which happens when trying to
		 fit a 16 bit address truncated into an 8 bit address
		 of something like bset.  */
	    }
	  else if (strcmp (string, "@") == 0
		   && width == 0xffff
		   && (operand->exp.X_add_number & 0xff8000) == 0xff8000)
	    {
	      /* Just ignore this one - which happens when trying to
		 fit a 24 bit address truncated into a 16 bit address
		 of something like mov.w.  */
	    }
	  else
	    {
	      as_warn (_("operand %s0x%lx out of range."), string,
		       (unsigned long) operand->exp.X_add_number);
	    }
	}
    }
}

/* RELAXMODE has one of 3 values:

   0 Output a "normal" reloc, no relaxing possible for this insn/reloc

   1 Output a relaxable 24bit absolute mov.w address relocation
     (may relax into a 16bit absolute address).

   2 Output a relaxable 16/24 absolute mov.b address relocation
     (may relax into an 8bit absolute address).  */

static void
do_a_fix_imm (offset, operand, relaxmode)
     int offset;
     struct h8_op *operand;
     int relaxmode;
{
  int idx;
  int size;
  int where;

  char *t = operand->mode & IMM ? "#" : "@";

  if (operand->exp.X_add_symbol == 0)
    {
      char *bytes = frag_now->fr_literal + offset;
      switch (operand->mode & SIZE)
	{
	case L_2:
	  check_operand (operand, 0x3, t);
	  bytes[0] |= (operand->exp.X_add_number) << 4;
	  break;
	case L_3:
	  check_operand (operand, 0x7, t);
	  bytes[0] |= (operand->exp.X_add_number) << 4;
	  break;
	case L_8:
	  check_operand (operand, 0xff, t);
	  bytes[0] = operand->exp.X_add_number;
	  break;
	case L_16:
	  check_operand (operand, 0xffff, t);
	  bytes[0] = operand->exp.X_add_number >> 8;
	  bytes[1] = operand->exp.X_add_number >> 0;
	  break;
	case L_24:
	  check_operand (operand, 0xffffff, t);
	  bytes[0] = operand->exp.X_add_number >> 16;
	  bytes[1] = operand->exp.X_add_number >> 8;
	  bytes[2] = operand->exp.X_add_number >> 0;
	  break;

	case L_32:
	  /* This should be done with bfd.  */
	  bytes[0] = operand->exp.X_add_number >> 24;
	  bytes[1] = operand->exp.X_add_number >> 16;
	  bytes[2] = operand->exp.X_add_number >> 8;
	  bytes[3] = operand->exp.X_add_number >> 0;
	  if (relaxmode != 0)
	    {
	      idx = (relaxmode == 2) ? R_MOV24B1 : R_MOVL1;
	      fix_new_exp (frag_now, offset, 4, &operand->exp, 0, idx);
	    }
	  break;
	}
    }
  else
    {
      switch (operand->mode & SIZE)
	{
	case L_24:
	case L_32:
	  size = 4;
	  where = (operand->mode & SIZE) == L_24 ? -1 : 0;
	  if (relaxmode == 2)
	    idx = R_MOV24B1;
	  else if (relaxmode == 1)
	    idx = R_MOVL1;
	  else
	    idx = R_RELLONG;
	  break;
	default:
	  as_bad (_("Can't work out size of operand.\n"));
	case L_16:
	  size = 2;
	  where = 0;
	  if (relaxmode == 2)
	    idx = R_MOV16B1;
	  else
	    idx = R_RELWORD;
	  operand->exp.X_add_number =
	    ((operand->exp.X_add_number & 0xffff) ^ 0x8000) - 0x8000;
	  break;
	case L_8:
	  size = 1;
	  where = 0;
	  idx = R_RELBYTE;
	  operand->exp.X_add_number =
	    ((operand->exp.X_add_number & 0xff) ^ 0x80) - 0x80;
	}

      fix_new_exp (frag_now,
		   offset + where,
		   size,
		   &operand->exp,
		   0,
		   idx);
    }
}

/* Now we know what sort of opcodes it is, let's build the bytes.  */

static void
build_bytes (this_try, operand)
     const struct h8_instruction *this_try;
     struct h8_op *operand;
{
  int i;
  char *output = frag_more (this_try->length);
  op_type *nibble_ptr = this_try->opcode->data.nib;
  op_type c;
  unsigned int nibble_count = 0;
  int absat = 0;
  int immat = 0;
  int nib = 0;
  int movb = 0;
  char asnibbles[30];
  char *p = asnibbles;

  if (!(this_try->opcode->inbase || Hmode))
    as_warn (_("Opcode `%s' with these operand types not available in H8/300 mode"),
	     this_try->opcode->name);

  while (*nibble_ptr != E)
    {
      int d;
      c = *nibble_ptr++;

      d = (c & (DST | SRC_IN_DST)) != 0;

      if (c < 16)
	nib = c;
      else
	{
	  if (c & (REG | IND | INC | DEC))
	    nib = operand[d].reg;

	  else if ((c & DISPREG) == (DISPREG))
	    nib = dispreg;

	  else if (c & ABS)
	    {
	      operand[d].mode = c;
	      absat = nibble_count / 2;
	      nib = 0;
	    }
	  else if (c & (IMM | PCREL | ABS | ABSJMP | DISP))
	    {
	      operand[d].mode = c;
	      immat = nibble_count / 2;
	      nib = 0;
	    }
	  else if (c & IGNORE)
	    nib = 0;

	  else if (c & DBIT)
	    {
	      switch (operand[0].exp.X_add_number)
		{
		case 1:
		  nib = c;
		  break;
		case 2:
		  nib = 0x8 | c;
		  break;
		default:
		  as_bad (_("Need #1 or #2 here"));
		}
	    }
	  else if (c & KBIT)
	    {
	      switch (operand[0].exp.X_add_number)
		{
		case 1:
		  nib = 0;
		  break;
		case 2:
		  nib = 8;
		  break;
		case 4:
		  if (!Hmode)
		    as_warn (_("#4 not valid on H8/300."));
		  nib = 9;
		  break;

		default:
		  as_bad (_("Need #1 or #2 here"));
		  break;
		}
	      /* Stop it making a fix.  */
	      operand[0].mode = 0;
	    }

	  if (c & MEMRELAX)
	    operand[d].mode |= MEMRELAX;

	  if (c & B31)
	    nib |= 0x8;

	  if (c & MACREG)
	    {
	      if (operand[0].mode == MACREG)
		/* stmac has mac[hl] as the first operand.  */
		nib = 2 + operand[0].reg;
	      else
		/* ldmac has mac[hl] as the second operand.  */
		nib = 2 + operand[1].reg;
	    }
	}
      nibble_count++;

      *p++ = nib;
    }

  /* Disgusting.  Why, oh why didn't someone ask us for advice
     on the assembler format.  */
  if (strcmp (this_try->opcode->name, "stm.l") == 0
      || strcmp (this_try->opcode->name, "ldm.l") == 0)
    {
      int high, low;
      high = (operand[this_try->opcode->name[0] == 'l' ? 1 : 0].reg >> 8) & 0xf;
      low = operand[this_try->opcode->name[0] == 'l' ? 1 : 0].reg & 0xf;

      asnibbles[2] = high - low;
      asnibbles[7] = (this_try->opcode->name[0] == 'l') ? high : low;
    }

  for (i = 0; i < this_try->length; i++)
    output[i] = (asnibbles[i * 2] << 4) | asnibbles[i * 2 + 1];

  /* Note if this is a movb instruction -- there's a special relaxation
     which only applies to them.  */
  if (strcmp (this_try->opcode->name, "mov.b") == 0)
    movb = 1;

  /* Output any fixes.  */
  for (i = 0; i < 2; i++)
    {
      int x = operand[i].mode;

      if (x & (IMM | DISP))
	do_a_fix_imm (output - frag_now->fr_literal + immat,
		      operand + i, (x & MEMRELAX) != 0);

      else if (x & ABS)
	do_a_fix_imm (output - frag_now->fr_literal + absat,
		      operand + i, (x & MEMRELAX) ? movb + 1 : 0);

      else if (x & PCREL)
	{
	  int size16 = x & (L_16);
	  int where = size16 ? 2 : 1;
	  int size = size16 ? 2 : 1;
	  int type = size16 ? R_PCRWORD : R_PCRBYTE;
	  fixS *fixP;

	  check_operand (operand + i, size16 ? 0x7fff : 0x7f, "@");

	  if (operand[i].exp.X_add_number & 1)
	    as_warn (_("branch operand has odd offset (%lx)\n"),
		     (unsigned long) operand->exp.X_add_number);
#ifndef OBJ_ELF
	  /* The COFF port has always been off by one, changing it
	     now would be an incompatible change, so we leave it as-is.

	     We don't want to do this for ELF as we want to be
	     compatible with the proposed ELF format from Hitachi.  */
	  operand[i].exp.X_add_number -= 1;
#endif
	  operand[i].exp.X_add_number =
	    ((operand[i].exp.X_add_number & 0xff) ^ 0x80) - 0x80;

	  fixP = fix_new_exp (frag_now,
			      output - frag_now->fr_literal + where,
			      size,
			      &operand[i].exp,
			      1,
			      type);
	  fixP->fx_signed = 1;
	}
      else if (x & MEMIND)
	{
	  check_operand (operand + i, 0xff, "@@");
	  fix_new_exp (frag_now,
		       output - frag_now->fr_literal + 1,
		       1,
		       &operand[i].exp,
		       0,
		       R_MEM_INDIRECT);
	}
      else if (x & ABSJMP)
	{
	  int where = 0;

#ifdef OBJ_ELF
	  /* To be compatible with the proposed H8 ELF format, we
	     want the relocation's offset to point to the first byte
	     that will be modified, not to the start of the instruction.  */
	  where += 1;
#endif

	  /* This jmp may be a jump or a branch.  */

	  check_operand (operand + i, Hmode ? 0xffffff : 0xffff, "@");

	  if (operand[i].exp.X_add_number & 1)
	    as_warn (_("branch operand has odd offset (%lx)\n"),
		     (unsigned long) operand->exp.X_add_number);

	  if (!Hmode)
	    operand[i].exp.X_add_number =
	      ((operand[i].exp.X_add_number & 0xffff) ^ 0x8000) - 0x8000;
	  fix_new_exp (frag_now,
		       output - frag_now->fr_literal + where,
		       4,
		       &operand[i].exp,
		       0,
		       R_JMPL1);
	}
    }
}

/* Try to give an intelligent error message for common and simple to
   detect errors.  */

static void
clever_message (instruction, operand)
     const struct h8_instruction *instruction;
     struct h8_op *operand;
{
  /* Find out if there was more than one possible opcode.  */

  if ((instruction + 1)->idx != instruction->idx)
    {
      int argn;

      /* Only one opcode of this flavour, try to guess which operand
         didn't match.  */
      for (argn = 0; argn < instruction->noperands; argn++)
	{
	  switch (instruction->opcode->args.nib[argn])
	    {
	    case RD16:
	      if (operand[argn].mode != RD16)
		{
		  as_bad (_("destination operand must be 16 bit register"));
		  return;

		}
	      break;

	    case RS8:
	      if (operand[argn].mode != RS8)
		{
		  as_bad (_("source operand must be 8 bit register"));
		  return;
		}
	      break;

	    case ABS16DST:
	      if (operand[argn].mode != ABS16DST)
		{
		  as_bad (_("destination operand must be 16bit absolute address"));
		  return;
		}
	      break;
	    case RD8:
	      if (operand[argn].mode != RD8)
		{
		  as_bad (_("destination operand must be 8 bit register"));
		  return;
		}
	      break;

	    case ABS16SRC:
	      if (operand[argn].mode != ABS16SRC)
		{
		  as_bad (_("source operand must be 16bit absolute address"));
		  return;
		}
	      break;

	    }
	}
    }
  as_bad (_("invalid operands"));
}

/* This is the guts of the machine-dependent assembler.  STR points to
   a machine dependent instruction.  This function is supposed to emit
   the frags/bytes it assembles.  */

void
md_assemble (str)
     char *str;
{
  char *op_start;
  char *op_end;
  struct h8_op operand[2];
  const struct h8_instruction *instruction;
  const struct h8_instruction *prev_instruction;

  char *dot = 0;
  char c;
  int size;

  /* Drop leading whitespace.  */
  while (*str == ' ')
    str++;

  /* Find the op code end.  */
  for (op_start = op_end = str;
       *op_end != 0 && *op_end != ' ';
       op_end++)
    {
      if (*op_end == '.')
	{
	  dot = op_end + 1;
	  *op_end = 0;
	  op_end += 2;
	  break;
	}
    }

  if (op_end == op_start)
    {
      as_bad (_("can't find opcode "));
    }
  c = *op_end;

  *op_end = 0;

  instruction = (const struct h8_instruction *)
    hash_find (opcode_hash_control, op_start);

  if (instruction == NULL)
    {
      as_bad (_("unknown opcode"));
      return;
    }

  /* We used to set input_line_pointer to the result of get_operands,
     but that is wrong.  Our caller assumes we don't change it.  */

  (void) get_operands (instruction->noperands, op_end, operand);
  *op_end = c;
  prev_instruction = instruction;

  size = SN;
  if (dot)
    {
      switch (*dot)
	{
	case 'b':
	  size = SB;
	  break;

	case 'w':
	  size = SW;
	  break;

	case 'l':
	  size = SL;
	  break;
	}
    }
  instruction = get_specific (instruction, operand, size);

  if (instruction == 0)
    {
      /* Couldn't find an opcode which matched the operands.  */
      char *where = frag_more (2);

      where[0] = 0x0;
      where[1] = 0x0;
      clever_message (prev_instruction, operand);

      return;
    }
  if (instruction->size && dot)
    {
      if (instruction->size != *dot)
	{
	  as_warn (_("mismatch between opcode size and operand size"));
	}
    }

  build_bytes (instruction, operand);

#ifdef BFD_ASSEMBLER
  dwarf2_emit_insn (instruction->length);
#endif
}

#ifndef BFD_ASSEMBLER
void
tc_crawl_symbol_chain (headers)
     object_headers *headers ATTRIBUTE_UNUSED;
{
  printf (_("call to tc_crawl_symbol_chain \n"));
}
#endif

symbolS *
md_undefined_symbol (name)
     char *name ATTRIBUTE_UNUSED;
{
  return 0;
}

#ifndef BFD_ASSEMBLER
void
tc_headers_hook (headers)
     object_headers *headers ATTRIBUTE_UNUSED;
{
  printf (_("call to tc_headers_hook \n"));
}
#endif

/* Various routines to kill one day */
/* Equal to MAX_PRECISION in atof-ieee.c */
#define MAX_LITTLENUMS 6

/* Turn a string in input_line_pointer into a floating point constant
   of type TYPE, and store the appropriate bytes in *LITP.  The number
   of LITTLENUMS emitted is stored in *SIZEP.  An error message is
   returned, or NULL on OK.  */

char *
md_atof (type, litP, sizeP)
     char type;
     char *litP;
     int *sizeP;
{
  int prec;
  LITTLENUM_TYPE words[MAX_LITTLENUMS];
  LITTLENUM_TYPE *wordP;
  char *t;

  switch (type)
    {
    case 'f':
    case 'F':
    case 's':
    case 'S':
      prec = 2;
      break;

    case 'd':
    case 'D':
    case 'r':
    case 'R':
      prec = 4;
      break;

    case 'x':
    case 'X':
      prec = 6;
      break;

    case 'p':
    case 'P':
      prec = 6;
      break;

    default:
      *sizeP = 0;
      return _("Bad call to MD_ATOF()");
    }
  t = atof_ieee (input_line_pointer, type, words);
  if (t)
    input_line_pointer = t;

  *sizeP = prec * sizeof (LITTLENUM_TYPE);
  for (wordP = words; prec--;)
    {
      md_number_to_chars (litP, (long) (*wordP++), sizeof (LITTLENUM_TYPE));
      litP += sizeof (LITTLENUM_TYPE);
    }
  return 0;
}

const char *md_shortopts = "";
struct option md_longopts[] = {
  {NULL, no_argument, NULL, 0}
};

size_t md_longopts_size = sizeof (md_longopts);

int
md_parse_option (c, arg)
     int c ATTRIBUTE_UNUSED;
     char *arg ATTRIBUTE_UNUSED;
{
  return 0;
}

void
md_show_usage (stream)
     FILE *stream ATTRIBUTE_UNUSED;
{
}

void tc_aout_fix_to_chars PARAMS ((void));

void
tc_aout_fix_to_chars ()
{
  printf (_("call to tc_aout_fix_to_chars \n"));
  abort ();
}

void
md_convert_frag (headers, seg, fragP)
#ifdef BFD_ASSEMBLER
     bfd *headers ATTRIBUTE_UNUSED;
#else
     object_headers *headers ATTRIBUTE_UNUSED;
#endif
     segT seg ATTRIBUTE_UNUSED;
     fragS *fragP ATTRIBUTE_UNUSED;
{
  printf (_("call to md_convert_frag \n"));
  abort ();
}

#ifdef BFD_ASSEMBLER
valueT
md_section_align (segment, size)
     segT segment;
     valueT size;
{
  int align = bfd_get_section_alignment (stdoutput, segment);
  return ((size + (1 << align) - 1) & (-1 << align));
}
#else
valueT
md_section_align (seg, size)
     segT seg;
     valueT size;
{
  return ((size + (1 << section_alignment[(int) seg]) - 1)
	  & (-1 << section_alignment[(int) seg]));
}
#endif


void
md_apply_fix3 (fixP, valP, seg)
     fixS *fixP;
     valueT *valP;
     segT seg ATTRIBUTE_UNUSED;
{
  char *buf = fixP->fx_where + fixP->fx_frag->fr_literal;
  long val = *valP;

  switch (fixP->fx_size)
    {
    case 1:
      *buf++ = val;
      break;
    case 2:
      *buf++ = (val >> 8);
      *buf++ = val;
      break;
    case 4:
      *buf++ = (val >> 24);
      *buf++ = (val >> 16);
      *buf++ = (val >> 8);
      *buf++ = val;
      break;
    default:
      abort ();
    }

  if (fixP->fx_addsy == NULL && fixP->fx_pcrel == 0)
    fixP->fx_done = 1;
}

int
md_estimate_size_before_relax (fragP, segment_type)
     register fragS *fragP ATTRIBUTE_UNUSED;
     register segT segment_type ATTRIBUTE_UNUSED;
{
  printf (_("call tomd_estimate_size_before_relax \n"));
  abort ();
}

/* Put number into target byte order.  */
void
md_number_to_chars (ptr, use, nbytes)
     char *ptr;
     valueT use;
     int nbytes;
{
  number_to_chars_bigendian (ptr, use, nbytes);
}

long
md_pcrel_from (fixP)
     fixS *fixP ATTRIBUTE_UNUSED;
{
  abort ();
}

#ifndef BFD_ASSEMBLER
void
tc_reloc_mangle (fix_ptr, intr, base)
     fixS *fix_ptr;
     struct internal_reloc *intr;
     bfd_vma base;

{
  symbolS *symbol_ptr;

  symbol_ptr = fix_ptr->fx_addsy;

  /* If this relocation is attached to a symbol then it's ok
     to output it.  */
  if (fix_ptr->fx_r_type == TC_CONS_RELOC)
    {
      /* cons likes to create reloc32's whatever the size of the reloc..
       */
      switch (fix_ptr->fx_size)
	{
	case 4:
	  intr->r_type = R_RELLONG;
	  break;
	case 2:
	  intr->r_type = R_RELWORD;
	  break;
	case 1:
	  intr->r_type = R_RELBYTE;
	  break;
	default:
	  abort ();
	}
    }
  else
    {
      intr->r_type = fix_ptr->fx_r_type;
    }

  intr->r_vaddr = fix_ptr->fx_frag->fr_address + fix_ptr->fx_where + base;
  intr->r_offset = fix_ptr->fx_offset;

  if (symbol_ptr)
    {
      if (symbol_ptr->sy_number != -1)
	intr->r_symndx = symbol_ptr->sy_number;
      else
	{
	  symbolS *segsym;

	  /* This case arises when a reference is made to `.'.  */
	  segsym = seg_info (S_GET_SEGMENT (symbol_ptr))->dot;
	  if (segsym == NULL)
	    intr->r_symndx = -1;
	  else
	    {
	      intr->r_symndx = segsym->sy_number;
	      intr->r_offset += S_GET_VALUE (symbol_ptr);
	    }
	}
    }
  else
    intr->r_symndx = -1;
}
#else /* BFD_ASSEMBLER */
arelent *
tc_gen_reloc (section, fixp)
     asection *section ATTRIBUTE_UNUSED;
     fixS *fixp;
{
  arelent *rel;
  bfd_reloc_code_real_type r_type;

  if (fixp->fx_addsy && fixp->fx_subsy)
    {
      if ((S_GET_SEGMENT (fixp->fx_addsy) != S_GET_SEGMENT (fixp->fx_subsy))
	  || S_GET_SEGMENT (fixp->fx_addsy) == undefined_section)
	{
	  as_bad_where (fixp->fx_file, fixp->fx_line,
			"Difference of symbols in different sections is not supported");
	  return NULL;
	}
    }

  rel = (arelent *) xmalloc (sizeof (arelent));
  rel->sym_ptr_ptr = (asymbol **) xmalloc (sizeof (asymbol *));
  *rel->sym_ptr_ptr = symbol_get_bfdsym (fixp->fx_addsy);
  rel->address = fixp->fx_frag->fr_address + fixp->fx_where;
  rel->addend = fixp->fx_offset;

  r_type = fixp->fx_r_type;

#define DEBUG 0
#if DEBUG
  fprintf (stderr, "%s\n", bfd_get_reloc_code_name (r_type));
  fflush(stderr);
#endif
  rel->howto = bfd_reloc_type_lookup (stdoutput, r_type);
  if (rel->howto == NULL)
    {
      as_bad_where (fixp->fx_file, fixp->fx_line,
		    _("Cannot represent relocation type %s"),
		    bfd_get_reloc_code_name (r_type));
      return NULL;
    }

  return rel;
}
#endif
