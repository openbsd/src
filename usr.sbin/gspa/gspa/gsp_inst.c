/*
 * TMS34010 GSP assembler - Instruction encoding
 *
 * Copyright (c) 1993 Paul Mackerras.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Paul Mackerras.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "gsp_ass.h"
#include "gsp_code.h"
#include <string.h>

struct inst {
	char	*opname;
	u_int16_t opcode;
	u_char	class;		/* instruction class + flags */
	u_char	optypes[4];	/* permissible operand classes */
};

/* Values for flags in class field */
#define NOIMM16	0x80		/* can't have 16-bit immediate */
#define K32	0x40		/* values 1..32 for K-type constant */
#define IMMCOM	0x20		/* immediate value is complemented */
#define IMMNEG	0x80		/* immediate value is negated */
#define NODSJS	0x80		/* can't use 5-bit branch offset */
#define DSHORT	0x40		/* must use 5-bit offset */

#define CLASS	0x1F

/* Values for class */
#define NOP	0		/* no operands */
#define ONEREG	1		/* reg */
#define TWOREG	2		/* reg, reg */
#define DYADIC	3		/* immediate or reg, reg */
#define ADD	(DYADIC|K32)
#define SUB	(DYADIC|IMMCOM|K32)
#define CMP	(DYADIC|IMMCOM)
#define AND	(DYADIC|NOIMM16|IMMCOM)
#define OR	(DYADIC|NOIMM16)
#define IMMREG	4		/* immediate, reg */
#define IMMREGC	(IMMREG|IMMCOM)
#define LIMREG	(IMMREG|NOIMM16)
#define LIMREGC	(LIMREG|IMMCOM)
#define KREG	5		/* short immediate, reg */
#define K32REG	(KREG|K32)
#define SRA	(KREG|IMMNEG)
#define BTST	(KREG|IMMCOM)
#define CALL	6		/* reg or address */
#define JUMP	7
#define CLR	8		/* reg appears twice in encoding */
#define DSJ	9
#define DSJEQ	(DSJ|NODSJS)
#define DSJS	(DSJ|DSHORT)
#define EXGF	10
#define SETF	11
#define FILL	12
#define LINE	13
#define PIXBLT	14
#define PIXT	15
#define MMFM	16
#define MOVB	17
#define MOVE	18
#define MOVEK	(MOVE|K32)
#define RETS	19
#define PSEUDO	20

/* Composite operand classes */
#define EXREG	(REG|EXPR)
#define EAREG	(REG|EA)
#define EXAREG	(REG|EXPR|EA)
#define OPTOPRN	0x80		/* signals optional operand */
#define SPEC	(0x10|EXPR)		/* field or length specifier */
#define OPTREG	(OPTOPRN|REG)
#define OPTEXPR	(OPTOPRN|EXPR)
#define OPTSPEC	(OPTOPRN|SPEC)
#define OPTXREG	(OPTOPRN|EXREG)

#define MIN(a, b)	((a) < (b)? (a): (b))

/*
 * N.B. This list must be sorted in order of opname.
 */
