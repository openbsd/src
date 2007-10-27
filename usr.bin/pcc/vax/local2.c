/*	$OpenBSD: local2.c,v 1.2 2007/10/27 14:19:18 ragge Exp $	*/
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

# include "pass2.h"
# include "ctype.h"
/* a lot of the machine dependent parts of the second pass */

static void prtype(NODE *n);
static void acon(NODE *p);

# define BITMASK(n) ((1L<<n)-1)

/*
 * Print out the prolog assembler.
 * addto and regoff are already calculated.
 */
void
prologue(struct interpass_prolog *ipp)
{
	if (ipp->ipp_vis)
		printf("	.globl %s\n", ipp->ipp_name);
	printf("	.align 4\n");
	printf("%s:\n", ipp->ipp_name);
	printf("	.word 0x%x\n", ipp->ipp_regs);
	if (p2maxautooff)
		printf("	subl2 $%d,%%sp\n", p2maxautooff);
}

/*
 * Called after all instructions in a function are emitted.
 * Generates code for epilog here.
 */
void
eoftn(struct interpass_prolog *ipp)
{
	if (ipp->ipp_ip.ip_lbl == 0)
		return; /* no code needs to be generated */
	printf("	ret\n");
}

struct hoptab { int opmask; char * opstring; } ioptab[] = {

	{ PLUS,	"add", },
	{ MINUS,	"sub", },
	{ MUL,	"mul", },
	{ DIV,	"div", },
	{ OR,	"bis", },
	{ ER,	"xor", },
	{ AND,	"bic", },
	{ -1, ""     },
};

void
hopcode( f, o ){
	/* output the appropriate string from the above table */

	register struct hoptab *q;

	for( q = ioptab;  q->opmask>=0; ++q ){
		if( q->opmask == o ){
			printf( "%s", q->opstring );
/* tbl
			if( f == 'F' ) printf( "e" );
			else if( f == 'D' ) printf( "d" );
   tbl */
/* tbl */
			switch( f ) {
				case 'L':
				case 'W':
				case 'B':
				case 'D':
				case 'F':
					printf("%c", tolower(f));
					break;

				}
/* tbl */
			return;
			}
		}
	cerror( "no hoptab for %s", opst[o] );
	}

char *
rnames[] = {  /* keyed to register number tokens */

	"r0", "r1", "r2", "r3", "r4", "r5",
	"r6", "r7", "r8", "r9", "r10", "r11",
	"ap", "fp", "sp", "pc",
	/* The concatenated regs has the name of the lowest */
	"r0", "r1", "r2", "r3", "r4", "r5",
	"r6", "r7", "r8", "r9", "r10"
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
			return(2);

		case DOUBLE:
		case LDOUBLE:
		case LONGLONG:
		case ULONGLONG:
			return(8);

		default:
			return(4);
		}
}

static int
mixtypes(NODE *p, NODE *q)
{
	TWORD tp, tq;

	tp = p->n_type;
	tq = q->n_type;

	return( (tp==FLOAT || tp==DOUBLE) !=
		(tq==FLOAT || tq==DOUBLE) );
}

void
prtype(NODE *n)
{
	switch (n->n_type)
		{
		case DOUBLE:
			printf("d");
			return;

		case FLOAT:
			printf("f");
			return;

		case LONG:
		case ULONG:
		case INT:
		case UNSIGNED:
			printf("l");
			return;

		case SHORT:
		case USHORT:
			printf("w");
			return;

		case CHAR:
		case UCHAR:
			printf("b");
			return;

		default:
			if ( !ISPTR( n->n_type ) ) cerror("zzzcode- bad type");
			else {
				printf("l");
				return;
				}
		}
}

