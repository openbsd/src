/* tc-m32r.c -- Assembler for the Mitsubishi M32R.
   Copyright (C) 1996, 1997 Free Software Foundation.

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
#include "cgen-opc.h"

/* Non-null if last insn was a 16 bit insn on a 32 bit boundary
   (i.e. was the first of two 16 bit insns).  */
static const struct cgen_insn *prev_insn = NULL;

/* Non-zero if we've seen a relaxable insn since the last 32 bit
   alignment request.  */
static int seen_relaxable_p = 0;

/* Non-zero if -relax specified, in which case sufficient relocs are output
   for the linker to do relaxing.
   We do simple forms of relaxing internally, but they are always done.
   This flag does not apply to them.  */
static int m32r_relax;

/* If non-NULL, pointer to cpu description file to read.
   This allows runtime additions to the assembler.  */
static char *m32r_cpu_desc;

/* stuff for .scomm symbols.  */
static segT sbss_section;
static asection scom_section;
static asymbol scom_symbol;

const char comment_chars[] = ";";
const char line_comment_chars[] = "#";
const char line_separator_chars[] = "";
const char EXP_CHARS[] = "eE";
const char FLT_CHARS[] = "dD";

/* Relocations against symbols are done in two
   parts, with a HI relocation and a LO relocation.  Each relocation
   has only 16 bits of space to store an addend.  This means that in
   order for the linker to handle carries correctly, it must be able
   to locate both the HI and the LO relocation.  This means that the
   relocations must appear in order in the relocation table.

   In order to implement this, we keep track of each unmatched HI
   relocation.  We then sort them so that they immediately precede the
   corresponding LO relocation. */

struct m32r_hi_fixup
{
  /* Next HI fixup.  */
  struct m32r_hi_fixup *next;
  /* This fixup.  */
  fixS *fixp;
  /* The section this fixup is in.  */
  segT seg;
};

/* The list of unmatched HI relocs.  */

static struct m32r_hi_fixup *m32r_hi_fixup_list;

static void m32r_record_hi16 PARAMS ((int, fixS *, segT seg));

const char *md_shortopts = "";

struct option md_longopts[] = {
#if 0 /* not supported yet */
#define OPTION_RELAX  (OPTION_MD_BASE)
  {"relax", no_argument, NULL, OPTION_RELAX},
#define OPTION_CPU_DESC (OPTION_MD_BASE + 1)
  {"cpu-desc", required_argument, NULL, OPTION_CPU_DESC},
#endif
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
#if 0 /* not supported yet */
    case OPTION_RELAX:
      m32r_relax = 1;
      break;
    case OPTION_CPU_DESC:
      m32r_cpu_desc = arg;
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
  fprintf (stream, "M32R options:\n");
#if 0
  fprintf (stream, "\
--relax			create linker relaxable code\n");
  fprintf (stream, "\
--cpu-desc		provide runtime cpu description file\n");
#else
  fprintf (stream, "[none]\n");
#endif
} 

static void fill_insn PARAMS ((int));
static void m32r_scomm PARAMS ((int));

/* Set by md_assemble for use by m32r_fill_insn.  */
static subsegT prev_subseg;
static segT prev_seg;

/* The target specific pseudo-ops which we support.  */
const pseudo_typeS md_pseudo_table[] =
{
  { "word", cons, 4 },
  { "fillinsn", fill_insn, 0 },
  { "scomm", m32r_scomm, 0 },
  { NULL, NULL, 0 }
};

/* FIXME: Should be machine generated.  */
#define NOP_INSN 0x7000
#define PAR_NOP_INSN 0xf000 /* can only be used in 2nd slot */

/* When we align the .text section, insert the correct NOP pattern.
   N is the power of 2 alignment.  LEN is the length of pattern FILL.
   MAX is the maximum number of characters to skip when doing the alignment,
   or 0 if there is no maximum.  */

