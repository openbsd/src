#include <sys/types.h>

#include "libsa.h"

time_t
getsecs(void)
{
	uint32_t count;

	__asm volatile ("mrc p6, 0, %0, c3, c1, 0" : "=r" (count));
	return ((0xffffffff - count) / 12500000);
}
