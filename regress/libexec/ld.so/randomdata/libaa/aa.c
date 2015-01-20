/*	$OpenBSD: aa.c,v 1.2 2015/01/20 04:41:01 krw Exp $	*/
#include <sys/types.h>

static int64_t aavalue __attribute__((section(".openbsd.randomdata")));

int64_t
getaavalue()
{
	return (aavalue);
}
