/*	$Id: reader.c,v 1.1.1.1 2007/09/15 18:12:37 otto Exp $	*/
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
 * Copyright(C) Caldera International Inc. 2001-2002. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * Redistributions of source code and documentation must retain the above
 * copyright notice, this list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditionsand the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 * 	This product includes software developed or owned by Caldera
 *	International, Inc.
 * Neither the name of Caldera International, Inc. nor the names of other
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * USE OF THE SOFTWARE PROVIDED FOR UNDER THIS LICENSE BY CALDERA
 * INTERNATIONAL, INC. AND CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL CALDERA INTERNATIONAL, INC. BE LIABLE
 * FOR ANY DIRECT, INDIRECT INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OFLIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE 
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Everything is entered via pass2_compile().  Three functions are 
 * allowed to recurse into pass2_compile(), so be careful:
 * - deluseless()
 * - myreader()
 * Especially in myreader note that trees may be rewritten twice if
 * things are not carefully handled.
 */

# include "pass2.h"

#include <string.h>
#include <stdarg.h>
#include <stdlib.h>

/*	some storage declarations */
int nrecur;
int lflag;
int x2debug;
int udebug = 0;
int thisline;
int fregs;
int p2autooff, p2maxautooff;

NODE *nodepole;
FILE *prfil;

void saveip(struct interpass *ip);
void deljumps(void);
void deltemp(NODE *p);
void mkhardops(NODE *p);
void optdump(struct interpass *ip);
void cvtemps(struct interpass *epil);
NODE *store(NODE *);
void rcount(void);
void compile2(struct interpass *ip);
void compile3(struct interpass *ip);
void compile4(struct interpass *ip);

static void gencode(NODE *p, int cookie);

char *ltyp[] = { "", "LREG", "LOREG", "LTEMP" };
char *rtyp[] = { "", "RREG", "ROREG", "RTEMP" };

/* used when removing nodes */
struct tmpsave {
	struct tmpsave *next;
	CONSZ tempaddr;
	int tempno;
} *tmpsave;

#ifdef PCC_DEBUG
static void
cktree(NODE *p)
{
	if (p->n_op > MAXOP)
		cerror("op %d slipped through", p->n_op);
	if (BTYPE(p->n_type) > MAXTYPES)
		cerror("type %x slipped through", p->n_type);
	if (p->n_op == CBRANCH && !logop(p->n_left->n_op))
		cerror("not logop branch");
	if ((dope[p->n_op] & ASGOPFLG) && p->n_op != RETURN)
		cerror("asgop %d slipped through", p->n_op);
}
#endif

/*
 * Check if a node has side effects.
 */
static int
isuseless(NODE *n)
{
	switch (n->n_op) {
	case FUNARG:
	case UCALL:
	case UFORTCALL:
	case FORCE:
/*	case INIT: */
	case ASSIGN:
	case CALL:
	case FORTCALL:
	case CBRANCH:
	case RETURN:
	case GOTO:
	case STCALL:
	case USTCALL:
	case STASG:
	case STARG:
		return 0;
	default:
		return 1;
	}
}

/*
 * Delete statements with no meaning (like a+b; or 513.4;)
 */
static NODE *
deluseless(NODE *p)
{
	struct interpass *ip;
	NODE *l, *r;

	if (optype(p->n_op) == LTYPE) {
		nfree(p);
		return NULL;
	}
	if (isuseless(p) == 0)
		return p;

	if (optype(p->n_op) == UTYPE) {
		l = p->n_left;
		nfree(p);
		return deluseless(l);
	}

	/* Be sure that both leaves may be valid */
	l = deluseless(p->n_left);
	r = deluseless(p->n_right);
	nfree(p);
	if (l && r) {
		/* Put left on queue first */
		ip = tmpalloc(sizeof(*ip));
		ip->type = IP_NODE;
		ip->lineno = 0; /* XXX */
		ip->ip_node = l;
		pass2_compile(ip);
		return r;
	} else if (l)
		return l;
	else if (r)
		return r;
	return NULL;
}

static struct interpass ipole;
struct interpass_prolog *ipp, *epp;

/*
 * Receives interpass structs from pass1.
 */
