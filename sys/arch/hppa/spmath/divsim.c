/*	$OpenBSD: divsim.c,v 1.3 1998/07/02 19:05:12 mickey Exp $	*/

/*
 * Copyright 1996 1995 by Open Software Foundation, Inc.   
 *              All Rights Reserved 
 *  
 * Permission to use, copy, modify, and distribute this software and 
 * its documentation for any purpose and without fee is hereby granted, 
 * provided that the above copyright notice appears in all copies and 
 * that both the copyright notice and this permission notice appear in 
 * supporting documentation. 
 *  
 * OSF DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE 
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS 
 * FOR A PARTICULAR PURPOSE. 
 *  
 * IN NO EVENT SHALL OSF BE LIABLE FOR ANY SPECIAL, INDIRECT, OR 
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM 
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN ACTION OF CONTRACT, 
 * NEGLIGENCE, OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION 
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE. 
 * 
 */
/*
 * pmk1.1
 */
/*
 * (c) Copyright 1986 HEWLETT-PACKARD COMPANY
 *
 * To anyone who acknowledges that this file is provided "AS IS" 
 * without any express or implied warranty:
 *     permission to use, copy, modify, and distribute this file 
 * for any purpose is hereby granted without fee, provided that 
 * the above copyright notice and this notice appears in all 
 * copies, and that the name of Hewlett-Packard Company not be 
 * used in advertising or publicity pertaining to distribution 
 * of the software without specific, written prior permission.  
 * Hewlett-Packard Company makes no representations about the 
 * suitability of this software for any purpose.
 */


#include "md.h"

void
divsim(opnd1,opnd2,result)

int opnd1, opnd2;
struct mdsfu_register *result;
{
	int sign, op1_sign;

	/* check divisor for zero */
	if (opnd2 == 0) {
		overflow = TRUE;
		return;
	}

	/* get sign of result */
	sign = opnd1 ^ opnd2;

	/* get absolute value of operands */
	if (opnd1 < 0) {
		opnd1 = -opnd1;
		op1_sign = TRUE;
	}
	else op1_sign = FALSE;
	if (opnd2 < 0) opnd2 = -opnd2;

	/* check for opnd2 == -2**31 */
	if (opnd2 < 0) {
		if (opnd1 == opnd2) {
			result_hi = 0;	/* remainder = 0 */
			result_lo = 1;
		}
		else {
			result_hi = opnd1;	/* remainder = opnd1 */
			result_lo = 0;
		}
	}
	else {
		/* do the divide */
		divu(0,opnd1,opnd2,result);

		/* 
		 * check for overflow
                 *        
                 * at this point, the only way we can get overflow
                 * is with opnd1 = -2**31 and opnd2 = -1
                 */
                if (sign>0 && result_lo<0) {
                        overflow = TRUE;
                        return;
                }
	}
	overflow = FALSE;

	/* return positive residue */
	if (op1_sign && result_hi) {
		result_hi = opnd2 - result_hi;
		if (++result_lo < 0) {
			overflow = TRUE;
			return;
		}
	}

	/* return appropriately signed result */
	if (sign<0) result_lo = -result_lo;
	return;
}
