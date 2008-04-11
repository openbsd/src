/*      $OpenBSD: local2.c,v 1.4 2008/04/11 20:45:52 stefan Exp $    */
/*
 * Copyright (c) 2007 Gregory McGarry (g.mcgarry@ieee.org).
 * Copyright (c) 2003 Anders Magnusson (ragge@ludd.luth.se).
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
 * 3. The name of the author may not be used to endorse or promote products
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

#include <assert.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

#include "pass2.h"

extern void defalign(int);

#define	exname(x) x

char *rnames[] = {
	"r0", "r1", "r2", "r3","r4","r5", "r6", "r7",
	"r8", "r9", "r10", "fp", "ip", "sp", "lr", "pc",
	"r0r1", "r1r2", "r2r3", "r3r4", "r4r5", "r5r6",
	"r6r7", "r7r8", "r8r9", "r9r10",
	"f0", "f1", "f2", "f3", "f4", "f5", "f6", "f7",
};

/*
 * Handling of integer constants.  We have 8 bits + an even
 * number of rotates available as a simple immediate.
 * If a constant isn't trivially representable, use an ldr
 * and a subsequent sequence of orr operations.
 */

static int
trepresent(const unsigned int val)
{
	int i;
#define rotate_left(v, n) (v << n | v >> (32 - n))

	for (i = 0; i < 32; i += 2)
		if (rotate_left(val, i) <= 0xff)
			return 1;
	return 0;
}

/*
 * Return values are:
 * 0 - output constant as is (should be covered by trepresent() above)
 * 1 - 4 generate 1-4 instructions as needed.
 */
static int
encode_constant(int constant, int *values)
{
	int tmp = constant;
	int i = 0;
	int first_bit, value;

	while (tmp) {
		first_bit = ffs(tmp);
		first_bit -= 1; /* ffs indexes from 1, not 0 */
		first_bit &= ~1; /* must use even bit offsets */

		value = tmp & (0xff << first_bit);
		values[i++] = value;
		tmp &= ~value;
	}
	return i;
}

#if 0
static void
load_constant(NODE *p)
{
	int v = p->n_lval & 0xffffffff;
	int reg = DECRA(p->n_reg, 1);

	load_constant_into_reg(reg, v);
}
#endif

static void
load_constant_into_reg(int reg, int v)
{
	if (trepresent(v))
		printf("\tmov %s,#%d\n", rnames[reg], v);
	else if (trepresent(-v))
		printf("\tmvn %s,#%d\n", rnames[reg], -v);
	else {
		int vals[4], nc, i;

		nc = encode_constant(v, vals);
		for (i = 0; i < nc; i++) {
			if (i == 0) {
				printf("\tmov %s,#%d" COM "load constant %d\n",
				    rnames[reg], vals[i], v);
			} else {
				printf("\torr %s,%s,#%d\n",	
				    rnames[reg], rnames[reg], vals[i]);
			}
		}
	}
}

static TWORD ftype;

/*
 * calculate stack size and offsets
 */
static int
offcalc(struct interpass_prolog *ipp)
{
	int addto;

#ifdef PCC_DEBUG
	if (x2debug)
		printf("offcalc: p2maxautooff=%d\n", p2maxautooff);
#endif

	addto = p2maxautooff;

#if 0
	addto += 7;
	addto &= ~7;
#endif

#ifdef PCC_DEBUG
	if (x2debug)
		printf("offcalc: addto=%d\n", addto);
#endif

	addto -= AUTOINIT / SZCHAR;

	return addto;
}

void
prologue(struct interpass_prolog *ipp)
{
	int addto;
	int vals[4], nc, i;

#ifdef PCC_DEBUG
	if (x2debug)
		printf("prologue: type=%d, lineno=%d, name=%s, vis=%d, ipptype=%d, regs=0x%x, autos=%d, tmpnum=%d, lblnum=%d\n",
			ipp->ipp_ip.type,
			ipp->ipp_ip.lineno,
			ipp->ipp_name,
			ipp->ipp_vis,
			ipp->ipp_type,
			ipp->ipp_regs,
			ipp->ipp_autos,
			ipp->ip_tmpnum,
			ipp->ip_lblnum);
#endif

	ftype = ipp->ipp_type;

#if 0
	printf("\t.align 2\n");
	if (ipp->ipp_vis)
		printf("\t.global %s\n", exname(ipp->ipp_name));
	printf("\t.type %s,%%function\n", exname(ipp->ipp_name));
#endif
	printf("%s:\n", exname(ipp->ipp_name));

	/*
	 * We here know what register to save and how much to 
	 * add to the stack.
	 */
	addto = offcalc(ipp);

	printf("\tsub %s,%s,#%d\n", rnames[SP], rnames[SP], 16);
	printf("\tmov %s,%s\n", rnames[IP], rnames[SP]);
	printf("\tstmfd %s!,{%s,%s,%s,%s}\n", rnames[SP], rnames[FP],
	    rnames[IP], rnames[LR], rnames[PC]);
	printf("\tsub %s,%s,#4\n", rnames[FP], rnames[IP]);

	if (addto == 0)
		return;

	if (trepresent(addto)) {
		printf("\tsub %s,%s,#%d\n", rnames[SP], rnames[SP], addto);
	} else {
		nc = encode_constant(addto, vals);
		for (i = 0; i < nc; i++)
			printf("\tsub %s,%s,#%d\n",
			    rnames[SP], rnames[SP], vals[i]);
	}
}

void
eoftn(struct interpass_prolog *ipp)
{
	if (ipp->ipp_ip.ip_lbl == 0)
		return; /* no code needs to be generated */

	/* struct return needs special treatment */
	if (ftype == STRTY || ftype == UNIONTY) {
		assert(0);
	} else {
		printf("\tldmea %s,{%s,%s,%s}\n", rnames[FP], rnames[FP],
		    rnames[SP], rnames[PC]);
		printf("\tadd %s,%s,#%d\n", rnames[SP], rnames[SP], 16);
	}
	printf("\t.size %s,.-%s\n", exname(ipp->ipp_name),
	    exname(ipp->ipp_name));
}


/*
 * these mnemonics match the order of the preprocessor decls
 * EQ, NE, LE, LT, GE, GT, ULE, ULT, UGE, UGT
 */

