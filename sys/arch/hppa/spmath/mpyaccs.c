/*	$OpenBSD: mpyaccs.c,v 1.3 1998/07/02 19:05:40 mickey Exp $	*/

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
mpyaccs(opnd1,opnd2,result)

int opnd1, opnd2;
struct mdsfu_register *result;
{
	struct mdsfu_register temp;
	int carry, sign;

	impys(&opnd1,&opnd2,&temp);

	/* get result of low word add, and check for carry out */
	if ((result_lo += (unsigned)temp.rslt_lo) < (unsigned)temp.rslt_lo) 
		carry = 1;
	else carry = 0;

	/* get result of high word add, and determine overflow status */
	sign = result_hi ^ temp.rslt_hi;
	result_hi += temp.rslt_hi + carry;
	if (sign >= 0 && (temp.rslt_hi ^ result_hi) < 0) overflow = TRUE;

	return;
}
