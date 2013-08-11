/* m88k.c -- Assembler for the Motorola 88000
   Contributed by Devon Bowen of Buffalo University
   and Torbjorn Granlund of the Swedish Institute of Computer Science.
   Copyright 1989, 1990, 1991, 1992, 1993, 1994, 1995, 1996, 1997, 1999,
   2000, 2001, 2002
   Free Software Foundation, Inc.

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

#include "as.h"
#include "safe-ctype.h"
#include "subsegs.h"
#include "m88k-opcode.h"

#if defined (OBJ_ELF)
#include "elf/m88k.h"
#endif

#ifdef BFD_ASSEMBLER
#define	RELOC_LO16	BFD_RELOC_LO16
#define	RELOC_HI16	BFD_RELOC_HI16
#define	RELOC_PC16	BFD_RELOC_18_PCREL_S2
#define	RELOC_PC26	BFD_RELOC_28_PCREL_S2
#define	RELOC_32	BFD_RELOC_32
#define NO_RELOC	BFD_RELOC_NONE
#endif

struct field_val_assoc
{
  char *name;
  unsigned val;
};

struct field_val_assoc m88100_cr_regs[] =
{
  {"PID", 0},
  {"PSR", 1},
  {"EPSR", 2},
  {"SSBR", 3},
  {"SXIP", 4},
  {"SNIP", 5},
  {"SFIP", 6},
  {"VBR", 7},
  {"DMT0", 8},
  {"DMD0", 9},
  {"DMA0", 10},
  {"DMT1", 11},
  {"DMD1", 12},
  {"DMA1", 13},
  {"DMT2", 14},
  {"DMD2", 15},
  {"DMA2", 16},
  {"SR0", 17},
  {"SR1", 18},
  {"SR2", 19},
  {"SR3", 20},

  {NULL, 0},
};

struct field_val_assoc m88110_cr_regs[] =
{
  {"PID", 0},
  {"PSR", 1},
  {"EPSR", 2},
  {"EXIP", 4},
  {"ENIP", 5},
  {"VBR", 7},
  {"SRX", 16},
  {"SR0", 17},
  {"SR1", 18},
  {"SR2", 19},
  {"SR3", 20},
  {"ICMD", 25},
  {"ICTL", 26},
  {"ISAR", 27},
  {"ISAP", 28},
  {"IUAP", 29},
  {"IIR", 30},
  {"IBP", 31},
  {"IPPU", 32},
  {"IPPL", 33},
  {"ISR", 34},
  {"ILAR", 35},
  {"IPAR", 36},
  {"DCMD", 40},
  {"DCTL", 41},
  {"DSAR", 42},
  {"DSAP", 43},
  {"DUAP", 44},
  {"DIR", 45},
  {"DBP", 46},
  {"DPPU", 47},
  {"DPPL", 48},
  {"DSR", 49},
  {"DLAR", 50},
  {"DPAR", 51},

  {NULL, 0},
};

struct field_val_assoc fcr_regs[] =
{
  {"FPECR", 0},
  {"FPHS1", 1},
  {"FPLS1", 2},
  {"FPHS2", 3},
  {"FPLS2", 4},
  {"FPPT", 5},
  {"FPRH", 6},
  {"FPRL", 7},
  {"FPIT", 8},

  {"FPSR", 62},
  {"FPCR", 63},

  {NULL, 0},
};

struct field_val_assoc cmpslot[] =
{
/* Integer	Floating point */
  {"nc", 0},
  {"cp", 1},
  {"eq", 2},
  {"ne", 3},
  {"gt", 4},
  {"le", 5},
  {"lt", 6},
  {"ge", 7},
  {"hi", 8},	{"ou", 8},
  {"ls", 9},	{"ib", 9},
  {"lo", 10},	{"in", 10},
  {"hs", 11},	{"ob", 11},
  {"be", 12},	{"ue", 12},
  {"nb", 13},	{"lg", 13},
  {"he", 14},	{"ug", 14},
  {"nh", 15},	{"ule", 15},
		{"ul", 16},
		{"uge", 17},

  {NULL, 0},
};

struct field_val_assoc cndmsk[] =
{
  {"gt0", 1},
  {"eq0", 2},
  {"ge0", 3},
  {"lt0", 12},
  {"ne0", 13},
  {"le0", 14},

  {NULL, 0},
};

struct m88k_insn
{
  unsigned long opcode;
  expressionS exp;
  enum m88k_reloc_type reloc;
};

static char *get_bf PARAMS ((char *param, unsigned *valp));
static char *get_cmp PARAMS ((char *param, unsigned *valp));
static char *get_cnd PARAMS ((char *param, unsigned *valp));
static char *get_bf2 PARAMS ((char *param, int bc));
static char *get_bf_offset_expression PARAMS ((char *param, unsigned *offsetp));
static char *get_cr PARAMS ((char *param, unsigned *regnop));
static char *get_fcr PARAMS ((char *param, unsigned *regnop));
static char *get_imm16 PARAMS ((char *param, struct m88k_insn *insn));
static char *get_o6 PARAMS ((char *param, unsigned *valp));
static char *match_name PARAMS ((char *, struct field_val_assoc *, unsigned *));
static char *get_reg PARAMS ((char *param, unsigned *regnop, unsigned int reg_prefix));
static char *get_vec9 PARAMS ((char *param, unsigned *valp));
static char *getval PARAMS ((char *param, unsigned int *valp));
static char *get_pcr PARAMS ((char *param, struct m88k_insn *insn,
		      enum m88k_reloc_type reloc));

