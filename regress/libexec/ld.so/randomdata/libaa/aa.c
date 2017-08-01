/*	$OpenBSD: aa.c,v 1.3 2017/08/01 13:05:55 deraadt Exp $	*/
#include <sys/types.h>

int64_t aavalue __attribute__((section(".openbsd.randomdata")));

int64_t
getaavalue()
{
	return (aavalue);
}