struct inst instructions[] = {
	".BLKB", BLKB,	PSEUDO,	{0,	0,	0,	0},
	".BLKL", BLKL,	PSEUDO,	{0,	0,	0,	0},
	".BLKW", BLKW,	PSEUDO,	{0,	0,	0,	0},
#ifdef EQU
	".EQU",	EQU,	PSEUDO,	{0,	0,	0,	0},
#endif
	".INCLUDE", INCL, PSEUDO, {0,	0,	0,	0},
	".LONG",LONG,	PSEUDO,	{0,	0,	0,	0},
	".ORG",	ORG,	PSEUDO,	{0,	0,	0,	0},
	".START",START,	PSEUDO,	{0,	0,	0,	0},
	".WORD",WORD,	PSEUDO,	{0,	0,	0,	0},
	"ABS",	0x0380,	ONEREG,	{REG,	0,	0,	0},
	"ADD",	0x4000,	ADD,	{EXREG,	REG,	OPTSPEC,0},
	"ADDC",	0x4200,	TWOREG,	{REG,	REG,	0,	0},
	"ADDI",	0x0B20,	IMMREG,	{EXPR,	REG,	OPTSPEC,0},
	"ADDK",	0x1000,	K32REG,	{EXPR,	REG,	0,	0},
	"ADDXY",0xE000,	TWOREG,	{REG,	REG,	0,	0},
	"AND",	0x5000,	AND,	{EXREG,	REG,	0,	0},
	"ANDI",	0x0B80,	LIMREGC,{EXPR,	REG,	0,	0},
	"ANDN",	0x5200,	OR,	{EXREG,	REG,	0,	0},
	"ANDNI",0x0B80,	LIMREG,	{EXPR,	REG,	0,	0},
	"BTST",	0x1C00,	BTST,	{EXREG,	REG,	0,	0},
	"CALL",	0x0920,	CALL,	{EXREG,	0,	0,	0},
	"CALLA",0x0D5F,	CALL,	{EXPR,	0,	0,	0},
	"CALLR",0x0D3F,	CALL,	{EXPR,	0,	0,	0},
	"CLR",	0x5600,	CLR,	{REG,	0,	0,	0},
	"CLRC",	0x0320,	NOP,	{0,	0,	0,	0},
	"CMP",	0x4800,	CMP,	{EXREG,	REG,	OPTSPEC,0},
	"CMPI",	0x0B60,	IMMREGC,{EXPR,	REG,	OPTSPEC,0},
	"CMPXY",0xE400,	TWOREG,	{REG,	REG,	0,	0},
	"CPW",	0xE600,	TWOREG,	{REG,	REG,	0,	0},
	"CVXYL",0xE800,	TWOREG,	{REG,	REG,	0,	0},
	"DEC",	0x1420,	ONEREG,	{REG,	0,	0,	0},
	"DINT",	0x0360,	NOP,	{0,	0,	0,	0},
	"DIVS",	0x5800,	TWOREG,	{REG,	REG,	0,	0},
	"DIVU",	0x5A00,	TWOREG,	{REG,	REG,	0,	0},
	"DRAV",	0xF600,	TWOREG,	{REG,	REG,	0,	0},
	"DSJ",	0x0D80,	DSJ,	{REG,	EXPR,	0,	0},
	"DSJEQ",0x0DA0,	DSJEQ,	{REG,	EXPR,	0,	0},
	"DSJNE",0x0DC0,	DSJEQ,	{REG,	EXPR,	0,	0},
	"DSJS",	0x3800,	DSJS,	{REG,	EXPR,	0,	0},
	"EINT",	0x0D60,	NOP,	{0,	0,	0,	0},
	"EMU",	0x0100,	NOP,	{0,	0,	0,	0},
	"EXGF",	0xD500,	EXGF,	{REG,	OPTSPEC,0,	0},
	"EXGPC",0x0120,	ONEREG,	{REG,	0,	0,	0},
	"FILL",	0x0FC0,	FILL,	{SPEC,	0,	0,	0},
	"GETPC",0x0140,	ONEREG,	{REG,	0,	0,	0},
	"GETST",0x0180,	ONEREG,	{REG,	0,	0,	0},
	"INC",	0x1020,	ONEREG,	{REG,	0,	0,	0},
	"JAB",	0xC880,	JUMP,	{EXPR,	0,	0,	0},
	"JAC",	0xC880,	JUMP,	{EXPR,	0,	0,	0},
	"JAEQ",	0xCA80,	JUMP,	{EXPR,	0,	0,	0},
	"JAGE",	0xC580,	JUMP,	{EXPR,	0,	0,	0},
	"JAGT",	0xC780,	JUMP,	{EXPR,	0,	0,	0},
	"JAHI",	0xC380,	JUMP,	{EXPR,	0,	0,	0},
	"JAHS",	0xC980,	JUMP,	{EXPR,	0,	0,	0},
	"JALE",	0xC680,	JUMP,	{EXPR,	0,	0,	0},
	"JALO",	0xC880,	JUMP,	{EXPR,	0,	0,	0},
	"JALS",	0xC280,	JUMP,	{EXPR,	0,	0,	0},
	"JALT",	0xC480,	JUMP,	{EXPR,	0,	0,	0},
	"JAN",	0xCE80,	JUMP,	{EXPR,	0,	0,	0},
	"JANB",	0xC980,	JUMP,	{EXPR,	0,	0,	0},
	"JANC",	0xC980,	JUMP,	{EXPR,	0,	0,	0},
	"JANE",	0xCB80,	JUMP,	{EXPR,	0,	0,	0},
	"JANN",	0xCF80,	JUMP,	{EXPR,	0,	0,	0},
	"JANV",	0xCD80,	JUMP,	{EXPR,	0,	0,	0},
	"JANZ",	0xCB80,	JUMP,	{EXPR,	0,	0,	0},
	"JAP",	0xC180,	JUMP,	{EXPR,	0,	0,	0},
	"JAUC",	0xC080,	JUMP,	{EXPR,	0,	0,	0},
	"JAV",	0xCC80,	JUMP,	{EXPR,	0,	0,	0},
	"JAZ",	0xCA80,	JUMP,	{EXPR,	0,	0,	0},
	"JRB",	0xC800,	JUMP,	{EXPR,	OPTSPEC,0,	0},
	"JRC",	0xC800,	JUMP,	{EXPR,	OPTSPEC,0,	0},
	"JREQ",	0xCA00,	JUMP,	{EXPR,	OPTSPEC,0,	0},
	"JRGE",	0xC500,	JUMP,	{EXPR,	OPTSPEC,0,	0},
	"JRGT",	0xC700,	JUMP,	{EXPR,	OPTSPEC,0,	0},
	"JRHI",	0xC300,	JUMP,	{EXPR,	OPTSPEC,0,	0},
	"JRHS",	0xC900,	JUMP,	{EXPR,	OPTSPEC,0,	0},
	"JRLE",	0xC600,	JUMP,	{EXPR,	OPTSPEC,0,	0},
	"JRLO",	0xC800,	JUMP,	{EXPR,	OPTSPEC,0,	0},
	"JRLS",	0xC200,	JUMP,	{EXPR,	OPTSPEC,0,	0},
	"JRLT",	0xC400,	JUMP,	{EXPR,	OPTSPEC,0,	0},
	"JRN",	0xCE00,	JUMP,	{EXPR,	OPTSPEC,0,	0},
	"JRNB",	0xC900,	JUMP,	{EXPR,	OPTSPEC,0,	0},
	"JRNC",	0xC900,	JUMP,	{EXPR,	OPTSPEC,0,	0},
	"JRNE",	0xCB00,	JUMP,	{EXPR,	OPTSPEC,0,	0},
	"JRNN",	0xCF00,	JUMP,	{EXPR,	OPTSPEC,0,	0},
	"JRNV",	0xCD00,	JUMP,	{EXPR,	OPTSPEC,0,	0},
	"JRNZ",	0xCB00,	JUMP,	{EXPR,	OPTSPEC,0,	0},
	"JRP",	0xC100,	JUMP,	{EXPR,	OPTSPEC,0,	0},
	"JRUC",	0xC000,	JUMP,	{EXPR,	OPTSPEC,0,	0},
	"JRV",	0xCC00,	JUMP,	{EXPR,	OPTSPEC,0,	0},
	"JRZ",	0xCA00,	JUMP,	{EXPR,	OPTSPEC,0,	0},
	"JUMP",	0x0160,	JUMP,	{EXREG,	OPTSPEC,0,	0},
	"LINE",	0xDF1A,	LINE,	{SPEC,	0,	0,	0},
	"LMO",	0x6A00,	TWOREG,	{REG,	REG,	0,	0},
	"MMFM",	0x09A0,	MMFM,	{REG,	OPTXREG,OPTREG,	OPTREG},
	"MMTM",	0x0980,	MMFM,	{REG,	OPTXREG,OPTREG,	OPTREG},
	"MODS",	0x6C00,	TWOREG,	{REG,	REG,	0,	0},
	"MODU",	0x6E00,	TWOREG,	{REG,	REG,	0,	0},
	"MOVB",	0,	MOVB,	{EAREG,	EAREG,	0,	0},
	"MOVE",	0x4C00,	MOVEK,	{EXAREG,EAREG,	OPTSPEC,0},
	"MOVI",	0x09E0,	IMMREG,	{EXPR,	REG,	OPTSPEC,0},
	"MOVK",	0x1800,	K32REG,	{EXPR,	REG,	0,	0},
	"MOVX",	0xEC00,	TWOREG,	{REG,	REG,	0,	0},
	"MOVY",	0xEE00,	TWOREG,	{REG,	REG,	0,	0},
	"MPYS",	0x5C00,	TWOREG,	{REG,	REG,	0,	0},
	"MPYU",	0x5E00,	TWOREG,	{REG,	REG,	0,	0},
	"NEG",	0x03A0,	ONEREG,	{REG,	0,	0,	0},
	"NEGB",	0x03C0,	ONEREG,	{REG,	0,	0,	0},
	"NOP",	0x0300,	NOP,	{0,	0,	0,	0},
	"NOT",	0x03E0,	ONEREG,	{REG,	0,	0,	0},
	"OR",	0x5400,	OR,	{EXREG,	REG,	0,	0},
	"ORI",	0x0BA0,	LIMREG,	{EXPR,	REG,	0,	0},
	"PIXBLT",0x0F00,PIXBLT,	{SPEC,	SPEC,	0,	0},
	"PIXT",	0,	PIXT,	{EAREG,	EAREG,	0,	0},
	"POPST",0x01C0,	NOP,	{0,	0,	0,	0},
	"PUSHST",0x01E0,NOP,	{0,	0,	0,	0},
	"PUTST",0x01A0,	ONEREG,	{REG,	0,	0,	0},
	"RETI",	0x0940,	NOP,	{0,	0,	0,	0},
	"RETS",	0x0960,	RETS,	{OPTEXPR,0,	0,	0},
	"REV",	0x0020,	ONEREG,	{REG,	0,	0,	0},
	"RL",	0x3000,	KREG,	{EXREG,	REG,	0,	0},
	"SETC",	0x0DE0,	NOP,	{0,	0,	0,	0},
	"SETF",	0x0540,	SETF,	{EXPR,	EXPR,	OPTSPEC,0},
	"SEXT",	0x0500,	EXGF,	{REG,	OPTSPEC,0,	0},
	"SLA",	0x2000,	KREG,	{EXREG,	REG,	0,	0},
	"SLL",	0x2400,	KREG,	{EXREG,	REG,	0,	0},
	"SRA",	0x2800,	SRA,	{EXREG,	REG,	0,	0},
	"SRL",	0x2C00,	SRA,	{EXREG,	REG,	0,	0},
	"SUB",	0x4400,	SUB,	{EXREG,	REG,	OPTSPEC,0},
	"SUBB",	0x4600,	TWOREG,	{REG,	REG,	0,	0},
	"SUBI",	0x0D00,	IMMREGC,{EXPR,	REG,	OPTSPEC,0},
	"SUBK",	0x1400,	K32REG,	{EXPR,	REG,	0,	0},
	"SUBXY",0xE200,	TWOREG,	{REG,	REG,	0,	0},
	"TRAP",	0x0900,	RETS,	{EXPR,	0,	0,	0},
	"XOR",	0x5600,	OR,	{EXREG,	REG,	0,	0},
	"XORI",	0x0BC0,	LIMREG,	{EXPR,	REG,	0,	0},
	"ZEXT",	0x0520,	EXGF,	{REG,	OPTSPEC,0,	0},
	NULL
};