static int calcop PARAMS ((struct m88k_opcode *format,
			   char *param, struct m88k_insn *insn));

static void s_m88k_88110 PARAMS ((int));

static struct hash_control *op_hash = NULL;

/* Current cpu (either 88100 or 88110, or 0 if unspecified).  Defaults to
   zero, overriden with -m<cpu> options or assembler pseudo-ops.  */
static int current_cpu = 0;

/* These chars start a comment anywhere in a source file (except inside
   another comment.  */
#if defined(OBJ_ELF)
const char comment_chars[] = "|";
#elif defined(OBJ_AOUT)
const char comment_chars[] = "|#";
#else
const char comment_chars[] = ";";
#endif

/* These chars only start a comment at the beginning of a line.  */
#if defined(OBJ_AOUT)
const char line_comment_chars[] = ";";
#else
const char line_comment_chars[] = "#";
#endif

#if defined(OBJ_ELF)
const char line_separator_chars[] = ";";
#else
const char line_separator_chars[] = "";
#endif

/* Chars that can be used to separate mant from exp in floating point nums */
const char EXP_CHARS[] = "eE";

/* Chars that mean this number is a floating point constant */
/* as in 0f123.456 */
/* or    0H1.234E-12 (see exp chars above) */
const char FLT_CHARS[] = "dDfF";

const pseudo_typeS md_pseudo_table[] =
{
#ifndef OBJ_ELF
  {"align", s_align_bytes, 4},
#else
  /* handled with s_align_ptwo in read.c potable[] */
#endif
  {"bss", s_lcomm, 1},
  {"def", s_set, 0},
  {"half", cons, 2},
  {"requires_88110", s_m88k_88110, 0},
  {"sbss", s_lcomm, 1},
#if !defined(OBJ_ELF) || !defined(TE_OpenBSD) /* i.e. NO_PSEUDO_DOT == 1 */
  /* Force set to be treated as an instruction.  */
  {"set", NULL, 0},
  {".set", s_set, 0},
#endif
  {"uahalf", cons, 2},
  {"uaword", cons, 4},
  {"word", cons, 4}, /* override potable[] which has word == short */
  {NULL, NULL, 0}
};

static void
s_m88k_88110(i)
     int i ATTRIBUTE_UNUSED;
{
  current_cpu = 88110;
}

void
md_begin ()
{
  const char *retval = NULL;
  unsigned int i = 0;

  /* Initialize hash table.  */
  op_hash = hash_new ();

  while (*m88k_opcodes[i].name)
    {
      char *name = m88k_opcodes[i].name;

      /* Hash each mnemonic and record its position.  */
      retval = hash_insert (op_hash, name, &m88k_opcodes[i]);

      if (retval != NULL)
	as_fatal (_("Can't hash instruction '%s':%s"),
		  m88k_opcodes[i].name, retval);

      /* Skip to next unique mnemonic or end of list.  */
      for (i++; !strcmp (m88k_opcodes[i].name, name); i++)
	;
    }

#ifdef OBJ_ELF
  record_alignment (text_section, 2);
  record_alignment (data_section, 2);
  record_alignment (bss_section, 2);

  bfd_set_private_flags (stdoutput, 0);
#endif
}

const char *md_shortopts = "m:";
struct option md_longopts[] = {
  {NULL, no_argument, NULL, 0}
};
size_t md_longopts_size = sizeof (md_longopts);

int
md_parse_option (c, arg)
     int c;
     char *arg;
{
  switch (c)
    {
    case 'm':
      if (strcmp (arg, "88100") == 0)
	current_cpu = 88100;
      else if (strcmp (arg, "88110") == 0)
	current_cpu = 88110;
      else
	as_bad (_("Option `%s' is not recognized."), arg);
      break;

    default:
      return 0;
    }

  return 1;
}

void
md_show_usage (stream)
     FILE *stream;
{
  fputs (_("\
M88k options:\n\
  -m88100 | -m88110       select processor type\n"),
	 stream);
}

#ifdef OBJ_ELF
enum m88k_pic_reloc_type {
  pic_reloc_none,
  pic_reloc_abdiff,
  pic_reloc_gotrel,
  pic_reloc_plt
};

static bfd_reloc_code_real_type
m88k_get_reloc_code(struct m88k_insn *insn)
{
  switch (insn->exp.X_md)
    {
    default:
    case pic_reloc_none:
      return insn->reloc;

    case pic_reloc_abdiff:
      if (insn->reloc == BFD_RELOC_LO16)
	return BFD_RELOC_LO16_BASEREL;
      if (insn->reloc == BFD_RELOC_HI16)
	return BFD_RELOC_HI16_BASEREL;
      break;

    case pic_reloc_gotrel:
      if (insn->reloc == BFD_RELOC_LO16)
	return BFD_RELOC_LO16_GOTOFF;
      if (insn->reloc == BFD_RELOC_HI16)
	return BFD_RELOC_HI16_GOTOFF;
      break;

    case pic_reloc_plt:
      if (insn->reloc == BFD_RELOC_32)
	return BFD_RELOC_32_PLTOFF;
      if (insn->reloc == BFD_RELOC_28_PCREL_S2)
	return BFD_RELOC_32_PLT_PCREL;
      break;
    }

  as_bad ("Can't process pic type %d relocation type %d",
	  insn->exp.X_md, insn->reloc);

  return BFD_RELOC_NONE;
}
#else
#define m88k_get_reloc_code(insn)	(insn).reloc
#endif

