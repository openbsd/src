
/* generate instructions for Z8KSIM
   Copyright (C) 1992, 1993 Free Software Foundation, Inc.

This file is part of Z8KSIM

Z8KSIM is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

Z8KSIM is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Z8KZIM; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

/* This program generates the code which emulates each of the z8k
   instructions

   code goes into three files, tc-gen1.h, tc-gen2.h and tc-gen3.h.
   which file being made depends upon the options

   -1  tc-gen1.h contains the fully expanded code for some selected
       opcodes, (those in the quick.c list)

   -2   tc-gen2.h contains a list of pointers to functions, one for each
   opcode.  It points to functions in tc-gen3.h and tc-gen1.h
   depending upon quick.c

   -3   tc-gen3.h contains all the opcodes in unexpanded form.

   -b3   tc-genb3.h same as -3 but for long pointers

   -m  regenerates list.c, which is an inverted list of opcodes to
       pointers into the z8k dissassemble opcode table, it's just there
       to makes things faster.
   */

/* steve chamberlain
   sac@cygnus.com */

#include "config.h"

#include <ansidecl.h>
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#else
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#endif

#define NICENAMES

#define DEFINE_TABLE
#include "../opcodes/z8k-opc.h"

#define NOPS 500

#define DIRTY_HACK 0 /* Enable if your gcc can't cope with huge tables */
extern short z8k_inv_list[];
struct opcode_value
{
  int n;
  struct opcode_value *next;
};

#define NICENAMES
int BIG;

static char *reg_names[] =
{"bad", "src", "dst", "aux_a", "aux_b", "aux_r", "aux_x"};

#define IS_DST(x) ((x & 0xf) == 2)
#define IS_SRC(x) ((x & 0xf)==1)
#define SIZE_ADDRESS (BIG ? 8 : 4)	/* number of nibbles in a ptr*/

static int file;
static int makelist;

static int nibs = 0;

static char *current_size;
static char *current_name;
static char current_word0[40];
static char current_byte0[40];
static char current_byte1[40];
static int indent;
static char *p;
static char *d;

struct opcode_value *list[NOPS];

static opcode_entry_type *
lookup_inst (what)
     int what;
{
  if (makelist)
    {

      int nibl_index;
      int nibl_matched;
      unsigned short instr_nibl;
      unsigned short tabl_datum, datum_class, datum_value;
      char instr_nibbles[8];

      opcode_entry_type *ptr = z8k_table;

      nibl_matched = 0;

      instr_nibbles[3] = (what >> 0) & 0xf;
      instr_nibbles[2] = (what >> 4) & 0xf;
      instr_nibbles[1] = (what >> 8) & 0xf;
      instr_nibbles[0] = (what >> 12) & 0xf;

      while (ptr->name)
	{
	  nibl_matched = 1;
	  for (nibl_index = 0; nibl_index < 4 && nibl_matched; nibl_index++)
	    {
	      instr_nibl = instr_nibbles[nibl_index];

	      tabl_datum = ptr->byte_info[nibl_index];
	      datum_class = tabl_datum & CLASS_MASK;
	      datum_value = ~CLASS_MASK & tabl_datum;

	      switch (datum_class)
		{
		case CLASS_BIT_1OR2:
		  if (datum_value != (instr_nibl & ~0x2))
		    nibl_matched = 0;
		  break;

		case CLASS_BIT:
		  if (datum_value != instr_nibl)
		    nibl_matched = 0;
		  break;
		case CLASS_00II:
		  if (!((~instr_nibl) & 0x4))
		    nibl_matched = 0;
		  break;
		case CLASS_01II:
		  if (!(instr_nibl & 0x4))
		    nibl_matched = 0;
		  break;
		case CLASS_0CCC:
		  if (!((~instr_nibl) & 0x8))
		    nibl_matched = 0;
		  break;
		case CLASS_1CCC:
		  if (!(instr_nibl & 0x8))
		    nibl_matched = 0;
		  break;
		case CLASS_0DISP7:
		  if (!((~instr_nibl) & 0x8))
		    nibl_matched = 0;
		  nibl_index += 1;
		  break;
		case CLASS_1DISP7:
		  if (!(instr_nibl & 0x8))
		    nibl_matched = 0;
		  nibl_index += 1;
		  break;
		case CLASS_REGN0:
		  if (instr_nibl == 0)
		    nibl_matched = 0;
		  break;
		default:
		  break;
		}
	    }
	  if (nibl_matched)
	    {
	      return ptr;
	    }
	  ptr++;
	}
      return 0;
    }
  else
    {

      if (z8k_inv_list[what] < 0)
	return 0;
      return z8k_table + z8k_inv_list[what];
    }
}

static char *
insn_4 (n)
     int n;
{
  switch (n)
    {
    case 1:
      return "((iwords_0>>8) & 0xf)";
    case 2:
      return "((ibytes_1 >> 4) & 0xf)";
    case 3:
      return "((ibytes_1) & 0xf)";
    case 4:
      return "((ibytes_2>>4) & 0xf)";
    case 5:
      return "((ibytes_2) & 0xf)";
    case 6:
      return "((ibytes_3 >> 4) & 0xf)";
    case 7:
      return "((ibytes_3) & 0xf)";
    default:
      return "****";
    }
}

char *
ptr_mode ()
{
  if (BIG)
    {
      return "ptr_long";
    }
  return "word";
}

static
char *
ptr_size ()
{
  if (BIG)
    return "4";
  return "2";
}

static char *
reg_n (x)
     unsigned int x;
{
  return reg_names[x & 0xf];
}

char *
stack_ptr ()
{
  return BIG ? "14" : "15";
}

char *
mem ()
{
#if 0
  return BIG ? "segmem" : "unsegmem";
#else
  return "mem";
#endif
}

int
match (a)
     char *a;
{
  if (strncmp (p, a, strlen (a)) == 0)
    {
      p += strlen (a);
      return 1;
    }
  return 0;
}

static
void
sub (y)
     char *y;
{
  sprintf (d, "%s", y);
  d += strlen (d);
}

static char *
insn_16 (n)
     int n;
{
  switch (n)
    {
    case 0:
      return "(iwords_0)";
    case 4:
      return "(iwords_1)";
    case 8:
      return "(iwords_2)";
    case 12:
      return "(iwords_3)";
    default:
      return "****";
    }
}