static char *
ccbranches[] = {
	"beq",		/* branch if equal */
	"bne",		/* branch if not-equal */
	"ble",		/* branch if less-than-or-equal */
	"blt",		/* branch if less-than */
	"bge",		/* branch if greater-than-or-equal */
	"bgt",		/* branch if greater-than */
	/* what should these be ? */
	"bls",		/* branch if lower-than-or-same */
	"blo",		/* branch if lower-than */
	"bhs",		/* branch if higher-than-or-same */
	"bhi",		/* branch if higher-than */
};

/*
 * add/sub/...
 *
 * Param given:
 */
void
hopcode(int f, int o)
{
	char *str;

	switch (o) {
	case PLUS:
		str = "add";
		break;
	case MINUS:
		str = "sub";
		break;
	case AND:
		str = "and";
		break;
	case OR:
		str = "orr";
		break;
	case ER:
		str = "eor";
		break;
	default:
		comperr("hopcode2: %d", o);
		str = 0; /* XXX gcc */
	}
	printf("%s%c", str, f);
}

/*
 * Return type size in bytes.  Used by R2REGS, arg 2 to offset().
 */
int
tlen(NODE *p)
{
	switch(p->n_type) {
		case CHAR:
		case UCHAR:
			return(1);

		case SHORT:
		case USHORT:
			return(SZSHORT/SZCHAR);

		case DOUBLE:
			return(SZDOUBLE/SZCHAR);

		case INT:
		case UNSIGNED:
		case LONG:
		case ULONG:
			return(SZINT/SZCHAR);

		case LONGLONG:
		case ULONGLONG:
			return SZLONGLONG/SZCHAR;

		default:
			if (!ISPTR(p->n_type))
				comperr("tlen type %d not pointer");
			return SZPOINT(p->n_type)/SZCHAR;
		}
}

/*
 * Emit code to compare two longlong numbers.
 */
static void
twollcomp(NODE *p)
{
	int o = p->n_op;
	int s = getlab();
	int e = p->n_label;
	int cb1, cb2;

	if (o >= ULE)
		o -= (ULE-LE);
	switch (o) {
	case NE:
		cb1 = 0;
		cb2 = NE;
		break;
	case EQ:
		cb1 = NE;
		cb2 = 0;
		break;
	case LE:
	case LT:
		cb1 = GT;
		cb2 = LT;
		break;
	case GE:
	case GT:
		cb1 = LT;
		cb2 = GT;
		break;
	
	default:
		cb1 = cb2 = 0; /* XXX gcc */
	}
	if (p->n_op >= ULE)
		cb1 += 4, cb2 += 4;
	expand(p, 0, "\tcmp UR,UL" COM "compare 64-bit values (upper)\n");
	if (cb1) cbgen(cb1, s);
	if (cb2) cbgen(cb2, e);
	expand(p, 0, "\tcmp AR,AL" COM "(and lower)\n");
	cbgen(p->n_op, e);
	deflab(s);
}

int
fldexpand(NODE *p, int cookie, char **cp)
{
	CONSZ val;
	int shft;

        if (p->n_op == ASSIGN)
                p = p->n_left;

	if (features(FEATURE_BIGENDIAN))
		shft = SZINT - UPKFSZ(p->n_rval) - UPKFOFF(p->n_rval);
	else
		shft = UPKFOFF(p->n_rval);

        switch (**cp) {
        case 'S':
                printf("#%d", UPKFSZ(p->n_rval));
                break;
        case 'H':
                printf("#%d", shft);
                break;
        case 'M':
        case 'N':
                val = (CONSZ)1 << UPKFSZ(p->n_rval);
                --val;
                val <<= shft;
                printf("%lld", (**cp == 'M' ? val : ~val)  & 0xffffffff);
                break;
        default:
                comperr("fldexpand");
        }
        return 1;
}

#if 0
/*
 * Assign to a bitfield.
 * Clumsy at least, but what to do?
 */
static void
bfasg(NODE *p)
{
	NODE *fn = p->n_left;
	int shift = UPKFOFF(fn->n_rval);
	int fsz = UPKFSZ(fn->n_rval);
	int andval, tch = 0;

	/* get instruction size */
	switch (p->n_type) {
	case CHAR: case UCHAR: tch = 'b'; break;
	case SHORT: case USHORT: tch = 'w'; break;
	case INT: case UNSIGNED: tch = 'l'; break;
	default: comperr("bfasg");
	}

	/* put src into a temporary reg */
	fprintf(stdout, "	mov%c ", tch);
	adrput(stdout, getlr(p, 'R'));
	fprintf(stdout, ",");
	adrput(stdout, getlr(p, '1'));
	fprintf(stdout, "\n");

	/* AND away the bits from dest */
	andval = ~(((1 << fsz) - 1) << shift);
	fprintf(stdout, "	and%c $%d,", tch, andval);
	adrput(stdout, fn->n_left);
	fprintf(stdout, "\n");

	/* AND away unwanted bits from src */
	andval = ((1 << fsz) - 1);
	fprintf(stdout, "	and%c $%d,", tch, andval);
	adrput(stdout, getlr(p, '1'));
	fprintf(stdout, "\n");

	/* SHIFT left src number of bits */
	if (shift) {
		fprintf(stdout, "	sal%c $%d,", tch, shift);
		adrput(stdout, getlr(p, '1'));
		fprintf(stdout, "\n");
	}

	/* OR in src to dest */
	fprintf(stdout, "	or%c ", tch);
	adrput(stdout, getlr(p, '1'));
	fprintf(stdout, ",");
	adrput(stdout, fn->n_left);
	fprintf(stdout, "\n");
}
#endif

/*
 * Push a structure on stack as argument.
 * the scratch registers are already free here
 */
static void
stasg(NODE *p)
{
	NODE *l = p->n_left;
	int val = l->n_lval;

	load_constant_into_reg(R2, p->n_stsize);
	if (l->n_rval != R0 || l->n_lval != 0) {
		if (trepresent(val)) {
			printf("\tadd %s,%s,#%d\n",
			    rnames[R0], rnames[regno(l)], val);
		} else {
			load_constant_into_reg(R0, val);
			printf("\tadd %s,%s,%s\n", rnames[R0],
			    rnames[R0], rnames[regno(l)]);
		}
	}
	printf("\tbl %s\n", exname("memcpy"));
}

