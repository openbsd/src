/*	$OpenBSD: prog.c,v 1.3 2017/08/01 13:05:55 deraadt Exp $	*/
#include <sys/types.h>
#include <assert.h>

extern int64_t getaavalue(void);

int64_t progvalue __attribute__((section(".openbsd.randomdata")));

int
main()
{
	int64_t aavalue = getaavalue();

	assert(progvalue != 0);
	assert(aavalue != 0);
	assert(progvalue != aavalue);
	return (0);
}
