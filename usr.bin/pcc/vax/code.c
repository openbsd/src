/*	$OpenBSD: code.c,v 1.2 2007/10/27 14:19:18 ragge Exp $	*/
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

# include "pass1.h"

short log2tab[] = {0, 0, 1, 2, 2, 3, 3, 3, 3};
#define LOG2SZ 9

void
defalign(n) {
	/* cause the alignment to become a multiple of n */
	n /= SZCHAR;
	if( lastloc != PROG && n > 1 ) printf( "	.align	%d\n", n >= 0 && n < LOG2SZ ? log2tab[n] : 0 );
	}

/*
 * output something to define the current position as label n
 */
void
deflab1(int n)
{
	printf(LABFMT ":\n", n);
}

void
efcode(){
	/* code for the end of a function */
	if (cftnsp->stype != STRTY+FTN && cftnsp->stype != UNIONTY+FTN)
		return;
	cerror("efcode");

#ifdef notyet
	if( strftn ){  /* copy output (in R2) to caller */
		register NODE *l, *r;
		register struct symtab *p;
		register TWORD t;
		register int j;
		int i;

		p = &stab[curftn];
		t = p->stype;
		t = DECREF(t);

		deflab( retlab );

		i = getlab();	/* label for return area */
		printf("	.data\n" );
		printf("	.align	2\n" );
		deflab1(i);
		printf("\t.space  %d\n", tsize(t, p->dimoff, p->sizoff)/SZCHAR);
		printf("	.text\n" );
		psline();
		printf("	movab	" LABFMT ",r1\n", i);

		reached = 1;
		l = block( REG, NIL, NIL, PTR|t, p->dimoff, p->sizoff );
		l->rval = 1;  /* R1 */
		l->lval = 0;  /* no offset */
		r = block( REG, NIL, NIL, PTR|t, p->dimoff, p->sizoff );
		r->rval = 0;  /* R0 */
		r->lval = 0;
		l = buildtree( UNARY MUL, l, NIL );
		r = buildtree( UNARY MUL, r, NIL );
		l = buildtree( ASSIGN, l, r );
		l->op = FREE;
		ecomp( l->left );
		printf( "	movab	" LABFMT ",r0\n", i );
		/* turn off strftn flag, so return sequence will be generated */
		strftn = 0;
		}
	branch( retlab );
	printf( "	.set	.R%d,0x%x\n", ftnno, ent_mask[reg_use] );
	reg_use = 11;
	p2bend();
	fdefflag = 0;
#endif
	}

void
bfcode(struct symtab **a, int n)
{
	int i;

	if (cftnsp->stype != STRTY+FTN && cftnsp->stype != UNIONTY+FTN)
		return;
	/* Function returns struct, adjust arg offset */
	for (i = 0; i < n; i++)
		a[i]->soffset += SZPOINT(INT);
}

void
bccode(){ /* called just before the first executable statment */
		/* by now, the automatics and register variables are allocated */
	SETOFF( autooff, SZINT );
#if 0
	/* set aside store area offset */
	p2bbeg( autooff, regvar );
	reg_use = (reg_use > regvar ? regvar : reg_use);
#endif
	}

void
ejobcode( flag ){
	/* called just before final exit */
	/* flag is 1 if errors, 0 if none */
	}

void
bjobcode()
{
}

#if 0
aobeg(){
	/* called before removing automatics from stab */
	}

aocode(p) struct symtab *p; {
	/* called when automatic p removed from stab */
	}

aoend(){
	/* called after removing all automatics from stab */
	}
#endif

void
defnam( p ) register struct symtab *p; {
	/* define the current location as the name p->sname */

	if( p->sclass == EXTDEF ){
		printf( "	.globl	%s\n", exname( p->sname ) );
		}
	printf( "%s:\n", exname( p->sname ) );

	}

void
bycode( t, i ){
	/* put byte i+1 in a string */

	i &= 07;
	if( t < 0 ){ /* end of the string */
		if( i != 0 ) printf( "\n" );
		}

	else { /* stash byte t into string */
		if( i == 0 ) printf( "	.byte	" );
		else printf( "," );
		printf( "0x%x", t );
		if( i == 07 ) printf( "\n" );
		}
	}