void
md_assemble (op)
     char *op;
{
  char *param, *thisfrag;
  char c;
  struct m88k_opcode *format;
  struct m88k_insn insn;
  fixS *fixP;

  assert (op);

  /* Skip over instruction to find parameters.  */
  for (param = op; *param != 0 && !ISSPACE (*param); param++)
    ;
  c = *param;
  *param++ = '\0';

  /* Try to find the instruction in the hash table.  */
  /* XXX will not match XRF flavours of 88100 instructions on 88110 */
  if ((format = (struct m88k_opcode *) hash_find (op_hash, op)) == NULL)
    {
      as_bad (_("Invalid mnemonic '%s'"), op);
      return;
    }

  /* Try parsing this instruction into insn.  */
  insn.exp.X_add_symbol = 0;
  insn.exp.X_op_symbol = 0;
  insn.exp.X_add_number = 0;
  insn.exp.X_op = O_illegal;
  insn.exp.X_md = pic_reloc_none;
  insn.reloc = NO_RELOC;

  while (!calcop (format, param, &insn))
    {
      /* If it doesn't parse try the next instruction.  */
      if (!strcmp (format[0].name, format[1].name))
	format++;
      else
	{
	  as_fatal (_("Parameter syntax error"));
	  return;
	}
    }

  /* Grow the current frag and plop in the opcode.  */
  thisfrag = frag_more (4);
  md_number_to_chars (thisfrag, insn.opcode, 4);

  /* If this instruction requires labels mark it for later.  */
  switch (insn.reloc)
    {
    case NO_RELOC:
      break;

    case RELOC_LO16:
    case RELOC_HI16:
      fixP = fix_new_exp (frag_now,
		   thisfrag - frag_now->fr_literal + 2,
		   2,
		   &insn.exp,
		   0,
		   m88k_get_reloc_code(&insn));
      fixP->fx_no_overflow = 1;
      break;

#ifdef M88KCOFF
    case RELOC_IW16:
      fix_new_exp (frag_now,
		   thisfrag - frag_now->fr_literal,
		   4,
		   &insn.exp,
		   0,
		   m88k_get_reloc_code(&insn));
      break;
#endif

    case RELOC_PC16:
#ifdef OBJ_ELF
      fix_new_exp (frag_now,
		   thisfrag - frag_now->fr_literal ,
		   4,
		   &insn.exp,
		   1,
		   m88k_get_reloc_code(&insn));
#else
      fix_new_exp (frag_now,
		   thisfrag - frag_now->fr_literal + 2,
		   2,
		   &insn.exp,
		   1,
		   m88k_get_reloc_code(&insn));
#endif
      break;

    case RELOC_PC26:
      fix_new_exp (frag_now,
		   thisfrag - frag_now->fr_literal,
		   4,
		   &insn.exp,
		   1,
		   m88k_get_reloc_code(&insn));
      break;

    case RELOC_32:
      fix_new_exp (frag_now,
		   thisfrag - frag_now->fr_literal,
		   4,
		   &insn.exp,
		   0,
		   m88k_get_reloc_code(&insn));
      break;

    default:
      as_fatal (_("Unknown relocation type"));
      break;
    }
}

static int
calcop (format, param, insn)
     struct m88k_opcode *format;
     char *param;
     struct m88k_insn *insn;
{
  char *fmt = format->op_spec;
  int f;
  unsigned val;
  unsigned opcode;
  unsigned int reg_prefix = 'r';

  insn->opcode = format->opcode;
  opcode = 0;

  /*
   * Instructions which have no arguments (such as rte) will get
   * correctly reported only if param == "", although there could be
   * whitespace following the instruction.
   * Rather than eating whitespace here, let's assume everything is
   * fine. If there were non-wanted arguments, they will be parsed as
   * an incorrect opcode at the offending line, so that's not too bad.
   * -- miod
   */
  if (*fmt == '\0')
    return 1;

  for (;;)
    {
      if (param == NULL)
	return 0;

      f = *fmt++;
      switch (f)
	{
	case 0:
	  insn->opcode |= opcode;
	  return (*param == 0 || *param == '\n');

	default:
	  if (f != *param++)
	    return 0;
	  break;

	case 'd':
	  param = get_reg (param, &val, reg_prefix);
	  reg_prefix = 'r';
	  opcode |= val << 21;
	  break;

	case 'o':
	  param = get_o6 (param, &val);
	  opcode |= ((val >> 2) << 7);
	  break;

	case 'x':
	  reg_prefix = 'x';
	  break;

	case '1':
	  param = get_reg (param, &val, reg_prefix);
	  reg_prefix = 'r';
	  opcode |= val << 16;
	  break;

	case '2':
	  param = get_reg (param, &val, reg_prefix);
	  reg_prefix = 'r';
	  opcode |= val;
	  break;

	case '3':
	  param = get_reg (param, &val, 'r');
	  opcode |= (val << 16) | val;
	  break;

	case 'I':
	  param = get_imm16 (param, insn);
	  break;

	case 'b':
	  param = get_bf (param, &val);
	  opcode |= val;
	  break;

	case 'p':
	  param = get_pcr (param, insn, RELOC_PC16);
	  break;

	case 'P':
	  param = get_pcr (param, insn, RELOC_PC26);
	  break;

	case 'B':
	  param = get_cmp (param, &val);
	  opcode |= val;
	  break;

	case 'M':
	  param = get_cnd (param, &val);
	  opcode |= val;
	  break;

	case 'c':
	  param = get_cr (param, &val);
	  opcode |= val << 5;
	  break;

	case 'f':
	  param = get_fcr (param, &val);
	  opcode |= val << 5;
	  break;

	case 'V':
	  param = get_vec9 (param, &val);
	  opcode |= val;
	  break;

	case '?':
	  /* Having this here repeats the warning sometimes.
	   But can't we stand that?  */
	  as_warn (_("Use of obsolete instruction"));
	  break;
	}
    }
}

