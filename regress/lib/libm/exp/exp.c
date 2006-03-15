/*	$OpenBSD: exp.c,v 1.1 2006/03/15 21:44:06 otto Exp $	*/

/*	Written by Otto Moerbeek, 2006,  Public domain.	*/

#include <math.h>
#include <err.h>

int
main(void)
{
	double rd, bigd = HUGE_VAL;
	float rf, bigf = HUGE_VAL;

	rd = exp(bigd);
	if (!isinf(rd))
		errx(1, "exp(bigd) = %f", rd);
	rd = exp(-bigd);
	if (rd != 0.0)
		errx(1, "exp(-bigd) = %f", rd);

	rf = expf(bigf);
	if (!isinff(rf))
		errx(1, "exp(bigf) = %f", rf);
	rf = expf(-bigf);
	if (rf != 0.0F)
		errx(1, "exp(-bigf) = %f", rf);
	return (0);
}
