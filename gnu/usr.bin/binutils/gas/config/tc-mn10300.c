/* tc-mn10300.c -- Assembler code for the Matsushita 10300

   Copyright (C) 1996 Free Software Foundation.

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
   along with GAS; see the file COPYING.  If not, write to
   the Free Software Foundation, 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

#include <stdio.h>
#include <ctype.h>
#include "as.h"
#include "subsegs.h"     
#include "opcode/mn10300.h"

/* Structure to hold information about predefined registers.  */
struct reg_name
{
  const char *name;
  int value;
};

/* Generic assembler global variables which must be defined by all targets. */

/* Characters which always start a comment. */
const char comment_chars[] = "#";

/* Characters which start a comment at the beginning of a line.  */
const char line_comment_chars[] = ";#";

/* Characters which may be used to separate multiple commands on a 
   single line.  */
const char line_separator_chars[] = ";";

/* Characters which are used to indicate an exponent in a floating 
   point number.  */
const char EXP_CHARS[] = "eE";

/* Characters which mean that a number is a floating point constant, 
   as in 0d1.0.  */
const char FLT_CHARS[] = "dD";


/* local functions */
static unsigned long mn10300
  PARAMS ((unsigned long insn, const struct mn10300_operand *operand,
	   offsetT val, char *file, unsigned int line));
static int reg_name_search PARAMS ((const struct reg_name *, int, const char *));
static boolean register_name PARAMS ((expressionS *expressionP));
static boolean system_register_name PARAMS ((expressionS *expressionP));
static boolean cc_name PARAMS ((expressionS *expressionP));


/* fixups */
#define MAX_INSN_FIXUPS (5)
struct mn10300_fixup
{
  expressionS exp;
  int opindex;
  bfd_reloc_code_real_type reloc;
};
struct mn10300_fixup fixups[MAX_INSN_FIXUPS];
static int fc;

const char *md_shortopts = "";
struct option md_longopts[] = {
  {NULL, no_argument, NULL, 0}
};
size_t md_longopts_size = sizeof(md_longopts); 

/* The target specific pseudo-ops which we support.  */
const pseudo_typeS md_pseudo_table[] =
{
  { NULL,       NULL,           0 }
};

/* Opcode hash table.  */
static struct hash_control *mn10300_hash;

/* This table is sorted. Suitable for searching by a binary search. */
static const struct reg_name data_registers[] =
{
  { "d0", 0 },
  { "d1", 1 },
  { "d2", 2 },
  { "d3", 3 },
};
#define DATA_REG_NAME_CNT	(sizeof(data_registers) / sizeof(struct reg_name))

static const struct reg_name address_registers[] =
{
  { "a0", 0 },
  { "a1", 1 },
  { "a2", 2 },
  { "a3", 3 },
};
#define ADDRESS_REG_NAME_CNT	(sizeof(address_registers) / sizeof(struct reg_name))

static const struct reg_name other_registers[] =
{
  { "mdr", 0 },
  { "psw", 0 },
  { "sp", 0 },
};
#define OTHER_REG_NAME_CNT	(sizeof(other_registers) / sizeof(struct reg_name))

/* reg_name_search does a binary search of the given register table
   to see if "name" is a valid regiter name.  Returns the register
   number from the array on success, or -1 on failure. */

static int
reg_name_search (regs, regcount, name)
     const struct reg_name *regs;
     int regcount;
     const char *name;
{
  int middle, low, high;
  int cmp;

  low = 0;
  high = regcount - 1;

  do
    {
      middle = (low + high) / 2;
      cmp = strcasecmp (name, regs[middle].name);
      if (cmp < 0)
	high = middle - 1;
      else if (cmp > 0)
	low = middle + 1;
      else 
	  return regs[middle].value;
    }
  while (low <= high);
  return -1;
}


/* Summary of register_name().
 *
 * in: Input_line_pointer points to 1st char of operand.
 *
 * out: A expressionS.
 *	The operand may have been a register: in this case, X_op == O_register,
 *	X_add_number is set to the register number, and truth is returned.
 *	Input_line_pointer->(next non-blank) char after operand, or is in
 *	its original state.
 */
