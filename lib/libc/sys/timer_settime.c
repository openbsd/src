#include <signal.h>
#include <time.h>
#include <errno.h>

int
timer_settime(timerid, flags, value, ovalue)
	timer_t timerid;
	int flags;
	const struct itimerspec *value;
	struct itimerspec *ovalue;
{
	errno = ENOSYS;
	return -1;
}
