/*	$OpenBSD: local2.c,v 1.1 2007/10/07 17:58:52 otto Exp $	*/
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

# define putstr(s)	fputs((s), stdout)

void acon(FILE *, NODE *p);
int argsize(NODE *p);
void genargs(NODE *p);

static int ftlab1, ftlab2;
static int offlab;
int offarg;

void
lineid(int l, char *fn)
{
	/* identify line l and file fn */
	printf("#	line %d, file %s\n", l, fn);
}

void
defname(char *name, int visib)
{
	if (visib)
		printf("	.globl %s\n", name);
	printf("%s:\n", name);
}

void
deflab(int label)
{
	printf(LABFMT ":\n", label);
}

static int isoptim;

void
prologue(int regs, int autos)
{
	int i, addto;

	offlab = getlab();
	if (regs < 0 || autos < 0) {
		/*
		 * non-optimized code, jump to epilogue for code generation.
		 */
		ftlab1 = getlab();
		ftlab2 = getlab();
		printf("	jrst L%d\n", ftlab1);
		printf("L%d:\n", ftlab2);
	} else {
		/*
		 * We here know what register to save and how much to 
		 * add to the stack.
		 */
		autos = autos + (SZINT-1);
		addto = (autos - AUTOINIT)/SZINT + (MAXRVAR-regs);
		if (addto || gflag) {
			printf("	push %s,%s\n",rnames[017], rnames[016]);
			printf("	move %s,%s\n", rnames[016],rnames[017]);
			for (i = regs; i < MAXRVAR; i++) {
				int db = ((i+1) < MAXRVAR);
				printf("	%smovem %s,0%o(%s)\n",
				    db ? "d" : "",
				    rnames[i+1], i+1-regs, rnames[016]);
				if (db)
					i++;
			}
			if (addto)
				printf("	addi %s,0%o\n", rnames[017], addto);
		} else
			offarg = 1;
		isoptim = 1;
	}
}

/*
 * End of block.
 */
void
eoftn(int regs, int autos, int retlab)
{
	register OFFSZ spoff;	/* offset from stack pointer */
	int i;

	spoff = autos + (SZINT-1);
	if (spoff >= AUTOINIT)
		spoff -= AUTOINIT;
	spoff /= SZINT;
	/* return from function code */
	printf("L%d:\n", retlab);
	if (gflag || isoptim == 0 || autos != AUTOINIT || regs != MAXRVAR) {
		for (i = regs; i < MAXRVAR; i++) {
			int db = ((i+1) < MAXRVAR);
			printf("	%smove %s,0%o(%s)\n", db ? "d" : "",
			    rnames[i+1], i+1-regs, rnames[016]);
			if (db)
				i++;
		}
		printf("	move %s,%s\n", rnames[017], rnames[016]);
		printf("	pop %s,%s\n", rnames[017], rnames[016]);
	}
	printf("	popj %s,\n", rnames[017]);

	/* Prolog code */
	if (isoptim == 0) {
		printf("L%d:\n", ftlab1);
		printf("	push %s,%s\n", rnames[017], rnames[016]);
		printf("	move %s,%s\n", rnames[016], rnames[017]);
		for (i = regs; i < MAXRVAR; i++) {
			int db = ((i+1) < MAXRVAR);
			printf("	%smovem %s,0%o(%s)\n", db ? "d" : "",
			    rnames[i+1], i+1-regs, rnames[016]);
			spoff++;
			if (db)
				i++, spoff++;
		}
		if (spoff)
			printf("	addi %s,0%llo\n", rnames[017], spoff);
		printf("	jrst L%d\n", ftlab2);
	}
	printf("	.set " LABFMT ",0%o\n", offlab, MAXRVAR-regs);
	offarg = isoptim = 0;
}

static char *loctbl[] = { "text", "data", "data", "text", "text", "stab" };

void
setlocc(int locctr)
{
	static int lastloc;

	if (locctr == lastloc)
		return;

	lastloc = locctr;
	printf("	.%s\n", loctbl[locctr]);
}