int
m32r_do_align (n, fill, len, max)
     int n;
     const char *fill;
     int len;
     int max;
{
  if ((fill == NULL || (*fill == 0 && len == 1))
      && (now_seg->flags & SEC_CODE) != 0
      /* Only do this special handling if aligning to at least a
	 4 byte boundary.  */
      && n > 1
      /* Only do this special handling if we're allowed to emit at
	 least two bytes.  */
      && (max == 0 || max > 1))
    {
      static const unsigned char nop_pattern[] = { 0xf0, 0x00 };

#if 0
      /* First align to a 2 byte boundary, in case there is an odd .byte.  */
      /* FIXME: How much memory will cause gas to use when assembling a big
	 program?  Perhaps we can avoid the frag_align call?  */
      frag_align (1, 0, 0);
#endif
      /* Next align to a 4 byte boundary (we know n >= 2) using a parallel
	 nop.  */
      frag_align_pattern (2, nop_pattern, sizeof nop_pattern, 0);
      /* If doing larger alignments use a repeating sequence of appropriate
	 nops.  */
      if (n > 2)
	{
	  static const unsigned char multi_nop_pattern[] = { 0x70, 0x00, 0xf0, 0x00 };
	  frag_align_pattern (n, multi_nop_pattern, sizeof multi_nop_pattern,
			      max ? max - 2 : 0);
	}
      return 1;
    }

  return 0;
}

static void
assemble_nop (opcode)
     int opcode;
{
  char *f = frag_more (2);
  md_number_to_chars (f, opcode, 2);
}

/* If the last instruction was the first of 2 16 bit insns,
   output a nop to move the PC to a 32 bit boundary.

   This is done via an alignment specification since branch relaxing
   may make it unnecessary.

   Internally, we need to output one of these each time a 32 bit insn is
   seen after an insn that is relaxable.  */

static void
fill_insn (ignore)
     int ignore;
{
  (void) m32r_do_align (2, NULL, 0, 0);
  prev_insn = NULL;
  seen_relaxable_p = 0;
}

/* Cover function to fill_insn called after a label and at end of assembly.

   The result is always 1: we're called in a conditional to see if the
   current line is a label.  */

int
m32r_fill_insn (done)
     int done;
{
  segT seg;
  subsegT subseg;

  if (prev_seg != NULL)
    {
      seg = now_seg;
      subseg = now_subseg;
      subseg_set (prev_seg, prev_subseg);
      fill_insn (0);
      subseg_set (seg, subseg);
    }
  return 1;
}

void
md_begin ()
{
  flagword applicable;
  segT seg;
  subsegT subseg;

  /* Initialize the `cgen' interface.  */

  /* This is a callback from cgen to gas to parse operands.  */
  cgen_parse_operand_fn = cgen_parse_operand;
  /* Set the machine number and endian.  */
  CGEN_SYM (init_asm) (0 /* mach number */,
		       target_big_endian ? CGEN_ENDIAN_BIG : CGEN_ENDIAN_LITTLE);

#if 0 /* not supported yet */
  /* If a runtime cpu description file was provided, parse it.  */
  if (m32r_cpu_desc != NULL)
    {
      const char *errmsg;

      errmsg = cgen_read_cpu_file (m32r_cpu_desc);
      if (errmsg != NULL)
	as_bad ("%s: %s", m32r_cpu_desc, errmsg);
    }
#endif

  /* Save the current subseg so we can restore it [it's the default one and
     we don't want the initial section to be .sbss.  */
  seg = now_seg;
  subseg = now_subseg;

  /* The sbss section is for local .scomm symbols.  */
  sbss_section = subseg_new (".sbss", 0);
  /* This is copied from perform_an_assembly_pass.  */
  applicable = bfd_applicable_section_flags (stdoutput);
  bfd_set_section_flags (stdoutput, sbss_section, applicable & SEC_ALLOC);
#if 0 /* What does this do? [see perform_an_assembly_pass]  */
  seg_info (bss_section)->bss = 1;
#endif

  subseg_set (seg, subseg);

  /* We must construct a fake section similar to bfd_com_section
     but with the name .scommon.  */
  scom_section = bfd_com_section;
  scom_section.name = ".scommon";
  scom_section.output_section = &scom_section;
  scom_section.symbol = &scom_symbol;
  scom_section.symbol_ptr_ptr = &scom_section.symbol;
  scom_symbol = *bfd_com_section.symbol;
  scom_symbol.name = ".scommon";
  scom_symbol.section = &scom_section;
}