static char *
match_name (param, assoc_tab, valp)
     char *param;
     struct field_val_assoc *assoc_tab;
     unsigned *valp;
{
  int i;
  char *name;
  int name_len;

  for (i = 0;; i++)
    {
      name = assoc_tab[i].name;
      if (name == NULL)
	return NULL;
      name_len = strlen (name);
      if (!strncmp (param, name, name_len))
	{
	  *valp = assoc_tab[i].val;
	  return param + name_len;
	}
    }
}

static char *
get_reg (param, regnop, reg_prefix)
     char *param;
     unsigned *regnop;
     unsigned int reg_prefix;
{
  unsigned c;
  unsigned regno;

#ifdef REGISTER_PREFIX
  c = *param++;
  if (c != REGISTER_PREFIX)
    return NULL;
#endif

  c = *param++;
  if (c == reg_prefix)
    {
      regno = *param++ - '0';
      if (regno < 10)
	{
	  if (regno == 0)
	    {
	      *regnop = 0;
	      return param;
	    }
	  c = *param - '0';
	  if (c < 10)
	    {
	      regno = regno * 10 + c;
	      if (c < 32)
		{
		  *regnop = regno;
		  return param + 1;
		}
	    }
	  else
	    {
	      *regnop = regno;
	      return param;
	    }
	}
      return NULL;
    }
  else if (c == 's' && param[0] == 'p')
    {
      *regnop = 31;
      return param + 1;
    }

  return NULL;
}

static char *
get_imm16 (param, insn)
     char *param;
     struct m88k_insn *insn;
{
  enum m88k_reloc_type reloc = NO_RELOC;
  unsigned int val;
  char *save_ptr;
#ifdef REGISTER_PREFIX
  int found_prefix = 0;
#endif

#ifdef REGISTER_PREFIX
  if (*param == REGISTER_PREFIX)
    {
      param++;
      found_prefix = 1;
    }
#endif

  if (!strncmp (param, "hi16", 4) && !ISALNUM (param[4]))
    {
      reloc = RELOC_HI16;
      param += 4;
    }
  else if (!strncmp (param, "lo16", 4) && !ISALNUM (param[4]))
    {
      reloc = RELOC_LO16;
      param += 4;
    }
#ifdef M88KCOFF
  else if (!strncmp (param, "iw16", 4) && !ISALNUM (param[4]))
    {
      reloc = RELOC_IW16;
      param += 4;
    }
#endif

#ifdef REGISTER_PREFIX
  if (found_prefix && reloc == NO_RELOC)
    return NULL;
#endif

  save_ptr = input_line_pointer;
  input_line_pointer = param;
  expression (&insn->exp);
  param = input_line_pointer;
  input_line_pointer = save_ptr;

  val = insn->exp.X_add_number;

  if (insn->exp.X_op == O_constant)
    {
      /* Insert the value now, and reset reloc to NO_RELOC.  */
      if (reloc == NO_RELOC)
	{
	  /* Warn about too big expressions if not surrounded by xx16.  */
	  if (val > 0xffff)
	    as_warn (_("Expression truncated to 16 bits"));
	}

      if (reloc == RELOC_HI16)
	val >>= 16;

      insn->opcode |= val & 0xffff;
      reloc = NO_RELOC;
    }
  else if (reloc == NO_RELOC)
    /* We accept a symbol even without lo16, hi16, etc, and assume
       lo16 was intended.  */
    reloc = RELOC_LO16;

  insn->reloc = reloc;

  return param;
}

static char *
get_pcr (param, insn, reloc)
     char *param;
     struct m88k_insn *insn;
     enum m88k_reloc_type reloc;
{
  char *saveptr, *saveparam;

  saveptr = input_line_pointer;
  input_line_pointer = param;

  expression (&insn->exp);

  saveparam = input_line_pointer;
  input_line_pointer = saveptr;

  /* Botch: We should relocate now if O_constant.  */
  insn->reloc = reloc;

  return saveparam;
}

static char *
get_cmp (param, valp)
     char *param;
     unsigned *valp;
{
  unsigned int val;
  char *save_ptr;

  save_ptr = param;

#ifdef REGISTER_PREFIX
  /* SVR4 compiler prefixes condition codes with the register prefix */
  if (*param == REGISTER_PREFIX)
    param++;
#endif
  param = match_name (param, cmpslot, valp);
  val = *valp;

  if (param == NULL)
    {
      param = save_ptr;

      save_ptr = input_line_pointer;
      input_line_pointer = param;
      val = get_absolute_expression ();
      param = input_line_pointer;
      input_line_pointer = save_ptr;

      if (val >= 32)
	{
	  as_warn (_("Expression truncated to 5 bits"));
	  val %= 32;
	}
    }

  *valp = val << 21;
  return param;
}

static char *
get_cnd (param, valp)
     char *param;
     unsigned *valp;
{
  unsigned int val;

  if (ISDIGIT (*param))
    {
      param = getval (param, &val);

      if (val >= 32)
	{
	  as_warn (_("Expression truncated to 5 bits"));
	  val %= 32;
	}
    }
  else
    {
#ifdef REGISTER_PREFIX
      /* SVR4 compiler prefixes condition codes with the register prefix */
      if (*param == REGISTER_PREFIX)
	param++;
#endif

      param[0] = TOLOWER (param[0]);
      param[1] = TOLOWER (param[1]);

      param = match_name (param, cndmsk, valp);

      if (param == NULL)
	return NULL;

      val = *valp;
    }

  *valp = val << 21;
  return param;
}