/*
 * add/sub/...
 *
 * Param given:
 *	R - Register
 *	M - Memory
 *	C - Constant
 */
void
hopcode(int f, int o)
{
	cerror("hopcode: f %d %d", f, o);
}

char *
rnames[] = {  /* keyed to register number tokens */
	"%0", "%1", "%2", "%3", "%4", "%5", "%6", "%7",
	"%10", "%11", "%12", "%13", "%14", "%15", "%16", "%17",
};

int rstatus[] = {
	0, STAREG, STAREG, STAREG, STAREG, STAREG, STAREG, STAREG,
	SAREG, SAREG, SAREG, SAREG, SAREG, SAREG, 0, 0,
};

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
				cerror("tlen type %d not pointer");
			return SZPOINT/SZCHAR;
		}
}

static char *
binskip[] = {
	"e",	/* jumpe */
	"n",	/* jumpn */
	"le",	/* jumple */
	"l",	/* jumpl */
	"ge",	/* jumpge */
	"g",	/* jumpg */
};

/*
 * Extract the higher 36 bits from a longlong.
 */
static CONSZ
gethval(CONSZ lval)
{
	CONSZ hval = (lval >> 35) & 03777777777LL;

	if ((hval & 03000000000LL) == 03000000000LL) {
		hval |= 0777000000000LL;
	} else if ((hval & 03000000000LL) == 02000000000LL) {
		hval &= 01777777777LL;
		hval |= 0400000000000LL;
	}
	return hval;
}

/*
 * Do a binary comparision, and jump accordingly.
 */
static void
twocomp(NODE *p)
{
	int o = p->n_op;
	extern int negrel[];
	int isscon = 0, iscon = p->n_right->n_op == ICON;

	if (o < EQ || o > GT)
		cerror("bad binary conditional branch: %s", opst[o]);

	if (iscon && p->n_right->n_name[0] != 0) {
		printf("	cam%s ", binskip[negrel[o-EQ]-EQ]);
		adrput(stdout, getlr(p, 'L'));
		putchar(',');
		printf("[ .long ");
		adrput(stdout, getlr(p, 'R'));
		putchar(']');
		printf("\n	jrst L%d\n", p->n_label);
		return;
	}
	if (iscon)
		isscon = p->n_right->n_lval >= 0 &&
		    p->n_right->n_lval < 01000000;

	printf("	ca%c%s ", iscon && isscon ? 'i' : 'm',
	    binskip[negrel[o-EQ]-EQ]);
	adrput(stdout, getlr(p, 'L'));
	putchar(',');
	if (iscon && (isscon == 0)) {
		printf("[ .long ");
		adrput(stdout, getlr(p, 'R'));
		putchar(']');
	} else
		adrput(stdout, getlr(p, 'R'));
	printf("\n	jrst L%d\n", p->n_label);
}

/*
 * Compare byte/word pointers.
 * XXX - do not work for highest bit set in address
 */
static void
ptrcomp(NODE *p)
{
	printf("	rot "); adrput(stdout, getlr(p, 'L')); printf(",6\n");
	printf("	rot "); adrput(stdout, getlr(p, 'R')); printf(",6\n");
	twocomp(p);
}

/*
 * Do a binary comparision of two long long, and jump accordingly.
 * XXX - can optimize for constants.
 */
