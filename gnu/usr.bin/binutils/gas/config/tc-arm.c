/* tc-arm.c  All the arm specific stuff in one convenient, huge,
   slow to compile, easy to find file.
   Contributed by Richard Earnshaw (rwe@pegasus.esprit.ec.org)
	Modified by David Taylor (dtaylor@armltd.co.uk)

   Copyright (C) 1994, 1995 Free Software Foundation, Inc.

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

#include <ctype.h>
#include <string.h>
#define  NO_RELOC 0
#include "as.h"

/* need TARGET_CPU */
#include "config.h"
#include "subsegs.h"
#include "obstack.h"
#include "symbols.h"
#include "listing.h"

/* ??? This is currently unused.  */
#ifdef __STDC__
#define internalError() \
  as_fatal ("ARM Internal Error, line %d, %s", __LINE__, __FILE__)
#else
#define internalError() as_fatal ("ARM Internal Error")
#endif

/* Types of processor to assemble for.  */
#define ARM_1		0x00000001
#define ARM_2		0x00000002
#define ARM_3		0x00000004
#define ARM_250		ARM_3
#define ARM_6		0x00000008
#define ARM_7		ARM_6           /* same core instruction set */

/* The following bitmasks control CPU extensions (ARM7 onwards): */
#define ARM_LONGMUL	0x00000010	/* allow long multiplies */
#define ARM_ARCH4       0x00000020
#define ARM_THUMB       ARM_ARCH4

/* Some useful combinations:  */
#define ARM_ANY		0x00ffffff
#define ARM_2UP		0x00fffffe
#define ARM_ALL		ARM_2UP		/* Not arm1 only */
#define ARM_3UP		0x00fffffc
#define ARM_6UP		0x00fffff8      /* Includes ARM7 */

#define FPU_CORE	0x80000000
#define FPU_FPA10	0x40000000
#define FPU_FPA11	0x40000000
#define FPU_NONE	0

/* Some useful combinations  */
#define FPU_ALL		0xff000000	/* Note this is ~ARM_ANY */
#define FPU_MEMMULTI	0x7f000000	/* Not fpu_core */

#ifndef CPU_DEFAULT
#define CPU_DEFAULT ARM_ALL
#endif

#ifndef FPU_DEFAULT
#define FPU_DEFAULT FPU_ALL
#endif

unsigned long cpu_variant = CPU_DEFAULT | FPU_DEFAULT;

/* This array holds the chars that always start a comment.  If the
   pre-processor is disabled, these aren't very useful */
CONST char comment_chars[] = "@";

/* This array holds the chars that only start a comment at the beginning of
   a line.  If the line seems to have the form '# 123 filename'
   .line and .file directives will appear in the pre-processed output */
/* Note that input_file.c hand checks for '#' at the beginning of the
   first line of the input file.  This is because the compiler outputs
   #NO_APP at the beginning of its output. */
/* Also note that comments like this one will always work. */
CONST char line_comment_chars[] = "#";

CONST char line_separator_chars[] = "";

/* Chars that can be used to separate mant from exp in floating point nums */
CONST char EXP_CHARS[] = "eE";

/* Chars that mean this number is a floating point constant */
/* As in 0f12.456 */
/* or    0d1.2345e12 */

CONST char FLT_CHARS[] = "rRsSfFdDxXeEpP";

CONST int md_reloc_size = 8;		/* Size of relocation record */

static int thumb_mode = 0;      /* non-zero if assembling thumb instructions */

typedef struct arm_fix
{
  int thumb_mode;
} arm_fix_data;

struct arm_it
{
  CONST char *error;
  unsigned long instruction;
  int suffix;
  int size;
  struct
    {
      bfd_reloc_code_real_type type;
      expressionS exp;
      int pc_rel;
    } reloc;
};

struct arm_it inst;

struct asm_shift
{
  CONST char *template;
  unsigned long value;
};

static CONST struct asm_shift shift[] =
{
  {"asl", 0},
  {"lsl", 0},
  {"lsr", 0x00000020},
  {"asr", 0x00000040},
  {"ror", 0x00000060},
  {"rrx", 0x00000060},
  {"ASL", 0},
  {"LSL", 0},
  {"LSR", 0x00000020},
  {"ASR", 0x00000040},
  {"ROR", 0x00000060},
  {"RRX", 0x00000060}
};

#define NO_SHIFT_RESTRICT 1
#define SHIFT_RESTRICT	  0

#define NUM_FLOAT_VALS 8

CONST char *fp_const[] = 
{
  "0.0", "1.0", "2.0", "3.0", "4.0", "5.0", "0.5", "10.0", 0
};

/* Number of littlenums required to hold an extended precision number */
#define MAX_LITTLENUMS 6

LITTLENUM_TYPE fp_values[NUM_FLOAT_VALS][MAX_LITTLENUMS];

#define FAIL	(-1)
#define SUCCESS (0)

#define SUFF_S 1
#define SUFF_D 2
#define SUFF_E 3
#define SUFF_P 4

#define CP_T_X   0x00008000
#define CP_T_Y   0x00400000
#define CP_T_Pre 0x01000000
#define CP_T_UD  0x00800000
#define CP_T_WB  0x00200000

#define CONDS_BIT       (0x00100000)
#define LOAD_BIT        (0x00100000)
#define TRANS_BIT	(0x00200000)

struct asm_cond
{
  CONST char *template;
  unsigned long value;
};

/* This is to save a hash look-up in the common case */
#define COND_ALWAYS 0xe0000000

static CONST struct asm_cond conds[] = 
{
  {"eq", 0x00000000},
  {"ne", 0x10000000},
  {"cs", 0x20000000}, {"hs", 0x20000000},
  {"cc", 0x30000000}, {"ul", 0x30000000}, {"lo", 0x30000000},
  {"mi", 0x40000000},
  {"pl", 0x50000000},
  {"vs", 0x60000000},
  {"vc", 0x70000000},
  {"hi", 0x80000000},
  {"ls", 0x90000000},
  {"ge", 0xa0000000},
  {"lt", 0xb0000000},
  {"gt", 0xc0000000},
  {"le", 0xd0000000},
  {"al", 0xe0000000},
  {"nv", 0xf0000000}
};

/* Warning: If the top bit of the set_bits is set, then the standard
   instruction bitmask is ignored, and the new bitmask is taken from
   the set_bits: */
struct asm_flg
{
  CONST char *template;		/* Basic flag string */
  unsigned long set_bits;	/* Bits to set */
};

static CONST struct asm_flg s_flag[] =
{
  {"s", CONDS_BIT},
  {NULL, 0}
};

static CONST struct asm_flg ldr_flags[] =
{
  {"b",  0x00400000},
  {"t",  TRANS_BIT},
  {"bt", 0x00400000 | TRANS_BIT},
  {"h",  0x801000b0},
  {"sh", 0x801000f0},
  {"sb", 0x801000d0},
  {NULL, 0}
};

static CONST struct asm_flg str_flags[] =
{
  {"b",  0x00400000},
  {"t",  TRANS_BIT},
  {"bt", 0x00400000 | TRANS_BIT},
  {"h",  0x800000b0},
  {NULL, 0}
};

static CONST struct asm_flg byte_flag[] =
{
  {"b", 0x00400000},
  {NULL, 0}
};

static CONST struct asm_flg cmp_flags[] =
{
  {"s", CONDS_BIT},
  {"p", 0x0010f000},
  {NULL, 0}
};

static CONST struct asm_flg ldm_flags[] =
{
  {"ed", 0x01800000},
  {"fd", 0x00800000},
  {"ea", 0x01000000},
  {"fa", 0x08000000},
  {"ib", 0x01800000},
  {"ia", 0x00800000},
  {"db", 0x01000000},
  {"da", 0x08000000},
  {NULL, 0}
};

static CONST struct asm_flg stm_flags[] =
{
  {"ed", 0x08000000},
  {"fd", 0x01000000},
  {"ea", 0x00800000},
  {"fa", 0x01800000},
  {"ib", 0x01800000},
  {"ia", 0x00800000},
  {"db", 0x01000000},
  {"da", 0x08000000},
  {NULL, 0}
};

static CONST struct asm_flg lfm_flags[] =
{
  {"fd", 0x00800000},
  {"ea", 0x01000000},
  {NULL, 0}
};

static CONST struct asm_flg sfm_flags[] =
{
  {"fd", 0x01000000},
  {"ea", 0x00800000},
  {NULL, 0}
};

static CONST struct asm_flg round_flags[] =
{
  {"p", 0x00000020},
  {"m", 0x00000040},
  {"z", 0x00000060},
  {NULL, 0}
};

/* The implementation of the FIX instruction is broken on some assemblers,
   in that it accepts a precision specifier as well as a rounding specifier,
   despite the fact that this is meaningless.  To be more compatible, we
   accept it as well, though of course it does not set any bits.  */
static CONST struct asm_flg fix_flags[] =
{
  {"p", 0x00000020},
  {"m", 0x00000040},
  {"z", 0x00000060},
  {"sp", 0x00000020},
  {"sm", 0x00000040},
  {"sz", 0x00000060},
  {"dp", 0x00000020},
  {"dm", 0x00000040},
  {"dz", 0x00000060},
  {"ep", 0x00000020},
  {"em", 0x00000040},
  {"ez", 0x00000060},
  {NULL, 0}
};

static CONST struct asm_flg except_flag[] =
{
  {"e", 0x00400000},
  {NULL, 0}
};

static CONST struct asm_flg cplong_flag[] =
{
  {"l", 0x00400000},
  {NULL, 0}
};

struct asm_psr
{
  CONST char *template;
  unsigned long number;
};

#define PSR_ALL		0x00010000

static CONST struct asm_psr psrs[] =
{
  /* Valid <psr>'s */
  {"cpsr",	0},
  {"cpsr_all",	0},
  {"spsr",	1},
  {"spsr_all",	1},

  /* Valid <psrf>'s */
  {"cpsr_flg",	2},
  {"spsr_flg",	3}
};

/* Functions called by parser */
/* ARM instructions */
static void do_arit		PARAMS ((char *operands, unsigned long flags));
static void do_cmp		PARAMS ((char *operands, unsigned long flags));
static void do_mov		PARAMS ((char *operands, unsigned long flags));
static void do_ldst		PARAMS ((char *operands, unsigned long flags));
static void do_ldmstm		PARAMS ((char *operands, unsigned long flags));
static void do_branch		PARAMS ((char *operands, unsigned long flags));
static void do_swi		PARAMS ((char *operands, unsigned long flags));
/* Pseudo Op codes */
static void do_adr		PARAMS ((char *operands, unsigned long flags));
static void do_nop		PARAMS ((char *operands, unsigned long flags));
/* ARM 2 */
static void do_mul		PARAMS ((char *operands, unsigned long flags));
static void do_mla		PARAMS ((char *operands, unsigned long flags));
/* ARM 3 */
static void do_swap		PARAMS ((char *operands, unsigned long flags));
/* ARM 6 */
static void do_msr		PARAMS ((char *operands, unsigned long flags));
static void do_mrs		PARAMS ((char *operands, unsigned long flags));
/* ARM 7M */
static void do_mull		PARAMS ((char *operands, unsigned long flags));
/* ARM THUMB */
static void do_bx               PARAMS ((char *operands, unsigned long flags));

/* Coprocessor Instructions */
static void do_cdp		PARAMS ((char *operands, unsigned long flags));
static void do_lstc		PARAMS ((char *operands, unsigned long flags));
static void do_co_reg		PARAMS ((char *operands, unsigned long flags));
static void do_fp_ctrl		PARAMS ((char *operands, unsigned long flags));
static void do_fp_ldst		PARAMS ((char *operands, unsigned long flags));
static void do_fp_ldmstm	PARAMS ((char *operands, unsigned long flags));
static void do_fp_dyadic	PARAMS ((char *operands, unsigned long flags));
static void do_fp_monadic	PARAMS ((char *operands, unsigned long flags));
static void do_fp_cmp		PARAMS ((char *operands, unsigned long flags));
static void do_fp_from_reg	PARAMS ((char *operands, unsigned long flags));
static void do_fp_to_reg	PARAMS ((char *operands, unsigned long flags));

static void fix_new_arm		PARAMS ((fragS *frag, int where, 
					 short int size, expressionS *exp,
					 int pc_rel, int reloc));
static int arm_reg_parse	PARAMS ((char **ccp));
static int arm_psr_parse	PARAMS ((char **ccp));

/* ARM instructions take 4bytes in the object file, Thumb instructions
   take 2: */
#define INSN_SIZE       4

/* LONGEST_INST is the longest basic instruction name without conditions or 
 * flags.
 * ARM7M has 4 of length 5
 */

#define LONGEST_INST 5

struct asm_opcode 
{
  CONST char *template;		/* Basic string to match */
  unsigned long value;		/* Basic instruction code */
  CONST char *comp_suffix;	/* Compulsory suffix that must follow conds */
  CONST struct asm_flg *flags;	/* Bits to toggle if flag 'n' set */
  unsigned long variants;	/* Which CPU variants this exists for */
  void (*parms)();		/* Function to call to parse args */
};

static CONST struct asm_opcode insns[] = 
{
/* ARM Instructions */
  {"and",   0x00000000, NULL,   s_flag,      ARM_ANY,      do_arit},
  {"eor",   0x00200000, NULL,   s_flag,      ARM_ANY,      do_arit},
  {"sub",   0x00400000, NULL,   s_flag,      ARM_ANY,      do_arit},
  {"rsb",   0x00600000, NULL,   s_flag,      ARM_ANY,      do_arit},
  {"add",   0x00800000, NULL,   s_flag,      ARM_ANY,      do_arit},
  {"adc",   0x00a00000, NULL,   s_flag,      ARM_ANY,      do_arit},
  {"sbc",   0x00c00000, NULL,   s_flag,      ARM_ANY,      do_arit},
  {"rsc",   0x00e00000, NULL,   s_flag,      ARM_ANY,      do_arit},
  {"orr",   0x01800000, NULL,   s_flag,      ARM_ANY,      do_arit},
  {"bic",   0x01c00000, NULL,   s_flag,      ARM_ANY,      do_arit},
  {"tst",   0x01000000, NULL,   cmp_flags,   ARM_ANY,      do_cmp},
  {"teq",   0x01200000, NULL,   cmp_flags,   ARM_ANY,      do_cmp},
  {"cmp",   0x01400000, NULL,   cmp_flags,   ARM_ANY,      do_cmp},
  {"cmn",   0x01600000, NULL,   cmp_flags,   ARM_ANY,      do_cmp},
  {"mov",   0x01a00000, NULL,   s_flag,      ARM_ANY,      do_mov},
  {"mvn",   0x01e00000, NULL,   s_flag,      ARM_ANY,      do_mov},
  {"str",   0x04000000, NULL,   str_flags,   ARM_ANY,      do_ldst},
  {"ldr",   0x04100000, NULL,   ldr_flags,   ARM_ANY,      do_ldst},
  {"stm",   0x08000000, NULL,   stm_flags,   ARM_ANY,      do_ldmstm},
  {"ldm",   0x08100000, NULL,   ldm_flags,   ARM_ANY,      do_ldmstm},
  {"swi",   0x0f000000, NULL,   NULL,        ARM_ANY,      do_swi},
  {"bl",    0x0bfffffe, NULL,   NULL,        ARM_ANY,      do_branch},
  {"b",     0x0afffffe, NULL,   NULL,        ARM_ANY,      do_branch},

/* Pseudo ops */
  {"adr",   0x028f0000, NULL,   NULL,        ARM_ANY,      do_adr},
  {"nop",   0x01a00000, NULL,   NULL,        ARM_ANY,      do_nop},

/* ARM 2 multiplies */
  {"mul",   0x00000090, NULL,   s_flag,      ARM_2UP,      do_mul},
  {"mla",   0x00200090, NULL,   s_flag,      ARM_2UP,      do_mla},

/* ARM 3 - swp instructions */
  {"swp",   0x01000090, NULL,   byte_flag,   ARM_3UP,      do_swap},

/* ARM 6 Coprocessor instructions */
  {"mrs",   0x010f0000, NULL,   NULL,        ARM_6UP,      do_mrs},
  {"msr",   0x0128f000, NULL,   NULL,        ARM_6UP,      do_msr},

/* ARM 7M long multiplies - need signed/unsigned flags! */
  {"smull", 0x00c00090, NULL,   s_flag,      ARM_LONGMUL,  do_mull},
  {"umull", 0x00800090, NULL,   s_flag,      ARM_LONGMUL,  do_mull},
  {"smlal", 0x00e00090, NULL,   s_flag,      ARM_LONGMUL,  do_mull},
  {"umlal", 0x00a00090, NULL,   s_flag,      ARM_LONGMUL,  do_mull},

/* ARM THUMB interworking */
  {"bx",    0x012fff10, NULL,   NULL,        ARM_THUMB,    do_bx},

/* Floating point instructions */
  {"wfs",   0x0e200110, NULL,   NULL,        FPU_ALL,      do_fp_ctrl},
  {"rfs",   0x0e300110, NULL,   NULL,        FPU_ALL,      do_fp_ctrl},
  {"wfc",   0x0e400110, NULL,   NULL,        FPU_ALL,      do_fp_ctrl},
  {"rfc",   0x0e500110, NULL,   NULL,        FPU_ALL,      do_fp_ctrl},
  {"ldf",   0x0c100100, "sdep", NULL,        FPU_ALL,      do_fp_ldst},
  {"stf",   0x0c000100, "sdep", NULL,        FPU_ALL,      do_fp_ldst},
  {"lfm",   0x0c100200, NULL,   lfm_flags,   FPU_MEMMULTI, do_fp_ldmstm},
  {"sfm",   0x0c000200, NULL,   sfm_flags,   FPU_MEMMULTI, do_fp_ldmstm},
  {"mvf",   0x0e008100, "sde",  round_flags, FPU_ALL,      do_fp_monadic},
  {"mnf",   0x0e108100, "sde",  round_flags, FPU_ALL,      do_fp_monadic},
  {"abs",   0x0e208100, "sde",  round_flags, FPU_ALL,      do_fp_monadic},
  {"rnd",   0x0e308100, "sde",  round_flags, FPU_ALL,      do_fp_monadic},
  {"sqt",   0x0e408100, "sde",  round_flags, FPU_ALL,      do_fp_monadic},
  {"log",   0x0e508100, "sde",  round_flags, FPU_ALL,      do_fp_monadic},
  {"lgn",   0x0e608100, "sde",  round_flags, FPU_ALL,      do_fp_monadic},
  {"exp",   0x0e708100, "sde",  round_flags, FPU_ALL,      do_fp_monadic},
  {"sin",   0x0e808100, "sde",  round_flags, FPU_ALL,      do_fp_monadic},
  {"cos",   0x0e908100, "sde",  round_flags, FPU_ALL,      do_fp_monadic},
  {"tan",   0x0ea08100, "sde",  round_flags, FPU_ALL,      do_fp_monadic},
  {"asn",   0x0eb08100, "sde",  round_flags, FPU_ALL,      do_fp_monadic},
  {"acs",   0x0ec08100, "sde",  round_flags, FPU_ALL,      do_fp_monadic},
  {"atn",   0x0ed08100, "sde",  round_flags, FPU_ALL,      do_fp_monadic},
  {"urd",   0x0ee08100, "sde",  round_flags, FPU_ALL,      do_fp_monadic},
  {"nrm",   0x0ef08100, "sde",  round_flags, FPU_ALL,      do_fp_monadic},
  {"adf",   0x0e000100, "sde",  round_flags, FPU_ALL,      do_fp_dyadic},
  {"suf",   0x0e200100, "sde",  round_flags, FPU_ALL,      do_fp_dyadic},
  {"rsf",   0x0e300100, "sde",  round_flags, FPU_ALL,      do_fp_dyadic},
  {"muf",   0x0e100100, "sde",  round_flags, FPU_ALL,      do_fp_dyadic},
  {"dvf",   0x0e400100, "sde",  round_flags, FPU_ALL,      do_fp_dyadic},
  {"rdf",   0x0e500100, "sde",  round_flags, FPU_ALL,      do_fp_dyadic},
  {"pow",   0x0e600100, "sde",  round_flags, FPU_ALL,      do_fp_dyadic},
  {"rpw",   0x0e700100, "sde",  round_flags, FPU_ALL,      do_fp_dyadic},
  {"rmf",   0x0e800100, "sde",  round_flags, FPU_ALL,      do_fp_dyadic},
  {"fml",   0x0e900100, "sde",  round_flags, FPU_ALL,      do_fp_dyadic},
  {"fdv",   0x0ea00100, "sde",  round_flags, FPU_ALL,      do_fp_dyadic},
  {"frd",   0x0eb00100, "sde",  round_flags, FPU_ALL,      do_fp_dyadic},
  {"pol",   0x0ec00100, "sde",  round_flags, FPU_ALL,      do_fp_dyadic},
  {"cmf",   0x0e90f110, NULL,   except_flag, FPU_ALL,      do_fp_cmp},
  {"cnf",   0x0eb0f110, NULL,   except_flag, FPU_ALL,      do_fp_cmp},
/* The FPA10 data sheet suggests that the 'E' of cmfe/cnfe should not
   be an optional suffix, but part of the instruction.  To be compatible,
   we accept either.  */
  {"cmfe",  0x0ed0f110, NULL,   NULL,        FPU_ALL,      do_fp_cmp},
  {"cnfe",  0x0ef0f110, NULL,   NULL,        FPU_ALL,      do_fp_cmp},
  {"flt",   0x0e000110, "sde",  round_flags, FPU_ALL,      do_fp_from_reg},
  {"fix",   0x0e100110, NULL,   fix_flags,   FPU_ALL,      do_fp_to_reg},

/* Generic copressor instructions */
  {"cdp",   0x0e000000, NULL,  NULL,         ARM_2UP,      do_cdp},
  {"ldc",   0x0c100000, NULL,  cplong_flag,  ARM_2UP,      do_lstc},
  {"stc",   0x0c000000, NULL,  cplong_flag,  ARM_2UP,      do_lstc},
  {"mcr",   0x0e000010, NULL,  NULL,         ARM_2UP,      do_co_reg},
  {"mrc",   0x0e100010, NULL,  NULL,         ARM_2UP,      do_co_reg},
};