static boolean
data_register_name (expressionP)
     expressionS *expressionP;
{
  int reg_number;
  char *name;
  char *start;
  char c;

  /* Find the spelling of the operand */
  start = name = input_line_pointer;

  c = get_symbol_end ();
  reg_number = reg_name_search (data_registers, DATA_REG_NAME_CNT, name);

  /* look to see if it's in the register table */
  if (reg_number >= 0) 
    {
      expressionP->X_op = O_register;
      expressionP->X_add_number = reg_number;

      /* make the rest nice */
      expressionP->X_add_symbol = NULL;
      expressionP->X_op_symbol = NULL;
      *input_line_pointer = c;	/* put back the delimiting char */
      return true;
    }
  else
    {
      /* reset the line as if we had not done anything */
      *input_line_pointer = c;   /* put back the delimiting char */
      input_line_pointer = start; /* reset input_line pointer */
      return false;
    }
}

/* Summary of register_name().
 *
 * in: Input_line_pointer points to 1st char of operand.
 *
 * out: A expressionS.
 *	The operand may have been a register: in this case, X_op == O_register,
 *	X_add_number is set to the register number, and truth is returned.
 *	Input_line_pointer->(next non-blank) char after operand, or is in
 *	its original state.
 */
static boolean
address_register_name (expressionP)
     expressionS *expressionP;
{
  int reg_number;
  char *name;
  char *start;
  char c;

  /* Find the spelling of the operand */
  start = name = input_line_pointer;

  c = get_symbol_end ();
  reg_number = reg_name_search (address_registers, ADDRESS_REG_NAME_CNT, name);

  /* look to see if it's in the register table */
  if (reg_number >= 0) 
    {
      expressionP->X_op = O_register;
      expressionP->X_add_number = reg_number;

      /* make the rest nice */
      expressionP->X_add_symbol = NULL;
      expressionP->X_op_symbol = NULL;
      *input_line_pointer = c;	/* put back the delimiting char */
      return true;
    }
  else
    {
      /* reset the line as if we had not done anything */
      *input_line_pointer = c;   /* put back the delimiting char */
      input_line_pointer = start; /* reset input_line pointer */
      return false;
    }
}

/* Summary of register_name().
 *
 * in: Input_line_pointer points to 1st char of operand.
 *
 * out: A expressionS.
 *	The operand may have been a register: in this case, X_op == O_register,
 *	X_add_number is set to the register number, and truth is returned.
 *	Input_line_pointer->(next non-blank) char after operand, or is in
 *	its original state.
 */
static boolean
other_register_name (expressionP)
     expressionS *expressionP;
{
  int reg_number;
  char *name;
  char *start;
  char c;

  /* Find the spelling of the operand */
  start = name = input_line_pointer;

  c = get_symbol_end ();
  reg_number = reg_name_search (other_registers, OTHER_REG_NAME_CNT, name);

  /* look to see if it's in the register table */
  if (reg_number >= 0) 
    {
      expressionP->X_op = O_register;
      expressionP->X_add_number = reg_number;

      /* make the rest nice */
      expressionP->X_add_symbol = NULL;
      expressionP->X_op_symbol = NULL;
      *input_line_pointer = c;	/* put back the delimiting char */
      return true;
    }
  else
    {
      /* reset the line as if we had not done anything */
      *input_line_pointer = c;   /* put back the delimiting char */
      input_line_pointer = start; /* reset input_line pointer */
      return false;
    }
}

void
md_show_usage (stream)
  FILE *stream;
{
  fprintf(stream, "MN10300 options:\n\
none yet\n");
} 

int
md_parse_option (c, arg)
     int c;
     char *arg;
{
  return 0;
}

symbolS *
md_undefined_symbol (name)
  char *name;
{
  return 0;
}

char *
md_atof (type, litp, sizep)
  int type;
  char *litp;
  int *sizep;
{
  int prec;
  LITTLENUM_TYPE words[4];
  char *t;
  int i;

  switch (type)
    {
    case 'f':
      prec = 2;
      break;

    case 'd':
      prec = 4;
      break;

    default:
      *sizep = 0;
      return "bad call to md_atof";
    }
  
  t = atof_ieee (input_line_pointer, type, words);
  if (t)
    input_line_pointer = t;

  *sizep = prec * 2;

  for (i = prec - 1; i >= 0; i--)
    {
      md_number_to_chars (litp, (valueT) words[i], 2);
      litp += 2;
    }

  return NULL;
}