static void     
twollcomp(NODE *p)
{       
	int o = p->n_op;
	int iscon = p->n_right->n_op == ICON;
	int m;

	if (o < EQ || o > GT)
		cerror("bad long long conditional branch: %s", opst[o]);

	/* Special strategy for equal/not equal */
	if (o == EQ || o == NE) {
		if (o == EQ)
			m = getlab();
		printf("	came ");
		upput(getlr(p, 'L'), SZLONG);
		putchar(',');
		if (iscon)
			printf("[ .long ");
		upput(getlr(p, 'R'), SZLONG);
		if (iscon)
			putchar(']');
		printf("\n	jrst L%d\n", o == EQ ? m : p->n_label);
		printf("	cam%c ", o == EQ ? 'n' : 'e');
		adrput(stdout, getlr(p, 'L'));
		putchar(',');
		if (iscon)
			printf("[ .long ");
		adrput(stdout, getlr(p, 'R'));
		if (iscon)
			putchar(']');
		printf("\n	jrst L%d\n", p->n_label);
		if (o == EQ)
			printf("L%d:\n", m);
		return;
	}
	/* First test highword */
	printf("	cam%ce ", o == GT || o == GE ? 'l' : 'g');
	adrput(stdout, getlr(p, 'L'));
	putchar(',');
	if (iscon)
		printf("[ .long ");
	adrput(stdout, getlr(p, 'R'));
	if (iscon)
		putchar(']');
	printf("\n	jrst L%d\n", p->n_label);

	/* Test equality */
	printf("	came ");
	adrput(stdout, getlr(p, 'L'));
	putchar(',');
	if (iscon)
		printf("[ .long ");
	adrput(stdout, getlr(p, 'R'));
	if (iscon)
		putchar(']');
	printf("\n	jrst L%d\n", m = getlab());

	/* Test lowword. Only works with pdp10 format for longlongs */
	printf("	cam%c%c ", o == GT || o == GE ? 'l' : 'g',
	    o == LT || o == GT ? 'e' : ' ');
	upput(getlr(p, 'L'), SZLONG);
	putchar(',');
	if (iscon)  
		printf("[ .long ");
	upput(getlr(p, 'R'), SZLONG);
	if (iscon)
		putchar(']');
	printf("\n	jrst L%d\n", p->n_label);
	printf("L%d:\n", m);
}

/*
 * Print the correct instruction for constants.
 */
static void
constput(NODE *p)
{
	CONSZ val = p->n_right->n_lval;
	int reg = p->n_left->n_rval;

	/* Only numeric constant */
	if (p->n_right->n_name[0] == '\0') {
		if (val == 0) {
			printf("movei %s,0", rnames[reg]);
		} else if ((val & 0777777000000LL) == 0) {
			printf("movei %s,0%llo", rnames[reg], val);
		} else if ((val & 0777777) == 0) {
			printf("hrlzi %s,0%llo", rnames[reg], val >> 18);
		} else {
			printf("move %s,[ .long 0%llo]", rnames[reg],
			    szty(p->n_right->n_type) > 1 ? val :
			    val & 0777777777777LL);
		}
		/* Can have more tests here, hrloi etc */
		return;
	} else {
		printf("xmovei %s,%s", rnames[reg], p->n_right->n_name);
		if (val)
			printf("+" CONFMT, val);
	}
}

/*
 * Return true if the constant can be bundled in an instruction (immediate).
 */
static int
oneinstr(NODE *p)
{
	if (p->n_name[0] != '\0')
		return 0;
	if ((p->n_lval & 0777777000000ULL) != 0)
		return 0;
	return 1;
}

/*
 * Emit a halfword or byte instruction, from OREG to REG.
 * Sign extension must also be done here.
 */