static
char *
insn_32 (n)
     int n;
{
  switch (n)
    {
    case 0:
      return "((iwords_0<<16) | (iwords_1))";
    case 4:
      return "((iwords_1<<16) | (iwords_2))";
    case 8:
      return "((iwords_2<<16) | (iwords_3))";
    default:
      return "****";
    }
}

static char *
size_name (x)
     int x;
{
  switch (x)
    {
    case 8:
      return "byte";
    case 16:
      return "word";
    case 32:
      return "long";
    case 64:
      return "quad";
    }
  return "!!";
}

/*VARARGS*/
void
emit (string, a1, a2, a3, a4, a5)
     char *string;
     char* a1;
     char* a2;
     char* a3;
     char* a4;
     char* a5;
{
  int indent_inc = 0;
  int indent_dec = 0;
  int i;
  char buffer[1000];

  d = buffer;
  p = string;

  while (*p)
    {
      if (match ("<fop>"))
	{
	  if (BIG)
	    {
	      sub ("bfop");
	    }
	  else
	    {
	      sub ("sfop");
	    }
	}
      else if (match ("<iptr>"))
	{
	  if (BIG)
	    {
	      switch (nibs)
		{
		case 4:
		  sub ("(((iwords_1 << 8) | (iwords_2)) & 0x7fffff)");
		  break;
		default:
		  sub ("fail(context,124)");
		  break;
		}
	    }
	  else
	    {
	      switch (nibs)
		{
		case 4:
		  sub ("iwords_1");
		  break;
		default:
		  sub ("fail(context,123)");
		  break;
		}
	    }
	}
      else if (match ("<name>"))
	{
	  sub (current_name);
	}
      else if (match ("<size>"))
	{
	  sub (current_size);
	}
      else if (match ("<insn_4>"))
	{
	  sub (insn_4 (nibs));
	}
      else if (match ("<insn_16>"))
	{
	  sub (insn_16 (nibs));
	}
      else if (match ("<insn_32>"))
	{
	  sub (insn_32 (nibs));
	}
      else if (match ("iwords_0"))
	{
	  sub (current_word0);
	}
      else if (match ("ibytes_0"))
	{
	  sub (current_byte0);
	}
      else if (match ("<ibytes_1>"))
	{
	  sub (current_byte1);
	}
      else if (match ("<next_size>"))
	{
	  if (strcmp (current_size, "word") == 0)
	    sub ("long");
	  if (strcmp (current_size, "byte") == 0)
	    sub ("word");
	  else if (strcmp (current_size, "long") == 0)
	    sub ("quad");
	  else
	    abort ();
	}
      else if (match ("<addr_type>"))
	{
	  if (BIG)
	    sub ("unsigned long");
	  else
	    sub ("unsigned short");
	}

      else if (match ("<c_size>"))
	{
	  if (strcmp (current_size, "word") == 0)
	    sub ("short");
	  else if (strcmp (current_size, "byte") == 0)
	    sub ("char");
	  else if (strcmp (current_size, "long") == 0)
	    sub ("long");
	  else
	    abort ();
	}

      else if (match ("<pc>"))
	{
	  sub ("pc");
	}
      else if (match ("<mem>"))
	{
	  sub (mem ());
	}
      else if (match ("<sp>"))
	{
	  sub (stack_ptr ());
	}
      else if (match ("<ptr_size>"))
	{
	  sub (ptr_size ());
	}
      else if (match ("<ptr_mode>"))
	{
	  sub (ptr_mode ());
	}
      else if (match ("<insn_8>"))
	{
	  switch (nibs)
	    {
	    case 2:
	      sub ("(iwords_0&0xff)");
	      break;
	    case 4:
	      sub ("(iwords_1>>8)");
	      break;
	    case 6:
	      sub ("(iwords_1&0xff)");
	      break;
	    case 8:
	      sub ("(iwords_2>>8)");
	      break;
	    case 12:
	      sub ("(/* WHO */iwords_3&0xff)");
	      break;
	    default:
	      abort ();
	    }
	}
      else
	{
	  if (*p == '{')
	    indent_inc++;
	  if (*p == '}')
	    indent_dec++;
	  *d++ = *p;
	  p++;
	}
    }
  *d++ = 0;
  indent -= indent_dec;
  for (i = 0; i < indent; i++)
    printf ("\t");
  indent += indent_inc;
  printf (buffer, a1, a2, a3, a4, a5);

}

