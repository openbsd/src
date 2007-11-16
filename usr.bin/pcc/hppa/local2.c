/*	$OpenBSD: local2.c,v 1.1 2007/11/16 08:36:23 otto Exp $	*/

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
int argsize(NODE *p);

static int stkpos;

static const int rl[] =
  { R1, R5, R7, R9, R11, R13, R15, R17, T4, T2, ARG3, ARG1, RET1 };
static const int rh[] =
  { R31, R4, R6, R8, R10, R12, R14, R16, T3, T1, ARG2, ARG0, RET0 };

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
static void
prtprolog(struct interpass_prolog *ipp, int addto)
{
	int i;

	printf("\tcopy\t%%r3,%%r1\n"
	    "\tcopy\t%%sp,%%r3\n"
	    "\tstw,ma\t%%r1,%d(%%sp)\n", addto);
	for (i = 0; i < MAXREGS; i++)
		if (TESTBIT(ipp->ipp_regs, i)) {
			if (i <= R31)
				printf("\tstw\t%s,%d(%s)\n",
				    rnames[i], regoff[i], rnames[FPREG]);
			else if (i < RETD0)
				printf("\tstw\t%s,%d(%s)\n\tstw\t%s,%d(%s)\n",
				    rnames[rl[i - RD0]], regoff[i] + 0, rnames[FPREG],
				    rnames[rh[i - RD0]], regoff[i] + 4, rnames[FPREG]);
			else if (i <= FR31)
				;
			else
				;
		}
}

/*
 * calculate stack size and offsets
 */
static int
offcalc(struct interpass_prolog *ipp)
{
	int i, addto;

	addto = 32;
	for (i = 0; i < MAXREGS; i++)
		if (TESTBIT(ipp->ipp_regs, i)) {
			regoff[i] = addto;
			addto += SZINT/SZCHAR;
		}
	addto += p2maxautooff;
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
		if (TESTBIT(ipp->ipp_regs, i))
			printf("\tldw\t%d(%s),%s\n",
			    regoff[i], rnames[FPREG], rnames[i]);

	/* TODO restore sp,rp */
	printf("\tbv\t%%r0(%%rp)\n\tnop\n"
	    "\t.exit\n\t.procend\n\t.size\t%s, .-%s\n",
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
	case GE:
		str = ">";
		break;
	case GT:
		str = ">=";
		break;
	case ULE:
		str = "<<";
		break;
	case ULT:
		str = "<<=";
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
				comperr("tlen type %d not pointer");
			return SZPOINT(p->n_type)/SZCHAR;
		}
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
		return 8;	/* quad is 16 */
	if (t == STRTY || t == UNIONTY)
		return p->n_stsize;
	comperr("argsiz");
	return 0;
}

void
zzzcode(NODE *p, int c)
{
	int n;

	switch (c) {

	case 'C':	/* after-call fixup */
		n = p->n_qual;
		if (n)
			printf("\tldo\t-%d(%%sp),%%sp\n", n);
		break;

	case 'P':	/* returning struct-call setup */
		n = p->n_qual;
		if (n)
			printf("\tldo\t%d(%%sp),%%sp\n", n);
		break;

	case 'F':	/* output extr/dep offset/len parts for bitfields */

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
	case MOVE:
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

void
myreader(struct interpass *ipole)
{
	struct interpass *ip;

	stkpos = p2autooff;
	DLIST_FOREACH(ip, ipole, qelem) {
		if (ip->type != IP_NODE)
			continue;
		walkf(ip->ip_node, fixcalls);
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
	"%r1", "%rp", "%r3", "%r4", "%r5", "%r6", "%r7", "%r8", "%r9",
	"%r10", "%r11", "%r12", "%r13", "%r14", "%r15", "%r16", "%r17", "%r18",
	"%t4", "%t3", "%t2", "%t1", "%arg3", "%arg2", "%arg1", "%arg0", "%dp",
	"%ret0", "%ret1", "%sp", "%r31",
	"%rd0", "%rd1", "%rd2", "%rd3", "%rd4", "%rd5", "%rd6", "%rd7",
	"%td2", "%td1", "%ad1", "%ad0", "%retd0",
	"%fr0", "%fr4", "%fr5", "%fr6", "%fr7", "%fr8", "%fr9", "%fr10",
	"%fr11", "%fr12", "%fr13", "%fr14", "%fr15", "%fr16", "%fr17", "%fr18",
	"%fr19", "%fr20", "%fr21", "%fr22", "%fr23", "%fr24", "%fr25", "%fr26",
	"%fr27", "%fr28", "%fr29", "%fr30", "%fr31",
	"%fr0l", "%fr0r", "%fr4l", "%fr4r", "%fr5l", "%fr5r", "%fr6l", "%fr6r",
	"%fr7l", "%fr7r", "%fr8l", "%fr8r", "%fr9l", "%fr9r",
	"%fr10l", "%fr10r", "%fr11l", "%fr11r", "%fr12l", "%fr12r",
	"%fr13l", "%fr13r", "%fr14l", "%fr14r", "%fr15l", "%fr15r",
	"%fr16l", "%fr16r", "%fr17l", "%fr17r",
#ifdef __hppa64__
	"%fr18l", "%fr18r", "%fr19l", "%fr19r",
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
