/* tc-alpha.c - Processor-specific code for the DEC Alpha CPU.
   Copyright (C) 1989, 1993, 1994 Free Software Foundation, Inc.
   Contributed by Carnegie Mellon University, 1993.
   Written by Alessandro Forin, based on earlier gas-1.38 target CPU files.
   Modified by Ken Raeburn for gas-2.x and ECOFF support.

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
   the Free Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

/*
 * Mach Operating System
 * Copyright (c) 1993 Carnegie Mellon University
 * All Rights Reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */
/*
 * HISTORY
 *  5-Oct-93  Alessandro Forin (af) at Carnegie-Mellon University
 *	First Checkin
 *
 *    Author:	Alessandro Forin, Carnegie Mellon University
 *    Date:	Jan 1993
 */

#include <ctype.h>

#include "as.h"
#include "alpha-opcode.h"
#include "subsegs.h"

/* The OSF/1 V2.0 Alpha compiler can't compile this file with inline
   functions.  */
#ifndef __GNUC__
#undef inline
#define inline
#endif

/* @@ Will a simple 0x8000 work here?  If not, why not?  */
#define GP_ADJUSTMENT	(0x8000 - 0x10)

/* These are exported to relaxing code, even though we don't do any
   relaxing on this processor currently.  */
int md_short_jump_size = 4;
int md_long_jump_size = 4;

/* handle of the OPCODE hash table */
static struct hash_control *op_hash;

/* Sections and symbols we'll want to keep track of.  */
static segT lita_sec, rdata, sdata, lit8_sec, lit4_sec;
static symbolS *lit8_sym, *lit4_sym;

/* Setting for ".set [no]{at,macro}".  */
static int at_ok = 1, macro_ok = 1;

/* Keep track of global pointer.  */
valueT alpha_gp_value;
static symbolS *gp;

/* We'll probably be using this relocation frequently, and we
   will want to compare for it.  */
static reloc_howto_type *gpdisp_hi16_howto;

/* These are exported to ECOFF code.  */
unsigned long alpha_gprmask, alpha_fprmask;

/* Used for LITUSE relocations.  */
static expressionS lituse_basereg, lituse_byteoff, lituse_jsr;

/* Address size: In OSF/1 1.3, an undocumented "-32addr" option will
   cause all addresses to be treated as 32-bit values in memory.  (The
   in-register versions are all sign-extended to 64 bits, of course.)
   Some other systems may want this option too.  */
static int addr32;

/* Symbol labelling the current insn.  When the Alpha gas sees
     foo:
       .quad 0
   and the section happens to not be on an eight byte boundary, it
   will align both the symbol and the .quad to an eight byte boundary.  */
static symbolS *insn_label;

/* Whether we should automatically align data generation pseudo-ops.
   .align 0 will turn this off.  */
static int auto_align = 1;

/* Imported functions -- they should be defined in header files somewhere.  */
extern segT subseg_get ();
extern PTR bfd_alloc_by_size_t ();
extern void s_globl (), s_long (), s_short (), s_space (), cons (), s_text (),
  s_data (), float_cons ();

/* Static functions, needing forward declarations.  */
static void s_base (), s_proc (), s_alpha_set ();
static void s_gprel32 (), s_rdata (), s_sdata (), s_alpha_comm ();
static void s_alpha_text PARAMS ((int));
static void s_alpha_data PARAMS ((int));
static void s_alpha_align PARAMS ((int));
static void s_alpha_cons PARAMS ((int));
static void s_alpha_float_cons PARAMS ((int));
static int alpha_ip ();

static void emit_unaligned_io PARAMS ((char *, int, valueT, int));
static void emit_load_unal PARAMS ((int, valueT, int));
static void emit_store_unal PARAMS ((int, valueT, int));
static void emit_byte_manip_r PARAMS ((char *, int, int, int, int, int));
static void emit_extract_r PARAMS ((int, int, int, int, int));
static void emit_insert_r PARAMS ((int, int, int, int, int));
static void emit_mask_r PARAMS ((int, int, int, int, int));
static void emit_sign_extend PARAMS ((int, int));
static void emit_bis_r PARAMS ((int, int, int));
static int build_mem PARAMS ((int, int, int, bfd_signed_vma));
static int build_operate_n PARAMS ((int, int, int, int, int));
static void emit_sll_n PARAMS ((int, int, int));
static void emit_ldah_num PARAMS ((int, bfd_vma, int));
static void emit_addq_r PARAMS ((int, int, int));
static void emit_lda_n PARAMS ((int, bfd_vma, int));
static void emit_add64 PARAMS ((int, int, bfd_vma));
static int in_range_signed PARAMS ((bfd_vma, int));
static void alpha_align PARAMS ((int, int, symbolS *));

const pseudo_typeS md_pseudo_table[] =
{
  {"common", s_comm, 0},	/* is this used? */
  {"comm", s_alpha_comm, 0},	/* osf1 compiler does this */
  {"text", s_alpha_text, 0},
  {"data", s_alpha_data, 0},
  {"rdata", s_rdata, 0},
  {"sdata", s_sdata, 0},
  {"gprel32", s_gprel32, 0},
  {"t_floating", s_alpha_float_cons, 'd'},
  {"s_floating", s_alpha_float_cons, 'f'},
  {"f_floating", s_alpha_float_cons, 'F'},
  {"g_floating", s_alpha_float_cons, 'G'},
  {"d_floating", s_alpha_float_cons, 'D'},

  {"proc", s_proc, 0},
  {"aproc", s_proc, 1},
  {"set", s_alpha_set, 0},
  {"reguse", s_ignore, 0},
  {"livereg", s_ignore, 0},
  {"base", s_base, 0},		/*??*/
  {"option", s_ignore, 0},
  {"prologue", s_ignore, 0},
  {"aent", s_ignore, 0},
  {"ugen", s_ignore, 0},

  {"align", s_alpha_align, 0},
  {"byte", s_alpha_cons, 0},
  {"hword", s_alpha_cons, 1},
  {"int", s_alpha_cons, 2},
  {"long", s_alpha_cons, 2},
  {"octa", s_alpha_cons, 4},
  {"quad", s_alpha_cons, 3},
  {"short", s_alpha_cons, 1},
  {"word", s_alpha_cons, 1},
  {"double", s_alpha_float_cons, 'd'},
  {"float", s_alpha_float_cons, 'f'},
  {"single", s_alpha_float_cons, 'f'},

/* We don't do any optimizing, so we can safely ignore these.  */
  {"noalias", s_ignore, 0},
  {"alias", s_ignore, 0},

  {NULL, 0, 0},
};

#define	SA	21		/* shift for register Ra */
#define	SB	16		/* shift for register Rb */
#define	SC	0		/* shift for register Rc */
#define	SN	13		/* shift for 8 bit immediate # */

#define	T9	23
#define	T10	24
#define	T11	25
#define T12	26
#define RA	26		/* note: same as T12 */
#define	PV	27
#define	AT	28
#define	GP	29
#define	SP	30
#define	ZERO	31

#define OPCODE(X)	(((X) >> 26) & 0x3f)
#define OP_FCN(X)	(((X) >> 5) & 0x7f)

#ifndef FIRST_32BIT_QUADRANT
#define FIRST_32BIT_QUADRANT 0
#endif

int first_32bit_quadrant = FIRST_32BIT_QUADRANT;
int base_register = FIRST_32BIT_QUADRANT ? ZERO : GP;

int no_mixed_code = 0;
int nofloats = 0;

/* This array holds the chars that always start a comment.  If the
    pre-processor is disabled, these aren't very useful */
const char comment_chars[] = "#";

/* This array holds the chars that only start a comment at the beginning of
   a line.  If the line seems to have the form '# 123 filename'
   .line and .file directives will appear in the pre-processed output */
/* Note that input_file.c hand checks for '#' at the beginning of the
   first line of the input file.  This is because the compiler outputs
   #NO_APP at the beginning of its output. */
/* Also note that C style comments are always recognized.  */
const char line_comment_chars[] = "#!";

/* Chars that can be used to separate mant from exp in floating point nums */
const char EXP_CHARS[] = "eE";

const char line_separator_chars[1];

/* Chars that mean this number is a floating point constant, as in
   "0f12.456" or "0d1.2345e12".  */
/* @@ Do all of these really get used on the alpha??  */
char FLT_CHARS[] = "rRsSfFdDxXpP";

/* Also be aware that MAXIMUM_NUMBER_OF_CHARS_FOR_FLOAT may have to be
   changed in read.c.  Ideally it shouldn't have to know about it at all,
   but nothing is ideal around here.  */

struct reloc_data {
  expressionS exp;
  int pcrel;
  bfd_reloc_code_real_type code;
};

/* Occasionally, two relocations will be desired for one address.
   Mainly only in cases like "jsr $r,foo" where we want both a LITUSE
   and a HINT reloc.  */
#define MAX_RELOCS 2

struct alpha_it {
  unsigned long opcode;	/* need at least 32 bits */
  struct reloc_data reloc[MAX_RELOCS];
};

static void getExpression (char *str, struct alpha_it *insn);
static char *expr_end;

#define note_gpreg(R)		(alpha_gprmask |= (1 << (R)))
#define note_fpreg(R)		(alpha_fprmask |= (1 << (R)))

int
tc_get_register (frame)
     int frame;
{
  int framereg = SP;

  SKIP_WHITESPACE ();
  if (*input_line_pointer == '$')
    {
      input_line_pointer++;
      if (input_line_pointer[0] == 's'
	  && input_line_pointer[1] == 'p')
	{
	  input_line_pointer += 2;
	  framereg = SP;
	}
      else
	framereg = get_absolute_expression ();
      framereg &= 31;		/* ? */
    }
  else
    as_warn ("frame reg expected, using $%d.", framereg);

  note_gpreg (framereg);
  return framereg;
}

