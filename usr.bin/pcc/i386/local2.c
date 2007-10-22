/*	$OpenBSD: local2.c,v 1.1 2007/10/22 13:39:37 otto Exp $	*/
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

# include "pass2.h"
# include <ctype.h>
# include <string.h>

void acon(NODE *p);
int argsize(NODE *p);

static int stkpos;

void
lineid(int l, char *fn)
{
	/* identify line l and file fn */
	printf("#	line %d, file %s\n", l, fn);
}

void
deflab(int label)
{
	printf(LABFMT ":\n", label);
}

static int regoff[7];
static TWORD ftype;

/*
 * Print out the prolog assembler.
 * addto and regoff are already calculated.
 */
static void
prtprolog(struct interpass_prolog *ipp, int addto)
{
	int i, j;

	printf("	pushl %%ebp\n");
	printf("	movl %%esp,%%ebp\n");
	if (addto)
		printf("	subl $%d,%%esp\n", addto);
	for (i = ipp->ipp_regs, j = 0; i; i >>= 1, j++)
		if (i & 1)
			fprintf(stdout, "	movl %s,-%d(%s)\n",
			    rnames[j], regoff[j], rnames[FPREG]);
}

/*
 * calculate stack size and offsets
 */
static int
offcalc(struct interpass_prolog *ipp)
{
	int i, j, addto;

	addto = p2maxautooff;
	if (addto >= AUTOINIT/SZCHAR)
		addto -= AUTOINIT/SZCHAR;
	for (i = ipp->ipp_regs, j = 0; i ; i >>= 1, j++) {
		if (i & 1) {
			addto += SZINT/SZCHAR;
			regoff[j] = addto;
		}
	}
	return addto;
}

