/*
 * GSP assembler - expression evaluation
 *
 * Copyright (c) 1993 Paul Mackerras.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Paul Mackerras.
 * 4. The name of the author may not be used to endorse or promote products
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
#include "gsp_ass.h"
#include "y.tab.h"

int32_t eval_op(int, int32_t, int32_t);
int32_t eval_subtree(expr, unsigned int *);

expr
fold(register expr x)
{
	register int32_t l;
	register expr lp, rp;

	switch( x->e_op ){
	case SYM:
	case CONST:
	case '.':
		return x;
	}
	x->e_left = lp = fold(x->e_left);
	if( x->e_right != NULL )
		x->e_right = rp = fold(x->e_right);
	else
		rp = NULL;
	if( lp->e_op == CONST && (rp == NULL || rp->e_op == CONST) ){
		/* operator with constant subtree(s) */
		if( rp != NULL ){
			l = eval_op(x->e_op, lp->e_val, rp->e_val);
			free(rp);
		} else
			l = eval_op(x->e_op, lp->e_val, 0);
		free(lp);
		x->e_op = CONST;
		x->e_val = l;
	}
	return x;
}

int32_t
eval_op(int op, register int32_t l, register int32_t r)
{
	switch( op ){
	case NEG:	l = -l;		break;
	case '~':	l = ~l;		break;
	case '+':	l += r;		break;
	case '-':	l -= r;		break;
	case '*':	l *= r;		break;
	case '&':	l &= r;		break;
	case '|':	l |= r;		break;
	case '^':	l ^= r;		break;
	case '/':
		if( r == 0 )
			perr("Divide by zero");
		else
			l /= r;
		break;
	case ':':
		l = (l << 16) | (r & 0xFFFF);
		break;
	case LEFT_SHIFT:
		l <<= r;
		break;
	case RIGHT_SHIFT:
		l >>= r;
		break;
	}
	return l;
}

int
eval_expr(expr e, int32_t *vp, unsigned int *lp)
{
	e = fold(e);
	*vp = eval_subtree(e, lp);
	return (*lp < NOT_YET);
}

int32_t
eval_subtree(expr e, unsigned int *lp)
{
	register symbol s;
	int32_t v1, v2;
	unsigned int l2;

	switch( e->e_op ){
	case SYM:
		s = e->e_sym;
		*lp = s->lineno;
		if( (s->flags & DEFINED) != 0 )
			return s->value;
		perr("Undefined symbol %s", s->name);
		return 0;
	case CONST:
		*lp = 0;
		return e->e_val;
	case '.':
		*lp = lineno;
		return pc;
	default:
		v1 = eval_subtree(e->e_left, lp);
		if( e->e_right == NULL )
			return eval_op(e->e_op, v1, 0);
		v2 = eval_subtree(e->e_right, &l2);
		if( l2 > *lp )
			*lp = l2;
		return eval_op(e->e_op, v1, v2);
	}
}