/* fetch the lvalues of the operands */
void
info_args (p)
     opcode_entry_type *p;
{
  unsigned int *s;

  int done_one_imm8 = 0;

  /* int done_read = 4;*/
  s = p->byte_info;
  nibs = 0;
  while (*s)
    {
      switch (*s & CLASS_MASK)
	{
	case CLASS_BIT_1OR2:
	  emit ("register unsigned int imm_src=(<insn_4>& 2)?2:1;\n");
	  break;
	case CLASS_BIT:
	  /* Just ignore these, we've already decoded this bit */
	  nibs++;
	  break;
	case CLASS_REGN0:
	case CLASS_REG:
	  /* this nibble tells us which register to use as an arg,
	     if we've already gobbled the nibble we know what to use */
	  {
	    int regname = *s & 0xf;

	    emit ("register unsigned int reg_%s=<insn_4>;\n",
		  reg_names[regname]);

	    nibs++;
	  }
	  break;
	case CLASS_ADDRESS:
	  emit ("register unsigned base_%s=<iptr>;\n", reg_n (*s));

	  nibs += SIZE_ADDRESS;

	  break;
	case CLASS_01II:
	case CLASS_00II:
	  emit ("register unsigned int imm_src=<insn_4>&0x2;\n");
	  nibs++;
	  break;
	case CLASS_FLAGS:
		emit ("register unsigned int imm_src=<insn_4>;\n");
		nibs++;
break;	  
	case CLASS_IMM:
	  /* Work out the size of the think to fetch */

	  {
	    switch (*s & ~CLASS_MASK)
	      {
	      case ARG_IMM16:
		emit ("register unsigned imm_src=<insn_16>;\n");
		nibs += 4;
		break;
	      case ARG_IMM32:
		emit ("register unsigned int imm_src= %s;\n", insn_32 (nibs));
		nibs += 8;
		break;
	      case ARG_IMM4:
		emit ("register unsigned int imm_src=<insn_4>;\n");
		nibs++;
		break;
	      case ARG_IMM2:
		emit ("register unsigned int imm_src=<insn_4> & 0x2;\n");
		nibs++;
		break;

	      case ARG_IMM4M1:
		emit ("register unsigned int imm_src=(<insn_4> + 1);\n");
		nibs++;
		break;
	      case ARG_IMM_1:
		emit ("register unsigned int imm_src=1;\n");
		break;
	      case ARG_IMM_2:
		emit ("register unsigned int imm_src=2;\n");
		break;
	      case ARG_NIM8:
		emit ("register unsigned int imm_src=-<insn_8>;\n");
		nibs += 2;
		break;
	      case ARG_IMM8:
		if (!done_one_imm8)
		  {
		    emit ("register unsigned int imm_src=<insn_8>;\n");
		    nibs += 2;
		    done_one_imm8 = 1;
		  }
		break;
	      default:
		emit ("register int fail%d=fail(context,1);\n", nibs);
		break;
	      }
	    break;

	case CLASS_DISP8:
	    /* We can't use `(char)' since char might be unsigned.
	       We can't use `(signed char)' because the compiler might be K&R.
	       This seems safe, since it only assumes that bytes are 8
	       bits.  */
	    emit ("register unsigned int oplval_dst=((ibytes_1 << (sizeof (int) * 8 - 8)) >> (sizeof (int) * 8 - 9)) + pc;\n");
#if 0
	    /* Original code: fails if characters are unsigned.  */
	    emit ("register unsigned int oplval_dst=(((char)ibytes_1)<<1) + pc;\n");
#endif
	    nibs += 2;
	    break;
	case CLASS_CC:
	    emit ("register unsigned int op_cc=<insn_4>;\n");
	    nibs++;
	    break;
	default:
	    emit ("register int FAIL%d=fail(context,2);\n", nibs);
	    break;
	  }
	  ;
	  /* work out how to fetch the immediate value */
	}

      s++;
    }
}

void
info_special (p, getdst, nostore, before, nosrc)
     opcode_entry_type *p;
     int *getdst;
     int *nostore;
     int *before;
     int *nosrc;
{
  switch (p->opcode)
    {
    case OPC_exts:
    case OPC_extsb:
    case OPC_extsl:
      *nostore = 1;
      *nosrc = 1;
      break;
    case OPC_ldm:
      *nostore = 1;
      *nosrc = 1;
      break;
    case OPC_negb:
    case OPC_neg:
    case OPC_sla:
    case OPC_slab:
    case OPC_slal:
    case OPC_sda:
    case OPC_sdab:
    case OPC_sdal:
    case OPC_com:
    case OPC_comb:
    case OPC_adc:
    case OPC_sbc:
    case OPC_nop:
    case OPC_adcb:
    case OPC_add:
    case OPC_addb:
    case OPC_addl:
    case OPC_inc:
    case OPC_sub:
    case OPC_subb:
    case OPC_subl:
    case OPC_and:
    case OPC_andb:
    case OPC_xorb:
    case OPC_xor:
      break;

    case OPC_mult:
    case OPC_multl:
    case OPC_div:
    case OPC_divl:

      *nostore = 1;
      break;

    case OPC_testb:
    case OPC_test:
    case OPC_testl:
    case OPC_cp:
    case OPC_cpb:
    case OPC_cpl:
    case OPC_bit:
      *nostore = 1;
      *before = 0;
      break;

    case OPC_bpt:
    case OPC_jr:
    case OPC_jp:
    case OPC_ret:
    case OPC_call:
    case OPC_tcc:
      *nosrc = 1;
      *nostore = 1;
      *before = 1;
      break;
    case OPC_sc:
      *nostore = 1;
      *before = 0;
      break;
    case OPC_clrb:
    case OPC_clr:
      *before = 1;
      *nosrc = 1;
      break;
    case OPC_ldi:
    case OPC_ldib:
    case OPC_lddb:
    case OPC_ldd:

      *before = 1;
      *nostore = 1;
      *nosrc = 1;
      break;
    case OPC_ldk:
    case OPC_ld:
    case OPC_ldb:
    case OPC_ldl:
      *before = 1;
      *getdst = 0;
      break;
    case OPC_push:
    case OPC_pushl:
    case OPC_pop:
    case OPC_popl:
      *before = 1;
      *getdst = 0;
      break;
    case OPC_lda:
      *nosrc = 1;
      break;
    }
}

/* calculate the lvalues required for the opcode */
void
info_lvals (p)
     opcode_entry_type *p;
{
  /* emit code to work out lvalues, if any */
  unsigned int *i = p->arg_info;

  while (*i)
    {
      current_name = reg_n (*i);
      current_size = size_name (p->type);
      switch (*i & CLASS_MASK)
	{
	case CLASS_X:
	  /* address(reg) */
	  emit ("register  <addr_type> oplval_<name>= ((base_<name> + (short)get_word_reg(context,reg_<name>)) & 0xffff) + (base_<name> & ~0xffff);\n");
	  break;
	case CLASS_IR:
	  /* Indirect register */
	  emit ("register  int oplval_<name> = get_<ptr_mode>_reg(context,reg_<name>);\n");
	  break;
	case CLASS_DA:
	  emit ("register  int oplval_<name>=base_<name>;\n");
	  break;
	case CLASS_IMM:
	case CLASS_REG_WORD:
	case CLASS_REG_LONG:
	case CLASS_REG_BYTE:
	case CLASS_PR:
	  break;
	case CLASS_BA:
	  emit ("register  int oplval_<name> = get_<ptr_mode>_reg(context,reg_<name>) + (short)(imm_src);\n");
	  break;
	case CLASS_BX:
	  emit ("register  int oplval_<name> = get_<ptr_mode>_reg(context,reg_<name>)\n");
	  emit ("  + get_word_reg(context,reg_aux_x);\n");
	  break;
	}
      i++;
    }
}

