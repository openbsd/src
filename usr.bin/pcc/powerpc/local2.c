/*	$OpenBSD: local2.c,v 1.5 2007/12/22 14:05:04 stefan Exp $	*/
/*
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

#include "pass1.h" // for exname()
#include "pass2.h"
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

int argsize(NODE *p);

static int stkpos;

extern struct stub stublist;
extern struct stub nlplist;

static void
addstub(struct stub *list, char *name)
{
	struct stub *s;

	DLIST_FOREACH(s, list, link) {
		if (strcmp(s->name, name) == 0)
			return;
	}

	if ((s = malloc(sizeof(struct stub))) == NULL)
		cerror("addstub: malloc");
	if ((s->name = strdup(name)) == NULL)
		cerror("addstub: strdup");
	DLIST_INSERT_BEFORE(list, s, link);
}

void
deflab(int label)
{
	printf(LABFMT ":\n", label);
}

static int regoff[7];
static TWORD ftype;

static char *funcname = NULL;
/*
 * Print out the prolog assembler.
 * addto and regoff are already calculated.
 */
static void
prtprolog(struct interpass_prolog *ipp, int addto)
{
#if 1
	int i, j;
#endif
	addto = FIXEDSTACKSIZE;

	// get return address (not required for leaf function)
	printf("	mflr %s\n", rnames[R0]);
	// save registers R30 and R31
	printf("	stmw %s,-8(%s)\n", rnames[R30], rnames[R1]);
	// save return address (not required for leaf function)
	printf("	stw %s,8(%s)\n", rnames[R0], rnames[R1]);
	// create the new stack frame
	printf("	stwu %s,-%d(%s)\n", rnames[R1], addto, rnames[R1]);

	if (kflag) {
		funcname = ipp->ipp_name;
		printf("	bcl 20,31,L%s$pb\n", exname(ipp->ipp_name));
		printf("L%s$pb:\n", exname(ipp->ipp_name));
		printf("	mflr %s\n", rnames[R31]);
	}

#ifdef PCC_DEBUG
	if (x2debug) {
		printf("ipp_regs = 0x%x\n", ipp->ipp_regs);
	}
#endif

	for (i = ipp->ipp_regs, j = 0; i; i >>= 1, j++) {
		if (i & 1) {
			printf("	stw %s,%d(%s)\n",
			    rnames[j], regoff[j], rnames[FPREG]);
		}
	}
}

/*
 * calculate stack size and offsets
 */
static int
offcalc(struct interpass_prolog *ipp)
{
	int i, j, addto;

#ifdef PCC_DEBUG
	if (x2debug)
		printf("offcalc: p2maxautooff=%d\n", p2maxautooff);
#endif

	addto = p2maxautooff;

	// space is always allocated on the stack to save the registers
	for (i = ipp->ipp_regs, j = 0; i ; i >>= 1, j++) {
		if (i & 1) {
			addto += SZINT/SZCHAR;
			regoff[j] = addto;
		}
	}

	addto += 8; /* for R31 and R30 */

	/* round to 16-byte boundary */
	addto += 15;
	addto &= ~15;

#ifdef PCC_DEBUG
	if (x2debug)
		printf("offcalc: addto=%d\n", addto);
#endif

	return addto;
}

void
prologue(struct interpass_prolog *ipp)
{
	int addto;

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
	if (ipp->ipp_vis)
		printf("	.globl %s\n", exname(ipp->ipp_name));
	printf("	.align 2\n");
	printf("%s:\n", exname(ipp->ipp_name));
	/*
	 * We here know what register to save and how much to 
	 * add to the stack.
	 */
	addto = offcalc(ipp);
	prtprolog(ipp, addto);
}

