/* tc-sparc.c -- Assemble for the SPARC
   Copyright (C) 1989, 90, 91, 92, 93, 94, 1995 Free Software Foundation, Inc.

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
   the Free Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA. */

#include <stdio.h>
#include <ctype.h>

#include "as.h"
#include "subsegs.h"

/* careful, this file includes data *declarations* */
#include "opcode/sparc.h"

static void sparc_ip PARAMS ((char *));

#ifdef sparcv9
static enum sparc_architecture current_architecture = v9;
#else
static enum sparc_architecture current_architecture = v6;
#endif
static int architecture_requested;
static int warn_on_bump;

/* Non-zero if we are generating PIC code.  */
int sparc_pic_code;

extern int target_big_endian;

/* handle of the OPCODE hash table */
static struct hash_control *op_hash;

static void s_data1 PARAMS ((void));
static void s_seg PARAMS ((int));
static void s_proc PARAMS ((int));
static void s_reserve PARAMS ((int));
static void s_common PARAMS ((int));

const pseudo_typeS md_pseudo_table[] =
{
  {"align", s_align_bytes, 0},	/* Defaulting is invalid (0) */
  {"common", s_common, 0},
  {"global", s_globl, 0},
  {"half", cons, 2},
  {"optim", s_ignore, 0},
  {"proc", s_proc, 0},
  {"reserve", s_reserve, 0},
  {"seg", s_seg, 0},
  {"skip", s_space, 0},
  {"word", cons, 4},
#ifndef NO_V9
  {"xword", cons, 8},
#ifdef OBJ_ELF
  {"uaxword", cons, 8},
#endif
#endif
#ifdef OBJ_ELF
  /* these are specific to sparc/svr4 */
  {"pushsection", obj_elf_section, 0},
  {"popsection", obj_elf_previous, 0},
  {"uaword", cons, 4},
  {"uahalf", cons, 2},
#endif
  {NULL, 0, 0},
};

const int md_reloc_size = 12;	/* Size of relocation record */

/* This array holds the chars that always start a comment.  If the
   pre-processor is disabled, these aren't very useful */
const char comment_chars[] = "!";	/* JF removed '|' from comment_chars */

/* This array holds the chars that only start a comment at the beginning of
   a line.  If the line seems to have the form '# 123 filename'
   .line and .file directives will appear in the pre-processed output */
/* Note that input_file.c hand checks for '#' at the beginning of the
   first line of the input file.  This is because the compiler outputs
   #NO_APP at the beginning of its output. */
/* Also note that comments started like this one will always
   work if '/' isn't otherwise defined. */
const char line_comment_chars[] = "#";

const char line_separator_chars[] = "";

/* Chars that can be used to separate mant from exp in floating point nums */
const char EXP_CHARS[] = "eE";

/* Chars that mean this number is a floating point constant */
/* As in 0f12.456 */
/* or    0d1.2345e12 */
const char FLT_CHARS[] = "rRsSfFdDxXpP";

/* Also be aware that MAXIMUM_NUMBER_OF_CHARS_FOR_FLOAT may have to be
   changed in read.c.  Ideally it shouldn't have to know about it at all,
   but nothing is ideal around here.  */

static unsigned char octal[256];
#define isoctal(c)  octal[(unsigned char) (c)]
static unsigned char toHex[256];

struct sparc_it
  {
    char *error;
    unsigned long opcode;
    struct nlist *nlistp;
    expressionS exp;
    int pcrel;
    bfd_reloc_code_real_type reloc;
  };

struct sparc_it the_insn, set_insn;

static INLINE int
in_signed_range (val, max)
     bfd_signed_vma val, max;
{
  if (max <= 0)
    abort ();
  if (val > max)
    return 0;
  if (val < ~max)
    return 0;
  return 1;
}

#if 0
static void print_insn PARAMS ((struct sparc_it *insn));
#endif
static int getExpression PARAMS ((char *str));

static char *expr_end;
static int special_case;

/*
 * Instructions that require wierd handling because they're longer than
 * 4 bytes.
 */
#define	SPECIAL_CASE_SET	1
#define	SPECIAL_CASE_FDIV	2

/*
 * sort of like s_lcomm
 *
 */
#ifndef OBJ_ELF
static int max_alignment = 15;
#endif

static void
s_reserve (ignore)
     int ignore;
{
  char *name;
  char *p;
  char c;
  int align;
  int size;
  int temp;
  symbolS *symbolP;

  name = input_line_pointer;
  c = get_symbol_end ();
  p = input_line_pointer;
  *p = c;
  SKIP_WHITESPACE ();

  if (*input_line_pointer != ',')
    {
      as_bad ("Expected comma after name");
      ignore_rest_of_line ();
      return;
    }

  ++input_line_pointer;

  if ((size = get_absolute_expression ()) < 0)
    {
      as_bad ("BSS length (%d.) <0! Ignored.", size);
      ignore_rest_of_line ();
      return;
    }				/* bad length */

  *p = 0;
  symbolP = symbol_find_or_make (name);
  *p = c;

  if (strncmp (input_line_pointer, ",\"bss\"", 6) != 0
      && strncmp (input_line_pointer, ",\".bss\"", 7) != 0)
    {
      as_bad ("bad .reserve segment -- expected BSS segment");
      return;
    }

  if (input_line_pointer[2] == '.')
    input_line_pointer += 7;
  else
    input_line_pointer += 6;
  SKIP_WHITESPACE ();

  if (*input_line_pointer == ',')
    {
      ++input_line_pointer;

      SKIP_WHITESPACE ();
      if (*input_line_pointer == '\n')
	{
	  as_bad ("Missing alignment");
	  return;
	}

      align = get_absolute_expression ();
#ifndef OBJ_ELF
      if (align > max_alignment)
	{
	  align = max_alignment;
	  as_warn ("Alignment too large: %d. assumed.", align);
	}
#endif
      if (align < 0)
	{
	  align = 0;
	  as_warn ("Alignment negative. 0 assumed.");
	}

      record_alignment (bss_section, align);

      /* convert to a power of 2 alignment */
      for (temp = 0; (align & 1) == 0; align >>= 1, ++temp);;

      if (align != 1)
	{
	  as_bad ("Alignment not a power of 2");
	  ignore_rest_of_line ();
	  return;
	}			/* not a power of two */

      align = temp;
    }				/* if has optional alignment */
  else
    align = 0;

  if (!S_IS_DEFINED (symbolP)
#ifdef OBJ_AOUT
      && S_GET_OTHER (symbolP) == 0
      && S_GET_DESC (symbolP) == 0
#endif
      )
    {
      if (! need_pass_2)
	{
	  char *pfrag;
	  segT current_seg = now_seg;
	  subsegT current_subseg = now_subseg;

	  subseg_set (bss_section, 1); /* switch to bss */

	  if (align)
	    frag_align (align, 0); /* do alignment */

	  /* detach from old frag */
	  if (S_GET_SEGMENT(symbolP) == bss_section)
	    symbolP->sy_frag->fr_symbol = NULL;

	  symbolP->sy_frag = frag_now;
	  pfrag = frag_var (rs_org, 1, 1, (relax_substateT)0, symbolP,
			    size, (char *)0);
	  *pfrag = 0;

	  S_SET_SEGMENT (symbolP, bss_section);

	  subseg_set (current_seg, current_subseg);
	}
    }
  else
    {
      as_warn("Ignoring attempt to re-define symbol %s",
	      S_GET_NAME (symbolP));
    }				/* if not redefining */

  demand_empty_rest_of_line ();
}

