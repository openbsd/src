/*
 * GSP assembler - semantic actions
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

void
free_operands(operand l)
{
	register operand op, oq;

	for( op = l; op != NULL; op = oq ){
		oq = op->next;
		if( op->type == EXPR || op->type == EA
					&& op->mode >= M_INDEX )
			free_expr(op->op_u.value);
		free(op);
	}
}

operand
add_operand(operand first, operand last)
{
	operand p;

	for( p = first; p->next != NULL; p = p->next )
		;
	p->next = last;
	return first;
}

operand
reg_op(int reg)
{
	register operand o;

/*	printf("reg_op reg=%d sign=%d\n", reg, sign);	*/
	ALLOC(o, operand);
	o->type = REG;
	o->reg_no = reg;
	o->next = NULL;
	return o;
}

operand
expr_op(expr val)
{
	register operand o;

/*	printf("immed len=%d\n", len);	*/
	ALLOC(o, operand);
	o->type = EXPR;
	o->op_u.value = val;
	o->next = NULL;
	return o;
}

operand
string_op(char *str)
{
	register operand o;

/*	printf("string_op str=%s\n", str);	*/
	ALLOC(o, operand);
	o->type = STR_OPN;
	o->op_u.string = str;
	o->next = NULL;
	return o;
}

operand
abs_adr(expr adr)
{
	register operand o;

/*	printf("abs_adr len=%d\n", len);	*/
	ALLOC(o, operand);
	o->type = EA;
	o->mode = M_ABSOLUTE;
	o->op_u.value = adr;
	o->next = NULL;
	return o;
}

operand
reg_ind(int reg, int mode)
{
	register operand o;

/*	printf("reg_adr r1=%d r2=%d mode=%d\n", r1, r2, mode);	*/
	ALLOC(o, operand);
	o->type = EA;
	o->mode = mode;
	o->reg_no = reg;
	o->next = NULL;
	return o;
}

operand
reg_indxy(int reg, char *xy)
{
	register operand o;

	ucasify(xy);
	if( strcmp(xy, ".XY") != 0 )
		perr("Register format must be .XY");
	return reg_ind(reg, M_INDXY);
}

operand
reg_index(int reg, expr disp)
{
	register operand o;

	o = reg_ind(reg, M_INDEX);
	o->op_u.value = disp;
	return o;
}

expr
id_expr(char *id)
{
	register expr x;

/*	printf("id_expr id=%s\n", id);	*/
	ALLOC(x, expr);
	x->e_op = SYM;
	x->e_sym = lookup(id, TRUE);
	return x;
}

expr
num_expr(int val)
{
	register expr x;

/*	printf("num_expr val=%d\n", val);	*/
	ALLOC(x, expr);
	x->e_op = CONST;
	x->e_val = val;
	return x;
}

expr
here_expr()
{
	register expr x;

/*	printf("here_expr()\n");	*/
	ALLOC(x, expr);
	x->e_op = '.';
	return x;
}

expr
bexpr(int op, expr l, expr r)
{
	register expr x;

/*	printf("bexpr op=%d\n", op);	*/
	ALLOC(x, expr);
	x->e_op = op;
	x->e_left = l;
	x->e_right = r;
	return x;
}

void
free_expr(register expr x)
{
	if( x->e_op != SYM && x->e_op != CONST && x->e_op != '.' ){
		free_expr(x->e_left);
		if( x->e_right != NULL )
			free_expr(x->e_right);
	}
	free(x);
}
