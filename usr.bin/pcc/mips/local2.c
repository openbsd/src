/*	$OpenBSD: local2.c,v 1.5 2008/04/11 20:45:52 stefan Exp $	 */
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
#include <string.h>
#include <assert.h>

#include "pass1.h"
#include "pass2.h"

#ifdef TARGET_BIG_ENDIAN
int bigendian = 1;
#else
int bigendian = 0;
#endif

int nargregs = MIPS_O32_NARGREGS;

static int argsiz(NODE *p);

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

        /* round to 8-byte boundary */
        addto += 7;
        addto &= ~7;

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

	/* for the moment, just emit this PIC stuff - NetBSD does it */
	printf("\t.frame %s,%d,%s\n", rnames[FP], ARGINIT/SZCHAR, rnames[RA]);
	printf("\t.set noreorder\n");
	printf("\t.cpload $25\t# pseudo-op to load GOT ptr into $25\n");
	printf("\t.set reorder\n");

	printf("\tsubu %s,%s,%d\n", rnames[SP], rnames[SP], ARGINIT/SZCHAR);
	/* for the moment, just emit PIC stuff - NetBSD does it */
	printf("\t.cprestore 8\t# pseudo-op to store GOT ptr at 8(sp)\n");

	printf("\tsw %s,4(%s)\n", rnames[RA], rnames[SP]);
	printf("\tsw %s,(%s)\n", rnames[FP], rnames[SP]);
	printf("\tmove %s,%s\n", rnames[FP], rnames[SP]);

#ifdef notyet
	/* profiling */
	if (pflag) {
		printf("\t.set noat\n");
		printf("\tmove %s,%s\t# save current return address\n",
		    rnames[AT], rnames[RA]);
		printf("\tsubu %s,%s,8\t# _mcount pops 2 words from stack\n",
		    rnames[SP], rnames[SP]);
		printf("\tjal %s\n", exname("_mcount"));
		printf("\tnop\n");
		printf("\t.set at\n");
	}
#endif

	if (addto)
		printf("\tsubu %s,%s,%d\n", rnames[SP], rnames[SP], addto);

	for (i = ipp->ipp_regs, j = 0; i; i >>= 1, j++)
		if (i & 1)
			fprintf(stdout, "\tsw %s,-%d(%s) # save permanent\n",
				rnames[j], regoff[j], rnames[FP]);

}

void
eoftn(struct interpass_prolog * ipp)
{
	int i, j;
	int addto;

	addto = offcalc(ipp);

	if (ipp->ipp_ip.ip_lbl == 0)
		return;		/* no code needs to be generated */

	/* return from function code */
	for (i = ipp->ipp_regs, j = 0; i; i >>= 1, j++) {
		if (i & 1)
			fprintf(stdout, "\tlw %s,-%d(%s)\n\tnop\n",
				rnames[j], regoff[j], rnames[FP]);
	}

	printf("\taddu %s,%s,%d\n", rnames[SP], rnames[FP], ARGINIT/SZCHAR);
	printf("\tlw %s,%d(%s)\n", rnames[RA], 4-ARGINIT/SZCHAR,  rnames[SP]);
	printf("\tlw %s,%d(%s)\n", rnames[FP], 0-ARGINIT/SZCHAR,  rnames[SP]);

	printf("\tjr %s\n", rnames[RA]);
	printf("\tnop\n");

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
	case ULE:
	case LE:
		str = "blez";
		break;
	case ULT:
	case LT:
		str = "bltz";
		break;
	case UGE:
	case GE:
		str = "bgez";
		break;
	case UGT:
	case GT:
		str = "bgtz";
		break;
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
		str = 0;	/* XXX gcc */
	}

	printf("%s%c", str, f);
}