static void
s_common (ignore)
     int ignore;
{
  char *name;
  char c;
  char *p;
  int temp, size;
  symbolS *symbolP;

  name = input_line_pointer;
  c = get_symbol_end ();
  /* just after name is now '\0' */
  p = input_line_pointer;
  *p = c;
  SKIP_WHITESPACE ();
  if (*input_line_pointer != ',')
    {
      as_bad ("Expected comma after symbol-name");
      ignore_rest_of_line ();
      return;
    }
  input_line_pointer++;		/* skip ',' */
  if ((temp = get_absolute_expression ()) < 0)
    {
      as_bad (".COMMon length (%d.) <0! Ignored.", temp);
      ignore_rest_of_line ();
      return;
    }
  size = temp;
  *p = 0;
  symbolP = symbol_find_or_make (name);
  *p = c;
  if (S_IS_DEFINED (symbolP))
    {
      as_bad ("Ignoring attempt to re-define symbol");
      ignore_rest_of_line ();
      return;
    }
  if (S_GET_VALUE (symbolP) != 0)
    {
      if (S_GET_VALUE (symbolP) != size)
	{
	  as_warn ("Length of .comm \"%s\" is already %ld. Not changed to %d.",
		   S_GET_NAME (symbolP), (long) S_GET_VALUE (symbolP), size);
	}
    }
  else
    {
#ifndef OBJ_ELF
      S_SET_VALUE (symbolP, (valueT) size);
      S_SET_EXTERNAL (symbolP);
#endif
    }
  know (symbolP->sy_frag == &zero_address_frag);
  if (*input_line_pointer != ',')
    {
      as_bad ("Expected comma after common length");
      ignore_rest_of_line ();
      return;
    }
  input_line_pointer++;
  SKIP_WHITESPACE ();
  if (*input_line_pointer != '"')
    {
      temp = get_absolute_expression ();
#ifndef OBJ_ELF
      if (temp > max_alignment)
	{
	  temp = max_alignment;
	  as_warn ("Common alignment too large: %d. assumed", temp);
	}
#endif
      if (temp < 0)
	{
	  temp = 0;
	  as_warn ("Common alignment negative; 0 assumed");
	}
#ifdef OBJ_ELF
      if (symbolP->local)
	{
	  segT old_sec;
	  int old_subsec;
	  char *p;
	  int align;

	allocate_bss:
	  old_sec = now_seg;
	  old_subsec = now_subseg;
	  align = temp;
	  record_alignment (bss_section, align);
	  subseg_set (bss_section, 0);
	  if (align)
	    frag_align (align, 0);
	  if (S_GET_SEGMENT (symbolP) == bss_section)
	    symbolP->sy_frag->fr_symbol = 0;
	  symbolP->sy_frag = frag_now;
	  p = frag_var (rs_org, 1, 1, (relax_substateT) 0, symbolP, size,
			(char *) 0);
	  *p = 0;
	  S_SET_SEGMENT (symbolP, bss_section);
	  S_CLEAR_EXTERNAL (symbolP);
	  subseg_set (old_sec, old_subsec);
	}
      else
#endif
	{
	allocate_common:
	  S_SET_VALUE (symbolP, (valueT) size);
#ifdef OBJ_ELF
	  S_SET_ALIGN (symbolP, temp);
#endif
	  S_SET_EXTERNAL (symbolP);
	  /* should be common, but this is how gas does it for now */
	  S_SET_SEGMENT (symbolP, bfd_und_section_ptr);
	}
    }
  else
    {
      input_line_pointer++;
      /* @@ Some use the dot, some don't.  Can we get some consistency??  */
      if (*input_line_pointer == '.')
	input_line_pointer++;
      /* @@ Some say data, some say bss.  */
      if (strncmp (input_line_pointer, "bss\"", 4)
	  && strncmp (input_line_pointer, "data\"", 5))
	{
	  while (*--input_line_pointer != '"')
	    ;
	  input_line_pointer--;
	  goto bad_common_segment;
	}
      while (*input_line_pointer++ != '"')
	;
      goto allocate_common;
    }
  demand_empty_rest_of_line ();
  return;

  {
  bad_common_segment:
    p = input_line_pointer;
    while (*p && *p != '\n')
      p++;
    c = *p;
    *p = '\0';
    as_bad ("bad .common segment %s", input_line_pointer + 1);
    *p = c;
    input_line_pointer = p;
    ignore_rest_of_line ();
    return;
  }
}

static void
s_seg (ignore)
     int ignore;
{

  if (strncmp (input_line_pointer, "\"text\"", 6) == 0)
    {
      input_line_pointer += 6;
      s_text (0);
      return;
    }
  if (strncmp (input_line_pointer, "\"data\"", 6) == 0)
    {
      input_line_pointer += 6;
      s_data (0);
      return;
    }
  if (strncmp (input_line_pointer, "\"data1\"", 7) == 0)
    {
      input_line_pointer += 7;
      s_data1 ();
      return;
    }
  if (strncmp (input_line_pointer, "\"bss\"", 5) == 0)
    {
      input_line_pointer += 5;
      /* We only support 2 segments -- text and data -- for now, so
	 things in the "bss segment" will have to go into data for now.
	 You can still allocate SEG_BSS stuff with .lcomm or .reserve. */
      subseg_set (data_section, 255);	/* FIXME-SOMEDAY */
      return;
    }
  as_bad ("Unknown segment type");
  demand_empty_rest_of_line ();
}

static void
s_data1 ()
{
  subseg_set (data_section, 1);
  demand_empty_rest_of_line ();
}

static void
s_proc (ignore)
     int ignore;
{
  while (!is_end_of_line[(unsigned char) *input_line_pointer])
    {
      ++input_line_pointer;
    }
  ++input_line_pointer;
}

#ifndef NO_V9

struct priv_reg_entry
  {
    char *name;
    int regnum;
  };

struct priv_reg_entry priv_reg_table[] =
{
  {"tpc", 0},
  {"tnpc", 1},
  {"tstate", 2},
  {"tt", 3},
  {"tick", 4},
  {"tba", 5},
  {"pstate", 6},
  {"tl", 7},
  {"pil", 8},
  {"cwp", 9},
  {"cansave", 10},
  {"canrestore", 11},
  {"cleanwin", 12},
  {"otherwin", 13},
  {"wstate", 14},
  {"fq", 15},
  {"ver", 31},
  {"", -1},			/* end marker */
};

static int
cmp_reg_entry (p, q)
     struct priv_reg_entry *p, *q;
{
  return strcmp (q->name, p->name);
}

#endif

/* This function is called once, at assembler startup time.  It should
   set up all the tables, etc. that the MD part of the assembler will need. */
void
md_begin ()
{
  register const char *retval = NULL;
  int lose = 0;
  register unsigned int i = 0;

  op_hash = hash_new ();

  while (i < NUMOPCODES)
    {
      const char *name = sparc_opcodes[i].name;
      retval = hash_insert (op_hash, name, &sparc_opcodes[i]);
      if (retval != NULL)
	{
	  fprintf (stderr, "internal error: can't hash `%s': %s\n",
		   sparc_opcodes[i].name, retval);
	  lose = 1;
	}
      do
	{
	  if (sparc_opcodes[i].match & sparc_opcodes[i].lose)
	    {
	      fprintf (stderr, "internal error: losing opcode: `%s' \"%s\"\n",
		       sparc_opcodes[i].name, sparc_opcodes[i].args);
	      lose = 1;
	    }
	  ++i;
	}
      while (i < NUMOPCODES
	     && !strcmp (sparc_opcodes[i].name, name));
    }

  if (lose)
    as_fatal ("Broken assembler.  No assembly attempted.");

  for (i = '0'; i < '8'; ++i)
    octal[i] = 1;
  for (i = '0'; i <= '9'; ++i)
    toHex[i] = i - '0';
  for (i = 'a'; i <= 'f'; ++i)
    toHex[i] = i + 10 - 'a';
  for (i = 'A'; i <= 'F'; ++i)
    toHex[i] = i + 10 - 'A';

#ifndef NO_V9
  qsort (priv_reg_table, sizeof (priv_reg_table) / sizeof (priv_reg_table[0]),
	 sizeof (priv_reg_table[0]), cmp_reg_entry);
#endif

  target_big_endian = 1;
}