void
zzzcode( p, c ) register NODE *p; {
	int m;
	int val;
	switch( c ){

	case 'N':  /* logical ops, turned into 0-1 */
		/* use register given by register 1 */
		cbgen( 0, m=getlab());
		deflab( p->n_label );
		printf( "	clrl	%s\n", rnames[getlr( p, '1' )->n_rval] );
		deflab( m );
		return;

	case 'A':
		{
		register NODE *l, *r;

		if (xdebug) e2print(p, 0, &val, &val);
		r = getlr(p, 'R');
		if (optype(p->n_op) == LTYPE || p->n_op == UMUL) {
			l = resc;
			l->n_type = (r->n_type==FLOAT || r->n_type==DOUBLE ? DOUBLE : INT);
		} else
			l = getlr(p, 'L');
		if (r->n_op == ICON  && r->n_name[0] == '\0') {
			if (r->n_lval == 0) {
				printf("clr");
				prtype(l);
				printf("	");
				adrput(stdout, l);
				return;
			}
			if (r->n_lval < 0 && r->n_lval >= -63) {
				printf("mneg");
				prtype(l);
				r->n_lval = -r->n_lval;
				goto ops;
			}
			r->n_type = (r->n_lval < 0 ?
					(r->n_lval >= -128 ? CHAR
					: (r->n_lval >= -32768 ? SHORT
					: INT )) : r->n_type);
			r->n_type = (r->n_lval >= 0 ?
					(r->n_lval <= 63 ? INT
					: ( r->n_lval <= 127 ? CHAR
					: (r->n_lval <= 255 ? UCHAR
					: (r->n_lval <= 32767 ? SHORT
					: (r->n_lval <= 65535 ? USHORT
					: INT ))))) : r->n_type );
			}
		if (l->n_op == REG && l->n_type != FLOAT && l->n_type != DOUBLE)
			l->n_type = INT;
		if (!mixtypes(l,r))
			{
			if (tlen(l) == tlen(r))
				{
				printf("mov");
				prtype(l);
				goto ops;
				}
			else if (tlen(l) > tlen(r) && ISUNSIGNED(r->n_type))
				{
				printf("movz");
				}
			else
				{
				printf("cvt");
				}
			}
		else
			{
			printf("cvt");
			}
		prtype(r);
		prtype(l);
	ops:
		printf("	");
		adrput(stdout, r);
		printf(",");
		adrput(stdout, l);
		return;
		}

	case 'C':	/* num words pushed on arg stack */
		{
			int pr = p->n_qual;

			if (p->n_op == STCALL || p->n_op == USTCALL)
				pr += 4;
			printf("$%d", pr);
			break;
		}

	case 'D':	/* INCR and DECR */
		zzzcode(p->n_left, 'A');
		printf("\n	");

#if 0
	case 'E':	/* INCR and DECR, FOREFF */
		if (p->n_right->n_lval == 1)
			{
			printf("%s", (p->n_op == INCR ? "inc" : "dec") );
			prtype(p->n_left);
			printf("	");
			adrput(stdout, p->n_left);
			return;
			}
		printf("%s", (p->n_op == INCR ? "add" : "sub") );
		prtype(p->n_left);
		printf("2	");
		adrput(stdout, p->n_right);
		printf(",");
		adrput(p->n_left);
		return;
#endif

	case 'F':	/* register type of right operand */
		{
		register NODE *n;
		extern int xdebug;
		register int ty;

		n = getlr( p, 'R' );
		ty = n->n_type;

		if (xdebug) printf("->%d<-", ty);

		if ( ty==DOUBLE) printf("d");
		else if ( ty==FLOAT ) printf("f");
		else printf("l");
		return;
		}

	case 'L':	/* type of left operand */
	case 'R':	/* type of right operand */
		{
		register NODE *n;
		extern int xdebug;

		n = getlr ( p, c);
		if (xdebug) printf("->%d<-", n->n_type);

		prtype(n);
		return;
		}

	case 'Z':	/* complement mask for bit instr */
		printf("$%Ld", ~p->n_right->n_lval);
		return;

	case 'U':	/* 32 - n, for unsigned right shifts */
		printf("$" CONFMT, 32 - p->n_right->n_lval );
		return;

	case 'T':	/* rounded structure length for arguments */
		{
		int size;

		size = p->n_stsize;
		SETOFF( size, 4);
		printf("$%d", size);
		return;
		}

	case 'S':  /* structure assignment */
		{
			register NODE *l, *r;
			register int size;

			l = r = NULL; /* XXX gcc */
			if( p->n_op == STASG ){
				l = p->n_left;
				r = p->n_right;

				}
			else if( p->n_op == STARG ){  /* store an arg into a temporary */
				l = getlr( p, '3' );
				r = p->n_left;
				}
			else cerror( "STASG bad" );

			if( r->n_op == ICON ) r->n_op = NAME;
			else if( r->n_op == REG ) r->n_op = OREG;
			else if( r->n_op != OREG ) cerror( "STASG-r" );

			size = p->n_stsize;

			if( size <= 0 || size > 65535 )
				cerror("structure size <0=0 or >65535");

			switch(size) {
				case 1:
					printf("	movb	");
					break;
				case 2:
					printf("	movw	");
					break;
				case 4:
					printf("	movl	");
					break;
				case 8:
					printf("	movq	");
					break;
				default:
					printf("	movc3	$%d,", size);
					break;
			}
			adrput(stdout, r);
			printf(",");
			adrput(stdout, l);
			printf("\n");

			if( r->n_op == NAME ) r->n_op = ICON;
			else if( r->n_op == OREG ) r->n_op = REG;

			}
		break;

	default:
		comperr("illegal zzzcode '%c'", c);
		}
	}