/* emit code to fetch the args from calculated lvalues */
int allregs;
void
info_fetch (p, getdst)
     opcode_entry_type *p;
     int getdst;
{
  unsigned int *i = p->arg_info;
  int had_src = 0;

  allregs = 1;
  while (*i)
    {

      current_name = reg_n (*i);
      current_size = size_name (p->type);
      switch (*i & CLASS_MASK)
	{
	case CLASS_X:
	case CLASS_IR:
	case CLASS_BA:
	case CLASS_BX:
	case CLASS_DA:
	  if (!getdst && IS_DST (*i))
	    break;
	  emit ("register int op_<name>= get_<size>_<mem>_da(context,oplval_<name>);\n");
	  allregs = 0;
	  break;
	case CLASS_IMM:
	  if (!had_src)
	    {
	      if (p->opcode == OPC_out ||
		  p->opcode == OPC_outb ||
		  p->opcode == OPC_sout ||
		  p->opcode == OPC_soutb)
		{
		  /* The imm is a dest here */
		  emit ("register int op_dst = imm_src;\n");
		}
	      else
		{
		  emit ("register int op_src = imm_src;\n");
		}
	    }
	  break;
	case CLASS_REG_QUAD:
	  if (!getdst && IS_DST (*i))
	    break;
	  had_src |= IS_SRC (*i);
	  emit ("UDItype op_<name> ;\n");

	  break;
	case CLASS_REG_WORD:
	  if (!getdst && IS_DST (*i))
	    break;
	  had_src |= IS_SRC (*i);
	  emit ("register int op_<name> = get_word_reg(context,reg_<name>);\n");
	  break;

	case CLASS_REG_LONG:
	  if (!getdst && IS_DST (*i))
	    break;
	  had_src |= IS_SRC (*i);
	  emit ("register int op_<name> = get_long_reg(context,reg_<name>);\n");
	  break;
	case CLASS_REG_BYTE:
	  if (!getdst && IS_DST (*i))
	    break;
	  had_src |= IS_SRC (*i);
	  emit ("register int op_<name> = get_byte_reg(context,reg_<name>);\n");
	  break;
	}
      i++;
    }
}

static void
normal_flags (p, s, neg)
     opcode_entry_type *p;
     char *s;
{
  emit (" %s;\n", s);
  emit ("NORMAL_FLAGS(context,%d, tmp,  op_dst, op_src,%d); \n", p->type,neg);
}

static void
test_normal_flags (p, s, opt)
     opcode_entry_type *p;
     char *s;
     int opt;
{
  emit (" %s;\n", s);
  if (0 && opt)
    {
      emit ("context->broken_flags = TST_FLAGS;\n");
      emit ("context->size = %d;\n", p->type);
    }
  else
    {
      emit ("TEST_NORMAL_FLAGS(context,%d, tmp); \n", p->type);
    }

}

static void
optimize_normal_flags (p, s,neg)
     opcode_entry_type *p;
     char *s;
{
  emit (" %s;\n", s);
#if 0
  emit ("context->broken_flags = CMP_FLAGS;\n");
#else
  emit ("NORMAL_FLAGS(context,%d, tmp,  op_dst, op_src,%d); \n", p->type, neg);
#endif
}

static
void
jp (p)
     opcode_entry_type *p;
{

  emit ("if(op_cc == 8 || COND(context,op_cc)) pc = oplval_dst;\n");
}

static void
jr (p)
     opcode_entry_type *p;
{
  emit ("if(op_cc == 8 || COND(context,op_cc)) pc = oplval_dst;\n");
}

static void
ret (p)
     opcode_entry_type *p;
{
  emit ("if(op_cc == 8 || COND(context,op_cc))\n{\n");
  emit ("pc = get_<ptr_mode>_<mem>_ir(context,<sp>);\n");
  emit ("put_<ptr_mode>_reg(context,<sp>, get_<ptr_mode>_reg(context,<sp>) + <ptr_size>);\n");
  emit ("};\n");
}

static void
call (p)
     opcode_entry_type *p;
{
  emit ("put_<ptr_mode>_reg(context,<sp>,tmp =  get_<ptr_mode>_reg(context,<sp>) - <ptr_size>);\n");
  emit ("put_<ptr_mode>_<mem>_da(context,tmp, pc);\n");
  emit ("pc = oplval_dst;\n");
}

static void
push (p)
     opcode_entry_type *p;
{
  emit ("tmp = op_src;\n");
  emit ("oplval_dst -= %d;\n", p->type / 8);
  emit ("put_<ptr_mode>_reg(context,reg_dst, oplval_dst);\n");
}

static void
pop (p)
     opcode_entry_type *p;
{
  emit ("tmp = op_src;\n");
  emit ("put_<ptr_mode>_reg(context,reg_src, oplval_src + %d);\n", p->type / 8);
}

static void
ld (p)
     opcode_entry_type *p;
{
  emit ("tmp = op_src;\n");
}

static void
sc ()
{
  emit ("support_call(context,imm_src);\n");
}

static void
bpt ()
{
  emit ("pc -=2; \n");
  emit ("context->exception = SIM_BREAKPOINT;\n");
}

static void
ldi (p, size, inc)
     opcode_entry_type *p;
     int size;
     int inc;
{
  int dinc = (size / 8) * inc;

  current_size = size_name (size);
  emit ("{ \n");
  emit ("int type = %s;\n", insn_4 (7));
  emit ("int rs = get_<ptr_mode>_reg(context,reg_src);\n");
  emit ("int rd = get_<ptr_mode>_reg(context,reg_dst);\n");
  emit ("int rr = get_word_reg(context,reg_aux_r);\n");
  emit ("do {\n");
  emit ("put_<size>_<mem>_da(context,rd, get_<size>_<mem>_da(context,rs));\n");
  emit ("rd += %d;\n", dinc);
  emit ("rs += %d;\n", dinc);
  emit ("rr --;\n");
  emit ("context->cycles += 9;\n");
  emit ("} while (!type && rr != 0 && context->exception <= 1);\n");
  emit ("if (context->exception>1) pc -=4;\n");
  emit ("put_<ptr_mode>_reg(context,reg_src, rs);\n");
  emit ("put_<ptr_mode>_reg(context,reg_dst, rd);\n");
  emit ("put_word_reg(context,reg_aux_r, rr);\n");
  emit ("}\n");

}