/* Handle the .text pseudo-op.  This is like the usual one, but it
   clears insn_label and restores auto alignment.  */

static void
s_alpha_text (i)
     int i;
{
  s_text (i);
  insn_label = NULL;
  auto_align = 1;
}  

/* Handle the .data pseudo-op.  This is like the usual one, but it
   clears insn_label and restores auto alignment.  */

static void
s_alpha_data (i)
     int i;
{
  s_data (i);
  insn_label = NULL;
  auto_align = 1;
}  

static void
s_rdata (ignore)
     int ignore;
{
  int temp;

  temp = get_absolute_expression ();
#if 0
  if (!rdata)
    rdata = subseg_get (".rdata", 0);
  subseg_set (rdata, (subsegT) temp);
#else
  rdata = subseg_new (".rdata", 0);
#endif
  demand_empty_rest_of_line ();
  insn_label = NULL;
  auto_align = 1;
}

static void
s_sdata (ignore)
     int ignore;
{
  int temp;

  temp = get_absolute_expression ();
#if 0
  if (!sdata)
    sdata = subseg_get (".sdata", 0);
  subseg_set (sdata, (subsegT) temp);
#else
  sdata = subseg_new (".sdata", 0);
#endif
  demand_empty_rest_of_line ();
  insn_label = NULL;
  auto_align = 1;
}

static void
s_alpha_comm (ignore)
     int ignore;
{
  register char *name;
  register char c;
  register char *p;
  offsetT temp;
  register symbolS *symbolP;

  name = input_line_pointer;
  c = get_symbol_end ();
  /* just after name is now '\0' */
  p = input_line_pointer;
  *p = c;
  SKIP_WHITESPACE ();
  /* Alpha OSF/1 compiler doesn't provide the comma, gcc does.  */
  if (*input_line_pointer == ',')
    {
      input_line_pointer++;
      SKIP_WHITESPACE ();
    }
  if ((temp = get_absolute_expression ()) < 0)
    {
      as_warn (".COMMon length (%ld.) <0! Ignored.", (long) temp);
      ignore_rest_of_line ();
      return;
    }
  *p = 0;
  symbolP = symbol_find_or_make (name);
  *p = c;
  if (S_IS_DEFINED (symbolP))
    {
      as_bad ("Ignoring attempt to re-define symbol");
      ignore_rest_of_line ();
      return;
    }
  if (S_GET_VALUE (symbolP))
    {
      if (S_GET_VALUE (symbolP) != (valueT) temp)
	as_bad ("Length of .comm \"%s\" is already %ld. Not changed to %ld.",
		S_GET_NAME (symbolP),
		(long) S_GET_VALUE (symbolP),
		(long) temp);
    }
  else
    {
      S_SET_VALUE (symbolP, (valueT) temp);
      S_SET_EXTERNAL (symbolP);
    }

  know (symbolP->sy_frag == &zero_address_frag);
  demand_empty_rest_of_line ();
}

arelent *
tc_gen_reloc (sec, fixp)
     asection *sec;
     fixS *fixp;
{
  arelent *reloc;

  reloc = (arelent *) bfd_alloc_by_size_t (stdoutput, sizeof (arelent));
  reloc->sym_ptr_ptr = &fixp->fx_addsy->bsym;
  reloc->address = fixp->fx_frag->fr_address + fixp->fx_where;

  if (fixp->fx_r_type > BFD_RELOC_UNUSED)
    abort ();

  if (fixp->fx_r_type == BFD_RELOC_ALPHA_GPDISP_HI16)
    {
      if (!gpdisp_hi16_howto)
	gpdisp_hi16_howto = bfd_reloc_type_lookup (stdoutput,
						   fixp->fx_r_type);
      reloc->howto = gpdisp_hi16_howto;
    }
  else
    reloc->howto = bfd_reloc_type_lookup (stdoutput, fixp->fx_r_type);
  assert (reloc->howto != 0);
  if (!fixp->fx_pcrel != !reloc->howto->pc_relative)
    {
      as_fatal ("internal error? cannot generate `%s' relocation",
		bfd_get_reloc_code_name (fixp->fx_r_type));
    }
  assert (!fixp->fx_pcrel == !reloc->howto->pc_relative);

  if (fixp->fx_r_type == BFD_RELOC_ALPHA_LITERAL)
    {
      /* fake out bfd_perform_relocation. sigh */
      reloc->addend = -alpha_gp_value;
    }
  else if (reloc->howto->pc_relative && reloc->howto->pcrel_offset)
    {
      reloc->addend = fixp->fx_offset - reloc->address;
    }
  else
    reloc->addend = fixp->fx_offset;
  return reloc;
}

static void
s_base ()
{
  if (first_32bit_quadrant)
    {
      /* not fatal, but it might not work in the end */
      as_warn ("File overrides no-base-register option.");
      first_32bit_quadrant = 0;
    }

  SKIP_WHITESPACE ();
  if (*input_line_pointer == '$')
    {				/* $rNN form */
      input_line_pointer++;
      if (*input_line_pointer == 'r')
	input_line_pointer++;
    }

  base_register = get_absolute_expression ();
  if (base_register < 0 || base_register > 31)
    {
      base_register = GP;
      as_warn ("Bad base register, using $%d.", base_register);
    }
  demand_empty_rest_of_line ();
}

static int in_range_signed (val, nbits)
     bfd_vma val;
     int nbits;
{
  /* Look at top bit of value that would be stored, figure out how it
     would be extended by the hardware, and see if that matches the
     original supplied value.  */
  bfd_vma mask;
  bfd_vma one = 1;
  bfd_vma top_bit, stored_value, missing_bits;

  mask = (one << nbits) - 1;
  stored_value = val & mask;
  top_bit = stored_value & (one << (nbits - 1));
  missing_bits = val & ~mask;
  /* will sign-extend */
  if (top_bit)
    {
      /* all remaining bits beyond mask should be one */
      missing_bits |= mask;
      return missing_bits + 1 == 0;
    }
  else
    {
      /* all other bits should be zero */
      return missing_bits == 0;
    }
}

#if 0
static int in_range_unsigned (val, nbits)
     bfd_vma val;
     int nbits;
{
  /* Look at top bit of value that would be stored, figure out how it
     would be extended by the hardware, and see if that matches the
     original supplied value.  */
  bfd_vma mask;
  bfd_vma one = 1;
  bfd_vma top_bit, stored_value, missing_bits;

  mask = (one << nbits) - 1;
  stored_value = val & mask;
  top_bit = stored_value & (one << nbits - 1);
  missing_bits = val & ~mask;
  return missing_bits == 0;
}
#endif

static void
s_gprel32 ()
{
  expressionS e;
  char *p;

  SKIP_WHITESPACE ();
  expression (&e);
  switch (e.X_op)
    {
    case O_constant:
      e.X_add_symbol = section_symbol (absolute_section);
      /* fall through */
    case O_symbol:
      e.X_op = O_subtract;
      e.X_op_symbol = gp;
      break;
    default:
      abort ();
    }
  if (auto_align)
    alpha_align (2, 0, insn_label);
  p = frag_more (4);
  memset (p, 0, 4);
  fix_new_exp (frag_now, p - frag_now->fr_literal, 4, &e, 0,
	       BFD_RELOC_GPREL32);
  insn_label = NULL;
}

static void
create_literal_section (secp, name)
     segT *secp;
     const char *name;
{
  segT current_section = now_seg;
  int current_subsec = now_subseg;
  segT new_sec;

  *secp = new_sec = subseg_new (name, 0);
  subseg_set (current_section, current_subsec);
  bfd_set_section_alignment (stdoutput, new_sec, 3);
  bfd_set_section_flags (stdoutput, new_sec,
			 SEC_RELOC | SEC_ALLOC | SEC_LOAD | SEC_READONLY
			 | SEC_DATA);
}

static valueT
get_lit8_offset (val)
     bfd_vma val;
{
  valueT retval;
  if (lit8_sec == 0)
    {
      create_literal_section (&lit8_sec, ".lit8");
      lit8_sym = section_symbol (lit8_sec);
    }
  retval = add_to_literal_pool ((symbolS *) 0, val, lit8_sec, 8);
  if (retval >= 0xfff0)
    as_fatal ("overflow in fp literal (.lit8) table");
  return retval;
}

static valueT
get_lit4_offset (val)
     bfd_vma val;
{
  valueT retval;
  if (lit4_sec == 0)
    {
      create_literal_section (&lit4_sec, ".lit4");
      lit4_sym = section_symbol (lit4_sec);
    }
  retval = add_to_literal_pool ((symbolS *) 0, val, lit4_sec, 4);
  if (retval >= 0xfff0)
    as_fatal ("overflow in fp literal (.lit4) table");
  return retval;
}

static struct alpha_it clear_insn;

/* This function is called once, at assembler startup time.  It should
   set up all the tables, etc. that the MD part of the assembler will
   need, that can be determined before arguments are parsed.  */
