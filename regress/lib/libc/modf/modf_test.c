/* Public domain, 2014, Tobias Ulmer <tobiasu@tmux.org> */

/* Test for bug introduced in 4.4BSD modf() on sparc */

#include <math.h>

#define BIGFLOAT (5e15) /* Number large enough to trigger the "big" case */

int
main(void)
{
	double f, i;

	f = modf(BIGFLOAT, &i);
	if (i != BIGFLOAT)
		return 1;
	if (f != 0.0)
		return 1;

	/* Repeat, maybe we were lucky */
	f = modf(BIGFLOAT, &i);
	if (i != BIGFLOAT)
		return 1;
	if (f != 0.0)
		return 1;

	/* With negative number, for good measure */
	f = modf(-BIGFLOAT, &i);
	if (i != -BIGFLOAT)
		return 1;
	if (f != 0.0)
		return 1;

	return 0;
}