static void
shift (p, arith)
     opcode_entry_type *p;
     int arith;
{

  /* We can't use `(char)' since char might be unsigned.
     We can't use `(signed char)' because the compiler might be K&R.
     This seems safe, since it only assumes that bytes are 8 bits.  */
  emit ("op_src = (op_src << (sizeof (int) * 8 - 8)) >> (sizeof (int) * 8 - 8);\n");
#if 0
  /* Original code: fails if characters are unsigned.  */
  emit ("op_src = (char)op_src;\n");
#endif
  emit ("if (op_src < 0) \n");
  emit ("{\n");
  emit ("op_src = -op_src;\n");
  emit ("op_dst = (%s <c_size>)op_dst;\n", arith ? "" : "unsigned");
  emit ("tmp = (%s op_dst) >> op_src;\n", arith ? "" : "(unsigned)");
  emit ("context->carry = op_dst >> (op_src-1);\n", p->type);
  emit ("}\n");
  emit ("else\n");
  emit ("{\n");
  emit ("tmp = op_dst << op_src;\n");
  emit ("context->carry = op_dst >> (%d - op_src);\n", p->type);
  emit ("}\n");
  emit ("context->zero = (<c_size>)tmp == 0;\n");
  emit ("context->sign = (int)((<c_size>)tmp) < 0;\n");
  emit ("context->overflow = ((int)tmp < 0) != ((int)op_dst < 0);\n");
  emit ("context->cycles += 3*op_src;\n");
  emit ("context->broken_flags = 0;\n");

}

static void
rotate (p, through_carry, size, left)
     opcode_entry_type *p;
     int through_carry;
     int size;
     int left;
{

  if (!left)
    {
      emit ("while (op_src--) {\n");
      emit ("int rotbit;\n");
      emit ("rotbit = op_dst & 1;\n");
      emit ("op_dst = ((unsigned)op_dst) >> 1;\n");

      if (through_carry)
	{
	  emit ("op_dst |= context->carry << %d;\n", size - 1);
	}
      else
	{
	  emit ("op_dst |= rotbit << %d;\n", size - 1);
	}
      emit ("context->carry = rotbit;\n");
      emit ("}\n");
    }
  else
    {
      emit ("while (op_src--) {\n");
      emit ("int rotbit;\n");

      emit ("rotbit = (op_dst >> (%d))&1;\n", size - 1);
      emit ("op_dst <<=1;\n");
      if (through_carry)
	{
	  emit ("if (context->carry) op_dst |=1;\n");
	}
      else
	{
	  emit ("if (rotbit) op_dst |= 1;\n");
	}
      emit ("context->carry = rotbit;\n");
      emit ("}\n");
    }
  emit ("tmp = (<c_size>)op_dst;\n");
  emit ("context->zero = tmp == 0;\n");
  emit ("context->sign = (int)tmp < 0;\n");
  emit ("context->overflow = ((int)tmp < 0) != ((int)op_dst < 0);\n");
  emit ("context->cycles += 3*op_src;\n");
  emit ("context->broken_flags = 0;\n");

}

static void
adiv (p)
     opcode_entry_type *p;
{
  emit ("if (op_src==0)\n");
  emit ("{\n");
  emit ("context->exception = SIM_DIV_ZERO;\n");
  emit ("}\n");
  emit ("else\n");
  emit ("{\n");

  if (p->type == 32)
    {
      emit ("op_dst.low = (int)get_long_reg(context,reg_dst+2);\n");
      emit ("op_dst.high = (int)get_long_reg(context,reg_dst+0);\n");
#ifdef __GNUC__
      emit ("tmp = (((long long)op_dst.high << 32) + (op_dst.low)) / (int)op_src;\n");
#else
      emit ("tmp = (long)op_dst.low / (int)op_src;\n");
#endif
      emit ("put_long_reg(context,reg_dst+2, tmp);\n");
#ifdef __GNUC__
      emit ("put_long_reg(context,reg_dst, (((long long)op_dst.high << 32) + (op_dst.low)) %% (int)op_src);\n");
#else
      emit ("put_long_reg(context,reg_dst, (int)op_dst.low %% (int)op_src);\n");
#endif

      emit ("context->zero = op_src == 0 || (op_dst.low==0 && op_dst.high==0);\n");
    }
  else
    {
      emit ("tmp = (long)op_dst / (short)op_src;\n");
      emit ("put_word_reg(context,reg_dst+1, tmp);\n");
      emit ("put_word_reg(context,reg_dst, (long) op_dst %% (short)op_src);\n");
      emit ("context->zero = op_src == 0 || op_dst==0;\n");
    }

  emit ("context->sign = (int)tmp < 0;\n");
  emit ("context->overflow =(tmp & 0x%x) != 0;\n",
	~((1 << (p->type)) - 1));
  emit ("context->carry = (tmp & 0x%x) != 0;\n",
	~(1 << (p->type)));

  emit ("}\n");
}

static void
dobit (p)
opcode_entry_type *p;
{
  emit("context->zero = (op_dst & (1<<op_src))==0;\n");
  emit("context->broken_flags = 0;\n");
}
static void
doset (p, v)
opcode_entry_type*p;
int v;
{
  if (v) 
    emit (" tmp = op_dst | (1<< op_src);\n");
  else
    emit (" tmp = op_dst & ~(1<< op_src);\n");
}

static void
mult (p)
     opcode_entry_type *p;
{

  if (p->type == 32)
    {
      emit ("op_dst.low =  get_long_reg(context,reg_dst+2);\n");
      emit ("tmp = op_dst.low * op_src;\n");
      emit ("put_long_reg(context,reg_dst+2, tmp);\n");
      emit ("put_long_reg(context,reg_dst, 0);\n");
    }
  else
    {
      emit ("op_dst =  get_word_reg(context,reg_dst+1);\n");
      emit ("tmp = op_dst * op_src;\n");
      emit ("put_long_reg(context,reg_dst, tmp);\n");
    }

  emit ("context->sign = (int)tmp < 0;\n");
  emit ("context->overflow =0;\n");
  emit ("context->carry = (tmp & 0x%x) != 0;\n", ~((1 << (p->type)) - 1));
  emit ("context->zero = tmp == 0;\n");

}