/* defines for various bits that we will want to toggle */

#define INST_IMMEDIATE	0x02000000
#define OFFSET_REG	0x02000000
#define HWOFFSET_IMM    0x00400000
#define SHIFT_BY_REG	0x00000010
#define PRE_INDEX	0x01000000
#define INDEX_UP	0x00800000
#define WRITE_BACK	0x00200000
#define MULTI_SET_PSR	0x00400000

#define LITERAL_MASK	0xf000f000
#define COND_MASK	0xf0000000
#define OPCODE_MASK	0xfe1fffff
#define DATA_OP_SHIFT	21

/* Codes to distinguish the arithmetic instructions */

#define OPCODE_AND	0
#define OPCODE_EOR	1
#define OPCODE_SUB	2
#define OPCODE_RSB	3
#define OPCODE_ADD	4
#define OPCODE_ADC	5
#define OPCODE_SBC	6
#define OPCODE_RSC	7
#define OPCODE_TST	8
#define OPCODE_TEQ	9
#define OPCODE_CMP	10
#define OPCODE_CMN	11
#define OPCODE_ORR	12
#define OPCODE_MOV	13
#define OPCODE_BIC	14
#define OPCODE_MVN	15

static void do_t_arit		PARAMS ((char *operands));
static void do_t_add		PARAMS ((char *operands));
static void do_t_asr		PARAMS ((char *operands));
static void do_t_branch		PARAMS ((char *operands));
static void do_t_bx		PARAMS ((char *operands));
static void do_t_compare	PARAMS ((char *operands));
static void do_t_ldmstm		PARAMS ((char *operands));
static void do_t_ldr		PARAMS ((char *operands));
static void do_t_ldrb		PARAMS ((char *operands));
static void do_t_ldrh		PARAMS ((char *operands));
static void do_t_lds		PARAMS ((char *operands));
static void do_t_lsl		PARAMS ((char *operands));
static void do_t_lsr		PARAMS ((char *operands));
static void do_t_mov		PARAMS ((char *operands));
static void do_t_push_pop	PARAMS ((char *operands));
static void do_t_str		PARAMS ((char *operands));
static void do_t_strb		PARAMS ((char *operands));
static void do_t_strh		PARAMS ((char *operands));
static void do_t_sub		PARAMS ((char *operands));
static void do_t_swi		PARAMS ((char *operands));
static void do_t_adr		PARAMS ((char *operands));

#define T_OPCODE_MUL 0x4340
#define T_OPCODE_TST 0x4200
#define T_OPCODE_CMN 0x42c0
#define T_OPCODE_NEG 0x4240
#define T_OPCODE_MVN 0x43c0

#define T_OPCODE_ADD_R3	0x1800
#define T_OPCODE_SUB_R3 0x1a00
#define T_OPCODE_ADD_HI 0x4400
#define T_OPCODE_ADD_ST 0xb000
#define T_OPCODE_SUB_ST 0xb080
#define T_OPCODE_ADD_SP 0xa800
#define T_OPCODE_ADD_PC 0xa000
#define T_OPCODE_ADD_I8 0x3000
#define T_OPCODE_SUB_I8 0x3800
#define T_OPCODE_ADD_I3 0x1c00
#define T_OPCODE_SUB_I3 0x1e00

#define T_OPCODE_ASR_R	0x4100
#define T_OPCODE_LSL_R	0x4080
#define T_OPCODE_LSR_R  0x40c0
#define T_OPCODE_ASR_I	0x1000
#define T_OPCODE_LSL_I	0x0000
#define T_OPCODE_LSR_I	0x0800

#define T_OPCODE_MOV_I8	0x2000
#define T_OPCODE_CMP_I8 0x2800
#define T_OPCODE_CMP_LR 0x4280
#define T_OPCODE_MOV_HR 0x4600
#define T_OPCODE_CMP_HR 0x4500

#define T_OPCODE_LDR_PC 0x4800
#define T_OPCODE_LDR_SP 0x9800
#define T_OPCODE_STR_SP 0x9000
#define T_OPCODE_LDR_IW 0x6800
#define T_OPCODE_STR_IW 0x6000
#define T_OPCODE_LDR_IH 0x8800
#define T_OPCODE_STR_IH 0x8000
#define T_OPCODE_LDR_IB 0x7800
#define T_OPCODE_STR_IB 0x7000
#define T_OPCODE_LDR_RW 0x5800
#define T_OPCODE_STR_RW 0x5000
#define T_OPCODE_LDR_RH 0x5a00
#define T_OPCODE_STR_RH 0x5200
#define T_OPCODE_LDR_RB 0x5c00
#define T_OPCODE_STR_RB 0x5400

#define T_OPCODE_PUSH	0xb400
#define T_OPCODE_POP	0xbc00

#define T_OPCODE_BRANCH 0xe7fe

static int thumb_reg		PARAMS ((char **str, int hi_lo));

#define THUMB_SIZE	2	/* Size of thumb instruction */
#define THUMB_REG_LO	0x1
#define THUMB_REG_HI	0x2
#define THUMB_REG_ANY	0x3

#define THUMB_H1	0x0080
#define THUMB_H2	0x0040

#define THUMB_ASR 0
#define THUMB_LSL 1
#define THUMB_LSR 2

#define THUMB_MOVE 0
#define THUMB_COMPARE 1

#define THUMB_LOAD 0
#define THUMB_STORE 1

#define THUMB_PP_PC_LR 0x0100

/* These three are used for immediate shifts, do not alter */
#define THUMB_WORD 2
#define THUMB_HALFWORD 1
#define THUMB_BYTE 0

struct thumb_opcode 
{
  CONST char *template;		/* Basic string to match */
  unsigned long value;		/* Basic instruction code */
  int size;
  void (*parms)();		/* Function to call to parse args */
};

static CONST struct thumb_opcode tinsns[] =
{
  {"adc",	0x4140,		2,	do_t_arit},
  {"add",	0x0000,		2,	do_t_add},
  {"and",	0x4000,		2,	do_t_arit},
  {"asr",	0x0000,		2,	do_t_asr},
  {"b",		T_OPCODE_BRANCH, 2,	do_t_branch},
  {"beq",	0xd0fe,		2,	do_t_branch},
  {"bne",	0xd1fe,		2,	do_t_branch},
  {"bcs",	0xd2fe,		2,	do_t_branch},
  {"bhs",	0xd2fe,		2,	do_t_branch},
  {"bcc",	0xd3fe,		2,	do_t_branch},
  {"bul",	0xd3fe,		2,	do_t_branch},
  {"blo",	0xd3fe,		2,	do_t_branch},
  {"bmi",	0xd4fe,		2,	do_t_branch},
  {"bpl",	0xd5fe,		2,	do_t_branch},
  {"bvs",	0xd6fe,		2,	do_t_branch},
  {"bvc",	0xd7fe,		2,	do_t_branch},
  {"bhi",	0xd8fe,		2,	do_t_branch},
  {"bls",	0xd9fe,		2,	do_t_branch},
  {"bge",	0xdafe,		2,	do_t_branch},
  {"blt",	0xdbfe,		2,	do_t_branch},
  {"bgt",	0xdcfe,		2,	do_t_branch},
  {"ble",	0xddfe,		2,	do_t_branch},
  {"bic",	0x4380,		2,	do_t_arit},
  {"bl",	0xf7fffffe,	4,	do_t_branch},
  {"bx",	0x4700,		2,	do_t_bx},
  {"cmn",	T_OPCODE_CMN,	2,	do_t_arit},
  {"cmp",	0x0000,		2,	do_t_compare},
  {"eor",	0x4040,		2,	do_t_arit},
  {"ldmia",	0xc800,		2,	do_t_ldmstm},
  {"ldr",	0x0000,		2,	do_t_ldr},
  {"ldrb",	0x0000,		2,	do_t_ldrb},
  {"ldrh",	0x0000,		2,	do_t_ldrh},
  {"ldrsb",	0x5600,		2,	do_t_lds},
  {"ldrsh",	0x5e00,		2,	do_t_lds},
  {"ldsb",	0x5600,		2,	do_t_lds},
  {"ldsh",	0x5e00,		2,	do_t_lds},
  {"lsl",	0x0000,		2,	do_t_lsl},
  {"lsr",	0x0000,		2,	do_t_lsr},
  {"mov",	0x0000,		2,	do_t_mov},
  {"mul",	T_OPCODE_MUL,	2,	do_t_arit},
  {"mvn",	T_OPCODE_MVN,	2,	do_t_arit},
  {"neg",	T_OPCODE_NEG,	2,	do_t_arit},
  {"orr",	0x4300,		2,	do_t_arit},
  {"pop",	0xbc00,		2,	do_t_push_pop},
  {"push",	0xb400,		2,	do_t_push_pop},
  {"ror",	0x41c0,		2,	do_t_arit},
  {"sbc",	0x4180,		2,	do_t_arit},
  {"stmia",	0xc000,		2,	do_t_ldmstm},
  {"str",	0x0000,		2,	do_t_str},
  {"strb",	0x0000,		2,	do_t_strb},
  {"strh",	0x0000,		2,	do_t_strh},
  {"swi",	0xdf00,		2,	do_t_swi},
  {"sub",	0x0000,		2,	do_t_sub},
  {"tst",	T_OPCODE_TST,	2,	do_t_arit},
  /* Pseudo ops: */
  {"adr",       0x0000,         2,      do_t_adr},
  {"nop",       0x0000,         2,      do_nop},
};

struct reg_entry
{
  CONST char *name;
  int number;
};

#define int_register(reg) ((reg) >= 0 && (reg) <= 15)
#define cp_register(reg) ((reg) >= 32 && (reg) <= 47)
#define fp_register(reg) ((reg) >= 16 && (reg) <= 23)

#define REG_PC	15
#define REG_LR  14
#define REG_SP  13

/* These are the standard names;  Users can add aliases with .req */
static CONST struct reg_entry reg_table[] =
{
  /* Processor Register Numbers */
  {"r0", 0},    {"r1", 1},      {"r2", 2},      {"r3", 3},
  {"r4", 4},    {"r5", 5},      {"r6", 6},      {"r7", 7},
  {"r8", 8},    {"r9", 9},      {"r10", 10},    {"r11", 11},
  {"r12", 12},  {"r13", REG_SP},{"r14", REG_LR},{"r15", REG_PC},
  /* APCS conventions */
  {"a1", 0},	{"a2", 1},    {"a3", 2},     {"a4", 3},
  {"v1", 4},	{"v2", 5},    {"v3", 6},     {"v4", 7},     {"v5", 8},
  {"v6", 9},	{"sb", 9},    {"v7", 10},    {"sl", 10},
  {"fp", 11},	{"ip", 12},   {"sp", REG_SP},{"lr", REG_LR},{"pc", REG_PC},
  /* FP Registers */
  {"f0", 16},   {"f1", 17},   {"f2", 18},   {"f3", 19},
  {"f4", 20},   {"f5", 21},   {"f6", 22},   {"f7", 23},
  {"c0", 32},   {"c1", 33},   {"c2", 34},   {"c3", 35},
  {"c4", 36},   {"c5", 37},   {"c6", 38},   {"c7", 39},
  {"c8", 40},   {"c9", 41},   {"c10", 42},  {"c11", 43},
  {"c12", 44},  {"c13", 45},  {"c14", 46},  {"c15", 47},
  {"cr0", 32},  {"cr1", 33},  {"cr2", 34},  {"cr3", 35},
  {"cr4", 36},  {"cr5", 37},  {"cr6", 38},  {"cr7", 39},
  {"cr8", 40},  {"cr9", 41},  {"cr10", 42}, {"cr11", 43},
  {"cr12", 44}, {"cr13", 45}, {"cr14", 46}, {"cr15", 47},
  {NULL, 0}
};

static CONST char *bad_args = "Bad arguments to instruction";
static CONST char *bad_pc = "r15 not allowed here";

static struct hash_control *arm_ops_hsh = NULL;
static struct hash_control *arm_tops_hsh = NULL;
static struct hash_control *arm_cond_hsh = NULL;
static struct hash_control *arm_shift_hsh = NULL;
static struct hash_control *arm_reg_hsh = NULL;
static struct hash_control *arm_psr_hsh = NULL;

/* This table describes all the machine specific pseudo-ops the assembler
   has to support.  The fields are:
   pseudo-op name without dot
   function to call to execute this pseudo-op
   Integer arg to pass to the function
   */

static void s_req PARAMS ((int));
static void s_align PARAMS ((int));
static void s_bss PARAMS ((int));
static void s_even PARAMS ((int));
static void s_ltorg PARAMS ((int));
static void s_arm PARAMS ((int));
static void s_thumb PARAMS ((int));
static void s_code PARAMS ((int));

static int my_get_expression PARAMS ((expressionS *, char **));

CONST pseudo_typeS md_pseudo_table[] =
{
  {"req", s_req, 0},	/* Never called becasue '.req' does not start line */
  {"bss", s_bss, 0},
  {"align", s_align, 0},
  {"arm", s_arm, 0},
  {"thumb", s_thumb, 0},
  {"code", s_code, 0},
  {"even", s_even, 0},
  {"ltorg", s_ltorg, 0},
  {"pool", s_ltorg, 0},
  {"word", cons, 4},
  {"extend", float_cons, 'x'},
  {"ldouble", float_cons, 'x'},
  {"packed", float_cons, 'p'},
  {0, 0, 0}
};

/* Stuff needed to resolve the label ambiguity
   As:
     ...
     label:   <insn>
   may differ from:
     ...
     label:
              <insn>
*/

symbolS *last_label_seen;

/* Literal stuff */

#define MAX_LITERAL_POOL_SIZE 1024

typedef struct literalS
{
  struct expressionS  exp;
  struct arm_it      *inst;
} literalT;

literalT literals[MAX_LITERAL_POOL_SIZE];
int next_literal_pool_place = 0; /* Next free entry in the pool */
int lit_pool_num = 1; /* Next literal pool number */
symbolS *current_poolP = NULL;
symbolS *symbol_make_empty (); 

static int
add_to_lit_pool ()
{
  int lit_count = 0;

  if (current_poolP == NULL)
    current_poolP = symbol_make_empty();

  /* Check if this literal value is already in the pool: */
  while (lit_count < next_literal_pool_place)
    {
      if (literals[lit_count].exp.X_op == inst.reloc.exp.X_op
          && inst.reloc.exp.X_op == O_constant
          && literals[lit_count].exp.X_add_number == inst.reloc.exp.X_add_number
          && literals[lit_count].exp.X_unsigned == inst.reloc.exp.X_unsigned)
        break;
      lit_count++;
    }

  if (lit_count == next_literal_pool_place) /* new entry */
    {
      if (next_literal_pool_place > MAX_LITERAL_POOL_SIZE)
        {
          inst.error = "Literal Pool Overflow\n";
          return FAIL;
        }

      literals[next_literal_pool_place].exp = inst.reloc.exp;
      lit_count = next_literal_pool_place++;
    }

  inst.reloc.exp.X_op = O_symbol;
  inst.reloc.exp.X_add_number = (lit_count)*4-8;
  inst.reloc.exp.X_add_symbol = current_poolP;

  return SUCCESS;
}
 
/* Can't use symbol_new here, so have to create a symbol and them at
   a later date assign it a value. Thats what these functions do */
static void
symbol_locate (symbolP, name, segment, valu, frag)
     symbolS *symbolP; 
     CONST char *name;		/* It is copied, the caller can modify */
     segT segment;		/* Segment identifier (SEG_<something>) */
     valueT valu;		/* Symbol value */
     fragS *frag;		/* Associated fragment */
{
  unsigned int name_length;
  char *preserved_copy_of_name;

  name_length = strlen (name) + 1;      /* +1 for \0 */
  obstack_grow (&notes, name, name_length);
  preserved_copy_of_name = obstack_finish (&notes);
#ifdef STRIP_UNDERSCORE
  if (preserved_copy_of_name[0] == '_')
    preserved_copy_of_name++;
#endif

#ifdef tc_canonicalize_symbol_name
  preserved_copy_of_name =
    tc_canonicalize_symbol_name (preserved_copy_of_name);
#endif

  S_SET_NAME (symbolP, preserved_copy_of_name);

  S_SET_SEGMENT (symbolP, segment);
  S_SET_VALUE (symbolP, valu);
  symbol_clear_list_pointers(symbolP);

  symbolP->sy_frag = frag;

  /*
   * Link to end of symbol chain.
   */
  {
    extern int symbol_table_frozen;
    if (symbol_table_frozen)
      abort ();
  }

  symbol_append (symbolP, symbol_lastP, &symbol_rootP, &symbol_lastP);

  obj_symbol_new_hook (symbolP);

#ifdef tc_symbol_new_hook
  tc_symbol_new_hook (symbolP);
#endif
 
#ifdef DEBUG_SYMS
  verify_symbol_chain(symbol_rootP, symbol_lastP);
#endif /* DEBUG_SYMS */
}

symbolS *
symbol_make_empty () 
{
  symbolS *symbolP; 

  symbolP = (symbolS *) obstack_alloc (&notes, sizeof (symbolS));

  /* symbol must be born in some fixed state.  This seems as good as any. */
  memset (symbolP, 0, sizeof (symbolS));

#ifdef BFD_ASSEMBLER
  symbolP->bsym = bfd_make_empty_symbol (stdoutput);
  assert (symbolP->bsym != 0);
  symbolP->bsym->udata.p = (PTR) symbolP;
#endif

  return symbolP;
}
 
/* Check that an immediate is valid, and if so, convert it to the right format
 */

/* OH, for a rotate instruction in C! */