static void
shiftop(NODE *p)
{
	NODE *r = p->n_right;
	TWORD ty = p->n_type;
	char *shifttype;

	if (p->n_op == LS && r->n_op == ICON && r->n_lval < 32) {
		expand(p, INBREG, "\tmov A1,AL,lsr ");
		printf(CONFMT COM "64-bit left-shift\n", 32 - r->n_lval);
		expand(p, INBREG, "\tmov U1,UL,asl AR\n");
		expand(p, INBREG, "\torr U1,U1,A1\n");
		expand(p, INBREG, "\tmov A1,AL,asl AR\n");
	} else if (p->n_op == LS && r->n_op == ICON && r->n_lval < 64) {
		expand(p, INBREG, "\tmov A1,#0" COM "64-bit left-shift\n");
		expand(p, INBREG, "\tmov U1,AL");
		if (r->n_lval - 32 != 0)
			printf(",asl " CONFMT, r->n_lval - 32);
		printf("\n");
	} else if (p->n_op == LS && r->n_op == ICON) {
		expand(p, INBREG, "\tmov A1,#0" COM "64-bit left-shift\n");
		expand(p, INBREG, "\tmov U1,#0\n");
	} else if (p->n_op == RS && r->n_op == ICON && r->n_lval < 32) {
		expand(p, INBREG, "\tmov U1,UL,asl ");
		printf(CONFMT COM "64-bit right-shift\n", 32 - r->n_lval);
		expand(p, INBREG, "\tmov A1,AL,lsr AR\n");
		expand(p, INBREG, "\torr A1,A1,U1\n");
		if (ty == LONGLONG)
			expand(p, INBREG, "\tmov U1,UL,asr AR\n");
		else
			expand(p, INBREG, "\tmov U1,UL,lsr AR\n");
	} else if (p->n_op == RS && r->n_op == ICON && r->n_lval < 64) {
		if (ty == LONGLONG) {
			expand(p, INBREG, "\tmvn U1,#1" COM "64-bit right-shift\n");
			expand(p, INBREG, "\tmov A1,UL");
			shifttype = "asr";
		}else {
			expand(p, INBREG, "\tmov U1,#0" COM "64-bit right-shift\n");
			expand(p, INBREG, "\tmov A1,UL");
			shifttype = "lsr";
		}
		if (r->n_lval - 32 != 0)
			printf(",%s " CONFMT, shifttype, r->n_lval - 32);
		printf("\n");
	} else if (p->n_op == RS && r->n_op == ICON) {
		expand(p, INBREG, "\tmov A1,#0" COM "64-bit right-shift\n");
		expand(p, INBREG, "\tmov U1,#0\n");
	}
}

/*
 * http://gcc.gnu.org/onlinedocs/gccint/Soft-float-library-routines.html#Soft-float-library-routines
 */