void do_statement(char *opcode, operand operands);
int encode_instr(struct inst *ip, operand ops,
		 int *spec, u_int16_t *iwords);

void
statement(char *opcode, operand operands)
{
	do_statement(opcode, operands);
	free_operands(operands);
}

void
do_statement(char *opcode, operand operands)
{
	register struct inst *ip;
	register int i, nop, req;
	register operand op;
	int spec[3];
	u_int16_t iwords[6];

	ucasify(opcode);
	i = 1;
	for( ip = instructions; ip->opname != NULL; ++ip )
		if( opcode[0] == ip->opname[0] ){
			i = strcmp(opcode, ip->opname);
			if( i <= 0 )
				break;
		}
	if( i != 0 ){
		perr("Unknown instruction code %s", opcode);
		return;
	}
	if( ip->class == PSEUDO ){
		pseudo(ip->opcode, operands);
		return;
	}

	/* Check correspondence of operands with instruction requirements */
	nop = 0;
	spec[0] = spec[1] = spec[2] = 0;
	for( op = operands; op != NULL; op = op->next ){
		req = ip->optypes[MIN(nop, 3)];
		if( req == 0 )
			break;
		if( (op->type & req) == 0 ){
			perr("Inappropriate type for operand %d", nop+1);
			return;
		}
		if( (req & ~OPTOPRN) == SPEC )
			/* operand is a field/type/length specifier */
			spec[nop] = specifier(op);
		++nop;
	}
	if( nop < 4 && ip->optypes[nop] != 0
	   && (ip->optypes[nop] & OPTOPRN) == 0 ){
		perr("Insufficient operands");
		return;
	}
	if( op != NULL )
		perr("Extra operands ignored");

	i = encode_instr(ip, operands, spec, iwords);

	/* Pass 1 processing */
	if( !pass2 ){
		/* for pass 1, just work out the instruction size */
/*		printf("pc = %#x, size = %d\n", pc, i);	*/
		pc += i << 4;
		return;
	}

	/* Pass 2 processing */
	if( i > 0 )
		putcode(iwords, i);
}