void
md_begin ()
{
  const char *retval, *name;
  unsigned int i = 0;

  op_hash = hash_new ();

  for (i = 0; i < NUMOPCODES; )
    {
      const char *name = alpha_opcodes[i].name;
      retval = hash_insert (op_hash, name, (PTR) &alpha_opcodes[i]);
      if (retval)
	as_fatal ("internal error: can't hash opcode `%s': %s",
		  name, retval);

      do
	i++;
      while (i < NUMOPCODES
	     && (alpha_opcodes[i].name == name
		 || !strcmp (alpha_opcodes[i].name, name)));
    }
  /* Some opcodes include modifiers of various sorts with a "/mod"
     syntax, like the architecture documentation suggests.  However,
     for use with gcc at least, we also need to access those same
     opcodes without the "/".  */
  for (i = 0; i < NUMOPCODES; )
    {
      name = alpha_opcodes[i].name;

      if (strchr (name, '/'))
	{
	  char *p = xmalloc (strlen (name));
	  const char *q = name;
	  char *q2 = p;

	  for (; *q; q++)
	    if (*q != '/')
	      *q2++ = *q;

	  *q2++ = 0;
	  retval = hash_insert (op_hash, p, (PTR) &alpha_opcodes[i]);
	  /* Ignore failures -- the opcode table does duplicate some
	     variants in different forms, like "hw_stq" and "hw_st/q".
	     Maybe the variants can be eliminated, and this error
	     checking restored.  */
	}

      do
	i++;
      while (i < NUMOPCODES
	     && (alpha_opcodes[i].name == name
		 || !strcmp (alpha_opcodes[i].name, name)));
    }

  lituse_basereg.X_op = O_constant;
  lituse_basereg.X_add_number = 1;
  lituse_byteoff.X_op = O_constant;
  lituse_byteoff.X_add_number = 2;
  lituse_jsr.X_op = O_constant;
  lituse_jsr.X_add_number = 3;

  /* So .sbss will get used for tiny objects.  */
  bfd_set_gp_size (stdoutput, 8);
  create_literal_section (&lita_sec, ".lita");
  /* For handling the GP, create a symbol that won't be output in the
     symbol table.  We'll edit it out of relocs later.  */
  gp = symbol_create ("<GP value>", lita_sec, 0x8000, &zero_address_frag);

  memset (&clear_insn, 0, sizeof (clear_insn));
  for (i = 0; i < MAX_RELOCS; i++)
    clear_insn.reloc[i].code = BFD_RELOC_NONE;
}

int optnum = 1;

static void
emit_insn (insn)
     struct alpha_it *insn;
{
  char *toP;
  int j;

  toP = frag_more (4);

  /* put out the opcode */
  md_number_to_chars (toP, insn->opcode, 4);

  /* put out the symbol-dependent stuff */
  for (j = 0; j < MAX_RELOCS; j++)
    {
      struct reloc_data *r = &insn->reloc[j];
      fixS *f;

      if (r->code != BFD_RELOC_NONE)
	{
	  if (r->exp.X_op == O_constant)
	    {
	      r->exp.X_add_symbol = section_symbol (absolute_section);
	      r->exp.X_op = O_symbol;
	    }
	  f = fix_new_exp (frag_now, (toP - frag_now->fr_literal), 4,
			   &r->exp, r->pcrel, r->code);
	  if (r->code == BFD_RELOC_ALPHA_GPDISP_LO16)
	    {
	      static bit_fixS cookie;
	      /* @@ This'll make the range checking in write.c shut up.  */
	      f->fx_bit_fixP = &cookie;
	    }
	}
    }

  insn_label = NULL;
}

void
md_assemble (str)
     char *str;
{
  int i, count;
#define	MAX_INSNS	5
  struct alpha_it insns[MAX_INSNS];

  count = alpha_ip (str, insns);
  if (count <= 0)
    return;

  for (i = 0; i < count; i++)
    emit_insn (&insns[i]);
}

static inline void
maybe_set_gp (sec)
     asection *sec;
{
  bfd_vma vma;
  if (!sec)
    return;
  vma = bfd_get_section_vma (foo, sec);
  if (vma && vma < alpha_gp_value)
    alpha_gp_value = vma;
}

static void
select_gp_value ()
{
  if (alpha_gp_value != 0)
    abort ();

  /* Get minus-one in whatever width...  */
  alpha_gp_value = 0; alpha_gp_value--;

  /* Select the smallest VMA of these existing sections.  */
  maybe_set_gp (lita_sec);
/* maybe_set_gp (sdata);   Was disabled before -- should we use it?  */
#if 0
  maybe_set_gp (lit8_sec);
  maybe_set_gp (lit4_sec);
#endif

  alpha_gp_value += GP_ADJUSTMENT;

  S_SET_VALUE (gp, alpha_gp_value);

#ifdef DEBUG1
  printf ("Chose GP value of %lx\n", alpha_gp_value);
#endif
}

int
alpha_force_relocation (f)
     fixS *f;
{
  switch (f->fx_r_type)
    {
    case BFD_RELOC_ALPHA_GPDISP_HI16:
    case BFD_RELOC_ALPHA_GPDISP_LO16:
    case BFD_RELOC_ALPHA_LITERAL:
    case BFD_RELOC_ALPHA_LITUSE:
    case BFD_RELOC_GPREL32:
      return 1;
    case BFD_RELOC_ALPHA_HINT:
    case BFD_RELOC_64:
    case BFD_RELOC_32:
    case BFD_RELOC_16:
    case BFD_RELOC_8:
    case BFD_RELOC_23_PCREL_S2:
    case BFD_RELOC_14:
    case BFD_RELOC_26:
      return 0;
    default:
      abort ();
      return 0;
    }
}

int
alpha_fix_adjustable (f)
     fixS *f;
{
  /* Are there any relocation types for which we must generate a reloc
     but we can adjust the values contained within it?  */
  switch (f->fx_r_type)
    {
    case BFD_RELOC_ALPHA_GPDISP_HI16:
    case BFD_RELOC_ALPHA_GPDISP_LO16:
      return 0;
    case BFD_RELOC_GPREL32:
      return 1;
    default:
      return !alpha_force_relocation (f);
    }
  /*NOTREACHED*/
}

valueT
md_section_align (seg, size)
     segT seg;
     valueT size;
{
#ifdef OBJ_ECOFF
  /* This should probably be handled within BFD, or by pulling the
     number from BFD at least.  */
#define MIN 15
  size += MIN;
  size &= ~MIN;
#endif
  return size;
}

/* Add this thing to the .lita section and produce a LITERAL reloc referring
   to it.  */

/* Are we currently eligible to emit a LITUSE reloc for the literal
   references just generated?  */
static int lituse_pending;

static void
load_symbol_address (reg, insn)
     int reg;
     struct alpha_it *insn;
{
  static symbolS *lita_sym;

  int x;
  valueT retval;

  if (!lita_sym)
    {
      lita_sym = section_symbol (lita_sec);
      S_CLEAR_EXTERNAL (lita_sym);
    }

  retval = add_to_literal_pool (insn->reloc[0].exp.X_add_symbol,
				insn->reloc[0].exp.X_add_number,
				lita_sec, 8);

  /* Now emit a LITERAL relocation for the original section.  */
  insn->reloc[0].exp.X_op = O_symbol;
  insn->reloc[0].exp.X_add_symbol = lita_sym;
  insn->reloc[0].exp.X_add_number = retval;
  insn->reloc[0].code = BFD_RELOC_ALPHA_LITERAL;
  lituse_pending = 1;

  if (retval == 0x8000)
    /* Overflow? */
    as_fatal ("overflow in literal (.lita) table");
  x = retval;
  if (addr32)
    insn->opcode = (0xa0000000	/* ldl */
		    | (reg << SA)
		    | (base_register << SB)
		    | (x & 0xffff));
  else
    insn->opcode = (0xa4000000	/* ldq */
		    | (reg << SA)
		    | (base_register << SB)
		    | (x & 0xffff));
  note_gpreg (base_register);
}

/* To load an address with a single instruction,
   emit a LITERAL reloc in this section, and a REFQUAD
   for the .lita section, so that we'll be able to access
   it via $gp:
		lda REG, xx	->	ldq REG, -32752(gp)
		lda REG, xx+4	->	ldq REG, -32752(gp)
					lda REG, 4(REG)

   The offsets need to start near -0x8000, and the generated LITERAL
   relocations should negate the offset.  I don't completely grok the
   scheme yet.  */

static int
load_expression (reg, insn)
     int reg;
     struct alpha_it *insn;
{
  valueT addend, addendhi, addendlo;
  int num_insns = 1;

  if (insn->reloc[0].exp.X_add_symbol->bsym->flags & BSF_SECTION_SYM)
    {
      addend = 0;
    }
  else
    {
      addend = insn->reloc[0].exp.X_add_number;
      insn->reloc[0].exp.X_add_number = 0;
    }
  load_symbol_address (reg, insn);
  if (addend)
    {
      if ((addend & ~0x7fffffff) != 0
	  && (addend & ~0x7fffffff) + 0x80000000 != 0)
	{
	  as_bad ("assembler not prepared to handle constants >32 bits yet");
	  addend = 0;
	}
      addendlo = addend & 0xffff;
      addend -= addendlo;
      addendhi = addend >> 16;
      if (addendlo & 0x8000)
	addendhi++;
      /* It appears that the BASEREG LITUSE reloc should not be used on
	 an LDAH instruction.  */
      if (addendlo)
	{
	  insn[1].opcode = (0x20000000	/* lda */
			    | (reg << SA)
			    | (reg << SB)
			    | (addendlo & 0xffff));
	  insn[1].reloc[0].code = BFD_RELOC_ALPHA_LITUSE;
	  insn[1].reloc[0].exp = lituse_basereg;
	  num_insns++;
	}
      if (addendhi)
	{
	  insn[num_insns].opcode = (0x24000000
				    | (reg << SA)
				    | (reg << SB)
				    | (addendhi & 0xffff));
	  num_insns++;
	}
      if (num_insns == 1)
	abort ();
      lituse_pending = 0;
    }
  return num_insns;
}