void
md_assemble (str)
     char *str;
{
  char *toP;
  int rsd;

  know (str);
  sparc_ip (str);

  /* See if "set" operand is absolute and small; skip sethi if so. */
  if (special_case == SPECIAL_CASE_SET
      && the_insn.exp.X_op == O_constant)
    {
      if (the_insn.exp.X_add_number >= -(1 << 12)
	  && the_insn.exp.X_add_number < (1 << 12))
	{
	  the_insn.opcode = 0x80102000	/* or %g0,imm,... */
	    | (the_insn.opcode & 0x3E000000)	/* dest reg */
	    | (the_insn.exp.X_add_number & 0x1FFF);	/* imm */
	  special_case = 0;	/* No longer special */
	  the_insn.reloc = BFD_RELOC_NONE;	/* No longer relocated */
	}
    }

  toP = frag_more (4);
  /* put out the opcode */
  md_number_to_chars (toP, (valueT) the_insn.opcode, 4);

  /* put out the symbol-dependent stuff */
  if (the_insn.reloc != BFD_RELOC_NONE)
    {
      fix_new_exp (frag_now,	/* which frag */
		   (toP - frag_now->fr_literal),	/* where */
		   4,		/* size */
		   &the_insn.exp,
		   the_insn.pcrel,
		   the_insn.reloc);
    }

  switch (special_case)
    {
    case SPECIAL_CASE_SET:
      special_case = 0;
      assert (the_insn.reloc == BFD_RELOC_HI22);
      /* See if "set" operand has no low-order bits; skip OR if so. */
      if (the_insn.exp.X_op == O_constant
	  && ((the_insn.exp.X_add_number & 0x3FF) == 0))
	return;
      toP = frag_more (4);
      rsd = (the_insn.opcode >> 25) & 0x1f;
      the_insn.opcode = 0x80102000 | (rsd << 25) | (rsd << 14);
      md_number_to_chars (toP, (valueT) the_insn.opcode, 4);
      fix_new_exp (frag_now,	/* which frag */
		   (toP - frag_now->fr_literal),	/* where */
		   4,		/* size */
		   &the_insn.exp,
		   the_insn.pcrel,
		   BFD_RELOC_LO10);
      return;

    case SPECIAL_CASE_FDIV:
      /* According to information leaked from Sun, the "fdiv" instructions
	 on early SPARC machines would produce incorrect results sometimes.
	 The workaround is to add an fmovs of the destination register to
	 itself just after the instruction.  This was true on machines
	 with Weitek 1165 float chips, such as the Sun-4/260 and /280. */
      special_case = 0;
      assert (the_insn.reloc == BFD_RELOC_NONE);
      toP = frag_more (4);
      rsd = (the_insn.opcode >> 25) & 0x1f;
      the_insn.opcode = 0x81A00020 | (rsd << 25) | rsd;	/* fmovs dest,dest */
      md_number_to_chars (toP, (valueT) the_insn.opcode, 4);
      return;

    case 0:
      return;

    default:
      as_fatal ("failed sanity check.");
    }
}

/* Implement big shift right.  */
static bfd_vma
BSR (val, amount)
     bfd_vma val;
     int amount;
{
  if (sizeof (bfd_vma) <= 4 && amount >= 32)
    as_fatal ("Support for 64-bit arithmetic not compiled in.");
  return val >> amount;
}

/* Parse an argument that can be expressed as a keyword.
   (eg: #StoreStore).
   The result is a boolean indicating success.
   If successful, INPUT_POINTER is updated.  */

static int
parse_keyword_arg (lookup_fn, input_pointerP, valueP)
     int (*lookup_fn) ();
     char **input_pointerP;
     int *valueP;
{
  int value;
  char c, *p, *q;

  p = *input_pointerP;
  for (q = p + (*p == '#'); isalpha (*q) || *q == '_'; ++q)
    continue;
  c = *q;
  *q = 0;
  value = (*lookup_fn) (p);
  *q = c;
  if (value == -1)
    return 0;
  *valueP = value;
  *input_pointerP = q;
  return 1;
}

/* Parse an argument that is a constant expression.
   The result is a boolean indicating success.  */

static int
parse_const_expr_arg (input_pointerP, valueP)
     char **input_pointerP;
     int *valueP;
{
  char *save = input_line_pointer;
  expressionS exp;

  input_line_pointer = *input_pointerP;
  /* The next expression may be something other than a constant
     (say if we're not processing the right variant of the insn).
     Don't call expression unless we're sure it will succeed as it will
     signal an error (which we want to defer until later).  */
  /* FIXME: It might be better to define md_operand and have it recognize
     things like %asi, etc. but continuing that route through to the end
     is a lot of work.  */
  if (*input_line_pointer == '%')
    {
      input_line_pointer = save;
      return 0;
    }
  expression (&exp);
  *input_pointerP = input_line_pointer;
  input_line_pointer = save;
  if (exp.X_op != O_constant)
    return 0;
  *valueP = exp.X_add_number;
  return 1;
}

