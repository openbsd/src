/*	$OpenBSD: local.c,v 1.3 2007/10/29 16:38:55 ragge Exp $	*/
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
 *	This product includes software developed or owned by Caldera
 *	International, Inc.
 * Neither the name of Caldera International, Inc. nor the names of other
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * USE OF THE SOFTWARE PROVIDED FOR UNDER THIS LICENSE BY CALDERA
 * INTERNATIONAL, INC. AND CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.	IN NO EVENT SHALL CALDERA INTERNATIONAL, INC. BE LIABLE
 * FOR ANY DIRECT, INDIRECT INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OFLIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE 
 * POSSIBILITY OF SUCH DAMAGE.
 */

# include "pass1.h"

/*	this file contains code which is dependent on the target machine */

#if 0
NODE *
cast( p, t ) register NODE *p; TWORD t; {
	/* cast node p to type t */

	p = buildtree( CAST, block( NAME, NIL, NIL, t, 0, (int)t ), p );
	p->left->op = FREE;
	p->op = FREE;
	return( p->right );
	}
#endif

NODE *
clocal(p) NODE *p; {

	/* this is called to do local transformations on
	   an expression tree preparitory to its being
	   written out in intermediate code.
	*/

	/* the major essential job is rewriting the
	   automatic variables and arguments in terms of
	   REG and OREG nodes */
	/* conversion ops which are not necessary are also clobbered here */
	/* in addition, any special features (such as rewriting
	   exclusive or) are easily handled here as well */

	register struct symtab *q;
	register NODE *r;
	register int o;
	register int m, ml;

	switch( o = p->n_op ){

	case NAME:
		if((q = p->n_sp) == 0 ) { /* already processed; ignore... */
			return(p);
			}
		switch( q->sclass ){

		case AUTO:
		case PARAM:
			/* fake up a structure reference */
			r = block( REG, NIL, NIL, PTR+STRTY, 0, 0 );
			r->n_lval = 0;
			r->n_rval = (q->sclass==AUTO?STKREG:ARGREG);
			p = stref( block( STREF, r, p, 0, 0, 0 ) );
			break;

		case STATIC:
			if( q->slevel == 0 ) break;
			p->n_lval = 0;
			p->n_rval = -q->soffset;
			break;

		case REGISTER:
			p->n_op = REG;
			p->n_lval = 0;
			p->n_rval = q->soffset;
			break;

			}
		break;

	case PCONV:
		/* do pointer conversions for char and longs */
		ml = p->n_left->n_type;
		if( ( ml==CHAR || ml==UCHAR || ml==SHORT || ml==USHORT ) && p->n_left->n_op != ICON ) break;

		/* pointers all have the same representation; the type is inherited */

	inherit:
		p->n_left->n_type = p->n_type;
		p->n_left->n_df = p->n_df;
		p->n_left->n_sue = p->n_sue;
		r = p->n_left;
		nfree(p);
		return( r );

	case SCONV:
		m = (p->n_type == FLOAT || p->n_type == DOUBLE );
		ml = (p->n_left->n_type == FLOAT || p->n_left->n_type == DOUBLE );
		if( m != ml ) break;

		/* now, look for conversions downwards */

		m = p->n_type;
		ml = p->n_left->n_type;
		if( p->n_left->n_op == ICON ){ /* simulate the conversion here */
			CONSZ val;
			val = p->n_left->n_lval;
			switch( m ){
			case CHAR:
				p->n_left->n_lval = (char) val;
				break;
			case UCHAR:
				p->n_left->n_lval = val & 0XFF;
				break;
			case USHORT:
				p->n_left->n_lval = val & 0XFFFFL;
				break;
			case SHORT:
				p->n_left->n_lval = (short)val;
				break;
			case UNSIGNED:
				p->n_left->n_lval = val & 0xFFFFFFFFL;
				break;
			case INT:
				p->n_left->n_lval = (int)val;
				break;
				}
			p->n_left->n_type = m;
			}
		else {
			/* meaningful ones are conversion of int to char, int to short,
			   and short to char, and unsigned version of them */
			if( m==CHAR || m==UCHAR ){
				if( ml!=CHAR && ml!= UCHAR ) break;
				}
			else if( m==SHORT || m==USHORT ){
				if( ml!=CHAR && ml!=UCHAR && ml!=SHORT && ml!=USHORT ) break;
				}
			}

		/* clobber conversion */
		if( tlen(p) == tlen(p->n_left) ) goto inherit;
		r = p->n_left;
		nfree(p);
		return( r );  /* conversion gets clobbered */

	case PVCONV:
	case PMCONV:
		if( p->n_right->n_op != ICON ) cerror( "bad conversion", 0);
		r = buildtree( o==PMCONV?MUL:DIV, p->n_left, p->n_right);
		nfree(p);
		return r;

	case RS:
	case RSEQ:
		/* convert >> to << with negative shift count */
		/* only if type of left operand is not unsigned */
		if( ISUNSIGNED(p->n_left->n_type) ) break;
		p->n_right = buildtree( UMINUS, p->n_right, NIL );
		if( p->n_op == RS ) p->n_op = LS;
		else p->n_op = LSEQ;
		break;

	case FORCE:
		p->n_op = ASSIGN;
		p->n_right = p->n_left;
		p->n_left = block(REG, NIL, NIL, p->n_type, 0, MKSUE(INT));
		p->n_left->n_rval = p->n_left->n_type == BOOL ? 
		    RETREG(CHAR) : RETREG(p->n_type);
		break;

	case STCALL:
	case CALL:
		/* Fix function call arguments. On vax, just add funarg */
		for (r = p->n_right; r->n_op == CM; r = r->n_left) {
			if (r->n_right->n_op != STARG &&
			    r->n_right->n_op != FUNARG)
				r->n_right = block(FUNARG, r->n_right, NIL, 
				    r->n_right->n_type, r->n_right->n_df,
				    r->n_right->n_sue);
		}
		if (r->n_op != STARG && r->n_op != FUNARG) {
			NODE *l = talloc();
			*l = *r;
			r->n_op = FUNARG; r->n_left = l; r->n_type = l->n_type;
		}
		break;
	}

	return(p);
}