static char *
get_bf2 (param, bc)
     char *param;
     int bc;
{
  int depth = 0;
  int c;

  for (;;)
    {
      c = *param;
      if (c == 0)
	return param;
      else if (c == '(')
	depth++;
      else if (c == ')')
	depth--;
      else if (c == bc && depth <= 0)
	return param;
      param++;
    }
}

static char *
get_bf_offset_expression (param, offsetp)
     char *param;
     unsigned *offsetp;
{
  unsigned offset;

#ifdef REGISTER_PREFIX
  /* SVR4 compiler prefixes condition codes with the register prefix */
  if (*param == REGISTER_PREFIX && ISALPHA (param[1]))
    param++;
#endif

  if (ISALPHA (param[0]))
    {
      param[0] = TOLOWER (param[0]);
      param[1] = TOLOWER (param[1]);

      param = match_name (param, cmpslot, offsetp);

      return param;
    }
  else
    {
      input_line_pointer = param;
      offset = get_absolute_expression ();
      param = input_line_pointer;
    }

  *offsetp = offset;
  return param;
}

static char *
get_bf (param, valp)
     char *param;
     unsigned *valp;
{
  unsigned offset = 0;
  unsigned width = 0;
  char *xp;
  char *save_ptr;

  xp = get_bf2 (param, '<');

  save_ptr = input_line_pointer;
  input_line_pointer = param;
  if (*xp == 0)
    {
      /* We did not find '<'.  We have an offset (width implicitly 32).  */
      param = get_bf_offset_expression (param, &offset);
      input_line_pointer = save_ptr;
      if (param == NULL)
	return NULL;
    }
  else
    {
      *xp++ = 0;		/* Overwrite the '<' */
      param = get_bf2 (xp, '>');
      if (*param == 0)
	return NULL;
      *param++ = 0;		/* Overwrite the '>' */

      width = get_absolute_expression ();
      xp = get_bf_offset_expression (xp, &offset);
      input_line_pointer = save_ptr;

      if (xp + 1 != param)
	return NULL;
    }

  *valp = ((width % 32) << 5) | (offset % 32);

  return param;
}

static char *
get_cr (param, regnop)
     char *param;
     unsigned *regnop;
{
  unsigned regno;
  unsigned c;

#ifdef REGISTER_PREFIX
  if (*param++ != REGISTER_PREFIX)
    return NULL;
#endif

  if (!strncmp (param, "cr", 2))
    {
      param += 2;

      regno = *param++ - '0';
      if (regno < 10)
	{
	  if (regno == 0)
	    {
	      *regnop = 0;
	      return param;
	    }
	  c = *param - '0';
	  if (c < 10)
	    {
	      regno = regno * 10 + c;
	      if (c < 64)
		{
		  *regnop = regno;
		  return param + 1;
		}
	    }
	  else
	    {
	      *regnop = regno;
	      return param;
	    }
	}
      return NULL;
    }

  param = match_name (param,
		      current_cpu == 88110 ? m88110_cr_regs : m88100_cr_regs,
		      regnop);

  return param;
}

static char *
get_fcr (param, regnop)
     char *param;
     unsigned *regnop;
{
  unsigned regno;
  unsigned c;

#ifdef REGISTER_PREFIX
  if (*param++ != REGISTER_PREFIX)
    return NULL;
#endif

  if (!strncmp (param, "fcr", 3))
    {
      param += 3;

      regno = *param++ - '0';
      if (regno < 10)
	{
	  if (regno == 0)
	    {
	      *regnop = 0;
	      return param;
	    }
	  c = *param - '0';
	  if (c < 10)
	    {
	      regno = regno * 10 + c;
	      if (c < 64)
		{
		  *regnop = regno;
		  return param + 1;
		}
	    }
	  else
	    {
	      *regnop = regno;
	      return param;
	    }
	}
      return NULL;
    }

  param = match_name (param, fcr_regs, regnop);

  return param;
}

static char *
get_vec9 (param, valp)
     char *param;
     unsigned *valp;
{
  unsigned val;
  char *save_ptr;

  save_ptr = input_line_pointer;
  input_line_pointer = param;
  val = get_absolute_expression ();
  param = input_line_pointer;
  input_line_pointer = save_ptr;

  if (val >= 1 << 9)
    as_warn (_("Expression truncated to 9 bits"));

  *valp = val % (1 << 9);

  return param;
}

static char *
get_o6 (param, valp)
     char *param;
     unsigned *valp;
{
  unsigned val;
  char *save_ptr;

  save_ptr = input_line_pointer;
  input_line_pointer = param;
  val = get_absolute_expression ();
  param = input_line_pointer;
  input_line_pointer = save_ptr;

  if (val & 0x3)
    as_warn (_("Removed lower 2 bits of expression"));

  *valp = val;

  return (param);
}

#define hexval(z) \
  (ISDIGIT (z) ? (z) - '0' :						\
   ISLOWER (z) ? (z) - 'a' + 10 : 					\
   ISUPPER (z) ? (z) - 'A' + 10 : (unsigned) -1)