static void
fpemul(NODE *p)
{
	NODE *l = p->n_left;
	char *ch = NULL;

	if (p->n_op == PLUS && p->n_type == FLOAT) ch = "addsf3";
	else if (p->n_op == PLUS && p->n_type == DOUBLE) ch = "adddf3";
	else if (p->n_op == PLUS && p->n_type == LDOUBLE) ch = "adddf3";

	else if (p->n_op == MINUS && p->n_type == FLOAT) ch = "subsf3";
	else if (p->n_op == MINUS && p->n_type == DOUBLE) ch = "subdf3";
	else if (p->n_op == MINUS && p->n_type == LDOUBLE) ch = "subdf3";

	else if (p->n_op == MUL && p->n_type == FLOAT) ch = "mulsf3";
	else if (p->n_op == MUL && p->n_type == DOUBLE) ch = "muldf3";
	else if (p->n_op == MUL && p->n_type == LDOUBLE) ch = "muldf3";

	else if (p->n_op == DIV && p->n_type == FLOAT) ch = "divsf3";
	else if (p->n_op == DIV && p->n_type == DOUBLE) ch = "divdf3";
	else if (p->n_op == DIV && p->n_type == LDOUBLE) ch = "divdf3";

	else if (p->n_op == UMINUS && p->n_type == FLOAT) ch = "negsf2";
	else if (p->n_op == UMINUS && p->n_type == DOUBLE) ch = "negdf2";
	else if (p->n_op == UMINUS && p->n_type == LDOUBLE) ch = "negdf2";

	else if (p->n_op == EQ && l->n_type == FLOAT) ch = "eqsf2";
	else if (p->n_op == EQ && l->n_type == DOUBLE) ch = "eqdf2";
	else if (p->n_op == EQ && l->n_type == LDOUBLE) ch = "eqdf2";

	else if (p->n_op == NE && l->n_type == FLOAT) ch = "nesf2";
	else if (p->n_op == NE && l->n_type == DOUBLE) ch = "nedf2";
	else if (p->n_op == NE && l->n_type == LDOUBLE) ch = "nedf2";

	else if (p->n_op == GE && l->n_type == FLOAT) ch = "gesf2";
	else if (p->n_op == GE && l->n_type == DOUBLE) ch = "gedf2";
	else if (p->n_op == GE && l->n_type == LDOUBLE) ch = "gedf2";

	else if (p->n_op == LE && l->n_type == FLOAT) ch = "lesf2";
	else if (p->n_op == LE && l->n_type == DOUBLE) ch = "ledf2";
	else if (p->n_op == LE && l->n_type == LDOUBLE) ch = "ledf2";

	else if (p->n_op == GT && l->n_type == FLOAT) ch = "gtsf2";
	else if (p->n_op == GT && l->n_type == DOUBLE) ch = "gtdf2";
	else if (p->n_op == GT && l->n_type == LDOUBLE) ch = "gtdf2";

	else if (p->n_op == LT && l->n_type == FLOAT) ch = "ltsf2";
	else if (p->n_op == LT && l->n_type == DOUBLE) ch = "ltdf2";
	else if (p->n_op == LT && l->n_type == LDOUBLE) ch = "ltdf2";

	else if (p->n_op == SCONV && p->n_type == FLOAT) {
		if (l->n_type == DOUBLE) ch = "truncdfsf2";
		else if (l->n_type == LDOUBLE) ch = "truncdfsf2";
		else if (l->n_type == ULONGLONG) ch = "floatundisf";
		else if (l->n_type == LONGLONG) ch = "floatdisf";
		else if (l->n_type == LONG) ch = "floatsisf";
		else if (l->n_type == ULONG) ch = "floatunsisf";
		else if (l->n_type == INT) ch = "floatsisf";
		else if (l->n_type == UNSIGNED) ch = "floatunsisf";
	} else if (p->n_op == SCONV && p->n_type == DOUBLE) {
		if (l->n_type == FLOAT) ch = "extendsfdf2";
		else if (l->n_type == LDOUBLE) ch = "trunctfdf2";
		else if (l->n_type == ULONGLONG) ch = "floatunsdidf";
		else if (l->n_type == LONGLONG) ch = "floatdidf";
		else if (l->n_type == LONG) ch = "floatsidf";
		else if (l->n_type == ULONG) ch = "floatunsidf";
		else if (l->n_type == INT) ch = "floatsidf";
		else if (l->n_type == UNSIGNED) ch = "floatunsidf";
	} else if (p->n_op == SCONV && p->n_type == LDOUBLE) {
		if (l->n_type == FLOAT) ch = "extendsfdf2";
		else if (l->n_type == DOUBLE) ch = "extenddftd2";
		else if (l->n_type == ULONGLONG) ch = "floatundidf";
		else if (l->n_type == LONGLONG) ch = "floatdidf";
		else if (l->n_type == LONG) ch = "floatsidf";
		else if (l->n_type == ULONG) ch = "floatunsidf";
		else if (l->n_type == INT) ch = "floatsidf";
		else if (l->n_type == UNSIGNED) ch = "floatunsidf";
	} else if (p->n_op == SCONV && p->n_type == ULONGLONG) {
		if (l->n_type == FLOAT) ch = "fixunssfdi";
		else if (l->n_type == DOUBLE) ch = "fixunsdfdi";
		else if (l->n_type == LDOUBLE) ch = "fixunsdfdi";
	} else if (p->n_op == SCONV && p->n_type == LONGLONG) {
		if (l->n_type == FLOAT) ch = "fixsfdi";
		else if (l->n_type == DOUBLE) ch = "fixdfdi";
		else if (l->n_type == LDOUBLE) ch = "fixdfdi";
	} else if (p->n_op == SCONV && p->n_type == LONG) {
		if (l->n_type == FLOAT) ch = "fixsfsi";
		else if (l->n_type == DOUBLE) ch = "fixdfsi";
		else if (l->n_type == LDOUBLE) ch = "fixdfsi";
	} else if (p->n_op == SCONV && p->n_type == ULONG) {
		if (l->n_type == FLOAT) ch = "fixunssfdi";
		else if (l->n_type == DOUBLE) ch = "fixunsdfdi";
		else if (l->n_type == LDOUBLE) ch = "fixunsdfdi";
	} else if (p->n_op == SCONV && p->n_type == INT) {
		if (l->n_type == FLOAT) ch = "fixsfsi";
		else if (l->n_type == DOUBLE) ch = "fixdfsi";
		else if (l->n_type == LDOUBLE) ch = "fixdfsi";
	} else if (p->n_op == SCONV && p->n_type == UNSIGNED) {
		if (l->n_type == FLOAT) ch = "fixunssfsi";
		else if (l->n_type == DOUBLE) ch = "fixunsdfsi";
		else if (l->n_type == LDOUBLE) ch = "fixunsdfsi";
	}

	if (ch == NULL) comperr("ZF: op=0x%x (%d)\n", p->n_op, p->n_op);

	printf("\tbl __%s" COM "softfloat operation\n", exname(ch));

	if (p->n_op >= EQ && p->n_op <= GT)
		printf("\tcmp %s,#0\n", rnames[R0]);
}


/*
 * http://gcc.gnu.org/onlinedocs/gccint/Integer-library-routines.html#Integer-library-routines
 */

static void
emul(NODE *p)
{
	char *ch = NULL;

/**/	if (p->n_op == LS && DEUNSIGN(p->n_type) == LONGLONG) ch = "ashlti3";
	else if (p->n_op == LS && DEUNSIGN(p->n_type) == LONG) ch = "ashldi3";
	else if (p->n_op == LS && DEUNSIGN(p->n_type) == INT) ch = "ashlsi3";

/**/	else if (p->n_op == RS && p->n_type == ULONGLONG) ch = "lshrti3";
	else if (p->n_op == RS && p->n_type == ULONG) ch = "lshrdi3";
	else if (p->n_op == RS && p->n_type == UNSIGNED) ch = "lshrsi3";

/**/	else if (p->n_op == RS && p->n_type == LONGLONG) ch = "ashrti3";
	else if (p->n_op == RS && p->n_type == LONG) ch = "ashrdi3";
	else if (p->n_op == RS && p->n_type == INT) ch = "ashrsi3";
	
	else if (p->n_op == DIV && p->n_type == LONGLONG) ch = "divdi3";
	else if (p->n_op == DIV && p->n_type == LONG) ch = "divdi3";
	else if (p->n_op == DIV && p->n_type == INT) ch = "divsi3";

	else if (p->n_op == DIV && p->n_type == ULONGLONG) ch = "udivdi3";
	else if (p->n_op == DIV && p->n_type == ULONG) ch = "udivdi3";
	else if (p->n_op == DIV && p->n_type == UNSIGNED) ch = "udivsi3";

	else if (p->n_op == MOD && p->n_type == LONGLONG) ch = "moddi3";
	else if (p->n_op == MOD && p->n_type == LONG) ch = "moddi3";
	else if (p->n_op == MOD && p->n_type == INT) ch = "modsi3";

	else if (p->n_op == MOD && p->n_type == ULONGLONG) ch = "umoddi3";
	else if (p->n_op == MOD && p->n_type == ULONG) ch = "umoddi3";
	else if (p->n_op == MOD && p->n_type == UNSIGNED) ch = "umodsi3";

	else if (p->n_op == MUL && p->n_type == LONGLONG) ch = "muldi3";
	else if (p->n_op == MUL && p->n_type == LONG) ch = "muldi3";
	else if (p->n_op == MUL && p->n_type == INT) ch = "mulsi3";

	else if (p->n_op == UMINUS && p->n_type == LONGLONG) ch = "negti2";
	else if (p->n_op == UMINUS && p->n_type == LONG) ch = "negdi2";

	else ch = 0, comperr("ZE");
	printf("\tbl __%s" COM "emulated operation\n", exname(ch));
}