void
md_convert_frag (abfd, sec, fragP)
  bfd *abfd;
  asection *sec;
  fragS *fragP;
{
  /* printf ("call to md_convert_frag \n"); */
  abort ();
}

valueT
md_section_align (seg, addr)
     asection *seg;
     valueT addr;
{
  int align = bfd_get_section_alignment (stdoutput, seg);
  return ((addr + (1 << align) - 1) & (-1 << align));
}

void
md_begin ()
{
  char *prev_name = "";
  register const struct mn10300_opcode *op;

  mn10300_hash = hash_new();

  /* Insert unique names into hash table.  The MN10300 instruction set
     has many identical opcode names that have different opcodes based
     on the operands.  This hash table then provides a quick index to
     the first opcode with a particular name in the opcode table.  */

  op     = mn10300_opcodes;
  while (op->name)
    {
      if (strcmp (prev_name, op->name)) 
	{
	  prev_name = (char *) op->name;
	  hash_insert (mn10300_hash, op->name, (char *) op);
	}
      op++;
    }
}

void
md_assemble (str) 
     char *str;
{
  char *s;
  struct mn10300_opcode *opcode;
  struct mn10300_opcode *next_opcode;
  const unsigned char *opindex_ptr;
  int next_opindex;
  unsigned long insn, extension, size;
  char *f;
  int i;
  int match;
  bfd_reloc_code_real_type reloc;

  /* Get the opcode.  */
  for (s = str; *s != '\0' && ! isspace (*s); s++)
    ;
  if (*s != '\0')
    *s++ = '\0';

  /* find the first opcode with the proper name */
  opcode = (struct mn10300_opcode *)hash_find (mn10300_hash, str);
  if (opcode == NULL)
    {
      as_bad ("Unrecognized opcode: `%s'", str);
      return;
    }

  str = s;
  while (isspace (*str))
    ++str;

  input_line_pointer = str;

  for(;;)
    {
      const char *errmsg = NULL;
      int op_idx;
      char *hold;
      int extra_shift = 0;

      fc = 0;
      match = 0;
      next_opindex = 0;
      insn = opcode->opcode;
      extension = 0;
      for (op_idx = 1, opindex_ptr = opcode->operands;
	   *opindex_ptr != 0;
	   opindex_ptr++, op_idx++)
	{
	  const struct mn10300_operand *operand;
	  expressionS ex;

	  if (next_opindex == 0)
	    {
	      operand = &mn10300_operands[*opindex_ptr];
	    }
	  else
	    {
	      operand = &mn10300_operands[next_opindex];
	      next_opindex = 0;
	    }

	  errmsg = NULL;

	  while (*str == ' ' || *str == ',' || *str == '[' || *str == ']')
	    ++str;

	  /* Gather the operand. */
	  hold = input_line_pointer;
	  input_line_pointer = str;

	  if (operand->flags & MN10300_OPERAND_PAREN)
	    {
	      if (*input_line_pointer != ')' && *input_line_pointer != '(')
		{
		  input_line_pointer = hold;
		  str = hold;
		  goto error;
		}
	      input_line_pointer++;
	      goto keep_going;
	    }
	  /* See if we can match the operands.  */
	  else if (operand->flags & MN10300_OPERAND_DREG)
	    {
	      if (!data_register_name (&ex))
		{
		  input_line_pointer = hold;
		  str = hold;
		  goto error;
		}
	    }
	  else if (operand->flags & MN10300_OPERAND_AREG)
	    {
	      if (!address_register_name (&ex))
		{
		  input_line_pointer = hold;
		  str = hold;
		  goto error;
		}
	    }
	  else if (operand->flags & MN10300_OPERAND_SP)
	    {
	      char *start = input_line_pointer;
	      char c = get_symbol_end ();

	      if (strcmp (start, "sp") != 0)
		{
		  *input_line_pointer = c;
		  input_line_pointer = hold;
		  str = hold;
		  goto error;
		}
	      *input_line_pointer = c;
	      goto keep_going;
	    }
	  else if (operand->flags & MN10300_OPERAND_PSW)
	    {
	      char *start = input_line_pointer;
	      char c = get_symbol_end ();

	      if (strcmp (start, "psw") != 0)
		{
		  *input_line_pointer = c;
		  input_line_pointer = hold;
		  str = hold;
		  goto error;
		}
	      *input_line_pointer = c;
	      goto keep_going;
	    }
	  else if (operand->flags & MN10300_OPERAND_MDR)
	    {
	      char *start = input_line_pointer;
	      char c = get_symbol_end ();

	      if (strcmp (start, "mdr") != 0)
		{
		  *input_line_pointer = c;
		  input_line_pointer = hold;
		  str = hold;
		  goto error;
		}
	      *input_line_pointer = c;
	      goto keep_going;
	    }
	  else if (data_register_name (&ex))
	    {
	      input_line_pointer = hold;
	      str = hold;
	      goto error;
	    }
	  else if (address_register_name (&ex))
	    {
	      input_line_pointer = hold;
	      str = hold;
	      goto error;
	    }
	  else if (other_register_name (&ex))
	    {
	      input_line_pointer = hold;
	      str = hold;
	      goto error;
	    }
	  else if (*str == ')' || *str == '(')
	    {
	      input_line_pointer = hold;
	      str = hold;
	      goto error;
	    }
	  else
	    {
	      expression (&ex);
	    }

	  switch (ex.X_op) 
	    {
	    case O_illegal:
	      errmsg = "illegal operand";
	      goto error;
	    case O_absent:
	      errmsg = "missing operand";
	      goto error;
	    case O_register:
	      if (operand->flags & (MN10300_OPERAND_DREG 
				    | MN10300_OPERAND_AREG) == 0)
		{
		  input_line_pointer = hold;
		  str = hold;
		  goto error;
		}
		
	      if (opcode->format == FMT_D1 || opcode->format == FMT_S1)
		extra_shift = 8;
	      else if (opcode->format == FMT_D2 || opcode->format == FMT_D4
		       || opcode->format == FMT_S2 || opcode->format == FMT_S4
		       || opcode->format == FMT_S6 || opcode->format == FMT_D5)
		extra_shift = 16;
	      else
		extra_shift = 0;
	      
	      mn10300_insert_operand (&insn, &extension, operand,
				      ex.X_add_number, (char *) NULL,
				      0, extra_shift);

	      break;

	    case O_constant:
	      /* If this operand can be promoted, and it doesn't
		 fit into the allocated bitfield for this insn,
		 then promote it (ie this opcode does not match).  */
	      if (operand->flags & MN10300_OPERAND_PROMOTE
		  && ! check_operand (insn, operand, ex.X_add_number))
		{
		  input_line_pointer = hold;
		  str = hold;
		  goto error;
		}

	      mn10300_insert_operand (&insn, &extension, operand,
				      ex.X_add_number, (char *) NULL,
				      0, 0);
	      break;

	    default:
	      /* If this operand can be promoted, then this opcode didn't
		 match since we can't know if it needed promotion!  */
	      if (operand->flags & MN10300_OPERAND_PROMOTE)
		{
		  input_line_pointer = hold;
		  str = hold;
		  goto error;
		}

	      /* We need to generate a fixup for this expression.  */
	      if (fc >= MAX_INSN_FIXUPS)
		as_fatal ("too many fixups");
	      fixups[fc].exp = ex;
	      fixups[fc].opindex = *opindex_ptr;
	      fixups[fc].reloc = BFD_RELOC_UNUSED;
	      ++fc;
	      break;
	    }

keep_going:
	  str = input_line_pointer;
	  input_line_pointer = hold;

	  while (*str == ' ' || *str == ',' || *str == '[' || *str == ']')
	    ++str;

	}

      /* Make sure we used all the operands!  */
      if (*str != ',')
	match = 1;

    error:
      if (match == 0)
        {
	  next_opcode = opcode + 1;
	  if (next_opcode->opcode != 0 && !strcmp(next_opcode->name, opcode->name))
	    {
	      opcode = next_opcode;
	      continue;
	    }
	  
	  as_bad ("%s", errmsg);
	  return;
        }
      break;
    }
      
  while (isspace (*str))
    ++str;

  if (*str != '\0')
    as_bad ("junk at end of line: `%s'", str);

  input_line_pointer = str;

  /* Determine the size of the instruction.  */
  if (opcode->format == FMT_S0)
    size = 1;

  if (opcode->format == FMT_S1 || opcode->format == FMT_D0)
    size = 2;

  if (opcode->format == FMT_S2 || opcode->format == FMT_D1)
    size = 3;

  if (opcode->format == FMT_S4)
    size = 5;

  if (opcode->format == FMT_S6 || opcode->format == FMT_D5)
    size = 7;

  if (opcode->format == FMT_D2)
    size = 4;

  if (opcode->format == FMT_D4)
    size = 6;

  /* Write out the instruction.  */

  f = frag_more (size);
  number_to_chars_bigendian (f, insn, size > 4 ? 4 : size);
  if (size > 4)
    number_to_chars_bigendian (f + 4, extension, size - 4);
}


