/*	$OpenBSD: except.c,v 1.6 2004/04/02 03:06:12 mickey Exp $	*/

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <assert.h>
#include <ieeefp.h>
#include <float.h>

volatile sig_atomic_t signal_caught;

volatile const double one  = 1.0;
volatile const double zero = 0.0;
volatile const double huge = DBL_MAX;
volatile const double tiny = DBL_MIN;

void
sigfpe(int sig, siginfo_t *si, void *v)
{
	char buf[132];

	if (si) {
		snprintf(buf, sizeof(buf), "sigfpe: addr=%p, code=%d\n",
		    si->si_addr, si->si_code);
		write(1, buf, strlen(buf));
	}
	signal_caught = 1;
}


int
main(int argc, char *argv[])
{
	struct sigaction sa;
	volatile double x;

	/*
	 * check to make sure that all exceptions are masked and 
	 * that the accumulated exception status is clear.
 	 */
	assert(fpgetmask() == 0);
	assert(fpgetsticky() == 0);

	memset(&sa, 0, sizeof(sa));
	sa.sa_sigaction = sigfpe;
	sa.sa_flags = SA_SIGINFO;
	sigaction(SIGFPE, &sa, NULL);
	signal_caught = 0;

	/* trip divide by zero */
	x = one / zero;
	assert(fpgetsticky() & FP_X_DZ);
	assert(signal_caught == 0);
	fpsetsticky(0);

	/* trip invalid operation */
	x = zero / zero;
	assert(fpgetsticky() & FP_X_INV);
	assert(signal_caught == 0);
	fpsetsticky(0);

	/* trip overflow */
	x = huge * huge;
	assert(fpgetsticky() & FP_X_OFL);
	assert(signal_caught == 0);
	fpsetsticky(0);

	/* trip underflow */
	x = tiny * tiny;
	assert(fpgetsticky() & FP_X_UFL);
	assert(signal_caught == 0);
	fpsetsticky(0);

	/* unmask and then trip divide by zero */
	fpsetmask(FP_X_DZ);
	x = one / zero;
	assert(signal_caught == 1);
	signal_caught = 0;

	/* unmask and then trip invalid operation */
	fpsetmask(FP_X_INV);
	x = zero / zero;
	assert(signal_caught == 1);
	signal_caught = 0;

	/* unmask and then trip overflow */
	fpsetmask(FP_X_OFL);
	x = huge * huge;
	assert(signal_caught == 1);
	signal_caught = 0;

	/* unmask and then trip underflow */
	fpsetmask(FP_X_UFL);
	x = tiny * tiny;
	assert (signal_caught == 1);
	signal_caught = 0;

	exit(0);
}