void
rmove( int rt,int  rs, TWORD t ){
	printf( "	%s	%s,%s\n",
		(t==FLOAT ? "movf" : (t==DOUBLE ? "movd" : "movl")),
		rnames[rs], rnames[rt] );
	}

#if 0
setregs(){ /* set up temporary registers */
	fregs = 6;	/* tbl- 6 free regs on VAX (0-5) */
	;
	}

szty(t){ /* size, in registers, needed to hold thing of type t */
	return( (t==DOUBLE||t==FLOAT) ? 2 : 1 );
	}
#endif

int
rewfld( p ) NODE *p; {
	return(1);
	}

#if 0
callreg(p) NODE *p; {
	return( R0 );
	}

base( p ) register NODE *p; {
	register int o = p->op;

	if( (o==ICON && p->name[0] != '\0')) return( 100 ); /* ie no base reg */
	if( o==REG ) return( p->rval );
    if( (o==PLUS || o==MINUS) && p->left->op == REG && p->right->op==ICON)
		return( p->left->rval );
    if( o==OREG && !R2TEST(p->rval) && (p->type==INT || p->type==UNSIGNED || ISPTR(p->type)) )
		return( p->rval + 0200*1 );
	if( o==INCR && p->left->op==REG ) return( p->left->rval + 0200*2 );
	if( o==ASG MINUS && p->left->op==REG) return( p->left->rval + 0200*4 );
	if( o==UNARY MUL && p->left->op==INCR && p->left->left->op==REG
	  && (p->type==INT || p->type==UNSIGNED || ISPTR(p->type)) )
		return( p->left->left->rval + 0200*(1+2) );
	return( -1 );
	}

offset( p, tyl ) register NODE *p; int tyl; {

	if( tyl==1 && p->op==REG && (p->type==INT || p->type==UNSIGNED) ) return( p->rval );
	if( (p->op==LS && p->left->op==REG && (p->left->type==INT || p->left->type==UNSIGNED) &&
	      (p->right->op==ICON && p->right->name[0]=='\0')
	      && (1<<p->right->lval)==tyl))
		return( p->left->rval );
	return( -1 );
	}
#endif

#if 0
void
makeor2( p, q, b, o) register NODE *p, *q; register int b, o; {
	register NODE *t;
	NODE *f;

	p->n_op = OREG;
	f = p->n_left; 	/* have to free this subtree later */

	/* init base */
	switch (q->n_op) {
		case ICON:
		case REG:
		case OREG:
			t = q;
			break;

		case MINUS:
			q->n_right->n_lval = -q->n_right->n_lval;
		case PLUS:
			t = q->n_right;
			break;

		case UMUL:
			t = q->n_left->n_left;
			break;

		default:
			cerror("illegal makeor2");
			t = NULL; /* XXX gcc */
	}

	p->n_lval = t->n_lval;
	p->n_name = t->n_name;

	/* init offset */
	p->n_rval = R2PACK( (b & 0177), o, (b>>7) );

	tfree(f);
	return;
	}