static void
sparc_ip (str)
     char *str;
{
  char *error_message = "";
  char *s;
  const char *args;
  char c;
  const struct sparc_opcode *insn;
  char *argsStart;
  unsigned long opcode;
  unsigned int mask = 0;
  int match = 0;
  int comma = 0;
  long immediate_max = 0;

  for (s = str; islower (*s) || (*s >= '0' && *s <= '3'); ++s)
    ;

  switch (*s)
    {
    case '\0':
      break;

    case ',':
      comma = 1;

      /*FALLTHROUGH */

    case ' ':
      *s++ = '\0';
      break;

    default:
      as_fatal ("Unknown opcode: `%s'", str);
    }
  insn = (struct sparc_opcode *) hash_find (op_hash, str);
  if (insn == NULL)
    {
      as_bad ("Unknown opcode: `%s'", str);
      return;
    }
  if (comma)
    {
      *--s = ',';
    }

  argsStart = s;
  for (;;)
    {
      opcode = insn->match;
      memset (&the_insn, '\0', sizeof (the_insn));
      the_insn.reloc = BFD_RELOC_NONE;

      /*
       * Build the opcode, checking as we go to make
       * sure that the operands match
       */
      for (args = insn->args;; ++args)
	{
	  switch (*args)
	    {
#ifndef NO_V9
	    case 'K':
	      {
		int kmask = 0;

		/* Parse a series of masks.  */
		if (*s == '#')
		  {
		    while (*s == '#')
		      {
			int mask;

			if (! parse_keyword_arg (sparc_encode_membar, &s,
						 &mask))
			  {
			    error_message = ": invalid membar mask name";
			    goto error;
			  }
			kmask |= mask;
			while (*s == ' ') { ++s; continue; }
			if (*s == '|' || *s == '+')
			  ++s;
			while (*s == ' ') { ++s; continue; }
		      }
		  }
		else
		  {
		    if (! parse_const_expr_arg (&s, &kmask))
		      {
			error_message = ": invalid membar mask expression";
			goto error;
		      }
		    if (kmask < 0 || kmask > 127)
		      {
			error_message = ": invalid membar mask number";
			goto error;
		      }
		  }

		opcode |= MEMBAR (kmask);
		continue;
	      }

	    case '*':
	      {
		int fcn = 0;

		/* Parse a prefetch function.  */
		if (*s == '#')
		  {
		    if (! parse_keyword_arg (sparc_encode_prefetch, &s, &fcn))
		      {
			error_message = ": invalid prefetch function name";
			goto error;
		      }
		  }
		else
		  {
		    if (! parse_const_expr_arg (&s, &fcn))
		      {
			error_message = ": invalid prefetch function expression";
			goto error;
		      }
		    if (fcn < 0 || fcn > 31)
		      {
			error_message = ": invalid prefetch function number";
			goto error;
		      }
		  }
		opcode |= RD (fcn);
		continue;
	      }

	    case '!':
	    case '?':
	      /* Parse a privileged register.  */
	      if (*s == '%')
		{
		  struct priv_reg_entry *p = priv_reg_table;
		  unsigned int len = 9999999; /* init to make gcc happy */

		  s += 1;
		  while (p->name[0] > s[0])
		    p++;
		  while (p->name[0] == s[0])
		    {
		      len = strlen (p->name);
		      if (strncmp (p->name, s, len) == 0)
			break;
		      p++;
		    }
		  if (p->name[0] != s[0])
		    {
		      error_message = ": unrecognizable privileged register";
		      goto error;
		    }
		  if (*args == '?')
		    opcode |= (p->regnum << 14);
		  else
		    opcode |= (p->regnum << 25);
		  s += len;
		  continue;
		}
	      else
		{
		  error_message = ": unrecognizable privileged register";
		  goto error;
		}
#endif

	    case 'M':
	    case 'm':
	      if (strncmp (s, "%asr", 4) == 0)
		{
		  s += 4;

		  if (isdigit (*s))
		    {
		      long num = 0;

		      while (isdigit (*s))
			{
			  num = num * 10 + *s - '0';
			  ++s;
			}

		      if (num < 16 || 31 < num)
			{
			  error_message = ": asr number must be between 15 and 31";
			  goto error;
			}	/* out of range */

		      opcode |= (*args == 'M' ? RS1 (num) : RD (num));
		      continue;
		    }
		  else
		    {
		      error_message = ": expecting %asrN";
		      goto error;
		    }		/* if %asr followed by a number. */

		}		/* if %asr */
	      break;

#ifndef NO_V9
	    case 'I':
	      the_insn.reloc = BFD_RELOC_SPARC_11;
	      immediate_max = 0x03FF;
	      goto immediate;

	    case 'j':
	      the_insn.reloc = BFD_RELOC_SPARC_10;
	      immediate_max = 0x01FF;
	      goto immediate;

	    case 'k':
	      the_insn.reloc = /* RELOC_WDISP2_14 */ BFD_RELOC_SPARC_WDISP16;
	      the_insn.pcrel = 1;
	      goto immediate;

	    case 'G':
	      the_insn.reloc = BFD_RELOC_SPARC_WDISP19;
	      the_insn.pcrel = 1;
	      goto immediate;

	    case 'N':
	      if (*s == 'p' && s[1] == 'n')
		{
		  s += 2;
		  continue;
		}
	      break;

	    case 'T':
	      if (*s == 'p' && s[1] == 't')
		{
		  s += 2;
		  continue;
		}
	      break;

	    case 'z':
	      if (*s == ' ')
		{
		  ++s;
		}
	      if (strncmp (s, "%icc", 4) == 0)
		{
		  s += 4;
		  continue;
		}
	      break;

	    case 'Z':
	      if (*s == ' ')
		{
		  ++s;
		}
	      if (strncmp (s, "%xcc", 4) == 0)
		{
		  s += 4;
		  continue;
		}
	      break;

	    case '6':
	      if (*s == ' ')
		{
		  ++s;
		}
	      if (strncmp (s, "%fcc0", 5) == 0)
		{
		  s += 5;
		  continue;
		}
	      break;

	    case '7':
	      if (*s == ' ')
		{
		  ++s;
		}
	      if (strncmp (s, "%fcc1", 5) == 0)
		{
		  s += 5;
		  continue;
		}
	      break;

	    case '8':
	      if (*s == ' ')
		{
		  ++s;
		}
	      if (strncmp (s, "%fcc2", 5) == 0)
		{
		  s += 5;
		  continue;
		}
	      break;

	    case '9':
	      if (*s == ' ')
		{
		  ++s;
		}
	      if (strncmp (s, "%fcc3", 5) == 0)
		{
		  s += 5;
		  continue;
		}
	      break;

	    case 'P':
	      if (strncmp (s, "%pc", 3) == 0)
		{
		  s += 3;
		  continue;
		}
	      break;

	    case 'W':
	      if (strncmp (s, "%tick", 5) == 0)
		{
		  s += 5;
		  continue;
		}
	      break;
#endif /* NO_V9 */

	    case '\0':		/* end of args */
	      if (*s == '\0')
		{
		  match = 1;
		}
	      break;

	    case '+':
	      if (*s == '+')
		{
		  ++s;
		  continue;
		}
	      if (*s == '-')
		{
		  continue;
		}
	      break;

	    case '[':		/* these must match exactly */
	    case ']':
	    case ',':
	    case ' ':
	      if (*s++ == *args)
		continue;
	      break;

	    case '#':		/* must be at least one digit */
	      if (isdigit (*s++))
		{
		  while (isdigit (*s))
		    {
		      ++s;
		    }
		  continue;
		}
	      break;

	    case 'C':		/* coprocessor state register */
	      if (strncmp (s, "%csr", 4) == 0)
		{
		  s += 4;
		  continue;
		}
	      break;

	    case 'b':		/* next operand is a coprocessor register */
	    case 'c':
	    case 'D':
	      if (*s++ == '%' && *s++ == 'c' && isdigit (*s))
		{
		  mask = *s++;
		  if (isdigit (*s))
		    {
		      mask = 10 * (mask - '0') + (*s++ - '0');
		      if (mask >= 32)
			{
			  break;
			}
		    }
		  else
		    {
		      mask -= '0';
		    }
		  switch (*args)
		    {

		    case 'b':
		      opcode |= mask << 14;
		      continue;

		    case 'c':
		      opcode |= mask;
		      continue;

		    case 'D':
		      opcode |= mask << 25;
		      continue;
		    }
		}
	      break;

	    case 'r':		/* next operand must be a register */
	    case '1':
	    case '2':
	    case 'd':
	      if (*s++ == '%')
		{
		  switch (c = *s++)
		    {

		    case 'f':	/* frame pointer */
		      if (*s++ == 'p')
			{
			  mask = 0x1e;
			  break;
			}
		      goto error;

		    case 'g':	/* global register */
		      if (isoctal (c = *s++))
			{
			  mask = c - '0';
			  break;
			}
		      goto error;

		    case 'i':	/* in register */
		      if (isoctal (c = *s++))
			{
			  mask = c - '0' + 24;
			  break;
			}
		      goto error;

		    case 'l':	/* local register */
		      if (isoctal (c = *s++))
			{
			  mask = (c - '0' + 16);
			  break;
			}
		      goto error;

		    case 'o':	/* out register */
		      if (isoctal (c = *s++))
			{
			  mask = (c - '0' + 8);
			  break;
			}
		      goto error;

		    case 's':	/* stack pointer */
		      if (*s++ == 'p')
			{
			  mask = 0xe;
			  break;
			}
		      goto error;

		    case 'r':	/* any register */
		      if (!isdigit (c = *s++))
			{
			  goto error;
			}
		      /* FALLTHROUGH */
		    case '0':
		    case '1':
		    case '2':
		    case '3':
		    case '4':
		    case '5':
		    case '6':
		    case '7':
		    case '8':
		    case '9':
		      if (isdigit (*s))
			{
			  if ((c = 10 * (c - '0') + (*s++ - '0')) >= 32)
			    {
			      goto error;
			    }
			}
		      else
			{
			  c -= '0';
			}
		      mask = c;
		      break;

		    default:
		      goto error;
		    }

		  /* Got the register, now figure out where
		     it goes in the opcode.  */
		  switch (*args)
		    {

		    case '1':
		      opcode |= mask << 14;
		      continue;

		    case '2':
		      opcode |= mask;
		      continue;

		    case 'd':
		      opcode |= mask << 25;
		      continue;

		    case 'r':
		      opcode |= (mask << 25) | (mask << 14);
		      continue;
		    }
		}
	      break;

	    case 'e':		/* next operand is a floating point register */
	    case 'v':
	    case 'V':

	    case 'f':
	    case 'B':
	    case 'R':

	    case 'g':
	    case 'H':
	    case 'J':
	      {
		char format;

		if (*s++ == '%'
		    && ((format = *s) == 'f')
		    && isdigit (*++s))
		  {
		    for (mask = 0; isdigit (*s); ++s)
		      {
			mask = 10 * mask + (*s - '0');
		      }		/* read the number */

		    if ((*args == 'v'
			 || *args == 'B'
			 || *args == 'H')
			&& (mask & 1))
		      {
			break;
		      }		/* register must be even numbered */

		    if ((*args == 'V'
			 || *args == 'R'
			 || *args == 'J')
			&& (mask & 3))
		      {
			break;
		      }		/* register must be multiple of 4 */

#ifndef NO_V9
		    if (mask >= 64)
		      {
			error_message = ": There are only 64 f registers; [0-63]";
			goto error;
		      }	/* on error */
		    if (mask >= 32)
		      {
			mask -= 31;
		      }	/* wrap high bit */
#else
		    if (mask >= 32)
		      {
			error_message = ": There are only 32 f registers; [0-31]";
			goto error;
		      }	/* on error */
#endif
		  }
		else
		  {
		    break;
		  }	/* if not an 'f' register. */

		switch (*args)
		  {

		  case 'v':
		  case 'V':
		  case 'e':
		    opcode |= RS1 (mask);
		    continue;


		  case 'f':
		  case 'B':
		  case 'R':
		    opcode |= RS2 (mask);
		    continue;

		  case 'g':
		  case 'H':
		  case 'J':
		    opcode |= RD (mask);
		    continue;
		  }		/* pack it in. */

		know (0);
		break;
	      }			/* float arg */

	    case 'F':
	      if (strncmp (s, "%fsr", 4) == 0)
		{
		  s += 4;
		  continue;
		}
	      break;

	    case 'h':		/* high 22 bits */
	      the_insn.reloc = BFD_RELOC_HI22;
	      goto immediate;

	    case 'l':		/* 22 bit PC relative immediate */
	      the_insn.reloc = BFD_RELOC_SPARC_WDISP22;
	      the_insn.pcrel = 1;
	      goto immediate;

	    case 'L':		/* 30 bit immediate */
	      the_insn.reloc = BFD_RELOC_32_PCREL_S2;
	      the_insn.pcrel = 1;
	      goto immediate;

	    case 'n':		/* 22 bit immediate */
	      the_insn.reloc = BFD_RELOC_SPARC22;
	      goto immediate;

	    case 'i':		/* 13 bit immediate */
	      the_insn.reloc = BFD_RELOC_SPARC13;
	      immediate_max = 0x0FFF;

	      /*FALLTHROUGH */

	    immediate:
	      if (*s == ' ')
		s++;
	      if (*s == '%')
		{
		  if ((c = s[1]) == 'h' && s[2] == 'i')
		    {
		      the_insn.reloc = BFD_RELOC_HI22;
		      s += 3;
		    }
		  else if (c == 'l' && s[2] == 'o')
		    {
		      the_insn.reloc = BFD_RELOC_LO10;
		      s += 3;
		    }
#ifndef NO_V9
		  else if (c == 'u'
			   && s[2] == 'h'
			   && s[3] == 'i')
		    {
		      the_insn.reloc = BFD_RELOC_SPARC_HH22;
		      s += 4;
		    }
		  else if (c == 'u'
			   && s[2] == 'l'
			   && s[3] == 'o')
		    {
		      the_insn.reloc = BFD_RELOC_SPARC_HM10;
		      s += 4;
		    }
#endif /* NO_V9 */
		  else
		    break;
		}
	      /* Note that if the getExpression() fails, we will still
		 have created U entries in the symbol table for the
		 'symbols' in the input string.  Try not to create U
		 symbols for registers, etc.  */
	      {
		/* This stuff checks to see if the expression ends in
		   +%reg.  If it does, it removes the register from
		   the expression, and re-sets 's' to point to the
		   right place.  */

		char *s1;

		for (s1 = s; *s1 && *s1 != ',' && *s1 != ']'; s1++);;

		if (s1 != s && isdigit (s1[-1]))
		  {
		    if (s1[-2] == '%' && s1[-3] == '+')
		      {
			s1 -= 3;
			*s1 = '\0';
			(void) getExpression (s);
			*s1 = '+';
			s = s1;
			continue;
		      }
		    else if (strchr ("goli0123456789", s1[-2]) && s1[-3] == '%' && s1[-4] == '+')
		      {
			s1 -= 4;
			*s1 = '\0';
			(void) getExpression (s);
			*s1 = '+';
			s = s1;
			continue;
		      }
		  }
	      }
	      (void) getExpression (s);
	      s = expr_end;

	      if (the_insn.exp.X_op == O_constant
		  && the_insn.exp.X_add_symbol == 0
		  && the_insn.exp.X_op_symbol == 0)
		{
#ifndef NO_V9
		  /* Handle %uhi/%ulo by moving the upper word to the lower
		     one and pretending it's %hi/%lo.  We also need to watch
		     for %hi/%lo: the top word needs to be zeroed otherwise
		     fixup_segment will complain the value is too big.  */
		  switch (the_insn.reloc)
		    {
		    case BFD_RELOC_SPARC_HH22:
		      the_insn.reloc = BFD_RELOC_HI22;
		      the_insn.exp.X_add_number = BSR (the_insn.exp.X_add_number, 32);
		      break;
		    case BFD_RELOC_SPARC_HM10:
		      the_insn.reloc = BFD_RELOC_LO10;
		      the_insn.exp.X_add_number = BSR (the_insn.exp.X_add_number, 32);
		      break;
		    default:
		      break;
		    case BFD_RELOC_HI22:
		    case BFD_RELOC_LO10:
		      the_insn.exp.X_add_number &= 0xffffffff;
		      break;
		    }
#endif
		  /* For pc-relative call instructions, we reject
		     constants to get better code.  */
		  if (the_insn.pcrel
		      && the_insn.reloc == BFD_RELOC_32_PCREL_S2
		      && in_signed_range (the_insn.exp.X_add_number, 0x3fff)
		      )
		    {
		      error_message = ": PC-relative operand can't be a constant";
		      goto error;
		    }
		  /* Check for invalid constant values.  Don't warn if
		     constant was inside %hi or %lo, since these
		     truncate the constant to fit.  */
		  if (immediate_max != 0
		      && the_insn.reloc != BFD_RELOC_LO10
		      && the_insn.reloc != BFD_RELOC_HI22
		      && !in_signed_range (the_insn.exp.X_add_number,
					   immediate_max)
		      )
		    {
		      if (the_insn.pcrel)
			/* Who knows?  After relocation, we may be within
			   range.  Let the linker figure it out.  */
			{
			  the_insn.exp.X_op = O_symbol;
			  the_insn.exp.X_add_symbol = section_symbol (absolute_section);
			}
		      else
			/* Immediate value is non-pcrel, and out of
                           range.  */
			as_bad ("constant value %ld out of range (%ld .. %ld)",
				the_insn.exp.X_add_number,
				~immediate_max, immediate_max);
		    }
		}

	      /* Reset to prevent extraneous range check.  */
	      immediate_max = 0;

	      continue;

	    case 'a':
	      if (*s++ == 'a')
		{
		  opcode |= ANNUL;
		  continue;
		}
	      break;

	    case 'A':
	      {
		int asi = 0;

		/* Parse an asi.  */
		if (*s == '#')
		  {
		    if (! parse_keyword_arg (sparc_encode_asi, &s, &asi))
		      {
			error_message = ": invalid ASI name";
			goto error;
		      }
		  }
		else
		  {
		    if (! parse_const_expr_arg (&s, &asi))
		      {
			error_message = ": invalid ASI expression";
			goto error;
		      }
		    if (asi < 0 || asi > 255)
		      {
			error_message = ": invalid ASI number";
			goto error;
		      }
		  }
		opcode |= ASI (asi);
		continue;
	      }			/* alternate space */

	    case 'p':
	      if (strncmp (s, "%psr", 4) == 0)
		{
		  s += 4;
		  continue;
		}
	      break;

	    case 'q':		/* floating point queue */
	      if (strncmp (s, "%fq", 3) == 0)
		{
		  s += 3;
		  continue;
		}
	      break;

	    case 'Q':		/* coprocessor queue */
	      if (strncmp (s, "%cq", 3) == 0)
		{
		  s += 3;
		  continue;
		}
	      break;

	    case 'S':
	      if (strcmp (str, "set") == 0)
		{
		  special_case = SPECIAL_CASE_SET;
		  continue;
		}
	      else if (strncmp (str, "fdiv", 4) == 0)
		{
		  special_case = SPECIAL_CASE_FDIV;
		  continue;
		}
	      break;

#ifndef NO_V9
	    case 'o':
	      if (strncmp (s, "%asi", 4) != 0)
		break;
	      s += 4;
	      continue;

	    case 's':
	      if (strncmp (s, "%fprs", 5) != 0)
		break;
	      s += 5;
	      continue;

	    case 'E':
	      if (strncmp (s, "%ccr", 4) != 0)
		break;
	      s += 4;
	      continue;
#endif /* NO_V9 */

	    case 't':
	      if (strncmp (s, "%tbr", 4) != 0)
		break;
	      s += 4;
	      continue;

	    case 'w':
	      if (strncmp (s, "%wim", 4) != 0)
		break;
	      s += 4;
	      continue;

#ifndef NO_V9
	    case 'x':
	      {
		char *push = input_line_pointer;
		expressionS e;

		input_line_pointer = s;
		expression (&e);
		if (e.X_op == O_constant)
		  {
		    int n = e.X_add_number;
		    if (n != e.X_add_number || (n & ~0x1ff) != 0)
		      as_bad ("OPF immediate operand out of range (0-0x1ff)");
		    else
		      opcode |= e.X_add_number << 5;
		  }
		else
		  as_bad ("non-immediate OPF operand, ignored");
		s = input_line_pointer;
		input_line_pointer = push;
		continue;
	      }
#endif

	    case 'y':
	      if (strncmp (s, "%y", 2) != 0)
		break;
	      s += 2;
	      continue;

	    default:
	      as_fatal ("failed sanity check.");
	    }			/* switch on arg code */

	  /* Break out of for() loop.  */
	  break;
	}			/* for each arg that we expect */

    error:
      if (match == 0)
	{
	  /* Args don't match. */
	  if (((unsigned) (&insn[1] - sparc_opcodes)) < NUMOPCODES
	      && (insn->name == insn[1].name
		  || !strcmp (insn->name, insn[1].name)))
	    {
	      ++insn;
	      s = argsStart;
	      continue;
	    }
	  else
	    {
	      as_bad ("Illegal operands%s", error_message);
	      return;
	    }
	}
      else
	{
	  if (insn->architecture > current_architecture
	      || (insn->architecture != current_architecture
		  && current_architecture > v8))
	    {
	      if ((!architecture_requested || warn_on_bump)
		  && !ARCHITECTURES_CONFLICT_P (current_architecture,
						insn->architecture)
		  && !ARCHITECTURES_CONFLICT_P (insn->architecture,
						current_architecture))
		{
		  if (warn_on_bump)
		    {
		      as_warn ("architecture bumped from \"%s\" to \"%s\" on \"%s\"",
			       architecture_pname[current_architecture],
			       architecture_pname[insn->architecture],
			       str);
		    }		/* if warning */

		  current_architecture = insn->architecture;
		}
	      else
		{
		  as_bad ("Architecture mismatch on \"%s\".", str);
		  as_tsktsk (" (Requires %s; current architecture is %s.)",
			     architecture_pname[insn->architecture],
			     architecture_pname[current_architecture]);
		  return;
		}		/* if bump ok else error */
	    }			/* if architecture higher */
	}			/* if no match */

      break;
    }				/* forever looking for a match */

  the_insn.opcode = opcode;
}