void
pass2_compile(struct interpass *ip)
{
	if (ip->type == IP_PROLOG) {
		tmpsave = NULL;
		ipp = (struct interpass_prolog *)ip;
		DLIST_INIT(&ipole, qelem);
	}
	DLIST_INSERT_BEFORE(&ipole, ip, qelem);
	if (ip->type != IP_EPILOG)
		return;

#ifdef PCC_DEBUG
	if (e2debug) {
		printf("Entering pass2\n");
		printip(&ipole);
	}
#endif

	epp = (struct interpass_prolog *)DLIST_PREV(&ipole, qelem);
	p2maxautooff = p2autooff = epp->ipp_autos;

	myreader(&ipole); /* local massage of input */

	DLIST_FOREACH(ip, &ipole, qelem) {
		if (ip->type != IP_NODE)
			continue;
		if (xtemps == 0)
			walkf(ip->ip_node, deltemp);
	}
	DLIST_FOREACH(ip, &ipole, qelem) {
		if (ip->type != IP_NODE)
			continue;
		canon(ip->ip_node);
		walkf(ip->ip_node, cktree);
		if ((ip->ip_node = deluseless(ip->ip_node)) == NULL)
			DLIST_REMOVE(ip, qelem);
	}

	optimize(&ipole);
	ngenregs(&ipole);

	DLIST_FOREACH(ip, &ipole, qelem)
		emit(ip);
}

void
emit(struct interpass *ip)
{
	NODE *p;
	int o;

	switch (ip->type) {
	case IP_NODE:
		p = ip->ip_node;

		nodepole = p;
//printf("bu:\n");
//fwalk(p, e2print, 0);
		canon(p); /* may convert stuff after genregs */
//fwalk(p, e2print, 0);
		switch (p->n_op) {
		case CBRANCH:
			/* Only emit branch insn if RESCC */
			if (table[TBLIDX(p->n_left->n_su)].rewrite & RESCC) {
				o = p->n_left->n_op;
				gencode(p, FORCC);
				cbgen(o, p->n_right->n_lval);
			} else
				gencode(p, FORCC);
			break;
		case FORCE:
			gencode(p->n_left, INREGS);
			break;
		default:
			if (p->n_op != REG || p->n_type != VOID) /* XXX */
				gencode(p, FOREFF); /* Emit instructions */
		}

		tfree(p);
		break;
	case IP_PROLOG:
		prologue((struct interpass_prolog *)ip);
		break;
	case IP_EPILOG:
		eoftn((struct interpass_prolog *)ip);
		tmpsave = NULL;	/* Always forget old nodes */
		p2maxautooff = p2autooff = AUTOINIT/SZCHAR;
		break;
	case IP_DEFLAB:
		deflab(ip->ip_lbl);
		break;
	case IP_ASM:
		printf("\t%s\n", ip->ip_asm);
		break;
	default:
		cerror("compile4 %d", ip->type);
	}
}

#ifdef PCC_DEBUG
char *cnames[] = {
	"SANY",
	"SAREG",
	"SBREG",
	"SCREG",
	"SDREG",
	"SCC",
	"SNAME",
	"SCON",
	"SFLD",
	"SOREG",
	"STARNM",
	"STARREG",
	"INTEMP",
	"FORARG",
	"SWADD",
	0,
};

/*
 * print a nice-looking description of cookie
 */
char *
prcook(int cookie)
{
	static char buf[50];
	int i, flag;

	if (cookie & SPECIAL) {
		switch (cookie) {
		case SZERO:
			return "SZERO";
		case SONE:
			return "SONE";
		case SMONE:
			return "SMONE";
		default:
			snprintf(buf, sizeof(buf), "SPECIAL+%d", cookie & ~SPECIAL);
			return buf;
		}
	}

	flag = 0;
	buf[0] = 0;
	for (i = 0; cnames[i]; ++i) {
		if (cookie & (1<<i)) {
			if (flag)
				strlcat(buf, "|", sizeof(buf));
			++flag;
			strlcat(buf, cnames[i], sizeof(buf));
		}
	}
	return buf;
}

#endif

int odebug = 0;