void
eoftn(struct interpass_prolog *ipp)
{
	int i, j;

#ifdef PCC_DEBUG
	if (x2debug)
		printf("eoftn:\n");
#endif

	if (ipp->ipp_ip.ip_lbl == 0)
		return; /* no code needs to be generated */

	/* return from function code */
	for (i = ipp->ipp_regs, j = 0; i ; i >>= 1, j++) {
		if (i & 1)
			printf("\tlwz %s,%d(%s)\n",
			    rnames[j], regoff[j], rnames[FPREG]);
			
	}

//	assert(ftype != ipp->ipp_type);

	/* struct return needs special treatment */
	if (ftype == STRTY || ftype == UNIONTY) {
		assert(0);
		printf("	movl 8(%%ebp),%%eax\n");
		printf("	leave\n");
		printf("	ret $4\n");
	} else {
		// unwind stack frame
		printf("\tlwz %s,0(%s)\n", rnames[R1], rnames[R1]);
		printf("\tlwz %s,8(%s)\n", rnames[R0], rnames[R1]);
		printf("\tmtlr %s\n", rnames[R0]);
		printf("\tlmw %s,-8(%s)\n", rnames[R30], rnames[R1]);
		printf("\tblr\n");
	}
}

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
		str = "addw";
		break;
	case MINUS:
		str = "subw";
		break;
	case AND:
		str = "and";
		break;
	case OR:
		str = "or";
		break;
	case ER:
		str = "xor";
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
	expand(p, 0, "\tcmplw UR,UL		# compare 64-bit values (upper)\n");
	if (cb1) cbgen(cb1, s);
	if (cb2) cbgen(cb2, e);
	expand(p, 0, "\tcmplw AR,AL		# (and lower)\n");
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

#if 0
/*
 * Push a structure on stack as argument.
 * the scratch registers are already free here
 */
static void
starg(NODE *p)
{
	FILE *fp = stdout;

	fprintf(fp, "	subl $%d,%%esp\n", p->n_stsize);
	fprintf(fp, "	pushl $%d\n", p->n_stsize);
	expand(p, 0, "	pushl AL\n");
	expand(p, 0, "	leal 8(%esp),A1\n");
	expand(p, 0, "	pushl A1\n");
	fprintf(fp, "	call memcpy\n");
	fprintf(fp, "	addl $12,%%esp\n");
}
#endif

#if 0
/*
 * Compare two floating point numbers.
 */
static void
fcomp(NODE *p)  
{
	if (p->n_left->n_op == REG) {
		if (p->n_su & DORIGHT)
			expand(p, 0, "	fxch\n");
		expand(p, 0, "	fucompp\n");	/* emit compare insn  */
	} else if (p->n_left->n_type == DOUBLE)
		expand(p, 0, "	fcompl AL\n");	/* emit compare insn  */
	else if (p->n_left->n_type == FLOAT)
		expand(p, 0, "	fcomp AL\n");	/* emit compare insn  */
	else
		comperr("bad compare %p\n", p);
	expand(p, 0, "	fnstsw %ax\n");	/* move status reg to ax */
	
	switch (p->n_op) {
	case EQ:
		expand(p, 0, "	andb $64,%ah\n	jne LC\n");
		break;
	case NE:
		expand(p, 0, "	andb $64,%ah\n	je LC\n");
		break;
	case LE:
		expand(p, 0, "	andb $65,%ah\n	cmpb $1,%ah\n	jne LC\n");
		break;
	case LT:
		expand(p, 0, "	andb $65,%ah\n	je LC\n");
		break;
	case GT:
		expand(p, 0, "	andb $1,%ah\n	jne LC\n");
		break;
	case GE:
		expand(p, 0, "	andb $65,%ah\n	jne LC\n");
		break;
	default:
		comperr("fcomp op %d\n", p->n_op);
	}
}
#endif

#if 0
/*
 * Convert an unsigned long long to floating point number.
 */
