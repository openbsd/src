/* tc-m68k.c -- Assemble for the m68k family
   Copyright (C) 1987, 91, 92, 93, 94, 1995 Free Software Foundation, Inc.

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

#include <ctype.h>
#define  NO_RELOC 0
#include "as.h"
#include "obstack.h"
#include "subsegs.h"

#include "opcode/m68k.h"
#include "m68k-parse.h"

/* This array holds the chars that always start a comment.  If the
   pre-processor is disabled, these aren't very useful */
#if defined (OBJ_ELF) || defined (TE_DELTA)
const char comment_chars[] = "|#";
#else
const char comment_chars[] = "|";
#endif

/* This array holds the chars that only start a comment at the beginning of
   a line.  If the line seems to have the form '# 123 filename'
   .line and .file directives will appear in the pre-processed output */
/* Note that input_file.c hand checks for '#' at the beginning of the
   first line of the input file.  This is because the compiler outputs
   #NO_APP at the beginning of its output. */
/* Also note that comments like this one will always work. */
const char line_comment_chars[] = "#";

const char line_separator_chars[] = "";

/* Chars that can be used to separate mant from exp in floating point nums */
CONST char EXP_CHARS[] = "eE";

/* Chars that mean this number is a floating point constant, as
   in "0f12.456" or "0d1.2345e12".  */

CONST char FLT_CHARS[] = "rRsSfFdDxXeEpP";

/* Also be aware that MAXIMUM_NUMBER_OF_CHARS_FOR_FLOAT may have to be
   changed in read.c .  Ideally it shouldn't have to know about it at all,
   but nothing is ideal around here.  */

const int md_reloc_size = 8;	/* Size of relocation record */

/* Are we trying to generate PIC code?  If so, absolute references
   ought to be made into linkage table references or pc-relative
   references.  */
int flag_want_pic;

static int flag_short_refs;	/* -l option */
static int flag_long_jumps;	/* -S option */

#ifdef REGISTER_PREFIX_OPTIONAL
int flag_reg_prefix_optional = REGISTER_PREFIX_OPTIONAL;
#else
int flag_reg_prefix_optional;
#endif

/* The floating point coprocessor to use by default.  */
static enum m68k_register m68k_float_copnum = COP1;

/* If this is non-zero, then references to number(%pc) will be taken
   to refer to number, rather than to %pc + number.  */
static int m68k_abspcadd;

/* If this is non-zero, then the quick forms of the move, add, and sub
   instructions are used when possible.  */
static int m68k_quick = 1;

/* If this is non-zero, then if the size is not specified for a base
   or outer displacement, the assembler assumes that the size should
   be 32 bits.  */
static int m68k_rel32 = 1;

/* Its an arbitrary name:  This means I don't approve of it */
/* See flames below */
static struct obstack robyn;

#define TAB(x,y)	(((x)<<2)+(y))
#define TABTYPE(xy)     ((xy) >> 2)
#define BYTE		0
#define SHORT		1
#define LONG		2
#define SZ_UNDEF	3
#undef BRANCH
/* Case `g' except when BCC68000 is applicable.  */
#define ABRANCH		1
/* Coprocessor branches.  */
#define FBRANCH		2
/* Mode 7.2 -- program counter indirect with (16-bit) displacement,
   supported on all cpus.  Widens to 32-bit absolute.  */
#define PCREL		3
/* For inserting an extra jmp instruction with long offset on 68000,
   for expanding conditional branches.  (Not bsr or bra.)  Since the
   68000 doesn't support 32-bit displacements for conditional
   branches, we fake it by reversing the condition and branching
   around a jmp with an absolute long operand.  */
#define BCC68000        4
/* For the DBcc "instructions".  If the displacement requires 32 bits,
   the branch-around-a-jump game is played here too.  */
#define DBCC            5
/* Not currently used?  */
#define PCLEA		6
/* Mode AINDX (apc-relative) using PC, with variable target, might fit
   in 16 or 8 bits.  */
#define PCINDEX		7

struct m68k_incant
  {
    const char *m_operands;
    unsigned long m_opcode;
    short m_opnum;
    short m_codenum;
    int m_arch;
    struct m68k_incant *m_next;
  };

#define getone(x)	((((x)->m_opcode)>>16)&0xffff)
#define gettwo(x)	(((x)->m_opcode)&0xffff)

static const enum m68k_register m68000_control_regs[] = { 0 };
static const enum m68k_register m68010_control_regs[] = {
  SFC, DFC, USP, VBR,
  0
};
static const enum m68k_register m68020_control_regs[] = {
  SFC, DFC, USP, VBR, CACR, CAAR, MSP, ISP,
  0
};
static const enum m68k_register m68040_control_regs[] = {
  SFC, DFC, CACR, TC, ITT0, ITT1, DTT0, DTT1,
  USP, VBR, MSP, ISP, MMUSR, URP, SRP,
  0
};
static const enum m68k_register m68060_control_regs[] = {
  SFC, DFC, CACR, TC, ITT0, ITT1, DTT0, DTT1, BUSCR,
  USP, VBR, URP, SRP, PCR,
  0
};
#define cpu32_control_regs m68010_control_regs

static const enum m68k_register *control_regs;

/* internal form of a 68020 instruction */
struct m68k_it
{
  const char *error;
  const char *args;		/* list of opcode info */
  int numargs;

  int numo;			/* Number of shorts in opcode */
  short opcode[11];

  struct m68k_op operands[6];

  int nexp;			/* number of exprs in use */
  struct m68k_exp exprs[4];

  int nfrag;			/* Number of frags we have to produce */
  struct
    {
      int fragoff;		/* Where in the current opcode the frag ends */
      symbolS *fadd;
      long foff;
      int fragty;
    }
  fragb[4];

  int nrel;			/* Num of reloc strucs in use */
  struct
    {
      int n;
      expressionS exp;
      char wid;
      char pcrel;
      /* In a pc relative address the difference between the address
	 of the offset and the address that the offset is relative
	 to.  This depends on the addressing mode.  Basically this
	 is the value to put in the offset field to address the
	 first byte of the offset, without regarding the special
	 significance of some values (in the branch instruction, for
	 example).  */
      int pcrel_fix;
    }
  reloc[5];			/* Five is enough??? */
};

#define cpu_of_arch(x)		((x) & m68000up)
#define float_of_arch(x)	((x) & mfloat)
#define mmu_of_arch(x)		((x) & mmmu)

static struct m68k_it the_ins;	/* the instruction being assembled */

#define op(ex)		((ex)->exp.X_op)
#define adds(ex)	((ex)->exp.X_add_symbol)
#define subs(ex)	((ex)->exp.X_op_symbol)
#define offs(ex)	((ex)->exp.X_add_number)

/* Macros for adding things to the m68k_it struct */

#define addword(w)	the_ins.opcode[the_ins.numo++]=(w)

/* Like addword, but goes BEFORE general operands */
static void
insop (w, opcode)
     int w;
     struct m68k_incant *opcode;
{
  int z;
  for(z=the_ins.numo;z>opcode->m_codenum;--z)
    the_ins.opcode[z]=the_ins.opcode[z-1];
  for(z=0;z<the_ins.nrel;z++)
    the_ins.reloc[z].n+=2;
  for (z = 0; z < the_ins.nfrag; z++)
    the_ins.fragb[z].fragoff++;
  the_ins.opcode[opcode->m_codenum]=w;
  the_ins.numo++;
}

/* The numo+1 kludge is so we can hit the low order byte of the prev word.
   Blecch.  */
static void
add_fix (width, exp, pc_rel, pc_fix)
     char width;
     struct m68k_exp *exp;
     int pc_rel;
     int pc_fix;
{
  the_ins.reloc[the_ins.nrel].n = (((width)=='B')
				   ? (the_ins.numo*2-1)
				   : (((width)=='b')
				      ? (the_ins.numo*2+1)
				      : (the_ins.numo*2)));
  the_ins.reloc[the_ins.nrel].exp = exp->exp;
  the_ins.reloc[the_ins.nrel].wid = width;
  the_ins.reloc[the_ins.nrel].pcrel_fix = pc_fix;
  the_ins.reloc[the_ins.nrel++].pcrel = pc_rel;
}

/* Cause an extra frag to be generated here, inserting up to 10 bytes
   (that value is chosen in the frag_var call in md_assemble).  TYPE
   is the subtype of the frag to be generated; its primary type is
   rs_machine_dependent.

   The TYPE parameter is also used by md_convert_frag_1 and
   md_estimate_size_before_relax.  The appropriate type of fixup will
   be emitted by md_convert_frag_1.

   ADD becomes the FR_SYMBOL field of the frag, and OFF the FR_OFFSET.  */
static void
add_frag(add,off,type)
     symbolS *add;
     long off;
     int type;
{
  the_ins.fragb[the_ins.nfrag].fragoff=the_ins.numo;
  the_ins.fragb[the_ins.nfrag].fadd=add;
  the_ins.fragb[the_ins.nfrag].foff=off;
  the_ins.fragb[the_ins.nfrag++].fragty=type;
}

#define isvar(ex) \
  (op (ex) != O_constant && op (ex) != O_big)

static char *crack_operand PARAMS ((char *str, struct m68k_op *opP));
static int get_num PARAMS ((struct m68k_exp *exp, int ok));
static int reverse_16_bits PARAMS ((int in));
static int reverse_8_bits PARAMS ((int in));
static void install_gen_operand PARAMS ((int mode, int val));
static void install_operand PARAMS ((int mode, int val));
static void s_bss PARAMS ((int));
static void s_data1 PARAMS ((int));
static void s_data2 PARAMS ((int));
static void s_even PARAMS ((int));
static void s_proc PARAMS ((int));
static void mri_chip PARAMS ((void));
static void s_chip PARAMS ((int));
static void s_fopt PARAMS ((int));
static void s_opt PARAMS ((int));
static void s_reg PARAMS ((int));
static void s_restore PARAMS ((int));
static void s_save PARAMS ((int));
static void s_mri_if PARAMS ((int));
static void s_mri_else PARAMS ((int));
static void s_mri_endi PARAMS ((int));
static void s_mri_break PARAMS ((int));
static void s_mri_next PARAMS ((int));
static void s_mri_for PARAMS ((int));
static void s_mri_endf PARAMS ((int));
static void s_mri_repeat PARAMS ((int));
static void s_mri_until PARAMS ((int));
static void s_mri_while PARAMS ((int));
static void s_mri_endw PARAMS ((int));

static int current_architecture;

struct m68k_cpu {
  unsigned long arch;
  const char *name;
};

static const struct m68k_cpu archs[] = {
  { m68000, "68000" },
  { m68010, "68010" },
  { m68020, "68020" },
  { m68030, "68030" },
  { m68040, "68040" },
  { m68060, "68060" },
  { cpu32,  "cpu32" },
  { m68881, "68881" },
  { m68851, "68851" },
  /* Aliases (effectively, so far as gas is concerned) for the above
     cpus.  */
  { m68020, "68k" },
  { m68000, "68302" },
  { m68000, "68008" },
  { m68000, "68ec000" },
  { m68000, "68hc000" },
  { m68000, "68hc001" },
  { m68020, "68ec020" },
  { m68030, "68ec030" },
  { m68040, "68ec040" },
  { cpu32,  "68330" },
  { cpu32,  "68331" },
  { cpu32,  "68332" },
  { cpu32,  "68333" },
  { cpu32,  "68340" },
  { cpu32,  "68360" },
  { m68881, "68882" },
};

static const int n_archs = sizeof (archs) / sizeof (archs[0]);

/* BCC68000 is for patching in an extra jmp instruction for long offsets
   on the 68000.  The 68000 doesn't support long branches with branchs */

/* This table desribes how you change sizes for the various types of variable
   size expressions.  This version only supports two kinds. */

/* Note that calls to frag_var need to specify the maximum expansion
   needed; this is currently 10 bytes for DBCC.  */

/* The fields are:
   How far Forward this mode will reach:
   How far Backward this mode will reach:
   How many bytes this mode will add to the size of the frag
   Which mode to go to if the offset won't fit in this one
   */
relax_typeS md_relax_table[] =
{
  {1, 1, 0, 0},			/* First entries aren't used */
  {1, 1, 0, 0},			/* For no good reason except */
  {1, 1, 0, 0},			/* that the VAX doesn't either */
  {1, 1, 0, 0},

  {(127), (-128), 0, TAB (ABRANCH, SHORT)},
  {(32767), (-32768), 2, TAB (ABRANCH, LONG)},
  {0, 0, 4, 0},
  {1, 1, 0, 0},

  {1, 1, 0, 0},			/* FBRANCH doesn't come BYTE */
  {(32767), (-32768), 2, TAB (FBRANCH, LONG)},
  {0, 0, 4, 0},
  {1, 1, 0, 0},

  {1, 1, 0, 0},			/* PCREL doesn't come BYTE */
  {(32767), (-32768), 2, TAB (PCREL, LONG)},
  {0, 0, 4, 0},
  {1, 1, 0, 0},

  {(127), (-128), 0, TAB (BCC68000, SHORT)},
  {(32767), (-32768), 2, TAB (BCC68000, LONG)},
  {0, 0, 6, 0},			/* jmp long space */
  {1, 1, 0, 0},

  {1, 1, 0, 0},			/* DBCC doesn't come BYTE */
  {(32767), (-32768), 2, TAB (DBCC, LONG)},
  {0, 0, 10, 0},		/* bra/jmp long space */
  {1, 1, 0, 0},

  {1, 1, 0, 0},			/* PCLEA doesn't come BYTE */
  {32767, -32768, 2, TAB (PCLEA, LONG)},
  {0, 0, 6, 0},
  {1, 1, 0, 0},

  /* For, e.g., jmp pcrel indexed.  */
  {125, -130, 0, TAB (PCINDEX, SHORT)},
  {32765, -32770, 2, TAB (PCINDEX, LONG)},
  {0, 0, 4, 0},
  {1, 1, 0, 0},
};

/* These are the machine dependent pseudo-ops.  These are included so
   the assembler can work on the output from the SUN C compiler, which
   generates these.
   */

/* This table describes all the machine specific pseudo-ops the assembler
   has to support.  The fields are:
   pseudo-op name without dot
   function to call to execute this pseudo-op
   Integer arg to pass to the function
   */
const pseudo_typeS md_pseudo_table[] =
{
  {"data1", s_data1, 0},
  {"data2", s_data2, 0},
  {"bss", s_bss, 0},
  {"even", s_even, 0},
  {"skip", s_space, 0},
  {"proc", s_proc, 0},
#ifdef TE_SUN3
  {"align", s_align_bytes, 0},
#endif
#ifdef OBJ_ELF
  {"swbeg", s_ignore, 0},
#endif

  /* The following pseudo-ops are supported for MRI compatibility.  */
  {"chip", s_chip, 0},
  {"comline", s_space, 1},
  {"fopt", s_fopt, 0},
  {"mask2", s_ignore, 0},
  {"opt", s_opt, 0},
  {"reg", s_reg, 0},
  {"restore", s_restore, 0},
  {"save", s_save, 0},

  {"if", s_mri_if, 0},
  {"if.b", s_mri_if, 'b'},
  {"if.w", s_mri_if, 'w'},
  {"if.l", s_mri_if, 'l'},
  {"else", s_mri_else, 0},
  {"else.s", s_mri_else, 's'},
  {"else.l", s_mri_else, 'l'},
  {"endi", s_mri_endi, 0},
  {"break", s_mri_break, 0},
  {"break.s", s_mri_break, 's'},
  {"break.l", s_mri_break, 'l'},
  {"next", s_mri_next, 0},
  {"next.s", s_mri_next, 's'},
  {"next.l", s_mri_next, 'l'},
  {"for", s_mri_for, 0},
  {"for.b", s_mri_for, 'b'},
  {"for.w", s_mri_for, 'w'},
  {"for.l", s_mri_for, 'l'},
  {"endf", s_mri_endf, 0},
  {"repeat", s_mri_repeat, 0},
  {"until", s_mri_until, 0},
  {"until.b", s_mri_until, 'b'},
  {"until.w", s_mri_until, 'w'},
  {"until.l", s_mri_until, 'l'},
  {"while", s_mri_while, 0},
  {"while.b", s_mri_while, 'b'},
  {"while.w", s_mri_while, 'w'},
  {"while.l", s_mri_while, 'l'},
  {"endw", s_mri_endw, 0},

  {0, 0, 0}
};


/* The mote pseudo ops are put into the opcode table, since they
   don't start with a . they look like opcodes to gas.
   */
extern void obj_coff_section ();

CONST pseudo_typeS mote_pseudo_table[] =
{

  {"dcl", cons, 4},
  {"dc", cons, 2},
  {"dcw", cons, 2},
  {"dcb", cons, 1},

  {"dsl", s_space, 4},
  {"ds", s_space, 2},
  {"dsw", s_space, 2},
  {"dsb", s_space, 1},

  {"xdef", s_globl, 0},
  {"align", s_align_ptwo, 0},
#ifdef M68KCOFF
  {"sect", obj_coff_section, 0},
  {"section", obj_coff_section, 0},
#endif
  {0, 0, 0}
};

#define issbyte(x)	((x)>=-128 && (x)<=127)
#define isubyte(x)	((x)>=0 && (x)<=255)
#define issword(x)	((x)>=-32768 && (x)<=32767)
#define isuword(x)	((x)>=0 && (x)<=65535)

#define isbyte(x)	((x)>= -255 && (x)<=255)
#define isword(x)	((x)>=-65536 && (x)<=65535)
#define islong(x)	(1)

extern char *input_line_pointer;

static char mklower_table[256];
#define mklower(c) (mklower_table[(unsigned char)(c)])
static char notend_table[256];
static char alt_notend_table[256];
#define notend(s)						\
  (! (notend_table[(unsigned char) *s]				\
      || (*s == ':'						\
	  && alt_notend_table[(unsigned char) s[1]])))

#if defined (M68KCOFF) && !defined (BFD_ASSEMBLER)

#ifdef NO_PCREL_RELOCS

int
make_pcrel_absolute(fixP, add_number)
    fixS *fixP;
    long *add_number;
{
  register unsigned char *opcode = fixP->fx_frag->fr_opcode;

  /* rewrite the PC relative instructions to absolute address ones.
   * these are rumoured to be faster, and the apollo linker refuses
   * to deal with the PC relative relocations.
   */
  if (opcode[0] == 0x60 && opcode[1] == 0xff) /* BRA -> JMP */
    {
      opcode[0] = 0x4e;
      opcode[1] = 0xf9;
    }
  else if (opcode[0] == 0x61 && opcode[1] == 0xff) /* BSR -> JSR */
    {
      opcode[0] = 0x4e;
      opcode[1] = 0xb9;
    }
  else
    as_fatal ("Unknown PC relative instruction");
  *add_number -= 4;
  return 0;
}

#endif /* NO_PCREL_RELOCS */

short
tc_coff_fix2rtype (fixP)
     fixS *fixP;
{
  if (fixP->fx_tcbit && fixP->fx_size == 4)
    return R_RELLONG_NEG;
#ifdef NO_PCREL_RELOCS
  know (fixP->fx_pcrel == 0);
  return (fixP->fx_size == 1 ? R_RELBYTE
	  : fixP->fx_size == 2 ? R_DIR16
	  : R_DIR32);
#else
  return (fixP->fx_pcrel ?
	  (fixP->fx_size == 1 ? R_PCRBYTE :
	   fixP->fx_size == 2 ? R_PCRWORD :
	   R_PCRLONG) :
	  (fixP->fx_size == 1 ? R_RELBYTE :
	   fixP->fx_size == 2 ? R_RELWORD :
	   R_RELLONG));
#endif
}

#endif

#ifdef BFD_ASSEMBLER

arelent *
tc_gen_reloc (section, fixp)
     asection *section;
     fixS *fixp;
{
  arelent *reloc;
  bfd_reloc_code_real_type code;

  if (fixp->fx_tcbit)
    abort ();

#define F(SZ,PCREL)		(((SZ) << 1) + (PCREL))
  switch (F (fixp->fx_size, fixp->fx_pcrel))
    {
#define MAP(SZ,PCREL,TYPE)	case F(SZ,PCREL): code = (TYPE); break
      MAP (1, 0, BFD_RELOC_8);
      MAP (2, 0, BFD_RELOC_16);
      MAP (4, 0, BFD_RELOC_32);
      MAP (1, 1, BFD_RELOC_8_PCREL);
      MAP (2, 1, BFD_RELOC_16_PCREL);
      MAP (4, 1, BFD_RELOC_32_PCREL);
    default:
      abort ();
    }

  reloc = (arelent *) bfd_alloc_by_size_t (stdoutput, sizeof (arelent));
  assert (reloc != 0);
  reloc->sym_ptr_ptr = &fixp->fx_addsy->bsym;
  reloc->address = fixp->fx_frag->fr_address + fixp->fx_where;
  if (fixp->fx_pcrel)
    reloc->addend = fixp->fx_addnumber;
  else
    reloc->addend = 0;

  reloc->howto = bfd_reloc_type_lookup (stdoutput, code);
  assert (reloc->howto != 0);

  return reloc;
}

#endif /* BFD_ASSEMBLER */

/* Handle of the OPCODE hash table.  NULL means any use before
   m68k_ip_begin() will crash.  */
static struct hash_control *op_hash;

/* Assemble an m68k instruction.  */