char *specs[] = { "B", "L", "W", "XY", NULL };

int
specifier(operand op)
{
	register char **sl;
	register expr e;
	char sp[4];

	if( op->type != EXPR )
		return '?';
	e = op->op_u.value;
	if( e->e_op == CONST ){
		if( e->e_val == 0 || e->e_val == 1 )
			return e->e_val + '0';
	} else if( e->e_op == SYM ){
		if( strlen(e->e_sym->name) > 2 )
			return '?';
		strcpy(sp, e->e_sym->name);
		ucasify(sp);
		for( sl = specs; *sl != NULL; ++sl )
			if( strcmp(*sl, sp) == 0 )
				return sp[0];
	}
	return '?';
}

int
check_spec(int spec, char *valid, char *what)
{
	register char *p;

	if( spec == 0 )
		return 0;
	p = strchr(valid, spec);
	if( p == NULL ){
		perr("Invalid %s specifier", what);
		return 0;
	}
	return p - valid;
}

u_int16_t code_to_imm[] = {
	0x0B20,		/* ADDI */
	0,
	0x0D00,		/* SUBI */
	0,
	0x0B60,		/* CMPI */
	0,
	0x09E0,		/* MOVI */
	0,
	0x0B80,		/* ANDI */
	0x0B80, 	/* ANDNI */
	0x0BA0,		/* ORI */
	0x0BC0,		/* XORI */
};