static void
halfword(NODE *p)
{
        NODE *r = getlr(p, 'R');
        NODE *l = getlr(p, 'L');
	int idx0 = 0, idx1 = 1;

	if (features(FEATURE_BIGENDIAN)) {
		idx0 = 1;
		idx1 = 0;
	}

	if (p->n_op == ASSIGN && r->n_op == OREG) {
                /* load */
                expand(p, 0, "\tldrb A1,");
                printf("[%s," CONFMT "]\n", rnames[r->n_rval], r->n_lval+idx0);
                expand(p, 0, "\tldrb AL,");
                printf("[%s," CONFMT "]\n", rnames[r->n_rval], r->n_lval+idx1);
                expand(p, 0, "\torr AL,A1,AL,asl #8\n");
        } else if (p->n_op == ASSIGN && l->n_op == OREG) {
                /* store */
                expand(p, 0, "\tstrb AR,");
                printf("[%s," CONFMT "]\n", rnames[l->n_rval], l->n_lval+idx0);
                expand(p, 0, "\tmov A1,AR,asr #8\n");
                expand(p, 0, "\tstrb A1,");
                printf("[%s," CONFMT "]\n", rnames[l->n_rval], l->n_lval+idx1);
        } else if (p->n_op == SCONV || p->n_op == UMUL) {
                /* load */
                expand(p, 0, "\tldrb A1,");
                printf("[%s," CONFMT "]\n", rnames[l->n_rval], l->n_lval+idx0);
                expand(p, 0, "\tldrb A2,");
                printf("[%s," CONFMT "]\n", rnames[l->n_rval], l->n_lval+idx1);
                expand(p, 0, "\torr A1,A1,A2,asl #8\n");
        } else if (p->n_op == NAME || p->n_op == ICON || p->n_op == OREG) {
                /* load */
                expand(p, 0, "\tldrb A1,");
                printf("[%s," CONFMT "]\n", rnames[p->n_rval], p->n_lval+idx0);
                expand(p, 0, "\tldrb A2,");
                printf("[%s," CONFMT "]\n", rnames[p->n_rval], p->n_lval+idx1);
                expand(p, 0, "\torr A1,A1,A2,asl #8\n");
	} else {
		comperr("halfword");
        }
}

static void
bfext(NODE *p)
{
        int sz;

        if (ISUNSIGNED(p->n_right->n_type))
                return;
        sz = 32 - UPKFSZ(p->n_left->n_rval);

	expand(p, 0, "\tmov AD,AD,asl ");
        printf("#%d\n", sz);
	expand(p, 0, "\tmov AD,AD,asr ");
        printf("#%d\n", sz);
}

static int
argsiz(NODE *p)
{
	TWORD t = p->n_type;

	if (t < LONGLONG || t == FLOAT || t > BTMASK)
		return 4;
	if (t == LONGLONG || t == ULONGLONG)
		return 8;
	if (t == DOUBLE || t == LDOUBLE)
		return 8;
	if (t == STRTY || t == UNIONTY)
		return p->n_stsize;
	comperr("argsiz");
	return 0;
}

void
zzzcode(NODE *p, int c)
{
	int pr;

	switch (c) {

	case 'B': /* bit-field sign extension */
		bfext(p);
		break;

	case 'C':  /* remove from stack after subroutine call */
		pr = p->n_qual;
#if 0
		if (p->n_op == STCALL || p->n_op == USTCALL)
			pr += 4;
#endif
		if (p->n_op == UCALL)
			return; /* XXX remove ZC from UCALL */
		if (pr > 0)
			printf("\tadd %s,%s,#%d\n", rnames[SP], rnames[SP], pr);
		break;

	case 'D': /* Long long comparision */
		twollcomp(p);
		break;

	case 'E': /* print out emulated ops */
		emul(p);
                break;

	case 'F': /* print out emulated floating-point ops */
		fpemul(p);
		break;

	case 'H':		/* do halfword access */
		halfword(p);
		break;

	case 'I':		/* init constant */
		if (p->n_name[0] != '\0')
			comperr("named init");
		load_constant_into_reg(DECRA(p->n_reg, 1),
		    p->n_lval & 0xffffffff);
		break;

	case 'J':		/* init longlong constant */
		load_constant_into_reg(DECRA(p->n_reg, 1) - R0R1,
		    p->n_lval & 0xffffffff);
		load_constant_into_reg(DECRA(p->n_reg, 1) - R0R1 + 1,
                    (p->n_lval >> 32));
                break;

	case 'O': /* 64-bit left and right shift operators */
		shiftop(p);
		break;

	case 'Q': /* emit struct assign */
		stasg(p);
		break;

	default:
		comperr("zzzcode %c", c);
	}
}

/*ARGSUSED*/
int
rewfld(NODE *p)
{
	return(1);
}

/*
 * Does the bitfield shape match?
 */
int
flshape(NODE *p)
{
	int o = p->n_op;

	if (o == OREG || o == REG || o == NAME)
		return SRDIR; /* Direct match */
	if (o == UMUL && shumul(p->n_left))
		return SROREG; /* Convert into oreg */
	return SRREG; /* put it into a register */
}

/* INTEMP shapes must not contain any temporary registers */
/* XXX should this go away now? */
int
shtemp(NODE *p)
{
	return 0;
#if 0
	int r;

	if (p->n_op == STARG )
		p = p->n_left;

	switch (p->n_op) {
	case REG:
		return (!istreg(p->n_rval));

	case OREG:
		r = p->n_rval;
		if (R2TEST(r)) {
			if (istreg(R2UPK1(r)))
				return(0);
			r = R2UPK2(r);
		}
		return (!istreg(r));

	case UMUL:
		p = p->n_left;
		return (p->n_op != UMUL && shtemp(p));
	}

	if (optype(p->n_op) != LTYPE)
		return(0);
	return(1);
#endif
}

void
adrcon(CONSZ val)
{
	printf(CONFMT, val);
}