void
m68k_ip (instring)
     char *instring;
{
  register char *p;
  register struct m68k_op *opP;
  register struct m68k_incant *opcode;
  register const char *s;
  register int tmpreg = 0, baseo = 0, outro = 0, nextword;
  char *pdot, *pdotmove;
  enum m68k_size siz1, siz2;
  char c;
  int losing;
  int opsfound;
  char *crack_operand ();
  LITTLENUM_TYPE words[6];
  LITTLENUM_TYPE *wordp;
  unsigned long ok_arch = 0;

  if (*instring == ' ')
    instring++;			/* skip leading whitespace */

  /* Scan up to end of operation-code, which MUST end in end-of-string
     or exactly 1 space. */
  pdot = 0;
  for (p = instring; *p != '\0'; p++)
    {
      if (*p == ' ')
	break;
      if (*p == '.')
	pdot = p;
    }

  if (p == instring)
    {
      the_ins.error = "No operator";
      return;
    }

  /* p now points to the end of the opcode name, probably whitespace.
     Make sure the name is null terminated by clobbering the
     whitespace, look it up in the hash table, then fix it back.
     Remove a dot, first, since the opcode tables have none.  */
  if (pdot != NULL)
    {
      for (pdotmove = pdot; pdotmove < p; pdotmove++)
	*pdotmove = pdotmove[1];
      p--;
    }

  c = *p;
  *p = '\0';
  opcode = (struct m68k_incant *) hash_find (op_hash, instring);
  *p = c;

  if (pdot != NULL)
    {
      for (pdotmove = p; pdotmove > pdot; pdotmove--)
	*pdotmove = pdotmove[-1];
      *pdot = '.';
      ++p;
    }

  if (opcode == NULL)
    {
      the_ins.error = "Unknown operator";
      return;
    }

  /* found a legitimate opcode, start matching operands */
  while (*p == ' ')
    ++p;

  if (opcode->m_operands == 0)
    {
      char *old = input_line_pointer;
      *old = '\n';
      input_line_pointer = p;
      /* Ahh - it's a motorola style psuedo op */
      mote_pseudo_table[opcode->m_opnum].poc_handler
	(mote_pseudo_table[opcode->m_opnum].poc_val);
      input_line_pointer = old;
      *old = 0;

      return;
    }

  if (flag_mri && opcode->m_opnum == 0)
    {
      /* In MRI mode, random garbage is allowed after an instruction
         which accepts no operands.  */
      the_ins.args = opcode->m_operands;
      the_ins.numargs = opcode->m_opnum;
      the_ins.numo = opcode->m_codenum;
      the_ins.opcode[0] = getone (opcode);
      the_ins.opcode[1] = gettwo (opcode);
      return;
    }

  for (opP = &the_ins.operands[0]; *p; opP++)
    {
      p = crack_operand (p, opP);

      if (opP->error)
	{
	  the_ins.error = opP->error;
	  return;
	}
    }

  opsfound = opP - &the_ins.operands[0];

  /* This ugly hack is to support the floating pt opcodes in their
     standard form.  Essentially, we fake a first enty of type COP#1 */
  if (opcode->m_operands[0] == 'I')
    {
      int n;

      for (n = opsfound; n > 0; --n)
	the_ins.operands[n] = the_ins.operands[n - 1];

      memset ((char *) (&the_ins.operands[0]), '\0',
	      sizeof (the_ins.operands[0]));
      the_ins.operands[0].mode = CONTROL;
      the_ins.operands[0].reg = m68k_float_copnum;
      opsfound++;
    }

  /* We've got the operands.  Find an opcode that'll accept them */
  for (losing = 0;;)
    {
      /* If we didn't get the right number of ops, or we have no
	 common model with this pattern then reject this pattern. */

      if (opsfound != opcode->m_opnum
	  || ((opcode->m_arch & current_architecture) == 0))
	{
	  ++losing;
	  ok_arch |= opcode->m_arch;
	}
      else
	{
	  for (s = opcode->m_operands, opP = &the_ins.operands[0];
	       *s && !losing;
	       s += 2, opP++)
	    {
	      /* Warning: this switch is huge! */
	      /* I've tried to organize the cases into this order:
		 non-alpha first, then alpha by letter.  Lower-case
		 goes directly before uppercase counterpart.  */
	      /* Code with multiple case ...: gets sorted by the lowest
		 case ... it belongs to.  I hope this makes sense.  */
	      switch (*s)
		{
		case '!':
		  switch (opP->mode)
		    {
		    case IMMED:
		    case DREG:
		    case AREG:
		    case FPREG:
		    case CONTROL:
		    case AINC:
		    case ADEC:
		    case REGLST:
		      losing++;
		      break;
		    default:
		      break;
		    }
		  break;

		case '`':
		  switch (opP->mode)
		    {
		    case IMMED:
		    case DREG:
		    case AREG:
		    case FPREG:
		    case CONTROL:
		    case AINC:
		    case REGLST:
		    case AINDR:
		      losing++;
		      break;
		    default:
		      break;
		    }
		  break;

		case '#':
		  if (opP->mode != IMMED)
		    losing++;
		  else if (s[1] == 'b'
			   && ! isvar (&opP->disp)
			   && (opP->disp.exp.X_op != O_constant
			       || ! isbyte (opP->disp.exp.X_add_number)))
		    losing++;
		  else if (s[1] == 'w'
			   && ! isvar (&opP->disp)
			   && (opP->disp.exp.X_op != O_constant
			       || ! isword (opP->disp.exp.X_add_number)))
		    losing++;
		  else if (s[1] == 'W'
			   && ! isvar (&opP->disp)
			   && (opP->disp.exp.X_op != O_constant
			       || ! issword (opP->disp.exp.X_add_number)))
		    losing++;
		  break;

		case '^':
		case 'T':
		  if (opP->mode != IMMED)
		    losing++;
		  break;

		case '$':
		  if (opP->mode == AREG
		      || opP->mode == CONTROL
		      || opP->mode == FPREG
		      || opP->mode == IMMED
		      || opP->mode == REGLST
		      || (opP->mode != ABSL
			  && (opP->reg == PC
			      || opP->reg == ZPC)))
		    losing++;
		  break;

		case '%':
		  if (opP->mode == CONTROL
		      || opP->mode == FPREG
		      || opP->mode == REGLST
		      || opP->mode == IMMED
		      || (opP->mode != ABSL
			  && (opP->reg == PC
			      || opP->reg == ZPC)))
		    losing++;
		  break;

		case '&':
		  switch (opP->mode)
		    {
		    case DREG:
		    case AREG:
		    case FPREG:
		    case CONTROL:
		    case IMMED:
		    case AINC:
		    case ADEC:
		    case REGLST:
		      losing++;
		      break;
		    case ABSL:
		      break;
		    default:
		      if (opP->reg == PC
			  || opP->reg == ZPC)
			losing++;
		      break;
		    }
		  break;

		case '*':
		  if (opP->mode == CONTROL
		      || opP->mode == FPREG
		      || opP->mode == REGLST)
		    losing++;
		  break;

		case '+':
		  if (opP->mode != AINC)
		    losing++;
		  break;

		case '-':
		  if (opP->mode != ADEC)
		    losing++;
		  break;

		case '/':
		  switch (opP->mode)
		    {
		    case AREG:
		    case CONTROL:
		    case FPREG:
		    case AINC:
		    case ADEC:
		    case IMMED:
		    case REGLST:
		      losing++;
		      break;
		    default:
		      break;
		    }
		  break;

		case ';':
		  switch (opP->mode)
		    {
		    case AREG:
		    case CONTROL:
		    case FPREG:
		    case REGLST:
		      losing++;
		      break;
		    default:
		      break;
		    }
		  break;

		case '?':
		  switch (opP->mode)
		    {
		    case AREG:
		    case CONTROL:
		    case FPREG:
		    case AINC:
		    case ADEC:
		    case IMMED:
		    case REGLST:
		      losing++;
		      break;
		    case ABSL:
		      break;
		    default:
		      if (opP->reg == PC || opP->reg == ZPC)
			losing++;
		      break;
		    }
		  break;

		case '@':
		  switch (opP->mode)
		    {
		    case AREG:
		    case CONTROL:
		    case FPREG:
		    case IMMED:
		    case REGLST:
		      losing++;
		      break;
		    default:
		      break;
		    }
		  break;

		case '~':	/* For now! (JF FOO is this right?) */
		  switch (opP->mode)
		    {
		    case DREG:
		    case AREG:
		    case CONTROL:
		    case FPREG:
		    case IMMED:
		    case REGLST:
		      losing++;
		      break;
		    case ABSL:
		      break;
		    default:
		      if (opP->reg == PC
			  || opP->reg == ZPC)
			losing++;
		      break;
		    }
		  break;

		case '3':
		  if (opP->mode != CONTROL
		      || (opP->reg != TT0 && opP->reg != TT1))
		    losing++;
		  break;

		case 'A':
		  if (opP->mode != AREG)
		    losing++;
		  break;

		case 'a':
		  if (opP->mode != AINDR)
		    ++losing;
		  break;

		case 'B':	/* FOO */
		  if (opP->mode != ABSL
		      || (flag_long_jumps
			  && strncmp (instring, "jbsr", 4) == 0))
		    losing++;
		  break;

		case 'C':
		  if (opP->mode != CONTROL || opP->reg != CCR)
		    losing++;
		  break;

		case 'd':
		  if (opP->mode != DISP
		      || opP->reg < ADDR0
		      || opP->reg > ADDR7)
		    losing++;
		  break;

		case 'D':
		  if (opP->mode != DREG)
		    losing++;
		  break;

		case 'F':
		  if (opP->mode != FPREG)
		    losing++;
		  break;

		case 'I':
		  if (opP->mode != CONTROL
		      || opP->reg < COP0
		      || opP->reg > COP7)
		    losing++;
		  break;

		case 'J':
		  if (opP->mode != CONTROL
		      || opP->reg < USP
		      || opP->reg > last_movec_reg)
		    losing++;
		  else
		    {
		      const enum m68k_register *rp;
		      for (rp = control_regs; *rp; rp++)
			if (*rp == opP->reg)
			  break;
		      if (*rp == 0)
			losing++;
		    }
		  break;

		case 'k':
		  if (opP->mode != IMMED)
		    losing++;
		  break;

		case 'l':
		case 'L':
		  if (opP->mode == DREG
		      || opP->mode == AREG
		      || opP->mode == FPREG)
		    {
		      if (s[1] == '8')
			losing++;
		      else
			{
			  switch (opP->mode)
			    {
			    case DREG:
			      opP->mask = 1 << (opP->reg - DATA0);
			      break;
			    case AREG:
			      opP->mask = 1 << (opP->reg - ADDR0 + 8);
			      break;
			    case FPREG:
			      opP->mask = 1 << (opP->reg - FP0 + 16);
			      break;
			    default:
			      abort ();
			    }
			  opP->mode = REGLST;
			}
		    }
		  else if (opP->mode == CONTROL)
		    {
		      if (s[1] != '8')
			losing++;
		      else
			{
			  switch (opP->reg)
			    {
			    case FPI:
			      opP->mask = 1 << 24;
			      break;
			    case FPS:
			      opP->mask = 1 << 25;
			      break;
			    case FPC:
			      opP->mask = 1 << 26;
			      break;
			    default:
			      losing++;
			      break;
			    }
			  opP->mode = REGLST;
			}
		    }
		  else if (opP->mode == ABSL
			   && opP->disp.size == SIZE_UNSPEC
			   && opP->disp.exp.X_op == O_constant)
		    {
		      /* This is what the MRI REG pseudo-op generates.  */
		      opP->mode = REGLST;
		      opP->mask = opP->disp.exp.X_add_number;
		    }
		  else if (opP->mode != REGLST)
		    losing++;
		  else if (s[1] == '8' && (opP->mask & 0x0ffffff) != 0)
		    losing++;
		  else if (s[1] == '3' && (opP->mask & 0x7000000) != 0)
		    losing++;
		  break;

		case 'M':
		  if (opP->mode != IMMED)
		    losing++;
		  else if (opP->disp.exp.X_op != O_constant
			   || ! issbyte (opP->disp.exp.X_add_number))
		    losing++;
		  else if (! m68k_quick
			   && instring[3] != 'q'
			   && instring[4] != 'q')
		    losing++;
		  break;

		case 'O':
		  if (opP->mode != DREG && opP->mode != IMMED)
		    losing++;
		  break;

		case 'Q':
		  if (opP->mode != IMMED)
		    losing++;
		  else if (opP->disp.exp.X_op != O_constant
			   || opP->disp.exp.X_add_number < 1
			   || opP->disp.exp.X_add_number > 8)
		    losing++;
		  else if (! m68k_quick
			   && (strncmp (instring, "add", 3) == 0
			       || strncmp (instring, "sub", 3) == 0)
			   && instring[3] != 'q')
		    losing++;
		  break;

		case 'R':
		  if (opP->mode != DREG && opP->mode != AREG)
		    losing++;
		  break;

		case 'r':
		  if (opP->mode != AINDR
		      && (opP->mode != BASE
			  || (opP->reg != 0
			      && opP->reg != ZADDR0)
			  || opP->disp.exp.X_op != O_absent
			  || ((opP->index.reg < DATA0
			       || opP->index.reg > DATA7)
			      && (opP->index.reg < ADDR0
				  || opP->index.reg > ADDR7))
			  || opP->index.size != SIZE_UNSPEC
			  || opP->index.scale != 1))
		    losing++;
		  break;

		case 's':
		  if (opP->mode != CONTROL
		      || ! (opP->reg == FPI
			    || opP->reg == FPS
			    || opP->reg == FPC))
		    losing++;
		  break;

		case 'S':
		  if (opP->mode != CONTROL || opP->reg != SR)
		    losing++;
		  break;

		case 't':
		  if (opP->mode != IMMED)
		    losing++;
		  else if (opP->disp.exp.X_op != O_constant
			   || opP->disp.exp.X_add_number < 0
			   || opP->disp.exp.X_add_number > 7)
		    losing++;
		  break;

		case 'U':
		  if (opP->mode != CONTROL || opP->reg != USP)
		    losing++;
		  break;

		  /* JF these are out of order.  We could put them
		     in order if we were willing to put up with
		     bunches of #ifdef m68851s in the code.

		     Don't forget that you need these operands
		     to use 68030 MMU instructions.  */
#ifndef NO_68851
		  /* Memory addressing mode used by pflushr */
		case '|':
		  if (opP->mode == CONTROL
		      || opP->mode == FPREG
		      || opP->mode == DREG
		      || opP->mode == AREG
		      || opP->mode == REGLST)
		    losing++;
		  /* We should accept immediate operands, but they
                     supposedly have to be quad word, and we don't
                     handle that.  I would like to see what a Motorola
                     assembler does before doing something here.  */
		  if (opP->mode == IMMED)
		    losing++;
		  break;

		case 'f':
		  if (opP->mode != CONTROL
		      || (opP->reg != SFC && opP->reg != DFC))
		    losing++;
		  break;

		case '0':
		  if (opP->mode != CONTROL || opP->reg != TC)
		    losing++;
		  break;

		case '1':
		  if (opP->mode != CONTROL || opP->reg != AC)
		    losing++;
		  break;

		case '2':
		  if (opP->mode != CONTROL
		      || (opP->reg != CAL
			  && opP->reg != VAL
			  && opP->reg != SCC))
		    losing++;
		  break;

		case 'V':
		  if (opP->mode != CONTROL
		      || opP->reg != VAL)
		    losing++;
		  break;

		case 'W':
		  if (opP->mode != CONTROL
		      || (opP->reg != DRP
			  && opP->reg != SRP
			  && opP->reg != CRP))
		    losing++;
		  break;

		case 'X':
		  if (opP->mode != CONTROL
		      || (!(opP->reg >= BAD && opP->reg <= BAD + 7)
			  && !(opP->reg >= BAC && opP->reg <= BAC + 7)))
		    losing++;
		  break;

		case 'Y':
		  if (opP->mode != CONTROL || opP->reg != PSR)
		    losing++;
		  break;

		case 'Z':
		  if (opP->mode != CONTROL || opP->reg != PCSR)
		    losing++;
		  break;
#endif
		case 'c':
		  if (opP->mode != CONTROL
		      || (opP->reg != NC
			  && opP->reg != IC
			  && opP->reg != DC
			  && opP->reg != BC))
		    {
		      losing++;
		    }		/* not a cache specifier. */
		  break;

		case '_':
		  if (opP->mode != ABSL)
		    ++losing;
		  break;

		default:
		  abort ();
		}		/* switch on type of operand */

	      if (losing)
		break;
	    }			/* for each operand */
	}			/* if immediately wrong */

      if (!losing)
	{
	  break;
	}			/* got it. */

      opcode = opcode->m_next;

      if (!opcode)
	{
	  if (ok_arch
	      && !(ok_arch & current_architecture))
	    {
	      char buf[200], *cp;
	      int len;
	      strcpy (buf,
		      "invalid instruction for this architecture; needs ");
	      cp = buf + strlen (buf);
	      switch (ok_arch)
		{
		case mfloat:
		  strcpy (cp, "fpu (68040, 68060 or 68881/68882)");
		  break;
		case mmmu:
		  strcpy (cp, "mmu (68030 or 68851)");
		  break;
		case m68020up:
		  strcpy (cp, "68020 or higher");
		  break;
		case m68000up:
		  strcpy (cp, "68000 or higher");
		  break;
		case m68010up:
		  strcpy (cp, "68010 or higher");
		  break;
		default:
		  {
		    int got_one = 0, idx;
		    for (idx = 0; idx < sizeof (archs) / sizeof (archs[0]);
			 idx++)
		      {
			if (archs[idx].arch & ok_arch)
			  {
			    if (got_one)
			      {
				strcpy (cp, " or ");
				cp += strlen (cp);
			      }
			    got_one = 1;
			    strcpy (cp, archs[idx].name);
			    cp += strlen (cp);
			  }
		      }
		  }
		}
	      len = cp - buf + 1;
	      cp = malloc (len);
	      strcpy (cp, buf);
	      the_ins.error = cp;
	    }
	  else
	    the_ins.error = "operands mismatch";
	  return;
	}			/* Fell off the end */

      losing = 0;
    }

  /* now assemble it */

  the_ins.args = opcode->m_operands;
  the_ins.numargs = opcode->m_opnum;
  the_ins.numo = opcode->m_codenum;
  the_ins.opcode[0] = getone (opcode);
  the_ins.opcode[1] = gettwo (opcode);

  for (s = the_ins.args, opP = &the_ins.operands[0]; *s; s += 2, opP++)
    {
      /* This switch is a doozy.
       Watch the first step; its a big one! */
      switch (s[0])
	{

	case '*':
	case '~':
	case '%':
	case ';':
	case '@':
	case '!':
	case '&':
	case '$':
	case '?':
	case '/':
	case '`':
#ifndef NO_68851
	case '|':
#endif
	  switch (opP->mode)
	    {
	    case IMMED:
	      tmpreg = 0x3c;	/* 7.4 */
	      if (strchr ("bwl", s[1]))
		nextword = get_num (&opP->disp, 80);
	      else
		nextword = get_num (&opP->disp, 0);
	      if (isvar (&opP->disp))
		add_fix (s[1], &opP->disp, 0, 0);
	      switch (s[1])
		{
		case 'b':
		  if (!isbyte (nextword))
		    opP->error = "operand out of range";
		  addword (nextword);
		  baseo = 0;
		  break;
		case 'w':
		  if (!isword (nextword))
		    opP->error = "operand out of range";
		  addword (nextword);
		  baseo = 0;
		  break;
		case 'W':
		  if (!issword (nextword))
		    opP->error = "operand out of range";
		  addword (nextword);
		  baseo = 0;
		  break;
		case 'l':
		  addword (nextword >> 16);
		  addword (nextword);
		  baseo = 0;
		  break;

		case 'f':
		  baseo = 2;
		  outro = 8;
		  break;
		case 'F':
		  baseo = 4;
		  outro = 11;
		  break;
		case 'x':
		  baseo = 6;
		  outro = 15;
		  break;
		case 'p':
		  baseo = 6;
		  outro = -1;
		  break;
		default:
		  abort ();
		}
	      if (!baseo)
		break;

	      /* We gotta put out some float */
	      if (op (&opP->disp) != O_big)
		{
		  valueT val;
		  int gencnt;

		  /* Can other cases happen here?  */
		  if (op (&opP->disp) != O_constant)
		    abort ();

		  val = (valueT) offs (&opP->disp);
		  gencnt = 0;
		  do
		    {
		      generic_bignum[gencnt] = (LITTLENUM_TYPE) val;
		      val >>= LITTLENUM_NUMBER_OF_BITS;
		      ++gencnt;
		    }
		  while (val != 0);
		  offs (&opP->disp) = gencnt;
		}
	      if (offs (&opP->disp) > 0)
		{
		  if (offs (&opP->disp) > baseo)
		    {
		      as_warn ("Bignum too big for %c format; truncated",
			       s[1]);
		      offs (&opP->disp) = baseo;
		    }
		  baseo -= offs (&opP->disp);
		  while (baseo--)
		    addword (0);
		  for (wordp = generic_bignum + offs (&opP->disp) - 1;
		       offs (&opP->disp)--;
		       --wordp)
		    addword (*wordp);
		  break;
		}
	      gen_to_words (words, baseo, (long) outro);
	      for (wordp = words; baseo--; wordp++)
		addword (*wordp);
	      break;
	    case DREG:
	      tmpreg = opP->reg - DATA;	/* 0.dreg */
	      break;
	    case AREG:
	      tmpreg = 0x08 + opP->reg - ADDR;	/* 1.areg */
	      break;
	    case AINDR:
	      tmpreg = 0x10 + opP->reg - ADDR;	/* 2.areg */
	      break;
	    case ADEC:
	      tmpreg = 0x20 + opP->reg - ADDR;	/* 4.areg */
	      break;
	    case AINC:
	      tmpreg = 0x18 + opP->reg - ADDR;	/* 3.areg */
	      break;
	    case DISP:

	      nextword = get_num (&opP->disp, 80);

	      if (opP->reg == PC
		  && ! isvar (&opP->disp)
		  && m68k_abspcadd)
		{
		  opP->disp.exp.X_op = O_symbol;
#ifndef BFD_ASSEMBLER
		  opP->disp.exp.X_add_symbol = &abs_symbol;
#else
		  opP->disp.exp.X_add_symbol =
		    section_symbol (absolute_section);
#endif
		}

	      /* Force into index mode.  Hope this works */

	      /* We do the first bit for 32-bit displacements, and the
		 second bit for 16 bit ones.  It is possible that we
		 should make the default be WORD instead of LONG, but
		 I think that'd break GCC, so we put up with a little
		 inefficiency for the sake of working output.  */

	      if (!issword (nextword)
		  || (isvar (&opP->disp)
		      && ((opP->disp.size == SIZE_UNSPEC
			   && flag_short_refs == 0
			   && cpu_of_arch (current_architecture) >= m68020)
			  || opP->disp.size == SIZE_LONG)))
		{
		  if (opP->reg == PC)
		    tmpreg = 0x3B;	/* 7.3 */
		  else
		    tmpreg = 0x30 + opP->reg - ADDR;	/* 6.areg */
		  if (isvar (&opP->disp))
		    {
		      if (opP->reg == PC)
			{
#if 0
			  addword (0x0170);
			  add_fix ('l', &opP->disp, 1, 2);
			  addword (0), addword (0);
#else
			  add_frag (adds (&opP->disp),
				    offs (&opP->disp),
				    TAB (PCLEA, SZ_UNDEF));
#endif
			  break;
			}
		      else
			{
			  addword (0x0170);
			  add_fix ('l', &opP->disp, 0, 0);
			}
		    }
		  else
		    addword (0x0170);
		  addword (nextword >> 16);
		}
	      else
		{
		  if (opP->reg == PC)
		    tmpreg = 0x3A;	/* 7.2 */
		  else
		    tmpreg = 0x28 + opP->reg - ADDR;	/* 5.areg */

		  if (isvar (&opP->disp))
		    {
		      if (opP->reg == PC)
			{
			  add_fix ('w', &opP->disp, 1, 0);
			}
		      else
			add_fix ('w', &opP->disp, 0, 0);
		    }
		}
	      addword (nextword);
	      break;

	    case POST:
	    case PRE:
	    case BASE:
	      nextword = 0;
	      baseo = get_num (&opP->disp, 80);
	      if (opP->mode == POST || opP->mode == PRE)
		outro = get_num (&opP->odisp, 80);
	      /* Figure out the `addressing mode'.
		 Also turn on the BASE_DISABLE bit, if needed.  */
	      if (opP->reg == PC || opP->reg == ZPC)
		{
		  tmpreg = 0x3b;	/* 7.3 */
		  if (opP->reg == ZPC)
		    nextword |= 0x80;
		}
	      else if (opP->reg == 0)
		{
		  nextword |= 0x80;
		  tmpreg = 0x30;	/* 6.garbage */
		}
	      else if (opP->reg >= ZADDR0 && opP->reg <= ZADDR7)
		{
		  nextword |= 0x80;
		  tmpreg = 0x30 + opP->reg - ZADDR0;
		}
	      else
		tmpreg = 0x30 + opP->reg - ADDR;	/* 6.areg */

	      siz1 = opP->disp.size;
	      if (opP->mode == POST || opP->mode == PRE)
		siz2 = opP->odisp.size;
	      else
		siz2 = SIZE_UNSPEC;

	      /* Index register stuff */
	      if (opP->index.reg != 0
		  && opP->index.reg >= DATA
		  && opP->index.reg <= ADDR7)
		{
		  nextword |= (opP->index.reg - DATA) << 12;

		  if (opP->index.size == SIZE_UNSPEC
		      || opP->index.size == SIZE_LONG)
		    nextword |= 0x800;

		  if (cpu_of_arch (current_architecture) < m68020)
		    {
		      if (opP->index.scale != 1)
			{
			  opP->error =
			    "scale factor invalid on this architecture; needs 68020 or higher";
			}
		    }

		  switch (opP->index.scale)
		    {
		    case 1:
		      break;
		    case 2:
		      nextword |= 0x200;
		      break;
		    case 4:
		      nextword |= 0x400;
		      break;
		    case 8:
		      nextword |= 0x600;
		      break;
		    default:
		      abort ();
		    }
		  /* IF its simple,
		     GET US OUT OF HERE! */

		  /* Must be INDEX, with an index register.  Address
		     register cannot be ZERO-PC, and either :b was
		     forced, or we know it will fit.  For a 68000 or
		     68010, force this mode anyways, because the
		     larger modes aren't supported.  */
		  if (opP->mode == BASE
		      && ((opP->reg >= ADDR0
			   && opP->reg <= ADDR7)
			  || opP->reg == PC))
		    {
		      if (siz1 == SIZE_BYTE
			  || cpu_of_arch (current_architecture) < m68020
			  || (siz1 == SIZE_UNSPEC
			      && ! isvar (&opP->disp)
			      && issbyte (baseo)))
			{
 			  nextword += baseo & 0xff;
 			  addword (nextword);
 			  if (isvar (&opP->disp))
			    {
			      /* Do a byte relocation.  If it doesn't
				 fit (possible on m68000) let the
				 fixup processing complain later.  */
			      if (opP->reg == PC)
				add_fix ('B', &opP->disp, 1, 1);
			      else
				add_fix ('B', &opP->disp, 0, 0);
			    }
			  else if (siz1 != SIZE_BYTE)
			    {
			      if (siz1 != SIZE_UNSPEC)
				as_warn ("Forcing byte displacement");
			      if (! issbyte (baseo))
				opP->error = "byte displacement out of range";
			    }

			  break;
			}
		      else if (siz1 == SIZE_UNSPEC
			       && opP->reg == PC
			       && isvar (&opP->disp)
			       && subs (&opP->disp) == NULL)
			{
 			  nextword += baseo & 0xff;
 			  addword (nextword);
 			  add_frag (adds (&opP->disp), offs (&opP->disp),
 				    TAB (PCINDEX, SZ_UNDEF));

			  break;
 			}
		    }
		}
	      else
		{
		  nextword |= 0x40;	/* No index reg */
		  if (opP->index.reg >= ZDATA0
		      && opP->index.reg <= ZDATA7)
		    nextword |= (opP->index.reg - ZDATA0) << 12;
		  else if (opP->index.reg >= ZADDR0
			   || opP->index.reg <= ZADDR7)
		    nextword |= (opP->index.reg - ZADDR0 + 8) << 12;
		}

	      /* It isn't simple.  */

	      if (cpu_of_arch (current_architecture) < m68020)
		opP->error =
		  "invalid operand mode for this architecture; needs 68020 or higher";

	      nextword |= 0x100;
	      /* If the guy specified a width, we assume that it is
		 wide enough.  Maybe it isn't.  If so, we lose.  */
	      switch (siz1)
		{
		case SIZE_UNSPEC:
		  if (isvar (&opP->disp)
		      ? m68k_rel32
		      : ! issword (baseo))
		    {
		      siz1 = SIZE_LONG;
		      nextword |= 0x30;
		    }
		  else if (! isvar (&opP->disp) && baseo == 0)
		    nextword |= 0x10;
		  else
		    {
		      nextword |= 0x20;
		      siz1 = SIZE_WORD;
		    }
		  break;
		case SIZE_BYTE:
		  as_warn (":b not permitted; defaulting to :w");
		  /* Fall through.  */
		case SIZE_WORD:
		  nextword |= 0x20;
		  break;
		case SIZE_LONG:
		  nextword |= 0x30;
		  break;
		}

	      /* Figure out innner displacement stuff */
	      if (opP->mode == POST || opP->mode == PRE)
		{
		  switch (siz2)
		    {
		    case SIZE_UNSPEC:
		      if (isvar (&opP->odisp)
			  ? m68k_rel32
			  : ! issword (outro))
			{
			  siz2 = SIZE_LONG;
			  nextword |= 0x3;
			}
		      else if (! isvar (&opP->odisp) && outro == 0)
			nextword |= 0x1;
		      else
			{
			  nextword |= 0x2;
			  siz2 = SIZE_WORD;
			}
		      break;
		    case 1:
		      as_warn (":b not permitted; defaulting to :w");
		      /* Fall through.  */
		    case 2:
		      nextword |= 0x2;
		      break;
		    case 3:
		      nextword |= 0x3;
		      break;
		    }
		  if (opP->mode == POST
		      && (nextword & 0x40) == 0)
		    nextword |= 0x04;
		}
	      addword (nextword);

	      if (siz1 != SIZE_UNSPEC && isvar (&opP->disp))
		{
		  if (opP->reg == PC || opP->reg == ZPC)
		    add_fix (siz1 == SIZE_LONG ? 'l' : 'w', &opP->disp, 1, 2);
		  else
		    add_fix (siz1 == SIZE_LONG ? 'l' : 'w', &opP->disp, 0, 0);
		}
	      if (siz1 == SIZE_LONG)
		addword (baseo >> 16);
	      if (siz1 != SIZE_UNSPEC)
		addword (baseo);

	      if (siz2 != SIZE_UNSPEC && isvar (&opP->odisp))
		add_fix (siz2 == SIZE_LONG ? 'l' : 'w', &opP->odisp, 0, 0);
	      if (siz2 == SIZE_LONG)
		addword (outro >> 16);
	      if (siz2 != SIZE_UNSPEC)
		addword (outro);

	      break;

	    case ABSL:
	      nextword = get_num (&opP->disp, 80);
	      switch (opP->disp.size)
		{
		default:
		  abort ();
		case SIZE_UNSPEC:
		  if (!isvar (&opP->disp) && issword (offs (&opP->disp)))
		    {
		      tmpreg = 0x38;	/* 7.0 */
		      addword (nextword);
		      break;
		    }
		  /* Don't generate pc relative code on 68010 and
		     68000.  */
		  if (isvar (&opP->disp)
		      && !subs (&opP->disp)
		      && adds (&opP->disp)
		      && S_GET_SEGMENT (adds (&opP->disp)) == now_seg
		      && cpu_of_arch (current_architecture) >= m68020
		      && !flag_long_jumps
		      && !strchr ("~%&$?", s[0]))
		    {
		      tmpreg = 0x3A;	/* 7.2 */
		      add_frag (adds (&opP->disp),
				offs (&opP->disp),
				TAB (PCREL, SZ_UNDEF));
		      break;
		    }
		  /* Fall through into long */
		case SIZE_LONG:
		  if (isvar (&opP->disp))
		    add_fix ('l', &opP->disp, 0, 0);

		  tmpreg = 0x39;/* 7.1 mode */
		  addword (nextword >> 16);
		  addword (nextword);
		  break;

		case SIZE_WORD:	/* Word */
		  if (isvar (&opP->disp))
		    add_fix ('w', &opP->disp, 0, 0);

		  tmpreg = 0x38;/* 7.0 mode */
		  addword (nextword);
		  break;
		}
	      break;
	    case CONTROL:
	    case FPREG:
	    default:
	      as_bad ("unknown/incorrect operand");
	      /* abort(); */
	    }
	  install_gen_operand (s[1], tmpreg);
	  break;

	case '#':
	case '^':
	  switch (s[1])
	    {			/* JF: I hate floating point! */
	    case 'j':
	      tmpreg = 70;
	      break;
	    case '8':
	      tmpreg = 20;
	      break;
	    case 'C':
	      tmpreg = 50;
	      break;
	    case '3':
	    default:
	      tmpreg = 80;
	      break;
	    }
	  tmpreg = get_num (&opP->disp, tmpreg);
	  if (isvar (&opP->disp))
	    add_fix (s[1], &opP->disp, 0, 0);
	  switch (s[1])
	    {
	    case 'b':		/* Danger:  These do no check for
				   certain types of overflow.
				   user beware! */
	      if (!isbyte (tmpreg))
		opP->error = "out of range";
	      insop (tmpreg, opcode);
	      if (isvar (&opP->disp))
		the_ins.reloc[the_ins.nrel - 1].n = (opcode->m_codenum) * 2;
	      break;
	    case 'w':
	      if (!isword (tmpreg))
		opP->error = "out of range";
	      insop (tmpreg, opcode);
	      if (isvar (&opP->disp))
		the_ins.reloc[the_ins.nrel - 1].n = (opcode->m_codenum) * 2;
	      break;
	    case 'W':
	      if (!issword (tmpreg))
		opP->error = "out of range";
	      insop (tmpreg, opcode);
	      if (isvar (&opP->disp))
		the_ins.reloc[the_ins.nrel - 1].n = (opcode->m_codenum) * 2;
	      break;
	    case 'l':
	      /* Because of the way insop works, we put these two out
		 backwards.  */
	      insop (tmpreg, opcode);
	      insop (tmpreg >> 16, opcode);
	      if (isvar (&opP->disp))
		the_ins.reloc[the_ins.nrel - 1].n = (opcode->m_codenum) * 2;
	      break;
	    case '3':
	      tmpreg &= 0xFF;
	    case '8':
	    case 'C':
	      install_operand (s[1], tmpreg);
	      break;
	    default:
	      abort ();
	    }
	  break;

	case '+':
	case '-':
	case 'A':
	case 'a':
	  install_operand (s[1], opP->reg - ADDR);
	  break;

	case 'B':
	  tmpreg = get_num (&opP->disp, 80);
	  switch (s[1])
	    {
	    case 'B':
	      /* The pc_fix argument winds up in fx_pcrel_adjust,
                 which is a char, and may therefore be unsigned.  We
                 want to pass -1, but we pass 64 instead, and convert
                 back in md_pcrel_from.  */
	      add_fix ('B', &opP->disp, 1, 64);
	      break;
	    case 'W':
	      add_fix ('w', &opP->disp, 1, 0);
	      addword (0);
	      break;
	    case 'L':
	    long_branch:
	      if (cpu_of_arch (current_architecture) < m68020)
		as_warn ("Can't use long branches on 68000/68010");
	      the_ins.opcode[the_ins.numo - 1] |= 0xff;
	      add_fix ('l', &opP->disp, 1, 0);
	      addword (0);
	      addword (0);
	      break;
	    case 'g':
	      if (subs (&opP->disp))	/* We can't relax it */
		goto long_branch;

	      /* This could either be a symbol, or an absolute
		 address.  No matter, the frag hacking will finger it
		 out.  Not quite: it can't switch from BRANCH to
		 BCC68000 for the case where opnd is absolute (it
		 needs to use the 68000 hack since no conditional abs
		 jumps).  */
	      if (((cpu_of_arch (current_architecture) < m68020)
		   || (0 == adds (&opP->disp)))
		  && (the_ins.opcode[0] >= 0x6200)
		  && (the_ins.opcode[0] <= 0x6f00))
		add_frag (adds (&opP->disp), offs (&opP->disp),
			  TAB (BCC68000, SZ_UNDEF));
	      else
		add_frag (adds (&opP->disp), offs (&opP->disp),
			  TAB (ABRANCH, SZ_UNDEF));
	      break;
	    case 'w':
	      if (isvar (&opP->disp))
		{
#if 1
		  /* check for DBcc instruction */
		  if ((the_ins.opcode[0] & 0xf0f8) == 0x50c8)
		    {
		      /* size varies if patch */
		      /* needed for long form */
		      add_frag (adds (&opP->disp), offs (&opP->disp),
				TAB (DBCC, SZ_UNDEF));
		      break;
		    }
#endif
		  add_fix ('w', &opP->disp, 1, 0);
		}
	      addword (0);
	      break;
	    case 'C':		/* Fixed size LONG coproc branches */
	      add_fix ('l', &opP->disp, 1, 0);
	      addword (0);
	      addword (0);
	      break;
	    case 'c':		/* Var size Coprocesssor branches */
	      if (subs (&opP->disp))
		{
		  add_fix ('l', &opP->disp, 1, 0);
		  add_frag ((symbolS *) 0, (long) 0, TAB (FBRANCH, LONG));
		}
	      else if (adds (&opP->disp))
		add_frag (adds (&opP->disp), offs (&opP->disp),
			  TAB (FBRANCH, SZ_UNDEF));
	      else
		{
		  /* add_frag((symbolS *) 0, offs(&opP->disp),
		     TAB(FBRANCH,SHORT)); */
		  the_ins.opcode[the_ins.numo - 1] |= 0x40;
		  add_fix ('l', &opP->disp, 1, 0);
		  addword (0);
		  addword (0);
		}
	      break;
	    default:
	      abort ();
	    }
	  break;

	case 'C':		/* Ignore it */
	  break;

	case 'd':		/* JF this is a kludge */
	  install_operand ('s', opP->reg - ADDR);
	  tmpreg = get_num (&opP->disp, 80);
	  if (!issword (tmpreg))
	    {
	      as_warn ("Expression out of range, using 0");
	      tmpreg = 0;
	    }
	  addword (tmpreg);
	  break;

	case 'D':
	  install_operand (s[1], opP->reg - DATA);
	  break;

	case 'F':
	  install_operand (s[1], opP->reg - FP0);
	  break;

	case 'I':
	  tmpreg = opP->reg - COP0;
	  install_operand (s[1], tmpreg);
	  break;

	case 'J':		/* JF foo */
	  switch (opP->reg)
	    {
	    case SFC:
	      tmpreg = 0x000;
	      break;
	    case DFC:
	      tmpreg = 0x001;
	      break;
	    case CACR:
	      tmpreg = 0x002;
	      break;
	    case TC:
	      tmpreg = 0x003;
	      break;
	    case ITT0:
	      tmpreg = 0x004;
	      break;
	    case ITT1:
	      tmpreg = 0x005;
	      break;
	    case DTT0:
	      tmpreg = 0x006;
	      break;
	    case DTT1:
	      tmpreg = 0x007;
	      break;
	    case BUSCR:
	      tmpreg = 0x008;
	      break;

	    case USP:
	      tmpreg = 0x800;
	      break;
	    case VBR:
	      tmpreg = 0x801;
	      break;
	    case CAAR:
	      tmpreg = 0x802;
	      break;
	    case MSP:
	      tmpreg = 0x803;
	      break;
	    case ISP:
	      tmpreg = 0x804;
	      break;
	    case MMUSR:
	      tmpreg = 0x805;
	      break;
	    case URP:
	      tmpreg = 0x806;
	      break;
	    case SRP:
	      tmpreg = 0x807;
	      break;
	    case PCR:
	      tmpreg = 0x808;
	      break;
	    default:
	      abort ();
	    }
	  install_operand (s[1], tmpreg);
	  break;

	case 'k':
	  tmpreg = get_num (&opP->disp, 55);
	  install_operand (s[1], tmpreg & 0x7f);
	  break;

	case 'l':
	  tmpreg = opP->mask;
	  if (s[1] == 'w')
	    {
	      if (tmpreg & 0x7FF0000)
		as_bad ("Floating point register in register list");
	      insop (reverse_16_bits (tmpreg), opcode);
	    }
	  else
	    {
	      if (tmpreg & 0x700FFFF)
		as_bad ("Wrong register in floating-point reglist");
	      install_operand (s[1], reverse_8_bits (tmpreg >> 16));
	    }
	  break;

	case 'L':
	  tmpreg = opP->mask;
	  if (s[1] == 'w')
	    {
	      if (tmpreg & 0x7FF0000)
		as_bad ("Floating point register in register list");
	      insop (tmpreg, opcode);
	    }
	  else if (s[1] == '8')
	    {
	      if (tmpreg & 0x0FFFFFF)
		as_bad ("incorrect register in reglist");
	      install_operand (s[1], tmpreg >> 24);
	    }
	  else
	    {
	      if (tmpreg & 0x700FFFF)
		as_bad ("wrong register in floating-point reglist");
	      else
		install_operand (s[1], tmpreg >> 16);
	    }
	  break;

	case 'M':
	  install_operand (s[1], get_num (&opP->disp, 60));
	  break;

	case 'O':
	  tmpreg = ((opP->mode == DREG)
		    ? 0x20 + opP->reg - DATA
		    : (get_num (&opP->disp, 40) & 0x1F));
	  install_operand (s[1], tmpreg);
	  break;

	case 'Q':
	  tmpreg = get_num (&opP->disp, 10);
	  if (tmpreg == 8)
	    tmpreg = 0;
	  install_operand (s[1], tmpreg);
	  break;

	case 'R':
	  /* This depends on the fact that ADDR registers are eight
	     more than their corresponding DATA regs, so the result
	     will have the ADDR_REG bit set */
	  install_operand (s[1], opP->reg - DATA);
	  break;

	case 'r':
	  if (opP->mode == AINDR)
	    install_operand (s[1], opP->reg - DATA);
	  else
	    install_operand (s[1], opP->index.reg - DATA);
	  break;

	case 's':
	  if (opP->reg == FPI)
	    tmpreg = 0x1;
	  else if (opP->reg == FPS)
	    tmpreg = 0x2;
	  else if (opP->reg == FPC)
	    tmpreg = 0x4;
	  else
	    abort ();
	  install_operand (s[1], tmpreg);
	  break;

	case 'S':		/* Ignore it */
	  break;

	case 'T':
	  install_operand (s[1], get_num (&opP->disp, 30));
	  break;

	case 'U':		/* Ignore it */
	  break;

	case 'c':
	  switch (opP->reg)
	    {
	    case NC:
	      tmpreg = 0;
	      break;
	    case DC:
	      tmpreg = 1;
	      break;
	    case IC:
	      tmpreg = 2;
	      break;
	    case BC:
	      tmpreg = 3;
	      break;
	    default:
	      as_fatal ("failed sanity check");
	    }			/* switch on cache token */
	  install_operand (s[1], tmpreg);
	  break;
#ifndef NO_68851
	  /* JF: These are out of order, I fear. */
	case 'f':
	  switch (opP->reg)
	    {
	    case SFC:
	      tmpreg = 0;
	      break;
	    case DFC:
	      tmpreg = 1;
	      break;
	    default:
	      abort ();
	    }
	  install_operand (s[1], tmpreg);
	  break;

	case '0':
	case '1':
	case '2':
	  switch (opP->reg)
	    {
	    case TC:
	      tmpreg = 0;
	      break;
	    case CAL:
	      tmpreg = 4;
	      break;
	    case VAL:
	      tmpreg = 5;
	      break;
	    case SCC:
	      tmpreg = 6;
	      break;
	    case AC:
	      tmpreg = 7;
	      break;
	    default:
	      abort ();
	    }
	  install_operand (s[1], tmpreg);
	  break;

	case 'V':
	  if (opP->reg == VAL)
	    break;
	  abort ();

	case 'W':
	  switch (opP->reg)
	    {
	    case DRP:
	      tmpreg = 1;
	      break;
	    case SRP:
	      tmpreg = 2;
	      break;
	    case CRP:
	      tmpreg = 3;
	      break;
	    default:
	      abort ();
	    }
	  install_operand (s[1], tmpreg);
	  break;

	case 'X':
	  switch (opP->reg)
	    {
	    case BAD:
	    case BAD + 1:
	    case BAD + 2:
	    case BAD + 3:
	    case BAD + 4:
	    case BAD + 5:
	    case BAD + 6:
	    case BAD + 7:
	      tmpreg = (4 << 10) | ((opP->reg - BAD) << 2);
	      break;

	    case BAC:
	    case BAC + 1:
	    case BAC + 2:
	    case BAC + 3:
	    case BAC + 4:
	    case BAC + 5:
	    case BAC + 6:
	    case BAC + 7:
	      tmpreg = (5 << 10) | ((opP->reg - BAC) << 2);
	      break;

	    default:
	      abort ();
	    }
	  install_operand (s[1], tmpreg);
	  break;
	case 'Y':
	  know (opP->reg == PSR);
	  break;
	case 'Z':
	  know (opP->reg == PCSR);
	  break;
#endif /* m68851 */
	case '3':
	  switch (opP->reg)
	    {
	    case TT0:
	      tmpreg = 2;
	      break;
	    case TT1:
	      tmpreg = 3;
	      break;
	    default:
	      abort ();
	    }
	  install_operand (s[1], tmpreg);
	  break;
	case 't':
	  tmpreg = get_num (&opP->disp, 20);
	  install_operand (s[1], tmpreg);
	  break;
	case '_':	/* used only for move16 absolute 32-bit address */
	  tmpreg = get_num (&opP->disp, 80);
	  addword (tmpreg >> 16);
	  addword (tmpreg & 0xFFFF);
	  break;
	default:
	  abort ();
	}
    }

  /* By the time whe get here (FINALLY) the_ins contains the complete
     instruction, ready to be emitted. . . */
}