/* Opcodes for MOVE instruction */
u_int16_t move_opc[7][7] = {
/*				Source */
/*	Reg	*Reg	*Reg+	*-Reg	*Reg.XY	*Reg(n)	@addr 	  Dest */
	0x4C00,	0x8400,	0x9400,	0xA400,	0,	0xB400,	0x05A0,	/* R */
	0x8000,	0x8800,	0,	0,	0,	0,	0,	/* *R */
	0x9000,	0,	0x9800,	0,	0,	0xD000,	0xD400,	/* *R+ */
	0xA000,	0,	0,	0xA800,	0,	0,	0,	/* *-R */
	0,	0,	0,	0,	0,	0,	0,	/* *R.XY */
	0xB000,	0,	0,	0,	0,	0xB800,	0,	/* *R(n) */
	0x0580,	0,	0,	0,	0,	0,	0x05C0,	/* @adr */
};

/* Opcodes for MOVB instruction */
u_int16_t movb_opc[7][7] = {
/*				Source */
/*	Reg	*Reg	*Reg+	*-Reg	*Reg.XY	*Reg(n)	@addr 	  Dest */
	0,	0x8E00,	0,	0,	0,	0xAE00,	0x07E0,	/* R */
	0x8C00,	0x9C00,	0,	0,	0,	0,	0,	/* *R */
	0,	0,	0,	0,	0,	0,	0,	/* *R+ */
	0,	0,	0,	0,	0,	0,	0,	/* *-R */
	0,	0,	0,	0,	0,	0,	0,	/* *R.XY */
	0xAC00,	0,	0,	0,	0,	0xBC00,	0,	/* *R(n) */
	0x05E0,	0,	0,	0,	0,	0,	0x0340,	/* @adr */
};

