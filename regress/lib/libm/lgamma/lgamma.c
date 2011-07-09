/*	$OpenBSD: lgamma.c,v 1.2 2011/07/09 03:33:07 martynas Exp $	*/

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
	assert(lgammaf(-HUGE_VALF) == HUGE_VALF && signgam == 1);
	signgam = 0;
	assert(lgammal(-HUGE_VALL) == HUGE_VALL && signgam == 1);

	signgam = 0;
	assert(lgamma(HUGE_VAL) == HUGE_VAL && signgam == 1);
	signgam = 0;
	assert(lgammaf(HUGE_VALF) == HUGE_VALF && signgam == 1);
	signgam = 0;
	assert(lgammal(HUGE_VALL) == HUGE_VALL && signgam == 1);

	signgam = 0;
	assert(lgamma(-0.0) == HUGE_VAL && signgam == -1);
	signgam = 0;
	assert(lgammaf(-0.0F) == HUGE_VALF && signgam == -1);
	signgam = 0;
	assert(lgammal(-0.0L) == HUGE_VALL && signgam == -1);

	signgam = 0;
	assert(lgamma(0.0) == HUGE_VAL && signgam == 1);
	signgam = 0;
	assert(lgammaf(0.0F) == HUGE_VALF && signgam == 1);
	signgam = 0;
	assert(lgammal(0.0L) == HUGE_VALL && signgam == 1);

	signgam = 0;
	assert(lgamma(1.0) == 0.0 && signgam == 1);
	signgam = 0;
	assert(lgammaf(1.0F) == 0.0F && signgam == 1);
	signgam = 0;
	assert(lgammal(1.0L) == 0.0L && signgam == 1);

	signgam = 0;
	assert(lgamma(3.0) == M_LN2 && signgam == 1);
	signgam = 0;
	assert(lgammaf(3.0F) == (float)M_LN2 && signgam == 1);
	signgam = 0;
	assert(lgammal(3.0L) == 0.6931471805599453094172321214581766L &&
	    signgam == 1);

	return (0);
}