static void
emitshort(NODE *p)
{
	CONSZ off = p->n_lval;
	TWORD type = p->n_type;
	int reg = p->n_rval;
	int issigned = !ISUNSIGNED(type);
	int ischar = type == CHAR || type == UCHAR;
	int reg1 = getlr(p, '1')->n_rval;

	if (off < 0) { /* argument, use move instead */
		printf("	move ");
	} else if (off == 0 && p->n_name[0] == 0) {
		printf("	ldb %s,%s\n", rnames[reg1], rnames[reg]);
		/* XXX must sign extend here even if not necessary */
		switch (type) {
		case CHAR:
			printf("	lsh %s,033\n", rnames[reg1]);
			printf("	ash %s,-033\n", rnames[reg1]);
			break;
		case SHORT:
			printf("	hrre %s,%s\n",
			    rnames[reg1], rnames[reg1]);
			break;
		}
		return;
	} else if (ischar) {
		if (off >= 0700000000000LL && p->n_name[0] != '\0') {
			cerror("emitsh");
			/* reg contains index integer */
			if (!istreg(reg))
				cerror("emitshort !istreg");
			printf("	adjbp %s,[ .long 0%llo+%s ]\n",
			    rnames[reg], off, p->n_name);
			printf("	ldb ");
			adrput(stdout, getlr(p, '1'));
			printf(",%s\n", rnames[reg]);
			goto signe;
		}
		printf("	ldb ");
		adrput(stdout, getlr(p, '1'));
		if (off)
			printf(",[ .long 0%02o11%02o%06o ]\n",
			    (int)(27-(9*(off&3))), reg, (int)off/4);
		else
			printf(",%s\n", rnames[reg]);
signe:		if (issigned) {
			printf("	lsh ");
			adrput(stdout, getlr(p, '1'));
			printf(",033\n	ash ");
			adrput(stdout, getlr(p, '1'));
			printf(",-033\n");
		}
		return;
	} else {
		printf("	h%cr%c ", off & 1 ? 'r' : 'l',
		    issigned ? 'e' : 'z');
	}
	p->n_lval /= (ischar ? 4 : 2);
	adrput(stdout, getlr(p, '1'));
	putchar(',');
	adrput(stdout, getlr(p, 'L'));
	putchar('\n');
}

/*
 * Store a short from a register. Destination is a OREG.
 */
static void
storeshort(NODE *p)
{
	NODE *l = p->n_left;
	CONSZ off = l->n_lval;
	int reg = l->n_rval;
	int ischar = BTYPE(p->n_type) == CHAR || BTYPE(p->n_type) == UCHAR;

	if (l->n_op == NAME) {
		if (ischar) {
			printf("	dpb ");
			adrput(stdout, getlr(p, 'R'));
			printf(",[ .long 0%02o%010o+%s ]\n",
			    070+((int)off&3), (int)(off/4), l->n_name);
			return;
		}
		printf("	hr%cm ", off & 1 ? 'r' : 'l');
		l->n_lval /= 2;
		adrput(stdout, getlr(p, 'R'));
		putchar(',');   
		adrput(stdout, getlr(p, 'L'));
		putchar('\n');
		return;
	}

	if (off || reg == FPREG) { /* Can emit halfword instructions */
		if (off < 0) { /* argument, use move instead */
			printf("	movem ");
		} else if (ischar) {
			printf("	dpb ");
			adrput(stdout, getlr(p, '1'));
			printf(",[ .long 0%02o11%02o%06o ]\n",
			    (int)(27-(9*(off&3))), reg, (int)off/4);
			return;
		} else {
			printf("	hr%cm ", off & 1 ? 'r' : 'l');
		}
		l->n_lval /= 2;
		adrput(stdout, getlr(p, 'R'));
		putchar(',');
		adrput(stdout, getlr(p, 'L'));
	} else {
		printf("	dpb ");
		adrput(stdout, getlr(p, 'R'));
		putchar(',');
		l = getlr(p, 'L');
		l->n_op = REG;
		adrput(stdout, l);
		l->n_op = OREG;
	}
	putchar('\n');
}

/*
 * Multiply a register with a constant.
 */
static void     
imuli(NODE *p)
{
	NODE *r = p->n_right;

	if (r->n_lval >= 0 && r->n_lval <= 0777777) {
		printf("	imuli ");
		adrput(stdout, getlr(p, 'L'));
		printf(",0%llo\n", r->n_lval);
	} else {
		printf("	imul ");
		adrput(stdout, getlr(p, 'L'));
		printf(",[ .long 0%llo ]\n", r->n_lval & 0777777777777LL);
	}
}

/*
 * Divide a register with a constant.
 */
static void     
idivi(NODE *p)
{
	NODE *r = p->n_right;

	if (r->n_lval >= 0 && r->n_lval <= 0777777) {
		printf("	idivi ");
		adrput(stdout, getlr(p, '1'));
		printf(",0%llo\n", r->n_lval);
	} else {
		printf("	idiv ");
		adrput(stdout, getlr(p, '1'));
		printf(",[ .long 0%llo ]\n", r->n_lval & 0777777777777LL);
	}
}