static int
reverse_16_bits (in)
     int in;
{
  int out = 0;
  int n;

  static int mask[16] =
  {
    0x0001, 0x0002, 0x0004, 0x0008, 0x0010, 0x0020, 0x0040, 0x0080,
    0x0100, 0x0200, 0x0400, 0x0800, 0x1000, 0x2000, 0x4000, 0x8000
  };
  for (n = 0; n < 16; n++)
    {
      if (in & mask[n])
	out |= mask[15 - n];
    }
  return out;
}				/* reverse_16_bits() */

static int
reverse_8_bits (in)
     int in;
{
  int out = 0;
  int n;

  static int mask[8] =
  {
    0x0001, 0x0002, 0x0004, 0x0008, 0x0010, 0x0020, 0x0040, 0x0080,
  };

  for (n = 0; n < 8; n++)
    {
      if (in & mask[n])
	out |= mask[7 - n];
    }
  return out;
}				/* reverse_8_bits() */

/* Cause an extra frag to be generated here, inserting up to 10 bytes
   (that value is chosen in the frag_var call in md_assemble).  TYPE
   is the subtype of the frag to be generated; its primary type is
   rs_machine_dependent.

   The TYPE parameter is also used by md_convert_frag_1 and
   md_estimate_size_before_relax.  The appropriate type of fixup will
   be emitted by md_convert_frag_1.

   ADD becomes the FR_SYMBOL field of the frag, and OFF the FR_OFFSET.  */