static int
getExpression (str)
     char *str;
{
  char *save_in;
  segT seg;

  save_in = input_line_pointer;
  input_line_pointer = str;
  seg = expression (&the_insn.exp);
  if (seg != absolute_section
      && seg != text_section
      && seg != data_section
      && seg != bss_section
      && seg != undefined_section)
    {
      the_insn.error = "bad segment";
      expr_end = input_line_pointer;
      input_line_pointer = save_in;
      return 1;
    }
  expr_end = input_line_pointer;
  input_line_pointer = save_in;
  return 0;
}				/* getExpression() */


/*
  This is identical to the md_atof in m68k.c.  I think this is right,
  but I'm not sure.

  Turn a string in input_line_pointer into a floating point constant of type
  type, and store the appropriate bytes in *litP.  The number of LITTLENUMS
  emitted is stored in *sizeP .  An error message is returned, or NULL on OK.
  */

/* Equal to MAX_PRECISION in atof-ieee.c */
#define MAX_LITTLENUMS 6

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
  char *atof_ieee ();

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
      return "Bad call to MD_ATOF()";
    }
  t = atof_ieee (input_line_pointer, type, words);
  if (t)
    input_line_pointer = t;
  *sizeP = prec * sizeof (LITTLENUM_TYPE);
  for (wordP = words; prec--;)
    {
      md_number_to_chars (litP, (valueT) (*wordP++), sizeof (LITTLENUM_TYPE));
      litP += sizeof (LITTLENUM_TYPE);
    }
  return 0;
}

