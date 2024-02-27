/* Public domain */

#include <signal.h>
#include <stdint.h>
#include <stdio.h>
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

#if defined(__amd64__)
static int
has_cet_ibt(void)
{
	uint32_t d;

	asm("cpuid" : "=d" (d) : "a" (7), "c" (0));
	return (d & (1U << 20)) ? 1 : 0;
}
#endif

int
main(void)
{
	struct sigaction sa;

#if defined(__amd64__)
	if (!has_cet_ibt()) {
		printf("Unsupported CPU\n");
		printf("SKIPPED\n");
		exit(0);
	}
#endif

	sa.sa_sigaction = handler;
	sa.sa_mask = 0;
	sa.sa_flags = SA_SIGINFO;
	sigaction(SIGILL, &sa, NULL);

	bar();
	exit(1);
}