/*
 * move a constant into a register.
 */
static void
xmovei(NODE *p)
{
	/*
	 * Trick: If this is an unnamed constant, just move it directly,
	 * otherwise use xmovei to get section number.
	 */
	if (p->n_name[0] == '\0' || p->n_lval > 0777777) {
		printf("	");
		zzzcode(p, 'D');
		putchar(' ');
		adrput(stdout, getlr(p, '1'));
		putchar(',');
		zzzcode(p, 'E');
	} else {
		printf("	xmovei ");
		adrput(stdout, getlr(p, '1'));
		printf(",%s", p->n_name);
		if (p->n_lval != 0)
			printf("+0%llo", p->n_lval);
	}
	putchar('\n');
}

static void
printcon(NODE *p) 
{
	CONSZ cz;

	p = p->n_left;
	if (p->n_lval >= 0700000000000LL) {
		/* converted to pointer in clocal() */
		conput(p);
		return;
	}
	if (p->n_lval == 0 && p->n_name[0] == '\0') {
		putchar('0');
		return;
	}
	if (BTYPE(p->n_type) == CHAR || BTYPE(p->n_type) == UCHAR)
		cz = (p->n_lval/4) | ((p->n_lval & 3) << 30);
	else
		cz = (p->n_lval/2) | (((p->n_lval & 1) + 5) << 30);
	cz |= 0700000000000LL;
	printf("0%llo", cz);
	if (p->n_name[0] != '\0')
		printf("+%s", p->n_name);
}

static void
putcond(NODE *p)
{               
	char *c;

	switch (p->n_op) {
	case EQ: c = "e"; break;
	case NE: c = "n"; break;
	case LE: c = "le"; break;
	case LT: c = "l"; break;
	case GT: c = "g"; break;
	case GE: c = "ge"; break;
	default:
		cerror("putcond");
	}
	printf("%s", c);
}

void
zzzcode(NODE *p, int c)
{
	NODE *l;
	CONSZ hval;

	switch (c) {
	case 'A': /* ildb right arg */
		adrput(stdout, p->n_left->n_left);
		break;

	case 'B': /* remove from stack after subroutine call */
		if (p->n_rval)
			printf("	subi %%17,0%o\n", p->n_rval/SZINT);
		break;

	case 'C':
		constput(p);
		break;

	case 'D': /* Find out which type of const load insn to use */
		if (p->n_op != ICON)
			cerror("zzzcode not ICON");
		if (p->n_name[0] == '\0') {
			if ((p->n_lval <= 0777777) && (p->n_lval > 0))
				printf("movei");
			else if ((p->n_lval & 0777777) == 0)
				printf("hrlzi");
			else
				printf("move");
		} else
			printf("move");
		break;

	case 'E': /* Print correct constant expression */
		if (p->n_name[0] == '\0') {
			if ((p->n_lval <= 0777777) && (p->n_lval > 0)){
				printf("0%llo", p->n_lval);
			} else if ((p->n_lval & 0777777) == 0) {
				printf("0%llo", p->n_lval >> 18);
			} else {
				if (p->n_lval < 0)
					printf("[ .long -0%llo]", -p->n_lval);
				else
					printf("[ .long 0%llo]", p->n_lval);
			}
		} else {
			if (p->n_lval == 0)
				printf("[ .long %s]", p->n_name);
			else
				printf("[ .long %s+0%llo]",
				    p->n_name, p->n_lval);
		}
		break;

	case 'P':
		p = getlr(p, 'R');
		/* FALLTHROUGH */
	case 'O':
		/*
		 * Print long long expression.
		 */
		hval = gethval(p->n_lval);
		printf("[ .long 0%llo,0%llo", hval,
		    (p->n_lval & 0377777777777LL) | (hval & 0400000000000LL));
		if (p->n_name[0] != '\0')
			printf("+%s", p->n_name);
		printf(" ]");
		break;

	case 'F': /* Print an "opsimp" instruction based on its const type */
		hopcode(oneinstr(p->n_right) ? 'C' : 'R', p->n_op);
		break;

	case 'H': /* Print a small constant */
		p = p->n_right;
		printf("0%llo", p->n_lval & 0777777);
		break;

	case 'Q': /* two-param long long comparisions */
		twollcomp(p);
		break;

	case 'R': /* two-param conditionals */
		twocomp(p);
		break;

	case 'U':
		emitshort(p);
		break;
		
	case 'V':
		storeshort(p);
		break;

	case 'Z':
		ptrcomp(p);
		break;

	case 'a':
		imuli(p);
		break;

	case 'b':
		idivi(p);
		break;

	case 'c':
		xmovei(p);
		break;

	case 'd':
		printcon(p);
		break;

	case 'e':
		putcond(p);
		break;

	case 'g':
		if (p->n_right->n_op != OREG || p->n_right->n_lval != 0)
			comperr("bad Zg oreg");
		printf("%s", rnames[p->n_right->n_rval]);
		break;

#if 0
	case '1': /* double upput */
		p = getlr(p, '1');
		p->n_rval += 2;
		adrput(stdout, p);
		p->n_rval -= 2;
		break;
#endif

	case 'i': /* Write instruction for short load from name */
		l = getlr(p, 'L');
		printf("	h%cr%c %s,%s+" CONFMT "\n",
		    l->n_lval & 1 ? 'r' : 'l',
		    ISUNSIGNED(p->n_type) ? 'z' : 'e',
		    rnames[getlr(p, '1')->n_rval],
		    l->n_name, l->n_lval >> 1);
		break;

	default:
		cerror("zzzcode %c", c);
	}
}

