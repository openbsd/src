
#include <stdarg.h>
#include "stand.h"

extern volatile void abort();
extern int _estack[];

__dead void
panic(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    printf(fmt, ap);
    printf("\n");
    va_end(ap);
	stackdump(0);
	abort();
}

stackdump(dummy)
	int dummy;
{
	int *ip;

	printf("stackdump:\n");
	for (ip = &dummy; ip < _estack; ip += 4) {
		printf("%x: %x %x %x %x\n",
			   (int)ip, ip[0], ip[1], ip[2], ip[3]);
	}
}
