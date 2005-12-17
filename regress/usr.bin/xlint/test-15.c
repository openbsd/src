/*	$OpenBSD: test-15.c,v 1.2 2005/12/17 20:05:49 cloder Exp $	*/

/*
 * Placed in the public domain by Chad Loder <cloder@openbsd.org>.
 *
 * Test lint dealing with division by zero.
 */

/* ARGSUSED */
void dbzd(double d) { }
/* ARGSUSED */
void dbzf(float f) { }
/* ARGSUSED */
void dbzi(int i) { }
/* ARGSUSED */
void dbzl(long L) { }

/*ARGSUSED*/
int
main(int argc, char* argv[])
{
	double d;
	long L;
	int i;
	float f;

	i = 1 / 0;
	f = 1.0f / 0.0f;
	d = 1.0 / 0.0;
	L = 1L / 0L;

	dbzd(1.0 / 0.0);
	dbzf(1.0f / 0.0f);
	dbzi(1 / 0);
	dbzl(1L / 0L);

	i = 1 % 0;
	L = 1L % 0L;

	i++;
	f++;
	d++;
	L++;

	return 0;
}



