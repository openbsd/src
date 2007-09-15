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

/*
 * MIPS port by Jan Enoksson (janeno-1@student.ltu.se) and
 * Simon Olsson (simols-1@student.ltu.se) 2005.
 */

# include "pass2.h"
# include <ctype.h>

void acon(NODE *p);
int argsize(NODE *p);
void genargs(NODE *p);
static void sconv(NODE *p);
void branchfunc(NODE *p);
void offchg(NODE *p);

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

static int regoff[32];
static TWORD ftype;

/*
 * Print out the prolog assembler.
 * addto and regoff are already calculated.
 */
static void
prtprolog(struct interpass_prolog *ipp, int addto)
{
    int i, j;

    printf("	addi $sp, $sp, -%d\n", addto + 8);
    printf("	sw $ra, %d($sp)\n", addto + 4);
    printf("	sw $fp, %d($sp)\n", addto);
    printf("	addi $fp, $sp, %d\n", addto);

    for (i = ipp->ipp_regs, j = 0; i; i >>= 1, j++)
	if (i & 1)
	    fprintf(stdout, "	sw %s, -%d(%s)\n",
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
    if (addto >= AUTOINIT)
	addto -= AUTOINIT;
    addto /= SZCHAR;

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
    int addto;

    addto = offcalc(ipp);
	
    if (ipp->ipp_ip.ip_lbl == 0)
	return; /* no code needs to be generated */
	
    /* return from function code */
    for (i = ipp->ipp_regs, j = 0; i ; i >>= 1, j++) {
	if (i & 1)
	    fprintf(stdout, "	lw %s, -%d(%s)\n",
		    rnames[j], regoff[j], rnames[FPREG]);
    }

    printf("	lw $ra, %d($sp)\n", addto + 4);
    printf("	lw $fp, %d($sp)\n", addto);	
    printf("	addi $sp, $sp, %d\n", addto + 8);
	
    /* struct return needs special treatment */
    if (ftype == STRTY || ftype == UNIONTY) {
	/* XXX - implement struct return support. */
    } else {
	printf("	jr $ra\n	nop\n");
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
	str = 0; /* XXX gcc */
    }

    printf("%s%c", str, f);
}

char *
rnames[] = {  /* keyed to register number tokens */
    "$t0", "$t1", "$t2", "$t3", "$t4", "$t5", "$t6", "$t7", "$t8",
    "$t9", "$v0", "$v1", "$zero", "$at", "$a0", "$a1", "$a2", "$a3",
    "$s0", "$s1", "$s2", "$s3", "$s4", "$s5", "$s6", "$s7", "$k0",
    "$k1", "$gp", "$sp", "$fp", "$ra", 
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
	    comperr("tlen type %d not pointer");
	return SZPOINT(p->n_type)/SZCHAR;
    }
}


/*
 * Push a structure on stack as argument.
 * the scratch registers are already free here
 */
static void
starg(NODE *p)
{
    FILE *fp = stdout;

    if (p->n_left->n_op == REG && p->n_left->n_type == PTR+STRTY)
	return; /* already on stack */

}

void
zzzcode(NODE *p, int c)
{
    NODE *r;

    switch (c) {
    case 'A': /* Set the right offset for SCON OREG to REG */
	offchg(p);
	break;
	
    case 'B':
	/*
	 * Function arguments
	 */

	break;

    case 'C':  /* remove arguments from stack after subroutine call */
	printf("	addi %s, %s, %d\n",
	       rnames[STKREG], rnames[STKREG], (p->n_rval + 4) * 4);
	break;

    case 'H': /* Fix correct order of sub from stack */
	/* Check which leg was evaluated first */
	if ((p->n_su & DORIGHT) == 0)
	    putchar('r');
	break;

    case 'I': /* high part of init constant */
	if (p->n_name[0] != '\0')
	    comperr("named highword");
	fprintf(stdout, CONFMT, (p->n_lval >> 32) & 0xffffffff);
	break;
	
    case 'Q':     /* Branch instructions */
	branchfunc(p);
	break;
		
    default:
	comperr("zzzcode %c", c);
    }
}

/* set up temporary registers */
void
setregs()
{
    /* 12 free regs on the mips (0-9, temporary and 10-11 is v0 and v1). */
    fregs = 12;	
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
    printf(CONFMT, val);
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

    size /= SZCHAR;
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
	fprintf(stdout, CONFMT, p->n_lval >> 32);
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
	if (p->n_name[0] != '\0')
	    fputs(p->n_name, io);
	if (p->n_lval != 0)
	    fprintf(io, "+" CONFMT, p->n_lval);
	return;

    case OREG:
	r = p->n_rval;
		
	if (p->n_lval)
	    fprintf(io, "%d", (int)p->n_lval);
		
	fprintf(io, "(%s)", rnames[p->n_rval]);
	return;
    case ICON:
	/* addressable value of the constant */
	//fputc('$', io);
	conput(io, p);
	return;

    case MOVE:
    case REG:
	fprintf(io, "%s", rnames[p->n_rval]);
	return;

    default:
	comperr("illegal address, op %d, node %p", p->n_op, p);
	return;

    }
}