void
md_assemble (str)
     char *str;
{
#ifdef CGEN_INT_INSN
  cgen_insn_t buffer[CGEN_MAX_INSN_SIZE / sizeof (cgen_insn_t)];
#else
  char buffer[CGEN_MAX_INSN_SIZE];
#endif
  struct cgen_fields fields;
  const struct cgen_insn *insn;
  char *errmsg;

  /* Initialize GAS's cgen interface for a new instruction.  */
  cgen_asm_init_parse ();

  insn = CGEN_SYM (assemble_insn) (str, &fields, buffer, &errmsg);
  if (!insn)
    {
      as_bad (errmsg);
      return;
    }

  if (CGEN_INSN_BITSIZE (insn) == 32)
    {
      /* 32 bit insns must live on 32 bit boundaries.  */
      /* FIXME: If calling fill_insn too many times turns us into a memory
	 pig, can we call assemble_nop instead of !seen_relaxable_p?  */
      if (prev_insn || seen_relaxable_p)
	fill_insn (0);
      cgen_asm_finish_insn (insn, buffer, CGEN_FIELDS_BITSIZE (&fields));
    }
  else
    {
      /* Keep track of whether we've seen a pair of 16 bit insns.
	 PREV_INSN is NULL when we're on a 32 bit boundary.  */
      if (prev_insn)
	prev_insn = NULL;
      else
	prev_insn = insn;
      cgen_asm_finish_insn (insn, buffer, CGEN_FIELDS_BITSIZE (&fields));

      /* If the insn needs the following one to be on a 32 bit boundary
	 (e.g. subroutine calls), fill this insn's slot.  */
      if (prev_insn
	  && CGEN_INSN_ATTR (insn, CGEN_INSN_FILL_SLOT) != 0)
	fill_insn (0);
    }

  /* If this is a relaxable insn (can be replaced with a larger version)
     mark the fact so that we can emit an alignment directive for a following
     32 bit insn if we see one.   */
  if (CGEN_INSN_ATTR (insn, CGEN_INSN_RELAXABLE) != 0)
    seen_relaxable_p = 1;

  /* Set these so m32r_fill_insn can use them.  */
  prev_seg = now_seg;
  prev_subseg = now_subseg;
}

/* The syntax in the manual says constants begin with '#'.
   We just ignore it.  */

void 
md_operand (expressionP)
     expressionS *expressionP;
{
  if (*input_line_pointer == '#')
    {
      input_line_pointer++;
      expression (expressionP);
    }
}

valueT
md_section_align (segment, size)
     segT segment;
     valueT size;
{
  int align = bfd_get_section_alignment (stdoutput, segment);
  return ((size + (1 << align) - 1) & (-1 << align));
}

symbolS *
md_undefined_symbol (name)
  char *name;
{
  return 0;
}

/* .scomm pseudo-op handler.

   This is a new pseudo-op to handle putting objects in .scommon.
   By doing this the linker won't need to do any work and more importantly
   it removes the implicit -G arg necessary to correctly link the object file.
*/