/* if while processing a fixup, a reloc really needs to be created */
/* then it is done here */
                 
arelent *
tc_gen_reloc (seg, fixp)
     asection *seg;
     fixS *fixp;
{
  arelent *reloc;
  reloc = (arelent *) bfd_alloc_by_size_t (stdoutput, sizeof (arelent));
  reloc->sym_ptr_ptr = &fixp->fx_addsy->bsym;
  reloc->address = fixp->fx_frag->fr_address + fixp->fx_where;
  reloc->howto = bfd_reloc_type_lookup (stdoutput, fixp->fx_r_type);
  if (reloc->howto == (reloc_howto_type *) NULL)
    {
      as_bad_where (fixp->fx_file, fixp->fx_line,
                    "reloc %d not supported by object file format", (int)fixp->fx_r_type);
      return NULL;
    }
  reloc->addend = fixp->fx_addnumber;
  /*  printf("tc_gen_reloc: addr=%x  addend=%x\n", reloc->address, reloc->addend); */
  return reloc;
}

int
md_estimate_size_before_relax (fragp, seg)
     fragS *fragp;
     asection *seg;
{
  return 0;
} 

long
md_pcrel_from (fixp)
     fixS *fixp;
{
  if (fixp->fx_addsy != (symbolS *) NULL && ! S_IS_DEFINED (fixp->fx_addsy))
    {
      /* The symbol is undefined.  Let the linker figure it out.  */
      return 0;
    }
  return fixp->fx_frag->fr_address + fixp->fx_where;
}