void
zecode( n ){
	/* n integer words of zeros */
	if( n <= 0 ) return;
	printf( "	.space	%d\n", (SZINT/SZCHAR)*n );
	}

int
fldal( t ) unsigned t; { /* return the alignment of field of type t */
	uerror( "illegal field type" );
	return( ALINT );
	}

void
fldty( p ) struct symtab *p; { /* fix up type of field p */
	;
	}

/* p points to an array of structures, each consisting
 * of a constant value and a label.
 * The first is >=0 if there is a default label;
 * its value is the label number
 * The entries p[1] to p[n] are the nontrivial cases
 * XXX - fix genswitch.
 */
void
genswitch(int num, struct swents **p, int n)
{
	cerror("genswitch");
}

#ifdef notyet
struct sw heapsw[SWITSZ];	/* heap for switches */

genswitch(p,n) register struct sw *p;{
	/*	p points to an array of structures, each consisting
		of a constant value and a label.
		The first is >=0 if there is a default label;
		its value is the label number
		The entries p[1] to p[n] are the nontrivial cases
		*/
	register i;
	register CONSZ j, range;
	register dlab, swlab;

	range = p[n].sval-p[1].sval;

	if( range>0 && range <= 3*n && n>=4 ){ /* implement a direct switch */

		swlab = getlab();
		dlab = p->slab >= 0 ? p->slab : getlab();

		/* already in r0 */
		printf("	casel	r0,$%ld,$%ld\n", p[1].sval, range);
		deflab1(swlab);
		for( i=1,j=p[1].sval; i<=n; j++) {
			printf("	.word	" LABFMT "-" LABFMT "\n",
			    (j == p[i].sval ? ((j=p[i++].sval), p[i-1].slab) : dlab),
				swlab);
			}

		if( p->slab >= 0 ) branch( dlab );
		else deflab1(dlab);
		return;

		}

	if( n>8 ) {	/* heap switch */

		heapsw[0].slab = dlab = p->slab >= 0 ? p->slab : getlab();
		makeheap(p, n, 1);	/* build heap */

		walkheap(1, n);	/* produce code */

		if( p->slab >= 0 )
			branch( dlab );
		else
			deflab1(dlab);
		return;
	}

	/* debugging code */

	/* out for the moment
	if( n >= 4 ) werror( "inefficient switch: %d, %d", n, (int) (range/n) );
	*/

	/* simple switch code */

	for( i=1; i<=n; ++i ){
		/* already in r0 */

		printf( "	cmpl	r0,$" );
		printf( CONFMT, p[i].sval );
		printf( "\n	jeql	" LBLFMT "\n", p[i].slab );
		}

	if( p->slab>=0 ) branch( p->slab );
	}

makeheap(p, m, n)
register struct sw *p;
{
	register int q;

	q = select(m);
	heapsw[n] = p[q];
	if( q>1 ) makeheap(p, q-1, 2*n);
	if( q<m ) makeheap(p+q, m-q, 2*n+1);
}

select(m) {
	register int l,i,k;

	for(i=1; ; i*=2)
		if( (i-1) > m ) break;
	l = ((k = i/2 - 1) + 1)/2;
	return( l + (m-k < l ? m-k : l));
}

walkheap(start, limit)
{
	int label;


	if( start > limit ) return;
	printf("	cmpl	r0,$%d\n",  heapsw[start].sval);
	printf("	jeql	" LBLFMT "\n", heapsw[start].slab);
	if( (2*start) > limit ) {
		printf("	jbr 	" LBLFMT "\n", heapsw[0].slab);
		return;
	}
	if( (2*start+1) <= limit ) {
		label = getlab();
		printf("	jgtr	" LBLFMT "\n", label);
	} else
		printf("	jgtr	" LBLFMT "\n", heapsw[0].slab);
	walkheap( 2*start, limit);
	if( (2*start+1) <= limit ) {
		deflab1(label);
		walkheap( 2*start+1, limit);
	}
}
#endif