int
canaddr( p ) NODE *p; {
	register int o = p->n_op;

	if( o==NAME || o==REG || o==ICON || o==OREG || (o==UMUL && shumul(p->n_left)) ) return(1);
	return(0);
	}

shltype( o, p ) register NODE *p; {
	return( o== REG || o == NAME || o == ICON || o == OREG || ( o==UMUL && shumul(p->n_left)) );
	}
#endif

int
flshape( p ) register NODE *p; {
	return( p->n_op == REG || p->n_op == NAME || p->n_op == ICON ||
		(p->n_op == OREG && (!R2TEST(p->n_rval) || tlen(p) == 1)) );
	}

int
shtemp( p ) register NODE *p; {
	if( p->n_op == STARG ) p = p->n_left;
	return( p->n_op==NAME || p->n_op ==ICON || p->n_op == OREG || (p->n_op==UMUL && shumul(p->n_left)) );
	}

int
shumul( p ) register NODE *p; {
	register int o;
	extern int xdebug;

	if (xdebug) {
		 printf("\nshumul:op=%d,lop=%d,rop=%d", p->n_op, p->n_left->n_op, p->n_right->n_op);
		printf(" prname=%s,plty=%d, prlval=%lld\n", p->n_right->n_name, p->n_left->n_type, p->n_right->n_lval);
		}


	o = p->n_op;
	if( o == NAME || (o == OREG && !R2TEST(p->n_rval)) || o == ICON ) return( STARNM );

#ifdef notyet
	if( ( o == INCR || o == ASG MINUS ) &&
	    ( p->n_left->n_op == REG && p->n_right->n_op == ICON ) &&
	    p->n_right->n_name[0] == '\0' )
		{
		switch (p->n_left->n_type)
			{
			case CHAR|PTR:
			case UCHAR|PTR:
				o = 1;
				break;

			case SHORT|PTR:
			case USHORT|PTR:
				o = 2;
				break;

			case INT|PTR:
			case UNSIGNED|PTR:
			case LONG|PTR:
			case ULONG|PTR:
			case FLOAT|PTR:
				o = 4;
				break;

			case DOUBLE|PTR:
				o = 8;
				break;

			default:
				if ( ISPTR(p->n_left->n_type) ) {
					o = 4;
					break;
					}
				else return(0);
			}
		return( p->n_right->n_lval == o ? STARREG : 0);
		}
#endif

	return( 0 );
	}

void
adrcon( val ) CONSZ val; {
	printf( "$" );
	printf( CONFMT, val );
	}

void
conput(FILE *fp, NODE *p)
{
	switch( p->n_op ){

	case ICON:
		acon( p );
		return;

	case REG:
		printf( "%s", rnames[p->n_rval] );
		return;

	default:
		cerror( "illegal conput" );
		}
	}

void
insput( p ) register NODE *p; {
	cerror( "insput" );
	}

void
upput( p , size) register NODE *p; {
	cerror( "upput" );
	}

