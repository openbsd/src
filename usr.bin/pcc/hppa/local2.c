/*	$OpenBSD: local2.c,v 1.4 2008/04/11 20:45:52 stefan Exp $	*/

/*
 * Copyright (c) 2007 Michael Shalayeff
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
void prtprolog(struct interpass_prolog *, int);
int countargs(NODE *p, int *);
void fixcalls(NODE *p);

static int stkpos;
int p2calls;

static const int rl[] =
  { R0, R1, R1, R1, R1, R1, R31, R31, R31, R31,
    R4, R5, R6, R7, R8, R9, R10, R11, R12, R13, R14, R15, R16, R17, R18,
    T1, T4, T3, T2, ARG3, ARG1, RET1 };
static const int rh[] =
  { R0, R31, T4, T3, T2, T1, T4, T3, T2, T1,
    R18, R4, R5, R6, R7, R8, R9, R10, R11, R12, R13, R14, R15, R16, R17,
    T4, T3, T2, T1, ARG2, ARG0, RET0 };

void
deflab(int label)
{
	printf("\t.label\t" LABFMT "\n", label);
}

static int regoff[MAXREGS];
static TWORD ftype;

/*
 * Print out the prolog assembler.
 * addto and regoff are already calculated.
 */
void
prtprolog(struct interpass_prolog *ipp, int addto)
{
	int i;

	/* if this functions calls nothing -- no frame is needed */
	if (p2calls || p2maxautooff > 4) {
		printf("\tcopy\t%%r3,%%r1\n\tcopy\t%%sp,%%r3\n");
		if (addto < 0x2000)
			printf("\tstw,ma\t%%r1,%d(%%sp)\n", addto);
		else if (addto < 0x802000)
			printf("\tstw,ma\t%%r1,8192(%%sp)\n"
			    "\taddil\t%d-8192,%%sp\n"
			    "\tcopy\t%%r1,%%sp\n", addto);
		else
			comperr("too much local allocation");
		if (p2calls)
			printf("\tstw\t%%rp,-20(%%r3)\n");
	}

	for (i = 0; i < MAXREGS; i++)
		if (TESTBIT(ipp->ipp_regs, i)) {
			if (i <= R31)
				printf("\tstw\t%s,%d(%%r3)\n",
				    rnames[i], regoff[i]);
			else if (i <= RETD0)
				printf("\tstw\t%s,%d(%%r3)\n"
				    "\tstw\t%s,%d(%%r3)\n",
				    rnames[rl[i - RD0]], regoff[i] + 0,
				    rnames[rh[i - RD0]], regoff[i] + 4);
			else if (i <= FR31)
				printf("\tfstws\t%s,%d(%%r3)\n",
				    rnames[i], regoff[i]);
			else
				printf("\tfstds\t%s,%d(%%r3)\n",
				    rnames[i], regoff[i]);
		}
}

/*
 * calculate stack size and offsets
 */
static int
offcalc(struct interpass_prolog *ipp)
{
	int i, addto, off;

	addto = 32;
	if (p2calls) {
		i = p2calls - 1;
		/* round up to 4 args */
		if (i < 4)
			i = 4;
		addto += i * 4;
	}

	for (off = 4, i = 0; i < MAXREGS; i++)
		if (TESTBIT(ipp->ipp_regs, i)) {
			regoff[i] = off;
			off += szty(PERMTYPE(i)) * SZINT/SZCHAR;
		}
	addto += off + p2maxautooff;
	return (addto + 63) & ~63;
}

void
prologue(struct interpass_prolog *ipp)
{
	int addto;

	ftype = ipp->ipp_type;
	printf("\t.align\t4\n");
	if (ipp->ipp_vis)
		printf("\t.export\t%s, code\n", ipp->ipp_name);
	printf("\t.label\t%s\n\t.proc\n", ipp->ipp_name);

	/*
	 * We here know what register to save and how much to 
	 * add to the stack.
	 */
	addto = offcalc(ipp);
	printf("\t.callinfo frame=%d, save_rp, save_sp\n\t.entry\n", addto);
	prtprolog(ipp, addto);
}