int
md_apply_fix3 (fixp, valuep, seg)
     fixS *fixp;
     valueT *valuep;
     segT seg;
{
  valueT value;
  char *where;

  fixp->fx_done = 1;
  return 0;

  if (fixp->fx_addsy == (symbolS *) NULL)
    {
      value = *valuep;
      fixp->fx_done = 1;
    }
  else if (fixp->fx_pcrel)
    value = *valuep;
  else
    {
      value = fixp->fx_offset;
      if (fixp->fx_subsy != (symbolS *) NULL)
	{
	  if (S_GET_SEGMENT (fixp->fx_subsy) == absolute_section)
	    value -= S_GET_VALUE (fixp->fx_subsy);
	  else
	    {
	      /* We don't actually support subtracting a symbol.  */
	      as_bad_where (fixp->fx_file, fixp->fx_line,
			    "expression too complex");
	    }
	}
    }

  /* printf("md_apply_fix: value=0x%x  type=%d\n",  value, fixp->fx_r_type); */

  if ((int) fixp->fx_r_type >= (int) BFD_RELOC_UNUSED)
    {
      int opindex;
      const struct mn10300_operand *operand;
      char *where;
      unsigned long insn, extension;

      opindex = (int) fixp->fx_r_type - (int) BFD_RELOC_UNUSED;
      operand = &mn10300_operands[opindex];

      /* Fetch the instruction, insert the fully resolved operand
         value, and stuff the instruction back again.

	 Note the instruction has been stored in little endian
	 format!  */
      where = fixp->fx_frag->fr_literal + fixp->fx_where;

      insn = bfd_getl32((unsigned char *) where);
      extension = 0;
      mn10300_insert_operand (&insn, &extension, operand,
			      (offsetT) value, fixp->fx_file,
			      fixp->fx_line, 0);
      bfd_putl32((bfd_vma) insn, (unsigned char *) where);

      if (fixp->fx_done)
	{
	  /* Nothing else to do here. */
	  return 1;
	}

      /* Determine a BFD reloc value based on the operand information.  
	 We are only prepared to turn a few of the operands into relocs. */

	{
	  as_bad_where(fixp->fx_file, fixp->fx_line,
		       "unresolved expression that must be resolved");
	  fixp->fx_done = 1;
	  return 1;
	}
    }
  else if (fixp->fx_done)
    {
      /* We still have to insert the value into memory!  */
      where = fixp->fx_frag->fr_literal + fixp->fx_where;
      if (fixp->fx_size == 1)
	*where = value & 0xff;
      if (fixp->fx_size == 2)
	bfd_putl16(value & 0xffff, (unsigned char *) where);
      if (fixp->fx_size == 4)
	bfd_putl32(value, (unsigned char *) where);
    }

  fixp->fx_addnumber = value;
  return 1;
}