int
geninsn(NODE *p, int cookie)
{
	NODE *p1, *p2;
	int o, rv = 0;

#ifdef PCC_DEBUG
	if (odebug) {
		printf("geninsn(%p, %s)\n", p, prcook(cookie));
		fwalk(p, e2print, 0);
	}
#endif

again:	switch (o = p->n_op) {
	case EQ:
	case NE:
	case LE:
	case LT:
	case GE:
	case GT:
	case ULE:
	case ULT:
	case UGE:
	case UGT:
		rv = relops(p);
		break;

	case PLUS:
	case MINUS:
	case MUL:
	case DIV:
	case MOD:
	case AND:
	case OR:
	case ER:
	case LS:
	case RS:
		rv = findops(p, cookie);
		break;

	case ASSIGN:
	case STASG:
		rv = findasg(p, cookie);
		break;

	case UMUL: /* May turn into an OREG */
		rv = findumul(p, cookie);
		break;

	case REG:
	case TEMP:
	case NAME:
	case ICON:
	case OREG:
		rv = findleaf(p, cookie);
		break;

	case STCALL:
	case CALL:
		/* CALL arguments are handled special */
		for (p1 = p->n_right; p1->n_op == CM; p1 = p1->n_left)
			geninsn(p1->n_right, FOREFF);
		geninsn(p1, FOREFF);
		/* FALLTHROUGH */
	case COMPL:
	case UMINUS:
	case PCONV:
	case SCONV:
/*	case INIT: */
	case GOTO:
	case FUNARG:
	case STARG:
	case UCALL:
	case USTCALL:
		rv = finduni(p, cookie);
		break;

	case CBRANCH:
		p1 = p->n_left;
		p2 = p->n_right;
		p1->n_label = p2->n_lval;
		o = p1->n_op;
		geninsn(p1, FORCC);
		p->n_su = 0;
		break;

	case FORCE: /* XXX needed? */
		geninsn(p->n_left, INREGS);
		p->n_su = 0; /* su calculations traverse left */
		break;

	default:
		comperr("geninsn: bad op %s, node %p", opst[o], p);
	}
	if (rv == FFAIL)
		comperr("Cannot generate code, node %p op %s", p,opst[p->n_op]);
	if (rv == FRETRY)
		goto again;
	return rv;
}

/*
 * Store a given subtree in a temporary location.
 * Return an OREG node where it is located.
 */
NODE *
store(NODE *p)
{
	extern struct interpass *storesave;
	struct interpass *ip;
	NODE *q, *r;
	int s;

	s = BITOOR(freetemp(szty(p->n_type)));
	q = mklnode(OREG, s, FPREG, p->n_type);
	r = mklnode(OREG, s, FPREG, p->n_type);
	ip = ipnode(mkbinode(ASSIGN, q, p, p->n_type));

	storesave = ip;
	return r;
}

#ifdef PCC_DEBUG
#define	CDEBUG(x) if (c2debug) printf x
#else
#define	CDEBUG(x)
#endif

/*
 * Do a register-register move if necessary.
 */
static void
ckmove(NODE *p, NODE *q)
{
	if (q->n_op != REG || p->n_reg == -1)
		return; /* no register */
	if (DECRA(p->n_reg, 0) == DECRA(q->n_reg, 0))
		return; /* no move necessary */
	CDEBUG(("rmove: node %p, %s -> %s\n", p, rnames[DECRA(q->n_reg, 0)],
	    rnames[DECRA(p->n_reg, 0)]));
	rmove(DECRA(q->n_reg, 0), DECRA(p->n_reg, 0), p->n_type);
	q->n_reg = q->n_rval = DECRA(p->n_reg, 0);
}

/*
 * Rewrite node to register after instruction emit.
 */