static int
validate_immediate (val)
     int val;
{
  unsigned int a = (unsigned int) val;
  int i;
  
  /* Do the easy (and most common ones) quickly */
  for (i = 0; i <= 24; i += 2)
    {
      if ((a & (0xff << i)) == a)
	return (int) (((32 - i) & 0x1e) << 7) | ((a >> i) & 0xff);
    }

  /* Now do the harder ones */
  for (; i < 32; i += 2)
    {
      if ((a & ((0xff << i) | (0xff >> (32 - i)))) == a)
	{
	  a = ((a >> i) & 0xff) | ((a << (32 - i)) & 0xff);
	  return (int) a | (((32 - i) >> 1) << 8);
	}
    }
  return FAIL;
}

static int
validate_offset_imm (val, hwse)
     int val;
     int hwse;
{
  if ((hwse && (val < -255 || val > 255))
      || (val < -4095 || val > 4095))
     return FAIL;
  return val;
}

    
static void
s_req (a)
     int a;
{
  as_bad ("Invalid syntax for .req directive.");
}

static void
s_bss (ignore)
     int ignore;
{
  /* We don't support putting frags in the BSS segment, we fake it by
     marking in_bss, then looking at s_skip for clues?.. */
  subseg_set (bss_section, 0);
  demand_empty_rest_of_line ();
}

static void
s_even (ignore)
     int ignore;
{
  if (!need_pass_2)		/* Never make frag if expect extra pass. */
    frag_align (1, 0);
  record_alignment (now_seg, 1);
  demand_empty_rest_of_line ();
}

static void
s_ltorg (internal)
     int internal;
{
  int lit_count = 0;
  char sym_name[20];

  if (current_poolP == NULL)
    {
      /* Nothing to do */
      if (!internal)
	as_tsktsk ("Nothing to put in the pool\n");
      return;
    }

  /* Align pool as you have word accesses */
  /* Only make a frag if we have to ... */
  if (!need_pass_2)
    frag_align (2, 0);

  record_alignment (now_seg, 2);

  if (internal)
    as_tsktsk ("Inserting implicit pool at change of section");

  sprintf (sym_name, "$$lit_\002%x", lit_pool_num++);

  symbol_locate (current_poolP, sym_name, now_seg,
		 (valueT) frag_now_fix (), frag_now);
  symbol_table_insert (current_poolP);

  while (lit_count < next_literal_pool_place)
    /* First output the expression in the instruction to the pool */
    emit_expr (&(literals[lit_count++].exp), 4); /* .word */

  next_literal_pool_place = 0;
  current_poolP = NULL;
}

#if 0 /* not used */
static void
arm_align (power, fill)
     int power;
     int fill;
{
  /* Only make a frag if we HAVE to ... */
  if (power && !need_pass_2)
    frag_align (power, fill);

  record_alignment (now_seg, power);
}
#endif

static void
s_align (unused)	/* Same as s_align_ptwo but align 0 => align 2 */
     int unused;
{
  register int temp;
  register long temp_fill;
  long max_alignment = 15;

  temp = get_absolute_expression ();
  if (temp > max_alignment)
    as_bad ("Alignment too large: %d. assumed.", temp = max_alignment);
  else if (temp < 0)
    {
      as_bad ("Alignment negative. 0 assumed.");
      temp = 0;
    }

  if (*input_line_pointer == ',')
    {
      input_line_pointer++;
      temp_fill = get_absolute_expression ();
    }
  else
    temp_fill = 0;

  if (!temp)
    temp = 2;

  /* Only make a frag if we HAVE to. . . */
  if (temp && !need_pass_2)
    frag_align (temp, (int) temp_fill);
  demand_empty_rest_of_line ();

  record_alignment (now_seg, temp);
}

static void
opcode_select (width)
     int width;
{
  switch (width)
    {
    case 16:
      if (! thumb_mode)
	{
	  if (! (cpu_variant & ARM_THUMB))
	    as_bad ("selected processor does not support THUMB opcodes");
	  thumb_mode = 1;
          /* No need to force the alignment, since we will have been
             coming from ARM mode, which is word-aligned. */
          record_alignment (now_seg, 1);
	}
      break;

    case 32:
      if (thumb_mode)
	{
          if ((cpu_variant & ARM_ANY) == ARM_THUMB)
	    as_bad ("selected processor does not support ARM opcodes");
	  thumb_mode = 0;
          if (!need_pass_2)
            frag_align (2, 0);
          record_alignment (now_seg, 1);
	}
      break;

    default:
      as_bad ("invalid instruction size selected (%d)", width);
    }
}

static void
s_arm (ignore)
     int ignore;
{
  opcode_select (32);
  demand_empty_rest_of_line ();
}

static void
s_thumb (ignore)
     int ignore;
{
  opcode_select (16);
  demand_empty_rest_of_line ();
}

static void
s_code (unused)
     int unused;
{
  register int temp;

  temp = get_absolute_expression ();
  switch (temp)
    {
    case 16:
    case 32:
      opcode_select(temp);
      break;

    default:
      as_bad ("invalid operand to .code directive (%d)", temp);
    }
}

static void
end_of_line (str)
     char *str;
{
  while (*str == ' ')
    str++;

  if (*str != '\0')
    inst.error = "Garbage following instruction";
}

static int
skip_past_comma (str)
     char **str;
{
  char *p = *str, c;
  int comma = 0;
    
  while ((c = *p) == ' ' || c == ',')
    {
      p++;
      if (c == ',' && comma++)
	return FAIL;
    }

  if (c == '\0')
    return FAIL;

  *str = p;
  return comma ? SUCCESS : FAIL;
}

/* A standard register must be given at this point.  Shift is the place to
   put it in the instruction. */

static int
reg_required_here (str, shift)
     char **str;
     int shift;
{
  int reg;
  char *start = *str;

  if ((reg = arm_reg_parse (str)) != FAIL && int_register (reg))
    {
      inst.instruction |= reg << shift;
      return reg;
    }

  /* In the few cases where we might be able to accept something else
     this error can be overridden */
  inst.error = "Register expected";

  /* Restore the start point, we may have got a reg of the wrong class.  */
  *str = start;
  return FAIL;
}

static int
psr_required_here (str, shift)
     char **str;
     int shift;
{
  int psr;
  char *start = *str;

  if  ((psr = arm_psr_parse (str)) != FAIL && psr < 2)
    {
      if (psr == 1)
	inst.instruction |= 1 << shift; /* Should be bit 22 */
      return psr;
    }

  /* In the few cases where we might be able to accept something else
     this error can be overridden */
  inst.error = "<psr> expected";

  /* Restore the start point.  */
  *str = start;
  return FAIL;
}

static int
psrf_required_here (str, shift)
     char **str;
     int shift;
{
  int psrf;
  char *start = *str;

  if  ((psrf = arm_psr_parse (str)) != FAIL && psrf > 1)
    {
      if (psrf == 1 || psrf == 3)
	inst.instruction |= 1 << shift; /* Should be bit 22 */
      return psrf;
    }

  /* In the few cases where we might be able to accept something else
     this error can be overridden */
  inst.error = "<psrf> expected";

  /* Restore the start point.  */
  *str = start;
  return FAIL;
}

static int
co_proc_number (str)
     char **str;
{
  int processor, pchar;

  while (**str == ' ')
    (*str)++;

  /* The data sheet seems to imply that just a number on its own is valid
     here, but the RISC iX assembler seems to accept a prefix 'p'.  We will
     accept either.  */
  if (**str == 'p' || **str == 'P')
    (*str)++;

  pchar = *(*str)++;
  if (pchar >= '0' && pchar <= '9')
    {
      processor = pchar - '0';
      if (**str >= '0' && **str <= '9')
	{
	  processor = processor * 10 + *(*str)++ - '0';
	  if (processor > 15)
	    {
	      inst.error = "Illegal co-processor number";
	      return FAIL;
	    }
	}
    }
  else
    {
      inst.error = "Bad or missing co-processor number";
      return FAIL;
    }

  inst.instruction |= processor << 8;
  return SUCCESS;
}

static int
cp_opc_expr (str, where, length)
     char **str;
     int where;
     int length;
{
  expressionS expr;

  while (**str == ' ')
    (*str)++;

  memset (&expr, '\0', sizeof (expr));

  if (my_get_expression (&expr, str))
    return FAIL;
  if (expr.X_op != O_constant)
    {
      inst.error = "bad or missing expression";
      return FAIL;
    }

  if ((expr.X_add_number & ((1 << length) - 1)) != expr.X_add_number)
    {
      inst.error = "immediate co-processor expression too large";
      return FAIL;
    }

  inst.instruction |= expr.X_add_number << where;
  return SUCCESS;
}

static int
cp_reg_required_here (str, where)
     char **str;
     int where;
{
  int reg;
  char *start = *str;

  if ((reg = arm_reg_parse (str)) != FAIL && cp_register (reg))
    {
      reg &= 15;
      inst.instruction |= reg << where;
      return reg;
    }

  /* In the few cases where we might be able to accept something else
     this error can be overridden */
  inst.error = "Co-processor register expected";

  /* Restore the start point */
  *str = start;
  return FAIL;
}

static int
fp_reg_required_here (str, where)
     char **str;
     int where;
{
  int reg;
  char *start = *str;

  if ((reg = arm_reg_parse (str)) != FAIL && fp_register (reg))
    {
      reg &= 7;
      inst.instruction |= reg << where;
      return reg;
    }

  /* In the few cases where we might be able to accept something else
     this error can be overridden */
  inst.error = "Floating point register expected";

  /* Restore the start point */
  *str = start;
  return FAIL;
}

static int
cp_address_offset (str)
     char **str;
{
  int offset;

  while (**str == ' ')
    (*str)++;

  if (**str != '#')
    {
      inst.error = "immediate expression expected";
      return FAIL;
    }

  (*str)++;
  if (my_get_expression (&inst.reloc.exp, str))
    return FAIL;
  if (inst.reloc.exp.X_op == O_constant)
    {
      offset = inst.reloc.exp.X_add_number;
      if (offset & 3)
	{
	  inst.error = "co-processor address must be word aligned";
	  return FAIL;
	}

      if (offset > 1023 || offset < -1023)
	{
	  inst.error = "offset too large";
	  return FAIL;
	}

      if (offset >= 0)
	inst.instruction |= INDEX_UP;
      else
	offset = -offset;

      inst.instruction |= offset >> 2;
    }
  else
    inst.reloc.type = BFD_RELOC_ARM_CP_OFF_IMM;

  return SUCCESS;
}

static int
cp_address_required_here (str)
     char **str;
{
  char *p = *str;
  int pre_inc = 0;
  int write_back = 0;

  if (*p == '[')
    {
      int reg;

      p++;
      while (*p == ' ')
	p++;

      if ((reg = reg_required_here (&p, 16)) == FAIL)
	{
	  inst.error = "Register required";
	  return FAIL;
	}

      while (*p == ' ')
	p++;

      if (*p == ']')
	{
	  p++;
	  if (skip_past_comma (&p) == SUCCESS)
	    {
	      /* [Rn], #expr */
	      write_back = WRITE_BACK;
	      if (reg == REG_PC)
		{
		  inst.error = "pc may not be used in post-increment";
		  return FAIL;
		}

	      if (cp_address_offset (&p) == FAIL)
		return FAIL;
	    }
	  else
	    pre_inc = PRE_INDEX | INDEX_UP;
	}
      else
	{
	  /* '['Rn, #expr']'[!] */

	  if (skip_past_comma (&p) == FAIL)
	    {
	      inst.error = "pre-indexed expression expected";
	      return FAIL;
	    }

	  pre_inc = PRE_INDEX;
	  if (cp_address_offset (&p) == FAIL)
	    return FAIL;

	  while (*p == ' ')
	    p++;

	  if (*p++ != ']')
	    {
	      inst.error = "missing ]";
	      return FAIL;
	    }

	  while (*p == ' ')
	    p++;

	  if (*p == '!')
	    {
	      if (reg == REG_PC)
		{
		  inst.error = "pc may not be used with write-back";
		  return FAIL;
		}

	      p++;
	      write_back = WRITE_BACK;
	    }
	}
    }
  else
    {
      if (my_get_expression (&inst.reloc.exp, &p))
	return FAIL;

      inst.reloc.type = BFD_RELOC_ARM_CP_OFF_IMM;
      inst.reloc.exp.X_add_number -= 8;  /* PC rel adjust */
      inst.reloc.pc_rel = 1;
      inst.instruction |= (REG_PC << 16);
      pre_inc = PRE_INDEX;
    }

  inst.instruction |= write_back | pre_inc;
  *str = p;
  return SUCCESS;
}

static void
do_nop (str, flags)
     char *str;
     unsigned long flags;
{
  /* Do nothing really */
  inst.instruction |= flags; /* This is pointless */
  end_of_line (str);
  return;
}

static void
do_mrs (str, flags)
     char *str;
     unsigned long flags;
{
  /* Only one syntax */
  while (*str == ' ')
    str++;

  if (reg_required_here (&str, 12) == FAIL)
    {
      inst.error = bad_args;
      return;
    }

  if (skip_past_comma (&str) == FAIL
      || psr_required_here (&str, 22) == FAIL)
    {
      inst.error = "<psr> expected";
      return;
    }

  inst.instruction |= flags;
  end_of_line (str);
  return;
}

static void
do_msr (str, flags)
     char *str;
     unsigned long flags;
{
  int psr, psrf, reg;
  /* Three possible forms: "<psr>, Rm", "<psrf>, Rm", "<psrf>, #expression" */

  while (*str == ' ')
    str++;

  if ((psr = psr_required_here (&str, 22)) != FAIL)
    {
      inst.instruction |= PSR_ALL;
      /* Sytax should be "<psr>, Rm" */
      if (skip_past_comma (&str) == FAIL
	  || (reg = reg_required_here (&str, 0)) == FAIL)
	{
	  inst.error = bad_args;
	  return;
	}
    }
  else if ((psrf = psrf_required_here (&str, 22)) != FAIL)
    /* Syntax could be "<psrf>, rm", "<psrf>, #expression" */
    {
      if (skip_past_comma (&str) == FAIL)
	{
	  inst.error = bad_args;
	  return;
	}
      if ((reg = reg_required_here (&str, 0)) != FAIL)
	;
      /* Immediate expression */
      else if (*(str++) == '#')
	{
	  inst.error = NULL;
	  if (my_get_expression (&inst.reloc.exp, &str))
	    {
	      inst.error = "Register or shift expression expected";
	      return;
	    }

	  if (inst.reloc.exp.X_add_symbol)
	    {
	      inst.reloc.type = BFD_RELOC_ARM_IMMEDIATE;
	      inst.reloc.pc_rel = 0;
	    }
	  else
	    {
	      int value = validate_immediate (inst.reloc.exp.X_add_number);
	      if (value == FAIL)
		{
		  inst.error = "Invalid constant";
		  return;
		}

	      inst.instruction |= value;
	    }

	  flags |= INST_IMMEDIATE;
	}
      else
	{
	  inst.error = "Error: the other";
	  return;
	}
    }
  else
    {
      inst.error = bad_args;
      return;
    }
     
  inst.error = NULL; 
  inst.instruction |= flags;
  end_of_line (str);
  return;
}

/* Long Multiply Parser
   UMULL RdLo, RdHi, Rm, Rs
   SMULL RdLo, RdHi, Rm, Rs
   UMLAL RdLo, RdHi, Rm, Rs
   SMLAL RdLo, RdHi, Rm, Rs
*/   
static void
do_mull (str, flags)
     char *str;
     unsigned long flags;
{
  int rdlo, rdhi, rm, rs;

  /* only one format "rdlo, rdhi, rm, rs" */
  while (*str == ' ')
    str++;

  if ((rdlo = reg_required_here (&str, 12)) == FAIL)
    {
      inst.error = bad_args;
      return;
    }

  if (skip_past_comma (&str) == FAIL
      || (rdhi = reg_required_here (&str, 16)) == FAIL)
    {
      inst.error = bad_args;
      return;
    }

  if (skip_past_comma (&str) == FAIL
      || (rm = reg_required_here (&str, 0)) == FAIL)
    {
      inst.error = bad_args;
      return;
    }

  /* rdhi, rdlo and rm must all be different */
  if (rdlo == rdhi || rdlo == rm || rdhi == rm)
    as_tsktsk ("rdhi, rdlo and rm must all be different");

  if (skip_past_comma (&str) == FAIL
      || (rs = reg_required_here (&str, 8)) == FAIL)
    {
      inst.error = bad_args;
      return;
    }

  if (rdhi == REG_PC || rdhi == REG_PC || rdhi == REG_PC || rdhi == REG_PC)
    {
      inst.error = bad_pc;
      return;
    }
   
  inst.instruction |= flags;
  end_of_line (str);
  return;
}

static void
do_mul (str, flags)
     char *str;
     unsigned long flags;
{
  int rd, rm;
  
  /* only one format "rd, rm, rs" */
  while (*str == ' ')
    str++;

  if ((rd = reg_required_here (&str, 16)) == FAIL)
    {
      inst.error = bad_args;
      return;
    }

  if (rd == REG_PC)
    {
      inst.error = bad_pc;
      return;
    }

  if (skip_past_comma (&str) == FAIL
      || (rm = reg_required_here (&str, 0)) == FAIL)
    {
      inst.error = bad_args;
      return;
    }

  if (rm == REG_PC)
    {
      inst.error = bad_pc;
      return;
    }

  if (rm == rd)
    as_tsktsk ("rd and rm should be different in mul");

  if (skip_past_comma (&str) == FAIL
      || (rm = reg_required_here (&str, 8)) == FAIL)
    {
      inst.error = bad_args;
      return;
    }

  if (rm == REG_PC)
    {
      inst.error = bad_pc;
      return;
    }

  inst.instruction |= flags;
  end_of_line (str);
  return;
}

static void
do_mla (str, flags)
     char *str;
     unsigned long flags;
{
  int rd, rm;

  /* only one format "rd, rm, rs, rn" */
  while (*str == ' ')
    str++;

  if ((rd = reg_required_here (&str, 16)) == FAIL)
    {
      inst.error = bad_args;
      return;
    }

  if (rd == REG_PC)
    {
      inst.error = bad_pc;
      return;
    }

  if (skip_past_comma (&str) == FAIL
      || (rm = reg_required_here (&str, 0)) == FAIL)
    {
      inst.error = bad_args;
      return;
    }

  if (rm == REG_PC)
    {
      inst.error = bad_pc;
      return;
    }

  if (rm == rd)
    as_tsktsk ("rd and rm should be different in mla");

  if (skip_past_comma (&str) == FAIL
      || (rd = reg_required_here (&str, 8)) == FAIL
      || skip_past_comma (&str) == FAIL
      || (rm = reg_required_here (&str, 12)) == FAIL)
    {
      inst.error = bad_args;
      return;
    }

  if (rd == REG_PC || rm == REG_PC)
    {
      inst.error = bad_pc;
      return;
    }

  inst.instruction |= flags;
  end_of_line (str);
  return;
}

/* Returns the index into fp_values of a floating point number, or -1 if
   not in the table.  */
static int
my_get_float_expression (str)
     char **str;
{
  LITTLENUM_TYPE words[MAX_LITTLENUMS];
  char *save_in;
  expressionS exp;
  int i, j;

  memset (words, 0, MAX_LITTLENUMS * sizeof (LITTLENUM_TYPE));
  /* Look for a raw floating point number */
  if ((save_in = atof_ieee (*str, 'x', words)) != NULL
      && (is_end_of_line [(int)(*save_in)] || *save_in == '\0'))
    {
      for (i = 0; i < NUM_FLOAT_VALS; i++)
	{
	  for (j = 0; j < MAX_LITTLENUMS; j++)
	    {
	      if (words[j] != fp_values[i][j])
		break;
	    }

	  if (j == MAX_LITTLENUMS)
	    {
	      *str = save_in;
	      return i;
	    }
	}
    }

  /* Try and parse a more complex expression, this will probably fail
     unless the code uses a floating point prefix (eg "0f") */
  save_in = input_line_pointer;
  input_line_pointer = *str;
  if (expression (&exp) == absolute_section
      && exp.X_op == O_big
      && exp.X_add_number < 0)
    {
      /* FIXME: 5 = X_PRECISION, should be #define'd where we can use it.
	 Ditto for 15.  */
      if (gen_to_words (words, 5, (long)15) == 0)
	{
	  for (i = 0; i < NUM_FLOAT_VALS; i++)
	    {
	      for (j = 0; j < MAX_LITTLENUMS; j++)
		{
		  if (words[j] != fp_values[i][j])
		    break;
		}

	      if (j == MAX_LITTLENUMS)
		{
		  *str = input_line_pointer;
		  input_line_pointer = save_in;
		  return i;
		}
	    }
	}
    }

  *str = input_line_pointer;
  input_line_pointer = save_in;
  return -1;
}

