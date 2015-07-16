/*	$OpenBSD: complex.c,v 1.1 2015/07/16 13:29:11 martynas Exp $	*/

/*
 * Written by Martynas Venckus.  Public domain
 */

#include <assert.h>
#include <complex.h>
#include <math.h>

#define	PREC	1000
#define	test(f, r, i)	(					\
	floor((__real__ (f)) * PREC) == floor((r) * PREC) && 	\
	floor((__imag__ (f)) * PREC) == floor((i) * PREC)	\
)
#define	testf(f, r, i)	(					\
	floorf((__real__ (f)) * PREC) == floorf((r) * PREC) && 	\
	floorf((__imag__ (f)) * PREC) == floorf((i) * PREC)	\
)

int
main(int argc, char *argv[])
{
	double complex r, z4 = -1.1 - 1.1 * I;
	float complex rf, z4f = -1.1 - 1.1 * I;

	r = cacosh(z4);
	assert(test(r, 1.150127, -2.256295));
	r = casinh(z4);
	assert(test(r, -1.150127, -0.685498));
	r = catanh(z4);
	assert(test(r, -0.381870, -1.071985));

	rf = cacoshf(z4f);
	assert(testf(rf, 1.150127, -2.256295));
	rf = casinhf(z4f);
	assert(testf(rf, -1.150127, -0.685498));
	rf = catanhf(z4f);
	assert(testf(rf, -0.381870, -1.071985));

	return (0);
}