static void
m32r_scomm (ignore)
     int ignore;
{
  register char *name;
  register char c;
  register char *p;
  offsetT size;
  register symbolS *symbolP;
  offsetT align;
  int align2;

  name = input_line_pointer;
  c = get_symbol_end ();

  /* just after name is now '\0' */
  p = input_line_pointer;
  *p = c;
  SKIP_WHITESPACE ();
  if (*input_line_pointer != ',')
    {
      as_bad ("Expected comma after symbol-name: rest of line ignored.");
      ignore_rest_of_line ();
      return;
    }

  input_line_pointer++;		/* skip ',' */
  if ((size = get_absolute_expression ()) < 0)
    {
      as_warn (".SCOMMon length (%ld.) <0! Ignored.", (long) size);
      ignore_rest_of_line ();
      return;
    }

  /* The third argument to .scomm is the alignment.  */
  if (*input_line_pointer != ',')
    align = 8;
  else
    {
      ++input_line_pointer;
      align = get_absolute_expression ();
      if (align <= 0)
	{
	  as_warn ("ignoring bad alignment");
	  align = 8;
	}
    }
  /* Convert to a power of 2 alignment.  */
  if (align)
    {
      for (align2 = 0; (align & 1) == 0; align >>= 1, ++align2)
	continue;
      if (align != 1)
	{
	  as_bad ("Common alignment not a power of 2");
	  ignore_rest_of_line ();
	  return;
	}
    }
  else
    align2 = 0;

  *p = 0;
  symbolP = symbol_find_or_make (name);
  *p = c;

  if (S_IS_DEFINED (symbolP))
    {
      as_bad ("Ignoring attempt to re-define symbol `%s'.",
	      S_GET_NAME (symbolP));
      ignore_rest_of_line ();
      return;
    }

  if (S_GET_VALUE (symbolP) && S_GET_VALUE (symbolP) != (valueT) size)
    {
      as_bad ("Length of .scomm \"%s\" is already %ld. Not changed to %ld.",
	      S_GET_NAME (symbolP),
	      (long) S_GET_VALUE (symbolP),
	      (long) size);

      ignore_rest_of_line ();
      return;
    }

  if (symbolP->local)
    {
      segT old_sec = now_seg;
      int old_subsec = now_subseg;
      char *pfrag;

      record_alignment (sbss_section, align2);
      subseg_set (sbss_section, 0);
      if (align2)
	frag_align (align2, 0, 0);
      if (S_GET_SEGMENT (symbolP) == sbss_section)
	symbolP->sy_frag->fr_symbol = 0;
      symbolP->sy_frag = frag_now;
      pfrag = frag_var (rs_org, 1, 1, (relax_substateT) 0, symbolP, size,
			(char *) 0);
      *pfrag = 0;
      S_SET_SIZE (symbolP, size);
      S_SET_SEGMENT (symbolP, sbss_section);
      S_CLEAR_EXTERNAL (symbolP);
      subseg_set (old_sec, old_subsec);
    }
  else
    {
      S_SET_VALUE (symbolP, (valueT) size);
      S_SET_ALIGN (symbolP, align2);
      S_SET_EXTERNAL (symbolP);
      S_SET_SEGMENT (symbolP, &scom_section);
    }

  demand_empty_rest_of_line ();
}

/* Interface to relax_segment.  */

/* FIXME: Build table by hand, get it working, then machine generate.  */

const relax_typeS md_relax_table[] =
{
/* The fields are:
   1) most positive reach of this state,
   2) most negative reach of this state,
   3) how many bytes this mode will add to the size of the current frag
   4) which index into the table to try if we can't fit into this one.  */

  /* The first entry must be unused because an `rlx_more' value of zero ends
     each list.  */
  {1, 1, 0, 0},

  /* The displacement used by GAS is from the end of the 2 byte insn,
     so we subtract 2 from the following.  */
  /* 16 bit insn, 8 bit disp -> 10 bit range.
     This doesn't handle a branch in the right slot at the border:
     the "& -4" isn't taken into account.  It's not important enough to
     complicate things over it, so we subtract an extra 2 (or + 2 in -ve
     case).  */
  {511 - 2 - 2, -512 - 2 + 2, 0, 2 },
  /* 32 bit insn, 24 bit disp -> 26 bit range.  */
  {0x2000000 - 1 - 2, -0x2000000 - 2, 2, 0 },
  /* Same thing, but with leading nop for alignment.  */
  {0x2000000 - 1 - 2, -0x2000000 - 2, 4, 0 }
};

long
m32r_relax_frag (fragP, stretch)
     fragS *fragP;
     long stretch;
{
  /* Address of branch insn.  */
  long address = fragP->fr_address + fragP->fr_fix - 2;
  long growth = 0;

  /* Keep 32 bit insns aligned on 32 bit boundaries.  */
  if (fragP->fr_subtype == 2)
    {
      if ((address & 3) != 0)
	{
	  fragP->fr_subtype = 3;
	  growth = 2;
	}
    }
  else if (fragP->fr_subtype == 3)
    {
      if ((address & 3) == 0)
	{
	  fragP->fr_subtype = 2;
	  growth = -2;
	}
    }
  else
    {
      growth = relax_frag (fragP, stretch);

      /* Long jump on odd halfword boundary?  */
      if (fragP->fr_subtype == 2 && (address & 3) != 0)
	{
	  fragP->fr_subtype = 3;
	  growth += 2;
	}
    }

  return growth;
}

/* Return an initial guess of the length by which a fragment must grow to
   hold a branch to reach its destination.
   Also updates fr_type/fr_subtype as necessary.

   Called just before doing relaxation.
   Any symbol that is now undefined will not become defined.
   The guess for fr_var is ACTUALLY the growth beyond fr_fix.
   Whatever we do to grow fr_fix or fr_var contributes to our returned value.
   Although it may not be explicit in the frag, pretend fr_var starts with a
   0 value.  */

