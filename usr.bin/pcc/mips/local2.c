/*	$OpenBSD: local2.c,v 1.2 2007/11/16 08:34:55 otto Exp $	 */
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

/*
 * MIPS port by Jan Enoksson (janeno-1@student.ltu.se) and
 * Simon Olsson (simols-1@student.ltu.se) 2005.
 */

#include <ctype.h>
#include <assert.h>

#include "pass1.h"
#include "pass2.h"

static int argsiz(NODE * p);

void
deflab(int label)
{
	printf(LABFMT ":\n", label);
}

static int regoff[32];
static TWORD ftype;

/*
 * calculate stack size and offsets
 */
static int
offcalc(struct interpass_prolog * ipp)
{
	int i, j, addto;

	addto = p2maxautooff;

	for (i = ipp->ipp_regs, j = 0; i; i >>= 1, j++) {
		if (i & 1) {
			addto += SZINT / SZCHAR;
			regoff[j] = addto;
		}
	}

	return addto;
}

/*
 * Print out the prolog assembler.
 */
void
prologue(struct interpass_prolog * ipp)
{
	int addto;
	int i, j;

	ftype = ipp->ipp_type;
	printf("\t.align 2\n");
	if (ipp->ipp_vis)
		printf("\t.globl %s\n", ipp->ipp_name);
	printf("\t.ent %s\n", ipp->ipp_name);
	printf("%s:\n", ipp->ipp_name);

	addto = offcalc(ipp);
#ifdef USE_GAS
	if (kflag) {
		printf("\t.frame %s,%d,%s\n", rnames[FP], 12, rnames[RA]);
		printf("\t.set noreorder\n");
		printf("\t.cpload $25	# pseudo-op to load GOT ptr into $25\n");
		printf("\t.set reorder\n");
	}
#endif
	printf("\taddi %s,%s,-%d\n", rnames[SP], rnames[SP], 12);
#ifdef USE_GAS
	if (kflag)
		printf("\t.cprestore 8	# pseudo-op to store GOT ptr at 8(sp)\n");
#endif
	printf("\tsw %s,%d(%s)\n", rnames[RA], 4, rnames[SP]);
	printf("\tsw %s,0(%s)\n", rnames[FP], rnames[SP]);
	printf("\tmove %s,%s\n", rnames[FP], rnames[SP]);
	if (addto)
		printf("\taddi %s,%s,-%d\n", rnames[SP], rnames[SP], addto);

	for (i = ipp->ipp_regs, j = 0; i; i >>= 1, j++)
		if (i & 1)
			fprintf(stdout, "\tsw %s,-%d(%s) # save permanents\n",
				rnames[j], regoff[j], rnames[FP]);
}

void
eoftn(struct interpass_prolog * ipp)
{
	int i, j;
	int addto;
	int off;

	addto = offcalc(ipp);
	off = 8;

#ifdef USE_GAS
	if (kflag)
		off += 4;
#endif

	if (ipp->ipp_ip.ip_lbl == 0)
		return;		/* no code needs to be generated */

	/* return from function code */
	for (i = ipp->ipp_regs, j = 0; i; i >>= 1, j++) {
		if (i & 1)
			fprintf(stdout, "\tlw %s,-%d(%s)\n",
				rnames[j], regoff[j], rnames[FP]);
	}

	printf("\tlw %s,%d($sp)\n", rnames[RA], addto + 4);
	printf("\tlw %s,%d($sp)\n", rnames[FP], addto);
	printf("\taddi %s,%s,%d\n", rnames[SP], rnames[SP], addto + off);

	/* struct return needs special treatment */
	if (ftype == STRTY || ftype == UNIONTY) {
		/* XXX - implement struct return support. */
	} else {
		printf("\tjr %s\n", rnames[RA]);
		printf("\tnop\n");
	}
#ifdef USE_GAS
	printf("\t.end %s\n", ipp->ipp_name);
	printf("\t.size %s,.-%s\n", ipp->ipp_name, ipp->ipp_name);
#endif
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
	case EQ:
		str = "beqz";	/* pseudo-op */
		break;
	case NE:
		str = "bnez";	/* pseudo-op */
		break;
	case LE:
		str = "blez";
		break;
	case LT:
		str = "bltz";
		break;
	case GE:
		str = "bgez";
		break;
	case GT:
		str = "bgtz";
		break;
	case PLUS:
		str = "addu";
		break;
	case MINUS:
		str = "subu";
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
		str = 0;	/* XXX gcc */
	}

	printf("%s%c", str, f);
}