static char *
getval (param, valp)
     char *param;
     unsigned int *valp;
{
  unsigned int val = 0;
  unsigned int c;

  c = *param++;
  if (c == '0')
    {
      c = *param++;
      if (c == 'x' || c == 'X')
	{
	  c = *param++;
	  c = hexval (c);
	  while (c < 16)
	    {
	      val = val * 16 + c;
	      c = *param++;
	      c = hexval (c);
	    }
	}
      else
	{
	  c -= '0';
	  while (c < 8)
	    {
	      val = val * 8 + c;
	      c = *param++ - '0';
	    }
	}
    }
  else
    {
      c -= '0';
      while (c < 10)
	{
	  val = val * 10 + c;
	  c = *param++ - '0';
	}
    }

  *valp = val;
  return param - 1;
}

void
md_number_to_chars (buf, val, nbytes)
     char *buf;
     valueT val;
     int nbytes;
{
  number_to_chars_bigendian (buf, val, nbytes);
}

#define MAX_LITTLENUMS 6

/* Turn a string in input_line_pointer into a floating point constant of type
   type, and store the appropriate bytes in *litP.  The number of LITTLENUMS
   emitted is stored in *sizeP .  An error message is returned, or NULL on OK.
 */
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

int md_short_jump_size = 4;
int md_long_jump_size = 4;

void
md_create_short_jump (ptr, from_addr, to_addr, frag, to_symbol)
     char *ptr;
     addressT from_addr ATTRIBUTE_UNUSED;
     addressT to_addr ATTRIBUTE_UNUSED;
     fragS *frag;
     symbolS *to_symbol;
{
  /* Since all instructions have the same width, it does not make sense to
     try and abuse a conditional instruction to get a short displacement
     (such as bb1 0, %r0, address).  */
  md_create_long_jump (ptr, from_addr, to_addr, frag, to_symbol);
}

void
md_create_long_jump (ptr, from_addr, to_addr, frag, to_symbol)
     char *ptr;
     addressT from_addr ATTRIBUTE_UNUSED;
     addressT to_addr ATTRIBUTE_UNUSED;
     fragS *frag;
     symbolS *to_symbol;
{
  ptr[0] = (char) 0xc0;		/* br to_addr */
  ptr[1] = 0x00;
  ptr[2] = 0x00;
  ptr[3] = 0x00;
  fix_new (frag,
	   ptr - frag->fr_literal,
	   4,
	   to_symbol,
	   (offsetT) 0,
	   0,
	   RELOC_PC26);
}

int
md_estimate_size_before_relax (fragP, segment_type)
     fragS *fragP ATTRIBUTE_UNUSED;
     segT segment_type ATTRIBUTE_UNUSED;
{
  as_fatal (_("Relaxation should never occur"));
  return (-1);
}

#ifdef M88KCOFF

/* These functions are needed if we are linking with obj-coffbfd.c.
   That file may be replaced by a more BFD oriented version at some
   point.  If that happens, these functions should be reexamined.

   Ian Lance Taylor, Cygnus Support, 13 July 1993.  */

/* Given a fixS structure (created by a call to fix_new, above),
   return the BFD relocation type to use for it.  */

short
tc_coff_fix2rtype (fixp)
     fixS *fixp;
{
  switch (fixp->fx_r_type)
    {
    case RELOC_LO16:
      return R_LVRT16;
    case RELOC_HI16:
      return R_HVRT16;
    case RELOC_PC16:
      return R_PCR16L;
    case RELOC_PC26:
      return R_PCR26L;
    case RELOC_32:
      return R_VRT32;
    case RELOC_IW16:
      return R_VRT16;
    default:
      abort ();
    }
}

/* Apply a fixS to the object file.  Since COFF does not use addends
   in relocs, the addend is actually stored directly in the object
   file itself.  */

void
md_apply_fix3 (fixP, valP, seg)
     fixS *fixP;
     valueT * valP;
     segT seg ATTRIBUTE_UNUSED;
{
  long val = * (long *) valP;
  char *buf;

  buf = fixP->fx_frag->fr_literal + fixP->fx_where;
  fixP->fx_addnumber = val;
  fixP->fx_offset = 0;

  switch (fixP->fx_r_type)
    {
    case RELOC_IW16:
      fixP->fx_offset = val >> 16;
      buf[2] = val >> 8;
      buf[3] = val;
      break;

    case RELOC_LO16:
      fixP->fx_offset = val >> 16;
      buf[0] = val >> 8;
      buf[1] = val;
      break;

    case RELOC_HI16:
      buf[0] = val >> 24;
      buf[1] = val >> 16;
      break;

    case RELOC_PC16:
      buf[0] = val >> 10;
      buf[1] = val >> 2;
      break;

    case RELOC_PC26:
      buf[0] |= (val >> 26) & 0x03;
      buf[1] = val >> 18;
      buf[2] = val >> 10;
      buf[3] = val >> 2;
      break;

    case RELOC_32:
      buf[0] = val >> 24;
      buf[1] = val >> 16;
      buf[2] = val >> 8;
      buf[3] = val;
      break;

    default:
      abort ();
    }

  if (fixP->fx_addsy == NULL && fixP->fx_pcrel == 0)
    fixP->fx_done = 1;
}

#endif /* M88KCOFF */

/* Fill in rs_align_code fragments.  */

void
m88k_handle_align (fragp)
     fragS *fragp;
{
  static const unsigned char nop_pattern[] = { 0xf4, 0x00, 0x58, 0x00 };

  int bytes;
  char *p;

  if (fragp->fr_type != rs_align_code)
    return;

  bytes = fragp->fr_next->fr_address - fragp->fr_address - fragp->fr_fix;
  p = fragp->fr_literal + fragp->fr_fix;

  if (bytes & 3)
    {
      int fix = bytes & 3;
      memset (p, 0, fix);
      p += fix;
      bytes -= fix;
      fragp->fr_fix += fix;
    }

  memcpy (p, nop_pattern, 4);
  fragp->fr_var = 4;
}

