#include <signal.h>
#include <time.h>
#include <errno.h>

int
timer_delete(timerid)
	timer_t timerid;
{
	errno = ENOSYS;
	return -1;
}