static void
exts (p)
     opcode_entry_type *p;
{
  /* Fetch the ls part of the src */
  current_size = size_name (p->type * 2);

  if (p->type == 32)
    {
      emit ("tmp= get_long_reg(context,reg_dst+2);\n");
      emit ("if (tmp & (1<<%d)) {\n", p->type - 1);
      emit ("put_long_reg(context,reg_dst, 0xffffffff);\n");
      emit ("}\n");
      emit ("else\n");
      emit ("{\n");
      emit ("put_long_reg(context,reg_dst, 0);\n");
      emit ("}\n");
    }
  else
    {
      emit ("tmp= get_<size>_reg(context,reg_dst);\n");
      emit ("if (tmp & (1<<%d)) {\n", p->type - 1);
      emit ("tmp |= 0x%x;\n", ~((1 << p->type) - 1));
      emit ("}\n");
      emit ("else\n");
      emit ("{\n");

      emit ("tmp &= 0x%x;\n", ((1 << p->type) - 1));
      emit ("}\n");
      emit ("put_<size>_reg(context,reg_dst, tmp);\n");
    }
}
doflag(on)
int on;
{
  /* Load up the flags */
  emit(" COND (context, 0x0b);\n");

  if (on)
    emit ("{ int on =1;\n ");
  else
    emit ("{ int on =0;\n ");

  emit ("if (imm_src & 1)\n");
  emit ("PSW_OVERFLOW = on;\n");

  emit ("if (imm_src & 2)\n");
  emit ("PSW_SIGN = on;\n");

  emit ("if (imm_src & 4)\n");
  emit ("PSW_ZERO = on;\n");

  emit ("if (imm_src & 8)\n");
  emit ("PSW_CARRY = on;\n");
  emit("}\n");


}
/* emit code to perform operation */
void
info_docode (p)
     opcode_entry_type *p;
{
  switch (p->opcode)
    {
    case OPC_clr:
    case OPC_clrb:
      emit ("tmp = 0;\n");
      break;
    case OPC_ex:
    case OPC_exb:

      emit ("tmp = op_src; \n");
      if (allregs)
	{
	  emit ("put_<size>_reg(context,reg_src, op_dst);\n");
	}
      else
	{
	  emit ("put_<size>_mem_da(context, oplval_src, op_dst);\n");
	}
      break;
    case OPC_adc:
    case OPC_adcb:
      normal_flags (p, "op_src += COND(context,7);tmp = op_dst + op_src ;",0);
      break;
    case OPC_sbc:
      normal_flags (p, "op_src +=  COND(context,7);tmp = op_dst - op_src ;",1);
      break;
    case OPC_nop:
      break;
    case OPC_com:
    case OPC_comb:
      test_normal_flags (p, "tmp = ~ op_dst", 1);
      break;
    case OPC_and:
    case OPC_andb:
      test_normal_flags (p, "tmp = op_dst & op_src", 1);
      break;
    case OPC_xor:
    case OPC_xorb:
      test_normal_flags (p, "tmp = op_dst ^ op_src", 1);
      break;
    case OPC_or:
    case OPC_orb:
      test_normal_flags (p, "tmp = op_dst | op_src", 1);
      break;
    case OPC_sla:
    case OPC_slab:
    case OPC_slal:
    case OPC_sda:
    case OPC_sdab:
    case OPC_sdal:
      shift (p, 1);
      break;

    case OPC_sll:
    case OPC_sllb:
    case OPC_slll:
    case OPC_sdl:
    case OPC_sdlb:
    case OPC_sdll:
      shift (p, 0);
      break;
    case OPC_rl:
      rotate (p, 0, 16, 1);
      break;
    case OPC_rlb:
      rotate (p, 0, 8, 1);
      break;
    case OPC_rr:
      rotate (p, 0, 16, 0);
      break;
    case OPC_rrb:
      rotate (p, 0, 8, 0);
      break;
    case OPC_rrc:
      rotate (p, 1, 16, 0);
      break;
    case OPC_rrcb:
      rotate (p, 1, 8, 0);
      break;
    case OPC_rlc:
      rotate (p, 1, 16, 1);
      break;
    case OPC_rlcb:
      rotate (p, 1, 8, 1);
      break;

    case OPC_extsb:
    case OPC_exts:
    case OPC_extsl:
      exts (p);
      break;
    case OPC_add:
    case OPC_addb:
    case OPC_addl:
    case OPC_inc:
    case OPC_incb:
      optimize_normal_flags (p, "tmp = op_dst + op_src",0);
      break;
    case OPC_testb:
    case OPC_test:
    case OPC_testl:
      test_normal_flags (p, "tmp = op_dst", 0);
      break;
    case OPC_cp:
    case OPC_cpb:
    case OPC_cpl:
      normal_flags (p, "tmp = op_dst - op_src",1);
      break;
    case OPC_negb:
    case OPC_neg:
      emit ("{\n");
      emit ("int op_src = -op_dst;\n");
      emit ("op_dst = 0;\n");
      optimize_normal_flags (p, "tmp = op_dst + op_src;\n",1);
      emit ("}");
      break;

    case OPC_sub:
    case OPC_subb:
    case OPC_subl:
    case OPC_dec:
    case OPC_decb:
      optimize_normal_flags (p, "tmp = op_dst - op_src",1);
      break;
    case OPC_bpt:
      bpt ();
      break;
    case OPC_jr:
      jr (p);
      break;
    case OPC_sc:
      sc ();
      break;
    case OPC_jp:
      jp (p);
      break;
    case OPC_ret:
      ret (p);
      break;
    case OPC_call:
      call (p);
      break;
    case OPC_tcc:
    case OPC_tccb:
      emit ("if(op_cc == 8 || COND(context,op_cc)) put_word_reg(context,reg_dst, 1);\n");
      break;
    case OPC_lda:
      emit ("tmp = oplval_src; \n");
      /*(((oplval_src) & 0xff0000) << 8) | (oplval_src & 0xffff); \n");*/
      break;
    case OPC_ldk:
    case OPC_ld:

    case OPC_ldb:
    case OPC_ldl:
      ld (p);
      break;
    case OPC_ldib:
      ldi (p, 8, 1);
      break;
    case OPC_ldi:
      ldi (p, 16, 1);
      break;

    case OPC_lddb:
      ldi (p, 8, -1);
      break;
    case OPC_ldd:
      ldi (p, 16, -1);
      break;

    case OPC_push:
    case OPC_pushl:
      push (p);
      break;

    case OPC_div:
    case OPC_divl:
      adiv (p);
      break;
    case OPC_mult:
    case OPC_multl:
      mult (p);
      break;
    case OPC_pop:
    case OPC_popl:
      pop (p);
      break;
    case OPC_set:
      doset (p,1);
      break;
    case OPC_res:
      doset (p,0);
      break;
    case OPC_bit:
      dobit(p);
      break;
    case OPC_resflg:
      doflag(0);
      break;
    case OPC_setflg:
      doflag(1);
      break;
    default:

      emit ("tmp = fail(context,%d);\n", p->opcode);
      break;
    }
}