char *
rnames[] = {		/* keyed to register number tokens */
#ifdef USE_GAS
	/* gnu assembler */
	"$zero", "$at", "$2", "$3", "$4", "$5", "$6", "$7",
	"$8", "$9", "$10", "$11", "$12", "$13", "$14", "$15",
	"$16", "$17", "$18", "$19", "$20", "$21", "$22", "$23",
	"$24", "$25",
	"$kt0", "$kt1", "$gp", "$sp", "$fp", "$ra",
	"$2\0$3\0",
	"$4\0$5\0", "$5\0$6\0", "$6\0$7\0", "$7\0$8\0",
	"$8\0$9", "$9\0$10", "$10$11", "$11$12", "$12$13", "$13$14", "$14$15",
	"$24$25",
	"$16$17", "$17$18", "$18$19", "$19$20", "$2021", "$21$22", "$22$23",
#else
	/* mips assembler */
	 "$zero", "$at", "$v0", "$v1", "$a0", "$a1", "$a2", "$a3",
#if defined(MIPS_N32) || defined(MIPS_N64)
	"$a4", "$a5", "$a6", "$a7", "$t0", "$t1", "$t2", "$t3",
#else
	"$t0", "$t1", "$t2", "$t3", "$t4", "$t5", "$t6", "$t7",
#endif
	"$s0", "$s1", "$s2", "$s3", "$s4", "$s5", "$s6", "$s7",
	"$t8", "$t9",
	"$k0", "$k1", "$gp", "$sp", "$fp", "$ra",
	"$v0$v1",
	"$a0a1", "$a1$a2", "$a2$a3", "$a3$t0",
	"$t0t1", "$t1$t2", "$t2$t3", "$t3$t4", "$t4t5", "$t5t6", "$t6t7",
	"$t8t9",
	"$s0s1", "$s1$s2", "$s2$s3", "$s3$s4", "$s4$s5", "$s5$s6", "$s6s7",
#endif
	"$f0", "$f1", "$f2",
};


int
tlen(NODE *p)
{
	switch (p->n_type) {
	case CHAR:
	case UCHAR:
		return (1);

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
			comperr("tlen type %d not pointer");
		return SZPOINT(p->n_type) / SZCHAR;
	}
}


#if 0
/*
 * Push a structure on stack as argument.
 * the scratch registers are already free here
 */
static void
starg(NODE * p)
{
	if (p->n_left->n_op == REG && p->n_left->n_type == PTR + STRTY)
		return;		/* already on stack */
}
#endif

/*
 * Structure assignment.
 */
static void
stasg(NODE *p)
{
	assert(p->n_right->n_rval == A1);
	/* A0 = dest, A1 = src, A2 = len */
	printf("\tli %s,%d	# structure size\n",
	   rnames[A2], p->n_stsize);
	if (p->n_left->n_op == OREG) {
		printf("\taddi %s,%s," CONFMT "	# dest address\n",
		    rnames[A0], rnames[p->n_left->n_rval],
		    p->n_left->n_lval);
	} else if (p->n_left->n_op == NAME) {
		printf("\tla %s,", rnames[A0]);
		adrput(stdout, p->n_left);
		printf("\n");
	}
	printf("\taddi %s,%s,-16\n", rnames[SP], rnames[SP]);
	printf("\tbl %s		# structure copy\n",
	    exname("memcpy"));
	printf("\tnop\n");
	printf("\taddi %s,%s,16\n", rnames[SP], rnames[SP]);
}