static void
install_operand (mode, val)
     int mode;
     int val;
{
  switch (mode)
    {
    case 's':
      the_ins.opcode[0] |= val & 0xFF;	/* JF FF is for M kludge */
      break;
    case 'd':
      the_ins.opcode[0] |= val << 9;
      break;
    case '1':
      the_ins.opcode[1] |= val << 12;
      break;
    case '2':
      the_ins.opcode[1] |= val << 6;
      break;
    case '3':
      the_ins.opcode[1] |= val;
      break;
    case '4':
      the_ins.opcode[2] |= val << 12;
      break;
    case '5':
      the_ins.opcode[2] |= val << 6;
      break;
    case '6':
      /* DANGER!  This is a hack to force cas2l and cas2w cmds to be
	 three words long! */
      the_ins.numo++;
      the_ins.opcode[2] |= val;
      break;
    case '7':
      the_ins.opcode[1] |= val << 7;
      break;
    case '8':
      the_ins.opcode[1] |= val << 10;
      break;
#ifndef NO_68851
    case '9':
      the_ins.opcode[1] |= val << 5;
      break;
#endif

    case 't':
      the_ins.opcode[1] |= (val << 10) | (val << 7);
      break;
    case 'D':
      the_ins.opcode[1] |= (val << 12) | val;
      break;
    case 'g':
      the_ins.opcode[0] |= val = 0xff;
      break;
    case 'i':
      the_ins.opcode[0] |= val << 9;
      break;
    case 'C':
      the_ins.opcode[1] |= val;
      break;
    case 'j':
      the_ins.opcode[1] |= val;
      the_ins.numo++;		/* What a hack */
      break;
    case 'k':
      the_ins.opcode[1] |= val << 4;
      break;
    case 'b':
    case 'w':
    case 'W':
    case 'l':
      break;
    case 'e':
      the_ins.opcode[0] |= (val << 6);
      break;
    case 'L':
      the_ins.opcode[1] = (val >> 16);
      the_ins.opcode[2] = val & 0xffff;
      break;
    case 'c':
    default:
      as_fatal ("failed sanity check.");
    }
}				/* install_operand() */

static void
install_gen_operand (mode, val)
     int mode;
     int val;
{
  switch (mode)
    {
    case 's':
      the_ins.opcode[0] |= val;
      break;
    case 'd':
      /* This is a kludge!!! */
      the_ins.opcode[0] |= (val & 0x07) << 9 | (val & 0x38) << 3;
      break;
    case 'b':
    case 'w':
    case 'l':
    case 'f':
    case 'F':
    case 'x':
    case 'p':
      the_ins.opcode[0] |= val;
      break;
      /* more stuff goes here */
    default:
      as_fatal ("failed sanity check.");
    }
}				/* install_gen_operand() */

/*
 * verify that we have some number of paren pairs, do m68k_ip_op(), and
 * then deal with the bitfield hack.
 */

static char *
crack_operand (str, opP)
     register char *str;
     register struct m68k_op *opP;
{
  register int parens;
  register int c;
  register char *beg_str;
  int inquote = 0;

  if (!str)
    {
      return str;
    }
  beg_str = str;
  for (parens = 0; *str && (parens > 0 || inquote || notend (str)); str++)
    {
      if (! inquote)
	{
	  if (*str == '(')
	    parens++;
	  else if (*str == ')')
	    {
	      if (!parens)
		{			/* ERROR */
		  opP->error = "Extra )";
		  return str;
		}
	      --parens;
	    }
	}
      if (flag_mri && *str == '\'')
	inquote = ! inquote;
    }
  if (!*str && parens)
    {				/* ERROR */
      opP->error = "Missing )";
      return str;
    }
  c = *str;
  *str = '\0';
  if (m68k_ip_op (beg_str, opP) != 0)
    {
      *str = c;
      return str;
    }
  *str = c;
  if (c == '}')
    c = *++str;			/* JF bitfield hack */
  if (c)
    {
      c = *++str;
      if (!c)
	as_bad ("Missing operand");
    }
  return str;
}

/* This is the guts of the machine-dependent assembler.  STR points to a
   machine dependent instruction.  This function is supposed to emit
   the frags/bytes it assembles to.
   */

void
insert_reg (regname, regnum)
     char *regname;
     int regnum;
{
  char buf[100];
  int i;

#ifdef REGISTER_PREFIX
  if (!flag_reg_prefix_optional)
    {
      buf[0] = REGISTER_PREFIX;
      strcpy (buf + 1, regname);
      regname = buf;
    }
#endif

  symbol_table_insert (symbol_new (regname, reg_section, regnum,
				   &zero_address_frag));

  for (i = 0; regname[i]; i++)
    buf[i] = islower (regname[i]) ? toupper (regname[i]) : regname[i];
  buf[i] = '\0';

  symbol_table_insert (symbol_new (buf, reg_section, regnum,
				   &zero_address_frag));
}

struct init_entry
  {
    const char *name;
    int number;
  };

static const struct init_entry init_table[] =
{
  { "d0", DATA0 },
  { "d1", DATA1 },
  { "d2", DATA2 },
  { "d3", DATA3 },
  { "d4", DATA4 },
  { "d5", DATA5 },
  { "d6", DATA6 },
  { "d7", DATA7 },
  { "a0", ADDR0 },
  { "a1", ADDR1 },
  { "a2", ADDR2 },
  { "a3", ADDR3 },
  { "a4", ADDR4 },
  { "a5", ADDR5 },
  { "a6", ADDR6 },
  { "fp", ADDR6 },
  { "a7", ADDR7 },
  { "sp", ADDR7 },
  { "ssp", ADDR7 },
  { "fp0", FP0 },
  { "fp1", FP1 },
  { "fp2", FP2 },
  { "fp3", FP3 },
  { "fp4", FP4 },
  { "fp5", FP5 },
  { "fp6", FP6 },
  { "fp7", FP7 },
  { "fpi", FPI },
  { "fpiar", FPI },
  { "fpc", FPI },
  { "fps", FPS },
  { "fpsr", FPS },
  { "fpc", FPC },
  { "fpcr", FPC },
  { "control", FPC },
  { "status", FPS },
  { "iaddr", FPI },

  { "cop0", COP0 },
  { "cop1", COP1 },
  { "cop2", COP2 },
  { "cop3", COP3 },
  { "cop4", COP4 },
  { "cop5", COP5 },
  { "cop6", COP6 },
  { "cop7", COP7 },
  { "pc", PC },
  { "zpc", ZPC },
  { "sr", SR },

  { "ccr", CCR },
  { "cc", CCR },

  { "usp", USP },
  { "isp", ISP },
  { "sfc", SFC },
  { "sfcr", SFC },
  { "dfc", DFC },
  { "dfcr", DFC },
  { "cacr", CACR },
  { "caar", CAAR },

  { "vbr", VBR },

  { "msp", MSP },
  { "itt0", ITT0 },
  { "itt1", ITT1 },
  { "dtt0", DTT0 },
  { "dtt1", DTT1 },
  { "mmusr", MMUSR },
  { "tc", TC },
  { "srp", SRP },
  { "urp", URP },
  { "buscr", BUSCR },
  { "pcr", PCR },

  { "ac", AC },
  { "bc", BC },
  { "cal", CAL },
  { "crp", CRP },
  { "drp", DRP },
  { "pcsr", PCSR },
  { "psr", PSR },
  { "scc", SCC },
  { "val", VAL },
  { "bad0", BAD0 },
  { "bad1", BAD1 },
  { "bad2", BAD2 },
  { "bad3", BAD3 },
  { "bad4", BAD4 },
  { "bad5", BAD5 },
  { "bad6", BAD6 },
  { "bad7", BAD7 },
  { "bac0", BAC0 },
  { "bac1", BAC1 },
  { "bac2", BAC2 },
  { "bac3", BAC3 },
  { "bac4", BAC4 },
  { "bac5", BAC5 },
  { "bac6", BAC6 },
  { "bac7", BAC7 },

  { "ic", IC },
  { "dc", DC },
  { "nc", NC },

  { "tt0", TT0 },
  { "tt1", TT1 },
  /* 68ec030 versions of same */
  { "ac0", TT0 },
  { "ac1", TT1 },
  /* 68ec030 access control unit, identical to 030 MMU status reg */
  { "acusr", PSR },

  /* Suppressed data and address registers.  */
  { "zd0", ZDATA0 },
  { "zd1", ZDATA1 },
  { "zd2", ZDATA2 },
  { "zd3", ZDATA3 },
  { "zd4", ZDATA4 },
  { "zd5", ZDATA5 },
  { "zd6", ZDATA6 },
  { "zd7", ZDATA7 },
  { "za0", ZADDR0 },
  { "za1", ZADDR1 },
  { "za2", ZADDR2 },
  { "za3", ZADDR3 },
  { "za4", ZADDR4 },
  { "za5", ZADDR5 },
  { "za6", ZADDR6 },
  { "za7", ZADDR7 },

  { 0, 0 }
};

void
init_regtable ()
{
  int i;
  for (i = 0; init_table[i].name; i++)
    insert_reg (init_table[i].name, init_table[i].number);
}

static int no_68851, no_68881;

#ifdef OBJ_AOUT
/* a.out machine type.  Default to 68020.  */
int m68k_aout_machtype = 2;
#endif

void
md_assemble (str)
     char *str;
{
  const char *er;
  short *fromP;
  char *toP = NULL;
  int m, n = 0;
  char *to_beg_P;
  int shorts_this_frag;
  fixS *fixP;

  /* In MRI mode, the instruction and operands are separated by a
     space.  Anything following the operands is a comment.  The label
     has already been removed.  */
  if (flag_mri)
    {
      char *s;
      int fields = 0;
      int infield = 0;
      int inquote = 0;

      for (s = str; *s != '\0'; s++)
	{
	  if ((*s == ' ' || *s == '\t') && ! inquote)
	    {
	      if (infield)
		{
		  ++fields;
		  if (fields >= 2)
		    {
		      *s = '\0';
		      break;
		    }
		  infield = 0;
		}
	    }
	  else
	    {
	      if (! infield)
		infield = 1;
	      if (*s == '\'')
		inquote = ! inquote;
	    }
	}
    }

  memset ((char *) (&the_ins), '\0', sizeof (the_ins));
  m68k_ip (str);
  er = the_ins.error;
  if (!er)
    {
      for (n = 0; n < the_ins.numargs; n++)
	if (the_ins.operands[n].error)
	  {
	    er = the_ins.operands[n].error;
	    break;
	  }
    }
  if (er)
    {
      as_bad ("%s -- statement `%s' ignored", er, str);
      return;
    }

  if (the_ins.nfrag == 0)
    {
      /* No frag hacking involved; just put it out */
      toP = frag_more (2 * the_ins.numo);
      fromP = &the_ins.opcode[0];
      for (m = the_ins.numo; m; --m)
	{
	  md_number_to_chars (toP, (long) (*fromP), 2);
	  toP += 2;
	  fromP++;
	}
      /* put out symbol-dependent info */
      for (m = 0; m < the_ins.nrel; m++)
	{
	  switch (the_ins.reloc[m].wid)
	    {
	    case 'B':
	      n = 1;
	      break;
	    case 'b':
	      n = 1;
	      break;
	    case '3':
	      n = 2;
	      break;
	    case 'w':
	      n = 2;
	      break;
	    case 'l':
	      n = 4;
	      break;
	    default:
	      as_fatal ("Don't know how to figure width of %c in md_assemble()",
			the_ins.reloc[m].wid);
	    }

	  fixP = fix_new_exp (frag_now,
			      ((toP - frag_now->fr_literal)
			       - the_ins.numo * 2 + the_ins.reloc[m].n),
			      n,
			      &the_ins.reloc[m].exp,
			      the_ins.reloc[m].pcrel,
			      NO_RELOC);
	  fixP->fx_pcrel_adjust = the_ins.reloc[m].pcrel_fix;
	}
      return;
    }

  /* There's some frag hacking */
  for (n = 0, fromP = &the_ins.opcode[0]; n < the_ins.nfrag; n++)
    {
      int wid;

      if (n == 0)
	wid = 2 * the_ins.fragb[n].fragoff;
      else
	wid = 2 * (the_ins.numo - the_ins.fragb[n - 1].fragoff);
      toP = frag_more (wid);
      to_beg_P = toP;
      shorts_this_frag = 0;
      for (m = wid / 2; m; --m)
	{
	  md_number_to_chars (toP, (long) (*fromP), 2);
	  toP += 2;
	  fromP++;
	  shorts_this_frag++;
	}
      for (m = 0; m < the_ins.nrel; m++)
	{
	  if ((the_ins.reloc[m].n) >= 2 * shorts_this_frag)
	    {
	      the_ins.reloc[m].n -= 2 * shorts_this_frag;
	      break;
	    }
	  wid = the_ins.reloc[m].wid;
	  if (wid == 0)
	    continue;
	  the_ins.reloc[m].wid = 0;
	  wid = (wid == 'b') ? 1 : (wid == 'w') ? 2 : (wid == 'l') ? 4 : 4000;

	  fixP = fix_new_exp (frag_now,
			      ((toP - frag_now->fr_literal)
			       - the_ins.numo * 2 + the_ins.reloc[m].n),
			      wid,
			      &the_ins.reloc[m].exp,
			      the_ins.reloc[m].pcrel,
			      NO_RELOC);
	  fixP->fx_pcrel_adjust = the_ins.reloc[m].pcrel_fix;
	}
      (void) frag_var (rs_machine_dependent, 10, 0,
		       (relax_substateT) (the_ins.fragb[n].fragty),
		       the_ins.fragb[n].fadd, the_ins.fragb[n].foff, to_beg_P);
    }
  n = (the_ins.numo - the_ins.fragb[n - 1].fragoff);
  shorts_this_frag = 0;
  if (n)
    {
      toP = frag_more (n * sizeof (short));
      while (n--)
	{
	  md_number_to_chars (toP, (long) (*fromP), 2);
	  toP += 2;
	  fromP++;
	  shorts_this_frag++;
	}
    }
  for (m = 0; m < the_ins.nrel; m++)
    {
      int wid;

      wid = the_ins.reloc[m].wid;
      if (wid == 0)
	continue;
      the_ins.reloc[m].wid = 0;
      wid = (wid == 'b') ? 1 : (wid == 'w') ? 2 : (wid == 'l') ? 4 : 4000;

      fixP = fix_new_exp (frag_now,
			  ((the_ins.reloc[m].n + toP - frag_now->fr_literal)
			   - shorts_this_frag * 2),
			  wid,
			  &the_ins.reloc[m].exp,
			  the_ins.reloc[m].pcrel,
			  NO_RELOC);
      fixP->fx_pcrel_adjust = the_ins.reloc[m].pcrel_fix;
    }
}