static inline void
getExpression (str, this_insn)
     char *str;
     struct alpha_it *this_insn;
{
  char *save_in;
  segT seg;

#if 0 /* Not converted to bfd yet, and I don't think we need them
	 for ECOFF.  Re-adding a.out support will probably require
	 them though.  */
  static const struct am {
    char *name;
    bfd_reloc_code_real_type reloc;
  } macro[] = {
    { "hi", RELOC_48_63 },
    { "lo", RELOC_0_15 },
    { "ml", RELOC_16_31 },
    { "mh", RELOC_32_47 },
    { "uhi", RELOC_U_48_63 },
    { "uml", RELOC_U_16_31 },
    { "umh", RELOC_U_32_47 },
    { 0, }
  };

  /* Handle macros: "%macroname(expr)" */
  if (*str == '%')
    {
      struct am *m;
      char *p, *q;

      str++;
      m = &macro[0];
      while (q = m->name)
	{
	  p = str;
	  while (*q && *p == *q)
	    p++, q++;
	  if (*q == 0)
	    break;
	  m++;
	}
      if (q)
	{
	  str = p;		/* keep the '(' */
	  this_insn->reloc = m->reloc;
	}
    }
#endif

  save_in = input_line_pointer;
  input_line_pointer = str;

  seg = expression (&this_insn->reloc[0].exp);
  /* XXX validate seg and exp, make sure they're reasonable */
  expr_end = input_line_pointer;
  input_line_pointer = save_in;
}

static void
emit_unaligned_io (dir, addr_reg, addr_offset, reg)
     char *dir;
     int addr_reg, reg;
     valueT addr_offset;
{
  char buf[90];
  sprintf (buf, "%sq_u $%d,%ld($%d)", dir, reg, (long) addr_offset, addr_reg);
  md_assemble (buf);
}

static void
emit_load_unal (addr_reg, addr_offset, reg)
     int addr_reg, reg;
     valueT addr_offset;
{
  emit_unaligned_io ("ld", addr_reg, addr_offset, reg);
}

static void
emit_store_unal (addr_reg, addr_offset, reg)
     int addr_reg, reg;
     valueT addr_offset;
{
  emit_unaligned_io ("st", addr_reg, addr_offset, reg);
}

static void
emit_byte_manip_r (op, in, mask, out, mode, which)
     char *op;
     int in, mask, out, mode, which;
{
  char buf[90];
  sprintf (buf, "%s%c%c $%d,$%d,$%d", op, mode, which, in, mask, out);
  md_assemble (buf);
}

static void
emit_extract_r (in, mask, out, mode, which)
     int in, mask, out, mode, which;
{
  emit_byte_manip_r ("ext", in, mask, out, mode, which);
}

static void
emit_insert_r (in, mask, out, mode, which)
     int in, mask, out, mode, which;
{
  emit_byte_manip_r ("ins", in, mask, out, mode, which);
}

static void
emit_mask_r (in, mask, out, mode, which)
     int in, mask, out, mode, which;
{
  emit_byte_manip_r ("msk", in, mask, out, mode, which);
}

static void
emit_sign_extend (reg, size)
     int reg, size;
{
  char buf[90];
  sprintf (buf, "sll $%d,0x%x,$%d", reg, 64 - size, reg);
  md_assemble (buf);
  sprintf (buf, "sra $%d,0x%x,$%d", reg, 64 - size, reg);
  md_assemble (buf);
}

static void
emit_bis_r (in1, in2, out)
     int in1, in2, out;
{
  char buf[90];
  sprintf (buf, "bis $%d,$%d,$%d", in1, in2, out);
  md_assemble (buf);
}

static int
build_mem (opc, ra, rb, disp)
     int opc, ra, rb;
     bfd_signed_vma disp;
{
  if ((disp >> 15) != 0
      && (disp >> 15) + 1 != 0)
    abort ();
  return ((opc << 26) | (ra << SA) | (rb << SB) | (disp & 0xffff));
}

static int
build_operate_n (opc, fn, ra, lit, rc)
     int opc, fn, ra, rc;
     int lit;
{
  if (lit & ~0xff)
    abort ();
  return ((opc << 26) | (fn << 5) | (ra << SA) | (lit << SN) | (1 << 12) | (rc << SC));
}

static void
emit_sll_n (dest, disp, src)
     int dest, disp, src;
{
  struct alpha_it insn = clear_insn;
  insn.opcode = build_operate_n (0x12, 0x39, src, disp, dest);
  emit_insn (&insn);
}

static void
emit_ldah_num (dest, addend, src)
     int dest, src;
     bfd_vma addend;
{
  struct alpha_it insn = clear_insn;
  insn.opcode = build_mem (0x09, dest, src, addend);
  emit_insn (&insn);
}

static void
emit_addq_r (in1, in2, out)
     int in1, in2, out;
{
  struct alpha_it insn = clear_insn;
  insn.opcode = 0x40000400 | (in1 << SA) | (in2 << SB) | (out << SC);
  emit_insn (&insn);
}

static void
emit_lda_n (dest, addend, src)
     int dest, src;
     bfd_vma addend;
{
  struct alpha_it insn = clear_insn;
  insn.opcode = build_mem (0x08, dest, src, addend);
  emit_insn (&insn);
}

static void
emit_add64 (in, out, num)
     int in, out;
     bfd_vma num;
{
  bfd_signed_vma snum = num;

  if (in_range_signed (num, 16))
    {
      emit_lda_n (out, num, in);
      return;
    }
  if ((num & 0xffff) == 0
      && in == ZERO
      && in_range_signed (snum >> 16, 16))
    {
      emit_ldah_num (out, snum >> 16, in);
      return;
    }
  /* I'm not sure this one is getting invoked when it could.  */
  if ((num & 1) == 0 && in == ZERO)
    {
      if (in_range_signed (snum >> 1, 16))
	{
	  emit_lda_n (out, snum >> 1, in);
	  emit_addq_r (out, out, out);
	  return;
	}
      else if (num & 0x1fffe == 0
	       && in_range_signed (snum >> 17, 16))
	{
	  emit_ldah_num (out, snum >> 17, in);
	  emit_addq_r (out, out, out);
	  return;
	}
    }
  if (in_range_signed (num, 32))
    {
      bfd_vma lo = num & 0xffff;
      if (lo & 0x8000)
	lo -= 0x10000;
      num -= lo;
      emit_ldah_num (out, snum >> 16, in);
      if (lo)
	emit_lda_n (out, lo, out);
      return;
    }

  if (in != ZERO && in != AT && out != AT && at_ok)
    {
      emit_add64 (ZERO, AT, num);
      emit_addq_r (AT, in, out);
      return;
    }

  if (in != ZERO)
    as_bad ("load expression too complex to expand");

  /* Could check also for loading 16- or 32-bit value and shifting by
     arbitrary displacement.  */

  {
    bfd_vma lo = snum & 0xffffffff;
    if (lo & 0x80000000)
      lo -= ((bfd_vma)0x10000000 << 4);
    snum -= lo;
    emit_add64 (ZERO, out, snum >> 32);
    emit_sll_n (out, 32, out);
    if (lo != 0)
      emit_add64 (out, out, lo);
  }
}

static int
alpha_ip (str, insns)
     char *str;
     struct alpha_it insns[];
{
  char *s;
  const char *args;
  char c;
  unsigned long i;
  struct alpha_opcode *pattern;
  char *argsStart;
  unsigned int opcode;
  unsigned int mask = 0;
  int match = 0, num_gen = 1;
  int comma = 0;
  int do_add64, add64_in = 0, add64_out = 0;
  bfd_vma add64_addend = 0;

  for (s = str;
       islower (*s) || *s == '_' || *s == '/' || *s == '4' || *s == '8';
       ++s)
    ;
  switch (*s)
    {

    case '\0':
      break;

    case ',':
      comma = 1;

      /*FALLTHROUGH*/

    case ' ':
      *s++ = '\0';
      break;

    default:
      as_fatal ("Unknown opcode: `%s'", str);
    }
  if ((pattern = (struct alpha_opcode *) hash_find (op_hash, str)) == NULL)
    {
      as_bad ("Unknown opcode: `%s'", str);
      return -1;
    }
  if (comma)
    *--s = ',';