static void
rewrite(NODE *p, int rewrite, int cookie)
{
	NODE *l, *r;
	int o;

	l = getlr(p, 'L');
	r = getlr(p, 'R');
	o = p->n_op;
	p->n_op = REG;
	p->n_lval = 0;
	p->n_name = "";

	if (o == ASSIGN) {
		/* special rewrite care */
		int reg = DECRA(p->n_reg, 0);
#define	TL(x) (TBLIDX(x->n_su) || x->n_op == REG)
		if (p->n_reg == -1)
			;
		else if (TL(l) && (DECRA(l->n_reg, 0) == reg))
			;
		else if (TL(r) && (DECRA(r->n_reg, 0) == reg))
			;
		else if (TL(l))
			rmove(DECRA(l->n_reg, 0), reg, p->n_type);
		else if (TL(r))
			rmove(DECRA(r->n_reg, 0), reg, p->n_type);
#if 0
		else
			comperr("rewrite");
#endif
#undef TL
	}
	if (optype(o) != LTYPE)
		tfree(l);
	if (optype(o) == BITYPE)
		tfree(r);
	if (rewrite == 0)
		return;
	CDEBUG(("rewrite: %p, reg %s\n", p, rnames[DECRA(p->n_reg, 0)]));
	p->n_rval = DECRA(p->n_reg, 0);
}

void
gencode(NODE *p, int cookie)
{
	struct optab *q = &table[TBLIDX(p->n_su)];
	NODE *p1, *l, *r;
	int o = optype(p->n_op);

	l = p->n_left;
	r = p->n_right;

	if (TBLIDX(p->n_su) == 0) {
		if (o == BITYPE && (p->n_su & DORIGHT))
			gencode(r, 0);
		if (optype(p->n_op) != LTYPE)
			gencode(l, 0);
		if (o == BITYPE && !(p->n_su & DORIGHT))
			gencode(r, 0);
		return;
	}

	CDEBUG(("gencode: node %p\n", p));

	if (p->n_op == REG && DECRA(p->n_reg, 0) == p->n_rval)
		return; /* meaningless move to itself */

	if (callop(p->n_op))
		lastcall(p); /* last chance before function args */
	if (p->n_op == CALL || p->n_op == FORTCALL || p->n_op == STCALL) {
		/* Print out arguments first */
		for (p1 = r; p1->n_op == CM; p1 = p1->n_left)
			gencode(p1->n_right, FOREFF);
		gencode(p1, FOREFF);
		o = UTYPE; /* avoid going down again */
	}

	if (o == BITYPE && (p->n_su & DORIGHT)) {
		gencode(r, INREGS);
		if (q->rewrite & RRIGHT)
			ckmove(p, r);
	}
	if (o != LTYPE) {
		gencode(l, INREGS);
		if (q->rewrite & RLEFT)
			ckmove(p, l);
	}
	if (o == BITYPE && !(p->n_su & DORIGHT)) {
		gencode(r, INREGS);
		if (q->rewrite & RRIGHT)
			ckmove(p, r);
	}

	canon(p);

	if (q->needs & NSPECIAL) {
		int rr = rspecial(q, NRIGHT);
		int lr = rspecial(q, NLEFT);

		if (rr >= 0) {
			if (r->n_op != REG)
				comperr("gencode: rop != REG");
			if (rr != r->n_rval)
				rmove(r->n_rval, rr, r->n_type);
			r->n_rval = r->n_reg = rr;
		}
		if (lr >= 0) {
			if (l->n_op != REG)
				comperr("gencode: %p lop != REG", p);
			if (lr != l->n_rval)
				rmove(l->n_rval, lr, l->n_type);
			l->n_rval = l->n_reg = lr;
		}
		if (rr >= 0 && lr >= 0 && (l->n_reg == rr || r->n_reg == lr))
			comperr("gencode: cross-reg-move");
	}

	if (p->n_op == ASSIGN &&
	    p->n_left->n_op == REG && p->n_right->n_op == REG &&
	    p->n_left->n_rval == p->n_right->n_rval){
		/* do not emit anything */
		CDEBUG(("gencode(%p) assign nothing\n", p));
		rewrite(p, q->rewrite, cookie);
		return;
	}

	CDEBUG(("emitting node %p\n", p));
	if (TBLIDX(p->n_su) == 0)
		return;

	expand(p, cookie, q->cstring);
	if (callop(p->n_op) && cookie != FOREFF &&
	    DECRA(p->n_reg, 0) != RETREG(p->n_type)) {
		CDEBUG(("gencode(%p) retreg\n", p));
		rmove(RETREG(p->n_type), DECRA(p->n_reg, 0), p->n_type);
	} else if (q->needs & NSPECIAL) {
		int rr = rspecial(q, NRES);

		if (rr >= 0 && DECRA(p->n_reg, 0) != rr) {
			CDEBUG(("gencode(%p) nspec retreg\n", p));
			rmove(rr, DECRA(p->n_reg, 0), p->n_type);
		}
	} else if ((q->rewrite & RESC1) &&
	    (DECRA(p->n_reg, 1) != DECRA(p->n_reg, 0))) {
		CDEBUG(("gencode(%p) RESC1 retreg\n", p));
		rmove(DECRA(p->n_reg, 1), DECRA(p->n_reg, 0), p->n_type);
	}
#if 0
		/* XXX - kolla upp det här */
	   else if (p->n_op == ASSIGN) {
		/* may need move added if RLEFT/RRIGHT */
		/* XXX should be handled in sucomp() */
		if ((q->rewrite & RLEFT) && (p->n_left->n_op == REG) &&
		    (p->n_left->n_rval != DECRA(p->n_reg, 0)) &&
		    TCLASS(p->n_su)) {
			rmove(p->n_left->n_rval, DECRA(p->n_reg, 0), p->n_type);
		} else if ((q->rewrite & RRIGHT) && (p->n_right->n_op == REG) &&
		    (p->n_right->n_rval != DECRA(p->n_reg, 0)) &&
		    TCLASS(p->n_su)) {
			rmove(p->n_right->n_rval, DECRA(p->n_reg, 0), p->n_type);
		}
	}
#endif
	rewrite(p, q->rewrite, cookie);
}

