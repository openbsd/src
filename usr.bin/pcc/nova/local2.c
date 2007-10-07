/*	$OpenBSD: local2.c,v 1.1 2007/10/07 17:58:51 otto Exp $	*/
/*
 * Copyright (c) 2006 Anders Magnusson (ragge@ludd.luth.se).
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

static int prolnum;
static struct ldq {
	struct ldq *next;
	int val;
	int lab;
	char *name;
} *ldq;


void
prologue(struct interpass_prolog *ipp)
{
	int i, j;

	for (i = ipp->ipp_regs, j = 0; i; i >>= 1)
		if (i&1)
			j++;
	printf(".LP%d:	.word 0%o\n", prolnum, j);
	if (ipp->ipp_vis)
		printf("	.globl %s\n", ipp->ipp_name);
	printf("%s:\n", ipp->ipp_name);
	printf("	sta 3,@40\n");	/* save ret pc on stack */
	printf("	lda 2,.LP%d-.,1\n", prolnum);
	printf("	jsr @45\n");
	prolnum++;
}

void
eoftn(struct interpass_prolog *ipp)
{
	int i, j;

	if (ipp->ipp_ip.ip_lbl == 0)
		return; /* no code needs to be generated */

	/* return from function code */
	for (i = ipp->ipp_regs, j = 0; i ; i >>= 1)
		if (i & 1)
			j++;
	printf("	lda 2,.LP%d-.,1\n", prolnum);
	printf("	jmp @46\n");
	printf(".LP%d:	.word 0%o\n", prolnum, j);
	prolnum++;
	while (ldq) {
		printf(".LP%d:	.word 0%o", ldq->lab, ldq->val);
		if (ldq->name && *ldq->name)
			printf("+%s", ldq->name);
		printf("\n");
		ldq = ldq->next;
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
	char *str = 0;

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
		cerror("hopcode OR");
		break;
	case ER:
		cerror("hopcode xor");
		str = "xor";
		break;
	default:
		comperr("hopcode2: %d", o);
		str = 0; /* XXX gcc */
	}
	printf("%s%c", str, f);
}

#if 0
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
#endif

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

void
zzzcode(NODE *p, int c)
{
	struct ldq *ld;

	switch (c) {
	case 'A': /* print out a skip ending if any numbers in queue */
		if (ldq == NULL)
			return;
		printf(",skp\n.LP%d:	.word 0%o", ldq->lab, ldq->val);
		if (ldq->name && *ldq->name)
			printf("+%s", ldq->name);
		printf("\n");
		ldq = ldq->next;
		break;

	case 'B': /* print a label for later load */
		ld = tmpalloc(sizeof(struct ldq));
		ld->val = p->n_lval;
		ld->name = p->n_name;
		ld->lab = prolnum++;
		ld->next = ldq;
		ldq = ld;
		printf(".LP%d-.", ld->lab);
		break;

	case 'C': /* fix reference to external variable via indirection */
		zzzcode(p->n_left, 'B');
		break;

	case 'D': /* fix reference to external variable via indirection */
		zzzcode(p, 'B');
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

cerror("flshape");
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

/*
 * Conput should only be used by e2print on Nova.
 */
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
comperr("upput");
#if 0
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
#endif
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
		printf("%d,%s", (int)p->n_lval, rnames[p->n_rval]);
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

/*   printf conditional and unconditional branches */
void
cbgen(int o, int lab)
{
	comperr("cbgen");
}

void
myreader(struct interpass *ipole)
{
	if (x2debug)
		printip(ipole);
}

void
mycanon(NODE *p)
{
}

void
myoptim(struct interpass *ip)
{
}

void
rmove(int s, int d, TWORD t)
{
	comperr("rmove");
}

/*
 * For class c, find worst-case displacement of the number of
 * registers in the array r[] indexed by class.
 * Return true if we always can find a color.
 */
int
COLORMAP(int c, int *r)
{
	int num;

	switch (c) {
	case CLASSA:
		num = r[CLASSB] + r[CLASSA];
		return num < 4;
	case CLASSB:
		num = r[CLASSB] + r[CLASSA];
		return num < 2;
	case CLASSC:
		return r[CLASSC] < CREGCNT;
	case CLASSD:
		return r[CLASSD] < DREGCNT;
	}
	return 0; /* XXX gcc */
}

char *rnames[] = {
	"0", "1", "2", "3",
	"050", "051", "052", "053", "054", "055", "056", "057",
	"060", "061", "062", "063", "064", "065", "066", "067",
	"070", "071", "072", "073", "074", "075", "076", "077",
	"041", "040"
};

/*
 * Return a class suitable for a specific type.
 */
int
gclass(TWORD t)
{
	return CLASSA;
}

/*
 * Calculate argument sizes.
 */
void
lastcall(NODE *p)
{
}

/*
 * Special shapes.
 */
int
special(NODE *p, int shape)
{
	return SRNOPE;
}