  argsStart = s;
  for (;;)
    {
      do_add64 = 0;
      opcode = pattern->match;
      num_gen = 1;
      for (i = 0; i < MAX_INSNS; i++)
	insns[i] = clear_insn;

      /* Build the opcode, checking as we go to make sure that the
	 operands match.  */
      for (args = pattern->args;; ++args)
	{
	  switch (*args)
	    {

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

	    case '(':		/* these must match exactly */
	    case ')':
	    case ',':
	    case ' ':
	    case '0':
	      if (*s++ == *args)
		continue;
	      break;

	    case '1':		/* next operand must be a register */
	    case '2':
	    case '3':
	    case 'r':
	    case 'R':
	      if (*s++ == '$')
		{
		  switch (c = *s++)
		    {

		    case 'a':	/* $at: as temporary */
		      if (*s++ != 't')
			goto error;
		      mask = AT;
		      break;

		    case 'g':	/* $gp: base register */
		      if (*s++ != 'p')
			goto error;
		      mask = base_register;
		      break;

		    case 's':	/* $sp: stack pointer */
		      if (*s++ != 'p')
			goto error;
		      mask = SP;
		      break;


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
		      if ((c == GP) && first_32bit_quadrant)
			c = ZERO;

		      mask = c;
		      break;

		    default:
		      goto error;
		    }
		  note_gpreg (mask);
		  /* Got the register, now figure out where it goes in
		     the opcode.  */
		doregister:
		  switch (*args)
		    {

		    case '1':
		    case 'e':
		      opcode |= mask << SA;
		      continue;

		    case '2':
		    case 'f':
		      opcode |= mask << SB;
		      continue;

		    case '3':
		    case 'g':
		      opcode |= mask;
		      continue;

		    case 'r':
		      opcode |= (mask << SA) | mask;
		      continue;

		    case 'R':	/* ra and rb are the same */
		      opcode |= (mask << SA) | (mask << SB);
		      continue;

		    case 'E':
		      opcode |= (mask << SA) | (mask << SB) | (mask);
		      continue;
		    }
		}
	      break;

	    case 'e':		/* next operand is a floating point register */
	    case 'f':
	    case 'g':
	    case 'E':
	      if (*s++ == '$' && *s++ == 'f' && isdigit (*s))
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
		  note_fpreg (mask);
		  /* same encoding as gp registers */
		  goto doregister;
		}
	      break;

#if 0
	    case 'h':		/* bits 16..31 */
	      insns[0].reloc = RELOC_16_31;
	      goto immediate;
#endif

	    case 'l':		/* bits 0..15 */
	      insns[0].reloc[0].code = BFD_RELOC_16;
	      goto immediate;

	    case 'L':		/* 21 bit PC relative immediate */
	      insns[0].reloc[0].code = BFD_RELOC_23_PCREL_S2;
	      insns[0].reloc[0].pcrel = 1;
	      goto immediate;

	    case 'i':		/* 14 bit immediate */
	      if (OPCODE (opcode) != 0x1a)
		/* Not a jmp variant?? */
		abort ();
	      else if (opcode & 0x8000)
		/* ret or jsr_coroutine */
		{
		  insns[0].reloc[0].code = BFD_RELOC_14;
		  insns[0].reloc[0].pcrel = 0;
		}
	      else
		/* jmp or jsr */
		{
		  insns[0].reloc[0].code = BFD_RELOC_ALPHA_HINT;
		  insns[0].reloc[0].pcrel = 1;
		}
	      goto immediate;

	    case 'b':		/* 8 bit immediate */
	      insns[0].reloc[0].code = BFD_RELOC_8;
	      goto immediate;

	    case 'I':		/* 26 bit immediate, for PALcode */
	      insns[0].reloc[0].code = BFD_RELOC_26;
	      goto immediate;

	    case 't':		/* 12 bit displacement, for PALcode */
	      insns[0].reloc[0].code = BFD_RELOC_12_PCREL;
	      goto immediate;

	    case '8':		/* 8 bit 0...7 */
	      insns[0].reloc[0].code = BFD_RELOC_8;
	      goto immediate;

	    immediate:
	      if (*s == ' ')
		s++;
	      getExpression (s, &insns[0]);
	      s = expr_end;
	      /* Handle overflow in certain instructions by converting
		 to other instructions.  */
	      if (insns[0].reloc[0].code == BFD_RELOC_8
		  && insns[0].reloc[0].exp.X_op == O_constant
		  && (insns[0].reloc[0].exp.X_add_number < 0
		      || insns[0].reloc[0].exp.X_add_number > 0xff))
		{
		  if (OPCODE (opcode) == 0x10
		      && (OP_FCN (opcode) == 0x00	/* addl */
			  || OP_FCN (opcode) == 0x40	/* addl/v */
			  || OP_FCN (opcode) == 0x20	/* addq */
			  || OP_FCN (opcode) == 0x60	/* addq/v */
			  || OP_FCN (opcode) == 0x09	/* subl */
			  || OP_FCN (opcode) == 0x49	/* subl/v */
			  || OP_FCN (opcode) == 0x29	/* subq */
			  || OP_FCN (opcode) == 0x69	/* subq/v */
			  || OP_FCN (opcode) == 0x02	/* s4addl */
			  || OP_FCN (opcode) == 0x22	/* s4addq */
			  || OP_FCN (opcode) == 0x0b	/* s4subl */
			  || OP_FCN (opcode) == 0x2b	/* s4subq */
			  || OP_FCN (opcode) == 0x12	/* s8addl */
			  || OP_FCN (opcode) == 0x32	/* s8addq */
			  || OP_FCN (opcode) == 0x1b	/* s8subl */
			  || OP_FCN (opcode) == 0x3b	/* s8subq */
		      )
		      /* Can we make it fit by negating?  */
		      && -insns[0].reloc[0].exp.X_add_number < 0xff
		      && -insns[0].reloc[0].exp.X_add_number > 0)
		    {
		      opcode ^= 0x120;	/* convert add<=>sub */
		      insns[0].reloc[0].exp.X_add_number *= -1;
		    }
		  else if (at_ok && macro_ok)
		    {
		      /* Constant value supplied, but it's too large.  */
		      do_add64 = 1;
		      add64_in = ZERO;
		      add64_out = AT;
		      add64_addend = insns[0].reloc[0].exp.X_add_number;
		      opcode &= ~ 0x1000;
		      opcode |= (AT << SB);
		      insns[0].reloc[0].code = BFD_RELOC_NONE;
		    }
		  else
		    as_bad ("overflow in 8-bit literal field in `operate' format insn");
		}
	      else if (insns[0].reloc[0].code == BFD_RELOC_16
		       && insns[0].reloc[0].exp.X_op == O_constant
		       && !in_range_signed (insns[0].reloc[0].exp.X_add_number,
					    16))
		{
		  bfd_vma val = insns[0].reloc[0].exp.X_add_number;
		  if (OPCODE (opcode) == 0x08)
		    {
		      do_add64 = 1;
		      add64_in = ZERO;
		      add64_out = AT;
		      add64_addend = val;
		      opcode &= ~0x1000;
		      opcode |= (AT << SB);
		      insns[0].reloc[0].code = BFD_RELOC_NONE;
		    }
		  else if (OPCODE (opcode) == 0x09
			   && in_range_signed (val >> 16, 16))
		    {
		      /* ldah with high operand - convert to low */
		      insns[0].reloc[0].exp.X_add_number >>= 16;
		    }
		  else
		    as_bad ("I don't know how to handle 32+ bit constants here yet, sorry.");
		}
	      else if (insns[0].reloc[0].code == BFD_RELOC_32
		       && insns[0].reloc[0].exp.X_op == O_constant)
		{
		  bfd_vma val = insns[0].reloc[0].exp.X_add_number;
		  bfd_signed_vma sval = val;
		  if (val >> 32 != 0
		      && sval >> 32 != 0
		      && sval >> 32 != -1)
		    as_bad ("I don't know how to handle 64 bit constants here yet, sorry.");
		}
	      continue;

	    case 'F':
	      {
		int format, length, mode, i;
		char temp[20 /*MAXIMUM_NUMBER_OF_CHARS_FOR_FLOAT*/];
		char *err;
		static const char formats[4] = "FGfd";
		bfd_vma bits, offset;
		char *old_input_line_pointer = input_line_pointer;

		input_line_pointer = s;
		SKIP_WHITESPACE ();
		memset (temp, 0, sizeof (temp));
		mode = (opcode >> 26) & 3;
		format = formats[mode];
		err = md_atof (format, temp, &length);
		if (err)
		  {
		    as_bad ("Bad floating literal: %s", err);
		    bits = 0;
		  }
		else
		  {
		    /* Generate little-endian number from byte sequence.  */
		    bits = 0;
		    for (i = length - 1; i >= 0; i--)
		      bits += ((bfd_vma)(temp[i] & 0xff)) << (i * 8);
		  }
		switch (length)
		  {
		  case 8:
		    offset = get_lit8_offset (bits) - 0x8000;
		    insns[0].reloc[0].exp.X_add_symbol = lit8_sym;
		    insns[0].reloc[0].exp.X_add_number = 0x8000;
		    break;
		  case 4:
		    offset = get_lit4_offset (bits) - 0x8000;
		    insns[0].reloc[0].exp.X_add_symbol = lit4_sym;
		    insns[0].reloc[0].exp.X_add_number = 0x8000;
		    break;
		  default:
		    abort ();
		  }
		insns[0].reloc[0].exp.X_op = O_symbol;
		offset &= 0xffff;
		num_gen = load_expression (AT, &insns[0]);
		if (lituse_pending)
		  {
		    insns[num_gen].reloc[0].code = BFD_RELOC_ALPHA_LITUSE;
		    insns[num_gen].reloc[0].exp = lituse_basereg;
		    lituse_pending = 0;
		  }
		insns[num_gen++].opcode = opcode | (AT << SB) | offset;
		opcode = insns[0].opcode;
		s = input_line_pointer;
		input_line_pointer = old_input_line_pointer;
	      }
	      continue;

	      /* The following two.. take advantage of the fact that
	         opcode already contains most of what we need to know.
	         We just prepend to the instr an "ldah
	         $r,%ml(expr)($base)" and turn this one (done later
	         after we return) into something like "stq
	         $r,%lo(expr)(at)" or "ldq $r,%lo(expr)($r)".

		 NOTE: This can fail later on at link time if the
		 offset from $base actually turns out to be more than
		 2**31 or 2**47 if use_large_offsets is set.  */
	    case 'P':		/* Addressing macros: PUT */
	      mask = AT;	/* register 'at' */
	      /* fall through */

	    case 'G':		/* Addressing macros: GET */
	      /* All it is missing is the expression, which is what we
		 will get now */

	      if (*s == ' ')
		s++;
	      getExpression (s, &insns[0]);
	      s = expr_end;

	      /* Must check for "lda ..,number" too */
	      if (insns[0].reloc[0].exp.X_op == O_big)
		{
		  as_warn ("Sorry, not yet. Put bignums in .data section yourself.");
		  return -1;
		}
	      if (insns[0].reloc[0].exp.X_op == O_constant)
		{
		  bfd_vma val = insns[0].reloc[0].exp.X_add_number;
		  bfd_vma top, low;

		  insns[0].reloc[0].code = BFD_RELOC_NONE;
		  insns[1].reloc[0].code = BFD_RELOC_NONE;

		  low = val & 0xffff;
		  if (low & 0x8000)
		    low -= 0x10000;
		  top = val - low;
		  if (top)
		    {
		      do_add64 = 1;
		      add64_in = ZERO;
		      add64_out = AT;
		      add64_addend = top;
		      opcode |= AT << SB;
		    }
		  else
		    opcode |= ZERO << SB;
		  opcode &= ~0x1000;
		  opcode |= low & 0xffff;
		}
	      else if (insns[0].reloc[0].exp.X_op == O_symbol)
		{
		  unsigned long old_opcode = opcode;
		  int tmp_reg = -1;

		  if (!macro_ok)
		    as_bad ("insn requires expansion but `nomacro' specified");
		  else if (*args == 'G')
		    tmp_reg = mask;
		  else if (!at_ok)
		    as_bad ("insn expansion requires AT use, but `noat' specified");
		  else
		    tmp_reg = AT;
		  num_gen = load_expression (tmp_reg, insns);
		  opcode = insns[0].opcode;
		  /* lda is opcode 8, 0x20000000, and the macros that use
		     this code have an opcode field of 0.  The latter
		     require further processing, and we don't have the
		     true opcode here.  */
		  if (OPCODE (old_opcode) != 0
		      && OPCODE (old_opcode) != 0x08)
		    {
		      struct alpha_it *i;
		      i = &insns[num_gen++];
		      i->opcode = old_opcode | (tmp_reg << SB);

		      if (lituse_pending)
			{
			  i->reloc[0].code = BFD_RELOC_ALPHA_LITUSE;
			  i->reloc[0].exp = lituse_basereg;
			  lituse_pending = 0;
			}
		    }
		}
	      else
		{
		  /* Not a number */
		  num_gen = 2;
		  insns[1].reloc[0].exp = insns[0].reloc[0].exp;

		  /* Generate: ldah REG,x1(GP); OP ?,x0(REG) */

		  abort ();	/* relocs need fixing */
#if 0
		  insns[1].reloc = RELOC_0_15;
		  insns[1].opcode = opcode | mask << SB;

		  insns[0].reloc = RELOC_16_31;
		  opcode = 0x24000000 /*ldah*/  | mask << SA | (base_register << SB);
#endif
		}

	      continue;

	      /* Same failure modes as above, actually most of the
		 same code shared.  */
	    case 'B':		/* Builtins */
	      args++;
	      switch (*args)
		{

		case 'a':	/* ldgp */

		  if (first_32bit_quadrant || no_mixed_code)
		    return -1;
		  switch (OUTPUT_FLAVOR)
		    {
		    case bfd_target_aout_flavour:
		      /* this is cmu's a.out version */
		      insns[0].reloc[0].code = BFD_RELOC_NONE;
		      /* generate "zap %r,0xf,%r" to take high 32 bits */
		      opcode |= 0x48001600 /* zap ?,#,?*/  | (0xf << SN);
		      break;
		    case bfd_target_ecoff_flavour:
		      /* Given "ldgp R1,N(R2)", turn it into something
			 like "ldah R1,###(R2) ; lda R1,###(R1)" with
			 appropriate constants and relocations.  */
		      {
			unsigned long r1, r2;
			unsigned long addend = 0;

			num_gen = 2;
			r2 = mask;
			r1 = opcode & 0x3f;
			insns[0].reloc[0].code = BFD_RELOC_ALPHA_GPDISP_HI16;
			insns[0].reloc[0].pcrel = 1;
			insns[0].reloc[0].exp.X_op = O_symbol;
			insns[0].reloc[0].exp.X_add_symbol = gp;
			insns[0].reloc[0].exp.X_add_number = 0;
			insns[0].opcode = (0x24000000	/* ldah */
					   | (r1 << SA)
					   | (r2 << SB));
			insns[1].reloc[0].code = BFD_RELOC_ALPHA_GPDISP_LO16;
			insns[1].reloc[0].exp.X_op = O_symbol;
			insns[1].reloc[0].exp.X_add_symbol = gp;
			insns[1].reloc[0].exp.X_add_number = 4;
			insns[1].reloc[0].pcrel = 1;
			insns[1].opcode = 0x20000000 | (r1 << SA) | (r1 << SB);
			opcode = insns[0].opcode;
			/* merge in addend */
			insns[1].opcode |= addend & 0xffff;
			insns[0].opcode |= ((addend >> 16)
					    + (addend & 0x8000 ? 1 : 0));
			if (r2 == PV)
			  ecoff_set_gp_prolog_size (0);
		      }
		      break;
		    default:
		      abort ();
		    }
		  continue;


		case 'b':	/* setgp */
		  switch (OUTPUT_FLAVOR)
		    {
		    case bfd_target_aout_flavour:
		      /* generate "zap %r,0xf,$gp" to take high 32 bits */
		      opcode |= 0x48001600	/* zap ?,#,?*/
			| (0xf << SN) | (base_register);
		      break;
		    default:
		      abort ();
		    }
		  continue;

		case 'c':	/* jsr $r,foo  becomes
					lda $27,foo
					jsr $r,($27),foo
				   Register 27, t12, is used by convention
				   here.  */
		  {
		    struct alpha_it *jsr;
		    expressionS etmp;
		    struct reloc_data *r;

		    /* We still have to parse the function name */
		    if (*s == ' ')
		      s++;
		    getExpression (s, &insns[0]);
		    etmp = insns[0].reloc[0].exp;
		    s = expr_end;
		    num_gen = load_expression (PV, &insns[0]);
		    note_gpreg (PV);

		    jsr = &insns[num_gen++];
		    jsr->opcode = (pattern->match
				   | (mask << SA)
				   | (PV << SB)
				   | 0);
		    if (lituse_pending)
		      {
			/* LITUSE wasn't emitted yet */
			jsr->reloc[0].code = BFD_RELOC_ALPHA_LITUSE;
			jsr->reloc[0].exp = lituse_jsr;
			r = &jsr->reloc[1];
			lituse_pending = 0;
		      }
		    else
		      r = &jsr->reloc[0];
		    r->exp = etmp;
		    r->code = BFD_RELOC_ALPHA_HINT;
		    r->pcrel = 1;
		    opcode = insns[0].opcode;
		  }
		  continue;

		case 'd':
		  /* Sub-word loads and stores.  We load the address into
		     $at, which might involve using the `P' parameter
		     processing too, then emit a sequence to get the job
		     done, using unaligned memory accesses and byte
		     manipulation, with t9 and t10 as temporaries.  */
		  {
		    /* Characteristics of access.  */
		    int is_load = 99, is_unsigned = 0, is_unaligned = 0;
		    int mode_size, mode;
		    /* Register operand.  */
		    int reg = -1;
		    /* Addend for loads and stores.  */
		    valueT addend;
		    /* Which register do we use for the address?  */
		    int addr;

		    {
		      /* Pick apart name and set flags.  */
		      const char *s = pattern->name;

		      if (*s == 'u')
			{
			  is_unaligned = 1;
			  s++;
			}

		      if (s[0] == 'l' && s[1] == 'd')
			is_load = 1;
		      else if (s[0] == 's' && s[1] == 't')
			is_load = 0;
		      else
			as_fatal ("unrecognized sub-word access insn `%s'",
				  str);
		      s += 2;

		      mode = *s++;
		      if (mode == 'b') mode_size = 1;
		      else if (mode == 'w') mode_size = 2;
		      else if (mode == 'l') mode_size = 4;
		      else if (mode == 'q') mode_size = 8;
		      else abort ();

		      if (*s == 'u')
			{
			  is_unsigned = 1;
			  s++;
			}

		      assert (*s == 0);

		      /* Longwords are always kept sign-extended.  */
		      if (mode == 'l' && is_unsigned)
			abort ();
		      /* There's no special unaligned byte handling.  */
		      if (mode == 'b' && is_unaligned)
			abort ();
		      /* Stores don't care about signedness.  */
		      if (!is_load && is_unsigned)
			abort ();
		    }

		    if (args[-2] == 'P')
		      {
			addr = AT;
			addend = 0;
		      }
		    else
		      {
			/* foo r1,num(r2)
			   r2 -> mask
			   r1 -> (opcode >> SA) & 31
			   num -> insns->reloc[0].*

			   We want to emit "lda at,num(r2)", since these
			   operations require the use of a single register
			   with the starting address of the memory operand
			   we want to access.

			   We could probably get away without doing this
			   (and use r2 below, with the addend for the
			   actual reads and writes) in cases where the
			   addend is known to be a multiple of 8.  */
			int r2 = mask;
			int r1 = (opcode >> SA) & 31;

			if (insns[0].reloc[0].code == BFD_RELOC_NONE)
			  addend = 0;
			else if (insns[0].reloc[0].code == BFD_RELOC_16)
			  {
			    if (insns[0].reloc[0].exp.X_op != O_constant)
			      abort ();
			    addend = insns[0].reloc[0].exp.X_add_number;
			  }
			else
			  abort ();

			if (addend + mode_size - 1 < 0x7fff
			    && (addend % 8) == 0
			    && (r2 < T9 || r2 > T12))
			  {
			    addr = r2;
			    num_gen = 0;
			  }
			else
			  {
			    /* Let later relocation processing deal
			       with the addend field.  */
			    insns[num_gen-1].opcode = (0x20000000 /* lda */
						       | (AT << SA)
						       | (r2 << SB));
			    addr = AT;
			    addend = 0;
			  }
			reg = r1;
		      }

		    /* Because the emit_* routines append directly to
		       the current frag, we now need to flush any
		       pending insns.  */
		    {
		      int i;
		      for (i = 0; i < num_gen; i++)
			emit_insn (&insns[i]);
		      num_gen = 0;
		    }

		    if (is_load)
		      {
			int reg2, reg3 = -1;

			if (is_unaligned)
			  reg2 = T9, reg3 = T10;
			else
			  reg2 = reg;

			emit_load_unal (addr, addend, T9);
			if (is_unaligned)
			  emit_load_unal (addr, addend + mode_size - 1, T10);
			emit_extract_r (T9, addr, reg2, mode, 'l');
			if (is_unaligned)
			  {
			    emit_extract_r (T10, addr, reg3, mode, 'h');
			    emit_bis_r (T9, T10, reg);
			  }
			if (!is_unsigned)
			  emit_sign_extend (reg, mode_size * 8);
		      }
		    else
		      {
			/* The second word gets processed first
			   because if the address does turn out to be
			   aligned, the processing for the second word
			   will be pushing around all-zeros, and the
			   entire value will be handled as the `first'
			   word.  So we want to store the `first' word
			   last.  */
			/* Pair these up so that the memory loads get
			   separated from each other, as well as being
			   well in advance of the uses of the values
			   loaded.  */
			if (is_unaligned)
			  {
			    emit_load_unal (addr, addend + mode_size - 1, T11);
			    emit_insert_r (reg, addr, T12, mode, 'h');
			  }
			emit_load_unal (addr, addend, T9);
			emit_insert_r (reg, addr, T10, mode, 'l');
			if (is_unaligned)
			  emit_mask_r (T12, addr, T12, mode, 'h');
			emit_mask_r (T10, addr, T10, mode, 'l');
			if (is_unaligned)
			  emit_bis_r (T11, T12, T11);
			emit_bis_r (T9, T10, T9);
			if (is_unaligned)
			  emit_store_unal (addr, addend + mode_size - 1, T11);
			emit_store_unal (addr, addend, T9);
		      }
		  }
		  return 0;

		  /* DIVISION and MODULUS. Yech.
		       Convert	OP x,y,result
		       to	mov x,t10
				mov y,t11
				jsr t9, __OP
				mov t12,result

		       with appropriate optimizations if t10,t11,t12
		       are the registers specified by the compiler.
		       We are missing an obvious optimization
		       opportunity here; if the ldq generated by the
		       jsr assembly requires a cycle or two to make
		       the value available, initiating it before one
		       or two of the mov instructions would result in
		       faster execution.  */
		case '0':	/* reml */
		case '1':	/* divl */
		case '2':	/* remq */
		case '3':	/* divq */
		case '4':	/* remlu */
		case '5':	/* divlu */
		case '6':	/* remqu */
		case '7':	/* divqu */
		  {
		    static char func[8][6] = {
		      "reml", "divl", "remq", "divq",
		      "remlu", "divlu", "remqu", "divqu"
		    };
		    char expansion[64];
		    int reg;

		    /* All regs parsed, in opcode */

		    /* Do the expansions, one instr at a time */

		    reg = (opcode >> SA) & 31;
		    if (reg != T10)
		      {
			/* x->t10 */
			sprintf (expansion, "mov $%d,$%d", reg, T10);
			md_assemble (expansion);
		      }
		    reg = (opcode >> SB) & 31;
		    if (reg == T10)
		      /* we already overwrote it! */
		      abort ();
		    else if (reg != T11)
		      {
			/* y->t11 */
			sprintf (expansion, "mov $%d,$%d", reg, T11);
			md_assemble (expansion);
		      }
		    sprintf (expansion, "lda $%d,__%s", PV, func[*args - '0']);
		    md_assemble (expansion);
		    sprintf (expansion, "jsr $%d,($%d),__%s", T9, PV,
			     func[*args - '0']);
		    md_assemble (expansion);
#if 0 /* huh? */
		    if (!first_32bit_quadrant)
		      {
			sprintf (expansion,
				 "zap $%d,0xf,$%d",
				 T9, base_register);
			md_assemble (expansion);
		      }
#endif
		    sprintf (expansion, "ldgp $%d,0($%d)",
			     base_register, T9);
		    md_assemble (expansion);

		    /* Use insns[0] to get at the result */
		    if ((reg = (opcode & 31)) != PV)
		      opcode = (0x47e00400	/* or zero,zero,zero */
				| (PV << SB)
				| reg /* Rc */ );	/* pv->z */
		    else
		      num_gen = 0;
		  }
		  continue;
		}
	      /* fall through */

	    default:
	      abort ();
	    }
	  break;
	}
    error:
      if (match == 0)
	{
	  /* Args don't match.  */
	  if (&pattern[1] - alpha_opcodes < NUMOPCODES
	      && !strcmp (pattern->name, pattern[1].name))
	    {
	      ++pattern;
	      s = argsStart;
	      continue;
	    }
	  else
	    {
	      as_warn ("Illegal operands");
	      return -1;
	    }
	}
      else
	{
	  /* Args match, see if a float instructions and -nofloats */
	  if (nofloats && pattern->isa_float)
	    return -1;
	}
      break;
    }

  if (do_add64)
    {
      emit_add64 (add64_in, add64_out, add64_addend);
    }

  insns[0].opcode = opcode;
  return num_gen;
}