/* Opcodes for PIXT instruction */
u_int16_t pixt_opc[7][7] = {
/*				Source */
/*	Reg	*Reg	*Reg+	*-Reg	*Reg.XY	*Reg(n)	@addr 	  Dest */
	0,	0xFA00,	0,	0,	0xF200,	0,	0,	/* R */
	0xF800,	0xFC00,	0,	0,	0,	0,	0,	/* *R */
	0,	0,	0,	0,	0,	0,	0,	/* *R+ */
	0,	0,	0,	0,	0,	0,	0,	/* *-R */
	0xF000,	0,	0,	0,	0xF400,	0,	0,	/* *R.XY */
	0,	0,	0,	0,	0,	0,	0,	/* *R(n) */
	0,	0,	0,	0,	0,	0,	0,	/* @adr */
};

#define USES_REG(op)	((op)->type == REG \
			 || (op)->type == EA && (op)->mode != M_ABSOLUTE)
#define USES_EXPR(op)	((op)->type == EXPR \
			 || (op)->type == EA && (op)->mode >= M_INDEX)

int
encode_instr(struct inst *ip, operand ops, int *spec, u_int16_t *iwords)
{
	register int rs, rd;
	int opc, nw, class, flags, ms, md, off;
	int mask, file, bit, i;
	register operand op0, op1;
	unsigned int line[2];
	int32_t val[2];

	opc = ip->opcode;
	nw = 1;
	op0 = ops;
	if( op0 != NULL ){
		if( spec[0] == 0 && USES_EXPR(op0) )
			eval_expr(op0->op_u.value, &val[0], &line[0]);
		op1 = ops->next;
		if( op1 != NULL && spec[1] == 0 && USES_EXPR(op1) )
			eval_expr(op1->op_u.value, &val[1], &line[1]);
	} else
		op1 = NULL;
	class = ip->class & CLASS;
	flags = ip->class & ~CLASS;
	if( class == MOVE && op1->type == REG ){
		if (op0->type == REG ){
			class = DYADIC;
			if( (op0->reg_no & op1->reg_no & REGFILE) == 0 ){
				opc += 0x0200;
				op1->reg_no ^= A0^B0;
			}
		} else if ( op0->type == EXPR )
			class = DYADIC;
	}
	if( class == DYADIC ){
		/* turn it into TWOREG, IMMREG or KREG */
		if( op0->type == REG ){
			class = TWOREG;
		} else if( (flags & K32) != 0 && line[0] <= lineno
			  && spec[2] == 0
			  && 0 < val[0] && val[0] <= 32 ){
			/* use 5-bit immediate */
			class = KREG;
			opc -= 0x3000;
			if( opc == 0x1C00 )
				opc = 0x1800;
			flags &= ~IMMCOM;
		} else {
			class = IMMREG;
			opc = code_to_imm[(opc - 0x4000) >> 9];
		}
		if( (class == TWOREG || class == KREG)
		   && spec[2] != 0 && op1->next->next == NULL )
			perr("Extra operands ignored");
	} else if( class == KREG ){
		if( op0->type == REG ){
			class = TWOREG;
			if( opc < 0x2000 )
				opc = 0x4A00;	/* BTST */
			else
				opc = (opc >> 1) + 0x5000;
		}
	}

	if( op0 != NULL )
		rs = op0->reg_no;
	if( op1 != NULL ){
		rd = op1->reg_no;
		if( USES_REG(op0) && USES_REG(op1) ){
			if( (rs & rd & REGFILE) == 0 )
				perr("Registers must be in the same register file");
			/* force SP to the file of the other operand */
			if( rs == SP )
				rs |= rd;
			if( rd == SP )
				rd |= rs;
		}
	}

	switch( class ){
	case NOP:			/* no operands */
		break;
	case ONEREG:			/* reg */
		opc |= rs & 0x1F;
		break;
	case TWOREG:			/* reg, reg */
		opc |= ((rs & 0x0F) << 5) | (rd & 0x1F);
		break;
	case IMMREG:			/* immediate, reg */
		opc |= rd & 0x1F;
		if( (flags & IMMCOM) != 0 )
			val[0] = ~ val[0];
		i = check_spec(spec[2], " WL", "length");
		if( i == 1
		   || i == 0 && (flags & NOIMM16) == 0 && line[0] <= lineno
		      && (int16_t)val[0] == val[0] ){
			if( (int16_t) val[0] != val[0] )
				perr("Value truncated to 16 bits");
			opc -= 0x20;
			if( opc == 0x0CE0 )	/* SUBI,W */
				opc = 0x0BE0;
			nw = 2;
		} else {
			iwords[2] = (val[0] >> 16);
			nw = 3;
		}
		iwords[1] = val[0];
		break;
	case KREG:			/* short immediate, reg */
		opc |= rd & 0x1F;
		if( val[0] < 0 || (flags & K32) == 0 && val[0] > 31
		   || (flags & K32) != 0 && val[0] <= 0 || val[0] > 32 )
			perr("5-bit constant out of range");
		rs = val[0];
		if( (flags & IMMCOM) != 0 )
			rs = ~rs;
		else if( (flags & IMMNEG) != 0 )
			rs = -rs;
		opc |= (rs & 0x1F) << 5;
		break;
	case CALL:			/* reg or address */
		if( op0->type == REG ){
			opc |= rs & 0x1F;
			break;
		}
		off = (int)(val[0] - pc - 0x20) >> 4;
		if( opc == 0x0920 ){		/* CALL */
			if( line[0] <= lineno && (int16_t) off == off )
				opc = 0x0D3F;	/* CALLR */
			else
				opc = 0x0D5F;	/* CALLA */
		}
		if( opc == 0x0D3F ){	/* CALLR */
			if( (int16_t) off != off )
				perr("Displacement too large");
			iwords[1] = off;
			nw = 2;
		} else {		/* CALLA */
			iwords[1] = val[0];
			iwords[2] = val[0] >> 16;
			nw = 3;
		}
		break;
	case JUMP:
		if( op0->type == REG ){
			opc |= rs & 0x1F;
			break;
		}
		off = (int)(val[0] - pc - 0x10) >> 4;
		if( (opc & 0x80) != 0 )		/* JAcc */
			i = 2;
		else
			i = check_spec(spec[1], " WL", "length");
		if( opc == 0x0160 ){	/* JUMP */
			opc = 0xC000;	/* JRUC */
			if( i == 0 )
				i = 1;	/* ,W is the default for JUMP */
		}
		switch( i ){
		case 2:		/* JAcc */
			iwords[1] = val[0];
			iwords[2] = val[0] >> 16;
			opc |= 0x80;
			nw = 3;
			break;
		case 1:
			--off;
			if( (int16_t) off != off )
				perr("Displacement too large (word)");
			iwords[1] = off;
			nw = 2;
			break;
		default:
			if( off == 0 || off < -127 || off > 127 )
				perr("Short displacement too large or 0");
			opc |= off & 0xFF;
		}
		break;
	case CLR:			/* reg appears twice in encoding */
		opc |= (rs & 0x1F) | ((rs & 0x0F) << 5);
		break;
	case DSJ:
		off = (int)(val[1] - pc - 0x10) >> 4;
		if( flags == 0 ){	/* DSJ */
			if( off != 0 && off >= -31 && off <= 31 ){
				flags = DSHORT;
				opc = 0x3800;	/* DSJS */
			}
		}
		if( flags == DSHORT ){
			if( off == 0 || off < -31 || off > 31 )
				perr("DSJS displacement too large");
			if( off > 0 )
				opc |= (off & 0x1F) << 5;
			else
				opc |= 0x400 | ((-off & 0x1F) << 5);
		} else {
			--off;
			if( (int16_t) off != off )
				perr("Displacement too large (word)");
			iwords[1] = off;
			nw = 2;
		}
		opc |= rs & 0x1F;
		break;
	case EXGF:
		opc |= rs & 0x1F;
		opc |= check_spec(spec[1], "01", "field") << 9;
		break;
	case SETF:
		rs = val[0];
		rd = val[1];
		if( rs <= 0 || rs > 32 )
			perr("Field size must be 1..32");
		if( rd != 0 && rd != 1 )
			perr("Field extension must be 0 or 1");
		opc |= (rs & 0x1F) | ((rd & 1) << 5);
		opc |= check_spec(spec[2], "01", "field") << 9;
		break;
	case FILL:
		opc |= check_spec(spec[0], "LX", "array type") << 5;
		break;
	case LINE:
		opc |= check_spec(spec[0], "01", "algorithm") << 7;
		break;
	case PIXBLT:
		rs = check_spec(spec[0], "LXB", "source array type");
		rd = check_spec(spec[1], "LX", "destination array type");
		opc |= (rs << 6) | (rd << 5);
		break;
	case MMFM:
		opc |= rs & 0xF;
		file = rs & REGFILE;
		if( op1 == NULL )
			mask = 0xFFFF;
		else if( op1->type == REG ){
			file &= rd;
			mask = 0;
			for( ; op1 != NULL; op1 = op1->next ){
				rd = op1->reg_no;
				bit = 1 << (~rd & 0xF);
				if( file != 0 && (file &= rd) == 0 )
					perr("Registers must all be in the same file");
				if( file != 0 && (mask & bit) != 0 )
					perr("Register name repeated");
				mask |= bit;
			}
		} else {
			if( val[1] < 0 || val[1] > 0xFFFFL )
				perr("Mask value out of range");
			mask = val[1];
			if( op1->next != NULL )
				perr("Extra operands ignored");
		}
		if( (file & A0 & REGFILE) == 0 )
			opc |= 0x10;
		if( (opc & 0x20) != 0 ){
			/* mask reversed for MMFM */
			rs = 0;
			for( bit = 16; bit != 0; --bit ){
				rs <<= 1;
				rs |= mask & 1;
				mask >>= 1;
			}
			mask = rs;
		}
		iwords[1] = mask;
		nw = 2;
		break;
	case PIXT:
	case MOVB:
	case MOVE:
		ms = op0->type == REG? M_REG: op0->mode;
		md = op1->type == REG? M_REG: op1->mode;
		opc = class == MOVE? move_opc[md][ms]:
		      class == MOVB? movb_opc[md][ms]: pixt_opc[md][ms];
		if( opc == 0 ){
			perr("Illegal combination of addressing modes");
			nw = 0;
			break;
		}
		if( ms == M_INDEX ){
			if( (int16_t) val[0] != val[0] )
				perr("Source displacement too large");
			iwords[1] = val[0];
			nw = 2;
		} else if( ms == M_ABSOLUTE ){
			iwords[1] = val[0];
			iwords[2] = val[0] >> 16;
			nw = 3;
			rs = 0;
		}
		if( md == M_INDEX ){
			if( (int16_t) val[1] != val[1] )
				perr("Destination displacement too large");
			iwords[nw] = val[1];
			++nw;
		} else if( md == M_ABSOLUTE ){
			iwords[nw] = val[1];
			iwords[nw+1] = val[1] >> 16;
			nw += 2;
			rd = rs;
			rs = 0;
		}
		opc |= (rd & 0x1F) | ((rs & 0xF) << 5);
		opc |= check_spec(spec[2], "01", "field") << 9;
		break;
	case RETS:
		if( op0 == NULL )
			val[0] = 0;
		else if( val[0] < 0 || val[0] > 31 )
			perr("%s out of range",
			     (opc > 0x900? "Pop count": "Trap number"));
		opc |= val[0] & 0x1F;
		break;
	default:
		perr("BUG: unknown instruction class %d\n", class);
	}
	iwords[0] = opc;
	return nw;
}