/* Where a PC relative offset is calculated from.  On the m88k they
   are calculated from just after the instruction.  */

long
md_pcrel_from (fixp)
     fixS *fixp;
{
  switch (fixp->fx_r_type)
    {
    case RELOC_PC16:
#ifdef OBJ_ELF
      /* FALLTHROUGH */
#else
      return fixp->fx_frag->fr_address + fixp->fx_where - 2;
#endif
    case RELOC_PC26:
#ifdef OBJ_ELF
    case BFD_RELOC_32_PLT_PCREL:
#endif
      return fixp->fx_frag->fr_address + fixp->fx_where;
    default:
      abort ();
    }
  /*NOTREACHED*/
}

#ifdef OBJ_ELF

valueT
md_section_align (segment, size)
     segT   segment ATTRIBUTE_UNUSED;
     valueT size;
{
  return size;
}

/* Generate the BFD reloc to be stuck in the object file from the
   fixup used internally in the assembler.  */

arelent *
tc_gen_reloc (sec, fixp)
     asection *sec ATTRIBUTE_UNUSED;
     fixS *fixp;
{
  arelent *reloc;
  bfd_reloc_code_real_type code;

  reloc = (arelent *) xmalloc (sizeof (arelent));
  reloc->sym_ptr_ptr = (asymbol **) xmalloc (sizeof (asymbol *));
  *reloc->sym_ptr_ptr = symbol_get_bfdsym (fixp->fx_addsy);
  reloc->address = fixp->fx_frag->fr_address + fixp->fx_where;

  /* Make sure none of our internal relocations make it this far.
     They'd better have been fully resolved by this point.  */
  assert ((int) fixp->fx_r_type > 0);

  code = fixp->fx_r_type;
  reloc->howto = bfd_reloc_type_lookup (stdoutput, code);
  if (reloc->howto == NULL)
    {
      as_bad_where (fixp->fx_file, fixp->fx_line,
		    _("cannot represent `%s' relocation in object file"),
		    bfd_get_reloc_code_name (code));
      return NULL;
    }

  if (!fixp->fx_pcrel != !reloc->howto->pc_relative)
    {
      as_fatal (_("internal error? cannot generate `%s' relocation"),
		bfd_get_reloc_code_name (code));
    }
  assert (!fixp->fx_pcrel == !reloc->howto->pc_relative);

  reloc->addend = fixp->fx_offset;

  return reloc;
}

/* Apply a fixS to the object file.  This is called for all the
   fixups we generated by the call to fix_new_exp, above.  In the call
   above we used a reloc code which was the largest legal reloc code
   plus the operand index.  Here we undo that to recover the operand
   index.  At this point all symbol values should be fully resolved,
   and we attempt to completely resolve the reloc.  If we can not do
   that, we determine the correct reloc code and put it back in the
   fixup.

   This is the ELF version.
*/

void
md_apply_fix3 (fixP, valP, seg)
     fixS *fixP;
     valueT * valP;
     segT seg ATTRIBUTE_UNUSED;
{
  valueT val = * (valueT *) valP;
  char *buf;
  long insn;

  buf = fixP->fx_frag->fr_literal + fixP->fx_where;

  if (fixP->fx_subsy != NULL)
    as_bad_where (fixP->fx_file, fixP->fx_line, _("expression too complex"));

  if (fixP->fx_addsy)
    {
#if 0
      /* can't empty 26-bit relocation values with memset() */
      if (fixP->fx_r_type == BFD_RELOC_28_PCREL_S2)
	{
	  insn = bfd_getb32 ((unsigned char *) buf);
	  insn &= ~0x03ffffff;
	  bfd_putb32(insn, buf);
	}
      else
	memset(buf, 0, fixP->fx_size);
#endif

      if (fixP->fx_r_type == BFD_RELOC_VTABLE_INHERIT
	  && !S_IS_DEFINED (fixP->fx_addsy)
	  && !S_IS_WEAK (fixP->fx_addsy))
	S_SET_WEAK (fixP->fx_addsy);

      return;
    }

  switch (fixP->fx_r_type)
    {
    case BFD_RELOC_VTABLE_INHERIT:
    case BFD_RELOC_VTABLE_ENTRY:
      return;

    case BFD_RELOC_HI16_BASEREL:
    case BFD_RELOC_LO16_BASEREL:
    case BFD_RELOC_HI16_GOTOFF:
    case BFD_RELOC_LO16_GOTOFF:
    case BFD_RELOC_32_PLTOFF:
      return;

    case BFD_RELOC_LO16:
    case BFD_RELOC_HI16:
      if (fixP->fx_pcrel)
	abort ();
      buf[0] = val >> 8;
      buf[1] = val;
      break;

    case BFD_RELOC_18_PCREL_S2:
      if ((val & 0x03) != 0)
	as_bad_where (fixP->fx_file, fixP->fx_line,
		      "Branch to unaligned address (%lx)", (long)val);
      buf[2] = val >> 10;
      buf[3] = val >> 2;
      break;

    case BFD_RELOC_32_PLT_PCREL:
    case BFD_RELOC_28_PCREL_S2:
      if ((val & 0x03) != 0)
	as_bad_where (fixP->fx_file, fixP->fx_line,
		      "Branch to unaligned address (%lx)", (long)val);
      buf[0] |= (val >> 26) & 0x03;
      buf[1] = val >> 18;
      buf[2] = val >> 10;
      buf[3] = val >> 2;
      break;

    case BFD_RELOC_32:
      insn = val;
      bfd_putb32(insn, buf);
      break;

    default:
      abort ();
    }

  if (/* fixP->fx_addsy == NULL && */ fixP->fx_pcrel == 0)
    fixP->fx_done = 1;
}