/* Turn a string in input_line_pointer into a floating point constant
   of type type, and store the appropriate bytes in *litP.  The number
   of LITTLENUMS emitted is stored in *sizeP.  An error message is
   returned, or NULL on OK.  */

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
  char *atof_ieee (), *vax_md_atof ();

  switch (type)
    {
      /* VAX floats */
    case 'G':
      /* VAX md_atof doesn't like "G" for some reason.  */
      type = 'g';
    case 'F':
    case 'D':
      return vax_md_atof (type, litP, sizeP);

      /* IEEE floats */
    case 'f':
      prec = 2;
      break;

    case 'd':
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

  for (wordP = words + prec - 1; prec--;)
    {
      md_number_to_chars (litP, (long) (*wordP--), sizeof (LITTLENUM_TYPE));
      litP += sizeof (LITTLENUM_TYPE);
    }

  return 0;
}

void
md_bignum_to_chars (buf, bignum, nchars)
     char *buf;
     LITTLENUM_TYPE *bignum;
     int nchars;
{
  while (nchars)
    {
      LITTLENUM_TYPE work = *bignum++;
      int nb = CHARS_PER_LITTLENUM;

      do
	{
	  *buf++ = work & ((1 << BITS_PER_CHAR) - 1);
	  if (--nchars == 0)
	    return;
	  work >>= BITS_PER_CHAR;
	}
      while (--nb);
    }
}