static void
ulltofp(NODE *p)
{
	static int loadlab;
	int jmplab;

	if (loadlab == 0) {
		loadlab = getlab();
		expand(p, 0, "	.data\n");
		printf(LABFMT ":	.long 0,0x80000000,0x403f\n", loadlab);
		expand(p, 0, "	.text\n");
	}
	jmplab = getlab();
	expand(p, 0, "	pushl UL\n	pushl AL\n");
	expand(p, 0, "	fildq (%esp)\n");
	expand(p, 0, "	addl $8,%esp\n");
	expand(p, 0, "	cmpl $0,UL\n");
	printf("	jge " LABFMT "\n", jmplab);
	printf("	fldt " LABFMT "\n", loadlab);
	printf("	faddp %%st,%%st(1)\n");
	printf(LABFMT ":\n", jmplab);
}
#endif

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
#if 0
	NODE *r, *l;
	int pr, lr, s;
	char *ch;
#endif
	char inst[50];

	switch (c) {
#if 0
	case 'C':  /* remove from stack after subroutine call */
		pr = p->n_qual;
		if (p->n_op == STCALL || p->n_op == USTCALL)
			pr += 4;
		if (p->n_op == UCALL)
			return; /* XXX remove ZC from UCALL */
		if (pr)
			printf("	addl $%d, %s\n", pr, rnames[ESP]);
		break;
#endif

	case 'D': /* Long long comparision */
		twollcomp(p);
		break;

#if 0
	case 'E': /* Assign to bitfield */
		bfasg(p);
		break;

	case 'F': /* Structure argument */
		if (p->n_stalign != 0) /* already on stack */
			starg(p);
		break;

	case 'G': /* Floating point compare */
		fcomp(p);
		break;

	case 'J': /* convert unsigned long long to floating point */
		ulltofp(p);
		break;

	case 'N': /* output extended reg name */
		printf("%s", rnames[getlr(p, '1')->n_rval]);
		break;
#endif

	case 'O': /* 64-bit left and right shift operators */

		if (p->n_op == LS && p->n_right->n_lval < 32) {
			expand(p, INBREG, "\tsrwi A1,AL,32-AR        ; 64-bit left-shift\n");
			expand(p, INBREG, "\tslwi U1,UL,AR\n");
			expand(p, INBREG, "\tor U1,U1,A1\n");
			expand(p, INBREG, "\tslwi A1,AL,AR\n");
		} else if (p->n_op == LS) {
			expand(p, INBREG, "\tli A1,0	; 64-bit left-shift\n");
			expand(p, INBREG, "\tslwi U1,AL,AR-32\n");
		} else if (p->n_op == RS && p->n_right->n_lval < 32) {
			expand(p, INBREG, "\tslwi U1,UL,32-AR        ; 64-bit right-shift\n");
			expand(p, INBREG, "\tsrwi A1,AL,AR\n");
			expand(p, INBREG, "\tor A1,A1,U1\n");
			expand(p, INBREG, "\tsrwi U1,UL,AR\n");
		} else if (p->n_op == RS) {
			expand(p, INBREG, "\tli U1,0	; 64-bit right-shift\n");
			expand(p, INBREG, "\tsrwi A1,UL,AR-32\n");
		}
		break;


#if 0
	case 'O': /* print out emulated ops */
		pr = 16;
		if (p->n_op == RS || p->n_op == LS) {
			expand(p, INAREG, "\tpushl AR\n");
			pr = 12;
		} else
			expand(p, INCREG, "\tpushl UR\n\tpushl AR\n");
		expand(p, INCREG, "\tpushl UL\n\tpushl AL\n");
		if (p->n_op == DIV && p->n_type == ULONGLONG) ch = "udiv";
		else if (p->n_op == DIV) ch = "div";
		else if (p->n_op == MUL) ch = "mul";
		else if (p->n_op == MOD && p->n_type == ULONGLONG) ch = "umod";
		else if (p->n_op == MOD) ch = "mod";
		else if (p->n_op == RS && p->n_type == ULONGLONG) ch = "lshr";
		else if (p->n_op == RS) ch = "ashr";
		else if (p->n_op == LS) ch = "ashl";
		else ch = 0, comperr("ZO");
		printf("\tbl __%sdi3\n\n", ch,);
                break;
#endif

#if 0
	case 'P': /* push hidden argument on stack */
		r = (NODE *)p->n_sue;
		printf("\tleal -%d(%%ebp),", stkpos);
		adrput(stdout, getlr(p, '1'));
		printf("\n\tpushl ");
		adrput(stdout, getlr(p, '1'));
		putchar('\n');
		break;
#endif

	case 'Q': /* emit struct assign */
		if (p->n_stsize == 4) {
			int sz = sizeof inst;

			snprintf(inst, sz, "\tlwz %s,0(AR)\n", rnames[R5]);
			expand(p, INAREG, inst);
			snprintf(inst, sz, "\tstw %s,AL\n", rnames[R5]);
			expand(p, INAREG, inst);
			return;
		}

		if ((p->n_stsize & ~0xffff) != 0) {
			printf("\tlis %s,%d\n", rnames[R5], p->n_stsize & 0xffff);
			printf("\taddis %s,%s,%d\n", rnames[R5], rnames[R5], (p->n_stsize >> 16) & 0xffff);
		} else {
			printf("\tli %s,%d\n", rnames[R5], p->n_stsize);
		}
		printf("\taddi %s,%s,%lld\n", rnames[R3], rnames[p->n_left->n_rval], p->n_left->n_lval);
		printf("\tbl %s%s\n", exname("memcpy"),
		    kflag ? "$stub" : "");
		addstub(&stublist, exname("memcpy"));
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

int canaddr(NODE *);
int
canaddr(NODE *p)
{
	int o = p->n_op;

	if (o==NAME || o==REG || o==ICON || o==OREG ||
	    (o==UMUL && shumul(p->n_left)))
		return(1);
	return(0);
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
	printf("; shtemp\n");
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
	printf( CONFMT, val);
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
			if (kflag && p->n_sp && ISFTN(p->n_sp->stype)) {
				if (p->n_sp && p->n_sp->sclass == EXTERN) {
					fprintf(fp, "%s$stub", s);
					addstub(&stublist, s);
				} else {
					fprintf(fp, "%s", s);
				}
			} else if (kflag) {
				if (p->n_sp && p->n_sp->sclass == EXTERN) {
					fprintf(fp, "L%s$non_lazy_ptr", s);
					addstub(&nlplist, s);
				} else {
					fprintf(fp, "%s", s);
				}
				fprintf(fp, "-L%s$pb", exname(funcname));
			} else {
				fprintf(fp, "%s", s);
			}
			if (val > 0)
				fprintf(fp, "+%d", val);
			else if (val < 0)
				fprintf(fp, "-%d", -val);
		} else
			fprintf(fp, "%d", val);
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
 * Print lower or upper name of 64-bit register.
 */
static void
reg64name(int rval, int hi)
{
	int off = 3 * (hi != 0);

#ifdef ELFABI
	fputc('%', stdout);
#endif

	fprintf(stdout, "%c%c",
		 rnames[rval][off],
		 rnames[rval][off + 1]);
	if (rnames[rval][off + 2])
		fputc(rnames[rval][off + 2], stdout);
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
		reg64name(p->n_rval, 1);
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
			if (kflag && p->n_sp && (p->n_sp->sclass == EXTERN || p->n_sp->sclass == EXTDEF)) {
				fprintf(io, "L%s$non_lazy_ptr", exname(p->n_name));
				addstub(&nlplist, exname(p->n_name));
				fprintf(io, "-L%s$pb", exname(funcname));
			} else if (kflag && p->n_sp && p->n_sp->sclass == STATIC && p->n_sp->slevel == 0) {
				fprintf(io, "%s", exname(p->n_name));
				fprintf(io, "-L%s$pb", exname(funcname));
			} else if (kflag && p->n_sp && (p->n_sp->sclass == ILABEL || (p->n_sp->sclass == STATIC && p->n_sp->sclass > 0))) {
				fprintf(io, "%s", p->n_name);
				fprintf(io, "-L%s$pb", exname(funcname));
			} else {
				fputs(p->n_name, io);
			}
			if (p->n_lval != 0)
				fprintf(io, "+" CONFMT, p->n_lval);
		} else
			fprintf(io, CONFMT, p->n_lval);
		return;

	case OREG:
		r = p->n_rval;
		fprintf(io, "%d", (int)p->n_lval);
		fprintf(io, "(%s)", rnames[p->n_rval]);
		return;

	case ICON:
		/* addressable value of the constant */
		conput(io, p);
		return;

	case MOVE:
	case REG:
		switch (p->n_type) {
		case LONGLONG:
		case ULONGLONG:
			reg64name(p->n_rval, 0);
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
	"ble",		/* branch if less-than-or-equal */
	"blt",		/* branch if less-than */
	"bge",		/* branch if greater-than-or-equal */
	"bgt",		/* branch if greater-than */

};


/*   printf conditional and unconditional branches */
void
cbgen(int o, int lab)
{
	if (o < EQ || o > UGT)
		comperr("bad conditional branch: %s", opst[o]);
	printf("\t%s " LABFMT "\n", ccbranches[o-EQ], lab);
}

static void
fixcalls(NODE *p)
{
	/* Prepare for struct return by allocating bounce space on stack */
	switch (p->n_op) {
	case STCALL:
	case USTCALL:
		if (p->n_stsize+p2autooff > stkpos)
			stkpos = p->n_stsize+p2autooff;
		break;
	}
}

/*
 * Must store floats in memory if there are two function calls involved.
 */
static int
storefloat(struct interpass *ip, NODE *p)
{
	int l, r;

	switch (optype(p->n_op)) {
	case BITYPE:
		l = storefloat(ip, p->n_left);
		r = storefloat(ip, p->n_right);
		if (p->n_op == CM)
			return 0; /* arguments, don't care */
		if (callop(p->n_op))
			return 1; /* found one */
#define ISF(p) ((p)->n_type == FLOAT || (p)->n_type == DOUBLE || \
	(p)->n_type == LDOUBLE)
		if (ISF(p->n_left) && ISF(p->n_right) && l && r) {
			/* must store one. store left */
			struct interpass *nip;
			TWORD t = p->n_left->n_type;
			NODE *ll;
			int off;

                	off = BITOOR(freetemp(szty(t)));
                	ll = mklnode(OREG, off, FPREG, t);
			nip = ipnode(mkbinode(ASSIGN, ll, p->n_left, t));
			p->n_left = mklnode(OREG, off, FPREG, t);
                	DLIST_INSERT_BEFORE(ip, nip, qelem);
		}
		return l|r;

	case UTYPE:
		l = storefloat(ip, p->n_left);
		if (callop(p->n_op))
			l = 1;
		return l;
	default:
		return 0;
	}
}