char *
rnames[] = {
#ifdef USE_GAS
	/* gnu assembler */
	"$zero", "$at", "$2", "$3", "$4", "$5", "$6", "$7",
	"$8", "$9", "$10", "$11", "$12", "$13", "$14", "$15",
	"$16", "$17", "$18", "$19", "$20", "$21", "$22", "$23",
	"$24", "$25",
	"$kt0", "$kt1", "$gp", "$sp", "$fp", "$ra",
	"$2!!$3!!",
	"$4!!$5!!", "$5!!$6!!", "$6!!$7!!", "$7!!$8!!",
	"$8!!$9!!", "$9!!$10!", "$10!$11!", "$11!$12!",
	"$12!$13!", "$13!$14!", "$14!$15!", "$15!$24!", "$24!$25!",
	"$16!$17!", "$17!$18!", "$18!$19!", "$19!$20!",
	"$20!$21!", "$21!$22!", "$22!$23!",
#else
	/* mips assembler */
	 "$zero", "$at", "$v0", "$v1", "$a0", "$a1", "$a2", "$a3",
	"$t0", "$t1", "$t2", "$t3", "$t4", "$t5", "$t6", "$t7",
	"$s0", "$s1", "$s2", "$s3", "$s4", "$s5", "$s6", "$s7",
	"$t8", "$t9",
	"$k0", "$k1", "$gp", "$sp", "$fp", "$ra",
	"$v0!$v1!",
	"$a0!$a1!", "$a1!$a2!", "$a2!$a3!", "$a3!$t0!",
	"$t0!$t1!", "$t1!$t2!", "$t2!$t3!", "$t3!$t4!",
	"$t4!$t5!", "$t5!$t6!", "$t6!$t7!", "$t7!$t8!", "$t8!$t9!",
	"$s0!$s1!", "$s1!$s2!", "$s2!$s3!", "$s3!$s4!",
	"$s4!$s5!", "$s5!$s6!", "$s6!$s7!",
#endif
	"$f0!$f1!", "$f2!$f3!", "$f4!$f5!", "$f6!$f7!",
	"$f8!$f9!", "$f10$f11", "$f12$f13", "$f14$f15",
	"$f16$f17", "$f18$f19", "$f20$f21", "$f22$f23",
	"$f24$f25", "$f26$f27", "$f28$f29", "$f30$f31",
};