void
eoftn(struct interpass_prolog *ipp)
{
	int i;

	if (ipp->ipp_ip.ip_lbl == 0)
		return; /* no code needs to be generated */

	/* return from function code */
	for (i = 0; i < MAXREGS; i++)
		if (TESTBIT(ipp->ipp_regs, i)) {
			if (i <= R31)
				printf("\tldw\t%d(%%r3),%s\n",
				    regoff[i], rnames[i]);
			else if (i <= RETD0)
				printf("\tldw\t%d(%%r3),%s\n"
				    "\tldw\t%d(%%r3),%s\n",
				    regoff[i] + 0, rnames[rl[i - RD0]],
				    regoff[i] + 4, rnames[rh[i - RD0]]);
			else if (i <= FR31)
				printf("\tfldws\t%d(%%r3),%s\n",
				    regoff[i], rnames[i]);
			else
				printf("\tfldds\t%d(%%r3),%s\n",
				    regoff[i], rnames[i]);
		}

	if (p2calls || p2maxautooff > 4) {
		if (p2calls)
			printf("\tldw\t-20(%%r3),%%rp\n");
		printf("\tcopy\t%%r3,%%r1\n"
		    "\tldw\t(%%r3),%%r3\n"
		    "\tbv\t%%r0(%%rp)\n"
		    "\tcopy\t%%r1,%%sp\n");
	} else
		printf("\tbv\t%%r0(%%rp)\n\tnop\n");

	printf("\t.exit\n\t.procend\n\t.size\t%s, .-%s\n",
	    ipp->ipp_name, ipp->ipp_name);
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
	case EQ:
		str = "=";
		break;
	case NE:
		str = "<>";
		break;
	case LE:
		str = "<";
		break;
	case LT:
		str = "<=";
		break;
	case ULE:
		str = "<<";
		break;
	case ULT:
		str = "<<=";
		break;
	case GE:
		str = ">=";
		break;
	case GT:
		str = ">";
		break;
	case UGE:
		str = ">>";
		break;
	case UGT:
		str = ">>=";
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

		case FLOAT:
			return(SZFLOAT/SZCHAR);

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
				comperr("tlen type %d not pointer", p->n_type);
			return SZPOINT(p->n_type)/SZCHAR;
		}
}

static int
argsiz(NODE *p)
{
	NODE *q;
	TWORD t = p->n_type;

	if (t < LONGLONG || t == FLOAT || t > BTMASK)
		return 4;
	if (t == LONGLONG || t == ULONGLONG || t == DOUBLE)
		return 8;
	if (t == LDOUBLE)
		return 8;	/* LDOUBLE is 16 */
	if ((t == STRTY || t == UNIONTY) && p->n_right->n_op == STARG)
		return 4 + p->n_right->n_stsize;
        /* perhaps it's down there somewhere -- let me take another look! */
	if ((t == STRTY || t == UNIONTY) && p->n_right->n_op == CALL) {
		q = p->n_right->n_right->n_left->n_left->n_right;
		if (q->n_op == STARG)
			return 4 + q->n_stsize;
	}
	comperr("argsiz %p", p);
	return 0;
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
	if (cb1) {
		p->n_op = cb1;
		p->n_label = s;
		expand(p, 0, "\tcomb,O\tUR,UL,LC\n\tnop\n");
		p->n_label = e;
		p->n_op = o;
	}
	if (cb2) {
		p->n_op = cb2;
		expand(p, 0, "\tcomb,O\tUR,UL,LC\n\tnop\n");
		p->n_op = o;
	}
	expand(p, 0, "\tcomb,O\tAR,AL,LC\n\tnop\n");
	deflab(s);
}

void
zzzcode(NODE *p, int c)
{
	int n;

	switch (c) {

	case 'C':	/* after-call fixup */
		n = p->n_qual;	/* args */
		break;

	case 'P':	/* returning struct-call setup */
		n = p->n_qual;	/* args */
		break;

	case 'D':	/* Long long comparision */
		twollcomp(p);
		break;

	case 'F':	/* struct as an arg */

	default:
		comperr("zzzcode %c", c);
	}
}

int canaddr(NODE *);
int
canaddr(NODE *p)
{
	int o = p->n_op;

	if (o == NAME || o == REG || o == ICON || o == OREG ||
	    (o == UMUL && shumul(p->n_left)))
		return(1);
	return(0);
}

int
fldexpand(NODE *p, int cookie, char **cp)
{
	return 0;
}

/*
 * Does the bitfield shape match?
 */
int
flshape(NODE *p)
{
	int o = p->n_op;

	if (isreg(p))
		return SRDIR; /* Direct match */

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
	/* fix for L% and R% */
	printf(CONFMT, val);
}

