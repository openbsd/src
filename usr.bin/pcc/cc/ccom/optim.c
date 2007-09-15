/*	$Id: optim.c,v 1.1.1.1 2007/09/15 18:12:34 otto Exp $	*/
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

# define SWAP(p,q) {sp=p; p=q; q=sp;}
# define RCON(p) (p->n_right->n_op==ICON)
# define RO(p) p->n_right->n_op
# define RV(p) p->n_right->n_lval
# define LCON(p) (p->n_left->n_op==ICON)
# define LO(p) p->n_left->n_op
# define LV(p) p->n_left->n_lval

static int nncon(NODE *);

int oflag = 0;

/* remove left node */
static NODE *
zapleft(NODE *p)
{
	NODE *q;

	q = p->n_left;
	nfree(p->n_right);
	nfree(p);
	return q;
}

/*
 * fortran function arguments
 */
static NODE *
fortarg(NODE *p)
{
	if( p->n_op == CM ){
		p->n_left = fortarg( p->n_left );
		p->n_right = fortarg( p->n_right );
		return(p);
	}

	while( ISPTR(p->n_type) ){
		p = buildtree( UMUL, p, NIL );
	}
	return( optim(p) );
}

	/* mapping relationals when the sides are reversed */
short revrel[] ={ EQ, NE, GE, GT, LE, LT, UGE, UGT, ULE, ULT };

/*
 * local optimizations, most of which are probably
 * machine independent
 */