void
md_begin ()
{
  /*
   * md_begin -- set up hash tables with 68000 instructions.
   * similar to what the vax assembler does.  ---phr
   */
  /* RMS claims the thing to do is take the m68k-opcode.h table, and make
     a copy of it at runtime, adding in the information we want but isn't
     there.  I think it'd be better to have an awk script hack the table
     at compile time.  Or even just xstr the table and use it as-is.  But
     my lord ghod hath spoken, so we do it this way.  Excuse the ugly var
     names.  */

  register const struct m68k_opcode *ins;
  register struct m68k_incant *hack, *slak;
  register const char *retval = 0;	/* empty string, or error msg text */
  register unsigned int i;
  register char c;

  if (flag_mri)
    {
      flag_reg_prefix_optional = 1;
      m68k_abspcadd = 1;
      m68k_rel32 = 0;
    }

  op_hash = hash_new ();

  obstack_begin (&robyn, 4000);
  for (i = 0; i < m68k_numopcodes; i++)
    {
      hack = slak = (struct m68k_incant *) obstack_alloc (&robyn, sizeof (struct m68k_incant));
      do
	{
	  ins = &m68k_opcodes[i];
	  /* We *could* ignore insns that don't match our arch here
	     but just leaving them out of the hash. */
	  slak->m_operands = ins->args;
	  slak->m_opnum = strlen (slak->m_operands) / 2;
	  slak->m_arch = ins->arch;
	  slak->m_opcode = ins->opcode;
	  /* This is kludgey */
	  slak->m_codenum = ((ins->match) & 0xffffL) ? 2 : 1;
	  if (i + 1 != m68k_numopcodes
	      && !strcmp (ins->name, m68k_opcodes[i + 1].name))
	    {
	      slak->m_next = (struct m68k_incant *) obstack_alloc (&robyn, sizeof (struct m68k_incant));
	      i++;
	    }
	  else
	    slak->m_next = 0;
	  slak = slak->m_next;
	}
      while (slak);

      retval = hash_insert (op_hash, ins->name, (char *) hack);
      if (retval)
	as_fatal ("Internal Error:  Can't hash %s: %s", ins->name, retval);
    }

  for (i = 0; i < m68k_numaliases; i++)
    {
      const char *name = m68k_opcode_aliases[i].primary;
      const char *alias = m68k_opcode_aliases[i].alias;
      PTR val = hash_find (op_hash, name);
      if (!val)
	as_fatal ("Internal Error: Can't find %s in hash table", name);
      retval = hash_insert (op_hash, alias, val);
      if (retval)
	as_fatal ("Internal Error: Can't hash %s: %s", alias, retval);
    }

  /* In MRI mode, all unsized branches are variable sized.  Normally,
     they are word sized.  */
  if (flag_mri)
    {
      static struct m68k_opcode_alias mri_aliases[] =
	{
	  { "bhi",	"jhi", },
	  { "bls",	"jls", },
	  { "bcc",	"jcc", },
	  { "bcs",	"jcs", },
	  { "bne",	"jne", },
	  { "beq",	"jeq", },
	  { "bvc",	"jvc", },
	  { "bvs",	"jvs", },
	  { "bpl",	"jpl", },
	  { "bmi",	"jmi", },
	  { "bge",	"jge", },
	  { "blt",	"jlt", },
	  { "bgt",	"jgt", },
	  { "ble",	"jle", },
	  { "bra",	"jra", },
	  { "bsr",	"jbsr", },
	};

      for (i = 0; i < sizeof mri_aliases / sizeof mri_aliases[0]; i++)
	{
	  const char *name = mri_aliases[i].primary;
	  const char *alias = mri_aliases[i].alias;
	  PTR val = hash_find (op_hash, name);
	  if (!val)
	    as_fatal ("Internal Error: Can't find %s in hash table", name);
	  retval = hash_jam (op_hash, alias, val);
	  if (retval)
	    as_fatal ("Internal Error: Can't hash %s: %s", alias, retval);
	}
    }

  for (i = 0; i < sizeof (mklower_table); i++)
    mklower_table[i] = (isupper (c = (char) i)) ? tolower (c) : c;

  for (i = 0; i < sizeof (notend_table); i++)
    {
      notend_table[i] = 0;
      alt_notend_table[i] = 0;
    }
  notend_table[','] = 1;
  notend_table['{'] = 1;
  notend_table['}'] = 1;
  alt_notend_table['a'] = 1;
  alt_notend_table['A'] = 1;
  alt_notend_table['d'] = 1;
  alt_notend_table['D'] = 1;
  alt_notend_table['#'] = 1;
  alt_notend_table['&'] = 1;
  alt_notend_table['f'] = 1;
  alt_notend_table['F'] = 1;
#ifdef REGISTER_PREFIX
  alt_notend_table[REGISTER_PREFIX] = 1;
#endif

  /* We need to put '(' in alt_notend_table to handle
       cas2 %d0:%d2,%d3:%d4,(%a0):(%a1)
     */
  alt_notend_table['('] = 1;

  /* We need to put '@' in alt_notend_table to handle
       cas2 %d0:%d2,%d3:%d4,@(%d0):@(%d1)
     */
  alt_notend_table['@'] = 1;

#ifndef MIT_SYNTAX_ONLY
  /* Insert pseudo ops, these have to go into the opcode table since
     gas expects pseudo ops to start with a dot */
  {
    int n = 0;
    while (mote_pseudo_table[n].poc_name)
      {
	hack = (struct m68k_incant *)
	  obstack_alloc (&robyn, sizeof (struct m68k_incant));
	hash_insert (op_hash,
		     mote_pseudo_table[n].poc_name, (char *) hack);
	hack->m_operands = 0;
	hack->m_opnum = n;
	n++;
      }
  }
#endif

  init_regtable ();
}

void
m68k_init_after_args ()
{
  if (cpu_of_arch (current_architecture) == 0)
    {
      int i;
      const char *default_cpu = TARGET_CPU;

      if (*default_cpu == 'm')
	default_cpu++;
      for (i = 0; i < n_archs; i++)
	if (strcasecmp (default_cpu, archs[i].name) == 0)
	  break;
      if (i == n_archs)
	{
	  as_bad ("unrecognized default cpu `%s' ???", TARGET_CPU);
	  current_architecture |= m68020;
	}
      else
	current_architecture |= archs[i].arch;
    }
  /* Permit m68881 specification with all cpus; those that can't work
     with a coprocessor could be doing emulation.  */
  if (current_architecture & m68851)
    {
      if (current_architecture & m68040)
	{
	  as_warn ("68040 and 68851 specified; mmu instructions may assemble incorrectly");
	}
    }
  /* What other incompatibilities could we check for?  */

  /* Toss in some default assumptions about coprocessors.  */
  if (!no_68881
      && (cpu_of_arch (current_architecture)
	  /* Can CPU32 have a 68881 coprocessor??  */
	  & (m68020 | m68030 | cpu32)))
    {
      current_architecture |= m68881;
    }
  if (!no_68851
      && (cpu_of_arch (current_architecture) & m68020up) != 0
      && (cpu_of_arch (current_architecture) & m68040up) == 0)
    {
      current_architecture |= m68851;
    }
  if (no_68881 && (current_architecture & m68881))
    as_bad ("options for 68881 and no-68881 both given");
  if (no_68851 && (current_architecture & m68851))
    as_bad ("options for 68851 and no-68851 both given");

#ifdef OBJ_AOUT
  /* Work out the magic number.  This isn't very general.  */
  if (current_architecture & m68000)
    m68k_aout_machtype = 0;
  else if (current_architecture & m68010)
    m68k_aout_machtype = 1;
  else if (current_architecture & m68020)
    m68k_aout_machtype = 2;
  else
    m68k_aout_machtype = 2;
#endif

  /* Note which set of "movec" control registers is available.  */
  switch (cpu_of_arch (current_architecture))
    {
    case m68000:
      control_regs = m68000_control_regs;
      break;
    case m68010:
      control_regs = m68010_control_regs;
      break;
    case m68020:
    case m68030:
      control_regs = m68020_control_regs;
      break;
    case m68040:
      control_regs = m68040_control_regs;
      break;
    case m68060:
      control_regs = m68060_control_regs;
      break;
    case cpu32:
      control_regs = cpu32_control_regs;
      break;
    default:
      abort ();
    }

  if (cpu_of_arch (current_architecture) < m68020)
    md_relax_table[TAB (PCINDEX, BYTE)].rlx_more = 0;
}

/* Equal to MAX_PRECISION in atof-ieee.c */
#define MAX_LITTLENUMS 6

/* Turn a string in input_line_pointer into a floating point constant
   of type type, and store the appropriate bytes in *litP.  The number
   of LITTLENUMS emitted is stored in *sizeP .  An error message is
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
      md_number_to_chars (litP, (long) (*wordP++), sizeof (LITTLENUM_TYPE));
      litP += sizeof (LITTLENUM_TYPE);
    }
  return 0;
}

void
md_number_to_chars (buf, val, n)
     char *buf;
     valueT val;
     int n;
{
  number_to_chars_bigendian (buf, val, n);
}

static void
md_apply_fix_2 (fixP, val)
     fixS *fixP;
     offsetT val;
{
  addressT upper_limit;
  offsetT lower_limit;

  /* This is unnecessary but it convinces the native rs6000 compiler
     to generate the code we want.  */
  char *buf = fixP->fx_frag->fr_literal;
  buf += fixP->fx_where;
  /* end ibm compiler workaround */

  if (val & 0x80000000)
    val |= ~(addressT)0x7fffffff;
  else
    val &= 0x7fffffff;

  switch (fixP->fx_size)
    {
      /* The cast to offsetT below are necessary to make code correct for
	 machines where ints are smaller than offsetT */
    case 1:
      *buf++ = val;
      upper_limit = 0x7f;
      lower_limit = - (offsetT) 0x80;
      break;
    case 2:
      *buf++ = (val >> 8);
      *buf++ = val;
      upper_limit = 0x7fff;
      lower_limit = - (offsetT) 0x8000;
      break;
    case 4:
      *buf++ = (val >> 24);
      *buf++ = (val >> 16);
      *buf++ = (val >> 8);
      *buf++ = val;
      upper_limit = 0x7fffffff;
      lower_limit = - (offsetT) 0x7fffffff - 1;	/* avoid constant overflow */
      break;
    default:
      BAD_CASE (fixP->fx_size);
    }

  /* Fix up a negative reloc.  */
  if (fixP->fx_addsy == NULL && fixP->fx_subsy != NULL)
    {
      fixP->fx_addsy = fixP->fx_subsy;
      fixP->fx_subsy = NULL;
      fixP->fx_tcbit = 1;
    }

  /* For non-pc-relative values, it's conceivable we might get something
     like "0xff" for a byte field.  So extend the upper part of the range
     to accept such numbers.  We arbitrarily disallow "-0xff" or "0xff+0xff",
     so that we can do any range checking at all.  */
  if (!fixP->fx_pcrel)
    upper_limit = upper_limit * 2 + 1;

  if ((addressT) val > upper_limit
      && (val > 0 || val < lower_limit))
    as_bad_where (fixP->fx_file, fixP->fx_line, "value out of range");

  /* A one byte PC-relative reloc means a short branch.  We can't use
     a short branch with a value of 0 or -1, because those indicate
     different opcodes (branches with longer offsets).  */
  if (fixP->fx_pcrel
      && fixP->fx_size == 1
      && (fixP->fx_addsy == NULL
	  || S_IS_DEFINED (fixP->fx_addsy))
      && (val == 0 || val == -1))
    as_bad_where (fixP->fx_file, fixP->fx_line, "invalid byte branch offset");
}

#ifdef BFD_ASSEMBLER
int
md_apply_fix (fixP, valp)
     fixS *fixP;
     valueT *valp;
{
  md_apply_fix_2 (fixP, (addressT) *valp);
  return 1;
}
#else
void md_apply_fix (fixP, val)
     fixS *fixP;
     long val;
{
  md_apply_fix_2 (fixP, (addressT) val);
}
#endif

/* *fragP has been relaxed to its final size, and now needs to have
   the bytes inside it modified to conform to the new size  There is UGLY
   MAGIC here. ..
   */
void
md_convert_frag_1 (fragP)
     register fragS *fragP;
{
  long disp;
  long ext = 0;
  fixS *fixP;

  /* Address in object code of the displacement.  */
  register int object_address = fragP->fr_fix + fragP->fr_address;

  /* Address in gas core of the place to store the displacement.  */
  /* This convinces the native rs6000 compiler to generate the code we
     want. */
  register char *buffer_address = fragP->fr_literal;
  buffer_address += fragP->fr_fix;
  /* end ibm compiler workaround */

  /* The displacement of the address, from current location.  */
  disp = fragP->fr_symbol ? S_GET_VALUE (fragP->fr_symbol) : 0;
  disp = (disp + fragP->fr_offset) - object_address;

#ifdef BFD_ASSEMBLER
  disp += fragP->fr_symbol->sy_frag->fr_address;
#endif

  switch (fragP->fr_subtype)
    {
    case TAB (BCC68000, BYTE):
    case TAB (ABRANCH, BYTE):
      know (issbyte (disp));
      if (disp == 0)
	as_bad ("short branch with zero offset: use :w");
      fragP->fr_opcode[1] = disp;
      ext = 0;
      break;
    case TAB (DBCC, SHORT):
      know (issword (disp));
      ext = 2;
      break;
    case TAB (BCC68000, SHORT):
    case TAB (ABRANCH, SHORT):
      know (issword (disp));
      fragP->fr_opcode[1] = 0x00;
      ext = 2;
      break;
    case TAB (ABRANCH, LONG):
      if (cpu_of_arch (current_architecture) < m68020)
	{
	  if (fragP->fr_opcode[0] == 0x61)
	    /* BSR */
	    {
	      fragP->fr_opcode[0] = 0x4E;
	      fragP->fr_opcode[1] = (char) 0xB9; /* JBSR with ABSL LONG offset */

	      fix_new (fragP,
		       fragP->fr_fix,
		       4,
		       fragP->fr_symbol,
		       fragP->fr_offset,
		       0,
		       NO_RELOC);

	      fragP->fr_fix += 4;
	      ext = 0;
	    }
	  /* BRA */
	  else if (fragP->fr_opcode[0] == 0x60)
	    {
	      fragP->fr_opcode[0] = 0x4E;
	      fragP->fr_opcode[1] = (char) 0xF9; /* JMP  with ABSL LONG offset */
	      fix_new (fragP, fragP->fr_fix, 4, fragP->fr_symbol,
		       fragP->fr_offset, 0, NO_RELOC);
	      fragP->fr_fix += 4;
	      ext = 0;
	    }
	  else
	    {
	      as_bad ("Long branch offset not supported.");
	    }
	}
      else
	{
	  fragP->fr_opcode[1] = (char) 0xff;
	  ext = 4;
	}
      break;
    case TAB (BCC68000, LONG):
      /* only Bcc 68000 instructions can come here */
      /* change bcc into b!cc/jmp absl long */
      fragP->fr_opcode[0] ^= 0x01;	/* invert bcc */
      fragP->fr_opcode[1] = 0x6;/* branch offset = 6 */

      /* JF: these used to be fr_opcode[2,3], but they may be in a
	   different frag, in which case refering to them is a no-no.
	   Only fr_opcode[0,1] are guaranteed to work. */
      *buffer_address++ = 0x4e;	/* put in jmp long (0x4ef9) */
      *buffer_address++ = (char) 0xf9;
      fragP->fr_fix += 2;	/* account for jmp instruction */
      fix_new (fragP, fragP->fr_fix, 4, fragP->fr_symbol,
	       fragP->fr_offset, 0, NO_RELOC);
      fragP->fr_fix += 4;
      ext = 0;
      break;
    case TAB (DBCC, LONG):
      /* only DBcc 68000 instructions can come here */
      /* change dbcc into dbcc/jmp absl long */
      /* JF: these used to be fr_opcode[2-7], but that's wrong */
      *buffer_address++ = 0x00;	/* branch offset = 4 */
      *buffer_address++ = 0x04;
      *buffer_address++ = 0x60;	/* put in bra pc+6 */
      *buffer_address++ = 0x06;
      *buffer_address++ = 0x4e;	/* put in jmp long (0x4ef9) */
      *buffer_address++ = (char) 0xf9;

      fragP->fr_fix += 6;	/* account for bra/jmp instructions */
      fix_new (fragP, fragP->fr_fix, 4, fragP->fr_symbol,
	       fragP->fr_offset, 0, NO_RELOC);
      fragP->fr_fix += 4;
      ext = 0;
      break;
    case TAB (FBRANCH, SHORT):
      know ((fragP->fr_opcode[1] & 0x40) == 0);
      ext = 2;
      break;
    case TAB (FBRANCH, LONG):
      fragP->fr_opcode[1] |= 0x40;	/* Turn on LONG bit */
      ext = 4;
      break;
    case TAB (PCREL, SHORT):
      ext = 2;
      break;
    case TAB (PCREL, LONG):
      /* The thing to do here is force it to ABSOLUTE LONG, since
	PCREL is really trying to shorten an ABSOLUTE address anyway */
      /* JF FOO This code has not been tested */
      fix_new (fragP, fragP->fr_fix, 4, fragP->fr_symbol, fragP->fr_offset,
	       0, NO_RELOC);
      if ((fragP->fr_opcode[1] & 0x3F) != 0x3A)
	as_bad ("Internal error (long PC-relative operand) for insn 0x%04x at 0x%lx",
		(unsigned) fragP->fr_opcode[0],
		(unsigned long) fragP->fr_address);
      fragP->fr_opcode[1] &= ~0x3F;
      fragP->fr_opcode[1] |= 0x39;	/* Mode 7.1 */
      fragP->fr_fix += 4;
      ext = 0;
      break;
    case TAB (PCLEA, SHORT):
      fix_new (fragP, (int) (fragP->fr_fix), 2, fragP->fr_symbol,
	       fragP->fr_offset, 1, NO_RELOC);
      fragP->fr_opcode[1] &= ~0x3F;
      fragP->fr_opcode[1] |= 0x3A; /* 072 - mode 7.2 */
      ext = 2;
      break;
    case TAB (PCLEA, LONG):
      fixP = fix_new (fragP, (int) (fragP->fr_fix) + 2, 4, fragP->fr_symbol,
		      fragP->fr_offset, 1, NO_RELOC);
      fixP->fx_pcrel_adjust = 2;
      /* Already set to mode 7.3; this indicates: PC indirect with
	 suppressed index, 32-bit displacement.  */
      *buffer_address++ = 0x01;
      *buffer_address++ = 0x70;
      fragP->fr_fix += 2;
      ext = 4;
      break;

    case TAB (PCINDEX, BYTE):
      disp += 2;
      if (!issbyte (disp))
	{
	  as_bad ("displacement doesn't fit in one byte");
	  disp = 0;
	}
      assert (fragP->fr_fix >= 2);
      buffer_address[-2] &= ~1;
      buffer_address[-1] = disp;
      ext = 0;
      break;
    case TAB (PCINDEX, SHORT):
      disp += 2;
      assert (issword (disp));
      assert (fragP->fr_fix >= 2);
      buffer_address[-2] |= 0x1;
      buffer_address[-1] = 0x20;
      fixP = fix_new (fragP, (int) (fragP->fr_fix), 2, fragP->fr_symbol,
		      fragP->fr_offset, (fragP->fr_opcode[1] & 077) == 073,
		      NO_RELOC);
      fixP->fx_pcrel_adjust = 2;
      ext = 2;
      break;
    case TAB (PCINDEX, LONG):
      disp += 2;
      fixP = fix_new (fragP, (int) (fragP->fr_fix), 4, fragP->fr_symbol,
		      fragP->fr_offset, (fragP->fr_opcode[1] & 077) == 073,
		      NO_RELOC);
      fixP->fx_pcrel_adjust = 2;
      assert (fragP->fr_fix >= 2);
      buffer_address[-2] |= 0x1;
      buffer_address[-1] = 0x30;
      ext = 4;
      break;
    }

  if (ext)
    {
      md_number_to_chars (buffer_address, (long) disp, (int) ext);
      fragP->fr_fix += ext;
    }
}

#ifndef BFD_ASSEMBLER

void
md_convert_frag (headers, sec, fragP)
     object_headers *headers;
     segT sec;
     fragS *fragP;
{
  md_convert_frag_1 (fragP);
}

#else

void
md_convert_frag (abfd, sec, fragP)
     bfd *abfd;
     segT sec;
     fragS *fragP;
{
  md_convert_frag_1 (fragP);
}
#endif

/* Force truly undefined symbols to their maximum size, and generally set up
   the frag list to be relaxed
   */