static void
llshiftop(NODE *p)
{
	assert(p->n_right->n_op == ICON);

	if (p->n_op == LS && p->n_right->n_lval < 32) {
		expand(p, INBREG, "\tsrl A1,AL,32-AR        ; 64-bit left-shift\n");
		expand(p, INBREG, "\tsll U1,UL,AR\n");
		expand(p, INBREG, "\tor U1,U1,A1\n");
		expand(p, INBREG, "\tsll A1,AL,AR\n");
	} else if (p->n_op == LS) {
		expand(p, INBREG, "\tli A1,0    ; 64-bit left-shift\n");
		expand(p, INBREG, "\tsll U1,AL,AR-32\n");
	} else if (p->n_op == RS && p->n_right->n_lval < 32) {
		expand(p, INBREG, "\tsll U1,UL,32-AR        ; 64-bit right-shift\n");
		expand(p, INBREG, "\tsrl A1,AL,AR\n");
		expand(p, INBREG, "\tor A1,A1,U1\n");
		expand(p, INBREG, "\tsrl U1,UL,AR\n");
	} else if (p->n_op == RS) {
		expand(p, INBREG, "\tli U1,0    ; 64-bit right-shift\n");
		expand(p, INBREG, "\tsrl A1,UL,AR-32\n");
	}
}

/*
 *  Emulate unsupported instruction.
 */
static void
emulop(NODE *p)
{
	char *ch;

	if (p->n_op == DIV && p->n_type == ULONGLONG) ch = "udiv";
	else if (p->n_op == DIV) ch = "div";
	else if (p->n_op == MUL) ch = "mul";
	else if (p->n_op == MOD && p->n_type == ULONGLONG) ch = "umod";
	else if (p->n_op == MOD) ch = "mod";
	else if (p->n_op == RS && p->n_type == ULONGLONG) ch = "lshr";
	else if (p->n_op == RS) ch = "ashr";
	else if (p->n_op == LS) ch = "ashl";
	else ch = 0, comperr("ZE");
	printf("\taddi %s,%s,-16\n", rnames[SP], rnames[SP]);
	printf("\tbl __%sdi3\n", ch);
	printf("\tnop\n");
	printf("\taddi %s,%s,16\n", rnames[SP], rnames[SP]);
}

void
zzzcode(NODE * p, int c)
{
	int sz;

	switch (c) {

	case 'C':	/* remove arguments from stack after subroutine call */
		sz = p->n_qual > 16 ? p->n_qual : 16;
		printf("\taddi %s,%s,%d\n",
		       rnames[STKREG], rnames[STKREG], sz);
		break;

	case 'I':		/* high part of init constant */
		if (p->n_name[0] != '\0')
			comperr("named highword");
		fprintf(stdout, CONFMT, (p->n_lval >> 32) & 0xffffffff);
		break;

	case 'Q':		/* emit struct assign */
		stasg(p);
		break;

        case 'O': /* 64-bit left and right shift operators */
		llshiftop(p);
		break;

	case 'E':	/* emit emulated ops */
		emulop(p);
		break;

	default:
		comperr("zzzcode %c", c);
	}
}

/* ARGSUSED */
int
rewfld(NODE * p)
{
	return (1);
}

/*
 * Does the bitfield shape match?
 */
int
flshape(NODE * p)
{
	int o = p->n_op;

	if (o == OREG || o == REG || o == NAME)
		return SRDIR;	/* Direct match */
	if (o == UMUL && shumul(p->n_left))
		return SROREG;	/* Convert into oreg */
	return SRREG;		/* put it into a register */
}

/* INTEMP shapes must not contain any temporary registers */
/* XXX should this go away now? */
int
shtemp(NODE * p)
{
	return 0;
#if 0
	int r;

	if (p->n_op == STARG)
		p = p->n_left;

	switch (p->n_op) {
	case REG:
		return (!istreg(p->n_rval));

	case OREG:
		r = p->n_rval;
		if (R2TEST(r)) {
			if (istreg(R2UPK1(r)))
				return (0);
			r = R2UPK2(r);
		}
		return (!istreg(r));

	case UMUL:
		p = p->n_left;
		return (p->n_op != UMUL && shtemp(p));
	}

	if (optype(p->n_op) != LTYPE)
		return (0);
	return (1);
#endif
}

void
adrcon(CONSZ val)
{
	printf(CONFMT, val);
}

void
conput(FILE * fp, NODE * p)
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