CONST char *md_shortopts = "Fm:g";
struct option md_longopts[] = {
#define OPTION_32ADDR (OPTION_MD_BASE)
  {"32addr", no_argument, NULL, OPTION_32ADDR},
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
    case 'F':
      nofloats = 1;
      break;

    case OPTION_32ADDR:
      addr32 = 1;
      break;

    case 'g':
      /* Ignore `-g' so gcc can provide this option to the Digital
	 UNIX assembler, which otherwise would throw away info that
	 mips-tfile needs.  */
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
  fprintf(stream, "\
Alpha options:\n\
-32addr			treat addresses as 32-bit values\n\
-F			lack floating point instructions support\n\
-m21064 | -m21066 | -m21164\n\
			specify variant of Alpha architecture\n");
}

static void
s_proc (is_static)
     int is_static;
{
  /* XXXX Align to cache linesize XXXXX */
  char *name;
  char c;
  char *p;
  symbolS *symbolP;
  int temp;

  /* Takes ".proc name,nargs"  */
  name = input_line_pointer;
  c = get_symbol_end ();
  p = input_line_pointer;
  symbolP = symbol_find_or_make (name);
  *p = c;
  SKIP_WHITESPACE ();
  if (*input_line_pointer != ',')
    {
      *p = 0;
      as_warn ("Expected comma after name \"%s\"", name);
      *p = c;
      temp = 0;
      ignore_rest_of_line ();
    }
  else
    {
      input_line_pointer++;
      temp = get_absolute_expression ();
    }
  /*  symbolP->sy_other = (signed char) temp; */
  as_warn ("unhandled: .proc %s,%d", name, temp);
  demand_empty_rest_of_line ();
}

static void
s_alpha_set (x)
     int x;
{
  char *name = input_line_pointer, ch, *s;
  int yesno = 1;

  while (!is_end_of_line[(unsigned char) *input_line_pointer])
    input_line_pointer++;
  ch = *input_line_pointer;
  *input_line_pointer = '\0';

  s = name;
  if (s[0] == 'n' && s[1] == 'o')
    {
      yesno = 0;
      s += 2;
    }
  if (!strcmp ("reorder", s))
    /* ignore */ ;
  else if (!strcmp ("at", s))
    at_ok = yesno;
  else if (!strcmp ("macro", s))
    macro_ok = yesno;
  else if (!strcmp ("move", s))
    /* ignore */ ;
  else if (!strcmp ("volatile", s))
    /* ignore */ ;
  else
    as_warn ("Tried to .set unrecognized mode `%s'", name);
  *input_line_pointer = ch;
  demand_empty_rest_of_line ();
}