int
md_estimate_size_before_relax (fragP, segment)
     fragS *fragP;
     segT segment;
{
  int old_fr_fix = fragP->fr_fix;
  char *opcode = fragP->fr_opcode;

  /* The only thing we have to handle here are symbols outside of the
     current segment.  They may be undefined or in a different segment in
     which case linker scripts may place them anywhere.
     However, we can't finish the fragment here and emit the reloc as insn
     alignment requirements may move the insn about.  */

  if (S_GET_SEGMENT (fragP->fr_symbol) != segment)
    {
      /* The symbol is undefined in this segment.
	 Change the relaxation subtype to the max allowable and leave
	 all further handling to md_convert_frag.  */
      fragP->fr_subtype = 2;

#if 0 /* Can't use this, but leave in for illustration.  */     
      /* Change 16 bit insn to 32 bit insn.  */
      opcode[0] |= 0x80;

      /* Increase known (fixed) size of fragment.  */
      fragP->fr_fix += 2;

      /* Create a relocation for it.  */
      fix_new (fragP, old_fr_fix, 4,
	       fragP->fr_symbol,
	       fragP->fr_offset, 1 /* pcrel */,
	       /* FIXME: Can't use a real BFD reloc here.
		  cgen_md_apply_fix3 can't handle it.  */
	       BFD_RELOC_M32R_26_PCREL);

      /* Mark this fragment as finished.  */
      frag_wane (fragP);
#else
      return 2;
#endif
    }

  return (fragP->fr_var + fragP->fr_fix - old_fr_fix);
} 

/* *fragP has been relaxed to its final size, and now needs to have
   the bytes inside it modified to conform to the new size.

   Called after relaxation is finished.
   fragP->fr_type == rs_machine_dependent.
   fragP->fr_subtype is the subtype of what the address relaxed to.  */

void
md_convert_frag (abfd, sec, fragP)
  bfd *abfd;
  segT sec;
  fragS *fragP;
{
  char *opcode, *displacement;
  int target_address, opcode_address, extension, addend;

  opcode = fragP->fr_opcode;

  /* Address opcode resides at in file space.  */
  opcode_address = fragP->fr_address + fragP->fr_fix - 2;

  switch (fragP->fr_subtype)
    {
    case 1 :
      extension = 0;
      displacement = &opcode[1];
      break;
    case 2 :
      opcode[0] |= 0x80;
      extension = 2;
      displacement = &opcode[1];
      break;
    case 3 :
      opcode[2] = opcode[0] | 0x80;
      md_number_to_chars (opcode, PAR_NOP_INSN, 2);
      opcode_address += 2;
      extension = 4;
      displacement = &opcode[3];
      break;
    default :
      abort ();
    }

  if (S_GET_SEGMENT (fragP->fr_symbol) != sec)
    {
      /* symbol must be resolved by linker */
      if (fragP->fr_offset & 3)
	as_warn ("Addend to unresolved symbol not on word boundary.");
      addend = fragP->fr_offset >> 2;
    }
  else
    {
      /* Address we want to reach in file space.  */
      target_address = S_GET_VALUE (fragP->fr_symbol) + fragP->fr_offset;
      target_address += fragP->fr_symbol->sy_frag->fr_address;
      addend = (target_address - (opcode_address & -4)) >> 2;
    }

  /* Create a relocation for symbols that must be resolved by the linker.
     Otherwise output the completed insn.  */

  if (S_GET_SEGMENT (fragP->fr_symbol) != sec)
    {
      assert (fragP->fr_subtype != 1);
      assert (fragP->fr_targ.cgen.insn != 0);
      cgen_record_fixup (fragP,
			 /* Offset of branch insn in frag.  */
			 fragP->fr_fix + extension - 4,
			 fragP->fr_targ.cgen.insn,
			 4 /*length*/,
			 /* FIXME: quick hack */
#if 0
			 CGEN_OPERAND_ENTRY (fragP->fr_targ.cgen.opindex),
#else
			 CGEN_OPERAND_ENTRY (M32R_OPERAND_DISP24),
#endif
			 fragP->fr_targ.cgen.opinfo,
			 fragP->fr_symbol, fragP->fr_offset);
    }

#define SIZE_FROM_RELAX_STATE(n) ((n) == 1 ? 1 : 3)

  md_number_to_chars (displacement, (valueT) addend,
		      SIZE_FROM_RELAX_STATE (fragP->fr_subtype));

  fragP->fr_fix += extension;
}