int negrel[] = { NE, EQ, GT, GE, LT, LE, UGT, UGE, ULT, ULE } ;  /* negatives of relationals */

#ifdef PCC_DEBUG
#undef	PRTABLE
void
e2print(NODE *p, int down, int *a, int *b)
{
#ifdef PRTABLE
	extern int tablesize;
#endif

	prfil = stdout;
	*a = *b = down+1;
	while( down >= 2 ){
		fprintf(prfil, "\t");
		down -= 2;
		}
	if( down-- ) fprintf(prfil, "    " );


	fprintf(prfil, "%p) %s", p, opst[p->n_op] );
	switch( p->n_op ) { /* special cases */

	case REG:
		fprintf(prfil, " %s", rnames[p->n_rval] );
		break;

	case TEMP:
		fprintf(prfil, " " CONFMT, p->n_lval);
		break;

	case ICON:
	case NAME:
	case OREG:
		fprintf(prfil, " " );
		adrput(prfil, p );
		break;

	case STCALL:
	case USTCALL:
	case STARG:
	case STASG:
		fprintf(prfil, " size=%d", p->n_stsize );
		fprintf(prfil, " align=%d", p->n_stalign );
		break;
		}

	fprintf(prfil, ", " );
	tprint(prfil, p->n_type, p->n_qual);
	fprintf(prfil, ", " );
	{
		int gregn(struct regw *);
		if (p->n_reg == -1)
			fprintf(prfil, "REG <undef>");
		else if (p->n_reg < 100000) /* XXX */
			fprintf(prfil, "REG %s", rnames[DECRA(p->n_reg, 0)]);
		else
			fprintf(prfil, "TEMP %d", gregn(p->n_regw));
		}
	fprintf(prfil, ", SU= %d(%cREG,%s,%s,%s,%s)\n",
	    TBLIDX(p->n_su), 
	    TCLASS(p->n_su)+'@',
#ifdef PRTABLE
	    TBLIDX(p->n_su) >= 0 && TBLIDX(p->n_su) <= tablesize ?
	    table[TBLIDX(p->n_su)].cstring : "",
#else
	    "",
#endif
	    ltyp[LMASK&p->n_su],
	    rtyp[(p->n_su&RMASK) >> 2], p->n_su & DORIGHT ? "DORIGHT" : "");
}
#endif

#ifndef FIELDOPS
/*
 * do this if there is no special hardware support for fields
 */