void
conput(FILE *fp, NODE *p)
{
	char *s;
	int val = p->n_lval;

	switch (p->n_op) {
	case ICON:
#if 0
		if (p->n_sp)
			printf(" [class=%d,level=%d] ", p->n_sp->sclass, p->n_sp->slevel);
#endif
#ifdef notdef	/* ICON cannot ever use sp here */
		/* If it does, it's a giant bug */
		if (p->n_sp == NULL || (p->n_sp->sclass == ILABEL ||
		   (p->n_sp->sclass == STATIC && p->n_sp->slevel > 0)))
			s = p->n_name;
		else
			s = exname(p->n_name);
#else
		s = p->n_name;
#endif
			
		if (*s != '\0') {
			fprintf(fp, "%s", s);
			if (val > 0)
				fprintf(fp, "+%d", val);
			else if (val < 0)
				fprintf(fp, "-%d", -val);
		} else
			fprintf(fp, CONFMT, (CONSZ)val);
		return;

	default:
		comperr("illegal conput, p %p", p);
	}
}

/*ARGSUSED*/
void
insput(NODE *p)
{
	comperr("insput");
}

/*
 * Write out the upper address, like the upper register of a 2-register
 * reference, or the next memory location.
 */
void
upput(NODE *p, int size)
{

	size /= SZCHAR;
	switch (p->n_op) {
	case REG:
		fprintf(stdout, "%s", rnames[p->n_rval-R0R1+1]);
		break;

	case NAME:
	case OREG:
		p->n_lval += size;
		adrput(stdout, p);
		p->n_lval -= size;
		break;
	case ICON:
		fprintf(stdout, CONFMT, p->n_lval >> 32);
		break;
	default:
		comperr("upput bad op %d size %d", p->n_op, size);
	}
}

void
adrput(FILE *io, NODE *p)
{
	int r;
	/* output an address, with offsets, from p */

	if (p->n_op == FLD)
		p = p->n_left;

	switch (p->n_op) {

	case NAME:
		if (p->n_name[0] != '\0') {
			fputs(p->n_name, io);
			if (p->n_lval != 0)
				fprintf(io, "+%lld", p->n_lval);
		} else
			fprintf(io, CONFMT, p->n_lval);
		return;

	case OREG:
		r = p->n_rval;
                if (R2TEST(r))
			fprintf(io, "[%s, %s, lsl #%d]",
				rnames[R2UPK1(r)],
				rnames[R2UPK2(r)],
				R2UPK3(r));
		else
			fprintf(io, "[%s,#%d]", rnames[p->n_rval], (int)p->n_lval);
		return;

	case ICON:
		/* addressable value of the constant */
		conput(io, p);
		return;

	case REG:
		switch (p->n_type) {
		case DOUBLE:
		case LDOUBLE:
			if (features(FEATURE_HARDFLOAT)) {
				fprintf(io, "%s", rnames[p->n_rval]);
				break;
			}
			/* FALLTHROUGH */
		case LONGLONG:
		case ULONGLONG:
			fprintf(stdout, "%s", rnames[p->n_rval-R0R1]);
			break;
		default:
			fprintf(io, "%s", rnames[p->n_rval]);
		}
		return;

	default:
		comperr("illegal address, op %d, node %p", p->n_op, p);
		return;

	}
}

/*   printf conditional and unconditional branches */
void
cbgen(int o, int lab)
{
	if (o < EQ || o > UGT)
		comperr("bad conditional branch: %s", opst[o]);
	printf("\t%s " LABFMT COM "conditional branch\n",
	    ccbranches[o-EQ], lab);
}

/*
 * The arm can only address 4k to get a NAME, so there must be some
 * rewriting here.  Strategy:
 * For first 1000 nodes found, print out the word directly.
 * For the following 1000 nodes, group them together in asm statements
 * and create a jump over.
 * For the last <1000 statements, print out the words last.
 */
struct addrsymb {
	SLIST_ENTRY(addrsymb) link;
	char *name;	/* symbol name */
	int num;	/* symbol offset */
	char *str;	/* replace label */
};
SLIST_HEAD(, addrsymb) aslist;
static struct interpass *ipbase;
static int prtnumber, nodcnt, notfirst;
#define	PRTLAB	".LY%d"	/* special for here */

static struct interpass *
anode(char *p)
{
	extern int thisline;
	struct interpass *ip = tmpalloc(sizeof(struct interpass));

	ip->ip_asm = p;
	ip->type = IP_ASM;
	ip->lineno = thisline;
	return ip;
}

static void
flshlab(void)
{
	struct interpass *ip;
	struct addrsymb *el;
	int lab = prtnumber++;
	char *c;

	if (SLIST_FIRST(&aslist) == NULL)
		return;

	snprintf(c = tmpalloc(32), 32, "\tb " PRTLAB "\n", lab);
	ip = anode(c);
	DLIST_INSERT_BEFORE(ipbase, ip, qelem);

	SLIST_FOREACH(el, &aslist, link) {
		/* insert each node as asm */
		int l = 32+strlen(el->name);
		c = tmpalloc(l);
		if (el->num)
			snprintf(c, l, "%s:\n\t.word %s+%d\n",
			    el->str, el->name, el->num);
		else
			snprintf(c, l, "%s:\n\t.word %s\n", el->str, el->name);
		ip = anode(c);
		DLIST_INSERT_BEFORE(ipbase, ip, qelem);
	}
	/* generate asm label */
	snprintf(c = tmpalloc(32), 32, PRTLAB ":\n", lab);
	ip = anode(c);
	DLIST_INSERT_BEFORE(ipbase, ip, qelem);
}

