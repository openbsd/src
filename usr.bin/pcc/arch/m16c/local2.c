/*	$OpenBSD: local2.c,v 1.2 2007/09/15 22:04:38 ray Exp $	*/
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

void acon(NODE *p);
int argsize(NODE *p);
void genargs(NODE *p);

static int ftlab1, ftlab2;

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

static TWORD ftype;
static int addto;

void
prologue(struct interpass_prolog *ipp)
{
    ftype = ipp->ipp_type;

#if 0
    if (ipp->ipp_regs > 0 && ipp->ipp_regs != MINRVAR)
	comperr("fix prologue register savings", ipp->ipp_regs);
#endif
    
    printf("	RSEG CODE:CODE:REORDER:NOROOT(0)\n");
    if (ipp->ipp_vis)	
	printf("	PUBLIC %s\n", ipp->ipp_name);
    printf("%s:\n", ipp->ipp_name);
    
#if 0	
    if (xsaveip) {
	/* Optimizer running, save space on stack */
	addto = (p2maxautooff - AUTOINIT)/SZCHAR;
	printf("	enter #%d\n", addto);
    } else {
#endif

	/* non-optimized code, jump to epilogue for code generation */
	ftlab1 = getlab();
	ftlab2 = getlab();
	printf("	jmp.w " LABFMT "\n", ftlab1);
	deflab(ftlab2);
}

/*
 * End of block.
 */
void
eoftn(struct interpass_prolog *ipp)
{
#if 0
	if (ipp->ipp_regs != MINRVAR)
		comperr("fix eoftn register savings %x", ipp->ipp_regs);
#endif

	//	if (xsaveip == 0)
	addto = (p2maxautooff - AUTOINIT)/SZCHAR;

	/* return from function code */
	//deflab(ipp->ipp_ip.ip_lbl);   //XXX - is this necessary?
	
	/* If retval is a pointer and not a function pointer, put in A0 */
	if (ISPTR(DECREF(ipp->ipp_type)) &&
	    !ISFTN(DECREF(DECREF(ipp->ipp_type))))
	    printf("	mov.w r0,a0\n");
	
	/* struct return needs special treatment */
	if (ftype == STRTY || ftype == UNIONTY) {
		comperr("fix struct return in eoftn");
	} else
		printf("	exitd\n");

	/* Prolog code */
	//	if (xsaveip == 0) {
		deflab(ftlab1);
		printf("	enter #%d\n", addto);
		printf("	jmp.w " LABFMT "\n", ftlab2);
		//}
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
	printf("%s.%c", str, f);
}

char *
rnames[] = {  /* keyed to register number tokens */
    "r0", "r2", "r1", "r3", "a0", "a1", "fb", "sp", "r0h", "r0l",
    "r1h", "r1l",
};

/*
 * Return the size (in bytes) of some types.
 */
int
tlen(p) NODE *p;
{
	switch(p->n_type) {
		case CHAR:
		case UCHAR:
			return(1);

		case INT:
		case UNSIGNED:
		case FLOAT:
			return 2;

		case DOUBLE:
		case LONG:
		case ULONG:
			return 4;

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
	expand(p, 0, "	cmp.w UR,UL\n");
	if (cb1) cbgen(cb1, s);
	if (cb2) cbgen(cb2, e);
	expand(p, 0, "	cmp.w AR,AL\n");
	cbgen(p->n_op, e);
	deflab(s);
}


void
zzzcode(NODE *p, int c)
{
	NODE *l;

	switch (c) {
	case 'A': /* print negative shift constant */
		p = getlr(p, 'R');
		if (p->n_op != ICON)
			comperr("ZA bad use");
		p->n_lval = -p->n_lval;
		adrput(stdout, p);
		p->n_lval = -p->n_lval;
		break;

	case 'B':
		if (p->n_rval)
			printf("	add.b #%d,%s\n",
			    p->n_rval, rnames[STKREG]);
		break;

	case 'C': /* Print label address */
		p = p->n_left;
		if (p->n_lval)
			printf(LABFMT, (int)p->n_lval);
		else
			printf("%s", p->n_name);
		break;

	case 'D': /* copy function pointers */
		l = p->n_left;
		printf("\tmov.w #HWRD(%s),%s\n\tmov.w #LWRD(%s),%s\n",
		    p->n_right->n_name, rnames[l->n_rval+1],
		    p->n_right->n_name, rnames[l->n_rval]);
		break;

	case 'E': /* double-reg printout */
		/* XXX - always r0r2 here */
		printf("%s%s", rnames[R0], rnames[R2]);
		break;

	case 'F': /* long comparisions */
		twollcomp(p);
		break;

	case 'G':
		printf("R0R2");
		break;

	case 'H': /* push 32-bit address (for functions) */
		printf("\tpush.w #HWRD(%s)\n\tpush.w #LWRD(%s)\n",
		    p->n_left->n_name, p->n_left->n_name);
		break;

	case 'I': /* push 32-bit address (for functions) */
		l = p->n_left;
		printf("\tpush.w %d[%s]\n\tpush.w %d[%s]\n",
		    (int)l->n_lval, rnames[l->n_rval],
		    (int)l->n_lval+2, rnames[l->n_rval]);
		break;

	default:
		comperr("bad zzzcode %c", c);
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
	    (o==UMUL && shumul(p->n_left) == SRDIR))
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
		comperr("illegal conput");
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

	size /= SZINT;
	switch (p->n_op) {
	case REG:
		fputs(rnames[p->n_rval + 1], stdout);
		break;

	case NAME:
	case OREG:
		p->n_lval += size;
		adrput(stdout, p);
		p->n_lval -= size;
		break;
	case ICON:
		fprintf(stdout, "#" CONFMT, p->n_lval >> 16);
		break;
	default:
		comperr("upput bad op %d size %d", p->n_op, size);
	}
}

void
adrput(FILE *io, NODE *p)
{
	/* output an address, with offsets, from p */

	if (p->n_op == FLD)
		p = p->n_left;

	switch (p->n_op) {

	case NAME:
		if (p->n_name[0] != '\0')
			fputs(p->n_name, io);
		if (p->n_lval != 0)
			fprintf(io, "+" CONFMT, p->n_lval);
		return;

	case OREG:
		if (p->n_lval)
			fprintf(io, "%d", (int)p->n_lval);
		fprintf(io, "[%s]", rnames[p->n_rval]);
		return;
	case ICON:
		/* addressable value of the constant */
		fputc('#', io);
		conput(io, p);
		return;

	case MOVE:
	case REG:
	    /*if (DEUNSIGN(p->n_type) == CHAR) {
			fprintf(io, "R%c%c", p->n_rval < 2 ? '0' : '1',
			    (p->n_rval & 1) ? 'H' : 'L');
			    } else*/
	    fprintf(io, "%s", rnames[p->n_rval]);
	    return;

	default:
		comperr("illegal address, op %d, node %p", p->n_op, p);
		return;

	}
}

static char *
ccbranches[] = {
	"jeq",		/* jumpe */
	"jne",		/* jumpn */
	"jle",		/* jumple */
	"jlt",		/* jumpl */
	"jge",		/* jumpge */
	"jgt",		/* jumpg */
	"jleu",		/* jumple (jlequ) */
	"jltu",		/* jumpl (jlssu) */
	"jgeu",		/* jumpge (jgequ) */
	"jgtu",		/* jumpg (jgtru) */
};


/*   printf conditional and unconditional branches */
void
cbgen(int o, int lab)
{
	if (o < EQ || o > UGT)
		comperr("bad conditional branch: %s", opst[o]);
	printf("	%s " LABFMT "\n", ccbranches[o-EQ], lab);
}

#if 0
void
mygenregs(NODE *p)
{

	if (p->n_op == MINUS && p->n_type == DOUBLE &&
	    (p->n_su & (LMASK|RMASK)) == (LREG|RREG)) {
		p->n_su |= DORIGHT;
	}
	/* Must walk down correct node first for logops to work */
	if (p->n_op != CBRANCH)
		return;
	p = p->n_left;
	if ((p->n_su & (LMASK|RMASK)) != (LREG|RREG))
		return;
	p->n_su &= ~DORIGHT;

}
#endif

struct hardops hardops[] = {
	{ PLUS, FLOAT, "?F_ADD_L04" },
	{ MUL, LONG, "?L_MUL_L03" },
	{ MUL, ULONG, "?L_MUL_L03" },
	{ DIV, LONG, "?SL_DIV_L03" },
	{ DIV, ULONG, "?UL_DIV_L03" },
	{ MOD, LONG, "?SL_MOD_L03" },
	{ MOD, ULONG, "?UL_MOD_L03" },
	{ RS, LONGLONG, "__ashrdi3" },
	{ RS, ULONGLONG, "__lshrdi3" },
	{ LS, LONGLONG, "__ashldi3" },
	{ LS, ULONGLONG, "__ashldi3" },
	{ 0 },
};

int
special(NODE *p, int shape)
{
	switch (shape) {
	case SFTN:
		if (ISPTR(p->n_type) && ISFTN(DECREF(p->n_type))) {
			if (p->n_op == NAME || p->n_op == OREG)
				return SRDIR;
			else
				return SRREG;
		}
		break;
	}
	return SRNOPE;
}

void    
myreader(NODE *p)
{
	NODE *q, *r, *s, *right;

	if (optype(p->n_op) == LTYPE)
		return;
	if (optype(p->n_op) != UTYPE)
		myreader(p->n_right);
	myreader(p->n_left);

	switch (p->n_op) {
	case PLUS:
	case MINUS:
		if (p->n_type != LONG && p->n_type != ULONG)
			break;
		if (p->n_right->n_op == NAME || p->n_right->n_op == OREG)
			break;
		/* Must convert right into OREG */
		right = p->n_right;
		q = mklnode(OREG, BITOOR(freetemp(szty(right->n_type))),
		    FPREG, right->n_type);
		s = mkbinode(ASSIGN, q, right, right->n_type);
		r = talloc(); 
		*r = *q;
		p->n_right = r;
		pass2_compile(ipnode(s));
		break;
	}
}


void
rmove(int s, int d, TWORD t)
{
	switch (t) {
	case CHAR:
	case UCHAR:
	    printf("	mov.b %s,%s\n", rnames[s], rnames[d]);
	    break;
	default:
	    printf("	mov.w %s,%s\n", rnames[s], rnames[d]);
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
		num = r[CLASSA];
		num += r[CLASSC];
		return num < 4;
	case CLASSB:
		num = r[CLASSB];
		return num < 2;
	case CLASSC:
		num = 2*r[CLASSA];
		num += r[CLASSC];
		return num < 4;
	}
	return 0; /* XXX gcc */
}

/*
 * Return a class suitable for a specific type.
 */
int
gclass(TWORD t)
{
	if (t == CHAR || t == UCHAR)
		return CLASSC;
	
	if(ISPTR(t))
		return CLASSB;
	
	return CLASSA;
}

static int sizen;

/* XXX: Fix this. */
static int
argsiz(NODE *p)
{
        TWORD t = p->n_type;

        if (t < LONGLONG || t > MAXTYPES)
                return 4;
        if (t == LONGLONG || t == ULONGLONG || t == DOUBLE)
                return 8;
        if (t == LDOUBLE)
                return 12;
        if (t == STRTY)
                return p->n_stsize;
        comperr("argsiz");
        return 0;
}

/*
 * Calculate argument sizes.
 * XXX: Fix this.
 */
void
lastcall(NODE *p)
{
        sizen = 0;
        for (p = p->n_right; p->n_op == CM; p = p->n_left)
                sizen += argsiz(p->n_right);
        sizen += argsiz(p);
}