/* Return true if anything in the expression is a bignum */
static int
walk_no_bignums (sp)
     symbolS *sp;
{
  if (sp->sy_value.X_op == O_big)
    return 1;

  if (sp->sy_value.X_add_symbol)
    {
      return (walk_no_bignums (sp->sy_value.X_add_symbol)
	      || (sp->sy_value.X_op_symbol
		  && walk_no_bignums (sp->sy_value.X_op_symbol)));
    }

  return 0;
}

static int
my_get_expression (ep, str)
     expressionS *ep;
     char **str;
{
  char *save_in;
  segT seg;
  
  save_in = input_line_pointer;
  input_line_pointer = *str;
  seg = expression (ep);

#ifdef OBJ_AOUT
  if (seg != absolute_section
      && seg != text_section
      && seg != data_section
      && seg != bss_section
      && seg != undefined_section)
    {
      inst.error = "bad_segment";
      *str = input_line_pointer;
      input_line_pointer = save_in;
      return 1;
    }
#endif

  /* Get rid of any bignums now, so that we don't generate an error for which
     we can't establish a line number later on.  Big numbers are never valid
     in instructions, which is where this routine is always called.  */
  if (ep->X_op == O_big
      || (ep->X_add_symbol
	  && (walk_no_bignums (ep->X_add_symbol)
	      || (ep->X_op_symbol
		  && walk_no_bignums (ep->X_op_symbol)))))
    {
      inst.error = "Invalid constant";
      *str = input_line_pointer;
      input_line_pointer = save_in;
      return 1;
    }

  *str = input_line_pointer;
  input_line_pointer = save_in;
  return 0;
}

/* unrestrict should be one if <shift> <register> is permitted for this
   instruction */

static int
decode_shift (str, unrestrict)
     char **str;
     int unrestrict;
{
  struct asm_shift *shft;
  char *p;
  char c;
    
  while (**str == ' ')
    (*str)++;
    
  for (p = *str; isalpha (*p); p++)
    ;

  if (p == *str)
    {
      inst.error = "Shift expression expected";
      return FAIL;
    }

  c = *p;
  *p = '\0';
  shft = (struct asm_shift *) hash_find (arm_shift_hsh, *str);
  *p = c;
  if (shft)
    {
      if (!strcmp (*str, "rrx")
          || !strcmp (*str, "RRX"))
	{
	  *str = p;
	  inst.instruction |= shft->value;
	  return SUCCESS;
	}

      while (*p == ' ')
	p++;

      if (unrestrict && reg_required_here (&p, 8) != FAIL)
	{
	  inst.instruction |= shft->value | SHIFT_BY_REG;
	  *str = p;
	  return SUCCESS;
	}
      else if (*p == '#')
	{
	  inst.error = NULL;
	  p++;
	  if (my_get_expression (&inst.reloc.exp, &p))
	    return FAIL;

	  /* Validate some simple #expressions */
	  if (inst.reloc.exp.X_op == O_constant)
	    {
	      unsigned num = inst.reloc.exp.X_add_number;

	      /* Reject operations greater than 32, or lsl #32 */
	      if (num > 32 || (num == 32 && shft->value == 0))
		{
		  inst.error = "Invalid immediate shift";
		  return FAIL;
		}

	      /* Shifts of zero should be converted to lsl (which is zero)*/
	      if (num == 0)
		{
		  *str = p;
		  return SUCCESS;
		}

	      /* Shifts of 32 are encoded as 0, for those shifts that
		 support it.  */
	      if (num == 32)
		num = 0;

	      inst.instruction |= (num << 7) | shft->value;
	      *str = p;
	      return SUCCESS;
	    }

	  inst.reloc.type = BFD_RELOC_ARM_SHIFT_IMM;
	  inst.reloc.pc_rel = 0;
	  inst.instruction |= shft->value;
	  *str = p;
	  return SUCCESS;
	}
      else
	{
	  inst.error = unrestrict ? "shift requires register or #expression"
	    : "shift requires #expression";
	  *str = p;
	  return FAIL;
	}
    }

  inst.error = "Shift expression expected";
  return FAIL;
}

/* Do those data_ops which can take a negative immediate constant */
/* by altering the instuction. A bit of a hack really */
/*      MOV <-> MVN
        AND <-> BIC
        ADC <-> SBC
        by inverting the second operand, and
        ADD <-> SUB
        CMP <-> CMN
        by negating the second operand.
*/
static int
negate_data_op (instruction, value)
     unsigned long *instruction;
     unsigned long value;
{
  int op, new_inst;
  unsigned long negated, inverted;

  negated = validate_immediate (-value);
  inverted = validate_immediate (~value);

  op = (*instruction >> DATA_OP_SHIFT) & 0xf;
  switch (op)
    {
      /* First negates */
    case OPCODE_SUB:             /* ADD <-> SUB */
      new_inst = OPCODE_ADD;
      value = negated;
      break;

    case OPCODE_ADD: 
      new_inst = OPCODE_SUB;               
      value = negated;
      break;

    case OPCODE_CMP:             /* CMP <-> CMN */
      new_inst = OPCODE_CMN;
      value = negated;
      break;

    case OPCODE_CMN: 
      new_inst = OPCODE_CMP;               
      value = negated;
      break;

      /* Now Inverted ops */
    case OPCODE_MOV:             /* MOV <-> MVN */
      new_inst = OPCODE_MVN;               
      value = inverted;
      break;

    case OPCODE_MVN: 
      new_inst = OPCODE_MOV;
      value = inverted;
      break;

    case OPCODE_AND:             /* AND <-> BIC */ 
      new_inst = OPCODE_BIC;               
      value = inverted;
      break;

    case OPCODE_BIC: 
      new_inst = OPCODE_AND;
      value = inverted;
      break;

    case OPCODE_ADC:              /* ADC <-> SBC */
      new_inst = OPCODE_SBC;               
      value = inverted;
      break;

    case OPCODE_SBC: 
      new_inst = OPCODE_ADC;
      value = inverted;
      break;

      /* We cannot do anything */
    default:  
      return FAIL;
    }

  if (value == FAIL)
    return FAIL;

  *instruction &= OPCODE_MASK;
  *instruction |= new_inst << DATA_OP_SHIFT;
  return value; 
}

static int
data_op2 (str)
     char **str;
{
  int value;
  expressionS expr;

  while (**str == ' ')
    (*str)++;
    
  if (reg_required_here (str, 0) != FAIL)
    {
      if (skip_past_comma (str) == SUCCESS)
	{
	  /* Shift operation on register */
	  return decode_shift (str, NO_SHIFT_RESTRICT);
	}
      return SUCCESS;
    }
  else
    {
      /* Immediate expression */
      if (*((*str)++) == '#')
	{
	  inst.error = NULL;
	  if (my_get_expression (&inst.reloc.exp, str))
	    return FAIL;

	  if (inst.reloc.exp.X_add_symbol)
	    {
	      inst.reloc.type = BFD_RELOC_ARM_IMMEDIATE;
	      inst.reloc.pc_rel = 0;
	    }
	  else
	    {
	      if (skip_past_comma (str) == SUCCESS)
		{
		  /* #x, y -- ie explicit rotation by Y  */
		  if (my_get_expression (&expr, str))
		    return FAIL;

		  if (expr.X_op != O_constant)
		    {
		      inst.error = "Constant expression expected";
		      return FAIL;
		    }
 
		  /* Rotate must be a multiple of 2 */
		  if (((unsigned) expr.X_add_number) > 30
		      || (expr.X_add_number & 1) != 0
		      || ((unsigned) inst.reloc.exp.X_add_number) > 255)
		    {
		      inst.error = "Invalid constant";
		      return FAIL;
		    }
		  inst.instruction |= INST_IMMEDIATE;
		  inst.instruction |= inst.reloc.exp.X_add_number;
		  inst.instruction |= expr.X_add_number << 7;
		  return SUCCESS;
		}

	      /* Implicit rotation, select a suitable one  */
	      value = validate_immediate (inst.reloc.exp.X_add_number);

	      if (value == FAIL)
		{
		  /* Can't be done, perhaps the code reads something like
		     "add Rd, Rn, #-n", where "sub Rd, Rn, #n" would be ok */
		  if ((value = negate_data_op (&inst.instruction,
					       inst.reloc.exp.X_add_number))
		      == FAIL)
		    {
		      inst.error = "Invalid constant";
		      return FAIL;
		    }
		}

	      inst.instruction |= value;
	    }

	  inst.instruction |= INST_IMMEDIATE;
	  return SUCCESS;
	}

      inst.error = "Register or shift expression expected";
      return FAIL;
    }
}

static int
fp_op2 (str, flags)
     char **str;
     unsigned long flags;
{
  while (**str == ' ')
    (*str)++;

  if (fp_reg_required_here (str, 0) != FAIL)
    return SUCCESS;
  else
    {
      /* Immediate expression */
      if (*((*str)++) == '#')
	{
	  int i;

	  inst.error = NULL;
	  while (**str == ' ')
	    (*str)++;

	  /* First try and match exact strings, this is to guarantee that
	     some formats will work even for cross assembly */

	  for (i = 0; fp_const[i]; i++)
	    {
	      if (strncmp (*str, fp_const[i], strlen (fp_const[i])) == 0)
		{
		  char *start = *str;

		  *str += strlen (fp_const[i]);
		  if (is_end_of_line[(int)**str] || **str == '\0')
		    {
		      inst.instruction |= i + 8;
		      return SUCCESS;
		    }
		  *str = start;
		}
	    }

	  /* Just because we didn't get a match doesn't mean that the
	     constant isn't valid, just that it is in a format that we
	     don't automatically recognize.  Try parsing it with
	     the standard expression routines.  */
	  if ((i = my_get_float_expression (str)) >= 0)
	    {
	      inst.instruction |= i + 8;
	      return SUCCESS;
	    }

	  inst.error = "Invalid floating point immediate expression";
	  return FAIL;
	}
      inst.error = "Floating point register or immediate expression expected";
      return FAIL;
    }
}

static void
do_arit (str, flags)
     char *str;
     unsigned long flags;
{
  while (*str == ' ')
    str++;

  if (reg_required_here (&str, 12) == FAIL
      || skip_past_comma (&str) == FAIL
      || reg_required_here (&str, 16) == FAIL
      || skip_past_comma (&str) == FAIL
      || data_op2 (&str) == FAIL)
    {
      if (!inst.error)
	inst.error = bad_args;
      return;
    }

  inst.instruction |= flags;
  end_of_line (str);
  return;
}

static void
do_adr (str, flags)
     char *str;
     unsigned long flags;
{
  /* This is a pseudo-op of the form "adr rd, label" to be converted
     into a relative address of the form "add rd, pc, #label-.-8" */

  while (*str == ' ')
    str++;

  if (reg_required_here (&str, 12) == FAIL
      || skip_past_comma (&str) == FAIL
      || my_get_expression (&inst.reloc.exp, &str))
    {
      if (!inst.error)
	inst.error = bad_args;
      return;
    }
  /* Frag hacking will turn this into a sub instruction if the offset turns
     out to be negative.  */
  inst.reloc.type = BFD_RELOC_ARM_IMMEDIATE;
  inst.reloc.exp.X_add_number -= 8; /* PC relative adjust */
  inst.reloc.pc_rel = 1;
  inst.instruction |= flags;
  end_of_line (str);
  return;
}

static void
do_cmp (str, flags)
     char *str;
     unsigned long flags;
{
  while (*str == ' ')
    str++;

  if (reg_required_here (&str, 16) == FAIL)
    {
      if (!inst.error)
	inst.error = bad_args;
      return;
    }

  if (skip_past_comma (&str) == FAIL
      || data_op2 (&str) == FAIL)
    {
      if (!inst.error)
	inst.error = bad_args;
      return;
    }

  inst.instruction |= flags;
  if ((flags & 0x0000f000) == 0)
    inst.instruction |= CONDS_BIT;

  end_of_line (str);
  return;
}

static void
do_mov (str, flags)
     char *str;
     unsigned long flags;
{
  while (*str == ' ')
    str++;

  if (reg_required_here (&str, 12) == FAIL)
    {
      if (!inst.error)
	inst.error = bad_args;
      return;
    }

  if (skip_past_comma (&str) == FAIL
      || data_op2 (&str) == FAIL)
    {
      if (!inst.error)
	inst.error = bad_args;
      return;
    }

  inst.instruction |= flags;
  end_of_line (str);
  return;
}

static int
ldst_extend (str, hwse)
     char **str;
     int hwse;
{
  int add = INDEX_UP;

  switch (**str)
    {
    case '#':
      (*str)++;
      if (my_get_expression (&inst.reloc.exp, str))
	return FAIL;

      if (inst.reloc.exp.X_op == O_constant)
	{
	  int value = inst.reloc.exp.X_add_number;

          if ((hwse && (value < -255 || value > 255))
               || (value < -4095 || value > 4095))
	    {
	      inst.error = "address offset too large";
	      return FAIL;
	    }

	  if (value < 0)
	    {
	      value = -value;
	      add = 0;
	    }

          /* Halfword and signextension instructions have the
             immediate value split across bits 11..8 and bits 3..0 */
          if (hwse)
            inst.instruction |= add | HWOFFSET_IMM | (value >> 4) << 8 | value & 0xF;
          else
            inst.instruction |= add | value;
	}
      else
	{
          if (hwse)
            {
              inst.instruction |= HWOFFSET_IMM;
              inst.reloc.type = BFD_RELOC_ARM_OFFSET_IMM8;
            }
          else
            inst.reloc.type = BFD_RELOC_ARM_OFFSET_IMM;
	  inst.reloc.pc_rel = 0;
	}
      return SUCCESS;

    case '-':
      add = 0;	/* and fall through */
    case '+':
      (*str)++;	/* and fall through */
    default:
      if (reg_required_here (str, 0) == FAIL)
	{
	  inst.error = "Register expected";
	  return FAIL;
	}

      if (hwse)
        inst.instruction |= add;
      else
        {
          inst.instruction |= add | OFFSET_REG;
          if (skip_past_comma (str) == SUCCESS)
            return decode_shift (str, SHIFT_RESTRICT);
        }

      return SUCCESS;
    }
}

static void
do_ldst (str, flags)
     char *str;
     unsigned long flags;
{
  int halfword = 0;
  int pre_inc = 0;
  int conflict_reg;
  int value;

  /* This is not ideal, but it is the simplest way of dealing with the
     ARM7T halfword instructions (since they use a different
     encoding, but the same mnemonic): */
  if (halfword = ((flags & 0x80000000) != 0))
    {
      /* This is actually a load/store of a halfword, or a
         signed-extension load */
      if ((cpu_variant & ARM_ARCH4) == 0)
        {
          inst.error
           = "Processor does not support halfwords or signed bytes\n";
          return;
        }

      inst.instruction = (inst.instruction & COND_MASK)
                         | (flags & ~COND_MASK);

      flags = 0;
    }

  while (*str == ' ')
    str++;
    
  if ((conflict_reg = reg_required_here (&str, 12)) == FAIL)
    {
      if (!inst.error)
	inst.error = bad_args;
      return;
    }

  if (skip_past_comma (&str) == FAIL)
    {
      inst.error = "Address expected";
      return;
    }

  if (*str == '[')
    {
      int reg;

      str++;
      while (*str == ' ')
	str++;

      if ((reg = reg_required_here (&str, 16)) == FAIL)
	{
	  inst.error = "Register required";
	  return;
	}

      conflict_reg = (((conflict_reg == reg)
		       && (inst.instruction & LOAD_BIT))
		      ? 1 : 0);

      while (*str == ' ')
	str++;

      if (*str == ']')
	{
	  str++;
	  if (skip_past_comma (&str) == SUCCESS)
	    {
	      /* [Rn],... (post inc) */
	      if (ldst_extend (&str, halfword) == FAIL)
		return;
	      if (conflict_reg)
		as_warn ("destination register same as write-back base\n");
	    }
	  else
	    {
	      /* [Rn] */
              if (halfword)
                inst.instruction |= HWOFFSET_IMM;

              while (*str == ' ')
               str++;

              if (*str == '!')
               {
                 if (conflict_reg)
                  as_warn ("destination register same as write-back base\n");
                 str++;
                 inst.instruction |= WRITE_BACK;
               }

	      flags |= INDEX_UP;
	      if (! (flags & TRANS_BIT))
		pre_inc = 1;
	    }
	}
      else
	{
	  /* [Rn,...] */
	  if (skip_past_comma (&str) == FAIL)
	    {
	      inst.error = "pre-indexed expression expected";
	      return;
	    }

	  pre_inc = 1;
	  if (ldst_extend (&str, halfword) == FAIL)
	    return;

	  while (*str == ' ')
	    str++;

	  if (*str++ != ']')
	    {
	      inst.error = "missing ]";
	      return;
	    }

	  while (*str == ' ')
	    str++;

	  if (*str == '!')
	    {
	      if (conflict_reg)
		as_tsktsk ("destination register same as write-back base\n");
	      str++;
	      inst.instruction |= WRITE_BACK;
	    }
	}
    }
  else if (*str == '=')
    {
      /* Parse an "ldr Rd, =expr" instruction; this is another pseudo op */
      str++;

      while (*str == ' ')
	str++;

      if (my_get_expression (&inst.reloc.exp, &str))
	return;

      if (inst.reloc.exp.X_op != O_constant
	  && inst.reloc.exp.X_op != O_symbol)
	{
	  inst.error = "Constant expression expected";
	  return;
	}

      if (inst.reloc.exp.X_op == O_constant
	  && (value = validate_immediate(inst.reloc.exp.X_add_number)) != FAIL)
	{
	  /* This can be done with a mov instruction */
	  inst.instruction &= LITERAL_MASK;
	  inst.instruction |= INST_IMMEDIATE | (OPCODE_MOV << DATA_OP_SHIFT);
	  inst.instruction |= (flags & COND_MASK) | (value & 0xfff);
	  end_of_line(str);
	  return; 
	}
      else
	{
	  /* Insert into literal pool */     
	  if (add_to_lit_pool () == FAIL)
	    {
	      if (!inst.error)
		inst.error = "literal pool insertion failed\n"; 
	      return;
	    }

	  /* Change the instruction exp to point to the pool */
          if (halfword)
            {
              inst.instruction |= HWOFFSET_IMM;
              inst.reloc.type = BFD_RELOC_ARM_HWLITERAL;
            }
          else
	    inst.reloc.type = BFD_RELOC_ARM_LITERAL;
	  inst.reloc.pc_rel = 1;
	  inst.instruction |= (REG_PC << 16);
	  pre_inc = 1; 
	}
    }
  else
    {
      if (my_get_expression (&inst.reloc.exp, &str))
	return;

      if (halfword)
        {
          inst.instruction |= HWOFFSET_IMM;
          inst.reloc.type = BFD_RELOC_ARM_OFFSET_IMM8;
        }
      else
        inst.reloc.type = BFD_RELOC_ARM_OFFSET_IMM;
      inst.reloc.exp.X_add_number -= 8;  /* PC rel adjust */
      inst.reloc.pc_rel = 1;
      inst.instruction |= (REG_PC << 16);
      pre_inc = 1;
    }
    
  if (pre_inc && (flags & TRANS_BIT))
    inst.error = "Pre-increment instruction with translate";

  inst.instruction |= flags | (pre_inc ? PRE_INDEX : 0);
  end_of_line (str);
  return;
}

