/*      $OpenBSD: test-20.c,v 1.1 2006/04/27 20:41:19 otto Exp $ */

/*
 * Placed in the public domain by Otto Moerbeek <otto@drijf.net>.
 *
 * Test the 'expression has null effect warning'
 */


int f(int x,...)
{
	int p;
	int i = (1,33), j = (p=0,p), k = (i+j, i=0);

	int a = 1, b = 2, t;

	t = a, a = b, b = t;

	1 + b, t = a, a = b, b = t, a + 1, b - 1;

	a + t, t = b;

	a + 1;

	b + 1, t = t;

	t = t, b + 1;

	1 + a;

	1,2,3,4,5;

	a = (1,(b=2),3,4,5);

	if (a + 1, b)
		a = 2;

	a ? b=1 : t;

	a + 1, b = 1, t + 1;

	a = (1,b,f(a,(a,b),t));

	return k + k, x;
}