void
myreader(struct interpass *ipole)
{
	struct interpass *ip;

	stkpos = p2autooff;
	DLIST_FOREACH(ip, ipole, qelem) {
		if (ip->type != IP_NODE)
			continue;
		walkf(ip->ip_node, fixcalls);
		storefloat(ip, ip->ip_node);
	}
	if (stkpos > p2autooff)
		p2autooff = stkpos;
	if (stkpos > p2maxautooff)
		p2maxautooff = stkpos;
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
		if (p->n_type == (PTR|SHORT) || p->n_type == (PTR|USHORT)) {
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
myoptim(struct interpass *ip)
{
#ifdef PCC_DEBUG
	if (x2debug) {
		printf("myoptim\n");
	}
#endif
}

/*
 * Move data between registers.  While basic registers aren't a problem,
 * we have to handle the special case of overlapping composite registers.
 * It might just be easier to modify the register allocator so that
 * moves between overlapping registers isn't possible.
 */
void
rmove(int s, int d, TWORD t)
{
	switch (t) {
	case LONGLONG:
	case ULONGLONG:
		if (s == d+1) {
			/* dh = sl, copy low word first */
			printf("\tmr ");
			reg64name(d,0);
			printf(",");
			reg64name(s,0);
			printf("\n");
			printf("\tmr ");
			reg64name(d,1);
			printf(",");
			reg64name(s,1);
			printf("\n");
		} else {
			/* copy high word first */
			printf("\tmr ");
			reg64name(d,1);
			printf(",");
			reg64name(s,1);
			printf("\n");
			printf("\tmr ");
			reg64name(d,0);
			printf(",");
			reg64name(s,0);
			printf("\n");
		}
		break;
	case LDOUBLE:
#ifdef notdef
		/* a=b()*c(); will generate this */
		comperr("bad float rmove: %d %d", s, d);
#endif
		break;
	default:
		printf("\tmr %s,%s\n", rnames[d], rnames[s]);
	}
}

/*
 * For class c, find worst-case displacement of the number of
 * registers in the array r[] indexed by class.
 *
 * On PowerPC, we have:
 *
 * 32 32-bit registers (2 reserved)
 * 16 64-bit pseudo registers
 * 32 floating-point registers
 */
int
COLORMAP(int c, int *r)
{
	int num = 0;

	switch (c) {
	case CLASSA:
		num += r[CLASSA];
		num += 2*r[CLASSB];
		return num < 30;
	case CLASSB:
		num += 2*r[CLASSB];
		num += r[CLASSA];
		return num < 16;
	}
	assert(0);
	return 0; /* XXX gcc */
}

#ifdef ELFABI
char *rnames[] = {
	"%r0", "%r1", "%r2", "%r3","%r4","%r5", "%r6", "%r7", "%r8",
	"%r9", "%r10", "%r11", "%r12", "%r13", "%r14", "%r15", "%r16",
	"%r17", "%r18", "%r19", "%r20", "%r21", "%r22", "%r23", "%r24",
	"%r25", "%r26", "%r27", "%r28", "%r29", "%r30", "%r31",
	/* the order is flipped, because we are big endian */
	"r4\0r3\0", "r5\0r4\0", "r6\0r5\0", "r7\0r6\0",
	"r8\0r7\0", "r9\0r8\0", "r10r9\0", "r15r14", "r17r16",
	"r19r18", "r21r20", "r23r22", "r25r24", "r27r26",
	"r29r28", "r31r30",
};
#else
char *rnames[] = {
	"r0", "r1", "r2", "r3","r4","r5", "r6", "r7", "r8",
	"r9", "r10", "r11", "r12", "r13", "r14", "r15", "r16",
	"r17", "r18", "r19", "r20", "r21", "r22", "r23", "r24",
	"r25", "r26", "r27", "r28", "r29", "r30", "r31",
	/* the order is flipped, because we are big endian */
	"r4\0r3\0", "r5\0r4\0", "r6\0r5\0", "r7\0r6\0",
	"r8\0r7\0", "r9\0r8\0", "r10r9\0", "r15r14", "r17r16",
	"r19r18", "r21r20", "r23r22", "r25r24", "r27r26",
	"r29r28", "r31r30",
};
#endif

/*
 * Return a class suitable for a specific type.
 */
int
gclass(TWORD t)
{
	if (t == FLOAT || t == DOUBLE || t == LDOUBLE)
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

#ifdef PCC_DEBUG
	if (x2debug)
		printf("lastcall:\n");
#endif

	p->n_qual = 0;
	if (p->n_op != CALL && p->n_op != FORTCALL && p->n_op != STCALL)
		return;
	for (p = p->n_right; p->n_op == CM; p = p->n_left)
		size += argsiz(p->n_right);
	size += argsiz(p);
	op->n_qual = size; /* XXX */
}

/*
 * Special shapes.
 */
int
special(NODE *p, int shape)
{
	int o = p->n_op;

	switch (shape) {
	case SFUNCALL:
		if (o == STCALL || o == USTCALL)
			return SRREG;
		break;
	case SPCON:
		if (o == ICON && p->n_name[0] == 0 && (p->n_lval & ~0xffff) == 0)
			return SRDIR;
		break;
	}
	return SRNOPE;
}

/*
 * Target-dependent command-line options.
 */
void
mflags(char *str)
{
}