/* set up temporary registers */
void
setregs()
{
	fregs = 7;	/* 7 free regs on PDP10 (1-7) */
}

/*ARGSUSED*/
int
rewfld(NODE *p)
{
	return(1);
}

int
flshape(NODE *p)
{
	register int o = p->n_op;

	return (o == REG || o == NAME || o == ICON ||
		(o == OREG && (!R2TEST(p->n_rval) || tlen(p) == 1)));
}

/* INTEMP shapes must not contain any temporary registers */
int
shtemp(NODE *p)
{
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
}

int
shumul(NODE *p)
{
	register int o;

	if (x2debug) {
		int val;
		printf("shumul(%p)\n", p);
		eprint(p, 0, &val, &val);
	}

	o = p->n_op;
#if 0
	if (o == NAME || (o == OREG && !R2TEST(p->n_rval)) || o == ICON)
		return(STARNM);
#endif

	if ((o == INCR) &&
	    (p->n_left->n_op == REG && p->n_right->n_op == ICON) &&
	    p->n_right->n_name[0] == '\0') {
		switch (p->n_type) {
			case CHAR|PTR:
			case UCHAR|PTR:
				o = 1;
				break;

			case SHORT|PTR:
			case USHORT|PTR:
				o = 2;
				break;

			case INT|PTR:
			case UNSIGNED|PTR:
			case LONG|PTR:
			case ULONG|PTR:
			case FLOAT|PTR:
				o = 4;
				break;

			case DOUBLE|PTR:
			case LONGLONG|PTR:
			case ULONGLONG|PTR:
				o = 8;
				break;

			default:
				if (ISPTR(p->n_type) &&
				     ISPTR(DECREF(p->n_type))) {
					o = 4;
					break;
				} else
					return(0);
		}
		return( 0);
	}

	return( 0 );
}

void
adrcon(CONSZ val)
{
	cerror("adrcon: val %llo\n", val);
}

void
conput(NODE *p)
{
	switch (p->n_op) {
	case ICON:
		if (p->n_lval != 0) {
			acon(stdout, p);
			if (p->n_name[0] != '\0')
				putchar('+');
		}
		if (p->n_name[0] != '\0')
			printf("%s", p->n_name);
		if (p->n_name[0] == '\0' && p->n_lval == 0)
			putchar('0');
		return;

	case REG:
		putstr(rnames[p->n_rval]);
		return;

	default:
		cerror("illegal conput");
	}
}

