/* Public domain */

#include <signal.h>
#include <stdlib.h>

extern void foo(void);
void (*foobar)(void) = foo;

void
bar(void)
{
	foobar();
}

void
handler(int sig, siginfo_t *si, void *context)
{
	if (si->si_signo == SIGILL && si->si_code == ILL_BTCFI)
		exit(0);
}

int
main(void)
{
	struct sigaction sa;

	sa.sa_sigaction = handler;
	sa.sa_mask = 0;
	sa.sa_flags = SA_SIGINFO;
	sigaction(SIGILL, &sa, NULL);

	bar();
	exit(1);
}