/* Insert an operand value into an instruction.  */

static void
mn10300_insert_operand (insnp, extensionp, operand, val, file, line, shift)
     unsigned long *insnp;
     unsigned long *extensionp;
     const struct mn10300_operand *operand;
     offsetT val;
     char *file;
     unsigned int line;
     unsigned int shift;
{
  /* No need to check 32bit operands for a bit.  Note that
     MN10300_OPERAND_SPLIT is an implicit 32bit operand.  */
  if (operand->bits != 32
      && (operand->flags & MN10300_OPERAND_SPLIT) == 0)
    {
      long min, max;
      offsetT test;

      if ((operand->flags & MN10300_OPERAND_SIGNED) != 0)
	{
	  max = (1 << (operand->bits - 1)) - 1;
	  min = - (1 << (operand->bits - 1));
	}
      else
        {
          max = (1 << operand->bits) - 1;
          min = 0;
        }

      test = val;


      if (test < (offsetT) min || test > (offsetT) max)
        {
          const char *err =
            "operand out of range (%s not between %ld and %ld)";
          char buf[100];

          sprint_value (buf, test);
          if (file == (char *) NULL)
            as_warn (err, buf, min, max);
          else
            as_warn_where (file, line, err, buf, min, max);
        }
    }

  if ((operand->flags & MN10300_OPERAND_SPLIT) != 0)
    {
      *insnp |= (val >> 32 - operand->bits) & ((1 << operand->bits) - 1);
      *extensionp |= ((val & ((1 << (32 - operand->bits)) - 1))
		      << operand->shift);
    }
  else if ((operand->flags & MN10300_OPERAND_EXTENDED) == 0)
    {
      *insnp |= (((long) val & ((1 << operand->bits) - 1))
		 << (operand->shift + shift));

      if ((operand->flags & MN10300_OPERAND_REPEATED) != 0)
	*insnp |= (((long) val & ((1 << operand->bits) - 1))
		   << (operand->shift + shift + 2));
    }
  else
    {
      *extensionp |= (((long) val & ((1 << operand->bits) - 1))
		      << (operand->shift + shift));

      if ((operand->flags & MN10300_OPERAND_REPEATED) != 0)
	*extensionp |= (((long) val & ((1 << operand->bits) - 1))
			<< (operand->shift + shift + 2));
    }
}

static unsigned long
check_operand (insn, operand, val)
     unsigned long insn;
     const struct mn10300_operand *operand;
     offsetT val;
{
  /* No need to check 32bit operands for a bit.  Note that
     MN10300_OPERAND_SPLIT is an implicit 32bit operand.  */
  if (operand->bits != 32
      && (operand->flags & MN10300_OPERAND_SPLIT) == 0)
    {
      long min, max;
      offsetT test;

      if ((operand->flags & MN10300_OPERAND_SIGNED) != 0)
	{
	  max = (1 << (operand->bits - 1)) - 1;
	  min = - (1 << (operand->bits - 1));
	}
      else
        {
          max = (1 << operand->bits) - 1;
          min = 0;
        }

      test = val;


      if (test < (offsetT) min || test > (offsetT) max)
	return 0;
      else
	return 1;
    }
  return 1;
}