/* Set the ELF specific flags.  */
void
m88k_elf_final_processing ()
{
  if (current_cpu == 88110)
    elf_elfheader (stdoutput)->e_flags |= EF_M88110;
}

inline static char *
m88k_end_of_name (const char *suffix, const char *pattern, size_t patlen)
{
  if (strncmp (suffix, pattern, patlen) == 0
      && ! is_part_of_name (suffix[patlen]))
    return suffix + patlen;

  return NULL;
}

int
m88k_parse_name (name, expressionP, nextcharP)
    const char *name;
    expressionS *expressionP;
    char *nextcharP;
{
  char *next = input_line_pointer;
  char *next_end;
  enum m88k_pic_reloc_type reloc_type = pic_reloc_none;
  symbolS *symbolP;
  segT segment;

  if (*nextcharP != '#')
    return 0;

  if ((next_end = m88k_end_of_name (next + 1, "abdiff", 6)) != NULL)
    {
      reloc_type = pic_reloc_abdiff;
    }
  else if ((next_end = m88k_end_of_name (next + 1, "got_rel", 7)) != NULL)
    {
      reloc_type = pic_reloc_gotrel;
    }
  else if ((next_end = m88k_end_of_name (next + 1, "plt", 3)) != NULL)
    {
      reloc_type = pic_reloc_plt;
    }
  else
    return 0;

  symbolP = symbol_find_or_make (name);
  segment = S_GET_SEGMENT (symbolP);
  if (segment == absolute_section)
    {
      expressionP->X_op = O_constant;
      expressionP->X_add_number = S_GET_VALUE (symbolP);
    }
  else if (segment == reg_section)
    {
      expressionP->X_op = O_register;
      expressionP->X_add_number = S_GET_VALUE (symbolP);
    }
  else
    {
      expressionP->X_op = O_symbol;
      expressionP->X_add_symbol = symbolP;
      expressionP->X_add_number = 0;
    }
  expressionP->X_md = reloc_type;

  *input_line_pointer = *nextcharP;
  input_line_pointer = next_end;
  *nextcharP = *input_line_pointer;
  *input_line_pointer = '\0';

  return 1;
}

int
m88k_fix_adjustable (fix)
     fixS *fix;
{
  return (fix->fx_r_type != BFD_RELOC_LO16_GOTOFF
	  && fix->fx_r_type != BFD_RELOC_HI16_GOTOFF
	  && fix->fx_r_type != BFD_RELOC_VTABLE_INHERIT
	  && fix->fx_r_type != BFD_RELOC_VTABLE_ENTRY
	  && (fix->fx_pcrel
	      || (fix->fx_subsy != NULL
		  && (S_GET_SEGMENT (fix->fx_subsy)
		      == S_GET_SEGMENT (fix->fx_addsy)))
	      || S_IS_LOCAL (fix->fx_addsy)));
}
#endif /* OBJ_ELF */

#ifdef OBJ_AOUT

/* Round up a section size to the appropriate boundary. */
valueT
md_section_align (segment, size)
     segT segment ATTRIBUTE_UNUSED;
     valueT size;
{
#ifdef BFD_ASSEMBLER
  /* For a.out, force the section size to be aligned.  If we don't do
     this, BFD will align it for us, but it will not write out the
     final bytes of the section.  This may be a bug in BFD, but it is
     easier to fix it here since that is how the other a.out targets
     work.  */
  int align;

  align = bfd_get_section_alignment (stdoutput, segment);
  valueT mask = ((valueT) 1 << align) - 1;

  return (size + mask) & ~mask;
#else
  return (size + 7) & ~7;
#endif
}

const int md_reloc_size = 12; /* sizeof(struct relocation_info); */

void
tc_aout_fix_to_chars (where, fixP, segment_address_in_file)
     char *where;
     fixS *fixP;
     relax_addressT segment_address_in_file;
{
  long r_symbolnum;
  long r_addend = 0;
  long r_address;

  know (fixP->fx_addsy != NULL);

  r_address = fixP->fx_frag->fr_address + fixP->fx_where
	      - segment_address_in_file;
  md_number_to_chars (where, r_address, 4);

  r_symbolnum = (S_IS_DEFINED (fixP->fx_addsy)
                 ? S_GET_TYPE (fixP->fx_addsy)
                 : fixP->fx_addsy->sy_number);

  where[4] = (r_symbolnum >> 16) & 0x0ff;
  where[5] = (r_symbolnum >> 8) & 0x0ff;
  where[6] = r_symbolnum & 0x0ff;
  where[7] = ((((!S_IS_DEFINED (fixP->fx_addsy)) << 7) & 0x80) | (0 & 0x70) |
	      (fixP->fx_r_type & 0xf));

  if (fixP->fx_addsy->sy_frag) {
    r_addend = fixP->fx_addsy->sy_frag->fr_address;
  }

  if (fixP->fx_pcrel) {
    r_addend -= r_address;
  } else {
    r_addend = fixP->fx_addnumber;
  }

  md_number_to_chars(&where[8], r_addend, 4);
}

void
tc_headers_hook(headers)
     object_headers *headers;
{
#if defined(TE_NetBSD) || defined(TE_OpenBSD)
  N_SET_INFO(headers->header, OMAGIC, M_88K_OPENBSD, 0);
  headers->header.a_info = htonl(headers->header.a_info);
#endif
}

#endif /* OBJ_AOUT */