/* This function changes the offset of a OREG when doing a type cast. */
void
offchg(NODE *p)
{

    if (p->n_op != SCONV) {
	comperr("illegal offchg");
    }

#ifndef RTOLBYTES
    /* change the offset depending on source and target types */
    switch(p->n_left->n_type) {
    case SHORT:
    case USHORT:
 	if (p->n_type == CHAR || p->n_type == UCHAR) {
 	    p->n_left->n_lval += 1;
 	}
 	break;
	
    case UNSIGNED:
    case ULONG:
    case INT:
    case LONG:
	if (p->n_type == CHAR || p->n_type == UCHAR) {
	    p->n_left->n_lval += 3;
	} else if (p->n_type == SHORT || p->n_type == USHORT) {
	    p->n_left->n_lval += 2;
	}
	break;

	/* This code is not tested!
    case LONGLONG:
    case ULONGLONG:
	if (p->n_type == CHAR || p->n_type == UCHAR) {
	    p->n_lval += 7;
	} else if (p->n_type == SHORT || p->n_type == USHORT) {
	    p->n_lval += 6;
	} else if (p->n_type == UNSIGNED || p->n_type == ULONG || 
		   p->n_type == INT || p->n_type == LONG) {
	    
	    p->n_lval += 4;
	}
	break;
	*/
    }
#endif

    /* print the code for the OREG */
    if (p->n_left->n_lval) {
	printf("%d", (int)p->n_left->n_lval);
    }
    
    printf("(%s)", rnames[p->n_left->n_rval]);
    
}


/*   printf conditional and unconditional branches */
void
cbgen(int o, int lab)
{
}

void branchfunc(NODE *p)
{
    int o = p->n_op;

    if (o < EQ || o > GT)
	cerror("bad binary conditional branch: %s", opst[o]);

    switch(o) {
    case EQ:
	printf("beq ");
	adrput(stdout, getlr(p, 'L'));
	printf(", ");
	adrput(stdout, getlr(p, 'R'));
	printf(", ");
	break;
    case NE:
	printf("bne ");
	adrput(stdout, getlr(p, 'L'));
	printf(", ");
	adrput(stdout, getlr(p, 'R'));
	printf(", ");
	break;
    case LE:
	expand(p, 0, "blez A1, ");
	break;
    case LT:
	expand(p, 0, "bltz A1, ");
	break;
    case GE:
	expand(p, 0, "bgez A1, ");
	break;
    case GT:
	expand(p, 0, "bgez A1, ");		
	break;		
    }
    printf(".L%d\n", p->n_label);
    printf("	nop\n");
}

#if 0
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
}
#endif

static void
myhardops(NODE *p)
{
    int ty = optype(p->n_op);
    NODE *l, *r, *q;

    if (ty == UTYPE)
	return myhardops(p->n_left);
    if (ty != BITYPE)
	return;
    myhardops(p->n_right);
    if (p->n_op != STASG)
	return;

    /*
     * If the structure size to copy is less than 32 byte, let it
     * be and generate move instructions later.  Otherwise convert it 
     * to memcpy() calls, unless it has a STCALL function as its
     * right node, in which case it is untouched.
     * STCALL returns are handled special.
     */
    if (p->n_right->n_op == STCALL || p->n_right->n_op == USTCALL)
	return;
    l = p->n_left;
    if (l->n_op == UMUL)
	l = nfree(l);
    else if (l->n_op == NAME) {
	l->n_op = ICON; /* Constant reference */
	l->n_type = INCREF(l->n_type);
    } else
	comperr("myhardops");
    r = p->n_right;
    q = mkbinode(CM, l, r, 0);
    q = mkbinode(CM, q, mklnode(ICON, p->n_stsize, 0, INT), 0);
    p->n_op = CALL;
    p->n_right = q;
    p->n_left = mklnode(ICON, 0, 0, 0);
    p->n_left->n_name = "memcpy";
}

void
myreader(NODE *p)
{
    int e2print(NODE *p, int down, int *a, int *b);
    //	walkf(p, optim2);
    myhardops(p);
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

/*
 * Remove last goto.
 */
void
myoptim(struct interpass *ip)
{
#if 0
    while (ip->sqelem.sqe_next->type != IP_EPILOG)
	ip = ip->sqelem.sqe_next;
    if (ip->type != IP_NODE || ip->ip_node->n_op != GOTO)
	comperr("myoptim");
    tfree(ip->ip_node);
    *ip = *ip->sqelem.sqe_next;
#endif
}

struct hardops hardops[] = {
    { MUL, LONGLONG, "__muldi3" },
    { MUL, ULONGLONG, "__muldi3" },
    { DIV, LONGLONG, "__divdi3" },
    { DIV, ULONGLONG, "__udivdi3" },
    { MOD, LONGLONG, "__moddi3" },
    { MOD, ULONGLONG, "__umoddi3" },
    { RS, LONGLONG, "__ashrdi3" },
    { RS, ULONGLONG, "__lshrdi3" },
    { LS, LONGLONG, "__ashldi3" },
    { LS, ULONGLONG, "__ashldi3" },
#if 0
    { STASG, PTR+STRTY, "memcpy" },
    { STASG, PTR+UNIONTY, "memcpy" },
#endif
    { 0 },
};

void
rmove(int s, int d, TWORD t)
{
    printf("	move %s, %s\n", rnames[d], rnames[s]);
}



