/*	$OpenBSD: time.c,v 1.3 2015/01/20 04:41:01 krw Exp $	*/
#include <sys/types.h>

#include "libsa.h"

time_t
getsecs(void)
{
	uint32_t count;

	__asm volatile ("mftb %0" : "=r" (count));
	return (count / 66666666);
}
