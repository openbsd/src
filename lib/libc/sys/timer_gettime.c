#include <signal.h>
#include <time.h>
#include <errno.h>

int
timer_gettime(timerid, value)
	timer_t timerid;
	struct itimerspec *value;
{
	errno = ENOSYS;
	return -1;
}