static long
reg_list (strp)
     char **strp;
{
  char *str = *strp;
  long range = 0;
  int another_range;

  /* We come back here if we get ranges concatenated by '+' or '|' */
  do
    {
      another_range = 0;

      if (*str == '{')
	{
	  int in_range = 0;
	  int cur_reg = -1;
      
	  str++;
	  do
	    {
	      int reg;
	    
	      while (*str == ' ')
		str++;

	      if ((reg = arm_reg_parse (&str)) == FAIL || !int_register (reg))
		{
		  inst.error = "Register expected";
		  return FAIL;
		}

	      if (in_range)
		{
		  int i;
	      
		  if (reg <= cur_reg)
		    {
		      inst.error = "Bad range in register list";
		      return FAIL;
		    }

		  for (i = cur_reg + 1; i < reg; i++)
		    {
		      if (range & (1 << i))
			as_tsktsk 
			  ("Warning: Duplicated register (r%d) in register list",
			   i);
		      else
			range |= 1 << i;
		    }
		  in_range = 0;
		}

	      if (range & (1 << reg))
		as_tsktsk ("Warning: Duplicated register (r%d) in register list",
			   reg);
	      else if (reg <= cur_reg)
		as_tsktsk ("Warning: Register range not in ascending order");

	      range |= 1 << reg;
	      cur_reg = reg;
	    } while (skip_past_comma (&str) != FAIL
		     || (in_range = 1, *str++ == '-'));
	  str--;
	  while (*str == ' ')
	    str++;

	  if (*str++ != '}')
	    {
	      inst.error = "Missing `}'";
	      return FAIL;
	    }
	}
      else
	{
	  expressionS expr;

	  if (my_get_expression (&expr, &str))
	    return FAIL;

	  if (expr.X_op == O_constant)
	    {
	      if (expr.X_add_number 
		  != (expr.X_add_number & 0x0000ffff))
		{
		  inst.error = "invalid register mask";
		  return FAIL;
		}

	      if ((range & expr.X_add_number) != 0)
		{
		  int regno = range & expr.X_add_number;

		  regno &= -regno;
		  regno = (1 << regno) - 1;
		  as_tsktsk 
		    ("Warning: Duplicated register (r%d) in register list",
		     regno);
		}

	      range |= expr.X_add_number;
	    }
	  else
	    {
	      if (inst.reloc.type != 0)
		{
		  inst.error = "expression too complex";
		  return FAIL;
		}

	      memcpy (&inst.reloc.exp, &expr, sizeof (expressionS));
	      inst.reloc.type = BFD_RELOC_ARM_MULTI;
	      inst.reloc.pc_rel = 0;
	    }
	}

      while (*str == ' ')
	str++;

      if (*str == '|' || *str == '+')
	{
	  str++;
	  another_range = 1;
	}
    } while (another_range);

  *strp = str;
  return range;
}

static void
do_ldmstm (str, flags)
     char *str;
     unsigned long flags;
{
  int base_reg;
  long range;

  while (*str == ' ')
    str++;

  if ((base_reg = reg_required_here (&str, 16)) == FAIL)
    {
      if (!inst.error)
	inst.error = bad_args;
      return;
    }

  if (base_reg == REG_PC)
    {
      inst.error = "r15 not allowed as base register";
      return;
    }

  while (*str == ' ')
    str++;
  if (*str == '!')
    {
      flags |= WRITE_BACK;
      str++;
    }

  if (skip_past_comma (&str) == FAIL
      || (range = reg_list (&str)) == FAIL)
    {
      if (! inst.error)
	inst.error = bad_args;
      return;
    }

  if (*str == '^')
    {
      str++;
      flags |= MULTI_SET_PSR;
    }

  inst.instruction |= flags | range;
  end_of_line (str);
  return;
}

static void
do_swi (str, flags)
     char *str;
     unsigned long flags;
{
  /* Allow optional leading '#'.  */
  while (*str == ' ')
    str++;
  if (*str == '#')
    str++;

  if (my_get_expression (&inst.reloc.exp, &str))
    return;

  inst.reloc.type = BFD_RELOC_ARM_SWI;
  inst.reloc.pc_rel = 0;
  inst.instruction |= flags;
  end_of_line (str);
  return;
}

static void
do_swap (str, flags)
     char *str;
     unsigned long flags;
{
  int reg;
  
  while (*str == ' ')
    str++;

  if ((reg = reg_required_here (&str, 12)) == FAIL)
    return;

  if (reg == REG_PC)
    {
      inst.error = "r15 not allowed in swap";
      return;
    }

  if (skip_past_comma (&str) == FAIL
      || (reg = reg_required_here (&str, 0)) == FAIL)
    {
      if (!inst.error)
	inst.error = bad_args;
      return;
    }

  if (reg == REG_PC)
    {
      inst.error = "r15 not allowed in swap";
      return;
    }

  if (skip_past_comma (&str) == FAIL
      || *str++ != '[')
    {
      inst.error = bad_args;
      return;
    }

  while (*str == ' ')
    str++;

  if ((reg = reg_required_here (&str, 16)) == FAIL)
    return;

  if (reg == REG_PC)
    {
      inst.error = bad_pc;
      return;
    }

  while (*str == ' ')
    str++;

  if (*str++ != ']')
    {
      inst.error = "missing ]";
      return;
    }

  inst.instruction |= flags;
  end_of_line (str);
  return;
}

static void
do_branch (str, flags)
     char *str;
     unsigned long flags;
{
  if (my_get_expression (&inst.reloc.exp, &str))
    return;
  inst.reloc.type = BFD_RELOC_ARM_PCREL_BRANCH;
  inst.reloc.pc_rel = 1;
  end_of_line (str);
  return;
}

static void
do_bx (str, flags)
     char *str;
     unsigned long flags;
{
  int reg;

  while (*str == ' ')
    str++;

  if ((reg = reg_required_here (&str, 0)) == FAIL)
    return;

  if (reg == REG_PC)
    as_tsktsk ("Use of r15 in bx has undefined behaviour");

  end_of_line (str);
  return;
}

static void
do_cdp (str, flags)
     char *str;
     unsigned long flags;
{
  /* Co-processor data operation.
     Format: CDP{cond} CP#,<expr>,CRd,CRn,CRm{,<expr>}  */
  while (*str == ' ')
    str++;

  if (co_proc_number (&str) == FAIL)
    {
      if (!inst.error)
	inst.error = bad_args;
      return;
    }

  if (skip_past_comma (&str) == FAIL
      || cp_opc_expr (&str, 20,4) == FAIL)
    {
      if (!inst.error)
	inst.error = bad_args;
      return;
    }

  if (skip_past_comma (&str) == FAIL
      || cp_reg_required_here (&str, 12) == FAIL)
    {
      if (!inst.error)
	inst.error = bad_args;
      return;
    }

  if (skip_past_comma (&str) == FAIL
      || cp_reg_required_here (&str, 16) == FAIL)
    {
      if (!inst.error)
	inst.error = bad_args;
      return;
    }

  if (skip_past_comma (&str) == FAIL
      || cp_reg_required_here (&str, 0) == FAIL)
    {
      if (!inst.error)
	inst.error = bad_args;
      return;
    }

  if (skip_past_comma (&str) == SUCCESS)
    {
      if (cp_opc_expr (&str, 5, 3) == FAIL)
	{
	  if (!inst.error)
	    inst.error = bad_args;
	  return;
	}
    }

  end_of_line (str);
  return;
}

static void
do_lstc (str, flags)
     char *str;
     unsigned long flags;
{
  /* Co-processor register load/store.
     Format: <LDC|STC{cond}[L] CP#,CRd,<address>  */

  while (*str == ' ')
    str++;

  if (co_proc_number (&str) == FAIL)
    {
      if (!inst.error)
	inst.error = bad_args;
      return;
    }

  if (skip_past_comma (&str) == FAIL
      || cp_reg_required_here (&str, 12) == FAIL)
    {
      if (!inst.error)
	inst.error = bad_args;
      return;
    }

  if (skip_past_comma (&str) == FAIL
      || cp_address_required_here (&str) == FAIL)
    {
      if (! inst.error)
	inst.error = bad_args;
      return;
    }

  inst.instruction |= flags;
  end_of_line (str);
  return;
}

static void
do_co_reg (str, flags)
     char *str;
     unsigned long flags;
{
  /* Co-processor register transfer.
     Format: <MCR|MRC>{cond} CP#,<expr1>,Rd,CRn,CRm{,<expr2>}  */

  while (*str == ' ')
    str++;

  if (co_proc_number (&str) == FAIL)
    {
      if (!inst.error)
	inst.error = bad_args;
      return;
    }

  if (skip_past_comma (&str) == FAIL
      || cp_opc_expr (&str, 21, 3) == FAIL)
    {
      if (!inst.error)
	inst.error = bad_args;
      return;
    }

  if (skip_past_comma (&str) == FAIL
      || reg_required_here (&str, 12) == FAIL)
    {
      if (!inst.error)
	inst.error = bad_args;
      return;
    }

  if (skip_past_comma (&str) == FAIL
      || cp_reg_required_here (&str, 16) == FAIL)
    {
      if (!inst.error)
	inst.error = bad_args;
      return;
    }

  if (skip_past_comma (&str) == FAIL
      || cp_reg_required_here (&str, 0) == FAIL)
    {
      if (!inst.error)
	inst.error = bad_args;
      return;
    }

  if (skip_past_comma (&str) == SUCCESS)
    {
      if (cp_opc_expr (&str, 5, 3) == FAIL)
	{
	  if (!inst.error)
	    inst.error = bad_args;
	  return;
	}
    }

  end_of_line (str);
  return;
}

static void
do_fp_ctrl (str, flags)
     char *str;
     unsigned long flags;
{
  /* FP control registers.
     Format: <WFS|RFS|WFC|RFC>{cond} Rn  */

  while (*str == ' ')
    str++;

  if (reg_required_here (&str, 12) == FAIL)
    {
      if (!inst.error)
	inst.error = bad_args;
      return;
    }

  end_of_line (str);
  return;
}

static void
do_fp_ldst (str, flags)
     char *str;
     unsigned long flags;
{
  while (*str == ' ')
    str++;

  switch (inst.suffix)
    {
    case SUFF_S:
      break;
    case SUFF_D:
      inst.instruction |= CP_T_X;
      break;
    case SUFF_E:
      inst.instruction |= CP_T_Y;
      break;
    case SUFF_P:
      inst.instruction |= CP_T_X | CP_T_Y;
      break;
    default:
      abort ();
    }

  if (fp_reg_required_here (&str, 12) == FAIL)
    {
      if (!inst.error)
	inst.error = bad_args;
      return;
    }

  if (skip_past_comma (&str) == FAIL
      || cp_address_required_here (&str) == FAIL)
    {
      if (!inst.error)
	inst.error = bad_args;
      return;
    }

  end_of_line (str);
}

static void
do_fp_ldmstm (str, flags)
     char *str;
     unsigned long flags;
{
  int num_regs;

  while (*str == ' ')
    str++;

  if (fp_reg_required_here (&str, 12) == FAIL)
    {
      if (! inst.error)
	inst.error = bad_args;
      return;
    }

  /* Get Number of registers to transfer */
  if (skip_past_comma (&str) == FAIL
      || my_get_expression (&inst.reloc.exp, &str))
    {
      if (! inst.error)
	inst.error = "constant expression expected";
      return;
    }

  if (inst.reloc.exp.X_op != O_constant)
    {
      inst.error = "Constant value required for number of registers";
      return;
    }

  num_regs = inst.reloc.exp.X_add_number;

  if (num_regs < 1 || num_regs > 4)
    {
      inst.error = "number of registers must be in the range [1:4]";
      return;
    }

  switch (num_regs)
    {
    case 1:
      inst.instruction |= CP_T_X;
      break;
    case 2:
      inst.instruction |= CP_T_Y;
      break;
    case 3:
      inst.instruction |= CP_T_Y | CP_T_X;
      break;
    case 4:
      break;
    default:
      abort ();
    }

  if (flags)
    {
      int reg;
      int write_back;
      int offset;

      /* The instruction specified "ea" or "fd", so we can only accept
	 [Rn]{!}.  The instruction does not really support stacking or
	 unstacking, so we have to emulate these by setting appropriate
	 bits and offsets.  */
      if (skip_past_comma (&str) == FAIL
	  || *str != '[')
	{
	  if (! inst.error)
	    inst.error = bad_args;
	  return;
	}

      str++;
      while (*str == ' ')
	str++;

      if ((reg = reg_required_here (&str, 16)) == FAIL)
	{
	  inst.error = "Register required";
	  return;
	}

      while (*str == ' ')
	str++;

      if (*str != ']')
	{
	  inst.error = bad_args;
	  return;
	}

      str++;
      if (*str == '!')
	{
	  write_back = 1;
	  str++;
	  if (reg == REG_PC)
	    {
	      inst.error = "R15 not allowed as base register with write-back";
	      return;
	    }
	}
      else
	write_back = 0;

      if (flags & CP_T_Pre)
	{
	  /* Pre-decrement */
	  offset = 3 * num_regs;
	  if (write_back)
	    flags |= CP_T_WB;
	}
      else
	{
	  /* Post-increment */
	  if (write_back)
	    {
	      flags |= CP_T_WB;
	      offset = 3 * num_regs;
	    }
	  else
	    {
	      /* No write-back, so convert this into a standard pre-increment
		 instruction -- aesthetically more pleasing.  */
	      flags = CP_T_Pre | CP_T_UD;
	      offset = 0;
	    }
	}

      inst.instruction |= flags | offset;
    }
  else if (skip_past_comma (&str) == FAIL
	   || cp_address_required_here (&str) == FAIL)
    {
      if (! inst.error)
	inst.error = bad_args;
      return;
    }

  end_of_line (str);
}

static void
do_fp_dyadic (str, flags)
     char *str;
     unsigned long flags;
{
  while (*str == ' ')
    str++;

  switch (inst.suffix)
    {
    case SUFF_S:
      break;
    case SUFF_D:
      inst.instruction |= 0x00000080;
      break;
    case SUFF_E:
      inst.instruction |= 0x00080000;
      break;
    default:
      abort ();
    }

  if (fp_reg_required_here (&str, 12) == FAIL)
    {
      if (! inst.error)
	inst.error = bad_args;
      return;
    }

  if (skip_past_comma (&str) == FAIL
      || fp_reg_required_here (&str, 16) == FAIL)
    {
      if (! inst.error)
	inst.error = bad_args;
      return;
    }

  if (skip_past_comma (&str) == FAIL
      || fp_op2 (&str) == FAIL)
    {
      if (! inst.error)
	inst.error = bad_args;
      return;
    }

  inst.instruction |= flags;
  end_of_line (str);
  return;
}

static void
do_fp_monadic (str, flags)
     char *str;
     unsigned long flags;
{
  while (*str == ' ')
    str++;

  switch (inst.suffix)
    {
    case SUFF_S:
      break;
    case SUFF_D:
      inst.instruction |= 0x00000080;
      break;
    case SUFF_E:
      inst.instruction |= 0x00080000;
      break;
    default:
      abort ();
    }

  if (fp_reg_required_here (&str, 12) == FAIL)
    {
      if (! inst.error)
	inst.error = bad_args;
      return;
    }

  if (skip_past_comma (&str) == FAIL
      || fp_op2 (&str) == FAIL)
    {
      if (! inst.error)
	inst.error = bad_args;
      return;
    }

  inst.instruction |= flags;
  end_of_line (str);
  return;
}

static void
do_fp_cmp (str, flags)
     char *str;
     unsigned long flags;
{
  while (*str == ' ')
    str++;

  if (fp_reg_required_here (&str, 16) == FAIL)
    {
      if (! inst.error)
	inst.error = bad_args;
      return;
    }

  if (skip_past_comma (&str) == FAIL
      || fp_op2 (&str) == FAIL)
    {
      if (! inst.error)
	inst.error = bad_args;
      return;
    }

  inst.instruction |= flags;
  end_of_line (str);
  return;
}

static void
do_fp_from_reg (str, flags)
     char *str;
     unsigned long flags;
{
  while (*str == ' ')
    str++;

  switch (inst.suffix)
    {
    case SUFF_S:
      break;
    case SUFF_D:
      inst.instruction |= 0x00000080;
      break;
    case SUFF_E:
      inst.instruction |= 0x00080000;
      break;
    default:
      abort ();
    }

  if (fp_reg_required_here (&str, 16) == FAIL)
    {
      if (! inst.error)
	inst.error = bad_args;
      return;
    }

  if (skip_past_comma (&str) == FAIL
      || reg_required_here (&str, 12) == FAIL)
    {
      if (! inst.error)
	inst.error = bad_args;
      return;
    }

  inst.instruction |= flags;
  end_of_line (str);
  return;
}

static void
do_fp_to_reg (str, flags)
     char *str;
     unsigned long flags;
{
  while (*str == ' ')
    str++;

  if (reg_required_here (&str, 12) == FAIL)
    {
      if (! inst.error)
	inst.error = bad_args;
      return;
    }

  if (skip_past_comma (&str) == FAIL
      || fp_reg_required_here (&str, 0) == FAIL)
    {
      if (! inst.error)
	inst.error = bad_args;
      return;
    }

  inst.instruction |= flags;
  end_of_line (str);
  return;
}

/* Thumb specific routines */

/* Parse and validate that a register is of the right form, this saves
   repeated checking of this information in many similar cases. 
   Unlike the 32-bit case we do not insert the register into the opcode 
   here, since the position is often unknown until the full instruction 
   has been parsed.  */
static int
thumb_reg (strp, hi_lo)
     char **strp;
     int hi_lo;
{
  int reg;

  if ((reg = arm_reg_parse (strp)) == FAIL || ! int_register (reg))
    {
      inst.error = "Register expected";
      return FAIL;
    }

  switch (hi_lo)
    {
    case THUMB_REG_LO:
      if (reg > 7)
	{
	  inst.error = "lo register required";
	  return FAIL;
	}
      break;

    case THUMB_REG_HI:
      if (reg < 8)
	{
	  inst.error = "hi register required";
	  return FAIL;
	}
      break;

    default:
      break;
    }

  return reg;
}

/* Parse an add or subtract instruction, SUBTRACT is non-zero if the opcode
   was SUB.  */
