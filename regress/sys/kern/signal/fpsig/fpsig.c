/*	$OpenBSD: fpsig.c,v 1.2 2005/07/15 07:28:33 otto Exp $	*/

/*
 * Public domain.  2005, Otto Moerbeek
 *
 * Try to check if fp registers are properly saved and restored while
 * calling a signal hander.  This is not supposed to catch all that
 * can go wrong, but trashed fp registers will typically get caught.
 */
 
#include <err.h>
#include <signal.h>
#include <unistd.h>

#define LIMIT	10.0

volatile sig_atomic_t count;

volatile double g1;
volatile double g2;

void
handler(int signo)
{
	double a, b, c = 0.0;

	if (signo)
		alarm(1);
	for (a = 0.0; a < LIMIT; a++)
		for (b = 0.0; b < LIMIT; b++)
			c += a * a + b * b;

	if (signo) {
		g1 = c;
		count++;
	} else
		g2 = c;
}

int
main()
{
	signal(SIGALRM, handler);
	/* initialize global vars */
	handler(0);
	handler(1);
	
	while (count < 10) {
		handler(0);
		if (g1 != g2)
			errx(1, "%f %f", g1, g2);
	}
	return (0);
}