/*ARGSUSED*/
void
insput(NODE *p)
{
	cerror("insput");
}

/*
 * Write out the upper address, like the upper register of a 2-register
 * reference, or the next memory location.
 */
void
upput(NODE *p, int size)
{

	size /= SZLONG;
	switch (p->n_op) {
	case REG:
		putstr(rnames[p->n_rval + size]);
		break;

	case NAME:
	case OREG:
		p->n_lval += size;
		adrput(stdout, p);
		p->n_lval -= size;
		break;
	case ICON:
		printf(CONFMT, p->n_lval >> (36 * size));
		break;
	default:
		cerror("upput bad op %d size %d", p->n_op, size);
	}
}

void
adrput(FILE *fp, NODE *p)
{
	int r;
	/* output an address, with offsets, from p */

	if (p->n_op == FLD)
		p = p->n_left;

	switch (p->n_op) {

	case NAME:
		if (p->n_name[0] != '\0')
			fputs(p->n_name, fp);
		if (p->n_lval != 0)
			fprintf(fp, "+" CONFMT, p->n_lval & 0777777777777LL);
		return;

	case OREG:
		r = p->n_rval;
#if 0
		if (R2TEST(r)) { /* double indexing */
			register int flags;

			flags = R2UPK3(r);
			if (flags & 1)
				putc('*', fp);
			if (flags & 4)
				putc('-', fp);
			if (p->n_lval != 0 || p->n_name[0] != '\0')
				acon(p);
			if (R2UPK1(r) != 100)
				printf("(%s)", rnames[R2UPK1(r)]);
			if (flags & 2)
				putchar('+');
			printf("[%s]", rnames[R2UPK2(r)]);
			return;
		}
#endif
		if (R2TEST(r))
			cerror("adrput: unwanted double indexing: r %o", r);
		if (p->n_rval != FPREG && p->n_lval < 0 && p->n_name[0]) {
			fprintf(fp, "%s", p->n_name);
			acon(fp, p);
			fprintf(fp, "(%s)", rnames[p->n_rval]);
			return;
		}
		if (p->n_lval < 0 && p->n_rval == FPREG && offarg) {
			p->n_lval -= offarg-2; acon(fp, p); p->n_lval += offarg-2;
		} else if (p->n_lval != 0)
			acon(fp, p);
		if (p->n_name[0] != '\0')
			fprintf(fp, "%s%s", p->n_lval ? "+" : "", p->n_name);
		if (p->n_lval > 0 && p->n_rval == FPREG && offlab)
			fprintf(fp, "+" LABFMT, offlab);
		if (p->n_lval < 0 && p->n_rval == FPREG && offarg)
			fprintf(fp, "(017)");
		else
			fprintf(fp, "(%s)", rnames[p->n_rval]);
		return;
	case ICON:
		/* addressable value of the constant */
		if (p->n_lval > 0) {
			acon(fp, p);
			if (p->n_name[0] != '\0')
				putc('+', fp);
		}
		if (p->n_name[0] != '\0')
			fprintf(fp, "%s", p->n_name);
		if (p->n_lval < 0) 
			acon(fp, p);
		if (p->n_name[0] == '\0' && p->n_lval == 0)
			putc('0', fp);
		return;

	case REG:
		fputs(rnames[p->n_rval], fp);
		return;

	case MOVE: /* Specially generated node */
		fputs(rnames[p->n_rall], fp);
		return;

	default:
		cerror("illegal address, op %d", p->n_op);
		return;

	}
}

/*
 * print out a constant
*/
void
acon(FILE *fp, NODE *p)
{
	if (p->n_lval < 0 && p->n_lval > -0777777777777ULL)
		fprintf(fp, "-" CONFMT, -p->n_lval);
	else
		fprintf(fp, CONFMT, p->n_lval);
}

/*   printf conditional and unconditional branches */
void
cbgen(int o,int lab)
{
}

/*
 * Do some local optimizations that must be done after optim is called.
 */
