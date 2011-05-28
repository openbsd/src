/*	$OpenBSD: lgamma.c,v 1.1 2011/05/28 22:38:06 martynas Exp $	*/

/*
 * Written by Martynas Venckus.  Public domain
 */

#include <assert.h>
#include <math.h>

int
main(int argc, char *argv[])
{
	assert(isnan(lgamma(NAN)));
	assert(isnan(lgammaf(NAN)));

	signgam = 0;
	assert(lgamma(-HUGE_VAL) == HUGE_VAL && signgam == 1);
	signgam = 0;
	assert(lgammaf(-HUGE_VAL) == HUGE_VAL && signgam == 1);

	signgam = 0;
	assert(lgamma(HUGE_VAL) == HUGE_VAL && signgam == 1);
	signgam = 0;
	assert(lgammaf(HUGE_VAL) == HUGE_VAL && signgam == 1);

	signgam = 0;
	assert(lgamma(-0.0) == HUGE_VAL && signgam == -1);
	signgam = 0;
	assert(lgammaf(-0.0) == HUGE_VAL && signgam == -1);

	signgam = 0;
	assert(lgamma(0.0) == HUGE_VAL && signgam == 1);
	signgam = 0;
	assert(lgammaf(0.0) == HUGE_VAL && signgam == 1);

	signgam = 0;
	assert(lgamma(1.0) == 0.0 && signgam == 1);
	signgam = 0;
	assert(lgammaf(1.0) == 0.0 && signgam == 1);

	signgam = 0;
	assert(lgamma(3.0) == M_LN2 && signgam == 1);
	signgam = 0;
	assert(lgammaf(3.0) == (float)M_LN2 && signgam == 1);

	return (0);
}