static void
thumb_add_sub (str, subtract)
     char *str;
     int subtract;
{
  int Rd, Rs, Rn = FAIL;

  while (*str == ' ')
    str++;

  if ((Rd = thumb_reg (&str, THUMB_REG_ANY)) == FAIL
      || skip_past_comma (&str) == FAIL)
    {
      if (! inst.error)
	inst.error = bad_args;
      return;
    }

  if (*str == '#')
    {
      Rs = Rd;
      str++;
      if (my_get_expression (&inst.reloc.exp, &str))
	return;
    }
  else
    {
      if ((Rs = thumb_reg (&str, THUMB_REG_ANY)) == FAIL)
	return;

      if (skip_past_comma (&str) == FAIL)
	{
	  /* Two operand format, shuffle the registers and pretend there 
	     are 3 */
	  Rn = Rs;
	  Rs = Rd;
	}
      else if (*str == '#')
	{
	  str++;
	  if (my_get_expression (&inst.reloc.exp, &str))
	    return;
	}
      else if ((Rn = thumb_reg (&str, THUMB_REG_ANY)) == FAIL)
	return;
    }

  /* We now have Rd and Rs set to registers, and Rn set to a register or FAIL;
     for the latter case, EXPR contains the immediate that was found. */
  if (Rn != FAIL)
    {
      /* All register format.  */
      if (Rd > 7 || Rs > 7 || Rd > 7)
	{
	  if (Rs != Rd)
	    {
	      inst.error = "dest and source1 must be the same register";
	      return;
	    }

	  /* Can't do this for SUB */
	  if (subtract)
	    {
	      inst.error = "subtract valid only on lo regs";
	      return;
	    }

	  inst.instruction = (T_OPCODE_ADD_HI
			      | (Rd > 7 ? THUMB_H1 : 0)
			      | (Rn > 7 ? THUMB_H2 : 0));
	  inst.instruction |= (Rd & 7) | ((Rn & 7) << 3);
	}
      else
	{
	  inst.instruction = subtract ? T_OPCODE_SUB_R3 : T_OPCODE_ADD_R3;
	  inst.instruction |= Rd | (Rs << 3) | (Rn << 6);
	}
    }
  else
    {
      /* Immediate expression, now things start to get nasty.  */

      /* First deal with HI regs, only very restricted cases allowed:
	 Adjusting SP, and using PC or SP to get an address.  */
      if ((Rd > 7 && (Rd != REG_SP || Rs != REG_SP))
	  || (Rs > 7 && Rs != REG_SP && Rs != REG_PC))
	{
	  inst.error = "invalid Hi register with immediate";
	  return;
	}

      if (inst.reloc.exp.X_op != O_constant)
	{
	  /* Value isn't known yet, all we can do is store all the fragments
	     we know about in the instruction and let the reloc hacking 
	     work it all out.  */
	  inst.instruction = (subtract ? 0x8000 : 0) | (Rd << 4) | Rs;
	  inst.reloc.type = BFD_RELOC_ARM_THUMB_ADD;
	}
      else
	{
	  int offset = inst.reloc.exp.X_add_number;

	  if (subtract)
	    offset = -offset;

	  if (offset < 0)
	    {
	      offset = -offset;
	      subtract = 1;

	      /* Quick check, in case offset is MIN_INT */
	      if (offset < 0)
		{
		  inst.error = "immediate value out of range";
		  return;
		}
	    }
	  else
	    subtract = 0;

	  if (Rd == REG_SP)
	    {
	      if (offset & ~0x1fc)
		{
		  inst.error = "invalid immediate value for stack adjust";
		  return;
		}
	      inst.instruction = subtract ? T_OPCODE_SUB_ST : T_OPCODE_ADD_ST;
	      inst.instruction |= offset >> 2;
	    }
	  else if (Rs == REG_PC || Rs == REG_SP)
	    {
	      if (subtract
		  || (offset & ~0x3fc))
		{
		  inst.error = "invalid immediate for address calculation";
		  return;
		}
	      inst.instruction = (Rs == REG_PC ? T_OPCODE_ADD_PC
				  : T_OPCODE_ADD_SP);
	      inst.instruction |= (Rd << 8) | (offset >> 2);
	    }
	  else if (Rs == Rd)
	    {
	      if (offset & ~0xff)
		{
		  inst.error = "immediate value out of range";
		  return;
		}
	      inst.instruction = subtract ? T_OPCODE_SUB_I8 : T_OPCODE_ADD_I8;
	      inst.instruction |= (Rd << 8) | offset;
	    }
	  else
	    {
	      if (offset & ~0x7)
		{
		  inst.error = "immediate value out of range";
		  return;
		}
	      inst.instruction = subtract ? T_OPCODE_SUB_I3 : T_OPCODE_ADD_I3;
	      inst.instruction |= Rd | (Rs << 3) | (offset << 6);
	    }
	}
    }
  end_of_line (str);
}

static void
thumb_shift (str, shift)
     char *str;
     int shift;
{
  int Rd, Rs, Rn = FAIL;

  while (*str == ' ')
    str++;

  if ((Rd = thumb_reg (&str, THUMB_REG_LO)) == FAIL
      || skip_past_comma (&str) == FAIL)
    {
      if (! inst.error)
	inst.error = bad_args;
      return;
    }

  if (*str == '#')
    {
      /* Two operand immediate format, set Rs to Rd.  */
      Rs = Rd;
      str++;
      if (my_get_expression (&inst.reloc.exp, &str))
	return;
    }
  else
    {
      if ((Rs =  thumb_reg (&str, THUMB_REG_LO)) == FAIL)
	return;

      if (skip_past_comma (&str) == FAIL)
	{
	  /* Two operand format, shuffle the registers and pretend there
	     are 3 */
	  Rn = Rs;
	  Rs = Rd;
	}
      else if (*str == '#')
	{
	  str++;
	  if (my_get_expression (&inst.reloc.exp, &str))
	    return;
	}
      else if ((Rn = thumb_reg (&str, THUMB_REG_LO)) == FAIL)
	return;
    }

  /* We now have Rd and Rs set to registers, and Rn set to a register or FAIL;
     for the latter case, EXPR contains the immediate that was found. */

  if (Rn != FAIL)
    {
      if (Rs != Rd)
	{
	  inst.error = "source1 and dest must be same register";
	  return;
	}

      switch (shift)
	{
	case THUMB_ASR: inst.instruction = T_OPCODE_ASR_R; break;
	case THUMB_LSL: inst.instruction = T_OPCODE_LSL_R; break;
	case THUMB_LSR: inst.instruction = T_OPCODE_LSR_R; break;
	}

      inst.instruction |= Rd | (Rn << 3);
    }
  else
    {
      switch (shift)
	{
	case THUMB_ASR: inst.instruction = T_OPCODE_ASR_I; break;
	case THUMB_LSL: inst.instruction = T_OPCODE_LSL_I; break;
	case THUMB_LSR: inst.instruction = T_OPCODE_LSR_I; break;
	}

      if (inst.reloc.exp.X_op != O_constant)
	{
	  /* Value isn't known yet, create a dummy reloc and let reloc
	     hacking fix it up */

	  inst.reloc.type = BFD_RELOC_ARM_THUMB_SHIFT;
	}
      else
	{
	  unsigned shift_value = inst.reloc.exp.X_add_number;

	  if (shift_value > 32 || (shift_value == 32 && shift == THUMB_LSL))
	    {
	      inst.error = "Invalid immediate for shift";
	      return;
	    }

	  /* Shifts of zero are handled by converting to LSL */
	  if (shift_value == 0)
	    inst.instruction = T_OPCODE_LSL_I;

	  /* Shifts of 32 are encoded as a shift of zero */
	  if (shift_value == 32)
	    shift_value = 0;

	  inst.instruction |= shift_value << 6;
	}

      inst.instruction |= Rd | (Rs << 3);
    }
  end_of_line (str);
}

static void
thumb_mov_compare (str, move)
     char *str;
     int move;
{
  int Rd, Rs = FAIL;

  while (*str == ' ')
    str++;

  if ((Rd = thumb_reg (&str, THUMB_REG_ANY)) == FAIL
      || skip_past_comma (&str) == FAIL)
    {
      if (! inst.error)
	inst.error = bad_args;
      return;
    }

  if (*str == '#')
    {
      str++;
      if (my_get_expression (&inst.reloc.exp, &str))
	return;
    }
  else if ((Rs = thumb_reg (&str, THUMB_REG_ANY)) == FAIL)
    return;

  if (Rs != FAIL)
    {
      if (Rs < 8 && Rd < 8)
	{
	  if (move == THUMB_MOVE)
	    /* A move of two lowregs is, by convention, encoded as
	       ADD Rd, Rs, #0 */
	    inst.instruction = T_OPCODE_ADD_I3;
	  else
	    inst.instruction = T_OPCODE_CMP_LR;
	  inst.instruction |= Rd | (Rs << 3);
	}
      else
	{
	  if (move == THUMB_MOVE)
	    inst.instruction = T_OPCODE_MOV_HR;
	  else
	    inst.instruction = T_OPCODE_CMP_HR;

	  if (Rd > 7)
	    inst.instruction |= THUMB_H1;

	  if (Rs > 7)
	    inst.instruction |= THUMB_H2;

	  inst.instruction |= (Rd & 7) | ((Rs & 7) << 3);
	}
    }
  else
    {
      if (Rd > 7)
	{
	  inst.error = "only lo regs allowed with immediate";
	  return;
	}

      if (move == THUMB_MOVE)
	inst.instruction = T_OPCODE_MOV_I8;
      else
	inst.instruction = T_OPCODE_CMP_I8;

      inst.instruction |= Rd << 8;

      if (inst.reloc.exp.X_op != O_constant)
	inst.reloc.type = BFD_RELOC_ARM_THUMB_IMM;
      else
	{
	  unsigned value = inst.reloc.exp.X_add_number;

	  if (value > 255)
	    {
	      inst.error = "invalid immediate";
	      return;
	    }

	  inst.instruction |= value;
	}
    }

  end_of_line (str);
}

static void
thumb_load_store (str, load_store, size)
     char *str;
     int load_store;
     int size;
{
  int Rd, Rb, Ro = FAIL;

  while (*str == ' ')
    str++;

  if ((Rd = thumb_reg (&str, THUMB_REG_LO)) == FAIL
      || skip_past_comma (&str) == FAIL)
    {
      if (! inst.error)
	inst.error = bad_args;
      return;
    }

  if (*str == '[')
    {
      str++;
      if ((Rb = thumb_reg (&str, THUMB_REG_ANY)) == FAIL)
	return;

      if (skip_past_comma (&str) != FAIL)
	{
	  if (*str == '#')
	    {
	      str++;
	      if (my_get_expression (&inst.reloc.exp, &str))
		return;
	    }
	  else if ((Ro = thumb_reg (&str, THUMB_REG_LO)) == FAIL)
	    return;
	}
      else
	{
	  inst.reloc.exp.X_op = O_constant;
	  inst.reloc.exp.X_add_number = 0;
	}

      if (*str != ']')
	{
	  inst.error = "expected ']'";
	  return;
	}
      str++;
    }
  else if (*str == '=')
    {
      abort ();
    }
  else
    {
      if (my_get_expression (&inst.reloc.exp, &str))
	return;

      inst.instruction = T_OPCODE_LDR_PC | (Rd << 8);
      inst.reloc.pc_rel = 1;
      inst.reloc.exp.X_add_number -= 4; /* Pipeline offset */
      inst.reloc.type = BFD_RELOC_ARM_THUMB_OFFSET;
      end_of_line (str);
      return;
    }

  if (Rb == REG_PC || Rb == REG_SP)
    {
      if (size != THUMB_WORD)
	{
	  inst.error = "byte or halfword not valid for base register";
	  return;
	}
      else if (Rb == REG_PC && load_store != THUMB_LOAD)
	{
	  inst.error = "R15 based store not allowed";
	  return;
	}
      else if (Ro != FAIL)
	{
	  inst.error = "Invalid base register for register offset";
	  return;
	}

      if (Rb == REG_PC)
	inst.instruction = T_OPCODE_LDR_PC;
      else if (load_store == THUMB_LOAD)
	inst.instruction = T_OPCODE_LDR_SP;
      else
	inst.instruction = T_OPCODE_STR_SP;

      inst.instruction |= Rd << 8;
      if (inst.reloc.exp.X_op == O_constant)
	{
	  unsigned offset = inst.reloc.exp.X_add_number;

	  if (offset & ~0x3fc)
	    {
	      inst.error = "invalid offset";
	      return;
	    }

	  inst.instruction |= offset >> 2;
	}
      else
	inst.reloc.type = BFD_RELOC_ARM_THUMB_OFFSET;
    }
  else if (Rb > 7)
    {
      inst.error = "invalid base register in load/store";
      return;
    }
  else if (Ro == FAIL)
    {
      /* Immediate offset */
      if (size == THUMB_WORD)
	inst.instruction = (load_store == THUMB_LOAD
			    ? T_OPCODE_LDR_IW : T_OPCODE_STR_IW);
      else if (size == THUMB_HALFWORD)
	inst.instruction = (load_store == THUMB_LOAD
			    ? T_OPCODE_LDR_IH : T_OPCODE_STR_IH);
      else
	inst.instruction = (load_store == THUMB_LOAD
			    ? T_OPCODE_LDR_IB : T_OPCODE_STR_IB);

      inst.instruction |= Rd | (Rb << 3);

      if (inst.reloc.exp.X_op == O_constant)
	{
	  unsigned offset = inst.reloc.exp.X_add_number;
	  
	  if (offset & ~(0x1f << size))
	    {
	      inst.error = "Invalid offset";
	      return;
	    }
	  inst.instruction |= offset << 6;
	}
      else
	inst.reloc.type = BFD_RELOC_ARM_THUMB_OFFSET;
    }
  else
    {
      /* Register offset */
      if (size == THUMB_WORD)
	inst.instruction = (load_store == THUMB_LOAD
			    ? T_OPCODE_LDR_RW : T_OPCODE_STR_RW);
      else if (size == THUMB_HALFWORD)
	inst.instruction = (load_store == THUMB_LOAD
			    ? T_OPCODE_LDR_RH : T_OPCODE_STR_RH);
      else
	inst.instruction = (load_store == THUMB_LOAD
			    ? T_OPCODE_LDR_RB : T_OPCODE_STR_RB);

      inst.instruction |= Rd | (Rb << 3) | (Ro << 6);
    }

  end_of_line (str);
}

/* Handle the Format 4 instructions that do not have equivalents in other 
   formats.  That is, ADC, AND, EOR, SBC, ROR, TST, NEG, CMN, ORR, MUL,
   BIC and MVN.  */
static void
do_t_arit (str)
     char *str;
{
  int Rd, Rs, Rn;

  while (*str == ' ')
    str++;

  if ((Rd = thumb_reg (&str, THUMB_REG_LO)) == FAIL)
    return;

  if (skip_past_comma (&str) == FAIL
      || (Rs = thumb_reg (&str, THUMB_REG_LO)) == FAIL)
    {
      if (! inst.error)
	inst.error = bad_args;
      return;
    }

  if (skip_past_comma (&str) != FAIL)
    {
      /* Three operand format not allowed for TST, CMN, NEG and MVN.
	 (It isn't allowed for CMP either, but that isn't handled by this
	 function.)  */
      if (inst.instruction == T_OPCODE_TST
	  || inst.instruction == T_OPCODE_CMN
	  || inst.instruction == T_OPCODE_NEG
 	  || inst.instruction == T_OPCODE_MVN)
	{
	  inst.error = bad_args;
	  return;
	}

      if ((Rn = thumb_reg (&str, THUMB_REG_LO)) == FAIL)
	return;

      if (Rs != Rd)
	{
	  inst.error = "dest and source1 one must be the same register";
	  return;
	}
      Rs = Rn;
    }

  if (inst.instruction == T_OPCODE_MUL
      && Rs == Rd)
    as_tsktsk ("Rs and Rd must be different in MUL");

  inst.instruction |= Rd | (Rs << 3);
  end_of_line (str);
}

static void
do_t_add (str)
     char *str;
{
  thumb_add_sub (str, 0);
}

static void
do_t_asr (str)
     char *str;
{
  thumb_shift (str, THUMB_ASR);
}

static void
do_t_branch (str)
     char *str;
{
  if (my_get_expression (&inst.reloc.exp, &str))
    return;
  inst.reloc.type = BFD_RELOC_ARM_PCREL_BRANCH;
  inst.reloc.pc_rel = 1;
  end_of_line (str);
}

static void
do_t_bx (str)
     char *str;
{
  int reg;

  while (*str == ' ')
    str++;

  if ((reg = thumb_reg (&str, THUMB_REG_ANY)) == FAIL)
    return;

  /* This sets THUMB_H2 from the top bit of reg.  */
  inst.instruction |= reg << 3;

  /* ??? FIXME: Should add a hacky reloc here if reg is REG_PC.  The reloc
     should cause the alignment to be checked once it is known.  This is
     because BX PC only works if the instruction is word aligned.  */

  end_of_line (str);
}

static void
do_t_compare (str)
     char *str;
{
  thumb_mov_compare (str, THUMB_COMPARE);
}

static void
do_t_ldmstm (str)
     char *str;
{
  int Rb;
  long range;

  while (*str == ' ')
    str++;

  if ((Rb = thumb_reg (&str, THUMB_REG_LO)) == FAIL)
    return;

  if (*str != '!')
    as_warn ("Inserted missing '!': load/store multiple always writes back base register");
  else
    str++;

  if (skip_past_comma (&str) == FAIL
      || (range = reg_list (&str)) == FAIL)
    {
      if (! inst.error)
	inst.error = bad_args;
      return;
    }

  if (inst.reloc.type != BFD_RELOC_NONE)
    {
      /* This really doesn't seem worth it. */
      inst.reloc.type = BFD_RELOC_NONE;
      inst.error = "Expression too complex";
      return;
    }

  if (range & ~0xff)
    {
      inst.error = "only lo-regs valid in load/store multiple";
      return;
    }

  inst.instruction |= (Rb << 8) | range;
  end_of_line (str);
}

static void
do_t_ldr (str)
     char *str;
{
  thumb_load_store (str, THUMB_LOAD, THUMB_WORD);
}

static void
do_t_ldrb (str)
     char *str;
{
  thumb_load_store (str, THUMB_LOAD, THUMB_BYTE);
}

static void
do_t_ldrh (str)
     char *str;
{
  thumb_load_store (str, THUMB_LOAD, THUMB_HALFWORD);
}

static void
do_t_lds (str)
     char *str;
{
  int Rd, Rb, Ro;

  while (*str == ' ')
    str++;

  if ((Rd = thumb_reg (&str, THUMB_REG_LO)) == FAIL
      || skip_past_comma (&str) == FAIL
      || *str++ != '['
      || (Rb = thumb_reg (&str, THUMB_REG_LO)) == FAIL
      || skip_past_comma (&str) == FAIL
      || (Ro = thumb_reg (&str, THUMB_REG_LO)) == FAIL
      || *str++ != ']')
    {
      if (! inst.error)
	inst.error = "Syntax: ldrs[b] Rd, [Rb, Ro]";
      return;
    }

  inst.instruction |= Rd | (Rb << 3) | (Ro << 6);
  end_of_line (str);
}

static void
do_t_lsl (str)
     char *str;
{
  thumb_shift (str, THUMB_LSL);
}

static void
do_t_lsr (str)
     char *str;
{
  thumb_shift (str, THUMB_LSR);
}

static void
do_t_mov (str)
     char *str;
{
  thumb_mov_compare (str, THUMB_MOVE);
}

static void
do_t_push_pop (str)
     char *str;
{
  long range;

  while (*str == ' ')
    str++;

  if ((range = reg_list (&str)) == FAIL)
    {
      if (! inst.error)
	inst.error = bad_args;
      return;
    }

  if (inst.reloc.type != BFD_RELOC_NONE)
    {
      /* This really doesn't seem worth it. */
      inst.reloc.type = BFD_RELOC_NONE;
      inst.error = "Expression too complex";
      return;
    }

  if (range & ~0xff)
    {
      if ((inst.instruction == T_OPCODE_PUSH
	   && (range & ~0xff) == 1 << REG_LR)
	  || (inst.instruction == T_OPCODE_POP
	      && (range & ~0xff) == 1 << REG_PC))
	{
	  inst.instruction |= THUMB_PP_PC_LR;
	  range &= 0xff;
	}
      else
	{
	  inst.error = "invalid register list to push/pop instruction";
	  return;
	}
    }

  inst.instruction |= range;
  end_of_line (str);
}

static void
do_t_str (str)
     char *str;
{
  thumb_load_store (str, THUMB_STORE, THUMB_WORD);
}

static void
do_t_strb (str)
     char *str;
{
  thumb_load_store (str, THUMB_STORE, THUMB_BYTE);
}

static void
do_t_strh (str)
     char *str;
{
  thumb_load_store (str, THUMB_STORE, THUMB_HALFWORD);
}

static void
do_t_sub (str)
     char *str;
{
  thumb_add_sub (str, 1);
}

static void
do_t_swi (str)
     char *str;
{
  while (*str == ' ')
    str++;

  if (my_get_expression (&inst.reloc.exp, &str))
    return;

  inst.reloc.type = BFD_RELOC_ARM_SWI;
  end_of_line (str);
  return;
}

static void
do_t_adr (str)
     char *str;
{
  /* This is a pseudo-op of the form "adr rd, label" to be converted
     into a relative address of the form "add rd, pc, #label-.-8" */
  while (*str == ' ')
    str++;

  if (reg_required_here (&str, 8) == FAIL
      || skip_past_comma (&str) == FAIL
      || my_get_expression (&inst.reloc.exp, &str))
    {
      if (!inst.error)
	inst.error = bad_args;
      return;
    }

  inst.reloc.type = BFD_RELOC_ARM_THUMB_ADD;
  inst.reloc.exp.X_add_number -= 8; /* PC relative adjust */
  inst.reloc.pc_rel = 1;
  inst.instruction |= REG_PC; /* Rd is already placed into the instruction */
  end_of_line (str);
}

