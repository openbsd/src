#include <stdio.h>
#include <signal.h>
#include <string.h>
#include "test.h"

foo(int sig) 
{
	printf("%s\n", strsignal(sig));
	return;
}

main()
{
	sigset_t all;

	CHECKe(signal(1, foo));
	CHECKe(sigfillset(&all));
	CHECKe(sigprocmask(SIG_BLOCK, &all, NULL));
	CHECKe(pause());
	SUCCEED;
}
