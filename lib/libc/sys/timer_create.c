#include <signal.h>
#include <time.h>
#include <errno.h>

int
timer_create(clock_id, evp, timerid)
	clockid_t clock_id;
	struct sigevent *evp;
	timer_t *timerid;
{
	errno = ENOSYS;
	return -1;
}