void
prologue(struct interpass_prolog *ipp)
{
	int addto;

	ftype = ipp->ipp_type;
	if (ipp->ipp_vis)
		printf("	.globl %s\n", ipp->ipp_name);
	printf("	.align 4\n");
	printf("%s:\n", ipp->ipp_name);
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

	if (ipp->ipp_ip.ip_lbl == 0)
		return; /* no code needs to be generated */

	/* return from function code */
	for (i = ipp->ipp_regs, j = 0; i ; i >>= 1, j++) {
		if (i & 1)
			fprintf(stdout, "	movl -%d(%s),%s\n",
			    regoff[j], rnames[FPREG], rnames[j]);
			
	}

	/* struct return needs special treatment */
	if (ftype == STRTY || ftype == UNIONTY) {
		printf("	movl 8(%%ebp),%%eax\n");
		printf("	leave\n");
		printf("	ret $4\n");
	} else {
		printf("	leave\n");
		printf("	ret\n");
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
		str = "add";
		break;
	case MINUS:
		str = "sub";
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
tlen(p) NODE *p;
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
	expand(p, 0, "	cmpl UR,UL\n");
	if (cb1) cbgen(cb1, s);
	if (cb2) cbgen(cb2, e);
	expand(p, 0, "	cmpl AR,AL\n");
	cbgen(p->n_op, e);
	deflab(s);
}

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
	NODE *r, *l;
	int pr, lr, s;
	char *ch;

	switch (c) {
	case 'A': /* swap st0 and st1 if right is evaluated second */
		if ((p->n_su & DORIGHT) == 0) {
			if (logop(p->n_op))
				printf("	fxch\n");
			else
				printf("r");
		}
		break;

	case 'C':  /* remove from stack after subroutine call */
		pr = p->n_qual;
		if (p->n_op == STCALL || p->n_op == USTCALL)
			pr += 4;
		if (p->n_op == UCALL)
			return; /* XXX remove ZC from UCALL */
		if (pr)
			printf("	addl $%d, %s\n", pr, rnames[ESP]);
		break;

	case 'D': /* Long long comparision */
		twollcomp(p);
		break;

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

	case 'M': /* Output sconv move, if needed */
		l = getlr(p, 'L');
		/* XXX fixneed: regnum */
		pr = DECRA(p->n_reg, 0);
		lr = DECRA(l->n_reg, 0);
		if ((pr == AL && lr == EAX) || (pr == BL && lr == EBX) ||
		    (pr == CL && lr == ECX) || (pr == DL && lr == EDX))
			;
		else
			printf("	movb %%%cl,%s\n",
			    rnames[lr][2], rnames[pr]);
		l->n_rval = l->n_reg = p->n_reg; /* XXX - not pretty */
		break;

	case 'N': /* output extended reg name */
		printf("%s", rnames[getlr(p, '1')->n_rval]);
		break;

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
		printf("\tcall __%sdi3\n\taddl $%d,%s\n", ch, pr, rnames[ESP]);
                break;

	case 'P': /* push hidden argument on stack */
		r = (NODE *)p->n_sue;
		printf("\tleal -%d(%%ebp),", stkpos);
		adrput(stdout, getlr(p, '1'));
		printf("\n\tpushl ");
		adrput(stdout, getlr(p, '1'));
		putchar('\n');
		break;

	case 'Q': /* emit struct assign */
		/* XXX - optimize for small structs */
		printf("\tpushl $%d\n", p->n_stsize);
		expand(p, INAREG, "\tpushl AR\n");
		expand(p, INAREG, "\tleal AL,%eax\n\tpushl %eax\n");
		printf("\tcall memcpy\n");
		printf("\taddl $12,%%esp\n");
		break;

	case 'S': /* emit eventual move after cast from longlong */
		pr = DECRA(p->n_reg, 0);
		lr = p->n_left->n_rval;
		switch (p->n_type) {
		case CHAR:
		case UCHAR:
			if (rnames[pr][2] == 'l' && rnames[lr][2] == 'x' &&
			    rnames[pr][1] == rnames[lr][1])
				break;
			if (rnames[lr][2] == 'x') {
				printf("\tmovb %%%cl,%s\n",
				    rnames[lr][1], rnames[pr]);
				break;
			}
			/* Must go via stack */
			s = BITOOR(freetemp(1));
			printf("\tmovl %%e%ci,%d(%%ebp)\n", rnames[lr][1], s);
			printf("\tmovb %d(%%ebp),%s\n", s, rnames[pr]);
//			comperr("SCONV1 %s->%s", rnames[lr], rnames[pr]);
			break;

		case SHORT:
		case USHORT:
			if (rnames[lr][1] == rnames[pr][2] &&
			    rnames[lr][2] == rnames[pr][3])
				break;
			printf("\tmovw %%%c%c,%%%s\n",
			    rnames[lr][1], rnames[lr][2], rnames[pr]+2);
			break;
		case INT:
		case UNSIGNED:
			if (rnames[lr][1] == rnames[pr][2] &&
			    rnames[lr][2] == rnames[pr][3])
				break;
			printf("\tmovl %%e%c%c,%s\n",
				    rnames[lr][1], rnames[lr][2], rnames[pr]);
			break;

		default:
			if (rnames[lr][1] == rnames[pr][2] &&
			    rnames[lr][2] == rnames[pr][3])
				break;
			comperr("SCONV2 %s->%s", rnames[lr], rnames[pr]);
			break;
		}
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
	printf("$" CONFMT, val);
}

void
conput(FILE *fp, NODE *p)
{
	int val = p->n_lval;

	switch (p->n_op) {
	case ICON:
		if (p->n_name[0] != '\0') {
			fprintf(fp, "%s", p->n_name);
			if (val)
				fprintf(fp, "+%d", val);
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
 * Write out the upper address, like the upper register of a 2-register
 * reference, or the next memory location.
 */
void
upput(NODE *p, int size)
{

	size /= SZCHAR;
	switch (p->n_op) {
	case REG:
		fprintf(stdout, "%%%s", &rnames[p->n_rval][3]);
		break;

	case NAME:
	case OREG:
		p->n_lval += size;
		adrput(stdout, p);
		p->n_lval -= size;
		break;
	case ICON:
		fprintf(stdout, "$" CONFMT, p->n_lval >> 32);
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
				fprintf(io, "+" CONFMT, p->n_lval);
		} else
			fprintf(io, CONFMT, p->n_lval);
		return;

	case OREG:
		r = p->n_rval;
		if (p->n_lval)
			fprintf(io, "%d", (int)p->n_lval);
		if (R2TEST(r)) {
			fprintf(io, "(%s,%s,4)", rnames[R2UPK1(r)],
			    rnames[R2UPK2(r)]);
		} else
			fprintf(io, "(%s)", rnames[p->n_rval]);
		return;
	case ICON:
		/* addressable value of the constant */
		fputc('$', io);
		conput(io, p);
		return;

	case MOVE:
	case REG:
		switch (p->n_type) {
		case LONGLONG:
		case ULONGLONG:
			fprintf(io, "%%%c%c%c", rnames[p->n_rval][0],
			    rnames[p->n_rval][1], rnames[p->n_rval][2]);
			break;
		case SHORT:
		case USHORT:
			fprintf(io, "%%%s", &rnames[p->n_rval][2]);
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

static char *
ccbranches[] = {
	"je",		/* jumpe */
	"jne",		/* jumpn */
	"jle",		/* jumple */
	"jl",		/* jumpl */
	"jge",		/* jumpge */
	"jg",		/* jumpg */
	"jbe",		/* jumple (jlequ) */
	"jb",		/* jumpl (jlssu) */
	"jae",		/* jumpge (jgequ) */
	"ja",		/* jumpg (jgtru) */
};


/*   printf conditional and unconditional branches */
void
cbgen(int o, int lab)
{
	if (o < EQ || o > UGT)
		comperr("bad conditional branch: %s", opst[o]);
	printf("	%s " LABFMT "\n", ccbranches[o-EQ], lab);
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
}

static char rl[] =
  { EAX, EAX, EAX, EAX, EAX, EDX, EDX, EDX, EDX, ECX, ECX, ECX, EBX, EBX, ESI };
static char rh[] =
  { EDX, ECX, EBX, ESI, EDI, ECX, EBX, ESI, EDI, EBX, ESI, EDI, ESI, EDI, EDI };

void
rmove(int s, int d, TWORD t)
{
	int sl, sh, dl, dh;

	switch (t) {
	case LONGLONG:
	case ULONGLONG:
#if 1
		sl = rl[s-EAXEDX];
		sh = rh[s-EAXEDX];
		dl = rl[d-EAXEDX];
		dh = rh[d-EAXEDX];

		/* sanity checks, remove when satisfied */
		if (memcmp(rnames[s], rnames[sl]+1, 3) != 0 ||
		    memcmp(rnames[s]+3, rnames[sh]+1, 3) != 0)
			comperr("rmove source error");
		if (memcmp(rnames[d], rnames[dl]+1, 3) != 0 ||
		    memcmp(rnames[d]+3, rnames[dh]+1, 3) != 0)
			comperr("rmove dest error");
#define	SW(x,y) { int i = x; x = y; y = i; }
		if (sl == dh || sh == dl) {
			/* Swap if moving to itself */
			SW(sl, sh);
			SW(dl, dh);
		}
		if (sl != dl)
			printf("	movl %s,%s\n", rnames[sl], rnames[dl]);
		if (sh != dh)
			printf("	movl %s,%s\n", rnames[sh], rnames[dh]);
#else
		if (memcmp(rnames[s], rnames[d], 3) != 0)
			printf("	movl %%%c%c%c,%%%c%c%c\n",
			    rnames[s][0],rnames[s][1],rnames[s][2],
			    rnames[d][0],rnames[d][1],rnames[d][2]);
		if (memcmp(&rnames[s][3], &rnames[d][3], 3) != 0)
			printf("	movl %%%c%c%c,%%%c%c%c\n",
			    rnames[s][3],rnames[s][4],rnames[s][5],
			    rnames[d][3],rnames[d][4],rnames[d][5]);
#endif
		break;
	case CHAR:
	case UCHAR:
		printf("	movb %s,%s\n", rnames[s], rnames[d]);
		break;
	case FLOAT:
	case DOUBLE:
	case LDOUBLE:
#ifdef notdef
		/* a=b()*c(); will generate this */
		comperr("bad float rmove: %d %d", s, d);
#endif
		break;
	default:
		printf("	movl %s,%s\n", rnames[s], rnames[d]);
	}
}

/*
 * For class c, find worst-case displacement of the number of
 * registers in the array r[] indexed by class.
 */
int
COLORMAP(int c, int *r)
{
	int num;

	switch (c) {
	case CLASSA:
		num = r[CLASSB] > 4 ? 4 : r[CLASSB];
		num += 2*r[CLASSC];
		num += r[CLASSA];
		return num < 6;
	case CLASSB:
		num = r[CLASSA];
		num += 2*r[CLASSC];
		num += r[CLASSB];
		return num < 4;
	case CLASSC:
		num = r[CLASSA];
		num += r[CLASSB] > 4 ? 4 : r[CLASSB];
		num += 2*r[CLASSC];
		return num < 5;
	case CLASSD:
		return r[CLASSD] < DREGCNT;
	}
	return 0; /* XXX gcc */
}

char *rnames[] = {
	"%eax", "%edx", "%ecx", "%ebx", "%esi", "%edi", "%ebp", "%esp",
	"%al", "%ah", "%dl", "%dh", "%cl", "%ch", "%bl", "%bh",
	"eaxedx", "eaxecx", "eaxebx", "eaxesi", "eaxedi", "edxecx",
	"edxebx", "edxesi", "edxedi", "ecxebx", "ecxesi", "ecxedi",
	"ebxesi", "ebxedi", "esiedi",
	"%st0", "%st1", "%st2", "%st3", "%st4", "%st5", "%st6", "%st7",
};

/*
 * Return a class suitable for a specific type.
 */
int
gclass(TWORD t)
{
	if (t == CHAR || t == UCHAR)
		return CLASSB;
	if (t == LONGLONG || t == ULONGLONG)
		return CLASSC;
	if (t == FLOAT || t == DOUBLE || t == LDOUBLE)
		return CLASSD;
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
		if (o != ICON || p->n_name[0] ||
		    p->n_lval < 0 || p->n_lval > 0x7fffffff)
			break;
		return SRDIR;
	case SMIXOR:
		return tshape(p, SZERO);
	case SMILWXOR:
		if (o != ICON || p->n_name[0] ||
		    p->n_lval == 0 || p->n_lval & 0xffffffff)
			break;
		return SRDIR;
	case SMIHWXOR:
		if (o != ICON || p->n_name[0] ||
		     p->n_lval == 0 || (p->n_lval >> 32) != 0)
			break;
		return SRDIR;
	}
	return SRNOPE;
}