static void
prtaddr(NODE *p)
{
	NODE *l = p->n_left;
	struct addrsymb *el;
	int found = 0;
	int lab;

	nodcnt++;

	if (p->n_op == ASSIGN && p->n_right->n_op == ICON &&
	    p->n_right->n_name[0] != '\0') {
		/* named constant */
		p = p->n_right;

		/* Restore addrof */
		l = mklnode(NAME, p->n_lval, 0, 0);
		l->n_name = p->n_name;
		p->n_left = l;
		p->n_op = ADDROF;
	}

	if (p->n_op != ADDROF || l->n_op != NAME)
		return;

	/* if we passed 1k nodes printout list */
	if (nodcnt > 1000) {
		if (notfirst)
			flshlab();
		SLIST_INIT(&aslist);
		notfirst = 1;
		nodcnt = 0;
	}

	/* write address to byte stream */

	SLIST_FOREACH(el, &aslist, link) {
		if (el->num == l->n_lval && el->name[0] == l->n_name[0] &&
		    strcmp(el->name, l->n_name) == 0) {
			found = 1;
			break;
		}
	}

	if (!found) {
		/* we know that this is text segment */
		lab = prtnumber++;
		if (nodcnt <= 1000 && notfirst == 0) {
			if (l->n_lval)
				printf(PRTLAB ":\n\t.word %s+%lld\n",
				    lab, l->n_name, l->n_lval);
			else
				printf(PRTLAB ":\n\t.word %s\n",
				    lab, l->n_name);
		}
		el = tmpalloc(sizeof(struct addrsymb));
		el->num = l->n_lval;
		el->name = l->n_name;
		el->str = tmpalloc(32);
		snprintf(el->str, 32, PRTLAB, lab);
		SLIST_INSERT_LAST(&aslist, el, link);
	}

	nfree(l);
	p->n_op = NAME;
	p->n_lval = 0;
	p->n_name = el->str;
}

void
myreader(struct interpass *ipole)
{
	struct interpass *ip;

	SLIST_INIT(&aslist);
	notfirst = nodcnt = 0;

	DLIST_FOREACH(ip, ipole, qelem) {
		switch (ip->type) {
		case IP_NODE:
			lineno = ip->lineno;
			ipbase = ip;
			walkf(ip->ip_node, prtaddr);
			break;
		case IP_EPILOG:
			ipbase = ip;
			if (notfirst)
				flshlab();
			break;
		default:
			break;
		}
	}
	if (x2debug)
		printip(ipole);
}

/*
 * Remove some PCONVs after OREGs are created.
 */
static void
pconv2(NODE *p)
{
	NODE *q;

	if (p->n_op == PLUS) {
		if (p->n_type == (PTR+SHORT) || p->n_type == (PTR+USHORT)) {
			if (p->n_right->n_op != ICON)
				return;
			if (p->n_left->n_op != PCONV)
				return;
			if (p->n_left->n_left->n_op != OREG)
				return;
			q = p->n_left->n_left;
			nfree(p->n_left);
			p->n_left = q;
			/*
			 * This will be converted to another OREG later.
			 */
		}
	}
}

void
mycanon(NODE *p)
{
	walkf(p, pconv2);
}

void
myoptim(struct interpass *ipp)
{
}

/*
 * Register move: move contents of register 's' to register 'r'.
 */
void
rmove(int s, int d, TWORD t)
{
        switch (t) {
	case DOUBLE:
	case LDOUBLE:
		if (features(FEATURE_HARDFLOAT)) {
			printf("\tfmr %s,%s" COM "rmove\n",
				rnames[d], rnames[s]);
			break;
		}
		/* FALLTHROUGH */
        case LONGLONG:
        case ULONGLONG:
#define LONGREG(x, y) rnames[(x)-(R0R1-(y))]
                if (s == d+1) {
                        /* dh = sl, copy low word first */
                        printf("\tmov %s,%s" COM "rmove\n",
			    LONGREG(d,0), LONGREG(s,0));
                        printf("\tmov %s,%s\n",
			    LONGREG(d,1), LONGREG(s,1));
                } else {
                        /* copy high word first */
                        printf("\tmov %s,%s" COM "rmove\n",
			    LONGREG(d,1), LONGREG(s,1));
                        printf("\tmov %s,%s\n",
			    LONGREG(d,0), LONGREG(s,0));
                }
#undef LONGREG
                break;
	case FLOAT:
		if (features(FEATURE_HARDFLOAT)) {
			printf("\tmr %s,%s" COM "rmove\n",
				rnames[d], rnames[s]);
			break;
		}
		/* FALLTHROUGH */
        default:
		printf("\tmov %s,%s" COM "rmove\n", rnames[d], rnames[s]);
        }
}

/*
 * Can we assign a register from class 'c', given the set
 * of number of assigned registers in each class 'r'.
 *
 * On ARM, we have:
 *	11  CLASSA registers (32-bit hard registers)
 *	10  CLASSB registers (64-bit composite registers)
 *	8 or 32 CLASSC registers (floating-point)
 *
 *  There is a problem calculating the available composite registers
 *  (ie CLASSB).  The algorithm below assumes that given any two
 *  registers, we can make a composite register.  But this isn't true
 *  here (or with other targets), since the number of combinations
 *  of register pairs could become very large.  Additionally,
 *  having so many combinations really isn't so practical, since
 *  most register pairs cannot be used to pass function arguments.
 *  Consequently, when there is pressure composite registers,
 *  "beenhere" compilation failures are common.
 *
 *  [We need to know which registers are allocated, not simply
 *  the number in each class]
 */
int
COLORMAP(int c, int *r)
{
	int num = 0;	/* number of registers used */

#if 0
	static const char classes[] = { 'X', 'A', 'B', 'C', 'D' };
	printf("COLORMAP: requested class %c\n", classes[c]);
	printf("COLORMAP: class A: %d\n", r[CLASSA]);
	printf("COLORMAP: class B: %d\n", r[CLASSB]);
#endif

	switch (c) {
	case CLASSA:
		num += r[CLASSA];
		num += 2*r[CLASSB];
		return num < 11;
	case CLASSB:
		num += 2*r[CLASSB];
		num += r[CLASSA];
		return num < 6;  /* XXX see comments above */
	case CLASSC:
		num += r[CLASSC];
		if (features(FEATURE_FPA))
			return num < 8;
		else if (features(FEATURE_VFP))
			return num < 8;
		else
			cerror("colormap 1");
	}
	cerror("colormap 2");
	return 0; /* XXX gcc */
}

/*
 * Return a class suitable for a specific type.
 */
int
gclass(TWORD t)
{
	if (t == DOUBLE || t == LDOUBLE) {
		if (features(FEATURE_HARDFLOAT))
			return CLASSC;
		else
			return CLASSB;
	}
	if (t == FLOAT) {
		if (features(FEATURE_HARDFLOAT))
			return CLASSC;
		else
			return CLASSA;
	}
	if (DEUNSIGN(t) == LONGLONG)
		return CLASSB;
	return CLASSA;
}

