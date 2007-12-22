/*      $OpenBSD: local2.c,v 1.2 2007/12/22 12:38:56 stefan Exp $    */
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

#include "pass1.h"
#include "pass2.h"

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
	expand(p, 0, "\tcmp UR,UL\t@ compare 64-bit values (upper)\n");
	if (cb1) cbgen(cb1, s);
	if (cb2) cbgen(cb2, e);
	expand(p, 0, "\tcmp AR,AL\t@ (and lower)\n");
	cbgen(p->n_op, e);
	deflab(s);
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

	printf("\tldr %s,=%d\n", rnames[R2], p->n_stsize);
	if (l->n_rval != R0 || l->n_lval != 0)
		printf("\tadd %s,%s," CONFMT "\n", rnames[R0],
		    rnames[l->n_rval], l->n_lval);
	printf("\tbl %s\n", exname("memcpy"));
}

static void
shiftop(NODE *p)
{
	NODE *r = p->n_right;
	TWORD ty = p->n_type;

	if (p->n_op == LS && r->n_op == ICON && r->n_lval < 32) {
		expand(p, INBREG, "\tmov A1,AL,lsr ");
		printf(CONFMT "\t@ 64-bit left-shift\n", 32 - r->n_lval);
		expand(p, INBREG, "\tmov U1,UL,asl AR\n");
		expand(p, INBREG, "\torr U1,U1,A1\n");
		expand(p, INBREG, "\tmov A1,AL,asl AR\n");
	} else if (p->n_op == LS && r->n_op == ICON && r->n_lval < 64) {
		expand(p, INBREG, "\tldr A1,=0\t@ 64-bit left-shift\n");
		expand(p, INBREG, "\tmov U1,AL,asl ");
		printf(CONFMT "\n", r->n_lval - 32);
	} else if (p->n_op == LS && r->n_op == ICON) {
		expand(p, INBREG, "\tldr A1,=0\t@ 64-bit left-shift\n");
		expand(p, INBREG, "\tldr U1,=0\n");
	} else if (p->n_op == RS && r->n_op == ICON && r->n_lval < 32) {
		expand(p, INBREG, "\tmov U1,UL,asl ");
		printf(CONFMT "\t@ 64-bit right-shift\n", 32 - r->n_lval);
		expand(p, INBREG, "\tmov A1,AL,lsr AR\n");
		expand(p, INBREG, "\torr A1,A1,U1\n");
		if (ty == LONGLONG)
			expand(p, INBREG, "\tmov U1,UL,asr AR\n");
		else
			expand(p, INBREG, "\tmov U1,UL,lsr AR\n");
	} else if (p->n_op == RS && r->n_op == ICON && r->n_lval < 64) {
		if (ty == LONGLONG) {
			expand(p, INBREG, "\tldr U1,=-1\t@ 64-bit right-shift\n");
			expand(p, INBREG, "\tmov A1,UL,asr ");
		}else {
			expand(p, INBREG, "\tldr U1,=0\t@ 64-bit right-shift\n");
			expand(p, INBREG, "\tmov A1,UL,lsr ");
		}
		printf(CONFMT "\n", r->n_lval - 32);
	} else if (p->n_op == LS && r->n_op == ICON) {
		expand(p, INBREG, "\tldr A1,=0\t@ 64-bit right-shift\n");
		expand(p, INBREG, "\tldr U1,=0\n");
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
	else if (p->n_op == PLUS && p->n_type == LDOUBLE) ch = "addtf3";

	else if (p->n_op == MINUS && p->n_type == FLOAT) ch = "subsf3";
	else if (p->n_op == MINUS && p->n_type == DOUBLE) ch = "subdf3";
	else if (p->n_op == MINUS && p->n_type == LDOUBLE) ch = "subtf3";

	else if (p->n_op == MUL && p->n_type == FLOAT) ch = "mulsf3";
	else if (p->n_op == MUL && p->n_type == DOUBLE) ch = "muldf3";
	else if (p->n_op == MUL && p->n_type == LDOUBLE) ch = "multf3";

	else if (p->n_op == DIV && p->n_type == FLOAT) ch = "divsf3";
	else if (p->n_op == DIV && p->n_type == DOUBLE) ch = "divdf3";
	else if (p->n_op == DIV && p->n_type == LDOUBLE) ch = "divtf3";

	else if (p->n_op == UMINUS && p->n_type == FLOAT) ch = "negsf2";
	else if (p->n_op == UMINUS && p->n_type == DOUBLE) ch = "negdf2";
	else if (p->n_op == UMINUS && p->n_type == LDOUBLE) ch = "negtf2";

	else if (p->n_op == EQ && l->n_type == FLOAT) ch = "eqsf2";
	else if (p->n_op == EQ && l->n_type == DOUBLE) ch = "eqdf2";
	else if (p->n_op == EQ && l->n_type == LDOUBLE) ch = "eqtf2";

	else if (p->n_op == NE && l->n_type == FLOAT) ch = "nesf2";
	else if (p->n_op == NE && l->n_type == DOUBLE) ch = "nedf2";
	else if (p->n_op == NE && l->n_type == LDOUBLE) ch = "netf2";

	else if (p->n_op == GE && l->n_type == FLOAT) ch = "gesf2";
	else if (p->n_op == GE && l->n_type == DOUBLE) ch = "gedf2";
	else if (p->n_op == GE && l->n_type == LDOUBLE) ch = "getf2";

	else if (p->n_op == LE && l->n_type == FLOAT) ch = "lesf2";
	else if (p->n_op == LE && l->n_type == DOUBLE) ch = "ledf2";
	else if (p->n_op == LE && l->n_type == LDOUBLE) ch = "letf2";

	else if (p->n_op == GT && l->n_type == FLOAT) ch = "gtsf2";
	else if (p->n_op == GT && l->n_type == DOUBLE) ch = "gtdf2";
	else if (p->n_op == GT && l->n_type == LDOUBLE) ch = "gttf2";

	else if (p->n_op == LT && l->n_type == FLOAT) ch = "ltsf2";
	else if (p->n_op == LT && l->n_type == DOUBLE) ch = "ltdf2";
	else if (p->n_op == LT && l->n_type == LDOUBLE) ch = "lttf2";

	else if (p->n_op == SCONV && p->n_type == FLOAT) {
		if (l->n_type == DOUBLE) ch = "truncdfsf2";
		else if (l->n_type == LDOUBLE) ch = "trunctfsf2";
		else if (l->n_type == ULONGLONG) ch = "floatuntisf";
		else if (l->n_type == LONGLONG) ch = "floattisf";
		else if (l->n_type == LONG) ch = "floatdisf";
		else if (l->n_type == ULONG) ch = "floatundisf";
		else if (l->n_type == INT) ch = "floatsisf";
		else if (l->n_type == UNSIGNED) ch = "floatunsisf";
	} else if (p->n_op == SCONV && p->n_type == DOUBLE) {
		if (l->n_type == FLOAT) ch = "extendsfdf2";
		else if (l->n_type == LDOUBLE) ch = "trunctfdf2";
		else if (l->n_type == ULONGLONG) ch = "floatuntidf";
		else if (l->n_type == LONGLONG) ch = "floattidf";
		else if (l->n_type == LONG) ch = "floatdidf";
		else if (l->n_type == ULONG) ch = "floatundidf";
		else if (l->n_type == INT) ch = "floatsidf";
		else if (l->n_type == UNSIGNED) ch = "floatunsidf";
	} else if (p->n_op == SCONV && p->n_type == LDOUBLE) {
		if (l->n_type == FLOAT) ch = "extendsftf2";
		else if (l->n_type == DOUBLE) ch = "extenddftf2";
		else if (l->n_type == ULONGLONG) ch = "floatuntitf";
		else if (l->n_type == LONGLONG) ch = "floattitf";
		else if (l->n_type == LONG) ch = "floatditf";
		else if (l->n_type == ULONG) ch = "floatunsditf";
		else if (l->n_type == INT) ch = "floatsitf";
		else if (l->n_type == UNSIGNED) ch = "floatunsitf";
	} else if (p->n_op == SCONV && p->n_type == ULONGLONG) {
		if (l->n_type == FLOAT) ch = "fixunssfti";
		else if (l->n_type == DOUBLE) ch = "fixunsdfti";
		else if (l->n_type == LDOUBLE) ch = "fixunstfti";
	} else if (p->n_op == SCONV && p->n_type == LONGLONG) {
		if (l->n_type == FLOAT) ch = "fixsfti";
		else if (l->n_type == DOUBLE) ch = "fixdfti";
		else if (l->n_type == LDOUBLE) ch = "fixtfti";
	} else if (p->n_op == SCONV && p->n_type == LONG) {
		if (l->n_type == FLOAT) ch = "fixsfdi";
		else if (l->n_type == DOUBLE) ch = "fixdfdi";
		else if (l->n_type == LDOUBLE) ch = "fixtfdi";
	} else if (p->n_op == SCONV && p->n_type == ULONG) {
		if (l->n_type == FLOAT) ch = "fixunssfdi";
		else if (l->n_type == DOUBLE) ch = "fixunsdfdi";
		else if (l->n_type == LDOUBLE) ch = "fixunstfdi";
	} else if (p->n_op == SCONV && p->n_type == INT) {
		if (l->n_type == FLOAT) ch = "fixsfsi";
		else if (l->n_type == DOUBLE) ch = "fixdfsi";
		else if (l->n_type == LDOUBLE) ch = "fixtfsi";
	} else if (p->n_op == SCONV && p->n_type == UNSIGNED) {
		if (l->n_type == FLOAT) ch = "fixunssfsi";
		else if (l->n_type == DOUBLE) ch = "fixunsdfsi";
		else if (l->n_type == LDOUBLE) ch = "fixunstfsi";
	}

	if (ch == NULL) comperr("ZF: op=0x%x (%d)\n", p->n_op, p->n_op);

	printf("\tbl __%s\t@ softfloat operation\n", exname(ch));

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
	
	else if (p->n_op == DIV && p->n_type == LONGLONG) ch = "divti3";
	else if (p->n_op == DIV && p->n_type == LONG) ch = "divdi3";
	else if (p->n_op == DIV && p->n_type == INT) ch = "divsi3";

	else if (p->n_op == DIV && p->n_type == ULONGLONG) ch = "udivti3";
	else if (p->n_op == DIV && p->n_type == ULONG) ch = "udivdi3";
	else if (p->n_op == DIV && p->n_type == UNSIGNED) ch = "udivsi3";

	else if (p->n_op == MOD && p->n_type == LONGLONG) ch = "modti3";
	else if (p->n_op == MOD && p->n_type == LONG) ch = "moddi3";
	else if (p->n_op == MOD && p->n_type == INT) ch = "modsi3";

	else if (p->n_op == MOD && p->n_type == ULONGLONG) ch = "umodti3";
	else if (p->n_op == MOD && p->n_type == ULONG) ch = "umoddi3";
	else if (p->n_op == MOD && p->n_type == UNSIGNED) ch = "umodsi3";

	else if (p->n_op == MUL && p->n_type == LONGLONG) ch = "multi3";
	else if (p->n_op == MUL && p->n_type == LONG) ch = "muldi3";
	else if (p->n_op == MUL && p->n_type == INT) ch = "mulsi3";

	else if (p->n_op == UMINUS && p->n_type == LONGLONG) ch = "negti2";
	else if (p->n_op == UMINUS && p->n_type == LONG) ch = "negdi2";

	else ch = 0, comperr("ZE");
	printf("\tbl __%s\t@ emulated operation\n", exname(ch));
}

static int
argsiz(NODE *p)
{
	TWORD t = p->n_type;

	if (t < LONGLONG || t == FLOAT || t > BTMASK)
		return 4;
	if (t == LONGLONG || t == ULONGLONG || t == DOUBLE)
		return 8;
	if (t == LDOUBLE)
		return 12;
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

#if 0
	case 'B': /* Assign to bitfield */
		bfasg(p);
		break;
#endif

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

        case 'I':               /* init constant */
                if (p->n_name[0] != '\0')
                        comperr("named init");
                fprintf(stdout, "=%lld", p->n_lval & 0xffffffff);
                break;

	case 'J':		/* init longlong constant */
		expand(p, INBREG, "\tldr A1,");
                fprintf(stdout, "=%lld\t@ load 64-bit constant\n",
		    p->n_lval & 0xffffffff);
		expand(p, INBREG, "\tldr U1,");
                fprintf(stdout, "=%lld\n", (p->n_lval >> 32));
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
		if (p->n_sp == NULL || (p->n_sp->sclass == ILABEL ||
		   (p->n_sp->sclass == STATIC && p->n_sp->slevel > 0)))
			s = p->n_name;
		else
			s = exname(p->n_name);
			
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
		fprintf(io, "[%s,#%d]", rnames[p->n_rval], (int)p->n_lval);
		return;

	case ICON:
		/* addressable value of the constant */
		conput(io, p);
		return;

	case MOVE:
	case REG:
		switch (p->n_type) {
#if !defined(ARM_HAS_FPA) && !defined(ARM_HAS_VFP)
		case DOUBLE:
		case LDOUBLE:
#endif
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
	printf("\t%s " LABFMT "\t@ conditional branch\n",
	    ccbranches[o-EQ], lab);
}

struct addrsymb {
	DLIST_ENTRY(addrsymb) link;
	struct symtab *orig;
	struct symtab *new;
};
struct addrsymb addrsymblist;

static void
prtaddr(NODE *p)
{
	NODE *l = p->n_left;
	struct addrsymb *el;
	int found = 0;
	int lab;

	if (p->n_op != ADDROF || l->n_op != NAME)
		return;

	/* write address to byte stream */

	DLIST_FOREACH(el, &addrsymblist, link) {
		if (el->orig == l->n_sp) {
			found = 1;
			break;
		}
	}

	if (!found) {
		setloc1(PROG);
		defalign(SZPOINT(l->n_type));
		deflab1(lab = getlab());
		printf("\t.word ");
		adrput(stdout, l);
		printf("\n");
		el = tmpalloc(sizeof(struct addrsymb));
		el->orig = l->n_sp;
 		el->new = tmpalloc(sizeof(struct symtab_hdr));
		el->new->sclass = ILABEL;
		el->new->soffset = lab;
		el->new->sflags = 0;
		DLIST_INSERT_BEFORE(&addrsymblist, el, link);
	}

	nfree(l);
	p->n_op = NAME;
	p->n_lval = 0;
	p->n_sp = el->new;
	p2tree(p);
}

void
myreader(struct interpass *ipole)
{
	struct interpass *ip;

	DLIST_INIT(&addrsymblist, link);

	DLIST_FOREACH(ip, ipole, qelem) {
		if (ip->type != IP_NODE)
			continue;
		walkf(ip->ip_node, prtaddr);
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
#if !defined(ARM_HAS_FPU) && !defined(ARM_HAS_VFP)
	case DOUBLE:
	case LDOUBLE:
#endif
        case LONGLONG:
        case ULONGLONG:
#define LONGREG(x, y) rnames[(x)-(R0R1-(y))]
                if (s == d+1) {
                        /* dh = sl, copy low word first */
                        printf("\tmov %s,%s	@ rmove\n",
			    LONGREG(d,0), LONGREG(s,0));
                        printf("\tmov %s,%s\n",
			    LONGREG(d,1), LONGREG(s,1));
                } else {
                        /* copy high word first */
                        printf("\tmov %s,%s	@ rmove\n",
			    LONGREG(d,1), LONGREG(s,1));
                        printf("\tmov %s,%s\n",
			    LONGREG(d,0), LONGREG(s,0));
                }
#undef LONGREG
                break;
        default:
		printf("\tmov %s,%s	@ rmove\n", rnames[d], rnames[s]);
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
		return num < 8;
	}
	assert(0);
	return 0; /* XXX gcc */
}

/*
 * Return a class suitable for a specific type.
 */
int
gclass(TWORD t)
{
#if defined(ARM_HAS_FPA) || defined(ARM_HAS_VFP)
	if (t == FLOAT || t == DOUBLE || t == LDOUBLE)
		return CLASSC;
#endif
	if (t == DOUBLE || t == LDOUBLE || t == LONGLONG || t == ULONGLONG)
		return CLASSB;
	return CLASSA;
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
 * Target-dependent command-line options.
 */
void
mflags(char *str)
{
}
