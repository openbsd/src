/*	$OpenBSD: nextafter.c,v 1.1 2011/04/16 20:41:23 martynas Exp $	*/

/*
 * Written by Martynas Venckus.  Public domain
 */

#include <assert.h>
#include <math.h>

#define	test(f, r)	(					\
	((f) == (r) && signbit((f)) == signbit((r))) ||		\
	(isnan((f)) && isnan((r)))				\
)

int
main(int argc, char *argv[])
{
	assert(test(nextafter(0.0, 0.0), 0.0));
	assert(test(nextafter(-0.0, 0.0), 0.0));
	assert(test(nextafter(0.0, -0.0), -0.0));
	assert(test(nextafter(-0.0, -0.0), -0.0));

	assert(test(nextafterf(0.0, 0.0), 0.0));
	assert(test(nextafterf(-0.0, 0.0), 0.0));
	assert(test(nextafterf(0.0, -0.0), -0.0));
	assert(test(nextafterf(-0.0, -0.0), -0.0));

	assert(test(nextafter(NAN, 1.0), NAN));
	assert(test(nextafter(1.0, NAN), NAN));
	assert(test(nextafter(NAN, NAN), NAN));

	assert(test(nextafterf(NAN, 1.0), NAN));
	assert(test(nextafterf(1.0, NAN), NAN));
	assert(test(nextafterf(NAN, NAN), NAN));

	assert(test(nextafter(0x1.fffffffffffffp+0, INFINITY), 0x1p1));
	assert(test(nextafter(0x1p1, -INFINITY), 0x1.fffffffffffffp+0));

	assert(test(nextafterf(0x1.fffffep+0f, INFINITY), 0x1p1f));
	assert(test(nextafterf(0x1p1f, -INFINITY), 0x1.fffffep+0f));

	return (0);
}
