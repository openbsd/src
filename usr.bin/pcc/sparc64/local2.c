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

/*
 * Many arithmetic instructions take 'reg_or_imm' in SPARCv9, where imm
 * means we can use a signed 13-bit constant (simm13). This gives us a
 * shortcut for small constants, instead of loading them into a register.
 * Special handling is required because 13 bits lies between SSCON and SCON.
 */
#define SIMM13(val) (val < 4096 && val > -4097)

char *
rnames[] = {
	/* "\%g0", always zero, removed due to 31-element class limit */
	        "\%g1", "\%g2", "\%g3", "\%g4", "\%g5", "\%g6", "\%g7",
	"\%o0", "\%o1", "\%o2", "\%o3", "\%o4", "\%o5", "\%o6", "\%o7",
	"\%l0", "\%l1", "\%l2", "\%l3", "\%l4", "\%l5", "\%l6", "\%l7",
	"\%i0", "\%i1", "\%i2", "\%i3", "\%i4", "\%i5", "\%i6", "\%i7",

	"\%sp", "\%fp",

	"\%f0",  "\%f1",  "\%f2",  "\%f3",  "\%f4",  "\%f5",  "\%f6",  "\%f7",
	"\%f8",  "\%f9",  "\%f10", "\%f11", "\%f12", "\%f13", "\%f14", "\%f15",
	"\%f16", "\%f17", "\%f18", "\%f19", "\%f20", "\%f21", "\%f22", "\%f23",
	"\%f24", "\%f25", "\%f26", "\%f27", "\%f28", "\%f29", "\%f30"
	/*, "\%f31" XXX removed due to 31-element class limit */
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

	/*
	 * SPARCv9 has a 2047 bit stack bias. Looking at output asm from gcc
	 * suggests this means we need a base 192 bit offset for %sp. Further
	 * steps need to be 8-byte aligned.
	 */
	stack = 192 + p2maxautooff + (p2maxautooff % 8);

	for (i=ipp->ipp_regs; i; i >>= 1)
		if (i & 1)
			stack += 8;

	/* TODO printf("\t.proc %d\n"); */
	printf("\t.global %s\n", ipp->ipp_name);
	printf("\t.align 4\n");
	printf("%s:\n", ipp->ipp_name);
	printf("\tsave %%sp,-%d,%%sp\n", stack);
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
		case DOUBLE:
			return (SZDOUBLE / SZCHAR);
		case INT:
		case UNSIGNED:
		case LONG:
		case ULONG:
			return (SZINT / SZCHAR);
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
	switch (c) {

	case 'A':	/* Load constant to register. */
		if (!ISPTR(p->n_type) && SIMM13(p->n_lval))
			expand(p, 0, "\tor %g0,AL,A1\t\t\t! load const\n");
		else {
			expand(p, 0,
				"\tsethi %h44(AL),A1\t\t! load const\n"
				"\tor A1,%m44(AL),A1\n"
				"\tsllx A1,12,A1\n"
				"\tor A1,%l44(AL),A1\n");
		}
		break;
	case 'B':	/* Subtract const, store in temp. */
		/*
		 * If we are dealing with a stack location, SPARCv9 has a
		 * stack offset of +2047 bits. This is mostly handled by
		 * notoff(), but when passing as an argument this op is used.
		 */
		if (ISPTR(p->n_left->n_type) && p->n_left->n_rval == FP)
			p->n_right->n_lval -= 2047;

		if (SIMM13(p->n_right->n_lval))
			expand(p, 0, "\tsub AL,AR,A1\t\t! subtract const");
		else {
			expand(p, 0,
				"\tsethi %h44(AR),%g1\t\t! subtract const\n"
				"\tor %g1,%m44(AR),%g1\n"
				"\tsllx %g1,12,%g1\n"
				"\tor %g1,%l44(AR),%g1\n"
				"\tsub AL,%g1,A1\n");
		}
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
		if (p->n_lval < 0) {
			comperr("conput: negative offset (%lld) on label %s\n",
				p->n_lval, p->n_name);
			return;
		}
		if (p->n_lval)
			fprintf(fp, "+%lld", p->n_lval);
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
		/* SPARCv9 stack bias adjustment. */
		if (p->n_rval == FP)
			off += 2047;
		if (off > 0)
			fprintf(io, "+");
		if (off)
			fprintf(io, "%lld", off);
		return;
	case ICON:
		/* addressable value of the constant */
		conput(io, p);
		return;
	case MOVE:
	case REG:
		fputs(rnames[p->n_rval], io);
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
	return (t == FLOAT || t == DOUBLE || t == LDOUBLE) ? CLASSC : CLASSA;
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
			return 0;
		case CLASSC:
			num += r[CLASSC];
			return num < 32;
		default:
			comperr("COLORMAP: unknown class");
			return 0;
	}
}