static void
insert_reg (entry)
     int entry;
{
  int len = strlen (reg_table[entry].name) + 2;
  char *buf = (char *) xmalloc (len);
  char *buf2 = (char *) xmalloc (len);
  int i = 0;

#ifdef REGISTER_PREFIX
  buf[i++] = REGISTER_PREFIX;
#endif

  strcpy (buf + i, reg_table[entry].name);

  for (i = 0; buf[i]; i++)
    buf2[i] = islower (buf[i]) ? toupper (buf[i]) : buf[i];

  buf2[i] = '\0';

  hash_insert (arm_reg_hsh, buf, (PTR) &reg_table[entry]);
  hash_insert (arm_reg_hsh, buf2, (PTR) &reg_table[entry]);
}

static void
insert_reg_alias (str, regnum)
     char *str;
     int regnum;
{
  struct reg_entry *new =
    (struct reg_entry *)xmalloc (sizeof (struct reg_entry));
  char *name = xmalloc (strlen (str) + 1);
  strcpy (name, str);

  new->name = name;
  new->number = regnum;

  hash_insert (arm_reg_hsh, name, (PTR) new);
}

static void
set_constant_flonums ()
{
  int i;

  for (i = 0; i < NUM_FLOAT_VALS; i++)
    if (atof_ieee ((char *)fp_const[i], 'x', fp_values[i]) == NULL)
      abort ();
}

void
md_begin ()
{
  int i;

  if ((arm_ops_hsh = hash_new ()) == NULL
      || (arm_tops_hsh = hash_new ()) == NULL
      || (arm_cond_hsh = hash_new ()) == NULL
      || (arm_shift_hsh = hash_new ()) == NULL
      || (arm_reg_hsh = hash_new ()) == NULL
      || (arm_psr_hsh = hash_new ()) == NULL)
    as_fatal ("Virtual memory exhausted");
    
  for (i = 0; i < sizeof (insns) / sizeof (struct asm_opcode); i++)
    hash_insert (arm_ops_hsh, insns[i].template, (PTR) (insns + i));
  for (i = 0; i < sizeof (tinsns) / sizeof (struct thumb_opcode); i++)
    hash_insert (arm_tops_hsh, tinsns[i].template, (PTR) (tinsns + i));
  for (i = 0; i < sizeof (conds) / sizeof (struct asm_cond); i++)
    hash_insert (arm_cond_hsh, conds[i].template, (PTR) (conds + i));
  for (i = 0; i < sizeof (shift) / sizeof (struct asm_shift); i++)
    hash_insert (arm_shift_hsh, shift[i].template, (PTR) (shift + i));
  for (i = 0; i < sizeof (psrs) / sizeof (struct asm_psr); i++)
    hash_insert (arm_psr_hsh, psrs[i].template, (PTR) (psrs + i));

  for (i = 0; reg_table[i].name; i++)
    insert_reg (i);

  set_constant_flonums ();
}

/* Turn an integer of n bytes (in val) into a stream of bytes appropriate
   for use in the a.out file, and stores them in the array pointed to by buf.
   This knows about the endian-ness of the target machine and does
   THE RIGHT THING, whatever it is.  Possible values for n are 1 (byte)
   2 (short) and 4 (long)  Floating numbers are put out as a series of
   LITTLENUMS (shorts, here at least)
   */
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

static valueT 
md_chars_to_number (buf, n)
     char *buf;
     int n;
{
  valueT result = 0;
  unsigned char *where = (unsigned char *) buf;

  if (target_big_endian)
    {
      while (n--)
	{
	  result <<= 8;
	  result |= (*where++ & 255);
	}
    }
  else
    {
      while (n--)
	{
	  result <<= 8;
	  result |= (where[n] & 255);
	}
    }

  return result;
}

/* Turn a string in input_line_pointer into a floating point constant
   of type TYPE, and store the appropriate bytes in *litP.  The number
   of LITTLENUMS emitted is stored in *sizeP .  An error message is
   returned, or NULL on OK.

   Note that fp constants aren't represent in the normal way on the ARM.
   In big endian mode, things are as expected.  However, in little endian
   mode fp constants are big-endian word-wise, and little-endian byte-wise
   within the words.  For example, (double) 1.1 in big endian mode is
   the byte sequence 3f f1 99 99 99 99 99 9a, and in little endian mode is
   the byte sequence 99 99 f1 3f 9a 99 99 99.

   ??? The format of 12 byte floats is uncertain according to gcc's arm.h.  */

char *
md_atof (type, litP, sizeP)
     char type;
     char *litP;
     int *sizeP;
{
  int prec;
  LITTLENUM_TYPE words[MAX_LITTLENUMS];
  char *t;
  int i;

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
  *sizeP = prec * 2;

  if (target_big_endian)
    {
      for (i = 0; i < prec; i++)
	{
	  md_number_to_chars (litP, (valueT) words[i], 2);
	  litP += 2;
	}
    }
  else
    {
      /* For a 4 byte float the order of elements in `words' is 1 0.  For an
	 8 byte float the order is 1 0 3 2.  */
      for (i = 0; i < prec; i += 2)
	{
	  md_number_to_chars (litP, (valueT) words[i + 1], 2);
	  md_number_to_chars (litP + 2, (valueT) words[i], 2);
	  litP += 4;
	}
    }

  return 0;
}

/* We have already put the pipeline compensation in the instruction */

long
md_pcrel_from (fixP)
     fixS *fixP;
{
  if (fixP->fx_addsy && S_GET_SEGMENT (fixP->fx_addsy) == undefined_section
      && fixP->fx_subsy == NULL)
    return 0;	/* HACK */

  return fixP->fx_where + fixP->fx_frag->fr_address;
}

/* Round up a section size to the appropriate boundary. */
valueT
md_section_align (segment, size)
     segT segment;
     valueT size;
{
  /* Round all sects to multiple of 4 */
  return (size + 3) & ~3;
}

/* We have no need to default values of symbols.  */

/* ARGSUSED */
symbolS *
md_undefined_symbol (name)
     char *name;
{
  return 0;
}

/* arm_reg_parse () := if it looks like a register, return its token and 
   advance the pointer. */

static int
arm_reg_parse (ccp)
     register char **ccp;
{
  char *start = *ccp;
  char c;
  char *p;
  struct reg_entry *reg;

#ifdef REGISTER_PREFIX
  if (*start != REGISTER_PREFIX)
    return FAIL;
  p = start + 1;
#else
  p = start;
#ifdef OPTIONAL_REGISTER_PREFIX
  if (*p == OPTIONAL_REGISTER_PREFIX)
    p++, start++;
#endif
#endif
  if (!isalpha (*p) || !is_name_beginner (*p))
    return FAIL;

  c = *p++;
  while (isalpha (c) || isdigit (c) || c == '_')
    c = *p++;

  *--p = 0;
  reg = (struct reg_entry *) hash_find (arm_reg_hsh, start);
  *p = c;
  
  if (reg)
    {
      *ccp = p;
      return reg->number;
    }

  return FAIL;
}

static int
arm_psr_parse (ccp)
     register char **ccp;
{
  char *start = *ccp;
  char c, *p;
  CONST struct asm_psr *psr;

  p = start;
  c = *p++;
  while (isalpha (c) || c == '_')
    c = *p++;

  *--p = 0;  
  psr = (CONST struct asm_psr *) hash_find (arm_psr_hsh, start);
  *p = c;

  if (psr)
    {
      *ccp = p;
      return psr->number;
    }

  return FAIL;
}

int
md_apply_fix3 (fixP, val, seg)
     fixS *fixP;
     valueT *val;
     segT seg;
{
  offsetT value = *val;
  offsetT newval, temp;
  int sign;
  char *buf = fixP->fx_where + fixP->fx_frag->fr_literal;
  arm_fix_data *arm_data = (arm_fix_data *) fixP->tc_fix_data;

  assert (fixP->fx_r_type < BFD_RELOC_UNUSED);

  /* Note whether this will delete the relocation.  */
  if (fixP->fx_addsy == 0 && !fixP->fx_pcrel)
    fixP->fx_done = 1;

  /* If this symbol is in a different section then we need to leave it for
     the linker to deal with.  Unfortunately, md_pcrel_from can't tell,
     so we have to undo it's effects here.  */
  if (fixP->fx_pcrel)
    {
      if (S_IS_DEFINED (fixP->fx_addsy)
	  && S_GET_SEGMENT (fixP->fx_addsy) != seg)
	value += md_pcrel_from (fixP);
    }

  fixP->fx_addnumber = value;	/* Remember value for emit_reloc */

  switch (fixP->fx_r_type)
    {
    case BFD_RELOC_ARM_IMMEDIATE:
      newval = validate_immediate (value);
      temp = md_chars_to_number (buf, INSN_SIZE);

      /* If the instruction will fail, see if we can fix things up by
	 changing the opcode.  */
      if (newval == FAIL
	  && (newval = negate_data_op (&temp, value)) == FAIL)
	{
	  as_bad_where (fixP->fx_file, fixP->fx_line,
			"invalid constant after fixup\n");
	  break;
	}

      newval |= (temp & 0xfffff000);
      md_number_to_chars (buf, newval, INSN_SIZE);
      break;

     case BFD_RELOC_ARM_OFFSET_IMM:
      sign = value >= 0;
      if ((value = validate_offset_imm (value, 0)) == FAIL)
        {
          as_bad ("bad immediate value for offset (%d)", val);
          break;
        }
      if (value < 0)
	value = -value;

      newval = md_chars_to_number (buf, INSN_SIZE);
      newval &= 0xff7ff000;
      newval |= value | (sign ? INDEX_UP : 0);
      md_number_to_chars (buf, newval, INSN_SIZE);
      break;

     case BFD_RELOC_ARM_OFFSET_IMM8:
     case BFD_RELOC_ARM_HWLITERAL:
      sign = value >= 0;
      if ((value = validate_offset_imm (value, 1)) == FAIL)
        {
          if (fixP->fx_r_type == BFD_RELOC_ARM_HWLITERAL)
	    as_bad_where (fixP->fx_file, fixP->fx_line, 
			"invalid literal constant: pool needs to be closer\n");
          else
            as_bad ("bad immediate value for offset (%d)", value);
          break;
        }

      if (value < 0)
	value = -value;

      newval = md_chars_to_number (buf, INSN_SIZE);
      newval &= 0xff7ff0f0;
      newval |= ((value >> 4) << 8) | value & 0xf | (sign ? INDEX_UP : 0);
      md_number_to_chars (buf, newval, INSN_SIZE);
      break;

    case BFD_RELOC_ARM_LITERAL:
      sign = value >= 0;
      if (value < 0)
	value = -value;

      if ((value = validate_offset_imm (value, 0)) == FAIL)
	{
	  as_bad_where (fixP->fx_file, fixP->fx_line, 
			"invalid literal constant: pool needs to be closer\n");
	  break;
	}

      newval = md_chars_to_number (buf, INSN_SIZE);
      newval &= 0xff7ff000;
      newval |= value | (sign ? INDEX_UP : 0);
      md_number_to_chars (buf, newval, INSN_SIZE);
      break;

    case BFD_RELOC_ARM_SHIFT_IMM:
      newval = md_chars_to_number (buf, INSN_SIZE);
      if (((unsigned long) value) > 32
	  || (value == 32 
	      && (((newval & 0x60) == 0) || (newval & 0x60) == 0x60)))
	{
	  as_bad_where (fixP->fx_file, fixP->fx_line,
			"shift expression is too large");
	  break;
	}

      if (value == 0)
	newval &= ~0x60;	/* Shifts of zero must be done as lsl */
      else if (value == 32)
	value = 0;
      newval &= 0xfffff07f;
      newval |= (value & 0x1f) << 7;
      md_number_to_chars (buf, newval , INSN_SIZE);
      break;

    case BFD_RELOC_ARM_SWI:
      if (arm_data->thumb_mode)
	{
	  if (((unsigned long) value) > 0xff)
	    as_bad_where (fixP->fx_file, fixP->fx_line,
			  "Invalid swi expression");
	  newval = md_chars_to_number (buf, THUMB_SIZE) & 0xff00;
	  newval |= value;
	  md_number_to_chars (buf, newval, THUMB_SIZE);
	}
      else
	{
	  if (((unsigned long) value) > 0x00ffffff)
	    as_bad_where (fixP->fx_file, fixP->fx_line, 
			  "Invalid swi expression");
	  newval = md_chars_to_number (buf, INSN_SIZE) & 0xff000000;
	  newval |= value;
	  md_number_to_chars (buf, newval , INSN_SIZE);
	}
      break;

    case BFD_RELOC_ARM_MULTI:
      if (((unsigned long) value) > 0xffff)
	as_bad_where (fixP->fx_file, fixP->fx_line,
		      "Invalid expression in load/store multiple");
      newval = value | md_chars_to_number (buf, INSN_SIZE);
      md_number_to_chars (buf, newval, INSN_SIZE);
      break;

    case BFD_RELOC_ARM_PCREL_BRANCH:
      if (arm_data->thumb_mode)
	{
	  unsigned long newval2;
	  newval = md_chars_to_number (buf, THUMB_SIZE);
	  if (fixP->fx_size == 4)
	    {
	      unsigned long diff;

	      newval2 = md_chars_to_number (buf, THUMB_SIZE);
	      diff = ((newval & 0x7ff) << 12) | ((newval2 & 0x7ff) << 1);
	      if (diff & 0x400000)
		diff |= ~0x3fffff;
	      value += diff;
	      if ((value & 0x400000) && ((value & ~0x3fffff) != ~0x3fffff))
		as_bad_where (fixP->fx_file, fixP->fx_line,
			      "Branch with link out of range");

	      newval = (newval & 0xf800) | ((value & 0x7fffff) >> 12);
	      newval2 = (newval2 & 0xf800) | ((value & 0xfff) >> 1);
	      md_number_to_chars (buf, newval, THUMB_SIZE);
	      md_number_to_chars (buf, newval2, THUMB_SIZE);
	    }
	  else
	    {
	      if (newval == T_OPCODE_BRANCH)
		{
		  unsigned long diff = (newval & 0x7ff) << 1;
		  if (diff & 0x800)
		    diff |= ~0x7ff;

		  value += diff;
		  if ((value & 0x800) && ((value & ~0x7ff) != ~0x7ff))
		    as_bad_where (fixP->fx_file, fixP->fx_line,
				  "Branch out of range");
		  newval = (newval & 0xf800) | ((value & 0xfff) >> 1);
		}
	      else
		{
		  unsigned long diff = (newval & 0xff) << 1;
		  if (diff & 0x100)
		    diff |= ~0xff;

		  value += diff;
		  if ((value & 0x100) && ((value & ~0xff) != ~0xff))
		    as_bad_where (fixP->fx_file, fixP->fx_line,
				  "Branch out of range");
		  newval = (newval & 0xff00) | ((value & 0x1ff) >> 1);
		}
	      md_number_to_chars (buf, newval, THUMB_SIZE);
	    }
	}
      else
	{
	  value = (value >> 2) & 0x00ffffff;
	  newval = md_chars_to_number (buf, INSN_SIZE);
	  value = (value + (newval & 0x00ffffff)) & 0x00ffffff;
	  newval = value | (newval & 0xff000000);
	  md_number_to_chars (buf, newval, INSN_SIZE);
	}
      break;

    case BFD_RELOC_8:
      if (fixP->fx_done || fixP->fx_pcrel)
	md_number_to_chars (buf, value, 1);
      break;

    case BFD_RELOC_16:
      if (fixP->fx_done || fixP->fx_pcrel)
	md_number_to_chars (buf, value, 2);
      break;

    case BFD_RELOC_RVA:
    case BFD_RELOC_32:
      if (fixP->fx_done || fixP->fx_pcrel)
	md_number_to_chars (buf, value, 4);
      break;

    case BFD_RELOC_ARM_CP_OFF_IMM:
      sign = value >= 0;
      if (value < -1023 || value > 1023 || (value & 3))
	as_bad_where (fixP->fx_file, fixP->fx_line,
		      "Illegal value for co-processor offset");
      if (value < 0)
	value = -value;
      newval = md_chars_to_number (buf, INSN_SIZE) & 0xff7fff00;
      newval |= (value >> 2) | (sign ?  INDEX_UP : 0);
      md_number_to_chars (buf, newval , INSN_SIZE);
      break;

    case BFD_RELOC_ARM_THUMB_OFFSET:
      newval = md_chars_to_number (buf, THUMB_SIZE);
      /* Exactly what ranges, and where the offset is inserted depends on
	 the type of instruction, we can establish this from the top 4 bits */
      switch (newval >> 12)
	{
	case 4: /* PC load */
	  /* PC loads are somewhat odd, bit 2 of the PC is forced to zero
	     for these loads, so we may need to round up the offset if the
	     instruction is not word aligned since the final address must
	     be.   */

	  if ((fixP->fx_frag->fr_address + fixP->fx_where + value) & 3)
	    as_bad_where (fixP->fx_file, fixP->fx_line,
			  "Invalid offset, target not word aligned");

	  if ((value + 2) & ~0x3fe)
	    as_bad_where (fixP->fx_file, fixP->fx_line,
			  "Invalid offset");
	   /* Round up, since pc will be rounded down.  */
	  newval |= (value + 2) >> 2;
	  break;

	case 9: /* SP load/store */
	  if (value & ~0x3fc)
	    as_bad_where (fixP->fx_file, fixP->fx_line,
			  "Invalid offset");
	  newval |= value >> 2;
	  break;

	case 6: /* Word load/store */
	  if (value & ~0x7c)
	    as_bad_where (fixP->fx_file, fixP->fx_line,
			  "Invalid offset");
	  newval |= value << 4; /* 6 - 2 */
	  break;

	case 7: /* Byte load/store */
	  if (value & ~0x1f)
	    as_bad_where (fixP->fx_file, fixP->fx_line,
			  "Invalid offset");
	  newval |= value << 6;
	  break;

	case 8: /* Halfword load/store */
	  if (value & ~0x3e)
	    as_bad_where (fixP->fx_file, fixP->fx_line,
			  "Invalid offset");
	  newval |= value << 5; /* 6 - 1 */
	  break;

	default:
	  abort ();
	}
      md_number_to_chars (buf, newval, THUMB_SIZE);
      break;

    case BFD_RELOC_ARM_THUMB_ADD:
      /* This is a complicated relocation, since we use it for all of
         the following immediate relocations:
            3bit ADD/SUB
            8bit ADD/SUB
            9bit ADD/SUB SP word-aligned
           10bit ADD PC/SP word-aligned

         The type of instruction being processed is encoded in the
         instruction field:
           0x8000  SUB
           0x00F0  Rd
           0x000F  Rs
      */
      newval = md_chars_to_number (buf, THUMB_SIZE);
      {
        int rd = (newval >> 4) & 0xf;
        int rs = newval & 0xf;
        int subtract = newval & 0x8000;

        if (rd == REG_SP)
          {
            if (value & ~0x1fc)
              as_bad_where (fixP->fx_file, fixP->fx_line,
                            "Invalid immediate for stack address calculation");
            newval = subtract ? T_OPCODE_SUB_ST : T_OPCODE_ADD_ST;
            newval |= value >> 2;
          }
        else if (rs == REG_PC || rs == REG_SP)
          {
            if (subtract ||
                value & ~0x3fc)
              as_bad_where (fixP->fx_file, fixP->fx_line,
                            "Invalid immediate for address calculation (value = 0x%08X)", value);
            newval = (rs == REG_PC ? T_OPCODE_ADD_PC : T_OPCODE_ADD_SP);
            newval |= value >> 2;
          }
        else if (rs == rd)
          {
            if (value & ~0xff)
              as_bad_where (fixP->fx_file, fixP->fx_line,
                            "Invalid 8bit immediate");
            newval = subtract ? T_OPCODE_SUB_I8 : T_OPCODE_ADD_I8;
            newval |= (rd << 8) | value;
          }
        else
          {
            if (value & ~0x7)
              as_bad_where (fixP->fx_file, fixP->fx_line,
                            "Invalid 3bit immediate");
            newval = subtract ? T_OPCODE_SUB_I3 : T_OPCODE_ADD_I3;
            newval |= rd | (rs << 3) | (value << 6);
          }
      }
      md_number_to_chars (buf, newval , THUMB_SIZE);
      break;

    case BFD_RELOC_ARM_THUMB_IMM:
      newval = md_chars_to_number (buf, THUMB_SIZE);
      switch (newval >> 11)
        {
        case 0x04: /* 8bit immediate MOV */
        case 0x05: /* 8bit immediate CMP */
          if (value < 0 || value > 255)
            as_bad_where (fixP->fx_file, fixP->fx_line,
                          "Invalid immediate: %d is too large", value);
          newval |= value;
          break;

        default:
          abort ();
        }
      md_number_to_chars (buf, newval , THUMB_SIZE);
      break;

    case BFD_RELOC_ARM_THUMB_SHIFT:
      /* 5bit shift value (0..31) */
      if (value < 0 || value > 31)
	as_bad_where (fixP->fx_file, fixP->fx_line,
		      "Illegal Thumb shift value: %d", value);
      newval = md_chars_to_number (buf, THUMB_SIZE) & 0xf03f;
      newval |= value << 6;
      md_number_to_chars (buf, newval , THUMB_SIZE);
      break;

    case BFD_RELOC_NONE:
    default:
      as_bad_where (fixP->fx_file, fixP->fx_line,
		    "Bad relocation fixup type (%d)\n", fixP->fx_r_type);
    }

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

  /* @@ Why fx_addnumber sometimes and fx_offset other times?  */
  if (fixp->fx_pcrel == 0)
    reloc->addend = fixp->fx_offset;
  else
    reloc->addend = fixp->fx_offset = reloc->address;

  switch (fixp->fx_r_type)
    {
    case BFD_RELOC_8:
      if (fixp->fx_pcrel)
	{
	  code = BFD_RELOC_8_PCREL;
	  break;
	}

    case BFD_RELOC_16:
      if (fixp->fx_pcrel)
	{
	  code = BFD_RELOC_16_PCREL;
	  break;
	}

    case BFD_RELOC_32:
      if (fixp->fx_pcrel)
	{
	  code = BFD_RELOC_32_PCREL;
	  break;
	}

    case BFD_RELOC_ARM_PCREL_BRANCH:
    case BFD_RELOC_RVA:      
      code = fixp->fx_r_type;
      break;

    case BFD_RELOC_ARM_LITERAL:
    case BFD_RELOC_ARM_HWLITERAL:
      /* If this is called then the a literal has been referenced across
	 a section boundry - possibly due to an implicit dump */
      as_bad ("Literal referenced across section boundry (Implicit dump?)");
      return NULL;

    case BFD_RELOC_ARM_IMMEDIATE:
      as_bad ("Internal_relocation (type %d) not fixed up (IMMEDIATE)"
	      , fixp->fx_r_type);
      return NULL;

    case BFD_RELOC_ARM_OFFSET_IMM:
      as_bad ("Internal_relocation (type %d) not fixed up (OFFSET_IMM)"
	      , fixp->fx_r_type);
      return NULL;

    case BFD_RELOC_ARM_OFFSET_IMM8:
      as_bad ("Internal_relocation (type %d) not fixed up (OFFSET_IMM8)"
	      , fixp->fx_r_type);
      return NULL;

    case BFD_RELOC_ARM_SHIFT_IMM:
      as_bad ("Internal_relocation (type %d) not fixed up (SHIFT_IMM)"
	      , fixp->fx_r_type);
      return NULL;

    case BFD_RELOC_ARM_SWI:
      as_bad ("Internal_relocation (type %d) not fixed up (SWI)"
	      , fixp->fx_r_type);
      return NULL;

    case BFD_RELOC_ARM_MULTI:
      as_bad ("Internal_relocation (type %d) not fixed up (MULTI)"
	      , fixp->fx_r_type);
      return NULL;

    case BFD_RELOC_ARM_CP_OFF_IMM:
      as_bad ("Internal_relocation (type %d) not fixed up (CP_OFF_IMM)"
	      , fixp->fx_r_type);
      return NULL;

    case BFD_RELOC_ARM_THUMB_OFFSET:
      as_bad ("Internal_relocation (type %d) not fixed up (THUMB_OFFSET)"
	      , fixp->fx_r_type);
      return NULL;

    default:
      abort ();
    }

  reloc->howto = bfd_reloc_type_lookup (stdoutput, code);
  assert (reloc->howto != 0);

  return reloc;
}