static void
ffld(NODE *p, int down, int *down1, int *down2 )
{
	/*
	 * look for fields that are not in an lvalue context,
	 * and rewrite them...
	 */
	NODE *shp;
	int s, o, v, ty;

	*down1 =  asgop( p->n_op );
	*down2 = 0;

	if( !down && p->n_op == FLD ){ /* rewrite the node */

		if( !rewfld(p) ) return;

		ty = p->n_type;
		v = p->n_rval;
		s = UPKFSZ(v);
# ifdef RTOLBYTES
		o = UPKFOFF(v);  /* amount to shift */
# else
		o = szty(p->n_type)*SZINT - s - UPKFOFF(v);  /* amount to shift */
#endif

		/* make & mask part */

		p->n_left->n_type = ty;

		p->n_op = AND;
		p->n_right = mklnode(ICON, (1 << s)-1, 0, ty);

		/* now, if a shift is needed, do it */

		if( o != 0 ){
			shp = mkbinode(RS, p->n_left,
			    mklnode(ICON, o, 0, INT), ty);
			p->n_left = shp;
			/* whew! */
		}
	}
}
#endif

/*
 * change left TEMPs into OREGs
 */
void
deltemp(NODE *p)
{
	struct tmpsave *w;
	NODE *l;

	if (p->n_op == TEMP) {
		/* Check if already existing */
		for (w = tmpsave; w; w = w->next)
			if (w->tempno == p->n_lval)
				break;
		if (w == NULL) {
			/* new on stack */
			w = tmpalloc(sizeof(struct tmpsave));
			w->tempno = p->n_lval;
			w->tempaddr = BITOOR(freetemp(szty(p->n_type)));
			w->next = tmpsave;
			tmpsave = w;
		}
		p->n_op = OREG;
		p->n_rval = FPREG;
		p->n_lval = w->tempaddr;
	} else if (p->n_op == ADDROF) {
		/* TEMPs are already converted to OREGs */
		if ((l = p->n_left)->n_op != OREG)
			comperr("bad U&");
		p->n_op = PLUS;
		l->n_op = REG;
		l->n_type = INCREF(l->n_type);
		p->n_right = mklnode(ICON, l->n_lval, 0, INT);
	}
}

/*
 * for pointer/integer arithmetic, set pointer at left node
 */
static void
setleft(NODE *p)          
{        
	NODE *q;

	/* only additions for now */
	if (p->n_op != PLUS)
		return;
	if (ISPTR(p->n_right->n_type) && !ISPTR(p->n_left->n_type)) {
		q = p->n_right;
		p->n_right = p->n_left;
		p->n_left = q;
	}
}

/* It is OK to have these as externals */
static int oregr;
static CONSZ oregtemp;
static char *oregcp;
/*
 * look for situations where we can turn * into OREG
 * If sharp then do not allow temps.
 */