void
adrput(FILE *fp, NODE *p)
{
	register int r;
	/* output an address, with offsets, from p */

	if( p->n_op == FLD ){
		p = p->n_left;
		}
	switch( p->n_op ){

	case NAME:
		acon( p );
		return;

	case ICON:
		/* addressable value of the constant */
		if (p->n_name[0] == '\0') /* uses xxxab */
			printf("$");
		acon(p);
		return;

	case REG:
		printf( "%s", rnames[p->n_rval] );
		return;

	case OREG:
		r = p->n_rval;
		if( R2TEST(r) ){ /* double indexing */
			register int flags;

			flags = R2UPK3(r);
			if( flags & 1 ) printf("*");
			if( flags & 4 ) printf("-");
			if( p->n_lval != 0 || p->n_name[0] != '\0' ) acon(p);
			if( R2UPK1(r) != 100) printf( "(%s)", rnames[R2UPK1(r)] );
			if( flags & 2 ) printf("+");
			printf( "[%s]", rnames[R2UPK2(r)] );
			return;
			}
		if( r == AP ){  /* in the argument region */
			if( p->n_lval <= 0 || p->n_name[0] != '\0' ) werror( "bad arg temp" );
			printf( CONFMT, p->n_lval );
			printf( "(ap)" );
			return;
			}
		if( p->n_lval != 0 || p->n_name[0] != '\0') acon( p );
		printf( "(%s)", rnames[p->n_rval] );
		return;

	case UMUL:
		/* STARNM or STARREG found */
		if( tshape(p, STARNM) ) {
			printf( "*" );
			adrput(0,  p->n_left);
			}
		else {	/* STARREG - really auto inc or dec */
			register NODE *q;

/* tbl
			p = p->n_left;
			p->n_left->n_op = OREG;
			if( p->n_op == INCR ) {
				adrput( p->n_left );
				printf( "+" );
				}
			else {
				printf( "-" );
				adrput( p->n_left );
				}
   tbl */
#ifdef notyet
			printf("%c(%s)%c", (p->n_left->n_op==INCR ? '\0' : '-'),
				rnames[p->n_left->n_left->n_rval], 
				(p->n_left->n_op==INCR ? '+' : '\0') );
#else
			printf("%c(%s)%c", '-',
				rnames[p->n_left->n_left->n_rval], 
				'\0' );
#endif
			p->n_op = OREG;
			p->n_rval = p->n_left->n_left->n_rval;
			q = p->n_left;
#ifdef notyet

			p->n_lval = (p->n_left->n_op == INCR ? -p->n_left->n_right->n_lval : 0);
#else
			p->n_lval = 0;
#endif
			p->n_name[0] = '\0';
			tfree(q);
		}
		return;

	default:
		cerror( "illegal address" );
		return;
	}

}

/*
 * print out a constant
 */
void
acon(NODE *p)
{

	if (p->n_name[0] == '\0') {
		printf(CONFMT, p->n_lval);
	} else if( p->n_lval == 0 ) {
		printf("%s", p->n_name);
	} else {
		printf("%s+", p->n_name);
		printf(CONFMT, p->n_lval);
	}
}

#if 0
genscall( p, cookie ) register NODE *p; {
	/* structure valued call */
	return( gencall( p, cookie ) );
	}

/* tbl */
int gc_numbytes;
/* tbl */

gencall( p, cookie ) register NODE *p; {
	/* generate the call given by p */
	register NODE *p1, *ptemp;
	register temp, temp1;
	register m;

	if( p->right ) temp = argsize( p->right );
	else temp = 0;

	if( p->op == STCALL || p->op == UNARY STCALL ){
		/* set aside room for structure return */

		if( p->stsize > temp ) temp1 = p->stsize;
		else temp1 = temp;
		}

	if( temp > maxargs ) maxargs = temp;
	SETOFF(temp1,4);

	if( p->right ){ /* make temp node, put offset in, and generate args */
		ptemp = talloc();
		ptemp->op = OREG;
		ptemp->lval = -1;
		ptemp->rval = SP;
		ptemp->name[0] = '\0';
		ptemp->rall = NOPREF;
		ptemp->su = 0;
		genargs( p->right, ptemp );
		nfree(ptemp);
		}

	p1 = p->left;
	if( p1->op != ICON ){
		if( p1->op != REG ){
			if( p1->op != OREG || R2TEST(p1->rval) ){
				if( p1->op != NAME ){
					order( p1, INAREG );
					}
				}
			}
		}

/*
	if( p1->op == REG && p->rval == R5 ){
		cerror( "call register overwrite" );
		}
 */
/* tbl
	setup gc_numbytes so reference to ZC works */

	gc_numbytes = temp;
/* tbl */

	p->op = UNARY CALL;
	m = match( p, INTAREG|INTBREG );
/* tbl
	switch( temp ) {
	case 0:
		break;
	case 2:
		printf( "	tst	(sp)+\n" );
		break;
	case 4:
		printf( "	cmp	(sp)+,(sp)+\n" );
		break;
	default:
		printf( "	add	$%d,sp\n", temp);
		}
   tbl */
	return(m != MDONE);
	}
