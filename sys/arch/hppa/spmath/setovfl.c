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


#include "../spmath/float.h"
#include "../spmath/sgl_float.h"
#include "../spmath/dbl_float.h"

sgl_floating_point sgl_setoverflow(sign)

unsigned int sign;
{
	sgl_floating_point result;

	/* set result to infinity or largest number */
	/* ignore for now
	switch (Rounding_mode()) {
		case ROUNDPLUS:
			if (sign) {
				Sgl_setlargestnegative(result);
			}
			else {
				Sgl_setinfinitypositive(result);
			}
			break;
		case ROUNDMINUS:
			if (sign==0) {
				Sgl_setlargestpositive(result);
			}
			else {
				Sgl_setinfinitynegative(result);
			}
			break;
		case ROUNDNEAREST:
			Sgl_setinfinity(result,sign);
			break;
		case ROUNDZERO:
			Sgl_setlargest(result,sign);
	}
	return(result);
	*/
}

dbl_floating_point dbl_setoverflow(sign)

unsigned int sign;
{
	dbl_floating_point result;

	/* set result to infinity or largest number */
	/* ignore for now
	switch (Rounding_mode()) {
		case ROUNDPLUS:
			if (sign) {
				Dbl_setlargestnegative(result);
			}
			else {
				Dbl_setinfinitypositive(result);
			}
			break;
		case ROUNDMINUS:
			if (sign==0) {
				Dbl_setlargestpositive(result);
			}
			else {
				Dbl_setinfinitynegative(result);
			}
			break;
		case ROUNDNEAREST:
			Dbl_setinfinity(result,sign);
			break;
		case ROUNDZERO:
			Dbl_setlargest(result,sign);
	}
	return(result);
	*/
}