/* emit code to store result in calculated lvalue */

void
info_store (p)
     opcode_entry_type *p;
{
  unsigned int *i = p->arg_info;

  while (*i)
    {
      current_name = reg_n (*i);
      current_size = size_name (p->type);

      if (IS_DST (*i))
	{
	  switch (*i & CLASS_MASK)
	    {
	    case CLASS_PR:
	      emit ("put_<ptr_mode>_reg(context,reg_<name>, tmp);\n");
	      break;
	    case CLASS_REG_LONG:
	    case CLASS_REG_WORD:
	    case CLASS_REG_BYTE:

	      emit ("put_<size>_reg(context,reg_<name>,tmp);\n");
	      break;
	    case CLASS_X:
	    case CLASS_IR:
	    case CLASS_DA:
	    case CLASS_BX:
	    case CLASS_BA:

	      emit ("put_<size>_<mem>_da(context,oplval_<name>, tmp);\n");
	      break;
	    case CLASS_IMM:
	      break;
	    default:
	      emit ("abort(); ");
	      break;
	    }

	}
      i++;
    }
}

static
void
mangle (p, shortcut, value)
     opcode_entry_type *p;
     int shortcut;
     int value;
{
  int nostore = 0;
  int extra;
  int getdst = 1;
  int before = 0;
  int nosrc = 0;

  emit ("/\052 %s \052/\n", p->nicename);
  if (shortcut)
    {
      emit ("int <fop>_%04x(context,pc)\n", value);
    }
  else
    {
      emit ("int <fop>_%d(context,pc,iwords0)\n", p->idx);
      emit ("int iwords0;\n");
    }
  emit ("sim_state_type *context;\n");
  emit ("int pc;\n");
  emit ("{\n");
  emit ("register unsigned int tmp;\n");
  if (shortcut)
    {
      emit ("register unsigned int iwords0 = 0x%x;\n", value);
    }

  /* work out how much bigger this opcode could be because it's large
     model */
  if (BIG)
    {
      int i;

      extra = 0;
      for (i = 0; i < 4; i++)
	{
	  if ((p->arg_info[i] & CLASS_MASK) == CLASS_DA
	      || (p->arg_info[i] & CLASS_MASK) == CLASS_X)
	    extra += 2;
	}
    }
  else
    {
      extra = 0;
    }
  printf ("			/* Length %d */ \n", p->length + extra);
  switch (p->length + extra)
    {
    case 2:
      emit ("pc += 2\n;");
      break;
    case 4:
      emit ("register unsigned int iwords1 = get_word_mem_da(context,pc+2);\n");
      emit ("pc += 4;\n");
      break;
    case 6:

      emit ("register unsigned int iwords1 = get_word_mem_da(context,pc+2);\n");
      emit ("register unsigned int iwords2 = get_word_mem_da(context,pc+4);\n");
      emit ("pc += 6;\n");
      break;
    case 8:
      emit ("register unsigned int iwords1 = get_word_mem_da(context,pc+2);\n");
      emit ("register unsigned int iwords2 = get_word_mem_da(context,pc+4);\n");
      emit ("register unsigned int iwords3 = get_word_mem_da(context,pc+6);\n");
      emit ("pc += 8;\n");
      break;
    default:
      break;

    }
  emit ("context->cycles += %d;\n", p->cycles);

  emit ("{\n");
  info_args (p);
  info_special (p, &getdst, &nostore, &before, &nosrc);

  info_lvals (p);
  if (!nosrc)
    {
      info_fetch (p, getdst);
    }

  if (before)
    {
      info_docode (p);
    }
  else
    {
      info_docode (p);
    }
  if (!nostore)
    info_store (p);
  emit ("}\n");
  emit ("return pc;\n");
  emit ("}\n");
}

void
static
one_instruction (i)
     int i;
{
  /* find the table entry */
  opcode_entry_type *p = z8k_table + i;

  if (!p)
    return;
  mangle (p, 0, 0);
}

void
add_to_list (ptr, value)
     struct opcode_value **ptr;
     int value;
{
  struct opcode_value *prev;

  prev = *ptr;
  *ptr = (struct opcode_value *) malloc (sizeof (struct opcode_value));

  (*ptr)->n = value;
  (*ptr)->next = prev;
}

void
build_list (i)
     int i;
{
  opcode_entry_type *p = lookup_inst (i);

  if (!p)
    return;
  add_to_list (&list[p->idx], i);
}