int
md_estimate_size_before_relax (fragP, segment)
     register fragS *fragP;
     segT segment;
{
  int old_fix;
  register char *buffer_address = fragP->fr_fix + fragP->fr_literal;

  old_fix = fragP->fr_fix;

  /* handle SZ_UNDEF first, it can be changed to BYTE or SHORT */
  switch (fragP->fr_subtype)
    {

    case TAB (ABRANCH, SZ_UNDEF):
      {
	if ((fragP->fr_symbol != NULL)	/* Not absolute */
	    && S_GET_SEGMENT (fragP->fr_symbol) == segment)
	  {
	    fragP->fr_subtype = TAB (TABTYPE (fragP->fr_subtype), BYTE);
	    break;
	  }
	else if ((fragP->fr_symbol == 0) || (cpu_of_arch (current_architecture) < m68020))
	  {
	    /* On 68000, or for absolute value, switch to abs long */
	    /* FIXME, we should check abs val, pick short or long */
	    if (fragP->fr_opcode[0] == 0x61)
	      {
		fragP->fr_opcode[0] = 0x4E;
		fragP->fr_opcode[1] = (char) 0xB9; /* JBSR with ABSL LONG offset */
		fix_new (fragP, fragP->fr_fix, 4,
			 fragP->fr_symbol, fragP->fr_offset, 0, NO_RELOC);
		fragP->fr_fix += 4;
		frag_wane (fragP);
	      }
	    else if (fragP->fr_opcode[0] == 0x60)
	      {
		fragP->fr_opcode[0] = 0x4E;
		fragP->fr_opcode[1] = (char) 0xF9; /* JMP  with ABSL LONG offset */
		fix_new (fragP, fragP->fr_fix, 4,
			 fragP->fr_symbol, fragP->fr_offset, 0, NO_RELOC);
		fragP->fr_fix += 4;
		frag_wane (fragP);
	      }
	    else
	      {
		as_warn ("Long branch offset to extern symbol not supported.");
	      }
	  }
	else
	  {			/* Symbol is still undefined.  Make it simple */
	    fix_new (fragP, (int) (fragP->fr_fix), 4, fragP->fr_symbol,
		     fragP->fr_offset, 1, NO_RELOC);
	    fragP->fr_fix += 4;
	    fragP->fr_opcode[1] = (char) 0xff;
	    frag_wane (fragP);
	    break;
	  }

	break;
      }				/* case TAB(ABRANCH,SZ_UNDEF) */

    case TAB (FBRANCH, SZ_UNDEF):
      {
	if (S_GET_SEGMENT (fragP->fr_symbol) == segment || flag_short_refs)
	  {
	    fragP->fr_subtype = TAB (FBRANCH, SHORT);
	    fragP->fr_var += 2;
	  }
	else
	  {
	    fix_new (fragP, (int) fragP->fr_fix, 4, fragP->fr_symbol,
		     fragP->fr_offset, 1, NO_RELOC);
	    fragP->fr_fix += 4;
	    fragP->fr_opcode[1] |= 0x40; /* Turn on LONG bit */
	    frag_wane (fragP);
	  }
	break;
      }				/* TAB(FBRANCH,SZ_UNDEF) */

    case TAB (PCREL, SZ_UNDEF):
      {
	if (S_GET_SEGMENT (fragP->fr_symbol) == segment
	    || flag_short_refs
	    || cpu_of_arch (current_architecture) < m68020)
	  {
	    fragP->fr_subtype = TAB (PCREL, SHORT);
	    fragP->fr_var += 2;
	  }
	else
	  {
	    fragP->fr_subtype = TAB (PCREL, LONG);
	    fragP->fr_var += 4;
	  }
	break;
      }				/* TAB(PCREL,SZ_UNDEF) */

    case TAB (BCC68000, SZ_UNDEF):
      {
	if ((fragP->fr_symbol != NULL)
	    && S_GET_SEGMENT (fragP->fr_symbol) == segment)
	  {
	    fragP->fr_subtype = TAB (BCC68000, BYTE);
	    break;
	  }
	/* only Bcc 68000 instructions can come here */
	/* change bcc into b!cc/jmp absl long */
	fragP->fr_opcode[0] ^= 0x01;	/* invert bcc */
	if (flag_short_refs)
	  {
	    fragP->fr_opcode[1] = 0x04;	/* branch offset = 6 */
	    /* JF: these were fr_opcode[2,3] */
	    buffer_address[0] = 0x4e;	/* put in jmp long (0x4ef9) */
	    buffer_address[1] = (char) 0xf8;
	    fragP->fr_fix += 2;	/* account for jmp instruction */
	    fix_new (fragP, fragP->fr_fix, 2, fragP->fr_symbol,
		     fragP->fr_offset, 0, NO_RELOC);
	    fragP->fr_fix += 2;
	  }
	else
	  {
	    fragP->fr_opcode[1] = 0x06;	/* branch offset = 6 */
	    /* JF: these were fr_opcode[2,3] */
	    buffer_address[0] = 0x4e;	/* put in jmp long (0x4ef9) */
	    buffer_address[1] = (char) 0xf9;
	    fragP->fr_fix += 2;	/* account for jmp instruction */
	    fix_new (fragP, fragP->fr_fix, 4, fragP->fr_symbol,
		     fragP->fr_offset, 0, NO_RELOC);
	    fragP->fr_fix += 4;
	  }
	frag_wane (fragP);
	break;
      }				/* case TAB(BCC68000,SZ_UNDEF) */

    case TAB (DBCC, SZ_UNDEF):
      {
	if (fragP->fr_symbol != NULL && S_GET_SEGMENT (fragP->fr_symbol) == segment)
	  {
	    fragP->fr_subtype = TAB (DBCC, SHORT);
	    fragP->fr_var += 2;
	    break;
	  }
	/* only DBcc 68000 instructions can come here */
	/* change dbcc into dbcc/jmp absl long */
	/* JF: these used to be fr_opcode[2-4], which is wrong. */
	buffer_address[0] = 0x00;	/* branch offset = 4 */
	buffer_address[1] = 0x04;
	buffer_address[2] = 0x60;	/* put in bra pc + ... */

	if (flag_short_refs)
	  {
	    /* JF: these were fr_opcode[5-7] */
	    buffer_address[3] = 0x04;	/* plus 4 */
	    buffer_address[4] = 0x4e;	/* Put in Jump Word */
	    buffer_address[5] = (char) 0xf8;
	    fragP->fr_fix += 6;	/* account for bra/jmp instruction */
	    fix_new (fragP, fragP->fr_fix, 2, fragP->fr_symbol,
		     fragP->fr_offset, 0, NO_RELOC);
	    fragP->fr_fix += 2;
	  }
	else
	  {
	    /* JF: these were fr_opcode[5-7] */
	    buffer_address[3] = 0x06;	/* Plus 6 */
	    buffer_address[4] = 0x4e;	/* put in jmp long (0x4ef9) */
	    buffer_address[5] = (char) 0xf9;
	    fragP->fr_fix += 6;	/* account for bra/jmp instruction */
	    fix_new (fragP, fragP->fr_fix, 4, fragP->fr_symbol,
		     fragP->fr_offset, 0, NO_RELOC);
	    fragP->fr_fix += 4;
	  }

	frag_wane (fragP);
	break;
      }				/* case TAB(DBCC,SZ_UNDEF) */

    case TAB (PCLEA, SZ_UNDEF):
      {
	if ((S_GET_SEGMENT (fragP->fr_symbol)) == segment
	    || flag_short_refs
	    || cpu_of_arch (current_architecture) < m68020)
	  {
	    fragP->fr_subtype = TAB (PCLEA, SHORT);
	    fragP->fr_var += 2;
	  }
	else
	  {
	    fragP->fr_subtype = TAB (PCLEA, LONG);
	    fragP->fr_var += 6;
	  }
	break;
      }				/* TAB(PCLEA,SZ_UNDEF) */

    case TAB (PCINDEX, SZ_UNDEF):
      if (S_GET_SEGMENT (fragP->fr_symbol) == segment
	  || cpu_of_arch (current_architecture) < m68020)
	{
	  fragP->fr_subtype = TAB (PCINDEX, BYTE);
	}
      else
	{
	  fragP->fr_subtype = TAB (PCINDEX, LONG);
	  fragP->fr_var += 4;
	}
      break;

    default:
      break;
    }

  /* now that SZ_UNDEF are taken care of, check others */
  switch (fragP->fr_subtype)
    {
    case TAB (BCC68000, BYTE):
    case TAB (ABRANCH, BYTE):
      /* We can't do a short jump to the next instruction,
	   so we force word mode.  */
      if (fragP->fr_symbol && S_GET_VALUE (fragP->fr_symbol) == 0 &&
	  fragP->fr_symbol->sy_frag == fragP->fr_next)
	{
	  fragP->fr_subtype = TAB (TABTYPE (fragP->fr_subtype), SHORT);
	  fragP->fr_var += 2;
	}
      break;
    default:
      break;
    }
  return fragP->fr_var + fragP->fr_fix - old_fix;
}

#if defined(OBJ_AOUT) | defined(OBJ_BOUT)
/* the bit-field entries in the relocation_info struct plays hell
   with the byte-order problems of cross-assembly.  So as a hack,
   I added this mach. dependent ri twiddler.  Ugly, but it gets
   you there. -KWK */
/* on m68k: first 4 bytes are normal unsigned long, next three bytes
   are symbolnum, most sig. byte first.  Last byte is broken up with
   bit 7 as pcrel, bits 6 & 5 as length, bit 4 as pcrel, and the lower
   nibble as nuthin. (on Sun 3 at least) */
/* Translate the internal relocation information into target-specific
   format. */
#ifdef comment
void
md_ri_to_chars (the_bytes, ri)
     char *the_bytes;
     struct reloc_info_generic *ri;
{
  /* this is easy */
  md_number_to_chars (the_bytes, ri->r_address, 4);
  /* now the fun stuff */
  the_bytes[4] = (ri->r_symbolnum >> 16) & 0x0ff;
  the_bytes[5] = (ri->r_symbolnum >> 8) & 0x0ff;
  the_bytes[6] = ri->r_symbolnum & 0x0ff;
  the_bytes[7] = (((ri->r_pcrel << 7) & 0x80) | ((ri->r_length << 5) & 0x60) |
		  ((ri->r_extern << 4) & 0x10));
}

#endif /* comment */

#ifndef BFD_ASSEMBLER
void
tc_aout_fix_to_chars (where, fixP, segment_address_in_file)
     char *where;
     fixS *fixP;
     relax_addressT segment_address_in_file;
{
  /*
   * In: length of relocation (or of address) in chars: 1, 2 or 4.
   * Out: GNU LD relocation length code: 0, 1, or 2.
   */

  static CONST unsigned char nbytes_r_length[] = {42, 0, 1, 42, 2};
  long r_symbolnum;

  know (fixP->fx_addsy != NULL);

  md_number_to_chars (where,
       fixP->fx_frag->fr_address + fixP->fx_where - segment_address_in_file,
		      4);

  r_symbolnum = (S_IS_DEFINED (fixP->fx_addsy)
		 ? S_GET_TYPE (fixP->fx_addsy)
		 : fixP->fx_addsy->sy_number);

  where[4] = (r_symbolnum >> 16) & 0x0ff;
  where[5] = (r_symbolnum >> 8) & 0x0ff;
  where[6] = r_symbolnum & 0x0ff;
  where[7] = (((fixP->fx_pcrel << 7) & 0x80) | ((nbytes_r_length[fixP->fx_size] << 5) & 0x60) |
	      (((!S_IS_DEFINED (fixP->fx_addsy)) << 4) & 0x10));
}
#endif

#endif /* OBJ_AOUT or OBJ_BOUT */

#ifndef WORKING_DOT_WORD
CONST int md_short_jump_size = 4;
CONST int md_long_jump_size = 6;

void
md_create_short_jump (ptr, from_addr, to_addr, frag, to_symbol)
     char *ptr;
     addressT from_addr, to_addr;
     fragS *frag;
     symbolS *to_symbol;
{
  valueT offset;

  offset = to_addr - (from_addr + 2);

  md_number_to_chars (ptr, (valueT) 0x6000, 2);
  md_number_to_chars (ptr + 2, (valueT) offset, 2);
}

void
md_create_long_jump (ptr, from_addr, to_addr, frag, to_symbol)
     char *ptr;
     addressT from_addr, to_addr;
     fragS *frag;
     symbolS *to_symbol;
{
  valueT offset;

  if (cpu_of_arch (current_architecture) < m68020)
    {
      offset = to_addr - S_GET_VALUE (to_symbol);
      md_number_to_chars (ptr, (valueT) 0x4EF9, 2);
      md_number_to_chars (ptr + 2, (valueT) offset, 4);
      fix_new (frag, (ptr + 2) - frag->fr_literal, 4, to_symbol, (offsetT) 0,
	       0, NO_RELOC);
    }
  else
    {
      offset = to_addr - (from_addr + 2);
      md_number_to_chars (ptr, (valueT) 0x60ff, 2);
      md_number_to_chars (ptr + 2, (valueT) offset, 4);
    }
}

#endif

/* Different values of OK tell what its OK to return.  Things that
   aren't OK are an error (what a shock, no?)

   0:  Everything is OK
   10:  Absolute 1:8	only
   20:  Absolute 0:7	only
   30:  absolute 0:15	only
   40:  Absolute 0:31	only
   50:  absolute 0:127	only
   55:  absolute -64:63    only
   60:  absolute -128:127	only
   70:  absolute 0:4095	only
   80:  No bignums

   */

static int
get_num (exp, ok)
     struct m68k_exp *exp;
     int ok;
{
  if (exp->exp.X_op == O_absent)
    {
      /* Do the same thing the VAX asm does */
      op (exp) = O_constant;
      adds (exp) = 0;
      subs (exp) = 0;
      offs (exp) = 0;
      if (ok == 10)
	{
	  as_warn ("expression out of range: defaulting to 1");
	  offs (exp) = 1;
	}
    }
  else if (exp->exp.X_op == O_constant)
    {
      switch (ok)
	{
	case 10:
	  if (offs (exp) < 1 || offs (exp) > 8)
	    {
	      as_warn ("expression out of range: defaulting to 1");
	      offs (exp) = 1;
	    }
	  break;
	case 20:
	  if (offs (exp) < 0 || offs (exp) > 7)
	    goto outrange;
	  break;
	case 30:
	  if (offs (exp) < 0 || offs (exp) > 15)
	    goto outrange;
	  break;
	case 40:
	  if (offs (exp) < 0 || offs (exp) > 32)
	    goto outrange;
	  break;
	case 50:
	  if (offs (exp) < 0 || offs (exp) > 127)
	    goto outrange;
	  break;
	case 55:
	  if (offs (exp) < -64 || offs (exp) > 63)
	    goto outrange;
	  break;
	case 60:
	  if (offs (exp) < -128 || offs (exp) > 127)
	    goto outrange;
	  break;
	case 70:
	  if (offs (exp) < 0 || offs (exp) > 4095)
	    {
	    outrange:
	      as_warn ("expression out of range: defaulting to 0");
	      offs (exp) = 0;
	    }
	  break;
	default:
	  break;
	}
    }
  else if (exp->exp.X_op == O_big)
    {
      if (offs (exp) <= 0	/* flonum */
	  && (ok == 80		/* no bignums */
	      || (ok > 10	/* small-int ranges including 0 ok */
		  /* If we have a flonum zero, a zero integer should
		     do as well (e.g., in moveq).  */
		  && generic_floating_point_number.exponent == 0
		  && generic_floating_point_number.low[0] == 0)))
	{
	  /* HACK! Turn it into a long */
	  LITTLENUM_TYPE words[6];

	  gen_to_words (words, 2, 8L);	/* These numbers are magic! */
	  op (exp) = O_constant;
	  adds (exp) = 0;
	  subs (exp) = 0;
	  offs (exp) = words[1] | (words[0] << 16);
	}
      else if (ok != 0)
	{
	  op (exp) = O_constant;
	  adds (exp) = 0;
	  subs (exp) = 0;
	  offs (exp) = (ok == 10) ? 1 : 0;
	  as_warn ("Can't deal with expression; defaulting to %ld",
		   offs (exp));
	}
    }
  else
    {
      if (ok >= 10 && ok <= 70)
	{
	  op (exp) = O_constant;
	  adds (exp) = 0;
	  subs (exp) = 0;
	  offs (exp) = (ok == 10) ? 1 : 0;
	  as_warn ("Can't deal with expression; defaulting to %ld",
		   offs (exp));
	}
    }

  if (exp->size != SIZE_UNSPEC)
    {
      switch (exp->size)
	{
	case SIZE_UNSPEC:
	case SIZE_LONG:
	  break;
	case SIZE_BYTE:
	  if (!isbyte (offs (exp)))
	    as_warn ("expression doesn't fit in BYTE");
	  break;
	case SIZE_WORD:
	  if (!isword (offs (exp)))
	    as_warn ("expression doesn't fit in WORD");
	  break;
	}
    }

  return offs (exp);
}

/* These are the back-ends for the various machine dependent pseudo-ops.  */
void demand_empty_rest_of_line ();	/* Hate those extra verbose names */

static void
s_data1 (ignore)
     int ignore;
{
  subseg_set (data_section, 1);
  demand_empty_rest_of_line ();
}

static void
s_data2 (ignore)
     int ignore;
{
  subseg_set (data_section, 2);
  demand_empty_rest_of_line ();
}

static void
s_bss (ignore)
     int ignore;
{
  /* We don't support putting frags in the BSS segment, we fake it
     by marking in_bss, then looking at s_skip for clues.  */

  subseg_set (bss_section, 0);
  demand_empty_rest_of_line ();
}

static void
s_even (ignore)
     int ignore;
{
  register int temp;
  register long temp_fill;

  temp = 1;			/* JF should be 2? */
  temp_fill = get_absolute_expression ();
  if (!need_pass_2)		/* Never make frag if expect extra pass. */
    frag_align (temp, (int) temp_fill);
  demand_empty_rest_of_line ();
}

static void
s_proc (ignore)
     int ignore;
{
  demand_empty_rest_of_line ();
}

/* Pseudo-ops handled for MRI compatibility.  */

/* Handle an MRI style chip specification.  */

static void
mri_chip ()
{
  char *s;
  char c;
  int i;

  s = input_line_pointer;
  c = get_symbol_end ();
  for (i = 0; i < n_archs; i++)
    if (strcasecmp (s, archs[i].name) == 0)
      break;
  if (i >= n_archs)
    {
      as_bad ("%s: unrecognized processor name", s);
      *input_line_pointer = c;
      ignore_rest_of_line ();
      return;
    }
  *input_line_pointer = c;

  if (*input_line_pointer == '/')
    current_architecture = 0;
  else
    current_architecture &= m68881 | m68851;
  current_architecture |= archs[i].arch;

  while (*input_line_pointer == '/')
    {
      ++input_line_pointer;
      s = input_line_pointer;
      c = get_symbol_end ();
      if (strcmp (s, "68881") == 0)
	current_architecture |= m68881;
      else if (strcmp (s, "68851") == 0)
	current_architecture |= m68851;
      *input_line_pointer = c;
    }
}

/* The MRI CHIP pseudo-op.  */

static void
s_chip (ignore)
     int ignore;
{
  char *stop = NULL;
  char stopc;

  if (flag_mri)
    stop = mri_comment_field (&stopc);
  mri_chip ();
  if (flag_mri)
    mri_comment_end (stop, stopc);
  demand_empty_rest_of_line ();
}

/* The MRI FOPT pseudo-op.  */

static void
s_fopt (ignore)
     int ignore;
{
  SKIP_WHITESPACE ();

  if (strncasecmp (input_line_pointer, "ID=", 3) == 0)
    {
      int temp;

      input_line_pointer += 3;
      temp = get_absolute_expression ();
      if (temp < 0 || temp > 7)
	as_bad ("bad coprocessor id");
      else
	m68k_float_copnum = COP0 + temp;
    }
  else
    {
      as_bad ("unrecognized fopt option");
      ignore_rest_of_line ();
      return;
    }

  demand_empty_rest_of_line ();
}

/* The structure used to handle the MRI OPT pseudo-op.  */

struct opt_action
{
  /* The name of the option.  */
  const char *name;

  /* If this is not NULL, just call this function.  The first argument
     is the ARG field of this structure, the second argument is
     whether the option was negated.  */
  void (*pfn) PARAMS ((int arg, int on));

  /* If this is not NULL, and the PFN field is NULL, set the variable
     this points to.  Set it to the ARG field if the option was not
     negated, and the NOTARG field otherwise.  */
  int *pvar;

  /* The value to pass to PFN or to assign to *PVAR.  */
  int arg;

  /* The value to assign to *PVAR if the option is negated.  If PFN is
     NULL, and PVAR is not NULL, and ARG and NOTARG are the same, then
     the option may not be negated.  */
  int notarg;
};

/* The table used to handle the MRI OPT pseudo-op.  */

static void skip_to_comma PARAMS ((int, int));
static void opt_nest PARAMS ((int, int));
static void opt_chip PARAMS ((int, int));
static void opt_list PARAMS ((int, int));
static void opt_list_symbols PARAMS ((int, int));

static const struct opt_action opt_table[] =
{
  { "abspcadd", 0, &m68k_abspcadd, 1, 0 },

  /* We do relaxing, so there is little use for these options.  */
  { "b", 0, 0, 0, 0 },
  { "brs", 0, 0, 0, 0 },
  { "brb", 0, 0, 0, 0 },
  { "brl", 0, 0, 0, 0 },
  { "brw", 0, 0, 0, 0 },

  { "c", 0, 0, 0, 0 },
  { "cex", 0, 0, 0, 0 },
  { "case", 0, &symbols_case_sensitive, 1, 0 },
  { "cl", 0, 0, 0, 0 },
  { "cre", 0, 0, 0, 0 },
  { "d", 0, &flag_keep_locals, 1, 0 },
  { "e", 0, 0, 0, 0 },
  { "f", 0, &flag_short_refs, 1, 0 },
  { "frs", 0, &flag_short_refs, 1, 0 },
  { "frl", 0, &flag_short_refs, 0, 1 },
  { "g", 0, 0, 0, 0 },
  { "i", 0, 0, 0, 0 },
  { "m", 0, 0, 0, 0 },
  { "mex", 0, 0, 0, 0 },
  { "mc", 0, 0, 0, 0 },
  { "md", 0, 0, 0, 0 },
  { "nest", opt_nest, 0, 0, 0 },
  { "next", skip_to_comma, 0, 0, 0 },
  { "o", 0, 0, 0, 0 },
  { "old", 0, 0, 0, 0 },
  { "op", skip_to_comma, 0, 0, 0 },
  { "pco", 0, 0, 0, 0 },
  { "p", opt_chip, 0, 0, 0 },
  { "pcr", 0, 0, 0, 0 },
  { "pcs", 0, 0, 0, 0 },
  { "r", 0, 0, 0, 0 },
  { "quick", 0, &m68k_quick, 1, 0 },
  { "rel32", 0, &m68k_rel32, 1, 0 },
  { "s", opt_list, 0, 0, 0 },
  { "t", opt_list_symbols, 0, 0, 0 },
  { "w", 0, &flag_no_warnings, 0, 1 },
  { "x", 0, 0, 0, 0 }
};

#define OPTCOUNT (sizeof opt_table / sizeof opt_table[0])

/* The MRI OPT pseudo-op.  */

static void
s_opt (ignore)
     int ignore;
{
  do
    {
      int t;
      char *s;
      char c;
      int i;
      const struct opt_action *o;

      SKIP_WHITESPACE ();

      t = 1;
      if (*input_line_pointer == '-')
	{
	  ++input_line_pointer;
	  t = 0;
	}
      else if (strncasecmp (input_line_pointer, "NO", 2) == 0)
	{
	  input_line_pointer += 2;
	  t = 0;
	}

      s = input_line_pointer;
      c = get_symbol_end ();

      for (i = 0, o = opt_table; i < OPTCOUNT; i++, o++)
	{
	  if (strcasecmp (s, o->name) == 0)
	    {
	      if (o->pfn)
		{
		  /* Restore input_line_pointer now in case the option
		     takes arguments.  */
		  *input_line_pointer = c;
		  (*o->pfn) (o->arg, t);
		}
	      else if (o->pvar != NULL)
		{
		  if (! t && o->arg == o->notarg)
		    as_bad ("option `%s' may not be negated", s);
		  *input_line_pointer = c;
		  *o->pvar = t ? o->arg : o->notarg;
		}
	      else
		*input_line_pointer = c;
	      break;
	    }
	}
      if (i >= OPTCOUNT)
	{
	  as_bad ("option `%s' not recognized", s);
	  *input_line_pointer = c;
	}
    }
  while (*input_line_pointer++ == ',');

  /* Move back to terminating character.  */
  --input_line_pointer;
  demand_empty_rest_of_line ();
}