static void
optim2(NODE *p)
{
	int op = p->n_op;
	int m, ml;
	NODE *l;

	/* Remove redundant PCONV's */
	if (op == PCONV) {
		l = p->n_left;
		m = BTYPE(p->n_type);
		ml = BTYPE(l->n_type);
		if ((m == INT || m == LONG || m == LONGLONG || m == FLOAT ||
		    m == DOUBLE || m == STRTY || m == UNIONTY || m == ENUMTY ||
		    m == UNSIGNED || m == ULONG || m == ULONGLONG) &&
		    (ml == INT || ml == LONG || ml == LONGLONG || ml == FLOAT ||
		    ml == DOUBLE || ml == STRTY || ml == UNIONTY || 
		    ml == ENUMTY || ml == UNSIGNED || ml == ULONG ||
		    ml == ULONGLONG) && ISPTR(l->n_type)) {
			*p = *l;
			nfree(l);
			op = p->n_op;
		} else
		if (ISPTR(DECREF(p->n_type)) &&
		    (l->n_type == INCREF(STRTY))) {
			*p = *l;
			nfree(l);
			op = p->n_op;
		} else
		if (ISPTR(DECREF(l->n_type)) &&
		    (p->n_type == INCREF(INT) ||
		    p->n_type == INCREF(STRTY) ||
		    p->n_type == INCREF(UNSIGNED))) {
			*p = *l;
			nfree(l);
			op = p->n_op;
		}

	}
	/* Add constands, similar to the one in optim() */
	if (op == PLUS && p->n_right->n_op == ICON) {
		l = p->n_left;
		if (l->n_op == PLUS && l->n_right->n_op == ICON &&
		    (p->n_right->n_name[0] == '\0' ||
		     l->n_right->n_name[0] == '\0')) {
			l->n_right->n_lval += p->n_right->n_lval;
			if (l->n_right->n_name[0] == '\0')
				l->n_right->n_name = p->n_right->n_name;
			nfree(p->n_right);
			*p = *l;
			nfree(l);
		}
	}

	/* Convert "PTR undef" (void *) to "PTR uchar" */
	/* XXX - should be done in MI code */
	if (BTYPE(p->n_type) == VOID)
		p->n_type = (p->n_type & ~BTMASK) | UCHAR;
	if (op == ICON) {
		if ((p->n_type == (PTR|CHAR) || p->n_type == (PTR|UCHAR))
		    && p->n_lval == 0 && p->n_name[0] != '\0')
			p->n_lval = 0700000000000LL;
		if ((p->n_type == (PTR|SHORT) || p->n_type == (PTR|USHORT))
		    && p->n_lval == 0 && p->n_name[0] != '\0')
			p->n_lval = 0750000000000LL;
	}
	if (op == MINUS) {
		if ((p->n_left->n_type == (PTR|CHAR) ||
		    p->n_left->n_type == (PTR|UCHAR)) &&
		    (p->n_right->n_type == (PTR|CHAR) ||
		    p->n_right->n_type == (PTR|UCHAR))) {
			l = talloc();
			l->n_op = SCONV;
			l->n_type = INT;
			l->n_left = p->n_right;
			p->n_right = l;
			l = talloc();
			l->n_op = SCONV;
			l->n_type = INT;
			l->n_left = p->n_left;
			p->n_left = l;
		}
	}
}

void
myreader(NODE *p)
{
	int e2print(NODE *p, int down, int *a, int *b);
	walkf(p, optim2);
	if (x2debug) {
		printf("myreader final tree:\n");
		fwalk(p, e2print, 0);
	}
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

/*
 * Remove last goto.
 */
void
myoptim(struct interpass *ip)
{
	while (ip->sqelem.sqe_next->type != IP_EPILOG)
		ip = ip->sqelem.sqe_next;
	if (ip->type != IP_NODE || ip->ip_node->n_op != GOTO)
		cerror("myoptim");
	tfree(ip->ip_node);
	*ip = *ip->sqelem.sqe_next;
}