int
main (ac, av)
     int ac;
     char **av;
{
  int i;
  int needcomma = 0;

  makelist = 0;

  for (i = 1; i < ac; i++)
    {
      if (strcmp (av[i], "-m") == 0)
	makelist = 1;
      if (strcmp (av[i], "-1") == 0)
	file = 1;
      if (strcmp (av[i], "-2") == 0)
	file = 2;
      if (strcmp (av[i], "-3") == 0)
	file = 3;
      if (strcmp (av[i], "-b3") == 0)
	{
	  file = 3;
	  BIG = 1;
	}

    }
  if (makelist)
    {

      int i;
      needcomma = 0;
      printf ("short int z8k_inv_list[] = {\n");

      for (i = 0; i < 1 << 16; i++)
	{
	  opcode_entry_type *p = lookup_inst (i);

	  if(needcomma)
	    printf(",");
	  if ((i & 0xf) == 0)
	    printf ("\n");

#if 0
	  printf ("\n		/*%04x %s */", i, p ? p->nicename : "");
#endif

	  if (!p)
	    {
	      printf ("-1");
	    }
	  else
	    {
	      printf ("%d", p->idx);
	    }

	  if ((i & 0x3f) == 0 && DIRTY_HACK)
	    {
	      printf ("\n#ifdef __GNUC__\n");
	      printf ("};\n");
	      printf("short int int_list%d[] = {\n", i);
	      printf ("#else\n");
	      printf (",\n");
	      printf ("#endif\n");
	      needcomma = 0;
	    }
	  else
	    needcomma = 1;

	}
      printf ("};\n");
      return 1;
    }

  /* First work out which opcodes use which bit patterns,
     build a list of all matching bit pattens */
  for (i = 0; i < 1 << 16; i++)
    {
      build_list (i);
    }
#if DUMP_LIST
  for (i = 0; i < NOPS; i++)
    {
      struct opcode_value *p;

      printf ("%d,", i);
      p = list[i];
      while (p)
	{
	  printf (" %04x,", p->n);
	  p = p->next;
	}
      printf ("-1\n");
    }

#endif

  if (file == 1)
    {
      extern int quick[];

      /* Do the shortcuts */
      printf ("			/* SHORTCUTS */\n");
      for (i = 0; quick[i]; i++)
	{
	  int t = quick[i];

	  mangle (z8k_table + z8k_inv_list[t],
		  1,
		  t);
	}
    }
  if (file == 3)
    {
      printf ("			/* NOT -SHORTCUTS */\n");
      for (i = 0; i < NOPS; i++)
	{
	  if (list[i])
	    {
	      one_instruction (i);
	    }
	  else
	    {
	      emit ("int <fop>_%d(context,pc)\n", i);
	      printf ("sim_state_type *context;\n");
	      printf ("int pc;\n");
	      emit ("{ <fop>_bad1();return pc; }\n");
	    }
	}
      emit ("int <fop>_bad() ;\n");

      /* Write the jump table */
      emit ("int (*(<fop>_table[]))() = {");
      needcomma = 0;
      for (i = 0; i < NOPS; i++)
	{
	  if (needcomma)
	    printf (",");
	  emit ("<fop>_%d\n", i);
	  needcomma = 1;
	  if ((i & 0x3f) == 0 && DIRTY_HACK)
	    {
	      printf ("#ifdef __GNUC__\n");
	      printf ("};\n");
	      emit ("int (*(<fop>_table%d[]))() = {\n", i);
	      printf ("#else\n");
	      printf (",\n");
	      printf ("#endif\n");
	      needcomma = 0;
	    }
	}
      emit ("};\n");
    }

  if (file == 2)
    {
      extern int quick[];
      /* Static - since it's too be to be automatic on the apollo */
      static int big[64 * 1024];

      for (i = 0; i < 64 * 1024; i++)
	big[i] = 0;

      for (i = 0; quick[i]; i++)
	{
#if 0

	  printf ("extern int <fop>_%04x();\n", quick[i]);
#endif

	  big[quick[i]] = 1;
	}

      for (i = 0; i < NOPS; i++)
	{
#if 0
	  printf ("extern int fop_%d();\n", i);
#endif
	}
#if 0
      printf ("extern int fop_bad();\n");
#endif
      printf ("struct op_info op_info_table[] = {\n");
      for (i = 0; i < 1 << 16; i++)
	{
	  int inv = z8k_inv_list[i];
	  opcode_entry_type *p = z8k_table + inv;

	  if (needcomma)
	    printf (",");
#if 0
	  if (big[i])
	    {
	      printf ("<fop>_%04x", i);
	    }
	  else
#endif
	  if (inv >= 0)
	    {
	      printf ("%d", inv);
	    }
	  else
	    printf ("400");
	  if (inv >= 0)
	    {
	      printf ("		/* %04x %s */\n", i, p->nicename);
	    }
	  else
	    {
	      printf ("\n");
	    }
	  needcomma = 1;
	  if ((i & 0x3f) == 0 && DIRTY_HACK)
	    {
	      printf ("#ifdef __GNUC__\n");
	      printf ("}; \n");
	      printf ("struct op_info op_info_table%d[] = {\n", i);
	      printf ("#else\n");
	      printf (",\n");

	      printf ("#endif\n");
	      needcomma = 0;
	    }
	}
      printf ("};\n");

    }
  return 0;
}

char *
insn_ptr (n)
     int n;
{
  if (BIG)
    {
      abort ();
    }

  switch (n)
    {
    case 4:
      return "iwords_1";
    default:
      return "fail(context,123)";
    }
}

/* work out if the opcode only wants lvalues */
int
lvalue (p)
     opcode_entry_type *p;
{
  switch (p->opcode)
    {
    case OPC_lda:
      return 1;
    case OPC_call:
    case OPC_jp:
      return 1;
    default:
      return 0;
    }
}

int
info_len_in_words (o)
     opcode_entry_type *o;
{
  unsigned  int *p = o->byte_info;
  int nibs = 0;

  while (*p)
    {
      switch (*p & CLASS_MASK)
	{
	case CLASS_BIT:
	case CLASS_REGN0:
	case CLASS_REG:
	case CLASS_01II:
	case CLASS_00II:
	  nibs++;
	  break;
	case CLASS_ADDRESS:
	  nibs += SIZE_ADDRESS;
	  break;
	case CLASS_IMM:
	  switch (*p & ~CLASS_MASK)
	    {
	    case ARG_IMM16:
	      nibs += 4;
	      break;
	    case ARG_IMM32:
	      nibs += 8;
	      break;
	    case ARG_IMM2:
	    case ARG_IMM4:
	    case ARG_IMM4M1:
	    case ARG_IMM_1:
	    case ARG_IMM_2:
	    case ARG_IMMNMINUS1:
	      nibs++;
	      break;
	    case ARG_NIM8:

	    case ARG_IMM8:
	      nibs += 2;
	      break;
	    default:
	      abort ();
	    }
	  break;
	case CLASS_DISP:
	  switch (*p & ~CLASS_MASK)
	    {
	    case ARG_DISP16:
	      nibs += 4;
	      break;
	    case ARG_DISP12:
	      nibs += 3;
	      break;
	    case ARG_DISP8:
	      nibs += 2;
	      break;
	    default:
	      abort ();
	    }
	  break;
	case CLASS_0DISP7:
	case CLASS_1DISP7:
	case CLASS_DISP8:
	  nibs += 2;
	  break;
	case CLASS_BIT_1OR2:
	case CLASS_0CCC:
	case CLASS_1CCC:
	case CLASS_CC:
	  nibs++;
	  break;
	default:
	  emit ("don't know %x\n", *p);
	}
      p++;
    }

  return nibs / 4;		/* return umber of words */
}
