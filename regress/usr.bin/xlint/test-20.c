/*      $OpenBSD: test-20.c,v 1.2 2006/05/03 18:23:17 otto Exp $ */

/*
 * Placed in the public domain by Otto Moerbeek <otto@drijf.net>.
 *
 * Test the 'expression has null effect warning'
 */

#include <assert.h>

int f(int x,...)
{
	int p;
	char *q = 0;
	int i = (1,33), j = (p=0,p), k = (i+j, i=0);

	int a = i < 1 ? j : i, b = 2, t;

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

	*q = 0, *q = 0, *q = 0, *q = 0, *q = 0;

	*q + 0, *q = 0, *q = 0, *q = 0, *q = 0;

	q ? q = 0 : q++;

	assert(p == j);

	0;

	(void)0;

	j = j < 0 ? j + 1 : j + 2;

	return k + k, x;
}