CONST int md_short_jump_size = 4;
CONST int md_long_jump_size = 4;

/* These should never be called on the arm */
void
md_create_long_jump (ptr, from_addr, to_addr, frag, to_symbol)
     char *ptr;
     addressT from_addr, to_addr;
     fragS *frag;
     symbolS *to_symbol;
{
  as_fatal ("md_create_long_jump\n");
}

void
md_create_short_jump (ptr, from_addr, to_addr, frag, to_symbol)
     char *ptr;
     addressT from_addr, to_addr;
     fragS *frag;
     symbolS *to_symbol;
{
  as_fatal ("md_create_short_jump\n");
}

int
md_estimate_size_before_relax (fragP, segtype)
     fragS *fragP;
     segT segtype;
{
  as_fatal ("md_estimate_size_before_relax\n");
  return (1);
}

void
output_inst (str)
     char *str;
{
  char *to = NULL;
    
  if (inst.error)
    {
      as_bad ("%s -- statement `%s'\n", inst.error, str);
      return;
    }

  to = frag_more (inst.size);
  if (thumb_mode && (inst.size > 2))
    {
      md_number_to_chars (to, inst.instruction >> 16, 2);
      to += 2;
      inst.size = 2;
    }

  md_number_to_chars (to, inst.instruction, inst.size);

  if (inst.reloc.type != BFD_RELOC_NONE)
    fix_new_arm (frag_now, to - frag_now->fr_literal,
		 inst.size, &inst.reloc.exp, inst.reloc.pc_rel,
		 inst.reloc.type);

  return;
}

void
md_assemble (str)
     char *str;
{
  char c;
  char *p, *q, *start;

  /* Align the instruction */
  /* this may not be the right thing to do but ... */
  /* arm_align (2, 0); */
  listing_prev_line (); /* Defined in listing.h */

  /* Align the previous label if needed */
  if (last_label_seen != NULL)
    {
      last_label_seen->sy_frag = frag_now;
      S_SET_VALUE (last_label_seen, (valueT) frag_now_fix ());
      S_SET_SEGMENT (last_label_seen, now_seg);
    }

  memset (&inst, '\0', sizeof (inst));
  inst.reloc.type = BFD_RELOC_NONE;

  if (*str == ' ')
    str++;			/* Skip leading white space */
    
  /* scan up to the end of the op-code, which must end in white space or
     end of string */
  for (start = p = str; *p != '\0'; p++)
    if (*p == ' ')
      break;
    
  if (p == str)
    {
      as_bad ("No operator -- statement `%s'\n", str);
      return;
    }

  if (thumb_mode)
    {
      CONST struct thumb_opcode *opcode;

      c = *p;
      *p = '\0';
      opcode = (CONST struct thumb_opcode *) hash_find (arm_tops_hsh, str);
      *p = c;
      if (opcode)
	{
	  inst.instruction = opcode->value;
	  inst.size = opcode->size;
	  (*opcode->parms)(p);
	  output_inst (start);
	  return;
	}
    }
  else
    {
      CONST struct asm_opcode *opcode;

      inst.size = INSN_SIZE;
      /* p now points to the end of the opcode, probably white space, but we
	 have to break the opcode up in case it contains condionals and flags;
	 keep trying with progressively smaller basic instructions until one
	 matches, or we run out of opcode. */
      q = (p - str > LONGEST_INST) ? str + LONGEST_INST : p;
      for (; q != str; q--)
	{
	  c = *q;
	  *q = '\0';
	  opcode = (CONST struct asm_opcode *) hash_find (arm_ops_hsh, str);
	  *q = c;
	  if (opcode && opcode->template)
	    {
	      unsigned long flag_bits = 0;
	      char *r;

	      /* Check that this instruction is supported for this CPU */
	      if ((opcode->variants & cpu_variant) == 0)
		goto try_shorter;

	      inst.instruction = opcode->value;
	      if (q == p)		/* Just a simple opcode */
		{
		  if (opcode->comp_suffix != 0)
		    as_bad ("Opcode `%s' must have suffix from <%s>\n", str,
			    opcode->comp_suffix);
		  else
		    {
		      inst.instruction |= COND_ALWAYS;
		      (*opcode->parms)(q, 0);
		    }
		  output_inst (start);
		  return;
		}

	      /* Now check for a conditional */
	      r = q;
	      if (p - r >= 2)
		{
		  CONST struct asm_cond *cond;
		  char d = *(r + 2);

		  *(r + 2) = '\0';
		  cond = (CONST struct asm_cond *) hash_find (arm_cond_hsh, r);
		  *(r + 2) = d;
		  if (cond)
		    {
		      if (cond->value == 0xf0000000)
			as_tsktsk (
"Warning: Use of the 'nv' conditional is deprecated\n");

		      inst.instruction |= cond->value;
		      r += 2;
		    }
		  else
		    inst.instruction |= COND_ALWAYS;
		}
	      else
		inst.instruction |= COND_ALWAYS;

	      /* if there is a compulsory suffix, it should come here, before
		 any optional flags. */
	      if (opcode->comp_suffix)
		{
		  CONST char *s = opcode->comp_suffix;

		  while (*s)
		    {
		      inst.suffix++;
		      if (*r == *s)
			break;
		      s++;
		    }

		  if (*s == '\0')
		    {
		      as_bad ("Opcode `%s' must have suffix from <%s>\n", str,
			      opcode->comp_suffix);
		      return;
		    }

		  r++;
		}

	      /* The remainder, if any should now be flags for the instruction;
		 Scan these checking each one found with the opcode.  */
	      if (r != p)
		{
		  char d;
		  CONST struct asm_flg *flag = opcode->flags;

		  if (flag)
		    {
		      int flagno;

		      d = *p;
		      *p = '\0';

		      for (flagno = 0; flag[flagno].template; flagno++)
			{
			  if (! strcmp (r, flag[flagno].template))
			    {
			      flag_bits |= flag[flagno].set_bits;
			      break;
			    }
			}

		      *p = d;
		      if (! flag[flagno].template)
			goto try_shorter;
		    }
		  else
		    goto try_shorter;
		}

	      (*opcode->parms) (p, flag_bits);
	      output_inst (start);
	      return;
	    }

	try_shorter:
	  ;
	}
    }

  /* It wasn't an instruction, but it might be a register alias of the form
     alias .req reg
     */
  q = p;
  while (*q == ' ')
    q++;

  c = *p;
  *p = '\0';
    
  if (*q && !strncmp (q, ".req ", 4))
    {
      int reg;
      if ((reg = arm_reg_parse (&str)) == FAIL)
	{
	  char *r;
      
	  q += 4;
	  while (*q == ' ')
	    q++;

	  for (r = q; *r != '\0'; r++)
	    if (*r == ' ')
	      break;

	  if (r != q)
	    {
	      int regnum;
	      char d = *r;

	      *r = '\0';
	      regnum = arm_reg_parse (&q);
	      *r = d;
	      if (regnum != FAIL)
		{
		  insert_reg_alias (str, regnum);
		  *p = c;
		  return;
		}
	    }
	}
      else
	{
	  *p = c;
	  return;
	}
    }

  *p = c;
  as_bad ("bad instruction `%s'", start);
}

/*
 * md_parse_option
 *    Invocation line includes a switch not recognized by the base assembler.
 *    See if it's a processor-specific option.  These are:
 *    Cpu variants, the arm part is optional:
 *            -m[arm]1                Currently not supported.
 *            -m[arm]2, -m[arm]250    Arm 2 and Arm 250 processor
 *            -m[arm]3                Arm 3 processor
 *            -m[arm]6,               Arm 6 processors
 *            -m[arm]7[t][[d]m]       Arm 7 processors
 *            -mall                   All (except the ARM1)
 *    FP variants:
 *            -mfpa10, -mfpa11        FPA10 and 11 co-processor instructions
 *            -mfpe-old               (No float load/store multiples)
 *            -mno-fpu                Disable all floating point instructions
 *    Run-time endian selection:
 *            -EB                     big endian cpu
 *            -EL                     little endian cpu
 */

CONST char *md_shortopts = "m:";
struct option md_longopts[] = {
#ifdef ARM_BI_ENDIAN
#define OPTION_EB (OPTION_MD_BASE + 0)
  {"EB", no_argument, NULL, OPTION_EB},
#define OPTION_EL (OPTION_MD_BASE + 1)
  {"EL", no_argument, NULL, OPTION_EL},
#endif
  {NULL, no_argument, NULL, 0}
};
size_t md_longopts_size = sizeof (md_longopts);

int
md_parse_option (c, arg)
     int c;
     char *arg;
{
  char *str = arg;

  switch (c)
    {
#ifdef ARM_BI_ENDIAN
    case OPTION_EB:
      target_big_endian = 1;
      break;
    case OPTION_EL:
      target_big_endian = 0;
      break;
#endif

    case 'm':
      switch (*str)
	{
	case 'f':
	  if (! strcmp (str, "fpa10"))
	    cpu_variant = (cpu_variant & ~FPU_ALL) | FPU_FPA10;
	  else if (! strcmp (str, "fpa11"))
	    cpu_variant = (cpu_variant & ~FPU_ALL) | FPU_FPA11;
	  else if (! strcmp (str, "fpe-old"))
	    cpu_variant = (cpu_variant & ~FPU_ALL) | FPU_CORE;
	  else
	    goto bad;
	  break;

	case 'n':
	  if (! strcmp (str, "no-fpu"))
	    cpu_variant &= ~FPU_ALL;
	  break;

        case 't':
          /* Limit assembler to generating only Thumb instructions: */
          if (! strcmp (str, "thumb"))
            {
              cpu_variant = (cpu_variant & ~ARM_ANY) | ARM_THUMB;
              cpu_variant = (cpu_variant & ~FPU_ALL) | FPU_NONE;
              thumb_mode = 1;
            }
          else
	    goto bad;
          break;

	default:
	  if (! strcmp (str, "all"))
	    {
	      cpu_variant = ARM_ALL | FPU_ALL;
	      return 1;
	    }

	  /* Strip off optional "arm" */
	  if (! strncmp (str, "arm", 3))
	    str += 3;

	  switch (*str)
	    {
	    case '1':
	      if (! strcmp (str, "1"))
		cpu_variant = (cpu_variant & ~ARM_ANY) | ARM_1;
	      else
		goto bad;
	      break;

	    case '2':
	      if (! strcmp (str, "2"))
		cpu_variant = (cpu_variant & ~ARM_ANY) | ARM_2;
	      else if (! strcmp (str, "250"))
		cpu_variant = (cpu_variant & ~ARM_ANY) | ARM_250;
	      else
		goto bad;
	      break;

	    case '3':
	      if (! strcmp (str, "3"))
		cpu_variant = (cpu_variant & ~ARM_ANY) | ARM_3;
	      else
		goto bad;
	      break;

	    case '6':
	      if (! strcmp (str, "6"))
		cpu_variant = (cpu_variant & ~ARM_ANY) | ARM_6;
	      else
		goto bad;
	      break;

	    case '7':
              str++; /* eat the '7' */
              cpu_variant = (cpu_variant & ~ARM_ANY) | ARM_7;
              for (; *str; str++)
                {
                switch (*str)
                  {
                  case 't':
                    cpu_variant |= ARM_THUMB;
                    break;

                  case 'm':
                    cpu_variant |= ARM_LONGMUL;
                    break;

                  case 'd': /* debug */
                  case 'i': /* embedded ice */
                    /* Included for completeness in ARM processor
                       naming. */
                    break;

                  default:
                    goto bad;
                  }
                }
	      break;

	    default:
	    bad:
	      as_bad ("Invalid architecture -m%s", arg);
	      return 0;
	    }
	}
      break;

    default:
      return 0;
    }

   return 1;
}

void
md_show_usage (fp)
     FILE *fp;
{
  fprintf (fp,
"-m[arm]1, -m[arm]2, -m[arm]250,\n-m[arm]3, -m[arm]6, -m[arm]7[t][[d]m]\n\
-mthumb\t\t\tselect processor architecture\n\
-mall\t\t\tallow any instruction\n\
-mfpa10, -mfpa11\tselect floating point architecture\n\
-mfpe-old\t\tdon't allow floating-point multiple instructions\n\
-mno-fpu\t\tdon't allow any floating-point instructions.\n");
#ifdef ARM_BI_ENDIAN
  fprintf (fp,
"-EB\t\t\tassemble code for a big endian cpu\n\
-EL\t\t\tassemble code for a little endian cpu\n");
#endif
}

/* We need to be able to fix up arbitrary expressions in some statements.
   This is so that we can handle symbols that are an arbitrary distance from
   the pc.  The most common cases are of the form ((+/-sym -/+ . - 8) & mask),
   which returns part of an address in a form which will be valid for
   a data instruction.  We do this by pushing the expression into a symbol
   in the expr_section, and creating a fix for that.  */

static void
fix_new_arm (frag, where, size, exp, pc_rel, reloc)
     fragS *frag;
     int where;
     short int size;
     expressionS *exp;
     int pc_rel;
     int reloc;
{
  fixS *new_fix;
  arm_fix_data *arm_data;

  switch (exp->X_op)
    {
    case O_constant:
    case O_symbol:
    case O_add:
    case O_subtract:
      new_fix = fix_new_exp (frag, where, size, exp, pc_rel, reloc);
      break;

    default:
      {
	const char *fake;
	symbolS *symbolP;
	
	/* FIXME: This should be something which decode_local_label_name
	   will handle.  */
	fake = FAKE_LABEL_NAME;

	/* Putting constant symbols in absolute_section rather than
	   expr_section is convenient for the old a.out code, for which
	   S_GET_SEGMENT does not always retrieve the value put in by
	   S_SET_SEGMENT.  */
	symbolP = symbol_new (fake, expr_section, 0, &zero_address_frag);
	symbolP->sy_value = *exp;
	new_fix = fix_new (frag, where, size, symbolP, 0, pc_rel, reloc);
      }
      break;
    }

  /* Mark whether the fix is to a THUMB instruction, or an ARM instruction */
  arm_data = (arm_fix_data *) obstack_alloc (&notes, sizeof (arm_fix_data));
  new_fix->tc_fix_data = (PTR) arm_data;
  arm_data->thumb_mode = thumb_mode;

  return;
}

/* A good place to do this, although this was probably not intended
 * for this kind of use.  We need to dump the literal pool before
 * references are made to a null symbol pointer.  */
void
arm_after_pass_hook (ignore)
     asection *ignore;
{
  if (current_poolP != NULL)
    {
      subseg_set (text_section, 0); /* Put it at the end of text section */
      s_ltorg (0);
      listing_prev_line ();
    }
}

void
arm_start_line_hook ()
{
  last_label_seen = NULL;
}

void
arm_frob_label (sym)
     symbolS *sym;
{
  last_label_seen = sym;
}

int
arm_data_in_code ()
{
  if (thumb_mode && ! strncmp (input_line_pointer + 1, "data:", 5))
    {
      *input_line_pointer = '/';
      input_line_pointer += 5;
      *input_line_pointer = 0;
      return 1;
    }
  return 0;
}

char *
arm_canonicalize_symbol_name (name)
     char *name;
{
  int len;

  if (thumb_mode && (len = strlen (name)) > 5
      && ! strcmp (name + len - 5, "/data"))
    {
      *(name + len - 5) = 0;
    }

  return name;
}