int
retreg(int t)
{
	int c = gclass(t);
	if (c == CLASSB)
		return R0R1;
	else if (c == CLASSC)
		return F0;
	return R0;
}

/*
 * Calculate argument sizes.
 */
void
lastcall(NODE *p)
{
	NODE *op = p;
	int size = 0;

	p->n_qual = 0;
	if (p->n_op != CALL && p->n_op != FORTCALL && p->n_op != STCALL)
		return;
	for (p = p->n_right; p->n_op == CM; p = p->n_left)
		size += argsiz(p->n_right);
	size += argsiz(p);
	op->n_qual = size - 16; /* XXX */
}

/*
 * Special shapes.
 */
int
special(NODE *p, int shape)
{
	return SRNOPE;
}

/*
 * default to ARMv2
 */
#ifdef TARGET_BIG_ENDIAN
#define DEFAULT_FEATURES	FEATURE_BIGENDIAN | FEATURE_MUL
#else
#define DEFAULT_FEATURES	FEATURE_MUL
#endif

static int fset = DEFAULT_FEATURES;

/*
 * Target-dependent command-line options.
 */
void
mflags(char *str)
{
	if (strcasecmp(str, "little-endian") == 0) {
		fset &= ~FEATURE_BIGENDIAN;
	} else if (strcasecmp(str, "big-endian") == 0) {
		fset |= FEATURE_BIGENDIAN;
	} else if (strcasecmp(str, "fpe=fpa") == 0) {
		fset &= ~(FEATURE_VFP | FEATURE_FPA);
		fset |= FEATURE_FPA;
	} else if (strcasecmp(str, "fpe=vfp") == 0) {
		fset &= ~(FEATURE_VFP | FEATURE_FPA);
		fset |= FEATURE_VFP;
	} else if (strcasecmp(str, "soft-float") == 0) {
		fset &= ~(FEATURE_VFP | FEATURE_FPA);
	} else if (strcasecmp(str, "arch=armv1") == 0) {
		fset &= ~FEATURE_HALFWORDS;
		fset &= ~FEATURE_EXTEND;
		fset &= ~FEATURE_MUL;
		fset &= ~FEATURE_MULL;
	} else if (strcasecmp(str, "arch=armv2") == 0) {
		fset &= ~FEATURE_HALFWORDS;
		fset &= ~FEATURE_EXTEND;
		fset |= FEATURE_MUL;
		fset &= ~FEATURE_MULL;
	} else if (strcasecmp(str, "arch=armv2a") == 0) {
		fset &= ~FEATURE_HALFWORDS;
		fset &= ~FEATURE_EXTEND;
		fset |= FEATURE_MUL;
		fset &= ~FEATURE_MULL;
	} else if (strcasecmp(str, "arch=armv3") == 0) {
		fset &= ~FEATURE_HALFWORDS;
		fset &= ~FEATURE_EXTEND;
		fset |= FEATURE_MUL;
		fset &= ~FEATURE_MULL;
	} else if (strcasecmp(str, "arch=armv4") == 0) {
		fset |= FEATURE_HALFWORDS;
		fset &= ~FEATURE_EXTEND;
		fset |= FEATURE_MUL;
		fset |= FEATURE_MULL;
	} else if (strcasecmp(str, "arch=armv4t") == 0) {
		fset |= FEATURE_HALFWORDS;
		fset &= ~FEATURE_EXTEND;
		fset |= FEATURE_MUL;
		fset |= FEATURE_MULL;
	} else if (strcasecmp(str, "arch=armv4tej") == 0) {
		fset |= FEATURE_HALFWORDS;
		fset &= ~FEATURE_EXTEND;
		fset |= FEATURE_MUL;
		fset |= FEATURE_MULL;
	} else if (strcasecmp(str, "arch=armv5") == 0) {
		fset |= FEATURE_HALFWORDS;
		fset &= ~FEATURE_EXTEND;
		fset |= FEATURE_MUL;
		fset |= FEATURE_MULL;
	} else if (strcasecmp(str, "arch=armv5te") == 0) {
		fset |= FEATURE_HALFWORDS;
		fset &= ~FEATURE_EXTEND;
		fset |= FEATURE_MUL;
		fset |= FEATURE_MULL;
	} else if (strcasecmp(str, "arch=armv5tej") == 0) {
		fset |= FEATURE_HALFWORDS;
		fset &= ~FEATURE_EXTEND;
		fset |= FEATURE_MUL;
		fset |= FEATURE_MULL;
	} else if (strcasecmp(str, "arch=armv6") == 0) {
		fset |= FEATURE_HALFWORDS;
		fset |= FEATURE_EXTEND;
		fset |= FEATURE_MUL;
		fset |= FEATURE_MULL;
	} else if (strcasecmp(str, "arch=armv6t2") == 0) {
		fset |= FEATURE_HALFWORDS;
		fset |= FEATURE_EXTEND;
		fset |= FEATURE_MUL;
		fset |= FEATURE_MULL;
	} else if (strcasecmp(str, "arch=armv6kz") == 0) {
		fset |= FEATURE_HALFWORDS;
		fset |= FEATURE_EXTEND;
		fset |= FEATURE_MUL;
		fset |= FEATURE_MULL;
	} else if (strcasecmp(str, "arch=armv6k") == 0) {
		fset |= FEATURE_HALFWORDS;
		fset |= FEATURE_EXTEND;
		fset |= FEATURE_MUL;
		fset |= FEATURE_MULL;
	} else if (strcasecmp(str, "arch=armv7") == 0) {
		fset |= FEATURE_HALFWORDS;
		fset |= FEATURE_EXTEND;
		fset |= FEATURE_MUL;
		fset |= FEATURE_MULL;
	} else {
		fprintf(stderr, "unknown m option '%s'\n", str);
		exit(1);
	}
}

int
features(int mask)
{
	if (mask == FEATURE_HARDFLOAT)
		return ((fset & mask) != 0);
	return ((fset & mask) == mask);
}

/*
 * Define the current location as an internal label.
 */
void
deflab(int label)
{
	printf(LABFMT ":\n", label);
}