/* Functions concerning relocs.  */

/* The location from which a PC relative jump should be calculated,
   given a PC relative reloc.  */

long
md_pcrel_from_section (fixP, sec)
     fixS *fixP;
     segT sec;
{
  if (fixP->fx_addsy != (symbolS *) NULL
      && (! S_IS_DEFINED (fixP->fx_addsy)
	  || S_GET_SEGMENT (fixP->fx_addsy) != sec))
    {
      /* The symbol is undefined (or is defined but not in this section).
	 Let the linker figure it out.  */
      return 0;
    }

  return (fixP->fx_frag->fr_address + fixP->fx_where) & -4L;
}

/* Return the bfd reloc type for OPERAND of INSN at fixup FIXP.
   Returns BFD_RELOC_NONE if no reloc type can be found.
   *FIXP may be modified if desired.  */

bfd_reloc_code_real_type
CGEN_SYM (lookup_reloc) (insn, operand, fixP)
     const struct cgen_insn *insn;
     const struct cgen_operand *operand;
     fixS *fixP;
{
  switch (CGEN_OPERAND_TYPE (operand))
    {
    case M32R_OPERAND_DISP8 : return  BFD_RELOC_M32R_10_PCREL;
    case M32R_OPERAND_DISP16 : return BFD_RELOC_M32R_18_PCREL;
    case M32R_OPERAND_DISP24 : return BFD_RELOC_M32R_26_PCREL;
    case M32R_OPERAND_UIMM24 : return BFD_RELOC_M32R_24;
    case M32R_OPERAND_HI16 :
    case M32R_OPERAND_SLO16 :
    case M32R_OPERAND_ULO16 :
      /* If low/high/shigh/sda was used, it is recorded in `opinfo'.  */
      if (fixP->tc_fix_data.opinfo != 0)
	return fixP->tc_fix_data.opinfo;
      break;
    }
  return BFD_RELOC_NONE;
}

/* Called while parsing an instruction to create a fixup.
   We need to check for HI16 relocs and queue them up for later sorting.  */

fixS *
m32r_cgen_record_fixup_exp (frag, where, insn, length, operand, opinfo, exp)
     fragS *frag;
     int where;
     const struct cgen_insn *insn;
     int length;
     const struct cgen_operand *operand;
     int opinfo;
     expressionS *exp;
{
  fixS *fixP = cgen_record_fixup_exp (frag, where, insn, length,
				      operand, opinfo, exp);

  switch (CGEN_OPERAND_TYPE (operand))
    {
    case M32R_OPERAND_HI16 :
      /* If low/high/shigh/sda was used, it is recorded in `opinfo'.  */
      if (fixP->tc_fix_data.opinfo == BFD_RELOC_M32R_HI16_SLO
	  || fixP->tc_fix_data.opinfo == BFD_RELOC_M32R_HI16_ULO)
	m32r_record_hi16 (fixP->tc_fix_data.opinfo, fixP, now_seg);
      break;
    }

  return fixP;
}

/* Record a HI16 reloc for later matching with its LO16 cousin.  */

static void
m32r_record_hi16 (reloc_type, fixP, seg)
     int reloc_type;
     fixS *fixP;
     segT seg;
{
  struct m32r_hi_fixup *hi_fixup;

  assert (reloc_type == BFD_RELOC_M32R_HI16_SLO
	  || reloc_type == BFD_RELOC_M32R_HI16_ULO);

  hi_fixup = ((struct m32r_hi_fixup *)
	      xmalloc (sizeof (struct m32r_hi_fixup)));
  hi_fixup->fixp = fixP;
  hi_fixup->seg = now_seg;
  hi_fixup->next = m32r_hi_fixup_list;
  m32r_hi_fixup_list = hi_fixup;
}

/* Return BFD reloc type from opinfo field in a fixS.
   It's tricky using fx_r_type in m32r_frob_file because the values
   are BFD_RELOC_UNUSED + operand number.  */
#define FX_OPINFO_R_TYPE(f) ((f)->tc_fix_data.opinfo)

/* Sort any unmatched HI16 relocs so that they immediately precede
   the corresponding LO16 reloc.  This is called before md_apply_fix and
   tc_gen_reloc.  */

