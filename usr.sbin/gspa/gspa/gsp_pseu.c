/*
 * GSP assembler - assembler directives
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
#include "gsp_code.h"

extern unsigned int highest_pc, line_pc;

void
pseudo(int code, operand ops)
{
	operand o;
	int32_t val;
	unsigned int ln;
	u_int16_t words[2];

	switch( code ){
	case ORG:
		if( ops == NULL )
			break;
		if( ops->type != EXPR ){
			perr("Inappropriate operand");
			break;
		}
		if( !eval_expr(ops->op_u.value, &val, &ln) ){
			p1err("ORG operand must be defined on pass 1");
			break;
		}
		if( pc > highest_pc )
			highest_pc = pc;
		line_pc = pc = val;
		do_list_pc();
		break;
#ifdef EQU
	case EQU:
		if( label == NULL ){
			perr("Label required");
			break;
		}
		if( ops == NULL )
			break;
		if( ops->type != EXPR ){
			perr("Inappropriate operand");
			break;
		}
		do_asg(label, ops->op_u.value, 0);
		break;
#endif /* EQU */
	case WORD:
	case LONG:
		if( ops == NULL )
			break;
		for( o = ops; o != NULL; o = o->next ){
			if( o->type != EXPR ){
				perr("Inappropriate operand");
				continue;
			}
			if( pass2 ){
				eval_expr(o->op_u.value, &val, &ln);
				words[0] = val;
				if( code == LONG ){
					words[1] = val >> 16;
					putcode(words, 2);
				} else {
					if( val < -32768 || val > 65535 )
						perr("Word value too large");
					putcode(words, 1);
				}
			} else
				pc += code == LONG? 0x20: 0x10;
		}
		return;
	case INCL:
		if( ops == NULL )
			break;
		if( ops->type != STR_OPN ){
			perr("Require filename string");
			break;
		}
		push_input(ops->op_u.string);
		break;
	case BLKB:
	case BLKW:
	case BLKL:
		if( ops == NULL )
			break;
		if( ops->type != EXPR ){
			perr("Inappropriate operand");
			break;
		}
		if( !eval_expr(ops->op_u.value, &val, &ln) ){
			p1err(".BLK%c operand must be defined on pass 1",
				code==BLKB? 'B': code==BLKW? 'W': 'L');
			break;
		}
		val *= 8;
		if( code == BLKB )
			val = (val + 8) & ~15;	/* round to word */
		else
			val *= (code==BLKW? 2: 4);
		pc += val;
		do_list_pc();
		break;
	case START:
		if( !pass2 || ops == NULL )
			break;
		if( ops->type != EXPR ){
			perr("Inappropriate operand");
			break;
		}
		eval_expr(ops->op_u.value, &val, &ln);
		start_at(val);
		do_show_val(val);
		break;
	}
	if( ops == NULL )
		perr("Insufficient operands");
	else if( ops->next != NULL )
		perr("Extra operands ignored");
}