/* Skip ahead to a comma.  This is used for OPT options which we do
   not suppor tand which take arguments.  */

static void
skip_to_comma (arg, on)
     int arg;
     int on;
{
  while (*input_line_pointer != ','
	 && ! is_end_of_line[(unsigned char) *input_line_pointer])
    ++input_line_pointer;
}

/* Handle the OPT NEST=depth option.  */

static void
opt_nest (arg, on)
     int arg;
     int on;
{
  if (*input_line_pointer != '=')
    {
      as_bad ("bad format of OPT NEST=depth");
      return;
    }

  ++input_line_pointer;
  max_macro_nest = get_absolute_expression ();
}

/* Handle the OPT P=chip option.  */

static void
opt_chip (arg, on)
     int arg;
     int on;
{
  if (*input_line_pointer != '=')
    {
      /* This is just OPT P, which we do not support.  */
      return;
    }

  ++input_line_pointer;
  mri_chip ();
}

/* Handle the OPT S option.  */

static void
opt_list (arg, on)
     int arg;
     int on;
{
  listing_list (on);
}

/* Handle the OPT T option.  */

static void
opt_list_symbols (arg, on)
     int arg;
     int on;
{
  if (on)
    listing |= LISTING_SYMBOLS;
  else
    listing &=~ LISTING_SYMBOLS;
}

/* Handle the MRI REG pseudo-op.  */

static void
s_reg (ignore)
     int ignore;
{
  char *s;
  int c;
  struct m68k_op rop;
  unsigned long mask;
  char *stop = NULL;
  char stopc;

  if (line_label == NULL)
    {
      as_bad ("missing label");
      ignore_rest_of_line ();
      return;
    }

  if (flag_mri)
    stop = mri_comment_field (&stopc);

  SKIP_WHITESPACE ();

  s = input_line_pointer;
  while (isalnum ((unsigned char) *input_line_pointer)
#ifdef REGISTER_PREFIX
	 || *input_line_pointer == REGISTER_PREFIX
#endif
	 || *input_line_pointer == '/'
	 || *input_line_pointer == '-')
    ++input_line_pointer;
  c = *input_line_pointer;
  *input_line_pointer = '\0';

  if (m68k_ip_op (s, &rop) != 0)
    {
      if (rop.error == NULL)
	as_bad ("bad register list");
      else
	as_bad ("bad register list: %s", rop.error);
      *input_line_pointer = c;
      ignore_rest_of_line ();
      return;
    }

  *input_line_pointer = c;

  if (rop.mode == REGLST)
    mask = rop.mask;
  else if (rop.mode == DREG)
    mask = 1 << (rop.reg - DATA0);
  else if (rop.mode == AREG)
    mask = 1 << (rop.reg - ADDR0 + 8);
  else if (rop.mode == FPREG)
    mask = 1 << (rop.reg - FP0 + 16);
  else if (rop.mode == CONTROL
	   && rop.reg == FPI)
    mask = 1 << 24;
  else if (rop.mode == CONTROL
	   && rop.reg == FPS)
    mask = 1 << 25;
  else if (rop.mode == CONTROL
	   && rop.reg == FPC)
    mask = 1 << 26;
  else
    {
      as_bad ("bad register list");
      ignore_rest_of_line ();
      return;
    }

  S_SET_SEGMENT (line_label, absolute_section);
  S_SET_VALUE (line_label, mask);
  line_label->sy_frag = &zero_address_frag;

  if (flag_mri)
    mri_comment_end (stop, stopc);

  demand_empty_rest_of_line ();
}

/* This structure is used for the MRI SAVE and RESTORE pseudo-ops.  */

struct save_opts
{
  struct save_opts *next;
  int abspcadd;
  int symbols_case_sensitive;
  int keep_locals;
  int short_refs;
  int architecture;
  int quick;
  int rel32;
  int listing;
  int no_warnings;
  /* FIXME: We don't save OPT S.  */
};

/* This variable holds the stack of saved options.  */

static struct save_opts *save_stack;

/* The MRI SAVE pseudo-op.  */

static void
s_save (ignore)
     int ignore;
{
  struct save_opts *s;

  s = (struct save_opts *) xmalloc (sizeof (struct save_opts));
  s->abspcadd = m68k_abspcadd;
  s->symbols_case_sensitive = symbols_case_sensitive;
  s->keep_locals = flag_keep_locals;
  s->short_refs = flag_short_refs;
  s->architecture = current_architecture;
  s->quick = m68k_quick;
  s->rel32 = m68k_rel32;
  s->listing = listing;
  s->no_warnings = flag_no_warnings;

  s->next = save_stack;
  save_stack = s;

  demand_empty_rest_of_line ();
}

/* The MRI RESTORE pseudo-op.  */

static void
s_restore (ignore)
     int ignore;
{
  struct save_opts *s;

  if (save_stack == NULL)
    {
      as_bad ("restore without save");
      ignore_rest_of_line ();
      return;
    }

  s = save_stack;
  save_stack = s->next;

  m68k_abspcadd = s->abspcadd;
  symbols_case_sensitive = s->symbols_case_sensitive;
  flag_keep_locals = s->keep_locals;
  flag_short_refs = s->short_refs;
  current_architecture = s->architecture;
  m68k_quick = s->quick;
  m68k_rel32 = s->rel32;
  listing = s->listing;
  flag_no_warnings = s->no_warnings;

  free (s);

  demand_empty_rest_of_line ();
}

/* Types of MRI structured control directives.  */

enum mri_control_type
{
  mri_for,
  mri_if,
  mri_repeat,
  mri_while
};

/* This structure is used to stack the MRI structured control
   directives.  */

struct mri_control_info
{
  /* The directive within which this one is enclosed.  */
  struct mri_control_info *outer;

  /* The type of directive.  */
  enum mri_control_type type;

  /* Whether an ELSE has been in an IF.  */
  int else_seen;

  /* The add or sub statement at the end of a FOR.  */
  char *incr;

  /* The label of the top of a FOR or REPEAT loop.  */
  char *top;

  /* The label to jump to for the next iteration, or the else
     expression of a conditional.  */
  char *next;

  /* The label to jump to to break out of the loop, or the label past
     the end of a conditional.  */
  char *bottom;
};

/* The stack of MRI structured control directives.  */

static struct mri_control_info *mri_control_stack;

/* The current MRI structured control directive index number, used to
   generate label names.  */

static int mri_control_index;

/* Some function prototypes.  */

static char *mri_control_label PARAMS ((void));
static struct mri_control_info *push_mri_control
  PARAMS ((enum mri_control_type));
static void pop_mri_control PARAMS ((void));
static int parse_mri_condition PARAMS ((int *));
static int parse_mri_control_operand
  PARAMS ((int *, char **, char **, char **, char **));
static int swap_mri_condition PARAMS ((int));
static int reverse_mri_condition PARAMS ((int));
static void build_mri_control_operand
  PARAMS ((int, int, char *, char *, char *, char *, const char *,
	   const char *, int));
static void parse_mri_control_expression
  PARAMS ((char *, int, const char *, const char *, int));

/* Generate a new MRI label structured control directive label name.  */

static char *
mri_control_label ()
{
  char *n;

  n = (char *) xmalloc (20);
  sprintf (n, "%smc%d", FAKE_LABEL_NAME, mri_control_index);
  ++mri_control_index;
  return n;
}

/* Create a new MRI structured control directive.  */

static struct mri_control_info *
push_mri_control (type)
     enum mri_control_type type;
{
  struct mri_control_info *n;

  n = (struct mri_control_info *) xmalloc (sizeof (struct mri_control_info));

  n->type = type;
  n->else_seen = 0;
  if (type == mri_if || type == mri_while)
    n->top = NULL;
  else
    n->top = mri_control_label ();
  n->next = mri_control_label ();
  n->bottom = mri_control_label ();

  n->outer = mri_control_stack;
  mri_control_stack = n;

  return n;
}

/* Pop off the stack of MRI structured control directives.  */

static void
pop_mri_control ()
{
  struct mri_control_info *n;

  n = mri_control_stack;
  mri_control_stack = n->outer;
  if (n->top != NULL)
    free (n->top);
  free (n->next);
  free (n->bottom);
  free (n);
}

/* Recognize a condition code in an MRI structured control expression.  */

static int
parse_mri_condition (pcc)
     int *pcc;
{
  char c1, c2;

  know (*input_line_pointer == '<');

  ++input_line_pointer;
  c1 = *input_line_pointer++;
  c2 = *input_line_pointer++;

  if (*input_line_pointer != '>')
    {
      as_bad ("syntax error in structured control directive");
      return 0;
    }

  ++input_line_pointer;
  SKIP_WHITESPACE ();

  if (isupper (c1))
    c1 = tolower (c1);
  if (isupper (c2))
    c2 = tolower (c2);

  *pcc = (c1 << 8) | c2;

  return 1;
}

/* Parse a single operand in an MRI structured control expression.  */

static int
parse_mri_control_operand (pcc, leftstart, leftstop, rightstart, rightstop)
     int *pcc;
     char **leftstart;
     char **leftstop;
     char **rightstart;
     char **rightstop;
{
  char *s;

  SKIP_WHITESPACE ();

  *pcc = -1;
  *leftstart = NULL;
  *leftstop = NULL;
  *rightstart = NULL;
  *rightstop = NULL;

  if (*input_line_pointer == '<')
    {
      /* It's just a condition code.  */
      return parse_mri_condition (pcc);
    }

  /* Look ahead for the condition code.  */
  for (s = input_line_pointer; *s != '\0'; ++s)
    {
      if (*s == '<' && s[1] != '\0' && s[2] != '\0' && s[3] == '>')
	break;
    }
  if (*s == '\0')
    {
      as_bad ("missing condition code in structured control directive");
      return 0;
    }

  *leftstart = input_line_pointer;
  *leftstop = s;
  if (*leftstop > *leftstart
      && ((*leftstop)[-1] == ' ' || (*leftstop)[-1] == '\t'))
    --*leftstop;

  input_line_pointer = s;
  if (! parse_mri_condition (pcc))
    return 0;

  /* Look ahead for AND or OR or end of line.  */
  for (s = input_line_pointer; *s != '\0'; ++s)
    {
      if ((strncasecmp (s, "AND", 3) == 0
	   && (s[3] == '.' || ! is_part_of_name (s[3])))
	  || (strncasecmp (s, "OR", 2) == 0
	      && (s[2] == '.' || ! is_part_of_name (s[2]))))
	break;
    }

  *rightstart = input_line_pointer;
  *rightstop = s;
  if (*rightstop > *rightstart
      && ((*rightstop)[-1] == ' ' || (*rightstop)[-1] == '\t'))
    --*rightstop;

  input_line_pointer = s;

  return 1;
}

#define MCC(b1, b2) (((b1) << 8) | (b2))

/* Swap the sense of a condition.  This changes the condition so that
   it generates the same result when the operands are swapped.  */

static int
swap_mri_condition (cc)
     int cc;
{
  switch (cc)
    {
    case MCC ('h', 'i'): return MCC ('c', 's');
    case MCC ('l', 's'): return MCC ('c', 'c');
    case MCC ('c', 'c'): return MCC ('l', 's');
    case MCC ('c', 's'): return MCC ('h', 'i');
    case MCC ('p', 'l'): return MCC ('m', 'i');
    case MCC ('m', 'i'): return MCC ('p', 'l');
    case MCC ('g', 'e'): return MCC ('l', 'e');
    case MCC ('l', 't'): return MCC ('g', 't');
    case MCC ('g', 't'): return MCC ('l', 't');
    case MCC ('l', 'e'): return MCC ('g', 'e');
    }
  return cc;
}

/* Reverse the sense of a condition.  */

static int
reverse_mri_condition (cc)
     int cc;
{
  switch (cc)
    {
    case MCC ('h', 'i'): return MCC ('l', 's');
    case MCC ('l', 's'): return MCC ('h', 'i');
    case MCC ('c', 'c'): return MCC ('c', 's');
    case MCC ('c', 's'): return MCC ('c', 'c');
    case MCC ('n', 'e'): return MCC ('e', 'q');
    case MCC ('e', 'q'): return MCC ('n', 'e');
    case MCC ('v', 'c'): return MCC ('v', 's');
    case MCC ('v', 's'): return MCC ('v', 'c');
    case MCC ('p', 'l'): return MCC ('m', 'i');
    case MCC ('m', 'i'): return MCC ('p', 'l');
    case MCC ('g', 'e'): return MCC ('l', 't');
    case MCC ('l', 't'): return MCC ('g', 'e');
    case MCC ('g', 't'): return MCC ('l', 'e');
    case MCC ('l', 'e'): return MCC ('g', 't');
    }
  return cc;
}

/* Build an MRI structured control expression.  This generates test
   and branch instructions.  It goes to TRUELAB if the condition is
   true, and to FALSELAB if the condition is false.  Exactly one of
   TRUELAB and FALSELAB will be NULL, meaning to fall through.  QUAL
   is the size qualifier for the expression.  EXTENT is the size to
   use for the branch.  */

static void
build_mri_control_operand (qual, cc, leftstart, leftstop, rightstart,
			   rightstop, truelab, falselab, extent)
     int qual;
     int cc;
     char *leftstart;
     char *leftstop;
     char *rightstart;
     char *rightstop;
     const char *truelab;
     const char *falselab;
     int extent;
{
  char *buf;
  char *s;

  if (leftstart != NULL)
    {
      struct m68k_op leftop, rightop;
      char c;

      /* Swap the compare operands, if necessary, to produce a legal
	 m68k compare instruction.  Comparing a register operand with
	 a non-register operand requires the register to be on the
	 right (cmp, cmpa).  Comparing an immediate value with
	 anything requires the immediate value to be on the left
	 (cmpi).  */

      c = *leftstop;
      *leftstop = '\0';
      (void) m68k_ip_op (leftstart, &leftop);
      *leftstop = c;

      c = *rightstop;
      *rightstop = '\0';
      (void) m68k_ip_op (rightstart, &rightop);
      *rightstop = c;

      if (rightop.mode == IMMED
	  || ((leftop.mode == DREG || leftop.mode == AREG)
	      && (rightop.mode != DREG && rightop.mode != AREG)))
	{
	  char *temp;

	  cc = swap_mri_condition (cc);
	  temp = leftstart;
	  leftstart = rightstart;
	  rightstart = temp;
	  temp = leftstop;
	  leftstop = rightstop;
	  rightstop = temp;
	}
    }

  if (truelab == NULL)
    {
      cc = reverse_mri_condition (cc);
      truelab = falselab;
    }
      
  if (leftstart != NULL)
    {
      buf = (char *) xmalloc (20
			      + (leftstop - leftstart)
			      + (rightstop - rightstart));
      s = buf;
      *s++ = 'c';
      *s++ = 'm';
      *s++ = 'p';
      if (qual != '\0')
	*s++ = qual;
      *s++ = ' ';
      memcpy (s, leftstart, leftstop - leftstart);
      s += leftstop - leftstart;
      *s++ = ',';
      memcpy (s, rightstart, rightstop - rightstart);
      s += rightstop - rightstart;
      *s = '\0';
      md_assemble (buf);
      free (buf);
    }
      
  buf = (char *) xmalloc (20 + strlen (truelab));
  s = buf;
  *s++ = 'b';
  *s++ = cc >> 8;
  *s++ = cc & 0xff;
  if (extent != '\0')
    *s++ = extent;
  *s++ = ' ';
  strcpy (s, truelab);
  md_assemble (buf);
  free (buf);
}

/* Parse an MRI structured control expression.  This generates test
   and branch instructions.  STOP is where the expression ends.  It
   goes to TRUELAB if the condition is true, and to FALSELAB if the
   condition is false.  Exactly one of TRUELAB and FALSELAB will be
   NULL, meaning to fall through.  QUAL is the size qualifier for the
   expression.  EXTENT is the size to use for the branch.  */

static void
parse_mri_control_expression (stop, qual, truelab, falselab, extent)
     char *stop;
     int qual;
     const char *truelab;
     const char *falselab;
     int extent;
{
  int c;
  int cc;
  char *leftstart;
  char *leftstop;
  char *rightstart;
  char *rightstop;

  c = *stop;
  *stop = '\0';

  if (! parse_mri_control_operand (&cc, &leftstart, &leftstop,
				   &rightstart, &rightstop))
    {
      *stop = c;
      return;
    }

  if (strncasecmp (input_line_pointer, "AND", 3) == 0)
    {
      const char *flab;

      if (falselab != NULL)
	flab = falselab;
      else
	flab = mri_control_label ();

      build_mri_control_operand (qual, cc, leftstart, leftstop, rightstart,
				 rightstop, (const char *) NULL, flab, extent);

      input_line_pointer += 3;
      if (*input_line_pointer != '.'
	  || input_line_pointer[1] == '\0')
	qual = '\0';
      else
	{
	  qual = input_line_pointer[1];
	  input_line_pointer += 2;
	}

      if (! parse_mri_control_operand (&cc, &leftstart, &leftstop,
				       &rightstart, &rightstop))
	{
	  *stop = c;
	  return;
	}

      build_mri_control_operand (qual, cc, leftstart, leftstop, rightstart,
				 rightstop, truelab, falselab, extent);

      if (falselab == NULL)
	colon (flab);
    }
  else if (strncasecmp (input_line_pointer, "OR", 2) == 0)
    {
      const char *tlab;

      if (truelab != NULL)
	tlab = truelab;
      else
	tlab = mri_control_label ();

      build_mri_control_operand (qual, cc, leftstart, leftstop, rightstart,
				 rightstop, tlab, (const char *) NULL, extent);

      input_line_pointer += 2;
      if (*input_line_pointer != '.'
	  || input_line_pointer[1] == '\0')
	qual = '\0';
      else
	{
	  qual = input_line_pointer[1];
	  input_line_pointer += 2;
	}

      if (! parse_mri_control_operand (&cc, &leftstart, &leftstop,
				       &rightstart, &rightstop))
	{
	  *stop = c;
	  return;
	}

      build_mri_control_operand (qual, cc, leftstart, leftstop, rightstart,
				 rightstop, truelab, falselab, extent);

      if (truelab == NULL)
	colon (tlab);
    }
  else
    {
      build_mri_control_operand (qual, cc, leftstart, leftstop, rightstart,
				 rightstop, truelab, falselab, extent);
    }

  *stop = c;
  if (input_line_pointer != stop)
    as_bad ("syntax error in structured control directive");
}

/* Handle the MRI IF pseudo-op.  This may be a structured control
   directive, or it may be a regular assembler conditional, depending
   on its operands.  */

static void
s_mri_if (qual)
     int qual;
{
  char *s;
  int c;
  struct mri_control_info *n;

  /* A structured control directive must end with THEN with an
     optional qualifier.  */
  s = input_line_pointer;
  while (! is_end_of_line[(unsigned char) *s]
	 && (! flag_mri || *s != '*'))
    ++s;
  --s;
  while (s > input_line_pointer && (*s == ' ' || *s == '\t'))
    --s;

  if (s - input_line_pointer > 1
      && s[-1] == '.')
    s -= 2;

  if (s - input_line_pointer < 3
      || strncasecmp (s - 3, "THEN", 4) != 0)
    {
      if (qual != '\0')
	{
	  as_bad ("missing then");
	  ignore_rest_of_line ();
	  return;
	}

      /* It's a conditional.  */
      s_if (O_ne);
      return;
    }

  /* Since this might be a conditional if, this pseudo-op will be
     called even if we are supported to be ignoring input.  Double
     check now.  Clobber *input_line_pointer so that ignore_input
     thinks that this is not a special pseudo-op.  */
  c = *input_line_pointer;
  *input_line_pointer = 0;
  if (ignore_input ())
    {
      *input_line_pointer = c;
      while (! is_end_of_line[(unsigned char) *input_line_pointer])
	++input_line_pointer;
      demand_empty_rest_of_line ();
      return;
    }
  *input_line_pointer = c;

  n = push_mri_control (mri_if);

  parse_mri_control_expression (s - 3, qual, (const char *) NULL,
				n->next, s[1] == '.' ? s[2] : '\0');

  if (s[1] == '.')
    input_line_pointer = s + 3;
  else
    input_line_pointer = s + 1;

  if (flag_mri)
    {
      while (! is_end_of_line[(unsigned char) *input_line_pointer])
	++input_line_pointer;
    }

  demand_empty_rest_of_line ();
}

/* Handle the MRI else pseudo-op.  If we are currently doing an MRI
   structured IF, associate the ELSE with the IF.  Otherwise, assume
   it is a conditional else.  */

static void
s_mri_else (qual)
     int qual;
{
  int c;
  char *buf;
  char q[2];

  if (qual == '\0'
      && (mri_control_stack == NULL
	  || mri_control_stack->type != mri_if
	  || mri_control_stack->else_seen))
    {
      s_else (0);
      return;
    }

  c = *input_line_pointer;
  *input_line_pointer = 0;
  if (ignore_input ())
    {
      *input_line_pointer = c;
      while (! is_end_of_line[(unsigned char) *input_line_pointer])
	++input_line_pointer;
      demand_empty_rest_of_line ();
      return;
    }
  *input_line_pointer = c;

  if (mri_control_stack == NULL
      || mri_control_stack->type != mri_if
      || mri_control_stack->else_seen)
    {
      as_bad ("else without matching if");
      ignore_rest_of_line ();
      return;
    }

  mri_control_stack->else_seen = 1;

  buf = (char *) xmalloc (20 + strlen (mri_control_stack->bottom));
  q[0] = qual;
  q[1] = '\0';
  sprintf (buf, "bra%s %s", q, mri_control_stack->bottom);
  md_assemble (buf);
  free (buf);

  colon (mri_control_stack->next);

  if (flag_mri)
    {
      while (! is_end_of_line[(unsigned char) *input_line_pointer])
	++input_line_pointer;
    }

  demand_empty_rest_of_line ();
}