/*
 * Can we take & of a NAME?
 */
int
andable(NODE *p)
{

	if ((p->n_type & ~BTMASK) == FTN)
		return 1; /* functions are called by name */
	return 0; /* Delay name reference to table, for PIC code generation */
}
 
void
cendarg(){ /* at the end of the arguments of a ftn, set the automatic offset */
	autooff = AUTOINIT;
	}

int
cisreg( t ) TWORD t; { /* is an automatic variable of type t OK for a register variable */
	return(1);	/* all are now */
	}

NODE *
offcon(OFFSZ off, TWORD t, union dimfun *d, struct suedef *sue)
{

	/* return a node, for structure references, which is suitable for
	   being added to a pointer of type t, in order to be off bits offset
	   into a structure */

	register NODE *p;

	/* t, d, and s are the type, dimension offset, and sizeoffset */
	/* in general they  are necessary for offcon, but not on H'well */

	p = bcon(0);
	p->n_lval = off/SZCHAR;
	return(p);

	}

void
spalloc(NODE *t, NODE *p, OFFSZ off)
{
	cerror("spalloc");
}

static int inbits, inval;

/*
 * set fsz bits in sequence to zero.
 */
void
zbits(OFFSZ off, int fsz)
{
	int m;

	if (idebug)
		printf("zbits off %lld, fsz %d inbits %d\n", off, fsz, inbits);
	if ((m = (inbits % SZCHAR))) {
		m = SZCHAR - m;
		if (fsz < m) {
			inbits += fsz;
			return;
		} else {
			fsz -= m;
			printf("\t.byte %d\n", inval);
			inval = inbits = 0;
		}
	}
	if (fsz >= SZCHAR) {
		printf("\t.space %d\n", fsz/SZCHAR);
		fsz -= (fsz/SZCHAR) * SZCHAR;
	}
	if (fsz) {
		inval = 0;
		inbits = fsz;
	}
}

/*
 * Initialize a bitfield.
 */
void
infld(CONSZ off, int fsz, CONSZ val)
{
	if (idebug)
		printf("infld off %lld, fsz %d, val %lld inbits %d\n",
		    off, fsz, val, inbits);
	val &= ((CONSZ)1 << fsz)-1;
	while (fsz + inbits >= SZCHAR) {
		inval |= (val << inbits);
		printf("\t.byte %d\n", inval & 255);
		fsz -= (SZCHAR - inbits);
		val >>= (SZCHAR - inbits);
		inval = inbits = 0;
	}
	if (fsz) {
		inval |= (val << inbits);
		inbits += fsz;
	}
}