/* @@ Is this right?? */
long
md_pcrel_from (fixP)
     fixS *fixP;
{
  valueT addr = fixP->fx_where + fixP->fx_frag->fr_address;
  switch (fixP->fx_r_type)
    {
    case BFD_RELOC_ALPHA_GPDISP_HI16:
    case BFD_RELOC_ALPHA_GPDISP_LO16:
      return addr;
    default:
      return fixP->fx_size + addr;
    }
}

/* Handle the .align pseudo-op.  This aligns to a power of two.  It
   also adjusts any current instruction label.  We treat this the same
   way the MIPS port does: .align 0 turns off auto alignment.  */

static void
s_alpha_align (ignore)
     int ignore;
{
  register int temp;
  register long temp_fill;
  long max_alignment = 15;

  temp = get_absolute_expression ();
  if (temp > max_alignment)
    as_bad ("Alignment too large: %d. assumed.", temp = max_alignment);
  else if (temp < 0)
    {
      as_warn ("Alignment negative: 0 assumed.");
      temp = 0;
    }
  if (*input_line_pointer == ',')
    {
      input_line_pointer++;
      temp_fill = get_absolute_expression ();
    }
  else
    temp_fill = 0;
  if (temp)
    {
      auto_align = 1;
      alpha_align (temp, (int) temp_fill, insn_label);
    }
  else
    {
      auto_align = 0;
    }

  demand_empty_rest_of_line ();
}

static void
alpha_align (n, fill, label)
     int n;
     int fill;
     symbolS *label;
{
  if (fill == 0
      && (now_seg == text_section
	  || !strcmp (now_seg->name, ".init")
	  || !strcmp (now_seg->name, ".fini"))
      && n > 2)
    {
      static const unsigned char nop_pattern[] = { 0x1f, 0x04, 0xff, 0x47 };
      /* First, make sure we're on a four-byte boundary, in case
	 someone has been putting .byte values into the text section.
	 The DEC assembler silently fills with unaligned no-op
	 instructions.  This will zero-fill, then nop-fill with proper
	 alignment.  */
      frag_align (2, fill);
      frag_align_pattern (n, nop_pattern, sizeof (nop_pattern));
    }
  else
    frag_align (n, fill);

  if (label != NULL)
    {
      assert (S_GET_SEGMENT (label) == now_seg);
      label->sy_frag = frag_now;
      S_SET_VALUE (label, (valueT) frag_now_fix ());
    }
}

/* This function is called just before the generic pseudo-ops output
   something.  It just clears insn_label.  */

void
alpha_flush_pending_output ()
{
  insn_label = NULL;
}

/* Handle data allocation pseudo-ops.  This is like the generic
   version, but it makes sure the current label, if any, is correctly
   aligned.  */

static void
s_alpha_cons (log_size)
     int log_size;
{
  if (log_size > 0 && auto_align)
    alpha_align (log_size, 0, insn_label);
  insn_label = NULL;
  cons (1 << log_size);
}

/* Handle floating point allocation pseudo-ops.  This is like the
   generic vresion, but it makes sure the current label, if any, is
   correctly aligned.  */

static void
s_alpha_float_cons (type)
     int type;
{
  if (auto_align)
    {
      int log_size;

      switch (type)
	{
	default:
	case 'f':
	case 'F':
	  log_size = 2;
	  break;

	case 'd':
	case 'D':
	case 'G':
	  log_size = 3;
	  break;

	case 'x':
	case 'X':
	case 'p':
	case 'P':
	  log_size = 4;
	  break;
	}

      alpha_align (log_size, 0, insn_label);
    }

  insn_label = NULL;
  float_cons (type);
}

/* This function is called whenever a label is defined.  It is used to
   adjust the label when an automatic alignment occurs.  */

void
alpha_define_label (sym)
     symbolS *sym;
{
  insn_label = sym;
}

int
md_apply_fix (fixP, valueP)
     fixS *fixP;
     valueT *valueP;
{
  valueT value;
  int size;
  valueT addend;
  char *p = fixP->fx_frag->fr_literal + fixP->fx_where;

  value = *valueP;

  switch (fixP->fx_r_type)
    {
      /* The GPDISP relocations are processed internally with a symbol
	 referring to the current function; we need to drop in a value
	 which, when added to the address of the start of the function,
	 gives the desired GP.  */
    case BFD_RELOC_ALPHA_GPDISP_HI16:
    case BFD_RELOC_ALPHA_GPDISP_LO16:
      addend = value;
      if (fixP->fx_r_type == BFD_RELOC_ALPHA_GPDISP_HI16)
	{
	  assert (fixP->fx_next->fx_r_type == BFD_RELOC_ALPHA_GPDISP_LO16);
#ifdef DEBUG1
	  printf ("hi16: ");
	  fprintf_vma (stdout, addend);
	  printf ("\n");
#endif
	  if (addend & 0x8000)
	    addend += 0x10000;
	  addend >>= 16;
	  fixP->fx_offset = 4;	/* @@ Compute this using fx_next.  */
	}
      else
	{
#ifdef DEBUG1
	  printf ("lo16: ");
	  fprintf_vma (stdout, addend);
	  printf ("\n");
#endif
	  addend &= 0xffff;
	  fixP->fx_offset = 0;
	}
      md_number_to_chars (fixP->fx_frag->fr_literal + fixP->fx_where,
			  addend, 2);
      fixP->fx_addsy = section_symbol (absolute_section);
      fixP->fx_offset += fixP->fx_frag->fr_address + fixP->fx_where;
      break;

    case BFD_RELOC_8:
      /* Write 8 bits, shifted left 13 bit positions.  */
      value &= 0xff;
      p++;
      *p &= 0x1f;
      *p |= (value << 5) & 0xe0;
      value >>= 3;
      p++;
      *p &= 0xe0;
      *p |= value;
      value >>= 5;
      fixP->fx_done = 1;
      if (value != 0)
	as_bad_where (fixP->fx_file, fixP->fx_line,
		      "overflow in type-%d reloc", (int) fixP->fx_r_type);
      return 3;

    case BFD_RELOC_32:
      size = 4;
      goto do_it;
    case BFD_RELOC_64:
      size = 8;
      goto do_it;
    case BFD_RELOC_16:
      /* Don't want overflow checking.  */
      size = 2;
    do_it:
      if (fixP->fx_pcrel == 0
	  && fixP->fx_addsy == 0)
	{
	  md_number_to_chars (p, value, size);
	  /* @@ Overflow checks??  */
	  goto done;
	}
      break;

    case BFD_RELOC_26:
      if (fixP->fx_addsy != 0
	  && fixP->fx_addsy->bsym->section != absolute_section)
	as_bad_where (fixP->fx_file, fixP->fx_line,
		      "PALcode instructions require immediate constant function code");
      else if (value >> 26 != 0)
	as_bad_where (fixP->fx_file, fixP->fx_line,
		      "overflow in 26-bit PALcode function field");
      *p++ = value & 0xff;
      value >>= 8;
      *p++ = value & 0xff;
      value >>= 8;
      *p++ = value & 0xff;
      value >>= 8;
      {
	char x = *p;
	x &= ~3;
	x |= (value & 3);
	*p++ = x;
      }
      goto done;

    case BFD_RELOC_14:
      if (fixP->fx_addsy != 0
	  && fixP->fx_addsy->bsym->section != absolute_section)
	as_bad_where (fixP->fx_file, fixP->fx_line,
		  "ret/jsr_coroutine requires constant in displacement field");
      else if (value >> 14 != 0)
	as_bad_where (fixP->fx_file, fixP->fx_line,
		  "overflow in 14-bit operand field of ret or jsr_coroutine");
      *p++ = value & 0xff;
      value >>= 8;
      *p = (*p & 0xc0) | (value & 0x3f);
      goto done;

    case BFD_RELOC_23_PCREL_S2:
      /* Write 21 bits only.  */
      value >>= 2;
      *p++ = value & 0xff;
      value >>= 8;
      *p++ = value & 0xff;
      value >>= 8;
      *p &= 0xe0;
      *p |= (value & 0x1f);
      goto done;

    case BFD_RELOC_12_PCREL:
      *p++ = value & 0xff;
      value >>= 8;
      *p &= 0xf0;
      *p |= (value & 0x0f);
      goto done;

    case BFD_RELOC_ALPHA_LITERAL:
    case BFD_RELOC_ALPHA_LITUSE:
      return 2;

    case BFD_RELOC_GPREL32:
      assert (fixP->fx_subsy == gp);
      value = - alpha_gp_value;	/* huh?  this works... */
      fixP->fx_subsy = 0;
      md_number_to_chars (p, value, 4);
      break;

    case BFD_RELOC_ALPHA_HINT:
      if (fixP->fx_addsy == 0 && fixP->fx_pcrel == 0)
	{
	  size = 2;
	  goto do_it;
	}
      return 2;

    default:
      as_fatal ("unhandled relocation type %s",
		bfd_get_reloc_code_name (fixP->fx_r_type));
      return 9;
    }

  if (fixP->fx_addsy == 0 && fixP->fx_pcrel == 0)
    {
      printf ("type %d reloc done?\n", fixP->fx_r_type);
    done:
      fixP->fx_done = 1;
      return 42;
    }

  return 0x12345678;
}

void
alpha_frob_ecoff_data ()
{
  select_gp_value ();
  /* $zero and $f31 are read-only */
  alpha_gprmask &= ~1;
  alpha_fprmask &= ~1;
}

/* The Alpha has support for some VAX floating point types, as well as for
   IEEE floating point.  We consider IEEE to be the primary floating point
   format, and sneak in the VAX floating point support here.  */
#define md_atof vax_md_atof
#include "config/atof-vax.c"