/* Handle the MRI ENDI pseudo-op.  */

static void
s_mri_endi (ignore)
     int ignore;
{
  if (mri_control_stack == NULL
      || mri_control_stack->type != mri_if)
    {
      as_bad ("endi without matching if");
      ignore_rest_of_line ();
      return;
    }

  /* ignore_input will not return true for ENDI, so we don't need to
     worry about checking it again here.  */

  if (! mri_control_stack->else_seen)
    colon (mri_control_stack->next);
  colon (mri_control_stack->bottom);

  pop_mri_control ();

  if (flag_mri)
    {
      while (! is_end_of_line[(unsigned char) *input_line_pointer])
	++input_line_pointer;
    }

  demand_empty_rest_of_line ();
}

/* Handle the MRI BREAK pseudo-op.  */

static void
s_mri_break (extent)
     int extent;
{
  struct mri_control_info *n;
  char *buf;
  char ex[2];

  n = mri_control_stack;
  while (n != NULL
	 && n->type != mri_for
	 && n->type != mri_repeat
	 && n->type != mri_while)
    n = n->outer;
  if (n == NULL)
    {
      as_bad ("break outside of structured loop");
      ignore_rest_of_line ();
      return;
    }

  buf = (char *) xmalloc (20 + strlen (n->bottom));
  ex[0] = extent;
  ex[1] = '\0';
  sprintf (buf, "bra%s %s", ex, n->bottom);
  md_assemble (buf);
  free (buf);

  if (flag_mri)
    {
      while (! is_end_of_line[(unsigned char) *input_line_pointer])
	++input_line_pointer;
    }

  demand_empty_rest_of_line ();
}

/* Handle the MRI NEXT pseudo-op.  */

static void
s_mri_next (extent)
     int extent;
{
  struct mri_control_info *n;
  char *buf;
  char ex[2];

  n = mri_control_stack;
  while (n != NULL
	 && n->type != mri_for
	 && n->type != mri_repeat
	 && n->type != mri_while)
    n = n->outer;
  if (n == NULL)
    {
      as_bad ("next outside of structured loop");
      ignore_rest_of_line ();
      return;
    }

  buf = (char *) xmalloc (20 + strlen (n->next));
  ex[0] = extent;
  ex[1] = '\0';
  sprintf (buf, "bra%s %s", ex, n->next);
  md_assemble (buf);
  free (buf);

  if (flag_mri)
    {
      while (! is_end_of_line[(unsigned char) *input_line_pointer])
	++input_line_pointer;
    }

  demand_empty_rest_of_line ();
}

/* Handle the MRI FOR pseudo-op.  */

static void
s_mri_for (qual)
     int qual;
{
  const char *varstart, *varstop;
  const char *initstart, *initstop;
  const char *endstart, *endstop;
  const char *bystart, *bystop;
  int up;
  int by;
  int extent;
  struct mri_control_info *n;
  char *buf;
  char *s;
  char ex[2];

  /* The syntax is
       FOR.q var = init { TO | DOWNTO } end [ BY by ] DO.e
     */

  SKIP_WHITESPACE ();
  varstart = input_line_pointer;

  /* Look for the '='.  */
  while (! is_end_of_line[(unsigned char) *input_line_pointer]
	 && *input_line_pointer != '=')
    ++input_line_pointer;
  if (*input_line_pointer != '=')
    {
      as_bad ("missing =");
      ignore_rest_of_line ();
      return;
    }

  varstop = input_line_pointer;
  if (varstop > varstart
      && (varstop[-1] == ' ' || varstop[-1] == '\t'))
    --varstop;

  ++input_line_pointer;

  initstart = input_line_pointer;

  /* Look for TO or DOWNTO.  */
  up = 1;
  initstop = NULL;
  while (! is_end_of_line[(unsigned char) *input_line_pointer])
    {
      if (strncasecmp (input_line_pointer, "TO", 2) == 0
	  && ! is_part_of_name (input_line_pointer[2]))
	{
	  initstop = input_line_pointer;
	  input_line_pointer += 2;
	  break;
	}
      if (strncasecmp (input_line_pointer, "DOWNTO", 6) == 0
	  && ! is_part_of_name (input_line_pointer[6]))
	{
	  initstop = input_line_pointer;
	  up = 0;
	  input_line_pointer += 6;
	  break;
	}
      ++input_line_pointer;
    }
  if (initstop == NULL)
    {
      as_bad ("missing to or downto");
      ignore_rest_of_line ();
      return;
    }
  if (initstop > initstart
      && (initstop[-1] == ' ' || initstop[-1] == '\t'))
    --initstop;

  SKIP_WHITESPACE ();
  endstart = input_line_pointer;

  /* Look for BY or DO.  */
  by = 0;
  endstop = NULL;
  while (! is_end_of_line[(unsigned char) *input_line_pointer])
    {
      if (strncasecmp (input_line_pointer, "BY", 2) == 0
	  && ! is_part_of_name (input_line_pointer[2]))
	{
	  endstop = input_line_pointer;
	  by = 1;
	  input_line_pointer += 2;
	  break;
	}
      if (strncasecmp (input_line_pointer, "DO", 2) == 0
	  && (input_line_pointer[2] == '.'
	      || ! is_part_of_name (input_line_pointer[2])))
	{
	  endstop = input_line_pointer;
	  input_line_pointer += 2;
	  break;
	}
      ++input_line_pointer;
    }
  if (endstop == NULL)
    {
      as_bad ("missing do");
      ignore_rest_of_line ();
      return;
    }
  if (endstop > endstart
      && (endstop[-1] == ' ' || endstop[-1] == '\t'))
    --endstop;

  if (! by)
    {
      bystart = "#1";
      bystop = bystart + 2;
    }
  else
    {
      SKIP_WHITESPACE ();
      bystart = input_line_pointer;

      /* Look for DO.  */
      bystop = NULL;
      while (! is_end_of_line[(unsigned char) *input_line_pointer])
	{
	  if (strncasecmp (input_line_pointer, "DO", 2) == 0
	      && (input_line_pointer[2] == '.'
		  || ! is_part_of_name (input_line_pointer[2])))
	    {
	      bystop = input_line_pointer;
	      input_line_pointer += 2;
	      break;
	    }
	  ++input_line_pointer;
	}
      if (bystop == NULL)
	{
	  as_bad ("missing do");
	  ignore_rest_of_line ();
	  return;
	}
      if (bystop > bystart
	  && (bystop[-1] == ' ' || bystop[-1] == '\t'))
	--bystop;
    }

  if (*input_line_pointer != '.')
    extent = '\0';
  else
    {
      extent = input_line_pointer[1];
      input_line_pointer += 2;
    }

  /* We have fully parsed the FOR operands.  Now build the loop.  */

  n = push_mri_control (mri_for);

  buf = (char *) xmalloc (50 + (input_line_pointer - varstart));

  /* move init,var */
  s = buf;
  *s++ = 'm';
  *s++ = 'o';
  *s++ = 'v';
  *s++ = 'e';
  if (qual != '\0')
    *s++ = qual;
  *s++ = ' ';
  memcpy (s, initstart, initstop - initstart);
  s += initstop - initstart;
  *s++ = ',';
  memcpy (s, varstart, varstop - varstart);
  s += varstop - varstart;
  *s = '\0';
  md_assemble (buf);

  colon (n->top);

  /* cmp end,var */
  s = buf;
  *s++ = 'c';
  *s++ = 'm';
  *s++ = 'p';
  if (qual != '\0')
    *s++ = qual;
  *s++ = ' ';
  memcpy (s, endstart, endstop - endstart);
  s += endstop - endstart;
  *s++ = ',';
  memcpy (s, varstart, varstop - varstart);
  s += varstop - varstart;
  *s = '\0';
  md_assemble (buf);

  /* bcc bottom */
  ex[0] = extent;
  ex[1] = '\0';
  if (up)
    sprintf (buf, "blt%s %s", ex, n->bottom);
  else
    sprintf (buf, "bgt%s %s", ex, n->bottom);
  md_assemble (buf);

  /* Put together the add or sub instruction used by ENDF.  */
  s = buf;
  if (up)
    strcpy (s, "add");
  else
    strcpy (s, "sub");
  s += 3;
  if (qual != '\0')
    *s++ = qual;
  *s++ = ' ';
  memcpy (s, bystart, bystop - bystart);
  s += bystop - bystart;
  *s++ = ',';
  memcpy (s, varstart, varstop - varstart);
  s += varstop - varstart;
  *s = '\0';
  n->incr = buf;

  if (flag_mri)
    {
      while (! is_end_of_line[(unsigned char) *input_line_pointer])
	++input_line_pointer;
    }

  demand_empty_rest_of_line ();
}

/* Handle the MRI ENDF pseudo-op.  */

static void
s_mri_endf (ignore)
     int ignore;
{
  if (mri_control_stack == NULL
      || mri_control_stack->type != mri_for)
    {
      as_bad ("endf without for");
      ignore_rest_of_line ();
      return;
    }

  colon (mri_control_stack->next);

  md_assemble (mri_control_stack->incr);

  sprintf (mri_control_stack->incr, "bra %s", mri_control_stack->top);
  md_assemble (mri_control_stack->incr);

  free (mri_control_stack->incr);

  colon (mri_control_stack->bottom);

  pop_mri_control ();

  if (flag_mri)
    {
      while (! is_end_of_line[(unsigned char) *input_line_pointer])
	++input_line_pointer;
    }

  demand_empty_rest_of_line ();
}

/* Handle the MRI REPEAT pseudo-op.  */

static void
s_mri_repeat (ignore)
     int ignore;
{
  struct mri_control_info *n;

  n = push_mri_control (mri_repeat);
  colon (n->top);
  if (flag_mri)
    {
      while (! is_end_of_line[(unsigned char) *input_line_pointer])
	++input_line_pointer;
    }
  demand_empty_rest_of_line ();
}

/* Handle the MRI UNTIL pseudo-op.  */

static void
s_mri_until (qual)
     int qual;
{
  char *s;

  if (mri_control_stack == NULL
      || mri_control_stack->type != mri_repeat)
    {
      as_bad ("until without repeat");
      ignore_rest_of_line ();
      return;
    }

  colon (mri_control_stack->next);

  for (s = input_line_pointer; ! is_end_of_line[(unsigned char) *s]; s++)
    ;

  parse_mri_control_expression (s, qual, (const char *) NULL,
				mri_control_stack->top, '\0');

  colon (mri_control_stack->bottom);

  input_line_pointer = s;

  pop_mri_control ();

  if (flag_mri)
    {
      while (! is_end_of_line[(unsigned char) *input_line_pointer])
	++input_line_pointer;
    }

  demand_empty_rest_of_line ();
}

/* Handle the MRI WHILE pseudo-op.  */

static void
s_mri_while (qual)
     int qual;
{
  char *s;

  struct mri_control_info *n;

  s = input_line_pointer;
  while (! is_end_of_line[(unsigned char) *s]
	 && (! flag_mri || *s != '*'))
    s++;
  --s;
  while (*s == ' ' || *s == '\t')
    --s;
  if (s - input_line_pointer > 1
      && s[-1] == '.')
    s -= 2;
  if (s - input_line_pointer < 2
      || strncasecmp (s - 1, "DO", 2) != 0)
    {
      as_bad ("missing do");
      ignore_rest_of_line ();
      return;
    }

  n = push_mri_control (mri_while);

  colon (n->next);

  parse_mri_control_expression (s - 1, qual, (const char *) NULL, n->bottom,
				s[1] == '.' ? s[2] : '\0');

  input_line_pointer = s + 1;
  if (*input_line_pointer == '.')
    input_line_pointer += 2;

  if (flag_mri)
    {
      while (! is_end_of_line[(unsigned char) *input_line_pointer])
	++input_line_pointer;
    }

  demand_empty_rest_of_line ();
}

/* Handle the MRI ENDW pseudo-op.  */

static void
s_mri_endw (ignore)
     int ignore;
{
  char *buf;

  if (mri_control_stack == NULL
      || mri_control_stack->type != mri_while)
    {
      as_bad ("endw without while");
      ignore_rest_of_line ();
      return;
    }

  buf = (char *) xmalloc (20 + strlen (mri_control_stack->next));
  sprintf (buf, "bra %s", mri_control_stack->next);
  md_assemble (buf);
  free (buf);

  colon (mri_control_stack->bottom);

  pop_mri_control ();

  if (flag_mri)
    {
      while (! is_end_of_line[(unsigned char) *input_line_pointer])
	++input_line_pointer;
    }

  demand_empty_rest_of_line ();
}

/*
 * md_parse_option
 *	Invocation line includes a switch not recognized by the base assembler.
 *	See if it's a processor-specific option.  These are:
 *
 *	-[A]m[c]68000, -[A]m[c]68008, -[A]m[c]68010, -[A]m[c]68020, -[A]m[c]68030, -[A]m[c]68040
 *	-[A]m[c]68881, -[A]m[c]68882, -[A]m[c]68851
 *		Select the architecture.  Instructions or features not
 *		supported by the selected architecture cause fatal
 *		errors.  More than one may be specified.  The default is
 *		-m68020 -m68851 -m68881.  Note that -m68008 is a synonym
 *		for -m68000, and -m68882 is a synonym for -m68881.
 *	-[A]m[c]no-68851, -[A]m[c]no-68881
 *		Don't accept 688?1 instructions.  (The "c" is kind of silly,
 *		so don't use or document it, but that's the way the parsing
 *		works).
 *
 *	-pic	Indicates PIC.
 *	-k	Indicates PIC.  (Sun 3 only.)
 *
 */

#ifdef OBJ_ELF
CONST char *md_shortopts = "lSA:m:kQ:V";
#else
CONST char *md_shortopts = "lSA:m:k";
#endif

struct option md_longopts[] = {
#define OPTION_PIC (OPTION_MD_BASE)
  {"pic", no_argument, NULL, OPTION_PIC},
#define OPTION_REGISTER_PREFIX_OPTIONAL (OPTION_MD_BASE + 1)
  {"register-prefix-optional", no_argument, NULL,
     OPTION_REGISTER_PREFIX_OPTIONAL},
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
    case 'l':			/* -l means keep external to 2 bit offset
				   rather than 16 bit one */
      flag_short_refs = 1;
      break;

    case 'S':			/* -S means that jbsr's always turn into
				   jsr's.  */
      flag_long_jumps = 1;
      break;

    case 'A':
      if (*arg == 'm')
 	arg++;
      /* intentional fall-through */
    case 'm':

      if (arg[0] == 'n' && arg[1] == 'o' && arg[2] == '-')
	{
	  int i;
	  unsigned long arch;
	  const char *oarg = arg;

	  arg += 3;
	  if (*arg == 'm')
	    {
	      arg++;
	      if (arg[0] == 'c' && arg[1] == '6')
		arg++;
	    }
	  for (i = 0; i < n_archs; i++)
	    if (!strcmp (arg, archs[i].name))
	      break;
	  if (i == n_archs)
	    {
	    unknown:
	      as_bad ("unrecognized option `%s'", oarg);
	      return 0;
	    }
	  arch = archs[i].arch;
	  if (arch == m68881)
	    no_68881 = 1;
	  else if (arch == m68851)
	    no_68851 = 1;
	  else
	    goto unknown;
	}
      else
	{
	  int i;

	  if (arg[0] == 'c' && arg[1] == '6')
	    arg++;

	  for (i = 0; i < n_archs; i++)
	    if (!strcmp (arg, archs[i].name))
	      {
		unsigned long arch = archs[i].arch;
		if (cpu_of_arch (arch))
		  /* It's a cpu spec.  */
		  {
		    current_architecture &= ~m68000up;
		    current_architecture |= arch;
		  }
		else if (arch == m68881)
		  {
		    current_architecture |= m68881;
		    no_68881 = 0;
		  }
		else if (arch == m68851)
		  {
		    current_architecture |= m68851;
		    no_68851 = 0;
		  }
		else
		  /* ??? */
		  abort ();
		break;
	      }
	  if (i == n_archs)
	    {
	      as_bad ("unrecognized architecture specification `%s'", arg);
	      return 0;
	    }
	}
      break;

    case OPTION_PIC:
    case 'k':
      flag_want_pic = 1;
      break;			/* -pic, Position Independent Code */

    case OPTION_REGISTER_PREFIX_OPTIONAL:
      flag_reg_prefix_optional = 1;
      break;

    case 'Q':
    case 'V':
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
680X0 options:\n\
-l			use 1 word for refs to undefined symbols [default 2]\n\
-m68000 | -m68008 | -m68010 | -m68020 | -m68030 | -m68040 | -m68060\n\
 | -m68302 | -m68331 | -m68332 | -m68333 | -m68340 | -m68360\n\
 | -mcpu32\n\
			specify variant of 680X0 architecture [default 68020]\n\
-m68881 | -m68882 | -mno-68881 | -mno-68882\n\
			target has/lacks floating-point coprocessor\n\
			[default yes for 68020, 68030, and cpu32]\n");
  fprintf(stream, "\
-m68851 | -mno-68851\n\
			target has/lacks memory-management unit coprocessor\n\
			[default yes for 68020 and up]\n\
-pic, -k		generate position independent code\n\
-S			turn jbsr into jsr\n\
--register-prefix-optional\n\
			recognize register names without prefix character\n");
}

#ifdef TEST2

/* TEST2:  Test md_assemble() */
/* Warning, this routine probably doesn't work anymore */

main ()
{
  struct m68k_it the_ins;
  char buf[120];
  char *cp;
  int n;

  m68k_ip_begin ();
  for (;;)
    {
      if (!gets (buf) || !*buf)
	break;
      if (buf[0] == '|' || buf[1] == '.')
	continue;
      for (cp = buf; *cp; cp++)
	if (*cp == '\t')
	  *cp = ' ';
      if (is_label (buf))
	continue;
      memset (&the_ins, '\0', sizeof (the_ins));
      m68k_ip (&the_ins, buf);
      if (the_ins.error)
	{
	  printf ("Error %s in %s\n", the_ins.error, buf);
	}
      else
	{
	  printf ("Opcode(%d.%s): ", the_ins.numo, the_ins.args);
	  for (n = 0; n < the_ins.numo; n++)
	    printf (" 0x%x", the_ins.opcode[n] & 0xffff);
	  printf ("    ");
	  print_the_insn (&the_ins.opcode[0], stdout);
	  (void) putchar ('\n');
	}
      for (n = 0; n < strlen (the_ins.args) / 2; n++)
	{
	  if (the_ins.operands[n].error)
	    {
	      printf ("op%d Error %s in %s\n", n, the_ins.operands[n].error, buf);
	      continue;
	    }
	  printf ("mode %d, reg %d, ", the_ins.operands[n].mode, the_ins.operands[n].reg);
	  if (the_ins.operands[n].b_const)
	    printf ("Constant: '%.*s', ", 1 + the_ins.operands[n].e_const - the_ins.operands[n].b_const, the_ins.operands[n].b_const);
	  printf ("ireg %d, isiz %d, imul %d, ", the_ins.operands[n].ireg, the_ins.operands[n].isiz, the_ins.operands[n].imul);
	  if (the_ins.operands[n].b_iadd)
	    printf ("Iadd: '%.*s',", 1 + the_ins.operands[n].e_iadd - the_ins.operands[n].b_iadd, the_ins.operands[n].b_iadd);
	  (void) putchar ('\n');
	}
    }
  m68k_ip_end ();
  return 0;
}

is_label (str)
     char *str;
{
  while (*str == ' ')
    str++;
  while (*str && *str != ' ')
    str++;
  if (str[-1] == ':' || str[1] == '=')
    return 1;
  return 0;
}

#endif

/* Possible states for relaxation:

   0 0	branch offset	byte	(bra, etc)
   0 1			word
   0 2			long

   1 0	indexed offsets	byte	a0@(32,d4:w:1) etc
   1 1			word
   1 2			long

   2 0	two-offset index word-word a0@(32,d4)@(45) etc
   2 1			word-long
   2 2			long-word
   2 3			long-long

   */

/* We have no need to default values of symbols.  */

/* ARGSUSED */
symbolS *
md_undefined_symbol (name)
     char *name;
{
  return 0;
}

/* Round up a section size to the appropriate boundary.  */
valueT
md_section_align (segment, size)
     segT segment;
     valueT size;
{
  return size;			/* Byte alignment is fine */
}

/* Exactly what point is a PC-relative offset relative TO?
   On the 68k, it is relative to the address of the first extension
   word.  The difference between the addresses of the offset and the
   first extension word is stored in fx_pcrel_adjust. */
long
md_pcrel_from (fixP)
     fixS *fixP;
{
  int adjust;

  /* Because fx_pcrel_adjust is a char, and may be unsigned, we store
     -1 as 64.  */
  adjust = fixP->fx_pcrel_adjust;
  if (adjust == 64)
    adjust = -1;
  return fixP->fx_where + fixP->fx_frag->fr_address - adjust;
}

#ifndef BFD_ASSEMBLER
/*ARGSUSED*/
void
tc_coff_symbol_emit_hook (ignore)
     symbolS *ignore;
{
}

int
tc_coff_sizemachdep (frag)
     fragS *frag;
{
  switch (frag->fr_subtype & 0x3)
    {
    case BYTE:
      return 1;
    case SHORT:
      return 2;
    case LONG:
      return 4;
    default:
      abort ();
      return 0;
    }
}
#endif

/* end of tc-m68k.c */