int
oregok(NODE *p, int sharp)
{

	NODE *q;
	NODE *ql, *qr;
	int r;
	CONSZ temp;
	char *cp;

	q = p->n_left;
#if 0
	if ((q->n_op == REG || (q->n_op == TEMP && !sharp)) &&
	    q->n_rval == DECRA(q->n_reg, 0)) {
#endif
	if (q->n_op == REG || (q->n_op == TEMP && !sharp)) {
		temp = q->n_lval;
		r = q->n_rval;
		cp = q->n_name;
		goto ormake;
	}

	if (q->n_op != PLUS && q->n_op != MINUS)
		return 0;
	ql = q->n_left;
	qr = q->n_right;

#ifdef R2REGS

	/* look for doubly indexed expressions */
	/* XXX - fix checks */

	if( q->n_op == PLUS) {
		int i;
		if( (r=base(ql))>=0 && (i=offset(qr, tlen(p)))>=0) {
			makeor2(p, ql, r, i);
			return;
		} else if((r=base(qr))>=0 && (i=offset(ql, tlen(p)))>=0) {
			makeor2(p, qr, r, i);
			return;
		}
	}


#endif

#if 0
	if( (q->n_op==PLUS || q->n_op==MINUS) && qr->n_op == ICON &&
			(ql->n_op==REG || (ql->n_op==TEMP && !sharp)) &&
			szty(qr->n_type)==1 &&
			(ql->n_rval == DECRA(ql->n_reg, 0) ||
			/* XXX */
			 ql->n_rval == FPREG || ql->n_rval == STKREG)) {
#endif
	if ((q->n_op==PLUS || q->n_op==MINUS) && qr->n_op == ICON &&
	    (ql->n_op==REG || (ql->n_op==TEMP && !sharp))) {
	    
		temp = qr->n_lval;
		if( q->n_op == MINUS ) temp = -temp;
		r = ql->n_rval;
		temp += ql->n_lval;
		cp = qr->n_name;
		if( *cp && ( q->n_op == MINUS || *ql->n_name ) )
			return 0;
		if( !*cp ) cp = ql->n_name;

		ormake:
		if( notoff( p->n_type, r, temp, cp ))
			return 0;
		oregtemp = temp;
		oregr = r;
		oregcp = cp;
		return 1;
	}
	return 0;
}

static void
ormake(NODE *p)
{
	NODE *q = p->n_left;

	p->n_op = OREG;
	p->n_rval = oregr;
	p->n_lval = oregtemp;
	p->n_name = oregcp;
	tfree(q);
}

/*
 * look for situations where we can turn * into OREG
 */
void
oreg2(NODE *p)
{
	if (p->n_op != UMUL)
		return;
	if (oregok(p, 1))
		ormake(p);
	if (p->n_op == UMUL)
		myormake(p);
}

void
canon(p) NODE *p; {
	/* put p in canonical form */

	walkf(p, setleft);	/* ptrs at left node for arithmetic */
	walkf(p, oreg2);	/* look for and create OREG nodes */
#ifndef FIELDOPS
	fwalk(p, ffld, 0);	/* look for field operators */
# endif
#ifdef MYCANON
	MYCANON(p);		/* your own canonicalization routine(s) */
#endif

}

void
comperr(char *str, ...)
{
	extern char *ftitle;
	va_list ap;

	va_start(ap, str);
	fprintf(stderr, "%s, line %d: compiler error: ", ftitle, thisline);
	vfprintf(stderr, str, ap);
	fprintf(stderr, "\n");
	va_end(ap);
	prfil = stderr;

	if (nodepole && nodepole->n_op != FREE)
		fwalk(nodepole, e2print, 0);
	exit(1);
}

/*
 * allocate k integers worth of temp space
 * we also make the convention that, if the number of words is
 * more than 1, it must be aligned for storing doubles...
 * Returns bits offset from base register.
 * XXX - redo this.
 */
int
freetemp(int k)
{
#ifndef BACKTEMP
	int t;

	if (k > 1)
		SETOFF(p2autooff, ALDOUBLE/ALCHAR);

	t = p2autooff;
	p2autooff += k*(SZINT/SZCHAR);
	if (p2autooff > p2maxautooff)
		p2maxautooff = p2autooff;
	return (t);

#else
	p2autooff += k*(SZINT/SZCHAR);
	if (k > 1)
		SETOFF(p2autooff, ALDOUBLE/ALCHAR);

	if (p2autooff > p2maxautooff)
		p2maxautooff = p2autooff;
	return( -p2autooff );
#endif
	}

NODE *
mklnode(int op, CONSZ lval, int rval, TWORD type)
{
	NODE *p = talloc();

	p->n_name = "";
	p->n_qual = 0;
	p->n_op = op;
	p->n_lval = lval;
	p->n_rval = rval;
	p->n_type = type;
	p->n_regw = NULL;
	p->n_su = 0;
	return p;
}

NODE *
mkbinode(int op, NODE *left, NODE *right, TWORD type)
{
	NODE *p = talloc();

	p->n_name = "";
	p->n_qual = 0;
	p->n_op = op;
	p->n_left = left;
	p->n_right = right;
	p->n_type = type;
	p->n_regw = NULL;
	return p;
}

NODE *
mkunode(int op, NODE *left, int rval, TWORD type)
{
	NODE *p = talloc();

	p->n_name = "";
	p->n_qual = 0;
	p->n_op = op;
	p->n_left = left;
	p->n_rval = rval;
	p->n_type = type;
	p->n_regw = NULL;
	return p;
}

struct interpass *
ipnode(NODE *p)
{
	struct interpass *ip = tmpalloc(sizeof(struct interpass));

	ip->ip_node = p;
	ip->type = IP_NODE;
	ip->lineno = thisline;
	return ip;
}

int
rspecial(struct optab *q, int what)
{
	struct rspecial *r = nspecial(q);
	while (r->op) {
		if (r->op == what)
			return r->num;
		r++;
	}
	return -1;
}