NODE *
optim(NODE *p)
{
	int o, ty;
	NODE *sp, *q;
	int i;
	TWORD t;

	if( (t=BTYPE(p->n_type))==ENUMTY || t==MOETY ) econvert(p);
	if( oflag ) return(p);

	ty = coptype(p->n_op);
	if( ty == LTYPE ) return(p);

	if( ty == BITYPE ) p->n_right = optim(p->n_right);
	p->n_left = optim(p->n_left);

	/* collect constants */
again:	o = p->n_op;
	switch(o){

	case SCONV:
	case PCONV:
		return( clocal(p) );

	case FORTCALL:
		p->n_right = fortarg( p->n_right );
		break;

	case ADDROF:
		if (LO(p) == TEMP)
			return p;
		if( LO(p) != NAME ) cerror( "& error" );

		if( !andable(p->n_left) ) return(p);

		LO(p) = ICON;

		setuleft:
		/* paint over the type of the left hand side with the type of the top */
		p->n_left->n_type = p->n_type;
		p->n_left->n_df = p->n_df;
		p->n_left->n_sue = p->n_sue;
		q = p->n_left;
		nfree(p);
		return q;

	case UMUL:
		if( LO(p) != ICON ) break;
		LO(p) = NAME;
		goto setuleft;

	case RS:
		if (LO(p) == RS && RCON(p->n_left) && RCON(p)) {
			/* two right-shift  by constants */
			RV(p) += RV(p->n_left);
			p->n_left = zapleft(p->n_left);
		}
#if 0
		  else if (LO(p) == LS && RCON(p->n_left) && RCON(p)) {
			RV(p) -= RV(p->n_left);
			if (RV(p) < 0)
				o = p->n_op = LS, RV(p) = -RV(p);
			p->n_left = zapleft(p->n_left);
		}
#endif
		if (RO(p) == ICON) {
			if (RV(p) < 0) {
				RV(p) = -RV(p);
				p->n_op = LS;
				goto again;
			}
#ifdef notyet /* must check for side effects, --a >> 32; */
			if (RV(p) >= tsize(p->n_type, p->n_df, p->n_sue) &&
			    ISUNSIGNED(p->n_type)) { /* ignore signed shifts */
				/* too many shifts */
				tfree(p->n_left);
				nfree(p->n_right);
				p->n_op = ICON; p->n_lval = 0; p->n_sp = NULL;
			} else
#endif
			/* avoid larger shifts than type size */
			if (RV(p) >= p->n_sue->suesize) {
				RV(p) = RV(p) % p->n_sue->suesize;
				werror("shift larger than type");
			}
			if (RV(p) == 0)
				p = zapleft(p);
		}
		break;

	case LS:
		if (LO(p) == LS && RCON(p->n_left) && RCON(p)) {
			/* two left-shift  by constants */
			RV(p) += RV(p->n_left);
			p->n_left = zapleft(p->n_left);
		}
#if 0
		  else if (LO(p) == RS && RCON(p->n_left) && RCON(p)) {
			RV(p) -= RV(p->n_left);
			p->n_left = zapleft(p->n_left);
		}
#endif
		if (RO(p) == ICON) {
			if (RV(p) < 0) {
				RV(p) = -RV(p);
				p->n_op = RS;
				goto again;
			}
#ifdef notyet /* must check for side effects */
			if (RV(p) >= tsize(p->n_type, p->n_df, p->n_sue)) {
				/* too many shifts */
				tfree(p->n_left);
				nfree(p->n_right);
				p->n_op = ICON; p->n_lval = 0; p->n_sp = NULL;
			} else
#endif
			/* avoid larger shifts than type size */
			if (RV(p) >= p->n_sue->suesize) {
				RV(p) = RV(p) % p->n_sue->suesize;
				werror("shift larger than type");
			}
			if (RV(p) == 0)  
				p = zapleft(p);
		}
		break;

	case MINUS:
		if (LCON(p) && RCON(p) && p->n_left->n_sp == p->n_right->n_sp) {
			/* link-time constants, but both are the same */
			/* solve it now by forgetting the symbols */
			p->n_left->n_sp = p->n_right->n_sp = NULL;
		}
		if( !nncon(p->n_right) ) break;
		RV(p) = -RV(p);
		o = p->n_op = PLUS;

	case MUL:
	case PLUS:
	case AND:
	case OR:
	case ER:
		/* commutative ops; for now, just collect constants */
		/* someday, do it right */
		if( nncon(p->n_left) || ( LCON(p) && !RCON(p) ) )
			SWAP( p->n_left, p->n_right );
		/* make ops tower to the left, not the right */
		if( RO(p) == o ){
			NODE *t1, *t2, *t3;
			t1 = p->n_left;
			sp = p->n_right;
			t2 = sp->n_left;
			t3 = sp->n_right;
			/* now, put together again */
			p->n_left = sp;
			sp->n_left = t1;
			sp->n_right = t2;
			p->n_right = t3;
			}
		if(o == PLUS && LO(p) == MINUS && RCON(p) && RCON(p->n_left) &&
		   conval(p->n_right, MINUS, p->n_left->n_right)){
			zapleft:

			q = p->n_left->n_left;
			nfree(p->n_left->n_right);
			nfree(p->n_left);
			p->n_left = q;
		}
		if( RCON(p) && LO(p)==o && RCON(p->n_left) &&
		    conval( p->n_right, o, p->n_left->n_right ) ){
			goto zapleft;
			}
		else if( LCON(p) && RCON(p) && conval( p->n_left, o, p->n_right ) ){
			zapright:
			nfree(p->n_right);
			q = makety(p->n_left, p->n_type, p->n_qual,
			    p->n_df, p->n_sue);
			nfree(p);
			return clocal(q);
			}

		/* change muls to shifts */

		if( o == MUL && nncon(p->n_right) && (i=ispow2(RV(p)))>=0){
			if( i == 0 ) { /* multiplication by 1 */
				goto zapright;
				}
			o = p->n_op = LS;
			p->n_right->n_type = INT;
			p->n_right->n_df = NULL;
			RV(p) = i;
			}

		/* change +'s of negative consts back to - */
		if( o==PLUS && nncon(p->n_right) && RV(p)<0 ){
			RV(p) = -RV(p);
			o = p->n_op = MINUS;
			}

		/* remove ops with RHS 0 */
		if ((o == PLUS || o == MINUS || o == OR || o == ER) &&
		    nncon(p->n_right) && RV(p) == 0) {
			goto zapright;
		}
		break;

	case DIV:
		if( nncon( p->n_right ) && p->n_right->n_lval == 1 )
			goto zapright;
		if (LCON(p) && RCON(p) && conval(p->n_left, DIV, p->n_right))
			goto zapright;
		if (RCON(p) && ISUNSIGNED(p->n_type) && (i=ispow2(RV(p))) > 0) {
			p->n_op = RS;
			RV(p) = i;
			q = p->n_right;
			if(tsize(q->n_type, q->n_df, q->n_sue) > SZINT)
				p->n_right = makety(q, INT, 0, 0, MKSUE(INT));

			break;
		}
		break;

	case MOD:
		if (RCON(p) && ISUNSIGNED(p->n_type) && ispow2(RV(p)) > 0) {
			p->n_op = AND;
			RV(p) = RV(p) -1;
			break;
		}
		break;

	case EQ:
	case NE:
	case LT:
	case LE:
	case GT:
	case GE:
	case ULT:
	case ULE:
	case UGT:
	case UGE:
		if( !LCON(p) ) break;

		/* exchange operands */

		sp = p->n_left;
		p->n_left = p->n_right;
		p->n_right = sp;
		p->n_op = revrel[p->n_op - EQ ];
		break;

		}

	return(p);
	}

int
ispow2(CONSZ c)
{
	int i;
	if( c <= 0 || (c&(c-1)) ) return(-1);
	for( i=0; c>1; ++i) c >>= 1;
	return(i);
}

int
nncon( p ) NODE *p; {
	/* is p a constant without a name */
	return( p->n_op == ICON && p->n_sp == NULL );
	}
