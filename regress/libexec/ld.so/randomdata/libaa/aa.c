#include <sys/types.h>

static int64_t aavalue __attribute__((section(".openbsd.randomdata")));

int64_t
getaavalue()
{
	return (aavalue);
}