char *
exname( p ) char *p; {
	/* make a name look like an external name in the local machine */
	/* vad is elf now */
	if (p == NULL)
		return "";
	return( p );
	}

TWORD
ctype(TWORD type ){ /* map types which are not defined on the local machine */
	switch( BTYPE(type) ){

	case LONG:
		MODTYPE(type,INT);
		break;

	case ULONG:
		MODTYPE(type,UNSIGNED);
		break;

	case LDOUBLE:	/* for now */
		MODTYPE(type,DOUBLE);
		}
	return( type );
	}

void
calldec(NODE *p, NODE *q) 
{
}

void
extdec(struct symtab *q)
{
}

void
commdec( struct symtab *q ){ /* make a common declaration for id, if reasonable */
	OFFSZ off;

	printf( "	.comm	%s,", exname( q->sname ) );
	off = tsize( q->stype, q->sdf, q->ssue );
	printf( CONFMT, off/SZCHAR );
	printf( "\n" );
	}

/* make a local common declaration for id, if reasonable */
void
lcommdec(struct symtab *q)
{
	int off;

	off = tsize(q->stype, q->sdf, q->ssue);
	off = (off+(SZCHAR-1))/SZCHAR;
	if (q->slevel == 0)
#ifdef GCC_COMPAT
		printf("	.lcomm %s,0%o\n", gcc_findname(q), off);
#else
		printf("	.lcomm %s,0%o\n", exname(q->sname), off);
#endif
	else
		printf("	.lcomm " LABFMT ",0%o\n", q->soffset, off);
}


static char *loctbl[] = { "text", "data", "section .rodata", "section .rodata" };

void
setloc1(int locc)
{
	if (locc == lastloc)
		return;
	lastloc = locc;
	printf("	.%s\n", loctbl[locc]);
}

/*
 * print out a constant node, may be associated with a label.
 * Do not free the node after use.
 * off is bit offset from the beginning of the aggregate
 * fsz is the number of bits this is referring to
 * XXX - floating point constants may be wrong if cross-compiling.
 */
void
ninval(CONSZ off, int fsz, NODE *p)
{
	union { float f; double d; long double l; int i[3]; } u;
	struct symtab *q;
	TWORD t;

	t = p->n_type;
	if (t > BTMASK)
		t = INT; /* pointer */

	if (p->n_op != ICON && p->n_op != FCON)
		cerror("ninval: init node not constant");

	if (p->n_op == ICON && p->n_sp != NULL && DEUNSIGN(t) != INT)
		uerror("element not constant");

	switch (t) {
	case LONGLONG:
	case ULONGLONG:
		printf("\t.long 0x%x", (int)p->n_lval);
		printf("\t.long 0x%x", (int)(p->n_lval >> 32));
		break;
	case INT:
	case UNSIGNED:
		printf("\t.long 0x%x", (int)p->n_lval);
		if ((q = p->n_sp) != NULL) {
			if ((q->sclass == STATIC && q->slevel > 0) ||
			    q->sclass == ILABEL) {
				printf("+" LABFMT, q->soffset);
			} else
				printf("+%s", exname(q->sname));
		}
		printf("\n");
		break;
	case SHORT:
	case USHORT:
		printf("\t.short 0x%x\n", (int)p->n_lval & 0xffff);
		break;
	case BOOL:
		if (p->n_lval > 1)
			p->n_lval = p->n_lval != 0;
		/* FALLTHROUGH */
	case CHAR:
	case UCHAR:
		printf("\t.byte %d\n", (int)p->n_lval & 0xff);
		break;
	case LDOUBLE:
		u.i[2] = 0;
		u.l = (long double)p->n_dcon;
		printf("\t.long\t0x%x,0x%x,0x%x\n", u.i[0], u.i[1], u.i[2]);
		break;
	case DOUBLE:
		u.d = (double)p->n_dcon;
		printf("\t.long\t0x%x,0x%x\n", u.i[0], u.i[1]);
		break;
	case FLOAT:
		u.f = (float)p->n_dcon;
		printf("\t.long\t0x%x\n", u.i[0]);
		break;
	default:
		cerror("ninval");
	}

}