/*
 * Write out big-endian.
 */
void
md_number_to_chars (buf, val, n)
     char *buf;
     valueT val;
     int n;
{
  number_to_chars_bigendian (buf, val, n);
}

/* Apply a fixS to the frags, now that we know the value it ought to
   hold. */

int
md_apply_fix (fixP, value)
     fixS *fixP;
     valueT *value;
{
  char *buf = fixP->fx_where + fixP->fx_frag->fr_literal;
  offsetT val;

  val = *value;

  assert (fixP->fx_r_type < BFD_RELOC_UNUSED);

  fixP->fx_addnumber = val;	/* Remember value for emit_reloc */

#ifdef OBJ_ELF
  /* FIXME: SPARC ELF relocations don't use an addend in the data
     field itself.  This whole approach should be somehow combined
     with the calls to bfd_perform_relocation.  Also, the value passed
     in by fixup_segment includes the value of a defined symbol.  We
     don't want to include the value of an externally visible symbol.  */
  if (fixP->fx_addsy != NULL)
    {
      if ((S_IS_EXTERN (fixP->fx_addsy)
	   || (sparc_pic_code && ! fixP->fx_pcrel))
	  && S_GET_SEGMENT (fixP->fx_addsy) != absolute_section
	  && S_GET_SEGMENT (fixP->fx_addsy) != undefined_section
	  && ! bfd_is_com_section (S_GET_SEGMENT (fixP->fx_addsy)))
	fixP->fx_addnumber -= S_GET_VALUE (fixP->fx_addsy);
      return 1;
    }
#endif

  /* This is a hack.  There should be a better way to
     handle this.  Probably in terms of howto fields, once
     we can look at these fixups in terms of howtos.  */
  if (fixP->fx_r_type == BFD_RELOC_32_PCREL_S2 && fixP->fx_addsy)
    val += fixP->fx_where + fixP->fx_frag->fr_address;

#ifdef OBJ_AOUT
  /* FIXME: More ridiculous gas reloc hacking.  If we are going to
     generate a reloc, then we just want to let the reloc addend set
     the value.  We do not want to also stuff the addend into the
     object file.  Including the addend in the object file works when
     doing a static link, because the linker will ignore the object
     file contents.  However, the dynamic linker does not ignore the
     object file contents.  */
  if (fixP->fx_addsy != NULL
      && fixP->fx_r_type != BFD_RELOC_32_PCREL_S2)
    val = 0;

  /* When generating PIC code, we do not want an addend for a reloc
     against a local symbol.  We adjust fx_addnumber to cancel out the
     value already included in val, and to also cancel out the
     adjustment which bfd_install_relocation will create.  */
  if (sparc_pic_code
      && fixP->fx_r_type != BFD_RELOC_32_PCREL_S2
      && fixP->fx_addsy != NULL
      && ! S_IS_COMMON (fixP->fx_addsy)
      && (fixP->fx_addsy->bsym->flags & BSF_SECTION_SYM) == 0)
    fixP->fx_addnumber -= 2 * S_GET_VALUE (fixP->fx_addsy);
#endif

  switch (fixP->fx_r_type)
    {
    case BFD_RELOC_16:
      buf[0] = val >> 8;
      buf[1] = val;
      break;

    case BFD_RELOC_32:
      buf[0] = val >> 24;
      buf[1] = val >> 16;
      buf[2] = val >> 8;
      buf[3] = val;
      break;

    case BFD_RELOC_32_PCREL_S2:
      val = (val >>= 2);
      if (! sparc_pic_code
	  || fixP->fx_addsy == NULL
	  || (fixP->fx_addsy->bsym->flags & BSF_SECTION_SYM) != 0)
	++val;
      buf[0] |= (val >> 24) & 0x3f;
      buf[1] = (val >> 16);
      buf[2] = val >> 8;
      buf[3] = val;
      break;

#ifndef NO_V9
    case BFD_RELOC_64:
      {
	bfd_vma valh = BSR (val, 32);
	buf[0] = valh >> 24;
	buf[1] = valh >> 16;
	buf[2] = valh >> 8;
	buf[3] = valh;
	buf[4] = val >> 24;
	buf[5] = val >> 16;
	buf[6] = val >> 8;
	buf[7] = val;
      }
      break;

    case BFD_RELOC_SPARC_11:
      if (((val > 0) && (val & ~0x7ff))
	  || ((val < 0) && (~(val - 1) & ~0x7ff)))
	{
	  as_bad ("relocation overflow.");
	}			/* on overflow */

      buf[2] |= (val >> 8) & 0x7;
      buf[3] = val & 0xff;
      break;

    case BFD_RELOC_SPARC_10:
      if (((val > 0) && (val & ~0x3ff))
	  || ((val < 0) && (~(val - 1) & ~0x3ff)))
	{
	  as_bad ("relocation overflow.");
	}			/* on overflow */

      buf[2] |= (val >> 8) & 0x3;
      buf[3] = val & 0xff;
      break;

    case BFD_RELOC_SPARC_WDISP16:
      if (((val > 0) && (val & ~0x3fffc))
	  || ((val < 0) && (~(val - 1) & ~0x3fffc)))
	{
	  as_bad ("relocation overflow.");
	}			/* on overflow */

      val = (val >>= 2) + 1;
      buf[1] |= ((val >> 14) & 0x3) << 4;
      buf[2] |= (val >> 8) & 0x3f;
      buf[3] = val & 0xff;
      break;

    case BFD_RELOC_SPARC_WDISP19:
      if (((val > 0) && (val & ~0x1ffffc))
	  || ((val < 0) && (~(val - 1) & ~0x1ffffc)))
	{
	  as_bad ("relocation overflow.");
	}			/* on overflow */

      val = (val >>= 2) + 1;
      buf[1] |= (val >> 16) & 0x7;
      buf[2] = (val >> 8) & 0xff;
      buf[3] = val & 0xff;
      break;

    case BFD_RELOC_SPARC_HH22:
      val = BSR (val, 32);
      /* intentional fallthrough */
#endif /* NO_V9 */

#ifndef NO_V9
    case BFD_RELOC_SPARC_LM22:
#endif
    case BFD_RELOC_HI22:
      if (!fixP->fx_addsy)
	{
	  buf[1] |= (val >> 26) & 0x3f;
	  buf[2] = val >> 18;
	  buf[3] = val >> 10;
	}
      else
	{
	  buf[2] = 0;
	  buf[3] = 0;
	}
      break;

    case BFD_RELOC_SPARC22:
      if (val & ~0x003fffff)
	{
	  as_bad ("relocation overflow");
	}			/* on overflow */
      buf[1] |= (val >> 16) & 0x3f;
      buf[2] = val >> 8;
      buf[3] = val & 0xff;
      break;

#ifndef NO_V9
    case BFD_RELOC_SPARC_HM10:
      val = BSR (val, 32);
      /* intentional fallthrough */
#endif /* NO_V9 */

    case BFD_RELOC_LO10:
      if (!fixP->fx_addsy)
	{
	  buf[2] |= (val >> 8) & 0x03;
	  buf[3] = val;
	}
      else
	buf[3] = 0;
      break;

    case BFD_RELOC_SPARC13:
      if (! in_signed_range (val, 0x1fff))
	as_bad ("relocation overflow");

      buf[2] |= (val >> 8) & 0x1f;
      buf[3] = val;
      break;

    case BFD_RELOC_SPARC_WDISP22:
      val = (val >> 2) + 1;
      /* FALLTHROUGH */
    case BFD_RELOC_SPARC_BASE22:
      buf[1] |= (val >> 16) & 0x3f;
      buf[2] = val >> 8;
      buf[3] = val;
      break;

    case BFD_RELOC_NONE:
    default:
      as_bad ("bad or unhandled relocation type: 0x%02x", fixP->fx_r_type);
      break;
    }

  /* Are we finished with this relocation now?  */
  if (fixP->fx_addsy == 0 && !fixP->fx_pcrel)
    fixP->fx_done = 1;

  return 1;
}

