#include <sys/types.h>

#include "libsa.h"

time_t
getsecs(void)
{
	uint32_t count;

	__asm volatile ("mftb %0" : "=r" (count));
	return (count / 66666666);
}
