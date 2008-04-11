/*	$OpenBSD: local2.c,v 1.2 2008/04/11 20:45:52 stefan Exp $	*/
/*
 * Copyright (c) 2008 David Crawshaw <david@zentus.com>
 * 
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "pass1.h"
#include "pass2.h"


char *
rnames[] = {
	/* "\%g0", always zero, removed due to 31-element class limit */
	        "\%g1", "\%g2", "\%g3", "\%g4", "\%g5", "\%g6", "\%g7",
	"\%o0", "\%o1", "\%o2", "\%o3", "\%o4", "\%o5", "\%o6", "\%o7",
	"\%l0", "\%l1", "\%l2", "\%l3", "\%l4", "\%l5", "\%l6", "\%l7",
	"\%i0", "\%i1", "\%i2", "\%i3", "\%i4", "\%i5", "\%i6", "\%i7",

	"\%f0",  "\%f1",  "\%f2",  "\%f3",  "\%f4",  "\%f5",  "\%f6",  "\%f7",
	"\%f8",  "\%f9",  "\%f10", "\%f11", "\%f12", "\%f13", "\%f14", "\%f15",
	"\%f16", "\%f17", "\%f18", "\%f19", "\%f20", "\%f21", "\%f22", "\%f23",
	"\%f24", "\%f25", "\%f26", "\%f27", "\%f28", "\%f29", "\%f30",
	/*, "\%f31" XXX removed due to 31-element class limit */

	"\%f0",  "\%f2",  "\%f4",  "\%f6",  "\%f8",  "\%f10", "\%f12", "\%f14",
	"\%f16", "\%f18", "\%f20", "\%f22", "\%f24", "\%f26", "\%f28", "\%f30",

	"\%sp", "\%fp",
};

void
deflab(int label)
{
	printf(LABFMT ":\n", label);
}

void
prologue(struct interpass_prolog *ipp)
{
	int i, stack;

	stack = V9RESERVE + V9STEP(p2maxautooff);

	for (i=ipp->ipp_regs; i; i >>= 1)
		if (i & 1)
			stack += 16;

	/* TODO printf("\t.proc %d\n"); */
	printf("\t.global %s\n", ipp->ipp_name);
	printf("\t.align 4\n");
	printf("%s:\n", ipp->ipp_name);
	if (SIMM13(stack))
		printf("\tsave %%sp,-%d,%%sp\n", stack);
	else {
		printf("\tsetx -%d,%%g4,%%g1\n", stack);
		printf("\tsave %%sp,%%g1,%%sp\n");
	}
}

void
eoftn(struct interpass_prolog *ipp)
{
	printf("\tret\n");
	printf("\trestore\n");
	printf("\t.type %s,#function\n", ipp->ipp_name);
	printf("\t.size %s,(.-%s)\n", ipp->ipp_name, ipp->ipp_name);
}

void
hopcode(int f, int o)
{
	char *str;

	switch (o) {
		case EQ:        str = "brz"; break;
		case NE:        str = "brnz"; break;
		case ULE:
		case LE:        str = "brlez"; break;
		case ULT:
		case LT:        str = "brlz";  break;
		case UGE:
		case GE:        str = "brgez"; break;
		case UGT:
		case GT:        str = "brgz";  break;
		case PLUS:      str = "add"; break;
		case MINUS:     str = "sub"; break;
		case AND:       str = "and"; break;
		case OR:        str = "or";  break;
		case ER:        str = "xor"; break;
		default:
			comperr("unknown hopcode: %d (with %c)", o, f);
			return;
	}

	printf("%s%c", str, f);
}

int
tlen(NODE *p)
{
	switch (p->n_type) {
		case CHAR:
		case UCHAR:
			return 1;
		case SHORT:
		case USHORT:
			return (SZSHORT / SZCHAR);
		case FLOAT:
			return (SZFLOAT / SZCHAR);
		case DOUBLE:
			return (SZDOUBLE / SZCHAR);
		case INT:
		case UNSIGNED:
			return (SZINT / SZCHAR);
		case LONG:
		case ULONG:
		case LONGLONG:
		case ULONGLONG:
			return SZLONGLONG / SZCHAR;
		default:
			if (!ISPTR(p->n_type))
				comperr("tlen type unknown: %d");
			return SZPOINT(p->n_type) / SZCHAR;
	}
}