#endif

static char *
ccbranches[] = {
	"jeql",
	"jneq",
	"jleq",
	"jlss",
	"jgeq",
	"jgtr",
	"jlequ",
	"jlssu",
	"jgequ",
	"jgtru",
};

/*
 * printf conditional and unconditional branches
 */
void
cbgen(int o, int lab)
{

	if (o == 0) {
		printf("	jbr     " LABFMT "\n", lab);
	} else {
		if (o > UGT)
			comperr("bad conditional branch: %s", opst[o]);
		printf("\t%s\t" LABFMT "\n", ccbranches[o-EQ], lab);
	}
}

static void
optim2(NODE *p)
{
	/* do local tree transformations and optimizations */

	register NODE *r;

	switch( p->n_op ) {

	case AND:
		/* commute L and R to eliminate compliments and constants */
		if( (p->n_left->n_op==ICON&&p->n_left->n_name[0]==0) || p->n_left->n_op==COMPL ) {
			r = p->n_left;
			p->n_left = p->n_right;
			p->n_right = r;
			}
#if 0
	case ASG AND:
		/* change meaning of AND to ~R&L - bic on pdp11 */
		r = p->n_right;
		if( r->op==ICON && r->name[0]==0 ) { /* compliment constant */
			r->lval = ~r->lval;
			}
		else if( r->op==COMPL ) { /* ~~A => A */
			r->op = FREE;
			p->right = r->left;
			}
		else { /* insert complement node */
			p->right = talloc();
			p->right->op = COMPL;
			p->right->rall = NOPREF;
			p->right->type = r->type;
			p->right->left = r;
			p->right->right = NULL;
			}
		break;
#endif
		}
	}

void
myreader(struct interpass *ipole)
{
	struct interpass *ip;

	DLIST_FOREACH(ip, ipole, qelem) {
		if (ip->type != IP_NODE)
			continue;
		walkf(ip->ip_node, optim2);
	}
}

/*
 * Return argument size in bytes.
 */
static int
argsiz(NODE *p)
{
	TWORD t = p->n_type;

	if (t == STRTY || t == UNIONTY)
		return p->n_stsize;
	return szty(t) * (SZINT/SZCHAR);
}

/*
 * Last chance to do something before calling a function.
 */
void
lastcall(NODE *p)
{
	NODE *op = p;
	int size = 0;

	/* Calculate argument sizes */
	p->n_qual = 0;
	if (p->n_op != CALL && p->n_op != FORTCALL && p->n_op != STCALL)
		return;
	for (p = p->n_right; p->n_op == CM; p = p->n_left)
		size += argsiz(p->n_right);
	size += argsiz(p);
	op->n_qual = size; /* XXX */
}

/*
 * Return a class suitable for a specific type.
 */
int
gclass(TWORD t)
{
	return (szty(t) == 2 ? CLASSB : CLASSA);
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
		/* there are 12 classa, so min 6 classb are needed to block */
		num = r[CLASSB] * 2;
		num += r[CLASSA];
		return num < 12;
	case CLASSB:
		/* 6 classa may block all classb */
		num = r[CLASSB] + r[CLASSA];
		return num < 6;
	}
	comperr("COLORMAP");
	return 0; /* XXX gcc */
}

/*
 * Special shapes.
 */
int
special(NODE *p, int shape)
{
	switch (shape) {
	case SNCON:
		if (p->n_name[0] != '\0')
			return SRDIR;
		break;
	default:
		comperr("special");
	}
	return SRNOPE;
}