void
m32r_frob_file ()
{
  struct m32r_hi_fixup *l;

  for (l = m32r_hi_fixup_list; l != NULL; l = l->next)
    {
      segment_info_type *seginfo;
      int pass;

      assert (FX_OPINFO_R_TYPE (l->fixp) == BFD_RELOC_M32R_HI16_SLO
	      || FX_OPINFO_R_TYPE (l->fixp) == BFD_RELOC_M32R_HI16_ULO);

      /* Check quickly whether the next fixup happens to be a matching low.  */
      if (l->fixp->fx_next != NULL
	  && FX_OPINFO_R_TYPE (l->fixp->fx_next) == BFD_RELOC_M32R_LO16
	  && l->fixp->fx_addsy == l->fixp->fx_next->fx_addsy
	  && l->fixp->fx_offset == l->fixp->fx_next->fx_offset)
	continue;

      /* Look through the fixups for this segment for a matching `low'.
         When we find one, move the high/shigh just in front of it.  We do
         this in two passes.  In the first pass, we try to find a
         unique `low'.  In the second pass, we permit multiple high's
         relocs for a single `low'.  */
      seginfo = seg_info (l->seg);
      for (pass = 0; pass < 2; pass++)
	{
	  fixS *f, *prev;

	  prev = NULL;
	  for (f = seginfo->fix_root; f != NULL; f = f->fx_next)
	    {
	      /* Check whether this is a `low' fixup which matches l->fixp.  */
	      if (FX_OPINFO_R_TYPE (f) == BFD_RELOC_M32R_LO16
		  && f->fx_addsy == l->fixp->fx_addsy
		  && f->fx_offset == l->fixp->fx_offset
		  && (pass == 1
		      || prev == NULL
		      || (FX_OPINFO_R_TYPE (prev) != BFD_RELOC_M32R_HI16_SLO
			  && FX_OPINFO_R_TYPE (prev) != BFD_RELOC_M32R_HI16_ULO)
		      || prev->fx_addsy != f->fx_addsy
		      || prev->fx_offset !=  f->fx_offset))
		{
		  fixS **pf;

		  /* Move l->fixp before f.  */
		  for (pf = &seginfo->fix_root;
		       *pf != l->fixp;
		       pf = &(*pf)->fx_next)
		    assert (*pf != NULL);

		  *pf = l->fixp->fx_next;

		  l->fixp->fx_next = f;
		  if (prev == NULL)
		    seginfo->fix_root = l->fixp;
		  else
		    prev->fx_next = l->fixp;

		  break;
		}

	      prev = f;
	    }

	  if (f != NULL)
	    break;

	  if (pass == 1)
	    as_warn_where (l->fixp->fx_file, l->fixp->fx_line,
			   "Unmatched high/shigh reloc");
	}
    }
}

/* See whether we need to force a relocation into the output file.
   This is used to force out switch and PC relative relocations when
   relaxing.  */

int
m32r_force_relocation (fix)
     fixS *fix;
{
  if (! m32r_relax)
    return 0;

  return (fix->fx_pcrel
	  || 0 /* ??? */);
}

/* Write a value out to the object file, using the appropriate endianness.  */

void
md_number_to_chars (buf, val, n)
     char *buf;
     valueT val;
     int n;
{
  if (target_big_endian)
    number_to_chars_bigendian (buf, val, n);
  else
    number_to_chars_littleendian (buf, val, n);
}

/* Turn a string in input_line_pointer into a floating point constant of type
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
  int i,prec;
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

   /* FIXME: Some targets allow other format chars for bigger sizes here.  */

    default:
      *sizeP = 0;
      return "Bad call to md_atof()";
    }

  t = atof_ieee (input_line_pointer, type, words);
  if (t)
    input_line_pointer = t;
  *sizeP = prec * sizeof (LITTLENUM_TYPE);

  if (target_big_endian)
    {
      for (i = 0; i < prec; i++)
	{
	  md_number_to_chars (litP, (valueT) words[i], sizeof (LITTLENUM_TYPE));
	  litP += sizeof (LITTLENUM_TYPE);
	}
    }
  else
    {
      for (i = prec - 1; i >= 0; i--)
	{
	  md_number_to_chars (litP, (valueT) words[i], sizeof (LITTLENUM_TYPE));
	  litP += sizeof (LITTLENUM_TYPE);
	}
    }
     
  return 0;
}
