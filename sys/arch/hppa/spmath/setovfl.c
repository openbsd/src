/*	$OpenBSD: setovfl.c,v 1.6 2002/09/20 19:26:59 mickey Exp $	*/
/*
  (c) Copyright 1986 HEWLETT-PACKARD COMPANY
  To anyone who acknowledges that this file is provided "AS IS"
  without any express or implied warranty:
      permission to use, copy, modify, and distribute this file
  for any purpose is hereby granted without fee, provided that
  the above copyright notice and this notice appears in all
  copies, and that the name of Hewlett-Packard Company not be
  used in advertising or publicity pertaining to distribution
  of the software without specific, written prior permission.
  Hewlett-Packard Company makes no representations about the
  suitability of this software for any purpose.
*/
/* @(#)setovfl.c: Revision: 1.7.88.1 Date: 93/12/07 15:06:57 */

#include "float.h"
#include "sgl_float.h"
#include "dbl_float.h"

/*ARGSUSED*/
sgl_floating_point
sgl_setoverflow(sign)
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
	*/
	return(result);
}

/*ARGSUSED*/
dbl_floating_point
dbl_setoverflow(sign)
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
	*/
	return(result);
}
