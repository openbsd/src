/*	$OpenBSD: test-15.c,v 1.1 2005/12/16 03:02:22 cloder Exp $	*/

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

	i = 0 / 0;
	f = 0.0f / 0.0f;
	d = 0.0 / 0.0;
	L = 0L / 0L;

	dbzd(0.0 / 0.0);
	dbzf(0.0f / 0.0f);
	dbzi(0 / 0);
	dbzl(0L / 0L);

	i++;
	f++;
	d++;
	L++;

	return 0;
}