/* Translate internal representation of relocation info to BFD target
   format.  */
arelent *
tc_gen_reloc (section, fixp)
     asection *section;
     fixS *fixp;
{
  arelent *reloc;
  bfd_reloc_code_real_type code;

  reloc = (arelent *) bfd_alloc_by_size_t (stdoutput, sizeof (arelent));
  assert (reloc != 0);

  reloc->sym_ptr_ptr = &fixp->fx_addsy->bsym;
  reloc->address = fixp->fx_frag->fr_address + fixp->fx_where;

  switch (fixp->fx_r_type)
    {
    case BFD_RELOC_16:
    case BFD_RELOC_32:
    case BFD_RELOC_HI22:
    case BFD_RELOC_LO10:
    case BFD_RELOC_32_PCREL_S2:
    case BFD_RELOC_SPARC13:
    case BFD_RELOC_SPARC_BASE13:
    case BFD_RELOC_SPARC_WDISP16:
    case BFD_RELOC_SPARC_WDISP19:
    case BFD_RELOC_SPARC_WDISP22:
    case BFD_RELOC_64:
    case BFD_RELOC_SPARC_10:
    case BFD_RELOC_SPARC_11:
    case BFD_RELOC_SPARC_HH22:
    case BFD_RELOC_SPARC_HM10:
    case BFD_RELOC_SPARC_LM22:
    case BFD_RELOC_SPARC_PC_HH22:
    case BFD_RELOC_SPARC_PC_HM10:
    case BFD_RELOC_SPARC_PC_LM22:
      code = fixp->fx_r_type;
      break;
    default:
      abort ();
    }

#if defined (OBJ_ELF) || defined (OBJ_AOUT)
  /* If we are generating PIC code, we need to generate a different
     set of relocs.  */

#ifdef OBJ_ELF
#define GOT_NAME "_GLOBAL_OFFSET_TABLE_"
#else
#define GOT_NAME "__GLOBAL_OFFSET_TABLE_"
#endif

  if (sparc_pic_code)
    {
      switch (code)
	{
	case BFD_RELOC_32_PCREL_S2:
	  if (! S_IS_DEFINED (fixp->fx_addsy)
	      || S_IS_EXTERNAL (fixp->fx_addsy))
	    code = BFD_RELOC_SPARC_WPLT30;
	  break;
	case BFD_RELOC_HI22:
	  if (fixp->fx_addsy != NULL
	      && strcmp (S_GET_NAME (fixp->fx_addsy), GOT_NAME) == 0)
	    code = BFD_RELOC_SPARC_PC22;
	  else
	    code = BFD_RELOC_SPARC_GOT22;
	  break;
	case BFD_RELOC_LO10:
	  if (fixp->fx_addsy != NULL
	      && strcmp (S_GET_NAME (fixp->fx_addsy), GOT_NAME) == 0)
	    code = BFD_RELOC_SPARC_PC10;
	  else
	    code = BFD_RELOC_SPARC_GOT10;
	  break;
	case BFD_RELOC_SPARC13:
	  code = BFD_RELOC_SPARC_GOT13;
	  break;
	default:
	  break;
	}
    }
#endif /* defined (OBJ_ELF) || defined (OBJ_AOUT) */

  reloc->howto = bfd_reloc_type_lookup (stdoutput, code);
  if (reloc->howto == 0)
    {
      as_bad_where (fixp->fx_file, fixp->fx_line,
		    "internal error: can't export reloc type %d (`%s')",
		    fixp->fx_r_type, bfd_get_reloc_code_name (code));
      return 0;
    }

  /* @@ Why fx_addnumber sometimes and fx_offset other times?  */
#ifdef OBJ_AOUT

  if (reloc->howto->pc_relative == 0
      || code == BFD_RELOC_SPARC_PC10
      || code == BFD_RELOC_SPARC_PC22)
    reloc->addend = fixp->fx_addnumber;
  else
    reloc->addend = fixp->fx_offset - reloc->address;

#else /* elf or coff */

  if (reloc->howto->pc_relative == 0
      || code == BFD_RELOC_SPARC_PC10
      || code == BFD_RELOC_SPARC_PC22)
    reloc->addend = fixp->fx_addnumber;
  else if ((fixp->fx_addsy->bsym->flags & BSF_SECTION_SYM) != 0)
    reloc->addend = (section->vma
		     + fixp->fx_addnumber
		     + md_pcrel_from (fixp));
  else
    reloc->addend = fixp->fx_offset;
#endif

  return reloc;
}