void
conput(FILE *fp, NODE *p)
{
	int val = p->n_lval;

	switch (p->n_op) {
	case ICON:
		if (p->n_name[0] != '\0') {
			fprintf(fp, "RR'%s", p->n_name);
			if (val)
				fprintf(fp, "+%d", val);
			fprintf(fp, "-$global$");
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
		printf("%s", rnames[rh[p->n_rval - RD0]]);
		break;

	case OREG:
		p->n_lval += size;
		adrput(stdout, p);
		p->n_lval -= size;
		break;

	case ICON:
	case NAME:
		if (p->n_name[0] != '\0') {
			printf("LL'%s", p->n_name);
			if (p->n_lval != 0)
				printf("+" CONFMT, p->n_lval);
			printf("-$global$");
		} else
			printf("L%%" CONFMT, p->n_lval >> 32);
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

	case ICON:
	case NAME:
		if (p->n_name[0] != '\0') {
			fprintf(io, "RR'%s", p->n_name);
			if (p->n_lval != 0)
				fprintf(io, "+" CONFMT, p->n_lval);
			fprintf(io, "-$global$");
		} else
			fprintf(io, "R%%" CONFMT, p->n_lval);
		return;

	case OREG:
		r = p->n_rval;
		if (p->n_name[0] != '\0') {
			fprintf(io, "RR'%s", p->n_name);
			if (p->n_lval != 0)
				fprintf(io, "+" CONFMT, p->n_lval);
			fprintf(io, "-$global$");
		} else if (p->n_lval)
			fprintf(io, "%d", (int)p->n_lval);
		if (R2TEST(r)) {
			fprintf(io, "%s(%s)", rnames[R2UPK1(r)],
			    rnames[R2UPK2(r)]);
		} else
			fprintf(io, "(%s)", rnames[p->n_rval]);
		return;
	case REG:
		if (RD0 <= p->n_rval && p->n_rval <= RETD0)
			fprintf(io, "%s", rnames[rl[p->n_rval - RD0]]);
		else
			fprintf(io, "%s", rnames[p->n_rval]);
		return;

	default:
		comperr("illegal address, op %d, node %p", p->n_op, p);
		return;

	}
}

/* not used */
void
cbgen(int o, int lab)
{
}

int
countargs(NODE *p, int *n)
{
	int sz;
	
	if (p->n_op == CM) {
		countargs(p->n_left, n);
		countargs(p->n_right, n);
		return *n;
	}

	sz = argsiz(p) / 4;
	if (*n % (sz > 4? 4 : sz))
		(*n)++; /* XXX */

	return *n += sz;
}

void
fixcalls(NODE *p)
{
	int n, o;

	/* Prepare for struct return by allocating bounce space on stack */
	switch (o = p->n_op) {
	case STCALL:
	case USTCALL:
		if (p->n_stsize + p2autooff > stkpos)
			stkpos = p->n_stsize + p2autooff;
		/* FALLTHROGH */
	case CALL:
	case UCALL:
		n = 0;
		n = 1 + countargs(p->n_right, &n);
		if (n > p2calls)
			p2calls = n;
		break;
	}
}

void
myreader(struct interpass *ipole)
{
	struct interpass *ip;

	stkpos = p2autooff;
	DLIST_FOREACH(ip, ipole, qelem) {
		switch (ip->type) {
		case IP_PROLOG:
			p2calls = 0;
			break;

		case IP_NODE:
			walkf(ip->ip_node, fixcalls);
			break;
		}
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
myoptim(struct interpass *ipole)
{
}

void
rmove(int s, int d, TWORD t)
{
	int sl, sh, dl, dh;

	switch (t) {
	case LONGLONG:
	case ULONGLONG:
		sl = rl[s-RD0];
		sh = rh[s-RD0];
		dl = rl[d-RD0];
		dh = rh[d-RD0];

#define	SW(x,y) { int i = x; x = y; y = i; }
		if (sl == dh || sh == dl) {
			/* Swap if moving to itself */
			SW(sl, sh);
			SW(dl, dh);
		}
		if (sl != dl)
			printf("\tcopy\t%s,%s\n", rnames[sl], rnames[dl]);
		if (sh != dh)
			printf("\tcopy\t%s,%s\n", rnames[sh], rnames[dh]);
		break;
	case FLOAT:
		printf("\tfcpy,sgl\t%s,%s\n", rnames[s], rnames[d]);
		break;
	case DOUBLE:
	case LDOUBLE:
		printf("\tfcpy,dbl\t%s,%s\n", rnames[s], rnames[d]);
		break;
	default:
		printf("\tcopy\t%s,%s\n", rnames[s], rnames[d]);
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
		num = 2 * r[CLASSB];
		num += r[CLASSA];
		return num < 28;
	case CLASSB:
		num = r[CLASSA];
		num += r[CLASSB] * 2;
		return num < 28;
	case CLASSC:
		num = (r[CLASSD] > 8? 8 : r[CLASSD]) * 2;
		num += r[CLASSC];
		return num < 28;
	case CLASSD:
		num = (r[CLASSC] + 1) / 2;
		num += r[CLASSD];
		return num < 28;
	}
	return 0; /* XXX gcc */
}

const char * const rnames[MAXREGS] = {
	"%r0", "%r1", "%rp", "%r3", "%r4", "%r5", "%r6", "%r7", "%r8", "%r9",
	"%r10", "%r11", "%r12", "%r13", "%r14", "%r15", "%r16", "%r17", "%r18",
	"%t4", "%t3", "%t2", "%t1", "%arg3", "%arg2", "%arg1", "%arg0", "%dp",
	"%ret0", "%ret1", "%sp", "%r31",
	"%rd0", "%rd1", "%rd2", "%rd3", "%rd4", "%rd5", "%rd6", "%rd7",
	"%rd8", "%rd9", "%rd10", "%rd11", "%rd12", "%rd13", "%rd14", "%rd15",
	"%rd16", "%rd17", "%rd18", "%rd19", "%rd20", "%rd21", "%rd22", "%rd23",
	"%rd24", "%td4", "%td3", "%td2", "%td1", "%ad1", "%ad0", "%retd0",
	"%fr0", "%fr4", "%fr5", "%fr6", "%fr7", "%fr8", "%fr9", "%fr10",
	"%fr11", "%fr12", "%fr13", "%fr14", "%fr15", "%fr16", "%fr17", "%fr18",
	"%fr19", "%fr20", "%fr21", "%fr22", "%fr23", "%fr24", "%fr25", "%fr26",
	"%fr27", "%fr28", "%fr29", "%fr30", "%fr31",
	"%fr0l", "%fr0r", "%fr4l", "%fr4r", "%fr5l", "%fr5r", "%fr6l", "%fr6r",
	"%fr7l", "%fr7r", "%fr8l", "%fr8r", "%fr9l", "%fr9r",
	"%fr10l", "%fr10r", "%fr11l", "%fr11r", "%fr12l", "%fr12r",
	"%fr13l", "%fr13r", "%fr14l", "%fr14r", "%fr15l", "%fr15r",
	"%fr16l", "%fr16r", "%fr17l", "%fr17r", "%fr18l", "%fr18r",
#ifdef __hppa64__
	"%fr19l", "%fr19r",
	"%fr20l", "%fr20r", "%fr21l", "%fr21r", "%fr22l", "%fr22r",
	"%fr23l", "%fr23r", "%fr24l", "%fr24r", "%fr25l", "%fr25r",
	"%fr26l", "%fr26r", "%fr27l", "%fr27r", "%fr28l", "%fr28r",
	"%fr29l", "%fr29r", "%fr30l", "%fr30r", "%fr31l", "%fr31r",
#endif
};

/*
 * Return a class suitable for a specific type.
 */
int
gclass(TWORD t)
{
	switch (t) {
	case LONGLONG:
	case ULONGLONG:
		return CLASSB;
	case FLOAT:
		return CLASSC;
	case DOUBLE:
	case LDOUBLE:
		return CLASSD;
	default:
		return CLASSA;
	}
}

/*
 * Calculate argument sizes.
 */
void
lastcall(NODE *p)
{
	NODE *op = p;
	int size = 64;

	p->n_qual = size;
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
	case SPIMM:
		if (o != ICON || p->n_name[0] ||
		    p->n_lval < -31 || p->n_lval >= 32)
			break;
		return SRDIR;
	case SPICON:
		if (o != ICON || p->n_name[0] ||
		    p->n_lval < -1024 || p->n_lval >= 1024)
			break;
		return SRDIR;
	case SPCON:
		if (o != ICON || p->n_name[0] ||
		    p->n_lval < -8192 || p->n_lval >= 8192)
			break;
		return SRDIR;
	case SPNAME:
		if (o != ICON || !p->n_name[0])
			break;
		return SRDIR;
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