/* ARGSUSED */
void
insput(NODE * p)
{
	comperr("insput");
}

/*
 * Print lower or upper name of 64-bit register.
 */
static void
print_reg64name(FILE *fp, int rval, int hi)
{
        int off = 3 * (hi != 0);

        fprintf(fp, "%c%c",
                 rnames[rval][off],
                 rnames[rval][off + 1]);
        if (rnames[rval][off + 2])
                fputc(rnames[rval][off + 2], fp);
}

/*
 * Write out the upper address, like the upper register of a 2-register
 * reference, or the next memory location.
 */
void
upput(NODE * p, int size)
{

	size /= SZCHAR;
	switch (p->n_op) {
	case REG:
		print_reg64name(stdout, p->n_rval, 1);
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
adrput(FILE * io, NODE * p)
{
	int r;
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
		r = p->n_rval;

		if (p->n_lval)
			fprintf(io, "%d", (int) p->n_lval);

		fprintf(io, "(%s)", rnames[p->n_rval]);
		return;
	case ICON:
		/* addressable value of the constant */
		//fputc('$', io);
		conput(io, p);
		return;

	case MOVE:
	case REG:
		if (DEUNSIGN(p->n_type) == LONGLONG) 
			print_reg64name(io, p->n_rval, 0);
		else
			fputs(rnames[p->n_rval], io);
		return;

	default:
		comperr("illegal address, op %d, node %p", p->n_op, p);
		return;

	}
}

/* printf conditional and unconditional branches */
void
cbgen(int o, int lab)
{
}

void
myreader(struct interpass * ipole)
{
}

/*
 * Remove some PCONVs after OREGs are created.
 */
static void
pconv2(NODE * p)
{
	NODE *q;

	if (p->n_op == PLUS) {
		if (p->n_type == (PTR | SHORT) || p->n_type == (PTR | USHORT)) {
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
mycanon(NODE * p)
{
	walkf(p, pconv2);
}

void
myoptim(struct interpass * ip)
{
}

/*
 * Move data between registers.  While basic registers aren't a problem,
 * we have to handle the special case of overlapping composite registers.
 */
void
rmove(int s, int d, TWORD t)
{
        switch (t) {
        case LONGLONG:
        case ULONGLONG:
                if (s == d+1) {
                        /* dh = sl, copy low word first */
                        printf("\tmove ");
			print_reg64name(stdout, d, 0);
			print_reg64name(stdout, s, 0);
                        printf("\tmove ");
			print_reg64name(stdout, d, 1); 
			print_reg64name(stdout, s, 1);
                } else {
                        /* copy high word first */
                        printf("\tmove ");
			print_reg64name(stdout, d, 1);
			print_reg64name(stdout, s, 1);
                        printf("\tmove ");
			print_reg64name(stdout, d, 0);
			print_reg64name(stdout, s, 0);
                }
               	printf("\n");
                break;
        case LDOUBLE:
#ifdef notdef
                /* a=b()*c(); will generate this */
                comperr("bad float rmove: %d %d", s, d);
#endif
                break;
        default:
                printf("\tmove %s,%s\n", rnames[d], rnames[s]);
        }
}


/*
 * For class c, find worst-case displacement of the number of
 * registers in the array r[] indexed by class.
 *
 * On MIPS, we have:
 *
 * 32 32-bit registers (8 reserved)
 * 26 64-bit pseudo registers (1 unavailable)
 * 3? floating-point registers
 */
int
COLORMAP(int c, int *r)
{
	int num = 0;

        switch (c) {
        case CLASSA:
                num += r[CLASSA];
                num += 2*r[CLASSB];
                return num < 24;
        case CLASSB:
                num += 2*r[CLASSB];
                num += r[CLASSA];
                return num < 25;
	case CLASSC:
		num += r[CLASSC];
		return num < 3;
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
	return CLASSA;
}

/*
 * Calculate argument sizes.
 */
void
lastcall(NODE * p)
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
	op->n_qual = size;	/* XXX */
}

static int
argsiz(NODE * p)
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

/*
 * Special shapes.
 */
int
special(NODE * p, int shape)
{
	return SRNOPE;
}