#if 0
/* for debugging only */
static void
print_insn (insn)
     struct sparc_it *insn;
{
  const char *const Reloc[] = {
    "RELOC_8",
    "RELOC_16",
    "RELOC_32",
    "RELOC_DISP8",
    "RELOC_DISP16",
    "RELOC_DISP32",
    "RELOC_WDISP30",
    "RELOC_WDISP22",
    "RELOC_HI22",
    "RELOC_22",
    "RELOC_13",
    "RELOC_LO10",
    "RELOC_SFA_BASE",
    "RELOC_SFA_OFF13",
    "RELOC_BASE10",
    "RELOC_BASE13",
    "RELOC_BASE22",
    "RELOC_PC10",
    "RELOC_PC22",
    "RELOC_JMP_TBL",
    "RELOC_SEGOFF16",
    "RELOC_GLOB_DAT",
    "RELOC_JMP_SLOT",
    "RELOC_RELATIVE",
    "NO_RELOC"
  };

  if (insn->error)
    fprintf (stderr, "ERROR: %s\n");
  fprintf (stderr, "opcode=0x%08x\n", insn->opcode);
  fprintf (stderr, "reloc = %s\n", Reloc[insn->reloc]);
  fprintf (stderr, "exp = {\n");
  fprintf (stderr, "\t\tX_add_symbol = %s\n",
	   ((insn->exp.X_add_symbol != NULL)
	    ? ((S_GET_NAME (insn->exp.X_add_symbol) != NULL)
	       ? S_GET_NAME (insn->exp.X_add_symbol)
	       : "???")
	    : "0"));
  fprintf (stderr, "\t\tX_sub_symbol = %s\n",
	   ((insn->exp.X_op_symbol != NULL)
	    ? (S_GET_NAME (insn->exp.X_op_symbol)
	       ? S_GET_NAME (insn->exp.X_op_symbol)
	       : "???")
	    : "0"));
  fprintf (stderr, "\t\tX_add_number = %d\n",
	   insn->exp.X_add_number);
  fprintf (stderr, "}\n");
}
#endif

/*
 * md_parse_option
 *	Invocation line includes a switch not recognized by the base assembler.
 *	See if it's a processor-specific option.  These are:
 *
 *	-bump
 *		Warn on architecture bumps.  See also -A.
 *
 *	-Av6, -Av7, -Av8, -Av9, -Asparclite
 *		Select the architecture.  Instructions or features not
 *		supported by the selected architecture cause fatal errors.
 *
 *		The default is to start at v6, and bump the architecture up
 *		whenever an instruction is seen at a higher level.
 *
 *		If -bump is specified, a warning is printing when bumping to
 *		higher levels.
 *
 *		If an architecture is specified, all instructions must match
 *		that architecture.  Any higher level instructions are flagged
 *		as errors.
 *
 *		if both an architecture and -bump are specified, the
 *		architecture starts at the specified level, but bumps are
 *		warnings.
 *
 * Note:
 *		Bumping between incompatible architectures is always an
 *		error.  For example, from sparclite to v9.
 */

#ifdef OBJ_ELF
CONST char *md_shortopts = "A:K:VQ:sq";
#else
#ifdef OBJ_AOUT
CONST char *md_shortopts = "A:k";
#else
CONST char *md_shortopts = "A:";
#endif
#endif
struct option md_longopts[] = {
#define OPTION_BUMP (OPTION_MD_BASE)
  {"bump", no_argument, NULL, OPTION_BUMP},
#define OPTION_SPARC (OPTION_MD_BASE + 1)
  {"sparc", no_argument, NULL, OPTION_SPARC},
  {NULL, no_argument, NULL, 0}
};
size_t md_longopts_size = sizeof(md_longopts);

int
md_parse_option (c, arg)
     int c;
     char *arg;
{
  switch (c)
    {
    case OPTION_BUMP:
      warn_on_bump = 1;
      break;

    case 'A':
      {
	char *p = arg;
	const char **arch;

	for (arch = architecture_pname; *arch != NULL; ++arch)
	  {
	    if (strcmp (p, *arch) == 0)
	      break;
	  }

	if (*arch == NULL)
	  {
	    as_bad ("invalid architecture -A%s", p);
	    return 0;
	  }
	else
	  {
	    enum sparc_architecture new_arch = arch - architecture_pname;
#ifdef NO_V9
	    if (new_arch == v9)
	      {
		as_error ("v9 support not compiled in");
		return 0;
	      }
#endif
	    current_architecture = new_arch;
	    architecture_requested = 1;
	  }
      }
      break;

    case OPTION_SPARC:
      /* Ignore -sparc, used by SunOS make default .s.o rule.  */
      break;

#ifdef OBJ_AOUT
    case 'k':
      sparc_pic_code = 1;
      break;
#endif

#ifdef OBJ_ELF
    case 'V':
      print_version_id ();
      break;

    case 'Q':
      /* Qy - do emit .comment
	 Qn - do not emit .comment */
      break;

    case 's':
      /* use .stab instead of .stab.excl */
      break;

    case 'q':
      /* quick -- native assembler does fewer checks */
      break;

    case 'K':
      if (strcmp (arg, "PIC") != 0)
	as_warn ("Unrecognized option following -K");
      else
	sparc_pic_code = 1;
      break;
#endif

    default:
      return 0;
    }

 return 1;
}

void
md_show_usage (stream)
     FILE *stream;
{
  const char **arch;
  fprintf(stream, "SPARC options:\n");
  for (arch = architecture_pname; *arch; arch++)
    {
      if (arch != architecture_pname)
	fprintf (stream, " | ");
      fprintf (stream, "-A%s", *arch);
    }
  fprintf (stream, "\n\
			specify variant of SPARC architecture\n\
-bump			warn when assembler switches architectures\n\
-sparc			ignored\n");
#ifdef OBJ_AOUT
  fprintf (stream, "\
-k			generate PIC\n");
#endif
#ifdef OBJ_ELF
  fprintf (stream, "\
-KPIC			generate PIC\n\
-V			print assembler version number\n\
-q			ignored\n\
-Qy, -Qn		ignored\n\
-s			ignored\n");
#endif
}

/* We have no need to default values of symbols. */

/* ARGSUSED */
symbolS *
md_undefined_symbol (name)
     char *name;
{
  return 0;
}				/* md_undefined_symbol() */

/* Round up a section size to the appropriate boundary. */
valueT
md_section_align (segment, size)
     segT segment;
     valueT size;
{
#ifndef OBJ_ELF
  /* This is not right for ELF; a.out wants it, and COFF will force
     the alignment anyways.  */
  valueT align = ((valueT) 1
		  << (valueT) bfd_get_section_alignment (stdoutput, segment));
  valueT newsize;
  /* turn alignment value into a mask */
  align--;
  newsize = (size + align) & ~align;
  return newsize;
#else
  return size;
#endif
}

/* Exactly what point is a PC-relative offset relative TO?
   On the sparc, they're relative to the address of the offset, plus
   its size.  This gets us to the following instruction.
   (??? Is this right?  FIXME-SOON) */
long 
md_pcrel_from (fixP)
     fixS *fixP;
{
  long ret;

  ret = fixP->fx_where + fixP->fx_frag->fr_address;
  if (! sparc_pic_code
      || fixP->fx_addsy == NULL
      || (fixP->fx_addsy->bsym->flags & BSF_SECTION_SYM) != 0)
    ret += fixP->fx_size;
  return ret;
}

/* end of tc-sparc.c */