char *
rnames_n32[] = {
	/* mips assembler */
	"$zero", "$at", "$v0", "$v1", "$a0", "$a1", "$a2", "$a3",
	"$a4", "$a5", "$a6", "$a7", "$t0", "$t1", "$t2", "$t3",
	"$s0", "$s1", "$s2", "$s3", "$s4", "$s5", "$s6", "$s7",
	"$t8", "$t9",
	"$k0", "$k1", "$gp", "$sp", "$fp", "$ra",
	"$v0!$v1!",
	"$a0!$a1!", "$a1!$a2!", "$a2!$a3!", "$a3!$a4!",
	"$a4!$a5!", "$a5!$a6!", "$a6!$a7!", "$a7!$t0!",
	"$t0!$t1!", "$t1!$t2!", "$t2!$t3!", "$t3!$t8!", "$t8!$t9!",
	"$s0!$s1!", "$s1!$s2!", "$s2!$s3!", "$s3!$s4!",
	"$s4!$s5!", "$s5!$s6!", "$s6!$s7!",
	"$f0!$f1!", "$f2!$f3!", "$f4!$f5!", "$f6!$f7!",
	"$f8!$f9!", "$f10$f11", "$f12$f13", "$f14$f15",
	"$f16$f17", "$f18$f19", "$f20$f21", "$f22$f23",
	"$f24$f25", "$f26$f27", "$f28$f29", "$f30$f31",
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


/*
 * Push a structure on stack as argument.
 */
static void
starg(NODE *p)
{
	//assert(p->n_rval == A1);
	printf("\tsubu %s,%s,%d\n", rnames[SP], rnames[SP], p->n_stsize);
	/* A0 = dest, A1 = src, A2 = len */
	printf("\tmove %s,%s\n", rnames[A0], rnames[SP]);
	printf("\tli %s,%d\t# structure size\n", rnames[A2], p->n_stsize);
	printf("\tsubu %s,%s,16\n", rnames[SP], rnames[SP]);
	printf("\tjal %s\t# structure copy\n", exname("memcpy"));
	printf("\tnop\n");
	printf("\taddiu %s,%s,16\n", rnames[SP], rnames[SP]);
}

/*
 * Structure assignment.
 */
static void
stasg(NODE *p)
{
	assert(p->n_right->n_rval == A1);
	/* A0 = dest, A1 = src, A2 = len */
	printf("\tli %s,%d\t# structure size\n", rnames[A2], p->n_stsize);
	if (p->n_left->n_op == OREG) {
		printf("\taddi %s,%s," CONFMT "\t# dest address\n",
		    rnames[A0], rnames[p->n_left->n_rval],
		    p->n_left->n_lval);
	} else if (p->n_left->n_op == NAME) {
		printf("\tla %s,", rnames[A0]);
		adrput(stdout, p->n_left);
		printf("\n");
	}
	printf("\tsubu %s,%s,16\n", rnames[SP], rnames[SP]);
	printf("\tjal %s\t# structure copy\n", exname("memcpy"));
	printf("\tnop\n");
	printf("\taddiu %s,%s,16\n", rnames[SP], rnames[SP]);
}

static void
shiftop(NODE *p)
{
	NODE *r = p->n_right;
	TWORD ty = p->n_type;

	if (p->n_op == LS && r->n_op == ICON && r->n_lval < 32) {
		expand(p, INBREG, "\tsrl A1,AL,");
		printf(CONFMT "\t# 64-bit left-shift\n", 32 - r->n_lval);
		expand(p, INBREG, "\tsll U1,UL,AR\n");
		expand(p, INBREG, "\tor U1,U1,A1\n");
		expand(p, INBREG, "\tsll A1,AL,AR\n");
	} else if (p->n_op == LS && r->n_op == ICON && r->n_lval < 64) {
		expand(p, INBREG, "\tli A1,0\t# 64-bit left-shift\n");
		expand(p, INBREG, "\tsll U1,AL,");
		printf(CONFMT "\n", r->n_lval - 32);
	} else if (p->n_op == LS && r->n_op == ICON) {
		expand(p, INBREG, "\tli A1,0\t# 64-bit left-shift\n");
		expand(p, INBREG, "\tli U1,0\n");
	} else if (p->n_op == RS && r->n_op == ICON && r->n_lval < 32) {
		expand(p, INBREG, "\tsll U1,UL,");
		printf(CONFMT "\t# 64-bit right-shift\n", 32 - r->n_lval);
		expand(p, INBREG, "\tsrl A1,AL,AR\n");
		expand(p, INBREG, "\tor A1,A1,U1\n");
		if (ty == LONGLONG)
			expand(p, INBREG, "\tsra U1,UL,AR\n");
		else
			expand(p, INBREG, "\tsrl U1,UL,AR\n");
	} else if (p->n_op == RS && r->n_op == ICON && r->n_lval < 64) {
		if (ty == LONGLONG) {
			expand(p, INBREG, "\tsra U1,UL,31\t# 64-bit right-shift\n");
			expand(p, INBREG, "\tsra A1,UL,");
		}else {
			expand(p, INBREG, "\tli U1,0\t# 64-bit right-shift\n");
			expand(p, INBREG, "\tsrl A1,UL,");
		}
		printf(CONFMT "\n", r->n_lval - 32);
	} else if (p->n_op == LS && r->n_op == ICON) {
		expand(p, INBREG, "\tli A1,0\t# 64-bit right-shift\n");
		expand(p, INBREG, "\tli U1,0\n");
	} else {
		comperr("shiftop");
	}
}

/*
 * http://gcc.gnu.org/onlinedocs/gccint/Soft-float-library-routines.html#Soft-float-library-routines
 */
static void
fpemulop(NODE *p)
{
	NODE *l = p->n_left;
	char *ch = NULL;

	if (p->n_op == PLUS && p->n_type == FLOAT) ch = "addsf3";
	else if (p->n_op == PLUS && p->n_type == DOUBLE) ch = "adddf3";
	else if (p->n_op == PLUS && p->n_type == LDOUBLE) ch = "addtf3";

	else if (p->n_op == MINUS && p->n_type == FLOAT) ch = "subsf3";
	else if (p->n_op == MINUS && p->n_type == DOUBLE) ch = "subdf3";
	else if (p->n_op == MINUS && p->n_type == LDOUBLE) ch = "subtf3";

	else if (p->n_op == MUL && p->n_type == FLOAT) ch = "mulsf3";
	else if (p->n_op == MUL && p->n_type == DOUBLE) ch = "muldf3";
	else if (p->n_op == MUL && p->n_type == LDOUBLE) ch = "multf3";

	else if (p->n_op == DIV && p->n_type == FLOAT) ch = "divsf3";
	else if (p->n_op == DIV && p->n_type == DOUBLE) ch = "divdf3";
	else if (p->n_op == DIV && p->n_type == LDOUBLE) ch = "divtf3";

	else if (p->n_op == UMINUS && p->n_type == FLOAT) ch = "negsf2";
	else if (p->n_op == UMINUS && p->n_type == DOUBLE) ch = "negdf2";
	else if (p->n_op == UMINUS && p->n_type == LDOUBLE) ch = "negtf2";

	else if (p->n_op == EQ && l->n_type == FLOAT) ch = "eqsf2";
	else if (p->n_op == EQ && l->n_type == DOUBLE) ch = "eqdf2";
	else if (p->n_op == EQ && l->n_type == LDOUBLE) ch = "eqtf2";

	else if (p->n_op == NE && l->n_type == FLOAT) ch = "nesf2";
	else if (p->n_op == NE && l->n_type == DOUBLE) ch = "nedf2";
	else if (p->n_op == NE && l->n_type == LDOUBLE) ch = "netf2";

	else if (p->n_op == GE && l->n_type == FLOAT) ch = "gesf2";
	else if (p->n_op == GE && l->n_type == DOUBLE) ch = "gedf2";
	else if (p->n_op == GE && l->n_type == LDOUBLE) ch = "getf2";

	else if (p->n_op == LE && l->n_type == FLOAT) ch = "lesf2";
	else if (p->n_op == LE && l->n_type == DOUBLE) ch = "ledf2";
	else if (p->n_op == LE && l->n_type == LDOUBLE) ch = "letf2";

	else if (p->n_op == GT && l->n_type == FLOAT) ch = "gtsf2";
	else if (p->n_op == GT && l->n_type == DOUBLE) ch = "gtdf2";
	else if (p->n_op == GT && l->n_type == LDOUBLE) ch = "gttf2";

	else if (p->n_op == LT && l->n_type == FLOAT) ch = "ltsf2";
	else if (p->n_op == LT && l->n_type == DOUBLE) ch = "ltdf2";
	else if (p->n_op == LT && l->n_type == LDOUBLE) ch = "lttf2";

	else if (p->n_op == SCONV && p->n_type == FLOAT) {
		if (l->n_type == DOUBLE) ch = "truncdfsf2";
		else if (l->n_type == LDOUBLE) ch = "trunctfsf2";
		else if (l->n_type == ULONGLONG) ch = "floatdisf"; /**/
		else if (l->n_type == LONGLONG) ch = "floatdisf";
		else if (l->n_type == LONG) ch = "floatsisf";
		else if (l->n_type == ULONG) ch = "floatunsisf";
		else if (l->n_type == INT) ch = "floatsisf";
		else if (l->n_type == UNSIGNED) ch = "floatunsisf";
	} else if (p->n_op == SCONV && p->n_type == DOUBLE) {
		if (l->n_type == FLOAT) ch = "extendsfdf2";
		else if (l->n_type == LDOUBLE) ch = "trunctfdf2";
		else if (l->n_type == ULONGLONG) ch = "floatunsdidf";
		else if (l->n_type == LONGLONG) ch = "floatdidf";
		else if (l->n_type == LONG) ch = "floatsidf";
		else if (l->n_type == ULONG) ch = "floatunsidf";
		else if (l->n_type == INT) ch = "floatsidf";
		else if (l->n_type == UNSIGNED) ch = "floatunsidf";
	} else if (p->n_op == SCONV && p->n_type == LDOUBLE) {
		if (l->n_type == FLOAT) ch = "extendsftf2";
		else if (l->n_type == DOUBLE) ch = "extenddfdf2";
		else if (l->n_type == ULONGLONG) ch = "floatunsdidf";
		else if (l->n_type == LONGLONG) ch = "floatdidf";
		else if (l->n_type == LONG) ch = "floatsidf";
		else if (l->n_type == ULONG) ch = "floatunssidf";
		else if (l->n_type == INT) ch = "floatsidf";
		else if (l->n_type == UNSIGNED) ch = "floatunsidf";
	} else if (p->n_op == SCONV && p->n_type == ULONGLONG) {
		if (l->n_type == FLOAT) ch = "fixunssfdi";
		else if (l->n_type == DOUBLE) ch = "fixunsdfdi";
		else if (l->n_type == LDOUBLE) ch = "fixunsdfdi";
	} else if (p->n_op == SCONV && p->n_type == LONGLONG) {
		if (l->n_type == FLOAT) ch = "fixsfdi";
		else if (l->n_type == DOUBLE) ch = "fixdfdi";
		else if (l->n_type == LDOUBLE) ch = "fixdfdi";
	} else if (p->n_op == SCONV && p->n_type == LONG) {
		if (l->n_type == FLOAT) ch = "fixsfsi";
		else if (l->n_type == DOUBLE) ch = "fixdfsi";
		else if (l->n_type == LDOUBLE) ch = "fixdfsi";
	} else if (p->n_op == SCONV && p->n_type == ULONG) {
		if (l->n_type == FLOAT) ch = "fixunssfsi";
		else if (l->n_type == DOUBLE) ch = "fixunsdfsi";
		else if (l->n_type == LDOUBLE) ch = "fixunsdfsi";
	} else if (p->n_op == SCONV && p->n_type == INT) {
		if (l->n_type == FLOAT) ch = "fixsfsi";
		else if (l->n_type == DOUBLE) ch = "fixdfsi";
		else if (l->n_type == LDOUBLE) ch = "fixdfsi";
	} else if (p->n_op == SCONV && p->n_type == UNSIGNED) {
		if (l->n_type == FLOAT) ch = "fixunssfsi";
		else if (l->n_type == DOUBLE) ch = "fixunsdfsi";
		else if (l->n_type == LDOUBLE) ch = "fixunsdfsi";
	}

	if (ch == NULL) comperr("ZF: op=0x%x (%d)\n", p->n_op, p->n_op);

	printf("\tjal __%s\t# softfloat operation\n", exname(ch));
	printf("\tnop\n");

	if (p->n_op >= EQ && p->n_op <= GT)
		printf("\tcmp %s,0\n", rnames[V0]);
}

/*
 * http://gcc.gnu.org/onlinedocs/gccint/Integer-library-routines.html#Integer-library-routines
 */
static void
emulop(NODE *p)
{
	char *ch = NULL;

	if (p->n_op == LS && DEUNSIGN(p->n_type) == LONGLONG) ch = "ashldi3";
	else if (p->n_op == LS && (DEUNSIGN(p->n_type) == LONG ||
	    DEUNSIGN(p->n_type) == INT))
		ch = "ashlsi3";

	else if (p->n_op == RS && p->n_type == ULONGLONG) ch = "lshrdi3";
	else if (p->n_op == RS && (p->n_type == ULONG || p->n_type == INT))
		ch = "lshrsi3";

	else if (p->n_op == RS && p->n_type == LONGLONG) ch = "ashrdi3";
	else if (p->n_op == RS && (p->n_type == LONG || p->n_type == INT))
		ch = "ashrsi3";
	
	else if (p->n_op == DIV && p->n_type == LONGLONG) ch = "divdi3";
	else if (p->n_op == DIV && (p->n_type == LONG || p->n_type == INT))
		ch = "divsi3";

	else if (p->n_op == DIV && p->n_type == ULONGLONG) ch = "udivdi3";
	else if (p->n_op == DIV && (p->n_type == ULONG ||
	    p->n_type == UNSIGNED))
		ch = "udivsi3";

	else if (p->n_op == MOD && p->n_type == LONGLONG) ch = "moddi3";
	else if (p->n_op == MOD && (p->n_type == LONG || p->n_type == INT))
		ch = "modsi3";

	else if (p->n_op == MOD && p->n_type == ULONGLONG) ch = "umoddi3";
	else if (p->n_op == MOD && (p->n_type == ULONG ||
	    p->n_type == UNSIGNED))
		ch = "umodsi3";

	else if (p->n_op == MUL && p->n_type == LONGLONG) ch = "muldi3";
	else if (p->n_op == MUL && (p->n_type == LONG || p->n_type == INT))
		ch = "mulsi3";

	else if (p->n_op == UMINUS && p->n_type == LONGLONG) ch = "negdi2";
	else if (p->n_op == UMINUS && p->n_type == LONG) ch = "negsi2";

	else ch = 0, comperr("ZE");
	printf("\tsubu %s,%s,16\n", rnames[SP], rnames[SP]);
	printf("\tjal __%s\t# emulated operation\n", exname(ch));
	printf("\tnop\n");
	printf("\taddiu %s,%s,16\n", rnames[SP], rnames[SP]);
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
	expand(p, 0, "\tsub A1,UL,UR\t# compare 64-bit values (upper)\n");
	if (cb1) {
		printf("\t");
		hopcode(' ', cb1);
		expand(p, 0, "A1");
		printf("," LABFMT "\n", s);
		printf("\tnop\n");
	}
	if (cb2) {
		printf("\t");
		hopcode(' ', cb2);
		expand(p, 0, "A1");
		printf("," LABFMT "\n", e);
		printf("\tnop\n");
	}
	expand(p, 0, "\tsub A1,AL,AR\t# (and lower)\n");
	printf("\t");
	hopcode(' ', o);
	expand(p, 0, "A1");
	printf("," LABFMT "\n", e);
	printf("\tnop\n");
	deflab(s);
}

static void
fpcmpops(NODE *p)
{
	NODE *l = p->n_left;

	switch (p->n_op) {
	case EQ:
		if (l->n_type == FLOAT)
			expand(p, 0, "\tc.eq.s AL,AR\n");
		else
			expand(p, 0, "\tc.eq.d AL,AR\n");
		expand(p, 0, "\tnop\n\tbc1t LC\n");
		break;
	case NE:
		if (l->n_type == FLOAT)
			expand(p, 0, "\tc.eq.s AL,AR\n");
		else
			expand(p, 0, "\tc.eq.d AL,AR\n");
		expand(p, 0, "\tnop\n\tbc1f LC\n");
		break;
	case LT:
		if (l->n_type == FLOAT)
			expand(p, 0, "\tc.lt.s AL,AR\n");
		else
			expand(p, 0, "\tc.lt.d AL,AR\n");
		expand(p, 0, "\tnop\n\tbc1t LC\n");
		break;
	case GE:
		if (l->n_type == FLOAT)
			expand(p, 0, "\tc.lt.s AL,AR\n");
		else
			expand(p, 0, "\tc.lt.d AL,AR\n");
		expand(p, 0, "\tnop\n\tbc1f LC\n");
		break;
	case LE:
		if (l->n_type == FLOAT)
			expand(p, 0, "\tc.le.s AL,AR\n");
		else
			expand(p, 0, "\tc.le.d AL,AR\n");
		expand(p, 0, "\tnop\n\tbc1t LC\n");
		break;
	case GT:
		if (l->n_type == FLOAT)
			expand(p, 0, "\tc.le.s AL,AR\n");
		else
			expand(p, 0, "\tc.le.d AL,AR\n");
		expand(p, 0, "\tnop\n\tbc1f LC\n");
		break;
	}
	printf("\tnop\n\tnop\n");
}

void
zzzcode(NODE * p, int c)
{
	int sz;

	switch (c) {

	case 'C':	/* remove arguments from stack after subroutine call */
		sz = p->n_qual > 16 ? p->n_qual : 16;
		printf("\taddiu %s,%s,%d\n",
		       rnames[SP], rnames[SP], sz);
		break;

	case 'D':	/* long long comparison */
		twollcomp(p);
		break;

	case 'E':	/* emit emulated ops */
		emulop(p);
		break;

	case 'F':	/* emit emulate floating point ops */
		fpemulop(p);
		break;

	case 'G':	/* emit hardware floating-point compare op */
		fpcmpops(p);
		break;

	case 'H':	/* structure argument */
		starg(p);
		break;

	case 'I':		/* high part of init constant */
		if (p->n_name[0] != '\0')
			comperr("named highword");
		fprintf(stdout, CONFMT, (p->n_lval >> 32) & 0xffffffff);
		break;

        case 'O': /* 64-bit left and right shift operators */
		shiftop(p);
		break;

	case 'Q':		/* emit struct assign */
		stasg(p);
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

int
fldexpand(NODE *p, int cookie, char **cp)
{
        CONSZ val;

        if (p->n_op == ASSIGN)
                p = p->n_left;
        switch (**cp) {
        case 'S':
                printf("%d", UPKFSZ(p->n_rval));
                break;
        case 'H':
                printf("%d", UPKFOFF(p->n_rval));
                break;
        case 'M':
        case 'N':
                val = (CONSZ)1 << UPKFSZ(p->n_rval);
                --val;
                val <<= UPKFOFF(p->n_rval);
                printf("0x%llx", (**cp == 'M' ? val : ~val)  & 0xffffffff);
                break;
        default:
                comperr("fldexpand");
        }
        return 1;
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
	switch (p->n_op) {
	case ICON:
		if (p->n_name[0] != '\0') {
			fprintf(fp, "%s", p->n_name);
			if (p->n_lval)
				fprintf(fp, "+%d", (int)p->n_lval);
		} else
			fprintf(fp, CONFMT, p->n_lval & 0xffffffff);
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
        int off = 4 * (hi != 0);
	char *regname = rnames[rval];

        fprintf(fp, "%c%c",
                 regname[off],
                 regname[off + 1]);
        if (regname[off + 2] != '!')
                fputc(regname[off + 2], fp);
        if (regname[off + 3] != '!')
                fputc(regname[off + 3], fp);
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
		if (GCLASS(p->n_rval) == CLASSB || GCLASS(p->n_rval) == CLASSC)
			print_reg64name(stdout, p->n_rval, 1);
		else
			fputs(rnames[p->n_rval], stdout);
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
		conput(io, p);
		return;

	case REG:
		if (GCLASS(p->n_rval) == CLASSB || GCLASS(p->n_rval) == CLASSC)
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
 * If we're big endian, then all OREG loads of a type
 * larger than the destination, must have the
 * offset changed to point to the correct bytes in memory.
 */
static void
offchg(NODE *p)
{
	NODE *l;

	if (p->n_op != SCONV)
		return;

	l = p->n_left;

	if (l->n_op != OREG)
		return;

	switch (l->n_type) {
	case SHORT:
	case USHORT:
		if (DEUNSIGN(p->n_type) == CHAR)
			l->n_lval += 1;
		break;
	case LONG:
	case ULONG:
	case INT:
	case UNSIGNED:
		if (DEUNSIGN(p->n_type) == CHAR)
			l->n_lval += 3;
		else if (DEUNSIGN(p->n_type) == SHORT)
			l->n_lval += 2;
		break;
	case LONGLONG:
	case ULONGLONG:
		if (DEUNSIGN(p->n_type) == CHAR)
			l->n_lval += 7;
		else if (DEUNSIGN(p->n_type) == SHORT)
			l->n_lval += 6;
		else if (DEUNSIGN(p->n_type) == INT ||
		    DEUNSIGN(p->n_type) == LONG)
			l->n_lval += 4;
		break;
	default:
		comperr("offchg: unknown type");
		break;
	}
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
myoptim(struct interpass * ipole)
{
	struct interpass *ip;

	DLIST_FOREACH(ip, ipole, qelem) {
		if (ip->type != IP_NODE)
			continue;
		if (bigendian)
			walkf(ip->ip_node, offchg);
	}
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
			printf(",");
			print_reg64name(stdout, s, 0);
			printf("\t# 64-bit rmove\n");
                        printf("\tmove ");
			print_reg64name(stdout, d, 1); 
			printf(",");
			print_reg64name(stdout, s, 1);
			printf("\n");
                } else {
                        /* copy high word first */
                        printf("\tmove ");
			print_reg64name(stdout, d, 1);
			printf(",");
			print_reg64name(stdout, s, 1);
			printf(" # 64-bit rmove\n");
                        printf("\tmove ");
			print_reg64name(stdout, d, 0);
			printf(",");
			print_reg64name(stdout, s, 0);
			printf("\n");
                }
                break;
	case FLOAT:
	case DOUBLE:
        case LDOUBLE:
		if (t == FLOAT)
			printf("\tmov.s ");
		else
			printf("\tmov.d ");
		print_reg64name(stdout, d, 0);
		printf(",");
		print_reg64name(stdout, s, 0);
		printf("\t# float/double rmove\n");
                break;
        default:
                printf("\tmove %s,%s\t#default rmove\n", rnames[d], rnames[s]);
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
 * 16 floating-point register pairs
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
		return num < 6;
        }
	comperr("COLORMAP");
        return 0; /* XXX gcc */
}

/*
 * Return a class suitable for a specific type.
 */
int
gclass(TWORD t)
{
	if (t == LONGLONG || t == ULONGLONG)
		return CLASSB;
	if (t >= FLOAT && t <= LDOUBLE)
		return CLASSC;
	return CLASSA;
}

/*
 * Calculate argument sizes.
 */
void
lastcall(NODE *p)
{
#ifdef PCC_DEBUG
	if (x2debug)
		printf("lastcall:\n");
#endif

	p->n_qual = 0;
	if (p->n_op != CALL && p->n_op != FORTCALL && p->n_op != STCALL)
		return;
	p->n_qual = argsiz(p->n_right); /* XXX */
}

int
argsiz(NODE *p)
{
	TWORD t;
	int size = 0;
	int sz;

	if (p->n_op == CM) {
		size = argsiz(p->n_left);
		p = p->n_right;
	}

	t = p->n_type;
	if (t < LONGLONG || t == FLOAT || t > BTMASK)
		sz = 4;
	else if (DEUNSIGN(LONGLONG) || t == DOUBLE || t == LDOUBLE)
		sz = 8;
	else if (t == STRTY || t == UNIONTY)
		sz = p->n_stsize;

	if (p->n_type == STRTY || p->n_type == UNIONTY || sz == 4)
		return (size + sz);

	if ((size < 4*nargregs) && (sz == 8) && ((size & 7) != 0))
		sz += 4;

	return (size + sz);
}

/*
 * Special shapes.
 */
int
special(NODE *p, int shape)
{
	int o = p->n_op;
	switch(shape) {
	case SPCON:
		if (o == ICON && p->n_name[0] == 0 &&
		    (p->n_lval & ~0xffff) == 0)
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
	if (strcasecmp(str, "big-endian") == 0) {
		bigendian = 1;
	} else if (strcasecmp(str, "little-endian") == 0) {
		bigendian = 0;
	}
#if 0
	 else if (strcasecmp(str, "ips2")) {
	} else if (strcasecmp(str, "ips2")) {
	} else if (strcasecmp(str, "ips3")) {
	} else if (strcasecmp(str, "ips4")) {
	} else if (strcasecmp(str, "hard-float")) {
	} else if (strcasecmp(str, "soft-float")) {
	} else if (strcasecmp(str, "abi=32")) {
		nargregs = MIPS_O32_NARGREGS;
	} else if (strcasecmp(str, "abi=n32")) {
		nargregs = MIPS_N32_NARGREGS;
	} else if (strcasecmp(str, "abi=64")) {
		nargregs = MIPS_N32_NARGREGS;
	}
#endif
}