void
zzzcode(NODE * p, int c)
{
	char *str;
	NODE *l, *r;
	l = p->n_left;
	r = p->n_right;

	switch (c) {

	case 'A':	/* Add const. */
		if (ISPTR(l->n_type) && l->n_rval == FP)
			r->n_lval += V9BIAS;

		if (SIMM13(r->n_lval))
			expand(p, 0, "\tadd AL,AR,A1\t\t! add const\n");
		else
			expand(p, 0, "\tsetx AR,A3,A2\t\t! add const\n"
			             "\tadd AL,A2,A1\n");
		break;
	case 'B':	/* Subtract const. */
		if (ISPTR(l->n_type) && l->n_rval == FP)
			r->n_lval -= V9BIAS;

		if (SIMM13(r->n_lval))
			expand(p, 0, "\tsub AL,AR,A1\t\t! subtract const\n");
		else
			expand(p, 0, "\tsetx AR,A3,A2\t\t! subtract const\n"
			             "\tsub AL,A2,A1\n");
		break;
	case 'C':	/* Load constant to register. */
		if (ISPTR(p->n_type))
			expand(p, 0,
				"\tsethi %h44(AL),A1\t\t! load label\n"
				"\tor A1,%m44(AL),A1\n"
				"\tsllx A1,12,A1\n"
				"\tor A1,%l44(AL),A1\n");
		else if (SIMM13(p->n_lval))
			expand(p, 0, "\tor %g0,AL,A1\t\t\t! load const\n");
		else
			expand(p, 0, "\tsetx AL,A2,A1\t\t! load const\n");
		break;
	case 'F':	/* Floating-point comparison, cf. hopcode(). */
		switch (p->n_op) {
			case EQ:        str = "fbe"; break;
			case NE:        str = "fbne"; break;
			case ULE:
			case LE:        str = "fbule"; break;
			case ULT:
			case LT:        str = "fbul";  break;
			case UGE:
			case GE:        str = "fbuge"; break;
			case UGT:
			case GT:        str = "fbug";  break;
			/* XXX
			case PLUS:      str = "add"; break;
			case MINUS:     str = "sub"; break;
			case AND:       str = "and"; break;
			case OR:        str = "or";  break;
			case ER:        str = "xor"; break;*/
			default:
				comperr("unknown float code: %d", p->n_op);
				return;
		}
		printf(str);
		break;

	case 'Q':	/* Structure assignment. */
		/* TODO Check if p->n_stsize is small and use a few ldx's
		        to move the struct instead of memcpy. The equiv.
			could be done on all the architectures. */
		if (l->n_rval != O0)
			printf("\tmov %s,%s\n", rnames[l->n_rval], rnames[O0]);
		if (SIMM13(p->n_stsize))
			printf("\tor %%g0,%d,%%o2\n", p->n_stsize);
		else
			printf("\tsetx %d,%%g1,%%o2\n", p->n_stsize);
		printf("\tcall memcpy\t\t\t! struct assign (dest, src, len)\n");
		printf("\tnop\n");
		break;
	default:
		cerror("unknown zzzcode call: %c", c);
	}
}

int
rewfld(NODE * p)
{
	return (1);
}

int
fldexpand(NODE *p, int cookie, char **cp)
{
	printf("XXX fldexpand called\n"); /* XXX */
	return 1;
}

int
flshape(NODE * p)
{
	return SRREG;
}

int
shtemp(NODE * p)
{
	return 0;
}


void
adrcon(CONSZ val)
{
}

void
conput(FILE * fp, NODE * p)
{
	if (p->n_op != ICON) {
		comperr("conput got bad op: %s", copst(p->n_op));
		return;
	}

	if (p->n_name[0] != '\0') {
		fprintf(fp, "%s", p->n_name);
		if (p->n_lval > 0)
			fprintf(fp, "+");
		if (p->n_lval)
			fprintf(fp, "%lld", p->n_lval);
	} else
		fprintf(fp, CONFMT, p->n_lval);
}

void
insput(NODE * p)
{
	comperr("insput");
}

void
upput(NODE *p, int size)
{
	comperr("upput");
}

void
adrput(FILE * io, NODE * p)
{
	int64_t off;

	if (p->n_op == FLD) {
		printf("adrput a FLD\n");
		p = p->n_left;
	}

	if (p->n_op == UMUL && p->n_right == 0)
		p = p->n_left;

	off = p->n_lval;

	switch (p->n_op) {
	case NAME:
		if (p->n_name[0] != '\0')
			fputs(p->n_name, io);
		if (off > 0)
			fprintf(io, "+");
		if (off != 0)
			fprintf(io, CONFMT, off);
		return;
	case OREG:
		fprintf(io, "%s", rnames[p->n_rval]);
		if (p->n_rval == FP)
			off += V9BIAS;
		if (p->n_rval == SP)
			off += V9BIAS + V9RESERVE;
		if (off > 0)
			fprintf(io, "+");
		if (off)
			fprintf(io, "%lld", off);
		return;
	case ICON:
		/* addressable value of the constant */
		conput(io, p);
		return;
	case REG:
		fputs(rnames[p->n_rval], io);
		return;
	case FUNARG:
		/* We do something odd and store the stack offset in n_rval. */
		fprintf(io, "%d", V9BIAS + V9RESERVE + p->n_rval);
		return;
	default:
		comperr("bad address, %s, node %p", copst(p->n_op), p);
		return;
	}
}

void
cbgen(int o, int lab)
{
}

void
myreader(struct interpass * ipole)
{
}

void
mycanon(NODE * p)
{
}

void
myoptim(struct interpass * ipole)
{
}

void
rmove(int s, int d, TWORD t)
{
	printf("\tmov %s,%s\t\t\t! rmove()\n", rnames[s], rnames[d]);
}

int
gclass(TWORD t)
{
	if (t == FLOAT)
		return CLASSB;
	if (t == DOUBLE)
		return CLASSC;
	return CLASSA;
}

void
lastcall(NODE *p)
{
}

int
special(NODE *p, int shape)
{
	return SRNOPE;
}

void mflags(char *str)
{
}

int
COLORMAP(int c, int *r)
{
	int num=0;

	switch (c) {
		case CLASSA:
			num += r[CLASSA];
			return num < 32;
		case CLASSB:
			num += r[CLASSB];
			num += 2*r[CLASSC];
			return num < 32;;
		case CLASSC:
			num += r[CLASSC];
			num += 2*r[CLASSB];
			return num < 17;
		case CLASSD:
			return 0;
		default:
			comperr("COLORMAP: unknown class: %d", c);
			return 0;
	}
}
